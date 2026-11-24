param(
    [Parameter(Mandatory)][string]$ToolchainBin,
    [Parameter(Mandatory)][string]$UwbRoot,
    [ValidateRange(0,3)][int]$AnchorId = 0,
    [ValidateRange(0,255)][int]$PowerIndex = 27,
    [ValidateRange(0,65535)][int]$AntennaDelay = 16385
)

$ErrorActionPreference = 'Stop'
$uwb = (Resolve-Path -LiteralPath $UwbRoot).ProviderPath
$ToolchainBin = (Resolve-Path -LiteralPath $ToolchainBin).ProviderPath
$driver = Join-Path $uwb 'Shared/dwt_uwb_driver'
$example = Join-Path $uwb 'Src/examples'
$out = Join-Path $PSScriptRoot 'build'
if ($AnchorId) { $out = Join-Path $out "A$AnchorId" }
New-Item -ItemType Directory -Force -Path $out | Out-Null

$includes = @($PSScriptRoot, $driver, "$driver/lib/qmath/include", "$uwb/Src",
              "$uwb/platform", "$example/shared_data")
$sources = @("$PSScriptRoot/port.c", "$driver/deca_interface.c", "$driver/deca_compat.c",
             "$driver/deca_rsl.c", "$driver/lib/qmath/src/qmath.c", "$driver/dw3000/dw3000_device.c",
             "$uwb/Src/config_options.c")
$defines = @('-DUSE_DRV_DW3000', '-DCMAKE_UWB_CONFIG_OPTION=35', "-DCDK_POWER_INDEX=$PowerIndex")
if ($AnchorId) {
    $three = (Resolve-Path (Join-Path $PSScriptRoot '../../qorvo_3to1')).ProviderPath
    $includes += $three
    $sources += "$three/anchor.c"
    $defines += "-DUWB3_ANCHOR_ID=$AnchorId", "-DUWB3_ANTENNA_DELAY=$AntennaDelay"
} else {
    $sources += "$example/shared_data/shared_functions.c", "$example/ex_05a_ds_twr_init/ds_twr_initiator.c"
}

$elf = Join-Path $out 'cdk-initiator.elf'
$map = Join-Path $out 'cdk-initiator.map'
$bin = Join-Path $out 'cdk-initiator.bin'
$gcc = Join-Path $ToolchainBin 'arm-none-eabi-gcc.exe'
$objcopy = Join-Path $ToolchainBin 'arm-none-eabi-objcopy.exe'
$nm = Join-Path $ToolchainBin 'arm-none-eabi-nm.exe'

$includeFlags = $includes | ForEach-Object { "-I$_" }
& $gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -std=c11 -Os -g -ffunction-sections -fdata-sections `
    @defines @includeFlags @sources -nostartfiles --specs=nano.specs --specs=nosys.specs `
    '-Wl,--gc-sections,--wrap=waitforsysstatus,--wrap=dwt_configuretxrf' "-Wl,-Map=$map" "-T$PSScriptRoot/ram.ld" -lm -o $elf
if ($LASTEXITCODE -ne 0) { throw 'DWM3001CDK build failed.' }

& $objcopy -O binary $elf $bin
if ($LASTEXITCODE -ne 0) { throw 'DWM3001CDK RAM image conversion failed.' }

& $nm -n $elf | Select-String '\b(start|bench|mailbox)$'
if ($LASTEXITCODE -ne 0) { throw 'DWM3001CDK symbol lookup failed.' }
$imageHash = Get-FileHash -LiteralPath $bin
if ($AnchorId) {
    @{
        anchor_id = $AnchorId
        channel = 5
        antenna_delay_dtu = $AntennaDelay
        tx_power_index = $PowerIndex
        image_sha256 = $imageHash.Hash.ToLowerInvariant()
    } | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $out 'manifest.json')
}
$imageHash
