[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$session = Connect-DevVm -VMName $VMName
try {
    Invoke-Command -Session $session -ArgumentList $script:DevVmDeploymentRoot, $script:DevVmNotepadTaskName -ScriptBlock {
        param($DeploymentRoot, $NotepadTaskName)

        $os = Get-CimInstance -ClassName Win32_OperatingSystem -ErrorAction Stop
        if ($os.Caption -notlike '*Windows 11*') {
            throw "The development VM must run Windows 11; found '$($os.Caption)'."
        }
        if (-not [Environment]::Is64BitOperatingSystem) {
            throw 'The development VM must be 64-bit Windows.'
        }
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = New-Object Security.Principal.WindowsPrincipal($identity)
        if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
            throw 'PowerShell Direct must use a VM account in the local Administrators group to register the TSF DLL.'
        }

        $buildRoot = Join-Path $DeploymentRoot 'builds'
        New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null

        $interactiveUser = (Get-CimInstance -ClassName Win32_ComputerSystem).UserName
        if ([string]::IsNullOrWhiteSpace($interactiveUser)) {
            $interactiveUser = $identity.Name
        }
        $notepadPath = Join-Path $env:WINDIR 'System32\notepad.exe'
        if (-not (Test-Path -LiteralPath $notepadPath)) {
            throw 'Notepad was not found in the Windows guest.'
        }

        $action = New-ScheduledTaskAction -Execute $notepadPath
        $trigger = New-ScheduledTaskTrigger -AtLogOn -User $interactiveUser
        $taskPrincipal = New-ScheduledTaskPrincipal -UserId $interactiveUser -LogonType Interactive -RunLevel Limited
        $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
        $task = New-ScheduledTask -Action $action -Trigger $trigger -Principal $taskPrincipal -Settings $settings
        Register-ScheduledTask -TaskName $NotepadTaskName -InputObject $task -Force | Out-Null

        [pscustomobject]@{
            Guest = $os.Caption
            TestAppTask = $NotepadTaskName
            DeploymentRoot = $DeploymentRoot
        }
    }
} finally {
    Remove-DevVmSession $session
}

Write-Host "Development VM '$VMName' is ready."
