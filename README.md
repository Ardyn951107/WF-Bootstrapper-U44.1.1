# WF Bootstrapper U44.1.1

A small Windows utility for temporarily installing a diagnostic `dwmapi.dll` in a Warframe installation.

The installer backs up an existing game-folder DLL before replacing it. The scripts and native installer verify copied files with SHA-256 hashes. Diagnostic logs are written by the payload to:

```text
OpenWF\spaceninja-u44-v18.log
```

## Contents

- `payload\dwmapi.dll` — diagnostic build
- `payload-v17\dwmapi.dll` — known-good v17 restore build
- `install-v18d.ps1` — PowerShell diagnostic-build installer
- `restore-v18d.ps1` — PowerShell restore script
- `installer.exe` — native Windows installer
- `installer.cpp` — source for the native installer

## Before you start

- Windows is required.
- Close Warframe before installing or restoring the DLL.
- Make sure the selected game folder contains `Warframe.x64.exe`.
- The PowerShell scripts default to `E:\SteamLibrary\steamapps\common\Warframe`. If your game is installed elsewhere, pass its folder with `-WarframePath`.

## Install the diagnostic build

From the repository folder, run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\install-v18d.ps1"
```

For a different game-folder location:

```powershell
powershell -ExecutionPolicy Bypass -File ".\install-v18d.ps1" -WarframePath "C:\Games\Warframe"
```

The script checks that the game is closed and the game executable is present, backs up any existing `dwmapi.dll` to `OpenWF\u44-v18d-backup`, installs the diagnostic build, and verifies the copied file.

## Restore the v17 build

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\restore-v18d.ps1"
```

If Warframe is installed in a different folder, specify it:

```powershell
powershell -ExecutionPolicy Bypass -File ".\restore-v18d.ps1" -WarframePath "C:\Games\Warframe"
```

The restore script checks that Warframe is closed, copies the included v17 DLL into the game folder, and verifies the copy.

## Native installer

Run `installer.exe` to open the interactive menu. The native installer can also be used from PowerShell:

```powershell
.\installer.exe --install --path "C:\Games\Warframe"
.\installer.exe --restore --path "C:\Games\Warframe"
```

Available options:

| Option | Description |
| --- | --- |
| `--install` | Install the diagnostic build. |
| `--restore` | Restore the included v17 build. |
| `--path <dir>` | Specify the Warframe folder. If omitted, the installer tries to detect it through Steam, then checks its configured default folder. |
| `--payload <dll>` | Use a custom DLL for installation instead of `payload\dwmapi.dll`. |
| `--yes` | Run without the interactive menu or prompts. |
| `--help` | Show command-line usage. |

Example non-interactive install:

```powershell
.\installer.exe --install --path "C:\Games\Warframe" --yes
```

## Troubleshooting

- **Warframe is running:** Close the game, then retry. The game may lock the DLL while running.
- **Game folder not found:** Confirm the folder contains `Warframe.x64.exe`; use `-WarframePath` with a script or `--path` with the native installer.
- **PowerShell blocks the script:** Run it with `-ExecutionPolicy Bypass` as shown above.
- **Need to return to the previous DLL:** Use the restore script or the native installer's restore option. The original DLL backup created by installation is stored in `OpenWF\u44-v18d-backup`.
- **Looking for diagnostics:** Check `OpenWF\spaceninja-u44-v18.log` in the Warframe folder after launching the game.

## Contributors

- Mosiu
