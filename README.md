# WF Bootstrapper U44.1.1

This repository swaps Warframe's `dwmapi.dll` with a diagnostic build to trace the U44 OpenWF bootstrap path and log the relevant call flow without changing normal game behavior beyond the diagnostic instrumentation.

The diagnostic payload is intentionally built to behave like the known-good v17 baseline while logging calls to OpenWF's message builder. The generated log is written to:

`OpenWF\spaceninja-u44-v18.log`

## What this does

The instrumented `payload\dwmapi.dll` logs every call to OpenWF's message builder at RVA `0x7F330`, including the caller address. Example output:

`[18] msg-builder 0x7F330 called, ret=OpenWF+0x...`

This was used to verify the real call site for the "optional pattern scan" path. A first round of instrumentation at `0x28B80` produced no log output, which confirmed that address was not the active message-builder site.

## Repository layout

- `payload/` — diagnostic v18 build used for tracing (`dwmapi.dll`)
- `payload-v17/` — known-good v17 restore payload
- `install-v18d.ps1` — PowerShell installer for the diagnostic build
- `restore-v18d.ps1` — PowerShell restore script for the v17 DLL
- `installer.cpp` — native Windows installer that auto-detects the Warframe folder, backs up the original DLL, verifies SHA256 hashes, and supports CLI usage
- `installer.exe` — compiled native installer binary

## Prerequisites

- Close Warframe before installing or restoring the DLL.
- The game folder must contain `Warframe.x64.exe`.
- Run the scripts from a PowerShell session with execution policy bypass enabled if needed.

## Install diagnostic build

From the repo root:

```powershell
powershell -ExecutionPolicy Bypass -File ".\install-v18d.ps1"
```

This will:

- detect the Warframe install folder
- stop if Warframe is running
- back up the existing `dwmapi.dll` to `OpenWF\u44-v18d-backup`
- copy in the diagnostic payload
- verify the file hash matches the payload
- tell you where the log will appear

## Restore the known-good v17 DLL

```powershell
powershell -ExecutionPolicy Bypass -File ".\restore-v18d.ps1"
```

This restores the v17 DLL and verifies the hash before completing.

## Native installer usage

The project also includes a C++ installer with a small interactive menu and CLI flags:

```powershell
installer.exe --install --path "C:\Games\Warframe"
installer.exe --restore --path "C:\Games\Warframe"
```

Optional CLI switches:

- `--install` — install the diagnostic payload
- `--restore` — restore the known-good v17 payload
- `--path <dir>` — explicitly select the Warframe install folder
- `--payload <dll>` — install a custom replacement DLL instead of `payload\dwmapi.dll`
- `--yes` — non-interactive, no prompts

## Notes

- This is diagnostic-only tooling for investigation.
- It intentionally targets the `dwmapi.dll` replacement path used by the game.
- The backup files are stored under `OpenWF\u44-v18d-backup` so they can be restored later if needed.
