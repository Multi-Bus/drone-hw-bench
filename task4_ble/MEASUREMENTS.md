# Task 4 측정 대장

어떤 데이터를 언제, 무슨 명령으로, 어떤 펌웨어로 쟀는지 적는다. 다음 사람이 같은 측정을 다시 할 수 있게 하려는 기록이다.
결과 표와 그래프는 [`results/`](results/)에, 원자료는 [`data/`](data/)에 있다. 표와 그래프는 `python measure/analyze.py`로 원자료에서 다시 만든다.

## 공통 환경

| 항목 | 값 |
| --- | --- |
| PC | Windows 11 Home, Python 3.12, pyserial 3.5 |
| XIAO | XIAO nRF52840 Sense, Seeed nRF52 Boards 1.1.13 (Arduino, Bluefruit) |
| 동글 | nRF52840 Dongle, nRF Connect SDK v2.9.0 `central_uart` 수정본 (`dongle_central/`) |
| 포트 | XIAO = USB 시리얼(제어), 동글 = USB 시리얼(수신). 번호는 꽂을 때마다 바뀔 수 있다 (측정 때는 COM3, COM8) |
| XIAO와 동글 사이 거리·배치 | **미기록** (거리별 측정을 끝낸 뒤 알려 주면 채움) |
| 주변 2.4 GHz 환경 | **미기록** |
| 배치 사진 | **없음** |

## 데이터별 기록

| 작업 | 측정일 | 조건 | 실행 명령 | 동글 펌웨어 | XIAO 스케치 | 원자료 | 결과 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Lv1 PC 수신 | 2026-11-08 | 15초, XIAO가 약 100 ms마다 한 줄 송신 | 저장한 스크립트 없음 (PowerShell `SerialPort`로 동글 COM을 15초 읽어 줄 단위 저장) | `central_uart_bt_settings_off.hex` (커밋 `b6cb9da`, 이후 파일은 삭제됨, git 이력에서 복구 가능) | `xiao_peripheral` (커밋 `b2c6a98`) | [`data/lv1/pc_rx/`](data/lv1/pc_rx/) | [`results/lv1.md`](results/lv1.md) |
| Lv2 처리량 | 2026-11-08 20:29~20:57 | 패킷 20 / 100 / 240 B × 연결 간격 7.5 / 15 / 30 / 50 / 100 ms, 30초 × 3회 | `python measure/throughput.py --xiao COM3 --dongle COM8 --lens 20,100,240 --cis 7.5,15,30,50,100 --duration 30 --run <1~3>` | `central_uart_mtu247.hex` (커밋 `187d8e8`) | `xiao_throughput` (커밋 `187d8e8`, 최대 속도 송신만) | [`data/lv2/throughput/`](data/lv2/throughput/) | [`results/lv2_throughput.md`](results/lv2_throughput.md) |
| Lv2 요구치 비교 | 2026-11-09 18:21~18:30 | 64 B × 50패킷/초, 연결 간격 7.5 / 30 / 100 ms, 60초 × 3회 | `python measure/throughput.py --xiao COM3 --dongle COM8 --lens 64 --cis 7.5,30,100 --rate 50 --duration 60 --run <1~3>` | `central_uart_mtu247.hex` | `xiao_throughput` (커밋 `71ecc3f`, 송신 속도 지정 추가) | [`data/lv2/rate_check/`](data/lv2/rate_check/) | [`results/lv2_rate_check.md`](results/lv2_rate_check.md) |
| Lv2 RTT | 2026-11-09 18:34~18:38 | 64 B 핑 1,000회, 연결 간격 7.5 / 30 / 100 ms | `python measure/rtt.py --xiao COM3 --dongle COM8 --cis 7.5,30,100 --count 1000` | `central_uart_mtu247.hex` | `xiao_throughput` (커밋 `d111c7e`, 에코 추가) | [`data/lv2/rtt/`](data/lv2/rtt/) | [`results/lv2_rtt.md`](results/lv2_rtt.md) |
| Lv2 거리별 RSSI·손실률 | 미측정 | 1 / 5 / 10 / 20 m, 각 1분 | | | | | |

메모:
- 처리량 1회차는 `--run` 옵션을 만들기 전 버전의 스크립트로 쟀고, 파일 이름만 `_r1`로 바꿔 2·3회차와 맞췄다. 측정 방식은 같다.
- 처리량은 `xiao_throughput`의 이전 커밋 버전으로 쟀다. 이후 송신 속도 지정과 에코가 추가됐고, 최대 속도 송신 경로는 바꾸지 않았지만 처리량을 다시 재지는 않았다.
- 2026-11-09에 `data/`를 평평한 구조에서 레벨/작업 폴더로 옮기고, 요약 파일 이름을 `summary.csv`로 통일했다(내용은 그대로).
- 재현: 펌웨어 빌드·플래시는 [`dongle_central/README.md`](dongle_central/README.md), XIAO 업로드와 측정 실행은 [`README.md`](README.md)의 "빌드·플래싱" 참고.
