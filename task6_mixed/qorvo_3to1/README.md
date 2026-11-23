# DWM3001CDK 앵커 3대와 N6 태그 1대 거리 측정

2026-11-23. 태그 한 대에서 앵커별 거리 3개를 먼저 기록하고, 같은 배치의 줄자 기준 거리와 비교해 보정한다.

## 근거와 구현 범위

| 확인한 자료 | 결과 |
| --- | --- |
| 팀 Dropbox `indoor_localization_test_20261122.md` 및 `uwb_measurements_for_team_20261121.md` | 앵커가 먼저 묻는 7패킷 방식과 앵커별 3패킷을 3번 교환하는 9패킷 대안이 있다. 11월 23일 폴더 ZIP SHA-256은 이전 사본과 같았고, 동작하는 7패킷 이미지나 이번 조합의 3대1 이미지는 찾지 못했다. |
| `hw-bench` `task6`의 Task 2와 Task 6 | `--peers 1,3`로 거리를 번갈아 묻는 libdeca 구현과 실측 기록이 있다. 당시에는 N6가 initiator, DWM3001CDK가 responder였고 채널과 프레임도 이번 구성과 다르다. [태그 시험](../../task2_evb_stm32/lv2/README.md) · [앵커 시험](../anchor_3001/README.md) |
| `capstone_design/uwb` | 프레임 직렬화와 TDMA 검증용 코드가 있다. N6 실기 포팅은 예정으로 기록되어 있다. [UWB 구현 현황](https://github.com/Multi-Bus/capstone_design/tree/main/uwb) |
| `hw-bench` 1대1 PR | DWM3001CDK가 먼저 Poll을 보내는 채널 5 Qorvo DS-TWR에서 태그 거리 출력까지 확인했다. 이 브랜치는 해당 RAM 포트와 설정을 재사용한다. [1대1 시험 기록](../anchor_3001/qorvo_1to1/README.md) |

지상 시험에서는 PC가 J9의 SWD 메일박스를 통해 **A1 → A2 → A3** 순서로 한 대씩 호출한다. 각 호출은 `Poll → Response → Final` 최대 3개를 송신하고, 세 호출을 하나의 회차로 묶는다. 세 앵커의 자유 송신 주기를 맞출 필요 없이 교대하고, 제어용 UWB 패킷을 추가하지 않는다. PC의 USB/SWD 제어가 필요한 구성이다.

채널 5, 6.8 Mb/s, 프리앰블 128, 코드 9, SFD type 1, 표준 PHR, STS 꺼짐과 기존 응답 지연 900/700 UUS를 사용한다. PAN은 `0xDECA`, 앵커 주소는 `1`, `2`, `3`, 태그 주소는 `0x0100`이다. 주소와 세션 ID, 32비트 회차가 일치하는 Response와 Final만 사용한다. 태그는 Final을 받은 뒤 거리와 Poll/Final의 수신 세기를 기록하고, 앵커는 Response 수신 세기를 기록한다.

## 준비 및 실행

1. 팀 `n6_drone_handoff_20261120.zip`을 별도 폴더에 푼다. 그 안의 `stm32_projects/FSBL` 복사본에 태그 응답기를 설치한다. CubeIDE의 내부 RAM용 Debug 빌드를 실행하고 해당 ELF를 개발 모드로 RAM에 로드한다. 이전 ELF의 SHA-256과 시작 주소를 재사용할 때는 새 빌드의 ELF 심볼과 벡터를 먼저 대조한다. 태그 VCP는 115200 8N1이다.

   ```powershell
   python .\prepare_tag.py --fsbl-copy 'C:\bench\stm32_projects\FSBL'
   ```

2. 각 앵커용 RAM 이미지를 빌드하고 물리 보드에 A1, A2, A3를 표시한다. 아래는 세 보드에 참조 전력 인덱스 기본값 `27`과 안테나 지연 `16385`를 적용한다. A2/A3의 보드별 채널 5 전력 인덱스는 팀 보정 기록이나 실제 장비에서 확인 후 `-PowerIndex`로 지정한다.

   ```powershell
   foreach ($anchor in 1..3) {
       ..\anchor_3001\qorvo_1to1\build.ps1 -ToolchainBin '<ARM GNU bin>' `
           -UwbRoot 'C:\bench\stm32_projects\FSBL\Core\UWB' -AnchorId $anchor
   }
   ```

3. 앵커 세 대의 J9와 NUCLEO의 ST-LINK VCP를 같은 Windows PC에서 열 수 있게 연결한다. 프롭은 팀 작업서에 따라 분리하고 안테나가 서로 보이도록 장비를 고정한다. A1~A3의 `x, y, 높이`, 태그 기준점, 각 안테나 사이의 줄자 거리와 배치 사진을 남긴다. 다음 명령으로 우선 10회만 실행한다.

   ```powershell
   python -m pip install pyserial
   python .\capture.py --jlink '<JLink.exe 경로>' --probes <A1-SN> <A2-SN> <A3-SN> `
       --tag-port <COM> --tag-elf '<실행 중인 FSBL ELF>' --rounds 10 --out '<새 결과 폴더>'
   ```

4. `run.json`, `anchor.jsonl`, `tag.raw.log`, `packets.csv`, `rounds.csv`, `summary.json`을 확인한다. 같은 회차의 세 앵커별 거리가 출력되고, 한 앵커의 RF 교환이 실패해도 나머지 차례와 실패 기록이 남는지 확인한다. `no_tag_record`는 해당 회차의 태그 로그가 없다는 뜻이다. VCP 로깅 상태를 확인한 후 RF 손실로 해석한다. 송신 후 미수신율과 회차별 기대 패킷 기준 미완료율을 함께 기록한다. `all_three_ranges_rate`는 세 거리 모두를 얻은 회차의 비율이다.
5. 보정 전 3대1 로그를 저장한 후 팀이 정한 기준점과 알고리즘으로 보드별 안테나 지연을 조정한다. 앵커는 `-AntennaDelay`, 태그는 `prepare_tag.py --antenna-delay`를 사용한다. 보정 전후 배치와 설정을 기록한다. 0번 본시험은 팀 작업서에 따라 지점별 1,000회로 실행한다.

## 검증과 현장 확인

- 동일 핸드오프 드라이버로 A1/A2/A3와 기존 1대1 빌드를 확인했다. N6 응답기는 핸드오프 기반 별도 벤치 프로젝트에서 FSBL ELF 링크까지 확인했다.
- 호스트 테스트는 주소·세션·회차 검증, 32비트 타임스탬프 롤오버, 응답 누락 집계 및 부분 회차를 확인한다.
- 실장 3대1 시험, 세 J-Link의 동시 접속과 케이블 배치, A2/A3의 실제 보정값, N6 VCP의 거리와 수신 진단은 현장에서 확인해야 한다. 이 구현의 회차 속도에는 USB/SWD 호출 시간도 포함된다.
