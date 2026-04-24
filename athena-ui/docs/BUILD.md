> **⚠️ LEGACY:** Este documento refere-se à interface Qt6, removida na v0.8.1. A interface atual usa ImGui+GLFW.
> Consulte `Makefile.unified` para instruções de build atuais.

# ATHENA UI - Build Instructions

## Prerequisites

### Qt 6.x Commercial License

ATHENA UI requires Qt 6.x with a commercial license. Download from:
https://www.qt.io/pricing

Required Qt modules:
- Qt Core
- Qt Gui  
- Qt Widgets
- Qt Charts
- Qt OpenGL Widgets

### Compiler

- **Windows:** MSVC 2019 or later (Visual Studio)
- **Linux:** GCC 10+ or Clang 12+
- **macOS:** Clang (Xcode Command Line Tools)

### CMake

CMake 3.16 or later required.

---

## Build Steps

### 1. Set Qt Path

```bash
# Linux/macOS
export CMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64

# Windows (PowerShell)
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.x.x\msvc2019_64"
```

### 2. Configure

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### 3. Build

```bash
# Linux/macOS
cmake --build . --parallel

# Windows
cmake --build . --config Release --parallel
```

### 4. Run

```bash
# Linux/macOS
./athena-ui

# Windows
.\Release\athena-ui.exe
```

---

## Windows Deployment

After building, run `windeployqt` to copy Qt DLLs:

```powershell
cd build\Release
C:\Qt\6.x.x\msvc2019_64\bin\windeployqt.exe athena-ui.exe
```

---

## Linux Deployment

Option 1: System Qt (if user has Qt installed)
```bash
./athena-ui
```

Option 2: Bundle Qt libraries with `linuxdeployqt`:
```bash
linuxdeployqt athena-ui -appimage
```

---

## Project Structure

```
athena-ui/
├── CMakeLists.txt          # Build configuration
├── src/
│   ├── main.cpp            # Entry point
│   ├── app/
│   │   └── mainwindow.*    # Main application window
│   ├── widgets/
│   │   ├── mapview.*       # Tactical map widget
│   │   ├── platformbrowser.*   # Platform database browser
│   │   ├── forcetree.*     # Force composition tree
│   │   ├── simulationpanel.*   # Monte Carlo controls
│   │   └── resultspanel.*  # Results dashboard
│   ├── graphics/
│   │   ├── mapscene.*      # QGraphicsScene for map
│   │   ├── terrainlayer.*  # Terrain rendering
│   │   ├── unititem.*      # Unit symbols (NATO)
│   │   └── heatmaplayer.*  # Heatmap overlay
│   └── models/
│       ├── platformmodel.* # Platform database model
│       └── forcemodel.*    # Force composition model
├── resources/
│   ├── athena.qrc          # Qt resource file
│   ├── icons/              # Application icons
│   ├── symbols/            # NATO military symbols
│   └── themes/             # UI themes (dark/light)
└── docs/
    └── BUILD.md            # This file
```

---

## Integration with ATHENA Core

The UI links to `athena-core` library for:
- Platform database loading (`PlatformLoader`)
- Simulation execution (`MonteCarlo`, `Sobol`)
- Terrain data (`TerrainSystem`)

Ensure `athena-core` is built and the include path is configured:

```cmake
target_include_directories(athena-ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../athena-core/include
)

target_link_libraries(athena-ui PRIVATE
    athena-core  # When library target is available
)
```

---

## Development Notes

### Adding New Widgets

1. Create `.hpp` and `.cpp` in appropriate `src/` subdirectory
2. Add to `ATHENA_UI_SOURCES` and `ATHENA_UI_HEADERS` in CMakeLists.txt
3. Include `Q_OBJECT` macro if using signals/slots

### Adding Resources

1. Add file to `resources/` directory
2. Add entry to `resources/athena.qrc`
3. Access in code: `QIcon(":/icons/myicon.png")`

### Debugging

Enable debug output:
```bash
./athena-ui --debug
```

Qt debug categories:
```cpp
qDebug() << "Debug message";
qWarning() << "Warning message";
qCritical() << "Critical message";
```

---

## License

Qt Commercial License required for distribution.
ATHENA Core and UI are proprietary software.
