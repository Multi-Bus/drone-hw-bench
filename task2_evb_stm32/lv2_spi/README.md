# Task 2 Lv2: STM32N6 ↔ DWM3000EVB 하드웨어·SPI 통신 (Device ID 읽기)

이 폴더는 Task 2 Lv2 중 **STM32 ↔ DWM3000EVB 하드웨어 연결과 SPI 통신** 담당 범위다. Qorvo API 통합, TWR 거리 측정, 보정은 별도 담당자가 한다.
목표: MCU에서 SPI로 DW3000의 Device ID를 반복해서 정상적으로 읽고, 다른 담당자가 이 통신 계층을 이어받게 한다.

## 진행 현황 (2026-11-09)

| 단계 | 내용 | 상태 |
| --- | --- | --- |
| 1 | 전원·로직 레벨·핀맵 확인 | **완료.** 문서로 대조했고 보드 2 실물에서 동작으로 확인됨 ([`pinmap.md`](pinmap.md)) |
| 2 | SPI Master 프로젝트 (폴링) | **완료.** CubeIDE 빌드 `0 errors`, 보드 2에서 실행 |
| 3 | SPI SCK/MOSI/MISO/CS, RSTn/IRQ 설정 | **완료.** SCK, MISO, MOSI, CS, RSTn이 보드 2에서 동작 확인됨 (MOSI는 쓰기-읽기 시험, 아래 "MOSI 시험"). IRQ 핀은 읽기만 했다 |
| 4 | `uwb_port.c/.h` | **완료.** 보드 2에서 시험됨 (SPI 읽기·쓰기 2 MHz, 리셋). 빠른 속도(8 MHz, `uwb_port_spi_fast()`)는 아직 시험 전 |
| 5 | Device ID 읽기 | **보드 2에서 3회 PASS** (USB 재연결 2회 포함). `0xDECA0302` 20/20, 리셋 없이 10/10. 보드 1은 아직 |
| 6 | 인수인계 | 이 README, `pinmap.md`, 로그 `logs/`. 남은 확인은 "알려진 제약" 참고 |

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
| `FSBL/Src/main.c` | 시험 순서: (1) Device ID: 하드웨어 리셋 후 DEV_ID를 20번 읽고 리셋 없이 10번 더 읽어 값이 같은지 확인, (2) MOSI 시험: `PANADR` 쓰기-읽기 20회. 결과를 UART로 출력 |
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

### 출력 형식 (실제 결과의 일부)

```
== T2 Lv2: DWM3000EVB Device ID test ==
SPI clock = 2000000 Hz, IRQ level = 1
#01 id=0xDECA0302 raw=02 03 CA DE (RIDTAG DECA MODEL 03 VER 0 REV 2) IRQ=1  OK (C0 silicon)
...
verdict: PASS

== MOSI test: PANADR write/read-back, 20 rounds (each after a reset) ==
#01 eam_id=0xDECA0302(ok) PANADR_orig=0xFFFFFFFF mismatch=0/8 stuck_low=0x00000000 stuck_high=0x00000000 restored=ok  OK
...
MOSI verdict: PASS
== done ==
```

판정: 20번 모두 `0xDECA03xx`(DW3000 계열)이고 리셋 없이 다시 읽은 10번이 첫 값과 같으면 PASS. `0x00000000`은 응답 없음(전원, CS, 배선), `0xFFFFFFFF`는 MISO가 떠 있음(장치 없음 또는 배선), 다른 값은 SPI 모드나 속도부터 점검한다.

## 결과 (Device ID)

보드 2 (가상 COM6)에서 2026-11-09 실행. 원본 로그는 Tera Term 로그 파일이다 (옮겨 적은 값이 아님). 먼저 Device ID만 읽는 시험을 했고 ([`logs/device_id_board2.txt`](logs/device_id_board2.txt)), 이후 MOSI 시험을 추가한 프로그램으로 세 번 실행했다. 2회차와 3회차는 **USB를 뽑았다가 다시 꽂은 뒤** 실행했다.

| 로그 | 실행 | Device ID (리셋 후 20회) | 리셋 없이 10회 | MOSI 시험 (20회) |
| --- | --- | --- | --- | --- |
| [`logs/device_id_mosi_board2_run1.txt`](logs/device_id_mosi_board2_run1.txt) | 최초 연결 | 20/20 `0xDECA0302` | 10/10 | 20/20 PASS |
| [`logs/device_id_mosi_board2_run2.txt`](logs/device_id_mosi_board2_run2.txt) | USB 재연결 후 | 20/20 `0xDECA0302` | 10/10 | 20/20 PASS |
| [`logs/device_id_mosi_board2_run3.txt`](logs/device_id_mosi_board2_run3.txt) | USB 재연결 후 | 20/20 `0xDECA0302` | 10/10 | 20/20 PASS |

- run1은 프로그램이 두 번 시작된 기록이다 (앞의 8줄은 `#08`에서 끊긴 첫 실행, 이어서 처음부터 다시 시작한 완전한 실행). 판정은 완전한 두 번째 실행 기준이고 원본은 그대로 두었다. 원인은 모른다.
- run2와 run3은 바이트까지 같다 (출력에 시각 같은 값이 없어서 같은 결과면 같은 파일이 된다).

아래 표는 Device ID 시험의 결과다 (세 실행 모두 같음).

