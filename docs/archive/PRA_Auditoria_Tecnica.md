# PRA — BeamLabStudio v0.2.0
## Plan de Remediación Aprobable

**Fecha auditoría:** 2026-05-22
**Fuentes:** CSO Security Audit + BeamLabStudio_Audit_Completo.md + PLAN_DE_ATAQUE.md + git log
**Auditor:** DeepSeek V4 via OpenCode (cso + code-review + análisis estático)

---

## Resumen Ejecutivo

| Dimensión | Peso | Score (0-100) | Estado |
|-----------|------|---------------|--------|
| **Código y Lógica** | 25% | 62 | 12 bugs documentados, 6 corregidos |
| **Arquitectura** | 20% | 55 | MainWindow aún >2000 líneas, subsistemas duplicados |
| **Seguridad** | 20% | 70 | Sin superficie web; QProcess/scripts riesgo medio |
| **Performance** | 15% | 72 | Streaming pipeline ya implementado; 2.1x mejora import |
| **DevOps/Infra** | 10% | 80 | CI funcional, install targets, sin Docker (N/A) |
| **Calidad/Testing** | 10% | 58 | 41 tests pasan, cobertura <40%, sin tests integración |
| **Global** | 100% | **64/100** | **C** — Funcional pero requiere hardening |

### Top 5 Riesgos Críticos

1. **🔴 `push.txt` con credenciales** — Token GitHub posiblemente activo en historial git
2. **🟠 MP4 export race condition** — `QApplication::processEvents()` permite UI interactiva durante captura
3. **🟠 Command injection en scripts** — Paths de usuario pasados a QProcess/bash sin sanitización completa
4. **🟠 División por cero latente** — `finalizeAccumulator()` sin guard en la función misma
5. **🟡 Doble subsistema de simulación** — `src/simulation/` (viejo, sin Sternheimer) + `src/biosim/` (nuevo, completo)

### Esfuerzo estimado total: **~35 horas** (restantes, ya se invirtieron ~60h en fixes)

---

## Hallazgos Detallados

### DIMENSIÓN 1: CÓDIGO Y LÓGICA (Score: 62/100)

#### Bugs Activos

| ID | Severidad | Archivo:Línea | Descripción | Impacto | Estado |
|----|-----------|---------------|-------------|---------|--------|
| B-01 | 🟠 ALTO | `FrameStatisticsEngine.cpp:96` | `finalizeAccumulator()` sin guard `count==0` → NaN en cascada | Stats downstream corruptos si futuro caller omite guard | Pendiente |
| B-02 | 🟠 ALTO | `MainWindow.cpp:3023` | `processEvents()` en loop captura MP4 → frames corruptos si usuario interactúa | Video export dañado | Pendiente |
| B-03 | 🟠 ALTO | `MainWindow.cpp:2397` | `loadManifest()` sin validación de schema → crashes con JSON malformado | UI inestable con datos externos | Pendiente |
| B-04 | 🟡 MEDIO | `Geant4CsvImporter.cpp` | CSV multilínea (quoted fields con `\n`) no soportado | Pérdida silenciosa de datos | Pendiente |
| B-05 | 🟡 MEDIO | `FocusDetector.cpp:76` | `confidence=0.1` arbitrario para focos en bordes | Falsos negativos en foco legítimo | Pendiente |
| B-06 | 🟡 MEDIO | `BetheBlochEngine.cpp:57` | `beta2==0` retorna 0 MeV/cm (físicamente incorrecto) | Pico de Bragg aplanado artificialmente | Pendiente |

#### Bugs Corregidos (post-audit)

| ID | Fix | Commit |
|----|-----|--------|
| ✅ | `makeUniqueTrajectoryId` overflow | `Geant4CsvImporter.cpp` con `kMaxEvent` |
| ✅ | `projectSamples()` duplicada | `0e6d66a` — cache en `FrameStatisticsEngine::compute()` |
| ✅ | `QTimer::singleShot` hacks | `951415b` — `QMetaObject::invokeMethod` |
| ✅ | Funciones helper duplicadas | `d6683e9` — unificadas en `Vec3.h` |
| ✅ | OOM en datasets grandes | `236c61a` — SQLite file-backed, 9x reducción RAM |
| ✅ | Pipeline bash/QProcess | `d6ecfa6` — `AnalysisPipeline` nativo C++ |

