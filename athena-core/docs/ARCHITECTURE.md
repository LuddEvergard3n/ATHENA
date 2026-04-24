# ATHENA Core - Arquitetura Técnica

## Visão Geral

ATHENA é um motor de wargaming determinístico projetado para análise de cenários militares com suporte a Monte Carlo. O sistema prioriza reprodutibilidade bit-exact sobre performance ou elegância de código.

## Diretivas de Codificação (OBRIGATÓRIAS)

### 1. Determinismo Acima de Tudo
```cpp
// ERRADO: ordem não determinística
std::unordered_map<std::string, int> data;

// CERTO: ordem determinística
std::map<std::string, int> data;
```

### 2. RNG: Uma Fonte, Um Dono
- Toda aleatoriedade deriva de um único `master_seed`
- Cada fase tem seu próprio stream RNG
- Nunca criar RNG "avulso"

```cpp
// Streams por fase (constantes hexadecimais mnemônicas)
PRETICK:    0x5052455449434B00
MOVEMENT:   0x4D4F56454D454E54
DETECTION:  0x4445544543544900
COMBAT:     0x434F4D4241540000
LOGISTICS:  0x4C4F47495354494C
CASUALTIES: 0x434153554C545945
POSTTICK:   0x504F53545449434B
```

### 3. Memory Layout: SoA (Structure of Arrays)
```cpp
// ERRADO: AoS (Array of Structures)
struct Entity { f64 x, y, z; f64 health; };
std::vector<Entity> entities;

// CERTO: SoA
struct EntityStorage {
    std::vector<f64> pos_x;
    std::vector<f64> pos_y;
    std::vector<f64> pos_z;
    std::vector<f64> health;
};
```

### 4. Sem Exceções no Core
```cpp
// ERRADO
throw std::runtime_error("algo deu errado");

// CERTO
return Error(ErrorCode::INVALID_ARGUMENT, "algo deu errado");
```

### 5. IEEE-754 Strict
```makefile
# GCC/Clang
CXXFLAGS += -fno-fast-math -ffp-contract=off

# MSVC
CXXFLAGS += /fp:strict
```

### 6. Tudo Deixa Rastro
- Toda execução gera um Manifest
- Manifest inclui hash SHA-256 de inputs e outputs
- Manifest inclui info de plataforma (CPU, OS, compiler)

## Arquitetura de Componentes

```
┌─────────────────────────────────────────────────────────────┐
│                        ATHENA Core                          │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐        │
│  │  types  │  │   rng   │  │ context │  │entities │        │
│  │ Result  │  │  PCG64  │  │  Seed   │  │   SoA   │        │
│  │ Error   │  │ Streams │  │  Tick   │  │ Storage │        │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘        │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────┐       │
│  │                   Scheduler                      │       │
│  │  PRE_TICK → MOVEMENT → DETECTION → COMBAT →     │       │
│  │  LOGISTICS → CASUALTIES → POST_TICK             │       │
│  └─────────────────────────────────────────────────┘       │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │ Movement │  │  Combat  │  │Logistics │   Systems        │
│  │ Waypoint │  │Lanchester│  │ Resupply │                  │
│  │ Terrain  │  │ Attrition│  │  Morale  │                  │
│  └──────────┘  └──────────┘  └──────────┘                  │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │   JSON   │  │ Scenario │  │ Manifest │   Data I/O       │
│  │  Parser  │  │  Loader  │  │  SHA256  │                  │
│  └──────────┘  └──────────┘  └──────────┘                  │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────┐       │
│  │              Monte Carlo Executor                │       │
│  │  Batch Config → Seed Derivation → Statistics    │       │
│  └─────────────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

## Fluxo de Execução

### Simulação Única

```
1. Load Scenario (JSON)
2. Create Context (seed, config)
3. Initialize EntityManager
4. Convert Scenario → Entities
5. Initialize Systems (Movement, Combat, Logistics)
6. Register Systems with Scheduler
7. Loop:
   a. Scheduler.run_tick()
      - PRE_TICK phase
      - MOVEMENT phase (RNG stream: 0x4D4F56454D454E54)
      - DETECTION phase (RNG stream: 0x4445544543544900)
      - COMBAT phase (RNG stream: 0x434F4D4241540000)
      - LOGISTICS phase (RNG stream: 0x4C4F47495354494C)
      - CASUALTIES phase (RNG stream: 0x434153554C545945)
      - POST_TICK phase
   b. Check termination conditions
8. Generate Manifest
9. Output Results
```

### Monte Carlo Batch

```
1. Load Scenario
2. Configure Batch (master_seed, num_iterations)
3. For each iteration i:
   a. Derive iteration seed: hash(master_seed, i)
   b. Run single simulation with derived seed
   c. Collect metrics
4. Aggregate statistics
5. Export results (JSON)
```

## Modelo de Dados

### EntityStorage (SoA)

```cpp
struct EntityStorage {
    // Identity
    std::vector<EntityId> id;
    std::vector<Side> side;
    std::vector<u8> flags;
    
    // Position (metros, coordenadas locais)
    std::vector<f64> pos_x;
    std::vector<f64> pos_y;
    std::vector<f64> pos_z;
    
    // Velocity (m/s)
    std::vector<f64> vel_x;
    std::vector<f64> vel_y;
    std::vector<f64> vel_z;
    
