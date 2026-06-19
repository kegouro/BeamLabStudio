# Rediseño UI moderno + ejecutable macOS — Plan de Implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Modernizar la UI de BeamLabStudio (riel lateral de 6 secciones + paleta de comandos ⌘K + QSS nuevo) y producir `BeamLabStudio.app` + `BeamLabStudio.dmg` sin firmar para macOS arm64.

**Architecture:** Se rediseña la *cáscara* (navegación + estilo) y se reutilizan los widgets pesados de render/física existentes (`Scene3DWidget`, `StatsDashboardWidget`, etc.) reubicándolos en un `QStackedWidget` de 6 páginas guiado por un `NavigationRail`. `MainWindow` pasa a coordinador. La lógica de negocio (`AnalysisPresenter`, `beamlab_core`, `beamlab_domain`) no se toca.

**Tech Stack:** C++20, Qt6 Widgets/Concurrent, CMake, GoogleTest, macdeployqt, sips/iconutil/hdiutil.

**Build/test loop (macOS):**
```bash
cmake -B build-mac -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build-mac -j$(sysctl -n hw.ncpu)
cmake --build build-mac --target beamlab_tests -j$(sysctl -n hw.ncpu)
./build-mac/tests/unit/beamlab_tests
```

> **Nota de orden:** La Parte A (empaquetado) es independiente y entregable por sí sola — se hace primero porque da valor inmediato y bajo riesgo. La Parte B (rediseño) es la estructural.

---

## PARTE A — Ejecutable macOS (.app + .dmg)

### Task A1: Generar el icono `.icns` desde el SVG

**Files:**
- Create: `scripts/make_icns.sh`
- Create (output): `resources/macos/AppIcon.icns`

- [ ] **Step 1: Crear el script generador**

```bash
mkdir -p scripts resources/macos
```

Create `scripts/make_icns.sh`:

```bash
#!/bin/bash
# Genera resources/macos/AppIcon.icns desde el SVG de marca.
# Requiere: rsvg-convert o qlmanage/sips para rasterizar; iconutil (nativo macOS).
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SVG="${PROJECT_DIR}/src/ui/qt/assets/branding/beamlabstudio_icon.svg"
OUT="${PROJECT_DIR}/resources/macos/AppIcon.icns"
WORK="$(mktemp -d)"
ICONSET="${WORK}/AppIcon.iconset"
mkdir -p "${ICONSET}"

# Rasteriza un PNG base de 1024px. Preferimos rsvg-convert; si no, qlmanage.
BASE="${WORK}/base_1024.png"
if command -v rsvg-convert >/dev/null 2>&1; then
    rsvg-convert -w 1024 -h 1024 "${SVG}" -o "${BASE}"
else
    qlmanage -t -s 1024 -o "${WORK}" "${SVG}" >/dev/null 2>&1
    mv "${WORK}/$(basename "${SVG}").png" "${BASE}"
fi

# Escala a todos los tamaños requeridos por iconutil.
for size in 16 32 64 128 256 512 1024; do
    sips -z "${size}" "${size}" "${BASE}" --out "${ICONSET}/icon_${size}x${size}.png" >/dev/null
done
# Variantes @2x (retina): icon_NxN@2x = 2*N px.
cp "${ICONSET}/icon_32x32.png"   "${ICONSET}/icon_16x16@2x.png"
cp "${ICONSET}/icon_64x64.png"   "${ICONSET}/icon_32x32@2x.png"
cp "${ICONSET}/icon_256x256.png" "${ICONSET}/icon_128x128@2x.png"
cp "${ICONSET}/icon_512x512.png" "${ICONSET}/icon_256x256@2x.png"
cp "${ICONSET}/icon_1024x1024.png" "${ICONSET}/icon_512x512@2x.png"

mkdir -p "$(dirname "${OUT}")"
iconutil -c icns "${ICONSET}" -o "${OUT}"
echo "[make_icns] Generado: ${OUT}"
rm -rf "${WORK}"
```

- [ ] **Step 2: Ejecutar y verificar que produce el .icns**

Run:
```bash
chmod +x scripts/make_icns.sh && ./scripts/make_icns.sh && file resources/macos/AppIcon.icns
```
Expected: imprime `[make_icns] Generado: …/AppIcon.icns` y `file` reporta `Mac OS X icon`.

- [ ] **Step 3: Commit**

```bash
git add scripts/make_icns.sh resources/macos/AppIcon.icns
git commit -m "build(macos): generate AppIcon.icns from brand SVG"
```

---

### Task A2: Info.plist y bundle en CMake

**Files:**
- Create: `resources/macos/Info.plist.in`
- Modify: `src/ui/CMakeLists.txt` (target `beamlab_ui`, tras `add_executable`, ~línea 60)

- [ ] **Step 1: Crear el Info.plist plantilla**

Create `resources/macos/Info.plist.in`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>            <string>BeamLabStudio</string>
    <key>CFBundleDisplayName</key>     <string>BeamLabStudio</string>
    <key>CFBundleExecutable</key>      <string>BeamLabStudio</string>
    <key>CFBundleIdentifier</key>      <string>studio.beamlab.app</string>
    <key>CFBundleVersion</key>         <string>${PROJECT_VERSION}</string>
    <key>CFBundleShortVersionString</key><string>${PROJECT_VERSION}</string>
    <key>CFBundlePackageType</key>     <string>APPL</string>
    <key>CFBundleIconFile</key>        <string>AppIcon</string>
    <key>NSHighResolutionCapable</key> <true/>
    <key>LSMinimumSystemVersion</key>  <string>12.0</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.education</string>
    <key>NSHumanReadableCopyright</key><string>BeamLabStudio</string>
