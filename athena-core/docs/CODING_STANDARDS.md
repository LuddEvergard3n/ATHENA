# ATHENA Core - Padrões de Codificação

> "ATHENA não é escrito para programadores. É escrito para auditores, engenheiros futuros e pessoas que não confiam em você."

## Regras Absolutas (Não Negociáveis)

### 1. Determinismo

```cpp
// ❌ PROIBIDO: Ordem não determinística
std::unordered_map<K, V> data;
std::unordered_set<T> items;

// ✅ OBRIGATÓRIO: Ordem determinística
std::map<K, V> data;
std::set<T> items;
// Ou std::vector com sorting explícito
```

```cpp
// ❌ PROIBIDO: Iteração por ponteiro
for (auto* ptr : pointer_list) { ... }

// ✅ OBRIGATÓRIO: Iteração por índice
for (usize i = 0; i < count; ++i) { ... }
```

### 2. Floating Point

```cpp
// ❌ PROIBIDO: Operações que dependem de ordem de avaliação
f64 sum = a + b + c + d;  // Ordem pode variar

// ✅ OBRIGATÓRIO: Ordem explícita
f64 sum = ((a + b) + c) + d;
```

```cpp
// ❌ PROIBIDO: Comparação direta de floats
if (a == b) { ... }

// ✅ Quando necessário, usar epsilon explícito
constexpr f64 EPSILON = 1e-10;
if (std::abs(a - b) < EPSILON) { ... }
```

**Flags de compilação obrigatórias:**
```makefile
# GCC/Clang
-fno-fast-math -ffp-contract=off

# MSVC
/fp:strict
```

### 3. RNG

```cpp
// ❌ PROIBIDO: RNG global ou estático
static Rng global_rng;
f64 value = rand() / RAND_MAX;

// ✅ OBRIGATÓRIO: RNG passado explicitamente
Status my_system(EntityStorage& s, Rng& rng, Tick tick) {
    f64 value = rng.next_f64();
}
```

```cpp
// ❌ PROIBIDO: Criar RNG com seed arbitrária
Rng my_rng(time(nullptr));

// ✅ OBRIGATÓRIO: Derivar de master seed ou stream
Rng phase_rng = context.rng().split(PHASE_STREAM_ID);
```

### 4. Error Handling

```cpp
// ❌ PROIBIDO: Exceções
throw std::runtime_error("error");
try { ... } catch (...) { ... }

// ✅ OBRIGATÓRIO: Result<T> pattern
Result<Value> parse(const std::string& json) {
    if (invalid) {
        return Error(ErrorCode::INVALID_ARGUMENT, "details");
    }
    return value;
}

// Uso:
auto result = parse(input);
if (!result.ok()) {
    log_error(result.error.message);
    return result.error;
}
auto value = result.get();
```

### 5. Memory Layout

```cpp
// ❌ PROIBIDO no hot path: AoS (Array of Structures)
struct Entity {
    f64 x, y, z;
    f64 health;
};
std::vector<Entity> entities;

// ✅ OBRIGATÓRIO: SoA (Structure of Arrays)
struct EntityStorage {
    std::vector<f64> pos_x;
    std::vector<f64> pos_y;
    std::vector<f64> pos_z;
    std::vector<f64> health;
    usize count;
};
```

### 6. Headers são Contratos

```cpp
// Header deve documentar:
// - Pré-condições
// - Pós-condições
// - Comportamento de erro
// - Thread-safety
// - Determinismo

/// Compute damage between two entities.
/// 
/// PRE: attacker and defender are valid indices
/// PRE: both entities are active
/// POST: health values updated
/// DETERMINISM: Uses provided RNG only
/// THREAD: Not thread-safe
f64 compute_damage(usize attacker, usize defender,
                   const EntityStorage& storage, Rng& rng);
```

## Convenções de Código

### Tipos

```cpp
// Usar tipos explícitos de athena/types.hpp
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;

using Seed = u64;
using Tick = u64;
using EntityId = u32;
```

### Naming

```cpp
// Classes/Structs: PascalCase
class MonteCarloExecutor;
struct BatchConfig;

// Functions/Methods: snake_case
void compute_damage();
Status run_tick();

// Variables: snake_case
f64 total_health;
usize entity_count;

// Constants: SCREAMING_SNAKE_CASE
constexpr f64 EARTH_RADIUS = 6371000.0;
constexpr u64 MOVEMENT_STREAM = 0x4D4F56454D454E54;

// Member variables: trailing underscore
class MyClass {
    int value_;
    std::string name_;
};
```

