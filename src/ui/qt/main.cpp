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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
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
