[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release', [switch]$NoRestore, [switch]$Restore)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')
$repoRoot=Get-WindowsLiveImeRepositoryRoot
$vswhere=Get-VisualStudioWherePath
if (-not $vswhere) { throw 'Visual Studio C++ build tools are required for WinUI 3.' }
$vsPath=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
$msbuild=Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
$nuget=Join-Path $repoRoot 'build\tools\nuget.exe'
$packageRoot=Join-Path $repoRoot 'build\nuget'
$projectRoot=Join-Path $repoRoot 'platform\windows\settings'
$buildRoot=Join-Path $repoRoot 'build\settings'
New-Item -ItemType Directory -Force -Path (Split-Path $nuget),$packageRoot,$buildRoot | Out-Null
[xml]$packages=Get-Content (Join-Path $projectRoot 'packages.config')
$missingPackages=@($packages.packages.package | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $packageRoot "$($_.id).$($_.version)"))
})
if ($Restore -and $NoRestore) { throw 'Use either -Restore or -NoRestore.' }
if (-not $NoRestore -and ($Restore -or $missingPackages.Count -gt 0)) {
    if (-not (Test-Path $nuget)) {
        Invoke-WebRequest 'https://dist.nuget.org/win-x86-commandline/v6.14.0/nuget.exe' -OutFile $nuget
    }
    Invoke-CheckedCommand $nuget @('restore',(Join-Path $projectRoot 'packages.config'),'-PackagesDirectory',$packageRoot,'-Source','https://api.nuget.org/v3/index.json','-NonInteractive')
}
foreach ($extension in @('props','targets')) {
    $imports=@('<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')
    foreach ($package in $packages.packages.package) {
        $directory=Join-Path $packageRoot "$($package.id).$($package.version)"
        if (-not (Test-Path $directory)) { throw "Missing NuGet dependency: $($package.id). Run without -NoRestore." }
        $import=Join-Path $directory "build\native\$($package.id).$extension"
        if (Test-Path $import) {
            $escaped=[Security.SecurityElement]::Escape($import)
            $imports += "  <Import Project=`"$escaped`" />"
        }
    }
    $imports+='</Project>'
    $importFile=Join-Path $buildRoot "nuget.$extension"
    $importText=($imports -join [Environment]::NewLine)+[Environment]::NewLine
    if (-not (Test-Path -LiteralPath $importFile) -or [IO.File]::ReadAllText($importFile) -ne $importText) {
        [IO.File]::WriteAllText($importFile,$importText,[Text.UTF8Encoding]::new($false))
    }
}
# Windows App SDK uses Roslyn for inline MSBuild tasks even for C++ apps.
# Keep that compiler in the build directory instead of changing Visual Studio.
$originalCompiler=$env:CscToolExe
$originalLibraryPath=$env:LIB
$env:LIB=(@($env:LIB -split ';' | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Container) }) -join ';')
$env:CscToolExe=Join-Path $packageRoot 'Microsoft.Net.Compilers.Toolset.4.14.0\tasks\net472\csc.exe'
try {
    Invoke-CheckedCommand $msbuild @((Join-Path $projectRoot 'LiveImeSettings.vcxproj'),'/m','/v:minimal',"/p:Configuration=$Configuration",'/p:Platform=x64')
} finally { $env:CscToolExe=$originalCompiler; $env:LIB=$originalLibraryPath }
$output=Join-Path $buildRoot $Configuration
# An unpackaged programmatic WinUI app still needs an application PRI that
# merges the runtime's resource maps. Index only PRIs, not the same asset files
# again, so embedded and file-backed resource candidates do not collide.
$priInput=Join-Path $buildRoot "pri-input\$Configuration"
New-Item -ItemType Directory -Force -Path $priInput | Out-Null
foreach ($name in @('Microsoft.UI.pri','Microsoft.UI.Xaml.Controls.pri','Microsoft.WindowsAppRuntime.pri')) {
    Copy-Item -LiteralPath (Join-Path $output $name) -Destination $priInput -Force
}
$makepri=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.26100.0\x64\makepri.exe'
$mergedPri=Join-Path $buildRoot "$Configuration.resources.pri"
$priLog=Join-Path $buildRoot "$Configuration.makepri.log"
& $makepri new /pr $priInput /cf (Join-Path $projectRoot 'priconfig.xml') /of $mergedPri /in LiveImeSettings /o *> $priLog
if ($LASTEXITCODE -ne 0) { throw "WinUI resource indexing failed. See $priLog" }
Copy-Item -LiteralPath $mergedPri -Destination (Join-Path $output 'resources.pri') -Force
$msvcRuntime=Resolve-MsvcRuntimeDirectory $vsPath
Get-ChildItem -LiteralPath $msvcRuntime -File -Filter '*.dll' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $output -Force
}
& (Join-Path $PSScriptRoot 'write-build-info.ps1') -Destination $output
Write-Host "Settings app: $(Join-Path $output 'LiveImeSettings.exe')"
