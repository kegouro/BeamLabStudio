# PLAN MAESTRO — BeamLabStudio (v2)

> Reestructuración, rigor científico, reparación y pulido.
> Estado: **propuesta · pendiente de aprobación para implementar**.
> Fecha: 2026-06-19 · Rama base: `feat/ui-redesign-macos` · Integración: `integration/reconstruction`.
> v2 incorpora revisión externa (GLM 5.2) verificada contra el código real.

---

## 0. Meta

### Visión
Simulador y analizador de haces de partículas (física médica) con **física validada contra estándares NIST/ICRU**, pipeline de datos sin pérdida, UI moderna y arquitectura limpia en capas. Que un físico médico confíe en cada número y un estudiante entienda cada gráfico.

### Principios rectores
1. **Rigor primero.** Cada constante, fórmula y unidad se valida contra referencia publicada (NIST PSTAR/ASTAR, ICRU-49/73/90, PDG). Sin números mágicos sin cita.
2. **Ponytail (minimalismo).** La escalera: ¿necesita existir? → stdlib → feature nativo → dependencia ya instalada → una línea → mínimo código. **Borrar > añadir.** Cada fase con deuda cierra en **neto negativo de líneas**. Atajos deliberados con comentario `ponytail: <qué> | techo: <límite> | upgrade: <cómo>` → rastreable con `/ponytail-debt`.
3. **No recortar donde duele.** Ponytail jamás simplifica: validación en fronteras de confianza, manejo de errores que evita pérdida de datos, seguridad, accesibilidad, **y la física/calibración** ("el mundo real necesita una perilla que el modelo mínimo no ve").
4. **Una ruta, no dos.** Toda duplicación (io vs services, widget vs MVP, dos colormaps) se resuelve consolidando en una y **borrando** la otra. Migrar y verificar **antes** de borrar.
5. **Verde siempre.** Cada cambio no trivial deja **un check ejecutable**. `main` se toca solo al final; el trabajo vive en `integration/reconstruction`.

### ⚠️ Límite clínico (disclaimer vinculante, 4 frentes)
Validar contra NIST/ICRU da **precisión científica de grado de referencia**, apta para investigación y educación avanzada. **NO** equivale a certificación clínica regulatoria (IEC 62083 / FDA 510(k) / marcado CE / QA institucional). El disclaimer *"No apto para planificación de tratamiento de pacientes reales"* aparece en:
- **UI** — banner por pestaña relevante.
- **CLI** — `stdout` al iniciar.
- **Reports exportados** — header de cada archivo.
- **Logs** — tag `[NON_CLINICAL]`.

### Alcance
Dentro: reparación (biosim, gradientes), rigor físico clínico-grade, reestructuración (io→services, plugins, tests, CI), pulido UI, limpieza. Fuera: certificación regulatoria. Features de F6: se eligen contigo de una lista propuesta (no scope-creep abierto).

---

## 1. Diagnóstico consolidado

Evidencia de 4 agentes de diagnóstico (read-only). IDs: **B**uild · **S**cience · **G**radient · biosim(**X**) · **D**ebt.

### Build & estructura
| ID | Hallazgo | Archivo:línea | Sev |
|----|----------|---------------|-----|
| B1 | `energy_deposition_steps(1).csv` de **19 GB** versionado en working tree | `tests/` | 🔴 Crítica |
| B2 | `BeamLabStudio.dmg` (43 MB), `graphify-out/`, lock de LibreOffice = basura | repo | 🟠 Alta |
| B3 | `StoppingPowerEngine.cpp` compilado **2×** (domain + biosim) → símbolo duplicado / ODR | `src/domain/CMakeLists.txt:5` | 🟠 Alta |
| B4 | `beamlab_ui` no linka `beamlab_domain` → `SimulationEngine` inaccesible desde UI | `src/ui/CMakeLists.txt:93` | 🟠 Alta |
| B5 | Tests sin label `unit` → `ctest -L unit` da vacío (98 corren sin `-L`) | `tests/unit/CMakeLists.txt` | 🟡 Media |
| B6 | 15–17 warnings (`-Wsign-conversion`, unused, `nodiscard` ignorado). Flags ya activos salvo `-Werror` | varios | 🟡 Media |
| D1 | `pipeline_` (AnalysisPipeline) miembro muerto, 0 usos | `AnalysisPresenter.h:46` | 🟡 Media |
| D2 | Duplicación **`src/io` vs `src/services`** (importer/exporter/storage 2×) | ambos árboles | 🟠 Alta |
| D3 | Tests desconectados: domain, platform, services, integration, **performance** (7 benchmarks) | `tests/**` | 🟠 Alta |
| D4 | `BioSimPresenter` / `ExportPresenter` sin commit ni wiring CMake | `src/ui/qt/presenters/` | 🟡 Media |