</dict>
</plist>
```

- [ ] **Step 2: Añadir configuración del bundle en CMake**

In `src/ui/CMakeLists.txt`, after the `add_executable(beamlab_ui … )` block (currently ends at line 60), add:

```cmake
# ── macOS .app bundle ──────────────────────────────────────────────
if(APPLE)
    set(BEAMLAB_ICNS "${PROJECT_SOURCE_DIR}/resources/macos/AppIcon.icns")
    set_source_files_properties("${BEAMLAB_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(beamlab_ui PRIVATE "${BEAMLAB_ICNS}")
    set_target_properties(beamlab_ui PROPERTIES
        MACOSX_BUNDLE ON
        OUTPUT_NAME "BeamLabStudio"
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/resources/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "BeamLabStudio"
        MACOSX_BUNDLE_GUI_IDENTIFIER "studio.beamlab.app"
        MACOSX_BUNDLE_ICON_FILE "AppIcon")
endif()
```

- [ ] **Step 3: Reconfigurar, compilar y verificar que se crea el .app**

Run:
```bash
cmake -B build-mac -DCMAKE_BUILD_TYPE=Release -DBEAMLAB_ENABLE_QT_UI=ON -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build-mac --target beamlab_ui -j$(sysctl -n hw.ncpu)
ls -d build-mac/src/ui/BeamLabStudio.app && ls build-mac/src/ui/BeamLabStudio.app/Contents/Resources/AppIcon.icns
```
Expected: existen `BeamLabStudio.app` y el `AppIcon.icns` dentro del bundle.

- [ ] **Step 4: Verificar que arranca**

Run: `open build-mac/src/ui/BeamLabStudio.app`
Expected: abre la ventana de BeamLabStudio (aún con la UI vieja — es esperado en esta fase).

- [ ] **Step 5: Commit**

```bash
git add resources/macos/Info.plist.in src/ui/CMakeLists.txt
git commit -m "build(macos): produce BeamLabStudio.app bundle with icon and Info.plist"
```

---

### Task A3: Script de empaquetado (macdeployqt + DMG)

**Files:**
- Create: `scripts/package_macos.sh`

- [ ] **Step 1: Crear el script de empaque**

Create `scripts/package_macos.sh`:

```bash
#!/bin/bash
# Empaqueta BeamLabStudio.app autocontenido + BeamLabStudio.dmg (sin firmar).
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-release"
QT_PREFIX="$(brew --prefix qt)"
APP="${BUILD_DIR}/src/ui/BeamLabStudio.app"
DMG="${PROJECT_DIR}/BeamLabStudio.dmg"

echo "[package] Configurando Release…"
cmake -B "${BUILD_DIR}" -S "${PROJECT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBEAMLAB_ENABLE_QT_UI=ON \
    -DCMAKE_PREFIX_PATH="${QT_PREFIX}"

echo "[package] Compilando beamlab_ui…"
cmake --build "${BUILD_DIR}" --target beamlab_ui -j"$(sysctl -n hw.ncpu)"

echo "[package] Embebiendo frameworks Qt con macdeployqt…"
"${QT_PREFIX}/bin/macdeployqt" "${APP}" -always-overwrite

echo "[package] Verificando dependencias del binario…"
otool -L "${APP}/Contents/MacOS/BeamLabStudio" | grep -i qt || true

echo "[package] Creando DMG…"
rm -f "${DMG}"
STAGING="$(mktemp -d)"
cp -R "${APP}" "${STAGING}/"
ln -s /Applications "${STAGING}/Applications"
hdiutil create -volname "BeamLabStudio" -srcfolder "${STAGING}" \
    -ov -format UDZO "${DMG}"
rm -rf "${STAGING}"

echo "[package] Listo:"
echo "  App: ${APP}"
echo "  DMG: ${DMG}"
```

- [ ] **Step 2: Ejecutar el empaque completo**

Run: `chmod +x scripts/package_macos.sh && ./scripts/package_macos.sh`
Expected: termina con "Listo", genera `BeamLabStudio.dmg` en la raíz, y `otool -L` muestra rutas `@rpath/Qt*` (no `/opt/homebrew`).

- [ ] **Step 3: Verificar autocontenido (prueba dura)**

Run:
```bash
otool -L build-release/src/ui/BeamLabStudio.app/Contents/MacOS/BeamLabStudio | grep -c "/opt/homebrew" || echo 0
```
Expected: `0` — ninguna dependencia apunta a Homebrew (todo embebido).

- [ ] **Step 4: Commit**

```bash
git add scripts/package_macos.sh
git commit -m "build(macos): add package_macos.sh (macdeployqt + dmg)"
```

---

## PARTE B — Rediseño UI

### Task B1: `ActionRegistry` con fuzzy search (TDD, lógica pura)

**Files:**
- Create: `src/ui/qt/shell/ActionRegistry.h`
- Create: `src/ui/qt/shell/ActionRegistry.cpp`
- Test: `tests/unit/ui/test_ActionRegistry.cpp`
- Modify: `src/ui/CMakeLists.txt` (añadir el .cpp al target)
- Modify: `tests/unit/CMakeLists.txt` (añadir el test) y `CMakeLists.txt` raíz si hace falta para linkear

- [ ] **Step 1: Escribir el test que falla**

Create `tests/unit/ui/test_ActionRegistry.cpp`:

```cpp
#include "ui/qt/shell/ActionRegistry.h"
#include <gtest/gtest.h>

using beamlab::ui::Action;
using beamlab::ui::ActionRegistry;

static ActionRegistry makeRegistry() {
    ActionRegistry r;
    r.add({"nav.overview", "Ir a Overview",  "Navegación", "Cmd+1", []{}});
    r.add({"nav.scene3d",  "Ir a Scene 3D",  "Navegación", "Cmd+2", []{}});
    r.add({"file.open",    "Abrir archivo",  "Archivo",    "Cmd+O", []{}});
    r.add({"export.mp4",   "Exportar MP4",   "Exportar",   "",      []{}});
    return r;
}

TEST(ActionRegistryTest, EmptyQueryReturnsAllInInsertionOrder) {
    auto r = makeRegistry();
    auto results = r.search("");
    ASSERT_EQ(results.size(), 4u);
    EXPECT_EQ(results[0]->id, "nav.overview");
    EXPECT_EQ(results[3]->id, "export.mp4");
}

TEST(ActionRegistryTest, SubstringMatchIsCaseInsensitive) {
    auto r = makeRegistry();
    auto results = r.search("scene");
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0]->id, "nav.scene3d");
}