#### Code Smells Persistentes

- `MainWindow.cpp` aún >2000 líneas (bajó de 4445 a ~2000 post-refactor)
- `include_directories(${PROJECT_SOURCE_DIR}/src)` global — C++20 sin `std::span`/`string_view`
- Magic numbers sin `constexpr` (tamaños de ventana, thresholds)
- Sin `[[nodiscard]]` en algunos engines nuevos (aunque 173+ instances existen)

---

### DIMENSIÓN 2: ARQUITECTURA Y DISEÑO (Score: 55/100)

| ID | Severidad | Hallazgo | Estado |
|----|-----------|----------|--------|
| A-01 | 🔴 CRÍTICO | `MainWindow` mezcla UI + lógica negocio + export | Mejorado (4445→2000 líneas), aún God Object |
| A-02 | 🟠 ALTO | Doble subsistema simulación: `src/simulation/` (obsoleto) + `src/biosim/` (nuevo) | Sin unificar |
| A-03 | 🟠 ALTO | `TrajectoryDataset::trajectories` público — cualquier código muta sin normalización | Pendiente |
| A-04 | 🟡 MEDIO | `include_directories` global rompe encapsulación | Pendiente |
| A-05 | 🟡 MEDIO | Sin sistema de eventos entre capas (observer/signal-slot en negocio) | Pendiente |
| A-06 | 🟡 MEDIO | Manejo de errores inconsistente: `bool` vs `std::optional<Error>` vs `throw` | Pendiente |

#### Lo que funciona:
- `ISampleStorage` (factory pattern: InMemory <100MB, SQLite ≥100MB)
- `IImporter` / `IExporter` / `IEnvelopeExtractor` interfaces abstractas
- `AnalysisPipeline` nativo (remplazó QProcess + bash)
- `ServiceRegistry` DI container
- `ExportManager` extraído de MainWindow

---

### DIMENSIÓN 3: SEGURIDAD (Score: 70/100)

**Superficie de ataque:** Aplicación científica de escritorio. Sin endpoints HTTP, sin APIs web, sin multi-tenant. Riesgo principal: inyección vía archivos de datos maliciosos.

| ID | Severidad | Hallazgo | Archivo | Estado |
|----|-----------|----------|---------|--------|
| S-01 | 🔴 CRÍTICO | `push.txt` con `TU_TOKEN@github.com` — posible token real en historial | `push.txt` | **Verificar y revocar** |
| S-02 | 🟠 ALTO | QProcess con `input_path` de usuario a script shell | `MainWindow.cpp:2200` | Mitigado: `run_beamlab_full.sh` usa `"$@"` |
| S-03 | 🟠 ALTO | Path traversal en `copyDirectoryRecursive()` | `MainWindow.cpp` (export) | Pendiente |
| S-04 | 🟡 MEDIO | `ffmpeg` buscado en PATH sin verificar integridad | `MainWindow.cpp` | Bajo impacto |
| S-05 | 🟡 MEDIO | Temp files con nombre predecible `.beamlab_frames_*` | `MainWindow.cpp` | Bajo impacto |
| S-06 | 🟡 MEDIO | Script `.cmd` de Windows con `%*` sin escaping | `scripts/run_beamlab_full.cmd:4` | Pendiente |

#### Verificaciones CSO superadas:
- ✅ Sin `.env` en repo
- ✅ Sin secrets hardcodeados en código C++/Python
- ✅ Sin `std::system()` ni `popen()` en source
- ✅ Sin dependencias con CVEs conocidos (sqlite3 amalgamation, nlohmann_json, googletest)
- ✅ GitHub Actions CI sin `pull_request_target` peligroso
- ✅ Acciones third-party pinned (`actions/checkout@v4`)
- ✅ Sin Dockerfiles (N/A para app desktop)
- ✅ Sin LLM/AI en código (sin riesgo de prompt injection)

---

### DIMENSIÓN 4: PERFORMANCE (Score: 72/100)

