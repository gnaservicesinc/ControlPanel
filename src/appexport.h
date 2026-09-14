#pragma once
#include "embedded.h"

namespace cp {
QString appExportTemplatePath();
QString appFileName(const QString &panelName);
bool patchEmbeddedExecutable(const QString &executable, const QByteArray &payload, QString *error);
bool exportPanelApp(Panel *panel, const QString &sourceFile, const QString &destination,
                    const QString &templatePath, QString *error);
bool verifyAppSignature(const QString &bundle, QString *error);
QList<QPair<QString, QString>> localSigningIdentities();
}
