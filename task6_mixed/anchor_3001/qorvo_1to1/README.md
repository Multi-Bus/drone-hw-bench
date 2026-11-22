# N6 태그와 DWM3001CDK 앵커의 1대1 거리 확인

2026-11-22에 태그와 앵커 한 대를 서로 보이는 곳에 고정하고 안테나 사이를 줄자로 3.00 m로 맞췄다. 태그 시리얼에서 거리값이 반복 출력됐다.

| 항목 | 적용값 |
| --- | --- |
| 앵커 | DWM3001CDK, DW3110 `0xDECA0302`, Qorvo `ex_05a` / `DS TWR INIT v1.0`을 nRF52833 RAM에서 실행 |
| 태그 | NUCLEO-N657X0-Q + DWM3000EVB, Qorvo `ex_05b` / `DS TWR RESP v1.0`을 STM32N6 내부 RAM에서 실행, ST-LINK VCP 115200 8N1 |
| PHY | 채널 5, BPRF, 6.8 Mb/s, 프리앰블 128, PAC 8, TX/RX 코드 9, SFD type 1, SFD 타임아웃 129, 표준 PHR, STS 및 PDOA 끔 |
| 프레임 | PAN `0xDECA`, Poll `WAVE/0x21`, Response `VEWA/0x10`, Final `WAVE/0x23` |
| 타이밍 | 태그 Poll RX → Response TX 900 UUS. 앵커 Response 수신 지연/타임아웃 700/300 UUS, Response RX → Final TX 700 UUS. 태그 Final 수신 지연/타임아웃 500/220 UUS |
| 안테나 지연 | 양쪽 TX/RX 각각 16385 DTU, 보정 전 |
| 앵커 송신 전력 | 채널 5 참조 인덱스 `0x1B`를 Qorvo `dwt_calculate_linear_tx_power()`로 변환. 보드에서 `TX_POWER = 0x6D6D6D6D`를 읽었고, 변환 결과의 PLL bias 7을 적용 |

## 시험 결과

| 구간 | 거리 출력 | 평균 | 줄자 대비 | 표준편차 | 범위 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 앵커 수정 후 초기 측정 (21:26:23~21:29:48 KST, 팀 보고) | 165 | 3.625 m | +0.625 m | 0.181 m | 3.31~4.11 m |
| 30초 재측정 (21:32:01~21:32:32 KST) | 26 | 3.528 m | +0.528 m | 0.224 m (표본) | 3.29~4.30 m |

30초 재측정의 중앙값은 3.455 m다. 태그 로그는 수신과 거리 계산이 완료된 회차를 출력한다. 재측정 시간과 일치하는 앵커 송신 카운터가 없어 이 구간의 전체 시도 수와 손실률은 미확정이다. [태그 VCP 원본](태그_COM10_30초_213201-213232.log)

초기에는 앵커가 참조 전력 인덱스를 `TX_POWER`에 직접 써서 태그의 Poll 수신 오류가 잦았다. 변환을 적용하기 전에는 앵커 누적 Poll 347회 중 Final을 14회 보냈다. 수정 후 첫 카운터 스냅샷에서는 Poll 33회 중 Final을 23회 보냈다. 두 스냅샷의 구간 길이와 시작 시각이 다르다.

실측에 사용한 앵커 RAM 이미지 SHA-256은 `F9033E839C0366451C90CC329B9EAC71A85AFC15181A990BA6A43010F4AF1547`이고, 태그 ELF SHA-256은 `D980EC6BB2AA6495A8C894B6A41784A581C4C47EEE59D8F2CB28587B055B5A63`이다.

## 다시 실행하기

팀 핸드오프 `n6_drone_handoff_20261120.zip`을 풀어 `stm32_projects/FSBL/Core/UWB`를 사용한다. 아래 PowerShell 명령은 이 디렉터리에서 실행한다. ARM GNU Toolchain의 `bin` 폴더와 J-Link 실행 파일을 지정하고, 앵커는 J9에 연결한다.

```powershell
.\build.ps1 -ToolchainBin '<ARM GNU bin 경로>' -UwbRoot '<핸드오프 FSBL/Core/UWB 경로>'
.\run.ps1 -JLink '<JLink.exe 경로>' -ProbeSerial '<J-Link 일련번호>' -Mode Smoke
.\run.ps1 -JLink '<JLink.exe 경로>' -ProbeSerial '<J-Link 일련번호>' -Mode Measure
```

`Smoke`는 RAM 이미지를 검증하고 초기 카운터를 읽은 뒤 기존 플래시에서 재부팅한다. `Measure`는 이미지가 RAM에서 계속 실행되도록 둔다. 수정 빌드는 nRF52833 TIMER2를 사용한다. 시험 중 J-Link Commander를 종료해도 송신 카운터가 계속 증가하는 것을 확인했다.

다음 단계는 3.00 m 배치에서 안테나 지연을 보정하고 전후 로그를 비교하는 것이다. 이후 동일한 시간 구간에서 앵커 송신과 태그 결과를 함께 캡처해 성공과 실패 회차를 CSV에 남긴다.
