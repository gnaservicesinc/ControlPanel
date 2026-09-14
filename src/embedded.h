#pragma once
#include "model.h"

namespace cp {
constexpr qsizetype embeddedCapacity = 8 * 1024 * 1024;
constexpr qsizetype embeddedHeaderSize = 64;
constexpr qsizetype embeddedSlotSize = embeddedHeaderSize + embeddedCapacity;
struct EmbeddedPanel { Panel panel; QString workingDirectory; };
QByteArray encodeEmbeddedPanel(const Panel &panel, const QString &directory, QString *error);
bool decodeEmbeddedPanel(const QByteArray &data, EmbeddedPanel *result, QString *error);
bool readEmbeddedSlot(const char *slot, EmbeddedPanel *result, QString *error);
QString supportInformation(const Panel &panel);
}
