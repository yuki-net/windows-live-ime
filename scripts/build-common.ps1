Set-StrictMode -Version Latest

function Get-WindowsLiveImeRepositoryRoot {
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
}

function Add-DirectoryToPath([string]$Directory) {
    if ([string]::IsNullOrWhiteSpace($Directory) -or -not (Test-Path -LiteralPath $Directory)) {
        return
    }
    $pathEntries = $env:PATH -split ';'
    if ($pathEntries -notcontains $Directory) {
        $env:PATH = "$Directory;$env:PATH"
    }
}

function Get-VisualStudioWherePath {
    $fromPath = Get-Command vswhere.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $candidate = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }
    return $null
}

function Initialize-MsvcEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }

    $vswhere = Get-VisualStudioWherePath
    if (-not $vswhere) {
        throw 'MSVC was not found. Install Visual Studio 2022 C++ x64/x86 build tools.'
    }

    $installPathOutput = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vswhereExitCode = $LASTEXITCODE
    $installPath = $installPathOutput | Select-Object -First 1
    if ($vswhereExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($installPath)) {
        throw 'The MSVC x64/x86 workload is missing. Install Microsoft.VisualStudio.Component.VC.Tools.x86.x64.'
    }

    $developerCommand = Join-Path $installPath 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $developerCommand)) {
        throw "Visual Studio developer environment script was not found: $developerCommand"
    }

    $commandLine = 'call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand
    $environmentLines = & $env:ComSpec /d /c $commandLine
    $developerCommandExitCode = $LASTEXITCODE
    if ($developerCommandExitCode -ne 0) {
        throw 'VsDevCmd.bat could not initialize the x64 MSVC environment.'
    }

    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            [Environment]::SetEnvironmentVariable(
                $line.Substring(0, $separator),
                $line.Substring($separator + 1),
                'Process'
            )
        }
    }

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw 'MSVC was not available after VsDevCmd.bat initialized the environment.'
    }
}

function Get-ClionInstallations {
    $programsPath = Join-Path $env:LOCALAPPDATA 'Programs'
    if (-not (Test-Path -LiteralPath $programsPath)) {
        return @()
    }
    return @(Get-ChildItem -LiteralPath $programsPath -Directory -Filter 'CLion*' -ErrorAction SilentlyContinue)
}

