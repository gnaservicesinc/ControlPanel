#include "panelview.h"

#include <QApplication>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTextLayout>
#include <QTextDocument>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace cp {
namespace {
qreal textLayout(QPainter *painter, const QString &text, const QFont &font, int width, qreal top = 0) {
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setAlignment(Qt::AlignHCenter);
    layout.setTextOption(option);
    layout.beginLayout();
    qreal height = 0;
    while (true) {
        auto line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(std::max(1, width));
        line.setPosition({0, height});
        height += line.height();
    }
    layout.endLayout();
    if (painter) layout.draw(painter, {18, top});
    return height;
}

class ButtonGrid : public QWidget {
public:
    explicit ButtonGrid(QWidget *parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    }
    QList<PanelButton *> buttons;
protected:
    void resizeEvent(QResizeEvent *event) override { QWidget::resizeEvent(event); arrange(); }
    void showEvent(QShowEvent *event) override { QWidget::showEvent(event); arrange(); }
private:
    void arrange() {
        constexpr int margin = 20, gap = 14;
        const int available = std::max(60, width() - 2 * margin);
        const int columns = std::max(1, std::min({4, int(buttons.size()), (available + gap) / (210 + gap)}));
        const int cellWidth = (available - (columns - 1) * gap) / columns;
        int top = margin;
        for (int start = 0; start < buttons.size(); start += columns) {
            const int count = std::min(columns, int(buttons.size()) - start);
            int height = 76;
            for (int j = 0; j < count; ++j) height = std::max(height, buttons[start + j]->heightForWidth(cellWidth));
            const int left = margin + (available - count * cellWidth - (count - 1) * gap) / 2;
            for (int j = 0; j < count; ++j) buttons[start + j]->setGeometry(left + j * (cellWidth + gap), top, cellWidth, height);
            top += height + gap;
        }
        const int needed = top + margin - gap;
        if (minimumHeight() != needed) setMinimumHeight(needed);
    }
};
}

PanelButton::PanelButton(const Button &button, QWidget *parent) : QPushButton(button.name, parent) {
    setObjectName("panelButton");
    setAccessibleName(button.name);
    setAccessibleDescription(button.toolTip);
    setToolTip(button.toolTip.isEmpty() ? QString() : Qt::convertFromPlainText(button.toolTip));
    setCursor(Qt::PointingHandCursor);
    auto font = this->font(); font.setPointSizeF(font.pointSizeF() + 1); font.setWeight(QFont::DemiBold); setFont(font);
    auto policy = QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true); setSizePolicy(policy);
}

QSize PanelButton::sizeHint() const { return {220, heightForWidth(220)}; }
int PanelButton::heightForWidth(int width) const { return std::max(76, int(std::ceil(textLayout(nullptr, text(), font(), width - 36))) + 40); }
void PanelButton::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor background = palette().color(QPalette::Button);
    const auto windowColor = palette().color(QPalette::Window);
    const bool dark = windowColor.lightness() < 128;
    if (dark) background = windowColor.lighter(155);
    if (underMouse()) background = palette().color(QPalette::Highlight).lighter(175);
    if (isDown()) background = palette().color(QPalette::Highlight).lighter(135);
    const auto border = hasFocus() ? palette().color(QPalette::Highlight)
        : (dark ? windowColor.lighter(240) : palette().color(QPalette::Mid));
    painter.setPen(QPen(border, hasFocus() ? 2 : 1));
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF(rect()).adjusted(2, 2, -2, -2), 12, 12);
    painter.setPen(underMouse() || isDown() ? QColor("#142b41") : palette().color(QPalette::ButtonText));
    const qreal height = textLayout(nullptr, text(), font(), width() - 36);
    textLayout(&painter, text(), font(), width() - 36, (this->height() - height) / 2);
}

PanelView::PanelView(bool preview, QWidget *parent) : QWidget(parent), preview(preview) {
    setObjectName(preview ? "previewPanel" : "runtimePanel");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    heading = new QLabel;
    heading->setTextFormat(Qt::PlainText);
    heading->setWordWrap(true);
    auto font = heading->font(); font.setPointSize(23); font.setWeight(QFont::Bold); heading->setFont(font);
    summary = new QLabel;
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    tabs = new QTabWidget;
    tabs->setObjectName("panelTabs");
    tabs->setDocumentMode(true);
    tabs->setUsesScrollButtons(true);
    tabs->setElideMode(Qt::ElideNone);
    layout->addWidget(heading);
    layout->addWidget(summary);
    layout->addWidget(tabs, 1);
}

void PanelView::setPanel(const Panel &panel) {
    const int selected = currentTab();
    while (tabs->count()) { auto *page = tabs->widget(0); tabs->removeTab(0); delete page; }
    heading->setText(panel.name.isEmpty() ? "Untitled panel" : panel.name);
    int count = 0;
    for (const auto &tab : panel.tabs) count += tab.buttons.size();
    summary->setText(preview ? "Live preview · Actions are disabled" : QString("%1 tabs · %2 controls").arg(panel.tabs.size()).arg(count));
    for (const auto &tab : panel.tabs) {
        auto *scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        if (tab.buttons.isEmpty()) {
            auto *empty = new QLabel(preview ? "Add a button to this tab to see it here." : "This tab has no controls.");
            empty->setAlignment(Qt::AlignCenter); empty->setWordWrap(true); scroll->setWidget(empty);
        } else {
            auto *grid = new ButtonGrid;
            for (const auto &button : tab.buttons) {
                auto *control = new PanelButton(button, grid);
                grid->buttons.append(control);
                connect(control, &QPushButton::clicked, this, [this, button] { if (!preview) emit activated(button); });
            }
            scroll->setWidget(grid);
        }
        const auto label = tab.name.isEmpty() ? "Untitled tab" : tab.name;
        const int index = tabs->addTab(scroll, label);
        tabs->setTabToolTip(index, Qt::convertFromPlainText(label));
    }
    setCurrentTab(std::clamp(selected, 0, std::max(0, tabs->count() - 1)));
}
int PanelView::currentTab() const { return tabs->currentIndex(); }
void PanelView::setCurrentTab(int index) { tabs->setCurrentIndex(index); }
}
