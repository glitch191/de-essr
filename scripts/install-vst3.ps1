<#
.SYNOPSIS
    Copies the built de-essr.vst3 bundle to the system VST3 folder.

.DESCRIPTION
    Only the bundle of this project is written. Nothing else in the destination
    folder is read, renamed, changed or removed.

    The build runs this script after each Release build. It can also be run by
    hand, from a terminal opened as administrator if the destination needs it.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\install-vst3.ps1
#>
param(
    [string]$Source = (Join-Path $PSScriptRoot '..\build\DeEssr_artefacts\Release\VST3\de-essr.vst3'),
    [string]$Destination = 'C:\Program Files\Common Files\VST3',
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

if ($Configuration -ne 'Release') {
    Write-Host "install-vst3: skipped ($Configuration build, only Release builds are installed)."
    exit 0
}

if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
    Write-Host "install-vst3: bundle not found at '$Source'. Build the plugin in Release first."
    exit 1
}

if (-not (Test-Path -LiteralPath $Destination -PathType Container)) {
    Write-Host "install-vst3: destination folder '$Destination' does not exist."
    exit 1
}

$bundleName = Split-Path -Leaf $Source
$target = Join-Path $Destination $bundleName

try {
    # Copies into the existing bundle folder if there is one: only files of this plugin are replaced.
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}
catch [System.UnauthorizedAccessException] {
    Write-Host "install-vst3: no permission to write to '$Destination'."
    Write-Host 'Run this script from a terminal opened as administrator:'
    Write-Host "  powershell -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit 1
}
catch [System.IO.IOException] {
    Write-Host "install-vst3: could not replace '$target'."
    Write-Host 'The plugin file is probably open in a DAW. Close the DAW (or remove the plugin from the project) and try again.'
    Write-Host "Details: $($_.Exception.Message)"
    exit 1
}
catch {
    Write-Host "install-vst3: copy failed. $($_.Exception.Message)"
    exit 1
}

$binary = Join-Path $target (Join-Path 'Contents\x86_64-win' $bundleName)

if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
    Write-Host "install-vst3: copy finished but '$binary' is missing."
    exit 1
}

Write-Host "install-vst3: installed '$target'."
exit 0
