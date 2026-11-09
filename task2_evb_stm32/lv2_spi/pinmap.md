# Task 2 Lv2: STM32N6 ↔ DWM3000EVB 핀맵 (문서 대조 완료, 배선 전)

**상태: 문서로 대조했고 실물 연결은 아직 안 했다.** 근거가 일치하는 부분을 표시했다. 전원을 넣기 전에 6절 체크리스트의 실물 확인을 마친다.

근거 등급: **[공식]** 제조사 문서 (Qorvo 회로도, ST 매뉴얼), **[코드]** 제3자 오픈소스 코드, **[사진]** 실물 사진.

## 1. 한눈에 보기

| 항목 | 결론 | 근거 |
| --- | --- | --- |
| EVB 신호 ↔ Arduino 핀 | **확정에 가까움.** 회로도, 오픈소스 코드, 실물 사진이 모두 일치 | 3절 |
| N6 Nucleo Arduino 핀 ↔ STM32N657 핀 | **확정에 가까움.** ST 매뉴얼 표와 Zephyr 정의가 일치 | 2절 |
| 디지털 I/O 전압 | N6 헤더의 디지털 I/O는 3.3 V (ST 매뉴얼 각주), EVB 모듈 전원도 3.3 V. 맞음 | 2절, 3절 |
| 전원 점퍼 J1 | 기능은 회로도로 확인. **현재 위치는 1-2 (`3V3_DCDC`, 실드의 DC-DC 사용)로 보임.** 그래서 Nucleo의 `5V` 핀이 필요 | 3절, 근접 사진 |
| 기대 Device ID | `0xDECA03xx` 계열 6개 중 하나. 리비전을 모르므로 목록과 대조 | 5절 |
| 이 EVB 실물(`Module shield v1.3`)과 회로도(V1.2, 2020-03) 리비전 차이 | **확인 못 함.** 실크스크린의 신호 이름은 회로도와 같음 | 3절 |

## 2. N6 쪽: NUCLEO-N657X0-Q Arduino 헤더