TEST(ActionRegistryTest, SubsequenceMatchFindsScattered) {
    auto r = makeRegistry();
    // "s3" debe encajar en "Ir a Scene 3D" (S … 3) por subsecuencia.
    auto results = r.search("s3");
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0]->id, "nav.scene3d");
}

TEST(ActionRegistryTest, NoMatchIsExcluded) {
    auto r = makeRegistry();
    auto results = r.search("zzzz");
    EXPECT_TRUE(results.empty());
}

TEST(ActionRegistryTest, CloserMatchRanksHigher) {
    auto r = makeRegistry();
    // "abrir" es match consecutivo en "Abrir archivo" → debe ir primero
    // por encima de cualquier match difuso disperso.
    auto results = r.search("abrir");
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0]->id, "file.open");
}
```

- [ ] **Step 2: Crear la cabecera para que compile (sin lógica todavía)**

Create `src/ui/qt/shell/ActionRegistry.h`:

```cpp
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace beamlab::ui {

struct Action {
    std::string id;
    std::string title;
    std::string category;
    std::string shortcut;
    std::function<void()> run;
};

/// Registro de acciones invocables, con búsqueda difusa para la paleta ⌘K.
/// Lógica pura — sin dependencias de Qt, testeable en aislamiento.
class ActionRegistry {
public:
    void add(Action a);
    const std::vector<Action>& all() const { return actions_; }

    /// Devuelve punteros a las acciones cuyo título/categoría hacen match
    /// difuso (subsecuencia, case-insensitive) con `query`, ordenadas por
    /// score descendente. Query vacía devuelve todas en orden de inserción.
    /// Los punteros son válidos mientras no se llame add() de nuevo.
    std::vector<const Action*> search(std::string_view query) const;

private:
    std::vector<Action> actions_;
};

} // namespace beamlab::ui
```

- [ ] **Step 3: Conectar al build y correr el test para verlo fallar**

In `src/ui/CMakeLists.txt`, inside `add_executable(beamlab_ui … )`, after `qt/MainWindow.cpp` add:
```cmake
    qt/shell/ActionRegistry.cpp
```

The test target `beamlab_tests` links `beamlab_core` but NOT `beamlab_ui` (Qt). `ActionRegistry` is Qt-free, so compile the test against the source directly. In `tests/unit/CMakeLists.txt`, add to the `add_executable(beamlab_tests …)` list:
```cmake
    ui/test_ActionRegistry.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/qt/shell/ActionRegistry.cpp
```

Run:
```bash
cmake -B build-mac -DBEAMLAB_ENABLE_QT_UI=ON -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build-mac --target beamlab_tests -j$(sysctl -n hw.ncpu)
```
Expected: FALLA al linkear con "undefined symbols: ActionRegistry::add / ::search" (no hay .cpp aún).

- [ ] **Step 4: Implementar la lógica mínima**

Create `src/ui/qt/shell/ActionRegistry.cpp`:

```cpp
#include "ui/qt/shell/ActionRegistry.h"

#include <algorithm>
#include <cctype>
#include <optional>

namespace beamlab::ui {

namespace {

char lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool isWordStart(std::string_view s, std::size_t i) {
    if (i == 0) return true;
    const char p = s[i - 1];
    return p == ' ' || p == '.' || p == '-' || p == '/';
}

// Subsecuencia case-insensitive de `query` en `hay`. nullopt si no hay match.
// Score mayor = mejor: premia matches consecutivos y en inicio de palabra.
std::optional<int> fuzzyScore(std::string_view hay, std::string_view query) {
    if (query.empty()) return 0;
    int score = 0;
    std::size_t qi = 0;
    bool prevMatched = false;
    for (std::size_t i = 0; i < hay.size() && qi < query.size(); ++i) {
        if (lower(hay[i]) == lower(query[qi])) {
            score += 10;
            if (prevMatched) score += 15;          // consecutivo
            if (isWordStart(hay, i)) score += 20;   // inicio de palabra
            prevMatched = true;
            ++qi;
        } else {
            prevMatched = false;
        }
    }
    if (qi != query.size()) return std::nullopt;    // no se consumió toda la query
    return score;
}

} // namespace

void ActionRegistry::add(Action a) {
    actions_.push_back(std::move(a));
}

std::vector<const Action*> ActionRegistry::search(std::string_view query) const {
    struct Scored { const Action* action; int score; std::size_t order; };
    std::vector<Scored> scored;
    for (std::size_t i = 0; i < actions_.size(); ++i) {
        const auto& a = actions_[i];
        // Busca el mejor score entre título y categoría.
        auto st = fuzzyScore(a.title, query);
        auto sc = fuzzyScore(a.category, query);
        std::optional<int> best;
        if (st) best = st;
        if (sc && (!best || *sc > *best)) best = sc;
        if (best) scored.push_back({&a, *best, i});
    }
    // Orden estable: score desc, luego orden de inserción.
    std::stable_sort(scored.begin(), scored.end(),
        [](const Scored& x, const Scored& y) {
            if (x.score != y.score) return x.score > y.score;
            return x.order < y.order;
        });
    std::vector<const Action*> out;
    out.reserve(scored.size());
    for (const auto& s : scored) out.push_back(s.action);
    return out;
}

} // namespace beamlab::ui
```

- [ ] **Step 5: Correr los tests y verlos pasar**

Run:
```bash
cmake --build build-mac --target beamlab_tests -j$(sysctl -n hw.ncpu)
./build-mac/tests/unit/beamlab_tests --gtest_filter='ActionRegistryTest.*'
```
Expected: PASS (5 tests). Confirma además que el total sigue verde: `./build-mac/tests/unit/beamlab_tests`.

- [ ] **Step 6: Commit**

```bash
git add src/ui/qt/shell/ActionRegistry.h src/ui/qt/shell/ActionRegistry.cpp \
        tests/unit/ui/test_ActionRegistry.cpp src/ui/CMakeLists.txt tests/unit/CMakeLists.txt
