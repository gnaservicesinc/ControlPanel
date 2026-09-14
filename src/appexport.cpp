#include "appexport.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QXmlStreamWriter>
#include <QtEndian>
#ifdef Q_OS_MACOS
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#endif
#include <cstring>

namespace cp {
namespace {
bool runTool(const QString &program, const QStringList &args, QString *error, QByteArray *output = nullptr) {
    QProcess process;
    QProcessEnvironment environment;
    for (const auto &key : {"HOME", "USER", "TMPDIR"})
        if (qEnvironmentVariableIsSet(key)) environment.insert(key, qEnvironmentVariable(key));
    environment.insert("PATH", "/usr/bin:/bin:/usr/sbin:/sbin");
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, args);
    if (!process.waitForStarted(10000)) { *error = "Could not start " + program + ": " + process.errorString(); return false; }
    if (!process.waitForFinished(120000)) {
        process.kill(); process.waitForFinished(); *error = program + " timed out."; return false;
    }
    const auto bytes = process.readAll();
    if (output) *output = bytes;
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = program + " failed:\n" + QString::fromUtf8(bytes).right(8000); return false;
    }
    return true;
}
bool writeFile(const QString &path, const QByteArray &data, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = "Could not write " + path + ": " + file.errorString(); return false;
    }
    return true;
}
QByteArray appPlist(const Panel &panel) {
    QByteArray data;
    QXmlStreamWriter writer(&data); writer.setAutoFormatting(true); writer.writeStartDocument();
    writer.writeStartElement("plist"); writer.writeAttribute("version", "1.0"); writer.writeStartElement("dict");
    const auto field = [&writer](const QString &key, const QString &value) {
        writer.writeTextElement("key", key); writer.writeTextElement("string", value);
    };
    field("CFBundleExecutable", "PanelApp");
    field("CFBundleName", panel.name); field("CFBundleDisplayName", panel.name);
    field("CFBundleIdentifier", "com.gnaservices.controlpanel.export.p" + panel.uuid.toLower());
    field("CFBundleVersion", panel.appExport.version.isEmpty() ? "1.0.0" : panel.appExport.version);
    field("CFBundleShortVersionString", panel.appExport.version.isEmpty() ? "1.0.0" : panel.appExport.version);
    field("CPPanelUUID", panel.uuid); field("CFBundleIconFile", "PanelIcon.icns");
    field("CFBundlePackageType", "APPL"); field("NSPrincipalClass", "NSApplication");
    field("LSMinimumSystemVersion", "13.0");
    writer.writeTextElement("key", "NSHighResolutionCapable"); writer.writeEmptyElement("true");
    writer.writeEndElement(); writer.writeEndElement(); writer.writeEndDocument();
    return data;
}
bool makeIcon(const QString &source, const QString &destination, const QString &scratch, QString *error) {
    if (source.isEmpty()) return true; // Keep the built-in template icon.
    if (QFileInfo(source).suffix().compare("icns", Qt::CaseInsensitive) == 0) {
        QFile file(source);
        if (!file.open(QIODevice::ReadOnly)) { *error = "Could not read the app icon."; return false; }
        const auto data = file.read(16 * 1024 * 1024 + 1);
        if (data.size() < 8 || data.size() > 16 * 1024 * 1024 || !data.startsWith("icns")
            || qFromBigEndian<quint32>(data.constData() + 4) != quint32(data.size())) {
            *error = "Choose a valid ICNS icon, PNG, or JPEG image."; return false;
        }
        return writeFile(destination, data, error);
    }
    QImage image(source);
    if (image.isNull()) { *error = "Could not read the app icon. Choose an ICNS, PNG, or JPEG image."; return false; }
    const auto iconset = scratch + "/Panel.iconset";
    if (!QDir().mkpath(iconset)) { *error = "Could not create the icon workspace."; return false; }
    for (int size : {16, 32, 128, 256, 512}) for (int scale : {1, 2}) {
        const int pixels = size * scale;
        const auto scaled = image.scaled(pixels, pixels, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QImage square(pixels, pixels, QImage::Format_ARGB32); square.fill(Qt::transparent);
        // Center a non-square image without cropping it.
        QPainter painter(&square);
        painter.drawImage((pixels - scaled.width()) / 2, (pixels - scaled.height()) / 2, scaled); painter.end();
        const auto path = QString("%1/icon_%2x%2%3.png").arg(iconset).arg(size).arg(scale == 2 ? "@2x" : "");
        if (!square.save(path)) { *error = "Could not create the app icon."; return false; }
    }
    return runTool("/usr/bin/iconutil", {"-c", "icns", iconset, "-o", destination}, error);
}
}

