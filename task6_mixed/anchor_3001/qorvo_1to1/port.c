#include <stddef.h>
#include "deca_interface.h"
#include "deca_probe_interface.h"
#include "dw3000/dw3000_deca_regs.h"
#include "port.h"

/* DWM3001CDK wiring verified by the team's Task 6 port. */
enum { SCK = 3, MOSI = 8, MISO = 29, RESET = 25, CS = 6 };

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define P0(offset) REG(0x50000000u + (offset))
#define P1(offset) REG(0x50000300u + (offset))
#define SPI(offset) REG(0x40003000u + (offset))
#define TIMER2(offset) REG(0x4000A000u + (offset))
#define PIN_CONFIG(pin) (0x700u + 4u * (pin))

static uint8_t tx[1032];
static uint8_t rx[1032];

struct bench_status {
    uint32_t magic;
    uint32_t device_id;
    uint32_t polls_sent;
    uint32_t finals_sent;
    uint32_t rx_timeouts;
    char message[40];
    uint32_t channel_control;
    uint32_t tx_power;
    uint32_t pll_bias;
};

/* Read over SWD, without requiring a USB CDC stack in this RAM image. */
volatile struct bench_status bench __attribute__((section(".bench"), used));

static void timer_init(void)
{
    TIMER2(0x004) = 1; /* STOP */
    TIMER2(0x504) = 0; /* Timer mode */
    TIMER2(0x508) = 3; /* 32 bits */
    TIMER2(0x510) = 4; /* 16 MHz / 16 = 1 MHz */
    TIMER2(0x00C) = 1; /* CLEAR */
    TIMER2(0x000) = 1; /* START */
}

static uint32_t timer_us(void)
{
    TIMER2(0x040) = 1; /* CAPTURE[0] */
    return TIMER2(0x540); /* CC[0] */
}

static void delay_us(uint32_t us)
{
    const uint32_t start = timer_us();
    while ((uint32_t)(timer_us() - start) < us) { }
}

void deca_usleep(unsigned long us)
{
    delay_us(us);
}

void Sleep(unsigned int milliseconds)
{
    while (milliseconds-- > 0u) {
        delay_us(1000);
    }
}

void deca_sleep(unsigned int milliseconds)
{
    Sleep(milliseconds);
}

/* This image uses polling and starts with interrupts masked. */
decaIrqStatus_t decamutexon(void)
{
    return 0;
}

void decamutexoff(decaIrqStatus_t state)
{
    (void)state;
}

static void spi_slow(void)
{
    SPI(0x524) = 0x20000000u; /* 2 MHz */
}

void port_set_dw_ic_spi_fastrate(void)
{
    SPI(0x524) = 0x80000000u; /* 8 MHz */
}

static void spi_init(void)
{
    SPI(0x500) = 0;
    P0(0x50C) = (1u << SCK) | (1u << MOSI);
    P0(PIN_CONFIG(SCK)) = 3;
    P0(PIN_CONFIG(MOSI)) = 3;
    P0(PIN_CONFIG(MISO)) = 0;
    P1(0x508) = 1u << CS;
    P1(PIN_CONFIG(CS)) = 3;
    SPI(0x508) = SCK;
    SPI(0x50C) = MOSI;
    SPI(0x510) = MISO;
    SPI(0x554) = 0; /* MSB first, mode 0 */
    spi_slow();
    SPI(0x500) = 7; /* SPIM EasyDMA */
}

void reset_DWIC(void)
{
    P0(0x50C) = 1u << RESET;
    P0(PIN_CONFIG(RESET)) = 1;
    Sleep(2);
    P0(PIN_CONFIG(RESET)) = 0; /* Release RSTn to input, without driving it high. */
    Sleep(5);
}

static void wakeup_dw(void)
{
    P1(0x50C) = 1u << CS;
    delay_us(600);
    P1(0x508) = 1u << CS;
    Sleep(2);
}

static int32_t transfer(uint16_t header_len, const uint8_t *header,
                        uint16_t data_len, const uint8_t *out, uint8_t *in,
                        const uint8_t *crc)
{
    const uint32_t length = (uint32_t)header_len + data_len + (crc != NULL);
    if (length > sizeof(tx)) {
        return DWT_ERROR;
    }
    memcpy(tx, header, header_len);
    if (data_len != 0u) {
        if (out != NULL) {
            memcpy(tx + header_len, out, data_len);
        } else {
            memset(tx + header_len, 0, data_len);
        }
    }
    if (crc != NULL) {
        tx[length - 1u] = *crc;
    }

    SPI(0x118) = 0; /* EVENTS_END */
    SPI(0x120) = 0; /* EVENTS_STOPPED */
    SPI(0x534) = (uintptr_t)rx;
    SPI(0x538) = in != NULL ? length : 0;
    SPI(0x544) = (uintptr_t)tx;
    SPI(0x548) = length;
    P1(0x50C) = 1u << CS;
    SPI(0x010) = 1; /* TASKS_START */
    const uint32_t start = timer_us();
    while (SPI(0x118) == 0u) {
        if ((uint32_t)(timer_us() - start) >= 10000u) {
            SPI(0x014) = 1; /* TASKS_STOP */
            P1(0x508) = 1u << CS;
            return DWT_ERROR;
        }
    }
    P1(0x508) = 1u << CS;
    if (in != NULL) {
        memcpy(in, rx + header_len, data_len);
    }
    return DWT_SUCCESS;
}