git commit -m "feat(ui): ActionRegistry with fuzzy command search (TDD)"
```

---

### Task B2: `CommandPalette` (diálogo ⌘K)

**Files:**
- Create: `src/ui/qt/shell/CommandPalette.h`
- Create: `src/ui/qt/shell/CommandPalette.cpp`
- Modify: `src/ui/CMakeLists.txt` (añadir el .cpp)

- [ ] **Step 1: Crear la cabecera**

Create `src/ui/qt/shell/CommandPalette.h`:

```cpp
#pragma once

#include <QDialog>

#include "ui/qt/shell/ActionRegistry.h"

class QLineEdit;
class QListWidget;

namespace beamlab::ui {

/// Paleta de comandos estilo ⌘K: campo de búsqueda + lista filtrada en vivo.
/// Enter ejecuta la acción seleccionada; Esc cierra.
class CommandPalette final : public QDialog {
public:
    explicit CommandPalette(const ActionRegistry* registry, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh(const QString& query);
    void runSelected();

    const ActionRegistry* registry_;
    QLineEdit* search_{nullptr};
    QListWidget* list_{nullptr};
};

} // namespace beamlab::ui
```

- [ ] **Step 2: Implementar el diálogo**

Create `src/ui/qt/shell/CommandPalette.cpp`:

```cpp
#include "ui/qt/shell/CommandPalette.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace beamlab::ui {

CommandPalette::CommandPalette(const ActionRegistry* registry, QWidget* parent)
    : QDialog(parent), registry_(registry) {
    setObjectName("CommandPalette");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    resize(560, 360);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    search_ = new QLineEdit(this);
    search_->setObjectName("CommandSearch");
    search_->setPlaceholderText("Buscar acción…  (⌘K)");
    layout->addWidget(search_);

    list_ = new QListWidget(this);
    list_->setObjectName("CommandList");
    layout->addWidget(list_, 1);

    connect(search_, &QLineEdit::textChanged, this, &CommandPalette::refresh);
    connect(list_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem*) { runSelected(); });

    // Flechas desde el campo de búsqueda mueven la selección de la lista.
    search_->installEventFilter(this);
}

void CommandPalette::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    search_->clear();
    refresh("");
    search_->setFocus();
}

void CommandPalette::refresh(const QString& query) {
    list_->clear();
    const auto results = registry_->search(query.toStdString());
    for (const Action* a : results) {
        QString label = QString::fromStdString(a->title);
        if (!a->shortcut.empty())
            label += "\t" + QString::fromStdString(a->shortcut);
        auto* item = new QListWidgetItem(label, list_);
        item->setData(Qt::UserRole, QString::fromStdString(a->id));
    }
    if (list_->count() > 0) list_->setCurrentRow(0);
}

