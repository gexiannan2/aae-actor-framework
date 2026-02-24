@echo off
setlocal
set ROOT=%~dp0
if exist "%ROOT%build" rmdir /s /q "%ROOT%build"
call "%ROOT%win_build.bat" %*

pause