static int32_t spi_read(uint16_t hlen, uint8_t *header, uint16_t len, uint8_t *data)
{
    return transfer(hlen, header, len, NULL, data, NULL);
}

static int32_t spi_write(uint16_t hlen, const uint8_t *header, uint16_t len, const uint8_t *data)
{
    return transfer(hlen, header, len, data, NULL, NULL);
}

static int32_t spi_write_crc(uint16_t hlen, const uint8_t *header, uint16_t len,
                             const uint8_t *data, uint8_t crc)
{
    return transfer(hlen, header, len, data, NULL, &crc);
}

static struct dwt_spi_s spi = {
    .readfromspi = spi_read,
    .writetospi = spi_write,
    .writetospiwithcrc = spi_write_crc,
    .setslowrate = spi_slow,
    .setfastrate = port_set_dw_ic_spi_fastrate,
};

static const struct dwt_driver_s *drivers[] = { &dw3000_driver };

const struct dwt_probe_s dw3000_probe_interf = {
    .spi = &spi,
    .wakeup_device_with_io = wakeup_dw,
    .driver_list = (struct dwt_driver_s **)drivers,
    .dw_driver_num = 1,
};

void test_run_info(unsigned char *message)
{
    unsigned int index = 0;
    while (index < sizeof(bench.message) - 1u && message[index] != 0u) {
        bench.message[index] = (char)message[index];
        ++index;
    }
    bench.message[index] = '\0';
}

void __real_dwt_configuretxrf(dwt_txconfig_t *config);

void __wrap_dwt_configuretxrf(dwt_txconfig_t *config)
{
    /* Channel-5 reference-frame power index recorded on a CDK in the team's Task 1 LISTCAL. */
    power_indexes_t indexes = { .input = { 0x1B, 0x1B, 0x1B, 0x1B } };
    tx_adj_res_t result = { 0 };
    if (dwt_calculate_linear_tx_power(5, &indexes, &result) != DWT_SUCCESS) {
        test_run_info((unsigned char *)"TX POWER CONVERSION FAILED");
        for (;;) { }
    }

    dwt_txconfig_t adjusted = *config;
    adjusted.power = result.tx_frame_cfg.tx_power_setting;
    __real_dwt_configuretxrf(&adjusted);
    dwt_setpllbiastrim(result.tx_frame_cfg.pll_bias);
    bench.tx_power = dwt_read_reg(TX_POWER_ID);
    bench.pll_bias = result.tx_frame_cfg.pll_bias;
}

void __real_waitforsysstatus(uint32_t *low, uint32_t *high,
                             uint32_t low_mask, uint32_t high_mask);

void __wrap_waitforsysstatus(uint32_t *low, uint32_t *high,
                             uint32_t low_mask, uint32_t high_mask)
{
    uint32_t status = 0;
    __real_waitforsysstatus(&status, high, low_mask, high_mask);
    if (low != NULL) {
        *low = status;
    }
    if ((low_mask & DWT_INT_RXFCG_BIT_MASK) != 0u) {
        if ((status & DWT_INT_TXFRS_BIT_MASK) != 0u) {
            if (bench.polls_sent == 0u) {
                bench.channel_control = dwt_read_reg(CHAN_CTRL_ID);
            }
            ++bench.polls_sent;
        }
        if ((status & SYS_STATUS_ALL_RX_TO) != 0u) {
            ++bench.rx_timeouts;
        }
    } else if ((status & DWT_INT_TXFRS_BIT_MASK) != 0u) {
        ++bench.finals_sent;
    }
}

extern int ds_twr_initiator(void);
extern uint32_t __bss_start, __bss_end;

__attribute__((used, noreturn)) void cdk_init(void)
{
    for (uint32_t *word = &__bss_start; word < &__bss_end; ++word) {
        *word = 0;
    }
    bench = (struct bench_status){ .magic = 0x4E365557u };
    REG(0xE000E010u) = 0; /* SysTick off */
    timer_init();
    spi_init();
    reset_DWIC();
    uint8_t header = 0;
    uint8_t id[4] = { 0 };
    if (spi_read(1, &header, sizeof(id), id) == DWT_SUCCESS) {
        bench.device_id = (uint32_t)id[0] | ((uint32_t)id[1] << 8) |
                          ((uint32_t)id[2] << 16) | ((uint32_t)id[3] << 24);
    }

    ds_twr_initiator();
    for (;;) { }
}

__attribute__((naked, used, section(".text.start"))) void start(void)
{
    __asm volatile(
        "cpsid i\n"
        "movs r0, #0\n"
        "msr control, r0\n"
        "isb\n"
        "ldr r0, =0x20020000\n"
        "mov sp, r0\n"
        "b cdk_init\n");
}
