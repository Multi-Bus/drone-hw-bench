# Task 4: XIAO nRF52840 ↔ nRF52840 Dongle 무선 링크

## Lv1 체크리스트

- [x] 1. 개발환경 선택·설치 (Arduino IDE + Seeed nRF52 Boards; nRF Connect for Desktop은 설치 중)
- [x] 2. XIAO LED 깜빡이기, USB 시리얼 출력
- [x] 3. BLE 페리페럴(NUS)로 카운터 송신, 스마트폰 nRF Connect 앱 수신 확인
- [x] 4. 동글에 central 펌웨어 올리기 (nRF Connect for Desktop Programmer)
- [x] 5. XIAO → 동글 → PC 시리얼 수신 확인 (`data/t4_lv1_dongle_rx_15s.csv`)
- [x] 6. 패킷에 시퀀스 번호와 타임스탬프 (`seq,millis`)

통과 기준: 스마트폰 수신, 동글 경유 PC 수신, 시퀀스·타임스탬프가 든 패킷 출력.

## 개발환경 기록

| 항목 | 값 | 이유 |
| --- | --- | --- |
| 환경 | Arduino IDE 2.x | |
| 보드 패키지 | Seeed nRF52 Boards (비 mbed) 1.1.13 | BLE는 Bluefruit 계열이 있는 이 패키지 |
| BLE 라이브러리 | Bluefruit (`bluefruit.h`) | |
| 보드 모델 | XIAO nRF52840 Sense (FQBN `Seeeduino:nrf52:xiaonRF52840Sense`) | arduino-cli board list로 확인 |

## 빌드·플래싱

- 포트는 부트로더 진입·업로드 때 COM3 ↔ COM4로 바뀔 수 있다. 업로드 후 장치 관리자에서 확인.
- XIAO: Arduino IDE에서 `xiao_peripheral/xiao_peripheral.ino`를 열고 Board = Seeed XIAO nRF52840, 포트 선택 후 Upload.
- 동글: `dongle_central/`의 hex를 nRF Connect for Desktop > Programmer로 Write (RESET으로 부트로더 진입).

## 패킷 형식

ASCII 한 줄 `<seq>,<millis>\n`. seq는 송신 순번, millis는 XIAO 시계.

## 결과

(Lv1 완료 후 스마트폰 수신 스크린샷, PC 수신 출력 붙이기)

## 진행 기록

- 2단계 완료 (2026-11-07): `xiao_blink` 업로드, USB 시리얼에 `LED on/off` 출력 확인.
  - 막힘: `Serial`이 링크되지 않음(`undefined reference to Serial`) → `#include <Adafruit_TinyUSB.h>` 추가로 해결. 삽질기록.md 참고.
- 3단계 완료 (2026-11-07): `xiao_peripheral` 업로드, 스마트폰 nRF Connect 앱에서 `XIAO1` 연결, NUS TX 알림으로 `seq,millis` 수신 확인. (스크린샷은 이 폴더에 추가)
- 4단계 빌드 완료 (2026-11-07): `dongle_central/`의 `central_uart`(NCS v2.9.0)를 동글용으로 수정해 `nrf52840dongle/nrf52840`로 빌드, hex 생성.
- 4~6단계 완료 (2026-11-08): 동글 USB 미인식 원인은 `CONFIG_BT_SETTINGS=y`. `n`으로 바꾸고 플래시 → COM 포트 생성, XIAO1에서 `seq,millis` 수신 (15초, 148행, 끊김 0). 과정은 삽질기록.md Task 4 참고.
  - 남은 것: 스마트폰 수신 스크린샷, 결과 표 (거리·처리량 등 Lv2).
