# Task 4: XIAO nRF52840 ↔ nRF52840 Dongle 무선 링크

목표: 드론(XIAO) → PC(동글) 텔레메트리 링크를 실제로 쓸 수 있는지 확인하고 사용법을 정리한다.
원본 과제: `UWB · STM32N6 · BLE 과제.pdf` (2026-11-05).

## 진행 현황 (2026-11-09)

| 레벨 | 상태 | 비고 |
| --- | --- | --- |
| Lv1 동작 확인 | 완료 | 스마트폰 수신, 동글 경유 PC 수신 모두 근거 있음 |
| Lv2 성능 측정 | 진행 중 | 시퀀스·타임스탬프, 처리량, RTT, 요구치 비교 완료. 거리별 RSSI·손실률 남음 |
| Lv3 심화 | 시작 전 | Lv2 완료 후 |

## 결과 요약

표·그래프·관찰·한계는 각 결과 문서에 있다. 측정 조건(날짜, 명령, 펌웨어 버전)은 [`MEASUREMENTS.md`](MEASUREMENTS.md).

| 레벨 | 작업 | 결론 한 줄 | 결과 문서 |
| --- | --- | --- | --- |
| Lv1 | XIAO → 동글 → PC 수신 | `seq,millis`가 끊김 없이 도착 (15초, 148줄) | [`results/lv1.md`](results/lv1.md) |
| Lv2 | 처리량 | 20~240 B, 간격 7.5~100 ms 전 조건에서 3.2 kB/s 이상, 최고 약 109.5 kB/s (240 B·15 ms). 간격이 긴 조건에서 손실 최대 약 5% | [`results/lv2_throughput.md`](results/lv2_throughput.md) |
| Lv2 | 요구치 비교 (50 Hz × 64 B) | 간격 7.5 / 30 / 100 ms 모두 손실 0 (9회, 27,013 패킷) | [`results/lv2_rate_check.md`](results/lv2_rate_check.md) |
| Lv2 | RTT | 평균 26 ms(7.5 ms 간격) ~ 180 ms(100 ms 간격), 1,000회 모두 응답 | [`results/lv2_rtt.md`](results/lv2_rtt.md) |
| Lv2 | 거리별 RSSI·손실률 | 미측정 | |

## 폴더 구조

```
task4_ble/
  README.md              이 문서: 현황, 체크리스트, 개발환경, 빌드·플래싱
  MEASUREMENTS.md        측정 대장 (언제, 무슨 명령, 어떤 펌웨어, 어떤 환경)
  results/               결과 문서(표·그래프·관찰·한계·결론). 표는 analyze.py가 생성
    figures/             그래프 PNG
  data/                  원자료 CSV (조건마다 파일 하나, 파일명에 조건 표기)
    lv1/pc_rx/
    lv2/throughput/      초당 수신 bytes (조건×회차) + summary.csv
    lv2/rate_check/      고정 속도(50 Hz × 64 B) 결과
    lv2/rtt/             번호별 RTT
  photos/                스마트폰 수신 스크린샷, (측정 배치 사진은 추가 예정)
  measure/               throughput.py, rtt.py(측정), analyze.py(원자료 → 표·그래프)
  xiao_blink/ xiao_peripheral/ xiao_throughput/    XIAO 스케치 (Lv1 / Lv1 / Lv2)
  dongle_central/        동글 central 펌웨어 (빌드 방법, hex)
```

표와 그래프 다시 만들기: `python measure/analyze.py` (matplotlib 필요). `results/*.md`의 `BEGIN:table`~`END:table` 사이와 `results/figures/`가 원자료에서 새로 만들어진다.

## Lv1 체크리스트

- [x] 1. 개발환경 선택·설치 (선택과 이유는 아래 "개발환경 기록")
- [x] 2. XIAO LED 깜빡이기, USB 시리얼 출력 (`xiao_blink/`)
- [x] 3. XIAO를 BLE 페리페럴로 설정, NUS로 카운터 송신 (`xiao_peripheral/`), 스마트폰 nRF Connect 앱 수신 확인
- [x] 4. 동글에 central 펌웨어 올리기 (nRF Connect for Desktop Programmer, `dongle_central/`)
- [x] 5. XIAO → 동글 → PC 시리얼 수신 확인 (`data/lv1/pc_rx/t4_lv1_dongle_rx_15s.csv`)

