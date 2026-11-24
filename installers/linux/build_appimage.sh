#!/usr/bin/env bash
# Builds LUMAFLOW-x86_64.AppImage. Requires linuxdeploy, linuxdeploy-plugin-qt and
# appimagetool on PATH or in $TOOLS (default /tmp/tools).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${BUILD:-$ROOT/build-release}"
TOOLS="${TOOLS:-/tmp/tools}"
OUT="${OUT:-$ROOT/releases/3.0.0}"
APPDIR="$BUILD/AppDir"
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD" --prefix /usr
export APPIMAGE_EXTRACT_AND_RUN=1 QMAKE=/usr/bin/qmake6 EXTRA_QT_PLUGINS="xcbglintegrations" NO_STRIP=1
"$TOOLS/linuxdeploy" --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/lumaflow" --executable "$APPDIR/usr/bin/lumaflow-cli" \
  --desktop-file "$ROOT/installers/linux/lumaflow.desktop" --icon-file "$ROOT/resources/icons/lumaflow.png" \
  --plugin qt
mkdir -p "$OUT"
ARCH=x86_64 VERSION=3.0.0 "$TOOLS/appimagetool" --no-appstream "$APPDIR" "$OUT/LUMAFLOW-3.0.0-x86_64.AppImage"
