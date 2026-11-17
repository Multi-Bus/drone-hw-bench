# Task 6 앵커: DWM3001CDK 펌웨어 (br101 드라이버 v08.02.02)

3001(nRF52833 + DW3110)에 Task 2 태그(STM32N6 + DWM3000EVB)와 **같은 드라이버**를 올리는 프로젝트다. 나중에 같은 libdeca를 올려 3001↔3000이 같은 프로토콜로 거리를 재게 한다.

시험 프로그램은 CMake 옵션 `T6_TEST`로 고른다.

| `T6_TEST` | 파일 | 내용 |
| --- | --- | --- |
| `devid` | `src/test_devid.c` | Lv1 1단계: Device ID 읽기 |
| `fast_irq` | `src/test_fast_irq.c` | Lv1 A단계: SPI 8 MHz, 인터럽트(`dwt_isr`) |
| `twr` (기본) | `src/test_twr.c` + `src/dw3000_hw_t6.c` | Lv1 B단계: libdeca DS-TWR. `-DT6_ROLE=responder`(앵커, 주소 `-DT6_ANCHOR_ADDR`, 기본 `0x0001`) 또는 `initiator`(태그 `0x0002`, `0x0001`에 거리를 물음) |

## 구성

| 항목 | 내용 |
| --- | --- |
| 드라이버 | br101/dw3000-decadriver-source 커밋 `67dfbb7` (08.02.02) + Task 2 패치 `0001-dw3000_device-export-ull-symbols.patch`. Task 2 `T2_UWB_ISR`과 같은 파일 묶음 |
| 플랫폼 포트 | 드라이버 저장소의 `platform/nrf-sdk/` 그대로 (SPIM0 사용, 2 MHz 시작) |
| nRF5 SDK | Qorvo DW3_QM33_SDK 1.1.1에 들어 있는 17.0.2 (`Drivers/API/Build_Platforms/nRF52840-DK/sdk`) |
| 핀 (`src/config.h`) | SCK P0.03, MOSI P0.08, MISO P0.29, CS P1.06, IRQ P1.02, RSTn P0.25. 출처: Qorvo SDK `Projects/FreeRTOS/CLI/DWM3001CDK/ProjectDefinition/uwb_stack_llhw.cmake` |
| WAKEUP | SDK는 P1.19로 적었지만 nRF52833 포트 1은 P1.00~P1.09뿐이라 사용하지 않음(-1). 포트가 CS로 깨운다 |
| 출력 | SEGGER RTT 채널 0 (J-Link RTT Viewer) |

드라이버와 libdeca 소스는 라이선스 혼재로 repo에 넣지 않는다 (`driver/`, `libdeca/`는 `.gitignore`).

## 준비

```bash
git clone https://github.com/br101/dw3000-decadriver-source.git /tmp/br101_decadriver
git -C /tmp/br101_decadriver checkout 67dfbb7
mkdir driver
cp -r /tmp/br101_decadriver/dwt_uwb_driver /tmp/br101_decadriver/platform driver/
cd driver && git apply ../../../task2_evb_stm32/lv2_spi/T2_UWB_ISR/driver_patches/0001-dw3000_device-export-ull-symbols.patch
```

`twr`에는 libdeca도 필요하다 (Task 2 태그와 같은 커밋 `226f3db3bc4ef2be9c143e8340a3851cd571cce9`, 수정 없음):

```bash
git clone https://github.com/br101/libdeca.git libdeca
git -C libdeca checkout 226f3db3bc4ef2be9c143e8340a3851cd571cce9
```

도구: Arm GNU Toolchain 12.2.rel1, CMake, Ninja, J-Link.

