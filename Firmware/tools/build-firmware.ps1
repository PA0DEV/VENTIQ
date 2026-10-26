<#
.SYNOPSIS
  Baut und prueft ein OTA-faehiges VENTIQ-Firmwareimage fuer Core RevA.
.EXAMPLE
  .\tools\build-firmware.ps1
#>
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$image = Join-Path $projectRoot ".pio\build\ventiq_core_reva\firmware.bin"
$pioCommand = Get-Command "pio" -ErrorAction SilentlyContinue
$platformioExe = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
$pythonCommand = Get-Command "python" -ErrorAction SilentlyContinue
if ($pioCommand) {
  $pio = $pioCommand.Source
  $pioPrefix = @()
} elseif (Test-Path -LiteralPath $platformioExe -PathType Leaf) {
  $pio = $platformioExe
  $pioPrefix = @()
} elseif ($pythonCommand) {
  $pio = $pythonCommand.Source
  $pioPrefix = @("-m", "platformio")
} else {
  throw "PlatformIO nicht gefunden. Installiere PlatformIO oder fuege 'pio' zum PATH hinzu."
}

Push-Location $projectRoot
try {
  & $pio @pioPrefix run -e ventiq_core_reva | Out-Host
  if ($LASTEXITCODE -ne 0) {
    throw "PlatformIO-Build fehlgeschlagen (Exitcode $LASTEXITCODE)."
  }
}
finally {
  Pop-Location
}

if (-not (Test-Path -LiteralPath $image -PathType Leaf)) {
  throw "PlatformIO hat das erwartete Image nicht erzeugt: $image"
}

$bytes = [System.IO.File]::ReadAllBytes($image)
$magic = "VENTIQ-FW-META01"
$contents = [System.Text.Encoding]::ASCII.GetString($bytes)
$markerOffset = $contents.IndexOf($magic, [System.StringComparison]::Ordinal)
if ($markerOffset -lt 0) {
  throw "Das Image enthaelt keine VENTIQ-Firmwarekennung und ist nicht fuer den Web-Upload geeignet."
}
if ($contents.IndexOf($magic, $markerOffset + $magic.Length, [System.StringComparison]::Ordinal) -ge 0) {
  throw "Das Image enthaelt mehr als eine VENTIQ-Firmwarekennung."
}
if ($bytes.Length -lt ($markerOffset + 40)) {
  throw "Die Firmwarekennung im Image ist unvollstaendig."
}

$expectedHardware = [System.Text.Encoding]::ASCII.GetBytes("VENTIQ_CORE_REVA")
for ($i = 0; $i -lt 20; $i++) {
  $expectedByte = 0
  if ($i -lt $expectedHardware.Length) {
    $expectedByte = $expectedHardware[$i]
  }
  if ($bytes[$markerOffset + 16 + $i] -ne $expectedByte) {
    throw "Das Image ist nicht fuer VENTIQ Core RevA gebaut."
  }
}

$major = $bytes[$markerOffset + 36]
$minor = $bytes[$markerOffset + 37]
$patch = $bytes[$markerOffset + 38]
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $image).Hash.ToLowerInvariant()

Write-Host "Build erfolgreich: VENTIQ Core RevA v$major.$minor.$patch"
Write-Host "Image: $image ($($bytes.Length) Bytes)"
Write-Host "SHA256: $hash"
Write-Output $image
