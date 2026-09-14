#include "application.h"
#include "appexport.h"
#include "runtimewindow.h"
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <cstdio>
#ifdef Q_OS_MACOS
#include <mach-o/dyld.h>
#endif

extern const char cpEmbeddedPanel[cp::embeddedSlotSize];

int main(int argc, char **argv) {
    // Reject filenames AND Qt command-line overrides before constructing QApplication.
    const bool support = argc == 2 && QByteArray(argv[1]) == "--support-info";
    const bool version = argc == 2 && QByteArray(argv[1]) == "--version";
    const bool verify = argc == 2 && QByteArray(argv[1]) == "--verify";
    if (argc > 1 && !support && !version && !verify) {
        fprintf(stderr, "This app only runs its embedded control panel. It does not open panel files.\n"); return 2;
    }
    for (const auto &entry : QProcessEnvironment::systemEnvironment().keys())
        if (entry.startsWith("QT_") || entry.startsWith("QML_") || entry.startsWith("DYLD_")) qunsetenv(entry.toUtf8().constData());
    QString error;
#ifdef Q_OS_MACOS
    uint32_t size = 0; _NSGetExecutablePath(nullptr, &size);
    QByteArray path(size, 0);
    if (_NSGetExecutablePath(path.data(), &size) != 0) return 1;
    const auto bundle = QDir::cleanPath(QFileInfo(QFileInfo(QString::fromUtf8(path.constData())).canonicalFilePath()).absolutePath() + "/../..");
    QCoreApplication::setLibraryPaths({bundle + "/Contents/PlugIns"});
    const bool signedApp = cp::verifyAppSignature(bundle, &error);
#else
    const bool signedApp = false; error = "Exported control panel apps require macOS.";
#endif
    cp::EmbeddedPanel embedded;
    if (!signedApp || !cp::readEmbeddedSlot(cpEmbeddedPanel, &embedded, &error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        if (argc == 1) {
            cp::Application app(argc, argv);
            QMessageBox::critical(nullptr, "Could not open control panel", error);
        }
        return 1;
    }
    if (support || verify) { printf("%s\n", qPrintable(cp::supportInformation(embedded.panel))); return 0; }
    if (version) { printf("%s\n", qPrintable(embedded.panel.appExport.version.isEmpty() ? "Not set" : embedded.panel.appExport.version)); return 0; }
    cp::Application app(argc, argv);
    app.setApplicationName(embedded.panel.name); app.setApplicationDisplayName(embedded.panel.name);
    app.setApplicationVersion(embedded.panel.appExport.version);
    app.setOrganizationName("ControlPanel Export"); app.setOrganizationDomain("gnaservices.com");
    cp::configureAppearance();
    cp::RuntimeWindow window(embedded.panel, embedded.workingDirectory);
    // Application consumes Finder FileOpen events; no openFile callback is installed.
    window.show(); return app.exec();
}
