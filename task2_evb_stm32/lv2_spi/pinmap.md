# Task 2 Lv2: STM32N6 ↔ DWM3000EVB 핀맵 (초안, 배선 전)

**상태: 검증 전.** 이 표의 어떤 값도 실물 보드와 회로도로 확인되지 않았다. 확인 전에는 배선하거나 전원을 넣지 않는다.

## 1. 확인 상태 요약

| 항목 | 상태 | 근거 |
| --- | --- | --- |
| N6 Nucleo의 Arduino 헤더 ↔ STM32N657 핀 | 제3자 자료로 찾음, **ST 자료로 대조 필요** | 아래 2절 (Zephyr 저장소) |
| Arduino SPI(D10~D13)가 SPI5 | 같은 자료에서 일치 | 아래 2절 |
| DWM3000EVB의 Arduino 핀 ↔ 신호(IRQ, RSTn, WAKEUP 등) | **실물 사진의 실크스크린으로 판독함 (3절).** Arduino Uno 헤더 순서로 D번호를 대응시킨 것이라 실물 확인 필요 | 사진 `docs/evb_front_v1.3.jpg` |
| DWM3000EVB 전원 점퍼(J1) | 라벨 판독 (`3V3_Arduino` / `3V3_DCDC`). **현재 점퍼 위치와 기능은 미확인** | 같은 사진, Qorvo 문서는 못 열었음 |
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
- 공식 문서는 **Quick Start Guide (Rev A, 05/2021)** 와 **회로도 (DWS1000 & DWS3000 Schematics, 2020-03-12)** 이고 링크는 `https://www.qorvo.com/products/r/ra006825`, `https://www.qorvo.com/products/r/ra006729`다. 자동 접근이 막혀서(HTTP 429, 내장 브라우저 접근 거부) 내용을 읽지 못했다. 서드파티 예제(다른 보드, 다른 핀)의 값은 이 실드의 공식 값이 아니라서 쓰지 않는다.

### 3-1. 실물 사진 판독 (2026-11-09)

사진: [`docs/evb_front_v1.3.jpg`](docs/evb_front_v1.3.jpg). 실크스크린: `Qorvo`, `PCL298336`, `Module shield v1.3`, 모듈 `DWM3000 v1.4`, 날짜 코드로 보이는 `2426`.

모듈이 위쪽에 오도록 놓았을 때 오른쪽 헤더 두 개(`CON1`, `CON4`)에 신호 이름이 인쇄돼 있다 (위에서 아래 순서).

| 헤더 | 인쇄된 이름 (순서대로) |
| --- | --- |
| CON1 (10핀으로 보임) | (맨 위 핀 라벨 없음), `TCXO_EN`, (라벨 없음), `GND`, `SPICLK`, `SPIMISO`, `SPIMOSI`, `SPICSn`, `WAKEUP`, `IRQ` |
| CON4 (8핀) | `RSTn`, `GPIO0`, `GPIO1`, `GPIO2/RXLED`, `GPIO3/TXLED`, `GPIO4`, `GPIO5/SPIPOL`, `GPIO6/SPIPHA` |
| CON3 (왼쪽 아래) | `EXTON`, `GPIO7` (그 아래는 라벨 없음) |
| J1 (왼쪽 가운데) | 점퍼 옆에 `3V3_Arduino`, `3V3_DCDC` |

**Arduino D번호 대응 (추정):** 실드는 Arduino Uno 헤더 형태라서, CON1은 Uno의 위쪽 10핀 헤더(`SCL, SDA, AREF, GND, D13, D12, D11, D10, D9, D8`), CON4는 아래쪽 8핀 헤더(`D7 … D0`)의 순서와 같다고 보면 다음과 같다. 사진에서 CON1의 `GND` 이후 네 개가 SPI이고, 라벨 개수와 위치가 이 순서에 맞는다. **헤더 핀 순서는 눈으로 센 것이라 실물에서 Nucleo의 D번호 실크스크린과 맞춰 확인해야 한다.**

| 신호 | Arduino 핀 (추정) |
| --- | --- |
| SPICLK | D13 |
| SPIMISO | D12 |
| SPIMOSI | D11 |
| SPICSn | D10 |
| WAKEUP | D9 |
| IRQ | D8 |
| RSTn | D7 |
| GPIO0, GPIO1 | D6, D5 |
| GPIO2/RXLED, GPIO3/TXLED, GPIO4 | D4, D3, D2 |
| GPIO5/SPIPOL, GPIO6/SPIPHA | D1, D0 |

