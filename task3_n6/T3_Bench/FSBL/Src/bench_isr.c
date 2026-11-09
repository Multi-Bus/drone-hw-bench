// Task 3 Lv2: 1 kHz timer interrupt test, see bench_isr.h.
//
// TIM2 counts at the timer clock (400 MHz, TIMPRE = DIV1 as in the TIM_TimeBase example) with
// PSC = 0 and ARR = 400000 - 1, so the update event comes every 1 ms. The handler records, with no
// printing inside it:
//   enter   DWT->CYCCNT on the first line  -> differences between handlers = actual period
//   cnt     TIM2->CNT right after that      -> timer ticks since the update event = interrupt latency
//   leave   DWT->CYCCNT after the work      -> leave - enter = ISR execution time
// The work is bench_isr_workload(): one 3-axis PID update and one 12x12 matrix multiply.
#include "bench_isr.h"
#include "bench.h"
#include "bench_cases.h"
#include "stm32n6xx_hal.h"
#include <stdint.h>
#include <stdio.h>

#define ISR_PERIODS   BENCH_MAX_SAMPLES   // 10,000
#define PERIOD_CYCLES 600000U             // 1 ms at 600 MHz
#define TIM_RELOAD    (400000U - 1U)      // 400 MHz / 400000 = 1 kHz

static uint32_t period_cyc[ISR_PERIODS];
static uint32_t exec_cyc[ISR_PERIODS];
static uint16_t lat_ticks[ISR_PERIODS];
static volatile uint32_t isr_count;       // number of recorded periods
static uint32_t prev_enter;
static uint32_t have_prev;

void TIM2_IRQHandler(void)
{
  uint32_t enter = DWT->CYCCNT;   // first line: when the handler started
  uint32_t cnt = TIM2->CNT;       // ticks since the update event (counter restarts at 0 on update)
  TIM2->SR = ~TIM_SR_UIF;         // clear the update flag

  bench_isr_workload();           // PID 3-axis + 12x12 matmul

  uint32_t leave = DWT->CYCCNT;
  if (have_prev && isr_count < ISR_PERIODS) {
    period_cyc[isr_count] = enter - prev_enter;
    exec_cyc[isr_count] = leave - enter;
    lat_ticks[isr_count] = (uint16_t)cnt;
    isr_count++;
  }
  prev_enter = enter;
  have_prev = 1U;
}

static void report_array32(const char *name, const uint32_t *a)
{
  bench_reset();
  for (uint32_t i = 0; i < ISR_PERIODS; i++) bench_record(a[i]);
  bench_report(name);
}

void bench_isr_test_run(void)
{
  RCC_PeriphCLKInitTypeDef clk = {0};
  clk.PeriphClockSelection = RCC_PERIPHCLK_TIM;
  clk.TIMPresSelection = RCC_TIMPRES_DIV1;
  if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
    printf("TIM clock config failed\r\n");
    return;
  }

  printf("\r\n== T3 Lv2 1 kHz interrupt test (PID + 12x12 matmul in the ISR), %u periods ==\r\n",
         (unsigned)ISR_PERIODS);
  printf("I-cache %s, D-cache %s\r\n",
         (SCB->CCR & SCB_CCR_IC_Msk) ? "on" : "off",
         (SCB->CCR & SCB_CCR_DC_Msk) ? "on" : "off");

  __HAL_RCC_TIM2_CLK_ENABLE();
  TIM2->CR1 = 0U;
  TIM2->PSC = 0U;
  TIM2->ARR = TIM_RELOAD;
  TIM2->CNT = 0U;
  TIM2->EGR = TIM_EGR_UG;         // load PSC and ARR now
  TIM2->SR = 0U;
  TIM2->DIER = TIM_DIER_UIE;
  HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);

  isr_count = 0U;
  have_prev = 0U;
  TIM2->CR1 = TIM_CR1_CEN;

  while (isr_count < ISR_PERIODS) { }   // about 10 s

  TIM2->CR1 = 0U;
  HAL_NVIC_DisableIRQ(TIM2_IRQn);

  report_array32("ISR period (target 600000 cycles = 1 ms)", period_cyc);

  static uint32_t dev[ISR_PERIODS];
  for (uint32_t i = 0; i < ISR_PERIODS; i++) {
    dev[i] = (period_cyc[i] > PERIOD_CYCLES) ? (period_cyc[i] - PERIOD_CYCLES) : (PERIOD_CYCLES - period_cyc[i]);
  }
  report_array32("period deviation |period - 600000| (jitter)", dev);

  report_array32("ISR execution time (PID + 12x12 matmul)", exec_cyc);

  // timer ticks are 400 MHz, CPU cycles are 600 MHz: cycles = ticks * 3 / 2
  bench_reset();
  for (uint32_t i = 0; i < ISR_PERIODS; i++) bench_record((uint32_t)lat_ticks[i] * 3U / 2U);
  bench_report("interrupt latency (CNT at handler start, in CPU cycles)");

  uint32_t max_exec = 0U;
  for (uint32_t i = 0; i < ISR_PERIODS; i++) if (exec_cyc[i] > max_exec) max_exec = exec_cyc[i];
  uint32_t load = (uint32_t)(((uint64_t)max_exec * 10000ULL) / PERIOD_CYCLES);   // hundredths of a percent
  printf("control loop CPU share (max exec / 1 ms) = %lu.%02lu %%\r\n",
         (unsigned long)(load / 100U), (unsigned long)(load % 100U));
  printf("== interrupt test done ==\r\n");
}
