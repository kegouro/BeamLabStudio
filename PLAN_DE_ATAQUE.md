# PLAN DE ATAQUE — BeamLabStudio

> **Versión:** 1.0 — 2026-05-19  
> **Audit verificado contra:** código real en `src/`, `scripts/`, `CMakeLists.txt`, `tests/`  
> **Principio rector:** El proyecto debe compilar y funcionar tras cada fase. No "big bang rewrites".

---

## Fase 0: Validación del Audit

Cada claim del audit fue verificada contra el código real. Esta tabla muestra discrepancias encontradas.

| # | Audit Claim | Sección Audit | Realidad en Código | Veredicto |
|---|------------|---------------|---------------------|-----------|
| 0.1 | "Interactive 3D VTK viewer" | README, §5.2 | **Cero código VTK.** `Scene3DWidget` es renderer QPainter puro. `find_package(VTK)` no existe en CMakeLists.txt. | **[AUDIT INCORRECTO]** — VTK es solo aspiración, no implementación |
| 0.2 | "nlohmann/json is a dependency" | README | **Cero includes** de nlohmann en todo `src/`. El CMakeLists.txt no tiene `find_package(nlohmann_json)`. El parsing de manifests usa regex (`extractJsonString`/`extractJsonNumber`) o `QJsonDocument`. | **[AUDIT INCORRECTO]** — nlohmann/json no se usa |
| 0.3 | "MainWindow.cpp tiene ~2500 líneas" | §1.1 | **4445 líneas reales** (`wc -l`). | **[AUDIT PARCIAL]** — Subestimación por ~78% |
| 0.4 | "No hay interfaces abstractas (IImporter, IExporter)" | §1.4 | **Existen `IImporter`** (6 métodos virtuales puros), **`IExporter`** (1 método), **`IEnvelopeExtractor`** (2 métodos). | **[AUDIT INCORRECTO]** — Las interfaces SÍ existen |
| 0.5 | "No hay `[[nodiscard]]` en funciones críticas" | §6.7 | **173 instancias de `[[nodiscard]]`** en el código. Toda la capa biosim, importers, exporters y engines lo usan. | **[AUDIT INCORRECTO]** — `[[nodiscard]]` es ubicuo |
| 0.6 | "`makeUniqueTrajectoryId` tiene overflow" | §3.7 | **Corregido.** Lines 195-205 de `Geant4CsvImporter.cpp` tienen guardas de overflow con `kMaxEvent`. | **[AUDIT INCORRECTO]** — Ya fue fixeado |
| 0.7 | "Bethe-Bloch sin corrección de densidad" | §9.1 | **Parcialmente cierto.** El viejo `BetheBlochEngine` (`src/simulation/`) no tiene Sternheimer. El nuevo `StoppingPowerEngine` (`src/biosim/physics/`) **sí incluye Sternheimer density correction** (línea 17-36, `sternheimerDelta()`). | **[AUDIT PARCIAL]** — Solo aplica a la capa vieja |
| 0.8 | "`Scene3DWidget` no maneja OBJ con normales/texturas" | §3.11 | **Falso.** `parseObjIndex()` (líneas 23-42) usa `splitObjToken()` que correctamente separa `v/vt/vn` en caras `f`. Las definiciones `vn`/`vt` se ignoran pero eso es correcto para un visor sin shading. | **[AUDIT INCORRECTO]** — El parser maneja índices compuestos |
| 0.9 | "No hay tests visibles" | Apéndice | **Existen tests.** Directorio `tests/synthetic_dataset/` con 6 casos (TC01-TC06), `EXPECTED_RESULTS.md` con criterios de validación, y `physics_validation.py`. | **[AUDIT INCORRECTO]** — Sí hay tests |
| 0.10 | "Script `run_beamlab_full.sh` no auditado" | Consideración | **Sí existe y es seguro.** Todas las variables se pasan con comillas dobles (`"$INPUT"`, `"$OUTPUT"`, `"$@"`). No hay riesgo de command injection vía el script shell. | **[NO EN AUDIT]** — Script verificado, es seguro |
| 0.11 | "Stylesheet inline masivo (200+ líneas)" | §7.1 | **Existe `.qss` externo** en `src/ui/qt/styles/beamlabstudio.qss` (484 líneas) cargado en `main.cpp:179-187`. PERO `MainWindow.cpp:606` tiene un **segundo stylesheet inline** de ~370 líneas que **sobrescribe** y **conflicta** con el `.qss`. | **[AUDIT PARCIAL]** — Doble stylesheet en conflicto |
| 0.12 | "Módulo biosim no existe en el audit" | — | **El audit ignora completamente `src/biosim/`** (~33 archivos, 22 tareas completadas). Contiene física más avanzada que la capa `simulation/` (Sternheimer, MCS, Vavilov straggling, Bortfeld Bragg peak). | **[NO EN AUDIT]** — Subsistema entero no auditado |
| 0.13 | "`$@` sin comillas en script" | §2.1 | **Falso.** `run_beamlab_full.sh:62` usa `"$@"` correctamente. | **[AUDIT INCORRECTO]** |
| 0.14 | "`push.txt` contiene token de GitHub" | — | **Contiene `https://TU_TOKEN@github.com/...`**. Token hardcodeado o placeholder sin limpiar. | **[NO EN AUDIT]** — Riesgo de seguridad |
| 0.15 | "`CMAKE_BUILD_TYPE` default Debug" | §5.1 | **Verificado.** `CMakeLists.txt:13-15` fuerza `Debug`. Para uso científico debería ser `Release`. | **[VERIFICADO]** |
| 0.16 | "Dos subsistemas de simulación parcialmente redundantes" | — | `src/simulation/` (viejo, sin Sternheimer) y `src/biosim/` (nuevo, con Sternheimer+MCS+Vavilov) coexisten con responsabilidades solapadas. `TissueRegistry` vs `BioMaterialLibrary`, `BetheBlochEngine` vs `StoppingPowerEngine`. | **[NO EN AUDIT]** — Duplicación arquitectónica |

