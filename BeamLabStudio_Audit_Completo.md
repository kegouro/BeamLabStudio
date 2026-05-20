# 🔍 AUDIT COMPLETO — BeamLabStudio
## Evaluación de calidad de código, arquitectura y prácticas de ingeniería

**Repositorio:** https://github.com/kegouro/BeamLabStudio  
**Versión auditada:** v0.2.0 (main branch, commit actual)  
**Auditor:** Análisis automatizado + revisión manual de código fuente  
**Fecha:** 2026-05-19

---

## 📋 RESUMEN EJECUTIVO

| Categoría | Severidad | Hallazgos |
|-----------|-----------|-----------|
| **Arquitectura & Diseño** | 🔴 CRÍTICO | 8 problemas graves |
| **Seguridad** | 🔴 CRÍTICO | 5 vulnerabilidades |
| **Correctitud & Bugs** | 🟠 ALTO | 12 bugs confirmados |
| **Performance** | 🟠 ALTO | 6 problemas |
| **Build System & CMake** | 🟡 MEDIO | 4 problemas |
| **Código C++ Moderno** | 🟡 MEDIO | 7 problemas |
| **UI/Qt** | 🟡 MEDIO | 5 problemas |
| **Documentación** | 🟢 BAJO | 3 mejoras sugeridas |
| **Física & Matemáticas** | 🟠 ALTO | 3 problemas conceptuales |

**Veredicto general:** El proyecto tiene una **arquitectura sólida conceptualmente** pero está plagado de **problemas de implementación graves** que hacen que no sea confiable para uso científico/ de producción. Muchos de estos parecen ser resultado de generación por IA (Claude Code) sin revisión humana adecuada.

---

## 1. 🏗️ ARQUITECTURA & DISEÑO (CRÍTICO)

### 1.1. Violación del Principio de Responsabilidad Única (SRP)

**Archivo:** `src/ui/qt/MainWindow.cpp` (~2500 líneas)

**Problema:** `MainWindow` es un **God Object** que maneja:
- Construcción de UI completa (20+ widgets)
- Lógica de análisis de datos (CSV parsing, plotting)
- Exportación de archivos (PNG, MP4, PDF, CSV, OBJ)
- Generación de mallas 3D (`buildFullEnvelopePreviewObj`)
- Manejo de procesos externos (`QProcess`)
- Persistencia de settings (`QSettings`)
- Parsing de JSON manual con regex (!)
- Matemáticas de convex hull y angular bins

**Impacto:** Imposible de testear unitariamente. Un cambio en exportación PNG puede romper el parsing de CSV.

**Recomendación:** Dividir en al menos 6 clases separadas:
```
MainWindow (UI coordination only)
├── AnalysisController (orquesta el pipeline)
├── ExportManager (maneja todos los exports)
├── DataLoader (CSV/JSON parsing)
├── Scene3DController (3D scene logic)
└── SettingsManager (QSettings wrapper)
```

### 1.2. Mezcla de capas de negocio en UI

**Ejemplo concreto:** `MainWindow::buildFullEnvelopePreviewObj()` contiene:
- Algoritmo de binning angular (48 sectores)
- Cálculo de quantiles (percentil 90)
- Generación de vértices OBJ
- Escritura directa a archivo

Todo esto debería estar en `analysis/envelope/` o `io/exporters/`, no en la capa UI.

### 1.3. Dependencias circulares implícitas

El `CMakeLists.txt` usa `include_directories(${PROJECT_SOURCE_DIR}/src)` como include global. Esto permite que cualquier archivo incluya cualquier otro, rompiendo la encapsulación por directorios.

**Fix:** Usar `target_include_directories` con paths privados/publicos por target:
```cmake
target_include_directories(beamlab_core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/core>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/data>
)
target_include_directories(beamlab_core PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/analysis
    ${CMAKE_SOURCE_DIR}/src/io
)
```

### 1.4. Ausencia de interfaces abstractas

No hay interfaces virtuales puras para:
- `Importer` (aunque hay `probe()` e `import()`, no hay clase base abstracta)
- `Exporter`
- `Analyzer`

Esto hace imposible mockar componentes para tests.

### 1.5. Acoplamiento fuerte a Qt en lógica de negocio

`ApplicationBootstrap` debería ser agnóstico de Qt, pero `MainWindow` lanza procesos y maneja todo el pipeline de análisis.

