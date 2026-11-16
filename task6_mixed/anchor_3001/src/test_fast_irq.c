/* Task 6 Lv1 step A: on a DWM3001CDK with the br101 driver v08.02.02
 *   S) SPI at 8 MHz after dwt_initialise (libdeca's dwhw_init switches to fast SPI)
 *   I) DW3000 interrupt: GPIOTE on IRQ (P1.02) -> platform handler -> dwt_isr() -> SPIRDY callback
 * Output: SEGGER RTT channel 0. Interrupt test follows Task 2 T2_UWB_ISR (A: IRQ already high, B: 20 resets). */
#include <stdint.h>
#include <string.h>

#include "SEGGER_RTT.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

#include "config.h"
#include "deca_device_api.h"
#include "deca_probe_interface.h"
#include "deca_version.h"
#include "dw3000_hw.h"
#include "dw3000_spi.h"

#define EXPECTED_ID  0xDECA0302UL
#define ID_READS     1000
#define WR_ROUNDS    50
#define RESETS       20
#define SPIRDY_RCINIT (0x00800000UL | 0x01000000UL)

#define P(...) SEGGER_RTT_printf(0, __VA_ARGS__)

/* Raw register access with the Qorvo SPI header, bypassing the driver.
 * 2-byte header: [rw<<7 | 1<<6 | file<<1 | off>>6] [(off & 0x3F) << 2], register file 0. */
static int raw_read32(uint8_t off, uint32_t *v)
{
    uint8_t h[2] = {(uint8_t)(0x40 | ((off >> 6) & 1)), (uint8_t)((off & 0x3F) << 2)};
    uint8_t b[4] = {0};
    int rc = dw3000_spi_read(2, h, 4, b);
    *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return rc;
}

static int raw_write32(uint8_t off, uint32_t v)
{
    uint8_t h[2] = {(uint8_t)(0xC0 | ((off >> 6) & 1)), (uint8_t)((off & 0x3F) << 2)};
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    return dw3000_spi_write(2, h, 4, b);
}