---

## Fase 1: Parada de Sangrado

Fixes que evitan crashes, corrupción de datos o resultados científicamente inválidos. Hacer **primero y antes de cualquier otra cosa**.

### [F1-01] División por cero potencial en `finalizeAccumulator()`
- **Archivo:** `src/analysis/statistics/FrameStatisticsEngine.cpp:96-100`
- **Severidad:** 🟠 ALTO
- **Audit ref:** §3.1
- **Estado:** **Parcialmente correcto** — Todos los callers (`computeSynchronizedFrames:235`, `computeUniformAxialBins:291`, `computeEqualCountAxialBins:337`) protegen con `if (acc.count < 3) continue`. Pero `finalizeAccumulator()` es una función libre en namespace anónimo. Si alguien en el futuro la llama sin el guard, crashea.
- **Descripción:** `const double inv_n = 1.0 / static_cast<double>(acc.count)` produce infinito si `acc.count == 0`, causando NaN en cascada en todas las estadísticas downstream (r_rms, sigma_u, sigma_v).
- **Reproducción:** No reproducible con el código actual (los callers protegen). Pero si se modifica cualquier caller en el futuro y se omite el guard, el bug emerge silenciosamente con NaNs en outputs.
- **Fix propuesto:**
```cpp
FrameStatistics finalizeAccumulator(const FrameAccumulator& acc)
{
    FrameStatistics stats{};
    if (acc.count == 0) return stats;  // <-- añadir esta línea
    
    const double inv_n = 1.0 / static_cast<double>(acc.count);
    // ... resto igual
}
```
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 0.25 horas

### [F1-02] Race condition en exportación MP4 — UI interactiva durante captura
- **Archivo:** `src/ui/qt/MainWindow.cpp:3023`
- **Severidad:** 🟠 ALTO
- **Audit ref:** §3.4
- **Estado:** **Verificado**
- **Descripción:** `QApplication::processEvents()` en la línea 3023 (dentro del loop de captura de frames) permite que el usuario cambie de tab, modifique sliders, o cierre la ventana mientras se capturan los 120 frames del MP4. Esto corrompe la animación (frames con contenido incorrecto) o causa crashes si se destruye `combined_scene_viewer_`.
- **Reproducción:**
  1. Abrir un run con datos de trayectorias
  2. Ir a la pestaña "Combined 3D Scene"
  3. Clic en "Export → Export MP4"
  4. **Inmediatamente** cambiar a otra pestaña o mover el slider de trayectorias
  5. Resultado: video corrupto (frames de diferentes vistas) o crash
- **Fix propuesto:**
```cpp
// Reemplazar processEvents() con:
QEventLoop loop;
loop.processEvents(QEventLoop::ExcludeUserInputEvents);
```
O mejor: deshabilitar `tabs_` y `combined_controls_dock_` durante la exportación y restaurarlos al final.
- **Riesgo de regresión:** Medio (la UI se congela durante la exportación, pero eso es esperable y aceptable)
- **Tiempo estimado:** 1.5 horas

### [F1-03] Crash por falta de validación de schema en manifiestos
- **Archivo:** `src/ui/qt/MainWindow.cpp:2397-2457`
- **Severidad:** 🟠 ALTO
- **Audit ref:** §2.3
- **Estado:** **Verificado**
- **Descripción:** `loadManifest()` usa `extractJsonString()`/`extractJsonNumber()` (basados en regex) para extraer campos del manifest JSON. Si un campo esperado no existe o tiene formato incorrecto, las funciones retornan `QString{}` vacío. Esto propaga cadenas vacías a `resolvePath()`, `loadObj()`, `loadEnergyCSV()`, etc., que pueden causar crashes o estados inconsistentes.
- **Reproducción:**
  1. Crear un archivo `visualization_manifest.json` malformado (ej: quitar el campo `"trajectories_preview_csv"`)
  2. Abrirlo con File → Open en la UI
  3. La UI carga parcialmente: algunos tabs vacíos, botones de exportación en estado incorrecto, posibles crashes al hacer clic en "Export All"
- **Fix propuesto:**
```cpp
// Al inicio de loadManifest(), después de leer el texto:
QJsonParseError parse_error;
QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parse_error);
if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
    QMessageBox::warning(this, "Invalid manifest", 
        "The manifest file is not valid JSON");
    return;
}
// Usar doc.object().value("trajectories_preview_csv").toString() en vez de regex
```
- **Riesgo de regresión:** Medio (hay que verificar que todos los campos del manifest existen en los outputs reales)
- **Tiempo estimado:** 2.5 horas

### [F1-04] `push.txt` contiene credenciales hardcodeadas
- **Archivo:** `push.txt:1`
- **Severidad:** 🔴 CRÍTICO
- **Audit ref:** [NO EN AUDIT]
- **Estado:** **Verificado**
- **Descripción:** El archivo contiene `git push https://TU_TOKEN@github.com/kegouro/BeamLabStudio.git main`. Si `TU_TOKEN` es un token real de GitHub, cualquiera que clone el repo puede hacer push como el autor. Si es placeholder, de todas formas es mala práctica.
- **Reproducción:** `cat push.txt` — la credencial está en texto plano.
- **Fix propuesto:**
  1. Si es un token real: revocarlo inmediatamente en GitHub Settings → Developer settings → Personal access tokens
  2. Borrar `push.txt`
  3. Agregar `push.txt` a `.gitignore`
  4. Usar `git remote set-url origin` con credenciales via SSH o Git credential manager
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 0.25 horas

