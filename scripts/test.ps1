[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Debug',
    [switch]$CoreOnly,
    [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')

$repoRoot = Get-WindowsLiveImeRepositoryRoot
if (-not $NoBuild) {
    $buildScript = Join-Path $PSScriptRoot 'build.ps1'
    $buildArguments = @('-Configuration', $Configuration)
    if ($CoreOnly) {
        $buildArguments += '-CoreOnly'
    }
    & $buildScript @buildArguments
    if (-not $?) {
        throw 'Build script failed.'
    }
}

$tools = Initialize-WindowsLiveImeBuildTools
$cmakeBuildDirectory = if ($CoreOnly) {
    Join-Path $repoRoot 'build\core'
} else {
    Join-Path $repoRoot 'build\windows'
}
$artifactDirectory = Join-Path $repoRoot "build\artifacts\$Configuration"
$ctest = Join-Path (Split-Path -Parent $tools.CMake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctest)) {
    $ctestCommand = Get-Command ctest.exe -ErrorAction SilentlyContinue
    if ($ctestCommand) {
        $ctest = $ctestCommand.Source
    } else {
        throw 'CTest was not found next to CMake or on PATH.'
    }
}

Invoke-CheckedCommand $ctest @('--test-dir', $cmakeBuildDirectory, '-C', $Configuration, '--output-on-failure')

$packagePath = Join-Path $repoRoot 'engine\azookey'
$swiftConfiguration = if ($Configuration -eq 'Debug') { 'debug' } else { 'release' }
Invoke-CheckedCommand $tools.Swift @('test', '--package-path', $packagePath, '--configuration', $swiftConfiguration)

$engineHost = Join-Path $artifactDirectory 'engine-host.exe'
if (-not (Test-Path -LiteralPath $engineHost)) {
    throw "Engine host was not found at $engineHost. Run scripts/build.ps1 first."
}

$originalPath = $env:PATH
$runtimeDirectories = @($tools.SwiftRuntime, $tools.MsvcRuntime, (Split-Path -Parent $tools.Swift))
$excludedPaths = @{}
foreach ($runtimeDirectory in $runtimeDirectories) {
    $excludedPaths[[IO.Path]::GetFullPath($runtimeDirectory).TrimEnd('\')] = $true
}
$env:PATH = (($originalPath -split ';' | Where-Object {
    $entry = $_.Trim().Trim('"')
    if ([string]::IsNullOrWhiteSpace($entry)) { return $false }
    try {
        -not $excludedPaths.ContainsKey([IO.Path]::GetFullPath($entry).TrimEnd('\'))
    } catch {
        $true
    }
}) -join ';')
try {
    Invoke-CheckedCommand $engineHost @('--self-check')
} finally {
    $env:PATH = $originalPath
}
