#!/usr/bin/env bash
# Creates LUMAFLOW.app + DMG. Signs and notarises when the Apple credentials are present
# (MACOS_CERT_P12, MACOS_CERT_PASSWORD, APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD); otherwise an unsigned DMG is produced.
set -euo pipefail
BUILD="${1:-build}"; ARCH="${2:-$(uname -m)}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
APP="$BUILD/src/lumaflow.app"
mkdir -p "$ROOT/dist"
cp "$BUILD/src/lumaflow-cli" "$APP/Contents/MacOS/"
mkdir -p "$APP/Contents/PlugIns/lumaflow" && cp "$BUILD"/sdk/examples/effect_starfield/*.so "$APP/Contents/PlugIns/lumaflow/" || true
iconutil -c icns -o "$APP/Contents/Resources/lumaflow.icns" "$ROOT/installers/macos/lumaflow.iconset" 2>/dev/null || true
macdeployqt "$APP" -executable="$APP/Contents/MacOS/lumaflow-cli"
if [[ -n "${MACOS_CERT_P12:-}" ]]; then
  echo "$MACOS_CERT_P12" | base64 --decode > /tmp/cert.p12
  security create-keychain -p ci build.keychain && security default-keychain -s build.keychain && security unlock-keychain -p ci build.keychain
  security import /tmp/cert.p12 -k build.keychain -P "$MACOS_CERT_PASSWORD" -T /usr/bin/codesign
  security set-key-partition-list -S apple-tool:,apple: -s -k ci build.keychain
  IDENTITY=$(security find-identity -v -p codesigning build.keychain | head -1 | awk '{print $2}')
  codesign --deep --force --options runtime --entitlements "$ROOT/installers/macos/entitlements.plist" --sign "$IDENTITY" "$APP"
fi
DMG="$ROOT/dist/LUMAFLOW-3.0.0-macOS-$ARCH.dmg"
hdiutil create -volname LUMAFLOW -srcfolder "$APP" -ov -format UDZO "$DMG"
if [[ -n "${APPLE_ID:-}" ]]; then
  xcrun notarytool submit "$DMG" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD" --wait
  xcrun stapler staple "$DMG"
fi
echo "Created $DMG"
