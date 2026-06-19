#include "ui/qt/dockable/DockManager.h"

#include <QAction>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QSettings>

namespace beamlab::ui::qt {

// ── Constructor ────────────────────────────────────────────────────

DockManager::DockManager(QMainWindow* mainWindow)
    : QObject(mainWindow)
    , mainWindow_(mainWindow)
{
}

// ── Registration ───────────────────────────────────────────────────

void DockManager::registerDockable(std::unique_ptr<IDockableWidget> dockable)
{
    auto id    = dockable->id();
    auto title = dockable->title();

    auto* dock = new QDockWidget(title, mainWindow_);
    dock->setObjectName(id);  // Required for QMainWindow::saveState()
    dock->setWidget(dockable->widget());

    dock->setVisible(dockable->initiallyVisible());

    mainWindow_->addDockWidget(dockable->preferredArea(), dock);

    dockWidgets_[id] = dock;
    dockables_.push_back(std::move(dockable));
}

// ── View menu ──────────────────────────────────────────────────────

QMenu* DockManager::createViewMenu()
{
    auto* menu = new QMenu("&Docks", mainWindow_);
    menu->setToolTip("Toggle dock panels");

    for (const auto& d : dockables_) {
        auto* dock = dockWidgets_.at(d->id());

        auto* action = menu->addAction(d->title());
        action->setCheckable(true);
        action->setChecked(dock->isVisible());
        if (!d->shortcut().isEmpty())
            action->setShortcut(d->shortcut());

        // Keep action synced with dock visibility.
        connect(action, &QAction::toggled, dock, &QWidget::setVisible);
        connect(dock, &QDockWidget::visibilityChanged,
                action, &QAction::setChecked);
    }

    menu->addSeparator();
    menu->addAction("Reset to defaults", this, &DockManager::resetToDefaults);

    return menu;
}

// ── Toggle ─────────────────────────────────────────────────────────

void DockManager::toggleDockable(const QString& id)
{
    auto it = dockWidgets_.find(id);
    if (it != dockWidgets_.end()) {
        it->second->setVisible(!it->second->isVisible());
    }
}

// ── Layout save / restore ──────────────────────────────────────────

QByteArray DockManager::saveLayout() const
{
    return mainWindow_->saveState();
}

void DockManager::restoreLayout(const QByteArray& state)
{
    mainWindow_->restoreState(state);
}

void DockManager::saveToSettings()
{
    QSettings s("BeamLabStudio", "BeamLabStudio");
    s.setValue(kSettingsGroup, saveLayout());
}

void DockManager::restoreFromSettings()
{
    QSettings s("BeamLabStudio", "BeamLabStudio");
    auto state = s.value(kSettingsGroup).toByteArray();
    if (!state.isEmpty()) {
        restoreLayout(state);
    }
}

// ── Reset ──────────────────────────────────────────────────────────

void DockManager::resetToDefaults()
{
    for (const auto& d : dockables_) {
        auto* dock = dockWidgets_.at(d->id());
        mainWindow_->removeDockWidget(dock);
        mainWindow_->addDockWidget(d->preferredArea(), dock);
        dock->setVisible(d->initiallyVisible());
    }
}

} // namespace beamlab::ui::qt
