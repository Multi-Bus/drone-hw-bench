#include <inttypes.h>
#include <stdio.h>
#include <config_options.h>
#include <deca_spi.h>
#include <port.h>
#include "deca_probe_interface.h"
#include "radio.h"
#include "bench_config.h"

enum { MAX_PAYLOAD = 40, FRAME_CAPACITY = UWB3_HEADER + MAX_PAYLOAD + UWB3_FCS };
enum { OK, POLL_TX_FAILED, RESPONSE_MISSING, RESPONSE_INVALID, FINAL_TX_FAILED,
       RESPONSE_TX_FAILED, FINAL_MISSING, FINAL_INVALID };
static const uint64_t clock_mask = (UINT64_C(1) << 40) - 1;
static const uint16_t peers[] = BENCH_PEERS;
static const dwt_config_t phy = {
    BENCH_CHANNEL, DWT_PLEN_128, DWT_PAC8, 9, 9, DWT_SFD_DW_8,
    DWT_BR_6M8, DWT_PHRMODE_STD, DWT_PHRRATE_STD, 129,
    DWT_STS_MODE_OFF, DWT_STS_LEN_64, DWT_PDOA_M0
};
extern dwt_txconfig_t txconfig_options;
extern dwt_txconfig_t txconfig_options_ch9;

struct header {
    uint16_t source, destination;
    uint32_t session, round;
};

static uint32_t wait_events(uint32_t mask, uint32_t timeout_ms)
{
    const uint32_t start = portGetTickCnt();
    uint32_t status;
    do {
        status = dwt_readsysstatuslo();
    } while (!(status & mask) && portGetTickCnt() - start < timeout_ms);
    return status;
}

static void idle_radio(void)
{
    dwt_forcetrxoff();
    dwt_writesysstatuslo(UWB3_ATTEMPT_EVENTS);
}

static void encode(uint8_t *frame, uint8_t type, struct header h)
{
    const struct uwb3_key key = { h.source, h.session, h.round };
    uwb3_encode(frame, type, key);
    frame[5] = (uint8_t)h.destination;
    frame[6] = (uint8_t)(h.destination >> 8);
    frame[7] = (uint8_t)h.source;
    frame[8] = (uint8_t)(h.source >> 8);
}

static bool receive(uint8_t *frame, uint8_t type, unsigned body, struct header *h)
{
    uint8_t ranging_flag = 0;
    const uint16_t length = dwt_getframelength(&ranging_flag);
    if (!ranging_flag || length != UWB3_HEADER + body + UWB3_FCS) {
        return false;
    }
    dwt_readrxdata(frame, length, 0);
    *h = (struct header){ uwb3_get16(frame + 7), uwb3_get16(frame + 5),
                          uwb3_get32(frame + 10), uwb3_get32(frame + 14) };
    return frame[0] == 0x41 && frame[1] == 0x88 && frame[9] == type &&
           uwb3_get16(frame + 3) == UWB3_PAN && h->destination == BENCH_ADDRESS &&
           h->source != 0 && h->source != UINT16_MAX && h->source != BENCH_ADDRESS &&
           h->session && h->round && frame[2] == (uint8_t)h->round;
}

static bool reply_to(struct header incoming, struct header outgoing)
{
    return incoming.source == outgoing.destination && incoming.session == outgoing.session &&
           incoming.round == outgoing.round;
}

static uint32_t delayed_at(uint64_t timestamp, uint32_t uus)
{
    return (uint32_t)((timestamp + (uint64_t)uus * UWB3_DTU_PER_UUS) >> 8);
}

static void log_round(struct header request, unsigned result, uint64_t poll, uint64_t final,
                      uint32_t status, uint32_t started_ms)
{
    char line[144];
    snprintf(line, sizeof(line), "DWB,%c,%" PRIu32 ",%" PRIu32 ",%u,%u,%u,%u,%010" PRIx64
             ",%010" PRIx64 ",%08" PRIx32 ",%" PRIu32 ",%" PRIu32,
             BENCH_INITIATOR ? 'I' : 'R', request.session, request.round,
             (unsigned)request.source, (unsigned)request.destination, (unsigned)BENCH_PAYLOAD,
             result, poll & clock_mask, final & clock_mask, status, started_ms, portGetTickCnt());
    test_run_info((unsigned char *)line);
}

