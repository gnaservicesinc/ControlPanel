#pragma once
#include "model.h"
#include <QPushButton>
#include <QWidget>

class QTabWidget;
class QLabel;

namespace cp {
class PanelButton : public QPushButton {
    Q_OBJECT
public:
    explicit PanelButton(const Button &button, QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return {60, 64}; }
    int heightForWidth(int width) const override;
protected:
    void paintEvent(QPaintEvent *) override;
};

class PanelView : public QWidget {
    Q_OBJECT
public:
    explicit PanelView(bool preview = false, QWidget *parent = nullptr);
    void setPanel(const Panel &panel);
    void setCompact(bool compact);
    int currentTab() const;
    void setCurrentTab(int index);
signals:
    void activated(const cp::Button &button);
private:
    bool preview;
    bool compact = false;
    QLabel *heading;
    QLabel *summary;
    QTabWidget *tabs;
};
}
