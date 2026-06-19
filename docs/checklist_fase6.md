## Checklist Fase 6 — UI Renaissance

### MainWindow Shell

- [ ] **`MainWindow.h` < 100 líneas**: `wc -l < src/ui/qt/MainWindow.h` retorna < 100. Verificar con comando.
- [ ] **`MainWindow.cpp` < 400 líneas**: `wc -l < src/ui/qt/MainWindow.cpp` retorna < 400. (Target: 198 actual)
- [ ] **Sin includes de dominio**: `grep -cE "domain/|services/|core/|analysis/|io/" src/ui/qt/MainWindow.h` retorna 0. Solo `ApplicationContext.h` y UI widgets.
- [ ] **No instancia services**: `grep -cE "AnalysisOrchestrator|SimulationEngine|MaterialRegistry|ParticleRegistry|ExporterRegistry|ImporterRegistry" src/ui/qt/MainWindow.cpp` retorna 0.
- [ ] **Menú File**: contiene "Open..." (`QKeySequence::Open`) y "Quit" (`QKeySequence::Quit`). Ambos conectados.
- [ ] **Menú View**: contiene Analysis (Ctrl+1), BioSim (Ctrl+2), Export (Ctrl+3), y submenú de docks.
- [ ] **Menú Help**: contiene "About" con diálogo `QMessageBox::about`.
- [ ] **Toolbar**: botones Open y Cancel, conectados a `requestOpenFile()` y `requestCancelAnalysis()`.
- [ ] **StatusBar**: contiene `QLabel` (mensajes) + `QProgressBar` (ancho máximo 250px, oculto por defecto).
- [ ] **Geometría persistente**: `QSettings("BeamLabStudio").value("geometry")` guardado en `closeEvent()` y restaurado en constructor. Verificar: mover ventana, cerrar, reabrir → misma posición y tamaño.
- [ ] **`closeEvent`**: llama `saveSettings()` + `QMainWindow::closeEvent()`. No emite eventos de análisis pendientes sin confirmación.

### Vistas

- [ ] **AnalysisView** construye sin crash: `Scene3DWidget` visible, `StatsDashboardWidget` + `RunDashboardWidget` en splitter redimensionable. Verificar con: lanzar app, click en tab "Analysis".
- [ ] **BioSimView** construye sin crash: `BioViewport3D` + `SlabEditor3D` + botón "Materials...". Tab "Bio Simulator" cambia sin delay.
- [ ] **ExportView** construye sin crash: `QListWidget` con formatos csv, obj, parquet checkeados. Botones "Export Selected" y "Export All". `QTextEdit` log.
- [ ] **QSplitter redimensionable**: en AnalysisView, arrastrar divisor entre Scene3D y panels inferiores → ambos paneles se redimensionan suavemente.
- [ ] **Cambio de tabs**: click en Analysis → BioSim → Export → Analysis → el estado visual de cada tab se preserva (no se reconstruyen widgets).
- [ ] **Sin memory leaks en tabs**: abrir/cerrar tabs repetidamente (10×) → `QObject::children()` no crece monótonamente.

### DockManager

- [ ] **Menú View → Docks**: `createViewMenu()` genera submenú con items checkables por cada dockable registrado. Verificar: `menu->actions().size() >= 5`.
- [ ] **Toggle visibilidad**: cerrar dock con "×" → menú View → click en el dock → reaparece en la misma posición.
- [ ] **Mover dock**: arrastrar Scene3DWidget a Right dock area → soltar → se reposiciona. Arrastrar de vuelta a Left.
- [ ] **Persistencia de layout**: mover docks, cerrar app, reabrir → los docks están donde se dejaron. Verificar: `QSettings.value("ui/dock_layout").toByteArray()` idéntico antes/después de reinicio (excepto timestamp).
- [ ] **`resetToDefaults()`**: click en "Reset to defaults" en menú View → todos los docks vuelven a `preferredArea()` con `initiallyVisible()`.
- [ ] **IDockableWidget implementado**: 7 widgets implementan `title()`, `id()`, `widget()`, `preferredArea()`. Verificar con:
  ```bash
  grep -l "IDockableWidget" src/ui/qt/*.h src/ui/qt/widgets/*.h | wc -l
  ```
  debe retornar 7.

### Temas / QSS

- [ ] **Un solo `.qss`**: `grep -c "setStyleSheet" src/ui/qt/MainWindow.cpp src/ui/qt/*.cpp src/ui/qt/widgets/*.cpp 2>/dev/null` retorna 0. Todo el estilo está en `styles/beamlabstudio.qss`.
- [ ] **Tema oscuro por defecto**: fondo general `#071C17` o similar oscuro. Letra legible (contraste ≥4.5:1). Verificar con inspección visual.
- [ ] **QSS embebido**: `resources.qrc` contiene `styles/beamlabstudio.qss` y se carga en `main.cpp` vía `qApp->setStyleSheet()`. Verificar: `grep -c "setStyleSheet" src/ui/qt/main.cpp` ≥ 1.
- [ ] **Sin QSS inline**: `grep -rn "setStyleSheet" src/ui/qt/ --include="*.cpp" | grep -v "main.cpp" | grep -v "test_" | wc -l` retorna 0.
- [ ] **Tema seleccionado persiste**: `QSettings("BeamLabStudio").value("theme")` guardado en `ThemeManager`. Se restaura al iniciar.

