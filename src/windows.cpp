#include "windows.h"
#include "appexport.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
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
#include <QProgressDialog>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QThread>
#include <QVBoxLayout>

namespace cp {
namespace {
const QString panelFilter = "Control panels (*.controlpanel *.xml);;All files (*)";
class ExportProgressDialog : public QProgressDialog {
public:
    using QProgressDialog::QProgressDialog;
    void reject() override {} // Escape must not dismiss an export that is still saving/signing.
};
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

InterpreterWindow::InterpreterWindow(QWidget *parent)
    : RuntimeWindow({"ControlPanel Interpreter", {{"Welcome", {}}}}, QDir::homePath(), false, parent) {
    auto *file = findChild<QMenu *>("runtimeFileMenu");
    auto *open = new QAction("&Open Panel…", file); open->setShortcut(QKeySequence::Open);
    auto *reload = new QAction("&Reload Panel", file); reload->setShortcut(QKeySequence("Ctrl+R"));
    file->insertAction(file->actions().first(), reload); file->insertAction(reload, open);
    connect(open, &QAction::triggered, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Open a control panel", filePath, panelFilter);
        if (!path.isEmpty()) openFile(path);
    });
    connect(reload, &QAction::triggered, this, [this] { if (!filePath.isEmpty()) openFile(filePath); });
    statusBar()->showMessage("Open a .controlpanel or XML file to begin. Only open panels you trust.");
}

bool InterpreterWindow::openFile(const QString &path) {
    Panel candidate; QString error;
    if (!loadPanelForEditing(path, &candidate, &error)) { errorDialog(this, "Could not open panel", error); return false; }
    filePath = QFileInfo(path).absoluteFilePath();
    showPanel(candidate, QFileInfo(filePath).absolutePath());
    setWindowFilePath(filePath);
    statusBar()->showMessage("Loaded " + QFileInfo(path).fileName() + ". Click a control to run its action.");
    return true;
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
    menuAction(file, "App Export Settings…", {}, [this] { appExportSettings(); });
    auto *appExportAction = menuAction(file, "Export &App…", QKeySequence("Ctrl+Shift+E"), [this] { exportApp(); });
    appExportAction->setObjectName("exportAppAction");
    file->addSeparator(); menuAction(file, "&Close", QKeySequence::Close, [this] { close(); });
    menuAction(file, "&Quit", QKeySequence::Quit, [this] { close(); });
    auto *viewMenu = menuBar()->addMenu("&View");
    auto *previewAction = menuAction(viewMenu, "Preview Window", QKeySequence("Ctrl+P"), [this] { previewWindow(); });
    menuAction(menuBar()->addMenu("&Help"), "About ControlPanel", {}, [this] { about(this); });
    auto *toolbar = addToolBar("Document"); toolbar->setMovable(false);
    toolbar->addAction(newAction); toolbar->addAction(openAction); toolbar->addAction(saveAction);
    toolbar->addSeparator(); toolbar->addAction(previewAction); toolbar->addAction(exportAction);
    toolbar->addAction(appExportAction);

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
    auto *exportSettings = new QPushButton("App Export Settings…"); exportSettings->setObjectName("appExportSettings");
    editorLayout->addWidget(exportSettings);
    connect(exportSettings, &QPushButton::clicked, this, &CreatorWindow::appExportSettings);
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
    if (!maybeSave()) return false;
    if (!loadPanelForEditing(path, &candidate, &error)) { errorDialog(this, "Could not open panel", error); return false; }
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
void CreatorWindow::appExportSettings() {
    QDialog dialog(this); dialog.setObjectName("appExportSettingsDialog");
    dialog.setWindowTitle("App Export Settings"); dialog.resize(560, 550);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(label("Export a self-contained app named “" + panel.name + "”. These settings are saved in the panel file."));
    auto *form = new QFormLayout; form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *version = new QLineEdit(panel.appExport.version); version->setObjectName("appVersion");
    version->setPlaceholderText("Optional, for example 1.0.0"); form->addRow("Version for next export", version);
    auto *increment = new QCheckBox("Advance version after each successful export"); increment->setObjectName("appAutoIncrement");
    increment->setChecked(panel.appExport.autoIncrement); form->addRow(increment);
    auto *iconRow = new QWidget; auto *iconLayout = new QHBoxLayout(iconRow); iconLayout->setContentsMargins(0, 0, 0, 0);
    iconRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *icon = new QLineEdit(panel.appExport.iconPath); icon->setObjectName("appIcon"); icon->setPlaceholderText("Built-in icon");
    auto *browse = new QPushButton("Choose…"); iconLayout->addWidget(icon, 1); iconLayout->addWidget(browse);
    form->addRow("App icon", iconRow);
    connect(browse, &QPushButton::clicked, &dialog, [&] {
        const auto selected = QFileDialog::getOpenFileName(&dialog, "Choose an app icon", icon->text(), "App icons and images (*.icns *.png *.jpg *.jpeg)");
        if (!selected.isEmpty()) icon->setText(selected);
    });
    auto *contact = new QPlainTextEdit(panel.appExport.contact); contact->setObjectName("appContact");
    contact->setPlaceholderText("Team or person, email, phone, or support instructions"); contact->setMaximumHeight(90);
    form->addRow("Support contact (optional)", contact);
    auto *description = new QPlainTextEdit(panel.appExport.description); description->setObjectName("appDescription");
    description->setPlaceholderText("Optional version notes, copyright, or other About information"); description->setMaximumHeight(85);
    form->addRow("About information (optional)", description);
    auto *identity = new QComboBox; identity->setObjectName("appSigningIdentity");
    for (const auto &entry : localSigningIdentities()) identity->addItem(entry.first, entry.second);
    int selected = identity->findData(panel.appExport.signingIdentity);
    if (selected < 0) { identity->addItem("Saved certificate (not available on this Mac)", panel.appExport.signingIdentity); selected = identity->count() - 1; }
    identity->setCurrentIndex(selected); form->addRow("Signing", identity);
    auto *uuid = new QLineEdit(panel.uuid); uuid->setReadOnly(true); uuid->setObjectName("panelUuid");
    form->addRow("Panel ID (UUID)", uuid);
    layout->addLayout(form);
    layout->addWidget(label("End users can find the version, panel ID, and contact under Help / Support and copy them into a report. Apps contain one fixed panel and have no import or editing controls. Action programs and files stay at their configured paths."));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        Panel candidate = panel;
        candidate.appExport = {version->text().trimmed(), increment->isChecked(), icon->text(),
            contact->toPlainText(), description->toPlainText(), identity->currentData().toString()};
        // Export settings may be authored before the panel itself is complete.
        Panel validationPanel = newPanel(); validationPanel.appExport = candidate.appExport;
        const auto error = validate(validationPanel);
        if (!error.isEmpty()) { errorDialog(&dialog, "Check app export settings", error); return; }
        panel.appExport = candidate.appExport; changed(); dialog.accept();
    });
    dialog.exec();
}

