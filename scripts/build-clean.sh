#!/bin/bash
set -euo pipefail
ROOT="$1"; QT_ROOT="$2"; CMAKE="$3"; BUILD="$4"
ARCH="$5"; CMAKE_ARCH="$6"; COMPILER="$7"; PACKAGE="$8"
CTEST="$(/usr/bin/dirname "$CMAKE")/ctest"
echo "Building with $COMPILER"
echo "Qt: $QT_ROOT"
echo "SDK: $SDKROOT"
"$CMAKE" -S "$ROOT" -B "$BUILD" -G 'Unix Makefiles' \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
    -DCMAKE_CXX_COMPILER="$COMPILER" -DCMAKE_MAKE_PROGRAM=/usr/bin/make \
    -DCMAKE_CXX_FLAGS= -DCMAKE_EXE_LINKER_FLAGS= -DCMAKE_SHARED_LINKER_FLAGS= \
    -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG' \
    -DCMAKE_OSX_SYSROOT="$SDKROOT" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DCMAKE_OSX_ARCHITECTURES="$CMAKE_ARCH" \
    -DCMAKE_PREFIX_PATH="$QT_ROOT" -DQt6_DIR="$QT_ROOT/lib/cmake/Qt6" \
    -DCMAKE_IGNORE_PREFIX_PATH='/usr/local;/opt/homebrew;/opt/local' \
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF \
    -DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=OFF
JOBS="$(/usr/sbin/sysctl -n hw.ncpu)"
if [ "$JOBS" -gt 8 ]; then JOBS=8; fi
"$CMAKE" --build "$BUILD" --parallel "$JOBS"
"$CTEST" --test-dir "$BUILD" --output-on-failure
if [ "$PACKAGE" = 0 ]; then echo "Build and tests complete: $BUILD"; exit 0; fi

STAGE="$ROOT/dist/ControlPanel-macos-$ARCH"
# Only replace our own generated package, never an arbitrary user-selected directory.
if [ -e "$STAGE" ]; then /bin/rm -rf "$STAGE"; fi
/bin/mkdir -p "$STAGE"
for APP in 'ControlPanel Interpreter' 'ControlPanel Creator'; do
    /usr/bin/ditto "$BUILD/$APP.app" "$STAGE/$APP.app"
    CONTENTS="$STAGE/$APP.app/Contents"
    /bin/mkdir -p "$CONTENTS/PlugIns/platforms" "$CONTENTS/PlugIns/styles" "$CONTENTS/Resources/Licenses"
    /bin/cp "$ROOT/LICENSE" "$CONTENTS/Resources/Licenses/ControlPanel-GPL-3.0.txt"
    /bin/cp "$ROOT/THIRD_PARTY.md" "$CONTENTS/Resources/Licenses/THIRD_PARTY.md"
    if [ -d "$ROOT/resources/licenses" ]; then /usr/bin/ditto "$ROOT/resources/licenses" "$CONTENTS/Resources/Licenses/Qt"; fi
    /bin/cp "$QT_ROOT/plugins/platforms/libqcocoa.dylib" "$CONTENTS/PlugIns/platforms/"
    /bin/cp "$QT_ROOT/plugins/styles/libqmacstyle.dylib" "$CONTENTS/PlugIns/styles/"
    # This Widgets application needs Cocoa and the native macOS style. Deploy
    # their full dependency closure without unrelated QML/virtual-keyboard plugins.
    cat > "$CONTENTS/Resources/qt.conf" <<'QTCONF'
[Paths]
Plugins = PlugIns
Libraries = Frameworks
QTCONF
    "$QT_ROOT/bin/macdeployqt" "$STAGE/$APP.app" -always-overwrite -verbose=1 -no-plugins -codesign=- \
        "-executable=$CONTENTS/PlugIns/platforms/libqcocoa.dylib" \
        "-executable=$CONTENTS/PlugIns/styles/libqmacstyle.dylib"
    "$CMAKE" "-DAPP_BUNDLE=$STAGE/$APP.app" "-DAPP_NAME=$APP" "-DARCH=$ARCH" -P "$ROOT/cmake/VerifyBundle.cmake"
    "$STAGE/$APP.app/Contents/MacOS/$APP" --validate "$ROOT/examples/Getting Started.controlpanel"
done
/usr/bin/ditto "$ROOT/examples" "$STAGE/Examples"
/bin/cp "$ROOT/README.md" "$ROOT/LICENSE" "$ROOT/THIRD_PARTY.md" "$STAGE/"
if [ -d "$QT_ROOT/licenses" ]; then /usr/bin/ditto "$QT_ROOT/licenses" "$STAGE/Qt-Licenses"; fi
if [ -d "$ROOT/resources/licenses" ]; then /usr/bin/ditto "$ROOT/resources/licenses" "$STAGE/Qt-Licenses"; fi
ARCHIVE="$ROOT/dist/ControlPanel-macos-$ARCH.zip"
if [ -f "$ARCHIVE" ]; then /bin/rm "$ARCHIVE"; fi
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$STAGE" "$ARCHIVE"
(cd "$ROOT/dist" && /usr/bin/shasum -a 256 "ControlPanel-macos-$ARCH.zip" > "ControlPanel-macos-$ARCH.zip.sha256")
echo "Ready: $STAGE"
echo "Archive: $ARCHIVE"
