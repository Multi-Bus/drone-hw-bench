// Task 2 Lv2: hardware port layer, NUCLEO-N657X0-Q <-> DWM3000EVB (DW3110).
//
// The EVB is an Arduino shield, so it plugs straight into the Nucleo's Arduino header.
// Pins used (see task2_evb_stm32/lv2_spi/pinmap.md for the evidence):
//   SPI5 SCK  PE15 (D13)   MISO PG1 (D12)   MOSI PG2 (D11)   mode 0, 8 bit, MSB first
//   CSn       PA3  (D10)   driven as a GPIO (software chip select)
//   RSTn      PE11 (D7)    open-drain: driven low to reset, otherwise released (never driven high)
//   IRQ       PD12 (D8)    input (DW3000 interrupt output)
//   WAKEUP    PD7  (D9)    not used yet
// Deliberately NOT touched (module outputs / SPI-mode pins, see pinmap.md 3-1):
//   PE0 (D4), PE9 (D3) = DW GPIO2/GPIO3, PD8 (D1), PD9 (D0) = DW GPIO5/GPIO6.
//
// The two SPI hooks have the same arguments as the Qorvo DW3000 API port functions
// readfromspi() / writetospi(), so the Qorvo driver can be attached to them.
#ifndef UWB_PORT_H
#define UWB_PORT_H

#include <stdint.h>

#define UWB_OK     0
#define UWB_ERROR (-1)

int      uwb_port_init(void);                 // GPIOs + SPI5, starts at the slow rate
void     uwb_port_spi_slow(void);             // about 2 MHz (needed during reset/initialisation)
void     uwb_port_spi_fast(void);             // about 8 MHz
uint32_t uwb_port_spi_hz(void);               // actual SPI clock for the current setting
void     uwb_port_reset(void);                // hardware reset through RSTn, waits for the chip to start
int      uwb_port_irq_level(void);            // 1 = IRQ pin high
void     uwb_port_sleep_ms(uint32_t ms);

// Qorvo-style SPI hooks. Return UWB_OK (0) on success, UWB_ERROR (-1) on failure.
// header is sent first, then read_len bytes are read (or body_len bytes are written), all in one chip-select frame.
int uwb_spi_read(uint16_t header_len, const uint8_t *header, uint16_t read_len, uint8_t *read_buf);
int uwb_spi_write(uint16_t header_len, const uint8_t *header, uint16_t body_len, const uint8_t *body);

// Reads the DEV_ID register (file 0x00, offset 0, 4 bytes, little endian).
// raw[0..3] is the byte order as received (raw[0] is the least significant byte).
int uwb_port_read_dev_id(uint32_t *dev_id, uint8_t raw[4]);

#endif
