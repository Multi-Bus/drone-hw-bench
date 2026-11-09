// Task 3 Lv2: benchmark cases (plain C, float32), see bench_cases.h.
//
// Each case: BENCH_MAX_SAMPLES x { bench_start(); <one operation>; bench_stop(); }.
// Nothing is printed while measuring. Reports are printed after each case.
//
// Keeping the compiler from deleting the work (matters at -O2/-O3):
//  - kernels are noinline, so the call itself stays in the measured time
//  - inputs come from volatile variables, so they are not compile-time constants
//  - BARRIER() stops the compiler from moving memory accesses across start/stop
//  - KEEP(x) marks a float value as used/modified: the input is passed through it right after
//    bench_start() so the computation cannot move before the start, and the result right before
//    bench_stop() so it cannot be dropped or moved after the stop
#include "bench_cases.h"
#include "bench.h"
#include "stm32n6xx.h"   // SCB for the cache state
#include "arm_math.h"      // CMSIS-DSP (arm_mat_mult_f32)
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define BARRIER() __asm volatile("" ::: "memory")
#define KEEP(x)   __asm volatile("" : "+w"(x))

#define NMAX 12

static float MA[NMAX * NMAX], MB[NMAX * NMAX], MC[NMAX * NMAX];
static float MR[NMAX * NMAX];   // copy of the plain C result, to compare with CMSIS-DSP

static volatile float in_a = 0.7f;      // used as sinf/cosf/sqrtf/atan2f input and PID measurement
static volatile float in_b = 1.3f;
static volatile float in_g = 0.01f;     // gyro rate for the attitude step (rad/s)
static volatile float sink_f;           // checksum so that results are used

// ---------------------------------------------------------------- matrix multiply
__attribute__((noinline)) static void matmul_f32(const float *A, const float *B, float *C, int n)
{
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      float s = 0.0f;
      for (int k = 0; k < n; k++) {
        s += A[i * n + k] * B[k * n + j];
      }
      C[i * n + j] = s;
    }
  }
}

static void run_matmul(int n, const char *name)
{
  bench_reset();
  for (uint32_t it = 0; it < BENCH_MAX_SAMPLES; it++) {
    BARRIER();
    bench_start();
    matmul_f32(MA, MB, MC, n);
    BARRIER();
    bench_stop();
  }
  sink_f = sink_f + MC[0] + MC[n * n - 1];
  bench_report(name);
}

// ---------------------------------------------------------------- matrix multiply, CMSIS-DSP
// arm_mat_mult_f32 uses Helium (MVE) vector instructions when the build enables MVE (it does here).
static void run_matmul_dsp(uint16_t n, const char *name)
{
  arm_matrix_instance_f32 a = { n, n, MA };
  arm_matrix_instance_f32 b = { n, n, MB };
  arm_matrix_instance_f32 c = { n, n, MC };
  bench_reset();
  for (uint32_t it = 0; it < BENCH_MAX_SAMPLES; it++) {
    BARRIER();
    bench_start();
    arm_mat_mult_f32(&a, &b, &c);
    BARRIER();
    bench_stop();
  }
  bench_report(name);
}

// largest |plain C - CMSIS-DSP| over the n x n result, in units of 1e-6
static long max_diff_e6(int n)
{
  float m = 0.0f;
  for (int i = 0; i < n * n; i++) {
    float d = MC[i] - MR[i];
    if (d < 0.0f) d = -d;
    if (d > m) m = d;
  }
  return (long)(m * 1000000.0f);
}

// ---------------------------------------------------------------- 3-axis PID
typedef struct { float kp, ki, kd, integ, prev, lim; } pid_axis_t;
static pid_axis_t pid[3] = {
  { 1.5f, 0.4f, 0.05f, 0.0f, 0.0f, 10.0f },
  { 1.5f, 0.4f, 0.05f, 0.0f, 0.0f, 10.0f },
  { 2.0f, 0.1f, 0.02f, 0.0f, 0.0f, 10.0f },
};
static float pid_sp[3] = { 1.0f, -0.5f, 0.25f };
static float pid_meas[3];
static float pid_out[3];

__attribute__((noinline)) static void pid3_update(const float *sp, const float *meas, float dt, float *out)
{
  for (int i = 0; i < 3; i++) {
    float err = sp[i] - meas[i];
    pid[i].integ += pid[i].ki * err * dt;
    if (pid[i].integ > pid[i].lim)  pid[i].integ = pid[i].lim;
    if (pid[i].integ < -pid[i].lim) pid[i].integ = -pid[i].lim;
    float d = (err - pid[i].prev) / dt;
    pid[i].prev = err;
    float o = pid[i].kp * err + pid[i].integ + pid[i].kd * d;
    if (o > pid[i].lim)  o = pid[i].lim;
    if (o < -pid[i].lim) o = -pid[i].lim;
    out[i] = o;
  }
}

static void run_pid(void)
{
  bench_reset();
  for (uint32_t it = 0; it < BENCH_MAX_SAMPLES; it++) {
    pid_meas[0] = in_a;                 // not measured: fresh measurement each cycle
    pid_meas[1] = in_b;
    pid_meas[2] = in_a * 0.5f;
    BARRIER();
    bench_start();
    pid3_update(pid_sp, pid_meas, 0.001f, pid_out);
    BARRIER();
    bench_stop();
  }
  sink_f = sink_f + pid_out[0] + pid_out[1] + pid_out[2];
  bench_report("PID 3-axis, 1 update");
}