static uint32_t raw_dev_id(void)
{
    uint8_t h = 0x00;
    uint8_t b[4] = {0};
    if (dw3000_spi_read(1, &h, 4, b) != DWT_SUCCESS) {
        return 0;
    }
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static volatile uint32_t spirdy_count;
static volatile uint32_t spirdy_status;

static void on_spirdy(const dwt_cb_data_t *d)
{
    spirdy_count++;
    spirdy_status = d->status;
}

static int irq_level(void)
{
    return (int)nrf_gpio_pin_read(CONFIG_DW3000_GPIO_IRQ);
}

static void stop(const char *why)
{
    P("FAIL: %s\nverdict: FAIL\n== done ==\n", why);
    for (;;) {
        __WFE();
    }
}

static int test_fast_spi(void)
{
    static const uint32_t pat[] = {0xFFFFFFFF, 0x00000000, 0xA5A5A5A5, 0x5A5A5A5A,
                                   0x80402010, 0x08040201, 0x12345678, 0xCAFEBABE};
    const unsigned npat = sizeof(pat) / sizeof(pat[0]);

    dw3000_spi_speed_fast();
    P("\n-- S) SPI fast: NRF_SPIM0->FREQUENCY=0x%08X (8 MHz = 0x80000000)\n", (unsigned)NRF_SPIM0->FREQUENCY);

    unsigned drv_ok = 0, raw_ok = 0;
    for (unsigned i = 0; i < ID_READS; i++) {
        if (dwt_readdevid() == EXPECTED_ID) {
            drv_ok++;
        }
        if (raw_dev_id() == EXPECTED_ID) {
            raw_ok++;
        }
    }
    P("dwt_readdevid %u/%u, raw DEV_ID %u/%u\n", drv_ok, ID_READS, raw_ok, ID_READS);

    uint32_t orig = 0;
    raw_read32(0x0C, &orig);
    unsigned wr_total = 0, wr_bad = 0, spi_err = 0;
    for (unsigned r = 0; r < WR_ROUNDS; r++) {
        for (unsigned k = 0; k < npat; k++) {
            uint32_t back = 0;
            if (raw_write32(0x0C, pat[k]) != DWT_SUCCESS || raw_read32(0x0C, &back) != DWT_SUCCESS) {
                spi_err++;
            }
            wr_total++;
            if (back != pat[k]) {
                if (wr_bad < 5) {
                    P("  mismatch: wrote 0x%08X read 0x%08X\n", (unsigned)pat[k], (unsigned)back);
                }
                wr_bad++;
            }
        }
    }
    uint32_t restored = 0;
    raw_write32(0x0C, orig);
    raw_read32(0x0C, &restored);
    P("PANADR write/read %u/%u OK, SPI errors %u, original 0x%08X restored %s\n", wr_total - wr_bad, wr_total,
      spi_err, (unsigned)orig, restored == orig ? "ok" : "NG");

    int pass = drv_ok == ID_READS && raw_ok == ID_READS && wr_bad == 0 && spi_err == 0 && restored == orig;
    P("S: %s\n", pass ? "PASS" : "FAIL");
    dw3000_spi_speed_slow();
    return pass;
}

static int test_irq(void)
{
    dwt_callbacks_s cb;
    memset(&cb, 0, sizeof(cb));
    cb.cbSPIRdy = on_spirdy;
    dwt_setcallbacks(&cb);

    uint32_t st = 0, en = 0;
    raw_read32(0x44, &st);
    raw_read32(0x3C, &en);
    P("\n-- I) interrupt (SPI 2 MHz)\nbefore enabling: IRQ=%d SYS_STATUS=0x%08X SYS_ENABLE_LO=0x%08X\n", irq_level(),
      (unsigned)st, (unsigned)en);

    /* A) IRQ is already high (SPIRDY from init). GPIOTE senses a low-to-high edge,
     *    so enabling now gives no edge. Service it once by hand, as libdeca startup must. */
    if (dw3000_hw_init_interrupt() != 0) {
        stop("dw3000_hw_init_interrupt");
    }
    nrf_delay_ms(5);
    uint32_t a_irq_cb = spirdy_count;
    int a_level = irq_level();
    P("A: after enabling: IRQ=%d, SPIRDY callbacks from the interrupt=%u\n", a_level, (unsigned)a_irq_cb);
    if (a_level) {
        dwt_isr();
    }
    raw_read32(0x44, &st);
    int a_pass = irq_level() == 0 && spirdy_count == a_irq_cb + (a_level ? 1u : 0u) && (st & SPIRDY_RCINIT) == 0;
    P("A: after one dwt_isr(): IRQ=%d SYS_STATUS=0x%08X SPIRDY callbacks total=%u  %s\n", irq_level(),
      (unsigned)st, (unsigned)spirdy_count, a_pass ? "PASS" : "FAIL");

    /* B) real rising edges: hardware reset with the interrupt enabled */
    unsigned b_ok = 0;
    for (unsigned i = 1; i <= RESETS; i++) {
        uint32_t before = spirdy_count;
        dw3000_hw_reset();
        nrf_delay_ms(5);
        uint32_t delta = spirdy_count - before;
        raw_read32(0x44, &st);
        int ok = delta == 1 && irq_level() == 0 && (st & SPIRDY_RCINIT) == 0;
        b_ok += ok;
        P("#%02u SPIRDY+%u (status in cb 0x%08X) IRQ=%d STATUS=0x%08X %s\n", i, (unsigned)delta,
          (unsigned)spirdy_status, irq_level(), (unsigned)st, ok ? "OK" : "NG");
    }
    P("B: %u/%u resets serviced exactly once with IRQ low  %s\n", b_ok, RESETS, b_ok == RESETS ? "PASS" : "FAIL");
    dw3000_hw_interrupt_disable();
    return a_pass && b_ok == RESETS;
}

int main(void)
{
    SEGGER_RTT_Init();
    P("\n== T6 Lv1 A: DWM3001CDK SPI 8 MHz + interrupt (%s) ==\n", DRIVER_VERSION_STR);

    if (dw3000_hw_init() != 0) {
        stop("dw3000_hw_init");
    }
    dw3000_hw_reset();
    nrf_delay_ms(5);
    uint32_t st = 0;
    raw_read32(0x44, &st);
    if ((st & 0x01000000UL) == 0) {
        stop("RCINIT not set after reset");
    }
    if (dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf) != DWT_SUCCESS) {
        stop("dwt_probe");
    }
    if (dwt_initialise(DWT_DW_INIT) != DWT_SUCCESS) {
        stop("dwt_initialise");
    }
    P("probe/initialise OK, dwt_readdevid=0x%08X\n", (unsigned)dwt_readdevid());

    int s = test_fast_spi();
    int i = test_irq();
    P("\nverdict: %s (S %s, I %s)\n== done ==\n", (s && i) ? "PASS" : "FAIL", s ? "PASS" : "FAIL", i ? "PASS" : "FAIL");

    for (;;) {
        __WFE();
    }
}
