param([Parameter(Mandatory = $true)][string]$Sketchbook)
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../extras/CameraDmaFix'
$destination = Join-Path ([IO.Path]::GetFullPath($Sketchbook)) 'libraries/CameraDmaFix'
New-Item -ItemType Directory -Path (Join-Path $destination 'src') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $destination 'src/esp32p4') -Force | Out-Null
foreach ($relative in @('library.properties', 'src/CameraDmaFix.h', 'src/CameraDmaFix.cpp', 'src/esp32p4/libCameraDmaLink.a')) {
    Copy-Item -LiteralPath (Join-Path $source $relative) -Destination (Join-Path $destination $relative) -Force
}
Write-Output "CameraDmaFix installed at $destination"
