// Task 3: DWT cycle-counter measurement helpers, STM32N6 (Cortex-M55).
//
// Usage: call bench_init() once, then for each condition:
//   bench_reset();
//   for (i = 0; i < BENCH_MAX_SAMPLES; i++) {
//     bench_start();
//     /* code under test */
//     bench_stop();
//   }
//   bench_report("name");   // summary, printed after measuring
//   bench_dump();           // optional: raw samples as CSV
//
// Nothing is printed between bench_start() and bench_stop(): samples are stored in an
// array and printed at the end, because printing would disturb the measurement.
#ifndef BENCH_H
#define BENCH_H

#include <stdint.h>

#define BENCH_MAX_SAMPLES 10000U   // task PDF: 10,000 repetitions per benchmark

void bench_init(void);                // enable the DWT cycle counter (CYCCNT)
void bench_reset(void);               // drop stored samples
void bench_start(void);               // remember the current CYCCNT
void bench_stop(void);                // store (CYCCNT - start) as one sample
void bench_record(uint32_t cycles);   // store a value computed elsewhere (e.g. ISR period)
void bench_report(const char *name);  // print n, mean, min, max (cycles, us), index of the max, outlier count
void bench_dump(void);                // print raw samples as "index,cycles"

#endif
