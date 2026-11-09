#ifndef TASK2_UWB_APP_H
#define TASK2_UWB_APP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t sequence;
    uint16_t distance_cm;
} uwb_range_result_t;

/* Initialize the radio and libdeca stack. Call after STM32 hardware setup. */
bool uwb_app_init(uint16_t my_address, bool responder);

/* Start one double-sided TWR exchange. For initiator only. */
bool uwb_app_start_ds_twr(uint16_t peer_address);

/* Read the most recent completed measurement outside IRQ context. */
bool uwb_app_take_result(uwb_range_result_t *result);

#endif
