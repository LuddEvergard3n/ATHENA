# ATHENA UI Specification

**Version:** 1.1.2  
**Date:** 2026-02-15  
**Status:** Design Phase

## Overview

ATHENA é uma ferramenta de análise militar, não um jogo. O workflow principal é:

```
Configurar Cenário → Executar N Simulações → Analisar Resultados
```

### Princípios de Design

1. **Função sobre forma** - UI utilitária, não decorativa
2. **Performance** - Nativo C++ (Qt ou ImGui), sem Electron/web
3. **Batch-oriented** - Otimizado para rodar milhares de simulações
4. **Data-driven** - Foco em análise estatística, não animação
5. **Reprodutibilidade** - Cenários e resultados exportáveis

### O que NÃO terá

- Animação em tempo real de unidades
- Controle manual durante simulação
- Gráficos 3D elaborados
- Som/efeitos
- Gamificação

---

## Layout Geral

```
┌─────────────────────────────────────────────────────────────┐
│  [Scenario] [Simulation] [Results]              ATHENA 0.6  │
├───────────────────────┬─────────────────────────────────────┤
│                       │                                     │
│                       │         Main Panel                  │
│     Side Panel        │    (muda conforme aba ativa)        │
│                       │                                     │
│   - Force List        │    Scenario: Mapa + Editor          │
│   - Platform Browser  │    Simulation: Controles + Progress │
│   - Properties        │    Results: Gráficos + Tabelas      │
│                       │                                     │
├───────────────────────┴─────────────────────────────────────┤
│  Status Bar: "Ready" | "Running 847/1000" | "Complete"      │
└─────────────────────────────────────────────────────────────┘
```

---

## 1. Scenario Editor (Configuração)

### 1.1 Terrain/Region

| Elemento | Descrição |
|----------|-----------|
| Region Selector | Biblioteca de regiões pré-definidas (Bálticos, Taiwan Strait, Fulda Gap, etc) ou importar custom |
| Terrain Layers | Toggle de camadas: elevação, vegetação, urbano, água, estradas, bridges |
| Terrain Properties | Editar modificadores por célula se necessário (ex: destruir ponte) |
| Weather Presets | Condições que afetam toda a região (chuva, neve, neblina, noite) |
| Season | Afeta vegetação, mobilidade em lama, horas de luz |

### 1.2 Force Composition

| Elemento | Descrição |
|----------|-----------|
| Side Selector | BLUFOR, OPFOR, NEUTRAL (pode ter 3+ lados) |
| OOB Builder | Organizar em hierarquia: Army → Division → Brigade → Battalion → Company |
| Platform Browser | Filtrar 453 plataformas por país, tipo, era, role |
| Quick Templates | Carregar TOE padrão (US BCT, Russian BTG, Chinese Combined Arms) |
| Customization | Ajustar crew quality, supply level, ammunition load por unidade |
| Validation | Verificar se composição é plausível (ex: MANPADS sem infantaria?) |

### 1.3 Deployment

| Elemento | Descrição |
|----------|-----------|
| Deployment Zones | Definir áreas permitidas por lado (retângulos, polígonos) |
| Unit Placement | Drag-and-drop ou coordenadas exatas |
| Formation Presets | Line, wedge, column, dispersed |
| Facing/Orientation | Direção inicial das unidades |
| Dig-in Level | Entrincheirado, hasty defense, mobile |
| Hidden Setup | Unidades ocultas até detectadas (fog of war inicial) |

### 1.4 Objectives & Victory

| Elemento | Descrição |
|----------|-----------|
| Control Points | Áreas que precisam ser capturadas/mantidas |
| Phase Lines | Linhas de avanço (para medir progresso) |
| Time Limit | Duração máxima da simulação |
| Victory Conditions | Compostas: "BLUE wins if (holds X AND casualties < 30%) OR (destroys 70% OPFOR)" |
| Stalemate Definition | Quando considerar empate |

### 1.5 Rules of Engagement

| Elemento | Descrição |
|----------|-----------|
| Fire Discipline | Free fire, return fire only, hold fire |
| Engagement Ranges | Máximo/mínimo por tipo de unidade |
| Priority Targets | Ordem de prioridade (ex: SAM > tanks > infantry) |
| Retreat Threshold | % de perdas para unidade recuar |
| Surrender Threshold | % de perdas para unidade render |

---

## 2. Simulation Control (Execução)

### 2.1 Monte Carlo Settings

