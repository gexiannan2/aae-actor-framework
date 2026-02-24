@echo off
chcp 65001 > nul

rem 获取当前 Windows 目录
set WIN_DIR=%~dp0
set WIN_DIR=%WIN_DIR:~0,-1%

rem 转成 WSL 路径（/mnt/d/... 形式）
for /f "delims=" %%i in ('wsl wslpath -u "%WIN_DIR%"') do set WSL_DIR=%%i

rem 调用脚本
wsl bash "%WSL_DIR%/wsl_rebuild_release.sh"

pause
