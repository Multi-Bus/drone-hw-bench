# Task 2: DWM3000EVB + STM32 (STM32 학습)

목표: STM32 개발 흐름을 익히고, 드론 펌웨어의 바탕이 될 UWB 드라이버를 STM32에서 돌린다. DWM3000EVB는 MCU가 없어서 호스트 STM32가 필요하다.
원본 과제: `UWB · STM32N6 · BLE 과제.pdf` (2026-11-05).

## 진행 현황 (2026-11-09)

| 레벨 | 상태 | 비고 |
| --- | --- | --- |
| Lv1 STM32 기초 | 완료 | LED, UART, EXTI, 1 kHz 타이머 동작 확인. CubeMX 캡처는 EXTI만 없음 (아래 설명) |
| Lv2 UWB 연결 | 시작 전 | Task 1 로깅 스크립트(`task1-uwb-lv1` 브랜치)가 필요 |
| Lv3 보드 비교 | 시작 전 | |

## 보드

- 사용 보드: NUCLEO-N657X0-Q (Task 3과 같은 보드). 일반 STM32 보드 없음. Lv1은 **보드 2 (가상 COM6)** 에서 먼저 하고, 이어서 **보드 1 (가상 COM5)** 에서 EXTI와 `04_timer`를 같은 방법으로 확인했다.
- 점퍼: JP1 = BOOT0, JP2 = BOOT1. 개발 모드는 BOOT1 = 1 (ST 예제 README 기준 JP2 2-3, BOOT0는 상관없음). 자세한 내용은 `../task3_n6/README.md`.
- 개발 모드에서 올린 프로그램은 RAM에서 돈다. **RESET 버튼을 누르면 프로그램이 사라져서** 아무것도 실행되지 않는다. 다시 보려면 디버그를 Terminate하고 Debug를 새로 시작한다.
- 콘솔: USART1 (PE5 TX, PE6 RX), 115200, ST-LINK 가상 COM. 설정은 `8 Bits (including Parity)` + `Odd` + 1 stop, 즉 데이터 7 bit + 홀수 패리티다 (`UART_Printf.ioc` 캡처 `docs/t2_lv1_uart.png`). Tera Term은 7 bit / odd / 1 / none으로 맞춘다. 초안의 `8N1`이 아니다.
- 사용자 LED: `GPIO_IOToggle` 예제와 CubeMX `.ioc`에서 PG8이 `LED1` (파란색, 실제 깜빡임 확인). 초안에 적힌 LD5 PG10, LD6 PG0, active low 표기는 UM3417과 대조하지 못했다.
- 사용자 버튼: `GPIO_EXTI` 예제 README에 PC13이 USER 버튼이라고 적혀 있다.

## Lv1 체크리스트 (동작 확인)

- [x] STM32CubeIDE 설치, 보드 인식 확인 (Task 3 Lv1에서 완료)
- [x] GPIO LED 깜빡임: ST 예제 `GPIO_IOToggle`, LED1 (Task 3 Lv1에서 완료, 보드 2개)
- [x] UART `printf` 출력: ST 예제 `UART_Printf` (Task 3 Lv1)와 `04_timer`의 1초 출력
- [x] 버튼 EXTI로 LED 토글: ST 예제 `GPIO_EXTI`, 버튼 조작으로 LED1이 바뀜. 보드 2와 보드 1에서 확인했고 버튼을 **뗄 때** 바뀐다 (육안 확인, 파일 기록 없음)
- [x] `04_timer/`: 1 kHz 타이머 인터럽트, 1초마다 UART 출력, 인터럽트 안에서 printf 안 씀. 로그 `data/t2_tim_1khz_log_board2.txt`, `data/t2_tim_1khz_log_board1.txt` (보드 2개)
- [x] 단계별 CubeMX 설정 화면 캡처 → `docs/` (LED, UART, TIM2). EXTI는 `.ioc`에 설정이 없어 코드로 대체

통과 기준 (PDF): LED, UART, EXTI, 1 kHz 타이머 4가지 동작과 설정 캡처가 README에 있음.

## 단계별 내용

ST 예제는 STM32CubeN6 1.4.1의 `Projects/NUCLEO-N657X0-Q/Examples/` 아래에 있다. 수정하지 않은 예제는 소스를 이 repo에 복사하지 않았다.

| 단계 | 사용한 예제 | 수정 | 이 repo의 파일 |
| --- | --- | --- | --- |
| GPIO LED | `GPIO/GPIO_IOToggle` | 없음 | `docs/t2_lv1_gpio_led.png` |
| UART printf | `UART/UART_Printf` | Task 3에서 클럭·CYCCNT 출력만 추가 (`../task3_n6/`) | `docs/t2_lv1_uart.png`, `docs/t2_lv1_uart_pins.png` |
| EXTI 버튼 | `GPIO/GPIO_EXTI` | 없음 | 아래 코드 인용 |
| 1 kHz 타이머 + UART | `TIM/TIM_TimeBase` | 있음 (아래) | `04_timer/`, `data/` |

### EXTI