function Resolve-CmakeExecutable {
    $fromPath = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $candidates = @()
    foreach ($installation in Get-ClionInstallations) {
        $candidates += Join-Path $installation.FullName 'bin\cmake\win\x64\bin\cmake.exe'
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw 'CMake 3.25 or newer was not found. Add CMake to PATH or install the CLion CMake tools.'
}

function Resolve-NinjaExecutable([string]$VisualStudioPath) {
    $fromPath = Get-Command ninja.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $candidates = @()
    foreach ($installation in Get-ClionInstallations) {
        $candidates += Join-Path $installation.FullName 'bin\ninja\win\x64\ninja.exe'
    }
    if (-not [string]::IsNullOrWhiteSpace($VisualStudioPath)) {
        $candidates += Join-Path $VisualStudioPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw 'Ninja was not found. Add Ninja to PATH or install the CLion/Visual Studio CMake tools.'
}

function Resolve-SwiftExecutable {
    $fromPath = Get-Command swift.exe -ErrorAction SilentlyContinue
    $swiftPath = if ($fromPath) { $fromPath.Source } else { $null }

    if (-not $swiftPath) {
        $toolchainRoot = Join-Path $env:LOCALAPPDATA 'Programs\Swift\Toolchains'
        if (Test-Path -LiteralPath $toolchainRoot) {
            $swiftPath = Get-ChildItem -LiteralPath $toolchainRoot -Directory -ErrorAction SilentlyContinue |
                ForEach-Object { Join-Path $_.FullName 'usr\bin\swift.exe' } |
                Where-Object { Test-Path -LiteralPath $_ } |
                Select-Object -First 1
        }
    }

    if (-not $swiftPath) {
        throw 'Swift for Windows 6.1 or newer is required. Install the official Windows toolchain from swift.org, then add swift.exe to PATH.'
    }

    $versionOutputLines = & $swiftPath --version 2>&1
    $swiftExitCode = $LASTEXITCODE
    $versionOutput = $versionOutputLines | Out-String
    if ($swiftExitCode -ne 0) {
        throw "Swift could not report its version: $versionOutput"
    }
    $match = [regex]::Match($versionOutput, 'Swift version\s+(\d+)\.(\d+)(?:\.(\d+))?')
    if (-not $match.Success) {
        throw "Could not parse Swift version: $versionOutput"
    }
    $patchVersion = if ($match.Groups[3].Success) { [int]$match.Groups[3].Value } else { 0 }
    $version = [version]::new([int]$match.Groups[1].Value, [int]$match.Groups[2].Value, $patchVersion)
    if ($version -lt [version]'6.1.0') {
        throw "Swift 6.1 or newer is required by the pinned AzooKey package; found Swift $version."
    }

    Add-DirectoryToPath (Split-Path -Parent $swiftPath)
    return $swiftPath
}

function Resolve-SwiftRuntimeDirectory([string]$SwiftExecutable) {
    $versionOutputLines = & $SwiftExecutable --version 2>&1
    $swiftExitCode = $LASTEXITCODE
    $versionOutput = $versionOutputLines | Out-String
    if ($swiftExitCode -ne 0) {
        throw "Swift could not report its version while locating runtime DLLs: $versionOutput"
    }
    $versionMatch = [regex]::Match($versionOutput, 'Swift version\s+(\d+)\.(\d+)(?:\.(\d+))?')
    if (-not $versionMatch.Success) {
        throw "Could not determine the Swift runtime version from: $versionOutput"
    }
    $patchVersion = if ($versionMatch.Groups[3].Success) { $versionMatch.Groups[3].Value } else { '0' }
    $swiftVersion = '{0}.{1}.{2}' -f $versionMatch.Groups[1].Value, $versionMatch.Groups[2].Value, $patchVersion

    $swiftBinDirectory = Split-Path -Parent $SwiftExecutable
    $swiftUsrDirectory = Split-Path -Parent $swiftBinDirectory
    $toolchainDirectory = Split-Path -Parent $swiftUsrDirectory
    $swiftInstallRoots = @(
        (Split-Path -Parent (Split-Path -Parent $toolchainDirectory)),
        (Join-Path $env:LOCALAPPDATA 'Programs\Swift'),
        (Join-Path ${env:ProgramFiles} 'Swift')
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique

    $candidateDirectories = @($swiftBinDirectory)
    foreach ($installRoot in $swiftInstallRoots) {
        $runtimeRoot = Join-Path $installRoot 'Runtimes'
        $versionDirectories = Get-ChildItem -LiteralPath $runtimeRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match ('^{0}(\+.*)?$' -f [regex]::Escape($swiftVersion)) }
        foreach ($versionDirectory in $versionDirectories) {
            $candidateDirectories += Join-Path $versionDirectory.FullName 'usr\bin'
        }

        $candidateDirectories += Join-Path $installRoot 'runtime-development\usr\bin'
    }

    foreach ($candidate in ($candidateDirectories | Select-Object -Unique)) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'swiftCore.dll')) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Could not locate Swift $swiftVersion runtime DLLs. Expected swiftCore.dll beside swift.exe or under Swift\\Runtimes\\$swiftVersion\\usr\\bin."
}