static void initiate(struct header request, uint32_t started_ms)
{
    uint8_t frame[FRAME_CAPACITY];
    struct header response;
    uint64_t poll = 0, final = 0;
    uint32_t status = 0;
    unsigned result = POLL_TX_FAILED;
    idle_radio();
    dwt_setrxaftertxdelay(BENCH_RESPONSE_RX_DELAY);
    dwt_setrxtimeout(UWB3_RESPONSE_TIMEOUT);
    encode(frame, UWB3_POLL, request);
    if (uwb3_send(frame, UWB3_HEADER, DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED) != DWT_SUCCESS) {
        goto done;
    }
    status = wait_events(UWB3_RX_EVENTS, 10);
    if (!(status & DWT_INT_TXFRS_BIT_MASK)) {
        goto done;
    }
    poll = uwb3_timestamp(true);
    result = RESPONSE_MISSING;
    if (!(status & DWT_INT_RXFCG_BIT_MASK)) {
        goto done;
    }
    const uint64_t response_rx = uwb3_timestamp(false);
    result = RESPONSE_INVALID;
    if (!receive(frame, UWB3_RESPONSE, BENCH_PAYLOAD, &response) || !reply_to(response, request)) {
        goto done;
    }
    for (unsigned i = 0; i != BENCH_PAYLOAD; ++i) {
        if (frame[UWB3_HEADER + i] != i) {
            goto done;
        }
    }
    dwt_writesysstatuslo(UWB3_ATTEMPT_EVENTS);
    const uint32_t delayed = delayed_at(response_rx, BENCH_FINAL_DELAY);
    dwt_setdelayedtrxtime(delayed);
    encode(frame, UWB3_FINAL, request);
    uwb3_put32(frame + UWB3_HEADER, (uint32_t)poll);
    uwb3_put32(frame + UWB3_HEADER + 4, (uint32_t)response_rx);
    uwb3_put32(frame + UWB3_HEADER + 8, ((delayed & ~1U) << 8) + BENCH_ANTENNA_DELAY);
    result = FINAL_TX_FAILED;
    status = 0;
    if (uwb3_send(frame, UWB3_FINAL_LEN, DWT_START_TX_DELAYED) == DWT_SUCCESS) {
        status = wait_events(DWT_INT_TXFRS_BIT_MASK, 10);
        if (status & DWT_INT_TXFRS_BIT_MASK) {
            final = uwb3_timestamp(true);
            result = OK;
        }
    }
done:
    idle_radio();
    log_round(request, result, poll, final, status, started_ms);
}

static void respond(struct header request, uint64_t poll)
{
    const uint32_t started_ms = portGetTickCnt();
    uint8_t frame[FRAME_CAPACITY];
    struct header final_header;
    uint64_t final = 0;
    uint32_t status = 0;
    unsigned result = RESPONSE_TX_FAILED;
    const struct header response = { BENCH_ADDRESS, request.source, request.session, request.round };
    dwt_writesysstatuslo(UWB3_ATTEMPT_EVENTS);
    dwt_setdelayedtrxtime(delayed_at(poll, BENCH_RESPONSE_DELAY));
    dwt_setrxaftertxdelay(BENCH_FINAL_RX_DELAY);
    dwt_setrxtimeout(UWB3_FINAL_TIMEOUT);
    encode(frame, UWB3_RESPONSE, response);
    for (unsigned i = 0; i != BENCH_PAYLOAD; ++i) {
        frame[UWB3_HEADER + i] = (uint8_t)i;
    }
    if (uwb3_send(frame, UWB3_HEADER + BENCH_PAYLOAD, DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_SUCCESS) {
        status = wait_events(UWB3_RX_EVENTS, 10);
        if (status & DWT_INT_TXFRS_BIT_MASK) {
            result = FINAL_MISSING;
            if (status & DWT_INT_RXFCG_BIT_MASK) {
                result = FINAL_INVALID;
                if (receive(frame, UWB3_FINAL, 12, &final_header) && reply_to(final_header, response)) {
                    final = uwb3_timestamp(false);
                    result = OK;
                }
            }
        }
    }
    idle_radio();
    log_round(request, result, poll, final, status, started_ms);
}

/* Installed in the handoff responder example for both roles. */
int ds_twr_responder(void)
{
    reset_DWIC();
    Sleep(2);
    dwt_config_t config = phy;
    if (dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf) != DWT_SUCCESS ||
        !dwt_checkidlerc() || dwt_initialise(DWT_READ_OTP_ALL) != DWT_SUCCESS ||
        dwt_configure(&config) != DWT_SUCCESS) {
        test_run_info((unsigned char *)"DWB_INIT_FAILED");
        return -1;
    }
    port_set_dw_ic_spi_fastrate();
    dwt_configuretxrf(BENCH_CHANNEL == 5 ? &txconfig_options : &txconfig_options_ch9);
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);
    dwt_settxantennadelay(BENCH_ANTENNA_DELAY);
    dwt_setrxantennadelay(BENCH_ANTENNA_DELAY);
    dwt_setpreambledetecttimeout(0);
    char line[144];
    snprintf(line, sizeof(line), "DWB_READY,%c,%u,%u,%u,%u,%u,%u,%u,%u,%u",
             BENCH_INITIATOR ? 'I' : 'R', BENCH_ADDRESS, BENCH_CHANNEL, BENCH_PAYLOAD,
             BENCH_PERIOD_MS, BENCH_RESPONSE_DELAY, BENCH_FINAL_DELAY,
             BENCH_RESPONSE_RX_DELAY, BENCH_FINAL_RX_DELAY, BENCH_ROUNDS);
    test_run_info((unsigned char *)line);

    if (BENCH_INITIATOR) {
        Sleep(2000);
        for (uint32_t round = 1; round <= BENCH_ROUNDS; ++round) {
            const uint32_t start = portGetTickCnt();
            const struct header request = { BENCH_ADDRESS, peers[(round - 1) % (sizeof(peers) / sizeof(peers[0]))],
                                            BENCH_SESSION, round };
            initiate(request, start);
            while (portGetTickCnt() - start < BENCH_PERIOD_MS) { }
        }
        test_run_info((unsigned char *)"DWB_DONE");
        return 0;
    }
    for (;;) {
        uint8_t frame[FRAME_CAPACITY];
        struct header request;
        idle_radio();
        dwt_setrxtimeout(0);
        if (dwt_rxenable(DWT_START_RX_IMMEDIATE) == DWT_SUCCESS &&
            (wait_events(UWB3_RX_EVENTS, 1000) & DWT_INT_RXFCG_BIT_MASK) &&
            receive(frame, UWB3_POLL, 0, &request)) {
            respond(request, uwb3_timestamp(false));
        }
    }
}
