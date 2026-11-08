# Task 4: XIAO nRF52840 ↔ nRF52840 Dongle 무선 링크

목표: 드론(XIAO) → PC(동글) 텔레메트리 링크를 실제로 쓸 수 있는지 확인하고 사용법을 정리한다.
원본 과제: `UWB · STM32N6 · BLE 과제.pdf` (2026-11-05).

## 진행 현황 (2026-11-08)

| 레벨 | 상태 | 비고 |
| --- | --- | --- |
| Lv1 동작 확인 | 완료 | 스마트폰 수신, 동글 경유 PC 수신 모두 근거 있음 |
| Lv2 성능 측정 | 시작 전 | 패킷 형식(`seq,millis`)만 미리 반영 |
| Lv3 심화 | 시작 전 | Lv2 완료 후 |

## Lv1 체크리스트

- [x] 1. 개발환경 선택·설치 (선택과 이유는 아래 "개발환경 기록")
- [x] 2. XIAO LED 깜빡이기, USB 시리얼 출력 (`xiao_blink/`)
- [x] 3. XIAO를 BLE 페리페럴로 설정, NUS로 카운터 송신 (`xiao_peripheral/`), 스마트폰 nRF Connect 앱 수신 확인
- [x] 4. 동글에 central 펌웨어 올리기 (nRF Connect for Desktop Programmer, `dongle_central/`)
- [x] 5. XIAO → 동글 → PC 시리얼 수신 확인 (`data/t4_lv1_dongle_rx_15s.csv`)

통과 기준: 스마트폰 수신, 동글 경유 PC 수신.

## Lv2 체크리스트 (성능 측정)

- [x] 1. 패킷에 시퀀스 번호와 타임스탬프 (`seq,millis`, 손실·지연 계산용)
- [ ] 2. 처리량: 최대 bytes/s (패킷 크기, connection interval 변경)
- [ ] 3. RTT: PC → 동글 → XIAO → 에코 → PC, 1,000회 평균·최대
- [ ] 4. 거리별 RSSI와 패킷 손실률: 1 / 5 / 10 / 20 m, 각 1분
- [ ] 5. 드론 요구치 비교 (50 Hz × 64 B = 3.2 kB/s 손실 없이 가능한지)

Lv2 공통 결과물: 조건별 raw CSV(파일명에 조건 표기, 예 `t4_dist_5m.csv`), 결과 표(조건·측정 횟수·평균·표준편차·최대·성공률), 측정 배치 사진, 결론 한두 줄.

## Lv3 체크리스트 (심화)

- [ ] 1. 엔드투엔드: STM32 UART → XIAO → BLE → 동글 → PC (Task 2/3 데이터)
- [ ] 2. 끊김 처리: 자동 재연결과 걸리는 시간
- [ ] 3. 드론 프레임 장착 + 모터 구동 중 (프로펠러 제거) 손실률 재측정
- [ ] 4. (참고) nRF Sniffer로 BLE 패킷 캡처

## 개발환경 기록

| 항목 | 값 | 이유 |
| --- | --- | --- |
| XIAO 환경 | Arduino IDE 2.x (명령줄 빌드는 arduino-cli) | 과제가 제시한 두 선택지(Arduino IDE 또는 nRF Connect SDK) 중 하나. Bluefruit 예제로 NUS 페리페럴을 짧은 코드로 구현 가능 |
| 보드 패키지 | Seeed nRF52 Boards (비 mbed) 1.1.13 | BLE는 Bluefruit 계열이 있는 이 패키지 |
| BLE 라이브러리 | Bluefruit (`bluefruit.h`) | |
| 보드 모델 | XIAO nRF52840 Sense (FQBN `Seeeduino:nrf52:xiaonRF52840Sense`) | arduino-cli board list로 확인 |
| 동글 펌웨어 | Nordic `central_uart` (nRF Connect SDK v2.9.0, Toolchain Manager, `C:\ncs`) | 동글용 central을 새로 쓰지 않고 Nordic 공식 샘플을 동글용으로 수정 |
| 동글 플래싱 | nRF Connect for Desktop > Programmer | 과제 지침 |

## 빌드·플래싱

- 포트는 부트로더 진입·업로드 때마다 바뀔 수 있다 (COM3 ↔ COM4 등). 업로드 전에 장치 관리자에서 확인.
- XIAO: Arduino IDE에서 `xiao_peripheral/xiao_peripheral.ino`를 열고 Board = Seeed XIAO nRF52840 (Sense), 포트 선택 후 Upload.
- 동글: `dongle_central/README.md` 참고. `dongle_central/build.sh`로 빌드하거나 `dongle_central/artifacts/central_uart_bt_settings_off.hex`를 Programmer로 Write (RESET으로 부트로더 진입).

## 패킷 형식

ASCII 한 줄 `<seq>,<millis>\n`. seq는 송신 순번, millis는 XIAO 시계. XIAO는 약 100 ms마다 한 줄 송신.

## 결과 (Lv1)

| 항목 | 결과 | 근거 |
| --- | --- | --- |
| 스마트폰 수신 | 스마트폰 nRF Connect 앱에서 `XIAO1` 연결, Nordic UART Service의 UART TX(Notify) 특성에 `1724,172140` 형태의 `seq,millis` 값 확인 | [`photos/t4_lv1_phone_nus.png`](photos/t4_lv1_phone_nus.png) (CCCD 줄의 `Disabled`는 구독 전에 읽은 값) |
| 동글 경유 PC 수신 | COM 포트 `VID_2FE3&PID_0100`, `seq,millis` 줄 수신 | `data/t4_lv1_dongle_rx_15s.csv` |
| 수신 품질 (15초) | 148행, seq 끊김 0, 줄 간격 50–150 ms | 같은 CSV에서 계산 |

## 진행 기록

- 2단계 완료 (2026-11-07): `xiao_blink` 업로드, USB 시리얼에 `LED on/off` 출력 확인.
  - 막힘: `Serial`이 링크되지 않음(`undefined reference to Serial`) → `#include <Adafruit_TinyUSB.h>` 추가로 해결. 삽질기록.md 참고.
- 3단계 완료 (2026-11-07): `xiao_peripheral` 업로드, 스마트폰 nRF Connect 앱에서 `XIAO1` 연결, NUS TX 알림으로 `seq,millis` 수신 확인.
- 4단계 빌드 (2026-11-07): `dongle_central/`의 `central_uart`(NCS v2.9.0)를 동글용으로 수정해 `nrf52840dongle/nrf52840`로 빌드, hex 생성. 올린 뒤 USB가 인식되지 않음.
- 4·5단계 완료 (2026-11-08): 원인은 `CONFIG_BT_SETTINGS=y`. `n`으로 바꾸고 플래시 → COM 포트 생성, XIAO1에서 `seq,millis` 수신. 격리 과정은 삽질기록.md Task 4 참고.
