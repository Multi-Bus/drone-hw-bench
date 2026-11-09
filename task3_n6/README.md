# Task 3: STM32N6 실시간 성능

목표: 드론 제어 루프를 돌리면서 NPU 추론을 같이 돌릴 수 있는지 MCU 내장 사이클 카운터(DWT CYCCNT)로 측정한다.
원본 과제: `UWB · STM32N6 · BLE 과제.pdf` (2026-11-05).

## 진행 현황 (2026-11-09)

| 레벨 | 상태 | 비고 |
| --- | --- | --- |
| Lv1 동작 확인 | 완료 | 보드 2개 bring-up, 클럭, CYCCNT 검증, 배치 사진 완료. 측정 공통 코드(`measure/`)는 Lv1에서는 컴파일만 확인했고 Lv2에서 보드 시험까지 마침 |
| Lv2 성능 측정 | 완료 (일부 조건만) | 벤치마크 8종 × -O0/-O2/-O3, 캐시 켬·끔(-O3만), CMSIS-DSP(-O3만), 1 kHz 인터럽트 시험. 보드 2에서만 측정. 빠진 조건은 "Lv2 한계" 참고 |
| Lv3 심화 | 시작 전 | Lv2 완료 후 |

## 보드

- NUCLEO-N657X0-Q (MB1940C), 7개 보유. 이번에 2개 확인했다 (보드 1: 가상 COM5, 보드 2: COM6). 내부 플래시 없음.
- ST-LINK용 USB-C 커넥터는 CN10. 충전 전용 케이블은 인식되지 않는다. 보드는 한 번에 하나만 연결한다.
- 점퍼: JP1 = BOOT0, JP2 = BOOT1. 개발 모드는 BOOT1 = 1 (ST 예제 README 기준 JP2 2-3, BOOT0는 상관없음). 개발 모드에서 디버그로 올려 실행한다.
- 보드 1은 점퍼를 옮기지 않은 채 디버그가 연결됐다 (JP1·JP2 캡이 서로 반대쪽 끝에 있었음). 보드 2의 점퍼 위치: (직접 기재)
- 개발 모드에서는 외부 플래시의 공장 데모(LED 깜빡임)가 실행되지 않는다. 기본 점퍼가 아닌 보드에서 데모가 안 보여도 불량이 아니다. 개발 모드에서 올린 프로그램은 RAM에서 돌고, 전원을 끄면 사라진다. 디버거를 Terminate해도 프로그램은 계속 돈다.
- 사용자 LED: 보드에는 `LED1`~`LED3`으로 인쇄돼 있다. `GPIO_IOToggle` 예제가 토글하는 LED1은 PG8, 파란색 (실제 확인). `UART_Printf` 예제는 오류가 나면 LED2를 켠다 (예제 README).
- 콘솔: USART1 (PE5 TX, PE6 RX), ST-LINK 가상 COM.

## Lv1 체크리스트 (동작 확인)

- [x] 보드 연결, ST-LINK와 가상 COM 인식 (보드 1 COM5, 보드 2 COM6)
- [x] ST 예제 `GPIO_IOToggle`로 LED1 깜빡임 (보드 2개)
- [x] ST 예제 `UART_Printf`로 UART 출력 (보드 2개)
- [x] `SystemCoreClock` 출력: 600 MHz (보드 2개)
- [x] CYCCNT 검증: `HAL_Delay(100)` 측정값이 클럭 × 0.1 s 근처 (보드 2개)
- [x] `measure/`에 공통 측정 코드 (시작·종료·통계·일괄 출력). 측정 중 UART 출력 금지, 배열에 저장 후 출력 (Lv1에서는 컴파일만 확인, Lv2에서 보드 시험 완료)
- [x] 배치 사진 `photos/` (보드 1 전체·점퍼, 보드 2 전체)

통과 기준 (PDF): 클럭 값 확인, CYCCNT 검증 (100 ms 측정값), 측정 코드가 repo에 올라감.

## 결과 (Lv1)

측정 날짜: 2026-11-09. 값은 Tera Term 화면의 출력을 옮겨 적은 것이다 (로그 파일을 저장하지 않았다).

| 보드 | 회차 | `SystemCoreClock` | `HAL_Delay(100)` 사이클 | µs |
| --- | --- | --- | --- | --- |
| 1 (COM5) | 1 | 600000000 Hz | 60216089 | 100360 |
| 2 (COM6) | 1 | 600000000 Hz | 60216089 | 100360 |
| 2 (COM6) | 2 | 600000000 Hz | 60216241 | 100360 |