**Veredicto build:** sano (headless OK, Qt OK con `-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt`, 98/98 verde). Problema = deuda y basura, no compilación.

### Rigor científico
| ID | Hallazgo | Archivo:línea | Sev |
|----|----------|---------------|-----|
| S1 | Bethe-Bloch **+25% vs NIST PSTAR** — faltan correcciones de capa/densidad | `SimulationEngine.cpp:82` | 🟠 Alta |
| S2 | Straggling `0.05*xi` ad-hoc sin base física (debe Bohr/Landau) | `SimulationEngine.cpp:146` | 🟠 Alta |
| S3 | Signo de carga invertido por PDG (protón→−1, γ→−1) — metadatos erróneos | `Geant4CsvImporter.cpp:538` | 🟡 Media |
| S4 | `importStreaming` (producción) **omite** validaciones del batch → acepta energías/tiempos negativos | `Geant4CsvImporter.cpp:651` | 🟡 Media |
| S5 | `validateAgainstNist()` compara contra el **propio output**, no contra NIST → validación falsa | `SimulationEngine.cpp:244` | 🟡 Media |
| S6 | Tie-break de foco: bins de igual r_rms → off-by-one (~1.6 cm) | `FocusDetector.cpp:47` | 🟡 Media |
| S7 | r_rms centroide-corregido, no radio desde eje (documentado) | `FrameStatisticsEngine.cpp:109` | 🟢 Baja |
| S8 | Masa del protón inconsistente config (938.27208816) vs importer (938.272089) | `Geant4CsvImporter.cpp:544` | 🟢 Baja |
| S9 | Golden ref TC01 desactualizada (5 vs 6 columnas) | `tests/synthetic_dataset/TC01` | 🟢 Baja |

**Veredicto rigor:** el **pipeline geométrico** preserva datos fielmente. La **física activa** está degradada y su "validación" es ilusoria.

### Gradientes de energía (colormap de energía en trayectorias 3D)
| ID | Hallazgo | Archivo:línea | Sev |
|----|----------|---------------|-----|
| G1 | `loadEnergyCSV()` matchea vértices por **igualdad exacta de bits** de doubles → `vertex_energies_` vacío → gradiente off | `ObjViewerWidget.cpp:177` | 🔴 Crítica |
| G2 | `traj3d_page` **no registrada en nav rail** (`(void)traj3d_page`) → inaccesible | `MainWindow.cpp:1242` | 🔴 Crítica |
| G3 | `Scene3DWidget` asigna energía por índice posicional → frágil a desincronía | `Scene3DWidget.cpp:290` | 🟠 Alta |
| G4 | Exporter usa `setprecision(12)`; round-trip IEEE-754 exige **17 + formato** | `VisualizationDataExporter.cpp:126` | 🟠 Alta |
| G5 | `EnergyColorMapper` (8 paletas) = código muerto; viewports usan 5 stops hardcoded | `src/biosim/ui/qt/EnergyColorMapper.cpp` | 🟡 Media |
| G6 | Subsampling: `AnalysisPresenter` fuerza 10000 vs defaults 300 → deriva CSV/OBJ | `AnalysisPresenter.cpp:48` | 🟡 Media |

