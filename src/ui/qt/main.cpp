#include "ui/qt/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPalette>
#include <QStyleFactory>

namespace {

QString chooseFontFamily(const QStringList& candidates, const QString& fallback)
{
    const QStringList families = QFontDatabase::families();

    for (const auto& candidate : candidates) {
        if (families.contains(candidate, Qt::CaseInsensitive)) {
            return candidate;
        }
    }

    return fallback;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("BeamLabStudio");
    QApplication::setApplicationDisplayName("BeamLabStudio");
    QApplication::setOrganizationName("José Labarca");
    app.setWindowIcon(QIcon(":/branding/beamlabstudio_icon.svg"));

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    const QString ui_font = chooseFontFamily(
        {"Inter", "Noto Sans", "Cantarell", "Segoe UI", "DejaVu Sans"},
        app.font().family()
    );
    const QString mono_font = chooseFontFamily(
        {"JetBrains Mono", "Cascadia Mono", "Fira Code", "Noto Sans Mono", "DejaVu Sans Mono"},
        "monospace"
    );

    QFont app_font(ui_font, 10);
    app_font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(app_font);

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(20, 24, 32));
    palette.setColor(QPalette::WindowText, QColor(232, 238, 248));
    palette.setColor(QPalette::Base, QColor(13, 16, 22));
    palette.setColor(QPalette::AlternateBase, QColor(24, 29, 38));
    palette.setColor(QPalette::ToolTipBase, QColor(232, 238, 248));
    palette.setColor(QPalette::ToolTipText, QColor(20, 24, 32));
    palette.setColor(QPalette::Text, QColor(232, 238, 248));
    palette.setColor(QPalette::Button, QColor(34, 41, 54));
    palette.setColor(QPalette::ButtonText, QColor(232, 238, 248));
    palette.setColor(QPalette::BrightText, QColor(255, 120, 120));
    palette.setColor(QPalette::Highlight, QColor(80, 135, 240));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));

    app.setPalette(palette);

    app.setStyleSheet(QString(R"(
        QMainWindow {
            background-color: #141820;
        }

        QWidget {
            font-family: "%1";
            font-size: 14px;
        }

        QMenuBar {
            background-color: #141820;
            color: #e8eef8;
            border-bottom: 1px solid #30384a;
        }

        QPushButton {
            background-color: #22304a;
            color: #e8eef8;
            border: 1px solid #3f5f9f;
            border-radius: 6px;
            padding: 8px 12px;
        }

        QPushButton:hover {
            background-color: #2f4370;
        }

        QPushButton:disabled {
            background-color: #1a2130;
            color: #7f8ca3;
            border-color: #2b3548;
        }

        QLabel {
            color: #e8eef8;
        }

        QGroupBox {
            color: #e8eef8;
            border: 1px solid #293244;
            border-radius: 6px;
            margin-top: 12px;
            padding: 12px 10px 10px 10px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: #c7d4e8;
        }

        QTabWidget::pane {
            border: 1px solid #30384a;
            background-color: #0d1016;
            border-radius: 6px;
        }

        QTabBar::tab {
            background-color: #222936;
            color: #dce7f7;
            padding: 8px 14px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            margin-right: 2px;
        }

        QTabBar::tab:selected {
            background-color: #385d9f;
            color: white;
        }

        QPlainTextEdit, QTableWidget {
            background-color: #0d1016;
            color: #e8eef8;
            border: 1px solid #30384a;
            border-radius: 6px;
            selection-background-color: #385d9f;
            font-family: "%2";
        }

        QComboBox, QSpinBox {
            background-color: #202838;
            color: #e8eef8;
            border: 1px solid #3a465c;
            border-radius: 5px;
            padding: 4px 8px;
        }

        QSlider::groove:horizontal {
            height: 5px;
            background: #253149;
            border-radius: 2px;
        }

        QSlider::handle:horizontal {
            width: 13px;
            margin: -5px 0;
            border-radius: 4px;
            background: #93b7ff;
            border: 1px solid #526d9e;
        }

        QHeaderView::section {
            background-color: #222936;
            color: #e8eef8;
            border: 1px solid #30384a;
            padding: 6px;
        }
    )").arg(ui_font, mono_font));

    QFile brand_style(":/styles/beamlabstudio.qss");

    if (brand_style.open(QFile::ReadOnly)) {
        app.setStyleSheet(
            app.styleSheet() +
            "\n" +
            QString::fromUtf8(brand_style.readAll())
        );
    }

    beamlab::ui::MainWindow window;
    window.show();

    return app.exec();
}