- 클럭은 ST 예제가 600 MHz로 설정한다 (예제 README: "The system clock runs at 600 MHz"). 과제 PDF의 800 MHz가 아니다.
- 기대값 60,000,000 사이클(100 ms)보다 약 0.36% 크다. HAL 드라이버의 `HAL_Delay`는 1 ms 틱 단위로 기다려서 100~101 ms 사이가 나오는 것이 보통이다 (HAL 동작 설명 기준, 직접 확인하지 않음). 그래서 `HAL_Delay`는 카운터가 움직이는지 확인하는 용도로만 쓰고 정밀도 기준으로 삼지 않는다.
- 보드 간 차이는 없다 (회차 사이 오차 약 150 사이클).

## 결론 (Lv1)

두 보드 모두 SystemCoreClock이 600 MHz이고, HAL_Delay(100) 구간에서 CYCCNT가 약 6021만 사이클(100.36 ms) 늘어서 카운터가 클럭대로 센다.
측정 공통 코드(measure/)는 Lv1에서는 컴파일만 확인했고 Lv2에서 보드 시험을 마쳤다. 성능 측정은 Lv2부터다.

## Lv1 검증 코드

ST 예제 `UART_Printf`의 `main.c`에서 `USER CODE BEGIN 2`와 `USER CODE END 2` 사이에 추가했다.

```c
  SystemCoreClockUpdate();
  printf("SystemCoreClock = %lu Hz\n\r", (unsigned long)SystemCoreClock);

  DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  uint32_t t0 = DWT->CYCCNT;
  HAL_Delay(100);
  uint32_t cycles = DWT->CYCCNT - t0;
  printf("HAL_Delay(100): %lu cycles, %lu us\n\r",
         (unsigned long)cycles,
         (unsigned long)(cycles / (SystemCoreClock / 1000000U)));
```

- 과제 PDF의 `CoreDebug->DEMCR`은 이 CMSIS 버전의 Cortex-M55 헤더(`core_cm55.h`)에 없다. `DCB` 이름을 쓴다 (`DCB_DEMCR_TRCENA_Msk`).
- 소수 출력(`%f`)은 이 빌드 설정에서 지원되지 않을 수 있어 정수 계산으로 µs를 구했다.

## DWT 측정 방법 (모든 Lv 공통)

- 실행 시간: 구간 앞뒤 CYCCNT 차이 (unsigned 뺄셈이라 한 바퀴 돌아도 맞게 나온다)
- 600 MHz에서 1 cycle ≈ 1.667 ns, 32비트 카운터는 약 7.16 s마다 오버플로 (2^32 / 600 MHz)
- 결과는 평균 / 최소 / 최대. 실시간 판단은 최대값 기준.
- 측정 중에는 UART 출력 금지. 배열에 저장했다가 끝난 뒤 한 번에 출력한다.

## 측정 공통 코드 (`measure/`)

`bench.h`, `bench.c`. Task 3 Lv2 벤치마크와 Task 2 Lv2의 레인징 시간 측정이 같이 쓴다.

```c
bench_init();                          // 처음 한 번: DWT CYCCNT 켜기
bench_reset();                         // 조건마다: 저장된 샘플 비우기
for (i = 0; i < BENCH_MAX_SAMPLES; i++) {
  bench_start();
  /* 측정할 코드 */
  bench_stop();                        // 사이클 차이를 배열에 저장 (출력하지 않음)
}
bench_report("matmul_4x4_O2");         // 끝난 뒤 한 번에: n, 평균·최소·최대 (cycle, µs)
bench_dump();                          // 선택: raw 샘플을 index,cycles CSV로 출력
```

