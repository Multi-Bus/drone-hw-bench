// Task 2 Lv2: hardware port layer, see uwb_port.h.
#include "uwb_port.h"
#include "main.h"
#include <string.h>

#define UWB_SPI_MAX_FRAME 1100U   // header + payload (a DW3000 RX frame is up to 1023 bytes)

#define CS_PORT   GPIOA
#define CS_PIN    GPIO_PIN_3      // D10
#define RST_PORT  GPIOE
#define RST_PIN   GPIO_PIN_11     // D7
#define IRQ_PORT  GPIOD
#define IRQ_PIN   GPIO_PIN_12     // D8

// SPI5 kernel clock is HSI (64 MHz), set in HAL_SPI_MspInit as in the ST SPI_FullDuplex example.
#define SPI_KERNEL_HZ        64000000UL
#define SPI_PRESC_SLOW       SPI_BAUDRATEPRESCALER_32   // 64 MHz / 32 = 2 MHz
#define SPI_PRESC_FAST       SPI_BAUDRATEPRESCALER_8    // 64 MHz / 8  = 8 MHz

static SPI_HandleTypeDef hspi5;
static uint32_t spi_divider = 32U;
static int port_ready;   // uwb_port_init() already done
static uint8_t tx_buf[UWB_SPI_MAX_FRAME];
static uint8_t rx_buf[UWB_SPI_MAX_FRAME];

// Called by HAL_SPI_Init(). Pin and clock setup copied from the ST SPI_FullDuplex_ComDMA_Master example (no DMA).
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI5) {
    RCC_PeriphCLKInitTypeDef clk = {0};
    clk.PeriphClockSelection = RCC_PERIPHCLK_SPI5;
    clk.Spi5ClockSelection = RCC_SPI5CLKSOURCE_HSI;
    if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
      Error_Handler();
    }
    __HAL_RCC_SPI5_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF5_SPI5;
    gpio.Pin = GPIO_PIN_15;                    // PE15 = SPI5_SCK
    HAL_GPIO_Init(GPIOE, &gpio);
    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2;        // PG1 = SPI5_MISO, PG2 = SPI5_MOSI
    HAL_GPIO_Init(GPIOG, &gpio);
  }
}

static int spi5_start(uint32_t prescaler, uint32_t divider)
{
  hspi5.Instance = SPI5;
  hspi5.Init.Mode = SPI_MODE_MASTER;
  hspi5.Init.Direction = SPI_DIRECTION_2LINES;
  hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;     // SPI mode 0 (CPOL 0, CPHA 0)
  hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi5.Init.NSS = SPI_NSS_SOFT;                 // chip select is a GPIO (PA3)
  hspi5.Init.BaudRatePrescaler = prescaler;
  hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi5.Init.CRCPolynomial = 0x7;
  hspi5.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi5.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi5.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi5.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi5.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi5.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi5.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  hspi5.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  hspi5.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  hspi5.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
  if (HAL_SPI_Init(&hspi5) != HAL_OK) {
    return UWB_ERROR;
  }
  spi_divider = divider;
  return UWB_OK;
}

int uwb_port_init(void)
{
  if (port_ready) {
    return UWB_OK;   // safe to call from several places
  }
  // cycle counter for the microsecond delay
  DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {0};

  // chip select: output, idle high (set the level before switching the pin to output)
  HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
  gpio.Pin = CS_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(CS_PORT, &gpio);

  // RSTn: open-drain output, released (high = high impedance). Never driven high by the MCU.
  HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_SET);
  gpio.Pin = RST_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RST_PORT, &gpio);

  // IRQ: input. Pull-down so the pin reads low when no shield is attached.
  gpio.Pin = IRQ_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(IRQ_PORT, &gpio);

  int rc = spi5_start(SPI_PRESC_SLOW, 32U);
  port_ready = (rc == UWB_OK);
  return rc;
}

void uwb_port_spi_slow(void)
{
  (void)HAL_SPI_DeInit(&hspi5);
  (void)spi5_start(SPI_PRESC_SLOW, 32U);
}

void uwb_port_spi_fast(void)
{
  (void)HAL_SPI_DeInit(&hspi5);
  (void)spi5_start(SPI_PRESC_FAST, 8U);
}

uint32_t uwb_port_spi_hz(void)
{
  return SPI_KERNEL_HZ / spi_divider;
}

void uwb_port_sleep_ms(uint32_t ms)
{
  HAL_Delay(ms);
}