void CommandPalette::runSelected() {
    auto* item = list_->currentItem();
    if (item == nullptr) return;
    const QString id = item->data(Qt::UserRole).toString();
    for (const Action& a : registry_->all()) {
        if (QString::fromStdString(a.id) == id) {
            accept();
            if (a.run) a.run();
            return;
        }
    }
}

} // namespace beamlab::ui
```

> Nota: el `eventFilter` para flechas se añade en el siguiente paso; aquí basta con Enter (itemActivated) y el doble clic.

- [ ] **Step 3: Añadir navegación con flechas (eventFilter)**

In `CommandPalette.h`, add to the class:
```cpp
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
```

In `CommandPalette.cpp`, add:
```cpp
bool CommandPalette::eventFilter(QObject* obj, QEvent* event) {
    if (obj == search_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Down) {
            const int n = list_->count();
            if (n > 0) list_->setCurrentRow((list_->currentRow() + 1) % n);
            return true;
        }
        if (key->key() == Qt::Key_Up) {
            const int n = list_->count();
            if (n > 0) list_->setCurrentRow((list_->currentRow() - 1 + n) % n);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            runSelected();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}
```

- [ ] **Step 4: Conectar al build y compilar**

In `src/ui/CMakeLists.txt`, inside `add_executable(beamlab_ui …)`, after `qt/shell/ActionRegistry.cpp` add:
```cmake
    qt/shell/CommandPalette.cpp
```

Run: `cmake --build build-mac --target beamlab_ui -j$(sysctl -n hw.ncpu)`
Expected: compila sin errores (aún no se usa; se cablea en B5).

- [ ] **Step 5: Commit**

```bash
git add src/ui/qt/shell/CommandPalette.h src/ui/qt/shell/CommandPalette.cpp src/ui/CMakeLists.txt
git commit -m "feat(ui): CommandPalette dialog backed by ActionRegistry"
```

---

### Task B3: `NavigationRail` (riel lateral de secciones)

**Files:**
- Create: `src/ui/qt/shell/NavigationRail.h`
- Create: `src/ui/qt/shell/NavigationRail.cpp`
- Modify: `src/ui/CMakeLists.txt` (añadir el .cpp)

- [ ] **Step 1: Crear la cabecera**

Create `src/ui/qt/shell/NavigationRail.h`:

```cpp
#pragma once

#include <QWidget>
#include <vector>

class QButtonGroup;
class QVBoxLayout;

namespace beamlab::ui {

/// Riel lateral de navegación. Cada ítem corresponde a una página del
/// QStackedWidget del workspace. Emite sectionChanged(index) al activarse.
class NavigationRail final : public QWidget {
    Q_OBJECT
public:
    explicit NavigationRail(QWidget* parent = nullptr);

    /// Añade un ítem (icono textual + etiqueta). Devuelve su índice.
    int addItem(const QString& glyph, const QString& label);
    /// Añade un encabezado de grupo no seleccionable ("ANÁLISIS").
    void addGroupHeader(const QString& text);

    void setCurrentIndex(int index);
    int currentIndex() const;

signals:
    void sectionChanged(int index);

private:
    QVBoxLayout* layout_{nullptr};
    QButtonGroup* group_{nullptr};
    int next_index_{0};
};

} // namespace beamlab::ui
```

- [ ] **Step 2: Implementar el riel**

Create `src/ui/qt/shell/NavigationRail.cpp`:

```cpp
#include "ui/qt/shell/NavigationRail.h"

#include <QButtonGroup>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace beamlab::ui {

NavigationRail::NavigationRail(QWidget* parent) : QWidget(parent) {
    setObjectName("NavigationRail");
    setFixedWidth(184);
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(8, 10, 8, 10);
    layout_->setSpacing(3);
    group_ = new QButtonGroup(this);
    group_->setExclusive(true);
    connect(group_, &QButtonGroup::idClicked, this,
            [this](int id) { emit sectionChanged(id); });
}

void NavigationRail::addGroupHeader(const QString& text) {
    auto* header = new QLabel(text, this);
    header->setObjectName("NavGroupHeader");
    layout_->addWidget(header);
}

int NavigationRail::addItem(const QString& glyph, const QString& label) {
    auto* btn = new QToolButton(this);
    btn->setObjectName("NavItem");
    btn->setCheckable(true);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setText("  " + glyph + "   " + label);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const int index = next_index_++;
    group_->addButton(btn, index);
    layout_->addWidget(btn);
    if (index == 0) btn->setChecked(true);
    return index;
}

void NavigationRail::setCurrentIndex(int index) {
    if (auto* btn = group_->button(index)) btn->setChecked(true);
}

int NavigationRail::currentIndex() const {
    return group_->checkedId();
}

} // namespace beamlab::ui
```

> Cuando exista `addStretch` deseado entre grupos, MainWindow llamará `layout()->addStretch()` indirectamente; para mantener el riel simple, el stretch final se añade en MainWindow tras poblar los ítems mediante `findChild`/acceso directo no es necesario — el QSS da el espaciado. (Si se quiere empujar "Info" abajo, añadir un método `addStretch()` análogo.)

- [ ] **Step 3: Añadir `addStretch()` para empujar el último grupo abajo**

In `NavigationRail.h` public section add:
```cpp
    void addStretch();
```
In `NavigationRail.cpp` add:
```cpp
void NavigationRail::addStretch() { layout_->addStretch(1); }
```

- [ ] **Step 4: Conectar al build y compilar**

In `src/ui/CMakeLists.txt`, after `qt/shell/CommandPalette.cpp` add:
```cmake
    qt/shell/NavigationRail.cpp
```
Run: `cmake --build build-mac --target beamlab_ui -j$(sysctl -n hw.ncpu)`
Expected: compila (AUTOMOC procesa el `Q_OBJECT`).

- [ ] **Step 5: Commit**

```bash
git add src/ui/qt/shell/NavigationRail.h src/ui/qt/shell/NavigationRail.cpp src/ui/CMakeLists.txt
git commit -m "feat(ui): NavigationRail sidebar widget with grouped sections"
```

---

### Task B4: Reemplazar `QTabWidget` por `NavigationRail` + `QStackedWidget`

**Contexto:** En `src/ui/qt/MainWindow.cpp`, `buildUi()` crea `tabs_` (un `QTabWidget`) y le añade 14 páginas con `tabs_->addTab(widget, "Nombre")` (líneas ~1019–1224). Las páginas son widgets ya construidos (`dashboard_`, `combined_page_`, `statistics_page`, etc.). Esta tarea sustituye el contenedor sin reconstruir esas páginas.

> **Sobre el `PropertiesPanel` del spec (§4.5) y el `TopBar` (§4.3):** se satisfacen por **reutilización**, no por clases nuevas. Los controles de Scene 3D (capas, cámara, foco, λ) ya viven en `combined_controls_dock_` (un `QDockWidget` anclado a la derecha del `QMainWindow`) — es independiente de `tabs_`, así que al reemplazar `tabs_` el dock **sobrevive intacto** y cumple el rol del panel de propiedades. La barra superior (Open/Analyze/Export + `status_label_`) ya existe en `buildUi()` y se conserva; en Task B5 se le añade el atajo ⌘K (y opcionalmente un botón visible). No se crean clases `TopBar`/`PropertiesPanel` separadas para evitar reescribir cableado denso sin beneficio funcional.

**Files:**
- Modify: `src/ui/qt/MainWindow.h` (miembros: añadir riel + stack de secciones)
- Modify: `src/ui/qt/MainWindow.cpp` (`buildUi()` zona de tabs)

- [ ] **Step 1: Añadir miembros al header**

In `src/ui/qt/MainWindow.h`, after line `QTabWidget* tabs_{nullptr};` (line 118) add:
```cpp
    NavigationRail* nav_rail_{nullptr};
    QStackedWidget* section_stack_{nullptr};
