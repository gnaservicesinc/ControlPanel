#include "windows.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace cp {
namespace {
const QString panelFilter = "Control panels (*.controlpanel *.xml);;All files (*)";
QLabel *label(const QString &text) {
    auto *widget = new QLabel(text);
    widget->setTextFormat(Qt::PlainText); widget->setWordWrap(true); return widget;
}
QAction *menuAction(QMenu *menu, const QString &title, const QKeySequence &key, const std::function<void()> &fn) {
    auto *action = menu->addAction(title); action->setShortcut(key);
    QObject::connect(action, &QAction::triggered, menu, fn); return action;
}
void errorDialog(QWidget *parent, const QString &title, const QString &message) {
    QMessageBox box(QMessageBox::Warning, title, message, QMessageBox::Ok, parent);
    box.setTextFormat(Qt::PlainText); box.exec();
}
void about(QWidget *parent) {
    QMessageBox::about(parent, "About ControlPanel", "ControlPanel " CP_VERSION "\nCreate and run your own tabbed control panels.\n\nGNU GPL v3. See the included LICENSE file.");
}
QWidget *propertyPage(QFormLayout **form) {
    auto *page = new QWidget;
    *form = new QFormLayout(page);
    (*form)->setRowWrapPolicy(QFormLayout::WrapAllRows);
    (*form)->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    (*form)->setVerticalSpacing(9);
    return page;
}
}

void configureAppearance() {
    qApp->setStyleSheet("QLineEdit, QComboBox { min-height: 26px; } QToolBar { spacing: 8px; padding: 8px; } "
                       "QTreeWidget { border: none; } QTreeWidget::item { padding: 7px 3px; } "
                       "QStatusBar { min-height: 24px; } "
                       "QTabBar::tab { padding: 9px 15px; color: palette(window-text); background: palette(button); "
                       "border: 1px solid palette(mid); border-bottom: 2px solid transparent; } "
                       "QTabBar::tab:selected { border-bottom: 2px solid palette(highlight); }");
}

InterpreterWindow::InterpreterWindow(QWidget *parent) : QMainWindow(parent) {
    resize(800, 580); setMinimumSize(340, 300);
    setWindowTitle("ControlPanel Interpreter");
    view = new PanelView(false); setCentralWidget(view);
    view->setPanel({"ControlPanel Interpreter", {{"Welcome", {}}}});
    runner = new ActionRunner(this);
    auto *file = menuBar()->addMenu("&File");
    menuAction(file, "&Open Panel…", QKeySequence::Open, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Open a control panel", filePath, panelFilter);
        if (!path.isEmpty()) openFile(path);
    });
    menuAction(file, "&Reload Panel", QKeySequence("Ctrl+R"), [this] { if (!filePath.isEmpty()) openFile(filePath); });
    file->addSeparator();
    menuAction(file, "&Close", QKeySequence::Close, [this] { close(); });
    menuAction(file, "&Quit", QKeySequence::Quit, [this] { close(); });
    auto *dock = new QDockWidget("Activity", this);
    dock->setObjectName("activityDock");
    activity = new QPlainTextEdit; activity->setReadOnly(true); activity->setMaximumBlockCount(500);
    activity->setObjectName("activityLog");
    dock->setWidget(activity); addDockWidget(Qt::BottomDockWidgetArea, dock); dock->hide();
    auto *viewMenu = menuBar()->addMenu("&View"); viewMenu->addAction(dock->toggleViewAction());
    auto *actionsMenu = menuBar()->addMenu("&Actions");
    menuAction(actionsMenu, "Stop Running Actions…", {}, [this] {
        if (runner->activeCount() == 0) { statusBar()->showMessage("No actions are running.", 5000); return; }
        if (QMessageBox::question(this, "Stop running actions?", "Stop the programs launched by this panel? Any work in progress in those programs may be interrupted.", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) runner->stopAll();
    });
    menuAction(menuBar()->addMenu("&Help"), "About ControlPanel", {}, [this] { about(this); });
    connect(view, &PanelView::activated, this, [this](const Button &button) {
        runner->execute(button, QFileInfo(filePath).absolutePath());
    });
    connect(runner, &ActionRunner::started, this, [this](const QString &name) { statusBar()->showMessage("Running “" + name + "”…"); });
    connect(runner, &ActionRunner::finished, this, [this, dock](const QString &name, bool ok, const QString &message) {
        const auto title = (ok ? "Completed: " : "Failed: ") + name;
        activity->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + title + "\n" + message + "\n");
        statusBar()->showMessage(title + " — " + message.section('\n', 0, 0), 15000);
        if (!ok) dock->show();
    });
    statusBar()->showMessage("Open a .controlpanel or XML file to begin. Only open panels you trust.");
}