void uwb_port_reset(void)
{
  HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_RESET);   // pull RSTn low (open-drain)
  HAL_Delay(2);                                           // the chip needs at least about 10 us; HAL_Delay(2) is 2 to 3 ms
  HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_SET);     // release
  HAL_Delay(5);                                           // the chip needs about 2 ms to start (INIT_RC -> IDLE_RC)
}

int uwb_port_irq_level(void)
{
  return (HAL_GPIO_ReadPin(IRQ_PORT, IRQ_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

int uwb_spi_read(uint16_t header_len, const uint8_t *header, uint16_t read_len, uint8_t *read_buf)
{
  uint32_t total = (uint32_t)header_len + read_len;
  if (total == 0U || total > UWB_SPI_MAX_FRAME) {
    return UWB_ERROR;
  }
  memcpy(tx_buf, header, header_len);
  memset(tx_buf + header_len, 0, read_len);

  HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);
  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi5, tx_buf, rx_buf, (uint16_t)total, 100);
  HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
  if (st != HAL_OK) {
    return UWB_ERROR;
  }
  memcpy(read_buf, rx_buf + header_len, read_len);
  return UWB_OK;
}

int uwb_spi_write(uint16_t header_len, const uint8_t *header, uint16_t body_len, const uint8_t *body)
{
  uint32_t total = (uint32_t)header_len + body_len;
  if (total == 0U || total > UWB_SPI_MAX_FRAME) {
    return UWB_ERROR;
  }
  memcpy(tx_buf, header, header_len);
  memcpy(tx_buf + header_len, body, body_len);

  HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);
  HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi5, tx_buf, (uint16_t)total, 100);
  HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
  return (st == HAL_OK) ? UWB_OK : UWB_ERROR;
}

int uwb_port_read_dev_id(uint32_t *dev_id, uint8_t raw[4])
{
  // One-byte header: read (bit 7 = 0), register file 0 (DEV_ID), offset 0, "fast access" short header.
  // This follows dwt_xfer3000() in the Qorvo DW3000 API (a read of offset 0 uses the 1-byte header), which
  // gives 0x00 for DEV_ID. To be confirmed on the first real read (a good ID such as 0xDECA03xx confirms it).
  static const uint8_t header[1] = { 0x00 };
  uint8_t b[4];
  if (uwb_spi_read(1, header, 4, b) != UWB_OK) {
    return UWB_ERROR;
  }
  memcpy(raw, b, 4);
  *dev_id = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
  return UWB_OK;
}

// ---- MOSI self test (see uwb_port.h) ------------------------------------------------------------
static uint32_t le32(const uint8_t b[4])
{
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static int panadr_read(uint32_t *value)
{
  static const uint8_t header[2] = { 0x40, 0x30 };   // read, extended, file 0, offset 0x0C
  uint8_t b[4];
  if (uwb_spi_read(2, header, 4, b) != UWB_OK) {
    return UWB_ERROR;
  }
  *value = le32(b);
  return UWB_OK;
}

static int panadr_write(uint32_t value)
{
  static const uint8_t header[2] = { 0xC0, 0x30 };   // write, extended, file 0, offset 0x0C
  uint8_t b[4] = { (uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24) };
  return uwb_spi_write(2, header, 4, b);
}

int uwb_port_mosi_selftest(uwb_mosi_result_t *r)
{
  static const uint32_t patterns[UWB_MOSI_PATTERNS] = {
    0xFFFFFFFFUL, 0x00000000UL, 0xA5A5A5A5UL, 0x5A5A5A5AUL,
    0x80402010UL, 0x08040201UL, 0x12345678UL, 0xCAFEBABEUL
  };
  static const uint8_t header_eam_devid[2] = { 0x40, 0x00 };   // read DEV_ID with an extended header
  uint8_t b[4];

  memset(r, 0, sizeof(*r));

  if (uwb_spi_read(2, header_eam_devid, 4, b) != UWB_OK) {
    return UWB_ERROR;
  }
  r->eam_dev_id = le32(b);

  if (panadr_read(&r->panadr_orig) != UWB_OK) {
    return UWB_ERROR;
  }
  for (uint32_t i = 0; i < UWB_MOSI_PATTERNS; i++) {
    uint32_t rd = 0;
    r->written[i] = patterns[i];
    if (panadr_write(patterns[i]) != UWB_OK || panadr_read(&rd) != UWB_OK) {
      return UWB_ERROR;
    }
    r->readback[i] = rd;
    if (rd != patterns[i]) {
      r->mismatches++;
      r->stuck_low_bits |= patterns[i] & ~rd;
      r->stuck_high_bits |= ~patterns[i] & rd;
    }
  }
  // put the original value back
  if (panadr_write(r->panadr_orig) != UWB_OK || panadr_read(&r->restored_value) != UWB_OK) {
    return UWB_ERROR;
  }
  return UWB_OK;
}

// ---- additions for the DW3000 driver adapter (dw3000_spi.c, dw3000_hw.c) -------------------------
void uwb_port_delay_us(uint32_t us)
{
  uint32_t ticks = us * (SystemCoreClock / 1000000U);
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < ticks) {
  }
}

