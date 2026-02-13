@echo off
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

call "%VCVARS%"

echo Compiling Resources...
rc /fo resources.res resources.rc || echo Resource compilation skipped

echo Building WiggleMe.exe...
cl /EHsc /W4 /O2 main.cpp resources.res user32.lib gdi32.lib comctl32.lib gdiplus.lib shlwapi.lib /Fe:WiggleMe.exe || cl /EHsc /W4 /O2 main.cpp user32.lib gdi32.lib comctl32.lib gdiplus.lib shlwapi.lib /Fe:WiggleMe.exe

echo Packaging Assets...
if exist WiggleMe.zip del WiggleMe.zip
powershell -Command "Compress-Archive -Path 'Resources' -DestinationPath 'WiggleMe.zip' -Force"

echo Generating Installer...
iexpress /N /Q WiggleMe.sed

echo Build Successful!
