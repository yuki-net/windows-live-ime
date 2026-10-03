[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Debug',
    [switch]$CoreOnly
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')

$repoRoot = Get-WindowsLiveImeRepositoryRoot
$tools = Initialize-WindowsLiveImeBuildTools
$preset = if ($CoreOnly) { 'core-only' } else { 'windows-x64' }
$cmakeBuildDirectory = if ($CoreOnly) {
    Join-Path $repoRoot 'build\core'
} else {
    Join-Path $repoRoot 'build\windows'
}

Push-Location $repoRoot
try {
    Invoke-CheckedCommand $tools.CMake @('--preset', $preset)
    Invoke-CheckedCommand $tools.CMake @('--build', $cmakeBuildDirectory, '--config', $Configuration, '--parallel')

    $swiftConfiguration = if ($Configuration -eq 'Debug') { 'debug' } else { 'release' }
    $packagePath = Join-Path $repoRoot 'engine\azookey'
    Invoke-CheckedCommand $tools.Swift @('build', '--package-path', $packagePath, '--configuration', $swiftConfiguration)
    $swiftBinOutput = & $tools.Swift build --package-path $packagePath --configuration $swiftConfiguration --show-bin-path
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($swiftBinOutput)) {
        throw 'SwiftPM did not return the engine host output directory.'
    }
    $swiftBinDirectory = ($swiftBinOutput | Select-Object -Last 1).Trim()

    $artifactDirectory = Join-Path $repoRoot "build\artifacts\$Configuration"
    New-Item -ItemType Directory -Force -Path $artifactDirectory | Out-Null
    $cmakeArtifacts = Join-Path $cmakeBuildDirectory "artifacts\$Configuration"
    if (Test-Path -LiteralPath $cmakeArtifacts) {
        Get-ChildItem -LiteralPath $cmakeArtifacts -Force | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $artifactDirectory -Recurse -Force
        }
    }

    if (-not (Test-Path -LiteralPath $swiftBinDirectory)) {
        throw "SwiftPM output directory does not exist: $swiftBinDirectory"
    }
    Get-ChildItem -LiteralPath $swiftBinDirectory -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $artifactDirectory -Recurse -Force
    }

    $swiftRuntimeDlls = @(Get-ChildItem -LiteralPath $tools.SwiftRuntime -File -Filter '*.dll')
    if ($swiftRuntimeDlls.Name -notcontains 'swiftCore.dll') {
        throw "The Swift runtime directory does not contain swiftCore.dll: $($tools.SwiftRuntime)"
    }
    foreach ($runtimeDll in $swiftRuntimeDlls) {
        Copy-Item -LiteralPath $runtimeDll.FullName -Destination $artifactDirectory -Force
    }

    $msvcRuntimeFiles = Get-ChildItem -LiteralPath $tools.MsvcRuntime -File |
        Where-Object { $_.Extension -in @('.dll', '.manifest') }
    foreach ($runtimeFile in $msvcRuntimeFiles) {
        Copy-Item -LiteralPath $runtimeFile.FullName -Destination $artifactDirectory -Force
    }

    foreach ($requiredRuntimeDll in @('swiftCore.dll', 'vcruntime140.dll', 'msvcp140.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $artifactDirectory $requiredRuntimeDll))) {
            throw "The deployment runtime is missing '$requiredRuntimeDll': $artifactDirectory"
        }
    }

    $engineHost = Join-Path $artifactDirectory 'engine-host.exe'
    $ipcPing = Join-Path $artifactDirectory 'ime_ipc_ping.exe'
    if (-not (Test-Path -LiteralPath $engineHost)) {
        throw "Swift engine host was not produced: $engineHost"
    }
    if (-not (Test-Path -LiteralPath $ipcPing)) {
        throw "Named Pipe diagnostic client was not produced: $ipcPing"
    }
    if (-not $CoreOnly -and -not (Test-Path -LiteralPath (Join-Path $artifactDirectory 'windows-live-ime.dll'))) {
        throw 'The TSF DLL was not produced by the full Windows build.'
    }

    Write-Host "Build artifacts with Swift and MSVC runtime files: $artifactDirectory"
}
finally {
    Pop-Location
}