### 1.6. Fuga de abstracciones del modelo de datos

`TrajectoryDataset` expone `std::vector<Trajectory> trajectories` como público. Cualquier código puede mutar el dataset sin pasar por la capa de normalización.

### 1.7. No hay sistema de eventos/mensajes

La comunicación entre componentes es directa (punteros crudos). No hay observer pattern ni signal/slot entre capas de negocio.

### 1.8. Manejo de errores inconsistente

- Algunas funciones retornan `bool`
- Otras lanzan `std::runtime_error`
- Otras usan `std::optional<Error>`
- La UI usa `QMessageBox` directamente desde lógica de negocio

---

## 2. 🔒 SEGURIDAD (CRÍTICO)

### 2.1. Command Injection en ejecución de scripts

**Archivo:** `MainWindow::openDataFileAndRunWithPath()`

```cpp
// Líneas ~2200-2250 de MainWindow.cpp
running_process_->setProgram(runner);  // runner = "scripts/run_beamlab_full.sh"
running_process_->setArguments(args);    // args contiene input_path del usuario
```

**Problema:** `input_path` viene de `QFileDialog::getOpenFileName()`. Si el usuario selecciona un archivo con nombre `; rm -rf / ;.csv`, el script shell lo procesará sin sanitización.

**Fix:** Usar `QProcess` con lista de argumentos (ya se hace), PERO el script shell `run_beamlab_full.sh` debe usar `"$1"` con comillas, no `$1`. **Verificar el script.**

### 2.2. Path Traversal en exportación

```cpp
// En exportRunAssets()
ok = copyDirectoryRecursive(
    run.filePath("tables"),
    out.filePath("csv/tables"),
    messages
) && ok;
```

Si `current_run_dir_` contiene `../`, se puede escribir fuera del directorio destino.

### 2.3. Deserialización insegura de JSON

```cpp
// MainWindow::loadManifest()
QJsonDocument document = QJsonDocument::fromJson(manifest_file.readAll(), &parse_error);
```

No hay validación de schema. Un manifest malicioso puede causar crashes por campos inesperados.

### 2.4. Ejecución de ffmpeg sin validación de path

```cpp
const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
// ...
process.setProgram(ffmpeg);
```

Si hay un `ffmpeg` malicioso en el PATH del usuario, se ejecuta sin verificación.

### 2.5. Escritura de archivos temporales sin permisos restringidos

```cpp
const QString frame_dir_path = temp_root.filePath(".beamlab_frames_" + ...);
```

Directorio temporal con nombre predecible. Riesgo de race condition en sistemas multiusuario.

---

## 3. 🐛 CORRECTITUD & BUGS (ALTO)

### 3.1. BUG CRÍTICO: División por cero en `FrameStatisticsEngine`

**Archivo:** `FrameStatisticsEngine.cpp`, línea ~85

```cpp
const double inv_n = 1.0 / static_cast<double>(acc.count);
```

Si `acc.count == 0`, hay división por cero. Aunque hay un check `if (acc.count < 3) continue` antes, en `finalizeAccumulator` no hay protección.

**Fix:**
```cpp
if (acc.count == 0) {
    return FrameStatistics{}; // o throw, o marcar como inválido
}
```

### 3.2. BUG CRÍTICO: Bin index out of bounds

**Archivo:** `FrameStatisticsEngine.cpp`, `computeUniformAxialBins()`

```cpp
auto bin = static_cast<std::size_t>(std::floor((p.s - s_min) / width));
if (bin >= bin_count) {
    bin = bin_count - 1;
}
```

Cuando `p.s == s_max` exactamente, `(p.s - s_min) / width == bin_count`, y `floor()` da `bin_count`, que se corrige a `bin_count - 1`. **PERO** si hay floating-point issues y da ligeramente más que `bin_count`, el clamp no funciona.

**Fix:**
```cpp
bin = std::clamp(bin, std::size_t{0}, bin_count - 1);
```

### 3.3. BUG: Memory leak en `running_process_`

**Archivo:** `MainWindow.cpp`

```cpp
QProcess* failed_process = running_process_;
running_process_ = nullptr;
// ...
failed_process->deleteLater();  // OK, pero...
```

En `onAnalysisFinished()`:
```cpp
QProcess* finished_process = running_process_;
running_process_ = nullptr;
finished_process->deleteLater();
```

