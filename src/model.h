#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace cp {
enum class Action { Touch, StoreIncrement, Run };
struct Button {
    QString name;
    Action action = Action::Touch;
    QString path;
    QString args;
    QString value; // Empty means +1. Kept as text to preserve authored decimals.
    QString toolTip;
};
struct Tab { QString name; QList<Button> buttons; };
struct AppExportInfo {
    QString version = "1.0.0"; // Version to use on the next export; empty hides it in About.
    bool autoIncrement = true;
    QString iconPath;
    QString contact;
    QString description;
    QString signingIdentity = "-"; // Ad-hoc local signing, or a keychain identity SHA-1.
};
struct Panel {
    QString name;
    QList<Tab> tabs;
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    AppExportInfo appExport{};
};

QString actionName(Action action);
bool splitArguments(const QString &text, QStringList *args, QString *error);
bool parseNumber(const QString &text, double *number);
QString validate(const Panel &panel);
bool parseXml(const QByteArray &xml, Panel *panel, QString *error);
QByteArray toXml(const Panel &panel);
// Read-only callers (including --validate) do not modify their input.
bool loadPanel(const QString &path, Panel *panel, QString *error, bool *needsUuid = nullptr);
bool loadPanelForEditing(const QString &path, Panel *panel, QString *error);
QString nextExportVersion(const QString &version);
bool savePanel(const QString &path, const Panel &panel, QString *error);
QString resolvePath(const QString &path, const QString &baseDirectory);
Panel newPanel();
}
