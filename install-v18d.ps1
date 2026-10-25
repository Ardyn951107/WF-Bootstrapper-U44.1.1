param(
    [string]$WarframePath = 'E:\SteamLibrary\steamapps\common\Warframe'
)
$ErrorActionPreference = 'Stop'
$WarframePath = (Resolve-Path $WarframePath).Path
if (!(Test-Path (Join-Path $WarframePath 'Warframe.x64.exe'))) { throw "Warframe.x64.exe not found in $WarframePath" }
if (Get-Process -Name 'Warframe.x64' -ErrorAction SilentlyContinue) { throw 'Close Warframe first (the DLL is locked while the game runs).' }
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$payload = Join-Path $here 'payload\dwmapi.dll'
$target = Join-Path $WarframePath 'dwmapi.dll'
$backupDir = Join-Path $WarframePath 'OpenWF\u44-v18d-backup'
New-Item -ItemType Directory -Force $backupDir | Out-Null
if (Test-Path $target) { Copy-Item $target (Join-Path $backupDir ("dwmapi-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + ".dll")) -Force }
Copy-Item $payload $target -Force
if ((Get-FileHash $payload -Algorithm SHA256).Hash -ne (Get-FileHash $target -Algorithm SHA256).Hash) { throw 'Copy verification failed.' }
Write-Host "Installed v18-diag as $target (hash verified). Previous dwmapi.dll is in $backupDir"
Write-Host "Launch Warframe, log in, then check OpenWF\spaceninja-u44-v18.log"
