@echo off
setlocal EnableExtensions
chcp 65001 >nul

for %%i in ("%~dp0.") do set "RUN_DIR=%%~fi\"

set "EXE=%RUN_DIR%..\common\aa\lib\aae.exe"
if not exist "%EXE%" set "EXE=%RUN_DIR%..\bin\aae.exe"

if not exist "%EXE%" (
    echo [ERROR] aae executable not found.
    echo [ERROR] Tried:
    echo         %RUN_DIR%..\common\aa\lib\aae.exe
    echo         %RUN_DIR%..\bin\aae.exe
    exit /b 1
)

set "RUNTIME_BIN="
for %%D in ("C:\mingw64\bin" "C:\msys64\mingw64\bin" "C:\msys64\ucrt64\bin") do (
    if not defined RUNTIME_BIN if exist "%%~fD\libstdc++-6.dll" set "RUNTIME_BIN=%%~fD"
)
if defined RUNTIME_BIN set "PATH=%RUNTIME_BIN%;%PATH%"

set "ENTRY=%RUN_DIR%main.lua"

pushd "%RUN_DIR%" >nul
echo "%EXE%" mainfile "%ENTRY%" %*
"%EXE%" mainfile "%ENTRY%" %*
set "EC=%ERRORLEVEL%"
popd >nul

if not "%EC%"=="0" (
    if "%EC%"=="-1073741515" (
        echo [ERROR] Missing runtime DLL.
        echo [ERROR] Add MinGW bin to PATH, e.g. C:\msys64\mingw64\bin
    )
)

exit /b %EC%