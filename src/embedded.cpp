#include "embedded.h"
#include <QCborArray>
#include <QCborValue>
#include <QCryptographicHash>
#include <QDir>
#include <QtEndian>

namespace cp {
QByteArray encodeEmbeddedPanel(const Panel &panel, const QString &directory, QString *error) {
    *error = validate(panel);
    if (!error->isEmpty()) return {};
    if (!QDir::isAbsolutePath(directory)) { *error = "Save the panel before exporting an app."; return {}; }
    QCborArray tabs;
    for (const auto &tab : panel.tabs) {
        QCborArray buttons;
        for (const auto &b : tab.buttons) {
            // Freeze relative targets at export time; preserve ~/ for the user
            // running the app. Runtime never needs the source XML to resolve paths.
            const auto path = b.path == "~" || b.path.startsWith("~/") ? b.path : resolvePath(b.path, directory);
            buttons.append(QCborArray{b.name, int(b.action), path, b.args, b.value, b.toolTip});
        }
        tabs.append(QCborArray{tab.name, buttons});
    }
    auto bytes = QCborValue(QCborArray{1, panel.name, panel.uuid, panel.appExport.version,
        panel.appExport.contact, panel.appExport.description, directory, tabs}).toCbor();
    if (bytes.size() > embeddedCapacity) { *error = "This panel is too large to embed in an app."; return {}; }
    return bytes;
}

bool decodeEmbeddedPanel(const QByteArray &data, EmbeddedPanel *result, QString *error) {
    *error = "The embedded panel is invalid or from an unsupported export format.";
    if (data.isEmpty() || data.size() > embeddedCapacity) return false;
    QCborParserError parseError;
    const auto value = QCborValue::fromCbor(data, &parseError);
    if (parseError.error != QCborError::NoError || parseError.offset != data.size() || !value.isArray()) return false;
    const auto root = value.toArray();
    if (root.size() != 8 || root[0] != QCborValue(1) || !root[7].isArray()) return false;
    for (int i = 1; i <= 6; ++i) if (!root[i].isString()) return false;
    EmbeddedPanel candidate;
    candidate.panel.name = root[1].toString(); candidate.panel.uuid = root[2].toString();
    candidate.panel.appExport.version = root[3].toString(); candidate.panel.appExport.contact = root[4].toString();
    candidate.panel.appExport.description = root[5].toString(); candidate.workingDirectory = root[6].toString();
    if (!QDir::isAbsolutePath(candidate.workingDirectory) || candidate.workingDirectory.contains(QChar::Null)) return false;
    const auto tabs = root[7].toArray();
    if (tabs.isEmpty() || tabs.size() > 100) return false;
    int count = 0;
    for (const auto &t : tabs) {
        if (!t.isArray()) return false;
        const auto tabData = t.toArray();
        if (tabData.size() != 2 || !tabData[0].isString() || !tabData[1].isArray()) return false;
        Tab tab{tabData[0].toString(), {}};
        for (const auto &b : tabData[1].toArray()) {
            if (++count > 2000 || !b.isArray()) return false;
            const auto fields = b.toArray();
            if (fields.size() != 6 || !fields[1].isInteger() || fields[1].toInteger() < 0 || fields[1].toInteger() > 2) return false;
            for (int i : {0, 2, 3, 4, 5}) if (!fields[i].isString()) return false;
            tab.buttons.append({fields[0].toString(), Action(fields[1].toInteger()), fields[2].toString(),
                fields[3].toString(), fields[4].toString(), fields[5].toString()});
        }
        candidate.panel.tabs.append(tab);
    }
    *error = validate(candidate.panel);
    if (!error->isEmpty()) return false;
    *result = candidate;
    return true;
}

bool readEmbeddedSlot(const char *slot, EmbeddedPanel *result, QString *error) {
    const auto size = qFromBigEndian<quint32>(slot + 24);
    if (size == 0 || size > embeddedCapacity) { *error = "This app has no valid embedded panel. Export it again from Creator."; return false; }
    const auto data = QByteArray::fromRawData(slot + embeddedHeaderSize, size);
    if (QCryptographicHash::hash(data, QCryptographicHash::Sha256) != QByteArray(slot + 32, 32)) {
        *error = "The embedded panel has been changed or damaged. Ask your support contact for a fresh copy of the app.";
        return false;
    }
    return decodeEmbeddedPanel(data, result, error);
}

QString supportInformation(const Panel &panel) {
    QString text = "App: " + panel.name + "\nVersion: " + (panel.appExport.version.isEmpty() ? "Not set" : panel.appExport.version)
        + "\nPanel ID (UUID): " + panel.uuid;
    if (!panel.appExport.contact.trimmed().isEmpty()) text += "\n\nSupport contact:\n" + panel.appExport.contact;
    if (!panel.appExport.description.trimmed().isEmpty()) text += "\n\n" + panel.appExport.description;
    return text + "\n\nBuilt with ControlPanel " CP_VERSION;
}
}
