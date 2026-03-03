@echo off
setlocal EnableExtensions
chcp 65001 >nul

for %%i in ("%~dp0.") do set "RUN_DIR=%%~fi\"

set "EXE=%RUN_DIR%..\common\aa\lib\aae.exe"
if not exist "%EXE%" set "EXE=%RUN_DIR%..\bin\aae.exe"

if not exist "%EXE%" (
    echo [ERROR] aae executable not found.
    exit /b 1
)

set "ENTRY=%RUN_DIR%main.lua"

pushd "%RUN_DIR%" >nul
"%EXE%" mainfile "%ENTRY%" %*
set "EC=%ERRORLEVEL%"
popd >nul

exit /b %EC%
