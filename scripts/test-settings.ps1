[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release', [switch]$NoBuild)
$ErrorActionPreference='Stop'
if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot 'build-settings.ps1') -Configuration $Configuration
    if (-not $?) { throw 'Settings build failed.' }
}
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$directory=Join-Path $repoRoot "build\settings\$Configuration"
$exe=Join-Path $directory 'LiveImeSettings.exe'
$report=Join-Path $directory 'smoke-test.json'
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }
$expected=Get-Content -LiteralPath (Join-Path $directory 'build-info.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$originalPath=$env:PATH
try {
    # The app must start from its distribution folder without developer paths.
    $env:PATH="$(Join-Path $env:WINDIR 'System32');$env:WINDIR"
    $process=Start-Process -FilePath $exe -ArgumentList '--smoke-test' -WorkingDirectory $directory -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(30000)) {
        Stop-Process -Id $process.Id -Force
        throw 'Settings smoke run timed out.'
    }
    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) {
        throw "Settings smoke run failed (exit $($process.ExitCode)). See $directory\startup-error.log if present."
    }
    $actual=Get-Content -LiteralPath $report -Raw -Encoding UTF8 | ConvertFrom-Json
    if (-not $actual.winuiWindowCreated -or -not $actual.shortcutSettingsSaved -or $actual.version -ne $expected.version -or $actual.commit -ne $expected.commit -or
        -not $actual.displayedVersion.Contains($expected.version) -or -not $actual.displayedVersion.Contains($expected.commit)) {
        throw 'The WinUI version display does not match build-info.json.'
    }
    Write-Host "WinUI window and version display verified: $($actual.displayedVersion)"
} finally { $env:PATH=$originalPath }
