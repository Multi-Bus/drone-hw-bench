/* DW3000 GPIO/interrupt port for DWM3001CDK (nRF5 SDK 17.0.2).
 * Based on br101/dw3000-decadriver-source platform/nrf-sdk/dw3000_hw.c (ISC, Copyright (c) 2024-2025 Bruno Randolf),
 * used instead of that file because of two problems found in Task 6:
 *  1) interrupt_enable/disable/is_enabled passed the IRQ pin number (34) to nrf_gpiote_int_*(), which take a
 *     channel bitmask (INTENSET/INTENCLR). 34 = IN1|IN5, while our pin uses IN0, so decamutexon() never blocked
 *     the DW3000 interrupt during SPI transfers. Here the mutex masks GPIOTE_IRQn in the NVIC instead; an edge
 *     during the masked time stays latched in EVENTS_IN and is handled when the mask is lifted.
 *  2) GPIOTE only senses low-to-high edges. If IRQ is already high when the interrupt is enabled (SPIRDY after
 *     init), no edge comes (Task 6 step A). init_interrupt services the pending events once by hand.
 * The handler loop is bounded (Task 2 used the same limit). */
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrfx_gpiote.h>

#include "config.h"
#include "deca_device_api.h"
#include "dw3000_hw.h"
#include "dw3000_spi.h"
#include "log.h"

static const char *LOG_TAG = "DW3000";

#define ISR_LOOP_LIMIT 8

static volatile uint32_t loop_limit_hits;

uint32_t dw3000_hw_loop_limit_hits(void)
{
    return loop_limit_hits;
}

static void service_irq(void)
{
    int n = 0;
    while (nrf_gpio_pin_read(CONFIG_DW3000_GPIO_IRQ)) {
        if (++n > ISR_LOOP_LIMIT) {
            loop_limit_hits++;
            break;
        }
        dwt_isr();
    }
}

static void dw3000_isr(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    (void)pin;
    (void)action;
    service_irq();
}

int dw3000_hw_init(void)
{
    LOG_INF("HW Init (RESET:%d WAKEUP:%d IRQ:%d)", CONFIG_DW3000_GPIO_RESET, CONFIG_DW3000_GPIO_WAKEUP,
            CONFIG_DW3000_GPIO_IRQ);

    /* RESET: input, no pull (DW3000 drives it low while starting) */
    nrf_gpio_cfg_input(CONFIG_DW3000_GPIO_RESET, NRF_GPIO_PIN_NOPULL);
    int timeout = 1000;
    while (!nrf_gpio_pin_read(CONFIG_DW3000_GPIO_RESET) && --timeout > 0) {
        nrf_delay_ms(1);
    }
    if (timeout <= 0) {
        LOG_ERR("did not come out of reset");
        return -1;
    }
    return dw3000_spi_init();
}

int dw3000_hw_init_interrupt(void)
{
    nrfx_err_t ret = nrfx_gpiote_init();
    if (ret != NRFX_SUCCESS && ret != NRFX_ERROR_INVALID_STATE) {
        LOG_ERR("GPIOTE init failed");
        return -1;
    }
    nrfx_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
    config.pull = NRF_GPIO_PIN_PULLDOWN;
    ret = nrfx_gpiote_in_init(CONFIG_DW3000_GPIO_IRQ, &config, dw3000_isr);
    if (ret != NRFX_SUCCESS) {
        LOG_ERR("IRQ init failed");
        return -1;
    }
    nrfx_gpiote_in_event_enable(CONFIG_DW3000_GPIO_IRQ, true);

    /* already high: no edge will come */
    NVIC_DisableIRQ(GPIOTE_IRQn);
    service_irq();
    NVIC_EnableIRQ(GPIOTE_IRQn);
    return 0;
}

void dw3000_hw_interrupt_enable(void)
{
    NVIC_EnableIRQ(GPIOTE_IRQn);
}

void dw3000_hw_interrupt_disable(void)
{
    NVIC_DisableIRQ(GPIOTE_IRQn);
}

bool dw3000_hw_interrupt_is_enabled(void)
{
    return NVIC_GetEnableIRQ(GPIOTE_IRQn) != 0;
}

void dw3000_hw_fini(void)
{
    nrfx_gpiote_in_event_disable(CONFIG_DW3000_GPIO_IRQ);
    nrfx_gpiote_in_uninit(CONFIG_DW3000_GPIO_IRQ);
    nrfx_gpiote_uninit();
    nrf_gpio_cfg_default(CONFIG_DW3000_GPIO_RESET);
    dw3000_spi_fini();
}

void dw3000_hw_reset(void)
{
    LOG_INF("HW reset");
    nrf_gpio_cfg_output(CONFIG_DW3000_GPIO_RESET);
    nrf_gpio_pin_clear(CONFIG_DW3000_GPIO_RESET);
    nrf_delay_ms(1);
    nrf_gpio_pin_set(CONFIG_DW3000_GPIO_RESET);
    nrf_delay_ms(2);
    nrf_gpio_cfg_input(CONFIG_DW3000_GPIO_RESET, NRF_GPIO_PIN_NOPULL);
}

/* No WAKEUP pin on DWM3001CDK (see config.h): wake with SPI CS */
void dw3000_hw_wakeup(void)
{
    LOG_INF("WAKEUP CS");
    nrf_gpio_pin_clear(CONFIG_DW3000_SPI_CS);
    nrf_delay_us(500);
    nrf_gpio_pin_set(CONFIG_DW3000_SPI_CS);
    nrf_delay_ms(1);
}

void dw3000_hw_wakeup_pin_low(void)
{
}
