# common

Task 간 공용 코드.

## logger/

Task 1 로깅 스크립트. Task 2, 4, 5가 가져다 쓴다.

- 기능: 시리얼 → CSV. 필드 timestamp, distance, RSSI 또는 first-path power, 성공 여부.
- 파일 이름에 조건 표기 (예: `t1_dist_5m_los.csv`).
- 의존: Python 3, `pyserial`.

TODO: 스크립트 작성 후 사용법 기록.
