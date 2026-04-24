@echo off
REM ============================================
REM  ATHENA v1.1.2 — CLI (Windows x64)
REM  Duplo-clique para abrir o terminal.
REM ============================================
cd /d "%~dp0"
echo.
echo  ATHENA v1.1.2 — Command Line Interface
echo  =======================================
echo.
echo  Comandos disponiveis:
echo    athena-cli info
echo    athena-cli platforms athena-core\data\platforms
echo    athena-cli run athena-core\examples\test-scenario.json 42
echo    athena-cli batch athena-core\examples\test-scenario.json 100
echo    athena-cli -h
echo.
echo  Digite um comando abaixo:
echo.

doskey athena-cli="dist\win64\athena-cli.exe" $*
set PATH=%~dp0dist\win64;%PATH%

cmd /k "echo Pronto. Use: athena-cli.exe -h"
