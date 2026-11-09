# Task 2 Lv2: STM32N6 ↔ DWM3000EVB 핀맵 (초안, 배선 전)

**상태: 검증 전.** 이 표의 어떤 값도 실물 보드와 회로도로 확인되지 않았다. 확인 전에는 배선하거나 전원을 넣지 않는다.

## 1. 확인 상태 요약

| 항목 | 상태 | 근거 |
| --- | --- | --- |
| N6 Nucleo의 Arduino 헤더 ↔ STM32N657 핀 | 제3자 자료로 찾음, **ST 자료로 대조 필요** | 아래 2절 (Zephyr 저장소) |
| Arduino SPI(D10~D13)가 SPI5 | 같은 자료에서 일치 | 아래 2절 |
| DWM3000EVB의 Arduino 핀 ↔ 신호(IRQ, RSTn, WAKEUP 등) | **미확인** | Qorvo 문서를 열지 못함 (3절) |
| DWM3000EVB 전원 점퍼(J1) 위치와 기능 | **미확인** | 같음 |
| N6 Arduino 헤더 핀의 I/O 전압 (1.8 V / 3.3 V) | **미확인** | UM3417과 회로도를 열지 못함 |
| 기대 Device ID | **미확인** | Qorvo 드라이버 소스에서 찾아야 함 |

## 2. N6 쪽: NUCLEO-N657X0-Q Arduino 헤더