function Resolve-SwiftSdkDirectory([string]$SwiftExecutable) {
    $sdkCandidates = @()
    if (-not [string]::IsNullOrWhiteSpace($env:SDKROOT)) {
        $sdkCandidates += $env:SDKROOT
    }

    $swiftBinDirectory = Split-Path -Parent $SwiftExecutable
    $swiftUsrDirectory = Split-Path -Parent $swiftBinDirectory
    $toolchainDirectory = Split-Path -Parent $swiftUsrDirectory
    $swiftInstallRoots = @(
        (Split-Path -Parent (Split-Path -Parent $toolchainDirectory)),
        (Join-Path $env:LOCALAPPDATA 'Programs\Swift'),
        (Join-Path ${env:ProgramFiles} 'Swift')
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique

    foreach ($installRoot in $swiftInstallRoots) {
        $platformsDirectory = Join-Path $installRoot 'Platforms'
        if (-not (Test-Path -LiteralPath $platformsDirectory)) {
            continue
        }
        $sdkCandidates += Get-ChildItem -LiteralPath $platformsDirectory -Directory -Recurse -Filter 'Windows.sdk' -ErrorAction SilentlyContinue |
            ForEach-Object { $_.FullName }
    }

    foreach ($candidate in ($sdkCandidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'usr\lib\swift\windows')) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw 'The Swift Windows SDK was not found. Install the Swift for Windows SDK or set SDKROOT to a Windows.sdk directory.'
}

function Resolve-MsvcRuntimeDirectory([string]$VisualStudioPath) {
    $candidateDirectories = @()
    if (-not [string]::IsNullOrWhiteSpace($env:VCToolsRedistDir)) {
        $candidateDirectories += Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC143.CRT'
    }
    if (-not [string]::IsNullOrWhiteSpace($VisualStudioPath)) {
        $redistRoot = Join-Path $VisualStudioPath 'VC\Redist\MSVC'
        $versionDirectories = Get-ChildItem -LiteralPath $redistRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
        foreach ($versionDirectory in $versionDirectories) {
            $candidateDirectories += Join-Path $versionDirectory.FullName 'x64\Microsoft.VC143.CRT'
        }
    }

    foreach ($candidate in ($candidateDirectories | Select-Object -Unique)) {
        if ((Test-Path -LiteralPath (Join-Path $candidate 'vcruntime140.dll')) -and
            (Test-Path -LiteralPath (Join-Path $candidate 'msvcp140.dll'))) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'The x64 Visual C++ runtime redistributables were not found. Install the MSVC x64/x86 build tools with the VC++ redistributable files.'
}

function Initialize-WindowsLiveImeBuildTools {
    Initialize-MsvcEnvironment
    $vswhere = Get-VisualStudioWherePath
    $visualStudioPath = $null
    if ($vswhere) {
        $visualStudioPathOutput = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $vswhereExitCode = $LASTEXITCODE
        $visualStudioPath = $visualStudioPathOutput | Select-Object -First 1
        if ($vswhereExitCode -ne 0) {
            throw 'vswhere could not locate the Visual Studio installation.'
        }
    }

    $cmake = Resolve-CmakeExecutable
    $ninja = Resolve-NinjaExecutable $visualStudioPath
    $swift = Resolve-SwiftExecutable
    $swiftRuntime = Resolve-SwiftRuntimeDirectory $swift
    $swiftSdk = Resolve-SwiftSdkDirectory $swift
    $msvcRuntime = Resolve-MsvcRuntimeDirectory $visualStudioPath
    Add-DirectoryToPath (Split-Path -Parent $cmake)
    Add-DirectoryToPath (Split-Path -Parent $ninja)
    Add-DirectoryToPath $swiftRuntime
    $env:SDKROOT = $swiftSdk

    $cmakeVersionLines = & $cmake --version 2>&1
    $cmakeExitCode = $LASTEXITCODE
    $cmakeVersionOutput = $cmakeVersionLines | Select-Object -First 1
    if ($cmakeExitCode -ne 0) {
        throw 'CMake could not report its version.'
    }
    $cmakeMatch = [regex]::Match($cmakeVersionOutput, 'cmake version\s+(\d+)\.(\d+)\.(\d+)')
    if (-not $cmakeMatch.Success -or [version]::new([int]$cmakeMatch.Groups[1].Value, [int]$cmakeMatch.Groups[2].Value, [int]$cmakeMatch.Groups[3].Value) -lt [version]'3.25.0') {
        throw "CMake 3.25 or newer is required for the shared presets; found '$cmakeVersionOutput'."
    }

    return [pscustomobject]@{
        CMake = $cmake
        Ninja = $ninja
        Swift = $swift
        SwiftSdk = $swiftSdk
        SwiftRuntime = $swiftRuntime
        MsvcRuntime = $msvcRuntime
        VisualStudio = $visualStudioPath
    }
}

function Invoke-CheckedCommand([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $Executable $($Arguments -join ' ')"
    }
}
