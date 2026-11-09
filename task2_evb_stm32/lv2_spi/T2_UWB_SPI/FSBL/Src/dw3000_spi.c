// Adapter: br101/dw3000-decadriver-source platform SPI interface on top of uwb_port, see dw3000_spi.h.
#include "dw3000_spi.h"
#include "uwb_port.h"

#define DW3000_SPI_SUCCESS   0    // DWT_SUCCESS
#define DW3000_SPI_ERROR   (-1)   // DWT_ERROR

int dw3000_spi_init(void)
{
  return (uwb_port_init() == UWB_OK) ? DW3000_SPI_SUCCESS : DW3000_SPI_ERROR;
}

void dw3000_spi_fini(void)
{
  uwb_port_spi_deinit();
}

void dw3000_spi_wakeup(void)
{
  uwb_port_cs_wakeup();
}

void dw3000_spi_speed_slow(void)
{
  uwb_port_spi_slow();
}

void dw3000_spi_speed_fast(void)
{
  uwb_port_spi_fast();
}

int32_t dw3000_spi_read(uint16_t headerLength, uint8_t *headerBuffer, uint16_t readLength, uint8_t *readBuffer)
{
  return (uwb_spi_read(headerLength, headerBuffer, readLength, readBuffer) == UWB_OK) ? DW3000_SPI_SUCCESS : DW3000_SPI_ERROR;
}

int32_t dw3000_spi_write(uint16_t headerLength, const uint8_t *headerBuffer, uint16_t bodyLength, const uint8_t *bodyBuffer)
{
  return (uwb_spi_write(headerLength, headerBuffer, bodyLength, bodyBuffer) == UWB_OK) ? DW3000_SPI_SUCCESS : DW3000_SPI_ERROR;
}

int32_t dw3000_spi_write_crc(uint16_t headerLength, const uint8_t *headerBuffer, uint16_t bodyLength,
                             const uint8_t *bodyBuffer, uint8_t crc8)
{
  return (uwb_spi_write_crc(headerLength, headerBuffer, bodyLength, bodyBuffer, crc8) == UWB_OK)
             ? DW3000_SPI_SUCCESS : DW3000_SPI_ERROR;
}

void dw3000_spi_trace_output(void)
{
}
