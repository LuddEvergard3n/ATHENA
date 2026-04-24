@echo off
REM ============================================
REM  ATHENA v1.1.2 — GUI Launcher (Windows x64)
REM  Duplo-clique para abrir.
REM  Hides the CMD window via self-relaunch.
REM ============================================
cd /d "%~dp0"
if "%1"=="--minimized" (
    dist\win64\athena.exe gui athena-core\data\platforms
    exit /b
)
start /min "" cmd /c ""%~f0" --minimized"