| ID | Severidad | Hallazgo | Estado |
|----|-----------|----------|--------|
| P-01 | 🟠 ALTO | `Scene3DWidget` redibuja todo en `paintEvent` sin double buffering | Pendiente |
| P-02 | 🟡 MEDIO | Datasets grandes cargan CSVs completos para plotting | Mejorado: streaming pipeline |
| P-03 | 🟡 MEDIO | `ConvexHullEnvelopeExtractor` recalcula hull por frame | Pendiente |
| P-04 | 🟡 MEDIO | `computeEqualCountAxialBins` usa `sort` O(N log N) pudiendo usar `nth_element` O(N) | Pendiente |
| P-05 | 🟢 BAJO | `std::vector` sin `reserve()` en loops críticos | Pendiente |

#### Mejoras ya implementadas:
- ✅ CSV parser zero-allocation (`ea07e86`): import 77s → 37s (2.1x)
- ✅ SQL query reemplaza min/max scan (`51416f7`): stats 78s → 29s (2.7x)
- ✅ SQLite file-backed no `:memory:` (`236c61a`): 1GB CSV: 866MB → 94MB RAM (9x)
- ✅ `--stream` CLI mode (`954167f`): 845MB RAM peak, 35% más rápido
- ✅ SQLite WAL + 50k batch + indices diferidos + `EXCLUSIVE` lock

---

### DIMENSIÓN 5: DEVOPS / INFRA (Score: 80/100)

| ID | Severidad | Hallazgo | Estado |
|----|-----------|----------|--------|
| D-01 | 🟡 MEDIO | `CMAKE_BUILD_TYPE` default `Debug` (debería ser `Release`) | Pendiente |
| D-02 | 🟡 MEDIO | `BEAMLAB_ENABLE_ROOT` usa `root-config` sin fallback si no existe | Pendiente |
| D-03 | 🟢 BAJO | Sin `Dockerfile` ni containerización | N/A — app desktop |
| D-04 | 🟢 BAJO | Sin monitoreo/alerting | N/A |

#### Lo que funciona:
- ✅ GitHub Actions CI: build + test en ubuntu-24.04 (`build.yml`)
- ✅ `install()` targets: `cmake --install` funcional
- ✅ CTest integrado: 41/41 tests pasan
- ✅ `FetchContent` para dependencias (googletest v1.14.0, nlohmann_json v3.11.3, sqlite3)
- ✅ `CMAKE_EXPORT_COMPILE_COMMANDS ON` para clangd LSP
- ✅ `-Wall -Wextra -Wpedantic -Wconversion`

---

### DIMENSIÓN 6: CALIDAD, TESTING Y DOCUMENTACIÓN (Score: 58/100)

| ID | Severidad | Hallazgo | Estado |
|----|-----------|----------|--------|
| Q-01 | 🟠 ALTO | Cobertura tests <40%; sin tests integración real | Pendiente |
| Q-02 | 🟡 MEDIO | Sin `CONTRIBUTING.md` ni guía de estilo C++ | Pendiente |
| Q-03 | 🟡 MEDIO | Doble stylesheet en conflicto (`.qss` + inline `MainWindow`) | Pendiente |
| Q-04 | 🟢 BAJO | Sin `clang-format` config | Pendiente |
| Q-05 | 🟢 BAJO | README promete features no 100% funcionales (PDF, VTK) | Pendiente |

#### Lo que funciona:
- ✅ 41 tests unitarios pasan (GoogleTest/CTest)
- ✅ 6 casos de test sintéticos (TC01-TC06)
- ✅ `//!@math` documentación física embebida con generador Python
- ✅ `EXPECTED_RESULTS.md` con criterios de validación
- ✅ `physics_validation.py` para validación numérica
- ✅ README bilingüe EN/ES con badges, arquitectura, build instructions
- ✅ `NEW_ARCHITECTURE.md` — blueprint formal v2.0
- ✅ `BIOSIM_MEMORY.txt` — registro de decisiones

---

## Plan de Implementación por Fases

### FASE 0: Parada de Sangrado (Stop the Bleeding)
**Objetivo:** Fix críticos de seguridad y bugs que rompen producción
**Duración:** 2 días (8h)
**Prioridad:** INMEDIATA

- [ ] **S-01** Verificar y revocar `push.txt` token GitHub + eliminarlo del repo e historial
- [ ] **B-02** Fix race condition MP4: `QEventLoop::ExcludeUserInputEvents` o bloquear tabs
- [ ] **B-01** Agregar guard `count==0` en `finalizeAccumulator()`
- [ ] **B-03** Validación de schema JSON en `loadManifest()` con `QJsonDocument`
- [ ] **S-06** Sanitizar `run_beamlab_full.cmd`: reemplazar `%*` con `%1 %2 ... %9`

