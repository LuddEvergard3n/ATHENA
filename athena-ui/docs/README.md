# ATHENA UI (Qt6) — LEGACY

> **LEGACY CODE:** A interface Qt6 foi substituída por ImGui+GLFW na v0.8.1.
> Este código é mantido como referência arquitetural e **não é compilado** pelo Makefile.unified.
> A interface ativa está em `athena-core/src/gui/integrated_gui.cpp`.

**Versão original:** 0.6.2 - 0.8.0  
**Status:** Legacy (não compilado desde v0.8.1)

---

## Histórico

A interface Qt6 foi desenvolvida entre v0.6.2 e v0.8.0 (janeiro de 2026), atingindo 28 arquivos .cpp com ~14.500 linhas. Incluía QGraphicsScene, undo/redo, minimap, Sobol charts, engagement lines, formation templates, e mais.

Foi removida na v0.8.1 e substituída por ImGui+GLFW, que oferece distribuição sem dependências externas e compilação cross-platform trivial.

---

## Funcionalidades (Histórico)

- **Platform Browser** — Navegador de plataformas militares
- **Force Composition** — Drag-and-drop para montar forças
- **Map View** — QGraphicsScene com pan/zoom
- **Undo/Redo** — Via QUndoStack
- **Simulation Panel** — Controle de simulação com QThread
- **Results Dashboard** — Visualização de resultados com Qt Charts
- **Minimap** — Navegação rápida
- **Formation Templates** — Templates pré-definidos
- **Engagement Lines** — Visualização de engajamentos
- **Sobol Charts** — Visualização de índices de sensibilidade

---

## Requisitos (para compilação do código legado)

- Qt 6.x (Commercial ou Open Source)
- CMake 3.16+
- C++17 compiler
- athena-core compilado

---

**Nota:** Para a interface gráfica atual, consulte `athena-core/src/gui/integrated_gui.cpp` (ImGui+GLFW).
