[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = [IO.File]::ReadAllText((Join-Path $repoRoot 'VERSION')).Trim()
$commit = (& git -C $repoRoot rev-parse --short=12 HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine the Git commit for this build.' }
$dirty = @(& git -C $repoRoot status --porcelain).Count -gt 0
$info = [ordered]@{
    version=$version
    commit=$commit
    dirty=$dirty
    builtAt=[DateTimeOffset]::UtcNow.ToString('o')
}
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
[IO.File]::WriteAllText((Join-Path $Destination 'build-info.json'), ($info | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