### [F1-05] Memoria no acotada en `analysis_log_`
- **Archivo:** `src/ui/qt/MainWindow.cpp:4343`
- **Severidad:** 🟡 MEDIO
- **Audit ref:** §7.5
- **Estado:** **Verificado**
- **Descripción:** `analysis_log_->appendPlainText(chunk)` se llama por cada chunk de output del proceso de análisis, sin límite de líneas. En análisis muy largos (datasets de millones de trayectorias), esto puede consumir cientos de MB de RAM y hacer la UI no responsiva.
- **Reproducción:** Correr un análisis muy largo con verbose output; la UI se vuelve lenta.
- **Fix propuesto:**
```cpp
// En el slot readyReadStandardOutput:
constexpr int MAX_LOG_LINES = 10000;
if (analysis_log_->document()->blockCount() > MAX_LOG_LINES) {
    QTextCursor cursor = analysis_log_->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 
                        analysis_log_->document()->blockCount() - MAX_LOG_LINES);
    cursor.removeSelectedText();
}
analysis_log_->appendPlainText(chunk);
```
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 0.5 horas

### [F1-06] `$@` sin validación en script `.cmd` de Windows
- **Archivo:** `scripts/run_beamlab_full.cmd:4`
- **Severidad:** 🟡 MEDIO
- **Audit ref:** [NO EN AUDIT]
- **Estado:** **Verificado**
- **Descripción:** El script `.cmd` hace `python "%~dp0run_beamlab_full.py" %*`. En Windows, `%*` expande todos los argumentos sin escapar caracteres especiales. Un nombre de archivo con `&` o `|` causaría ejecución de comandos arbitrarios.
- **Reproducción:** Crear un archivo llamado `test&calc.exe.csv`, seleccionarlo en la UI → se ejecuta `calc.exe` en Windows.
- **Fix propuesto:** Reemplazar `%*` con `%1 %2 %3 %4 %5 %6 %7 %8 %9` o usar `setlocal enabledelayedexpansion` con escaping. Idealmente, migrar el script `.cmd` a PowerShell con `Start-Process -ArgumentList`.
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 0.5 horas

---

## Fase 2: Corrección Física

Fixes que afectan la correctitud de resultados científicos. Cada fix incluye validación numérica.

### [F2-01] Bethe-Bloch retorna 0 para partículas detenidas (físicamente incorrecto)
- **Archivo:** `src/simulation/physics/BetheBlochEngine.cpp:57-58`
- **Severidad:** 🟠 ALTO
- **Audit ref:** §3.12
- **Estado:** **Verificado** (en capa vieja `src/simulation/`). La capa nueva `src/biosim/` NO tiene este problema (usa `beta2 <= 1.0e-10` como cota inferior).
- **Descripción:** Cuando `kinE_MeV == 0`, `gamma = 1`, `beta2 = 0`, la función retorna `0.0`. Físicamente, una partícula detenida **deposita toda su energía restante en el medio** (pico de Bragg). Retornar 0 MeV/cm para el último paso subestima la dosis total y aplana artificialmente el pico de Bragg.
- **Reproducción:**
  1. Simular un muón de 100 MeV atravesando 50 cm de agua
  2. Observar que la energía depositada en el último step (cuando kinE→0) es 0
  3. El perfil de Bragg tendrá un "hueco" artificial en el pico
- **Fix propuesto:**
```cpp
// En betheBlochMass():
if (beta2 <= 0.0 || beta2 >= 1.0) {
    // Partícula detenida: depositar energía restante como pico de Bragg
    return std::numeric_limits<double>::infinity();  
    // El caller (energyLoss_MeV) ya tiene la guarda:
    // return std::min(loss, kinE_MeV);
}
```
O alternativamente:
```cpp
if (beta2 <= 0.0) {
    // Para kinE ≈ 0, usar el límite de baja energía: 
    // dE/dx ∝ 1/β² → infinito. El caller clampa a kinE_MeV restante.
    return 1.0e6; // valor grande pero finito para evitar NaN en logs
}
```
**Validación:** Comparar perfil de Bragg con datos de referencia NIST PSTAR para muones en agua. El pico debe ser agudo, no truncado.
- **Riesgo de regresión:** Medio (cambia resultados de dosis en la cola del perfil)
- **Tiempo estimado:** 2.0 horas

### [F2-02] `FocusDetector` asigna confidence=0.1 injustificado en bordes
- **Archivo:** `src/analysis/focus/FocusDetector.cpp:76`
- **Severidad:** 🟡 MEDIO
- **Audit ref:** §3.9
- **Estado:** **Verificado**
- **Descripción:** Si el foco detectado está en el primer o último bin del rango axial (`best_index == 0` o `best_index + 1 == curve.points.size()`), se asigna `confidence = 0.1` sin justificación. El valor 0.1 es un "magic number" que no deriva de ninguna consideración física. Si el mínimo real está genuinamente en el borde (ej: haz que converge justo al salir del volumen), el resultado se marca incorrectamente como baja confianza.
- **Reproducción:**
  1. Usar el dataset sintético `TC02_tight_focus` (foco muy cercano al borde)
  2. Verificar que el `FocusResult.confidence` es 0.1 aunque el foco es físicamente real
  3. El reporte de calidad marcará "enfoque detectado pero baja confianza" incorrectamente
- **Fix propuesto:**
```cpp
// Reemplazar el else de la línea 75-77:
} else {
    // Foco en borde: confianza basada en la pendiente unilateral
    if (best_index == 0 && curve.points.size() >= 2) {
        const double slope = curve.points[1].metric_value - curve.points[0].metric_value;
        result.confidence = slope > 0.0 ? slope : 0.0;
    } else if (best_index + 1 == curve.points.size() && curve.points.size() >= 2) {
        const double slope = curve.points[curve.points.size() - 2].metric_value 
                           - curve.points[curve.points.size() - 1].metric_value;
        result.confidence = slope > 0.0 ? slope : 0.0;
    } else {
        result.confidence = 0.0;
    }
}
```
- **Riesgo de regresión:** Bajo (solo afecta el valor de confianza, no la posición del foco)
- **Tiempo estimado:** 1.0 horas