QString appFileName(const QString &panelName) {
    QString name = panelName.trimmed();
    name.replace(QRegularExpression("[/\\\\:\\x00-\\x1f]"), "-");
    while (name.startsWith('.')) name.remove(0, 1);
    if (name.isEmpty()) name = "Control Panel";
    // Leave room for .app and keep each filename below macOS's UTF-8 limit.
    while (name.toUtf8().size() > 180) name.chop(1);
    return name + ".app";
}

QString appExportTemplatePath() {
    const auto embedded = QCoreApplication::applicationDirPath() + "/../Resources/AppExport/Panel Runtime.app";
    if (QFileInfo::exists(embedded)) return QDir::cleanPath(embedded);
    // Development builds keep the template next to the two app bundles.
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../../../Panel Runtime.app");
}

QList<QPair<QString, QString>> localSigningIdentities() {
    QList<QPair<QString, QString>> identities{{"Local use (ad-hoc signing)", "-"}};
#ifdef Q_OS_MACOS
    QString error; QByteArray output;
    if (runTool("/usr/bin/security", {"find-identity", "-v", "-p", "codesigning"}, &error, &output)) {
        const QRegularExpression pattern("([0-9A-Fa-f]{40}) \\\"([^\\\"]+)\\\"");
        auto matches = pattern.globalMatch(QString::fromUtf8(output));
        while (matches.hasNext()) { const auto match = matches.next(); identities.append({match.captured(2), match.captured(1)}); }
    }
#endif
    return identities;
}

