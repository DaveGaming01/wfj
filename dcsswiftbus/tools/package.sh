#!/usr/bin/env bash
# Builds a portable Windows folder: dist/dcsswiftbus/ with the exe, all needed
# DLLs, config, Export.lua and a numbered SETUP/START flow for non-technical
# users. Run from an MSYS2 MINGW64 shell (or GitHub Actions msys2 runner).
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

crlf() { sed 's/$/\r/' > "$1"; }

cp build/dcsswiftbus.exe "$DIST/"
cp dcsswiftbus.cfg "$DIST/"
cp dcs/Export.lua "$DIST/"
cp tools/install-export-lua.ps1 "$DIST/"
cp README.md "$DIST/"

# bundle every mingw64 DLL the exe needs, so it runs without MSYS2
ldd build/dcsswiftbus.exe | awk '/\/mingw64\//{print $3}' | sort -u | while read -r dll; do
    cp "$dll" "$DIST/"
done

crlf "$DIST/1. SETUP - run me once.bat" <<'EOF'
@echo off
cd /d "%~dp0"
echo ==================================================================
echo  dcsswiftbus one-time setup
echo ==================================================================
echo.
echo Installing the DCS export script into your Saved Games profile...
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-export-lua.ps1"
echo.
echo ==================================================================
echo  Almost done! Two things left to do BY HAND (one time only):
echo ==================================================================
echo.
echo  1. Install the swift pilot client:  https://swift-project.org
echo     During install, include its FlightGear plugin.
echo     (You do NOT need the FlightGear simulator itself!)
echo.
echo  2. In swift:  Settings - Simulator - enable "FlightGear"
echo     Its DBus settings must be: P2P, host 127.0.0.1, port 45003
echo     (that is usually already the default)
echo.
echo From now on, every time you want to fly on VATSIM:
echo     double-click "2. START dcsswiftbus.bat", start DCS, start swift.
echo.
pause
EOF

crlf "$DIST/2. START dcsswiftbus.bat" <<'EOF'
@echo off
cd /d "%~dp0"
dcsswiftbus.exe
echo.
echo dcsswiftbus has stopped.
pause
EOF

crlf "$DIST/READ ME FIRST.txt" <<'EOF'
dcsswiftbus - fly DCS World on VATSIM through the swift pilot client
====================================================================

FIRST TIME:
    Double-click "1. SETUP - run me once.bat" and follow what it says.

EVERY TIME YOU FLY:
    1. Double-click "2. START dcsswiftbus.bat"  (keep the window open)
    2. Start DCS and enter a mission
    3. Start swift and connect to VATSIM

RADIOS:
    Out of the box, the COM radios and squawk code you set in the DCS
    cockpit are what VATSIM sees (this needs DCS-SRS installed - most
    people have it. Do NOT run the SRS client program at the same time).

    If a module's radios don't export (or you don't have SRS), just tune
    COM1 and the squawk in the swift window instead - that always works.

    Prefer to ALWAYS use swift for radios? Open dcsswiftbus.cfg in
    Notepad and change:   radios = cockpit   to:   radios = swift

REMEMBER:
    - You cannot see other VATSIM traffic inside DCS. Keep swift's map
      open for awareness and follow ATC instructions.
    - Talk/listen happens through swift (set your push-to-talk there).

Problems? The dcsswiftbus window tells you what it's waiting for.
EOF

echo
echo "Packaged: $DIST"
ls -la "$DIST"