### Simulador biológico
| ID | Hallazgo | Archivo:línea | Sev |
|----|----------|---------------|-----|
| X1 | `BioSimPresenter.h` incluye headers inexistentes (`domain/simulation/BioSimConfig.h`) → nunca compiló | `BioSimPresenter.h:6` | 🔴 Crítica |
| X2 | `runSimulation()` = stub `TODO`, emite `BioSimResult{}` vacío (`valid=false`) | `BioSimPresenter.cpp:28` | 🔴 Crítica |
| X3 | `BioSimPresenter.cpp` no está en ningún `CMakeLists.txt` | `src/ui/CMakeLists.txt` | 🔴 Crítica |
| X4 | `BioSimView` solo header, sin `.cpp` → no instanciable | `src/ui/views/BioSimView.h` | 🟠 Alta |
| X5 | `MainWindow` no instancia el presenter (usa `BioSimWidget` directo) | `MainWindow.cpp:1252` | 🔴 Crítica |
| X6 | `onStop()` no cancela el `QFuture` en curso | `BioSimWidget.cpp:231` | 🟡 Media |

**Veredicto biosim:** la **física real corre** por `BioSimWidget → BioSimRunner`. La **capa MVP** es código muerto desde su creación. Decisión: completar MVP y consolidar — **con fallback** a la ruta directa si la MVP cuesta más que repararla.

---

## 2. Arquitectura objetivo

```
beamlab_ui        Qt6 · MainWindow + Presenters (MVP) + Views + widgets
   │  (linka domain + services + biosim)
beamlab_biosim    UI Qt de simulación + viewport 3D            [QT_UI=ON]
   │
beamlab_services  IImporter/IExporter/IStorageBackend, AnalysisOrchestrator
   │              ← reemplaza ApplicationBootstrap + src/io
beamlab_domain    Física pura (sin Qt): SimulationEngine, MaterialRegistry,
                  ParticleRegistry, validación NIST
```

**Movimientos estructurales (ponytail = consolidar y borrar):**
- `src/io/` → migrar lo que falte a `src/services/`, **luego borrar `src/io/`** (migrar y verificar primero).
- `ApplicationBootstrap` → `AnalysisOrchestrator`, con **feature flag** durante la transición. Borrar `pipeline_` muerto (D1).
- Una sola `StoppingPowerEngine` en `domain`; `biosim` la consume vía link (B3).
- Un solo `EnergyColorMapper`; borrar stops hardcoded (G5).
- BioSim: una sola ruta (MVP); borrar la directa tras verificar (X1-X6).
- `src/platform/` (EventBus/CommandBus/IPlugin): activar solo si una fase lo necesita (YAGNI).

---

## 3. Fases

Formato: **objetivo · cambios (IDs) · criterios · ponytail-move · skills · zona · check**.

### F0 — Higiene & build sano `[serial · lo hace el orquestador]`
- **Objetivo:** árbol limpio, build sin duplicación, tests etiquetados, convenciones y rama de integración listas.
- **Cambios:** B1 `rm` 19 GB CSV + `.gitignore`. B2 `rm` dmg/graphify/locks + `.gitignore`. B3 quitar dup en `domain/CMakeLists.txt:5`. B4 añadir `beamlab_domain` al link de `beamlab_ui`. B5 label `unit` en `add_test`. Consolidar planes redundantes (`BUG_ATTACK_PLAN.md`, `BLUEPRINT.md`, `AUDITORIA.md`, `PRA_*`) → este doc. Actualizar `CLAUDE.md` (restricciones UI levantadas + **convenciones de código**, ver Apéndice B). Crear `integration/reconstruction` y `.claude/PHASE_STATE.md`.
- **ponytail-move:** **−19 GB + −~4 docs + −1 compilación dup.**
- **Skills:** `ponytail-audit`, `gh-cli`.
- **Check:** build headless + Qt sin warning de símbolo dup; `ctest -L unit` → 98 verde; `git status` sin basura.

### F1a — Física básica + validación real `[domain · desbloquea F4 y §4]`
- **Objetivo:** Bethe-Bloch con I-values ICRU-90, **≤5 % vs NIST PSTAR** en banda clínica 50–250 MeV (agua).
- **Cambios:** S1 (Bethe-Bloch base, sin Sternheimer aún) · S5 `validateAgainstNist()` real contra fixture NIST · S8 masa protón desde `particles_builtin.json`. `SimulationEngine.cpp:82,244`.
- **Criterios:** `test_physics_regression` ≤5 % en 50–250 MeV; dE/dx monótona; sin número mágico sin cita.
- **ponytail-move:** solo la física que la validación exige; aproximaciones marcadas `ponytail:`. Perillas (I-value, tolerancia) configurables.
- **Skills:** `dimensional-analysis`, `property-based-testing`, `tdd`, WebSearch (NIST/ICRU).
- **Zona:** `src/domain/`, `config/`, `tests/fixtures/`.