```
And in the forward-declaration block (after line 47, inside `namespace beamlab::ui`) add:
```cpp
    class NavigationRail;
```

- [ ] **Step 2: Localizar y leer la zona de construcción de tabs**

Run:
```bash
grep -n "tabs_ = new QTabWidget\|tabs_->addTab\|tabs_->setCurrentIndex\|->addWidget(tabs_" src/ui/qt/MainWindow.cpp
```
Expected: ubica la creación de `tabs_`, las 14 llamadas `addTab`, y dónde `tabs_` se inserta en el layout. Anota esas líneas para los pasos siguientes.

- [ ] **Step 3: Sustituir el contenedor — construir riel + stack y agrupar páginas**

Replace the `tabs_ = new QTabWidget(...)` creation and the block of 14 `tabs_->addTab(...)` calls with the structure below. Keep every page-building statement that creates `dashboard_`, `combined_page_`, the statistics page, the plot pages, the CSV tables, `bio_sim_widget_`, `info_widget_` exactly as-is — only change where they get parented/added.

Add a helper near the top of `buildUi()` (or as a file-local lambda):

```cpp
    nav_rail_ = new NavigationRail;
    section_stack_ = new QStackedWidget;

    auto addSection = [&](const QString& glyph, const QString& label,
                          QWidget* page) {
        const int navIdx = nav_rail_->addItem(glyph, label);
        const int stackIdx = section_stack_->addWidget(page);
        Q_ASSERT(navIdx == stackIdx);
        return navIdx;
    };
```

Then, where the 14 tabs were added, build the 6 sections. For sections that previously had multiple tabs, wrap their pages in a small container with an inner selector. Concretely:

```cpp
    nav_rail_->addGroupHeader("ANÁLISIS");
    addSection("▦", "Overview", dashboard_);
    addSection("◉", "Scene 3D", combined_page_);
    addSection("◔", "Plots", buildPlotsSection());   // ver Task B6
    addSection("▤", "Data",  buildDataSection());     // ver Task B6
    nav_rail_->addGroupHeader("SIMULACIÓN");
    addSection("⚛", "BioSim", bio_sim_widget_);
    nav_rail_->addStretch();
    addSection("ⓘ", "Info & Docs", info_widget_);

    connect(nav_rail_, &NavigationRail::sectionChanged,
            section_stack_, &QStackedWidget::setCurrentIndex);
```

> En este paso, define `buildPlotsSection()` y `buildDataSection()` como stubs que devuelven un `QWidget*` placeholder (`return new QWidget;`) para que compile; se rellenan en Task B6. Declara ambos en `MainWindow.h` privados: `QWidget* buildPlotsSection(); QWidget* buildDataSection();`

- [ ] **Step 4: Insertar riel + stack en el layout del workspace**

Where `tabs_` was added to the workspace layout (found in Step 2), replace that with an HBox that places the rail left and the stack center:

```cpp
    auto* workspace_body = new QWidget;
    auto* body_layout = new QHBoxLayout(workspace_body);
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(0);
    body_layout->addWidget(nav_rail_);
    body_layout->addWidget(section_stack_, 1);
    // …añadir `workspace_body` donde antes iba `tabs_`.
```

Replace any remaining `tabs_->setCurrentIndex(n)` calls (e.g. in `loadManifest`, near line 2282) with `section_stack_->setCurrentIndex(1); nav_rail_->setCurrentIndex(1);` (Scene 3D is index 1).

- [ ] **Step 5: Compilar y lanzar**

Run:
```bash
cmake --build build-mac --target beamlab_ui -j$(sysctl -n hw.ncpu)
open build-mac/src/ui/BeamLabStudio.app
```
Expected: la ventana muestra el riel lateral con 6 ítems en vez de las 14 pestañas; al hacer clic cambia la página. Plots/Data muestran placeholder vacío (se completan en B6).

- [ ] **Step 6: Verificar regresión funcional**

Run: con la app abierta, Open + Analyze de `tests/muon_tracks.csv`.
Expected: el análisis corre, Scene 3D se activa y muestra el haz; el panel de foco se actualiza. (Si el flujo Analyze usaba `tabs_`, este paso lo revela.)

- [ ] **Step 7: Commit**

```bash
git add src/ui/qt/MainWindow.h src/ui/qt/MainWindow.cpp
git commit -m "feat(ui): replace 14-tab QTabWidget with NavigationRail + QStackedWidget"
```

---

### Task B5: Cablear la paleta ⌘K y registrar acciones

**Files:**
- Modify: `src/ui/qt/MainWindow.h` (miembros: registry + palette)
- Modify: `src/ui/qt/MainWindow.cpp` (`buildUi()` final: registrar acciones + QShortcut)

- [ ] **Step 1: Añadir miembros e includes**

In `src/ui/qt/MainWindow.h`, add includes near the top:
```cpp
#include "ui/qt/shell/ActionRegistry.h"
```
Forward-declare in the `beamlab::ui` namespace block:
```cpp
    class CommandPalette;
```
Add members:
```cpp
    ActionRegistry action_registry_{};
    CommandPalette* command_palette_{nullptr};