- **[공식]** ST UM3417 (Rev 1 사본, ManualsLib 29쪽) Table 11 "Arduino Uno V3 Connectors Pinout"과 **[코드]** Zephyr 보드 정의([`arduino_r3_connector.dtsi`](https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/boards/st/nucleo_n657x0_q/arduino_r3_connector.dtsi))가 **서로 일치**한다. ST 공식 PDF 원본([UM3417](https://www.st.com/resource/en/user_manual/um3417-stm32n6-nucleo144-board-mb1940-stmicroelectronics.pdf))은 열지 못했고, 사본 텍스트의 표 정렬은 추정이 섞여 있다.

| Arduino 핀 | STM32N657 핀 | 매뉴얼에 적힌 기능 |
| --- | --- | --- |
| D0 | PD9 | USART3_RX |
| D1 | PD8 | USART3_TX |
| D2 | PD0 | |
| D3 | PE9 | TIM1_CH1 |
| D4 | PE0 | |
| D5 | PE10 | TIM1_CH2N |
| D6 | PD5 | TIM1_CH4N |
| D7 | PE11 | |
| D8 | PD12 | |
| D9 | PD7 | TIM1_CH2 |
| **D10** | **PA3** | **SPI5_CS** (TIM16_CH1) |
| **D11** | **PG2** | **SPI5_MOSI** (TIM14_CH1) |
| **D12** | **PG1** | **SPI5_MISO** |
| **D13** | **PE15** | **SPI5_SCK** |
| D14, D15 | PC1, PH9 | I2C1 SDA, SCL |

- **전압:** 같은 표의 각주에 "ADC pins run at 1.8 V MCU level, while digital I/O runs at 3.3 V"로 적혀 있다. 즉 D0 ~ D13의 디지털 핀은 3.3 V다. `IOREF`는 3V3 기준, `3V3`와 `5V` 핀은 출력이다.
- Lv1 확인과 일치: 콘솔 USART1 = PE5, PE6 (헤더에 안 나옴), 파란 LED = PG8, 버튼 PC13 active high (Zephyr).
- **확인하지 못한 것:** 같은 매뉴얼의 납땜 브리지 표(37쪽, Table 18)에는 D10 ~ D13 관련 항목이 없었다. 다만 Arduino 커넥터 절(27 ~ 28쪽)은 읽지 않았다. 다른 Nucleo-144(F429ZI)에서는 이더넷 때문에 D11(MOSI)이 눌리는 문제가 있어 납땜 브리지를 바꿔야 했다 ([foldedtoad/dwm3000 README](https://github.com/foldedtoad/dwm3000)). N6 보드에도 비슷한 문제가 있는지는 **실제로 MOSI 파형이나 통신으로 확인**한다.

## 3. DWM3000EVB 쪽 (Qorvo 회로도)

- **[공식]** Qorvo DWS3000 / DWS1000 회로도 (V1.2, 2020-03-12). Qorvo 제품 페이지 [DWM3000EVB](https://www.qorvo.com/products/ek/DWM3000EVB)의 "Design Resources"에 있는 문서이고, 이번에는 [foldedtoad/dwm3000 저장소의 사본](https://github.com/foldedtoad/dwm3000/blob/master/docs/DWS3000_Schematics_V1_2.pdf)(Qorvo 문서 `DWS3000 Schematics`)을 이미지로 읽었다. DWS3000은 DWM3000EVB의 옛 이름이다.
- **[사진]** 실물 `Module shield v1.3`, 모듈 `DWM3000 v1.4`, 헤더의 실크스크린: [`docs/evb_front_v1.3.jpg`](docs/evb_front_v1.3.jpg). 회로도의 신호 이름과 같다.

회로도에서 읽은 헤더 (왼쪽 열 = Arduino 이름, 오른쪽 열 = 실드 신호):

| 헤더 | 핀 (Arduino 이름) | 실드 신호 |
| --- | --- | --- |
| CON1 (디지털 위, 10핀) | SCL | (없음) |
| | SDA | `TCXO_EN` |
| | AREF | (없음) |
| | GND | GND |
| | **SCK (D13)** | **SPICLK** |
| | **MISO (D12)** | **SPIMISO** |
| | **MOSI (D11)** | **SPIMOSI** |
| | **SS (D10)** | **SPICSn** |
| | OC1A (D9) | `WAKEUP` |
| | CLKO (D8) | `IRQ` |
| CON4 (디지털 아래, 8핀) | AIN1 (D7) | **`RSTn`** (`DWM1000_RSTn`) |
| | AIN0 (D6) | GPIO0 |
| | T1 (D5) | GPIO1 |
| | T0 (D4) | GPIO2 (RX LED) |
| | INT1 (D3) | GPIO3 (TX LED) |
| | INT0 (D2) | GPIO4 |
| | TXD (D1) | GPIO5 / SPIPOL |
| | RXD (D0) | GPIO6 / SPIPHA |
| CON3 (아날로그) | A0, A1 | `EXTON`, `GPIO7` |
| CON2 (전원) | NC, IOREF, RESET, 3V3, 5V, GND, GND, VIN | `3V3`는 `3V3_ARDUINO`, `5V`는 DC-DC 입력 |

- Arduino 이름(`SCK`, `SS`, `OC1A`, `CLKO` 등)이 Uno의 D13, D10, D9, D8에 해당하는 이름이라서 D번호 대응이 이름으로도 확정된다.
- **SPI 4선에는 49.9 Ω 직렬 저항**(R4 ~ R7)이 있고 레벨 변환기는 없다. 모듈이 3.3 V로 동작하므로 N6의 3.3 V 디지털 I/O와 맞다.
- **J1 (전원 선택)**: 3핀. 핀 3 = `3V3_ARDUINO`, 핀 2 = 모듈 3V3(공통), 핀 1 = `3V3_DCDC`. 점퍼를 2-3에 두면 호스트의 3V3, 1-2에 두면 실드의 DC-DC(U2, 5 V 입력 → 3.3 V)를 쓴다 (회로도의 핀 번호와 net 이름으로 읽은 것). Quick Start Guide의 서드파티 사본(manuals.plus)에도 Arduino 3V3 선택이 "Up, pin 2와 3 단락"으로 적혀 있어 일치하지만, 그 사본의 다른 구절은 어느 위치가 어느 전원인지 명확하지 않아 공식 PDF로 대조하지는 못했다. 실물에서 `3V3_Arduino`가 위, `3V3_DCDC`가 아래에 인쇄돼 있다.
  - **현재 점퍼 위치 (근접 사진 [`docs/evb_j1_closeup.jpg`](docs/evb_j1_closeup.jpg), 2026-11-09):** 검은 점퍼 캡이 **아래쪽 두 핀(핀 1-2)** 을 덮고 있고, 맨 위 핀(핀 3, `3V3_Arduino` 쪽)은 비어서 노출돼 있다. 즉 **`3V3_DCDC` (실드의 DC-DC 사용) 상태**로 보이며, 이는 출하 기본값으로 알려진 설정이다 (출하 기본값이라는 점은 문서로 확인하지 못함). 사진 판독이다.
  - **DC-DC 상태이면 실드가 Nucleo의 `5V` 핀(CON2 5번)에서 전류를 받는다.** UM3417 Table 11에는 `5V`가 "5 V output"으로 적혀 있다. Nucleo가 USB(ST-LINK)로 전원을 받는 기본 구성에서 이 핀에 5 V가 나오는지는 실물에서 확인한다.
  - 이 상태에서는 Nucleo의 `3V3` 핀이 모듈에 연결되지 않으므로 두 3.3 V 전원이 부딪치지 않는다 (점퍼가 한 번에 하나만 모듈에 연결).
  - 전원이 들어오면 실드의 `LED1`(DC-DC 출력에 연결, 회로도에서 R1 150 Ω 직렬)이 켜지는 것이 DC-DC 출력 확인에 쓸 수 있는 단서다 (회로도 기준 추정).
- **J2, J3**: 회로도의 표에서 DWS3000은 둘 다 GND에 연결하면 내부 크리스털을 쓰는 설정이다. Mouser 제품 설명은 "향후용, 출하 위치(가운데 핀을 GND에 연결)로 둘 것"이라고 한다. 둘 다 **건드리지 않고 출하 상태로 둔다.**

### 3-1. 사용하지 말아야 하는 핀 (주의)

| 핀 | 이유 |
| --- | --- |
| **D4 (GPIO2), D3 (GPIO3)** | 모듈의 출력이다. 실드의 LED2, LED3가 여기에 연결돼 있다 (R2 2.7 kΩ, R3 150 Ω). N6 쪽 핀(PE0, PE9)은 **입력(또는 Hi-Z)으로 두고 구동하지 않는다.** |
| **D1 (GPIO5/SPIPOL), D0 (GPIO6/SPIPHA)** | DW3000의 SPI 극성·위상 설정 핀과 같은 이름이다. 실드에는 풀업·풀다운 저항이 없고 핀에 바로 연결돼 있다. 오픈소스 포팅은 이 핀을 호스트가 구동해서 SPI 모드를 정한다 ([foldedtoad README](https://github.com/foldedtoad/dwm3000): "These lines are used to configure DWM3000's SPI Polarity and Phase options"). **어느 레벨이 모드 0인지는 데이터시트로 확인하기 전에는 모르므로, 처음에는 구동하지 않고(Hi-Z) 둔다.** |
| D9 (WAKEUP) | 처음에는 연결만 하고 사용하지 않는다 (SPI CS로 깨울 수 있다는 서드파티 드라이버 설명이 있으나 이 모듈에서 확인하지 않음) |

## 4. 최종 핀맵 (배선 표)

EVB는 Arduino 실드라서 **Nucleo의 Arduino 헤더에 직접 꽂는 구성**이다. SPI 4선이 양쪽에서 같은 D10 ~ D13에 놓이므로 별도 점퍼선이 필요 없을 가능성이 높다. 실물의 헤더 위치가 서로 맞는지는 꽂기 전에 확인한다.

| 신호 | Arduino 핀 | DWM3000EVB (회로도) | STM32N6 핀 | CubeMX 기능 | 검증 |
| --- | --- | --- | --- | --- | --- |
| 3V3 | CON2 3V3 | `3V3_ARDUINO` (J1 2-3일 때) | 3V3 | Power | ☐ |
| 5V | CON2 5V | DC-DC 입력 (J1 1-2일 때) | 5V | Power | ☐ |
| GND | GND | GND | GND | GND | ☐ |
| SCK | D13 | SPICLK | PE15 | SPI5_SCK | ☐ |
| MISO | D12 | SPIMISO | PG1 | SPI5_MISO | ☐ |
| MOSI | D11 | SPIMOSI | PG2 | SPI5_MOSI | ☐ |
| CSn | D10 | SPICSn | PA3 | GPIO Output (소프트웨어 CS) 또는 SPI5_NSS | ☐ |
| IRQ | D8 | IRQ | PD12 | GPIO Input / EXTI12 | ☐ |
| RSTn | D7 | RSTn | PE11 | GPIO (open-drain 출력 또는 Hi-Z) | ☐ |
| WAKEUP (선택) | D9 | WAKEUP | PD7 | GPIO (처음엔 미사용) | ☐ |
| (구동 금지) | D4, D3, D1, D0 | GPIO2, GPIO3, GPIO5, GPIO6 | PE0, PE9, PD8, PD9 | 사용 안 함 (Hi-Z) | ☐ |

검증 칸은 실물에서 확인되면 체크한다. 표의 값 자체는 위 세 가지 근거(회로도, ST 매뉴얼, 오픈소스 코드)가 일치한다.

## 5. SPI와 Device ID 정보 (오픈소스 코드 [코드], 데이터시트로 대조 필요)

모두 [foldedtoad/dwm3000](https://github.com/foldedtoad/dwm3000)(Qorvo DW3000 API를 DWS3000 실드에 포팅한 Zephyr 프로젝트)에서 읽은 것이다. 그 저장소의 Qorvo 소스 파일 사본이므로 우리가 쓸 Qorvo SDK 버전과 대조해야 한다.

| 항목 | 값 |
| --- | --- |
| SPI 모드 | Mode 0 (CPOL 0, CPHA 0), 8비트 워드 |
| SPI 속도 | 초기 2 MHz(느린 설정), 이후 8 MHz(빠른 설정). 오버레이는 최대 1 MHz로 시작. Qorvo 주석: `dwt_softreset` 전에는 7 MHz 이하, `dwt_initialise`는 3 MHz 미만이어야 한다 (API 버전에 따라 다름) |
| 리셋 | 소프트 리셋(`dwt_softreset`, SPI 느린 속도)이 기본. RSTn 핀 방식은 open-drain 출력으로 Low 10 µs 후 놓기. 리셋 후 2 ms 대기 |
| IRQ | 입력, 상승 에지 인터럽트 |
| Device ID 확인 순서 (Qorvo 예제 `ex_00a_reading_dev_id`) | SPI 속도 설정 → 리셋 → 2 ms 대기 → `dwt_check_dev_id()`. 성공이면 `DEV ID OK` |
| 기대 Device ID | Qorvo API 헤더의 6개 상수: `0xDECA0300`(A0), `0xDECA0310`(A0 PDOA), `0xDECA0301`(B0), `0xDECA0311`(B0 PDOA), `0xDECA0302`(C0), `0xDECA0312`(C0 PDOA). **이 모듈(DW3110)의 정확한 값은 모른다.** 값을 하나로 못 박지 말고 읽은 값을 이 목록과 대조하고 로그에 그대로 남긴다 |

## 6. 체크리스트 (배선 전 점검 + 실행 결과)

- [x] 실물 사진 확보 (`docs/evb_front_v1.3.jpg`, 리비전 `Module shield v1.3`, 모듈 `DWM3000 v1.4`)
- [x] 회로도로 EVB 쪽 신호와 J1 기능 확인 (Qorvo DWS3000 회로도 V1.2)
- [x] ST 매뉴얼 표로 N6 Arduino 핀 확인, 디지털 I/O 3.3 V 확인 (UM3417 사본)
- [x] J1 점퍼 위치 확인: 1-2 (`3V3_DCDC`)로 보임 (`docs/evb_j1_closeup.jpg`). 사진 판독이므로 전원 전에 한 번 더 눈으로 확인
- [x] DC-DC 상태에서 EVB의 `LED1`이 켜지고 정상 동작함 (보드 1). `5V` 핀 전압과 전류 여유를 측정하지는 않았다
- [x] EVB를 Nucleo의 Arduino 헤더에 올렸을 때 정렬 확인 (사용자 확인, 전원 끈 상태) 후 장착: `docs/evb_on_nucleo_board1.jpg`
- [x] D4, D3, D1, D0에 해당하는 N6 핀(PE0, PE9, PD8, PD9)은 `uwb_port.c`가 설정하지 않는다 (코드 확인)
- [x] SPI5 핀 기능: ST 예제 `SPI_FullDuplex`가 PE15, PG1, PG2를 AF5로 쓰고, 보드 1에서 SCK와 MISO가 동작함
- [ ] 회로도(V1.2)와 실물(`Module shield v1.3`)의 차이 여부를 가능하면 Qorvo Quick Start Guide 등으로 확인
- [x] 전원 인가, Device ID 읽기 `0xDECA0302` 20/20, 로그 저장 (`logs/device_id_board1.txt`)
- [x] **MOSI(PG2, D11) 검증됨**: 쓰기-읽기 시험 3회 실행 모두 20/20 PASS (`logs/device_id_mosi_board1_run1~3.txt`). 참고로 Device ID 읽기만으로는 검증되지 않아 별도 시험을 했다. 원래 문구:: Device ID 읽기는 보내는 바이트가 모두 0이라 MOSI가 눌려 있어도 성공한다. 0이 아닌 값을 보내는 시험이 필요하다
- [x] 전원 재투입(USB 재연결) 반복 2회: 같은 결과
- [ ] 보드 2에서의 시험
- [ ] IRQ 핀이 처음부터 High로 읽힌 이유 확인 (`SYS_STATUS` 플래그 가능성)