`GPIO_EXTI.ioc`를 CubeMX로 열어 보면 PC13이 어느 컨텍스트에도 배정돼 있지 않다 (Resource 표의 FSBL, Appli 칸이 비어 있음). 이 예제는 EXTI를 CubeMX가 아니라 `main.c`의 HAL 코드로 설정한다. 그래서 CubeMX 캡처 대신 설정 코드를 적는다.

```c
static void EXTI13_IRQHandler_Config(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull = GPIO_PULLDOWN;
  GPIO_InitStructure.Pin = BUTTON_USER_PIN;          // PC13
  HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

  HAL_NVIC_SetPriority(EXTI13_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI13_IRQn);
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BUTTON_USER_PIN)
  {
    BSP_LED_Toggle(LED1);
  }
}
```

- 인터럽트가 falling edge에서 발생하고, 콜백은 LED1을 토글한다.
- 초안의 "Rising edge, 50 ms 디바운스"는 PDF에 없는 조건이다 (PDF: 버튼 누르면 LED 토글). 예제는 falling edge이고 디바운스를 구현하지 않았다.
- **LED1은 버튼을 누를 때가 아니라 뗄 때 바뀐다** (보드 1에서 버튼을 누른 채 유지하며 확인). 예제 README는 "누를 때 토글"이라고 적었지만 이 보드에서는 다르다. 초안대로 B1이 active high(누르면 High)이고 예제가 falling edge로 설정돼 있으니 뗄 때 falling edge가 생긴다는 것과 앞뒤가 맞다. 버튼 회로는 UM3417과 대조하지 않았다.
- 누를 때 반응하게 하려면 `GPIO_MODE_IT_RISING`과 `HAL_GPIO_EXTI_Rising_Callback`으로 바꾸면 되지만, 이 repo에서는 바꾸지 않았다 (PDF 조건은 "버튼 누르면 LED 토글"이라서 필요하면 Lv2 전에 정리).

### 1 kHz 타이머 + UART (`04_timer/`)

`TIM_TimeBase` 예제를 바탕으로 했다. 예제는 TIM2를 1 Hz로 돌리고 LED1을 1초마다 토글하며 UART는 쓰지 않는다. 변경한 내용은 `04_timer/changes_vs_example.diff`에 있고, 수정한 파일 전체는 `04_timer/src/`에 있다.

| 파일 | 변경 |
| --- | --- |
| `main.h` | `PERIOD_VALUE` 9999 → 9. 카운터 10 kHz ÷ 10 = 1 kHz (`PRESCALER_VALUE`는 그대로, 타이머 입력 400 MHz ÷ 40000 = 10 kHz) |
| `main.c` | 인터럽트 콜백은 틱만 센다. 1000번째 틱에서 CYCCNT를 저장하고 플래그를 세운다. 메인 루프가 플래그를 보고 `printf`, LED1 토글. USART1 초기화와 `printf` 연결(`__io_putchar`) 추가 |
| `stm32n6xx_hal_msp.c` | `UART_Printf` 예제에서 가져온 USART1 핀 설정 (PE5 TX, PE6 RX) |
| `stm32n6xx_hal_conf.h` | `HAL_UART_MODULE_ENABLED` 켬 |
| `.project` (diff에만 있음) | 빌드에 `stm32n6xx_hal_uart.c`, `stm32n6xx_hal_uart_ex.c` 추가 |

`TIM_TimeBase`는 UART 모듈이 꺼져 있고 UART HAL 소스도 빌드에 들어 있지 않아서, 위 두 항목(`hal_conf.h`, `.project`)이 없으면 UART를 쓸 수 없다. `.project`를 바꾼 뒤에는 CubeIDE에서 Close Project 후 Open Project로 다시 읽혀야 한다.

핵심 코드 (인터럽트에서는 printf를 쓰지 않는다):

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  tick_count++;
  if ((tick_count % 1000U) == 0U)
  {
    cyccnt_at_second = DWT->CYCCNT;
    second_flag = 1;
  }
}

