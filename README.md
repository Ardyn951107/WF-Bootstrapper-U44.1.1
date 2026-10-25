# v18-diag (diagnostic only, behaviour identical to v17)
Round 2: logs every call of OpenWF's message builder (RVA 0x7F330) with the caller address:
`[18] msg-builder 0x7F330 called, ret=OpenWF+0x...`  ->  OpenWF\spaceninja-u44-v18.log
Round 1 (hooking 0x28B80) logged nothing: that function is not where the "optional pattern scan" message comes from.

Install (close Warframe first, use PowerShell):
  powershell -ExecutionPolicy Bypass -File ".\install-v18d.ps1"
Undo (back to v17):
  powershell -ExecutionPolicy Bypass -File ".\restore-v18d.ps1"
