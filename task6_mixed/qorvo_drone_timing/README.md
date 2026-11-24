# N6 + DWM3000EVB 드론 간 교환 시간 벤치

두 대의 N6에 요청자와 응답자 이미지를 각각 올린다. 요청자는 주소 목록을 순회하므로 여러 상대를 같은 설정으로 비교할 수 있다. 이 벤치는 지상 A2 계측용이다. 3001 앵커 측위와 각 드론이 송신하는 6대 TDMA 상태 공유는 각각의 펌웨어에서 계측한다.

## 실기체에 맞춘 설정

- 실기체 계획의 128심볼 프리앰블, 8심볼 SFD, 코드 9, 표준 PHR와 기본 PHR 전송률, 6.8 Mb/s 데이터, STS 끔을 펌웨어에서 명시한다. 채널 5를 기본으로 쓰며, 같은 장소에서 3001 측위와 나눠 진행할 때 채널 9로 예비 시험할 수 있다. 비교 결과에는 실제 채널을 기록한다.
- 기존 Qorvo 3:1의 900 / 700 UUS 응답·마무리 지연을 초기값으로 사용한다. 비행 펌웨어에서 지연이 확정되면 `--response-uus`와 `--final-uus`에 그 값을 넣고 두 역할을 같은 값으로 다시 빌드한다. RX 시작은 대응하는 예약 지연보다 설정상 200 UUS 앞당긴다. 이 오프셋의 실제 여유는 프레임 길이와 앞선 송신 완료 시점에 따라 달라지므로 현장에서 확인한다.
- 기본값 `--period-ms 20`은 한 요청자가 프레임마다 한 번 교환을 시도하는 조건이다. `--period-ms 0`은 대기를 제거한 벤치 스트레스 조건이며 VCP 출력이 시도 간격에 영향을 준다. 회차 시간은 UART 출력 전에 읽은 UWB 타임스탬프로 계산한다. 자동 재시도 없이 시도마다 성공 또는 실패를 집계한다.
- `Poll` 20 B, `Response` 20/40/60 B, `Final` 32 B의 PSDU를 전송한다. FCS는 DW3000이 처리하고, 요청자는 주소·회차·본문 패턴을 검사한다. 거리 계산 없이 기존의 안테나 지연 기본값으로 실행한다.

## 준비와 실행

팀의 `n6_drone_handoff_20261120.zip`에서 `stm32_projects/FSBL`을 보드별 별도 복사본으로 준비한다. `prepare.py`는 해당 복사본의 Qorvo 응답자 예제를 벤치로 교체한다. 예시는 payload 20 B, 상대 한 대, 예비 시험 100회다. 세 번째 보드를 추가할 때는 주소 3의 응답자 복사본을 만들고 요청자 명령을 `--peers 2 3`으로 바꾼다. 본시험은 새 세션 ID를 주고 `--rounds-per-peer`를 생략하면 상대마다 10,000회씩 진행한다.

```powershell
python prepare.py --fsbl-copy '<FSBL-I>' --role initiator --address 1 --peers 2 --session 1 --payload 20 --rounds-per-peer 100
python prepare.py --fsbl-copy '<FSBL-R2>' --role responder --address 2 --payload 20
```

기존 핸드오프의 N6 Qorvo 예제 빌드/실행 방식으로 각 FSBL을 빌드한다. 프로젝트에 UWB 예제 소스가 포함되어야 한다. 빌드에서 예제 선택 매크로를 직접 지정할 경우에는 `TEST_DS_TWR_RESPONDER`를 선택한다. VCP 115200 8N1을 보드별 별도 UTF-8 로그로 열고, 응답자를 먼저 실행한 뒤 요청자를 시작한다. 모든 로그에 `DWB_READY`가 남고 요청자 로그가 `DWB_DONE`까지 끝나야 한다. 첫 시험은 실제 편대에서 사용할 거리와 보드 방향을 고정하고 채널, 거리, 배치, 보드 ID 및 펌웨어 커밋을 함께 남긴다.

```powershell
python report.py --initiator n6-1.log --responders n6-2.log --out two-nodes.csv
python report.py --initiator n6-1-multi.log --responders n6-2-multi.log n6-3-multi.log --out three-nodes.csv
```

## 결과 해석

| 값 | 기준 |
| --- | --- |
| `exchange_us` | 요청자에서 읽은 실제 `Poll TX RMARKER → Final TX RMARKER` 차이. 동일 보드의 40비트 시계 사용 |
| `slot_estimate_us` | 교환 시각 차이에 첫 프레임의 RMARKER 이전 구간과 마지막 프레임의 RMARKER 이후 구간을 더한 슬롯 추정값. 두 프레임의 SHR가 같아 마지막 프레임 전체 길이만큼 더하면 된다 |
| `airtime_estimate_us` | 프리앰블, SFD, PHR, FCS, Reed–Solomon 오버헤드를 포함한 세 패킷의 이론 전송 시간 합계. 패킷 사이의 대기를 제외한다 |
| `confirmed` | 요청자가 `Final`을 송신했고 응답자도 같은 회차의 `Final`을 확인. 응답자 로그 누락은 미확인으로 남긴다 |
| `sweep_start_to_start_ms` | 첫 상대를 호출한 시점부터 다음 순회의 첫 상대를 호출할 때까지. 요청자 MCU의 ms 시계 사용 |
| `sweep_first_start_to_last_finish_ms` | 첫 상대를 호출한 시점부터 마지막 상대의 교환 처리 또는 타임아웃이 끝날 때까지 |
| `successful_update_gap_ms` | 같은 상대에서 양쪽 로그로 확인한 성공 회차 사이 간격 |

응답과 마무리 송신은 예약되어 있으므로, 페이로드가 늘어도 고정된 지연 안에 수신과 처리가 끝나면 교환 시간은 비슷할 수 있다. 평균뿐 아니라 p95, 최댓값과 미확인율을 보고, 실제 프레임 예산과 비교한다. 6대 순회 수치는 한 요청자의 순회 조건이다. 여러 송신자가 공유 채널에서 각자 상태를 보내는 성능은 TDMA 기반 현장 로그로 확인한다.

결과 코드는 `0` 성공, `1` Poll 송신 실패, `2` Response 미수신, `3` Response 형식·내용 불일치, `4` Final 송신 실패, `5` Response 송신 실패, `6` Final 미수신, `7` Final 형식 불일치다. 미수신 행의 상태 레지스터를 보면 CRC 오류와 타임아웃을 구분할 수 있다.

- [기존 앵커·태그 프레임과 지연](../qorvo_3to1/README.md)
- [Qorvo DW3000 데이터시트](https://www.mouser.com/datasheet/2/412/Qorvo_DW3000-2934245.pdf)의 Frame Format 및 Symbol Timings
