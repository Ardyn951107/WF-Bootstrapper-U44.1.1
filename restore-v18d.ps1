param([string]$WarframePath = 'E:\SteamLibrary\steamapps\common\Warframe')
$ErrorActionPreference = 'Stop'
if (Get-Process -Name 'Warframe.x64' -ErrorAction SilentlyContinue) { throw 'Close Warframe first.' }
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$good = Join-Path $here 'payload-v17\dwmapi.dll'
$target = Join-Path $WarframePath 'dwmapi.dll'
Copy-Item $good $target -Force
if ((Get-FileHash $good -Algorithm SHA256).Hash -ne (Get-FileHash $target -Algorithm SHA256).Hash) { throw 'Copy verification failed.' }
Write-Host "Restored the known-good v17 DLL to $target (hash verified)."