| Elemento | Descrição |
|----------|-----------|
| Iterations | Número de runs (100, 1000, 10000, custom) |
| Confidence Target | Rodar até atingir confiança estatística (ex: 95% CI width < 2%) |
| Batch Size | Agrupar runs para checkpoints |
| Early Termination | Parar se resultado já é estatisticamente conclusivo |

### 2.2 Parameter Uncertainty

| Elemento | Descrição |
|----------|-----------|
| Detection Ranges | ±X% variação |
| Hit Probability | ±X% variação |
| Weapon Reliability | Chance de falha |
| Communication | Chance de falha de C2 |
| Supply Consumption | Variação no consumo |
| Crew Quality | Modificador por experiência |
| Custom Parameters | Adicionar qualquer variável ao pool de incerteza |

### 2.3 Sensitivity Analysis

| Elemento | Descrição |
|----------|-----------|
| Sobol Analysis | Ativar/desativar |
| Parameters to Analyze | Selecionar quais incluir na análise |
| Sample Size | N para Sobol (tipicamente 1024+) |

### 2.4 Execution Control

| Elemento | Descrição |
|----------|-----------|
| Thread Count | Manual ou auto-detect |
| Priority | Background / Normal / High |
| Memory Limit | Cap para evitar swap |
| Checkpointing | Salvar estado a cada N runs |
| Resume | Continuar execução interrompida |
| Seed Management | Fixed seed (reproducible) ou random |

### 2.5 Progress & Monitoring

| Elemento | Descrição |
|----------|-----------|
| Progress Bar | X / N runs completas |
| ETA | Tempo estimado restante |
| Live Stats | Win rate parcial, casualties médias (atualizando) |
| Throughput | Runs/segundo |
| Resource Monitor | CPU%, RAM%, threads ativos |
| Log Stream | Warnings, errors em tempo real |
| **Pause / Cancel** | Controle de execução |

---

## 3. Results Dashboard (Análise)

### 3.1 Overview

| Elemento | Descrição |
|----------|-----------|
| Win/Loss/Draw | Pie chart ou barras |
| Confidence Interval | Ex: "BLUE wins 67% ± 2.3% (95% CI)" |
| Runs Summary | Total, válidas, failed, outliers |
| Duration Stats | Mean, median, std dev, min, max |

### 3.2 Casualty Analysis

| Elemento | Descrição |
|----------|-----------|
| By Side | Total losses BLUE vs RED |
| By Platform Type | Tanks, IFV, aircraft, etc |
| By Specific Platform | M1A2 vs T-90M performance |
| By Cause | Destroyed by: tanks, ATGMs, artillery, air |
| Kill/Death Ratios | Por plataforma |
| Survival Curves | % força restante ao longo do tempo |

### 3.3 Engagement Analysis

| Elemento | Descrição |
|----------|-----------|
| Shots Fired | Por tipo de arma |
| Hit Rate | Hits / shots por sistema |
| Kills per Engagement | Eficiência |
| Range Distribution | Histograma de distâncias de engajamento |
| First Shot Advantage | Quem detectou/atirou primeiro venceu? |

### 3.4 Temporal Analysis

| Elemento | Descrição |
|----------|-----------|
| Phase Completion | Tempo para atingir cada phase line |
| Casualty Rate | Perdas/hora ao longo da batalha |
| Momentum Shifts | Quando a batalha "virou" |
| Critical Moments | Eventos de alto impacto identificados |

### 3.5 Sensitivity Results (Sobol)

| Elemento | Descrição |
|----------|-----------|
| First-Order Indices | Impacto direto de cada parâmetro |
| Total-Order Indices | Impacto incluindo interações |
| Parameter Ranking | Quais mais importam |
| Interaction Matrix | Quais parâmetros interagem |
| Tornado Diagram | Visualização de sensibilidade |

### 3.6 Comparison

| Elemento | Descrição |
|----------|-----------|
| Scenario vs Scenario | Comparar dois cenários lado a lado |
| What-If Analysis | "E se BLUE tivesse +1 battalion?" |
| Force Ratio Curves | Win rate vs proporção de forças |
| Historical Comparison | Comparar com resultados anteriores salvos |

### 3.7 Export

| Elemento | Descrição |
|----------|-----------|
| Raw Data | CSV com todas as runs |
| Summary Report | PDF formatado |
| Charts | PNG/SVG dos gráficos |
| Scenario File | JSON do cenário para reproduzir |
| Full Archive | ZIP com tudo |

---

## 4. Map View (Visualização Agregada)

### 4.1 Base Layers

