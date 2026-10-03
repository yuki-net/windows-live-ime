[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$session = Connect-DevVm -VMName $VMName
try {
    Invoke-Command -Session $session -ArgumentList $script:DevVmNotepadTaskName -ScriptBlock {
        param($TaskName)
        $task = Get-ScheduledTask -TaskName $TaskName -ErrorAction Stop
        Start-ScheduledTask -InputObject $task -ErrorAction Stop
    }
} finally {
    Remove-DevVmSession $session
}
Write-Host 'Started the VM Notepad task in the interactive user session.'
