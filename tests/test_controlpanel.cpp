#include "actions.h"
#include "model.h"
#include "panelview.h"
#include "windows.h"
#include "appexport.h"

#include <QComboBox>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QLockFile>
#include <QPlainTextEdit>
#include <QProcess>
#include <QImage>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QTabBar>
#include <QTimer>

using namespace cp;
namespace {
QByteArray read(const QString &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
bool write(const QString &path, const QByteArray &bytes) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); }
Panel sample() {
    return {"A & B <controls>", {{"Files", {{"Touch & go", Action::Touch, "marker", {}, {}, "A <literal> tooltip"}}},
                                {"Scripts", {{"Run", Action::Run, "/bin/echo", "47 \"two words\" ''", {}, {}}}},
                                {"Counter", {{"Down", Action::StoreIncrement, "counter", {}, "-0.5", {}}}}}};
}
void type(QLineEdit *field, const QString &text) { field->setFocus(); field->selectAll(); QTest::keyClicks(field, text); }
}

class ControlPanelTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { configureAppearance(); }
    void xmlRoundTrip() {
        const auto original = sample();
        const auto xml = toXml(original); QVERIFY(xml.contains("&amp;")); QVERIFY(xml.contains("<ToolTip>"));
        Panel loaded; QString error;
        QVERIFY2(parseXml(xml, &loaded, &error), qPrintable(error));
        QCOMPARE(toXml(loaded), xml);
        QTemporaryDir dir; const auto path = dir.filePath("panel.xml");
        QVERIFY(savePanel(path, loaded, &error)); QVERIFY(loadPanel(path, &loaded, &error));
        QCOMPARE(toXml(loaded), xml);
    }
    void invalidXml_data() {
        QTest::addColumn<QByteArray>("xml");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("wrong root") << QByteArray("<panel/>");
        QTest::newRow("trailing root") << toXml(sample()) + "<extra/>";
        QTest::newRow("duplicate name") << QByteArray("<ControlPanel><name>A</name><name>B</name><Tab><name>T</name></Tab></ControlPanel>");
        QTest::newRow("empty tabs") << QByteArray("<ControlPanel><name>A</name></ControlPanel>");
        QTest::newRow("DTD") << QByteArray("<!DOCTYPE ControlPanel [<!ENTITY x 'abc'>]><ControlPanel><name>&x;</name><Tab><name>T</name></Tab></ControlPanel>");
        QTest::newRow("namespace") << QByteArray("<ControlPanel><name>A</name><Tab xmlns='wrong'><name>T</name></Tab></ControlPanel>");
        for (const auto &body : {QByteArray("<name>B</name><path>/tmp/a</path>"),
                                QByteArray("<name>B</name><action>delete</action><path>/tmp/a</path>"),
                                QByteArray("<name>B</name><action>touch</action><path>/tmp/a</path><path>/tmp/b</path>"),
                                QByteArray("<name>B</name><action>run</action><path>/tmp/a</path><args>'bad</args>"),
                                QByteArray("<name>B</name><action>storeincrement</action><path>/tmp/a</path><value>NaN</value>"),
                                QByteArray("<name>B</name><action>touch</action><path></path>"),
                                QByteArray("<name>B</name><action>touch</action><path><nested/></path>")}) {
            QTest::newRow(body.constData()) << QByteArray("<ControlPanel><name>A</name><Tab><name>T</name><button>") + body + "</button></Tab></ControlPanel>";
        }
    }
    void invalidXml() {
        QFETCH(QByteArray, xml); auto original = sample(); const auto before = toXml(original); QString error;
        QVERIFY(!parseXml(xml, &original, &error)); QVERIFY(!error.isEmpty()); QCOMPARE(toXml(original), before);
    }
    void legacyValueAndLimits() {
        auto xml = toXml(sample()); xml.replace("<value>", "<Value>").replace("</value>", "</Value>");
        Panel panel; QString error; QVERIFY(parseXml(xml, &panel, &error)); QCOMPARE(panel.tabs[2].buttons[0].value, "-0.5");
        QVERIFY(!parseXml(QByteArray(4 * 1024 * 1024 + 1, ' '), &panel, &error));
        panel = sample(); panel.tabs[0].buttons[0].name += QChar(1); QVERIFY(!validate(panel).isEmpty());
    }
    void uuidAndExportMetadata() {
        const auto first = newPanel(); const auto second = newPanel();
        QVERIFY(!QUuid(first.uuid).isNull()); QVERIFY(first.uuid != second.uuid);
        auto original = sample();
        original.appExport = {"4.12.9", false, "icons/custom & icon.png", "Help Desk\nsupport@example.test", "Build <internal> & QA", "-"};
        QString error; Panel loaded;
        QVERIFY2(parseXml(toXml(original), &loaded, &error), qPrintable(error));
        QCOMPARE(toXml(loaded), toXml(original)); QCOMPARE(loaded.uuid, original.uuid);
        const QByteArray legacy("<ControlPanel><name>Old panel</name><Tab><name>Controls</name></Tab></ControlPanel>");
        QTemporaryDir dir; const auto path = dir.filePath("legacy.controlpanel"); QVERIFY(write(path, legacy));
        bool missing = false;
        QVERIFY(loadPanel(path, &loaded, &error, &missing)); QVERIFY(missing);
        QCOMPARE(read(path), legacy); // Validation and read-only inspection never rewrite inputs.
        QVERIFY2(loadPanelForEditing(path, &loaded, &error), qPrintable(error));
        const auto id = loaded.uuid; QVERIFY(read(path).contains(id.toUtf8()));
        QVERIFY(loadPanelForEditing(path, &loaded, &error)); QCOMPARE(loaded.uuid, id);
        CreatorWindow creator; QVERIFY(creator.openFile(path)); QCOMPARE(creator.document().uuid, id);
        QVERIFY(creator.saveTo(dir.filePath("renamed.controlpanel"))); QCOMPARE(creator.document().uuid, id);
        for (const auto &field : {QByteArray("<uuid>invalid</uuid>"),
                                 QByteArray("<uuid>00000000-0000-0000-0000-000000000000</uuid>"),
                                 QByteArray("<uuid>" + id.toUtf8() + "</uuid><uuid>" + id.toUtf8() + "</uuid>"),
                                 QByteArray("<appExport><version>1.2-beta</version></appExport>"),
                                 QByteArray("<appExport><autoIncrement>yes</autoIncrement></appExport>"),
                                 QByteArray("<appExport><contact>A</contact><contact>B</contact></appExport>"),
                                 QByteArray("<appExport><unknown/></appExport>"),
                                 QByteArray("<appExport/><appExport/>"),
                                 QByteArray("<appExport><version format='x'>1</version></appExport>")}) {
            auto xml = legacy; xml.replace("</ControlPanel>", field + "</ControlPanel>");
            QVERIFY2(!parseXml(xml, &loaded, &error), field.constData());
        }
    }
    void appVersionsAndNames() {
        QCOMPARE(nextExportVersion("1.0.0"), "1.0.1"); QCOMPARE(nextExportVersion("1"), "2");
        QCOMPARE(nextExportVersion("1.9"), "1.10"); QCOMPARE(nextExportVersion("1.9999"), "2.0");
        QCOMPARE(nextExportVersion("1.9999.9999"), "2.0.0"); QVERIFY(nextExportVersion("9999.9999.9999").isEmpty());
        QVERIFY(nextExportVersion({}).isEmpty());
        for (const auto &version : {"1.2.3.4", "-1", "01", "1.", "1.10000", "beta"}) {
            auto panel = sample(); panel.appExport.version = version; QVERIFY(!validate(panel).isEmpty());
        }
        QCOMPARE(appFileName("...A/B:C\\D"), "A-B-C-D.app");
        QVERIFY(appFileName(QString(500, QChar(0x754c))).toUtf8().size() <= 184);
    }
    void embeddedPanelRoundTrip() {
        auto original = sample(); original.name = "Control <panel> & support";
        original.appExport = {"3.2.1", true, "private/source/icon.png", "Help Desk\n123-456", "Internal release", "-"};
        QString error; const auto payload = encodeEmbeddedPanel(original, "/opt/internal", &error);
        QVERIFY2(!payload.isEmpty(), qPrintable(error)); QVERIFY(!payload.contains("<ControlPanel>"));
        QVERIFY(!payload.contains("private/source/icon.png"));
        EmbeddedPanel decoded;
        QVERIFY2(decodeEmbeddedPanel(payload, &decoded, &error), qPrintable(error));
        QCOMPARE(decoded.panel.uuid, original.uuid); QCOMPARE(decoded.panel.name, original.name);
        QCOMPARE(decoded.panel.appExport.version, "3.2.1"); QCOMPARE(decoded.workingDirectory, "/opt/internal");
        QCOMPARE(decoded.panel.tabs[0].buttons[0].path, "/opt/internal/marker");
        QCOMPARE(decoded.panel.tabs[1].buttons[0].args, original.tabs[1].buttons[0].args);
        QVERIFY(supportInformation(decoded.panel).contains(original.uuid));
        QVERIFY(supportInformation(decoded.panel).contains("Version: 3.2.1"));
        for (const auto &bad : {payload.left(payload.size() - 1), payload + "trailing", QByteArray("invalid")})
            QVERIFY(!decodeEmbeddedPanel(bad, &decoded, &error));
        QVERIFY(encodeEmbeddedPanel(original, "relative", &error).isEmpty());
        const QByteArray blank(embeddedSlotSize, 0); QVERIFY(!readEmbeddedSlot(blank.constData(), &decoded, &error));
    }
    void standaloneWithoutSourceDirectory() {
        QTemporaryDir dir;
        auto panel = newPanel();
        panel.tabs[0].buttons = {{"Create marker", Action::Touch, dir.filePath("marker"), {}, {}, {}}};
        RuntimeWindow window(panel, dir.filePath("deleted-authoring-directory")); window.show();
        QTest::mouseClick(window.findChild<PanelButton *>(), Qt::LeftButton);
        QTRY_VERIFY(QFile::exists(dir.filePath("marker")));
    }
    void standaloneSupportAndControls() {
        auto panel = sample(); panel.tabs = {panel.tabs[0]}; panel.appExport.contact = "Internal Help Desk";
        RuntimeWindow window(panel, QDir::tempPath()); window.show(); QTest::qWait(30);
        QCOMPARE(window.windowTitle(), panel.name);
        for (const auto *action : window.findChildren<QAction *>()) {
            QVERIFY(!action->text().contains("Open")); QVERIFY(!action->text().contains("Import"));
            QVERIFY(!action->text().contains("Reload")); QVERIFY(!action->text().contains("Export"));
        }
        QVERIFY(!window.findChild<QTabBar *>()->isVisible());
        bool copied = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = window.findChild<QDialog *>("supportDialog");
            if (!dialog) return;
            const auto details = dialog->findChild<QPlainTextEdit *>("supportDetails")->toPlainText();
            QTest::mouseClick(dialog->findChild<QPushButton *>("copySupportInformation"), Qt::LeftButton);
            copied = QApplication::clipboard()->text() == details && details.contains(panel.uuid)
                && details.contains("Version: 1.0.0") && details.contains("Internal Help Desk");
            const auto destination = qEnvironmentVariable("CP_SCREENSHOT_DIR");
            if (!destination.isEmpty()) dialog->grab().save(destination + "/support.png");
            dialog->accept();
        });
        QTest::mouseClick(window.findChild<QPushButton *>("supportButton"), Qt::LeftButton);
        QVERIFY(copied);
        const auto destination = qEnvironmentVariable("CP_SCREENSHOT_DIR");
        if (!destination.isEmpty()) window.grab().save(destination + "/standalone.png");
    }
    void creatorAppExportSettings() {
        CreatorWindow creator; creator.show();
        const auto uuid = creator.document().uuid;
        bool visited = false;
        QTimer::singleShot(0, &creator, [&] {
            auto *dialog = creator.findChild<QDialog *>("appExportSettingsDialog");
            if (!dialog) return;
            visited = true;
            QCOMPARE(dialog->findChild<QLineEdit *>("panelUuid")->text(), uuid);
            dialog->findChild<QLineEdit *>("appVersion")->setText("5.3.2");
            dialog->findChild<QCheckBox *>("appAutoIncrement")->setChecked(false);
            dialog->findChild<QPlainTextEdit *>("appContact")->setPlainText("Operations Support\nsupport@example.test");
            dialog->findChild<QPlainTextEdit *>("appDescription")->setPlainText("Internal software controls");
            const auto destination = qEnvironmentVariable("CP_SCREENSHOT_DIR");
            if (!destination.isEmpty()) dialog->grab().save(destination + "/export-settings.png");
            QTest::mouseClick(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save), Qt::LeftButton);
        });
        QTest::mouseClick(creator.findChild<QPushButton *>("appExportSettings"), Qt::LeftButton); QVERIFY(visited);
        QTemporaryDir dir; const auto path = dir.filePath("settings.controlpanel"); QVERIFY(creator.saveTo(path));
        Panel loaded; QString error; QVERIFY(loadPanel(path, &loaded, &error));
        QCOMPARE(loaded.uuid, uuid); QCOMPARE(loaded.appExport.version, "5.3.2"); QVERIFY(!loaded.appExport.autoIncrement);
        QCOMPARE(loaded.appExport.contact, "Operations Support\nsupport@example.test");
        QCOMPARE(loaded.appExport.description, "Internal software controls");
    }
    void appExportBundle() {
        const auto templatePath = qEnvironmentVariable("CP_EXPORT_TEMPLATE");
        if (templatePath.isEmpty()) QSKIP("Packaging runs this test with a signed, self-contained runtime template.");
        QTemporaryDir dir; QVERIFY(dir.isValid());
        auto panel = sample(); panel.name = "Support & Operations";
        panel.appExport.version = "2.4.7"; panel.appExport.contact = "Internal Help Desk\nsupport@example.test";
        QImage icon(100, 80, QImage::Format_ARGB32); icon.fill(QColor("#3164ab"));
        panel.appExport.iconPath = "custom.png"; QVERIFY(icon.save(dir.filePath("custom.png")));
        const auto source = dir.filePath("source.controlpanel"); const auto destination = dir.filePath(appFileName(panel.name));
        QString error; QVERIFY(savePanel(source, panel, &error));
        const auto originalId = panel.uuid;
        QVERIFY2(exportPanelApp(&panel, source, destination, templatePath, &error), qPrintable(error));
        QCOMPARE(panel.appExport.version, "2.4.8"); QCOMPARE(panel.uuid, originalId);
        Panel saved; QVERIFY(loadPanel(source, &saved, &error)); QCOMPARE(saved.appExport.version, "2.4.8");
        QVERIFY2(verifyAppSignature(destination, &error), qPrintable(error));
        const auto plist = read(destination + "/Contents/Info.plist");
        QVERIFY(plist.contains(originalId.toUtf8())); QVERIFY(plist.contains("2.4.7"));
        QVERIFY(!plist.contains("CFBundleDocumentTypes")); QVERIFY(!plist.contains("UTExportedTypeDeclarations"));
        QVERIFY(!QFileInfo::exists(destination + "/Contents/Resources/panel.xml"));
        QVERIFY(read(destination + "/Contents/Resources/PanelIcon.icns").startsWith("icns"));
        QProcess process;
        const auto executable = destination + "/Contents/MacOS/PanelApp";
        process.start(executable, {"--support-info"}); QVERIFY(process.waitForFinished(30000));
        const auto output = process.readAllStandardOutput();
        QCOMPARE(process.exitCode(), 0); QVERIFY(output.contains(originalId.toUtf8())); QVERIFY(output.contains("Version: 2.4.7"));
        process.start(executable, {source}); QVERIFY(process.waitForFinished(30000)); QCOMPARE(process.exitCode(), 2);
        process.start(executable, {"-platform", "offscreen"}); QVERIFY(process.waitForFinished(30000)); QCOMPARE(process.exitCode(), 2);
        auto failed = panel; failed.appExport.iconPath = "missing.png";
        QVERIFY(savePanel(source, failed, &error)); const auto before = read(source);
        QVERIFY(!exportPanelApp(&failed, source, destination, templatePath, &error));
        QCOMPARE(read(source), before); QCOMPARE(failed.appExport.version, "2.4.8");
        QVERIFY(verifyAppSignature(destination, &error)); QCOMPARE(read(destination + "/Contents/Info.plist"), plist);
        auto unrelated = panel; unrelated.uuid = newPanel().uuid;
        QVERIFY(savePanel(source, unrelated, &error));
        QVERIFY(!exportPanelApp(&unrelated, source, destination, templatePath, &error));
        QVERIFY(verifyAppSignature(destination, &error));
        panel.appExport.autoIncrement = false; QVERIFY(savePanel(source, panel, &error));
        QVERIFY2(exportPanelApp(&panel, source, destination, templatePath, &error), qPrintable(error));
        QCOMPARE(panel.appExport.version, "2.4.8");
        // The source XML is not needed at runtime, and cannot override the embedded snapshot.
        QVERIFY(QFile::remove(source));
        process.start(executable, {"--verify"}); QVERIFY(process.waitForFinished(30000)); QCOMPARE(process.exitCode(), 0);
        QVERIFY(write(destination + "/Contents/Info.plist", read(destination + "/Contents/Info.plist") + "\n<!-- changed -->\n"));
        QVERIFY(!verifyAppSignature(destination, &error));
        process.start(executable, {"--support-info"}); QVERIFY(process.waitForFinished(30000)); QVERIFY(process.exitCode() != 0);
        const auto demo = qEnvironmentVariable("CP_EXPORT_DEMO_DIR");
        if (!demo.isEmpty()) {
            QVERIFY(QDir().mkpath(demo)); panel.appExport.iconPath.clear();
            QVERIFY(savePanel(demo + "/Support.controlpanel", panel, &error));
            QVERIFY2(exportPanelApp(&panel, demo + "/Support.controlpanel", demo + "/Support & Operations.app", templatePath, &error), qPrintable(error));
        }
    }
    void arguments() {
        QStringList args; QString error;
        QVERIFY(splitArguments("47 \"two words\" '' 'literal $HOME' a\\ b \\\"", &args, &error));
        QCOMPARE(args, QStringList({"47", "two words", "", "literal $HOME", "a b", "\""}));
        QVERIFY(splitArguments("$(touch pwned); * > file", &args, &error));
        QCOMPARE(args, QStringList({"$(touch", "pwned);", "*", ">", "file"}));
        QVERIFY(!splitArguments("'unclosed", &args, &error)); QVERIFY(!splitArguments("escape\\", &args, &error));
    }
    void counterBehavior() {
        QTemporaryDir dir; const auto path = dir.filePath("counter");
        QVERIFY(incrementFile(path, {}).ok); QCOMPARE(read(path), "1\n");
        QVERIFY(incrementFile(path, "-1").ok); QCOMPARE(read(path), "0\n");
        QVERIFY(incrementFile(path, "0.5").ok); QCOMPARE(read(path), "0.5\n");
        QVERIFY(write(path, "heading 4.5 99\n")); QVERIFY(incrementFile(path, "2").ok); QCOMPARE(read(path), "6.5\n");
        QVERIFY(write(path, "not a number\n")); QVERIFY(incrementFile(path, {}).ok); QCOMPARE(read(path), "1\n");
        QVERIFY(write(path, "")); QVERIFY(incrementFile(path, {}).ok); QCOMPARE(read(path), "1\n");
        QVERIFY(write(path, "1e308")); QVERIFY(!incrementFile(path, "1e308").ok); QCOMPARE(read(path), "1e308");
        QVERIFY(!incrementFile(path, "nan").ok); QVERIFY(!incrementFile(dir.filePath("absent/file"), {}).ok);
        QVERIFY(!incrementFile(dir.path(), {}).ok);
        QVERIFY(write(path, "5"));
        QLockFile lock(path + ".controlpanel.lock"); QVERIFY(lock.tryLock());
        QVERIFY(!incrementFile(path, {}).ok); QCOMPARE(read(path), "5"); lock.unlock();
        const auto link = dir.filePath("alias"); QVERIFY(QFile::link(path, link));
        QVERIFY(incrementFile(link, {}).ok); QCOMPARE(read(path), "6\n"); QVERIFY(QFileInfo(link).isSymLink());
        QVERIFY(write(path, QByteArray(65537, ' '))); QVERIFY(!incrementFile(path, {}).ok); QCOMPARE(read(path).size(), 65537);
    }
    void failedSavePreservesOriginal() {
        QTemporaryDir dir; const auto path = dir.filePath("panel.xml"); QString error;
        QVERIFY(savePanel(path, sample(), &error)); const auto original = read(path);
        auto invalid = sample(); invalid.tabs[0].buttons[0].path.clear();
        QVERIFY(!savePanel(path, invalid, &error)); QCOMPARE(read(path), original);
    }
    void touchAndRun() {
        QTemporaryDir dir;
        ActionRunner runner; QSignalSpy finished(&runner, &ActionRunner::finished);
        const auto marker = dir.filePath("-option ; $(literal)");
        runner.execute({"Touch", Action::Touch, marker, {}, {}, {}}, dir.path());
        QTRY_COMPARE(finished.size(), 1); QVERIFY(finished.last()[1].toBool()); QVERIFY(QFile::exists(marker));
        const auto script = dir.filePath("my script.sh");
        QVERIFY(write(script, "#!/bin/sh\nprintf '%s\\n' \"$@\" > result.txt\npwd\n"));
        QVERIFY(QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        runner.execute({"Run", Action::Run, "my script.sh", "47 \"two words\" '' '$(touch pwned)'", {}, {}}, dir.path());
        QTRY_COMPARE(finished.size(), 2); QVERIFY2(finished.last()[1].toBool(), qPrintable(finished.last()[2].toString()));
        QCOMPARE(read(dir.filePath("result.txt")), "47\ntwo words\n\n$(touch pwned)\n");
        QVERIFY(!QFile::exists(dir.filePath("pwned"))); QCOMPARE(runner.activeCount(), 0);
        runner.execute({"Fail", Action::Run, "/bin/sh", "-c 'echo expected-error; exit 7'", {}, {}}, dir.path());
        QTRY_COMPARE(finished.size(), 3); QVERIFY(!finished.last()[1].toBool()); QVERIFY(finished.last()[2].toString().contains("expected-error"));
        runner.execute({"Missing", Action::Run, "missing", {}, {}, {}}, dir.path());
        QCOMPARE(finished.size(), 4); QVERIFY(!finished.last()[1].toBool());
        runner.execute({"Touch failure", Action::Touch, "no-folder/file", {}, {}, {}}, dir.path());
        QTRY_COMPARE(finished.size(), 5); QVERIFY(!finished.last()[1].toBool());
    }
    void processLifecycle() {
        QTemporaryDir dir; ActionRunner runner; QSignalSpy finished(&runner, &ActionRunner::finished);
        const auto invalid = dir.filePath("bad-executable");
        QVERIFY(write(invalid, "#!/no/such/interpreter\n"));
        QVERIFY(QFile::setPermissions(invalid, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        runner.execute({"Bad executable", Action::Run, invalid, {}, {}, {}}, dir.path());
        QTRY_COMPARE(finished.size(), 1); QVERIFY(!finished.last()[1].toBool()); QCOMPARE(runner.activeCount(), 0);
        runner.execute({"Long action", Action::Run, "/bin/sleep", "20", {}, {}}, dir.path());
        QCOMPARE(runner.activeCount(), 1); QTest::qWait(50); runner.stopAll();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 3000); QCOMPARE(runner.activeCount(), 0); QVERIFY(!finished.last()[1].toBool());
    }
    void responsiveLayoutAndSafePreview() {
        auto panel = sample(); panel.tabs = {panel.tabs[0]};
        for (int i = 0; i < 25; ++i) panel.tabs[0].buttons.append({QString(140, 'W') + " long label " + QString::number(i), Action::Touch, "/unused", {}, {}, "Tooltip"});
        PanelView preview(true); QSignalSpy activated(&preview, &PanelView::activated);
        preview.setPanel(panel); preview.resize(340, 360); preview.show(); QTest::qWait(80);
        const auto buttons = preview.findChildren<PanelButton *>(); QCOMPARE(buttons.size(), 26);
        for (auto *button : buttons) {
            QVERIFY(button->x() >= 0); QVERIFY(button->geometry().right() < button->parentWidget()->width());
            QVERIFY(button->height() >= button->heightForWidth(button->width()));
        }
        const auto scroll = preview.findChild<QScrollArea *>(); QVERIFY(scroll); QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        QTest::mouseClick(buttons.first(), Qt::LeftButton); QCOMPARE(activated.size(), 0);
        preview.resize(1000, 600); QTest::qWait(80);
        QVERIFY(buttons[1]->x() > buttons[0]->x());
        QCOMPARE(buttons[1]->toolTip(), Qt::convertFromPlainText("Tooltip"));
        PanelView runtime(false); QSignalSpy live(&runtime, &PanelView::activated); runtime.setPanel(sample()); runtime.show();
        QTest::mouseClick(runtime.findChild<PanelButton *>(), Qt::LeftButton); QCOMPARE(live.size(), 1);
    }
    void creatorExportRoundTrip() {
        QTemporaryDir dir;
        CreatorWindow creator; creator.show();
        type(creator.findChild<QLineEdit *>("panelName"), "Created panel");
        QTest::mouseClick(creator.findChild<QPushButton *>("addButton"), Qt::LeftButton);
        type(creator.findChild<QLineEdit *>("buttonName"), "Increment alarm");
        type(creator.findChild<QLineEdit *>("buttonPath"), "alarm.txt");
        creator.findChild<QComboBox *>("buttonAction")->setCurrentIndex(int(Action::StoreIncrement));
        type(creator.findChild<QLineEdit *>("buttonValue"), "-1");
        creator.findChild<QPlainTextEdit *>("buttonToolTip")->setPlainText("Lower the level");
        QTest::mouseClick(creator.findChild<QPushButton *>("duplicateItem"), Qt::LeftButton);
        QCOMPARE(creator.document().tabs[0].buttons.size(), 2);
        const auto file = dir.filePath("created.controlpanel"); QVERIFY(creator.saveTo(file));
        Panel loaded; QString error; QVERIFY(loadPanel(file, &loaded, &error));
        QCOMPARE(loaded.name, "Created panel"); QCOMPARE(loaded.tabs[0].buttons[0].value, "-1");
        QCOMPARE(loaded.tabs[0].buttons[0].toolTip, "Lower the level");
        QVERIFY(creator.openFile(file)); QCOMPARE(toXml(creator.document()), toXml(loaded));
        InterpreterWindow interpreter; QVERIFY(interpreter.openFile(file)); QCOMPARE(interpreter.windowTitle(), "Created panel");
        interpreter.show(); QTest::mouseClick(interpreter.findChild<PanelButton *>(), Qt::LeftButton);
        QCOMPARE(read(dir.filePath("alarm.txt")), "-1\n"); creator.close(); interpreter.close();
    }
    void renderWindows() {
        const auto destination = qEnvironmentVariable("CP_SCREENSHOT_DIR");
        if (destination.isEmpty()) QSKIP("Set CP_SCREENSHOT_DIR to render visual review images.");
        QVERIFY(QDir().mkpath(destination));
        QTemporaryDir dir;
        auto panel = sample(); panel.name = "My Control Panel";
        panel.tabs[0].buttons = {{"Create a marker", Action::Touch, "marker.txt", {}, {}, "Create a marker file"},
                                {"Create a second marker with a longer label", Action::Touch, "marker2.txt", {}, {}, {}}};
        QString error; const auto file = dir.filePath("Getting Started.controlpanel"); QVERIFY(savePanel(file, panel, &error));
        CreatorWindow creator; QVERIFY(creator.openFile(file)); creator.show();
        auto *tree = creator.findChild<QTreeWidget *>("structureTree"); tree->setCurrentItem(tree->topLevelItem(0)->child(0));
        QTest::qWait(200); QVERIFY(creator.grab().save(destination + "/creator.png")); creator.close();
        InterpreterWindow interpreter; QVERIFY(interpreter.openFile(file)); interpreter.show();
        QTest::qWait(100); QVERIFY(interpreter.grab().save(destination + "/interpreter.png"));
        interpreter.resize(340, 340); QTest::qWait(100); QVERIFY(interpreter.grab().save(destination + "/interpreter-narrow.png")); interpreter.close();
    }
};
QTEST_MAIN(ControlPanelTests)
#include "test_controlpanel.moc"