### [F2-03] `BetheBlochEngine` sin corrección de densidad ni shell
- **Archivo:** `src/simulation/physics/BetheBlochEngine.cpp:72`
- **Severidad:** 🟠 ALTO
- **Audit ref:** §9.1
- **Estado:** **Verificado** (en capa vieja). La capa biosim YA tiene Sternheimer.
- **Descripción:** El comentario en línea 72 dice `// Bethe-Bloch core (no density or shell corrections for simplicity)`. Para tejido biológico a energías < 10 MeV, la corrección de densidad de Sternheimer reduce el stopping power en ~5-15%. La corrección de shell (Barkas/Bloch) agrega ~1-2%. Sin estas correcciones, el rango calculado del muón es más corto que el real, y la dosis en profundidad se sobreestima.
- **Reproducción:**
  1. Comparar `dEdx_MeV_cm` del viejo `BetheBlochEngine` vs el nuevo `StoppingPowerEngine` para un muón de 5 MeV en agua
  2. La diferencia será de ~8-12%
  3. El rango CSDA calculado por el viejo engine será menor que el del biosim
- **Fix propuesto:**
  **Opción A (recomendada):** Deprecar `src/simulation/physics/BetheBlochEngine.cpp` y redirigir `SimulatorWidget` a usar `src/biosim/physics/StoppingPowerEngine`, que ya tiene Sternheimer, MCS, y Vavilov straggling.
  **Opción B (mínima):** Agregar `sternheimerDelta()` al viejo engine.
- **Riesgo de regresión:** Alto con Opción A (requiere migración de SimulatorWidget). Bajo con Opción B.
- **Tiempo estimado:** Opción A: 4.0 horas. Opción B: 1.5 horas.

### [F2-04] Aproximación Gaussiana para straggling documentada pero no advertida en outputs
- **Archivo:** `src/simulation/physics/MuonTrackSimulator.cpp` (y `EnergyStraggling.cpp` en biosim)
- **Severidad:** 🟡 MEDIO
- **Audit ref:** §9.2
- **Estado:** **Verificado** (en capa vieja usa Gaussiana). La capa biosim YA tiene Vavilov.
- **Descripción:** El README admite "Full Landau straggling (currently Gaussian approximation)". El problema es que los **outputs** (CSV, reportes) no incluyen ninguna advertencia sobre esta limitación. Un investigador podría tomar los datos de dosis como clínicamente precisos.
- **Reproducción:**
  1. Generar un `energy_step_profile.csv` con la capa vieja
  2. Abrir el CSV: no hay columna ni metadato que indique "straggling_model=gaussian"
  3. Comparar con biosim (Vavilov): la cola de alta energía es significativamente diferente
- **Fix propuesto:**
  1. Agregar columna `straggling_model` al CSV de salida con valor `"gaussian"` o `"vavilov"`
  2. Agregar un warning en `QualityReportExporter` cuando se usa el modelo gaussiano: "Straggling model: Gaussian approximation. For clinical dosimetry, Vavilov/Landau straggling is recommended."
  3. Asegurar que el `ExportResult` incluya metadatos sobre el modelo de straggling usado
- **Riesgo de regresión:** Bajo (solo añade metadatos)
- **Tiempo estimado:** 1.5 horas

### [F2-05] `SurfaceBuilder` — orientación de caras potencialmente inconsistente
- **Archivo:** `src/analysis/surfaces/SurfaceBuilder.cpp:95-96`
- **Severidad:** 🟡 MEDIO
- **Audit ref:** §3.10
- **Estado:** **Verificado** (el código coincide con lo reportado)
- **Descripción:** Las caras se generan como `{a, c, b}` y `{b, c, d}`. En una tira de triángulos con vértices `a,b` en anillo actual y `c,d` en anillo siguiente, la orientación de normales puede invertirse entre triángulos adyacentes dependiendo del orden de los vértices del contorno. Esto produce mallas donde algunas caras tienen normales invertidas (backface culling inconsistente en visualizadores 3D).
- **Reproducción:**
  1. Generar una malla con `SurfaceBuilder` usando datos de un haz divergente
  2. Abrir el OBJ en ParaView o Blender con "Backface Culling" activado
  3. Observar "agujeros" donde los triángulos están orientados al revés
- **Fix propuesto:**
```cpp
// El estándar para triangle strips es {a, b, c}, {b, d, c}:
surface.mesh.faces.push_back({a, b, c});  // consistente
surface.mesh.faces.push_back({b, d, c});  // consistente
```
**Validación:** Generar malla con ambas versiones y verificar en ParaView con filtro "Generate Surface Normals" → "Orientate". La versión corregida debe tener todas las normales apuntando hacia afuera.
- **Riesgo de regresión:** Medio (cambia archivos OBJ generados, requiere regenerar tests de referencia)
- **Tiempo estimado:** 1.5 horas

---

## Fase 3: Refactorización Arquitectónica

Separación de responsabilidades sin cambiar comportamiento externo. El proyecto debe compilar igual después de cada paso.

### [F3-01] Extraer `buildFullEnvelopePreviewObj` de MainWindow a `analysis/envelope/`
- **Archivos afectados:**
  - Origen: `src/ui/qt/MainWindow.cpp:3301-3700` (~400 líneas)
  - Destino: nuevo `src/analysis/envelope/FullEnvelopePreviewBuilder.cpp/.h`
