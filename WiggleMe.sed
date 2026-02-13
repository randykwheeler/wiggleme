[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=1
HideExtractAnimation=1
UseLongFileName=1
InsideCompressed=1
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=Do you want to install Wiggle Me!?
DisplayLicense=
FinishMessage=Wiggle Me! has been installed successfully. You can launch it from your Desktop or Start Menu.
TargetName=WiggleMe_Setup.exe
FriendlyName=Wiggle Me! Installer
AppLaunched=powershell.exe -ExecutionPolicy Bypass -WindowStyle Hidden -File SetupHelper.ps1 -Action Install
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
SourceFiles=SourceFiles
[SourceFiles]
SourceFiles0=D:\ai\computer mgmt\MouseWigglerNative\
[SourceFiles0]
%File0%=WiggleMe.exe
%File1%=SetupHelper.ps1
%File2%=WiggleMe.zip
[Strings]
File0=WiggleMe.exe
File1=SetupHelper.ps1
File2=WiggleMe.zip
