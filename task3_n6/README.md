# Task 3: STM32N6 실시간 성능

목표: 드론 제어 루프를 돌리면서 NPU 추론을 같이 돌릴 수 있는지 MCU 내장 사이클 카운터(DWT CYCCNT)로 측정한다.
원본 과제: `UWB · STM32N6 · BLE 과제.pdf` (2026-11-05).

## 진행 현황 (2026-11-09)

| 레벨 | 상태 | 비고 |
| --- | --- | --- |
| Lv1 동작 확인 | 진행 중 | 보드 2개 bring-up, 클럭, CYCCNT 검증 완료. 측정 공통 코드와 배치 사진이 남음 |
| Lv2 성능 측정 | 시작 전 | |
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
- [ ] `measure/`에 공통 측정 코드 (시작·종료·통계·일괄 출력). 측정 중 UART 출력 금지, 배열에 저장 후 출력
- [ ] 배치 사진 `photos/`

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

## Lv1 데이터

| 파일 | 내용 | 상태 |
| --- | --- | --- |
| `data/t3_clock.txt` | `SystemCoreClock` UART 출력 | ☑ |
| `data/t3_cyccnt_100ms.csv` | CYCCNT 검증 (`HAL_Delay(100)`) | ☑ |
| `measure/` | DWT 측정 공통 코드 | ☐ |
| `photos/` | 보드 bring-up 사진 | ☐ |

## 빌드·플래싱

- 도구: STM32CubeIDE 2.2.0, STM32CubeMX 6.18.1, STM32CubeProgrammer 2.23.0, Tera Term 5.7.1.
- 예제 패키지: STM32CubeN6 1.4.1. 1.4.1이 1.4.0에 의존하는 것으로 보여 둘 다 설치했다 (CubeMX의 Help > Manage embedded software packages, 라이선스 동의 필요).
- 예제 위치: `STM32Cube\Repository\STM32Cube_FW_N6_V1.4.1\Projects\NUCLEO-N657X0-Q\Examples\` 아래 `GPIO\GPIO_IOToggle`, `UART\UART_Printf`.
- 가져오기: CubeIDE에서 별도 작업공간(예: `workspace_hwbench`)을 만들고 File > Import > General > Existing Projects into Workspace. 루트는 예제의 `STM32CubeIDE\FSBL` 폴더. **"Copy projects into workspace"는 해제한다.** 예제가 드라이버를 상대 경로로 참조해서 복사하면 빌드가 깨진다.
- 실행: 프로젝트 선택 > Project > Build Project > Run > Debug As > STM32 C/C++ Application. `main()`에서 멈추면 Resume(F8).
- ST-LINK 펌웨어 확인 창이 뜨면 Yes. STLinkUpgrade에서 Open in update mode, Upgrade. 보드마다 한 번씩 한다 (보드 1은 V3J17M11로 올렸다).
- 터미널: Tera Term, 시리얼 포트는 `STLink Virtual COM Port`. **`UART_Printf` 예제 설정은 115200, 데이터 7비트, 홀수(odd) 패리티, 정지 비트 1, 흐름 제어 없음**이다 (8N1이 아니다). 설정이 다르면 글자가 깨진다.
- 서명: 개발 모드의 디버그 실행에는 필요 없다. 전원을 다시 켜도 도는 플래시 부팅용이고, 예제 README의 명령은 `STM32_SigningTool_CLI.exe -bin Project.bin -nk -of 0x80000000 -t fsbl -o Project-trusted.bin -hv 2.3 -dump Project-trusted.bin`이며 CubeProgrammer 2.21 이상은 `-align`을 추가한다. 이번에는 쓰지 않았다.

## 확인 필요

- PDF 레이아웃상 "DWM3001CDK 결과와 비교"와 "레인징 1회 DWT 측정" 항목이 Lv1인지 Lv2인지 불명확. 현재 Lv2로 둠.
- 사용자 매뉴얼 UM3417 본문을 직접 열지 못했다. LED 이름(보드 인쇄 `LED1`~`LED3`)과 회로도상 이름(`LD5`~`LD7`)의 대응, 버튼 핀은 UM3417로 대조해야 한다.
- 막힌 곳은 `../삽질기록.md`에 남긴다.