    // State (0.0 - 1.0)
    std::vector<f64> health;
    std::vector<f64> supply;
    std::vector<f64> morale;
    std::vector<f64> readiness;
    
    // Resources
    std::vector<f64> fuel;
    std::vector<f64> ammo;
    std::vector<f64> consumption_rate;
    
    // Capabilities
    std::vector<f64> firepower;
    std::vector<f64> engagement_range;
    std::vector<f64> detection_range;
};
```

### Scenario Schema

```
Scenario
├── id, name, version
├── metadata (schema_version, created_at, tags)
├── scale (macro/meso/micro)
├── temporal (start_date, end_date, tick_duration)
├── spatial_bounds (lat/lon)
├── environment
│   ├── terrain (theater_id, resolution)
│   ├── climate (season, weather_model)
│   └── infrastructure (roads, bridges)
├── actors[]
│   ├── id, name, type, side
│   ├── position (lat, lon, alt)
│   ├── capabilities (mobility, firepower, sensors, logistics, command)
│   └── initial_state (health, supply, morale, readiness)
└── parameters{} (uncertain parameters for sensitivity analysis)
```

## Sistemas de Simulação

### Movement System

**Responsabilidades:**
- Atualizar posições baseado em velocidade
- Gerenciar waypoints
- Aplicar modificadores de terreno
- Consumir combustível

**Algoritmo:**
```
for each entity (by index order):
    if stationary: skip
    if no fuel: stop entity
    
    compute direction to target
    apply terrain modifier
    compute effective speed
    
    new_pos = old_pos + velocity * dt
    distance_traveled = |new_pos - old_pos|
    
    consume fuel(distance_traveled * consumption_rate)
    check waypoint arrival
```

### Combat System

**Responsabilidades:**
- Resolver engajamentos entre entidades hostis
- Aplicar dano baseado em modelo Lanchester
- Atualizar moral após combate
- Consumir munição

**Algoritmo:**
```
for each attacker (by index order):
    for each potential defender (by index order):
        if not in range: skip
        if same side: skip
        if engagement limit reached: break
        
        attacker_effectiveness = f(health, supply, morale, readiness)
        defender_effectiveness = f(health, supply, morale, readiness)
        
        base_damage = attrition_rate * (firepower_ratio) * range_factor
        variance = rng.normal(0, attrition_variance)
        
        apply_damage(attacker, defender_damage)
        apply_damage(defender, attacker_damage)
        consume_ammo(attacker)
```

### Logistics System

**Responsabilidades:**
- Consumir recursos por tick
- Aplicar resupply em depósitos
- Atualizar moral baseado em supply
- Calcular readiness

**Algoritmo:**
```
for each entity (by index order):
    base_consumption = f(activity_flags)
    
    fuel -= fuel_consumption * consumption_rate
    ammo -= ammo_consumption * consumption_rate
    
    if near_depot(entity, friendly_depot):
        resupply(entity, depot)
    
    update_morale(entity, supply_level)
    readiness = supply_factor * morale * health
```

## Garantias de Reprodutibilidade

### Verificação de Determinismo

O teste `test_full_simulation_determinism` executa:

```cpp
// Primeira execução
TestSetup setup1;
setup1.init(SEED, TICKS);
setup1.create_test_entities();
setup1.scheduler.run_until_complete();
StateSnapshot snapshot1 = capture(setup1);

// Segunda execução (mesmo seed)
TestSetup setup2;
setup2.init(SEED, TICKS);
setup2.create_test_entities();
setup2.scheduler.run_until_complete();
StateSnapshot snapshot2 = capture(setup2);

// Comparação bit-exact
assert(snapshot1 == snapshot2);  // DEVE passar
```

### Manifest de Execução

Cada execução produz um manifest com:

```json
{
  "manifest_version": "1.0",
  "job_id": "uuid",
  "created_at": "ISO-8601",
  "platform": {
    "os_name": "Linux",
    "cpu_vendor": "GenuineIntel",
    "cpu_features": "SSE4.2 AVX2"
  },
  "build": {
    "compiler": "g++ 13.3.0",
    "compiler_flags": "-fno-fast-math -ffp-contract=off"
  },
  "config": {
    "master_seed": 12345678,
    "thread_count": 1
  },
  "input_hashes": {
    "scenario_hash": "sha256:abc123..."
  },
  "output_hash": "sha256:def456...",
  "manifest_hash": "sha256:..."
}
```

## Considerações de Performance

### Atual (Single-threaded)
- Determinismo garantido
- ~10k entidades suportadas confortavelmente
- Monte Carlo com 1000 iterações em minutos

### Futuro (Multi-threaded)
- Paralelismo apenas entre iterações Monte Carlo
- Cada iteração permanece single-threaded (determinístico)
- Resultados agregados em ordem de iteração

## Extensibilidade

### Adicionando Novo Sistema

```cpp
// 1. Criar header em include/athena/systems/
// 2. Implementar em src/systems/
// 3. Registrar no scheduler:

scheduler.register_system(
    Scheduler::Phase::DETECTION,  // ou outra fase apropriada
    my_system_update,
    "my_system"
);
```

### Adicionando Novo Tipo de Distribuição

```cpp
// Em scenario.hpp, adicionar ao enum:
enum class DistributionType : u8 {
    // ...existentes...
    MY_DISTRIBUTION = 6
};

// Em montecarlo.cpp, adicionar caso:
case DistributionType::MY_DISTRIBUTION: {
    // implementação
}
```
