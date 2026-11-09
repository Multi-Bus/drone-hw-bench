// Adapter: br101/dw3000-decadriver-source platform hardware interface on top of uwb_port, see dw3000_hw.h.
#include "dw3000_hw.h"
#include "uwb_port.h"
#include "main.h"

#define WAKEUP_PORT  GPIOD
#define WAKEUP_PIN   GPIO_PIN_7      // D9
#define IRQ_PORT_    GPIOD
#define IRQ_PIN_     GPIO_PIN_12     // D8, EXTI line 12

void (*dw3000_hw_isr_hook)(void);

int dw3000_hw_init(void)
{
  if (uwb_port_init() != UWB_OK) {
    return -1;
  }

  // WAKEUP: output, idle low
  __HAL_RCC_GPIOD_CLK_ENABLE();
  HAL_GPIO_WritePin(WAKEUP_PORT, WAKEUP_PIN, GPIO_PIN_RESET);
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = WAKEUP_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(WAKEUP_PORT, &gpio);

  // wait until the chip released RSTn (like the reference port, 1 ms steps, 1 s timeout)
  uint32_t t0 = HAL_GetTick();
  while (!uwb_port_rst_is_high()) {
    if ((HAL_GetTick() - t0) > 1000U) {
      return -1;
    }
    HAL_Delay(1);
  }
  return 0;
}

int dw3000_hw_init_interrupt(void)
{
  __HAL_RCC_GPIOD_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = IRQ_PIN_;
  gpio.Mode = GPIO_MODE_IT_RISING;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(IRQ_PORT_, &gpio);
  HAL_NVIC_SetPriority(EXTI12_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI12_IRQn);
  return 0;
}

void dw3000_hw_fini(void)
{
  HAL_NVIC_DisableIRQ(EXTI12_IRQn);
  HAL_GPIO_DeInit(IRQ_PORT_, IRQ_PIN_);
}

void dw3000_hw_reset(void)
{
  uwb_port_reset();
}

void dw3000_hw_wakeup(void)
{
  HAL_GPIO_WritePin(WAKEUP_PORT, WAKEUP_PIN, GPIO_PIN_SET);
  uwb_port_delay_us(500);
  HAL_GPIO_WritePin(WAKEUP_PORT, WAKEUP_PIN, GPIO_PIN_RESET);
  HAL_Delay(1);
}

void dw3000_hw_wakeup_pin_low(void)
{
  HAL_GPIO_WritePin(WAKEUP_PORT, WAKEUP_PIN, GPIO_PIN_RESET);
}

void dw3000_hw_interrupt_enable(void)
{
  HAL_NVIC_EnableIRQ(EXTI12_IRQn);
}

void dw3000_hw_interrupt_disable(void)
{
  HAL_NVIC_DisableIRQ(EXTI12_IRQn);
}

bool dw3000_hw_interrupt_is_enabled(void)
{
  return NVIC_GetEnableIRQ(EXTI12_IRQn) != 0U;
}

// ---- interrupt dispatch ----------------------------------------------------------------------------
void EXTI12_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(IRQ_PIN_);
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin != IRQ_PIN_ || dw3000_hw_isr_hook == 0) {
    return;
  }
  // The reference ports call the driver's interrupt service while the IRQ pin stays high. The loop is limited
  // so that an IRQ pin that stays high (unserviced status bits) cannot lock up the interrupt handler.
  for (int i = 0; i < DW3000_HW_ISR_MAX_LOOPS && uwb_port_irq_level(); i++) {
    dw3000_hw_isr_hook();
  }
}