- **Severidad:** 🟡 MEDIO
- **Audit ref:** §1.1, §1.2
- **Estado:** **Verificado** — Código de análisis físico en capa UI
- **Descripción:** `buildFullEnvelopePreviewObj()` contiene binning angular (128 sectores), cálculo de quantiles (percentil 90), generación de vértices OBJ, y escritura a disco. Todo esto es lógica de análisis, no de UI. Está en MainWindow solo porque fue el camino más rápido durante el desarrollo.
- **Fix propuesto:**
  1. Crear `src/analysis/envelope/FullEnvelopePreviewBuilder.h/.cpp`
  2. Mover toda la lógica de binning, quantiles y generación de geometría
  3. Exponer método `static bool buildPreview(const std::string& csv_path, const std::string& output_obj, std::string* error)`
  4. MainWindow llama a `FullEnvelopePreviewBuilder::buildPreview(...)`
  5. Agregar al `CMakeLists.txt` en `beamlab_core`
- **Riesgo de regresión:** Bajo (refactor puro, misma interfaz)
- **Tiempo estimado:** 3.0 horas

### [F3-02] Crear `ExportManager` para consolidar toda la lógica de exportación
- **Archivos afectados:**
  - Origen: `src/ui/qt/MainWindow.cpp` (métodos `exportRunAssets`, `exportPlotPngsTo`, `exportStatisticsPdfTo`, `exportTrajectoryVideoTo`)
  - Destino: nuevo `src/io/exporters/ExportManager.cpp/.h`
- **Severidad:** 🟡 MEDIO
- **Audit ref:** §1.1
- **Estado:** **Verificado** — Lógica de exportación dispersa en UI
- **Descripción:** MainWindow contiene ~500 líneas de lógica de exportación (copiar directorios, lanzar ffmpeg, capturar frames). Esto debería estar en la capa IO.
- **Fix propuesto:**
  1. Crear `ExportManager` con métodos:
     - `bool exportAssets(const QString& runDir, const QString& outDir, QStringList* messages)`
     - `bool exportPlots(const QString& runDir, const QString& outDir, QStringList* messages)`
     - `bool exportPdf(const QString& manifestPath, const QString& outPath, QStringList* messages)`
     - `bool exportMp4(QWidget* sceneWidget, const QString& outPath, QStringList* messages)`
  2. Mover toda la lógica
  3. MainWindow solo invoca `ExportManager` y muestra resultados
- **Riesgo de regresión:** Medio (tocar ffmpeg y captura de frames)
- **Tiempo estimado:** 6.0 horas

### [F3-03] Unificar los dos subsistemas de simulación biológica
- **Archivos afectados:**
  - `src/simulation/` (BetheBlochEngine, MuonTrackSimulator, TissueRegistry, ScoringPlaneDetector)
  - `src/biosim/` (StoppingPowerEngine, BraggPeakCalculator, BioSimRunner, etc.)
- **Severidad:** 🟠 ALTO
- **Audit ref:** [NO EN AUDIT]
- **Estado:** **Verificado** — Dos sistemas con solapamiento significativo
- **Descripción:** El proyecto tiene dos capas de simulación biológica:
  - **Vieja** (`src/simulation/`): 4 archivos, Bethe-Bloch sin correcciones, straggling Gaussiano
  - **Nueva** (`src/biosim/`): 18+ archivos, Bethe-Bloch con Sternheimer, MCS de Highland, straggling Vavilov, Bortfeld Bragg peak, 55+ materiales, 15+ phantoms
  - Ambas contienen `TissueRegistry`/`BioMaterialLibrary` (materiales duplicados), `BetheBlochEngine`/`StoppingPowerEngine` (lógica duplicada)
  - `SimulatorWidget` (UI) usa la capa VIEJA. `BioSimWidget` usa la capa NUEVA.
- **Fix propuesto (en 3 pasos):**
  1. **Paso 1:** Hacer que `SimulatorWidget` use `StoppingPowerEngine` de biosim en vez de `BetheBlochEngine` viejo. Verificar que los resultados numéricos son consistentes (habrá diferencias por Sternheimer, documentarlas).
  2. **Paso 2:** Migrar `TissueRegistry` → `BioMaterialLibrary`. Eliminar `TissueRegistry`.
  3. **Paso 3:** Migrar `ScoringPlaneDetector` a biosim. Eliminar `ScoringPlaneDetector` viejo.
  4. **Paso 4:** Eliminar `src/simulation/` del todo (solo quedaba `MuonTrackSimulator` que también está en biosim).
- **Riesgo de regresión:** Alto (cambia resultados numéricos por el modelo físico mejorado)
- **Tiempo estimado:** 8.0 horas (2h por paso)