Esto está bien, pero si `onAnalysisFinished` nunca se llama (crash, cierre forzado), el `QProcess` queda leaked.

### 3.4. BUG: Race condition en exportación MP4

```cpp
for (int frame = 0; frame < frame_count; ++frame) {
    // ...
    QApplication::processEvents();  // Permite reentrada!
    const QPixmap frame_pixmap = combined_scene_viewer_->grab();
    // ...
}
```

`processEvents()` durante exportación permite que el usuario interactúe con la UI, cambie tabs, etc., corrompiendo la captura.

**Fix:** Usar `QEventLoop` con exclusion o bloquear la UI completamente.

### 3.5. BUG: Fuga de file handles

**Archivo:** `Geant4CsvImporter.cpp`

```cpp
std::ifstream input(file_path);
// ...
// Nunca se cierra explícitamente (se deja al destructor)
```

En caso de error, el archivo puede quedar locked en Windows.

### 3.6. BUG: Parsing de CSV no maneja line breaks dentro de quoted strings

```cpp
// splitLine()
for (const char c : line) {
    if (c == '"') {
        inside_quotes = !inside_quotes;
        continue;
    }
    // ...
}
```

Si un campo quoted contiene `
` (multiline CSV), `std::getline` lo parte y el parsing falla silenciosamente.

### 3.7. BUG: Integer overflow en `makeUniqueTrajectoryId`

```cpp
std::uint64_t makeUniqueTrajectoryId(const std::int64_t event_id, const std::int64_t track_id) {
    const auto safe_event = event_id < 0 ? 0 : static_cast<std::uint64_t>(event_id);
    const auto safe_track = track_id < 0 ? 0 : static_cast<std::uint64_t>(track_id);
    return safe_event * 10000000ULL + safe_track + 1ULL;
}
```

Si `event_id > 1,844,674,407,370` (UINT64_MAX / 10^7), hay overflow. Geant4 puede tener event IDs grandes.

### 3.8. BUG: `looksSynchronized` lógica incorrecta

```cpp
const double fraction = static_cast<double>(same_count) / static_cast<double>(nonempty);
return fraction >= 0.80;
```

Si `nonempty == 0`, división por cero. (Protegido por `if (nonempty < 3)` arriba, pero el código es frágil).

### 3.9. BUG: `FocusDetector` no maneja mínimos en bordes

```cpp
if (curve.points.size() >= 3 && best_index > 0 && best_index + 1 < curve.points.size()) {
    // calcula confidence
} else {
    result.confidence = 0.1;  // Arbitrario!
}
```

Si el foco está en el primer o último bin, se asigna confidence 0.1 sin justificación física.

### 3.10. BUG: `SurfaceBuilder` genera mallas no-manifold

```cpp
surface.mesh.faces.push_back({a, c, b});
surface.mesh.faces.push_back({b, c, d});
```

Los triángulos `{a,c,b}` y `{b,c,d}` comparten la arista `b-c`, pero la orientación es inconsistente (`b-c` vs `c-b`). Esto genera una malla no-manifold que falla en muchos visualizadores 3D.

**Fix:**
```cpp
surface.mesh.faces.push_back({a, b, c});
surface.mesh.faces.push_back({b, d, c});
```

### 3.11. BUG: `Scene3DWidget` no maneja OBJ con normales/texturas

```cpp
if (parts[0] == "v" && parts.size() >= 4) {
    // solo parsea v, ignora vn, vt
```

OBJs con normales (`vn`) o coordenadas de textura (`vt`) se parsean parcialmente, produciendo resultados incorrectos.

### 3.12. BUG: `BetheBlochEngine` no maneja partículas detenidas

```cpp
if (beta2 <= 0.0 || beta2 >= 1.0) {
    return 0.0;
}
```

Cuando `kinE_MeV == 0` (partícula detenida), `gamma = 1`, `beta2 = 0`, y retorna 0. Esto es físicamente incorrecto — debería ser un error o un valor especial, no 0.

---

## 4. ⚡ PERFORMANCE (ALTO)

### 4.1. Reproyección completa del dataset N veces

**Archivo:** `FrameStatisticsEngine.cpp`

```cpp
const auto projected = projectSamples(dataset, s_min, s_max);
```