**Criterios de aprobación:**
- `push.txt` no existe en repo ni historial reciente
- Exportación MP4 no se corrompe al interactuar con UI
- Manifests inválidos muestran error en vez de crash
- Scripts no ejecutables vía nombres de archivo maliciosos

---

### FASE 1: Fundamentos y Estabilidad
**Objetivo:** Tests, cobertura, bugs de lógica y hardening de seguridad
**Duración:** 1 semana (15h)

- [ ] **Q-01** Expandir tests unitarios: `StoppingPowerEngine` contra NIST PSTAR, `FocusDetector` edge cases
- [ ] **B-05** Fix `FocusDetector` confidence en bordes (pendiente unilateral en vez de 0.1)
- [ ] **B-04** Soporte CSV multilínea en `splitLine()`
- [ ] **B-06** Fix `BetheBlochEngine` partículas detenidas (retornar `inf` o `1e6`)
- [ ] **S-03** Validación path traversal en `copyDirectoryRecursive()`
- [ ] **D-01** Cambiar `CMAKE_BUILD_TYPE` default a `Release`
- [ ] **Q-03** Consolidar stylesheet en `beamlabstudio.qss` (única fuente de verdad)

**Criterios de aprobación:**
- Cobertura tests > 50% en módulos core
- Todos los bugs de severidad ALTO cerrados
- CI verde en GitHub Actions
- UI visualmente consistente en Linux + Windows

---

### FASE 2: Arquitectura y Deuda Técnica
**Objetivo:** Unificar subsistemas, eliminar duplicación, refactors estructurales
**Duración:** 1.5 semanas (20h)

- [ ] **A-02** Unificar `src/simulation/` → `src/biosim/` (migrar `SimulatorWidget` a `StoppingPowerEngine`)
- [ ] **A-01** Continuar descomposición de `MainWindow` (<1000 líneas objetivo)
- [ ] **A-03** Encapsular `TrajectoryDataset::trajectories` (getter const + mutadores controlados)
- [ ] **A-04** Restringir visibilidad de includes con `target_include_directories` PUBLIC/PRIVATE
- [ ] **A-05** Implementar observer pattern entre capas (Qt signals o custom callbacks)
- [ ] **A-06** Estandarizar manejo de errores: `Result<T, E>` pattern

**Criterios de aprobación:**
- `src/simulation/` eliminado o marcado `[[deprecated]]`
- `MainWindow.cpp` < 1000 líneas
- `SimulatorWidget` y `BioSimWidget` producen resultados equivalentes (diferencias documentadas por Sternheimer)
- Compilación sin warnings de acoplamiento cruzado

---

### FASE 3: Performance y Escalabilidad
**Objetivo:** Optimizaciones, caché, GPU rendering
**Duración:** 1 semana (12h)

- [ ] **P-01** Implementar double buffering o display lists en `Scene3DWidget`
- [ ] **P-03** Cachear convex hulls en `ConvexHullEnvelopeExtractor`
- [ ] **P-04** Reemplazar `std::sort` con `std::nth_element` en equal-count bins
- [ ] **P-02** Sampling estratificado para plotting de datasets >1M puntos
- [ ] **P-05** Agregar `.reserve()` en loops de construcción de vectores

**Criterios de aprobación:**
- Render 3D mantiene >30 FPS con 500k vértices
- Exportación de reporte < 5s para dataset de 5M rows
- Sin regresión en precisión de resultados

---

### FASE 4: Pulido y Documentación
**Objetivo:** Docs, DX, monitoreo
**Duración:** 3 días (8h)

- [ ] **Q-02** Crear `CONTRIBUTING.md` con guía de estilo C++, git workflow, build instructions
- [ ] **Q-04** Configurar `.clang-format` y hook pre-commit
- [ ] **Q-05** Actualizar README: quitar claims de VTK, reflejar estado real
- [ ] Agregar `clang-tidy` al CI
- [ ] `DIAGRAM.md` con diagrama de arquitectura actual (Mermaid)
- [ ] Runbook de troubleshooting para usuarios

