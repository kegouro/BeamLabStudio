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

void NavigationRail::addStretch() { layout_->addStretch(1); }

void NavigationRail::setCurrentIndex(int index) {
    if (auto* btn = group_->button(index)) btn->setChecked(true);
}

int NavigationRail::currentIndex() const {
    return group_->checkedId();
}

} // namespace beamlab::ui
