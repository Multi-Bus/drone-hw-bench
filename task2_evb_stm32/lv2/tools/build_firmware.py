"""Build RAM-development firmware without changing a CubeIDE workspace.

Requires the pinned external driver (with export patch), libdeca, STM32CubeN6
1.4.1 and an ARM GNU toolchain. No flashing or dependency downloads occur here.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('cube-root', 'driver-root', 'libdeca-root', 'tool-bin'):
        p.add_argument('--' + name, required=True, type=Path)
    p.add_argument('--role', choices=('initiator', 'responder', 'isr'), required=True)
    p.add_argument('--antenna-delay', type=int, default=16368)
    p.add_argument('--fast', action='store_true', help='Opt into unvalidated 8 MHz')
    p.add_argument('--out', type=Path)
    a = p.parse_args()
    if not 0 <= a.antenna_delay <= 65535:
        p.error('antenna delay must be 0..65535')
    lv2 = Path(__file__).resolve().parents[1]
    bench = lv2.parents[1]
    out = (a.out or bench / 'build' / ('t2_' + a.role)).resolve()
    out.mkdir(parents=True, exist_ok=True)
    expected = {
        'driver': (a.driver_root, '67dfbb7f2c5b1a4157b8c265b19f08776e91b1cc'),
        'libdeca': (a.libdeca_root, '226f3db3bc4ef2be9c143e8340a3851cd571cce9'),
    }
    for label, (root, sha) in expected.items():
        actual = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != sha:
            raise SystemExit(f'{label} HEAD must be {sha}, found {actual}')
    device = a.driver_root / 'dwt_uwb_driver/dw3000/dw3000_device.c'
    for signature in ('static int32_t ull_run_pgfcal(', 'static void ull_setplenfine(',
                      'static void ull_dis_otp_ips(', 'static void ull_setstslength('):
        if signature in device.read_text(encoding='utf-8'):
            raise SystemExit('Apply T2_UWB_ISR/driver_patches export patch first')
    project = (lv2.parent / 'lv2_spi/T2_UWB_ISR' if a.role == 'isr' else lv2 / 'twr_cubeide')
    fsbl = project / 'FSBL'
    ide = project / 'STM32CubeIDE/FSBL'
    adapter = lv2.parent / 'lv2_spi/T2_UWB_SPI/FSBL'
    driver = a.driver_root / 'dwt_uwb_driver'
    st = a.cube_root / 'Drivers'
    app = lv2 / 'uwb_app'
    includes = [adapter / 'Inc', fsbl / 'Inc', app, a.libdeca_root,
                driver, driver / 'dw3000', driver / 'lib/qmath/include', a.driver_root / 'platform',
                st / 'STM32N6xx_HAL_Driver/Inc', st / 'STM32N6xx_HAL_Driver/Inc/Legacy',
                st / 'CMSIS/Device/ST/STM32N6xx/Include', st / 'CMSIS/Include',
                st / 'BSP/STM32N6xx_Nucleo']
    cpu = ['-mcpu=cortex-m55', '-mthumb', '-mfpu=fpv5-d16', '-mfloat-abi=hard', '-mcmse']
    flags = cpu + ['-std=gnu11', '-O2', '-g3', '-ffunction-sections', '-fdata-sections',
                   '-Wall', '-Wextra', '-Werror=implicit-function-declaration',
                   '-DUSE_HAL_DRIVER', '-DSTM32N657xx', '-DUSE_DRV_DW3000',
                   '-DCONFIG_DW3000_CHIP_DW3000=1',
                   f'-DUWB_RESPONDER={int(a.role == "responder")}',
                   f'-DUWB_ANTENNA_DELAY={a.antenna_delay}',
                   f'-DUWB_SPI_ENABLE_FAST={int(a.fast)}']
    flags += ['-I' + str(d) for d in includes]
    sources = [fsbl / 'Src' / name for name in
               ('main.c', 'stm32n6xx_hal_msp.c', 'stm32n6xx_it.c', 'system_stm32n6xx_fsbl.c')]
    sources += [adapter / 'Src' / name for name in ('uwb_port.c', 'dw3000_hw.c', 'dw3000_spi.c', 'deca_port.c')]
    sources += [driver / 'deca_interface.c', driver / 'deca_rsl.c', device,
                driver / 'lib/qmath/src/qmath.c', a.driver_root / 'platform/deca_compat.c']
    hal = ('hal', 'hal_cortex', 'hal_dma', 'hal_dma_ex', 'hal_exti', 'hal_gpio',
           'hal_pwr', 'hal_pwr_ex', 'hal_rcc', 'hal_rcc_ex', 'hal_spi', 'hal_spi_ex',
           'hal_uart', 'hal_uart_ex')
    sources += [st / 'STM32N6xx_HAL_Driver/Src' / ('stm32n6xx_' + name + '.c') for name in hal]
    sources += [st / 'BSP/STM32N6xx_Nucleo/stm32n6xx_nucleo.c',
                ide / 'Application/User/syscalls.c', ide / 'Application/User/sysmem.c',
                ide / 'Application/Startup/startup_stm32n657x0hxq_fsbl.s']
    if a.role != 'isr':
        sources += [app / name for name in ('uwb_app.c', 'uwb_task_stm32.c', 'uwb_runner.c')]
        sources += [a.libdeca_root / (name + '.c') for name in
                    ('dwhw', 'dwmac', 'dwmac_irq', 'dwphy', 'dwtime', 'ranging',
                     'blink', 'sync', 'dwproto', 'mac802154', 'dwutil')]
    gcc = str(a.tool_bin / 'arm-none-eabi-gcc.exe')
    objects = []
    with (out / 'build.log').open('w', encoding='utf-8') as log:
        def run(args):
            result = subprocess.run([str(v) for v in args], stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, errors='replace')
            log.write(result.stdout)
            log.flush()
            if result.returncode:
                print(result.stdout)
                raise SystemExit(f'Build failed; see {out / "build.log"}')
        for i, source in enumerate(sources):
            obj = out / f'{i:02}_{source.stem}.o'
            run([gcc, *flags, '-c', source, '-o', obj])
            objects.append(obj)
        elf = out / ('t2_' + a.role + '.elf')
        run([gcc, *cpu, '-T', ide / 'STM32N657X0HXQ_AXISRAM2_fsbl.ld',
             '--specs=nano.specs', '--specs=nosys.specs', '-static',
             '-Wl,--gc-sections', '-Wl,-Map=' + str(out / 'firmware.map'),
             *objects, '-Wl,--start-group', '-lc', '-lm', '-Wl,--end-group', '-o', elf])
        run([a.tool_bin / 'arm-none-eabi-objcopy.exe', '-O', 'binary', elf, elf.with_suffix('.bin')])
        run([a.tool_bin / 'arm-none-eabi-size.exe', elf])
    manifest = {'role': a.role, 'antenna_delay': a.antenna_delay,
                'spi_hz': 8000000 if a.fast else 2000000,
                'elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
                'dependencies': {key: sha for key, (_, sha) in expected.items()},
                'hardware_validated': False,
                'boot': 'RAM development image; power loss/reset needs reload'}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(f'BUILT {elf}')
    print((out / 'build.log').read_text(encoding='utf-8')[-500:])


if __name__ == '__main__':
    main()
