# Task 2 Lv2: STM32N6 ↔ DWM3000EVB 하드웨어·SPI 통신 (Device ID 읽기)

이 폴더는 Task 2 Lv2 중 **STM32 ↔ DWM3000EVB 하드웨어 연결과 SPI 통신** 담당 범위다. Qorvo API 통합, TWR 거리 측정, 보정은 별도 담당자가 한다.
목표: MCU에서 SPI로 DW3000의 Device ID를 반복해서 정상적으로 읽고, 다른 담당자가 이 통신 계층을 이어받게 한다.

## 진행 현황 (2026-11-09)

| 단계 | 내용 | 상태 |
| --- | --- | --- |
| 1 | 전원·로직 레벨·핀맵 확인 | 문서로 대조 완료 ([`pinmap.md`](pinmap.md)). 실물 연결 후 검증 칸을 채운다 |
| 2 | SPI Master 프로젝트 (폴링) | 코드 작성, **빌드(링크 포함) 확인.** CubeIDE 빌드와 보드 실행은 아직 |
| 3 | SPI SCK/MOSI/MISO/CS, RSTn/IRQ 설정 | 코드에 반영 (`uwb_port.c`). 실물 확인 전 |
| 4 | `uwb_port.c/.h` | 작성 완료, 보드 시험 전 |
| 5 | Device ID 읽기 | **보드에서 아직 실행하지 않았다** |
| 6 | 인수인계 | 이 README. 5단계 후 결과 추가 |

## 보드와 연결

- 보드: NUCLEO-N657X0-Q + DWM3000EVB (`Module shield v1.3`, 모듈 `DWM3000 v1.4`). 사진: [`docs/`](docs/).
- EVB는 Arduino 실드라서 **Nucleo의 Arduino 헤더에 직접 꽂는다.** 점퍼선 불필요 (헤더 정렬은 사용자가 눈으로 확인함).
- 핀 (근거와 주의는 [`pinmap.md`](pinmap.md)): SPI5 SCK = PE15 (D13), MISO = PG1 (D12), MOSI = PG2 (D11), CS = PA3 (D10, GPIO), RSTn = PE11 (D7, open-drain), IRQ = PD12 (D8).
- **구동하면 안 되는 핀:** PE0 (D4), PE9 (D3) (모듈 출력, LED), PD8 (D1), PD9 (D0) (SPI 모드 설정 핀). 코드는 이 핀을 설정하지 않는다.
- **장착 (2026-11-09, 보드 2):** 전원을 끈 상태에서 헤더 위치를 확인하고 꽂았다. USB 연결 후 EVB의 `LED1`(DC-DC 출력)이 켜졌고 Nucleo의 전원·통신 LED도 정상이며 뜨거운 부분은 없었다 (사용자 확인). 사진: [`docs/evb_on_nucleo_board2.jpg`](docs/evb_on_nucleo_board2.jpg). 사진만으로 핀 정렬을 완전히 확인할 수는 없고, `LED1` 점등은 `5V` 핀이 연결됐다는 증거다.
- 전원: EVB의 J1 점퍼가 `3V3_DCDC`(핀 1-2)로 보인다 (사진 [`docs/evb_j1_closeup.jpg`](docs/evb_j1_closeup.jpg)). 이 상태에서는 EVB가 Nucleo의 `5V` 핀으로 동작한다. 점퍼는 바꾸지 않는다.

## 프로젝트 (`T2_UWB_SPI/`)

ST 예제 `UART/UART_Printf`를 복제해서 이름만 바꾼 CubeIDE 프로젝트(`T2_UWB_SPI_FSBL`)다. 원본 예제는 건드리지 않았다. 빌드 결과물은 넣지 않았다.

| 파일 | 내용 |
| --- | --- |
| `FSBL/Src/main.c` | Device ID 시험: 하드웨어 리셋 후 DEV_ID를 읽는 것을 20번, 리셋 없이 10번 더 읽어 값이 같은지 확인, UART로 출력 |
| `FSBL/Src/uwb_port.c`, `FSBL/Inc/uwb_port.h` | 통신 계층: SPI5 초기화(2 MHz / 8 MHz 전환), CS, RSTn, IRQ, 지연, Qorvo식 SPI 읽기·쓰기 |
| `STM32CubeIDE/FSBL/.project`, `.cproject` | 프로젝트 설정. SPI HAL 소스와 `uwb_port.c` 연결, SPI 모듈 켬 (`stm32n6xx_hal_conf.h`) |

### 통신 계층 인터페이스 (TWR 담당자용)