bool patchEmbeddedExecutable(const QString &executable, const QByteArray &payload, QString *error) {
#ifdef Q_OS_MACOS
    if (payload.isEmpty() || payload.size() > embeddedCapacity) { *error = "Invalid embedded panel size."; return false; }
    QFile file(executable);
    if (!file.open(QIODevice::ReadWrite)) { *error = file.errorString(); return false; }
    const auto data = file.readAll();
    QList<QPair<quint64, quint64>> slices;
    if (data.size() < 8) { *error = "Invalid runtime template."; return false; }
    const auto magic = qFromBigEndian<quint32>(data.constData());
    if (magic == FAT_MAGIC || magic == FAT_MAGIC_64) {
        const quint32 count = qFromBigEndian<quint32>(data.constData() + 4);
        const quint64 entrySize = magic == FAT_MAGIC_64 ? 32 : 20;
        if (count == 0 || count > 8 || 8 + count * entrySize > quint64(data.size())) { *error = "Invalid universal runtime."; return false; }
        for (quint32 i = 0; i < count; ++i) {
            const auto entry = data.constData() + 8 + i * entrySize;
            const quint64 offset = magic == FAT_MAGIC_64 ? qFromBigEndian<quint64>(entry + 8) : qFromBigEndian<quint32>(entry + 8);
            const quint64 size = magic == FAT_MAGIC_64 ? qFromBigEndian<quint64>(entry + 16) : qFromBigEndian<quint32>(entry + 12);
            slices.append({offset, size});
        }
    } else slices.append({0, quint64(data.size())});
    QList<quint64> sectionOffsets;
    for (const auto &slice : slices) {
        *error = "The runtime template has an invalid or missing embedded panel section.";
        const quint64 base = slice.first, size = slice.second;
        if (base > quint64(data.size()) || size > quint64(data.size()) - base || size < sizeof(mach_header_64)) return false;
        const auto start = data.constData() + base;
        if (qFromLittleEndian<quint32>(start) != MH_MAGIC_64) return false;
        const auto count = qFromLittleEndian<quint32>(start + 16);
        const auto commandBytes = qFromLittleEndian<quint32>(start + 20);
        quint64 position = sizeof(mach_header_64);
        if (commandBytes > size - position || count > commandBytes / 8) return false;
        const auto end = position + commandBytes;
        int found = 0;
        for (quint32 i = 0; i < count; ++i) {
            if (position + 8 > end) return false;
            const auto command = start + position;
            const auto type = qFromLittleEndian<quint32>(command);
            const auto length = qFromLittleEndian<quint32>(command + 4);
            if (length < 8 || length > end - position) return false;
            if (type == LC_SEGMENT_64) {
                if (length < sizeof(segment_command_64)) return false;
                const auto sections = qFromLittleEndian<quint32>(command + 64);
                if (sections > (length - sizeof(segment_command_64)) / sizeof(section_64)) return false;
                for (quint32 s = 0; s < sections; ++s) {
                    const auto section = command + sizeof(segment_command_64) + s * sizeof(section_64);
                    if (std::strncmp(section, "__cp_panel", 16) || std::strncmp(section + 16, "__TEXT", 16)) continue;
                    const auto sectionSize = qFromLittleEndian<quint64>(section + 40);
                    const auto offset = qFromLittleEndian<quint32>(section + 48);
                    if (sectionSize != quint64(embeddedSlotSize) || offset > size || sectionSize > size - offset) return false;
                    if (QByteArray(start + offset, 16) != QByteArray("CP_PANEL_SLOT_V1", 16)) return false;
                    // Templates must be blank; never patch a previously exported app.
                    for (qsizetype j = 16; j < embeddedSlotSize; ++j) if (start[offset + j] != 0) return false;
                    sectionOffsets.append(base + offset); ++found;
                }
            }
            position += length;
        }
        if (found != 1) return false;
    }
    QByteArray header(embeddedHeaderSize, 0);
    std::memcpy(header.data(), "CP_PANEL_SLOT_V1", 16);
    qToBigEndian<quint32>(payload.size(), header.data() + 24);
    const auto digest = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    std::memcpy(header.data() + 32, digest.constData(), 32);
    for (auto offset : sectionOffsets) {
        if (!file.seek(offset) || file.write(header) != header.size() || file.write(payload) != payload.size()) {
            *error = "Could not embed the panel: " + file.errorString(); return false;
        }
    }
    if (!file.flush()) { *error = file.errorString(); return false; }
    error->clear(); return true;
#else
    Q_UNUSED(executable); Q_UNUSED(payload); *error = "App export requires macOS."; return false;
#endif
}

