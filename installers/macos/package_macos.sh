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
# Embed Syphon.framework (if the build used it) before deployment/signing
if [[ -d /tmp/syphon-build/Syphon.framework ]]; then
  mkdir -p "$APP/Contents/Frameworks" && cp -R /tmp/syphon-build/Syphon.framework "$APP/Contents/Frameworks/"
  install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP/Contents/MacOS/lumaflow" || true
fi
macdeployqt "$APP" -executable="$APP/Contents/MacOS/lumaflow-cli"

# macdeployqt copies every Qt plugin it finds, including ones (PDF, SVG, virtual keyboard) whose
# frameworks live in other Homebrew kegs and were not bundled. Drop any plugin with an
# @rpath Qt dependency that is not in Contents/Frameworks, so nothing in the bundle has a broken link.
while IFS= read -r -d '' plugin; do
  for dep in $(otool -L "$plugin" | awk 'NR>1 {print $1}' | grep -o '^@rpath/Qt[A-Za-z0-9]*\.framework' | sed 's|@rpath/||'); do
    if [[ ! -d "$APP/Contents/Frameworks/$dep" ]]; then
      echo "Removing plugin with unbundled dependency $dep: ${plugin#$APP/}"
      rm -f "$plugin"; break
    fi
  done
done < <(find "$APP/Contents/PlugIns" -name '*.dylib' -print0)

if [[ -n "${MACOS_CERT_P12:-}" ]]; then
  echo "$MACOS_CERT_P12" | base64 --decode > /tmp/cert.p12
  security create-keychain -p ci build.keychain && security default-keychain -s build.keychain && security unlock-keychain -p ci build.keychain
  security import /tmp/cert.p12 -k build.keychain -P "$MACOS_CERT_PASSWORD" -T /usr/bin/codesign
  security set-key-partition-list -S apple-tool:,apple: -s -k ci build.keychain
  IDENTITY=$(security find-identity -v -p codesigning build.keychain | head -1 | awk '{print $2}')
  codesign --deep --force --options runtime --entitlements "$ROOT/installers/macos/entitlements.plist" --sign "$IDENTITY" "$APP"
else
  # No Developer ID: ad-hoc sign the whole bundle. Apple Silicon will not run code with a missing
  # or broken signature, and Syphon.framework arrives unsigned (built with CODE_SIGNING_ALLOWED=NO).
  # Sign inside-out: loose plugins and libraries first, then frameworks, then the bundle itself.
  find "$APP/Contents" \( -name '*.dylib' -o -name '*.so' \) -type f -exec codesign --force --sign - {} \;
  find "$APP/Contents/Frameworks" -maxdepth 1 -name '*.framework' -exec codesign --force --sign - {} \;
  codesign --force --sign - "$APP/Contents/MacOS/lumaflow-cli"
  codesign --force --sign - "$APP"
fi
codesign --verify --deep --strict --verbose=2 "$APP"
# Launch check: the bundled CLI must load every framework and the Qt platform plugin.
"$APP/Contents/MacOS/lumaflow-cli" --version
"$APP/Contents/MacOS/lumaflow-cli" effects > "$BUILD/effects-check.txt"
echo "Bundled CLI lists $(wc -l < "$BUILD/effects-check.txt") effects"
# GPU check (informational: CI virtual machines may not expose a usable OpenGL device)
"$APP/Contents/MacOS/lumaflow-cli" render-frame -e plasma -t 0.2 -o "$BUILD/render-check.png" \
  && echo "Bundled CLI rendered a frame on the GPU" || echo "WARNING: GPU render check failed on this runner"
DMG="$ROOT/dist/LUMAFLOW-3.0.0-macOS-$ARCH.dmg"
hdiutil create -volname LUMAFLOW -srcfolder "$APP" -ov -format UDZO "$DMG"
if [[ -n "${APPLE_ID:-}" ]]; then
  xcrun notarytool submit "$DMG" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD" --wait
  xcrun stapler staple "$DMG"
fi
echo "Created $DMG"
