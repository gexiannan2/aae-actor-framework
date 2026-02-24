@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul

rem Usage: win_build.bat [Debug|Release] [target|all]
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Debug"

set "TARGET=%~2"
if "%TARGET%"=="" set "TARGET=all"

rem Auto-detect MinGW toolchain if it is not in PATH.
set "MINGW_BIN="
for %%D in ("C:\mingw64\bin" "C:\msys64\mingw64\bin" "C:\msys64\ucrt64\bin") do (
    if not defined MINGW_BIN if exist "%%~fD\g++.exe" if exist "%%~fD\ninja.exe" set "MINGW_BIN=%%~fD"
)
if defined MINGW_BIN (
    set "PATH=%MINGW_BIN%;%PATH%"
    echo [INFO] Using MinGW toolchain: %MINGW_BIN%
) else (
    where g++ >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] g++ and ninja not found.
        echo [ERROR] Install MinGW/MSYS2 and ensure one of these exists:
        echo         C:\mingw64\bin
        echo         C:\msys64\mingw64\bin
        exit /b 1
    )
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH.
    exit /b 1
)
where ninja >nul 2>nul
if errorlevel 1 (
    echo [ERROR] ninja not found in PATH.
    exit /b 1
)

set "BUILD_DIR=%ROOT%\build\win\%BUILD_TYPE%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "CMAKE_GEN=-G Ninja"
cmake -S "%ROOT%" -B "%BUILD_DIR%" %CMAKE_GEN% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

if /i "%TARGET%"=="all" (
    cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%
) else (
    cmake --build "%BUILD_DIR%" --target %TARGET% --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%
)

if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [OK] Build succeeded.