// Adapter: functions that br101/dw3000-decadriver-source v08.02.02 expects from the platform (the same
// set as platform/nrf-sdk/deca_port.c in that repository), implemented on top of dw3000_hw.c / dw3000_spi.c.
//
// This file needs the driver's headers (deca_interface.h, deca_device_api.h). It is only compiled when the
// driver is part of the project, so it is NOT linked into the Device ID test project (see README).
// Written from the reference port; original application glue only, no driver source is copied here.
#include "deca_version.h"
#include "deca_interface.h"

#include "dw3000_hw.h"
#include "dw3000_spi.h"
#include "uwb_port.h"

#if DRIVER_VERSION_HEX < 0x080202
#error "deca_port.c is written for driver version 08.02.02 or newer"
#endif

void wakeup_device_with_io(void)
{
  dw3000_hw_wakeup();
}

// The driver calls these around SPI transactions so that the interrupt service does not interrupt a frame.
decaIrqStatus_t decamutexon(void)
{
  decaIrqStatus_t was_enabled = dw3000_hw_interrupt_is_enabled() ? 1 : 0;
  if (was_enabled) {
    dw3000_hw_interrupt_disable();
  }
  return was_enabled;
}

void decamutexoff(decaIrqStatus_t s)
{
  if (s) {
    dw3000_hw_interrupt_enable();
  }
}

// Busy waits on the cycle counter on purpose: these can be called from the interrupt service (the driver's
// interrupt handler runs at a higher priority than the SysTick that HAL_Delay() depends on, so HAL_Delay()
// would never return there).
void deca_sleep(unsigned int time_ms)
{
  while (time_ms-- > 0U) {
    uwb_port_delay_us(1000U);
  }
}

void deca_usleep(unsigned long time_us)
{
  uwb_port_delay_us((uint32_t)time_us);
}

static const struct dwt_spi_s dw3000_spi_fct = {
  .readfromspi = dw3000_spi_read,
  .writetospi = dw3000_spi_write,
  .writetospiwithcrc = dw3000_spi_write_crc,
  .setslowrate = dw3000_spi_speed_slow,
  .setfastrate = dw3000_spi_speed_fast,
};

// DWM3000 contains a DW3110, handled by the DW3000 chip driver.
extern const struct dwt_driver_s dw3000_driver;

static const struct dwt_driver_s *const driver_list[] = {
  &dw3000_driver,
};

const struct dwt_probe_s dw3000_probe_interf = {
  .dw = NULL,
  .spi = (void *)&dw3000_spi_fct,
  .wakeup_device_with_io = dw3000_hw_wakeup,
  .driver_list = (struct dwt_driver_s **)driver_list,
  .dw_driver_num = 1,
};
