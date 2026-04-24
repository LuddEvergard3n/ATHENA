@echo off
REM ATHENA v1.1.2 - Windows Build Script
REM 
REM Uso:
REM   build.bat           - Compilar tudo (GUI + CLI)
REM   build.bat gui       - Compilar apenas GUI
REM   build.bat cli       - Compilar apenas CLI
REM   build.bat clean     - Limpar build
REM   build.bat run       - Compilar e executar GUI
REM
REM Requisitos:
REM   - Visual Studio 2022 com C++ Desktop Development
REM   - vcpkg com glfw3 instalado
REM   - CMake 3.16+

setlocal enabledelayedexpansion

set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build
set VCPKG_ROOT=C:\vcpkg

REM Verificar se vcpkg existe
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [ERRO] vcpkg nao encontrado em %VCPKG_ROOT%
    echo.
    echo Instale vcpkg:
    echo   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    echo   cd C:\vcpkg
    echo   .\bootstrap-vcpkg.bat
    echo   .\vcpkg install glfw3:x64-windows
    exit /b 1
)

REM Processar argumentos
if "%1"=="" goto :build_all
if "%1"=="gui" goto :build_gui
if "%1"=="cli" goto :build_cli
if "%1"=="clean" goto :clean
if "%1"=="run" goto :run
if "%1"=="help" goto :help
goto :help

:build_all
echo.
echo === ATHENA v1.1.2 - Compilando Tudo ===
echo.
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
    -DATHENA_BUILD_GUI=ON ^
    -DATHENA_BUILD_CLI=ON

if errorlevel 1 (
    echo [ERRO] CMake falhou
    exit /b 1
)

cmake --build . --config Release --parallel

if errorlevel 1 (
    echo [ERRO] Build falhou
    exit /b 1
)

echo.
echo === Build concluido ===
echo Executaveis em: %BUILD_DIR%\Release\
dir /b "%BUILD_DIR%\Release\*.exe" 2>nul
goto :end

:build_gui
echo.
echo === Compilando GUI ===
echo.
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
    -DATHENA_BUILD_GUI=ON ^
    -DATHENA_BUILD_CLI=OFF

cmake --build . --config Release --target athena --parallel
echo.
echo Executavel: %BUILD_DIR%\Release\athena.exe
goto :end

:build_cli
echo.
echo === Compilando CLI ===
echo.
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DATHENA_BUILD_GUI=OFF ^
    -DATHENA_BUILD_CLI=ON

cmake --build . --config Release --target athena-cli --parallel
echo.
echo Executavel: %BUILD_DIR%\Release\athena-cli.exe
goto :end

:run
call :build_all
if errorlevel 1 exit /b 1
echo.
echo === Executando ATHENA ===
"%BUILD_DIR%\Release\athena.exe"
goto :end

:clean
echo.
echo === Limpando build ===
if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    echo Build removido
) else (
    echo Nada para limpar
)
goto :end

:help
echo.
echo ATHENA v1.1.2 - Script de Build para Windows
echo.
echo Uso:
echo   build.bat           Compilar tudo (GUI + CLI)
echo   build.bat gui       Compilar apenas GUI
echo   build.bat cli       Compilar apenas CLI (sem GLFW)
echo   build.bat clean     Limpar diretorio de build
echo   build.bat run       Compilar e executar GUI
echo   build.bat help      Mostrar esta ajuda
echo.
echo Requisitos:
echo   - Visual Studio 2022 com "Desktop development with C++"
echo   - CMake 3.16+ (https://cmake.org/download/)
echo   - vcpkg em C:\vcpkg com glfw3:x64-windows instalado
echo.
echo Instalacao do vcpkg:
echo   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
echo   cd C:\vcpkg
echo   .\bootstrap-vcpkg.bat
echo   .\vcpkg integrate install
echo   .\vcpkg install glfw3:x64-windows
echo.
goto :end

:end
cd "%PROJECT_DIR%"
endlocal