### F1b — Física avanzada `[domain · tras F1a, paraleliza con F4]`
- **Objetivo:** **≤2 % vs NIST** en 10–300 MeV.
- **Cambios:** S2 straggling Bohr `σ²=ξ·W_max·(1−β²/2)` / Landau-Vavilov según `κ` · efecto densidad (Sternheimer) · corrección de capa (`C/Z`) · MCS Highland 13.6 MeV con término log. `SimulationEngine.cpp:146`.
- **Criterios:** ≤2 % en 10–300 MeV; <10 MeV puede quedar con `ponytail:` (techo declarado).
- **Skills:** `dimensional-analysis`, `property-based-testing`, `tdd`.
- **Zona:** `src/domain/`.

### F2 — Pipeline de datos & rigor `[services/io · paralela]`
- **Objetivo:** ingestión y export sin pérdida ni sesgo; validación unificada.
- **Cambios:** S3 tabla PDG→carga. S4 unificar guards batch+streaming en **una** función (rechazar `edep<0`, `kinE<0`, `t<0`). S6 tie-break de foco (promedio del plateau). S9 regenerar golden TC01 (6 col). G4 export con `std::setprecision(17) + std::scientific`; **test round-trip por `memcmp` de bits, no por valor**.
- **ponytail-move:** una sola validación (borrar la divergencia batch/streaming, no duplicar guards).
- **Skills:** `tdd`, `property-based-testing`, `dimensional-analysis`.
- **Zona:** `src/io/`, `src/services/`, `src/analysis/`, `tests/synthetic_dataset/`.
- **Check:** round-trip exacto a bits; streaming≡batch; TC01..TC06 verdes.

### F3 — Gradientes de energía `[ui viewports · paralela]`
- **Objetivo:** colormap de energía visible, correcto y accesible.
- **Cambios:**
  - **G1 (fix definitivo, no posicional ciego):** el CSV **ya contiene** `trajectory_index, sample_index`; matchear por esos índices explícitos (o emitirlos como atributo del OBJ). **Borrar** el match por igualdad de bits. Decisión final (índices-en-CSV vs vertex-color-en-OBJ) se toma en F3 con `ponytail-review`: la más lazy que sea robusta. Fallback declarado: gradiente por profundidad Z si falta el dato (`ponytail:`).
  - G2 registrar `traj3d_page` en nav rail. G3 guard explícito + warning (no truncado silencioso). G5 conectar `EnergyColorMapper` (8 paletas) a ambos viewports + selector; **borrar** los 5 stops hardcoded. G6 subsampling a constante compartida CSV=OBJ.
- **ponytail-move:** una sola `energyToColor`; **−2 implementaciones**. Reusar mapper existente, no escribir uno nuevo.
- **Skills:** `systematic-debugging`, `frontend-design`, `tdd`.
- **Zona:** `src/ui/qt/`, `presenters`, `io/exporters`.
- **Check:** `vertex_energies_.size()==vertices_.size()`; gradiente visible (screenshot); paletas conmutables.

### F4 — BioSim MVP completo `[presenters/views/wiring · tras F1a]`
- **Objetivo:** una sola ruta biosim (MVP) con física real del dominio.
- **Cambios:** X1 includes → `biosim/core/...`. X2 `runSimulation()` real con `BioSimRunner` (ref: `BioSimWidget::onRun()`). X4 crear `BioSimView.cpp`. X3 añadir a CMake + link `beamlab_domain`. X5 instanciar+conectar en `MainWindow`. X6 `onStop()` cancela el `QFuture`. Tras verificar: **borrar** la ruta directa.
- **Fallback:** si la MVP no compila/conecta tras esfuerzo acotado (más caro que reparar la directa), **pivotar**: reparar `BioSimWidget` (X6) y reportar `BLOCKED` con razón. No callejón sin salida.
- **ponytail-move:** MVP = wiring, no reescritura; reusar `BioSimRunner`. Cerrar en una ruta.
- **Skills:** `systematic-debugging`, `tdd`, `ponytail`.
- **Zona:** `src/ui/qt/presenters/`, `src/ui/views/`, `MainWindow`, CMake.
- **Check:** tab BioSim end-to-end (CSV→Bragg→viewport); `valid=true`, tracks>0; cancelar detiene de verdad.