```c
int      uwb_port_init(void);        // GPIO + SPI5 (느린 속도로 시작)
void     uwb_port_spi_slow(void);    // 약 2 MHz  (리셋·초기화 중)
void     uwb_port_spi_fast(void);    // 약 8 MHz
void     uwb_port_reset(void);       // RSTn 리셋 후 칩이 켜질 때까지 대기
int      uwb_port_irq_level(void);
void     uwb_port_sleep_ms(uint32_t ms);

// Qorvo DW3000 API의 readfromspi() / writetospi()와 같은 인자. 성공 0, 실패 -1
int uwb_spi_read (uint16_t header_len, const uint8_t *header, uint16_t read_len, uint8_t *read_buf);
int uwb_spi_write(uint16_t header_len, const uint8_t *header, uint16_t body_len, const uint8_t *body);
```

- SPI 모드 0, 8비트, MSB first. 한 번의 호출이 chip select 한 프레임이다 (헤더 + 데이터).
- 한 프레임 최대 1100바이트 (DW3000 수신 프레임 최대 1023바이트 + 헤더).
- SPI5 커널 클럭은 HSI 64 MHz이고 프리스케일러 32 / 8로 2 MHz / 8 MHz를 만든다. Qorvo 소스 주석 기준으로 `dwt_softreset` 전에는 7 MHz 이하, 초기화(`dwt_initialise`)는 3 MHz 미만이어야 하므로 **초기화가 끝나기 전에는 `uwb_port_spi_slow()`** 를 쓴다 (사본 소스 기준, 사용할 Qorvo SDK 버전에서 확인).
- Qorvo 드라이버가 요구하는 다른 함수(`deca_sleep`, 인터럽트 콜백 등)는 아직 연결하지 않았다. TWR 담당자와 SDK 버전을 맞춘 뒤 어댑터를 쓴다.

## 실행 방법

1. **전원을 끈 상태**(USB 뽑음)에서 EVB를 Nucleo의 Arduino 헤더에 꽂는다 (J1 점퍼는 그대로). 꽂은 사진을 `docs/`에 남긴다.
2. 이 폴더의 `T2_UWB_SPI/`를 `STM32Cube\Repository\STM32Cube_FW_N6_V1.4.1\Projects\NUCLEO-N657X0-Q\Examples\UART\T2_UWB_SPI`로 복사한다 (드라이버를 상대 경로로 참조해서 이 위치여야 한다). CubeIDE에서 File > Import > Existing Projects into Workspace, 루트는 `...\T2_UWB_SPI\STM32CubeIDE\FSBL`, "Copy projects into workspace"는 해제.
3. 프로젝트를 선택하고 Clean, Build. `0 errors`여야 한다.
4. USB를 연결한다. **실드의 `LED1`이 켜지는지 본다** (DC-DC 출력 확인, 회로도 기준). 안 켜지거나 보드가 뜨거워지면 바로 뽑는다.
5. **우클릭 > Debug As > STM32 Cortex-M C/C++ Application.** `main`에서 멈추면 Tera Term(115200, 7 bit, odd, 1, none)에서 **로그를 켠 뒤** Resume. RESET 버튼은 누르지 않는다 (개발 모드).

### 기대 출력

```
== T2 Lv2: DWM3000EVB Device ID test ==
SPI clock = 2000000 Hz, IRQ level = 0
#01 id=0xDECA0302 raw=02 03 CA DE (RIDTAG DECA MODEL 03 VER 0 REV 2) IRQ=0  OK (C0 silicon)
...
verdict: PASS
```

판정: 20번 모두 `0xDECA03xx`(DW3000 계열)이고 리셋 없이 다시 읽은 10번이 첫 값과 같으면 PASS. `0x00000000`은 응답 없음(전원, CS, 배선), `0xFFFFFFFF`는 MISO가 떠 있음(장치 없음 또는 배선), 다른 값은 SPI 모드나 속도부터 점검한다.

## 알려진 제약과 확인 필요

- **보드에서 아직 실행하지 않았다.** 빌드(링크)까지만 확인했다.
- DEV_ID 읽기의 SPI 헤더를 `0x00` 한 바이트로 보낸다 (읽기, 레지스터 파일 0, 오프셋 0). Qorvo `dwt_xfer3000()` 코드에서 도출한 값이고 첫 실제 읽기로 확인된다.
- 기대 Device ID는 `0xDECA0302`(C0) 또는 `0xDECA0312`(C0 PDOA)를 "인식"으로 세고, `0xDECA03xx`는 "DW3000 계열"로 센다. 정확한 값은 모듈 리비전에 따라 다를 수 있어 읽은 값을 로그에 그대로 남긴다.
- N6 Nucleo에서 D11(MOSI)을 다른 주변장치가 누르는지는 매뉴얼에서 확인하지 못했다. MOSI가 이상하면 이 점을 먼저 본다.
- 회로도(V1.2)와 실물(`Module shield v1.3`)의 리비전 차이는 확인하지 못했다.
- `.project`/`.cproject`를 바꾸면 Close Project → Open Project로 다시 읽힌다 (`../../삽질기록.md` 참고).
