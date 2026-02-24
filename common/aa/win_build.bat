::
:: 通用 build
::
:: ygluu, ai
::
:: 2025-07-20 第 2 次改进
:: 2025-04-19 第 1 次改进
:: 2025-04-13 首版
::

@echo off
setlocal enabledelayedexpansion
chcp 65001 > nul

:: 用法：win_build.bat [debug|release] [proj_a|proj_b|proj_c|all]
set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Debug

set TARGET=%2
if "%TARGET%"=="" set TARGET=all

set BUILD_DIR=%ROOT%\build\win\%BUILD_TYPE%
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem 根据构建工具设置 CMake 生成器
::set CMAKE_GEN=-G "MinGW Makefiles"
set CMAKE_GEN=-G "Ninja"
rem 配置项目
cmake -S "%ROOT%" -B "%BUILD_DIR%" %CMAKE_GEN% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if errorlevel 1 (
    echo CMake 配置失败
    exit /b 1
)

rem 构建项目
if /i "%TARGET%"=="all" (
    cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%
) else (
    cmake --build "%BUILD_DIR%" --target %TARGET% --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%
)

if errorlevel 1 (
    echo 构建失败
    exit /b 1
)

echo 构建成功