### F5 — Reestructura: services + tests + CI + baseline `[tras F2]`
- **Objetivo:** arquitectura destino activa, deuda saldada, CI que protege el rigor y la performance.
- **Cambios:** D2 completar `src/io → src/services` y **borrar `src/io`** (tras migrar+verificar, con feature flag). D1 borrar `pipeline_`. Reemplazar `ApplicationBootstrap` por `AnalysisOrchestrator`. D3 reconectar tests (domain/platform/services/integration) o borrar obsoletos. **Reconectar los 7 benchmarks** y capturar `tests/performance/baseline.json` (tiempo sim TC01, memoria pico, tamaño output). B6 limpiar warnings → **activar `-Werror`** (solo ahora que están en cero). CI: workflow que corre validación física (§4) + `ctest` + comparación vs baseline (±10 %) en cada PR.
- **ponytail-move:** **−todo `src/io`**, −`pipeline_`, −`ApplicationBootstrap`, −tests muertos. `ponytail-audit` dirige; `ponytail-debt` audita los `ponytail:`.
- **Skills:** `improve-codebase-architecture`, `ponytail-audit`, `ponytail-debt`, `semgrep`, `codeql`.
- **Zona:** `src/services/`, `src/io/`, `src/app/`, `tests/`, `.github/workflows/`.
- **Check:** producción usa `AnalysisOrchestrator`; `src/io` no existe; CI verde con gate físico + de performance; 0 warnings; `-Werror` activo.

### F6 — Pulido + expansión + docs `[serial · cierre]`
- **Objetivo:** experiencia pulida, disclaimers, docs, y las 7 features elegidas.
- **Base:** disclaimers en los **4 frentes** (§0). `frontend-design` para consistencia/accesibilidad. Docs de arquitectura y física con citas. `ponytail-debt` final.
- **Features elegidas** (cada una pasa la escalera ponytail; versión mínima que aporte valor):
  | # | Feature | Reusa | Check |
  |---|---------|-------|-------|
  | C1 | **SOBP** (Spread-Out Bragg Peak): superponer Bragg de varias energías → meseta uniforme | `BraggPeakCalculator` | meseta plana dentro de tolerancia en región objetivo |
  | C2 | **DVH** (dosis-volumen): histograma acumulado | datos de dosis | DVH monótono no creciente; 100 % vol a dosis 0 |
  | C3 | **Mapa LET/RBE**: colormap LET y RBE | `EnergyScaleConverter` (LET/RBE/BED ya implementados) | LET>0, RBE≥1 en rango |
  | C4 | **Panel validación NIST en vivo**: tabla dE/dx vs NIST, % desviación | suite §4 / F1 | muestra desviaciones reales de F1a/F1b |
  | P1 | **Export científico**: Parquet/ROOT (flags ya existen) + DICOM-RT dose | `BEAMLAB_ENABLE_PARQUET/ROOT` | round-trip Parquet; DICOM válido (dcmtk/pydicom). DICOM-RT es el más caro → versión mínima primero |
  | P2 | **Reporte PDF**: curvas + tabla NIST + disclaimers | skill `pdf` | PDF con header de disclaimer |
  | P3 | **Consola Python / plugins**: exponer `pybeamlab` o activar `.so` | `src/scripting/` (`pybeamlab`), `src/platform/` (`IPlugin/PluginHost`) | script Python corre sobre un dataset; o un `.so` carga |
- **Backlog (no elegido):** Comparador multi-haz.
- **Skills:** `frontend-design`, `verification-before-completion`, `pdf`.
- **Zona:** `src/ui/`, `src/scripting/`, `src/platform/`, `src/io/exporters/`, `docs/`.
- **Check:** disclaimers en 4 frentes; docs ok; cada feature con su check verde; `main` verde.

---

## 4. Validación científica (transversal)

Construida en F1a/F1b/F2, automatizada en CI (F5).

