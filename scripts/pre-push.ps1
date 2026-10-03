$ErrorActionPreference = 'Stop'
$testScript = Join-Path $PSScriptRoot 'test.ps1'
& $testScript -Configuration Release -CoreOnly
if (-not $?) {
    throw 'Pre-push checks failed.'
}
