#include "model.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <cmath>

namespace cp {
namespace {
constexpr qint64 maxXmlBytes = 4 * 1024 * 1024;
constexpr int maxTabs = 100;
constexpr int maxButtons = 2000;

bool legalText(const QString &text) {
    for (const auto c : text.toUcs4())
        if (!(c == 9 || c == 10 || c == 13 || (c >= 0x20 && c <= 0xd7ff)
              || (c >= 0xe000 && c <= 0xfffd) || (c >= 0x10000 && c <= 0x10ffff)))
            return false;
    return !text.contains(QChar::Null);
}

bool uniqueField(QXmlStreamReader &reader, QSet<QString> &seen, const QString &name) {
    if (seen.contains(name)) {
        reader.raiseError("Duplicate <" + name + ">.");
        return false;
    }
    seen.insert(name);
    if (!reader.attributes().isEmpty() || !reader.namespaceUri().isEmpty()) {
        reader.raiseError("Attributes and namespaces are not supported.");
        return false;
    }
    return true;
}

Button readButton(QXmlStreamReader &reader) {
    Button button;
    QSet<QString> seen;
    while (reader.readNextStartElement()) {
        QString tag = reader.name().toString();
        if (tag == "Value") tag = "value";
        if (!uniqueField(reader, seen, tag)) break;
        const auto text = reader.readElementText();
        if (tag == "name") button.name = text;
        else if (tag == "path") button.path = text;
        else if (tag == "args") button.args = text;
        else if (tag == "value") button.value = text.trimmed();
        else if (tag == "ToolTip") button.toolTip = text;
        else if (tag == "action") {
            if (text.trimmed() == "touch") button.action = Action::Touch;
            else if (text.trimmed() == "storeincrement") button.action = Action::StoreIncrement;
            else if (text.trimmed() == "run") button.action = Action::Run;
            else reader.raiseError("Unknown action: " + text);
        } else reader.raiseError("Unknown button element: <" + tag + ">.");
    }
    for (const auto &required : {"name", "action", "path"})
        if (!seen.contains(required)) reader.raiseError(QString("Button requires <") + required + ">.");
    return button;
}

Tab readTab(QXmlStreamReader &reader, int &buttonCount) {
    Tab tab;
    bool named = false;
    while (reader.readNextStartElement()) {
        const auto tag = reader.name();
        if (!reader.attributes().isEmpty() || !reader.namespaceUri().isEmpty()) { reader.raiseError("Attributes and namespaces are not supported."); break; }
        if (tag == u"name" && !named) { tab.name = reader.readElementText(); named = true; }
        else if (tag == u"button") {
            if (++buttonCount > maxButtons) { reader.raiseError("At most 2000 buttons are supported."); break; }
            tab.buttons.append(readButton(reader));
        } else reader.raiseError("Expected one <name> and any number of <button> elements in a Tab.");
    }
    return tab;
}
}

QString actionName(Action action) {
    switch (action) {
    case Action::Touch: return "touch";
    case Action::StoreIncrement: return "storeincrement";
    case Action::Run: return "run";
    }
    return {};
}

bool splitArguments(const QString &text, QStringList *args, QString *error) {
    args->clear();
    QString token;
    QChar quote;
    bool started = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (c == '\\' && quote != '\'') {
            if (++i == text.size()) { *error = "Arguments end with an incomplete escape."; return false; }
            token += text[i]; started = true;
        } else if (!quote.isNull()) {
            if (c == quote) quote = QChar(); else token += c;
        } else if (c == '\'' || c == '"') { quote = c; started = true;
        } else if (c.isSpace()) {
            if (started) { args->append(token); token.clear(); started = false; }
        } else { token += c; started = true; }
    }
    if (!quote.isNull()) { *error = "Arguments contain an unclosed quote."; return false; }
    if (started) args->append(token);
    return true;
}

bool parseNumber(const QString &text, double *number) {
    static const QRegularExpression numberPattern(R"(^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$)");
    const auto trimmed = text.trimmed();
    if (!numberPattern.match(trimmed).hasMatch()) return false;
    bool ok = false;
    *number = QLocale::c().toDouble(trimmed, &ok);
    return ok && std::isfinite(*number);
}

