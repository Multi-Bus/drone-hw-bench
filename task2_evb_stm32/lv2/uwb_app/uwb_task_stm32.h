#ifndef TASK2_UWB_TASK_STM32_H
#define TASK2_UWB_TASK_STM32_H

#include <stdint.h>

/* Bare-metal STM32 event dispatcher for br101/libdeca.
 * Queue events in the DW3000 IRQ, process them in the main loop. */

/* Drain up to max_events in thread context; 0 -> at most 16. Never call in ISR. */
uint32_t uwb_task_poll(uint32_t max_events);

/* Queue overflow invalidates ranging measurements. Should stay 0. */
uint32_t uwb_task_dropped(void);

#endif
