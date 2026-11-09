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
| XIAO와 동글 사이 거리·배치 | 처리량·요구치·RTT 측정: **미기록**. 거리별 측정(2026-11-09 20:08~20:38): 실내 복도, 동글을 기준으로 XIAO를 1 / 5 / 10 / 20 m 띄움, 두 보드 높이 동일. 줄자 기준점(안테나 위치)과 동글을 PC에 꽂은 방식은 미기록 |
| 주변 2.4 GHz 환경 | **미기록** |
| 배치 사진 | **없음** (제외하기로 함) |

## 데이터별 기록

| 작업 | 측정일 | 조건 | 실행 명령 | 동글 펌웨어 | XIAO 스케치 | 원자료 | 결과 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Lv1 PC 수신 | 2026-11-08 | 15초, XIAO가 약 100 ms마다 한 줄 송신 | 저장한 스크립트 없음 (PowerShell `SerialPort`로 동글 COM을 15초 읽어 줄 단위 저장) | `central_uart_bt_settings_off.hex` (커밋 `b6cb9da`, 이후 파일은 삭제됨, git 이력에서 복구 가능) | `xiao_peripheral` (커밋 `b2c6a98`) | [`data/lv1/pc_rx/`](data/lv1/pc_rx/) | [`results/lv1.md`](results/lv1.md) |
| Lv2 처리량 | 2026-11-08 20:29~20:57 | 패킷 20 / 100 / 240 B × 연결 간격 7.5 / 15 / 30 / 50 / 100 ms, 30초 × 3회 | `python measure/throughput.py --xiao COM3 --dongle COM8 --lens 20,100,240 --cis 7.5,15,30,50,100 --duration 30 --run <1~3>` | `central_uart_mtu247.hex` (커밋 `187d8e8`) | `xiao_throughput` (커밋 `187d8e8`, 최대 속도 송신만) | [`data/lv2/throughput/`](data/lv2/throughput/) | [`results/lv2_throughput.md`](results/lv2_throughput.md) |
| Lv2 요구치 비교 | 2026-11-09 18:21~18:30 | 64 B × 50패킷/초, 연결 간격 7.5 / 30 / 100 ms, 60초 × 3회 | `python measure/throughput.py --xiao COM3 --dongle COM8 --lens 64 --cis 7.5,30,100 --rate 50 --duration 60 --run <1~3>` | `central_uart_mtu247.hex` | `xiao_throughput` (커밋 `71ecc3f`, 송신 속도 지정 추가) | [`data/lv2/rate_check/`](data/lv2/rate_check/) | [`results/lv2_rate_check.md`](results/lv2_rate_check.md) |
| Lv2 RTT | 2026-11-09 18:34~18:38 | 64 B 핑 1,000회, 연결 간격 7.5 / 30 / 100 ms | `python measure/rtt.py --xiao COM3 --dongle COM8 --cis 7.5,30,100 --count 1000` | `central_uart_mtu247.hex` | `xiao_throughput` (커밋 `d111c7e`, 에코 추가) | [`data/lv2/rtt/`](data/lv2/rtt/) | [`results/lv2_rtt.md`](results/lv2_rtt.md) |
| Lv2 거리별 RSSI·손실률 | 2026-11-09 20:08~20:38 | 실내 복도, 64 B × 50 Hz 자동 송신, 연결 간격 7.5 ms 요청, 1 / 5 / 10 / 20 m 각 60초 (순서 1 → 10 → 20 → 5 → 20 재측정 3회), XIAO는 보조배터리 | `python measure/distance.py --dongle COM8 --dists <거리>` (거리마다 따로 실행) | `central_uart_rssi.hex` (커밋 `e93d8bb`. RSSI 출력, `BT_CTLR_CONN_RSSI=y`, 알림 구독 재시도) | `xiao_throughput` (커밋 `e93d8bb`. 자동 송신 모드, 상태 LED) | [`data/lv2/distance/`](data/lv2/distance/) | [`results/lv2_distance.md`](results/lv2_distance.md) |

메모:
- 처리량 1회차는 `--run` 옵션을 만들기 전 버전의 스크립트로 쟀고, 파일 이름만 `_r1`로 바꿔 2·3회차와 맞췄다. 측정 방식은 같다.
- 처리량은 `xiao_throughput`의 이전 커밋 버전으로 쟀다. 이후 송신 속도 지정과 에코가 추가됐고, 최대 속도 송신 경로는 바꾸지 않았지만 처리량을 다시 재지는 않았다.
- 2026-11-09에 `data/`를 평평한 구조에서 레벨/작업 폴더로 옮기고, 요약 파일 이름을 `summary.csv`로 통일했다(내용은 그대로).
- 거리별 측정: 20 m는 같은 이름의 CSV를 덮어쓰며 4번 쟀다(20:23 첫 측정 2133 / 3000, 이후 20:35, 20:36, 20:38). 첫 3번의 초당 CSV는 남아 있지 않고 `summary.csv` 행만 남았다. 연결 간격은 이 측정 중에 STAT로 확인하지 않았다(같은 펌웨어로 따로 확인했을 때 `ci_units=6`, 7.5 ms).
- 거리별 측정 전에 동글 펌웨어를 여러 번 고쳤다(HCI Read RSSI 거부 수정, 알림 구독 간헐 실패 대비 재시도·진단 줄). 이 변경과 XIAO 스케치 변경은 커밋 `e93d8bb`에 들어 있다.
- 거리별 측정의 시계열(`t4_dist_<거리>m.csv`)에는 파일 이름에 회차가 없다. 같은 거리를 다시 재면 이전 CSV를 덮어쓴다.
- 재현: 펌웨어 빌드·플래시는 [`dongle_central/README.md`](dongle_central/README.md), XIAO 업로드와 측정 실행은 [`README.md`](README.md)의 "빌드·플래싱" 참고.
