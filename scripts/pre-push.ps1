$ErrorActionPreference = 'Stop'
$testScript = Join-Path $PSScriptRoot 'test.ps1'
& $testScript -Configuration Release -CoreOnly
if ($LASTEXITCODE -ne 0) {
    throw "Pre-push checks failed with exit code $LASTEXITCODE."
}
