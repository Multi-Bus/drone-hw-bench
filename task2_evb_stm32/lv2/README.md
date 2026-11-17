# Task 2 Lv2 — N6 + DWM3000EVB DS-TWR

## 현재 상태 (2026-11-17)

- 두 세트 모두 보완된 드라이버/IRQ 시험 PASS: 리셋 20/20, handler/hook/SPIRDY 각 1회, IRQ Low, loop-limit 0.
- 두 역할의 전체 펌웨어 컴파일·링크 및 RAM 실행 확인. SPI는 **2 MHz 유지**.
- 두 N6 외부 플래시 `0x70000000`에 부팅 이미지 저장 및 읽기 검증 PASS. 쓰기 전 첫 128 KiB는 로컬에 백업했다. 보안/OTP 설정은 변경하지 않았다.
- JP1/BOOT0와 JP2/BOOT1을 모두 1–2로 설정하고 USB 전원을 다시 연결한 후, **다운로드/실행 명령 없이** 양쪽 TWR 거리 출력 확인. 역할별 외부 플래시 이미지의 기본 안테나 지연은 각각 **16400**.
- UART → CSV 저장, 실패 포함 집계, 약 50 cm 책상 배치에서 300회 연속 출력과 흔들림 확인. **사용자가 기준거리의 부정확함을 알려 절대 정확도·보정 완료 판정은 하지 않는다.**
- **미완료:** 수정 과제 PDF의 **정확히 1.00 m에서 300회 측정·최종 보정**과 해당 보정값 재검증. 50 cm 결과로 대체 완료 처리하지 않는다.
- Lv3의 여러 거리 조건, 8 MHz, sleep/wakeup, write_crc 및 Task 6 혼합 쌍은 이번 검증 범위가 아니다.

## 보드와 실행 설정

| 구분 | 보드 1 | 보드 2 |
|---|---|---|
| 장치 | NUCLEO-N657X0-Q + DWM3000EVB | 동일 |
| 역할 / short address | initiator / 2 | responder / 1 |
| ST-LINK SN | `004700233335511035383531` | `0036004D3335510735383531` |
| 이번 PC의 UART | COM12 | COM13 |
| 임시 TX/RX antenna delay | 16400 | 16400 |

PC 연결은 N6의 **CN10 ST-LINK USB**다. CN8 USER USB가 아니다. COM 번호는 PC에 따라 달라지므로 ST-LINK SN으로 보드를 구분한다. UART는 기존 ST 예제와 같은 **115200 / 7 data bits / odd parity / 1 stop / flow control 없음**이다.

프로토콜은 **libdeca DS-TWR**이며 두 장치가 같은 버전을 사용한다. libdeca 기본 PHY: channel 9, preamble 64, codes 11, 6.8 Mbps, STS off. 애플리케이션 processing delay는 5000 us, 시작 주기는 약 100 ms다. libdeca 내부 재시도 최대 3회가 한 application attempt에 포함되므로 성공률은 **개별 RF 프레임 성공률이 아니다**. Task 6의 3001도 이 프로토콜/PHY와 맞춰야 한다. FiRa CLI와 바로 호환되지 않는다.

핀맵은 [기존 핀맵](../lv2_spi/pinmap.md)을 사용한다. 안테나 지연 16400은 이 보드 쌍과 책상 위 50 cm 배치에서 정한 **임시값**이며 다른 장치나 환경의 보정값으로 일반화하지 않는다.

## 약 50 cm 책상 배치: 연속 통신과 출력 흔들림

배치: 사용자가 약 50 cm를 재서 배치했으나 정밀 기준은 아니라고 정정했다. N6 보드는 책상에 눕힌 상태이며 높이는 별도 측정하지 않았다. [배치 사진](data/2026-11-17/desk-placement.png)에 줄자는 없으며, 실제 길이를 독립적으로 검증하지 않았다. 분리·재조립 후에는 다시 배치했다. 책상/방향에 따른 다중경로 영향도 있을 수 있다. 출력 해상도는 1 cm다. **다음 평균은 거리 정확도가 아니라 각 구간의 출력값이며, 구간 간 같은 배치라고 가정하지 않는다.**

