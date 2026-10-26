<#
.SYNOPSIS
  Laedt eine VENTIQ-Firmware per HTTP auf ein Geraet (POST /api/update).
.EXAMPLE
  .\tools\ota-upload.ps1 -Device 192.168.4.1 -Build
  .\tools\ota-upload.ps1 -Device 192.168.4.1 -Image .\firmware.bin -DryRun
#>
param(
  [Parameter(Mandatory = $true)][string]$Device,
  [string]$Image,
  [switch]$Build,
  [switch]$DryRun
)

$ErrorActionPreference = "Stop"

if ($Build -and $Image) {
  throw "-Build und -Image koennen nicht gemeinsam verwendet werden."
}

if ($Build) {
  $Image = & (Join-Path $PSScriptRoot "build-firmware.ps1")
}

if (-not $Image) {
  $root = Split-Path -Parent $MyInvocation.MyCommand.Path
  $Image = Join-Path $root "..\.pio\build\ventiq_core_reva\firmware.bin"
}

if (-not (Test-Path $Image)) { throw "Image nicht gefunden: $Image" }
$Image = (Resolve-Path $Image).Path
$size = (Get-Item $Image).Length
$hash = (Get-FileHash -Algorithm SHA256 -Path $Image).Hash.ToLower()

Write-Host "Image : $Image ($size Bytes)"
Write-Host "SHA256: $hash"

$base = "http://$Device"
$url = "$base/api/update?sha256=$hash"

if ($DryRun) {
  Write-Host "DryRun - wuerde hochladen nach: $url"
  return
}

Write-Host "Installiert: $(curl.exe -s --max-time 5 "$base/api/firmware")"
Write-Host "Lade hoch ..."

$body = curl.exe -s --max-time 120 -w "`n%{http_code}" -F "firmware=@$Image" $url
$lines = @($body -split "`n")
$code = [int]$lines[-1]
$json = ($lines[0..($lines.Count - 2)] -join "`n")

Write-Host "HTTP $code : $json"
if ($code -ne 200) { exit 1 }

Write-Host "Update uebernommen, Geraet startet neu. Pruefe Version ..."
Start-Sleep -Seconds 8
Write-Host "Jetzt: $(curl.exe -s --max-time 5 "$base/api/firmware")"
