// Adapter: br101/dw3000-decadriver-source platform hardware interface (v08.02.02) on top of uwb_port.
//
// The prototypes are the ones the driver's platform layer expects (platform/dw3000_hw.h in that repository,
// checked 2026-11-09). Pins: RSTn PE11 (D7), IRQ PD12 (D8, EXTI line 12), WAKEUP PD7 (D9). See pinmap.md.
#ifndef UWB_ADAPTER_DW3000_HW_H
#define UWB_ADAPTER_DW3000_HW_H

#include <stdbool.h>

int  dw3000_hw_init(void);              // pins + SPI, waits until the chip released RSTn (up to 1 s)
int  dw3000_hw_init_interrupt(void);    // IRQ pin as a rising-edge interrupt (EXTI12) and enables it
void dw3000_hw_fini(void);
void dw3000_hw_reset(void);             // RSTn low, release, wait for the chip to start
void dw3000_hw_wakeup(void);            // WAKEUP pin high for 500 us (about 1.5 ms in total)
void dw3000_hw_wakeup_pin_low(void);
void dw3000_hw_interrupt_enable(void);
void dw3000_hw_interrupt_disable(void);
bool dw3000_hw_interrupt_is_enabled(void);

// Set this to the function that runs the driver's interrupt service (the reference ports call dwt_isr()).
// The interrupt handler calls it while the IRQ pin is high, at most DW3000_HW_ISR_MAX_LOOPS times in a row.
extern void (*dw3000_hw_isr_hook)(void);

// Calls the hook while the IRQ pin is high (at most DW3000_HW_ISR_MAX_LOOPS times). Called by the interrupt
// handler; can also be called from the main loop. Does nothing while no hook is set.
void dw3000_hw_irq_poll(void);
#define DW3000_HW_ISR_MAX_LOOPS 8

#endif
