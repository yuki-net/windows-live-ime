[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev',
    [string]$CheckpointName
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$session = Connect-DevVm -VMName $VMName
try {
    Invoke-Command -Session $session -ArgumentList $script:DevVmDeploymentRoot -ScriptBlock {
        param($DeploymentRoot)
        $buildsRoot = Join-Path $DeploymentRoot 'builds'
        $activeBuildFile = Join-Path $DeploymentRoot 'active-build.txt'
        if (Test-Path -LiteralPath $activeBuildFile) {
            $buildId = [IO.File]::ReadAllText($activeBuildFile).Trim()
            if ($buildId -notmatch '^\d{6}$') {
                throw 'The VM active-build marker is invalid; refusing to unregister an unknown DLL.'
            }
            $dllPath = Join-Path (Join-Path $buildsRoot $buildId) 'windows-live-ime.dll'
            if (Test-Path -LiteralPath $dllPath) {
                $regsvr32 = Join-Path $env:WINDIR 'System32\regsvr32.exe'
                $process = Start-Process -FilePath $regsvr32 -ArgumentList ('/u /s "{0}"' -f $dllPath) -Wait -PassThru -WindowStyle Hidden
                if ($process.ExitCode -ne 0) {
                    throw "regsvr32 failed to unregister build $buildId (exit $($process.ExitCode))."
                }
            }
            Remove-Item -LiteralPath $activeBuildFile -Force
        }

        $oldHosts = Get-CimInstance -ClassName Win32_Process -Filter "Name='engine-host.exe'" |
            Where-Object { $_.ExecutablePath -like "$buildsRoot\*\engine-host.exe" }
        foreach ($oldHost in $oldHosts) {
            Stop-Process -Id $oldHost.ProcessId -Force -ErrorAction SilentlyContinue
        }
    }
} finally {
    Remove-DevVmSession $session
}

Import-DevVmHyperVModule
Stop-VM -Name $VMName -Shutdown -Confirm:$false -ErrorAction Stop
$shutdownDeadline = (Get-Date).AddMinutes(2)
do {
    Start-Sleep -Seconds 2
    $vm = Get-VM -Name $VMName -ErrorAction Stop
} while ($vm.State -ne 'Off' -and (Get-Date) -lt $shutdownDeadline)
if ($vm.State -ne 'Off') {
    throw "VM '$VMName' did not shut down before the timeout."
}

if (-not [string]::IsNullOrWhiteSpace($CheckpointName)) {
    $checkpoint = Get-VMSnapshot -VMName $VMName -Name $CheckpointName -ErrorAction Stop
    Restore-VMSnapshot -VMSnapshot $checkpoint -Confirm:$false -ErrorAction Stop
    Write-Host "Restored VM '$VMName' to checkpoint '$CheckpointName'."
} else {
    Write-Host "Unregistered the active IME and shut down VM '$VMName'. Deployed generations were retained."
}