| Elemento | Descrição |
|----------|-----------|
| Terrain Elevation | Heatmap de altitude |
| Terrain Type | Cores por tipo (forest, urban, water) |
| Road Network | Estradas e capacidade |
| Grid Overlay | Coordenadas militares |
| Scale | Régua de distância |

### 4.2 Deployment View

| Elemento | Descrição |
|----------|-----------|
| Unit Icons | Símbolos NATO por tipo |
| Unit Size | Tamanho indica quantidade |
| Side Colors | BLUE/RED/NEUTRAL |
| Zones | Deployment zones, objectives |
| Arcs | Campos de fogo, setores |

### 4.3 Heatmaps (Agregados de N runs)

| Elemento | Descrição |
|----------|-----------|
| Engagement Density | Onde ocorreram combates |
| Casualty Locations | Onde unidades foram destruídas |
| Movement Density | Rotas mais usadas |
| Detection Events | Onde unidades foram detectadas |
| Artillery Impact | Onde artilharia caiu |

### 4.4 Probability Maps

| Elemento | Descrição |
|----------|-----------|
| Control Probability | Chance de cada célula estar sob controle BLUE/RED |
| Survival Probability | Chance de unidade em posição X sobreviver |
| Threat Zones | Áreas de alto risco por tipo (SAM coverage, artillery range) |

### 4.5 Single Run Replay (Debug Mode)

| Elemento | Descrição |
|----------|-----------|
| Run Selector | Escolher uma run específica |
| Timeline Slider | Scrub pelo tempo |
| Event Log | Lista de eventos com timestamp |
| Step Forward/Back | Avançar por evento |
| Filter Events | Mostrar só combate, só movimento, etc |
| Export | GIF/video da run |

### 4.6 Layer Controls

| Elemento | Descrição |
|----------|-----------|
| Toggle Layers | Ligar/desligar cada overlay |
| Opacity | Transparência por layer |
| Filter by Type | Mostrar só tanks, só aircraft, etc |
| Filter by Side | Mostrar só BLUE, só RED |
| Time Filter | Mostrar estado em T específico |

---

## 5. Platform Database Browser

### 5.1 Search & Filter

| Elemento | Descrição |
|----------|-----------|
| Text Search | Por nome, designação |
| Country Filter | Dropdown ou checkboxes |
| Type Filter | Tank, IFV, APC, Aircraft, etc |
| Era Filter | Cold War, Modern, Future |
| Role Filter | MBT, IFV, SAM, etc |

### 5.2 Platform Details

| Elemento | Descrição |
|----------|-----------|
| Specifications | Todas as specs do JSON |
| Calculated Ratings | Firepower, Protection, Mobility |
| Operators | Lista de países e quantidades |
| Variants | Versões disponíveis |
| Sources | Referências dos dados |

### 5.3 Comparison

| Elemento | Descrição |
|----------|-----------|
| Side-by-Side | 2-4 plataformas comparadas |
| Radar Chart | Visualização de ratings |
| Diff Highlight | Destacar diferenças |

---

## 6. Scenario Library

### 6.1 Built-in Scenarios

| Cenário | Descrição |
|---------|-----------|
| Baltics 2025 | NATO vs Russia, terrain báltico |
| Taiwan Strait | PLA amphibious vs Taiwan defense |
| Fulda Gap (Historical) | Cold War classic |
| Korean DMZ | North vs South |
| Tutorial | Cenário simples para aprender |

### 6.2 User Scenarios

| Elemento | Descrição |
|----------|-----------|
| Save | Salvar cenário atual |
| Load | Carregar cenário salvo |
| Duplicate | Criar cópia para modificar |
| Delete | Remover cenário |
| Rename | Renomear |

### 6.3 Import/Export

| Elemento | Descrição |
|----------|-----------|
| Export JSON | Cenário completo |
| Import JSON | Carregar cenário externo |
| Share | Gerar arquivo para compartilhar |
| Version History | Histórico de modificações |

---

## 7. Settings

### 7.1 Performance

| Setting | Descrição |
|---------|-----------|
| Default Threads | Número padrão de threads |
| Memory Limit | RAM máxima para simulação |
| Checkpoint Interval | Frequência de salvamento automático |

### 7.2 Display

| Setting | Descrição |
|---------|-----------|
| Theme | Light / Dark |
| Units | Metric / Imperial |
| Coordinate System | MGRS / Lat-Long / UTM |
| Map Colors | Esquema de cores |

### 7.3 Paths

