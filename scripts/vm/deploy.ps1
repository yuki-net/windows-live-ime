[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev',
    [Parameter(Mandatory = $true)]
    [string]$ArtifactDirectory
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$resolvedArtifacts = (Resolve-Path -LiteralPath $ArtifactDirectory).Path
foreach ($requiredFile in @(
    'windows-live-ime.dll',
    'engine-host.exe',
    'ime_ipc_ping.exe',
    'swiftCore.dll',
    'vcruntime140.dll',
    'msvcp140.dll'
)) {
    if (-not (Test-Path -LiteralPath (Join-Path $resolvedArtifacts $requiredFile))) {
        throw "Deployment artifact is missing '$requiredFile': $resolvedArtifacts"
    }
}

$session = Connect-DevVm -VMName $VMName
try {
    $deployment = Invoke-Command -Session $session -ArgumentList $script:DevVmDeploymentRoot -ScriptBlock {
        param($DeploymentRoot)
        $buildRoot = Join-Path $DeploymentRoot 'builds'
        New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
        $existingIds = Get-ChildItem -LiteralPath $buildRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d{6}$' } |
            ForEach-Object { [int]$_.Name }
        [int]$nextId = if ($existingIds) { ($existingIds | Measure-Object -Maximum).Maximum + 1 } else { 1 }
        if ($nextId -gt 999999) {
            throw 'Development VM build ID space is exhausted.'
        }
        $buildId = $nextId.ToString('D6')
        $destination = Join-Path $buildRoot $buildId
        New-Item -ItemType Directory -Path $destination | Out-Null
        [pscustomobject]@{
            BuildId = $buildId
            Destination = $destination
        }
    }

    $sourcePattern = Join-Path $resolvedArtifacts '*'
    Copy-Item -ToSession $session -Path $sourcePattern -Destination $deployment.Destination -Recurse -Force
    Write-Host "Deployed build $($deployment.BuildId) to the VM."
    return $deployment
} finally {
    Remove-DevVmSession $session
}
