#!/usr/bin/env bash
# Render the standalone host against the mock MPRIS player and screenshot it.
# Usage: test/shoot.sh <out.png> [config.ini]
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-/tmp/zlefsdc.png}"
CFG="${2:-}"
COVER="${COVER:-/tmp/cover.png}"

export HERE OUT CFG COVER
export DISPLAY=:99
Xvfb :99 -screen 0 900x200x24 >/tmp/xvfb.log 2>&1 &
XVFB=$!
sleep 1

run() {
  python3 "$HERE/test/mock-mpris.py" "file://$COVER" >/tmp/mock.log 2>&1 &
  MOCK=$!
  sleep 1.2
  "$HERE/build/hosts/standalone/zlefsdc-standalone" $CFG >/tmp/app.log 2>&1 &
  APP=$!
  sleep 2.5
  python3 "$HERE/test/grab.py" "$OUT"
  kill $APP $MOCK 2>/dev/null || true
}
dbus-run-session -- bash -c "$(declare -f run); run"

kill $XVFB 2>/dev/null || true
echo "screenshot -> $OUT"
