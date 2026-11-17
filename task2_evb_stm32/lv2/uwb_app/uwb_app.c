#include "uwb_app.h"

#include <stddef.h>

#include "deca_device_api.h"
#include "dw3000_hw.h"
#include "dw3000_spi.h"
#include "uwb_port.h"
#include "stm32n6xx_hal.h"
#include "dwhw.h"
#include "dwmac.h"
#include "dwphy.h"
#include "dwproto.h"
#include "ranging.h"

#define UWB_PAN_ID               0xDECAu
#define UWB_PROCESSING_DELAY_US  5000u
#ifndef UWB_ANTENNA_DELAY
#define UWB_ANTENNA_DELAY DWPHY_ANTENNA_DELAY
#endif

/* One-slot result handoff from the TWR callback to the application loop.
 * A subsequent result can overwrite an unread result. Drain frequently. */
static volatile uint32_t s_result_packed;
static volatile uint8_t s_result_pending;
static bool s_is_responder;

static void twr_done_callback(uint64_t src, uint64_t dst,
                              uint16_t distance_cm, uint16_t sequence)
{
    (void)src;
    (void)dst;

    /* libdeca reserves its top two values for status/failure. */
    if (distance_cm == TWR_OK_VALUE) {
        return;
    }

    /* Do not printf here: the callback can be invoked during IRQ handling. */
    s_result_packed = ((uint32_t)sequence << 16) | distance_cm;
    s_result_pending = 1u;
}

bool uwb_app_init(uint16_t my_address, bool responder)
{
    s_is_responder = responder;
    s_result_pending = 0u;

    dw3000_hw_interrupt_disable();
    dw3000_hw_isr_hook = NULL;

    if (dw3000_hw_init() != 0) {
        return false;
    }
    dw3000_spi_speed_slow();
    dw3000_hw_reset();
    uint32_t started = HAL_GetTick();
    uint32_t status = 0, enabled = 0;
    do {
        if (uwb_port_read_status(&status, &enabled) != UWB_OK) {
            return false;
        }
        if (HAL_GetTick() - started >= 200u) {
            return false;
        }
    } while ((status & (1UL << 24)) == 0u);

    if (!dwhw_init()) {
        return false;
    }
    if (!dwphy_config()) {
        return false;
    }

    /* Placeholder from libdeca; calibrate antenna delay using measured data. */
    dwphy_set_antenna_delay(UWB_ANTENNA_DELAY);

    if (!dwmac_init(UWB_PAN_ID, my_address,
                    dwprot_rx_handler, NULL, NULL)) {
        return false;
    }
    dwmac_set_frame_filter();
    /* The hardware address filter drops frames addressed to another device (for
     * example a poll to a second anchor). Without the filter-error interrupt (ARFE)
     * the chip then goes idle and raises no event at all, so the receiver is never
     * re-armed and an anchor stops answering for good after overhearing one such
     * frame. With ARFE enabled libdeca's error callback re-arms RX
     * (dwmac_set_rx_reenable). libdeca enables this bit only in a "debug" build
     * (CONFIG_DECA_DEBUG_FRAME_FILTER), so enable it explicitly. */
    dwt_setinterrupt(DWT_INT_ARFE_BIT_MASK, 0, DWT_ENABLE_INT);
    twr_init(UWB_PROCESSING_DELAY_US, true);
    twr_set_observer(twr_done_callback);

    dw3000_hw_isr_hook = dwt_isr;
    if (dw3000_hw_init_interrupt() != 0) {
        dw3000_hw_interrupt_disable();
        dw3000_hw_isr_hook = NULL;
        return false;
    }

    if (s_is_responder) {
        dwmac_set_rx_reenable(true);
        dwt_forcetrxoff();
        if (dwt_rxenable(DWT_START_RX_IMMEDIATE) != DWT_SUCCESS) {
            dw3000_hw_interrupt_disable();
            dw3000_hw_isr_hook = NULL;
            return false;
        }
    }

    return true;
}

bool uwb_app_start_ds_twr(uint16_t peer_address)
{
    if (s_is_responder || twr_in_progress()) {
        return false;
    }
    return twr_start((uint64_t)peer_address);
}

bool uwb_app_take_result(uwb_range_result_t *result)
{
    if (result == NULL || s_result_pending == 0u) {
        return false;
    }

    /* Single-core initial integration: call this in the main loop.
     * If callbacks can preempt this read, protect it with a short critical
     * section supplied by the STM32 platform layer for lossless logging. */
    uint32_t packed = s_result_packed;
    s_result_pending = 0u;
    result->sequence = (uint16_t)(packed >> 16);
    result->distance_cm = (uint16_t)(packed & 0xFFFFu);
    return true;
}