출처: Zephyr의 이 보드 정의 [`arduino_r3_connector.dtsi`](https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/boards/st/nucleo_n657x0_q/arduino_r3_connector.dtsi)와 [`nucleo_n657x0_q-common.dtsi`](https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/boards/st/nucleo_n657x0_q/nucleo_n657x0_q-common.dtsi) (2026-11-09에 읽음). 오픈소스 커뮤니티 자료이고 ST 공식 문서가 아니다. UM3417 또는 [회로도 팩](https://www.st.com/resource/en/schematic_pack/mb1940-n657x0q-c02-schematic.pdf)의 Arduino 시트로 대조해야 한다.

| Arduino 핀 | STM32N657 핀 | 비고 (Zephyr 기본 매핑 기준) |
| --- | --- | --- |
| D0 | PD9 | USART3 RX |
| D1 | PD8 | USART3 TX |
| D2 | PD0 | |
| D3 | PE9 | |
| D4 | PE0 | |
| D5 | PE10 | |
| D6 | PD5 | |
| D7 | PE11 | |
| D8 | PD12 | |
| D9 | PD7 | |
| **D10** | **PA3** | **SPI5_NSS** |
| **D11** | **PG2** | **SPI5_MOSI** |
| **D12** | **PG1** | **SPI5_MISO** |
| **D13** | **PE15** | **SPI5_SCK** |
| D14 | PC1 | I2C1 SDA |
| D15 | PH9 | I2C1 SCL |
| A0 ~ A5 | PA8, PA9, PA10, PA12, PF3, PG15 | A1, A2는 ADC1 |

- Zephyr 문서의 SPI5 기본 매핑도 `SPI5_SCK = PE15`, `SPI5_MOSI = PG2`, `SPI5_MISO = PG1`, `SPI5_NSS = PA3`으로 같다 ([Zephyr 보드 문서](https://docs.zephyrproject.org/latest/boards/st/nucleo_n657x0_q/doc/index.html)). Zephyr가 이 핀들에 SPI5 기능을 지정했으므로 SPI5로 쓸 수 있을 가능성이 높지만, **CubeMX에서 각 핀의 SPI5 Alternate Function이 선택되는지 직접 확인해야 한다.**
- 우리가 Lv1에서 확인한 것과 Zephyr 정의가 일치하는 부분: 콘솔 USART1 = PE5(TX), PE6(RX), 파란 LED = PG8 (Zephyr `User LD7`, 우리 BSP에서는 `LED1`, active low), 사용자 버튼 = PC13 (Zephyr: active high, 풀다운). 버튼이 active high여서 `GPIO_EXTI` 예제의 falling edge가 버튼을 뗄 때 반응한다는 Lv1 설명과 앞뒤가 맞다.
- Arduino 헤더에 SPI5 외에 쓸 수 있는 핀(IRQ, RSTn, WAKEUP용): D2 ~ D9. IRQ는 EXTI가 가능한 핀이어야 하는데 이 조건도 CubeMX에서 확인해야 한다.

## 3. DWM3000EVB 쪽

- [Qorvo 제품 페이지](https://www.qorvo.com/products/ek/DWM3000EVB)에서 확인된 것: Arduino Uno 폼팩터 호환 실드, 온보드 3V3 DC-DC, **전원 선택과 소비 전류 측정용 점퍼**, 모든 모듈 핀이 Arduino 커넥터로 나옴. 이전 이름은 DWS3000.
- 신호 ↔ Arduino 핀 표와 점퍼 기능은 **Quick Start Guide (Rev A, 05/2021)** 와 **회로도 (DWS1000 & DWS3000 Schematics, 2020-03-12)** 에 있다. 같은 페이지의 문서 링크는 `https://www.qorvo.com/products/r/ra006825`(Quick Start Guide), `https://www.qorvo.com/products/r/ra006729`(회로도)다. 자동 접근이 막혀서(HTTP 429, 내장 브라우저 접근 거부) 내용을 읽지 못했다. 사람이 열어서 확인하거나 실물 실크스크린으로 확정한다.
- Qorvo 공식 포럼과 다른 사이트에서도 이 표를 찾지 못했다. 서드파티 예제(다른 보드, 다른 핀)의 값은 이 실드의 공식 값이 아니라서 쓰지 않는다.

## 4. 핀맵 기록 표 (채워 넣는다)

| 신호 | DWM3000EVB 실드 핀 | STM32N6 후보 | CubeMX 기능 | 검증 |
| --- | --- | --- | --- | --- |
| 3V3 | TODO | 3V3 (전압 확인) | Power | ☐ |
| GND | TODO | GND | GND | ☐ |
| SCK | TODO (Arduino SPI라면 D13) | PE15 | SPI5_SCK | ☐ |
| MOSI | TODO (Arduino SPI라면 D11) | PG2 | SPI5_MOSI | ☐ |
| MISO | TODO (Arduino SPI라면 D12) | PG1 | SPI5_MISO | ☐ |
| CSn | TODO (Arduino SPI라면 D10) | PA3 | SPI5_NSS 또는 GPIO Output | ☐ |
| IRQ | TODO | TODO (D2 ~ D9 중) | GPIO / EXTI | ☐ |
| RSTn | TODO | TODO (D2 ~ D9 중) | GPIO (전기적 요구사항 확인) | ☐ |

`TODO` 칸은 Quick Start Guide나 실물로 확인되면 채운다. "Arduino SPI라면"은 일반적인 Arduino Uno 규약이고 이 실드에서 확인한 값이 아니다.

## 5. 배선 전 점검 체크리스트

- [ ] DWM3000EVB 리비전과 헤더 방향 확인 (실물 사진)
- [ ] Quick Start Guide와 회로도로 4절의 `TODO` 채우기
- [ ] 전원 점퍼(J1) 기능과 현재 위치 확인
- [ ] N6 Arduino 헤더 핀의 I/O 전압과 3V3, IOREF 핀의 전압 확인 (UM3417 또는 회로도)
- [ ] CubeMX에서 PE15, PG2, PG1, PA3의 SPI5 기능 선택 가능 여부 확인
- [ ] 두 보드의 전원 경로가 겹치지 않는지 (실드 3V3 DC-DC와 Nucleo의 3V3) 확인
- [ ] 위가 모두 끝난 뒤에 전원을 끈 상태로 배선, 사진 촬영