| 항목 | 결과 |
| --- | --- |
| 리셋 후 DEV_ID 읽기 | **20 / 20** 모두 `0xDECA0302`, SPI 오류 0 |
| 리셋 없이 다시 읽기 | **10 / 10** 첫 값과 같음 |
| 값의 해석 | RIDTAG `DECA`, MODEL `03`, VER `0`, REV `2` (C0 실리콘). Qorvo API가 정상으로 판정하는 값 (`0xDECA0302` 또는 `0xDECA0312`) |
| 받은 바이트 | `02 03 CA DE` (리틀엔디언) |
| SPI | 모드 0, 2 MHz, CS = PA3 (소프트웨어), RSTn = PE11 (open-drain, Low 약 2~3 ms 후 약 5 ms 대기) |
| 판정 | **PASS** |

이 결과로 확인된 것:

- DEV_ID 읽기의 SPI 헤더 한 바이트 `0x00`이 맞다 (이전에는 Qorvo 코드에서 도출한 추정이었음).
- SCK (PE15), MISO (PG1), CS (PA3), RSTn (PE11)이 보드 2에서 동작한다. MOSI (PG2)는 아래 MOSI 시험으로 확인했다.
- 전원: J1이 `3V3_DCDC`인 상태에서 Nucleo의 `5V` 핀으로 EVB가 동작한다. 디지털 I/O 3.3 V도 맞다.
- USB를 뽑았다가 다시 꽂아도 같은 결과가 나온다 (3회 실행).

### MOSI 시험

DEV_ID를 읽을 때 MOSI로 보내는 바이트는 헤더 `0x00`과 더미 `0x00` 네 개로 **모두 0**이다. 그래서 MOSI가 Low로 눌려 있어도 Device ID 시험은 통과한다. 다른 Nucleo-144(F429ZI)에서는 이더넷 때문에 D11이 눌려서 납땜 브리지를 바꿔야 했다는 보고가 있어 ([foldedtoad/dwm3000 README](https://github.com/foldedtoad/dwm3000)), MOSI에 1 비트가 실제로 나가는지 따로 시험했다 (`uwb_port_mosi_selftest()`).

1. DEV_ID를 2바이트 확장 헤더 `40 00`(1 비트 포함)으로 읽는다. MOSI가 Low로 눌려 있으면 칩이 `00 00`을 받아 응답이 한 바이트 밀린다.
2. `PANADR`(레지스터 파일 0, 오프셋 `0x0C`, 32비트. PAN ID와 짧은 주소이며 주소 필터가 꺼져 있으면 칩 동작에 영향이 없다)에 8가지 패턴(`FFFFFFFF`, `00000000`, `A5A5A5A5`, `5A5A5A5A`, `80402010`, `08040201`, `12345678`, `CAFEBABE`)을 쓰고 되읽은 뒤, 원래 값으로 복원한다. 매번 리셋한 뒤 20회.

결과: **3번의 실행 모두 20/20 PASS.** 쓴 값이 모든 패턴에서 그대로 되읽혔고, 고정된 비트가 없고, 원래 값 복원도 성공했다. 확장 헤더의 DEV_ID 읽기도 매번 `0xDECA0302`였다. 즉 **MOSI로 0과 1이 모두 전달되고**, 쓰기 헤더 `C0 30`(쓰기, 확장, 파일 0, 오프셋 `0x0C`)과 읽기 헤더 `40 30`이 맞다. 리셋 직후 `PANADR`는 매번 `0xFFFFFFFF`였다 (기본값으로 보이나 데이터시트로 확인하지 않았다).

한계: 2 MHz에서만, 보드 2에서만, 세 번의 전원 투입에서 시험했다.

관찰 (원인 미확인):

- **IRQ 핀(PD12)이 처음부터 계속 1(High)로 읽혔다.** 초기화 직후(리셋 전)와 매 리셋 후 모두 1이었다. IRQ는 DW3000이 구동하는 출력이고 우리는 풀다운을 걸었으므로, 모듈이 High로 구동하고 있다는 뜻이다. 칩이 켜진 직후 처리되지 않은 상태 플래그(예: `SYS_STATUS`의 `RCINIT`, `SPIRDY`)가 있어 IRQ가 올라가 있을 가능성이 있지만 확인하지 않았다. Device ID 읽기에는 영향이 없었다. TWR 담당자가 인터럽트를 쓸 때 `SYS_STATUS`를 읽어서 확인하고 플래그를 지워야 한다.

## 알려진 제약과 확인 필요

- **보드 2에서 세 번 실행했다** (USB 재연결 2회 포함, 친구 문서의 "재부팅·재연결 반복" 항목). 보드 1에서의 시험은 아직이다.
- DEV_ID 읽기의 SPI 헤더는 `0x00` 한 바이트 (읽기, 레지스터 파일 0, 오프셋 0)다. 실제 읽기로 확인됐다. 다른 레지스터(오프셋이 있는 레지스터, 쓰기)의 헤더는 Qorvo 드라이버(`dwt_xfer3000`)가 만든다 (이 포팅은 1바이트 또는 2바이트 헤더를 그대로 받는다).
- 이 모듈(DWM3000 v1.4)의 Device ID는 `0xDECA0302`다. 다른 리비전의 모듈은 다른 값일 수 있어 `0xDECA03xx`를 "DW3000 계열"로도 센다.
- MOSI(D11)와 `uwb_spi_write()`는 2 MHz에서 시험됐다 (위 "MOSI 시험"). **8 MHz(`uwb_port_spi_fast()`)는 아직 시험하지 않았다.** Device ID는 초기화 전이라 느린 속도로만 읽었고, 빠른 속도에서의 동작은 Qorvo 초기화 이후 확인한다.
- 회로도(V1.2)와 실물(`Module shield v1.3`)의 리비전 차이는 확인하지 못했다.
- `.project`/`.cproject`를 바꾸면 Close Project → Open Project로 다시 읽힌다 (`../../삽질기록.md` 참고).
