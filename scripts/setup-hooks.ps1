$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

& git -C $repoRoot config --local core.hooksPath .githooks
if ($LASTEXITCODE -ne 0) {
    throw 'Could not configure the repository-managed Git hooks path.'
}

Write-Host 'Installed the repository-managed pre-push hook path.'
