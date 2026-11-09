/* STM32N6 bare-metal deferred event dispatcher for br101/libdeca.
 * Original integration code, not Qorvo driver source.
 *
 * libdeca's dwmac_irq.c calls dwtask_queue_event() from dwt_isr().
 * Do not run the TWR protocol handler inside the hardware interrupt.
 * RX frames and status words are COPIED: libdeca reuses its buffers.
 */
#include "uwb_task_stm32.h"

#include <stdint.h>
#include <string.h>

#include "stm32n6xx_hal.h"
#include "dwmac.h"
#include "platform/dwmac_task.h"

#define UWB_TASK_QUEUE_CAPACITY 16u

typedef struct {
    enum dwevent_e type;
    union {
        struct rxbuf rx;
        uint32_t status;
    } data;
} uwb_task_event_t;

static uwb_task_event_t s_queue[UWB_TASK_QUEUE_CAPACITY];
static volatile uint8_t s_head;
static volatile uint8_t s_tail;
static volatile uint8_t s_count;
static volatile uint32_t s_dropped;

static uint32_t lock_irq(void)
{
    uint32_t previous = __get_PRIMASK();
    __disable_irq();
    return previous;
}

static void unlock_irq(uint32_t previous)
{
    __set_PRIMASK(previous);
}

int dwtask_init(void)
{
    uint32_t previous = lock_irq();
    s_head = 0u;
    s_tail = 0u;
    s_count = 0u;
    s_dropped = 0u;
    unlock_irq(previous);
    return 0;
}

int dwtask_queue_event(enum dwevent_e type, const void *data)
{
    if (type != DWEVT_RX && type != DWEVT_RX_TIMEOUT &&
        type != DWEVT_TX_DONE && type != DWEVT_ERR) {
        return -1;
    }
    if (type != DWEVT_TX_DONE && data == NULL) {
        return -1;
    }

    uint32_t previous = lock_irq();
    if (s_count >= UWB_TASK_QUEUE_CAPACITY) {
        ++s_dropped;
        unlock_irq(previous);
        return -1;
    }

    uwb_task_event_t *slot = &s_queue[s_head];
    slot->type = type;
    if (type == DWEVT_RX) {
        memcpy(&slot->data.rx, data, sizeof(struct rxbuf));
    } else if (type != DWEVT_TX_DONE) {
        memcpy(&slot->data.status, data, sizeof(uint32_t));
    }

    s_head = (uint8_t)((s_head + 1u) % UWB_TASK_QUEUE_CAPACITY);
    ++s_count;
    unlock_irq(previous);
    return 0;
}

uint32_t uwb_task_poll(uint32_t max_events)
{
    if (max_events == 0u || max_events > UWB_TASK_QUEUE_CAPACITY) {
        max_events = UWB_TASK_QUEUE_CAPACITY;
    }

    uint32_t processed = 0u;
    while (processed < max_events) {
        uwb_task_event_t event;
        uint32_t previous = lock_irq();
        if (s_count == 0u) {
            unlock_irq(previous);
            break;
        }
        memcpy(&event, &s_queue[s_tail], sizeof(event));
        s_tail = (uint8_t)((s_tail + 1u) % UWB_TASK_QUEUE_CAPACITY);
        --s_count;
        unlock_irq(previous);

        switch (event.type) {
        case DWEVT_RX:
            dwmac_handle_rx_frame(&event.data.rx);
            break;
        case DWEVT_RX_TIMEOUT:
            dwmac_handle_rx_timeout(event.data.status);
            break;
        case DWEVT_TX_DONE:
            dwmac_handle_tx_done();
            break;
        case DWEVT_ERR:
            dwmac_handle_error(event.data.status);
            break;
        default:
            break;
        }
        ++processed;
    }
    return processed;
}

uint32_t uwb_task_dropped(void)
{
    uint32_t previous = lock_irq();
    uint32_t lost = s_dropped;
    unlock_irq(previous);
    return lost;
}