bool InterpreterWindow::openFile(const QString &path) {
    Panel candidate; QString error;
    if (!loadPanel(path, &candidate, &error)) { errorDialog(this, "Could not open panel", error); return false; }
    filePath = QFileInfo(path).absoluteFilePath();
    view->setPanel(candidate);
    setWindowTitle(candidate.name);
    setWindowFilePath(filePath);
    statusBar()->showMessage("Loaded " + QFileInfo(path).fileName() + ". Click a control to run its action.");
    return true;
}

void InterpreterWindow::closeEvent(QCloseEvent *event) {
    if (runner->activeCount() > 0) {
        errorDialog(this, "Actions are still running", "Wait for the running actions to finish, or use Actions → Stop Running Actions before closing. Their output is available in View → Activity.");
        event->ignore();
    } else event->accept();
}

CreatorWindow::CreatorWindow(QWidget *parent) : QMainWindow(parent), panel(newPanel()) {
    resize(1220, 760); setMinimumSize(820, 500);
    auto *file = menuBar()->addMenu("&File");
    auto *newAction = menuAction(file, "&New Panel", QKeySequence::New, [this] {
        if (!maybeSave()) return;
        panel = newPanel(); filePath.clear(); dirty = false;
        panelName->setText(panel.name); rebuildTree(0); refreshPreview(); updateTitle();
    });
    auto *openAction = menuAction(file, "&Open…", QKeySequence::Open, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Open a control panel", filePath, panelFilter);
        if (!path.isEmpty()) openFile(path);
    });
    auto *saveAction = menuAction(file, "&Save", QKeySequence::Save, [this] { save(); });
    menuAction(file, "Save &As…", QKeySequence::SaveAs, [this] { save(true); });
    auto *exportAction = menuAction(file, "&Export for Interpreter…", QKeySequence("Ctrl+E"), [this] { save(true); });
    file->addSeparator(); menuAction(file, "&Close", QKeySequence::Close, [this] { close(); });
    menuAction(file, "&Quit", QKeySequence::Quit, [this] { close(); });
    auto *viewMenu = menuBar()->addMenu("&View");
    auto *previewAction = menuAction(viewMenu, "Preview Window", QKeySequence("Ctrl+P"), [this] { previewWindow(); });
    menuAction(menuBar()->addMenu("&Help"), "About ControlPanel", {}, [this] { about(this); });
    auto *toolbar = addToolBar("Document"); toolbar->setMovable(false);
    toolbar->addAction(newAction); toolbar->addAction(openAction); toolbar->addAction(saveAction);
    toolbar->addSeparator(); toolbar->addAction(previewAction); toolbar->addAction(exportAction);

    auto *splitter = new QSplitter; setCentralWidget(splitter);
    auto *sidebar = new QWidget; sidebar->setMinimumWidth(185);
    auto *sideLayout = new QVBoxLayout(sidebar); sideLayout->setContentsMargins(14, 14, 14, 14);
    sideLayout->addWidget(label("PANEL STRUCTURE"));
    tree = new QTreeWidget; tree->setObjectName("structureTree"); tree->setHeaderHidden(true);
    tree->setAccessibleName("Tabs and buttons"); sideLayout->addWidget(tree, 1);
    auto *addRow = new QHBoxLayout;
    auto *addTabButton = new QPushButton("+ Tab"); addTabButton->setObjectName("addTab");
    auto *addControl = new QPushButton("+ Button"); addControl->setObjectName("addButton");
    addRow->addWidget(addTabButton); addRow->addWidget(addControl); sideLayout->addLayout(addRow);
    auto *orderRow = new QHBoxLayout;
    auto *up = new QPushButton("↑"); up->setAccessibleName("Move up"); up->setToolTip("Move selected item up");
    auto *down = new QPushButton("↓"); down->setAccessibleName("Move down"); down->setToolTip("Move selected item down");
    auto *duplicate = new QPushButton("Duplicate"); duplicate->setObjectName("duplicateItem");
    orderRow->addWidget(up); orderRow->addWidget(down); orderRow->addWidget(duplicate); sideLayout->addLayout(orderRow);
    auto *remove = new QPushButton("Remove selected"); remove->setObjectName("removeItem"); sideLayout->addWidget(remove);
    splitter->addWidget(sidebar);

    auto *editor = new QWidget; editor->setMinimumWidth(280);
    auto *editorLayout = new QVBoxLayout(editor); editorLayout->setContentsMargins(12, 14, 12, 14);
    editorLayout->addWidget(label("PANEL NAME"));
    panelName = new QLineEdit(panel.name); panelName->setObjectName("panelName"); panelName->setAccessibleName("Panel name");
    editorLayout->addWidget(panelName);
    properties = new QStackedWidget;
    QFormLayout *tabForm;
    auto *tabPage = propertyPage(&tabForm);
    tabName = new QLineEdit; tabName->setObjectName("tabName"); tabForm->addRow("Tab name", tabName);
    tabForm->addRow(label("Tabs group related controls. Select a button in the structure to edit its action."));
    properties->addWidget(tabPage);
    QFormLayout *buttonForm;
    auto *buttonPage = propertyPage(&buttonForm);
    buttonName = new QLineEdit; buttonName->setObjectName("buttonName"); buttonForm->addRow("Button label", buttonName);
    action = new QComboBox; action->setObjectName("buttonAction"); action->addItems({"Touch a file", "Increment a stored number", "Run a program or script"});
    buttonForm->addRow("Action", action);
    auto *pathRow = new QWidget; auto *pathLayout = new QHBoxLayout(pathRow); pathLayout->setContentsMargins(0, 0, 0, 0);
    path = new QLineEdit; path->setObjectName("buttonPath"); path->setAccessibleName("File path"); path->setPlaceholderText("/path/to/file or ~/file");
    auto *browse = new QPushButton("…"); browse->setMaximumWidth(36); browse->setAccessibleName("Browse for file");
    pathLayout->addWidget(path, 1); pathLayout->addWidget(browse); buttonForm->addRow("File path", pathRow);
    args = new QLineEdit; args->setObjectName("buttonArgs"); args->setPlaceholderText("47 \"two words\""); buttonForm->addRow("Arguments (run only)", args);
    value = new QLineEdit; value->setObjectName("buttonValue"); value->setPlaceholderText("1 (default)"); buttonForm->addRow("Increment (storeincrement only)", value);
    toolTip = new QPlainTextEdit; toolTip->setObjectName("buttonToolTip"); toolTip->setMaximumHeight(100); buttonForm->addRow("Tooltip (optional)", toolTip);
    buttonForm->addRow(label("Relative file paths start in the saved panel’s folder. Quote arguments containing spaces. Commands run without a shell."));
    properties->addWidget(buttonPage);
    auto *scroll = new QScrollArea; scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true); scroll->setWidget(properties);
    editorLayout->addWidget(scroll, 1);
    validation = label(""); validation->setObjectName("validationMessage"); editorLayout->addWidget(validation);
    splitter->addWidget(editor);
    preview = new PanelView(true); preview->setMinimumWidth(280); splitter->addWidget(preview);
    splitter->setSizes({220, 340, 660}); splitter->setCollapsible(0, false); splitter->setCollapsible(1, false); splitter->setCollapsible(2, false);
    splitter->setStretchFactor(2, 1);

    previewTimer = new QTimer(this); previewTimer->setSingleShot(true); previewTimer->setInterval(120);
    connect(previewTimer, &QTimer::timeout, this, &CreatorWindow::refreshPreview);
    connect(tree, &QTreeWidget::currentItemChanged, this, [this] { selectItem(); });
    connect(addTabButton, &QPushButton::clicked, this, &CreatorWindow::addTab);
    connect(addControl, &QPushButton::clicked, this, &CreatorWindow::addButton);
    connect(duplicate, &QPushButton::clicked, this, &CreatorWindow::duplicateItem);
    connect(remove, &QPushButton::clicked, this, &CreatorWindow::removeItem);
    connect(up, &QPushButton::clicked, this, [this] { moveItem(-1); });
    connect(down, &QPushButton::clicked, this, [this] { moveItem(1); });
    connect(panelName, &QLineEdit::textEdited, this, [this](const QString &text) { panel.name = text; changed(); });
    connect(tabName, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (updating || tabIndex < 0) return;
        panel.tabs[tabIndex].name = text; tree->topLevelItem(tabIndex)->setText(0, text); changed();
    });
    for (auto *field : {buttonName, path, args, value}) connect(field, &QLineEdit::textEdited, this, &CreatorWindow::editButton);
    connect(toolTip, &QPlainTextEdit::textChanged, this, &CreatorWindow::editButton);
    connect(action, &QComboBox::currentIndexChanged, this, [this] {
        if (updating) return;
        if (action->currentIndex() != int(Action::Run)) args->clear();
        if (action->currentIndex() != int(Action::StoreIncrement)) value->clear();
        editButton();
    });
    connect(browse, &QPushButton::clicked, this, [this] {
        QString chosen;
        const auto base = filePath.isEmpty() ? QDir::homePath() : QFileInfo(filePath).absolutePath();
        const auto initial = path->text().isEmpty() ? base : resolvePath(path->text(), base);
        if (action->currentIndex() == int(Action::Run)) chosen = QFileDialog::getOpenFileName(this, "Choose a program or executable script", initial);
        else chosen = QFileDialog::getSaveFileName(this, "Choose the file this action will update", initial, "All files (*)", nullptr, QFileDialog::DontConfirmOverwrite);
        if (!chosen.isEmpty()) { path->setText(chosen); editButton(); }
    });
    rebuildTree(0); refreshPreview(); updateTitle();
}

