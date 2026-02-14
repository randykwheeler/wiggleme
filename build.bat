@echo off
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

call "%VCVARS%"

echo Compiling Resources...
rc /fo resources.res resources.rc || echo Resource compilation skipped

echo Building WiggleMe.exe...
if exist resources.res (
    cl /EHsc /W4 /O2 main.cpp resources.res user32.lib gdi32.lib comctl32.lib shlwapi.lib shell32.lib /Fe:WiggleMe.exe
) else (
    cl /EHsc /W4 /O2 main.cpp user32.lib gdi32.lib comctl32.lib shlwapi.lib shell32.lib /Fe:WiggleMe.exe
)

echo Embedding Manifest...
mt.exe -manifest WiggleMe.exe.manifest -outputresource:WiggleMe.exe;#1

echo Preparing MSIX Package...
if exist Package rd /s /q Package
mkdir Package
mkdir Package\Resources
copy WiggleMe.exe Package\
copy AppxManifest.xml Package\
copy Resources\mascot.png Package\Resources\

echo Generating Self-Signed Certificate...
powershell -Command "if (!(Test-Path WiggleMeCert.pfx)) { $cert = New-SelfSignedCertificate -Type Custom -Subject 'CN=RandyKWheeler' -KeyUsage DigitalSignature -FriendlyName 'WiggleMe Cert' -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}'); $pwd = ConvertTo-SecureString -String '1234' -Force -AsPlainText; Export-PfxCertificate -Cert $cert -FilePath 'WiggleMeCert.pfx' -Password $pwd }"

echo Generating MSIX...
set "SDK_BIN=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"
"%SDK_BIN%\makeappx.exe" pack /d Package /p WiggleMe.msix /o

echo Signing MSIX...
"%SDK_BIN%\signtool.exe" sign /f WiggleMeCert.pfx /p 1234 /fd SHA256 /v WiggleMe.msix

if exist WiggleMe.msix (
    echo MSIX built and signed successfully!
) else (
    echo MSIX build failed!
)

echo Build Successful!
