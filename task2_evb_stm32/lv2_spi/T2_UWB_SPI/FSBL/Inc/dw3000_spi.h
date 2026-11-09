// Adapter: br101/dw3000-decadriver-source platform SPI interface (v08.02.02) on top of uwb_port.
//
// The prototypes are the ones the driver's platform layer expects (platform/dw3000_spi.h in that repository,
// checked 2026-11-09). When the driver is integrated, its own header can be used instead; the prototypes
// are the same. Return values follow the Qorvo convention DWT_SUCCESS = 0 and DWT_ERROR = -1
// (confirm against deca_device_api.h of the driver version in use).
#ifndef UWB_ADAPTER_DW3000_SPI_H
#define UWB_ADAPTER_DW3000_SPI_H

#include <stdint.h>

int  dw3000_spi_init(void);
void dw3000_spi_fini(void);
void dw3000_spi_wakeup(void);          // long CS low pulse (the board also has a WAKEUP pin, see dw3000_hw_wakeup)
void dw3000_spi_speed_slow(void);      // 2 MHz, required during reset and initialisation
void dw3000_spi_speed_fast(void);      // 8 MHz (not tested on the board yet)

int32_t dw3000_spi_read(uint16_t headerLength, uint8_t *headerBuffer, uint16_t readLength, uint8_t *readBuffer);
int32_t dw3000_spi_write(uint16_t headerLength, const uint8_t *headerBuffer, uint16_t bodyLength, const uint8_t *bodyBuffer);
int32_t dw3000_spi_write_crc(uint16_t headerLength, const uint8_t *headerBuffer, uint16_t bodyLength,
                             const uint8_t *bodyBuffer, uint8_t crc8);

void dw3000_spi_trace_output(void);    // only used by the driver when CONFIG_DW3000_SPI_TRACE is set; empty here

#endif
