#pragma once
#include "actions.h"
#include "panelview.h"
#include "runtimewindow.h"
#include <QMainWindow>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTreeWidget;
class QTimer;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace cp {
class InterpreterWindow : public RuntimeWindow {
    Q_OBJECT
public:
    explicit InterpreterWindow(QWidget *parent = nullptr);
    bool openFile(const QString &path);
private:
    QString filePath;
};

class CreatorWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit CreatorWindow(QWidget *parent = nullptr);
    bool openFile(const QString &path);
    bool saveTo(const QString &path);
    const Panel &document() const { return panel; }
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    bool maybeSave();
    bool save(bool saveAs = false);
    void rebuildTree(int tab, int button = -1);
    void selectItem();
    void editButton();
    void changed();
    void refreshPreview();
    void addTab();
    void addButton();
    void duplicateItem();
    void removeItem();
    void moveItem(int delta);
    void previewWindow();
    void updateTitle();
    void exportApp();
    void appExportSettings();
    Panel panel;
    QString filePath;
    bool dirty = false;
    bool updating = false;
    int tabIndex = -1;
    int buttonIndex = -1;
    QTreeWidget *tree;
    QLineEdit *panelName;
    QLineEdit *tabName;
    QLineEdit *buttonName;
    QLineEdit *path;
    QLineEdit *args;
    QLineEdit *value;
    QPlainTextEdit *toolTip;
    QComboBox *action;
    QStackedWidget *properties;
    PanelView *preview;
    QLabel *validation;
    QTimer *previewTimer;
};
void configureAppearance();
}
