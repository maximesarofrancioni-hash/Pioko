#!/bin/sh
# Start the browser version of filter-lab when the Pi's desktop logs in.
# Undo with: rm ~/.config/autostart/filter-lab.desktop
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/filter-lab.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=filter-lab
Exec=$DIR/run-browser.sh
DESKTOP
echo "filter-lab will start at the next desktop login"