- **Referencias versionadas en repo:** `tests/fixtures/nist_pstar_proton_water.csv` (+ ASTAR/alfa, otros materiales). <100 KB → git normal, **sin LFS**. **SHA-256 en el header** del archivo para auditoría/reproducibilidad.
- **Tests de regresión física:** `SimulationEngine` vs fixture con **tolerancia por banda** (≤5 % en 10–50 MeV donde dominan correcciones de capa; ≤2 % en 50–300 MeV). Reemplaza la autorreferencia S5.
- **Property-based:** dE/dx monótona decreciente con E; rango creciente; conservación de energía en el track; positividad de dosis.
- **Golden datasets:** TC01..TC06 regenerados (S9).
- **Gate CI:** PR que desvíe física fuera de tolerancia o performance >±10 % vs baseline **no mergea**.
- **Disclaimer** en cada reporte (header) y UI.

---

## 5. Ejecución

### Árbol de dependencias real (corregido)
```
F0 (serial)
  ├─→ F1a (domain básico) ─┬─→ F1b (domain avanzado) ─┐
  │                        └─→ F4 (biosim MVP) ────────┤
  ├─→ F2 (services/io) ───────→ F5 (reestructura) ─────┼─→ F6 (pulido) → merge a main
  └─→ F3 (ui viewports) ───────────────────────────────┘
```
Paralelo real tras F0: **F1a ∥ F2 ∥ F3**. Luego F1b y F4 (necesitan F1a). Luego F5 (necesita F2). F6 al final.

### Protocolo de delegación (comunicación por archivos = ahorro de tokens)
Por cada fase F1a–F5:
1. **Orquestador** abre worktree (`git worktree add ../beamlab-F<N> feat/phase-F<N>`) y escribe `.claude/briefs/F<N>.md`: IDs a resolver, archivos+líneas, criterio de aceptación, skills, **límite de zona** ("no toques fuera de X"), y "reporta a `.claude/reports/F<N>.md`".
2. **Subagente** recibe **solo su brief** (no el plan completo): `tdd` (test que falla) → fix mínimo → `ponytail-review` → tests locales → reporte con: archivos, líneas +/−, neto, tests, `ponytail:` añadidos, desviaciones, estado `PASS|PARTIAL|BLOCKED`.
3. **Orquestador** lee el reporte (no el diff a ojo): si `PASS` → `code-reviewer` agent + `verification-before-completion` (build+tests en worktree) → merge a `integration/reconstruction`. Si `PARTIAL/BLOCKED` → ajustar brief y re-delegar.

### Rollback / seguridad (no negociable)
- Nunca borrar `src/io` sin que F5 haya migrado y verificado. Nunca borrar `BioSimWidget` sin F4 verificado.
- Trabajo en `integration/reconstruction`; **merge a `main` solo al final de F6** con todos los gates verde.
- Fase que rompe tests en integración → revert inmediato + re-planear. Feature flag para `AnalysisOrchestrator` durante F5.

### QA gates (por fase, sin excepción)
`tdd` → fix → `ponytail-review` → `code-reviewer` agent → `verification-before-completion` → `ponytail-debt` (registrar atajos) → merge a integración → suite completa verde → actualizar `PHASE_STATE.md`. `second-opinion` opcional en fases de física.

### Reporting por fase
Tabla: hallazgos resueltos · archivos · líneas +/− · **neto** · tests nuevos/totales · `ponytail:` añadidos · desviaciones · estado · próxima fase desbloqueada.

### MCP / herramientas
WebSearch/WebFetch (NIST/ICRU) · claude-in-chrome / computer-use (verificación visual gradientes/biosim) · semgrep/codeql (F5). Memoria de sesión: archivos en `.claude/` (no MCP nuevo).

---

## 6. Métricas de éxito y riesgos

### Éxito
- Física: ≤2 % vs NIST en 50–300 MeV, ≤5 % en 10–50; validación real.
- Reparación: biosim end-to-end; gradiente visible y correcto.
- Estructura: `src/io` eliminado; `AnalysisOrchestrator` en producción; **neto de líneas negativo** global.
- Calidad: integración verde tras cada fase; CI con gate físico + performance; `-Werror`; 0 basura.
- Trazabilidad: todo atajo en `/ponytail-debt`, sin `no-trigger`.

