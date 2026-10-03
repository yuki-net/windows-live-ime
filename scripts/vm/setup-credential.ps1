[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$credentialPath = Join-Path $env:LOCALAPPDATA 'windows-live-ime\vm-credential.xml'
$credentialDirectory = Split-Path -Parent $credentialPath
New-Item -ItemType Directory -Force -Path $credentialDirectory | Out-Null

$credential = Get-Credential -Message 'Enter the Windows account used by PowerShell Direct inside the development VM.'
if ($null -eq $credential) {
    throw 'No VM credential was provided.'
}
$credential | Export-Clixml -LiteralPath $credentialPath
Write-Host 'Saved the VM credential encrypted for the current Windows user.'