### Includes

```cpp
// Ordem:
// 1. Header correspondente (para .cpp)
// 2. Headers do projeto (athena/*)
// 3. Headers da stdlib
// 4. Headers externos (se houver)

// Em scenario.cpp:
#include "athena/scenario.hpp"  // Correspondente

#include "athena/entities.hpp"  // Projeto
#include "athena/rng.hpp"

#include <cmath>                // Stdlib
#include <algorithm>
#include <set>
```

### Formatação

```cpp
// Braces: mesmo estilo K&R
if (condition) {
    do_something();
} else {
    do_other();
}

// Indentação: 4 espaços (não tabs)

// Linha máxima: 100 caracteres

// Namespaces: não indentar conteúdo
namespace athena {
namespace systems {

class Movement {
    // ...
};

}  // namespace systems
}  // namespace athena
```

## Padrões de Sistema

### Função de Update de Sistema

```cpp
// Assinatura padrão para sistemas
using SystemFn = std::function<Status(EntityStorage&, Rng&, Tick)>;

// Implementação típica
Status movement_system_update(EntityStorage& storage, Rng& rng, Tick tick) {
    // 1. Processar por índice (determinístico)
    for (usize i = 0; i < storage.count; ++i) {
        // 2. Skip inativos
        if (!storage.is_active(i)) continue;
        
        // 3. Lógica do sistema
        // ...
        
        // 4. Usar RNG fornecido para aleatoriedade
        f64 variance = rng.next_normal(0.0, 0.1);
    }
    
    return Status();  // Success
}
```

### Configuração de Sistema

```cpp
// Cada sistema tem seu próprio Config struct
struct MovementConfig {
    f64 dt_seconds = 3600.0;
    f64 terrain_speed_modifier = 1.0;
    bool terrain_effects_enabled = true;
    f64 fuel_consumption_per_km = 0.001;
    // ...
};

// Sistema inicializado com config
class MovementSystem {
public:
    void init(usize capacity, const MovementConfig& config);
    // ...
private:
    MovementConfig config_;
};
```

## Testes

### Estrutura de Teste

```cpp
// Macro de assert simples
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << "\n"; \
            return false; \
        } \
    } while(0)

// Função de teste retorna bool
bool test_something() {
    // Setup
    auto result = do_thing();
    
    // Assert
    TEST_ASSERT(result.ok(), "should succeed");
    TEST_ASSERT(result.get() == expected, "value match");
    
    return true;
}

// Runner
#define RUN_TEST(fn) \
    do { \
        std::cout << "Running " << #fn << "... "; \
        if (fn()) { \
            std::cout << "PASS\n"; \
            passed++; \
        } else { \
            failed++; \
        } \
    } while(0)
```

### Teste de Determinismo (Obrigatório)

Todo novo sistema DEVE ter teste de determinismo:

```cpp
bool test_my_system_determinism() {
    const Seed SEED = 0x12345678;
    
    // Run 1
    Setup s1;
    s1.init(SEED);
    s1.run();
    auto state1 = capture(s1);
    
    // Run 2 (same seed)
    Setup s2;
    s2.init(SEED);
    s2.run();
    auto state2 = capture(s2);
    
    // Must be identical
    TEST_ASSERT(state1 == state2, "Determinism failed");
    
    return true;
}
```

## Documentação

### Comentários de Arquivo

```cpp
// ATHENA Core - [Nome do Módulo]
// Contract: [Descrição do contrato/propósito]
//
// RULES:
// - [Regra 1]
// - [Regra 2]
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project
```

### Comentários de Função

```cpp
/// Brief description.
///
/// Detailed description if needed.
///
/// @param name Description
/// @return Description
/// @throws Never (ou lista de ErrorCodes)
///
/// @pre Precondition
/// @post Postcondition
Result<Value> my_function(Type param);
```

## Checklist de Code Review

- [ ] Determinismo: Sem unordered containers?
- [ ] Determinismo: Iteração por índice?
- [ ] Determinismo: RNG passado, não criado?
- [ ] FP: Flags de compilação corretas?
- [ ] FP: Sem comparação direta de floats?
- [ ] Error: Usa Result<T>, não exceções?
- [ ] Memory: SoA no hot path?
- [ ] Test: Tem teste de determinismo?
- [ ] Doc: Header documenta contrato?
