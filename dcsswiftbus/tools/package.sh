#!/usr/bin/env bash
# Builds a portable Windows folder: dist/dcsswiftbus/ with the exe, all needed
# DLLs, config, Export.lua and the installer - run it from an MSYS2 MINGW64 shell.
set -euo pipefail
cd "$(dirname "$0")/.."

case "$(uname -s)" in
MINGW64*) ;;
*)
    echo "Run this from an MSYS2 MINGW64 shell (it collects Windows DLLs)." >&2
    exit 1
    ;;
esac

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

DIST=dist/dcsswiftbus
rm -rf "$DIST"
mkdir -p "$DIST"

cp build/dcsswiftbus.exe "$DIST/"
cp dcsswiftbus.cfg "$DIST/"
cp dcs/Export.lua "$DIST/"
cp tools/install-export-lua.ps1 tools/install-export-lua.bat "$DIST/"
cp README.md "$DIST/"

# bundle every mingw64 DLL the exe needs, so it runs without MSYS2
ldd build/dcsswiftbus.exe | awk '/\/mingw64\//{print $3}' | sort -u | while read -r dll; do
    cp "$dll" "$DIST/"
done

cat > "$DIST/START HERE.txt" <<'EOF'
dcsswiftbus - fly DCS World on VATSIM through the swift pilot client
====================================================================

One-time setup
--------------
1. Double-click install-export-lua.bat
   (adds the dcsswiftbus feed to your DCS Export.lua; SRS/TacView survive)
2. Install the swift pilot client from https://swift-project.org
   with its FlightGear plugin (you do NOT need FlightGear itself).
3. In swift: Settings -> Simulator -> enable "FlightGear".
   DBus: P2P, host 127.0.0.1, port 45003 (usually already the default).

Every session
-------------
1. Double-click dcsswiftbus.exe (keep the window open, it shows status)
2. Start DCS and enter a mission
3. Start swift, connect to VATSIM

Radios
------
By default the radios/squawk you set in the DCS cockpit are used (needs
DCS-SRS installed; don't run the SRS client program at the same time).
To manage radios in the swift GUI instead, edit dcsswiftbus.cfg and set:
    radios = swift

All settings live in dcsswiftbus.cfg.
EOF

echo
echo "Packaged: $DIST"
ls -la "$DIST"
