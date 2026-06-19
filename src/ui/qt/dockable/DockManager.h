#pragma once

#include "ui/qt/dockable/IDockableWidget.h"

#include <QObject>
#include <memory>
#include <unordered_map>
#include <vector>

class QDockWidget;
class QMainWindow;
class QMenu;
QT_BEGIN_NAMESPACE class QAction; QT_END_NAMESPACE

namespace beamlab::ui::qt {

/// Manages a set of dockable widgets inside a QMainWindow.
///
/// Each registered IDockableWidget is wrapped in a QDockWidget and
/// added to the main window.  The View menu is auto-generated from
/// the registered dockables so new widgets appear automatically.
///
/// Layout persistence uses QMainWindow::saveState() / restoreState()
/// and QSettings under the key "ui/dock_layout".
class DockManager : public QObject {
    Q_OBJECT

public:
    explicit DockManager(QMainWindow* mainWindow);

    /// Register a dockable and wrap it in a QDockWidget.
    /// Called once per widget during startup.
    void registerDockable(std::unique_ptr<IDockableWidget> dockable);

    /// Restore a previously saved layout.
    void restoreLayout(const QByteArray& state);

    /// Serialise the current layout for persistence.
    [[nodiscard]] QByteArray saveLayout() const;

    /// Build a "View" menu containing a checkable action for each
    /// registered dockable.  Actions toggle visibility.
    [[nodiscard]] QMenu* createViewMenu();

    /// Show or hide a dockable by its persistent id.
    void toggleDockable(const QString& id);

    /// Reset all docks to their default area and visibility.
    void resetToDefaults();

    /// Save layout to QSettings.
    void saveToSettings();
    /// Restore layout from QSettings.
    void restoreFromSettings();

private:
    QMainWindow* mainWindow_;

    std::vector<std::unique_ptr<IDockableWidget>> dockables_;
    std::unordered_map<QString, QDockWidget*> dockWidgets_;

    // QMainWindow's saveState() uses object names, so we store the mapping.
    static constexpr const char* kSettingsGroup = "ui/dock_layout";
};

} // namespace beamlab::ui::qt
