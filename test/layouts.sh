#!/usr/bin/env bash
# Render a set of LAYOUT recipes (varying layout.order / nesting) in bare mode
# and crop each tightly. Output -> landing/public/assets/layouts/<name>.png
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUTDIR="$HERE/landing/public/assets/layouts"
COVER="${COVER:-/tmp/cover.png}"
APP="$HERE/build/hosts/standalone/zlefsdc-standalone"
CFGDIR=/tmp/zl-lay
mkdir -p "$OUTDIR" "$CFGDIR"

# name|order  — one recipe per line (order may contain brackets/commas)
recipes=(
  "inline|cover, info, prev, playpause, next, progress"
  "stacked|cover, [ info, [ prev, playpause, next ], progress ]"
  "tworows|[ [ cover, info ], [ prev, playpause, next, progress ] ]"
  "controlsleft|prev, playpause, next, cover, info"
  "coverless|[ info, [ prev, playpause, next ] ]"
  "buttonscolumn|cover, info, [ prev, playpause, next ]"
)

for r in "${recipes[@]}"; do
  name="${r%%|*}"; order="${r#*|}"
  cover_line="cover=true"; icon_line="icon=false"
  [ "$name" = "coverless" ] && { cover_line="cover=false"; icon_line="icon=true"; }
  cat > "$CFGDIR/$name.ini" <<EOF
[show]
$cover_line
$icon_line
title=true
artist=true
progress=true
[cover]
size=50
radius=8
[text]
max_chars=26
[layout]
order=$order
spacing=8
EOF
done

export DISPLAY=:99
Xvfb :99 -screen 0 1100x320x24 >/tmp/xvfb.log 2>&1 &
XVFB=$!
sleep 1

export HERE OUTDIR COVER APP CFGDIR
dbus-run-session -- bash -c '
  set -e
  python3 "$HERE/test/mock-mpris.py" "file://$COVER" >/tmp/mock.log 2>&1 &
  sleep 1.3
  for f in "$CFGDIR"/*.ini; do
    name=$(basename "$f" .ini)
    ZLEFSDC_BARE=1 "$APP" "$f" >/tmp/lay-$name.log 2>&1 &
    pid=$!
    sleep 2.2
    python3 "$HERE/test/grab.py" "/tmp/layshot-$name.png" >/dev/null
    kill $pid 2>/dev/null || true
    sleep 0.4
    OUT="$OUTDIR/$name.png" python3 - "$name" <<PY
import os,sys
from PIL import Image
n=sys.argv[1]
im=Image.open("/tmp/layshot-%s.png"%n).convert("RGB")
im.crop(im.getbbox()).save(os.environ["OUT"])
print("  ",n, im.getbbox())
PY
  done
'
kill $XVFB 2>/dev/null || true
echo "layouts -> $OUTDIR"; ls -1 "$OUTDIR"