| 구간 | 성공/시도 | 평균 출력 | 표본 표준편차 | 최대 출력 |
|---|---|---|---|---|
| 재조립 전, 양쪽 delay 16368 | 300/300 | 0.7640 m | 3.98 cm | 0.84 m |
| 재조립 전, 양쪽 delay 16396 | 300/300 | 0.5329 m | 3.59 cm | 0.61 m |
| 재조립 전, 양쪽 delay 16400 | 300/300 | 0.5105 m | 3.94 cm | 0.58 m |
| 외부 플래시 부팅·재조립 후, delay 16400 | 300/300 | 0.2580 m | 2.36 cm | 0.36 m |
| 같은 지연값 UART 재적용, 재조립 후 | 100/100 | 0.2400 m | 2.59 cm | 0.31 m |

위 구간은 모두 attempt ID 누락 0, queue_dropped 0이다. 16400은 동작 확인 중 선택한 임시 설정값이다. 재조립 후 평균이 달라졌고, 같은 값을 다시 적용해도 앞선 평균이 재현되지 않았다. 원인이 기준거리·방향·환경인지 펌웨어인지 분리 검증하지 않았으므로 **0.5105 m를 근거로 보정 성공을 주장하지 않는다.** RSSI/first-path power는 현재 포트가 출력하지 않아 빈칸이다.

데이터의 `nominal_distance_m=0.5`, `reference_quality=approximate`이며, `actual_distance_m`와 정확도 지표(평균 오차/95% 절대오차)는 비워 두었다. 원본 UART 값은 그대로 보존했다. 파일 이름의 baseline/trial/validation은 수집 구간 이름이며 정식 교정 인증을 뜻하지 않는다.

원본: [data/2026-11-17](data/2026-11-17). 각 CSV 옆에 `.raw.txt`와 `.summary.json`이 있다. `50cm-pre-recovery`는 앞선 수신 중단 상태를 30회 기록한 **0/30 실패 데이터**이며 숨기지 않고 함께 보존했다. 재시작 후 50 cm에서는 연속 성공했으나 중단 원인은 확정하지 못했다. receiver 재시작 명령을 제공하며 장시간 안정성 검증은 별도다.

## 빌드

외부 의존성은 팀 저장소에 넣지 않았다.

- STM32CubeN6 **V1.4.1**, ARM GCC **14.3** (이번 PC의 CubeIDE 2.2.0 번들).
- `br101/libdeca` **226f3db3bc4ef2be9c143e8340a3851cd571cce9**.
- `br101/dw3000-decadriver-source` **67dfbb7f2c5b1a4157b8c265b19f08776e91b1cc** + [4개 static 선언 제거 패치](../lv2_spi/T2_UWB_ISR/driver_patches/0001-dw3000_device-export-ull-symbols.patch).

이번 재현 경로는 **`tools/build_firmware.py`**다. 기존 CubeIDE `.cproject`는 개인 PC 경로가 남은 개발 스냅샷이므로 그대로의 IDE 빌드가 이 결과와 같다고 가정하지 않는다. 스크립트는 의존성 커밋과 static 패치를 확인하고 최신 어댑터/앱/드라이버/libdeca/HAL 전체를 직접 링크한다. 출력은 저장소 루트의 ignored `build/t2_<role>/`다.

```powershell
# 저장소 루트. 아래 세 외부 소스 경로와 도구 경로는 PC에 맞게 지정한다.
$cube = 'C:/Users/jmhwa/STM32Cube/Repository/STM32Cube_FW_N6_V1.4.1'
$driver = '<dw3000-decadriver-source 폴더>'
$libdeca = '<libdeca 폴더>'
$toolbin = '<arm-none-eabi-gcc.exe가 있는 폴더>'
python task2_evb_stm32/lv2/tools/build_firmware.py --cube-root $cube --driver-root $driver --libdeca-root $libdeca --tool-bin $toolbin --role initiator --antenna-delay 16400
python task2_evb_stm32/lv2/tools/build_firmware.py --cube-root $cube --driver-root $driver --libdeca-root $libdeca --tool-bin $toolbin --role responder --antenna-delay 16400
```

스크립트 기본 delay는 라이브러리 기본값 16368이다. 이 쌍의 임시값을 재현하려면 위처럼 16400을 명시한다. `--fast`를 주지 않으면 libdeca가 fast를 요청해도 2 MHz를 유지한다. 다른 SPI 프로젝트의 기본 fast 구현은 그대로다.

