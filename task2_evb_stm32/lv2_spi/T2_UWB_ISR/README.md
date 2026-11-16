# T2_UWB_ISR: DW3000 드라이버 + 인터럽트 시험 (Task 2 Lv2, 보드 1)

> 2026-11-16 보완: 시작 실패 시 후속 드라이버 접근을 중단하고, A/B의 PASS 조건에 handler/hook/callback 정확히 1회를 강제했다. 보완 소스는 ARM 객체 컴파일만 확인했으며 보드 재시험은 아직이다. 기존 로그는 보완 전 시험 기록이다. 다음 작업은 [인수인계 문서](../../lv2/docs/HANDOFF_2026-11-16.md)를 참고한다.

`T2_UWB_SPI`의 어댑터(`uwb_port`, `dw3000_spi`, `dw3000_hw`, `deca_port`)를 실제 br101 드라이버와 연결해서, `dwt_isr()`가 EXTI12 인터럽트로 불리고 `SPIRDY`가 처리되어 IRQ가 내려가는지 확인하는 CubeIDE 프로젝트다. 결과는 [`../README.md`](../README.md)의 "드라이버 + 인터럽트 시험", 원본 로그는 [`../logs/t2_lv2_isr_board1.txt`](../logs/t2_lv2_isr_board1.txt).

- 시험 코드: `FSBL/Src/main.c` (드라이버 시작 → A) IRQ가 High인 채 인터럽트 켜기 → B) 하드웨어 리셋 20회).
- 어댑터 소스는 이 폴더에 복사하지 않고 형제 폴더 `../T2_UWB_SPI/FSBL/Src`를 `.project`에서 링크한다. **`T2_UWB_SPI`와 `T2_UWB_ISR`이 같은 폴더 안에 나란히 있어야 한다.**
- 드라이버 소스는 repo에 넣지 않았다 (라이선스 혼재, 팀 합의). 아래 순서로 직접 준비한다.

## 드라이버 준비

```bash
git clone https://github.com/br101/dw3000-decadriver-source.git /tmp/br101_decadriver
git -C /tmp/br101_decadriver checkout 67dfbb7
mkdir driver
cp -r /tmp/br101_decadriver/dwt_uwb_driver /tmp/br101_decadriver/platform driver/
cd driver && git apply ../driver_patches/0001-dw3000_device-export-ull-symbols.patch
```

- 커밋 `67dfbb7` (`DRIVER_VERSION_STR` = 08.02.02) 기준이다. 다른 커밋은 시험하지 않았다.
- `driver/`는 `.gitignore`에 들어 있다.
- 패치: 이 커밋의 `dw3000_device.c`는 `ull_run_pgfcal`, `ull_setplenfine`, `ull_dis_otp_ips`, `ull_setstslength`를 `static`으로 선언하는데 `platform/deca_compat.c`가 밖에서 부른다. 그대로는 링크가 안 된다. 패치는 선언 네 줄에서 `static`만 뺀다. upstream 쪽에는 수정이나 이슈를 올리지 않았다.
- 프로젝트가 컴파일하는 드라이버 파일은 `dwt_uwb_driver/deca_interface.c`, `deca_rsl.c`, `dw3000/dw3000_device.c`, `lib/qmath/src/qmath.c`, `platform/deca_compat.c`다. **`dwt_uwb_driver/deca_compat.c`가 아니라 `platform/deca_compat.c`다** (구버전은 `ioctl` 컴파일 오류가 난다).
- 정의: `USE_DRV_DW3000`, `CONFIG_DW3000_CHIP_DW3000=1`.

## 빌드와 실행

1. `T2_UWB_SPI`와 같은 방식으로 ST 예제 폴더(`...\Projects\NUCLEO-N657X0-Q\Examples\UART\`) 안에 두고 CubeIDE로 `STM32CubeIDE/FSBL`을 가져온다 (HAL 소스를 상대 경로로 링크하기 때문).
2. Clean → Build. 링크 확인 결과: text 186 KB, 우리 파일 5개는 `-Wall -Wextra` 경고 없음.
3. 보드 1에서 Debug As로 시작해 main에서 멈춘 뒤 Tera Term(115200, 7비트 + odd 패리티, 1 stop) 로그를 시작하고 Resume한다.
4. `== done ==`과 `verdict: PASS`를 확인한다.

## 알아둘 점 (통합할 때)

- **`dwt_initialise()` 전에 `dwt_readdevid()`, `dwt_checkidlerc()` 같은 드라이버 레지스터 읽기를 부르지 않는다.** `dw->priv`가 아직 NULL이라 멈춘다 (자세한 내용은 `../README.md`).
- `deca_sleep()`은 DWT 사이클 카운터 busy wait이다 (ISR 안에서도 반환되도록).
- 8 MHz, 실제 슬립 wakeup, `write_crc`는 시험하지 않았다.
