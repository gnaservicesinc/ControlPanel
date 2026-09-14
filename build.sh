#!/bin/bash
# Deliberately use macOS's Bash and Apple tools, never login-shell startup files.
set -euo pipefail
ROOT="$(cd "$(/usr/bin/dirname "$0")" && /bin/pwd)"
QT_ROOT="${CONTROLPANEL_QT_ROOT:-/opt/Qt/6.11.2/macos}"
CMAKE="${CONTROLPANEL_CMAKE:-}"
BUILD="$ROOT/build"
ARCH=arm64
PACKAGE=1
while [ "$#" -gt 0 ]; do
    case "$1" in
        --qt|--cmake|--build-dir|--arch)
            if [ "$#" -lt 2 ]; then echo "Missing value for $1" >&2; exit 2; fi
            case "$1" in
                --qt) QT_ROOT="$2";; --cmake) CMAKE="$2";;
                --build-dir) BUILD="$2";; --arch) ARCH="$2";;
            esac
            shift 2;;
        --no-package) PACKAGE=0; shift;;
        --help|-h)
            echo "Usage: ./build.sh [--qt /path/to/Qt/macos] [--cmake /absolute/path/to/cmake]"
            echo "                  [--build-dir path] [--arch arm64] [--no-package]"
            echo "Builds both applications, runs tests, and packages self-contained apps in dist/."
            exit 0;;
        *) echo "Unknown option: $1 (see --help)" >&2; exit 2;;
    esac
done
if [ "$(/usr/bin/uname -s)" != Darwin ]; then echo "This helper requires macOS. See README.md for plain CMake builds." >&2; exit 1; fi
if [ "$ARCH" != arm64 ]; then echo "ControlPanel targets Apple Silicon (arm64) only." >&2; exit 2; fi
CMAKE_ARCH=arm64
if [ -z "$CMAKE" ]; then
    for candidate in /opt/Qt/Tools/CMake/CMake.app/Contents/bin/cmake /Applications/CMake.app/Contents/bin/cmake; do
        if [ -x "$candidate" ]; then CMAKE="$candidate"; break; fi
    done
fi
case "$CMAKE" in /*) ;; *) echo "Install CMake via the Qt installer or CMake.app, or pass --cmake with an absolute path." >&2; exit 1;; esac
if [ ! -x "$CMAKE" ]; then echo "CMake is not executable: $CMAKE" >&2; exit 1; fi
if [ ! -f "$QT_ROOT/lib/cmake/Qt6/Qt6Config.cmake" ]; then echo "Qt was not found at $QT_ROOT. Use --qt to select a Qt 6.8+ macOS installation." >&2; exit 1; fi
QT_ROOT="$(cd "$QT_ROOT" && /bin/pwd)"
/bin/mkdir -p "$BUILD"
BUILD="$(cd "$BUILD" && /bin/pwd)"
DEVELOPER="$(/usr/bin/env -i PATH=/usr/bin:/bin /usr/bin/xcode-select -p)"
SDK="$(/usr/bin/env -i PATH=/usr/bin:/bin DEVELOPER_DIR="$DEVELOPER" /usr/bin/xcrun --sdk macosx --show-sdk-path)"
COMPILER="$(/usr/bin/env -i PATH=/usr/bin:/bin DEVELOPER_DIR="$DEVELOPER" /usr/bin/xcrun --find clang++)"
# All build, test, and deploy commands cross the same environment boundary.
# No inherited compiler flags, package paths, Python setup, DYLD, or Qt plugins.
exec /usr/bin/env -i \
    HOME="$HOME" USER="$(/usr/bin/id -un)" TMPDIR="${TMPDIR:-/tmp}" \
    LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 \
    PATH="/usr/bin:/bin:/usr/sbin:/sbin" \
    DEVELOPER_DIR="$DEVELOPER" SDKROOT="$SDK" \
    /bin/bash "$ROOT/scripts/build-clean.sh" "$ROOT" "$QT_ROOT" "$CMAKE" "$BUILD" "$ARCH" "$CMAKE_ARCH" "$COMPILER" "$PACKAGE"
