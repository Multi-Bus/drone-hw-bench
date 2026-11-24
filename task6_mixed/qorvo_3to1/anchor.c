#include <config_options.h>
#include "deca_probe_interface.h"
#include "port.h"
#include "radio.h"

#ifndef UWB3_ANCHOR_ID
#error Set UWB3_ANCHOR_ID to the address of this board.
#endif

enum { MAILBOX_MAGIC = 0x55574233, ACK_READY = 1, ACK_INIT_FAILED = 2 };
enum { SENT_POLL = 1, RECEIVED_RESPONSE = 2, SENT_FINAL = 4 };

/* Word order is the SWD interface used by capture.py. The host writes token last. */
struct anchor_mailbox {
    uint32_t magic, anchor, state, request_session, request_round, request_token;
    uint32_t done_token, done_session, done_round, stages, response_status, final_status;
    int32_t response_rssi, response_first_path;
};

volatile struct anchor_mailbox mailbox __attribute__((section(".bench"), used));
extern dwt_txconfig_t txconfig_options;
extern dwt_config_t config_options;
extern uint32_t cdk_time_us(void);
void port_set_dw_ic_spi_fastrate(void);

static uint32_t wait_events(uint32_t mask)
{
    const uint32_t start = cdk_time_us();
    uint32_t status;
    do {
        status = dwt_readsysstatuslo();
    } while ((status & mask) == 0 && cdk_time_us() - start < 10000U);
    return status;
}

static void range(struct uwb3_key key)
{
    uint8_t frame[UWB3_FINAL_LEN + UWB3_FCS];
    struct uwb3_key response;
    dwt_writesysstatuslo(UWB3_ATTEMPT_EVENTS);
    uwb3_encode(frame, UWB3_POLL, key);
    if (uwb3_send(frame, UWB3_HEADER, DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED) != DWT_SUCCESS) {
        return;
    }

    const uint32_t status = wait_events(UWB3_RX_EVENTS);
    mailbox.response_status = status;
    if (status & DWT_INT_TXFRS_BIT_MASK) {
        mailbox.stages |= SENT_POLL;
    }
    if (!(status & DWT_INT_RXFCG_BIT_MASK) ||
        !uwb3_decode(frame, uwb3_read(frame), UWB3_RESPONSE, &response) ||
        !uwb3_same_key(key, response)) {
        return;
    }
    mailbox.stages |= RECEIVED_RESPONSE;
    dwt_writesysstatuslo(UWB3_ATTEMPT_EVENTS);
    const uint64_t poll_tx = uwb3_timestamp(true);
    const uint64_t response_rx = uwb3_timestamp(false);
    const uint32_t delayed = (uint32_t)((response_rx + (uint64_t)UWB3_FINAL_DELAY * UWB3_DTU_PER_UUS) >> 8);
    dwt_setdelayedtrxtime(delayed);
    uwb3_encode(frame, UWB3_FINAL, key);
    uwb3_put32(frame + UWB3_HEADER, (uint32_t)poll_tx);
    uwb3_put32(frame + UWB3_HEADER + 4, (uint32_t)response_rx);
    uwb3_put32(frame + UWB3_HEADER + 8, ((delayed & ~1U) << 8) + UWB3_ANTENNA_DELAY);
    const int sent = uwb3_send(frame, UWB3_FINAL_LEN, DWT_START_TX_DELAYED);
    const struct uwb3_power power = uwb3_read_power();
    mailbox.response_rssi = power.rssi;
    mailbox.response_first_path = power.first_path;
    if (sent == DWT_SUCCESS) {
        mailbox.final_status = wait_events(DWT_INT_TXFRS_BIT_MASK);
        if (mailbox.final_status & DWT_INT_TXFRS_BIT_MASK) {
            mailbox.stages |= SENT_FINAL;
        }
    }
}

void uwb3_anchor(void)
{
    mailbox = (struct anchor_mailbox){ .magic = MAILBOX_MAGIC, .anchor = UWB3_ANCHOR_ID };
    if (dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf) != DWT_SUCCESS ||
        !dwt_checkidlerc() || dwt_initialise(DWT_READ_OTP_ALL) != DWT_SUCCESS ||
        dwt_configure(&config_options) != DWT_SUCCESS) {
        mailbox.state = ACK_INIT_FAILED;
        return;
    }
    port_set_dw_ic_spi_fastrate();
    dwt_configuretxrf(&txconfig_options);
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);
    dwt_settxantennadelay(UWB3_ANTENNA_DELAY);
    dwt_setrxantennadelay(UWB3_ANTENNA_DELAY);
    dwt_configciadiag(DW_CIA_DIAG_LOG_ALL);
    dwt_setrxaftertxdelay(UWB3_RESPONSE_RX_DELAY);
    dwt_setrxtimeout(UWB3_RESPONSE_TIMEOUT);
    dwt_setpreambledetecttimeout(0);
    mailbox.state = ACK_READY;

    for (;;) {
        const uint32_t token = mailbox.request_token;
        if (token == 0 || token == mailbox.done_token) {
            continue;
        }
        const struct uwb3_key key = { UWB3_ANCHOR_ID, mailbox.request_session, mailbox.request_round };
        mailbox.stages = mailbox.response_status = mailbox.final_status = 0;
        mailbox.response_rssi = mailbox.response_first_path = UWB3_UNKNOWN_POWER;
        if (key.session && key.round) {
            range(key);
            dwt_forcetrxoff();
            dwt_writesysstatuslo(UWB3_ATTEMPT_EVENTS);
        }
        mailbox.done_session = key.session;
        mailbox.done_round = key.round;
        mailbox.done_token = token;
    }
}