출력: ELF/BIN/map/build.log/manifest.json. 외부 소스에는 unused 계열 경고 등이 남아 있고, 전체 빌드가 무경고라고 주장하지 않는다. 로깅 shim의 timestamp 매크로 중복은 제거했다.

## 저장 및 부팅

STM32CubeProgrammer **2.23.0**과 ST 공식 Nucleo loader `MX25UM51245G_STM32N6570-NUCLEO.stldr`를 사용했다. 외부 플래시 전체 지우기는 하지 않았다. 새 이미지가 차지한 sector 0–1만 지웠고, 앞 128 KiB를 쓰기 전에 백업했다. 이미지 해시는 [flash-images.json](data/2026-11-17/flash-images.json)에 기록했다.

```powershell
$cpbin = 'C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin'
# 각 역할의 BIN에 실행용 부트 헤더 생성. -nk는 키/OTP를 프로비저닝하지 않는다.
& "$cpbin/STM32_SigningTool_CLI.exe" -s -bin build/t2_initiator/t2_initiator.bin -nk -of 0x80000000 -t fsbl -o build/t2_initiator/t2_initiator-trusted.bin -hv 2.3 -align
& "$cpbin/STM32_SigningTool_CLI.exe" -s -bin build/t2_responder/t2_responder.bin -nk -of 0x80000000 -t fsbl -o build/t2_responder/t2_responder-trusted.bin -hv 2.3 -align
```

다시 쓰는 작업은 전원을 끈 뒤 BOOT1/JP2를 **2–3 개발 모드**로 설정하고 전원을 연결해 수행한다. BOOT0/JP1은 1–2로 둔다. ST-LINK SN을 명시해 잘못된 장치를 쓰지 않는다. backup 파일은 기존 파일을 덮어쓰지 않도록 새 이름을 쓴다.

```powershell
$loader = "$cpbin/ExternalLoader/MX25UM51245G_STM32N6570-NUCLEO.stldr"
$sn = '004700233335511035383531' # 보드1. 보드2는 0036004D3335510735383531
& "$cpbin/STM32_Programmer_CLI.exe" -c port=SWD "sn=$sn" ap=1 mode=HOTPLUG -el $loader -u 0x70000000 0x20000 '<새 백업 파일>.bin'
& "$cpbin/STM32_Programmer_CLI.exe" -c port=SWD "sn=$sn" ap=1 mode=HOTPLUG -el $loader -w build/t2_initiator/t2_initiator-trusted.bin 0x70000000 -v
```

