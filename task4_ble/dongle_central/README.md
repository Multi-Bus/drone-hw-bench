# 동글 central 펌웨어 (NUS 클라이언트)

Nordic `central_uart` 샘플(nRF Connect SDK v2.9.0)을 nRF52840 Dongle용으로 고친 것.
동글이 NUS를 가진 BLE 장치(XIAO)를 스캔해 연결하고, 받은 데이터를 USB 가상 COM으로 내보낸다.

## 원본에서 바꾼 것

- `central_uart/boards/nrf52840dongle_nrf52840.overlay`: NUS 데이터 UART를 USB CDC ACM(`cdc_acm_uart`)으로 지정 (신규)
- `central_uart/boards/nrf52840dongle_nrf52840.conf`: USB 스택과 `UART_ASYNC_ADAPTER` 켜기, 하드웨어 UART와 UART 콘솔 끄기, LFCLK를 내부 RC로 (신규)
- `central_uart/prj.conf`: `CONFIG_BT_SETTINGS=n` (bonding 저장 끔. 켜져 있으면 USB가 열거되지 않음, [삽질기록](../../삽질기록.md) Task 4 참고). 보드 conf와 충돌하던 UART 콘솔 줄 제거
- `central_uart/prj.conf` (Lv2 처리량용): `CONFIG_HEAP_MEM_POOL_SIZE` 2048 → 16384, `BT_L2CAP_TX_MTU=247`, `BT_BUF_ACL_RX_SIZE=251`, `BT_BUF_ACL_TX_SIZE=251`, `BT_CTLR_DATA_LENGTH_MAX=251`. 이전에는 MTU 65에 묶였고 최대 속도 송신에서 손실이 22–75%였다
- `central_uart/src/main.c`: `UART_BUF_SIZE` 20 → 64 (244 B 알림을 13조각 대신 4조각으로 USB 시리얼에 전달)
- `central_uart/src/main.c`: `peripheral_uart`와 같은 방식으로 `usb_enable()`, `uart_async_adapter` 추가 (CDC ACM이 비동기 UART API를 지원하지 않아서). `usb_enable()`을 `bt_enable()` 앞으로 옮김 (BT 실패와 무관하게 USB 열거)

## 빌드

NCS v2.9.0을 Toolchain Manager로 `C:\ncs`에 설치한 상태에서 Git Bash로 실행:

```
# hw-bench 루트에서 실행 (어느 폴더에서든 스크립트 경로만 맞으면 됨)
task4_ble/dongle_central/build.sh
```

결과: `central_uart/build_hwb/merged.hex` (복사본: `artifacts/central_uart_mtu247.hex`).

빌드 폴더가 깊으면 Windows 경로 길이 제한(260자)에 걸려 실패할 수 있음. 그때는 `build.sh`의 `-d` 경로를 `C:\hwb_xxx`처럼 짧게 바꿈.

## 동글에 올리기

1. 동글 RESET 버튼을 눌러 부트로더 진입 (LD2 붉게 깜빡임, 장치 관리자에 `nRF52 SDFU USB`).
2. nRF Connect for Desktop > Programmer에서 동글 선택, hex 추가, Write.
3. 기존 connectivity 펌웨어(Bluetooth Low Energy 앱용)는 지워진다. 되돌리려면 Bluetooth Low Energy 앱이 안내하는 펌웨어 업데이트를 따른다.

## 사용

동글을 꽂으면 가상 COM이 생긴다. 터미널로 열면 XIAO가 보내는 `<seq>,<millis>` 줄이 그대로 나온다. 에코 입력은 반대로 XIAO로 간다.

## 확인 결과 (Task 4 Lv1)

- 동글 COM 포트 `VID_2FE3&PID_0100`, XIAO1 연결 후 `seq,millis` 줄 수신 (약 100 ms 간격).
- 15초 수신 로그: [`../data/lv1/pc_rx/t4_lv1_dongle_rx_15s.csv`](../data/lv1/pc_rx/t4_lv1_dongle_rx_15s.csv) (148행, seq 끊김 0, 간격 50–150 ms).
- 확인 안 됨: RC(`K32SRC_RC`) 사용 조건에서 BLE 장시간 안정성, 연결 끊김 후 재연결 시간.
