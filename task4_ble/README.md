# Task 4: XIAO nRF52840 ↔ nRF52840 Dongle 무선 링크

목표: 드론(XIAO) → PC(동글) 텔레메트리 링크를 실제로 쓸 수 있는지 확인하고 사용법을 정리한다.
원본 과제: `UWB · STM32N6 · BLE 과제.pdf` (2026-11-05).

## 진행 현황 (2026-11-08)

| 레벨 | 상태 | 비고 |
| --- | --- | --- |
| Lv1 동작 확인 | 완료 | 스마트폰 수신, 동글 경유 PC 수신 모두 근거 있음 |
| Lv2 성능 측정 | 진행 중 | 시퀀스·타임스탬프, 처리량 완료. RTT, 거리별 손실률, 요구치 비교 남음 |
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
- [x] 2. 처리량: 최대 bytes/s (패킷 크기, connection interval 변경) — 아래 "결과 (Lv2 처리량)"
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
- 동글: `dongle_central/README.md` 참고. `dongle_central/build.sh`로 빌드하거나 `dongle_central/artifacts/central_uart_mtu247.hex`를 Programmer로 Write (RESET으로 부트로더 진입).
- 처리량 측정: XIAO에 `xiao_throughput/xiao_throughput.ino` 업로드(Lv1의 `xiao_peripheral`을 대체), `pip install pyserial`, 그다음 `python measure/throughput.py --xiao <XIAO 포트> --dongle <동글 포트>`.

## 패킷 형식

ASCII 한 줄 `<seq>,<millis>\n`. seq는 송신 순번, millis는 XIAO 시계. XIAO는 약 100 ms마다 한 줄 송신.

## 결과 (Lv1)

| 항목 | 결과 | 근거 |
| --- | --- | --- |
| 스마트폰 수신 | 스마트폰 nRF Connect 앱에서 `XIAO1` 연결, Nordic UART Service의 UART TX(Notify) 특성에 `1724,172140` 형태의 `seq,millis` 값 확인 | [`photos/t4_lv1_phone_nus.png`](photos/t4_lv1_phone_nus.png) (CCCD 줄의 `Disabled`는 구독 전에 읽은 값) |
| 동글 경유 PC 수신 | COM 포트 `VID_2FE3&PID_0100`, `seq,millis` 줄 수신 | `data/t4_lv1_dongle_rx_15s.csv` |
| 수신 품질 (15초) | 148행, seq 끊김 0, 줄 간격 50–150 ms | 같은 CSV에서 계산 |

## 결과 (Lv2 처리량)

측정 방법: XIAO(`xiao_throughput/`)가 쉬지 않고 최대 속도로 송신하고, PC(`measure/throughput.py`)가 동글(`central_uart`) USB 시리얼로 받은 bytes를 센다. 조건마다 30초씩 3회 반복(조건당 초 단위 값 90개). 패킷은 `seq,millis,xxxx…
` 전체 길이 기준이고, 처리량은 앱 계층 bytes/s(패딩과 개행 포함). 손실률은 XIAO가 보낸 패킷 수와 PC가 받은 고유 seq 수의 차이. XIAO는 연결 간격만 요청하고 MTU 247, PHY, 데이터 길이는 동글과 자동 협상된다(전 조건 `mtu=247`, 실제 연결 간격이 요청값과 일치).

원자료: `data/t4_tp_len<패킷>_ci<간격>ms_r<회차>.csv` (초당 수신 bytes), 요약 `data/t4_tp_summary.csv`(회차 열 포함). 실행: `python measure/throughput.py --xiao COM3 --dongle COM8 --run <회차>`.

표의 평균·표준편차·최대는 3회 × 30초의 초 단위 값 90개 기준이다. "회차별 평균"은 1/2/3회차 값이다.

