$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$desktop = [Environment]::GetFolderPath('Desktop')
$shortcutPath = Join-Path $desktop 'NBA Live 97 Boot Decomp.lnk'
$terminal = (Get-Command wt.exe -ErrorAction Stop).Source
$exe = Join-Path $repo 'build-windows\Debug\nba97_boot_decomp.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Missing native build: $exe" }

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $terminal
$shortcut.Arguments = "new-tab --title `"NBA Live 97 Decomp Debugger`" --startingDirectory `"$repo`" `"$exe`""
$shortcut.WorkingDirectory = $repo
$shortcut.Description = 'NBA Live 97 native original-asset boot decompilation with CLI trace'
$shortcut.Save()
Write-Host "Created shortcut: $shortcutPath"