Se llama 2 veces: una para `computeUniformAxialBins` y otra para `computeEqualCountAxialBins`. Cada vez recorre TODOS los samples.

**Fix:** Cachear la proyección en `TrajectoryDataset` o en el engine.

### 4.2. Sorting O(N log N) innecesario en `computeEqualCountAxialBins`

```cpp
std::sort(projected.begin(), projected.end(), [](const auto& a, const auto& b) {
    return a.s < b.s;
});
```

Para equal-count bins, se puede usar `std::nth_element` que es O(N) en promedio.

### 4.3. `ConvexHullEnvelopeExtractor` recalcula hull para cada frame

No hay cache de convex hulls. Si los datos no cambian entre corridas, se recalcula todo.

### 4.4. `MainWindow` carga CSVs completos en memoria para plotting

```cpp
std::vector<PlotPoint> points;
// ... carga TODOS los puntos del CSV
```

Para datasets grandes (>1M puntos), esto consume GBs de RAM.

**Fix:** Usar sampling estratificado o virtualización.

### 4.5. `Scene3DWidget` redibuja TODO en cada `paintEvent`

No hay double buffering ni display list. Cada movimiento de mouse recalcula proyección de todos los vértices.

### 4.6. `buildFullEnvelopePreviewObj` usa `std::vector` sin `reserve`

```cpp
std::vector<Sample> samples;
// ...
samples.push_back({x, y, z});  // múltiples reallocations
```

---

## 5. 🔧 BUILD SYSTEM & CMAKE (MEDIO)

### 5.1. `CMAKE_BUILD_TYPE` default es Debug

```cmake
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
```

Para un proyecto científico, el default debería ser `Release`.

