#include "runtimewindow.h"
#include "embedded.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>

namespace cp {
void configureAppearance() {
    qApp->setStyleSheet("QLineEdit, QComboBox { min-height: 26px; } QToolBar { spacing: 8px; padding: 8px; } "
                       "QTreeWidget { border: none; } QTreeWidget::item { padding: 7px 3px; } "
                       "QStatusBar { min-height: 24px; } "
                       "QTabBar::tab { padding: 9px 15px; color: palette(window-text); background: palette(button); "
                       "border: 1px solid palette(mid); border-bottom: 2px solid transparent; } "
                       "QTabBar::tab:selected { border-bottom: 2px solid palette(highlight); }");
}

RuntimeWindow::RuntimeWindow(const Panel &panel, const QString &directory, bool standalone, QWidget *parent)
    : QMainWindow(parent) {
    resize(800, 580); setMinimumSize(340, 300);
    view = new PanelView(false); view->setCompact(standalone); setCentralWidget(view);
    runner = new ActionRunner(this);
    auto *file = menuBar()->addMenu("&File"); file->setObjectName("runtimeFileMenu");
    auto *close = file->addAction("&Close"); close->setShortcut(QKeySequence::Close);
    connect(close, &QAction::triggered, this, &QWidget::close);
    auto *quit = file->addAction("&Quit"); quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    auto *dock = new QDockWidget("Activity", this); dock->setObjectName("activityDock");
    activity = new QPlainTextEdit; activity->setReadOnly(true); activity->setMaximumBlockCount(500);
    activity->setObjectName("activityLog"); dock->setWidget(activity);
    addDockWidget(Qt::BottomDockWidgetArea, dock); dock->hide();
    menuBar()->addMenu("&View")->addAction(dock->toggleViewAction());
    auto *stop = menuBar()->addMenu("&Actions")->addAction("Stop Running Actions…");
    connect(stop, &QAction::triggered, this, [this] {
        if (runner->activeCount() == 0) { statusBar()->showMessage("No actions are running.", 5000); return; }
        if (QMessageBox::question(this, "Stop running actions?", "Stop the programs launched by this panel? Any work in progress in those programs may be interrupted.", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) runner->stopAll();
    });
    auto *help = menuBar()->addMenu("&Help");
    auto *support = help->addAction("Support Information…"); support->setObjectName("supportInformationAction");
    support->setShortcut(QKeySequence::HelpContents);
    connect(support, &QAction::triggered, this, &RuntimeWindow::showSupport);
    QString aboutName = standalone ? panel.name : "ControlPanel"; aboutName.replace('&', "&&");
    auto *about = help->addAction("About " + aboutName); about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, &RuntimeWindow::showSupport);
    auto *supportButton = new QPushButton("Help / Support"); supportButton->setObjectName("supportButton");
    supportButton->setToolTip("Find the app version, panel ID, and support contact. Copy them for an issue report.");
    statusBar()->addPermanentWidget(supportButton);
    connect(supportButton, &QPushButton::clicked, this, &RuntimeWindow::showSupport);
    connect(view, &PanelView::activated, this, [this](const Button &button) { runner->execute(button, workingDirectory); });
    connect(runner, &ActionRunner::started, this, [this](const QString &name) { statusBar()->showMessage("Running “" + name + "”…"); });
    connect(runner, &ActionRunner::finished, this, [this, dock](const QString &name, bool ok, const QString &message) {
        const auto title = (ok ? "Completed: " : "Failed: ") + name;
        activity->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + title + "\n" + message + "\n");
        statusBar()->showMessage(title + " — " + message.section('\n', 0, 0), 15000);
        if (!ok) dock->show();
    });
    // Absolute programs remain runnable after an author removes their source
    // folder. Existing working folders retain their original relative-arg behavior.
    showPanel(panel, standalone && !QFileInfo(directory).isDir() ? QDir::homePath() : directory);
}

void RuntimeWindow::showPanel(const Panel &panel, const QString &directory) {
    currentPanel = panel; workingDirectory = directory;
    view->setPanel(panel); setWindowTitle(panel.name);
}

void RuntimeWindow::showSupport() {
    QDialog dialog(this); dialog.setObjectName("supportDialog");
    dialog.setWindowTitle("Support Information — " + currentPanel.name); dialog.resize(560, 410);
    auto *layout = new QVBoxLayout(&dialog);
    auto *hint = new QLabel("Reporting a problem? Copy these details into your message so support can identify your control panel.");
    hint->setWordWrap(true); layout->addWidget(hint);
    const auto details = supportInformation(currentPanel);
    auto *text = new QPlainTextEdit(details); text->setReadOnly(true); text->setObjectName("supportDetails");
    text->setAccessibleName("App name, version, panel ID, and support contact"); layout->addWidget(text);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *copy = buttons->addButton("Copy Support Information", QDialogButtonBox::ActionRole);
    copy->setObjectName("copySupportInformation");
    connect(copy, &QPushButton::clicked, &dialog, [copy, details] {
        QApplication::clipboard()->setText(details); copy->setText("Copied to Clipboard");
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons); dialog.exec();
}

void RuntimeWindow::closeEvent(QCloseEvent *event) {
    if (runner->activeCount() > 0) {
        QMessageBox::warning(this, "Actions are still running", "Wait for the running actions to finish, or use Actions → Stop Running Actions before closing. Their output is available in View → Activity.");
        event->ignore();
    } else event->accept();
}
}
