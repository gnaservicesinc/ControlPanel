# Third-party components

ControlPanel is licensed under GNU GPL v3; see LICENSE.

The macOS packages include dynamically linked Qt Core, Gui and Widgets frameworks
and the Cocoa and native style plugins from the selected Qt installation. Qt is copyright The Qt Company Ltd.
and other contributors and is available under commercial and open-source
licenses, including LGPL v3 / GPL v3. Qt's bundled third-party components have
their own licenses. Qt Base's license texts are included in `resources/licenses`
and copied inside each app under `Contents/Resources/Licenses`. The source code
is unmodified; deployment adjusts dynamic library paths and ad-hoc signatures.
Official notices and corresponding source downloads:

- https://doc.qt.io/qt-6/licensing.html
- https://download.qt.io/official_releases/qt/

Use the source archive matching the Qt version selected during the build (the
default is 6.11.2). The application does not statically link Qt. Users can rebuild
ControlPanel with a compatible replacement Qt installation using `--qt`.
