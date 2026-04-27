#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>

class QGraphicsScene;
class QGraphicsView;
class QGridLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QResizeEvent;
class QSpinBox;

namespace beamlab::ui {

class StatsDashboardWidget final : public QWidget {
public:
    explicit StatsDashboardWidget(QWidget* parent = nullptr);

    void loadRunFromManifest(const QString& manifest_path);
    bool exportChartsPng(const QString& directory, QString* error = nullptr) const;
    bool exportStatisticsPdf(const QString& path, QString* error = nullptr) const;
    bool exportBeamRadiusSpreadsheet(const QString& path,
                                     QString* error = nullptr) const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Series {
        QVector<double> x{};
        QVector<double> y{};
        QString x_label{};
        QString y_label{};
        QString title{};
        QString note{};
        bool integer_y_ticks{false};
        bool highlight_minimum{true};
    };

    struct BeamRadiusRow {
        int source_row{0};
        int relative_to_minimum{0};
        QString position_label{};
        QString index{"n/a"};
        QString reference_value_m{"n/a"};
        QString axial_position_m{"n/a"};
        QString radius_rms_mm{"n/a"};
        QString minimum_radius_rms_mm{"n/a"};
        QString delta_from_minimum_mm{"n/a"};
        QString delta_from_minimum_percent{"n/a"};
        QString point_count{"n/a"};
        QString input_points{"n/a"};
        QString boundary_points{"n/a"};
        QString area_m2{"n/a"};
        QString perimeter_m{"n/a"};
        QString area_equivalent_radius_mm{"n/a"};
        QString perimeter_equivalent_radius_mm{"n/a"};
        QString method_name{"n/a"};
        QString valid{"n/a"};
        QString is_focus{"false"};
        QString state{"n/a"};
        double reference_numeric{0.0};
        double radius_mm_numeric{0.0};
        bool has_reference{false};
        bool has_radius{false};
        bool focus{false};
    };

    QString readFilePreview(const QString& path, int max_lines) const;
    QString resolveRunDirFromManifest(const QString& manifest_path) const;

    Series readCsvSeries(const QString& path,
                         const QStringList& possible_x_columns,
                         const QStringList& possible_y_columns,
                         const QString& title,
                         const QString& fallback_x_label,
                         const QString& fallback_y_label) const;
    QVector<double> readFocalSliceRadii(const QString& path) const;
    QVector<BeamRadiusRow> readBeamRadiusProfile(const QString& focus_curve_path,
                                                 const QString& envelope_summary_path) const;

    void drawSeries(QGraphicsScene* scene, const Series& series);
    void drawRadialHistogram(QGraphicsScene* scene, const QVector<double>& radii_mm);
    void fitCharts();
    void refreshBeamRadiusInspector();
    void exportBeamRadiusCsvFromDialog();

    QGraphicsView* focus_view_{nullptr};
    QGraphicsView* envelope_area_view_{nullptr};
    QGraphicsView* point_count_view_{nullptr};
    QGraphicsView* radial_histogram_view_{nullptr};

    QGraphicsScene* focus_scene_{nullptr};
    QGraphicsScene* envelope_area_scene_{nullptr};
    QGraphicsScene* point_count_scene_{nullptr};
    QGraphicsScene* radial_histogram_scene_{nullptr};

    QSpinBox* radius_offset_spin_{nullptr};
    QLabel* radius_position_label_{nullptr};
    QGridLayout* radius_detail_grid_{nullptr};
    QPushButton* export_radius_csv_button_{nullptr};
    QVector<BeamRadiusRow> beam_radius_rows_{};
    int beam_radius_focus_row_{-1};

    QPlainTextEdit* report_preview_{nullptr};
};

} // namespace beamlab::ui