| Setting | Descrição |
|---------|-----------|
| Data Directory | Onde estão os JSONs de plataformas |
| Scenarios Directory | Onde salvar cenários |
| Export Directory | Onde salvar resultados |
| Temp Directory | Arquivos temporários |

### 7.4 Defaults

| Setting | Descrição |
|---------|-----------|
| Default Iterations | Monte Carlo padrão |
| Default Confidence | Nível de confiança padrão |
| Default Weather | Condições padrão |
| Default ROE | Regras de engajamento padrão |

---

## Technology Stack (Recomendado)

### Opção 1: Qt (Profissional)

```
Framework:     Qt 6.x (LGPL ou Commercial)
Widgets:       QMainWindow, QDockWidget, QTabWidget
Map:           QGraphicsView (custom 2D rendering)
Charts:        QCharts ou QtCustomPlot
Tables:        QTableView + QSortFilterProxyModel
Build:         CMake + Qt Creator
```

**Prós:** Maduro, usado em software militar real, widgets completos  
**Contras:** Licença dual, ~50MB de DLLs

### Opção 2: Dear ImGui (Leve)

```
Framework:     Dear ImGui 1.9x
Backend:       SDL2 + OpenGL3 (ou Vulkan)
Map:           Custom rendering (OpenGL quads)
Charts:        ImPlot
Tables:        ImGui tables
Build:         CMake
```

**Prós:** Extremamente leve (~300KB), máximo controle  
**Contras:** Mais trabalho manual, visual mais utilitário

### Decisão Pendente

Escolha depende de:
- Necessidade de visual "polido" → Qt
- Performance máxima e controle total → ImGui
- Distribuição (licença) → ImGui mais simples

---

## Implementation Phases

### Phase 1: Foundation
- [ ] Escolher framework (Qt ou ImGui)
- [ ] Setup de build com CMake
- [ ] Janela principal com layout básico
- [ ] Integração com core ATHENA

### Phase 2: Scenario Editor
- [ ] Map view básico (terrain rendering)
- [ ] Platform browser (lista das 453)
- [ ] Force composition (drag-drop)
- [ ] Deployment (posicionamento)

### Phase 3: Simulation Control
- [ ] Monte Carlo settings
- [ ] Progress monitoring
- [ ] Thread control
- [ ] Pause/Resume/Cancel

### Phase 4: Results Dashboard
- [ ] Win rate display
- [ ] Casualty tables
- [ ] Basic charts (histograms, bars)
- [ ] Export (CSV, JSON)

### Phase 5: Advanced Features
- [ ] Heatmaps agregados
- [ ] Sobol visualization
- [ ] Comparison tools
- [ ] PDF reports

### Phase 6: Polish
- [ ] Themes
- [ ] Keyboard shortcuts
- [ ] Undo/Redo
- [ ] Help/Documentation

---

## File Formats

### Scenario File (.athena-scenario)

```json
{
  "version": "1.0",
  "metadata": {
    "name": "Baltic Defense 2025",
    "author": "User",
    "created": "2026-01-23",
    "description": "NATO defense of Baltic states"
  },
  "terrain": {
    "region": "baltics",
    "weather": "clear",
    "season": "summer"
  },
  "forces": {
    "blufor": { ... },
    "opfor": { ... }
  },
  "deployment": { ... },
  "objectives": { ... },
  "rules_of_engagement": { ... },
  "simulation_settings": { ... }
}
```

### Results File (.athena-results)

```json
{
  "version": "1.0",
  "scenario_hash": "abc123...",
  "execution": {
    "iterations": 1000,
    "duration_seconds": 45.2,
    "threads": 8
  },
  "summary": {
    "blue_wins": 670,
    "red_wins": 280,
    "draws": 50
  },
  "casualties": { ... },
  "sobol_indices": { ... },
  "runs": [ ... ]
}
```

---

## Appendix: NATO Military Symbols

Para representação de unidades no mapa, usar símbolos APP-6A/MIL-STD-2525:

| Tipo | Símbolo |
|------|---------|
| Infantry | Retângulo com X |
| Armor | Retângulo com oval |
| Artillery | Retângulo com ponto |
| Air Defense | Retângulo com arco |
| Aviation | Retângulo com hélice |
| Naval | Retângulo com âncora |

Cores:
- BLUFOR: Azul (#0000FF)
- OPFOR: Vermelho (#FF0000)
- NEUTRAL: Verde (#00FF00)
- UNKNOWN: Amarelo (#FFFF00)

Tamanho indica escalão (team → squad → platoon → company → battalion → regiment → brigade → division → corps → army)