int uwb_port_rst_is_high(void)
{
  return (HAL_GPIO_ReadPin(RST_PORT, RST_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

void uwb_port_cs_wakeup(void)
{
  HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);   // wake the chip with a long CS low pulse
  uwb_port_delay_us(500);
  HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
  HAL_Delay(1);
}

void uwb_port_spi_deinit(void)
{
  (void)HAL_SPI_DeInit(&hspi5);
  port_ready = 0;
}

// Same as uwb_spi_write() but one more byte (a CRC-8 supplied by the driver) follows the body in the same frame.
int uwb_spi_write_crc(uint16_t header_len, const uint8_t *header, uint16_t body_len, const uint8_t *body, uint8_t crc8)
{
  uint32_t total = (uint32_t)header_len + body_len + 1U;
  if (total > UWB_SPI_MAX_FRAME) {
    return UWB_ERROR;
  }
  memcpy(tx_buf, header, header_len);
  memcpy(tx_buf + header_len, body, body_len);
  tx_buf[header_len + body_len] = crc8;

  HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);
  HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi5, tx_buf, (uint16_t)total, 100);
  HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
  return (st == HAL_OK) ? UWB_OK : UWB_ERROR;
}

// ---- IRQ diagnosis -------------------------------------------------------------------------------
// Register file 0, 32 bit, extended 2-byte header (same format as the PANADR test).
static void reg_header(uint8_t hdr[2], uint8_t offset, int write)
{
  hdr[0] = (uint8_t)((write ? 0x80U : 0x00U) | 0x40U | (uint8_t)(offset >> 6));
  hdr[1] = (uint8_t)((offset & 0x3FU) << 2);
}

static int reg_read32(uint8_t offset, uint32_t *value)
{
  uint8_t h[2];
  uint8_t b[4];
  reg_header(h, offset, 0);
  if (uwb_spi_read(2, h, 4, b) != UWB_OK) {
    return UWB_ERROR;
  }
  *value = le32(b);
  return UWB_OK;
}

static int reg_write32(uint8_t offset, uint32_t value)
{
  uint8_t h[2];
  uint8_t b[4] = { (uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24) };
  reg_header(h, offset, 1);
  return uwb_spi_write(2, h, 4, b);
}

#define DW_SYS_ENABLE_LO   0x3CU
#define DW_SYS_ENABLE_HI   0x40U
#define DW_SYS_STATUS      0x44U
#define DW_STATUS_SPIRDY   0x00800000UL
#define DW_STATUS_RCINIT   0x01000000UL

int uwb_port_irq_diag(uwb_irq_diag_t *d)
{
  memset(d, 0, sizeof(*d));
  d->rst_level = uwb_port_rst_is_high();
  d->irq_before = uwb_port_irq_level();
  if (reg_read32(DW_SYS_STATUS, &d->status_before) != UWB_OK ||
      reg_read32(DW_SYS_ENABLE_LO, &d->enable_lo) != UWB_OK ||
      reg_read32(DW_SYS_ENABLE_HI, &d->enable_hi) != UWB_OK) {
    return UWB_ERROR;
  }
  // clear only the two start-up events (write 1 to clear); leave every other bit alone
  uint32_t clear = d->status_before & (DW_STATUS_SPIRDY | DW_STATUS_RCINIT);
  d->cleared_mask = clear;
  if (clear != 0U && reg_write32(DW_SYS_STATUS, clear) != UWB_OK) {
    return UWB_ERROR;
  }
  if (reg_read32(DW_SYS_STATUS, &d->status_after) != UWB_OK) {
    return UWB_ERROR;
  }
  d->irq_after = uwb_port_irq_level();
  return UWB_OK;
}
