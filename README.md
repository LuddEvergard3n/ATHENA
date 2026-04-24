# ATHENA Core

**Advanced Tactical & Heuristic Engagement & Network Analyzer**

Motor de wargaming determinístico para análise de cenários militares via simulação Monte Carlo.

**Versão:** 1.1.2 "Database"  
**Licença:** Proprietary  
**Build:** Linux x86_64 (g++ 13.3, -O2)

---

## Execução Imediata (Binários Pré-compilados)

Binários Linux x86_64 incluídos em `build/bin/`. Sem necessidade de compilação.

```bash
# GUI (requer: libGL, libglfw3, X11/Wayland)
./build/bin/athena

# CLI
./build/bin/athena-cli -h
./build/bin/athena-cli -s 12345 -n 100 athena-core/examples/test-scenario.json
```

**Dependências runtime (GUI):** `sudo apt install libglfw3 libgl1-mesa-glx`

---

## Características

- **Simulação Determinística** — Mesma seed = mesmos resultados, bit-exact
- **1,238 Plataformas Militares** — Tanques, aviões, navios, mísseis, submarinos, helicópteros
- **Monte Carlo Analysis** — Análise estatística com Sobol indices e convergência automática
- **6 Sistemas de Simulação** — Movement, Combat, Logistics, Detection, C2, Tactical AI
- **Terrain Semantics** — Terreno com impacto em movimento, combate e detecção
- **PDF Reports** — Geração de relatórios PDF sem dependências externas
- **MC Profiling** — Cronometragem por iteração (chrono timing)
- **Zero Dependências Externas** — C++17 autocontido, ImGui embutido

---

## Compilação (Opcional)

### Linux

```bash
sudo apt install build-essential g++ make libglfw3-dev libgl-dev
make -f Makefile.unified athena athena-cli    # GUI + CLI
make -f Makefile.unified athena-cli           # CLI apenas
make -f Makefile.unified test                 # Testes
```

### Windows (MinGW/MSYS2)

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

---

## Estrutura

```
ATHENA/
├── build/bin/                # Binários pré-compilados (Linux x86_64)
│   ├── athena                # GUI (2.2 MB, stripped)
│   └── athena-cli            # CLI (707 KB, stripped)
├── athena-core/
│   ├── include/athena/       # Headers públicos
│   ├── src/                  # Implementações C++
│   ├── data/
│   │   ├── platforms/        # 1,238 plataformas (168 JSONs, 25 categorias)
│   │   ├── nations/          # Dados nacionais
│   │   ├── schema/           # Schemas JSON
│   │   ├── units/            # Tipos de unidade
│   │   └── weapons/          # Sistemas de armas
│   ├── examples/             # Cenários de exemplo
│   ├── external/imgui/       # ImGui (embutido)
│   └── test/                 # Testes
├── Makefile.unified          # Build Linux/macOS
├── Makefile.mingw            # Build Windows
├── BUILDING.md               # Instruções detalhadas
├── CONTINUATION.md           # Estado e roadmap
└── README.md                 # Este arquivo
```

---

## Plataformas Militares (1,238 total)

| Categoria | Qtd | Destaques |
|-----------|-----|-----------|
| Aircraft | 112 | F-35, Gripen E, KC-390, Su-57 |
| Ships | 111 | Tamandaré, Constellation, Type 055 |
| Tanks | 102 | Osório, K2PL, Leopard 2A6 HEL, T-72M4CZ |
| Small Arms | 89 | |
| Submarines | 89 | Riachuelo, SN-10 Álvaro Alberto, Columbia |
| UAV | 85 | |
| Regional | 75 | 20+ países |
| Helicopters | 60 | AH-11B Super Lynx, Z-10, Apache |
| Missiles | 56 | |
| Artillery | 52 | |
| SAM | 46 | |
| EW Systems | 45 | |
| IFV | 44 | EE-9 Cascavel |
| APC | 40 | EE-11 Urutu, Guaraní |
| ATGM | 39 | |
| Outros | 193 | Munições, radares, MANPADS, C-UAS |

**Cobertura: US 183 | RU 132 | CN 106 | DE 34 | GB 33 | FR 32 | BR 26 | KR 18 | IL 15**

---

## Versões

| Versão | Codename | Data | Features |
|--------|----------|------|----------|
| 1.1.2 | Database | 2026-02-15 | +15 plataformas (8 nações), export Abrams armor fix |
| 1.1.1 | Terrain | 2026-02-13 | MC profiling, terrain AI avoidance, PDF reports |
| 1.1.0 | Terrain | 2026-02-13 | Terrain semantics full integration |
| 1.0.0 | Athena | 2026-02-10 | Release candidate |
| 0.9.3 | Doctrine | 2026-02-09 | AI/Doctrine system |

---

## Princípios de Design

1. **Determinismo bit-exact** — Reproducibilidade garantida
2. **Zero machine learning** — Regras explícitas e auditáveis
3. **C++17 puro** — Performance nativa, zero dependências interpretadas
4. **IEEE-754 strict** — Sem fast-math
5. **Dados auditáveis** — Fontes documentadas (Jane's, IISS, SIPRI)

---

Proprietary — Copyright (c) 2026 ATHENA Project
