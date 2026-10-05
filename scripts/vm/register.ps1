[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev',
    [Parameter(Mandatory = $true)]
    [string]$BuildId,
    [ValidateRange(1, 100)]
    [int]$KeepBuildCount = 5
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
Assert-DevVmBuildId $BuildId

$session = Connect-DevVm -VMName $VMName
try {
    Invoke-Command -Session $session -ArgumentList $script:DevVmDeploymentRoot, $BuildId, $KeepBuildCount -ScriptBlock {
        param($DeploymentRoot, $BuildId, $KeepBuildCount)
        $activeBuildFile = Join-Path $DeploymentRoot 'active-build.txt'
        if (Test-Path -LiteralPath $activeBuildFile) {
            $activeId = [IO.File]::ReadAllText($activeBuildFile).Trim()
            throw "Build $activeId is still registered. Unregister it before registering build $BuildId."
        }

        $buildDirectory = Join-Path (Join-Path $DeploymentRoot 'builds') $BuildId
        $dllPath = Join-Path $buildDirectory 'windows-live-ime.dll'
        if (-not (Test-Path -LiteralPath $dllPath)) {
            throw "The TSF DLL is missing from deployed build $BuildId."
        }

        $regsvr32 = Join-Path $env:WINDIR 'System32\regsvr32.exe'
        $arguments = '/s "{0}"' -f $dllPath
        $process = Start-Process -FilePath $regsvr32 -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
        if ($process.ExitCode -ne 0) {
            throw "regsvr32 failed to register build $BuildId (exit $($process.ExitCode))."
        }

        $temporaryMarker = Join-Path $DeploymentRoot 'active-build.txt.tmp'
        [IO.File]::WriteAllText($temporaryMarker, $BuildId, [Text.UTF8Encoding]::new($false))
        Move-Item -LiteralPath $temporaryMarker -Destination $activeBuildFile -Force

        $settingsExe = Join-Path $buildDirectory 'settings\LiveImeSettings.exe'
        if (Test-Path -LiteralPath $settingsExe) {
            $desktop = [Environment]::GetFolderPath('DesktopDirectory')
            if (-not [string]::IsNullOrWhiteSpace($desktop)) {
                New-Item -ItemType Directory -Force -Path $desktop | Out-Null
                $shell = New-Object -ComObject WScript.Shell
                $shortcut = $null
                try {
                    # Keep PowerShell source ASCII: Windows PowerShell 5.1 reads
                    # UTF-8 without a BOM as the system ANSI code page.
                    $shortcutName = 'Live IME ' + [char]0x8A2D + [char]0x5B9A + '.lnk'
                    $shortcut = $shell.CreateShortcut((Join-Path $desktop $shortcutName))
                    $shortcut.TargetPath = $settingsExe
                    $shortcut.WorkingDirectory = Split-Path $settingsExe
                    $shortcut.Description = "Live IME settings, development build $BuildId"
                    $shortcut.Save()
                } finally {
                    if ($null -ne $shortcut) {
                        [Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcut) | Out-Null
                    }
                    [Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell) | Out-Null
                }
            }
        }

        $oldBuilds = Get-ChildItem -LiteralPath (Join-Path $DeploymentRoot 'builds') -Directory |
            Where-Object { $_.Name -match '^\d{6}$' } |
            Sort-Object Name -Descending |
            Select-Object -Skip $KeepBuildCount
        foreach ($oldBuild in $oldBuilds) {
            try {
                Remove-Item -LiteralPath $oldBuild.FullName -Recurse -Force -ErrorAction Stop
            } catch {
                Write-Verbose "Retained locked old build $($oldBuild.Name)."
            }
        }
    }
    Write-Host "Registered VM build $BuildId."
} finally {
    Remove-DevVmSession $session
}
