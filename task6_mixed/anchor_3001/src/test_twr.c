/* Task 6 Lv1 step B: libdeca DS-TWR on DWM3001CDK.
 * Same settings as the Task 2 tag (task2_evb_stm32/lv2/uwb_app/uwb_app.c): PAN 0xDECA, processing delay 2000 us,
 * send_report on, responder 0x0001 (anchor), initiator 0x0002 (tag). Build with -DT6_ROLE=responder|initiator;
 * a second anchor gets another address with -DT6_ANCHOR_ADDR=0x0003 (the tag only ranges to 0x0001).
 * Startup order follows Task 2 HANDOFF_2026-11-16: driver/libdeca init first, interrupt last.
 * Output: SEGGER RTT channel 0. Result lines: "TWR,<cnum>,<dist_cm>" or "TWR,<cnum>,FAIL". */
#include <stdbool.h>
#include <stdint.h>

#include "SEGGER_RTT.h"
#include "app_scheduler.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

#include "config.h"
#include "deca_device_api.h"
#include "dw3000_hw.h"
#include "dw3000_spi.h"
#include "dwhw.h"
#include "dwmac.h"
#include "dwphy.h"
#include "dwproto.h"
#include "ranging.h"

#define UWB_PAN_ID              0xDECAu
#define UWB_PROCESSING_DELAY_US 2000u
#define ADDR_RESPONDER          0x0001u /* the anchor the tag ranges to (Task 2 tag: 0x0001) */
#ifndef T6_ANCHOR_ADDR
#define T6_ANCHOR_ADDR          ADDR_RESPONDER
#endif
#define ADDR_INITIATOR          0x0002u
#define RANGING_PERIOD_MS       100u
#define STAT_PERIOD_MS          10000u
#define CPU_HZ                  64000000u

#define P(...) SEGGER_RTT_printf(0, __VA_ARGS__)

static uint32_t n_start, n_ok, n_fail, n_busy;
uint32_t dw3000_hw_loop_limit_hits(void);

static void twr_done(uint64_t src, uint64_t dst, uint16_t dist, uint16_t cnum)
{
    (void)src;
    (void)dst;
    /* runs from app_sched_execute() in the main loop, not from the IRQ */
    if (dist >= TWR_OK_VALUE) {
        n_fail++;
        P("TWR,%u,FAIL\n", cnum);
    } else {
        n_ok++;
        P("TWR,%u,%u\n", cnum, dist);
    }
}

static void stop(const char *why)
{
    dw3000_hw_interrupt_disable();
    P("FAIL: %s\n== stopped ==\n", why);
    for (;;) {
        __WFE();
    }
}

/* APP_SCHED_INIT uses APP_ERROR_CHECK */
void app_error_handler_bare(uint32_t err)
{
    P("app error 0x%08X\n", (unsigned)err);
    stop("app_error");
}

/* SYS_STATUS (file 0, offset 0x44) without the driver, for the pre-init RCINIT check */
static uint32_t raw_sys_status(void)
{
    uint8_t h[2] = {0x41, 0x10};
    uint8_t b[4] = {0};
    dw3000_spi_read(2, h, 4, b);
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint32_t ms_now(void)
{
    static uint32_t last, ms, rem;
    uint32_t c = DWT->CYCCNT;
    rem += c - last;
    last = c;
    ms += rem / (CPU_HZ / 1000u);
    rem %= CPU_HZ / 1000u;
    return ms;
}

int main(void)
{
    SEGGER_RTT_Init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    APP_SCHED_INIT(sizeof(struct rxbuf), 16);

#if T6_ROLE_INITIATOR
    const uint16_t me = ADDR_INITIATOR;
    const char *role = "initiator (tag)";
#else
    const uint16_t me = T6_ANCHOR_ADDR;
    const char *role = "responder (anchor)";
#endif
    P("\n== T6 Lv1 B: libdeca TWR on DWM3001CDK, %s 0x%04X ==\n", role, me);

    /* 1. hardware, slow SPI, reset, RCINIT by direct SPI read (interrupt still off) */
    if (dw3000_hw_init() != 0) {
        stop("dw3000_hw_init");
    }
    dw3000_hw_reset();
    uint32_t t0 = ms_now();
    while ((raw_sys_status() & 0x01000000UL) == 0) {
        if (ms_now() - t0 > 100) {
            stop("RCINIT timeout");
        }
    }
    /* 2. libdeca: probe, initialise, ID check, then fast SPI (8 MHz, tested in step A) */
    if (!dwhw_init()) {
        stop("dwhw_init");
    }
    if (!dwphy_config()) {
        stop("dwphy_config");
    }
    dwphy_set_antenna_delay(DWPHY_ANTENNA_DELAY); /* library default, not calibrated */
    if (!dwmac_init(UWB_PAN_ID, me, dwprot_rx_handler, NULL, NULL)) {
        stop("dwmac_init");
    }
    dwmac_set_frame_filter();
    twr_init(UWB_PROCESSING_DELAY_US, true);
    twr_set_observer(twr_done);

    /* 3. interrupt last. dw3000_hw_init_interrupt() (src/dw3000_hw_t6.c) services an IRQ that is
     *    already high (SPIRDY from init), because GPIOTE only sees low-to-high edges (step A). */
    if (dw3000_hw_init_interrupt() != 0) {
        stop("dw3000_hw_init_interrupt");
    }
    P("init done: IRQ=%u, antenna delay %u (library default)\n", (unsigned)nrf_gpio_pin_read(CONFIG_DW3000_GPIO_IRQ),
      (unsigned)DWPHY_ANTENNA_DELAY);

#if !T6_ROLE_INITIATOR
    dwmac_set_rx_reenable(true);
    dwt_forcetrxoff();
    dwt_rxenable(DWT_START_RX_IMMEDIATE);
    P("listening\n");
#endif

    uint32_t last_start = ms_now();
    uint32_t last_stat = last_start;
    for (;;) {
        app_sched_execute();
        uint32_t now = ms_now();
#if T6_ROLE_INITIATOR
        if (now - last_start >= RANGING_PERIOD_MS) {
            last_start = now;
            if (twr_in_progress()) {
                n_busy++;
            } else if (twr_start(ADDR_RESPONDER)) {
                n_start++;
            } else {
                n_busy++;
            }
        }
#endif
        if (now - last_stat >= STAT_PERIOD_MS) {
            last_stat = now;
            P("STAT,t=%u s,start=%u,ok=%u,fail=%u,busy=%u,irq_loop_limit=%u\n", (unsigned)(now / 1000u),
              (unsigned)n_start, (unsigned)n_ok, (unsigned)n_fail, (unsigned)n_busy,
              (unsigned)dw3000_hw_loop_limit_hits());
        }
    }
}
