# Task 1: DWM3001CDK UWB 성능 평가

## Lv1 체크리스트 (동작 확인)

- [ ] 보드 2개 J-Link 포트(J9) 연결, Qorvo DW3 QM33 SDK 다운로드
- [ ] SDK `Binaries` 폴더의 CLI 펌웨어 플래싱 (절차: SDK `Documentation/Developer Manual` 4.4.2절)
- [ ] 한쪽 initiator(`initf`), 다른 쪽 responder로 TWR 레인징, PC 시리얼에 거리 출력 확인
- [ ] 로깅 스크립트로 시리얼 → CSV (timestamp, distance, RSSI 또는 first-path power, 성공 여부). 위치: `../common/logger/`
- [ ] 안테나 딜레이 보정 (아래 기록)
- [ ] 배치 사진 `photos/`

통과 기준 (PDF): 거리가 PC에 출력되고 CSV로 저장됨. 보정값이 README에 기록됨.

## 안테나 딜레이 보정 기록

| 항목 | 값 |
| --- | --- |
| 측정 거리 | 1.00 m |
| 측정 횟수 | 300 |
| 보정 전 평균 | |
| 보정값 | |
| 보정 후 평균 | |
| 측정 날짜·장소 | |

## Lv1 데이터

| 파일 | 내용 | 상태 |
| --- | --- | --- |
| `data/t1_calib_1m.csv` | 보정용 300회 raw | ☐ |
| `data/t1_logger_test.csv` | 로깅 스크립트 시험 출력 | ☐ |
| `photos/` | 보드 배치 사진 | ☐ |

## 빌드·플래싱

TODO: SDK 버전, 플래싱 명령, 펌웨어 종류 기록.

## 참고

- 포럼 보고: SDK 1.1.0, 1.1.1 CLI는 세션 파라미터만 출력하고 레인징이 안 됨. 1.0.2에서 됐다는 보고가 있음. 막히면 SDK 버전을 바꿔 보고 `../삽질기록.md`에 남긴다.
- 1.1.1 예제에서 DWM3001CDK HardFault 보고 있음.
