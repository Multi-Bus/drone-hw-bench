# Task 2 Lv2 — STM32N6 bare-metal libdeca event port

**Status:** Source written; native/STM32 hardware build and TWR NOT yet verified.
This is application glue. It is not the licensed Qorvo driver.

## What is included
- `uwb_task_stm32.c/.h`: Implements `dwtask_init()`, `dwtask_queue_event()`,
  `uwb_task_poll()` and `uwb_task_dropped()` for bare-metal STM32N6.
- `log.h`: Minimal log shim, deliberately silent by default. Print measurement
  rows in the main loop rather than in a DW3000 IRQ callback.

## Usage
After validating your SPI port and integrating Decadriver 08.02.02,
libdeca, hardware IRQ/EXTI, `dw3000_hw_*`, `dw3000_spi_*`, `deca_port`
and the `dwt_probe` structure, initialize with:

```c
#include "uwb_app.h"
#include "uwb_task_stm32.h"
#include "ranging.h"

if (!uwb_app_init(0x0002u, false)) {
    printf("UWB init failed\r\n");
    Error_Handler();
}

uint32_t last_ms = 0;
for (;;) {
    (void)uwb_task_poll(8u); /* MUST be in main loop, not ISR */
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_ms) >= 100u && !twr_in_progress()) {
        last_ms = now;
        (void)uwb_app_start_ds_twr(0x0001u);
    }
    uwb_range_result_t r;
    if (uwb_app_take_result(&r)) {
        printf("TWR,%lu,%u,%u\r\n",
               (unsigned long)now,
               (unsigned)r.sequence,
               (unsigned)r.distance_cm);
    }
}
```

**Do not replace the SPI self-test main.c** before the driver's port and
responder are ready. A DWM3001CDK running Qorvo CLI isn't automatically
compatible with libdeca's TWR message protocol.

Queue capacity: 16 events. The code copies RX buffers so they remain valid
outside ISR context. `uwb_task_dropped()` must stay zero. The existing
single-slot `uwb_app_take_result()` can lose results if the main loop stalls;
use a ring-buffer/logger for a lossless 300-frame campaign.

## Needed from SPI teammate
- `dw3000_hw_*` including IRQ dispatch; diagnose IRQ stuck HIGH
- `dw3000_spi_*` incl. slower initial SPI and CRC transfer if required
- `deca_sleep`, `deca_usleep`, critical sections; `dw3000_probe_interf`
- Pin and voltage compatibility, 8 MHz testing after initialization

The **validated SPI5 + UART base** is in team branch `task2-lv2-spi`, not the
local `UART_Printf_Lv2_FSBL` staging project. After the PR merges, switch to
`T2_UWB_SPI_FSBL` for final integration.
