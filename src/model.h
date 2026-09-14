#pragma once

#include <QList>
#include <QString>
#include <QStringList>

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
struct Panel { QString name; QList<Tab> tabs; };

QString actionName(Action action);
bool splitArguments(const QString &text, QStringList *args, QString *error);
bool parseNumber(const QString &text, double *number);
QString validate(const Panel &panel);
bool parseXml(const QByteArray &xml, Panel *panel, QString *error);
QByteArray toXml(const Panel &panel);
bool loadPanel(const QString &path, Panel *panel, QString *error);
bool savePanel(const QString &path, const Panel &panel, QString *error);
QString resolvePath(const QString &path, const QString &baseDirectory);
Panel newPanel();
}
