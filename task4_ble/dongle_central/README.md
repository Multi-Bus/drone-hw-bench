# 동글 central 펌웨어 (NUS 클라이언트)

Nordic `central_uart` 샘플(nRF Connect SDK v2.9.0)을 nRF52840 Dongle용으로 고친 것.
동글이 NUS를 가진 BLE 장치(XIAO)를 스캔해 연결하고, 받은 데이터를 USB 가상 COM으로 내보낸다.

## 원본에서 바꾼 것

- `central_uart/boards/nrf52840dongle_nrf52840.overlay`: NUS 데이터 UART를 USB CDC ACM(`cdc_acm_uart`)으로 지정 (신규)
- `central_uart/boards/nrf52840dongle_nrf52840.conf`: USB 스택과 `UART_ASYNC_ADAPTER` 켜기, 하드웨어 UART와 UART 콘솔 끄기 (신규)
- `central_uart/src/main.c`: `peripheral_uart`와 같은 방식으로 `usb_enable()`, `uart_async_adapter` 추가 (CDC ACM이 비동기 UART API를 지원하지 않아서)

## 빌드

NCS v2.9.0을 Toolchain Manager로 `C:\ncs`에 설치한 상태에서 Git Bash로 실행:

```
cd /c/ncs/v2.9.0
/c/Users/sujin/coding/projects/hw-bench/task4_ble/dongle_central/build.sh
```

결과: `central_uart/build/merged.hex` (복사본: `artifacts/central_uart_nrf52840dongle_ncs2.9.0.hex`, 주소 0x1000~0x3EB60, MBR와 부트로더 영역은 건드리지 않음).

## 동글에 올리기

1. 동글 RESET 버튼을 눌러 부트로더 진입 (LD2 붉게 깜빡임, 장치 관리자에 `nRF52 SDFU USB`).
2. nRF Connect for Desktop > Programmer에서 동글 선택, hex 추가, Write.
3. 기존 connectivity 펌웨어(Bluetooth Low Energy 앱용)는 지워진다. 되돌리려면 Bluetooth Low Energy 앱이 안내하는 펌웨어 업데이트를 따른다.

## 사용

동글을 꽂으면 가상 COM이 생긴다. 터미널로 열면 XIAO가 보내는 `<seq>,<millis>` 줄이 그대로 나온다. 에코 입력은 반대로 XIAO로 간다.