```

- [ ] **Step 2: Registrar acciones de navegación + archivo + export y crear el atajo**

At the end of `buildUi()` (after the rail/stack exist and `analysisPresenter_` is wired), add:

```cpp
    // Navegación
    const char* navGlyphs[] = {"Overview","Scene 3D","Plots","Data","BioSim","Info & Docs"};
    for (int i = 0; i < 6; ++i) {
        action_registry_.add({
            std::string("nav.") + std::to_string(i),
            std::string("Ir a ") + navGlyphs[i],
            "Navegación",
            "Cmd+" + std::to_string(i + 1),
            [this, i] { nav_rail_->setCurrentIndex(i); section_stack_->setCurrentIndex(i); }
        });
    }
    // Acciones de archivo / análisis / export (reusan los slots existentes).
    action_registry_.add({"file.open", "Abrir archivo y analizar…", "Archivo", "Cmd+O",
        [this]{ openDataFileAndRun(); }});
    action_registry_.add({"export.all", "Exportar todo", "Exportar", "",
        [this]{ exportAllArtifacts(); }});
    action_registry_.add({"export.mp4", "Exportar video MP4", "Exportar", "",
        [this]{ exportTrajectoryVideoMp4(); }});
    action_registry_.add({"export.png", "Exportar gráficos PNG", "Exportar", "",
        [this]{ exportPlotsPng(); }});
    action_registry_.add({"camera.horizontal", "Cámara: eje del haz horizontal", "Vista", "",
        [this]{ if (combined_scene_viewer_) combined_scene_viewer_->frameLongestAxisHorizontally(); }});

    command_palette_ = new CommandPalette(&action_registry_, this);
    auto* paletteShortcut = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(paletteShortcut, &QShortcut::activated, this, [this] {
        command_palette_->move(
            geometry().center() - QPoint(command_palette_->width() / 2, 160));
        command_palette_->show();
    });
```

Add the includes needed at the top of `MainWindow.cpp`:
```cpp
#include "ui/qt/shell/CommandPalette.h"
#include "ui/qt/shell/NavigationRail.h"
#include <QShortcut>
#include <QStackedWidget>
```

- [ ] **Step 3: Compilar y verificar ⌘K**

Run:
```bash
cmake --build build-mac --target beamlab_ui -j$(sysctl -n hw.ncpu)
open build-mac/src/ui/BeamLabStudio.app
```
Expected: pulsar ⌘K abre la paleta; escribir "scene" la filtra; Enter salta a Scene 3D; "horizontal" ejecuta la alineación de cámara.

- [ ] **Step 4: Commit**

```bash
git add src/ui/qt/MainWindow.h src/ui/qt/MainWindow.cpp
git commit -m "feat(ui): wire ⌘K command palette with navigation/file/export actions"
```

---

### Task B6: Secciones Plots y Data con selector interno

**Files:**
- Modify: `src/ui/qt/MainWindow.cpp` (implementar `buildPlotsSection()` y `buildDataSection()`)

**Contexto:** Las páginas de plots ya existen como widgets: `statistics_dashboard_` (dentro de un scroll), `trajectory_plot_page_`, `focal_slice_plot_page_`, `envelope_plot_page_`. Las tablas CSV: `trajectories_table_`, `focal_slice_table_`, `envelope_table_`. Esta tarea las agrupa bajo un `QComboBox` + `QStackedWidget` interno por sección, en lugar de placeholders.

- [ ] **Step 1: Implementar `buildPlotsSection()`**

Replace the stub with:

```cpp
QWidget* MainWindow::buildPlotsSection() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    auto* selector = new QComboBox(page);
    selector->setObjectName("SectionSelector");
    auto* inner = new QStackedWidget(page);
    auto add = [&](const QString& name, QWidget* w) {
        selector->addItem(name);
        inner->addWidget(w);
    };
    // statistics_page_ es el QScrollArea que envuelve statistics_dashboard_
    // (asignado en Step 3). Si por orden de construcción aún es nullptr,
    // cae a statistics_dashboard_ directamente.
    add("Statistics", statistics_page_ ? statistics_page_
                                       : static_cast<QWidget*>(statistics_dashboard_));
    add("Trajectories 2D", trajectory_plot_page_);
    add("Focal slice", focal_slice_plot_page_);
    add("Envelope rings", envelope_plot_page_);
    connect(selector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            inner, &QStackedWidget::setCurrentIndex);
    v->addWidget(selector);
    v->addWidget(inner, 1);
    return page;
}
```

> Importante: `statistics_dashboard_` vive dentro de un `QScrollArea` (construido más arriba, ~línea 1051). En Step 3 guardas ese scroll area en el miembro `statistics_page_` cuando se crea, y `buildPlotsSection()` debe llamarse **después** de esa construcción. Por eso el orden en `buildUi()` importa: construir la página de estadística antes de `addSection("◔", "Plots", buildPlotsSection())`.

- [ ] **Step 2: Implementar `buildDataSection()`**

```cpp
QWidget* MainWindow::buildDataSection() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    auto* selector = new QComboBox(page);
    selector->setObjectName("SectionSelector");
    auto* inner = new QStackedWidget(page);
    auto add = [&](const QString& name, QWidget* w) {
        selector->addItem(name);
        inner->addWidget(w);
    };
    add("Trajectories CSV", trajectories_table_);
    add("Focal slice CSV", focal_slice_table_);
    add("Envelope rings CSV", envelope_table_);
    connect(selector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            inner, &QStackedWidget::setCurrentIndex);
    v->addWidget(selector);
    v->addWidget(inner, 1);
    return page;
}
```

- [ ] **Step 3: Añadir includes y el miembro `statistics_page_`**

In `MainWindow.h` add member `QWidget* statistics_page_{nullptr};`. In `MainWindow.cpp`, where the statistics `QScrollArea` is created, assign it to `statistics_page_`. Add includes at top:
```cpp
#include <QComboBox>
```
(`QStackedWidget` y `QVBoxLayout` ya deberían estar incluidos; si no, añadirlos.)

- [ ] **Step 4: Compilar y verificar**

Run:
```bash
cmake --build build-mac --target beamlab_ui -j$(sysctl -n hw.ncpu)
open build-mac/src/ui/BeamLabStudio.app
```
Expected: Plots muestra un combo con 4 vistas que conmutan; Data muestra un combo con las 3 tablas CSV. Tras Analyze, las 4 gráficas y las 3 tablas se llenan.

- [ ] **Step 5: Commit**

```bash
git add src/ui/qt/MainWindow.h src/ui/qt/MainWindow.cpp
git commit -m "feat(ui): Plots and Data sections with inner selector"
```

---

### Task B7: QSS modernizado

**Files:**
- Modify: `src/ui/qt/styles/beamlabstudio.qss`

- [ ] **Step 1: Añadir estilos para los componentes nuevos**

Append to `src/ui/qt/styles/beamlabstudio.qss` (keep existing rules; add these at the end so they win on specificity):

```css
/* ── NavigationRail ─────────────────────────────────────────── */
QWidget#NavigationRail {
    background: #0E1622;
    border-right: 1px solid #2A3A55;
}
QLabel#NavGroupHeader {
    color: #5B6B86;
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 1px;
    padding: 12px 10px 4px 10px;
}
QToolButton#NavItem {
    text-align: left;
    border: 1px solid transparent;
    border-radius: 9px;
    padding: 9px 10px;
    color: #AAB8CF;
    font-weight: 600;
    background: transparent;
}
QToolButton#NavItem:hover {
    background: #162033;
    color: #EAF2FF;
}
QToolButton#NavItem:checked {
    background: #1E2A3F;
    border: 1px solid #2563EB;
    color: #EAF2FF;
}

