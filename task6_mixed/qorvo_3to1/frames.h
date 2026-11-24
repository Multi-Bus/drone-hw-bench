#ifndef UWB_3TO1_FRAMES_H
#define UWB_3TO1_FRAMES_H

#include <stdbool.h>
#include <stdint.h>

enum {
    UWB3_PAN = 0xDECA, UWB3_TAG = 0x0100, UWB3_ANCHORS = 3,
    UWB3_POLL = 0x21, UWB3_RESPONSE = 0x10, UWB3_FINAL = 0x23,
    UWB3_HEADER = 18, UWB3_FINAL_LEN = UWB3_HEADER + 12, UWB3_FCS = 2,
    UWB3_RESPONSE_DELAY = 900, UWB3_FINAL_DELAY = 700,
    UWB3_RESPONSE_RX_DELAY = 700, UWB3_FINAL_RX_DELAY = 500,
    UWB3_RESPONSE_TIMEOUT = 300, UWB3_FINAL_TIMEOUT = 220,
    UWB3_DTU_PER_UUS = 63898, UWB3_UNKNOWN_POWER = 32767,
};

struct uwb3_key { uint16_t anchor; uint32_t session; uint32_t round; };

static inline void uwb3_put32(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) {
        out[i] = (uint8_t)(value >> (8 * i));
    }
}

static inline uint32_t uwb3_get32(const uint8_t *in)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        value |= (uint32_t)in[i] << (8 * i);
    }
    return value;
}

static inline uint16_t uwb3_get16(const uint8_t *in)
{
    return (uint16_t)in[0] | ((uint16_t)in[1] << 8);
}

static inline void uwb3_encode(uint8_t *frame, uint8_t type, struct uwb3_key key)
{
    const uint16_t source = type == UWB3_RESPONSE ? UWB3_TAG : key.anchor;
    const uint16_t destination = type == UWB3_RESPONSE ? key.anchor : UWB3_TAG;
    frame[0] = 0x41;
    frame[1] = 0x88;
    frame[2] = (uint8_t)key.round;
    frame[3] = (uint8_t)UWB3_PAN;
    frame[4] = UWB3_PAN >> 8;
    frame[5] = (uint8_t)destination;
    frame[6] = destination >> 8;
    frame[7] = (uint8_t)source;
    frame[8] = source >> 8;
    frame[9] = type;
    uwb3_put32(frame + 10, key.session);
    uwb3_put32(frame + 14, key.round);
}

/* Length is the received PSDU length, including its two-byte FCS. */
static inline bool uwb3_decode(const uint8_t *frame, uint16_t length,
                               uint8_t type, struct uwb3_key *key)
{
    const bool response = type == UWB3_RESPONSE;
    const uint16_t expected = (type == UWB3_FINAL ? UWB3_FINAL_LEN : UWB3_HEADER) + UWB3_FCS;
    if (length != expected || frame[0] != 0x41 || frame[1] != 0x88 ||
        uwb3_get16(frame + 3) != UWB3_PAN || frame[9] != type ||
        uwb3_get16(frame + (response ? 7 : 5)) != UWB3_TAG) {
        return false;
    }
    struct uwb3_key candidate = { uwb3_get16(frame + (response ? 5 : 7)),
                                  uwb3_get32(frame + 10), uwb3_get32(frame + 14) };
    if (candidate.anchor < 1 || candidate.anchor > UWB3_ANCHORS ||
        candidate.session == 0 || candidate.round == 0 || frame[2] != (uint8_t)candidate.round) {
        return false;
    }
    *key = candidate;
    return true;
}

static inline bool uwb3_same_key(struct uwb3_key left, struct uwb3_key right)
{
    return left.anchor == right.anchor && left.session == right.session && left.round == right.round;
}

/* Unsigned subtraction handles rollover of the low 32 bits of the DW clock. */
static inline bool uwb3_distance_mm(const uint8_t *final, uint32_t poll_rx,
                                    uint32_t response_tx, uint32_t final_rx, int32_t *mm)
{
    const uint32_t poll_tx = uwb3_get32(final + UWB3_HEADER);
    const uint32_t response_rx = uwb3_get32(final + UWB3_HEADER + 4);
    const uint32_t final_tx = uwb3_get32(final + UWB3_HEADER + 8);
    const uint32_t ra = response_rx - poll_tx;
    const uint32_t rb = final_rx - response_tx;
    const uint32_t da = final_tx - response_rx;
    const uint32_t db = response_tx - poll_rx;
    const uint32_t limit = 3000U * UWB3_DTU_PER_UUS;
    if (ra == 0 || rb == 0 || da == 0 || db == 0 || ra > limit || rb > limit || da > limit || db > limit) {
        return false;
    }
    const double dtu = ((double)ra * rb - (double)da * db) / ((double)ra + rb + da + db);
    *mm = (int32_t)(dtu * (299702547000.0 / (499200000.0 * 128.0)));
    return true;
}

#endif
