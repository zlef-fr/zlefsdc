#!/usr/bin/env bash
# Build a .deb for ZlefSDC from a clean meson install tree.
# Output: packaging/zlefsdc_<ver>_amd64.deb  (then publish with: aptpush <deb>)
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
VER="$(awk -F\' '/version:/{print $2; exit}' "$HERE/meson.build")"
PKG=zlefsdc
ARCH=amd64
ROOT="$(mktemp -d)"

meson setup "$ROOT/b" "$HERE" --prefix=/usr --buildtype=release >/dev/null
DESTDIR="$ROOT/pkg" meson install -C "$ROOT/b" >/dev/null

mkdir -p "$ROOT/pkg/DEBIAN"
INSTALLED_SIZE=$(du -ks "$ROOT/pkg/usr" | cut -f1)
cat > "$ROOT/pkg/DEBIAN/control" <<EOF
Package: $PKG
Version: $VER
Architecture: $ARCH
Maintainer: zlef.fr <apt@zlef.fr>
Installed-Size: $INSTALLED_SIZE
Depends: libgtk-3-0, libglib2.0-0, libxfce4panel-2.0-0, libxfce4ui-2-0
Section: xfce
Priority: optional
Homepage: https://sdc.zlef.fr
Description: Spotify Display Controls — now-playing panel widget (xfce4-panel)
 Shows the current track's cover, title and artist with play/pause, previous
 and next controls, live over MPRIS (any player). Ships the xfce4-panel plugin,
 a standalone host and the reusable DE-agnostic core. Highly configurable.
EOF

OUT="$HERE/packaging/${PKG}_${VER}_${ARCH}.deb"
fakeroot dpkg-deb --build "$ROOT/pkg" "$OUT" >/dev/null
rm -rf "$ROOT"
echo "$OUT"
