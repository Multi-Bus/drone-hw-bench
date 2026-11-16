# Task 6 앵커: DWM3001CDK 펌웨어 (br101 드라이버 v08.02.02)

3001(nRF52833 + DW3110)에 Task 2 태그(STM32N6 + DWM3000EVB)와 **같은 드라이버**를 올리는 프로젝트다. 나중에 같은 libdeca를 올려 3001↔3000이 같은 프로토콜로 거리를 재게 한다.

현재 단계: Lv1 1단계 Device ID 읽기 (`src/main.c`).

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
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNRF5_SDK="<Qorvo SDK>/Drivers/API/Build_Platforms/nRF52840-DK/sdk"
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

J-Link RTT Viewer (Device `nRF52833_xxAA`, SWD, 4000 kHz)를 열고 보드를 리셋한다. `verdict: PASS`가 나오면 통과다. 판정: 리셋 후 직접 읽은 DEV_ID 20/20이 `0xDECA0302`, `dwt_initialise` 성공, 드라이버로 읽은 `dwt_readdevid`도 `0xDECA0302`.

## 결과 (2026-11-16)

| 항목 | 결과 |
| --- | --- |
| 보드 | Task 1에 쓰던 3001. J-Link OB 시리얼 `760226397`, FICR 파트 `0x00052833` |
| 리셋 후 직접 읽은 DEV_ID | **20 / 20** `0xDECA0302` |
| `dwt_probe` / `dwt_initialise` | 0 / 0 (성공). `dwt_probe`가 CS로 칩을 깨움 (`WAKEUP CS`) |
| `dwt_readdevid` | `0xDECA0302` |
| 판정 | **PASS** |

원본: [`logs/t6_lv1_devid_jlink760226397.txt`](logs/t6_lv1_devid_jlink760226397.txt) (JLinkRTTLogger로 받은 RTT 출력, 옮겨 적은 값 아님).

- 지우기 전에 이 보드의 플래시 512 KB와 UICR을 `savebin`으로 백업했다 (repo 밖 `종합설계/ext/backup_3001/`). FiRa CLI 상태로 정확히 되돌릴 수 있다.
- 한 번의 전원 투입에서 얻은 결과다. 8 MHz SPI, 인터럽트(`dwt_isr`)는 아직 시험 전이다.
