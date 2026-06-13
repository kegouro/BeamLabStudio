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

} // namespace beamlab::ui
