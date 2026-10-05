[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev',
    [switch]$IgnoreMissing
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$session = Connect-DevVm -VMName $VMName
try {
    $buildId = Invoke-Command -Session $session -ArgumentList $script:DevVmDeploymentRoot, $IgnoreMissing.IsPresent -ScriptBlock {
        param($DeploymentRoot, $IgnoreMissing)
        $activeBuildFile = Join-Path $DeploymentRoot 'active-build.txt'
        if (-not (Test-Path -LiteralPath $activeBuildFile)) {
            if ($IgnoreMissing) {
                return $null
            }
            throw 'No active Windows Live IME build is registered in the VM.'
        }

        $buildId = [IO.File]::ReadAllText($activeBuildFile).Trim()
        if ($buildId -notmatch '^\d{6}$') {
            throw 'The VM active-build marker is invalid; refusing to unregister an unknown DLL.'
        }
        $dllPath = Join-Path (Join-Path (Join-Path $DeploymentRoot 'builds') $buildId) 'windows-live-ime.dll'
        if (-not (Test-Path -LiteralPath $dllPath)) {
            throw "The active TSF DLL is missing: $dllPath"
        }

        $regsvr32 = Join-Path $env:WINDIR 'System32\regsvr32.exe'
        $arguments = '/u /s "{0}"' -f $dllPath
        $process = Start-Process -FilePath $regsvr32 -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
        if ($process.ExitCode -ne 0) {
            throw "regsvr32 failed to unregister build $buildId (exit $($process.ExitCode))."
        }
        Remove-Item -LiteralPath $activeBuildFile -Force
        return $buildId
    }
    if ($buildId) {
        Write-Host "Unregistered VM build $buildId."
    } else {
        Write-Host 'The VM has no active build registration.'
    }
} finally {
    Remove-DevVmSession $session
}
