[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev',
    [string]$IsoPath
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($IsoPath)) {
    $IsoPath = Join-Path $PSScriptRoot '..\..\build\vm\Windows11-ja-x64-noprompt.iso'
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an Administrator PowerShell window.'
}

$iso = (Resolve-Path -LiteralPath $IsoPath).ProviderPath
$logPath = Join-Path $PSScriptRoot '..\..\build\vm\boot-diagnostic.log'
Start-Transcript -Path $logPath -Force | Out-Null
try {
    Import-Module Hyper-V -ErrorAction Stop
    $vm = Get-VM -Name $VMName
    if ($vm.Generation -ne 2) { throw 'This script requires a Generation 2 VM.' }
    $dvd = @(Get-VMDvdDrive -VMName $VMName)
    if ($dvd.Count -ne 1) { throw 'Expected exactly one virtual DVD drive.' }

    Write-Host 'Before switching to the no-keypress ISO:'
    Get-VMFirmware -VMName $VMName | Format-List SecureBoot,SecureBootTemplate
    $dvd | Format-List Path,ControllerNumber,ControllerLocation

    if ($vm.State -ne 'Off') {
        Stop-VM -Name $VMName -TurnOff -Confirm:$false
    }
    Set-VMDvdDrive -VMDvdDrive $dvd[0] -Path $iso
    Set-VMFirmware -VMName $VMName -FirstBootDevice $dvd[0]
    Start-VM -Name $VMName
    Get-VM -Name $VMName | Format-List Name,State,Generation,Status
    Get-VMDvdDrive -VMName $VMName | Format-List Path
    Write-Host 'The VM is running with the no-keypress ISO. Windows Setup is not yet verified.'
    Write-Host 'After Windows Setup first restarts, detach this ISO to boot the installed Windows.'
    $connectionTool = Join-Path $env:WINDIR 'System32\vmconnect.exe'
    if (Test-Path -LiteralPath $connectionTool) {
        Start-Process -FilePath $connectionTool -ArgumentList @('localhost', $VMName)
    }
} finally {
    Stop-Transcript | Out-Null
}