**Criterios de aprobación:**
- Nuevo dev puede compilar y contribuir en <30 min
- CI incluye linting + formatting check
- Documentación refleja estado real del código

---

## Guías de Implementación por Fase

### Fase 0 — Quick Wins

**S-01 (push.txt):**
```bash
git filter-branch --force --index-filter \
  "git rm --cached --ignore-unmatch push.txt" \
  --prune-empty --tag-name-filter cat -- --all
echo "push.txt" >> .gitignore && git add .gitignore
git commit -m "security: remove push.txt with credentials from history"
# Si token real: revocar en GitHub Settings → Developer settings → Tokens
```

**B-02 (MP4 race):**
```cpp
// En MainWindow::exportTrajectoryVideoTo(), reemplazar:
QApplication::processEvents();
// por:
QEventLoop loop;
loop.processEvents(QEventLoop::ExcludeUserInputEvents);
// ADEMÁS: deshabilitar tabs_ y combined_controls_dock_ antes del loop
// y restaurar al final (finally-style con scope guard)
```

**B-01 (div/0):**
```cpp
// En FrameStatisticsEngine.cpp, namespace anónimo:
FrameStatistics finalizeAccumulator(const FrameAccumulator& acc) {
    FrameStatistics stats{};
    if (acc.count == 0) return stats;  // ← añadir esta línea
    const double inv_n = 1.0 / static_cast<double>(acc.count);
    // ... resto igual
}
```

### Fase 1 — Unificación de Stylesheet

```bash
# 1. Extraer reglas únicas del inline stylesheet de MainWindow
# 2. Mergearlas en beamlabstudio.qss resolviendo conflictos
# 3. Eliminar setStyleSheet() inline de MainWindow.cpp
# 4. Usar setProperty("class", "...") en widgets para selectores CSS
```

### Fase 2 — Unificación de Subsistemas de Simulación

Paso a paso para `SimulatorWidget` → `StoppingPowerEngine`:
```cpp
// Antes (src/simulation/):
#include "simulation/physics/BetheBlochEngine.h"
double dEdx = BetheBlochEngine::betheBlochMass(beta2, gamma, material);

// Después (src/biosim/):
#include "biosim/physics/StoppingPowerEngine.h"
double dEdx = StoppingPowerEngine::dEdx_MeV_cm(kinE_MeV, material);
// Documentar diferencias por Sternheimer (~8-12% a baja energía)
```

---

## Riesgos y Mitigaciones

| Riesgo | Mitigación |
|--------|------------|
| Regresión física al unificar `BetheBlochEngine` → `StoppingPowerEngine` | Validar contra NIST PSTAR con `physics_validation.py`; documentar diferencias |
| `MainWindow` refactor rompe UI existente | Hacer incremental (1 método → 1 clase por commit); tests de humo manuales |
| `git filter-branch` corrompe historial | Hacer backup del repo antes; usar `--force` con precaución |
| Tests nuevos son flaky por dependencia de datos externos | Usar fixtures inline pequeñas, no datasets sintéticos completos |
| Estilos CSS rotos post-consolidación | Screenshot comparativo antes/después en Linux + Windows |

---

## Métricas de Éxito

| KPI | Actual | Objetivo Post-PRA | Cómo medir |
|-----|--------|-------------------|------------|
| Tests pasando | 41/41 | ≥60 tests, ≥50% cobertura | `ctest --output-on-failure` + `gcov/lcov` |
| `MainWindow.cpp` líneas | ~2000 | <1000 | `wc -l src/ui/qt/MainWindow.cpp` |
| Tiempo import 1GB CSV | 37s | <30s | `time beamlab --stream --input data.csv` |
| RAM peak 1GB CSV | 94MB | <100MB | `/usr/bin/time -v` → Maximum resident set size |
| CI build + test | ~3 min | <5 min | GitHub Actions log |
| Seguridad: secrets en repo | 1 conocido | 0 | `git log -p --all -S "TOKEN"` |
| Bugs críticos abiertos | 3 | 0 | Este documento |
| C++ warnings | 0 | 0 | `-Wall -Wextra -Wpedantic -Wconversion` |

---

*Documento generado como parte del Plan de Auditoría Técnica — BeamLabStudio / OpenCode*
*Revisar trimestralmente. Los hallazgos deben validarse contra el código real.*
