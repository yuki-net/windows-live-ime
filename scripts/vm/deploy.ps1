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
    'build-info.json'
    'settings\LiveImeSettings.exe'
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

    # Do not transfer stale Swift object/module directories or test binaries.
    $prefix = $resolvedArtifacts.TrimEnd('\') + '\'
    $files = @(Get-ChildItem -LiteralPath $resolvedArtifacts -File -Recurse | Where-Object {
        $relative = $_.FullName.Substring($prefix.Length)
        if ($relative -notmatch '\\') {
            ($_.Extension -eq '.dll' -and $_.Name -notlike '*Tests*') -or
                $_.Name -in @('engine-host.exe', 'ime_ipc_ping.exe', 'build-info.json')
        } else {
            ($relative -match '^[^\\]+\.bundle\\' -or $relative.StartsWith('settings\')) -and
                $_.Name -notin @('smoke-test.json', 'startup-error.log') -and $_.Extension -ne '.pdb'
        }
    })
    $totalBytes = ($files | Measure-Object -Property Length -Sum).Sum
    Write-Host ('Preparing build {0}: {1:N0} runtime files, {2:N1} MiB.' -f
        $deployment.BuildId, $files.Count, ($totalBytes / 1MB))
    $copyTimer = [Diagnostics.Stopwatch]::StartNew()
    $archivePath = Join-Path ([IO.Path]::GetTempPath()) ('live-ime-' + [guid]::NewGuid().ToString('N') + '.zip')
    $remoteArchive = $deployment.Destination + '.zip'
    $previousProgressPreference = $ProgressPreference
    try {
        # Per-file remoting progress flickers in IDE output panes. Use stable
        # start/completion messages instead of repeatedly redrawing that pane.
        $ProgressPreference = 'SilentlyContinue'
        Add-Type -AssemblyName System.IO.Compression
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive = [IO.Compression.ZipFile]::Open($archivePath, [IO.Compression.ZipArchiveMode]::Create)
        try {
            foreach ($file in $files) {
                $entry = $file.FullName.Substring($prefix.Length).Replace('\', '/')
                [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName,
                    $entry, [IO.Compression.CompressionLevel]::Fastest) | Out-Null
            }
        } finally { $archive.Dispose() }
        Write-Host ('Transferring one archive: {0:N1} MiB...' -f ((Get-Item -LiteralPath $archivePath).Length / 1MB))
        Copy-Item -ToSession $session -LiteralPath $archivePath -Destination $remoteArchive -Force
        Write-Host 'Extracting runtime files inside the VM...'
        Invoke-Command -Session $session -ArgumentList $remoteArchive, $deployment.Destination -ScriptBlock {
            param($ArchivePath, $Destination)
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            try { [IO.Compression.ZipFile]::ExtractToDirectory($ArchivePath, $Destination) }
            finally { Remove-Item -LiteralPath $ArchivePath -Force }
        }
    } finally {
        if (Test-Path -LiteralPath $archivePath) { Remove-Item -LiteralPath $archivePath -Force }
        $ProgressPreference = $previousProgressPreference
        $copyTimer.Stop()
    }
    Write-Host ('Deployed build {0} to the VM in {1:N1}s.' -f
        $deployment.BuildId, $copyTimer.Elapsed.TotalSeconds)
    return $deployment
} finally {
    Remove-DevVmSession $session
}