통과 기준: 스마트폰 수신, 동글 경유 PC 수신.

## Lv2 체크리스트 (성능 측정)

- [x] 1. 패킷에 시퀀스 번호와 타임스탬프 (`seq,millis`, 손실·지연 계산용)
- [x] 2. 처리량: 최대 bytes/s (패킷 크기, connection interval 변경) — [`results/lv2_throughput.md`](results/lv2_throughput.md)
- [x] 3. RTT: PC → 동글 → XIAO → 에코 → PC, 1,000회 평균·최대 — [`results/lv2_rtt.md`](results/lv2_rtt.md)
- [ ] 4. 거리별 RSSI와 패킷 손실률: 1 / 5 / 10 / 20 m, 각 1분
- [x] 5. 드론 요구치 비교 (50 Hz × 64 B = 3.2 kB/s 손실 없이 가능한지) — [`results/lv2_rate_check.md`](results/lv2_rate_check.md)

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
- 처리량·요구치·RTT 측정: XIAO에 `xiao_throughput/xiao_throughput.ino` 업로드(Lv1의 `xiao_peripheral`을 대체), `pip install pyserial`, 그다음 `python measure/throughput.py --xiao <XIAO 포트> --dongle <동글 포트>`(처리량, `--rate 50 --lens 64`로 요구치) 또는 `python measure/rtt.py --xiao <XIAO 포트> --dongle <동글 포트>`(RTT). 결과는 `data/lv2/` 아래 해당 폴더에 저장된다.

## 패킷 형식

ASCII 한 줄 `<seq>,<millis>\n`. seq는 송신 순번, millis는 XIAO 시계. Lv1의 XIAO는 약 100 ms마다 한 줄 송신. Lv2 측정 스케치는 같은 줄 뒤에 `,xxxx…`로 길이를 채운다.

## 진행 기록

- 2단계 완료 (2026-11-07): `xiao_blink` 업로드, USB 시리얼에 `LED on/off` 출력 확인.
  - 막힘: `Serial`이 링크되지 않음(`undefined reference to Serial`) → `#include <Adafruit_TinyUSB.h>` 추가로 해결. 삽질기록.md 참고.
- 3단계 완료 (2026-11-07): `xiao_peripheral` 업로드, 스마트폰 nRF Connect 앱에서 `XIAO1` 연결, NUS TX 알림으로 `seq,millis` 수신 확인.
- 4단계 빌드 (2026-11-07): `dongle_central/`의 `central_uart`(NCS v2.9.0)를 동글용으로 수정해 `nrf52840dongle/nrf52840`로 빌드, hex 생성. 올린 뒤 USB가 인식되지 않음.
- 4·5단계 완료 (2026-11-08): 원인은 `CONFIG_BT_SETTINGS=y`. `n`으로 바꾸고 플래시 → COM 포트 생성, XIAO1에서 `seq,millis` 수신. 격리 과정은 삽질기록.md Task 4 참고.
- Lv2 처리량 완료 (2026-11-08): `xiao_throughput` 스케치와 `measure/throughput.py`로 패킷 3종 × 연결 간격 5종, 조건당 30초 × 3회 측정. 막힘 3건(연결 직후 요청 충돌, MTU 65 제한, 동글 힙·버퍼 부족)은 삽질기록.md Task 4 참고.
- Lv2 요구치 비교 완료 (2026-11-09): 스케치에 송신 속도 지정 추가, 64 B × 50패킷/초를 연결 간격 3종 × 60초 × 3회 측정, 손실 0.
- Lv2 RTT 완료 (2026-11-09): XIAO 스케치에 에코 추가, `measure/rtt.py`로 연결 간격 3종 × 1,000회 측정, 응답률 100%, 평균 26~180 ms.
- 데이터 정리 (2026-11-09): `data/`를 레벨/작업 폴더로 나누고, 결과 표·그래프는 `results/`로 옮겨 `measure/analyze.py`가 원자료에서 생성하도록 했다. 측정 대장 `MEASUREMENTS.md` 추가.
