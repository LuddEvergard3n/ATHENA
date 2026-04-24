# ATHENA Development Backlog

**Versão:** 1.1.2  
**Última atualização:** 2026-02-15

---

## Status Geral

| Componente | Status | Notas |
|------------|--------|-------|
| Core Simulation | ✅ Completo | 6 sistemas funcionais |
| Platform Database | ✅ Completo | 1,238 plataformas, 168 JSONs |
| Monte Carlo | ✅ Completo | Convergência automática, profiling |
| Sobol Analysis | ✅ Completo | Sensitivity indices |
| CLI Interface | ✅ Completo | athena-cli funcional |
| ImGui GUI | ✅ Completo | integrated_gui.cpp (1,945 linhas) |
| AI/Doctrine | ✅ Completo | Behavior trees, 5 presets |
| Tactical AI | ✅ Completo | Seek enemy, terrain avoidance |
| Terrain Integration | ✅ Completo | Todos os sistemas conectados |
| Morale Propagation | ✅ Completo | POST_TICK, boost/penalty |
| MC Convergence | ✅ Completo | Wilson score CI, auto-stop |
| MC Profiling | ✅ Completo | chrono, ms/iter, throughput |
| PDF Reports | ✅ Completo | PDF 1.4, Helvetica, zero deps |
| Pre-compiled Binaries | ✅ Completo | Linux x86_64 + Windows x64 |
| Qt6 GUI | 🗄️ Legacy | Código mantido como referência, não compilado |

---

## Plataformas Militares — COMPLETO (v1.1.2)

### Ground Systems
| Categoria | Plataformas | Status |
|-----------|-------------|--------|
| Tanks | 102 | ✅ Completo |
| IFV | 44 | ✅ Completo |
| APC | 40 | ✅ Completo |
| Artillery | 52 | ✅ Completo |
| MLRS | 20 | ✅ Completo |
| SAM | 46 | ✅ Completo |
| ATGM | 39 | ✅ Completo |
| MANPADS | 14 | ✅ Completo |

### Air Systems
| Categoria | Plataformas | Status |
|-----------|-------------|--------|
| Aircraft | 112 | ✅ Completo |
| Helicopters | 60 | ✅ Completo |
| Bombers | 12 | ✅ Completo |
| UAV | 85 | ✅ Completo |

### Naval Systems
| Categoria | Plataformas | Status |
|-----------|-------------|--------|
| Ships | 111 | ✅ Completo |
| Submarines | 89 | ✅ Completo |

### Support Systems
| Categoria | Plataformas | Status |
|-----------|-------------|--------|
| Small Arms | 89 | ✅ Completo |
| EW Systems | 45 | ✅ Completo |
| Missiles | 56 | ✅ Completo |
| Munitions | 51 | ✅ Completo |
| Radars | 20 | ✅ Completo |
| Counter-UAS | 16 | ✅ Completo |
| Optics | 16 | ✅ Completo |
| Engineering | 15 | ✅ Completo |
| Body Armor | 15 | ✅ Completo |
| Comms | 14 | ✅ Completo |
| Regional | 75 | ✅ Completo |

**TOTAL: 1,238 plataformas em 25 categorias**

---

## Backlog de Desenvolvimento (v1.2+)

### Alta Prioridade

1. **100k MC Benchmark**
   - Profiling real com 100k iterações
   - Usar bench_mc.cpp existente
   - Status: ⏳ Próximo ciclo

2. **A* Pathfinding com Terrain Cost**
   - Rotas longas usando movement_cost do terreno
   - pathfinding.cpp já existe (405 linhas), precisa integrar terrain
   - Status: ⏳ Próximo ciclo

### Média Prioridade

3. **Terrain Editor Visual**
   - Click-drag zonas de floresta/urbano na GUI
   - Status: ⏳ Planejado

4. **PDF Multi-página**
   - Relatórios estendidos para cenários complexos
   - Status: ⏳ Planejado

### Baixa Prioridade

5. **C# Bindings**
   - Native wrapper para Windows
   - Status: ⏳ Planejado

6. **Network Play**
   - Multi-user scenarios
   - Distributed simulation
   - Status: ⏳ Planejado

---

## Métricas do Projeto

| Métrica | Valor |
|---------|-------|
| Arquivos de código | 82 .cpp |
| Linhas C++ (próprias) | ~32,200 |
| ImGui vendor | ~60,500 linhas |
| Testes | 15 + 1 benchmark |
| Testes passando | 100% |
| Sistemas | 6 |
| Plataformas | 1,238 |
| Categorias | 25 |
| JSONs | 168 |
| Dependências externas | 0 |

---

## Histórico de Marcos

| Versão | Data | Marco |
|--------|------|-------|
| 1.1.2 | 2026-02-15 | +15 plataformas, binários pré-compilados Linux+Windows |
| 1.1.1 | 2026-02-13 | MC profiling, terrain avoidance AI, PDF reports |
| 1.1.0 | 2026-02-13 | Terrain integration completa, morale, MC convergence |
| 1.0.0 | 2026-02-10 | Tactical AI compartilhado (MC funcional) |
| 0.9.3 | 2026-02-09 | AI/Doctrine com behavior trees |
| 0.9.0 | 2026-02-01 | Unified integration (GUI+CLI+TUI) |
| 0.8.9 | 2026-01-31 | 1,235 plataformas |
| 0.8.6 | 2026-01-28 | ImGui GUI + TUI browser |
| 0.8.1 | 2026-01-23 | Qt6 removido |
| 0.6.0 | 2026-01-23 | Qt6 UI integration, naval database |
| 0.5.0 | 2026-01-20 | Air domain, platform loader |
| 0.2.0 | 2026-01-10 | Core engine foundation |

---

**Próxima versão planejada:** v1.2 (100k Benchmark + A* Pathfinding)
