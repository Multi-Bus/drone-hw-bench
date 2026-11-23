#include <stdio.h>
#include <config_options.h>
#include <deca_spi.h>
#include <port.h>
#include "deca_probe_interface.h"
#include "radio.h"
#include "tag_antenna_delay.h"

extern dwt_txconfig_t txconfig_options;
extern dwt_config_t config_options;

static uint32_t wait_events(uint32_t max_ms)
{
    const uint32_t start = portGetTickCnt();
    uint32_t status;
    do {
        status = dwt_readsysstatuslo();
    } while ((status & UWB3_RX_EVENTS) == 0 &&
             (max_ms == 0 || portGetTickCnt() - start < max_ms));
    return status;
}

static void respond(struct uwb3_key key, uint32_t poll_status)
{
    uint8_t frame[UWB3_FINAL_LEN + UWB3_FCS];
    struct uwb3_key final;
    struct uwb3_power poll_power = { UWB3_UNKNOWN_POWER, UWB3_UNKNOWN_POWER };
    struct uwb3_power final_power = poll_power;
    uint32_t final_status = 0;
    int32_t mm = 0;
    unsigned response_sent = 0, final_good = 0, distance_valid = 0;
    const uint64_t poll_rx = uwb3_timestamp(false);
    dwt_writesysstatuslo(UWB3_ATTEMPT_EVENTS);
    dwt_setdelayedtrxtime((uint32_t)((poll_rx + (uint64_t)UWB3_RESPONSE_DELAY * UWB3_DTU_PER_UUS) >> 8));
    dwt_setrxaftertxdelay(UWB3_FINAL_RX_DELAY);
    dwt_setrxtimeout(UWB3_FINAL_TIMEOUT);
    uwb3_encode(frame, UWB3_RESPONSE, key);
    const int sent = uwb3_send(frame, UWB3_HEADER, DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED);
    poll_power = uwb3_read_power();
    if (sent == DWT_SUCCESS) {
        final_status = wait_events(10);
        response_sent = (final_status & DWT_INT_TXFRS_BIT_MASK) != 0;
        if ((final_status & DWT_INT_RXFCG_BIT_MASK) &&
            uwb3_decode(frame, uwb3_read(frame), UWB3_FINAL, &final) && uwb3_same_key(key, final)) {
            final_good = 1;
            const uint32_t response_tx = (uint32_t)uwb3_timestamp(true);
            const uint32_t final_rx = (uint32_t)uwb3_timestamp(false);
            distance_valid = uwb3_distance_mm(frame, (uint32_t)poll_rx, response_tx, final_rx, &mm);
            final_power = uwb3_read_power();
        }
    }
    dwt_forcetrxoff();
    dwt_writesysstatuslo(UWB3_ATTEMPT_EVENTS);
    char line[180];
    snprintf(line, sizeof(line), "TWR3,%lu,%lu,%u,%u,%u,%u,%ld,%d,%d,%d,%d,%08lX,%08lX",
             (unsigned long)key.session, (unsigned long)key.round, (unsigned)key.anchor,
             response_sent, final_good, distance_valid, (long)mm,
             poll_power.rssi, poll_power.first_path, final_power.rssi, final_power.first_path,
             (unsigned long)poll_status, (unsigned long)final_status);
    test_run_info((unsigned char *)line);
}

/* This entry is called by the handoff project's TEST_DS_TWR_RESPONDER selection. */
int ds_twr_responder(void)
{
    reset_DWIC();
    Sleep(2);
    if (dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf) != DWT_SUCCESS ||
        !dwt_checkidlerc() || dwt_initialise(DWT_READ_OTP_ALL) != DWT_SUCCESS ||
        dwt_configure(&config_options) != DWT_SUCCESS) {
        test_run_info((unsigned char *)"TWR3_INIT_FAILED");
        for (;;) { }
    }
    port_set_dw_ic_spi_fastrate();
    dwt_configuretxrf(&txconfig_options);
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);
    dwt_settxantennadelay(UWB3_ANTENNA_DELAY);
    dwt_setrxantennadelay(UWB3_ANTENNA_DELAY);
    dwt_configciadiag(DW_CIA_DIAG_LOG_ALL);
    dwt_setpreambledetecttimeout(0);
    test_run_info((unsigned char *)"TWR3_READY,channel=5,tag=256");

    for (;;) {
        struct uwb3_key key;
        uint8_t frame[UWB3_FINAL_LEN + UWB3_FCS];
        dwt_setrxtimeout(0);
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
        const uint32_t status = wait_events(0);
        if ((status & DWT_INT_RXFCG_BIT_MASK) && uwb3_decode(frame, uwb3_read(frame), UWB3_POLL, &key)) {
            respond(key, status);
        }
        dwt_writesysstatuslo(UWB3_ATTEMPT_EVENTS);
    }
}
