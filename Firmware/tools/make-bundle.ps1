<#
.SYNOPSIS
  Baut ein einzelnes VENTIQ-Updatepaket (.vtu) aus Firmware und Webassets.
.EXAMPLE
  .\tools\make-bundle.ps1
#>
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $PSScriptRoot "build-firmware.ps1"
$image = Join-Path $projectRoot ".pio\build\ventiq_core_reva\firmware.bin"
$bundlePath = Join-Path $projectRoot ".pio\build\ventiq_core_reva\ventiq-update.vtu"
$dataPath = Join-Path $projectRoot "data"
$assetNames = @("index.html", "app.css", "app.js", "ota.js", "appicon.svg", "logo.svg")

& $buildScript | Out-Null
if ($LASTEXITCODE -ne 0) {
  throw "Firmware-Build fehlgeschlagen (Exitcode $LASTEXITCODE)."
}
if (-not (Test-Path -LiteralPath $image -PathType Leaf)) {
  throw "Firmware-Image nicht gefunden: $image"
}

$firmware = [System.IO.File]::ReadAllBytes($image)
$firmwareText = [System.Text.Encoding]::ASCII.GetString($firmware)
$marker = "VENTIQ-FW-META01"
$markerOffset = $firmwareText.IndexOf($marker, [System.StringComparison]::Ordinal)
if ($markerOffset -lt 0 -or $firmware.Length -lt ($markerOffset + 40)) {
  throw "Firmwarekennung fehlt oder ist unvollstaendig."
}
$version = "{0}.{1}.{2}" -f $firmware[$markerOffset + 36], $firmware[$markerOffset + 37], $firmware[$markerOffset + 38]
$sha = [System.Security.Cryptography.SHA256]::Create()
try {
  $firmwareHash = ([System.BitConverter]::ToString($sha.ComputeHash($firmware))).Replace("-", "").ToLowerInvariant()
  $assets = @()
  $assetBytes = @()
  foreach ($name in $assetNames) {
    $path = Join-Path $dataPath $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
      throw "Webasset fehlt: $path"
    }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -eq 0 -or $bytes.Length -gt 131072) {
      throw "Webasset hat eine ungueltige Groesse: $name"
    }
    $hash = ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    $assets += [ordered]@{ name = $name; size = $bytes.Length; sha256 = $hash }
    $assetBytes += ,$bytes
  }

  $manifestObject = [ordered]@{
    format = 1
    hardware = "VENTIQ_CORE_REVA"
    version = $version
    firmware = [ordered]@{ size = $firmware.Length; sha256 = $firmwareHash }
    assets = $assets
  }
  $manifest = [System.Text.Encoding]::UTF8.GetBytes(($manifestObject | ConvertTo-Json -Depth 5 -Compress))
  if ($manifest.Length -eq 0 -or $manifest.Length -gt 1024) {
    throw "Bundle-Manifest ist zu gross."
  }
  $manifestHash = $sha.ComputeHash($manifest)

  $output = [System.IO.File]::Open($bundlePath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
  try {
    $output.Write([System.Text.Encoding]::ASCII.GetBytes("VTUBNDL1"), 0, 8)
    $lengthBytes = [System.BitConverter]::GetBytes([uint16]$manifest.Length)
    $output.Write($lengthBytes, 0, 2)
    $output.Write($manifest, 0, $manifest.Length)
    $output.Write($manifestHash, 0, $manifestHash.Length)
    foreach ($bytes in $assetBytes) {
      $output.Write($bytes, 0, $bytes.Length)
    }
    $output.Write($firmware, 0, $firmware.Length)
    $output.Write([System.Text.Encoding]::ASCII.GetBytes("VTUEND01"), 0, 8)
  }
  finally {
    $output.Dispose()
  }
}
finally {
  $sha.Dispose()
}

$bundleHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $bundlePath).Hash.ToLowerInvariant()
$bundleSize = (Get-Item -LiteralPath $bundlePath).Length
Write-Host "Updatepaket erstellt: VENTIQ Core RevA v$version"
Write-Host "Datei: $bundlePath ($bundleSize Bytes)"
Write-Host "SHA-256: $bundleHash"
Write-Output $bundlePath
