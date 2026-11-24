param(
    [Parameter(Mandatory)][string]$ToolchainBin,
    [Parameter(Mandatory)][string]$UwbRoot
)

$ErrorActionPreference = 'Stop'
$uwb = (Resolve-Path -LiteralPath $UwbRoot).ProviderPath
$ToolchainBin = (Resolve-Path -LiteralPath $ToolchainBin).ProviderPath
$driver = Join-Path $uwb 'Shared/dwt_uwb_driver'
$example = Join-Path $uwb 'Src/examples'
$out = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$includes = @($PSScriptRoot, $driver, "$driver/lib/qmath/include", "$uwb/Src",
              "$uwb/platform", "$example/shared_data")
$sources = @("$PSScriptRoot/port.c", "$driver/deca_interface.c", "$driver/deca_compat.c",
             "$driver/deca_rsl.c", "$driver/lib/qmath/src/qmath.c", "$driver/dw3000/dw3000_device.c",
             "$uwb/Src/config_options.c", "$example/shared_data/shared_functions.c",
             "$example/ex_05a_ds_twr_init/ds_twr_initiator.c")

$elf = Join-Path $out 'cdk-initiator.elf'
$map = Join-Path $out 'cdk-initiator.map'
$bin = Join-Path $out 'cdk-initiator.bin'
$gcc = Join-Path $ToolchainBin 'arm-none-eabi-gcc.exe'
$objcopy = Join-Path $ToolchainBin 'arm-none-eabi-objcopy.exe'
$nm = Join-Path $ToolchainBin 'arm-none-eabi-nm.exe'

$includeFlags = $includes | ForEach-Object { "-I$_" }
& $gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -std=c11 -Os -g -ffunction-sections -fdata-sections `
    -DUSE_DRV_DW3000 -DCMAKE_UWB_CONFIG_OPTION=35 @includeFlags @sources -nostartfiles --specs=nano.specs --specs=nosys.specs `
    '-Wl,--gc-sections,--wrap=waitforsysstatus,--wrap=dwt_configuretxrf' "-Wl,-Map=$map" "-T$PSScriptRoot/ram.ld" -lm -o $elf
if ($LASTEXITCODE -ne 0) { throw 'DWM3001CDK build failed.' }

& $objcopy -O binary $elf $bin
if ($LASTEXITCODE -ne 0) { throw 'DWM3001CDK RAM image conversion failed.' }

& $nm -n $elf | Select-String '\b(start|bench)$'
if ($LASTEXITCODE -ne 0) { throw 'DWM3001CDK symbol lookup failed.' }
Get-FileHash -LiteralPath $bin