// ---------------------------------------------------------------- math functions
static void run_sinf(void)
{
  bench_reset();
  for (uint32_t it = 0; it < BENCH_MAX_SAMPLES; it++) {
    float x = in_a, r;
    BARRIER();
    bench_start();
    KEEP(x);
    r = sinf(x);
    KEEP(r);
    bench_stop();
  }
  bench_report("sinf");
}

static void run_cosf(void)
{
  bench_reset();
  for (uint32_t it = 0; it < BENCH_MAX_SAMPLES; it++) {
    float x = in_a, r;
    BARRIER();
    bench_start();
    KEEP(x);
    r = cosf(x);
    KEEP(r);
    bench_stop();
  }
  bench_report("cosf");
}

static void run_sqrtf(void)
{
  bench_reset();
  for (uint32_t it = 0; it < BENCH_MAX_SAMPLES; it++) {
    float x = in_a, r;
    BARRIER();
    bench_start();
    KEEP(x);
    r = sqrtf(x);
    KEEP(r);
    bench_stop();
  }
  bench_report("sqrtf");
}

static void run_atan2f(void)
{
  bench_reset();
  for (uint32_t it = 0; it < BENCH_MAX_SAMPLES; it++) {
    float y = in_a, x = in_b, r;
    BARRIER();
    bench_start();
    KEEP(y);
    KEEP(x);
    r = atan2f(y, x);
    KEEP(r);
    bench_stop();
  }
  bench_report("atan2f");
}

// ---------------------------------------------------------------- attitude step
// q = q (x) (1, w*dt/2), then normalize: 16 multiplies for the product, 1 sqrtf, 1 division.
typedef struct { float w, x, y, z; } quat_t;
static quat_t q_att = { 1.0f, 0.0f, 0.0f, 0.0f };

__attribute__((noinline)) static void quat_step(quat_t *q, float gx, float gy, float gz, float dt)
{
  float hx = 0.5f * gx * dt, hy = 0.5f * gy * dt, hz = 0.5f * gz * dt;
  quat_t r;
  r.w = q->w - q->x * hx - q->y * hy - q->z * hz;
  r.x = q->x + q->w * hx + q->y * hz - q->z * hy;
  r.y = q->y + q->w * hy - q->x * hz + q->z * hx;
  r.z = q->z + q->w * hz + q->x * hy - q->y * hx;
  float n = 1.0f / sqrtf(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z);
  q->w = r.w * n;
  q->x = r.x * n;
  q->y = r.y * n;
  q->z = r.z * n;
}

static void run_quat(void)
{
  bench_reset();
  for (uint32_t it = 0; it < BENCH_MAX_SAMPLES; it++) {
    float g = in_g;
    BARRIER();
    bench_start();
    quat_step(&q_att, g, 2.0f * g, 0.5f * g, 0.001f);
    BARRIER();
    bench_stop();
  }
  sink_f = sink_f + q_att.w + q_att.x;
  bench_report("attitude step (quat integrate + normalize)");
}

// ---------------------------------------------------------------- work for the interrupt test
void bench_isr_workload(void)
{
  pid_meas[0] = in_a;                 // fresh measurement each cycle
  pid_meas[1] = in_b;
  pid_meas[2] = in_a * 0.5f;
  pid3_update(pid_sp, pid_meas, 0.001f, pid_out);
  matmul_f32(MA, MB, MC, 12);
}

// ---------------------------------------------------------------- driver
void bench_cases_run_all(void)
{
  for (int i = 0; i < NMAX * NMAX; i++) {
    MA[i] = 0.5f + 0.001f * (float)(i % 17);
    MB[i] = 1.0f - 0.002f * (float)(i % 13);
  }

#ifdef __OPTIMIZE__
  printf("\r\n== T3 Lv2 benchmarks, optimized build (level: see project settings) ==\r\n");
#else
  printf("\r\n== T3 Lv2 benchmarks, -O0 build (no optimization) ==\r\n");
#endif

  printf("I-cache %s, D-cache %s\r\n",
         (SCB->CCR & SCB_CCR_IC_Msk) ? "on" : "off",
         (SCB->CCR & SCB_CCR_DC_Msk) ? "on" : "off");

  run_matmul(4,  "matmul 4x4 f32");
  run_matmul_dsp(4, "matmul 4x4 f32 (CMSIS-DSP)");
  run_matmul(12, "matmul 12x12 f32");
  for (int i = 0; i < 12 * 12; i++) MR[i] = MC[i];
  run_matmul_dsp(12, "matmul 12x12 f32 (CMSIS-DSP)");
  printf("  max |plain C - CMSIS-DSP| = %ld e-6\r\n", max_diff_e6(12));
  run_pid();
  run_sinf();
  run_cosf();
  run_sqrtf();
  run_atan2f();
  run_quat();

  printf("checksum = %ld\r\n== done ==\r\n", (long)(sink_f * 1000.0f));
}
