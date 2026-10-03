[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev',
    [Parameter(Mandatory = $true)]
    [string]$BuildId,
    [ValidateRange(10, 300)]
    [int]$ReadyTimeoutSeconds = 45
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
Assert-DevVmBuildId $BuildId

$session = Connect-DevVm -VMName $VMName
try {
    Invoke-Command -Session $session -ArgumentList $script:DevVmDeploymentRoot, $BuildId, $ReadyTimeoutSeconds -ScriptBlock {
        param($DeploymentRoot, $BuildId, $ReadyTimeoutSeconds)
        $buildDirectory = Join-Path (Join-Path $DeploymentRoot 'builds') $BuildId
        $hostPath = Join-Path $buildDirectory 'engine-host.exe'
        $pingPath = Join-Path $buildDirectory 'ime_ipc_ping.exe'
        if (-not (Test-Path -LiteralPath $hostPath) -or -not (Test-Path -LiteralPath $pingPath)) {
            throw "Engine host or IPC health client is missing from build $BuildId."
        }

        $oldHosts = Get-CimInstance -ClassName Win32_Process -Filter "Name='engine-host.exe'" |
            Where-Object { $_.ExecutablePath -like "$(Join-Path $DeploymentRoot 'builds')\*\engine-host.exe" }
        foreach ($oldHost in $oldHosts) {
            Stop-Process -Id $oldHost.ProcessId -Force -ErrorAction SilentlyContinue
        }

        $process = Start-Process -FilePath $hostPath -WorkingDirectory $buildDirectory -WindowStyle Hidden -PassThru
        $deadline = (Get-Date).AddSeconds($ReadyTimeoutSeconds)
        do {
            & $pingPath -TimeoutMs 1000 1>$null 2>$null
            if ($LASTEXITCODE -eq 0) {
                return $process.Id
            }
            if (-not (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)) {
                throw "Engine host exited before its Named Pipe health check succeeded."
            }
            Start-Sleep -Milliseconds 500
        } while ((Get-Date) -lt $deadline)

        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "Engine host did not answer a Named Pipe ping within $ReadyTimeoutSeconds seconds."
    }
} finally {
    Remove-DevVmSession $session
}
Write-Host "Engine host build $BuildId is running and answered a Named Pipe ping."
