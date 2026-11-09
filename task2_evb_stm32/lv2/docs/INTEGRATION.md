# Task 2 Lv2: UWB application integration notes

## Scope
- `uwb_app.c/.h` is *our own* minimal adapter over `libdeca` TWR API.
- Source references: `br101/libdeca` (LGPL-3.0) and `br101/dw3000-decadriver-source` (mixed ISC and Qorvo licenses).
- These files are **not a complete standalone STM32N6 project**. They will not build until decadriver, libdeca, their dependencies, and STM32 platform glue are integrated.

## Required from SPI teammate / integration
1. Working SPI read/write (including transactions, CS timing, slow/fast mode).
2. Reset, wakeup, IRQ setup and dispatch to DW3000 handler, GPIO power/voltage verified.
3. `dw3000_hw_*`, `dw3000_spi_*`, `deca_port` implementation for STM32N6; `dwt_probe` provider for driver version 08.02.02.
4. Logging `log.h`, HAL delay and microsecond delay, platform/IRQ critical-section policy.
5. Test `dwt_readdevid()` returns expected ID (DW3110 on DWM3000: 0xDECA0302), then exercise libdeca initialization.

## Own application API
- `uwb_app_init(0x0002, false)` for initiator; peer `0x0001`.
- `uwb_app_init(0x0001, true)` for responder (on a compatible second device).
- Initiator periodically calls `uwb_app_start_ds_twr(0x0001)` when no session is active, and polls `uwb_app_take_result(...)`.
- Results are `sequence` and `distance_cm`. Callback never calls `printf` because UART IO in an IRQ can disrupt ranging deadlines.
- Single-slot result storage is an early integration convenience, **not lossless**. Add a queue or critical section for 300-frame measurement campaign.
- `DWPHY_ANTENNA_DELAY` is an initial library constant, **not a measured calibration**.

## UART logging example (paste into an already initialized STM32 main loop)
```c
static uint32_t last_started_ms = 0;
uwb_range_result_t r;
uint32_t now = HAL_GetTick();
if ((uint32_t)(now - last_started_ms) >= 100u && !twr_in_progress()) {
    last_started_ms = now;
    (void)uwb_app_start_ds_twr(0x0001);
}
if (uwb_app_take_result(&r)) {
    printf("TWR,%u,%u\r\n", (unsigned)r.sequence, (unsigned)r.distance_cm);
}
```
`HAL_GetTick` / `printf` come from the surrounding STM32 project, not this adapter.

## Important interoperability caveat
Task 1 used a Qorvo DWM3001CDK CLI initiator/responder. `libdeca` has its own message format (`TWR_MSG_POLL`, `TWR_MSG_RESP`, `TWR_MSG_FINA`, `TWR_MSG_REPO`) and protocol framing. **A DWM3001CDK running the Qorvo CLI cannot be assumed to respond to it.** Confirm protocol/PHY compatibility and implement matching responder firmware before expecting peer ranging. A second STM32N6+DWM3000EVB running libdeca responder is the straightforward peer if hardware is available.

## Licensing
Do not copy Qorvo driver or libdeca into a public team repository blindly. Review each license and track upstream source/version. These adapter files contain original application glue only.