## 빌드

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DT6_TEST=fast_irq -DNRF5_SDK="<Qorvo SDK>/Drivers/API/Build_Platforms/nRF52840-DK/sdk"
cmake --build build
```

결과: `build/anchor_3001.hex`. TWR은 역할마다 빌드 폴더를 나눈다 (예: `-B build_anchor -DT6_ROLE=responder`, `-B build_tag -DT6_ROLE=initiator`).

빌드 폴더가 OneDrive 안에 있으면 링크가 `ld returned 5 exit status`로 실패할 때가 있었다 (`build_responder` 폴더에서만 반복, 출력 `.elf`가 0바이트). 폴더 이름을 바꾸니 됐다. 원인은 확인하지 못했다 (OneDrive 잠금으로 추정).

## 올리기

3001의 J-Link USB 포트에 연결한 뒤:

```bash
JLink.exe -device nRF52833_xxAA -if SWD -speed 4000 -autoconnect 1 -CommanderScript flash.jlink
```

`flash.jlink`는 칩 전체를 지운다. FiRa CLI로 되돌리려면 Qorvo SDK의 `SDK/Binaries/DWM3001CDK/DWM3001CDK-CLI-FreeRTOS.hex`를 같은 방법으로 올린다.

## 확인

J-Link RTT Viewer (Device `nRF52833_xxAA`, SWD, 4000 kHz)를 열고 보드를 리셋한다. `verdict: PASS`가 나오면 통과다. 기록은 `JLinkRTTLogger.exe -Device nRF52833_xxAA -If SWD -Speed 4000 -RTTChannel 0 <파일>`로 받았다.

## 결과 1: Device ID (`devid`, 2026-11-16)

| 항목 | 결과 |
| --- | --- |
| 보드 | Task 1에 쓰던 3001. J-Link OB 시리얼 `760226397`, FICR 파트 `0x00052833` |
| 리셋 후 직접 읽은 DEV_ID | **20 / 20** `0xDECA0302` |
| `dwt_probe` / `dwt_initialise` | 0 / 0 (성공). `dwt_probe`가 CS로 칩을 깨움 (`WAKEUP CS`) |
| `dwt_readdevid` | `0xDECA0302` |
| 판정 | **PASS** |

원본: [`logs/t6_lv1_devid_jlink760226397.txt`](logs/t6_lv1_devid_jlink760226397.txt) (JLinkRTTLogger로 받은 RTT 출력, 옮겨 적은 값 아님).

- 지우기 전에 이 보드의 플래시 512 KB와 UICR을 `savebin`으로 백업했다 (repo 밖 `종합설계/ext/backup_3001/`). FiRa CLI 상태로 정확히 되돌릴 수 있다.
- 한 번의 전원 투입에서 얻은 결과다.

## 결과 2: SPI 8 MHz + 인터럽트 (`fast_irq`, 2026-11-16)

같은 보드(J-Link `760226397`). libdeca의 `dwhw_init()`이 초기화 뒤 `dw3000_spi_speed_fast()`를 부르므로, libdeca를 올리기 전에 8 MHz와 인터럽트 경로를 따로 확인했다. 인터럽트 시험은 Task 2 `T2_UWB_ISR`과 같은 구성(A: IRQ가 이미 High, B: 리셋 20회)이다.

| 항목 | 결과 |
| --- | --- |
| 8 MHz 설정 | `NRF_SPIM0->FREQUENCY = 0x80000000` (8 MHz) |
| 8 MHz Device ID | `dwt_readdevid` **1000/1000**, 직접 읽기 **1000/1000** `0xDECA0302` |
| 8 MHz 쓰기·되읽기 | `PANADR`에 8개 패턴 × 50회 **400/400** 일치, SPI 오류 0, 원래 값(`0xFFFFFFFF`) 복원 |
| A) IRQ가 이미 High인 채 인터럽트 켬 | 켠 뒤 IRQ=1, 인터럽트로 온 콜백 **0** (상승 에지가 없음). `dwt_isr()`를 한 번 직접 부르니 `SPIRDY` 콜백 1회, IRQ=0, `SYS_STATUS=0x00000004`. **PASS** |
| B) 인터럽트 켠 채 하드웨어 리셋 20회 | **20/20** 리셋마다 `SPIRDY` 콜백 정확히 1회, IRQ=0, `SYS_STATUS=0x00000004`. **PASS** |
| 판정 | **PASS** |

원본: [`logs/t6_lv1_fast_irq_jlink760226397.txt`](logs/t6_lv1_fast_irq_jlink760226397.txt).

알게 된 것 (libdeca 통합에 반영할 것):

- **nRF 포트는 IRQ가 이미 High일 때 인터럽트를 켜면 아무 일도 일어나지 않는다.** GPIOTE가 Low→High 에지만 보기 때문이다. 초기화 뒤에는 `SPIRDY`가 남아 IRQ가 High이므로, 인터럽트를 켠 직후 핀이 High면 `dwt_isr()`를 한 번 불러 줘야 한다 (Task 2는 같은 문제를 `NVIC_SetPendingIRQ`로 처리).
- 인터럽트를 켜기 전에는 IRQ 핀이 0으로 읽혔다 (`before enabling: IRQ=0`). 핀이 아직 입력으로 설정되지 않아 입력 버퍼가 끊긴 상태로 보인다 (추정). 실제 상태는 `SYS_STATUS`의 `SPIRDY`(`0x01800005`)로 확인했다.
- 플랫폼 포트의 인터럽트 핸들러는 IRQ가 High인 동안 `dwt_isr()`를 무한 반복한다 (`platform/nrf-sdk/dw3000_hw.c`). 이 시험에서는 매번 한 번에 내려갔다.
- 인터럽트 시험은 2 MHz에서 했다. 8 MHz는 폴링 읽기·쓰기만 시험했다. 한 번의 전원 투입에서 얻은 결과다.

## 결과 3: libdeca DS-TWR, 3001 두 개 (`twr`, 2026-11-16)

앵커(responder `0x0001`) = 보드 J-Link `760226397`, 태그(initiator `0x0002`) = 보드 J-Link `760226502`. 두 번째 보드도 지우기 전에 플래시와 UICR을 백업했다 (`종합설계/ext/backup_3001_760226502/`).

설정은 Task 2 태그(`task2_evb_stm32/lv2/uwb_app/uwb_app.c`)와 같다: libdeca 기본 PHY (채널 9, 6.8 Mbps, PRF 64 MHz, 프리앰블 64, 코드 11), PAN `0xDECA`, 처리 지연 2000 µs, 보고 메시지 켬, 안테나 딜레이 libdeca 기본값 16368 (보정 전). 태그는 100 ms마다 `twr_start(0x0001)`. 시작 순서는 Task 2 `HANDOFF_2026-11-16.md`대로 드라이버·libdeca 초기화를 먼저 하고 인터럽트를 마지막에 켠다.

| 배치 | 시간 | 태그 결과 | 거리 (태그 기준) | 원본 |
| --- | --- | --- | --- | --- |
| **약 1 m** (앵커만 고정, 태그는 손으로 들고 있었음. 거리를 재지 않음) | 32 s | 시작 300회 중 성공 299, 실패 0 (STAT 기준). 재시도·타임아웃 로그 20줄 | n = 318, 평균 **107.5 cm**, 표준편차 5.1, 최소 92, 중앙 107, 최대 127 | `logs/t6_lv1_twr_1m_*.txt` |
| 두 보드를 책상 위에 가까이 둠 (거리 기록 안 함) | 30 s | 시작 300회 중 성공 0, 실패 299 | ToF가 모두 음수 (−9 ~ −38 DTU) → libdeca가 0 cm로 처리하고 재시도 | `logs/t6_lv1_twr_close_*.txt` |

- 앵커도 같은 거리를 계산한다 (1 m에서 n = 328, 평균 107.3 cm). 태그 값은 앵커가 보낸 보고 메시지 값이다.
- 태그를 손으로 들고 있어서 실제 거리가 1.00 m가 아니었다. 그래서 평균 107.5 cm를 오차나 보정값 근거로 쓰지 않는다. 이 시험이 보여 주는 것은 3001 두 개 사이에서 libdeca DS-TWR이 동작한다는 것(300회 중 299 성공)까지다. 표준편차에도 손 떨림이 섞여 있다.
- 가까이 둔 시험에서는 20초쯤부터 앵커 로그가 크게 줄었다. 1 m 시험(32초)에서는 같은 현상이 없었다. 원인은 확인하지 못했다.

### 플랫폼 포트 수정 (`src/dw3000_hw_t6.c`)

br101 `platform/nrf-sdk/dw3000_hw.c`의 `dw3000_hw_interrupt_enable/disable/is_enabled`는 IRQ **핀 번호**(34)를 `nrf_gpiote_int_enable/disable/is_enabled()`에 넘긴다. 이 함수들은 `INTENSET/INTENCLR`에 쓰는 **채널 비트마스크**를 받는다 (nRF5 SDK `hal/nrf_gpiote.h`). 34 = IN1|IN5라서 실제 채널(IN0)은 막히지 않고, 드라이버의 `decamutexon()`이 SPI 전송 중 DW3000 인터럽트를 막지 못한다 (코드로 확인, 이로 인한 오동작을 직접 관찰하지는 않았다). TWR 빌드는 이 파일 대신 `src/dw3000_hw_t6.c`를 쓴다:

- 뮤텍스는 NVIC에서 `GPIOTE_IRQn`을 막는다. 막힌 동안 생긴 에지는 `EVENTS_IN`에 남아 있다가 풀릴 때 처리된다.
- `dw3000_hw_init_interrupt()`가 IRQ가 이미 High면 한 번 처리한다 (A단계에서 찾은 문제).
- 핸들러 반복은 8회로 제한하고 넘으면 센다 (`irq_loop_limit`, 이번 시험 0회).
- `devid`, `fast_irq`는 시험했던 원래 파일을 그대로 쓴다.

### 안테나 딜레이 보정에 대해

libdeca는 런타임 설정(`dwphy_set_antenna_delay()` → `dwt_settx/rxantennadelay`)과 별개로, 메시지에 넣는 송신 시각에 **컴파일 상수** `DWPHY_ANTENNA_DELAY`(`dwphy.h`, 16368)를 더한다 (`ranging.c:170, 206`, `sync.c`). 그래서 보드마다 다른 보정값을 쓰려면 런타임 값만 바꿔서는 안 되고 이 상수도 맞춰야 한다. libdeca를 고치거나 빌드 정의로 바꾸는 방법은 Task 2 태그와 같이 정해야 해서 아직 하지 않았다.

## 현재 보드 상태 (2026-11-17, 3001↔3000 시험 대기)

Task 2 `6689219`에서 N6 쪽 처리 지연이 2000 → **5000 µs**로 바뀌어 3001도 5000으로 맞췄다 (`src/test_twr.c`). 다시 올린 뒤 두 보드 모두 RTT에 `TWR: delay 5192 us RX delay 5078 us`와 `listening`이 나왔다.

| 보드 (J-Link) | 펌웨어 | 빌드 |
| --- | --- | --- |
| `760226397` | `twr` responder, 앵커 **`0x0001`** | `-B build_anchor -DT6_ROLE=responder` |
| `760226502` | `twr` responder, 앵커 **`0x0003`** | `-B build_anchor3 -DT6_ROLE=responder -DT6_ANCHOR_ADDR=0x0003` |

3000↔3000과 3000↔3001 동시 측정에서는 N6 보드 2가 주소 1을 쓰므로 `760226397`(`0x0001`)은 끈다. 3001끼리 시험하려면 한 보드를 `initiator`로 다시 올린다 (처리 지연 5000 µs로는 3001끼리 시험하지 않았다).

## 결과 4: 3000(N6 태그) ↔ 3001 앵커 첫 연결 (2026-11-17)

구성: N6 보드 1 = 태그 (Task 2 새 펌웨어, 시작 줄 `role=initiator address=2 peers=1,3 ant=16400`, 출력 8열), N6 보드 2 = 앵커 1, 3001 `760226502` = 앵커 3. 배치 거리는 재지 않았다.

| 단계 | peer 1 (N6 앵커) | peer 3 (3001 앵커) |
| --- | --- | --- |
| 처음 | 전부 FAIL, N6 보드 2 HEALTH `entries` 4에서 멈춤 | 전부 FAIL, 3001 수신 기록 없음 |
| 3001 수신 전용 감시 모드 (`-DT6_SNIFF=1`: 필터 끔, 응답 안 함) | | 12초에 태그 프레임 311개 (PAN `DECA`, src `0002`, dst `0001`/`0003` 번갈아, 수신 오류 0). 태그 송신과 PHY는 정상 |
| 3001에 ARFE 인터럽트 추가 (`T6_ARFE_FIX=1`) | 111회 전부 FAIL (N6는 그대로) | **110/110 OK** (출력 100~193 cm). 15초 재확인 76/76 OK |

원본: `logs/t6_lv1_mixed_sniff_jlink760226502.txt`, `logs/t6_lv1_mixed_arfe_fix_anchor3_jlink760226502.txt`, `logs/t6_lv1_mixed_arfe_fix_tag_n6board1.txt` (태그 UART 15초).

**원인:** libdeca `dwmac_init()`은 주소 필터 거부(ARFE) 인터럽트를 `CONFIG_DECA_DEBUG_FRAME_FILTER`일 때만 켠다. 앵커가 둘 이상이면 각 앵커가 다른 앵커 주소로 가는 프레임을 듣고 필터가 거부하는데, 알림이 없어 수신기가 다시 켜지지 않는다 (관찰로 추정, DW3000 데이터시트로는 확인 못 함). `dwt_isr()`가 ARFE를 수신 오류로 처리하고 libdeca 오류 콜백이 `rx_reenable`이면 수신을 다시 켜므로, 앱에서 `dwt_setinterrupt(DWT_INT_ARFE_BIT_MASK, 0, DWT_ENABLE_INT)`만 추가했다 (libdeca 소스는 그대로). N6 쪽에도 같은 수정이 필요하다.

- 거리 값은 보정 전이다 (3001 16368, N6 16400, libdeca 상수 16368). 배치도 재지 않았다.
- `STAT` 줄 다음에 이벤트 카운터(`EVC,crc_good,arfe,phe,sfdto,tx`)를 출력하도록 추가했다.

## 결과 5: N6 앵커 수정 후 두 쌍 동시 확인 (2026-11-17)

Task 2 쪽이 태그 `--peers 1,3`(원격 `task6` 브랜치 `c79da61`, `d49ff20`, stevechung7)과 N6 앵커의 ARFE 수정을 보드에 올린 뒤 확인했다. 배치 거리는 재지 않았다 (두 앵커를 나란히 두지도 않음).

| 확인 | 결과 |
| --- | --- |
| 20초 그대로 | peer 1 **100/100 OK**, peer 3 **100/100 OK**. N6 앵커 HEALTH `entries`가 계속 늘어남 (1032 → 1634) |
| 3001 CPU를 J-Link로 멈춤 (6초) | peer 3만 FAIL 30, peer 1은 OK 30. 다시 돌리자 바로 둘 다 OK (31 / 30) |
| 로거 (`log_twr.py --peers 1,3 --samples 50`) | 상대별 요약 생성, 둘 다 50/50, 누락 0, `queue_dropped` 0 |

원본: `logs/t6_lv1_mixed_check_peers.csv`, `.raw.txt`, `.summary.json`. 평균 출력은 peer 1 0.22 m, peer 3 1.27 m인데 두 앵커가 나란히 있지 않았고 보정 전이라 비교하지 않는다.