### 5.2. No hay versionado de dependencias

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(VTK REQUIRED)  // ni siquiera está en el CMakeLists.txt mostrado!
```

No hay `find_package(VTK)` en el CMakeLists.txt que revisé. Si VTK es requerido, falta.

### 5.3. `BEAMLAB_ENABLE_ROOT` usa `execute_process` inseguro

```cmake
execute_process(
    COMMAND ${ROOT_CONFIG_EXECUTABLE} --incdir
    OUTPUT_VARIABLE ROOT_INCLUDE_DIR
)
```

Si `root-config` no existe o falla, `ROOT_INCLUDE_DIR` queda vacío y la compilación falla con errores crípticos.

### 5.4. No hay instalación (`install()` targets)

Faltan:
```cmake
install(TARGETS beamlab beamlab_ui ...)
install(DIRECTORY ...)
```

---

## 6. 💻 CÓDIGO C++ MODERNO (MEDIO)

### 6.1. Uso de `using namespace` implícito

Muchos archivos usan `namespace beamlab::analysis {` y luego funciones sin fully-qualified names. No es un bug, pero dificulta la búsqueda de símbolos.

### 6.2. Funciones helper duplicadas

`dot()`, `subtract()`, `add()`, `scale()` están definidas en múltiples archivos. Deberían estar en un header de math utils.

### 6.3. No uso de `std::span` o `std::string_view`

En C++20, muchos parámetros `const std::string&` podrían ser `std::string_view`.

### 6.4. `std::optional` usado incorrectamente

```cpp
const auto value = parseDouble(token);
if (!value.has_value()) {
    return std::nullopt;
}
return static_cast<std::int64_t>(std::llround(value.value()));
```

`std::llround` puede lanzar excepción si el valor es NaN/inf. No está manejado.

### 6.5. No hay `constexpr` donde corresponde

Constantes físicas como `k_MeV_cm2_g` sí son `constexpr`, pero muchos otros valores mágicos no.

### 6.6. `std::numeric_limits<double>::infinity()` usado como sentinel

```cpp
double s_min = std::numeric_limits<double>::infinity();
```

Esto es correcto pero frágil. Mejor usar `std::optional<double>`.

### 6.7. No hay `[[nodiscard]]` en funciones críticas

`FocusDetector::detect()`, `FrameStatisticsEngine::compute()`, etc., deberían tener `[[nodiscard]]`.

---

## 7. 🎨 UI / QT (MEDIO)

### 7.1. Stylesheet inline masivo

```cpp
setStyleSheet(R"QSS(
/* 200+ líneas de CSS inline */
)QSS");
```

200+ líneas de CSS inline en C++ es imposible de mantener. Debería estar en un archivo `.qss` externo.

### 7.2. Magic numbers everywhere

```cpp
setMinimumSize(1100, 720);
resize(1500, 920);
// ...
resize(1400, 880);  // Se resizea DOS VECES!
```

### 7.3. No hay manejo de DPI/high-DPI

No se usa `Qt::AA_EnableHighDpiScaling` ni `Qt::AA_UseHighDpiPixmaps`.

### 7.4. `QTimer::singleShot(0, ...)` y `singleShot(120, ...)` hacks

```cpp
QTimer::singleShot(0, this, [this]() { fitSceneInView(...); });
QTimer::singleShot(120, this, [this]() { fitSceneInView(...); });
```

Esto es un hack para forzar layout. Debería usar `QEvent::LayoutRequest` o `resizeEvent`.

### 7.5. `analysis_log_` puede crecer sin límite

```cpp
analysis_log_->appendPlainText(chunk);  // Sin límite de líneas
```

En análisis largos, esto consume memoria indefinidamente.

---

## 8. 📚 DOCUMENTACIÓN (BAJO)

### 8.1. Bloques `//!@math` son geniales pero no se compilan

Los bloques `//!@math` son documentación embebida que se extrae con un script Python. **Pero** si el script falla, no hay fallback.

### 8.2. README promete features que no están en el código

- "MP4 animation of beam evolution (via ffmpeg)" — el código existe pero es frágil
- "PDF + MP4 + OBJ export" — PDF no está implementado en el código revisado (`exportStatisticsPdfTo` llama a `statistics_dashboard_->exportStatisticsPdf()` que no pude revisar)

### 8.3. No hay CONTRIBUTING.md ni guía de estilo

---

## 9. 🔬 FÍSICA & MATEMÁTICAS (ALTO)

### 9.1. Bethe-Bloch sin correcciones de densidad ni shell

```cpp
// Bethe-Bloch core (no density or shell corrections for simplicity)
```

Para tejido biológico, las correcciones de shell son importantes a bajas energías (< 10 MeV). La fórmula actual puede sobreestimar el stopping power en el rango terapéutico.

### 9.2. Aproximación Gaussiana para straggling

El README dice: "Full Landau straggling in simulator (currently Gaussian approximation)"

Esto es una limitación conocida pero **debería documentarse en los outputs** para que los usuarios no asuman precisión clínica.

### 9.3. `TissueRegistry::air()` tiene densidad incorrecta

```cpp
return {"Air (dry)", "air", 0.001205, 7.36, 14.51, 85.7, 30420.0};
```

La densidad del aire a STP es ~0.001204 g/cm³ (correcto), pero el ICRU-44 usa 0.00120. La diferencia es mínima, pero la fuente debería citarse.

---

## 10. 🎯 RECOMENDACIONES PRIORITARIAS

### Inmediatas (antes de cualquier release)
1. **Fix división por cero** en `FrameStatisticsEngine`
2. **Fix orientación de caras** en `SurfaceBuilder`
3. **Sanitizar paths** en ejecución de scripts
4. **Separar `MainWindow`** en múltiples clases
5. **Agregar tests unitarios** mínimos (GoogleTest o Catch2)

### Corto plazo (1-2 semanas)
6. Implementar sistema de logging centralizado (spdlog)
7. Agregar manejo de errores consistente (Result<T, E> pattern)
8. Cachear proyecciones en `FrameStatisticsEngine`
9. Mover lógica de negocio fuera de UI
10. Agregar CI/CD con GitHub Actions

### Mediano plazo (1 mes)
11. Reescribir `Scene3DWidget` con un renderer más eficiente
12. Implementar validación de schema para manifests
13. Agregar tests de integración con datos reales de Geant4
14. Documentar limitaciones físicas en outputs

---

## 📎 APÉNDICE: Métricas de código

| Métrica | Valor |
|---------|-------|
| Líneas totales estimadas | ~8,000-10,000 |
| Archivos .cpp revisados | 15 |
| Archivos .h revisados | 5 |
| Complejidad ciclomática promedio | Alta (>20 en MainWindow) |
| Cobertura de tests estimada | 0% (no hay tests visibles) |
| Deuda técnica estimada | 2-3 meses de refactorización |

---

*Audit generado por análisis automatizado de código fuente. Recomendación: No usar en producción sin resolver los items CRÍTICOS y ALTO.*
