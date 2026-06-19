# Spec: Rediseño UI moderno + ejecutable macOS

**Fecha:** 2026-06-12
**Estado:** Aprobado (pendiente de plan de implementación)
**Autor:** Brainstorming colaborativo (José + Claude)

---

## 1. Objetivo

Modernizar por completo la experiencia de usuario de BeamLabStudio (Qt6 Widgets)
y producir un ejecutable distribuible para macOS (Apple Silicon, arm64):
`BeamLabStudio.app` + `BeamLabStudio.dmg` sin firmar.

### Criterios de éxito

1. La navegación pasa de **14 pestañas en fila** a un **riel lateral de 6 secciones**.
2. Existe una **paleta de comandos ⌘K** que salta a cualquier vista/acción.
3. El look se moderniza (QSS único sobre la paleta obsidian/cyan/blue existente).
4. `BeamLabStudio.app` arranca por doble clic con los frameworks Qt embebidos.
5. `scripts/package_macos.sh` produce el `.app` y el `.dmg` de forma reproducible.
6. Los 91 tests unitarios actuales siguen verdes; se añaden tests para la lógica nueva.
7. La regresión de `tests/muon_tracks.csv` sigue dando foco bin 264 / z=11.656 m.

## 2. Restricciones y decisiones

- **Permiso:** libertad total en `src/ui/` (MainWindow, widgets, views, QSS). La lógica
  de negocio (presenters, `beamlab_core`, `beamlab_domain`) se preserva.
- **No tocar:** `docs/`, `README.md` (acuerdo CLAUDE.md). Por eso este spec vive en `specs/`.
- **Empaque:** `.app` + `.dmg` **sin firmar** (no hay cuenta Apple Developer). Primera
  apertura vía clic-derecho ▸ Abrir (documentar).
- **Plataforma de build:** macOS arm64, Qt6 vía Homebrew (`$(brew --prefix qt)`).

## 3. Estrategia: rediseñar el shell, reutilizar los widgets pesados

Los visores y motores de render son código probado y **no se reescriben**; se reubican
dentro de la nueva estructura:

- `Scene3DWidget` (`combined_scene_viewer_`) — visor 3D
- `ObjViewerWidget` — visores OBJ
- `StatsDashboardWidget` (`statistics_dashboard_`) — dashboard de estadística
- `RunDashboardWidget` (`dashboard_`) — overview
- `BioSimWidget` (`bio_sim_widget_`) — simulación biológica
- `InteractiveGraphicsView` — gráficos 2D
- `InfoWidget` (`info_widget_`)

Lo **nuevo** es la cáscara de navegación y el estilo.

## 4. Componentes nuevos — `src/ui/qt/shell/`

### 4.1 `ActionRegistry` (lógica pura, sin Qt GUI → testeable)

```cpp
struct Action {
    std::string id;          // "nav.scene3d", "file.open", "export.mp4"
    std::string title;       // "Ir a Scene 3D", "Abrir archivo…"
    std::string category;    // "Navegación", "Archivo", "Exportar"
    std::string shortcut;    // "⌘1", "⌘O" (display only)
    std::function<void()> run;
};

class ActionRegistry {
public:
    void add(Action a);
    const std::vector<Action>& all() const;
    // Filtro difuso por subsequence-match sobre title+category, rankeado.
    std::vector<const Action*> search(std::string_view query) const;
};
```

El algoritmo de `search()` (subsequence fuzzy match + scoring) es **pura lógica**
y se prueba con TDD en `beamlab_tests` sin instanciar Qt.

### 4.2 `NavigationRail : QWidget`

- Riel vertical con 6 ítems (icono + label), agrupados en "ANÁLISIS" y "SIMULACIÓN".
- Señal `void sectionChanged(int index)`.
- Ítem activo resaltado (borde `blue_deep`, icono `cyan`).

### 4.3 `TopBar : QWidget`

- Logo + título, botones Open / **Analyze** (primario) / Export ▾.
- A la derecha: pill de estado (`N muestras`, color verde/ámbar/rojo) y botón "⌘K".
- Emite señales `openRequested()`, `analyzeRequested()`, `exportRequested(format)`.

### 4.4 `CommandPalette : QDialog`

- `QLineEdit` de búsqueda + `QListView` filtrado en vivo contra `ActionRegistry::search()`.
- Navegación con flechas, Enter ejecuta `Action::run`, Esc cierra.
- Se abre con `QShortcut(QKeySequence("Ctrl+K"), mainWindow)` (Cmd+K en macOS).

### 4.5 `PropertiesPanel : QWidget`

- Panel derecho contextual. Para Scene 3D: lista de capas con opacidad, presets de
  cámara (incluido "Eje horizontal" = `frameLongestAxisHorizontally()`), slider λ,
  bloque de lectura de foco (r, z, bin, confianza de eje).
- Reutiliza los controles ya existentes en el dock combinado de MainWindow, movidos aquí.