| 패킷 | 연결 간격 | 측정 횟수 | 평균 (kB/s) | 표준편차 | 최대 | 회차별 평균 | 성공률 (손실률) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 20 B | 7.5 ms | 3회 × 30 s | 18.8 | 2.2 | 21.5 | 18.9 / 17.0 / 20.5 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 20 B | 15 ms | 3회 × 30 s | 18.8 | 1.6 | 22.8 | 18.1 / 19.5 / 18.8 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 20 B | 30 ms | 3회 × 30 s | 11.1 | 2.5 | 20.8 | 11.4 / 11.9 / 10.1 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 20 B | 50 ms | 3회 × 30 s | 7.0 | 1.4 | 12.0 | 6.5 / 7.0 / 7.3 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 20 B | 100 ms | 3회 × 30 s | 3.2 | 1.1 | 6.2 | 2.4 / 3.5 / 3.6 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 100 B | 7.5 ms | 3회 × 30 s | 67.9 | 6.8 | 77.0 | 69.4 / 61.8 / 72.5 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 100 B | 15 ms | 3회 × 30 s | 78.2 | 10.0 | 90.1 | 67.8 / 83.1 / 83.8 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 100 B | 30 ms | 3회 × 30 s | 65.2 | 13.7 | 87.8 | 54.4 / 67.8 / 73.3 | 99.96% (손실 평균 0.04%, 최대 0.12%) |
| 100 B | 50 ms | 3회 × 30 s | 54.5 | 16.6 | 83.0 | 40.0 / 60.4 / 63.0 | 99.33% (손실 평균 0.67%, 최대 0.84%) |
| 100 B | 100 ms | 3회 × 30 s | 42.4 | 14.7 | 72.1 | 33.1 / 44.9 / 49.2 | 97.12% (손실 평균 2.88%, 최대 4.84%) |
| 240 B | 7.5 ms | 3회 × 30 s | 89.9 | 3.9 | 98.2 | 88.7 / 91.6 / 89.5 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 240 B | 15 ms | 3회 × 30 s | 109.5 | 4.5 | 118.8 | 108.8 / 109.5 / 110.2 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 240 B | 30 ms | 3회 × 30 s | 91.3 | 18.3 | 122.9 | 72.5 / 98.4 / 103.0 | 100.00% (손실 평균 0.00%, 최대 0%) |
| 240 B | 50 ms | 3회 × 30 s | 76.0 | 21.4 | 118.8 | 53.1 / 85.9 / 89.1 | 99.98% (손실 평균 0.02%, 최대 0.07%) |
| 240 B | 100 ms | 3회 × 30 s | 60.4 | 21.1 | 100.0 | 45.3 / 65.8 / 70.2 | 98.50% (손실 평균 1.50%, 최대 2.07%) |

관찰:
- 가장 높은 값은 240 B·15 ms의 109.5 kB/s이고 회차별 값이 거의 같아(108.8 / 109.5 / 110.2) 재현성이 좋다.
- 7.5 ms가 15 ms보다 낮은 것(240 B: 89.9 vs 109.5)은 3회 모두 같게 나와 우연이 아니다. 원인은 확인하지 않았다.
- 연결 간격 30 ms 이상에서는 회차 간 차이가 크다(240 B·30 ms: 72.5 / 98.4 / 103.0). 1회차가 2·3회차보다 낮았다. 모든 회차를 같은 순서로 측정해서 첫 회차 효과인지 환경 영향인지는 구분하지 못했다.
- 손실은 연결 간격이 긴 조건에서 3회 모두 나왔다: 100 ms·100 B 평균 2.88%(최대 4.84%), 100 ms·240 B 1.50%, 50 ms·100 B 0.67%. 나머지 조건은 거의 0%다. 동글 쪽 버퍼 때문인지 링크 때문인지는 확인하지 않았다.
- 드론 요구치 3.2 kB/s(50 Hz × 64 B)와 비교하면 20 B·100 ms의 평균이 3.2 kB/s로 여유가 없고(1회차는 2.4), 나머지 조건의 평균은 모두 7.0 kB/s 이상이다.

한계: 쉬지 않고 최대 속도로 보낸 포화 측정이다. 50 Hz × 64 B 고정 속도에서 손실이 없는지는 별도 시험이 필요하다(Lv2 5번). XIAO와 동글 사이 거리와 배치, 주변 2.4 GHz 환경은 기록하지 않았다(배치 사진 필요).

동글 central을 처리량 측정에 맞게 고친 내용(MTU 247, 힙, 버퍼)은 `dongle_central/README.md` 참고.

## 진행 기록

- 2단계 완료 (2026-11-07): `xiao_blink` 업로드, USB 시리얼에 `LED on/off` 출력 확인.
  - 막힘: `Serial`이 링크되지 않음(`undefined reference to Serial`) → `#include <Adafruit_TinyUSB.h>` 추가로 해결. 삽질기록.md 참고.
- 3단계 완료 (2026-11-07): `xiao_peripheral` 업로드, 스마트폰 nRF Connect 앱에서 `XIAO1` 연결, NUS TX 알림으로 `seq,millis` 수신 확인.
- 4단계 빌드 (2026-11-07): `dongle_central/`의 `central_uart`(NCS v2.9.0)를 동글용으로 수정해 `nrf52840dongle/nrf52840`로 빌드, hex 생성. 올린 뒤 USB가 인식되지 않음.
- 4·5단계 완료 (2026-11-08): 원인은 `CONFIG_BT_SETTINGS=y`. `n`으로 바꾸고 플래시 → COM 포트 생성, XIAO1에서 `seq,millis` 수신. 격리 과정은 삽질기록.md Task 4 참고.
- Lv2 처리량 완료 (2026-11-08): `xiao_throughput` 스케치와 `measure/throughput.py`로 패킷 3종 × 연결 간격 5종, 조건당 30초 × 3회 측정. 막힘 3건(연결 직후 요청 충돌, MTU 65 제한, 동글 힙·버퍼 부족)은 삽질기록.md Task 4 참고.
