#!/bin/sh
# Run the browser version of filter-lab on a Raspberry Pi 4 (Raspberry Pi OS
# with desktop). Serves web/ on localhost and opens it full screen in Chromium.
#   ./pi/run-browser.sh            full screen (kiosk); Alt+F4 quits
#   ./pi/run-browser.sh --window   normal window
set -e
cd "$(dirname "$0")/../web"
PORT=${PORT:-8000}

python3 -m http.server "$PORT" --bind 127.0.0.1 >/dev/null 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null' EXIT INT TERM
sleep 1

BROWSER=$(command -v chromium || command -v chromium-browser) || {
	echo "Chromium not found: sudo apt install chromium" >&2; exit 1; }
MODE=--kiosk
[ "$1" = "--window" ] && MODE=--start-maximized
"$BROWSER" $MODE --noerrdialogs --disable-infobars --no-first-run \
	--autoplay-policy=no-user-gesture-required \
	--user-data-dir="$HOME/.config/filter-lab-chromium" \
	"http://127.0.0.1:$PORT/filter-lab.html"
