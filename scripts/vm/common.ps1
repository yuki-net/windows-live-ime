Set-StrictMode -Version Latest

$script:DevVmDeploymentRoot = 'C:\windows-live-ime-dev'
$script:DevVmCredentialPath = Join-Path $env:LOCALAPPDATA 'windows-live-ime\vm-credential.xml'
$script:DevVmNotepadTaskName = 'WindowsLiveIME-Dev-Notepad'

function Import-DevVmHyperVModule {
    try {
        Import-Module Hyper-V -ErrorAction Stop
    } catch {
        throw 'Hyper-V PowerShell tools are unavailable. Enable Hyper-V and its PowerShell module on the Windows host.'
    }
    if (-not (Get-Command Get-VM -ErrorAction SilentlyContinue)) {
        throw 'The Hyper-V module loaded without Get-VM. Enable the Hyper-V management tools on the Windows host.'
    }
}

function Import-DevVmCredential {
    if (-not (Test-Path -LiteralPath $script:DevVmCredentialPath)) {
        throw "VM credentials are missing. Run scripts/vm/setup-credential.ps1 first."
    }
    $credential = Import-Clixml -LiteralPath $script:DevVmCredentialPath
    if ($credential -isnot [System.Management.Automation.PSCredential]) {
        throw 'The saved VM credential file is invalid. Run scripts/vm/setup-credential.ps1 again.'
    }
    return $credential
}

function Connect-DevVm {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$VMName,
        [ValidateRange(10, 600)]
        [int]$StartupTimeoutSeconds = 120,
        [ValidateRange(10, 600)]
        [int]$ConnectionTimeoutSeconds = 120
    )

    Import-DevVmHyperVModule
    $vm = Get-VM -Name $VMName -ErrorAction Stop
    if ($vm.State -ne 'Running') {
        Start-VM -Name $VMName -ErrorAction Stop | Out-Null
        $startupDeadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)
        do {
            Start-Sleep -Seconds 2
            $vm = Get-VM -Name $VMName -ErrorAction Stop
        } while ($vm.State -ne 'Running' -and (Get-Date) -lt $startupDeadline)
        if ($vm.State -ne 'Running') {
            throw "VM '$VMName' did not reach Running state before the startup timeout."
        }
    }

    $credential = Import-DevVmCredential
    $connectionDeadline = (Get-Date).AddSeconds($ConnectionTimeoutSeconds)
    $lastConnectionError = $null
    do {
        try {
            return New-PSSession -VMName $VMName -Credential $credential -ErrorAction Stop
        } catch {
            if ($null -eq $lastConnectionError) {
                Write-Warning ("PowerShell Direct connection failed: {0}" -f $_.Exception.Message)
            }
            $lastConnectionError = $_
            Start-Sleep -Seconds 2
        }
    } while ((Get-Date) -lt $connectionDeadline)

    $reason = $lastConnectionError.Exception.Message
    throw "PowerShell Direct could not connect to VM '$VMName' before the connection timeout. Last error: $reason. Check the saved credential against the administrator account inside the VM; run scripts/vm/setup-credential.ps1 to update it."
}

function Remove-DevVmSession([System.Management.Automation.Runspaces.PSSession]$Session) {
    if ($null -ne $Session) {
        Remove-PSSession -Session $Session -ErrorAction SilentlyContinue
    }
}

function Assert-DevVmBuildId([string]$BuildId) {
    if ($BuildId -notmatch '^\d{6}$') {
        throw "Build ID must contain exactly six decimal digits: '$BuildId'"
    }
}
