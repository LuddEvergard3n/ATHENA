# ATHENA v1.1.2 - Instruções de Compilação

---

## Windows (MinGW Standalone) - TESTADO E FUNCIONAL

### Requisitos
- MinGW-w64 (GCC 13+)
- mingw32-make

### Passos

1. **Extrair MinGW** para `C:\mingw64` ou outro local

2. **Abrir CMD** (como administrador opcional)

3. **Navegar até a pasta ATHENA:**
```cmd
cd "C:\Users\SeuUsuario\Desktop\ATHENA"
```

4. **Adicionar MinGW ao PATH:**
```cmd
set PATH=C:\mingw64\bin;%PATH%
```

5. **Verificar GCC:**
```cmd
g++ --version
```

6. **Compilar CLI:**
```cmd
mingw32-make -f Makefile.mingw clean
mingw32-make -f Makefile.mingw cli
```

7. **Testar CLI:**
```cmd
build\bin\athena-cli.exe info
build\bin\athena-cli.exe run athena-core\examples\test-scenario.json 42
build\bin\athena-cli.exe batch athena-core\examples\test-scenario.json 100
```

8. **Compilar GUI (requer GLFW3):**

**Opcao A** — MSYS2 (se disponivel):
```cmd
pacman -S mingw-w64-x86_64-glfw
mingw32-make -f Makefile.mingw gui
```

**Opcao B** — MinGW standalone (sem pacman):
1. Baixar GLFW pre-compilado de https://www.glfw.org/download (64-bit Windows binaries)
2. Extrair para `C:\glfw-3.4.bin.WIN64` (ou outro local)
3. Build com:
```cmd
mingw32-make -f Makefile.mingw gui GLFW_DIR=C:\glfw-3.4.bin.WIN64
```

9. **Testar GUI:**
```cmd
build\bin\athena.exe --version
build\bin\athena.exe gui
```

### Resultado Esperado
```
> athena-cli.exe info
ATHENA Military Simulation Framework v1.1.2-cli

> athena-cli.exe run athena-core\examples\test-scenario.json 42
Seed: 42
Loading scenario...
Scenario loaded: Test Scenario
...
```

---

## Windows (Visual Studio 2022)

### Requisitos
1. Visual Studio 2022 com "Desktop development with C++"
2. vcpkg (opcional, para GUI)

### Compilação via Developer Command Prompt

```powershell
cd athena-core
mkdir build
cd build

cl /std:c++17 /EHsc /O2 /fp:strict /I..\include ^
   ..\src\types.cpp ..\src\json.cpp ..\src\rng.cpp ^
   ..\src\context.cpp ..\src\manifest.cpp ..\src\scenario.cpp ^
   ..\src\scheduler.cpp ..\src\serialization.cpp ^
   ..\src\terrain.cpp ..\src\terrain_semantics.cpp ^
   ..\src\pathfinding.cpp ..\src\platform_loader.cpp ^
   ..\src\environment.cpp ^
   ..\src\systems\movement.cpp ..\src\systems\combat.cpp ^
   ..\src\systems\logistics.cpp ..\src\systems\detection.cpp ^
   ..\src\systems\c2.cpp ^
   ..\src\analysis\montecarlo.cpp ..\src\analysis\sobol.cpp ^
   ..\src\analysis\binary_output.cpp ^
   ..\src\ai\doctrine.cpp ..\src\report\pdf_report.cpp ^
   ..\src\cli_main.cpp ^
   /link /OUT:athena-cli.exe
```

---

## Linux

### Requisitos
```bash
# CLI only
sudo apt install build-essential g++ make

# GUI (adicional)
sudo apt install libglfw3-dev libgl-dev
```

### Compilação
```bash
cd ATHENA

# CLI
make -f Makefile.unified athena-cli
./build/bin/athena-cli info

# GUI
make -f Makefile.unified athena
./build/bin/athena gui
```

---

## macOS

### Requisitos
```bash
xcode-select --install
# GUI (adicional)
brew install glfw
```

### Compilação
```bash
cd ATHENA

# CLI
make -f Makefile.unified athena-cli
./build/bin/athena-cli info

# GUI
make -f Makefile.unified athena
./build/bin/athena gui
```

---

## Flags de Compilação

| Flag | Propósito |
|------|-----------|
| `-std=c++17` | C++17 standard |
| `-O2` | Otimização |
| `-Wall -Wextra` | Warnings completos |
| `-g` | Debug symbols |
| `/fp:strict` (MSVC) | IEEE-754 strict (determinismo) |
| `-fno-fast-math` (GCC) | IEEE-754 strict |

---

## Troubleshooting

### "SIGINT redefinition"
- **Causa:** Macro Windows conflitando
- **Solução:** Já corrigido em v0.9.1 (renomeado para SIGNALS)

### "static_assert failed (BinaryIteration)"
- **Causa:** Tamanho de struct diferente
- **Solução:** Já corrigido em v0.9.1 (40 → 32 bytes)

### Warnings de "unused parameter"
- **Normal:** São apenas warnings, não impedem compilação
- **Ignorar:** `-Wno-unused-parameter` se desejar

---

## Arquivos de Build

| Arquivo | Plataforma | CLI | GUI |
|---------|------------|-----|-----|
| Makefile.mingw | Windows/MinGW | `mingw32-make -f Makefile.mingw cli` | `mingw32-make -f Makefile.mingw gui` |
| Makefile.unified | Linux/macOS | `make -f Makefile.unified athena-cli` | `make -f Makefile.unified athena` |

---

**Última atualização:** 2026-02-15
