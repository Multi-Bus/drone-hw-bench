// Task 3 Lv2: 1 kHz timer interrupt test. Runs PID + 12x12 matmul inside the interrupt for
// 10,000 periods and reports period, period deviation (jitter), ISR execution time, interrupt latency.
#ifndef BENCH_ISR_H
#define BENCH_ISR_H

void bench_isr_test_run(void);   // blocks for about 10 s, then prints the reports

#endif
