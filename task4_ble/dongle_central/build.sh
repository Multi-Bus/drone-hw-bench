#!/bin/bash
# Build central_uart for nRF52840 Dongle with NCS v2.9.0 (Windows, toolchain from Toolchain Manager)
TC=/c/ncs/toolchains/b620d30767
export PATH="$TC:$TC/mingw64/bin:$TC/bin:$TC/opt/bin:$TC/opt/bin/Scripts:$TC/opt/nanopb/generator-bin:$TC/opt/zephyr-sdk/arm-zephyr-eabi/bin:$PATH"
export PYTHONPATH="$TC/opt/bin:$TC/opt/bin/Lib:$TC/opt/bin/Lib/site-packages"
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR="$TC/opt/zephyr-sdk"
export ZEPHYR_BASE=/c/ncs/v2.9.0/zephyr
SRC="$(cd "$(dirname "$0")/central_uart" && pwd)" || exit 1
cd /c/ncs/v2.9.0 || exit 1   # west workspace root (NCS install)
west build -b nrf52840dongle/nrf52840 --pristine -d "$SRC/build_hwb" "$SRC" "$@"
