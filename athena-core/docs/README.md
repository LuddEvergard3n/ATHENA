# ATHENA Core

**Wargaming engine determinístico para análise de cenários militares.**

Versão: 1.1.2 "Database"  
Schema: 1.0  
Licença: Proprietary

## Princípios Fundamentais

1. **Determinismo bit-exact** — Mesma seed = mesmos resultados, sempre
2. **Auditabilidade total** — Todo parâmetro documentado, toda execução rastreada
3. **Zero dependências externas** — Código autocontido (ImGui vendorizado)
4. **IEEE-754 strict** — Sem fast-math, sem FMA implícito

## Estrutura do Projeto

```
athena-core/
├── include/
│   ├── athena.hpp                    # API principal
│   └── athena/
│       ├── types.hpp                 # Tipos, Result<T>, ErrorCode
│       ├── rng.hpp                   # PCG64 RNG determinístico
│       ├── context.hpp               # Contexto de simulação
│       ├── entities.hpp              # SoA entity storage
│       ├── scheduler.hpp             # Scheduler por fases
│       ├── manifest.hpp              # Manifesto + SHA-256
│       ├── json.hpp                  # Parser JSON
│       ├── scenario.hpp              # Loader de cenários
│       ├── terrain.hpp               # Layer 1: Physical terrain
│       ├── environment.hpp           # Layer 2: Environmental modifiers
│       ├── terrain_semantics.hpp     # Layer 3: Derived meaning
│       ├── platform_loader.hpp       # Database de plataformas
│       ├── pathfinding.hpp           # A* pathfinding
│       ├── serialization.hpp         # Binary I/O
│       ├── integrated.hpp            # GUI app integrada
│       ├── systems/
│       │   ├── movement.hpp
│       │   ├── combat.hpp
│       │   ├── logistics.hpp
│       │   ├── detection.hpp
│       │   ├── c2.hpp
│       │   └── tactical_ai.hpp
│       ├── ai/
│       │   └── doctrine.hpp
│       ├── report/
│       │   └── pdf_report.hpp
│       └── analysis/
│           ├── montecarlo.hpp
│           ├── sobol.hpp
│           └── binary_output.hpp
├── src/                              # 35 implementações
├── test/                             # 15 testes
├── tests/                            # bench_mc.cpp (benchmark)
├── data/platforms/                   # 168 JSONs, 1,238 plataformas
├── external/imgui/                   # ImGui vendorizado
└── examples/
    ├── baltics-2025.json
    └── test-scenario.json
```

## Compilação

### Linux (GCC)

```bash
cd ATHENA
make -f Makefile.unified athena-cli   # CLI
make -f Makefile.unified athena       # GUI (requer libglfw3-dev, libgl-dev)
make -f Makefile.unified test         # Testes
```

### Windows (MinGW)

```cmd
set PATH=C:\mingw64\bin;%PATH%
mingw32-make -f Makefile.mingw cli
mingw32-make -f Makefile.mingw gui    # Requer GLFW3
```

### macOS

```bash
brew install glfw
make -f Makefile.unified athena athena-cli
```

### Flags Obrigatórias

| Flag | Propósito |
|------|-----------|
| `-fno-fast-math` | IEEE-754 compliance |
| `-ffp-contract=off` | Sem FMA implícito |
| `/fp:strict` | Equivalente MSVC |

## Sistema de Terreno (3 Camadas)

ATHENA usa uma arquitetura de terreno em 3 camadas independentes:

```
┌─────────────────────────────────────────────────────────────────────┐
│  CAMADA 3: Semântica (DERIVADA - nunca armazenada)                  │
│  - movement_cost(x, y, t, type) → f64                               │
│  - defense_value(x, y, t) → {cover, concealment, advantage}         │
│  - sensor_effectiveness(x, y, t, sensor) → modifiers                │
└─────────────────────────────────────────────────────────────────────┘
                              ▲ deriva de
┌─────────────────────────────┴───────────────────────────────────────┐
│  CAMADA 2: Ambiental (varia com tempo)                              │
│  - Weather: clear, rain, snow, fog, blizzard, sandstorm            │
│  - Climate: temperate, arctic, tropical, arid                       │
│  - Events: flood, fire, landslide, chemical, nuclear               │
└─────────────────────────────────────────────────────────────────────┘
                              ▲ modifica
┌─────────────────────────────┴───────────────────────────────────────┐
│  CAMADA 1: Física (imutável durante simulação)                      │
│  - TerrainBase: FLAT, HILLS, MOUNTAINS, VALLEY, PLATEAU            │
│  - Overlays: FOREST, URBAN, SWAMP, RIVER, LAKE, ROAD, BRIDGE       │
│  - Gerado proceduralmente via Perlin Noise                          │
└─────────────────────────────────────────────────────────────────────┘
```

### Terrain Integration (v1.1.0+)

Terreno ativo em todos os sistemas de simulação:

| Sistema | Efeito |
|---------|--------|
| Movement | Velocidade /= movement_cost (floresta=1.5x, urbano=1.2x) |
| Combat LOS | Engajamento bloqueado se LOS < 0.1 |
| Defense | Cover + concealment do terreno reduzem dano recebido |
| Detection | Concealment reduz efetividade de sensores |
| Supply | Terrain logistics_penalty aumenta consumo |
| Tactical AI | is_passable() + 8 direções alternativas |

## Sistemas Implementados

| Sistema | Descrição | Status |
|---------|-----------|--------|
| Movement | Waypoints, fuel, terrain modifiers | ✅ |
| Combat | Lanchester attrition, LOS, cover | ✅ |
| Logistics | Fuel/ammo consumption, resupply | ✅ |
| Detection | Fog of war, contacts, LOS, concealment | ✅ |
| C2 | Command hierarchy, orders, comms, jamming | ✅ |
| Tactical AI | Seek enemy, terrain avoidance | ✅ |
| AI/Doctrine | Behavior trees, 5 presets | ✅ |
| Morale | Propagation, ally boost, casualty penalty | ✅ |
| Terrain | 3-layer procedural, full integration | ✅ |

## Monte Carlo Analysis

| Feature | Status |
|---------|--------|
| Batch execution (1 a 100k+ iterações) | ✅ |
| Convergência automática (Wilson score CI) | ✅ |
| Profiling chrono (ms/iter, throughput) | ✅ |
| Sensitivity analysis (Sobol indices) | ✅ |
| PDF report export | ✅ |
| JSON/binary export | ✅ |

## Métricas

| Categoria | Valor |
|-----------|-------|
| Arquivos de código (.cpp) | 82 |
| Linhas de código (próprias) | ~32,200 |
| Testes | 15 + 1 benchmark |
| Sistemas | 6 |
| Plataformas militares | 1,238 |
| Categorias | 25 |
| Dependências externas | 0 |

## Garantias de Determinismo

1. **RNG único** — PCG64 com streams derivados por fase
2. **Ordem explícita** — Entidades processadas por índice
3. **Sem containers desordenados** — std::map em vez de std::unordered_map
4. **IEEE-754 strict** — Mesmas operações FP em todas plataformas
5. **Manifest completo** — SHA-256 de inputs e outputs
6. **Terrain procedural** — Mesmo seed = mesmo terreno

## Licença

Proprietary. Copyright (c) 2026 ATHENA Project.
