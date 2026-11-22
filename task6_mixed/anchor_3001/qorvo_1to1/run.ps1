param(
    [ValidateSet('Smoke', 'Measure')][string]$Mode = 'Smoke',
    [string]$JLink = 'JLink.exe',
    [Parameter(Mandatory)][ValidatePattern('^\d+$')][string]$ProbeSerial
)

$ErrorActionPreference = 'Stop'
$image = Join-Path $PSScriptRoot 'build/cdk-initiator.bin'
$map = Join-Path $PSScriptRoot 'build/cdk-initiator.map'
$mapText = Get-Content -Raw -LiteralPath $map
$start = [regex]::Match($mapText, '(?m)^\s*(0x[0-9a-f]+)[ \t]+start[ \t]*\r?$').Groups[1].Value
$bench = [regex]::Match($mapText, '(?m)^\s*(0x[0-9a-f]+)[ \t]+bench[ \t]*\r?$').Groups[1].Value
if ($start -ne '0x20004000' -or $bench -notmatch '^0x200[0-1][0-9a-f]{4}$') {
    throw 'Unexpected RAM entry or bench status address.'
}

$length = (Get-Item -LiteralPath $image).Length
$benchAddress = [Convert]::ToUInt32($bench.Substring(2), 16)
if ($length -le 0 -or (0x20004000 + $length) -gt $benchAddress) {
    throw 'The RAM binary overlaps the bench status block.'
}

$commands = @('ExitOnError 1', 'r', "loadbin `"$image`", $start", "verifybin `"$image`", $start",
              "SetPC $start", 'g', 'Sleep 3600', "mem32 $bench 0x12")
if ($Mode -eq 'Smoke') {
    $commands += 'r', 'g'
}
$commands += 'exit'

$log = Join-Path $PSScriptRoot ('build/anchor-' + $Mode.ToLowerInvariant() + '-' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.log')
$commands | & $JLink -USB $ProbeSerial -Device NRF52833_XXAA -If SWD -Speed 4000 -AutoConnect 1 -NoGui 1 | Tee-Object -FilePath $log
if ($LASTEXITCODE -ne 0) { throw "J-Link RAM execution failed. See $log" }