### Riesgos y mitigación
| Riesgo | Mitigación |
|--------|------------|
| Correcciones ICRU desvían cronograma | F1a entrega banda clínica primero; F1b avanzado aparte; low-E como `ponytail:` |
| Borrar `src/io` rompe producción | Migrar+flag+verificar antes de borrar; tests cubren ambos en transición |
| MVP biosim más rota de lo esperado | Fallback a reparar ruta directa (X6); reportar BLOCKED |
| Refactor introduce regresión de performance | Baseline pre-F5 + gate CI ±10 % |
| Merges paralelos conflictivos | Zonas disjuntas; worktrees; integración secuencial con gate |
| "Clínico-grade" malinterpretado | Disclaimer en 4 frentes |

---

## 7. Próximo paso
Tras tu aprobación:
1. `writing-plans` genera el plan paso-a-paso de **F0 + F1a**.
2. Creo `PHASE_STATE.md`, ejecuto **F0** (serial), abro worktrees **F1a ∥ F2 ∥ F3**.
3. Cada fase cierra con sus gates y un reporte a ti.

---

## Apéndice A — Skills & MCP por tema
| Tema | Skills | MCP |
|------|--------|-----|
| Minimalismo / deuda | `ponytail`, `ponytail-audit`, `ponytail-review`, `ponytail-debt` | — |
| Física / unidades | `dimensional-analysis`, `property-based-testing`, `tdd` | WebSearch/WebFetch (NIST/ICRU) |
| Debug | `systematic-debugging` | claude-in-chrome, computer-use |
| Arquitectura | `improve-codebase-architecture` | — |
| Calidad / seguridad | `semgrep`, `codeql`, `c-review` | — |
| UI | `frontend-design` | claude-in-chrome, computer-use |
| Proceso | `using-git-worktrees`, `dispatching-parallel-agents`, `subagent-driven-development`, `requesting-code-review`, `verification-before-completion` | — |
| GitHub | `gh-cli` | — |

> Nota: las skills `cmake-modern`, `cpp20-concepts`, `qt6-signals`, `physics-validation`, etc. **no existen** y no se crearán (over-engineering). Las locales cubren el trabajo; las convenciones van en CLAUDE.md (Apéndice B).

## Apéndice B — Convenciones de código (→ CLAUDE.md en F0)
Ajustadas a **C++20** (verificado: `cxx_std_20`) y a las convenciones existentes.
- **Recursos:** RAII; `std::unique_ptr`/stack; `shared_ptr` solo con ownership compartido real; `std::span<const T>` para vistas.
- **const/noexcept:** `const` en métodos que no mutan; `noexcept` en moves/dtors/leaf; `[[nodiscard]]` en factories/validators/compute.
- **Errores:** `std::optional` + código de error para fallos esperables; excepciones para lo inesperado; nunca `throw` en dtor. **`std::expected` NO** (es C++23; si surge necesidad real de errores ricos, evaluar `tl::expected` como dependencia justificada).
- **Templates:** concepts C++20 donde aporten (`template<std::floating_point T>`), no SFINAE; sin generalizar por especular (YAGNI).
- **Constantes físicas:** `constexpr` con cita — `constexpr double proton_mass_MeV = 938.27208816; // PDG 2022`. Sin mágico sin cita.
- **Includes:** seguir la convención existente del repo (verificar en F0; no migración masiva).
- **Naming:** `snake_case` vars/funciones, `PascalCase` tipos, `CONSTANT_CASE` constexpr globales, miembro privado sufijo `_`.
- **Tests:** `test_<unidad>_<escenario>_<esperado>`; Arrange-Act-Assert.
- **CMake:** target-based (`target_*`); `PRIVATE/PUBLIC/INTERFACE` correcto; `-Werror` **solo tras** limpiar warnings (F5).
- **Qt6:** `connect` con functor (`&Clase::señal`), nunca macros `SIGNAL/SLOT`; `QPointer` para ownership no exclusiva; captura mínima en lambdas.
- **Git:** Conventional Commits (`feat(physics): …`, `fix(ui): …`, `refactor(services): …`); un commit por unidad lógica.
