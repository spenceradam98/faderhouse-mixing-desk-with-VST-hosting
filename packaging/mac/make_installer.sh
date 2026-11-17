#!/usr/bin/env bash
# Builds Faderhouse-<version>-mac.dmg containing Faderhouse-Installer.pkg, which
# installs the desk (VST3, AU, Standalone) and Faderhouse Strip (VST3, AU).
# Usage: packaging/mac/make_installer.sh <build dir> <version> <out dir>
set -euo pipefail

BUILD="${1:?build dir, e.g. build}"
VERSION="${2:?version}"
OUT="${3:?output dir}"
NAME="Faderhouse"
ID="com.yourbrand.faderhouse"
HERE="$(cd "$(dirname "$0")" && pwd)"
ART="$BUILD/Faderhouse_artefacts/Release"
STRIP="$BUILD/FaderhouseStrip_artefacts/Release"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT" "$WORK/pkgs"

component() { # $1 = bundle path, $2 = install location, $3 = id suffix
    local root="$WORK/root-$3"
    mkdir -p "$root"
    cp -R "$1" "$root/"
    pkgbuild --root "$root" --identifier "$ID.$3" --version "$VERSION" \
             --install-location "$2" "$WORK/pkgs/$NAME-$3.pkg"
}

component "$ART/VST3/$NAME.vst3"            "/Library/Audio/Plug-Ins/VST3"       vst3
component "$ART/AU/$NAME.component"         "/Library/Audio/Plug-Ins/Components" au
component "$ART/Standalone/$NAME.app"       "/Applications"                      app
component "$STRIP/VST3/$NAME Strip.vst3"    "/Library/Audio/Plug-Ins/VST3"       strip-vst3
component "$STRIP/AU/$NAME Strip.component" "/Library/Audio/Plug-Ins/Components" strip-au

sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" \
             --resources "$HERE/resources" "$WORK/$NAME-Installer.pkg"

DMGROOT="$WORK/dmg"
mkdir -p "$DMGROOT"
cp "$WORK/$NAME-Installer.pkg" "$DMGROOT/"
cp "$HERE/resources/HOW TO INSTALL.txt" "$DMGROOT/"
hdiutil create -volname "$NAME $VERSION" -srcfolder "$DMGROOT" -ov -format UDZO \
        "$OUT/$NAME-$VERSION-mac.dmg"
echo "Created $OUT/$NAME-$VERSION-mac.dmg"
