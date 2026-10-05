[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'build-settings.ps1') -Configuration $Configuration
if (-not $?) { throw 'Settings app build failed.' }
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe=Join-Path $repoRoot "build\settings\$Configuration\LiveImeSettings.exe"
Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
