#include "ui/qt/InfoWidget.h"

#include <QComboBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace beamlab::ui {

InfoWidget::InfoWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    loadDocs();
}

void InfoWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ─────────────────────────────────────────────────────────────
    auto* toolbar = new QWidget();
    toolbar->setStyleSheet("background:#141822; border-bottom: 1px solid #232A3A;");
    auto* tb_layout = new QHBoxLayout(toolbar);
    tb_layout->setContentsMargins(10, 6, 10, 6);
    tb_layout->setSpacing(10);

    auto* jump_label = new QLabel("Ir a módulo:");
    jump_label->setStyleSheet("color:#7CAFF2; font-weight:bold;");
    tb_layout->addWidget(jump_label);

    module_jump_ = new QComboBox();
    module_jump_->setMinimumWidth(280);
    module_jump_->setToolTip("Saltar directamente a un módulo matemático");
    tb_layout->addWidget(module_jump_);

    tb_layout->addStretch();

    source_label_ = new QLabel("Documentación generada desde bloques //!@math en el código fuente");
    source_label_->setStyleSheet("color:#4A6080; font-size:11px;");
    tb_layout->addWidget(source_label_);

    root->addWidget(toolbar);

    // ── Browser ─────────────────────────────────────────────────────────────
    browser_ = new QTextBrowser();
    browser_->setOpenLinks(false); // ancla interna manejada por nosotros
    browser_->setStyleSheet(
        "QTextBrowser {"
        "  background:#1A1E2A;"
        "  color:#C8D4E8;"
        "  border:none;"
        "  font-family:'Segoe UI',Arial,sans-serif;"
        "  font-size:13px;"
        "}"
        "QScrollBar:vertical {"
        "  background:#141822; width:8px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background:#2E3650; border-radius:4px;"
        "}"
    );
    root->addWidget(browser_, 1);

    connect(module_jump_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InfoWidget::jumpToModule);

    connect(browser_, &QTextBrowser::anchorClicked,
            this, [this](const QUrl& url) {
                browser_->scrollToAnchor(url.fragment());
            });
}

void InfoWidget::loadDocs()
{
    // Primary source: QRC (compiled into the binary from assets/math_docs.html)
    QFile res_file(":/docs/math_docs.html");
    if (!res_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        browser_->setHtml(
            "<h2 style='color:#F2997C;'>Documentación no disponible</h2>"
            "<p style='color:#C8D4E8;'>El archivo <code>math_docs.html</code> no está "
            "compilado en los recursos. Asegúrate de haber compilado con "
            "<code>-DBEAMLAB_ENABLE_QT_UI=ON</code> y de que Python 3 está disponible "
            "para que CMake ejecute <code>scripts/generate_math_docs.py</code>.</p>"
        );
        return;
    }

    const QString html = QString::fromUtf8(res_file.readAll());
    browser_->setHtml(html);

    // Populate jump combo from h2 id="mod-XXX" anchors
    module_jump_->blockSignals(true);
    module_jump_->clear();
    module_jump_->addItem("— Seleccionar módulo —", QString());

    const QRegularExpression re(
        R"rx(<h2\s+id="mod-([^"]+)"[^>]*>([^<]+)</h2>)rx");
    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString anchor = match.captured(1);
        const QString title  = match.captured(2);
        // Decode minimal HTML entities for display
        QString display = title;
        display.replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&");
        module_jump_->addItem(display, anchor);
    }

    module_jump_->blockSignals(false);
}

void InfoWidget::jumpToModule(int index)
{
    if (index <= 0) return;
    const QString anchor = module_jump_->itemData(index).toString();
    if (!anchor.isEmpty()) {
        browser_->scrollToAnchor("mod-" + anchor);
    }
}

} // namespace beamlab::ui
