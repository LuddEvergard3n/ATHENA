# ATHENA Platform Browser - GUI (Dear ImGui)

Interface gráfica para navegação do banco de dados de plataformas militares.

## Requisitos

### Dependências
- **C++17** compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **GLFW3** - Window/input handling
- **OpenGL 3.3+** - Graphics

### Instalação de Dependências

**Ubuntu/Debian:**
```bash
sudo apt install libglfw3-dev libgl1-mesa-dev
```

**macOS (Homebrew):**
```bash
brew install glfw
```

**Windows (vcpkg):**
```bash
vcpkg install glfw3
```

**Windows (Manual):**
1. Download GLFW de https://www.glfw.org/download.html
2. Extraia para `external/glfw`
3. Ajuste paths no Makefile.gui

## Build

```bash
# Verificar dependências e compilar
make -f Makefile.gui

# Ou build manual
g++ -std=c++17 -O2 -I include -I external/imgui \
    external/imgui/imgui*.cpp \
    src/gui/browser.cpp \
    src/gui/gui_main.cpp \
    src/platform_loader.cpp \
    src/json.cpp \
    -lglfw -lGL -ldl -lpthread \
    -o athena-gui
```

## Uso

```bash
./athena-gui [data_path]

# Exemplos:
./athena-gui                    # Usa data/platforms
./athena-gui /custom/path       # Caminho customizado
./athena-gui --help             # Mostra ajuda
```

## Interface

```
┌─────────────────────────────────────────────────────────────────────┐
│ File   View   Help                    Platforms: 1238 | Filtered: 102 │
├──────────────┬──────────────────────────────────────────────────────┤
│              │                                                      │
│   ATHENA     │  Platforms (102)                      [Compare]       │
│   Platform   │  ─────────────────────────────────────────────────   │
│   Database   │  Name              Type        Country  Year  Cat    │
│              │  ─────────────────────────────────────────────────   │
│  Search      │  M1A2 SEPv3 Abrams mbt         US       -     Tanks  │
│  ┌────────┐  │  T-90M Proryv      mbt         RU       -     Tanks  │
│  │ Type.. │  │  Type 99A          mbt         CN       -     Tanks  │
│  └────────┘  │  Leopard 2A7       mbt         DE       -     Tanks  │
│              │  K2 Black Panther  mbt         KR       -     Tanks  │
│  Categories  │  Merkava Mk4       mbt         IL       -     Tanks  │
│  ┌────────┐  │  ...                                                 │
│  │All  1238│  │                                                      │
│  │Tanks102│  │                                                      │
│  │Aircr112│  │                                                      │
│  │Ships111│  │                                                      │
│  │...     │  │                                                      │
│  └────────┘  │                                                      │
│              │                                                      │
│  Filters     │                                                      │
│  Country:    │                                                      │
│  [All     ▼] │                                                      │
│              │                                                      │
│  [Clear All] │                                                      │
│              │                                                      │
├──────────────┴──────────────────────────────────────────────────────┤
│ Selected: M1A2 SEPv3 Abrams              Compare: 2    ATHENA v1.1.2│
└─────────────────────────────────────────────────────────────────────┘
```

## Features

### Navegação
- **Tabela interativa** com scroll e seleção
- **Busca em tempo real** por nome, tipo, fabricante
- **Filtros** por categoria e país
- **Ordenação** por colunas (clique no header)

### Detalhes
- **Painel de detalhes** com todas as especificações
- **Seções colapsáveis**: Identity, Physical, Mobility, Armament, Protection
- **Ratings** com barras de progresso visuais

### Comparação
- **Adicionar até 5** plataformas para comparar
- **Tabela side-by-side** com specs principais
- Botões `+`/`-` em cada linha da tabela

### Menu
- `File > Exit` - Sair
- `View > Detail Panel` - Toggle painel de detalhes
- `View > Compare Panel` - Toggle comparação
- `View > ImGui Demo` - Demo do Dear ImGui
- `Help > About` - Informações do sistema

## Arquitetura

```
src/gui/
├── browser.cpp     # Lógica do browser e renderização
└── gui_main.cpp    # Entry point, setup GLFW/OpenGL

include/athena/gui/
└── browser.hpp     # Classes e estado da aplicação

external/imgui/
├── imgui.cpp/h             # Core
├── imgui_draw.cpp          # Rendering
├── imgui_tables.cpp        # Table widget
├── imgui_widgets.cpp       # Widgets
├── imgui_impl_glfw.cpp/h   # GLFW backend
└── imgui_impl_opengl3.cpp/h # OpenGL backend
```

## Customização

### Tema
O tema escuro militar está em `BrowserApp::setup_style()`. Cores principais:

```cpp
colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
colors[ImGuiCol_Header] = ImVec4(0.18f, 0.35f, 0.55f, 0.80f);
colors[ImGuiCol_Button] = ImVec4(0.20f, 0.35f, 0.50f, 1.00f);
```

### Fontes
Para usar fonte customizada:

```cpp
// Em gui_main.cpp, após criar contexto
io.Fonts->AddFontFromFileTTF("fonts/Roboto-Regular.ttf", 16.0f);
```

## Troubleshooting

### "GLFW not found"
```bash
# Verificar instalação
pkg-config --exists glfw3 && echo "OK" || echo "Not found"

# Ubuntu/Debian
sudo apt install libglfw3-dev

# macOS
brew install glfw
```

### "OpenGL 3.3 not supported"
Verifique suporte OpenGL:
```bash
glxinfo | grep "OpenGL version"
```

Requer OpenGL 3.3+. Drivers antigos ou VMs podem não suportar.

### "Segfault on startup"
1. Verifique se o caminho `data/platforms` existe
2. Verifique se há arquivos JSON válidos
3. Execute com `--help` para testar

## Comparação TUI vs GUI

| Aspecto | TUI | GUI |
|---------|-----|-----|
| Dependências | Nenhuma | GLFW, OpenGL |
| Performance | Baixa | Alta |
| Usabilidade | Teclado | Mouse + Teclado |
| Portabilidade | SSH, headless | Desktop only |
| Visual | ASCII | Gráfico |

## Versão

v1.1.2 - 2026-02-15

- Dear ImGui 1.90.1
- GLFW 3.x backend
- OpenGL 3.3 renderer
