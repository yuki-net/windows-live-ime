[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$session = Connect-DevVm -VMName $VMName
try {
    Invoke-Command -Session $session -ArgumentList $script:DevVmNotepadTaskName, $script:DevVmDeploymentRoot -ScriptBlock {
        param($TaskName, $DeploymentRoot)
        $activeId = [IO.File]::ReadAllText((Join-Path $DeploymentRoot 'active-build.txt')).Trim()
        $expectedDll = Join-Path (Join-Path (Join-Path $DeploymentRoot 'builds') $activeId) 'windows-live-ime.dll'
        foreach ($app in @(Get-Process -Name notepad,explorer,ctfmon -ErrorAction SilentlyContinue)) {
            foreach ($module in @($app.Modules | Where-Object ModuleName -eq 'windows-live-ime.dll')) {
                if ($module.FileName -ne $expectedDll) {
                    throw "$($app.ProcessName) PID $($app.Id) is still using an older IME: $($module.FileName). Save your documents and restart Windows inside the VM, then run Dev IME again. Registered build is $activeId."
                }
            }
        }
        $task = Get-ScheduledTask -TaskName $TaskName -ErrorAction Stop
        Start-ScheduledTask -InputObject $task -ErrorAction Stop
    }
} finally {
    Remove-DevVmSession $session
}
Write-Host 'Started the VM Notepad task in the interactive user session.'