**주의할 점**

- `GPIO5/SPIPOL`, `GPIO6/SPIPHA`는 DW3000의 SPI 모드(극성, 위상)를 정하는 핀과 같은 이름이다. 전원을 넣거나 리셋할 때 이 핀의 상태로 모드가 정해진다고 알려져 있으므로(데이터시트로 확인 필요), **D1, D0에 연결되는 N6 핀(USART3 TX/RX 후보, PD8/PD9)을 코드에서 구동하지 않는다.**
- `J1`은 전원 선택 점퍼로 보인다 (`3V3_Arduino` = 호스트의 3V3, `3V3_DCDC` = 실드의 DC-DC). 사진에서 점퍼 캡이 어느 위치를 덮고 있는지 확실히 구분되지 않는다. 가까이서 찍은 사진이 필요하다.
- `RSTn`은 호스트가 High로 밀어 올리지 않는 방식(open-drain)으로 쓰는 것이 일반적이라고 알고 있지만 데이터시트로 확인해야 한다. 아직 확인하지 못했다.

## 4. 핀맵 기록 표 (채워 넣는다)

| 신호 | DWM3000EVB 실드 핀 | STM32N6 후보 | CubeMX 기능 | 검증 |
| --- | --- | --- | --- | --- |
| 3V3 | 전원 헤더(CON2로 추정) 3V3 핀, J1 선택에 따라 | 3V3 (전압 확인) | Power | ☐ |
| GND | CON1 `GND` | GND | GND | ☐ |
| SCK | `SPICLK` (D13) | PE15 | SPI5_SCK | ☐ |
| MOSI | `SPIMOSI` (D11) | PG2 | SPI5_MOSI | ☐ |
| MISO | `SPIMISO` (D12) | PG1 | SPI5_MISO | ☐ |
| CSn | `SPICSn` (D10) | PA3 | SPI5_NSS 또는 GPIO Output | ☐ |
| IRQ | `IRQ` (D8) | PD12 | GPIO / EXTI12 | ☐ |
| RSTn | `RSTn` (D7) | PE11 | GPIO (전기적 요구사항 확인) | ☐ |
| WAKEUP (선택) | `WAKEUP` (D9) | PD7 | GPIO | ☐ |

EVB 쪽 열은 사진의 실크스크린과 Uno 헤더 순서로 정한 추정이고, N6 열은 Zephyr 정의다. **모든 줄이 아직 검증되지 않았다.** EVB는 Arduino 실드 형태라서 Nucleo의 Arduino 헤더에 직접 꽂을 수 있을 가능성이 높지만 (그러면 별도 배선이 필요 없다), 이 보드의 헤더 위치가 Uno와 맞는지는 실물로 확인해야 한다.

## 5. 배선 전 점검 체크리스트

- [ ] DWM3000EVB 리비전과 헤더 방향 확인 (실물 사진)
- [x] 실물 사진 확보 (`docs/evb_front_v1.3.jpg`, 리비전 `Module shield v1.3`, 모듈 `DWM3000 v1.4`)
- [ ] 4절의 EVB 쪽 열을 실물에서 확인 (헤더 핀 순서와 Nucleo의 D번호 실크스크린 대조), 가능하면 Quick Start Guide와도 대조
- [ ] 전원 점퍼(J1) 현재 위치와 기능 확인 (가까이서 찍은 사진, Quick Start Guide)
- [ ] D0, D1(`GPIO5/SPIPOL`, `GPIO6/SPIPHA`)에 연결되는 N6 핀을 구동하지 않는지 확인
- [ ] N6 Arduino 헤더 핀의 I/O 전압과 3V3, IOREF 핀의 전압 확인 (UM3417 또는 회로도)
- [ ] CubeMX에서 PE15, PG2, PG1, PA3의 SPI5 기능 선택 가능 여부 확인
- [ ] 두 보드의 전원 경로가 겹치지 않는지 (실드 3V3 DC-DC와 Nucleo의 3V3) 확인
- [ ] 위가 모두 끝난 뒤에 전원을 끈 상태로 배선, 사진 촬영