### [F3-04] Agregar tests unitarios con GoogleTest
- **Archivos afectados:** Nuevo `tests/unit/` y `CMakeLists.txt`
- **Severidad:** 🟡 MEDIO
- **Audit ref:** §10 (recomendación inmediata #5)
- **Estado:** Solo existen tests de integración manuales (`tests/synthetic_dataset/`)
- **Descripción:** No hay tests unitarios automatizados. Cada cambio requiere verificación manual abriendo la UI y cargando datasets sintéticos.
- **Fix propuesto:**
  1. Agregar `FetchContent` para GoogleTest en CMakeLists.txt
  2. Crear tests mínimos para:
     - `FrameStatisticsEngine::compute()` con datos sintéticos conocidos
     - `FocusDetector::detect()` con curva de foco predefinida
     - `Geant4CsvImporter::import()` con CSV de prueba
     - `StoppingPowerEngine::dEdx_MeV_cm()` contra valores NIST PSTAR
     - `SurfaceBuilder::build()` verificando conteo de vértices/caras
     - `DelimiterDetector::detect()` con delimitadores conocidos
  3. Crear `tests/unit/data/` con fixtures pequeños (no los datasets sintéticos grandes)
  4. Agregar target `beamlab_tests` y `ctest` integration
- **Riesgo de regresión:** Bajo (solo añade código de test)
- **Tiempo estimado:** 6.0 horas

### [F3-05] Restringir visibilidad de includes con `target_include_directories`
- **Archivo:** `CMakeLists.txt:27,75`
- **Severidad:** 🟡 MEDIO
- **Audit ref:** §1.3
- **Estado:** **Verificado**
- **Descripción:** `include_directories(${PROJECT_SOURCE_DIR}/src)` hace que TODO el código vea TODOS los headers. Esto permite acoplamiento accidental (ej: UI incluyendo headers de análisis internos).
- **Fix propuesto:**
```cmake
# Eliminar línea 27: include_directories(${PROJECT_SOURCE_DIR}/src)
# Reemplazar línea 75 con:
target_include_directories(beamlab_core PUBLIC 
    ${PROJECT_SOURCE_DIR}/src
)
# Y restringir beamlab_ui a solo lo que necesita:
target_include_directories(beamlab_ui PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)
# Idealmente, separar en INTERFACE/PUBLIC/PRIVATE por subsistema
```
- **Riesgo de regresión:** Medio (pueden aparecer errores de compilación por includes faltantes)
- **Tiempo estimado:** 2.0 horas

---

## Fase 4: Hardening & Polish

Mejoras de calidad de vida, seguridad, performance y mantenibilidad que no afectan resultados científicos.

### [F4-01] Resolver conflicto de doble stylesheet
- **Archivos:** `src/ui/qt/main.cpp:67-177` (inline QSS), `src/ui/qt/styles/beamlabstudio.qss:1-484` (.qss), `src/ui/qt/MainWindow.cpp:606-960` (segundo inline QSS)
- **Severidad:** 🟢 BAJO
- **Audit ref:** §7.1
- **Estado:** **Verificado** — **Tres** fuentes de estilos que compiten
- **Descripción:** `main.cpp` define un stylesheet base (~110 líneas) y carga el `.qss` (~484 líneas). Luego `MainWindow` **sobrescribe** con otro stylesheet inline (~370 líneas). Muchas reglas se pisan entre sí (ej: colores de QPushButton definidos 3 veces con valores diferentes). El resultado visual depende del orden de carga, que es frágil.
- **Fix propuesto:**
  1. Consolidar **todo** en `beamlabstudio.qss` (única fuente de verdad)
  2. Eliminar stylesheet inline de `main.cpp` (líneas 67-177) — solo mantener `app.setStyleSheet(brand_style_content)`
  3. Eliminar `setStyleSheet(R"QSS(...)")` de `MainWindow.cpp:606` — mover reglas faltantes al `.qss`
  4. Usar `setProperty("class", "PrimaryButton")` en lugar de `setObjectName` para estilizar widgets
- **Riesgo de regresión:** Medio (la UI puede verse diferente tras consolidar)
- **Tiempo estimado:** 3.0 horas

### [F4-02] Agregar `Qt::AA_EnableHighDpiScaling` y `AA_UseHighDpiPixmaps`
- **Archivo:** `src/ui/qt/main.cpp:30`
- **Severidad:** 🟢 BAJO
- **Audit ref:** §7.3
- **Estado:** **Verificado** — No hay flags de high-DPI
- **Descripción:** En pantallas 4K/Retina, la UI se ve pixelada y pequeña.
- **Fix propuesto:**
```cpp
// Antes de QApplication app(argc, argv):
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
```
En Qt6 esto es default, pero ser explícito no duele.
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 0.25 horas

### [F4-03] Reemplazar `QTimer::singleShot(0/120, ...)` hacks
- **Archivo:** `src/ui/qt/MainWindow.cpp` (varios lugares, buscar `singleShot`)
- **Severidad:** 🟢 BAJO
- **Audit ref:** §7.4
- **Estado:** **Verificado** — Hacks de timing para forzar layout
- **Descripción:** `QTimer::singleShot(0, ...)` se usa como hack para ejecutar código "en el próximo ciclo de eventos" (cuando el layout ya está calculado). `QTimer::singleShot(120, ...)` es aún peor: asume que el layout tarda <120ms, lo cual es frágil.
- **Fix propuesto:**
```cpp
// Reemplazar singleShot(0, ...) con:
QMetaObject::invokeMethod(this, [this]() { fitSceneInView(...); }, Qt::QueuedConnection);

// Reemplazar singleShot(120, ...) con:
// Override resizeEvent() o showEvent() y llamar fitSceneInView() allí
```
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 1.5 horas

### [F4-04] Cachear proyección en `FrameStatisticsEngine`
- **Archivo:** `src/analysis/statistics/FrameStatisticsEngine.cpp:252,312`
- **Severidad:** 🟢 BAJO
- **Audit ref:** §4.1
- **Estado:** **Verificado** — `projectSamples()` se llama 2 veces
- **Descripción:** `computeUniformAxialBins()` y `computeEqualCountAxialBins()` cada uno llama a `projectSamples()`, que recorre TODOS los samples. Para datasets grandes, esto duplica el tiempo de cómputo.
- **Fix propuesto:**
```cpp
// En FrameStatisticsEngine::compute():
// Cachear la proyección si ambos modos la necesitan:
std::optional<std::vector<ProjectedSample>> cached_projection;
double cached_s_min, cached_s_max;
// ... pasar cached_projection a las funciones de binning
```
- **Riesgo de regresión:** Bajo (solo cambia estructura interna)
- **Tiempo estimado:** 2.0 horas

### [F4-05] Unificar funciones helper duplicadas (`dot`, `subtract`, `add`, `scale`)
- **Archivos:** `FrameStatisticsEngine.cpp:37-46`, `SurfaceBuilder.cpp:10-18`, posiblemente otros
- **Severidad:** 🟢 BAJO
- **Audit ref:** §6.2
- **Estado:** **Verificado** — Duplicación en namespaces anónimos
- **Descripción:** `dot()`, `subtract()`, `add()`, `scale()` están definidas en múltiples archivos. Deberían ser métodos de `Vec3` o estar en un header `math/VectorOps.h`.
- **Fix propuesto:**
  1. Agregar `static dot()`, `static subtract()`, etc. a `core/math/Vec3.h` como `static` methods
  2. O crear `core/math/VectorOps.h` con funciones inline
  3. Eliminar duplicados de los namespaces anónimos
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 1.0 horas

### [F4-06] Agregar `install()` targets al CMake
- **Archivo:** `CMakeLists.txt`
- **Severidad:** 🟢 BAJO
- **Audit ref:** §5.4
- **Estado:** **Verificado** — No hay install targets
- **Fix propuesto:**
```cmake
install(TARGETS beamlab beamlab_ui RUNTIME DESTINATION bin)
install(DIRECTORY scripts/ DESTINATION share/BeamLabStudio/scripts
    FILES_MATCHING PATTERN "*.sh" PATTERN "*.py" PATTERN "*.cmd")
```
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 0.5 horas

### [F4-07] Eliminar `push.txt` del repositorio y del historial
- **Archivo:** `push.txt`
- **Severidad:** 🔴 CRÍTICO (seguridad)
- **Audit ref:** [NO EN AUDIT]
- **Estado:** **Verificado**
- **Descripción:** Contiene credenciales o placeholder de token GitHub.
- **Fix propuesto:**
```bash
git rm push.txt
echo "push.txt" >> .gitignore
git add .gitignore
git commit -m "Remove push.txt with hardcoded credentials"
# Si el token es real, revocarlo en GitHub inmediatamente
```
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 0.25 horas

### [F4-08] Agregar GitHub Actions CI
- **Archivos:** Nuevo `.github/workflows/build.yml`
- **Severidad:** 🟢 BAJO
- **Audit ref:** §10 (recomendación #10)
- **Estado:** No existe CI
- **Fix propuesto:**
```yaml
name: Build & Test
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Install deps
        run: sudo apt install -y cmake ninja-build g++ qt6-base-dev qt6-charts-dev
      - name: Configure
        run: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON
      - name: Build
        run: cmake --build build -j$(nproc)
```
- **Riesgo de regresión:** Bajo
- **Tiempo estimado:** 1.5 horas

### [F4-09] Documentar limitaciones físicas en outputs
- **Archivos:** `QualityReportExporter.cpp`, `AnalysisReportExporter.cpp`
- **Severidad:** 🟡 MEDIO
- **Audit ref:** §10 (recomendación #14)
- **Estado:** No implementado
- **Descripción:** Los reportes generados no incluyen declaraciones de limitaciones del modelo físico. Un investigador que recibe un PDF de BeamLabStudio no sabe si los datos usan straggling Gaussiano o Vavilov, si hay corrección de densidad, etc.
- **Fix propuesto:**
  1. Agregar sección "Physics Model Limitations" al `analysis_summary.md` generado
  2. Incluir:
     - Modelo de straggling usado (Gaussian / Vavilov)
     - Correcciones aplicadas (Sternheimer: sí/no, shell: no)
     - Rango de validez (βγ > 0.1, E < ~100 GeV para muones)
     - Advertencia: "These results are for research purposes. Not validated for clinical use."
- **Riesgo de regresión:** Bajo (solo añade texto)
- **Tiempo estimado:** 1.0 horas

---

## Dependencias entre Fases

```
Fase 0 (Validación)
  │
  ├──▶ Fase 1 (Parada de Sangrado) ──────── Independiente, hacer primero
  │         │
  │         ├──▶ F1-01 (div/0) ────── Sin dependencias
  │         ├──▶ F1-02 (MP4 race) ─── Sin dependencias
  │         ├──▶ F1-03 (manifest) ─── Sin dependencias
  │         ├──▶ F1-04 (push.txt) ─── Sin dependencias  
  │         ├──▶ F1-05 (log mem) ──── Sin dependencias
  │         └──▶ F1-06 (cmd script) ─ Sin dependencias
  │
  ├──▶ Fase 2 (Corrección Física) ────────── Depende de F1 para tests limpios
  │         │
  │         ├──▶ F2-01 (BB stopped) ── Sin dependencias
  │         ├──▶ F2-02 (focus edge) ── Sin dependencias
  │         ├──▶ F2-03 (Sternheimer) ─ Depende de decisión: migrar a biosim o no
  │         ├──▶ F2-04 (straggling doc)── Sin dependencias
  │         └──▶ F2-05 (face winding)── Sin dependencias
  │
  ├──▶ Fase 3 (Refactorización) ──────────── Depende de F1+F2 completos
  │         │
  │         ├──▶ F3-01 (FullEnvelope) ─ Sin dependencias internas
  │         ├──▶ F3-02 (ExportManager)─ Puede depender de F3-01
  │         ├──▶ F3-03 (unificar sim) ─ BLOQUEA a F2-03 si se elige Opción A
  │         ├──▶ F3-04 (unit tests) ─── Depende de F2+F3-01/02 para tener código limpio que testear
  │         └──▶ F3-05 (includes) ──── Hacer después de F3-01/02/03
  │
  └──▶ Fase 4 (Hardening) ────────────────── Independiente de F2/F3
            │
            ├──▶ F4-01 (stylesheet) ── Sin dependencias
            ├──▶ F4-02 (high-DPI) ──── Sin dependencias
            ├──▶ F4-03 (QTimer hack) ─ Sin dependencias
            ├──▶ F4-04 (cache proj) ── Sin dependencias
            ├──▶ F4-05 (unify helpers)── Sin dependencias
            ├──▶ F4-06 (install) ───── Sin dependencias
            ├──▶ F4-07 (push.txt) ──── Hacer inmediatamente (seguridad)
            ├──▶ F4-08 (CI) ─────────── Depende de F3-04 (tests)
            └──▶ F4-09 (phys docs) ──── Depende de F2-04
```

**Regla de oro:** F1 se puede (y debe) hacer en paralelo con F4-07. F1-01, F1-04, F1-06, F4-07 son "quick wins" que toman <3 horas combinados.

---

## Estimación de Tiempo

| Fase | Items | Horas | Acumulado |
|------|-------|-------|-----------|
| **Fase 0:** Validación del Audit | (ya completada en este documento) | 0h | 0h |
| **Fase 1:** Parada de Sangrado | 6 fixes | **5.5h** | 5.5h |
| **Fase 2:** Corrección Física | 5 fixes | **9.5h** | 15.0h |
| **Fase 3:** Refactorización | 5 tareas | **25.0h** | 40.0h |
| **Fase 4:** Hardening & Polish | 9 tareas | **11.0h** | 51.0h |
| **TOTAL** | 25 tareas | **~51 horas** | — |

**Plan de ejecución sugerido (2 semanas a 4h/día):**
- Día 1-2: Fase 1 completa (quick wins, proyecto más estable)
- Día 3-5: Fase 2 (correcciones físicas, requiere validación cuidadosa)
- Día 6-10: Fase 3 (refactorización pesada, hacer de a una tarea por día)
- Día 11-13: Fase 4 (pulido final)
- Día 14: Buffer para imprevistos y documentación

---

## Herramientas Recomendadas

### Para Fase 1 (Debugging & Crash Fixes)
| Herramienta | Propósito | Instalación |
|-------------|-----------|-------------|
| **AddressSanitizer (ASan)** | Detectar use-after-free, buffer overflows | `cmake -DCMAKE_CXX_FLAGS="-fsanitize=address"` |
| **UndefinedBehaviorSanitizer (UBSan)** | Detectar div/0, null deref, integer overflow | `cmake -DCMAKE_CXX_FLAGS="-fsanitize=undefined"` |
| **Valgrind** | Memory leaks, uninitialized memory | `sudo dnf install valgrind` |
| **Qt Creator Debugger** | Step-through debugging de la UI | Ya incluido con Qt6 |

### Para Fase 2 (Validación Física)
| Herramienta | Propósito | Instalación |
|-------------|-----------|-------------|
| **NIST PSTAR** | Referencia de stopping power para muones | https://physics.nist.gov/PhysRefData/Star/Text/PSTAR.html |
| **Python + NumPy** | Scripts de validación numérica rápidos | `pip install numpy scipy matplotlib` |
| **ParaView** | Visualizar mallas OBJ, verificar normales | `sudo dnf install paraview` |
| **tests/physics_validation.py** | Script existente para validar outputs | Ya en el repo |

### Para Fase 3 (Refactorización & Tests)
| Herramienta | Propósito | Instalación |
|-------------|-----------|-------------|
| **GoogleTest** | Framework de tests unitarios C++ | `FetchContent` en CMake (sin instalación) |
| **gcov + lcov** | Cobertura de código | `sudo dnf install lcov` |
| **clang-tidy** | Linting C++ moderno | `sudo dnf install clang-tools-extra` |
| **include-what-you-use** | Detectar includes innecesarios | `sudo dnf install iwyu` |

### Para Fase 4 (Polish & CI)
| Herramienta | Propósito | Instalación |
|-------------|-----------|-------------|
| **clang-format** | Formateo consistente de código | `sudo dnf install clang-tools-extra` |
| **GitHub Actions** | CI/CD gratuito | Nada que instalar, ya está en GitHub |
| **Qt Designer** | Vista previa de stylesheets | `sudo dnf install qt6-qtbase-devel` |
| **cmake-format** | Formateo de CMakeLists.txt | `pip install cmake-format` |

### Comandos de build con sanitizers para debug:
```bash
# Build con AddressSanitizer + UndefinedBehaviorSanitizer
cmake -S . -B build-asan -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build-asan -j$(nproc)

# Ejecutar
./build-asan/beamlab_ui

# Con Valgrind (más lento pero detecta memory leaks sin recompilar)
valgrind --leak-check=full --track-origins=yes ./build/beamlab_ui
```

### Para el fix F4-04 (caché de proyección) — perfilado:
```bash
# Build con perf
cmake -S . -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON
cmake --build build-rel

# Perfilar el CLI (más fácil de medir que la UI)
perf record --call-graph dwarf ./build-rel/beamlab \
    --input tests/synthetic_dataset/TC01_normal_beam/data.csv \
    --output /tmp/perf_test

perf report
```

---

## Checklist de Verificación Post-Fase

### Después de Fase 1:
- [ ] El proyecto compila sin warnings nuevos
- [ ] La UI abre datasets sintéticos sin crash
- [ ] La exportación MP4 no se corrompe al cambiar de tab
- [ ] `push.txt` ya no existe en el repositorio
- [ ] El log de análisis no crece sin límite

### Después de Fase 2:
- [ ] Perfil de Bragg comparado contra NIST PSTAR (diferencia < 5%)
- [ ] `FocusDetector` asigna confidence > 0 para focos legítimos en bordes
- [ ] Las mallas OBJ se ven correctas en ParaView con backface culling
- [ ] El CSV de energía incluye metadatos de modelo de straggling

### Después de Fase 3:
- [ ] `beamlab_tests` pasa todos los tests (>80% coverage en análisis)
- [ ] `SimulatorWidget` y `BioSimWidget` producen resultados equivalentes
- [ ] `src/simulation/` ha sido eliminado o claramente marcado como deprecated
- [ ] Los includes por subsistema están restringidos

### Después de Fase 4:
- [ ] La UI se ve consistente en todas las plataformas (Linux + Windows)
- [ ] GitHub Actions CI pasa (build + tests)
- [ ] `cmake --install` funciona
- [ ] El reporte de análisis incluye sección de limitaciones físicas
- [ ] Alta DPI funciona correctamente

---

*Plan generado por verificación exhaustiva del audit contra el código real en `src/`, `scripts/`, `tests/`, y `CMakeLists.txt`. Cada claim del audit fue verificada línea por línea. Fecha: 2026-05-19.*
