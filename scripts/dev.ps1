[CmdletBinding()]
param(
    [string]$VMName = 'WindowsLiveImeDev',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$vmScriptRoot = Join-Path $PSScriptRoot 'vm'
$artifactDirectory = Join-Path $repoRoot "build\artifacts\$Configuration"

& (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration
if (-not $?) {
    throw "Build failed for configuration $Configuration."
}

& (Join-Path $vmScriptRoot 'setup.ps1') -VMName $VMName
if (-not $?) {
    throw 'Development VM setup failed.'
}
$deployment = & (Join-Path $vmScriptRoot 'deploy.ps1') -VMName $VMName -ArtifactDirectory $artifactDirectory
if (-not $? -or $null -eq $deployment -or -not $deployment.BuildId) {
    throw 'Deployment did not return a build ID.'
}
$buildId = $deployment.BuildId

& (Join-Path $vmScriptRoot 'unregister.ps1') -VMName $VMName -IgnoreMissing
if (-not $?) {
    throw 'Could not unregister the previously active VM build.'
}
& (Join-Path $vmScriptRoot 'register.ps1') -VMName $VMName -BuildId $buildId
if (-not $?) {
    throw "Could not register VM build $buildId."
}
& (Join-Path $vmScriptRoot 'start-engine.ps1') -VMName $VMName -BuildId $buildId
if (-not $?) {
    throw "Could not start engine host build $buildId."
}
& (Join-Path $vmScriptRoot 'launch-test-app.ps1') -VMName $VMName
if (-not $?) {
    throw 'Could not start the VM test application.'
}

$vmConnect = Join-Path $env:WINDIR 'System32\vmconnect.exe'
if (-not (Test-Path -LiteralPath $vmConnect)) {
    throw 'VMConnect.exe was not found. Install the Hyper-V management tools on the host.'
}
Start-Process -FilePath $vmConnect -ArgumentList @('localhost', $VMName)
Write-Host "Dev IME is ready in VM '$VMName' with build $buildId."
