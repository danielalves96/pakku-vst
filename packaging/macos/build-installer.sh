#!/usr/bin/env bash
#
#  Builds the macOS .pkg installer.
#
#  With no environment variables set it produces an unsigned package. That
#  works, but Gatekeeper objects the first time it is opened. Set these and
#  the script signs and notarises instead:
#
#    DEVELOPER_ID_APP="Developer ID Application: Name (TEAMID)"
#    DEVELOPER_ID_INSTALLER="Developer ID Installer: Name (TEAMID)"
#    NOTARY_PROFILE="profile-name"     # see: xcrun notarytool store-credentials
#
#  All three need an Apple Developer Program membership. Without them the
#  package comes out identical in content, just without the stamps.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
DIST="$ROOT/dist"
STAGE="$ROOT/packaging/macos/stage"

VERSION="$(sed -n 's/^project(Pakku VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
[ -n "$VERSION" ] || { echo "could not read the version from CMakeLists.txt" >&2; exit 1; }

AU="$BUILD/src/Pakku_artefacts/Release/AU/Pakku.component"
VST3="$BUILD/src/Pakku_artefacts/Release/VST3/Pakku.vst3"

for b in "$AU" "$VST3"; do
    [ -d "$b" ] || { echo "$b is missing — build first (cmake --build build)" >&2; exit 1; }
done

echo "==> Pakku $VERSION"
rm -rf "$STAGE" "$DIST"
mkdir -p "$STAGE/au" "$STAGE/vst3" "$STAGE/pkg" "$DIST"

# ditto rather than cp: it preserves the bundles without scattering
# ._AppleDouble files, which appear when copying across filesystems and would
# otherwise end up inside the package
ditto "$AU"   "$STAGE/au/Pakku.component"
ditto "$VST3" "$STAGE/vst3/Pakku.vst3"
find "$STAGE" -name '._*' -delete

# ---- bundle signing (optional) ----
if [ -n "${DEVELOPER_ID_APP:-}" ]; then
    echo "==> signing the bundles"
    for b in "$STAGE/au/Pakku.component" "$STAGE/vst3/Pakku.vst3"; do
        codesign --force --deep --options runtime --timestamp \
                 --sign "$DEVELOPER_ID_APP" "$b"
        codesign --verify --strict --verbose=2 "$b"
    done
else
    echo "==> no DEVELOPER_ID_APP: bundles left unsigned"
fi

# ---- one package per format, so the installer can offer the choice ----
pkgbuild --root "$STAGE/au" \
         --identifier "com.kyantechlabs.pakku.au" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         "$STAGE/pkg/Pakku-AU.pkg" >/dev/null

pkgbuild --root "$STAGE/vst3" \
         --identifier "com.kyantechlabs.pakku.vst3" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         "$STAGE/pkg/Pakku-VST3.pkg" >/dev/null

# ---- distribution ----
RES="$ROOT/packaging/macos/resources"
cp "$ROOT/LICENSE" "$RES/license.txt"

PKG="$DIST/Pakku-$VERSION.pkg"
sed "s/@VERSION@/$VERSION/g" "$ROOT/packaging/macos/distribution.xml" \
    > "$STAGE/distribution.xml"

productbuild --distribution "$STAGE/distribution.xml" \
             --resources "$RES" \
             --package-path "$STAGE/pkg" \
             "$STAGE/Pakku-unsigned.pkg" >/dev/null

if [ -n "${DEVELOPER_ID_INSTALLER:-}" ]; then
    echo "==> signing the package"
    productsign --sign "$DEVELOPER_ID_INSTALLER" "$STAGE/Pakku-unsigned.pkg" "$PKG"
else
    echo "==> no DEVELOPER_ID_INSTALLER: package left unsigned"
    mv "$STAGE/Pakku-unsigned.pkg" "$PKG"
fi

# ---- notarisation (optional, needs a signed package) ----
if [ -n "${NOTARY_PROFILE:-}" ] && [ -n "${DEVELOPER_ID_INSTALLER:-}" ]; then
    echo "==> notarising (this can take a few minutes)"
    xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$PKG"
    xcrun stapler validate "$PKG"
fi

rm -f "$RES/license.txt"
echo
echo "done: $PKG"
ls -lh "$PKG" | awk '{print "     ", $5}'