QString validate(const Panel &panel) {
    if (panel.name.trimmed().isEmpty()) return "Give the panel a name.";
    if (!legalText(panel.name)) return "The panel name contains an invalid XML character.";
    if (panel.tabs.isEmpty() || panel.tabs.size() > maxTabs) return "A panel needs between 1 and 100 tabs.";
    int count = 0;
    for (const auto &tab : panel.tabs) {
        if (tab.name.trimmed().isEmpty()) return "Give every tab a name.";
        if (!legalText(tab.name)) return "A tab name contains an invalid XML character.";
        for (const auto &button : tab.buttons) {
            if (++count > maxButtons) return "A panel supports at most 2000 buttons.";
            const auto prefix = "Tab “" + tab.name + "”, button “" + button.name + "”: ";
            if (button.name.trimmed().isEmpty()) return "Give every button a name.";
            if (button.path.trimmed().isEmpty()) return prefix + "choose a file path.";
            for (const auto &field : {button.name, button.path, button.args, button.value, button.toolTip})
                if (!legalText(field)) return prefix + "a field contains an invalid XML character.";
            if (actionName(button.action).isEmpty()) return prefix + "unknown action.";
            if (button.action == Action::Run) {
                QStringList args; QString error;
                if (!splitArguments(button.args, &args, &error)) return prefix + error;
            }
            if (button.action == Action::StoreIncrement && !button.value.isEmpty()) {
                double number;
                if (!parseNumber(button.value, &number)) return prefix + "increment must be a finite number (for example 1, -1 or 0.5).";
            }
            if (button.action != Action::Run && !button.args.isEmpty()) return prefix + "only run actions accept arguments.";
            if (button.action != Action::StoreIncrement && !button.value.isEmpty()) return prefix + "only storeincrement actions accept a value.";
        }
    }
    return {};
}

bool parseXml(const QByteArray &xml, Panel *panel, QString *error) {
    if (xml.size() > maxXmlBytes) { *error = "Panel files must be no larger than 4 MiB."; return false; }
    QXmlStreamReader reader(xml);
    // DTDs are unnecessary for this format; reject them before parsing content.
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
        if (reader.isDTD()) reader.raiseError("DTDs and external entities are not supported.");
    }
    Panel candidate;
    bool named = false;
    int buttonCount = 0;
    if (!reader.isStartElement() || reader.name() != u"ControlPanel") reader.raiseError("Expected <ControlPanel> as the root.");
    else if (!reader.attributes().isEmpty() || !reader.namespaceUri().isEmpty()) reader.raiseError("Root attributes and namespaces are not supported.");
    else {
        while (reader.readNextStartElement()) {
            if (!reader.attributes().isEmpty() || !reader.namespaceUri().isEmpty()) { reader.raiseError("Attributes and namespaces are not supported."); break; }
            if (reader.name() == u"name" && !named) { candidate.name = reader.readElementText(); named = true; }
            else if (reader.name() == u"Tab") {
                if (candidate.tabs.size() >= maxTabs) { reader.raiseError("At most 100 tabs are supported."); break; }
                candidate.tabs.append(readTab(reader, buttonCount));
            } else reader.raiseError("Expected one <name> and <Tab> elements in ControlPanel.");
        }
        while (!reader.atEnd()) reader.readNext(); // Detect trailing roots / malformed trailing XML.
    }
    if (reader.hasError()) {
        *error = QString("Line %1, column %2: %3").arg(reader.lineNumber()).arg(reader.columnNumber()).arg(reader.errorString());
        return false;
    }
    *error = validate(candidate);
    if (!error->isEmpty()) return false;
    *panel = candidate;
    return true;
}

QByteArray toXml(const Panel &panel) {
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("ControlPanel");
    writer.writeTextElement("name", panel.name);
    for (const auto &tab : panel.tabs) {
        writer.writeStartElement("Tab");
        writer.writeTextElement("name", tab.name);
        for (const auto &button : tab.buttons) {
            writer.writeStartElement("button");
            writer.writeTextElement("name", button.name);
            writer.writeTextElement("action", actionName(button.action));
            writer.writeTextElement("path", button.path);
            if (!button.args.isEmpty()) writer.writeTextElement("args", button.args);
            if (!button.value.isEmpty()) writer.writeTextElement("value", button.value);
            if (!button.toolTip.isEmpty()) writer.writeTextElement("ToolTip", button.toolTip);
            writer.writeEndElement();
        }
        writer.writeEndElement();
    }
    writer.writeEndElement();
    writer.writeEndDocument();
    return xml;
}

bool loadPanel(const QString &path, Panel *panel, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { *error = file.errorString(); return false; }
    if (file.size() > maxXmlBytes) { *error = "Panel files must be no larger than 4 MiB."; return false; }
    const auto bytes = file.read(maxXmlBytes + 1);
    if (file.error() != QFileDevice::NoError) { *error = file.errorString(); return false; }
    return parseXml(bytes, panel, error);
}

bool savePanel(const QString &path, const Panel &panel, QString *error) {
    *error = validate(panel);
    if (!error->isEmpty()) return false;
    const auto data = toXml(panel);
    if (data.size() > maxXmlBytes) { *error = "Panel files must be no larger than 4 MiB."; return false; }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = file.errorString(); return false;
    }
    return true;
}

QString resolvePath(const QString &path, const QString &baseDirectory) {
    QString expanded = path;
    if (expanded == "~") expanded = QDir::homePath();
    else if (expanded.startsWith("~/")) expanded = QDir::homePath() + expanded.mid(1);
    return QDir::cleanPath(QDir(baseDirectory).absoluteFilePath(expanded));
}

Panel newPanel() { return {"My Control Panel", {{"Controls", {}}}}; }
}
