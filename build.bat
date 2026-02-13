@echo off
set "MSVC_DIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
set "SDK_VER=10.0.26100.0"
set "SDK_DIR=C:\Program Files (x86)\Windows Kits\10"

echo Compiling...
"%MSVC_DIR%\bin\Hostx64\x64\cl.exe" /EHsc /O2 /I"%MSVC_DIR%\include" /I"%SDK_DIR%\Include\%SDK_VER%\um" /I"%SDK_DIR%\Include\%SDK_VER%\shared" /I"%SDK_DIR%\Include\%SDK_VER%\ucrt" main.cpp /link /LIBPATH:"%MSVC_DIR%\lib\x64" /LIBPATH:"%SDK_DIR%\Lib\%SDK_VER%\um\x64" /LIBPATH:"%SDK_DIR%\Lib\%SDK_VER%\ucrt\x64" user32.lib gdi32.lib comctl32.lib shell32.lib Ole32.lib Shlwapi.lib /OUT:MouseWigglerPro.exe

if %ERRORLEVEL% == 0 (
    echo Build Successful!
) else (
    echo Build Failed with code %ERRORLEVEL%
)
