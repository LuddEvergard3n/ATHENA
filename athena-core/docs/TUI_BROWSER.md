# ATHENA Platform Browser - Terminal UI

Interface de terminal para navegação do banco de dados de plataformas militares.

## Build

### Requisitos
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Windows 10+ / macOS / Linux

### Compilação

```bash
# Usando Makefile
make -f Makefile.browser

# Ou manualmente
g++ -std=c++17 -I include -O2 \
    src/tui/terminal.cpp \
    src/tui/widgets.cpp \
    src/tui/browser.cpp \
    src/tui/browser_main.cpp \
    src/platform_loader.cpp \
    src/json.cpp \
    -o athena-browser
```

## Uso

```bash
./athena-browser [data_path]

# Exemplos:
./athena-browser                    # Usa data/platforms
./athena-browser /path/to/data      # Caminho customizado
./athena-browser --help             # Mostra ajuda
```

## Controles

| Tecla | Ação |
|-------|------|
| Tab | Alternar foco entre painéis |
| / | Focar caixa de busca |
| ↑↓ | Navegar lista |
| ←→ | Expandir/recolher |
| Enter | Selecionar / Abrir detalhes |
| Escape | Fechar detalhes / Limpar busca |
| PgUp/PgDn | Página anterior/próxima |
| Home/End | Início/fim da lista |
| Ctrl+Q | Sair |

## Interface

```
┌─ ATHENA Platform Browser ────────────────────────────────────┐
│                                                              │
│  SEARCH          │ Platforms (1238)                          │
│  ┌────────────┐  │ ┌──────────────────────────────────────┐ │
│  │ Type...    │  │ │ NAME              TYPE        CTRY   │ │
│  └────────────┘  │ │ ------------------------------------ │ │
│                  │ │ M1A2 SEPv3 Abrams mbt          US    │ │
│  CATEGORIES      │ │ T-90M Proryv      mbt          RU    │ │
│  [*] All      1238│ │ Type 99A          mbt          CN    │ │
│  [T] Tanks    87 │ │ Leopard 2A7       mbt          DE    │ │
│  [F] Aircraft 74 │ │ Challenger 3      mbt          GB    │ │
│  [N] Ships    65 │ │ K2 Black Panther  mbt          KR    │ │
│  [W] SmallArms83 │ │ Merkava Mk4       mbt          IL    │ │
│  ...             │ │ ...                                   │ │
│                  │ └──────────────────────────────────────┘ │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ [Tab] Focus  [/] Search  [Enter] Select  [Esc] Back  [^Q] Quit│
└──────────────────────────────────────────────────────────────┘
```

## Arquitetura

```
src/tui/
├── terminal.cpp    # Abstração do terminal (ANSI, raw mode)
├── widgets.cpp     # Componentes UI (ListView, TextInput, etc.)
├── browser.cpp     # Lógica do browser
└── browser_main.cpp # Entry point

include/athena/tui/
├── terminal.hpp    # Terminal API
├── widgets.hpp     # Widget classes
└── browser.hpp     # Browser class
```

## Features

- **Cross-platform**: Windows Terminal, macOS Terminal, Linux
- **Zero dependências**: Apenas C++ padrão
- **Busca em tempo real**: Filtra enquanto digita
- **Categorias**: 25 categorias navegáveis
- **Detalhes expandidos**: Specs, armamento, proteção
- **1,238 plataformas**: Banco de dados completo

## Migração para Dear ImGui

Os conceitos de UI foram projetados para facilitar migração futura:

| TUI Componente | Dear ImGui Equivalente |
|----------------|------------------------|
| ListView | ImGui::ListBox |
| TableView | ImGui::Table |
| TextInput | ImGui::InputText |
| Panel | ImGui::Begin/End |
| DetailView | ImGui::Text, ImGui::Columns |

## Versão

v1.1.2 - 2026-02-15