bool exportPanelApp(Panel *panel, const QString &sourceFile, const QString &destination,
                    const QString &templatePath, QString *error) {
#ifndef Q_OS_MACOS
    Q_UNUSED(panel); Q_UNUSED(sourceFile); Q_UNUSED(destination); Q_UNUSED(templatePath);
    *error = "App export requires macOS."; return false;
#else
    *error = validate(*panel);
    if (!error->isEmpty()) return false;
    const auto source = QFileInfo(sourceFile).absoluteFilePath();
    const auto dest = QFileInfo(destination).absoluteFilePath();
    if (!dest.endsWith(".app", Qt::CaseInsensitive) || QFileInfo(dest).isSymLink()) {
        *error = "Choose an app destination ending in .app, not a symbolic link."; return false;
    }
    const auto canonicalDest = QFileInfo(dest).canonicalFilePath();
    const auto canonicalSource = QFileInfo(source).canonicalFilePath();
    const auto canonicalTemplate = QFileInfo(templatePath).canonicalFilePath();
    if (!canonicalDest.isEmpty() && (canonicalSource.startsWith(canonicalDest + "/")
        || canonicalTemplate == canonicalDest || canonicalTemplate.startsWith(canonicalDest + "/"))) {
        *error = "Choose a destination outside the source document and Creator’s runtime bundle."; return false;
    }
    QLockFile lock(source + ".export.lock");
    if (!lock.tryLock(0)) { *error = "This panel is already being exported, or its folder is not writable."; return false; }
    Panel saved;
    if (!loadPanel(source, &saved, error)) return false;
    if (toXml(saved) != toXml(*panel)) { *error = "The panel file changed. Save or reopen it before exporting."; return false; }
    Panel next = *panel;
    if (next.appExport.autoIncrement && !next.appExport.version.isEmpty()) {
        next.appExport.version = nextExportVersion(next.appExport.version);
        if (next.appExport.version.isEmpty()) { *error = "This version cannot be incremented. Set another version or turn off automatic incrementing."; return false; }
    }
    const auto directory = QFileInfo(source).absolutePath();
    const auto payload = encodeEmbeddedPanel(*panel, directory, error);
    if (payload.isEmpty()) return false;
    const auto templateBundle = QFileInfo(templatePath).canonicalFilePath();
    if (templateBundle.isEmpty() || !QFileInfo::exists(templateBundle + "/Contents/Frameworks/QtCore.framework")) {
        *error = "The self-contained app runtime is missing. Build and package ControlPanel with build.sh, then open Creator from dist."; return false;
    }
    if (!verifyAppSignature(templateBundle, error)) return false;
    if (QFileInfo::exists(dest)) {
        QByteArray uuid;
        if (!runTool("/usr/bin/plutil", {"-extract", "CPPanelUUID", "raw", "-o", "-", dest + "/Contents/Info.plist"}, error, &uuid)
            || QString::fromUtf8(uuid).trimmed() != panel->uuid) {
            *error = "That destination is not an exported app from this panel. Choose another name or folder."; return false;
        }
    }
    QTemporaryDir scratch(QFileInfo(dest).absolutePath() + "/.controlpanel-export-XXXXXX");
    if (!scratch.isValid()) { *error = "The export folder is not writable."; return false; }
    const auto staged = scratch.filePath("Export.app");
    if (!runTool("/usr/bin/ditto", {templateBundle, staged}, error)) return false;
    const auto contents = staged + "/Contents";
    if (!QFile::rename(contents + "/MacOS/Panel Runtime", contents + "/MacOS/PanelApp")) {
        *error = "Could not prepare the app executable."; return false;
    }
    if (!patchEmbeddedExecutable(contents + "/MacOS/PanelApp", payload, error)
        || !writeFile(contents + "/Info.plist", appPlist(*panel), error)) return false;
    const auto icon = panel->appExport.iconPath.isEmpty() ? QString() : resolvePath(panel->appExport.iconPath, directory);
    if (!makeIcon(icon, contents + "/Resources/PanelIcon.icns", scratch.path(), error)) return false;
    // All template dependencies are sealed. Deep signing is limited to this freshly
    // copied, verified bundle and allows a locally selected development identity.
    if (!runTool("/usr/bin/codesign", {"--force", "--deep", "--sign", panel->appExport.signingIdentity,
                                     "--timestamp=none", staged}, error)
        || !verifyAppSignature(staged, error)) return false;
    Panel latest;
    if (!loadPanel(source, &latest, error) || toXml(latest) != toXml(saved)) {
        *error = "The source panel changed during export. Reopen it and export again."; return false;
    }
    // Publish only a complete, signed bundle. Keep the old app until saving the
    // next version succeeds, so failed exports do not consume a version.
    const auto previous = scratch.filePath("Previous.app");
    const bool hadPrevious = QFileInfo::exists(dest);
    if (hadPrevious && !QDir().rename(dest, previous)) { *error = "Could not move the previous exported app. Close it and retry."; return false; }
    if (!QDir().rename(staged, dest)) {
        *error = "Could not publish the app.";
        if (hadPrevious && !QDir().rename(previous, dest)) {
            scratch.setAutoRemove(false); *error += " The previous app is preserved at " + previous;
        }
        return false;
    }
    if (!savePanel(source, next, error)) {
        const auto saveError = *error;
        const bool moved = QDir().rename(dest, staged);
        const bool restored = !hadPrevious || (moved && QDir().rename(previous, dest));
        *error = "Could not save the next app version: " + saveError;
        if (!moved || !restored) {
            scratch.setAutoRemove(false); *error += " Recovery copies are preserved in " + scratch.path();
        }
        return false;
    }
    *panel = next; error->clear(); return true;
#endif
}
}
