# Task 3: STM32N6 실시간 성능

## 보드

- NUCLEO-N657X0-Q (7개 보유). 내부 플래시 없음. 개발 모드 부팅 (BOOT0=0, BOOT1=1), 실행 후 전원 재인가.
- 실행 전 전원 재인가 필수. 업로드 후 실행이 안 되면 점퍼 위치와 서명 여부 확인.
- 서명 도구: `STM32_SigningTool_CLI` (CubeProgrammer 2.18.0 이상, Zephyr 문서 기준. ST 예제에서 같은지 확인 필요).

## Lv1 체크리스트 (동작 확인)

- [ ] 점퍼 설정 후 전원 LED, LD5 깜빡임, B1 버튼 확인
- [ ] STM32CubeN6 예제에서 LED와 UART 출력 올리기 (예제 이름·경로 TODO)
- [ ] `SystemCoreClock` 출력, 800 MHz 근처 확인
- [ ] CYCCNT 검증: `HAL_Delay(100)` 측정값 ≈ 클럭 × 0.1 s
- [ ] `measure/`에 공통 측정 코드 (시작·종료·통계·일괄 출력). 측정 중 UART 출력 금지, 배열에 저장 후 출력
- [ ] 배치 사진 `photos/`

통과 기준 (PDF): 클럭 값 확인, CYCCNT 검증 (100 ms 측정값), 측정 코드가 repo에 올라감.

## DWT 측정 방법 (모든 Lv 공통)

- 실행 시간: 구간 앞뒤 CYCCNT 차이 (unsigned 뺄셈)
- 800 MHz에서 1 cycle = 1.25 ns, 약 5.4 s마다 오버플로
- 결과는 평균 / 최소 / 최대. 실시간 판단은 최대값 기준.

## Lv1 데이터

| 파일 | 내용 | 상태 |
| --- | --- | --- |
| `data/t3_clock.txt` | `SystemCoreClock` UART 출력 | ☐ |
| `data/t3_cyccnt_100ms.csv` | CYCCNT 검증 raw | ☐ |
| `measure/` | DWT 측정 공통 코드 | ☐ |
| `photos/` | 보드 bring-up 사진 | ☐ |

## 확인 필요

- PDF 레이아웃상 "DWM3001CDK 결과와 비교"와 "레인징 1회 DWT 측정" 항목이 Lv1인지 Lv2인지 불명확. 현재 Lv2로 둠.

## 빌드·플래싱

TODO: 예제 경로, 서명 명령, 업로드 방법 기록.
