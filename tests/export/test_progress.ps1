# Progress protocol parser tests for export.ps1
$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Fixtures = Join-Path $PSScriptRoot 'fixtures'
$script:Pass = 0
$script:Fail = 0

function Pass([string]$Name) { $script:Pass++; Write-Host "  PASS  $Name" }
function Fail([string]$Name, [string]$Detail = '') {
    $script:Fail++
    Write-Host "  FAIL  $Name"
    if ($Detail) { Write-Host "        $Detail" }
}

$env:TUYAOPEN_EXPORT_SKIP_MAIN = '1'
. (Join-Path $Root 'export.ps1')

Write-Host "`n== Cold start kind =="
$kind = Get-TuyaExportColdStartKind -Root $Root
if ($kind -in @('full', 'venv_only', 'warm')) {
    Pass "cold start kind valid ($kind)"
} else {
    Fail 'cold start kind valid' $kind
}

Write-Host "`n== IDE host gate =="
$env:TUYAOPEN_EXPORT_IDE = $null
$script:TuyaOpenIdeHost = $false
if (-not $script:TuyaOpenIdeHost) { Pass 'IdeHost false when unset' } else { Fail 'IdeHost false when unset' }
$env:TUYAOPEN_EXPORT_IDE = '1'
$script:TuyaOpenIdeHost = ($env:TUYAOPEN_EXPORT_IDE -eq '1')
if ($script:TuyaOpenIdeHost) { Pass 'IdeHost true when set' } else { Fail 'IdeHost true when set' }
Remove-Item Env:TUYAOPEN_EXPORT_IDE -ErrorAction SilentlyContinue
$script:TuyaOpenIdeHost = $false

Write-Host "`n== Python install line parser =="
$state = @{
    Artifact    = 'cpython'
    TotalMiB    = 0.0
    ReceivedMiB = -1.0
    LastEmitAt  = [datetime]::MinValue
    LastPct     = -1
    LastText    = ''
}
$lines = Get-Content (Join-Path $Fixtures 'python_install_lines.txt')
foreach ($line in $lines) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    Update-TuyaPythonInstallFromUvLine -Line $line -Version '3.12.13' -State $state
}
if ($state.TotalMiB -gt 0 -and $state.ReceivedMiB -ge 0) {
    Pass 'python parser updated state from fixture'
} else {
    Fail 'python parser updated state' "recv=$($state.ReceivedMiB) total=$($state.TotalMiB)"
}

Write-Host "`n== Sync line parser =="
$progressState = New-TuyaUvSyncProgressState
$syncLines = Get-Content (Join-Path $Fixtures 'uv_sync_lines.txt')
foreach ($line in $syncLines) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    Update-TuyaUvSyncFromUvLine -Line $line -TotalPackages 10 -State $progressState
}
if ($progressState.Current -ge 1) {
    Pass "sync parser advanced count ($($progressState.Current))"
} else {
    Fail 'sync parser advanced count'
}

Write-Host "`n== CN tz range =="
foreach ($pair in @(
        @{ Offset = 450; Want = $true }
        @{ Offset = 480; Want = $true }
        @{ Offset = 510; Want = $true }
        @{ Offset = 449; Want = $false }
        @{ Offset = 511; Want = $false }
    )) {
    $got = Test-TuyaCnTzRange -Offset $pair.Offset
    if ($got -eq $pair.Want) {
        Pass "tz range offset=$($pair.Offset) -> $got"
    } else {
        Fail "tz range offset=$($pair.Offset)" "want $($pair.Want) got $got"
    }
}

Write-Host "`n== Core functions exist =="
foreach ($fn in @(
        'Invoke-TuyaExportSetupCore',
        'Invoke-TuyaExportFinalize',
        'Write-TuyaCmdEnvBat',
        'Register-TuyaOpenCommandHelpers',
        'Invoke-TuyaRegionDetect',
        'Get-TuyaUvDownloadUrls',
        'Test-TuyaCnTzRange'
    )) {
    if (Get-Command $fn -ErrorAction SilentlyContinue) {
        Pass "$fn defined"
    } else {
        Fail "$fn defined"
    }
}

Write-Host "`n== Summary =="
Write-Host "Pass: $($script:Pass)  Fail: $($script:Fail)"
if ($script:Fail -gt 0) { exit 1 }
exit 0
