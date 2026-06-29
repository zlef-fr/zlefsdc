#!/usr/bin/env bash
# Render example widget configurations (bare mode) + crop each tightly.
# Output -> landing/public/assets/examples/<name>.png
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUTDIR="$HERE/landing/public/assets/examples"
COVER="${COVER:-/tmp/cover.png}"
APP="$HERE/build/hosts/standalone/zlefsdc-standalone"
CFGDIR=/tmp/zl-cfg
mkdir -p "$OUTDIR" "$CFGDIR"

# --- write the per-config keyfiles -----------------------------------------
cat > "$CFGDIR/full.ini" <<'EOF'
[show]
cover=true
title=true
artist=true
album=true
progress=true
[cover]
size=64
radius=12
[layout]
order=cover,info,prev,playpause,next,progress
spacing=10
EOF

cat > "$CFGDIR/compact.ini" <<'EOF'
[show]
cover=true
title=true
artist=false
album=false
prev=false
next=false
progress=false
[cover]
size=40
radius=20
[layout]
order=cover,info,playpause
info_inline=true
[text]
max_chars=24
EOF

cat > "$CFGDIR/textonly.ini" <<'EOF'
[show]
cover=false
icon=true
title=true
artist=true
album=false
[layout]
order=icon,info,prev,playpause,next
info_inline=true
spacing=8
[text]
max_chars=28
EOF

cat > "$CFGDIR/tinted.ini" <<'EOF'
[show]
cover=true
title=true
artist=true
album=false
progress=true
[cover]
size=52
radius=8
[layout]
order=cover,info,playpause,progress
spacing=10
[text]
color=#7e8a3f
EOF

# --- run them under Xvfb + a mock player -----------------------------------
export DISPLAY=:99
Xvfb :99 -screen 0 1000x240x24 >/tmp/xvfb.log 2>&1 &
XVFB=$!
sleep 1

export HERE OUTDIR COVER APP CFGDIR
dbus-run-session -- bash -c '
  set -e
  python3 "$HERE/test/mock-mpris.py" "file://$COVER" >/tmp/mock.log 2>&1 &
  sleep 1.3
  for f in "$CFGDIR"/*.ini; do
    name=$(basename "$f" .ini)
    ZLEFSDC_BARE=1 "$APP" "$f" >/tmp/app-$name.log 2>&1 &
    pid=$!
    sleep 2.2
    python3 "$HERE/test/grab.py" "/tmp/ex-$name.png" >/dev/null
    kill $pid 2>/dev/null || true
    sleep 0.4
    OUT="$OUTDIR/$name.png" python3 - "$name" <<PY
import os,sys
from PIL import Image
n=sys.argv[1]
im=Image.open("/tmp/ex-%s.png"%n).convert("RGB")
im.crop(im.getbbox()).save(os.environ["OUT"])
print("  ",n, im.getbbox())
PY
  done
'
kill $XVFB 2>/dev/null || true
echo "examples -> $OUTDIR"; ls -1 "$OUTDIR"
