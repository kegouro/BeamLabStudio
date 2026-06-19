#pragma once

#include <QWidget>

class QSplitter;

namespace beamlab::ui {

class AnalysisPresenter;
class Scene3DWidget;
class StatsDashboardWidget;
class RunDashboardWidget;

/// Primary analysis tab — 3D viewport + statistics dock.
///
/// Layout:
///   ┌──────────────────────────────────────────────────────────────┐
///   │                                                              │
///   │                    Scene3DWidget                             │
///   │                 (QPainter 3D viewport)                       │
///   │                                                              │
///   ├───────────────────────┬──────────────────────────────────────┤
///   │  RunDashboardWidget   │     StatsDashboardWidget             │
///   │  (runs, manifests)    │     (beam radius, focus, histograms) │
///   └───────────────────────┴──────────────────────────────────────┘
///
class AnalysisView final : public QWidget {
    Q_OBJECT

public:
    explicit AnalysisView(AnalysisPresenter* presenter,
                          QWidget* parent = nullptr);

    void setPresenter(AnalysisPresenter* presenter);

private:
    void setupLayout();
    void connectSignals();

    AnalysisPresenter* presenter_{nullptr};

    Scene3DWidget*        scene3D_{nullptr};
    StatsDashboardWidget* statsDashboard_{nullptr};
    RunDashboardWidget*   runDashboard_{nullptr};
    QSplitter*            verticalSplitter_{nullptr};
};

} // namespace beamlab::ui
