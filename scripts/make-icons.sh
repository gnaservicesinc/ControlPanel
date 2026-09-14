#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(/usr/bin/dirname "$0")/.." && /bin/pwd)"
TEMP="$(/usr/bin/mktemp -d)"
trap '/bin/rm -rf "$TEMP"' EXIT
/usr/bin/env -i HOME="$HOME" PATH=/usr/bin:/bin:/usr/sbin:/sbin /usr/bin/xcrun swift "$ROOT/scripts/make-icons.swift" "$TEMP"
for APP in Interpreter Creator; do
    /usr/bin/iconutil -c icns "$TEMP/$APP.iconset" -o "$ROOT/resources/$APP.icns"
done