void CreatorWindow::rebuildTree(int tab, int button) {
    updating = true; tree->clear();
    for (const auto &entry : panel.tabs) {
        auto *item = new QTreeWidgetItem(tree, {entry.name});
        for (const auto &control : entry.buttons) new QTreeWidgetItem(item, {control.name});
        item->setExpanded(true);
    }
    auto *selected = tree->topLevelItem(tab);
    if (selected && button >= 0) selected = selected->child(button);
    tree->setCurrentItem(selected); updating = false; selectItem();
}
void CreatorWindow::selectItem() {
    if (updating) return;
    auto *item = tree->currentItem();
    if (!item) { tabIndex = buttonIndex = -1; properties->setEnabled(false); return; }
    properties->setEnabled(true); updating = true;
    buttonIndex = item->parent() ? item->parent()->indexOfChild(item) : -1;
    tabIndex = tree->indexOfTopLevelItem(item->parent() ? item->parent() : item);
    tabName->setText(panel.tabs[tabIndex].name);
    properties->setCurrentIndex(buttonIndex < 0 ? 0 : 1);
    if (buttonIndex >= 0) {
        const auto &button = panel.tabs[tabIndex].buttons[buttonIndex];
        buttonName->setText(button.name); action->setCurrentIndex(int(button.action));
        path->setText(button.path); args->setText(button.args); value->setText(button.value); toolTip->setPlainText(button.toolTip);
        args->setEnabled(button.action == Action::Run); value->setEnabled(button.action == Action::StoreIncrement);
    }
    preview->setCurrentTab(tabIndex); updating = false;
}
void CreatorWindow::editButton() {
    if (updating || tabIndex < 0 || buttonIndex < 0) return;
    auto &button = panel.tabs[tabIndex].buttons[buttonIndex];
    button = {buttonName->text(), Action(action->currentIndex()), path->text(), args->text(), value->text(), toolTip->toPlainText()};
    args->setEnabled(button.action == Action::Run); value->setEnabled(button.action == Action::StoreIncrement);
    tree->currentItem()->setText(0, button.name); changed();
}
void CreatorWindow::changed() { dirty = true; updateTitle(); previewTimer->start(); }
void CreatorWindow::refreshPreview() {
    preview->setPanel(panel); if (tabIndex >= 0) preview->setCurrentTab(tabIndex);
    const auto error = validate(panel);
    validation->setText(error.isEmpty() ? "Ready to export" : error);
    statusBar()->showMessage(error.isEmpty() ? "Preview updates as you edit. Save or export to open this panel in Interpreter." : "Complete the panel details before exporting.");
}
void CreatorWindow::addTab() {
    if (panel.tabs.size() >= 100) { errorDialog(this, "Tab limit", "A panel supports at most 100 tabs."); return; }
    panel.tabs.append({QString("Tab %1").arg(panel.tabs.size() + 1), {}});
    rebuildTree(panel.tabs.size() - 1); changed(); tabName->setFocus(); tabName->selectAll();
}
void CreatorWindow::addButton() {
    if (tabIndex < 0) return;
    auto &buttons = panel.tabs[tabIndex].buttons;
    buttons.append({QString("Button %1").arg(buttons.size() + 1), Action::Touch, {}, {}, {}, {}});
    rebuildTree(tabIndex, buttons.size() - 1); changed(); buttonName->setFocus(); buttonName->selectAll();
}
void CreatorWindow::duplicateItem() {
    if (tabIndex < 0) return;
    if (buttonIndex < 0) {
        if (panel.tabs.size() >= 100) return;
        auto copy = panel.tabs[tabIndex]; copy.name += " copy";
        panel.tabs.insert(tabIndex + 1, copy); rebuildTree(tabIndex + 1);
    } else {
        auto copy = panel.tabs[tabIndex].buttons[buttonIndex]; copy.name += " copy";
        panel.tabs[tabIndex].buttons.insert(buttonIndex + 1, copy); rebuildTree(tabIndex, buttonIndex + 1);
    }
    changed();
}
void CreatorWindow::removeItem() {
    if (tabIndex < 0) return;
    if (buttonIndex < 0) {
        if (panel.tabs.size() == 1) { errorDialog(this, "Keep one tab", "A panel needs at least one tab."); return; }
        if (!panel.tabs[tabIndex].buttons.isEmpty() && QMessageBox::question(this, "Remove tab", "Remove this tab and all of its buttons?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        panel.tabs.removeAt(tabIndex); rebuildTree(std::min(tabIndex, int(panel.tabs.size()) - 1));
    } else {
        panel.tabs[tabIndex].buttons.removeAt(buttonIndex); rebuildTree(tabIndex);
    }
    changed();
}
void CreatorWindow::moveItem(int delta) {
    if (tabIndex < 0) return;
    if (buttonIndex < 0) {
        const int to = tabIndex + delta;
        if (to < 0 || to >= panel.tabs.size()) return;
        panel.tabs.move(tabIndex, to); rebuildTree(to);
    } else {
        const int to = buttonIndex + delta;
        auto &buttons = panel.tabs[tabIndex].buttons;
        if (to < 0 || to >= buttons.size()) return;
        buttons.move(buttonIndex, to); rebuildTree(tabIndex, to);
    }
    changed();
}
void CreatorWindow::previewWindow() {
    auto *dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(panel.name + " — Preview"); dialog->resize(800, 580); dialog->setMinimumSize(340, 300);
    auto *layout = new QVBoxLayout(dialog); layout->setContentsMargins(0, 0, 0, 0);
    auto *fullPreview = new PanelView(true); fullPreview->setPanel(panel); fullPreview->setCurrentTab(tabIndex);
    layout->addWidget(fullPreview); dialog->show();
}
bool CreatorWindow::openFile(const QString &path) {
    Panel candidate; QString error;
    if (!loadPanel(path, &candidate, &error)) { errorDialog(this, "Could not open panel", error); return false; }
    if (!maybeSave()) return false;
    panel = candidate; filePath = QFileInfo(path).absoluteFilePath(); dirty = false;
    panelName->setText(panel.name); rebuildTree(0); refreshPreview(); updateTitle(); return true;
}
bool CreatorWindow::saveTo(const QString &path) {
    QString error;
    if (!savePanel(path, panel, &error)) { errorDialog(this, "Could not save panel", error); return false; }
    filePath = QFileInfo(path).absoluteFilePath(); dirty = false; updateTitle();
    statusBar()->showMessage("Saved " + filePath); return true;
}
bool CreatorWindow::save(bool saveAs) {
    QString destination = filePath;
    const auto error = validate(panel);
    if (!error.isEmpty()) { errorDialog(this, "Complete the panel before saving", error); return false; }
    if (saveAs || destination.isEmpty()) destination = QFileDialog::getSaveFileName(this, "Save control panel", destination.isEmpty() ? QDir::homePath() + "/Panel.controlpanel" : destination, "Control panel (*.controlpanel);;XML (*.xml)");
    if (destination.isEmpty()) return false;
    if (QFileInfo(destination).suffix().isEmpty()) destination += ".controlpanel";
    return saveTo(destination);
}
bool CreatorWindow::maybeSave() {
    if (!dirty) return true;
    const auto answer = QMessageBox::warning(this, "Save changes?", "This panel has unsaved changes.", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save) return save();
    return answer == QMessageBox::Discard;
}
void CreatorWindow::updateTitle() {
    setWindowTitle((filePath.isEmpty() ? "Untitled" : QFileInfo(filePath).fileName()) + "[*] — ControlPanel Creator");
    setWindowModified(dirty); setWindowFilePath(filePath);
}
void CreatorWindow::closeEvent(QCloseEvent *event) { if (maybeSave()) event->accept(); else event->ignore(); }
}
