// Task 3 Lv2: benchmark cases (plain C, float32). Uses bench.h for the measurement.
#ifndef BENCH_CASES_H
#define BENCH_CASES_H

// Run every case BENCH_MAX_SAMPLES times and print one report per case.
void bench_cases_run_all(void);

// The work done inside the 1 kHz interrupt test: one 3-axis PID update + one 12x12 matmul.
void bench_isr_workload(void);

#endif
