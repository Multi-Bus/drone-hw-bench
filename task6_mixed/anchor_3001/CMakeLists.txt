# Task 6: DWM3001CDK (nRF52833 + DW3110) firmware with the br101 DW3000 driver v08.02.02
# Same driver as the Task 2 tag (STM32N6 + DWM3000EVB) so both sides speak the same protocol later (libdeca).
cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake)
project(anchor_3001 C ASM)

# nRF5 SDK 17.0.2 bundled with the Qorvo DW3_QM33_SDK (Drivers/API/Build_Platforms/nRF52840-DK/sdk)
set(NRF5_SDK "" CACHE PATH "nRF5 SDK root")
if(NOT EXISTS "${NRF5_SDK}/modules/nrfx/mdk/nrf52833.h")
  message(FATAL_ERROR "Set -DNRF5_SDK=<...>/Drivers/API/Build_Platforms/nRF52840-DK/sdk")
endif()
set(DRV ${CMAKE_CURRENT_SOURCE_DIR}/driver)
if(NOT EXISTS "${DRV}/dwt_uwb_driver/deca_device_api.h")
  message(FATAL_ERROR "Prepare driver/ first (see README.md)")
endif()

add_executable(anchor_3001.elf
  src/main.c
  # br101 driver v08.02.02 (same file set as Task 2 T2_UWB_ISR)
  ${DRV}/dwt_uwb_driver/deca_interface.c
  ${DRV}/dwt_uwb_driver/deca_rsl.c
  ${DRV}/dwt_uwb_driver/dw3000/dw3000_device.c
  ${DRV}/dwt_uwb_driver/lib/qmath/src/qmath.c
  ${DRV}/platform/deca_compat.c
  # br101 nRF5 SDK platform port (unchanged)
  ${DRV}/platform/nrf-sdk/deca_port.c
  ${DRV}/platform/nrf-sdk/dw3000_hw.c
  ${DRV}/platform/nrf-sdk/dw3000_spi.c
  # nRF5 SDK
  ${NRF5_SDK}/modules/nrfx/mdk/gcc_startup_nrf52833.S
  ${NRF5_SDK}/modules/nrfx/mdk/system_nrf52833.c
  ${NRF5_SDK}/modules/nrfx/drivers/src/nrfx_spim.c
  ${NRF5_SDK}/modules/nrfx/drivers/src/nrfx_gpiote.c
  ${NRF5_SDK}/components/libraries/util/app_util_platform.c
  ${NRF5_SDK}/external/segger_rtt/SEGGER_RTT.c
  ${NRF5_SDK}/external/segger_rtt/SEGGER_RTT_printf.c
)

target_include_directories(anchor_3001.elf PRIVATE
  src
  ${DRV}/dwt_uwb_driver
  ${DRV}/dwt_uwb_driver/dw3000
  ${DRV}/dwt_uwb_driver/lib/qmath/include
  ${DRV}/platform
  ${NRF5_SDK}/modules/nrfx
  ${NRF5_SDK}/modules/nrfx/hal
  ${NRF5_SDK}/modules/nrfx/mdk
  ${NRF5_SDK}/modules/nrfx/drivers/include
  ${NRF5_SDK}/integration/nrfx
  ${NRF5_SDK}/integration/nrfx/legacy
  ${NRF5_SDK}/components/toolchain/cmsis/include
  ${NRF5_SDK}/components/libraries/util
  ${NRF5_SDK}/components/libraries/delay
  ${NRF5_SDK}/components/drivers_nrf/nrf_soc_nosd
  ${NRF5_SDK}/components/libraries/log
  ${NRF5_SDK}/components/libraries/log/src
  ${NRF5_SDK}/components/libraries/experimental_section_vars
  ${NRF5_SDK}/components/libraries/strerror
  ${NRF5_SDK}/external/segger_rtt
)

target_compile_definitions(anchor_3001.elf PRIVATE
  NRF52833_XXAA
  FLOAT_ABI_HARD
  __HEAP_SIZE=8192
  __STACK_SIZE=8192
  USE_DRV_DW3000
  CONFIG_DW3000_CHIP_DW3000=1
)

target_link_options(anchor_3001.elf PRIVATE
  -L${NRF5_SDK}/modules/nrfx/mdk
  -T${NRF5_SDK}/modules/nrfx/mdk/nrf52833_xxaa.ld
  -Wl,-Map=${CMAKE_BINARY_DIR}/anchor_3001.map
)
target_link_libraries(anchor_3001.elf PRIVATE c nosys m)

add_custom_command(TARGET anchor_3001.elf POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex anchor_3001.elf anchor_3001.hex
  COMMAND ${CMAKE_SIZE} anchor_3001.elf)
