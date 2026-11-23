#include <assert.h>
#include <string.h>
#include "frames.h"

int main(void)
{
    uint8_t frame[UWB3_FINAL_LEN + UWB3_FCS] = { 0 };
    const struct uwb3_key key = { 3, 0x12345678, 257 };
    struct uwb3_key decoded;
    uwb3_encode(frame, UWB3_POLL, key);
    assert(uwb3_decode(frame, UWB3_HEADER + UWB3_FCS, UWB3_POLL, &decoded));
    assert(uwb3_same_key(key, decoded));
    assert(!uwb3_decode(frame, UWB3_HEADER, UWB3_POLL, &decoded));
    frame[7] = 4;
    assert(!uwb3_decode(frame, UWB3_HEADER + UWB3_FCS, UWB3_POLL, &decoded));

    uwb3_encode(frame, UWB3_RESPONSE, key);
    assert(uwb3_decode(frame, UWB3_HEADER + UWB3_FCS, UWB3_RESPONSE, &decoded));
    assert(!uwb3_same_key((struct uwb3_key){ 1, key.session, key.round }, decoded));
    assert(!uwb3_same_key((struct uwb3_key){ 3, key.session, key.round - 1 }, decoded));
    frame[2] ^= 1;
    assert(!uwb3_decode(frame, UWB3_HEADER + UWB3_FCS, UWB3_RESPONSE, &decoded));

    const uint32_t poll_tx = UINT32_MAX - 80U, tof = 640U;
    const uint32_t poll_rx = poll_tx + tof, response_tx = poll_rx + 900U * UWB3_DTU_PER_UUS;
    const uint32_t response_rx = response_tx + tof, final_tx = response_rx + 700U * UWB3_DTU_PER_UUS;
    uwb3_encode(frame, UWB3_FINAL, key);
    uwb3_put32(frame + UWB3_HEADER, poll_tx);
    uwb3_put32(frame + UWB3_HEADER + 4, response_rx);
    uwb3_put32(frame + UWB3_HEADER + 8, final_tx);
    int32_t mm;
    assert(uwb3_decode(frame, sizeof(frame), UWB3_FINAL, &decoded));
    assert(uwb3_distance_mm(frame, poll_rx, response_tx, final_tx + tof, &mm));
    assert(mm >= 2990 && mm <= 3010);
    memset(frame + UWB3_HEADER, 0, 12);
    assert(!uwb3_distance_mm(frame, poll_rx, response_tx, final_tx + tof, &mm));
    return 0;
}
