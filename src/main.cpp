#include "application.h"
#include "windows.h"
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv) {
    cp::Application app(argc, argv);
#ifdef CP_CREATOR
    app.setApplicationName("ControlPanel Creator");
#else
    app.setApplicationName("ControlPanel Interpreter");
#endif
    app.setApplicationVersion(CP_VERSION);
    app.setOrganizationName("GNA Services");
    app.setOrganizationDomain("gnaservices.com");
    QCommandLineParser parser;
    parser.setApplicationDescription("Create and run tabbed XML control panels.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("panel", "A .controlpanel or XML file to open.", "[panel]");
    QCommandLineOption check("validate", "Validate the panel without opening a window or running actions.");
    parser.addOption(check); parser.process(app);
    const auto positional = parser.positionalArguments();
    if (positional.size() > 1 || (parser.isSet(check) && positional.size() != 1)) parser.showHelp(2);
    if (parser.isSet(check)) {
        cp::Panel panel; QString error;
        if (!cp::loadPanel(positional[0], &panel, &error)) { fprintf(stderr, "%s\n", qPrintable(error)); return 1; }
        printf("Valid panel: %s (%lld tabs)\n", qPrintable(panel.name), static_cast<long long>(panel.tabs.size()));
        return 0;
    }
    cp::configureAppearance();
#ifdef CP_CREATOR
    cp::CreatorWindow window;
#else
    cp::InterpreterWindow window;
#endif
    app.openFile = [&window](const QString &path) { window.openFile(path); window.show(); window.raise(); window.activateWindow(); };
    window.show();
    if (!positional.isEmpty()) QTimer::singleShot(0, &window, [&window, positional] { window.openFile(positional[0]); });
    return app.exec();
}