### 4.6 Secciones — `QStackedWidget` con 6 páginas

| Índice | Sección | Contenido |
|---|---|---|
| 0 | Overview | `RunDashboardWidget` |
| 1 | Scene 3D | `Scene3DWidget` + `PropertiesPanel` |
| 2 | Plots | control segmentado: Statistics / Trajectories 2D / Focal slice / Envelope rings |
| 3 | Data | selector sobre 3 tablas CSV (trajectories / focal slice / envelope rings) |
| 4 | BioSim | `BioSimWidget` |
| 5 | Info & Docs | `InfoWidget` + `math_docs.html` |

`MainWindow` se reduce a coordinador: instancia el shell, conecta
`NavigationRail::sectionChanged` → `QStackedWidget::setCurrentIndex`, registra las
acciones en `ActionRegistry`, y mantiene el cableado existente con `AnalysisPresenter`
(`analysisPresenter_`) intacto.

## 5. QSS modernizado

Reescritura de `src/ui/qt/styles/beamlabstudio.qss` con:
- Tokens de color de `palette.json` (sin cambiar la identidad visual).
- Radios uniformes (10–14px), estados `:hover` / `:focus` / `:pressed` consistentes.
- Estilado de `NavigationRail`, `CommandPalette`, `PropertiesPanel`, control segmentado.
- **Única fuente de verdad**: eliminar `setStyleSheet()` inline de MainWindow (cierra Q-03 del PRA).

## 6. Empaquetado macOS — nuevos artefactos

### 6.1 CMake (`src/ui/CMakeLists.txt`)

```cmake
if(APPLE)
  set_target_properties(beamlab_ui PROPERTIES
    MACOSX_BUNDLE ON
    OUTPUT_NAME "BeamLabStudio"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/resources/macos/Info.plist.in"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
  # AppIcon.icns copiado a Contents/Resources vía RESOURCE o install.
endif()
```

### 6.2 Archivos nuevos

- `resources/macos/Info.plist.in` — `CFBundleIdentifier=studio.beamlab.app`,
  versión, `NSHighResolutionCapable`, categoría científica.
- `resources/macos/AppIcon.icns` — generado desde `beamlabstudio_icon.svg` con
  `sips` (rasteriza a PNG @ 16…1024) + `iconutil -c icns`.
- `scripts/make_icns.sh` — script reproducible de generación del icono.
- `scripts/package_macos.sh`:
  1. `cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON -DCMAKE_PREFIX_PATH=$(brew --prefix qt)`
  2. `cmake --build build-release --target beamlab_ui -j`
  3. `macdeployqt build-release/.../BeamLabStudio.app`
  4. `hdiutil create -volname BeamLabStudio -srcfolder … BeamLabStudio.dmg`

## 7. Testing y verificación

- **TDD** (en `beamlab_tests`, sin GUI):
  - `ActionRegistry::add/all/search` — orden, match difuso, ranking, query vacía.
  - Casos: subsequence match, case-insensitive, sin resultados, empate de score.
- **Verificación manual del .app** (skill `verification-before-completion`):
  - Lanzar `BeamLabStudio.app`, confirmar arranque sin Qt en el sistema.
  - Las 6 secciones cambian; ⌘K abre y filtra; "Eje horizontal" alinea la cámara.
  - Open + Analyze de `tests/muon_tracks.csv` → foco bin 264 / z=11.656 m.
- **Regresión:** `ctest` con los 91 tests verdes.

## 8. Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| MainWindow (3302 líneas) tiene cableado denso | Migración incremental: el shell es nuevo; las páginas reusan widgets existentes; se valida tras cada sección |
| `macdeployqt` deja frameworks sin resolver | Verificar con `otool -L` y lanzar el `.app` en limpio; `macdeployqt` antes del DMG |
| Romper el flujo Analyze al mover controles | No tocar `AnalysisPresenter`; solo reparentar widgets, conservando señales |
| QSS nuevo rompe legibilidad | Comparar antes/después lanzando el `.app`; mantener tokens de `palette.json` |

## 9. Fuera de alcance (YAGNI)

- Migración a QML/Qt Quick.
- Docks arrastrables (Dirección B del brainstorming).
- Firma y notarización Apple.
- Reescritura de motores de render o de física.
- Herramientas web (skills `frontend-design`, MCP de Chrome, `webapp-testing`): no
  aplican a una app Qt/C++ nativa.

## 10. Entregables

1. Componentes shell nuevos en `src/ui/qt/shell/`.
2. `MainWindow` reducido a coordinador + secciones reorganizadas.
3. `beamlabstudio.qss` reescrito.
4. `resources/macos/` (Info.plist, icono) + `scripts/package_macos.sh` + `scripts/make_icns.sh`.
5. Tests nuevos de `ActionRegistry` conectados al build.
6. Entrada en `CHANGELOG.md`.
