// Task 3: DWT cycle-counter measurement helpers, STM32N6 (Cortex-M55). See bench.h.
#include "bench.h"
#include "stm32n6xx.h"   // DCB, DWT, SystemCoreClock
#include <stdio.h>

#define BENCH_OUTLIER_MARGIN 100U   // a sample counts as an outlier if it is more than this many cycles above the minimum

static uint32_t samples[BENCH_MAX_SAMPLES];   // one measurement per entry, in cycles
static uint32_t count;                        // number of stored samples
static uint32_t t0;                           // CYCCNT value at bench_start()

void bench_init(void)
{
  // Cortex-M55 headers name this DCB; older CMSIS versions call it CoreDebug.
  DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void bench_reset(void)
{
  count = 0;
}

void bench_start(void)
{
  t0 = DWT->CYCCNT;
}

void bench_stop(void)
{
  uint32_t now = DWT->CYCCNT;   // read first so the code below is not measured
  bench_record(now - t0);       // unsigned subtraction is correct across one wrap (~7.16 s at 600 MHz)
}

void bench_record(uint32_t cycles)
{
  if (count < BENCH_MAX_SAMPLES) {
    samples[count++] = cycles;  // extra samples are dropped; n in the report shows how many were kept
  }
}

static uint64_t cycles_to_ns(uint32_t cycles)
{
  return (uint64_t)cycles * 1000000000ULL / SystemCoreClock;
}

// printf %f is not enabled in this build, so the microseconds are printed from integers.
static void print_line(const char *label, uint32_t cycles)
{
  uint64_t ns = cycles_to_ns(cycles);
  printf("  %-4s %10lu cycles  %lu.%03lu us\r\n", label, (unsigned long)cycles,
         (unsigned long)(ns / 1000U), (unsigned long)(ns % 1000U));
}

void bench_report(const char *name)
{
  if (count == 0) {
    printf("%s: no samples\r\n", name);
    return;
  }

  uint32_t min = samples[0];
  uint32_t max = samples[0];
  uint32_t max_idx = 0;
  uint64_t sum = 0;   // 10,000 samples of up to 2^32 cycles do not fit in 32 bits
  for (uint32_t i = 0; i < count; i++) {
    uint32_t v = samples[i];
    if (v < min) min = v;
    if (v > max) { max = v; max_idx = i; }
    sum += v;
  }

  printf("%s: n=%lu, clock=%lu Hz\r\n", name, (unsigned long)count, (unsigned long)SystemCoreClock);
  print_line("mean", (uint32_t)(sum / count));
  print_line("min", min);
  print_line("max", max);   // real-time decisions use the maximum
  printf("  max at sample #%lu\r\n", (unsigned long)max_idx);   // #0 = first run (cold cache)

  uint32_t outliers = 0;
  for (uint32_t i = 0; i < count; i++) {
    if (samples[i] > min + BENCH_OUTLIER_MARGIN) outliers++;
  }
  printf("  samples > min+%u cycles: %lu of %lu\r\n", (unsigned)BENCH_OUTLIER_MARGIN,
         (unsigned long)outliers, (unsigned long)count);
}

void bench_dump(void)
{
  printf("index,cycles\r\n");
  for (uint32_t i = 0; i < count; i++) {
    printf("%lu,%lu\r\n", (unsigned long)i, (unsigned long)samples[i]);
  }
}
