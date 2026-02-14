param([string]$Action = "Install")

$AppName = "Wiggle Me"
$AppId = "WiggleMe"
$InstallDir = Join-Path $Env:ProgramFiles $AppName
$ExePath = Join-Path $InstallDir "WiggleMe.exe"
$ShortcutPath = Join-Path ([Environment]::GetFolderPath("CommonStartMenu")) "Programs\$AppName.lnk"
$DesktopShortcutPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "$AppName.lnk"
$UninstallKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$AppId"

if ($Action -eq "Install") {
    # 1. Create Directory
    if (-not (Test-Path $InstallDir)) { New-Item -ItemType Directory -Path $InstallDir -Force }

    # 2. Copy Files (Assumes they are in the same folder as this script)
    $ScriptDir = Split-Path $MyInvocation.MyCommand.Path
    Copy-Item "$ScriptDir\WiggleMe.exe" $InstallDir -Force
    Copy-Item "$ScriptDir\Resources" $InstallDir -Recurse -Force
    Copy-Item "$ScriptDir\SetupHelper.ps1" $InstallDir -Force

    # 3. Create Shortcuts
    $WshShell = New-Object -ComObject WScript.Shell
    $Shortcut = $WshShell.CreateShortcut($ShortcutPath)
    $Shortcut.TargetPath = $ExePath
    $Shortcut.WorkingDirectory = $InstallDir
    $Shortcut.Save()

    $DShortcut = $WshShell.CreateShortcut($DesktopShortcutPath)
    $DShortcut.TargetPath = $ExePath
    $DShortcut.WorkingDirectory = $InstallDir
    $DShortcut.Save()

    # 4. Register Uninstaller
    New-Item -Path $UninstallKey -Force
    New-ItemProperty -Path $UninstallKey -Name "DisplayName" -Value $AppName -PropertyType String -Force
    New-ItemProperty -Path $UninstallKey -Name "UninstallString" -Value "powershell.exe -ExecutionPolicy Bypass -File `"$InstallDir\SetupHelper.ps1`" -Action Uninstall" -PropertyType String -Force
    New-ItemProperty -Path $UninstallKey -Name "DisplayIcon" -Value $ExePath -PropertyType String -Force
    New-ItemProperty -Path $UninstallKey -Name "Publisher" -Value "Randy K. Wheeler" -PropertyType String -Force

    Write-Host "Installation Complete!"
}
elseif ($Action -eq "Uninstall") {
    # 1. Kill Process
    Stop-Process -Name "WiggleMe" -ErrorAction SilentlyContinue

    # 2. Remove Shortcuts
    if (Test-Path $ShortcutPath) { Remove-Item $ShortcutPath -Force }
    if (Test-Path $DesktopShortcutPath) { Remove-Item $DesktopShortcutPath -Force }

    # 3. Remove Files
    if (Test-Path $InstallDir) { Remove-Item $InstallDir -Recurse -Force }

    # 4. Cleanup Registry
    if (Test-Path $UninstallKey) { Remove-Item $UninstallKey -Force }

    Write-Host "Uninstallation Complete!"
}
