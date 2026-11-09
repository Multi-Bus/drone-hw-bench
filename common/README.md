# common

Task 간 공용 코드.

## logger/

Task 1 로깅 스크립트. Task 2, 4, 5가 가져다 쓴다.

- 기능: 시리얼 → CSV. 필드 timestamp, distance, RSSI 또는 first-path power, 성공 여부.
- 파일 이름에 조건 표기 (예: `t1_dist_5m_los.csv`).
- 의존: Python 3, `pyserial`.

- 스크립트: `logger/log_ranging.py`. 기록만 하고 보드에 명령을 보내지 않는다.
- 사용: `python log_ranging.py <COM포트> <출력.csv> -n 300`. 같은 COM 포트를 다른 터미널이 열고 있으면 안 된다. 같은 이름으로 저장하면 덮어쓴다.
- CSV 필드: `timestamp, sequence_number, mac_address, status, distance_cm, success`. DWM3001CDK CLI 기본 출력에는 RSSI와 first-path power가 없어서 저장하지 않는다. `timestamp`는 PC가 받은 시각이다.
- Task 1(DWM3001CDK CLI 출력)용이다. Task 2, 4, 5가 쓰려면 출력 형식에 맞게 고쳐야 한다.