/* main loop */
if (second_flag)
{
  second_flag = 0;
  uint32_t now = cyccnt_at_second;
  seconds++;
  printf("t=%lu s, ticks=%lu, CYCCNT delta=%lu\n\r", (unsigned long)seconds,
         (unsigned long)tick_count, (unsigned long)(now - cyccnt_prev));
  cyccnt_prev = now;
  BSP_LED_Toggle(LED1);
}
```

## 결과

측정 날짜: 2026-11-09. `data/t2_tim_1khz_log_board*.txt`는 Tera Term 로그 파일이다 (옮겨 적은 값이 아님). 두 보드에 같은 `04_timer` 코드를 올렸다.

| 항목 | 보드 2 (COM6) | 보드 1 (COM5) |
| --- | --- | --- |
| 로그 파일 | `data/t2_tim_1khz_log_board2.txt` | `data/t2_tim_1khz_log_board1.txt` |
| 로그 줄 수 | 28 (`t = 378 ~ 405 s`) | 23 (`t = 35 ~ 57 s`) |
| `CYCCNT delta` | 28줄 모두 600000000 (= 600 MHz × 1 s) | 23줄 모두 600000000 |
| `ticks` | 모든 줄에서 `t × 1000`과 일치 | 모든 줄에서 `t × 1000`과 일치 |

로그는 프로그램을 시작하고 한참 뒤에 켜서 `t`가 1부터 시작하지 않는다. 보드 2는 약 6분, 보드 1은 약 35초 뒤다. 보드 2는 405초 동안, 보드 1은 57초 동안 인터럽트를 놓치지 않았다 (`ticks`가 `t × 1000`과 일치하는 것 기준, 로그에 찍힌 구간만 확인).

해석과 한계:

- 1000틱이 CPU 600 MHz 기준으로 정확히 1초다. 1 kHz 설정이 맞다.
- CPU 클럭(600 MHz)과 타이머 클럭(400 MHz)이 같은 PLL1에서 나온다 (예제 코드 기준, 비율 3:2). 그래서 델타가 정확히 같은 것은 설정이 맞다는 증거이고, 실제 시계 기준 절대 정확도를 증명하지는 않는다.
- 인터럽트 진입 지연의 흔들림(jitter)은 이 측정으로 알 수 없다. Task 3 Lv2에서 측정한다.
- 타이머 입력이 400 MHz라는 것은 예제 `main.h`의 매크로(`400000000`)에서 읽은 값이다. 클럭 트리로 따로 확인하지 않았다.

## CubeMX 캡처 (`docs/`)

복사본 `.ioc`를 열어서 찍었다 (원본은 건드리지 않고 Generate Code도 하지 않았다).

| 파일 | 내용 |
| --- | --- |
| `t2_lv1_gpio_led.png` | `GPIO_IOToggle.ioc`: PG8, Output Push Pull, 풀 없음, Very High, 라벨 LED1 |
| `t2_lv1_uart.png` | `UART_Printf.ioc`: USART1 115200, 8 Bits (including Parity), Odd, 1 stop |
| `t2_lv1_uart_pins.png` | 같은 파일: PE5, PE6이 USART1 핀 (TX/RX 구분은 `stm32n6xx_hal_msp.c` 주석 기준) |
| `t2_lv1_tim2.png` | `TIM_TimeBase.ioc`에서 Prescaler 39999, Counter Period 9로 **복사본에서 직접 바꾼** 화면. 원본 `.ioc`는 매크로 이름이고 값은 9999다. 우리 코드와 맞춘 값이다 |
| `t2_lv1_tim2_nvic.png` | TIM2 global interrupt 켜짐, 우선순위 3 / 0 (예제 코드 `HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0)`와 같음) |

## Lv1 데이터

| 파일 | 내용 | 상태 |
| --- | --- | --- |
| `04_timer/src/` | 수정한 파일 4개 | 완료 |
| `04_timer/changes_vs_example.diff` | 원본 예제와의 차이 (`.project` 포함) | 완료 |
| `data/t2_tim_1khz_log_board2.txt` | 보드 2의 1 kHz 타이머 UART 로그 28줄 | 완료 |
| `data/t2_tim_1khz_log_board1.txt` | 보드 1의 같은 로그 23줄 | 완료 |
| `docs/` | CubeMX 캡처 5장 (EXTI는 해당 없음) | 완료 |
| 보드 배치 사진 | 배선·보드 사진 | Task 3 사진 참조 (`../task3_n6/photos/`, 별도 브랜치) |

## 빌드·플래싱

1. CubeIDE에서 별도 workspace를 쓴다 (다른 과제와 섞이지 않게).
2. File > Import > Existing Projects into Workspace. 루트 경로에 예제의 `STM32CubeIDE\FSBL` 폴더를 직접 입력하고 "Copy projects into workspace"는 해제한다 (드라이버 상대 경로 때문).
   - 예: `...\STM32Cube_FW_N6_V1.4.1\Projects\NUCLEO-N657X0-Q\Examples\TIM\TIM_TimeBase\STM32CubeIDE\FSBL`
3. `04_timer`는 `TIM_TimeBase` 예제를 가져온 뒤 `04_timer/src/`의 4개 파일로 예제의 `FSBL\Src`, `FSBL\Inc` 해당 파일을 덮어쓰고, `.project`에 UART HAL 파일 2개를 추가한다 (`changes_vs_example.diff` 참조). `.project`는 Close Project 후 Open Project로 다시 읽는다.
4. Clean 후 Build, Debug로 실행하고 Resume. Tera Term은 115200 / 7 bit / odd / 1 / none.
5. ST-LINK 펌웨어 확인 창이 뜨면 `../task3_n6/README.md`의 방법대로 처리한다.

## 확인 필요

- 뗄 때 반응하는 것은 보드 1에서만 확인했다. 보드 2는 버튼을 눌러 LED가 바뀌는 것만 봤고 누를 때인지 뗄 때인지는 보지 않았다.
- LED 이름 (초안의 LD5/LD6/LD7 표기와 보드 인쇄 LED1~LED3), active low 여부는 UM3417과 대조하지 못했다.
- EXTI 동작은 육안 확인이고 파일 기록이 없다.
- DWM3000EVB와 N6 보드 헤더의 핀 호환 (Lv2 배선)은 조사하지 않았다.
