# ATHENA Core

Deterministic C++17 wargaming engine for auditable military scenario analysis and Monte Carlo simulation.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-Build-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![ImGui](https://img.shields.io/badge/ImGui-Interface-1F6FEB)](https://github.com/ocornut/imgui)
[![License](https://img.shields.io/badge/License-Proprietary-lightgrey)](#license)

**Advanced Tactical & Heuristic Engagement & Network Analyzer**

## Overview

ATHENA models military scenarios through explicit, inspectable rules. A fixed seed produces bit-exact results, making simulations reproducible and suitable for comparison, debugging, and audit.

**Version:** 1.1.2 "Database"
**Reference build:** Linux x86_64, g++ 13.3, `-O2`

## Highlights

- Deterministic, bit-exact simulation from a fixed seed.
- 1,238 military platforms across 25 categories.
- Monte Carlo analysis with Sobol indices and automatic convergence.
- Movement, combat, logistics, detection, command-and-control, and tactical AI systems.
- Terrain effects integrated into movement, combat, detection, and AI decisions.
- Dependency-free PDF reports and per-iteration profiling.
- Self-contained C++17 core with embedded ImGui.

## Run the prebuilt binaries

Linux x86_64 binaries are included in `build/bin/`.

```bash
# GUI: requires libGL, libglfw3, and X11 or Wayland
./build/bin/athena

# CLI
./build/bin/athena-cli -h
./build/bin/athena-cli -s 12345 -n 100 athena-core/examples/test-scenario.json
```

GUI runtime dependencies on Debian-based systems:

```bash
sudo apt install libglfw3 libgl1-mesa-glx
```

## Build

### Linux

```bash
sudo apt install build-essential g++ make libglfw3-dev libgl-dev
make -f Makefile.unified athena athena-cli
make -f Makefile.unified athena-cli
make -f Makefile.unified test
```

### Windows with MinGW/MSYS2

```cmd
pacman -S mingw-w64-x86_64-glfw
set PATH=C:\mingw64\bin;%PATH%
mingw32-make -f Makefile.mingw athena athena-cli
```

### macOS

```bash
brew install glfw
make -f Makefile.unified athena athena-cli
```

## Project structure

```text
ATHENA/
|-- build/bin/                 Prebuilt Linux x86_64 binaries
|-- athena-core/
|   |-- include/athena/        Public headers
|   |-- src/                   C++ implementations
|   |-- data/                  Platforms, nations, schemas, units, and weapons
|   |-- examples/              Example scenarios
|   |-- external/imgui/        Embedded ImGui source
|   `-- test/                  Tests
|-- athena-ui/                 Graphical interface
|-- Makefile.unified           Linux and macOS build
|-- Makefile.mingw             Windows build
|-- BUILDING.md                Detailed build instructions
`-- CONTINUATION.md            Current state and roadmap
```

## Dataset

| Category | Count | Examples |
|---|---:|---|
| Aircraft | 112 | F-35, Gripen E, KC-390, Su-57 |
| Ships | 111 | Tamandare, Constellation, Type 055 |
| Tanks | 102 | Osorio, K2PL, Leopard 2A6 HEL, T-72M4CZ |
| Small arms | 89 | Various systems |
| Submarines | 89 | Riachuelo, SN-10 Alvaro Alberto, Columbia |
| UAVs | 85 | Multiple classes |
| Regional systems | 75 | More than 20 countries |
| Other categories | 575 | Helicopters, missiles, artillery, air defense, EW, IFVs, APCs, and ATGMs |

Country coverage includes the United States, Russia, China, Germany, the United Kingdom, France, Brazil, South Korea, and Israel.

## Design principles

1. **Bit-exact determinism:** identical seed and input produce identical output.
2. **No machine learning:** behavior comes from explicit and auditable rules.
3. **Native C++17:** no interpreted runtime dependency.
4. **Strict IEEE-754 behavior:** builds do not use fast-math.
5. **Auditable data:** documented sources include Jane's, IISS, and SIPRI.

## Limitations

ATHENA is a scenario-analysis and educational engine. Its data and rules do not predict real-world outcomes, and the repository should not be treated as an operational military system.

## License

Proprietary. Copyright (c) 2026 ATHENA Project.