보드2는 responder 이미지를 선택한다. 저장·verify 성공 후 **USB 전원을 끄고 JP1/BOOT0, JP2/BOOT1 모두 1–2**로 옮겨 flash boot로 둔다. 전원을 끈 상태에서 DWM3000EVB를 원래 방향으로 다시 장착한다. 두 USB를 CN10에 연결하면 자동 실행한다. 번호/위치가 가려지면 전원을 끄고 shield를 분리해서 확인하며, CN9 전원 선택과 JP3 STLK_RST는 바꾸지 않는다. 보드 배치는 [ST UM3417](https://www.st.com/resource/en/user_manual/um3417-stm32n6-nucleo144-board-mb1940-stmicroelectronics.pdf), 부트 헤더 방법은 STM32CubeN6 UART_Printf README 기준이다.

RAM 개발 실행만 필요할 때는 개발 모드에서 ELF를 올려 실행할 수 있지만, 전원이 꺼지면 사라진다. 이번 인수인계 보드는 외부 플래시 부팅 방식이다.

## CSV 기록과 명령

```powershell
python -m pip install pyserial
python task2_evb_stm32/lv2/tools/log_twr.py --port COM12 --samples 300 --distance 0.5 --condition desk_50cm_check --ant-initiator 16400 --ant-responder 16400 --out '<새 데이터 폴더>/check.csv'
```

로거는 **initiator 포트**를 사용한다. 기본 `--reference-quality approximate`는 오차 지표를 계산하지 않는다. 정확히 측정한 기준거리에서만 `--reference-quality measured`를 명시한다. 실패와 TIMEOUT을 보존하고, 누락 attempt도 성공률 분모에 포함한다. 캡처 도중 재부팅/중복 ID가 있으면 오류로 처리한다. 수집 완료는 정확도 합격을 뜻하지 않는다. `.summary.json`의 성공률과 누락·큐 손실을 함께 확인한다. `timestamp`는 PC의 UTC 시각, `tick_ms`는 MCU tick이다. 내부 RF 재시도 횟수는 별도로 출력하지 않는다.

한 포트는 한 프로그램만 열 수 있다. Tera Term과 로거를 동시에 같은 COM에 연결하지 않는다. 측정 중 위치/방향/보정값을 바꾸지 않는다.

`tools/uart_command.py --port COM12 s`로 initiator를 멈추고, `--port COM12 r`로 재개한다. responder(COM13)의 `r`은 RX를 다시 시작한다. 멈춘 상태에서 `a16400` 같은 명령으로 각 포트의 TX/RX 지연을 바꿀 수 있다. 일치하는 ACK를 확인해야 적용된 것이다. **UART 설정은 RAM에만 남으므로 전원 후 유지하려면 해당 값으로 다시 빌드/저장해야 한다.**

## 앵커 두 개에 번갈아 묻는 모드 (`--peers`, Task 2 + Task 6 동시 측정)

요청서 `N6_태그_수정요청.md`(2026-11-17, Task 6 → Task 2. 저장소에는 없다)에 따라 추가했다. 태그(N6 보드 1, 주소 2)가 상대 목록을 시도마다 번갈아 묻는다. 한 시도가 끝난 뒤 다음 상대로 넘어가므로 한 번에 한 쌍만 통신한다.

- 빌드(태그만): `build_firmware.py ... --role initiator --antenna-delay 16400 --peers 1,3`. `--peers`를 주지 않으면 지금까지의 단일 상대(주소 1) 펌웨어와 **같은 출력**(7열, 시작 줄 `peer=1`)이고, 응답자 빌드에는 쓸 수 없다(오류). 주소 2(태그 자신)와 중복 주소도 거부한다. manifest의 `peers`에 기록된다.
- 출력(`--peers`를 준 태그): 시작 줄 `# T2 LIBDECA DS-TWR role=initiator address=2 peers=1,3 ant=16400`, 머리말 `# TWR,tick_ms,attempt,sequence,peer,distance_cm,status,queue_dropped`. `attempt`는 전체 시도 번호이고 시도 n은 `peers[(n-1) % 개수]`에게 묻는다. TIMEOUT 줄에도 그 시도의 `peer`가 들어간다.
- 로거: 8열과 기존 7열을 모두 읽는다. CSV에 `peer` 열이 추가된다(7열 입력은 비어 있음). `--samples`는 **상대마다** 센 개수이고, `.summary.json`은 `peers` 아래 상대별로 시도, 성공, 성공률, 누락, 평균, 표본 표준편차, 최대, `queue_dropped`를 낸다. 시도 번호는 전체에서 연속이어야 하고, 번호가 빠지면 그 차례였던 상대의 누락으로 센다. 상대 순서는 입력에서 추론하며, 한 상대가 아예 응답하지 않을 수 있으면 `--peers 1,3`으로 알려 준다.

```powershell
python task2_evb_stm32/lv2/tools/log_twr.py --port <태그 COM> --samples 300 --distance 1.0 --condition mixed_1m --peers 1,3 --ant-initiator 16400 --ant-responder 1=16400,3=16368 --out '<새 데이터 폴더>/mixed_1m.csv'
```

- 검증 상태: 로거는 단위 테스트(`tools/test_log_twr.py`)와 가짜 8열 입력으로 확인했고, 기존 7열 로그(`50cm-baseline.raw.txt`)로 다시 돌려 커밋된 요약과 같은 값이 나온다. 펌웨어는 `--peers 1,3`, 단일 상대, 응답자 세 가지를 모두 빌드했고(경고 없음), 단일 상대 빌드는 기준 커밋 `6689219`와 출력 문자열과 코드 크기가 같다. **보드 시험 (2026-11-17, 태그 = N6 보드 1 `0047…531`, 앵커 A = N6 보드 2 `0036…531`):** 태그에 `--peers 1,3` 이미지를 올려 확인한 것 — 시작 줄 `peers=1,3`, 8열 결과 줄, peer 1/3 번갈아 시도, 20초 동안 멈춤 없음(201회). 외부 플래시에 써서(JP2 1–2) 자동 부팅해도 같다. **아직 확인하지 못한 것(요청서 완료 확인 2~4번):** 태그가 앵커 A/3001에게서 꾸준히 `OK`를 받는 것, 3001을 껐을 때 peer 1만 `OK`인지, peer별 요약을 실제 캡처로 만든 것. 3001과는 한 번도 통신하지 못했다(3001이 이 PC에서 안 보임).
  - 새 이미지에서 `FAIL`이 이어졌다(첫 시도만 `OK`). 앵커 A는 교환 한 번을 마친 뒤 수신을 멈추는 모습이 반복됐다(`# HEALTH entries`가 늘지 않고 `state=00030000`). **원인은 모른다.** 후보: (1) 보드 간 거리가 너무 가깝거나 안테나 지연 16400의 영향으로 계산된 거리가 0 이하가 되면 libdeca(`ranging.c`)가 이를 실패로 보고 재시도한다 — 거리를 약 1 m로 띄워 확인 전, (2) 존재하지 않는 peer(3001이 꺼진 상태)에게 묻는 시도가 이어지는 교환에 영향을 주는지 확인 전. 기존 단일 상대 펌웨어로도 한때 880회 연속 실패한 적이 있어 이 모드만의 문제로 보지는 않는다.
  - 개발 모드(JP2 2–3)에서 RAM에 올려 실행하면 AP1 접속이 막히고 CPU 재개가 불안정했다. 새 이미지는 서명(`STM32_SigningTool_CLI -nk -of 0x80000000 -t fsbl -hv 2.3 -align`)해서 외부 플래시 `0x70000000`에 쓰고(`-el ...stldr -w ... -v`, 쓰기 전에 앞 128 KiB 백업) JP2를 1–2로 되돌려 부팅하는 편이 안정적이었다.

## 앵커가 둘 이상일 때 수신이 멈추는 문제와 수정 (ARFE 인터럽트, 2026-11-17)

**증상:** 태그가 앵커 두 개(주소 1, 3)에 번갈아 묻기 시작하면 앵커 A(주소 1)가 첫 교환 뒤 더 이상 받지 못하고(`# HEALTH entries`가 늘지 않고 `state=00030000`) 태그는 `FAIL`만 낸다. 앞서 단일 상대 펌웨어에서 보인 "수신 중단"도 같은 원인인지는 확인하지 못했다.

**원인 (코드로 확인, 동작은 관찰로 추정):** 앵커는 하드웨어 주소 필터(`dwmac_set_frame_filter()`)를 켠다. 다른 앵커 주소로 가는 프레임(예: peer 3으로 가는 poll)은 필터가 거부하는데, 주소 필터 거부 인터럽트(`DWT_INT_ARFE_BIT_MASK`)가 꺼져 있으면 칩이 아무 이벤트 없이 수신을 멈춘다. libdeca는 수신을 다시 켜는 코드를 에러 콜백(`dwmac_irq_err_cb`, `rx_reenable`일 때 `dwt_rxenable`)에 두었는데 이 콜백이 불리지 않는다. libdeca는 ARFE를 `CONFIG_DECA_DEBUG_FRAME_FILTER` 빌드에서만 켠다(`dwmac.c`). 이 사실은 Task 6 담당(손윤기)의 3001 시험에서도 독립적으로 확인됐다 (3001에만 수정을 적용하자 peer 3 110/110 `OK`, 수정 안 한 N6 앵커 peer 1은 111회 전부 `FAIL`).

**수정:** `uwb_app/uwb_app.c`의 `uwb_app_init()`에서 `dwmac_set_frame_filter()` 바로 뒤, IRQ를 켜기 전에 한 줄을 추가했다. libdeca 소스는 고치지 않았다. 태그와 앵커 빌드 모두에 들어간다.

```c
dwt_setinterrupt(DWT_INT_ARFE_BIT_MASK, 0, DWT_ENABLE_INT);
```

**보드 상태 (2026-11-17):**

| 보드 | 플래시 이미지 | 크기 | SHA256 | ARFE 수정 |
| --- | --- | --- | --- | --- |
| 앵커 A (N6 보드 2, ST-LINK `0036…531`) | `t2_responder-trusted.bin` (`--antenna-delay 16400`) | 65,696 B | `e7455e9b14ca75b4125f2564cee725597a913fe425ecf3443dff8969cb89bfb1` | 있음 |
| 태그 (N6 보드 1, ST-LINK `0047…531`) | `t2_initiator-trusted.bin` (`--peers 1,3 --antenna-delay 16400`) | 65,984 B | `bffff1dc2c08812e1c9f65d628252ca9e0e6cd05bb3704e70713516a728eda6e` | **없음** (수정 전 빌드) |

- 쓰기 전에 두 보드 모두 외부 플래시 앞 128 KiB를 백업했다(백업 파일은 저장소에 넣지 않았고 팀원 PC가 아니라 작업 PC의 `종합설계` 폴더에 있다). 쓰기와 verify는 `Download verified successfully`로 통과했고 JP2를 1–2로 되돌린 뒤 앵커는 수신 대기 상태(`state=01170500`)로 자동 부팅한다.
- **아직 확인하지 못한 것:** 수정이 실제로 앵커 멈춤을 없애는지(앵커 A의 `entries` 증가, 태그에서 peer 1 `OK`)는 태그와 함께 돌려 보지 않았다. 앵커의 UART 출력은 수정 전후가 같아서 부팅만으로는 구분되지 않는다. 3001(peer 3)과의 통신도 이 보드 조합에서는 시험하지 않았다.
- 서명은 `STM32_SigningTool_CLI -s -bin <bin> -nk -of 0x80000000 -t fsbl -o <trusted> -hv 2.3 -align`, 쓰기는 `-el MX25UM51245G_STM32N6570-NUCLEO.stldr -w <trusted> 0x70000000 -v`(개발 모드 JP2 2–3, ST-LINK 번호를 명시)다. 쓴 뒤 JP2를 1–2로 되돌려야 플래시에서 부팅한다.

## 수정 과제 기준으로 남은 일

1. 정확한 1.00 m, 같은 높이/방향, 안테나 기준점이 보이는 배치 사진을 준비한다.
2. 먼저 libdeca의 future TX timestamp 계산이 하드웨어 TX antenna delay와 같은 값을 쓰도록 확인·정리한다. 현재 외부 libdeca `ranging.c`는 `DWPHY_ANTENNA_DELAY=16368` 상수를 사용하고, 앱은 하드웨어 지연을 바꿀 수 있다. 정식 보정 전에 이 둘의 일치를 보장해야 한다. 이후 두 보드 delay를 기록하며 300회 측정·보정하고 조정/최종 검증 데이터를 분리한다. 쌍의 오차를 조정한 것으로 각 보드의 물리적 지연을 독립 추정했다고 말하지 않는다.
3. 최종값을 역할별 펌웨어에 포함해 다시 저장하고 전원 재연결 후 결과를 확인한다. README 값·이미지 해시·CSV도 함께 갱신한다.
4. Task 6 담당자에게 libdeca 버전, DS-TWR/PHY/주소/프레임 방식과 빌드 방법을 공유한다. 3001 포팅 및 혼합 쌍 실행은 아직 검증하지 않았다.

Lv3는 선택 항목이다. Task 2 전체를 완료했다고 말하기 전에 위 필수 1 m 보정을 마쳐야 한다.

## 겪은 문제와 해결/한계

- CN8에 연결하면 ST-LINK가 잡히지 않았다. CN10으로 옮긴 뒤 보드와 UART가 인식됐다.
- Qorvo 4개 static 선언과 잘못된 deca_compat 파일 선택은 기존 검토대로 수정했다. 올바른 파일은 `platform/deca_compat.c`다.
- 최초 드라이버 레지스터 접근은 successful initialise 이후로 제한했다. 이전 준비 상태는 raw SPI의 RCINIT로 확인하고 오류/시간 초과에서 중단한다.
- IRQ는 MAC callbacks/event queue가 준비된 뒤 hook=`dwt_isr`를 설정하고 켠다. main에서 poll하여 프로토콜을 처리하며 IRQ 안에서는 UART를 출력하지 않는다.
- N6 실행 후 AP1 재접속 실패가 발생했다. 전원 재연결 후 개발 모드 HotPlug로 복구했으며 영구 보안 설정은 바꾸지 않았다. 원인 확정 없이 보안/OTP unlock을 시도하지 않았다.
- 한 차례 수신 중단은 로그를 보존했고 원인은 미확정이다. 이후 재시작 및 고정 배치의 300회 캡처는 성공했지만 장시간 연속·전파 차단 후 복구까지 보증하지 않는다.