void CreatorWindow::exportApp() {
    if (!save()) return; // Establish the panel UUID and action base directory on disk.
    const auto destination = QFileDialog::getSaveFileName(this, "Export control panel app",
        QFileInfo(filePath).absolutePath() + "/" + appFileName(panel.name), "Application (*.app)");
    if (destination.isEmpty()) return;
    const auto target = destination.endsWith(".app", Qt::CaseInsensitive) ? destination : destination + ".app";
    Panel exported = panel; QString error; bool success = false;
    const auto templatePath = appExportTemplatePath();
    const auto source = filePath;
    ExportProgressDialog progress("Building and signing “" + panel.name + "”…", QString(), 0, 0, this);
    progress.setWindowTitle("Export App"); progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr); progress.setMinimumDuration(0);
    progress.setWindowFlag(Qt::WindowCloseButtonHint, false);
    auto *worker = QThread::create([&] { success = exportPanelApp(&exported, source, target, templatePath, &error); });
    connect(worker, &QThread::finished, &progress, &QDialog::accept);
    QTimer::singleShot(0, &progress, [worker] { worker->start(); });
    progress.exec(); worker->wait(); delete worker;
    if (!success) { errorDialog(this, "Could not export app", error); return; }
    const auto version = panel.appExport.version;
    panel = exported; dirty = false; updateTitle();
    statusBar()->showMessage("Exported " + target);
    QMessageBox box(QMessageBox::Information, "App exported", "Created " + target
        + (version.isEmpty() ? QString() : "\nApp version: " + version)
        + "\nPanel ID: " + panel.uuid
        + (panel.appExport.autoIncrement && !version.isEmpty() ? "\nNext export version: " + panel.appExport.version : QString()), QMessageBox::Ok, this);
    box.setTextFormat(Qt::PlainText); box.exec();
}

void CreatorWindow::updateTitle() {
    setWindowTitle((filePath.isEmpty() ? "Untitled" : QFileInfo(filePath).fileName()) + "[*] — ControlPanel Creator");
    setWindowModified(dirty); setWindowFilePath(filePath);
}
void CreatorWindow::closeEvent(QCloseEvent *event) { if (maybeSave()) event->accept(); else event->ignore(); }
}
