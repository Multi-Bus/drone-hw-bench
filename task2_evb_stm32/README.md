# Task 2: DWM3000EVB + STM32 (STM32 학습)

## 보드

- 사용 보드: NUCLEO-N657X0-Q (N6). 일반 STM32 보드 없음. N6는 부팅 방식이 달라서 Task 3 bring-up 후 진행.
- 점퍼: JP1 = BOOT0, JP2 = BOOT1. 개발 모드 BOOT0=0, BOOT1=1 (전원 켜기 전에 설정).
- 콘솔: USART1 (PE5 TX, PE6 RX), 115200 8N1, ST-LINK 가상 COM.
- 사용자 LED: LD5 빨강 PG10, LD6 초록 PG0, LD7 파랑 PG8 (active low). 사용자 버튼 B1 PC13 (active high).
- 핀 출처: NuttX 문서. UM3417 점퍼·핀 표로 대조 필요 (TODO).

## Lv1 체크리스트 (동작 확인)

- [ ] STM32CubeIDE 설치, 보드 인식 확인
- [ ] `01_led/`: GPIO LED 깜빡임
- [ ] `02_uart/`: UART `printf` 출력 (115200, 줄 끝 `\r\n`)
- [ ] `03_exti/`: 버튼 EXTI로 LED 토글 (B1은 Rising edge, 50 ms 디바운스)
- [ ] `04_timer/`: 1 kHz 타이머 인터럽트, 1초마다 UART 출력 (ISR에서 printf 금지)
- [ ] 단계별 CubeMX 설정 화면 캡처 → `docs/`, README에 정리

통과 기준 (PDF): LED, UART, EXTI, 1 kHz 타이머 4가지 동작과 설정 캡처가 README에 있음.

## Lv1 데이터

| 파일 | 내용 | 상태 |
| --- | --- | --- |
| `01_led/` 소스 | LED 프로젝트 | ☐ |
| `02_uart/` 소스 | UART 프로젝트 | ☐ |
| `03_exti/` 소스 | EXTI 프로젝트 | ☐ |
| `04_timer/` 소스 | 타이머 프로젝트 | ☐ |
| `docs/` | CubeMX 캡처 | ☐ |
| 보드 배치 사진 | 배선·보드 사진 | ☐ |

## 빌드·플래싱

TODO: CubeIDE 프로젝트 가져오기 방법, 빌드 설정, 플래싱 방법 기록.