### Presenters

- [ ] **`AnalysisPresenter` sin bloqueo**: `startAnalysis()` retorna inmediatamente (no espera al worker thread). Verificar con: `QElapsedTimer t; presenter.startAnalysis(); EXPECT_LT(t.elapsed(), 10)` (menos de 10ms en UI thread).
- [ ] **`AnalysisPresenter` emite `progressUpdated`**: llamar `startAnalysis()`, simular progreso desde mock orchestrator → signal se emite con percent > 0. Verificar con `QSignalSpy`.
- [ ] **`AnalysisPresenter` emite `analysisCompleted`**: mock orchestrator completa con éxito → signal `analysisCompleted` se emite con `result->success == true`.
- [ ] **`AnalysisPresenter` emite `errorOccurred`**: mock orchestrator falla → signal `errorOccurred` contiene mensaje de error.
- [ ] **`BioSimPresenter::runSimulation()` en background**: `QThread::create` + `QMetaObject::invokeMethod`. Verificar con: `EXPECT_FALSE(QThread::currentThread() == workerThread)` dentro del callback.
- [ ] **`ExportPresenter::availableFormats()`**: retorna lista de formatos registrados en `ExporterRegistry`. Si registry es nullptr, retorna lista por defecto `{"csv", "obj", "parquet"}`.
- [ ] **Sin `#include <QMainWindow>` o `<QApplication>`** en presenters: `grep -cE "QMainWindow|QApplication" src/ui/qt/presenters/*.h` retorna 0.
- [ ] **Sin includes de domain** en presenters: `grep -cE "FrameStatistics|TrajectorySample|Sqlite" src/ui/qt/presenters/*.h` retorna 0.

### Threading

- [ ] **Callbacks cross-thread usan `QueuedConnection`**: `grep -c "Qt::QueuedConnection" src/ui/qt/presenters/AnalysisPresenter.cpp` ≥ 2.
- [ ] **Servicios sin Qt**: `grep -rn "QObject\|QWidget\|QApplication\|QThread" src/services/ --include="*.h" | wc -l` retorna 0.
- [ ] **Sin QProcess**: `grep -c "QProcess" src/ui/qt/MainWindow.cpp` retorna 0 (el pipeline bash fue reemplazado por AnalysisOrchestrator nativo).

### Regresión

- [ ] **Tests existentes pasan**: `ctest --test-dir build --output-on-failure` reporta todos los tests como PASS. Comparar contra baseline de Fase 5.
- [ ] **Tests de UI**: `test_AnalysisPresenter` pasa 8/8 tests (constructor, startAnalysis, cancel, loadProfile, filters, formats, exportResults, setImporterRegistry).
- [ ] **Build sin warnings nuevos**: `cmake --build build 2>&1 | grep -E "warning:" | grep -v "third_party" | wc -l` no aumentó vs Fase 5.
- [ ] **Build Release**: `cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON && cmake --build build-release -j$(nproc)` completa en <150s.
- [ ] **Smoke test manual**: app arranca, tab Analysis muestra 3D viewport, File → Open abre diálogo con filtros dinámicos, BioSim tab tiene controles, Export tab lista formatos.

### Métricas Objetivo

- [ ] `MainWindow.cpp` líneas: `wc -l < src/ui/qt/MainWindow.cpp` = **______** (target: < 400, actual: 198).
- [ ] `MainWindow.h` líneas: `wc -l < src/ui/qt/MainWindow.h` = **______** (target: < 100, actual: 73).
- [ ] Comunidades con cohesión < 0.10: **______** (target: 0, medida con graphify post-migración).
- [ ] Nodos débilmente conectados: **______** (target: < 20, medido con graphify query).
- [ ] Tiempo de build: `cmake --build build-release -j$(nproc) 2>&1 | tail -1` = **______**.

### Comandos de verificación rápida

```bash
# 1. Líneas de MainWindow
wc -l src/ui/qt/MainWindow.h src/ui/qt/MainWindow.cpp

# 2. Sin includes de dominio en MainWindow
grep -cE "domain/|services/|core/|analysis/" src/ui/qt/MainWindow.h || echo "clean"
grep -cE "QProcess|std::thread" src/ui/qt/MainWindow.cpp || echo "clean"

# 3. Sin setStyleSheet inline
grep -rn "setStyleSheet" src/ui/qt/ --include="*.cpp" | grep -v "main.cpp" | grep -v "test_"
echo "(debe estar vacío)"

# 4. Build y tests
cmake --build build -j$(nproc) 2>&1 | grep -cE "error:|warning:"
ctest --test-dir build --output-on-failure -j$(nproc) 2>&1 | tail -5

# 5. Widgets IDockableWidget
grep -l "IDockableWidget" src/ui/qt/*.h src/ui/qt/widgets/*.h

# 6. QRC resources
grep -c "beamlabstudio.qss" src/ui/qt/resources.qrc

# 7. Tests de presenters
ctest --test-dir build -R test_AnalysisPresenter --output-on-failure
```
