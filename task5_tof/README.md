# Task 5: VL53L1X 레이저(ToF) 거리 센서

## 구성

- 호스트: XIAO nRF52840 Sense + Arduino 라이브러리 (빠른 확인용). 드론용 ST ULD 포팅은 나중. 이번 선택 이유: 하루 분량, XIAO 환경은 Task 4에서 이미 준비.
- 센서: Pololu VL53L1X 캐리어 기준. 제조사 확인 필요.
- 배선 (예시, 실제 라벨로 확인):

| VL53L1X | XIAO |
| --- | --- |
| VIN | 3V3 |
| GND | GND |
| SDA | D4 |
| SCL | D5 |
| XSHUT | D0 (예시) |
| GPIO1 | D1 (선택, 예시) |

- XSHUT: 5 V 내성 없음, 레벨 시프트 없음. LOW 또는 입력(하이 임피던스)만 사용. HIGH 구동 금지.
- 기본 I2C 주소 0x29. 주소 변경은 전원 재인가 시 사라짐 → 부팅마다 재설정.
- 빵판 사용 (센서 2개가 전원·접지·I2C 공유).

## Lv1 체크리스트 (동작 확인)

- [ ] I2C 스캔 스케치로 0x29 확인
- [ ] 거리값과 `range_status` UART 출력 (`range_status` 비정상 값도 버리지 않음)
- [ ] timing budget 20 ms, **Short 모드**에서 출력 50 Hz 확인 (Pololu 라이브러리: 최소 timing budget Short 20 ms, Medium/Long 33 ms. Lv2의 Long 모드와 다르므로 기록)
- [ ] 센서 2개 동시 사용: XSHUT 순차 기동, 한쪽 `setAddress(0x30)`, 두 값 동시 출력
- [ ] 배치 사진 `photos/`

통과 기준 (PDF): 0x29 확인, 거리·status 출력, 50 Hz 확인, 센서 2개 동시 출력.

## Lv1 데이터

| 파일 | 내용 | 상태 |
| --- | --- | --- |
| `data/t5_i2c_scan.txt` | I2C 스캔 결과 (0x29) | ☐ |
| `data/t5_range_20ms.csv` | 50 Hz 확인 raw (timestamp, range_mm, range_status) | ☐ |
| `data/t5_dual_sensor.csv` | 센서 2개 동시 출력 raw | ☐ |
| `photos/` | 배선 사진 | ☐ |

## 빌드·플래싱

TODO: Arduino IDE 보드 설정(Seeed nRF52 Boards, XIAO nRF52840 Sense), 라이브러리 버전, 스케치 경로 기록.
