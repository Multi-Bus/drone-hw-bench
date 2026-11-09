#include "uwb_app.h"

#include <stddef.h>

#include "deca_device_api.h"
#include "dw3000_hw.h"
#include "dwhw.h"
#include "dwmac.h"
#include "dwphy.h"
#include "dwproto.h"
#include "ranging.h"

#define UWB_PAN_ID               0xDECAu
#define UWB_PROCESSING_DELAY_US  2000u

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
    if (distance_cm >= TWR_OK_VALUE) {
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

    if (dw3000_hw_init() != 0) {
        return false;
    }
    dw3000_hw_reset();
    if (dw3000_hw_init_interrupt() != 0) {
        return false;
    }

    if (!dwhw_init()) {
        return false;
    }
    if (!dwphy_config()) {
        return false;
    }

    /* Placeholder from libdeca; calibrate antenna delay using measured data. */
    dwphy_set_antenna_delay(DWPHY_ANTENNA_DELAY);

    if (!dwmac_init(UWB_PAN_ID, my_address,
                    dwprot_rx_handler, NULL, NULL)) {
        return false;
    }
    dwmac_set_frame_filter();
    twr_init(UWB_PROCESSING_DELAY_US, true);
    twr_set_observer(twr_done_callback);

    if (s_is_responder) {
        dwmac_set_rx_reenable(true);
        dwt_forcetrxoff();
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
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
