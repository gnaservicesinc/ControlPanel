#pragma once
#include "actions.h"
#include "panelview.h"
#include <QMainWindow>

class QPlainTextEdit;
namespace cp {
// Shared action UI. This class has no document-opening or editing API.
class RuntimeWindow : public QMainWindow {
    Q_OBJECT
public:
    RuntimeWindow(const Panel &panel, const QString &directory, bool standalone = true, QWidget *parent = nullptr);
protected:
    void closeEvent(QCloseEvent *event) override;
    void showPanel(const Panel &panel, const QString &directory);
private:
    void showSupport();
    Panel currentPanel;
    QString workingDirectory;
    PanelView *view;
    ActionRunner *runner;
    QPlainTextEdit *activity;
};
void configureAppearance();
}