| 함수 | 하는 일 |
| --- | --- |
| `bench_init()` | DWT 사이클 카운터를 켠다 (`DCB->DEMCR`의 TRCENA, `DWT->CTRL`의 CYCCNTENA) |
| `bench_reset()` | 저장된 샘플 개수를 0으로 되돌린다 |
| `bench_start()` | 지금의 CYCCNT를 기억한다 |
| `bench_stop()` | `CYCCNT - 기억한 값`을 샘플 하나로 저장한다 |
| `bench_record(c)` | 다른 곳에서 구한 값을 샘플로 저장한다 (예: 타이머 ISR의 주기 jitter 측정에서 연속 CYCCNT 차이) |
| `bench_report(name)` | 샘플 개수, 평균·최소·최대를 cycle과 µs(소수 셋째 자리)로 출력한다. 실시간 판단은 최대값 기준. 최대값이 나온 샘플 번호(`max at sample #N`, #0은 첫 반복)와, 최소보다 100사이클(`BENCH_OUTLIER_MARGIN`) 넘게 느린 샘플 개수도 출력한다 (Lv2에서 추가) |
| `bench_dump()` | 샘플을 `index,cycles` 줄로 출력한다. 조건별 raw CSV로 저장할 때 쓴다 |

- 최대 10,000개(`BENCH_MAX_SAMPLES`, 과제 PDF의 반복 횟수)까지 저장하고 넘치는 샘플은 버린다. 리포트의 `n`으로 확인한다. 샘플 배열은 약 40 KB(RAM)이다.
- 측정에는 `bench_start()`와 `bench_stop()` 호출 자체의 사이클이 들어간다. 아주 짧은 코드는 빈 구간을 같은 방법으로 먼저 재서 비교해야 한다.
- 한 번의 측정이 7.16 s(600 MHz에서 32비트 카운터 한 바퀴)를 넘으면 안 된다.
- 평균은 정수 사이클로 내림한다. 합은 64비트로 계산해서 10,000개를 더해도 넘치지 않는다.
- `printf`로 출력하므로 프로젝트에 `printf`가 UART로 연결돼 있어야 한다 (ST 예제 `UART_Printf` 방식).

**시험 상태:** Lv1에서는 ARM 컴파일러(`arm-none-eabi-gcc` 14.3.1)로 컴파일만 확인했다. **Lv2에서 보드(보드 2)에서 시험했다.** 자체 시험 결과(Tera Term 화면에서 옮겨 적음, 로그 파일 없음): 빈 구간(start+stop만) 평균 14 사이클(최소 14, 최대 58) = 측정 자체의 오버헤드, 휘발성 변수 1000번 반복 루프 평균 9,027 사이클(최소 9,026, 최대 9,109), `HAL_Delay(1)` 100회는 607,659 ~ 1,199,943 사이클로 이론 범위 1 ms ~ 2 ms(600,000 ~ 1,200,000 사이클) 안에 들어와 사이클 → 시간 환산이 맞다. 이후 모든 Lv2 측정에 쓰였고 결과가 두 번 재현됐다.

## Lv1 데이터

| 파일 | 내용 | 상태 |
| --- | --- | --- |
| `data/t3_clock.txt` | `SystemCoreClock` UART 출력 | ☑ |
| `data/t3_cyccnt_100ms.csv` | CYCCNT 검증 (`HAL_Delay(100)`) | ☑ |
| `measure/bench.h`, `measure/bench.c` | DWT 측정 공통 코드 (Lv2에서 보드 시험 완료) | ☑ |
| `photos/` | 보드 bring-up 사진 | ☑ |

## Lv2 체크리스트 (성능 측정)

- [x] 벤치마크 8종을 각 10,000회 측정, 평균·최소·최대 (cycle, µs): 행렬곱 4×4, 12×12, PID 3축 1회, `sinf`, `cosf`, `sqrtf`, `atan2f`, 자세 적분 1스텝 (쿼터니언 곱 + 정규화)
- [x] 최적화 -O0 / -O2 / -O3 비교 (캐시 켬)
- [~] I/D 캐시 켬 / 끔: **-O3에서만** 비교했다
- [~] 일반 C vs CMSIS-DSP: **행렬곱 2종, -O3, 캐시 켬에서만** 비교했다 (계산 결과가 같음을 확인)
- [x] 1 kHz 타이머 인터럽트 안에서 PID + 12×12 행렬곱을 10,000주기 실행: 주기 jitter, ISR 최대 실행 시간, 인터럽트 지연
- [x] "조건 × 벤치마크" 표 (아래)
- [~] 조건별 raw 데이터: 샘플 전체가 아니라 **요약 통계(n, 평균, 최소, 최대, 최대 위치)** 만 로그에 있다. 샘플 전체를 출력하는 `bench_dump()`는 쓰지 않았다

통과 기준 (PDF): 조건 × 벤치마크 표 하나로 정리됨.

## 결과 (Lv2)

측정 날짜: 2026-11-09, 보드 2 (COM6), 600 MHz, 각 항목 10,000회. 1 µs = 600 사이클. 로그는 `data/t3_lv2_*.txt`이고 파일 이름이 조건이다. 전체 표는 `data/t3_lv2_summary.csv`(µs 포함), 아래 표는 `tools/summarize_logs.py`가 로그에서 뽑은 것이다. 측정에는 `bench_start()`/`bench_stop()` 호출 자체의 약 14 사이클이 들어 있고 빼지 않았다.

**평균 (사이클)**

| 항목 | -O0, 캐시 켬 | -O2, 캐시 켬 | -O3, 캐시 켬 | -O3, 캐시 끔 |
| --- | ---: | ---: | ---: | ---: |
| matmul 4x4 f32 | 3,202 | 704 | 260 | 3,359 |
| matmul 12x12 f32 | 74,186 | 13,962 | 2,846 | 47,748 |
| PID 3-axis, 1 update | 703 | 161 | 139 | 1,551 |
| sinf | 184 | 171 | 171 | 614 |
| cosf | 204 | 191 | 191 | 669 |
| sqrtf | 64 | 31 | 31 | 233 |
| atan2f | 214 | 194 | 196 | 1,921 |
| attitude step (quat integrate + normalize) | 268 | 100 | 98 | 782 |
| matmul 4x4 f32 (CMSIS-DSP) | - | - | 142 | - |
| matmul 12x12 f32 (CMSIS-DSP) | - | - | 2,472 | - |

**최대 (사이클)**

| 항목 | -O0, 캐시 켬 | -O2, 캐시 켬 | -O3, 캐시 켬 | -O3, 캐시 끔 |
| --- | ---: | ---: | ---: | ---: |
| matmul 4x4 f32 | 3,515 | 885 | 935 | 3,815 |
| matmul 12x12 f32 | 80,753 | 14,738 | 4,772 | 48,180 |
| PID 3-axis, 1 update | 1,485 | 531 | 753 | 2,003 |
| sinf | 360 | 381 | 396 | 1,002 |
| cosf | 374 | 474 | 450 | 1,065 |
| sqrtf | 194 | 96 | 128 | 600 |
| atan2f | 1,044 | 1,062 | 997 | 2,319 |
| attitude step (quat integrate + normalize) | 903 | 425 | 411 | 1,151 |
| matmul 4x4 f32 (CMSIS-DSP) | - | - | 630 | - |
| matmul 12x12 f32 (CMSIS-DSP) | - | - | 3,832 | - |

**반복 측정 (-O3, 캐시 켬): 같은 조건을 두 번 잰 평균 (사이클)**

| 항목 | 1회 (`bench` 로그) | 2회 (`isr` 로그의 벤치마크 부분) |
| --- | ---: | ---: |
| matmul 4x4 f32 | 260 | 261 |
| matmul 4x4 f32 (CMSIS-DSP) | 142 | 142 |
| matmul 12x12 f32 | 2,846 | 2,846 |
| matmul 12x12 f32 (CMSIS-DSP) | 2,472 | 2,471 |
| PID 3-axis, 1 update | 139 | 139 |
| sinf | 171 | 171 |
| cosf | 191 | 191 |
| sqrtf | 31 | 31 |
| atan2f | 196 | 298 |
| attitude step (quat integrate + normalize) | 98 | 98 |

**1 kHz 인터럽트 시험 (-O3, 캐시 켬, n=10000)**

| 항목 | 평균 | 최소 | 최대 | 최대 (µs) | 최대 위치 |
| --- | ---: | ---: | ---: | ---: | ---: |
| ISR period (target 600000 cycles = 1 ms) | 599,999 | 599,895 | 600,034 | 1000.056 | #4973 |
| period deviation \|period - 600000\| (jitter) | 0 | 0 | 105 | 0.175 | #0 |
| ISR execution time (PID + 12x12 matmul) | 3,044 | 3,037 | 3,119 | 5.198 | #9557 |
| interrupt latency (CNT at handler start, in CPU cycles) | 43 | 43 | 79 | 0.131 | #877 |

### 관찰

- **최적화 효과 (측정)**: 우리가 짠 계산 코드는 -O0 → -O3에서 12×12 행렬곱 26배(74,186 → 2,846 사이클, 123.6 µs → 4.7 µs), 4×4 12배, PID 5.0배, 자세 적분 2.7배 빨라졌다. `sinf`, `cosf`, `atan2f`는 1.1배 안팎으로 거의 안 변했다. 미리 컴파일된 수학 라이브러리를 부르기 때문일 가능성이 크지만 라이브러리 소스를 직접 확인하지는 않았다.
- **-O3 행렬곱이 크게 빠른 이유 (역어셈블로 확인)**: 같은 옵션(`-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard`, MVE 켜짐)으로 따로 컴파일해 역어셈블하면 일반 C 곱셈 코드에 -O3에서는 Helium 벡터(q 레지스터)를 쓰는 명령이 323줄 있고 -O0, -O2에는 0개다. 컴파일러가 -O3에서 곱셈을 자동으로 벡터화한 것이다. 또 -O3는 행렬 크기(4, 12)를 컴파일 시점 상수로 아는 코드를 따로 만든다. 크기가 실행 중에 정해지는 코드는 이만큼 빠르지 않을 수 있다.
- **캐시 (측정)**: -O3에서 캐시를 끄면 우리가 짠 계산 코드가 7.5 ~ 17배 느려진다 (12×12 곱셈 2,846 → 47,748 사이클, 16.8배; `sqrtf` 7.5배). `sinf`, `cosf`는 3.5배. 제어 루프는 캐시를 켜 둔 상태여야 한다.
- **CMSIS-DSP (측정)**: `arm_mat_mult_f32`는 4×4에서 142 사이클로 일반 C(260)보다 1.8배, 12×12에서 2,472 사이클로 일반 C(2,846)보다 1.15배 빠르다. 두 결과의 최대 차이는 0 (1e-6 단위). 일반 C가 -O3에서 이미 벡터화돼 있어서 차이가 작다. 일반 C가 벡터화되지 않는 -O0/-O2에서는 비교하지 않았다.
- **최대값이 튀는 이유**: 캐시를 켠 측정에서는 최대값이 거의 항상 **첫 번째 반복(#0)** 이고 느린 반복은 0 ~ 몇 개뿐이다. 첫 반복이 비어 있는 캐시 때문에 느린 것으로 보이며 (추정), 최대 − 평균은 항목마다 약 100 ~ 1,900 사이클(0.2 ~ 3.2 µs)로 달랐다 (`sqrtf`의 최대는 첫 반복이 아니었다). 캐시를 끄면 최대 위치가 흩어지고 느린 반복 개수가 측정 중에 지나간 1 ms 틱 개수와 거의 같다 (4×4 55 / 약 56, 12×12 795 / 약 796, `atan2f` 33 / 약 32). 최대 − 최소가 약 370 ~ 460 사이클(0.6 ~ 0.8 µs)로 일정한 것을 보면 1 ms HAL 틱 인터럽트 비용으로 보이지만 인터럽트 코드를 직접 확인하지는 않았다.
- **원인 불명 (-O0 12×12)**: -O0, 캐시 켬의 12×12에서만 반복의 12.4%(1,239개)가 최소보다 100 사이클 넘게 느렸고, 최대 80,753 사이클(최소보다 약 6.6천 큼)은 샘플 #4001에서 나왔다. 삭제한 첫 번째 로그에서도 최대가 80,738로 비슷했다. 위의 틱 가설로는 설명되지 않는다 (-O2 12×12는 틱이 약 233번 있었는데 느린 반복이 7개뿐).
- **재현성**: 같은 -O3, 캐시 켬 조건을 두 번 쟀을 때 10개 항목 중 9개는 평균이 1 사이클 이내로 같다. **`atan2f`만 196 → 298 사이클로 평균과 최소가 함께 약 100 사이클(52%) 올랐다.** 두 번째는 인터럽트 시험 코드를 추가한 빌드다. 같은 `atan2f`가 -O0 214, -O2 194, 이전 -O3 측정 196이었다. 원인은 확인하지 못했다. 코드가 늘면서 미리 컴파일된 라이브러리 함수가 다른 주소로 옮겨져 캐시 줄 경계에 걸렸을 가능성이 있지만 이전 빌드의 `.elf`가 없어서 주소를 비교하지 못했다. 표에는 첫 번째 값(196)을 쓰고 두 값 모두 적었다.

## 결론 (Lv2)

-O3, 캐시 켬에서 12×12 행렬곱은 2,846 사이클(4.7 µs), PID 3축 1회는 139 사이클(0.23 µs)이고, 같은 코드가 -O0에서는 26배 느리고 캐시를 끄면 16.8배 느려진다. **1 kHz 인터럽트에서 PID + 12×12 행렬곱을 돌리면 최악 jitter 0.18 µs (105 사이클), ISR 최대 실행 5.2 µs, 제어 루프 CPU 점유 0.51 ~ 0.52%, 인터럽트 지연 최대 0.13 µs였다 (-O3, 캐시 켬, 다른 부하 없음, 두 번 측정에서 같은 값).**
이 값은 다른 인터럽트와 다른 코드의 캐시 오염이 없는 가장 좋은 조건이다. 실제 드론 조건(NPU 추론과 RTOS를 같이 돌림)은 Lv3에서 측정한다.

### 1 kHz 인터럽트 시험 설명

- TIM2를 레지스터로 직접 설정했다 (타이머 클럭 400 MHz, PSC = 0, ARR = 399,999, 우선순위 3). 시험 전에 `TIMPRES = DIV1`로 맞춘다 (TIM_TimeBase 예제와 같음).
- 핸들러 첫 줄에서 `CYCCNT`를 읽고(주기와 실행 시간의 기준), 이어서 `TIM2->CNT`를 읽는다(업데이트 이벤트 이후 지난 틱 = 인터럽트 지연, 400 MHz 틱이라 1.5 사이클 단위). 핸들러 안에서는 출력하지 않는다.
- 핸들러가 하는 일은 PID 3축 1회 + 12×12 행렬곱 1회다 (`bench_cases.c`의 `bench_isr_workload()`). 메인은 10,000주기가 끝날 때까지 기다린다 (약 10초).
- `ISR period` 요약의 "최소보다 100 사이클 넘게 느린 샘플" 개수(9,987 / 10,000)는 의미가 없다. 최소값 자체가 첫 번째 주기의 이상치(599,895)여서 기준이 이상치에 맞춰졌다. jitter는 `period deviation` 항목으로 본다.
- 주기 편차 최대 105 사이클은 첫 번째 주기(#0)에서만 나왔고 나머지 9,999개는 모두 100 사이클 이하다 (이 요약으로는 더 작은 값은 알 수 없다. 주기 최대는 600,034, 즉 +34). 첫 인터럽트의 시작이 비어 있는 캐시 때문에 늦은 것으로 보인다 (추정).
- 두 번 측정한 결과: 주기 편차 최대 106 / 105, 실행 시간 평균 3,044 / 3,044, 인터럽트 지연 최대 79 / 79 (둘 다 샘플 #877), CPU 점유 0.52 / 0.51%. 첫 번째 측정은 로그 파일을 일찍 닫아 저장되지 않았고 화면에서 확인한 값이다. 파일은 두 번째 측정이다.

### Lv2 한계

- **보드 2에서만 측정**했다. 보드 1은 Task 3 Lv2를 측정하지 않았다.
- 캐시 끔은 -O3에서만, CMSIS-DSP는 -O3 캐시 켬에서만 비교했다. -O0, -O2의 캐시 끔 측정과 일반 C의 비교 대상이 되는 낮은 최적화 수준의 CMSIS-DSP는 측정하지 않았다.
- -O0, -O2, -O3 캐시 끔 로그는 CMSIS-DSP와 인터럽트 시험 코드를 추가하기 **전의 빌드**로 측정했다. 공통 벤치마크 코드는 같고 `checksum = 8512`가 모든 로그에서 같다. 그래서 이 로그들에는 CMSIS-DSP 항목이 없다. 이전 -O3 캐시 켬 로그(CMSIS-DSP 추가 전)는 같은 이름으로 덮어썼다 (평균이 새 로그와 5 사이클 이내로 같았다).
- 로그 제목은 -O2와 -O3를 구분하지 못한다 (`optimized build`). -O3는 CubeIDE가 만든 빌드 파일에서 `-O3`를 확인했고, -O2는 속성 화면에서 설정한 값을 따랐다.
- 샘플 전체가 아니라 요약 통계만 로그에 남아 있다. 최대값 원인 분석(첫 반복인지 틱인지)은 `최대 위치`와 느린 반복 개수로만 추정했다.
- 측정 중에는 다른 인터럽트(UART, SPI 등)와 다른 코드가 없었다. 클럭은 600 MHz (ST 예제 설정)다.

## Lv2 코드 (`T3_Bench/`)

`UART_Printf` 예제를 복제해서 프로젝트 이름만 바꾼 CubeIDE 프로젝트다 (`T3_Bench_FSBL`). 빌드 결과물(`Debug/`)은 넣지 않았다.

| 파일 | 내용 |
| --- | --- |
| `T3_Bench/FSBL/Src/main.c` | 벤치마크와 인터럽트 시험을 순서대로 실행. `BENCH_CACHE`(1: 캐시 켬, 0: 끔) 스위치 |
| `T3_Bench/FSBL/Src/bench_cases.c`, `Inc/bench_cases.h` | 벤치마크 8종 + CMSIS-DSP 행렬곱 2종, 인터럽트 시험이 부르는 작업(`bench_isr_workload`) |
| `T3_Bench/FSBL/Src/bench_isr.c`, `Inc/bench_isr.h` | TIM2 1 kHz 인터럽트 시험 |
| `T3_Bench/FSBL/Src/bench.c`, `Inc/bench.h` | `measure/`의 복사본 (같은 파일) |
| `T3_Bench/STM32CubeIDE/FSBL/.project`, `.cproject` | 프로젝트 설정. `bench*.c`와 `arm_mat_mult_f32.c`(CMSIS-DSP) 연결, DSP 인클루드 경로 |
| `tools/summarize_logs.py` | `data/t3_lv2_*.txt`에서 `data/t3_lv2_summary.csv`와 위 표를 만든다 |

- 일반 C 커널은 `noinline` 함수로 두고, 입력은 `volatile` 변수에서 읽고, 측정 구간 앞뒤에 컴파일러 메모리 장벽을 두었다 (-O3에서 계산이 지워지거나 구간 밖으로 옮겨지는 것을 막기 위해). `checksum = 8512`가 모든 조건에서 같아 계산 결과가 달라지지 않은 것을 확인했다. -O2/-O3 코드에서도 커널 함수가 사라지지 않고 호출됨을 역어셈블로 확인했다 (이름에 `.constprop`이 붙은 복제본).
- 출력 제목이 `-O0 build`면 최적화 없음, `optimized build`면 -O2나 -O3다. 캐시 상태(`I-cache on/off, D-cache on/off`)는 실행 중에 레지스터에서 읽어 찍는다.

## Lv2 데이터

| 파일 | 내용 | 상태 |
| --- | --- | --- |
| `data/t3_lv2_bench_O0_cacheon_board2.txt` | 벤치마크 -O0, 캐시 켬 (CMSIS-DSP 추가 전 빌드) | ☑ |
| `data/t3_lv2_bench_O2_cacheon_board2.txt` | 벤치마크 -O2, 캐시 켬 (CMSIS-DSP 추가 전 빌드) | ☑ |
| `data/t3_lv2_bench_O3_cacheon_board2.txt` | 벤치마크 -O3, 캐시 켬, CMSIS-DSP 포함 | ☑ |
| `data/t3_lv2_bench_O3_cacheoff_board2.txt` | 벤치마크 -O3, 캐시 끔 (CMSIS-DSP 추가 전 빌드) | ☑ |
| `data/t3_lv2_isr_O3_cacheon_board2.txt` | 벤치마크(2회째) + 1 kHz 인터럽트 시험, -O3, 캐시 켬 | ☑ |
| `data/t3_lv2_summary.csv` | 위 로그의 모든 항목 요약 (µs 포함) | ☑ |
| `T3_Bench/`, `tools/summarize_logs.py` | 코드 | ☑ |

## 사진

개발 모드로 디버그가 연결된 상태의 보드. USB-C는 CN10(ST-LINK)에 연결.

| 파일 | 내용 |
| --- | --- |
| [`photos/t3_board1_overview.jpg`](photos/t3_board1_overview.jpg) | 보드 1 전체. USB-C 연결, 전원·통신 LED 켜짐 |
| [`photos/t3_board1_jumpers.jpg`](photos/t3_board1_jumpers.jpg) | 보드 1 점퍼 확대. JP1(BOOT0), JP2(BOOT1) 캡 위치 |
| [`photos/t3_board2_overview.jpg`](photos/t3_board2_overview.jpg) | 보드 2 전체. USB-C 연결, 전원·통신 LED 켜짐, 점퍼 위치 |

## 빌드·플래싱

- 도구: STM32CubeIDE 2.2.0, STM32CubeMX 6.18.1, STM32CubeProgrammer 2.23.0, Tera Term 5.7.1.
- 예제 패키지: STM32CubeN6 1.4.1. 1.4.1이 1.4.0에 의존하는 것으로 보여 둘 다 설치했다 (CubeMX의 Help > Manage embedded software packages, 라이선스 동의 필요).
- 예제 위치: `STM32Cube\Repository\STM32Cube_FW_N6_V1.4.1\Projects\NUCLEO-N657X0-Q\Examples\` 아래 `GPIO\GPIO_IOToggle`, `UART\UART_Printf`.
- 가져오기: CubeIDE에서 별도 작업공간(예: `workspace_hwbench`)을 만들고 File > Import > General > Existing Projects into Workspace. 루트는 예제의 `STM32CubeIDE\FSBL` 폴더. **"Copy projects into workspace"는 해제한다.** 예제가 드라이버를 상대 경로로 참조해서 복사하면 빌드가 깨진다.
- 실행: 프로젝트 선택 > Project > Build Project > Run > Debug As > STM32 C/C++ Application. `main()`에서 멈추면 Resume(F8).
- ST-LINK 펌웨어 확인 창이 뜨면 Yes. STLinkUpgrade에서 Open in update mode, Upgrade. 보드마다 한 번씩 한다 (보드 1은 V3J17M11로 올렸다).
- 터미널: Tera Term, 시리얼 포트는 `STLink Virtual COM Port`. **`UART_Printf` 예제 설정은 115200, 데이터 7비트, 홀수(odd) 패리티, 정지 비트 1, 흐름 제어 없음**이다 (8N1이 아니다). 설정이 다르면 글자가 깨진다.
- 서명: 개발 모드의 디버그 실행에는 필요 없다. 전원을 다시 켜도 도는 플래시 부팅용이고, 예제 README의 명령은 `STM32_SigningTool_CLI.exe -bin Project.bin -nk -of 0x80000000 -t fsbl -o Project-trusted.bin -hv 2.3 -dump Project-trusted.bin`이며 CubeProgrammer 2.21 이상은 `-align`을 추가한다. 이번에는 쓰지 않았다.

- Lv2 프로젝트(`T3_Bench`): repo의 `T3_Bench/` 폴더를 `STM32Cube\Repository\STM32Cube_FW_N6_V1.4.1\Projects\NUCLEO-N657X0-Q\Examples\UART\T3_Bench`로 복사한다 (드라이버를 상대 경로로 참조하므로 다른 위치에서는 빌드가 깨진다). 그 안의 `STM32CubeIDE\FSBL`을 Import한다 ("Copy projects into workspace" 해제).
- 최적화 수준: 프로젝트 Properties > C/C++ Build > Settings > Tool Settings > MCU/MCU GCC Compiler > Optimization > Optimization level (Debug 구성). 바꾼 뒤에는 Clean 후 Build.
- 캐시 끄기: `main.c`의 `BENCH_CACHE`를 0으로 바꾸고 Clean 후 Build. 출력의 `I-cache off, D-cache off`로 확인한다.
- 실행은 프로젝트를 선택하고 **우클릭 > Debug As > STM32 Cortex-M C/C++ Application**으로 한다. 위쪽 벌레 아이콘은 선택한 프로젝트가 아니라 마지막에 실행한 설정을 다시 실행해서 다른 프로젝트(예: `TIM_TimeBase`)가 올라갈 수 있다.
- 로그: `main`에서 멈춘 뒤 Resume 직전에 Tera Term 로그(파일 > 로그)를 켠다. 그 전에 켜면 이전 프로그램의 출력이 섞인다. 인터럽트 시험은 벤치마크 뒤 약 10초 동안 출력이 없으니 `== interrupt test done ==`이 나온 뒤에 로그를 닫는다.
- 프로젝트 파일(`.project`, `.cproject`)을 바꾼 뒤에는 Close Project 후 Open Project로 다시 읽힌다.

## 확인 필요

- PDF 레이아웃상 "DWM3001CDK 결과와 비교"와 "레인징 1회 DWT 측정" 항목이 Lv1인지 Lv2인지 불명확. 현재 Lv2로 둠.
- 사용자 매뉴얼 UM3417 본문을 직접 열지 못했다. LED 이름(보드 인쇄 `LED1`~`LED3`)과 회로도상 이름(`LD5`~`LD7`)의 대응, 버튼 핀은 UM3417로 대조해야 한다.
- 막힌 곳은 `../삽질기록.md`에 남긴다.
- Lv2 한계는 "Lv2 한계" 항목 참고. 특히 `atan2f` 재현성(196 → 298 사이클)과 -O0 12×12의 느린 반복 12%는 원인을 확인하지 못했다.
- 보드 1에서는 Task 3 Lv2를 측정하지 않았다.
