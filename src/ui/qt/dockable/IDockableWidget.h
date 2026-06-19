#pragma once

#include <QKeySequence>
#include <QString>
#include <QWidget>

#include <Qt>

namespace beamlab::ui::qt {

/// Interface for widgets that can be hosted inside a QDockWidget.
///
/// Implementations define display metadata (title, id, preferred area)
/// and return the actual QWidget to embed via widget().
class IDockableWidget {
public:
    virtual ~IDockableWidget() = default;

    /// Display title shown on the dock window and in the View menu.
    [[nodiscard]] virtual QString title() const = 0;

    /// Persistent unique identifier (used by QMainWindow::saveState()).
    [[nodiscard]] virtual QString id() const = 0;

    /// The widget to embed inside the QDockWidget.
    [[nodiscard]] virtual QWidget* widget() = 0;

    /// Preferred docking area when first shown.
    [[nodiscard]] virtual Qt::DockWidgetArea preferredArea() const
    {
        return Qt::RightDockWidgetArea;
    }

    /// Whether the dock should be visible on first launch.
    [[nodiscard]] virtual bool initiallyVisible() const { return true; }

    /// Optional keyboard shortcut to toggle visibility.
    [[nodiscard]] virtual QKeySequence shortcut() const { return {}; }
};

} // namespace beamlab::ui::qt
