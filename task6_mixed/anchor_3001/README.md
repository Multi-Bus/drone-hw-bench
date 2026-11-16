# Task 6 앵커: DWM3001CDK 펌웨어 (br101 드라이버 v08.02.02)

3001(nRF52833 + DW3110)에 Task 2 태그(STM32N6 + DWM3000EVB)와 **같은 드라이버**를 올리는 프로젝트다. 나중에 같은 libdeca를 올려 3001↔3000이 같은 프로토콜로 거리를 재게 한다.

시험 프로그램은 CMake 옵션 `T6_TEST`로 고른다.

| `T6_TEST` | 파일 | 내용 |
| --- | --- | --- |
| `devid` | `src/test_devid.c` | Lv1 1단계: Device ID 읽기 |
| `fast_irq` (기본) | `src/test_fast_irq.c` | Lv1 A단계: SPI 8 MHz, 인터럽트(`dwt_isr`) |

## 구성

| 항목 | 내용 |
| --- | --- |
| 드라이버 | br101/dw3000-decadriver-source 커밋 `67dfbb7` (08.02.02) + Task 2 패치 `0001-dw3000_device-export-ull-symbols.patch`. Task 2 `T2_UWB_ISR`과 같은 파일 묶음 |
| 플랫폼 포트 | 드라이버 저장소의 `platform/nrf-sdk/` 그대로 (SPIM0 사용, 2 MHz 시작) |
| nRF5 SDK | Qorvo DW3_QM33_SDK 1.1.1에 들어 있는 17.0.2 (`Drivers/API/Build_Platforms/nRF52840-DK/sdk`) |
| 핀 (`src/config.h`) | SCK P0.03, MOSI P0.08, MISO P0.29, CS P1.06, IRQ P1.02, RSTn P0.25. 출처: Qorvo SDK `Projects/FreeRTOS/CLI/DWM3001CDK/ProjectDefinition/uwb_stack_llhw.cmake` |
| WAKEUP | SDK는 P1.19로 적었지만 nRF52833 포트 1은 P1.00~P1.09뿐이라 사용하지 않음(-1). 포트가 CS로 깨운다 |
| 출력 | SEGGER RTT 채널 0 (J-Link RTT Viewer) |

드라이버 소스는 라이선스 혼재로 repo에 넣지 않는다 (`driver/`는 `.gitignore`).

## 준비

```bash
git clone https://github.com/br101/dw3000-decadriver-source.git /tmp/br101_decadriver
git -C /tmp/br101_decadriver checkout 67dfbb7
mkdir driver
cp -r /tmp/br101_decadriver/dwt_uwb_driver /tmp/br101_decadriver/platform driver/
cd driver && git apply ../../../task2_evb_stm32/lv2_spi/T2_UWB_ISR/driver_patches/0001-dw3000_device-export-ull-symbols.patch
```

도구: Arm GNU Toolchain 12.2.rel1, CMake, Ninja, J-Link.

## 빌드

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DT6_TEST=fast_irq -DNRF5_SDK="<Qorvo SDK>/Drivers/API/Build_Platforms/nRF52840-DK/sdk"
cmake --build build
```

결과: `build/anchor_3001.hex`.

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