/* ── CommandPalette ─────────────────────────────────────────── */
QDialog#CommandPalette {
    background: #111827;
    border: 1px solid #3B4D6B;
    border-radius: 14px;
}
QLineEdit#CommandSearch {
    background: #0E1622;
    border: 1px solid #3B4D6B;
    border-radius: 10px;
    padding: 10px 12px;
    font-size: 15px;
    color: #EAF2FF;
}
QLineEdit#CommandSearch:focus { border: 1px solid #67E8F9; }
QListWidget#CommandList {
    background: transparent;
    border: none;
    outline: none;
}
QListWidget#CommandList::item {
    padding: 9px 12px;
    border-radius: 8px;
    color: #AAB8CF;
}
QListWidget#CommandList::item:selected {
    background: #1E2A3F;
    color: #EAF2FF;
}

/* ── Section selector (Plots/Data) ──────────────────────────── */
QComboBox#SectionSelector {
    background: #162033;
    border: 1px solid #3B4D6B;
    border-radius: 9px;
    padding: 7px 12px;
    margin: 8px;
    color: #EAF2FF;
    font-weight: 600;
}
```

- [ ] **Step 2: Compilar, lanzar y revisar el look**

Run:
```bash
cmake --build build-mac --target beamlab_ui -j$(sysctl -n hw.ncpu)
open build-mac/src/ui/BeamLabStudio.app
```
Expected: el riel resalta la sección activa con borde azul; ⌘K muestra la paleta estilizada; los combos de Plots/Data se ven integrados.

- [ ] **Step 3: Commit**

```bash
git add src/ui/qt/styles/beamlabstudio.qss
git commit -m "style(ui): modern QSS for NavigationRail, CommandPalette and selectors"
```

---

### Task B8: Verificación final y CHANGELOG

**Files:**
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Suite completa de tests**

Run:
```bash
cmake --build build-mac --target beamlab_tests -j$(sysctl -n hw.ncpu)
./build-mac/tests/unit/beamlab_tests
```
Expected: 100% pasan (91 previos + 5 de ActionRegistry = 96).

- [ ] **Step 2: Regresión CLI**

Run: `./build-mac/beamlab -i tests/muon_tracks.csv -o /tmp/beamlab_ui_check 2>&1 | grep -iE "focus index|axial position|confidence="`
Expected: `Focus index: 264`, `z=11.6556 m`, `confidence=0.982…`.

- [ ] **Step 3: Empaque final y verificación del .app autocontenido**

Run: `./scripts/package_macos.sh`
Expected: genera `BeamLabStudio.dmg`; `otool -L` sin rutas `/opt/homebrew`.

- [ ] **Step 4: Smoke test manual del .app**

Run: `open build-release/src/ui/BeamLabStudio.app`
Expected (checklist): riel de 6 secciones cambia de página; ⌘K abre/filtra/ejecuta; Open+Analyze de `muon_tracks.csv` corre y Scene 3D muestra el haz horizontal; Plots y Data conmutan y se llenan.

- [ ] **Step 5: Actualizar CHANGELOG**

Add under `## [Unreleased]` → new `### Added` entries:
```markdown
- **UI rediseñada** — navegación por riel lateral de 6 secciones (Overview, Scene 3D, Plots, Data, BioSim, Info) en lugar de 14 pestañas; paleta de comandos ⌘K con búsqueda difusa (`ActionRegistry`); QSS modernizado
- **BeamLabStudio.app + DMG (macOS arm64)** — bundle autocontenido con frameworks Qt embebidos (`scripts/package_macos.sh`), icono `.icns` desde el SVG de marca, Info.plist
```

- [ ] **Step 6: Commit**

```bash
git add CHANGELOG.md
git commit -m "docs(changelog): UI redesign + macOS app/dmg"
```

---

## Resumen de archivos

**Nuevos:**
- `scripts/make_icns.sh`, `scripts/package_macos.sh`
- `resources/macos/Info.plist.in`, `resources/macos/AppIcon.icns`
- `src/ui/qt/shell/ActionRegistry.{h,cpp}`
- `src/ui/qt/shell/CommandPalette.{h,cpp}`
- `src/ui/qt/shell/NavigationRail.{h,cpp}`
- `tests/unit/ui/test_ActionRegistry.cpp`

**Modificados:**
- `src/ui/CMakeLists.txt`, `tests/unit/CMakeLists.txt`
- `src/ui/qt/MainWindow.{h,cpp}`
- `src/ui/qt/styles/beamlabstudio.qss`
- `CHANGELOG.md`
