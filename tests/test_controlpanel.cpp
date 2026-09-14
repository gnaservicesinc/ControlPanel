#include "actions.h"
#include "model.h"
#include "panelview.h"
#include "windows.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QLockFile>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

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
