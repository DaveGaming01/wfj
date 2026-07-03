# dcsswiftbus

**Proof of concept: fly DCS World on VATSIM through the [swift pilot client](https://swift-project.org/).**

swift has no idea what a DCS is — but its FlightGear integration talks to a small DBus
server (*FGSwiftBus*) running inside the simulator. `dcsswiftbus` is a standalone daemon
that impersonates FGSwiftBus: swift connects to it believing it is FlightGear, while the
daemon's own-aircraft data actually comes from DCS via an `Export.lua` UDP feed.

```
DCS World --Export.lua--> UDP :47788 --> dcsswiftbus --DBus (tcp :45003)--> swift --> VATSIM
                                            (pretends to be FGSwiftBus)
```

The DBus layer is ported from FlightGear's FGSwiftBus implementation
(`flightgear/src/Network/Swift`, GPL-2.0-or-later), so the wire behaviour is identical
to what swift already interoperates with. API version reported: **3** (swift rejects 1 and 2).

## What works / what doesn't

| | |
|---|---|
| ✅ Own aircraft position/attitude on the network | streamed from DCS at 10 Hz |
| ✅ Voice + text with ATC | via swift's built-in Audio for VATSIM (PTT bound in swift) |
| ✅ Cockpit radio + squawk sync (~60 modules) | piggybacks on the DCS-SRS export broadcasts if SRS is installed; falls back to swift-GUI-managed radios otherwise |
| ✅ Pressure altitude | true altitude corrected with the mission QNH (`LoGetBasicAtmosphericPressure`), so your Mode C reads correctly on non-standard-pressure days |
| ✅ Model matching for others | file your type (e.g. `FA18`) — others see you with their military CSLs |
| ❌ Seeing VATSIM traffic inside DCS | traffic calls are accepted but nothing is rendered — DCS cannot spawn arbitrary aircraft from outside. Use swift's mapping/radar view for awareness. |

### Cockpit radio sync (via DCS-SRS)

[DCS-SRS](https://github.com/ciribob/DCS-SimpleRadioStandalone)' export script broadcasts
the full cockpit radio and IFF state as JSON to UDP `127.0.0.1:9084` on every export tick,
using per-aircraft exporters for ~60 modules (F/A-18C, F-16C, F-4E, F-14, A-10C, Mirage,
helicopters, warbirds, ...). The daemon listens on that port, so:

- **SRS installed** (only its scripts need to be installed — the SRS *client app* must NOT
  be running, it would grab port 9084): what you tune on COMM1/COMM2 in the cockpit is what
  swift transmits/receives on, and your IFF panel's Mode 3 code becomes your squawk.
  swift's own radio buttons are overridden while cockpit data is live.
- **No SRS**: COM1/COM2/squawk are controlled from the swift GUI, as before.

If you need to run the actual SRS client at the same time, give SRS a different port and
point `--srs-port` at it, or pass `--srs-port 0` to disable cockpit sync.

### Heading looks ~10-15° off on radar?

That's not a bug: VATSIM's protocol carries **true** heading, and controllers' scopes are
true-north referenced, while your HUD/HSI shows **magnetic** heading. The difference is the
local magnetic variation (e.g. ~13°E at SCCI on the South Atlantic map). Every other pilot
client reports true heading the same way.

You are procedurally blind to other traffic in-sim. Fly accordingly (and per whatever
arrangement you have with VATSIM: stay out of busy airspace, comply with ATC).

## Install (Windows, no tools needed)

1. **Download**: repo → **Actions** tab → newest green *"build dcsswiftbus"* run →
   download the **dcsswiftbus-win64** artifact (a zip). On tagged releases the same
   zip is attached to the GitHub **Release**.
2. **Extract** the `dcsswiftbus` folder anywhere (Desktop is fine).
3. Double-click **`1. SETUP - run me once.bat`** and follow what it prints
   (it installs the DCS export script for you — SRS/TacView setups survive,
   a backup is made, and re-running after an update is safe).
4. Every session: double-click **`2. START dcsswiftbus.bat`**, fly DCS, connect swift.

`READ ME FIRST.txt` in the folder covers the rest (radios, squawk, tips).

If you previously pasted the export block into `Export.lua` by hand, remove your
pasted copy once — the installer only manages its own marked block.

### Building the portable folder yourself

Under [MSYS2](https://www.msys2.org/) (MINGW64 shell):

```sh
pacman -S --needed git mingw-w64-x86_64-{gcc,cmake,ninja,pkgconf,dbus,libevent}
./tools/package.sh    # produces dist/dcsswiftbus/ - same folder CI ships
```

## Settings (`dcsswiftbus.cfg`)

All options live in `dcsswiftbus.cfg` next to the exe (command line overrides it,
see `--help`). The one you're most likely to touch:

```ini
# cockpit = radios/squawk follow what you tune in the DCS cockpit (needs DCS-SRS)
# swift   = always tune radios and squawk in the swift GUI
radios = cockpit
```

With `radios = cockpit` the swift GUI still takes over automatically whenever no
cockpit data is available (module without SRS support, IFF off, SRS not installed).

## Build (Linux)

```sh
sudo apt install cmake g++ pkg-config libdbus-1-dev libevent-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The code is plain C++17 + libdbus + libevent, no Qt.

## Run the proof of concept (no DCS or swift needed)

```sh
./test/run_poc.sh
```

This starts the daemon, feeds it a fake F/A-18 orbiting Batumi (`test/mock_dcs.py`),
and runs `mock_swift_client`, which performs the exact DBus call sequence swift's
FlightGear plugin performs on connect (version check → `traffic.initialize` →
`acquireMultiplayerPlanes` → own-model queries → situation/velocity polling → radio
writes → `addPlane`/signals). All checks passing means swift will accept the daemon
as a healthy FGSwiftBus.

## Run it for real

1. **DCS**: install `dcs/Export.lua` into `%USERPROFILE%\Saved Games\DCS\Scripts\Export.lua`
   (append to your existing one if you use SRS/TacView — it chains previous handlers).
2. **Daemon**: `./dcsswiftbus` (defaults: DBus on `tcp:host=127.0.0.1,port=45003`,
   UDP feed on `47788`). It logs feed status every 10 s.
3. **swift**: install the FlightGear simulator plugin (no FlightGear needed). In
   *Settings → Simulator → FlightGear*, set the DBus server address to
   `tcp:host=127.0.0.1,port=45003` (FG's default, so it may already match).
   Enable the plugin — swift should report the simulator as running.
4. Enter the mission in DCS, connect swift to the network, tune COM1 and squawk
   **in the swift GUI**, talk on frequency via swift's PTT.

Everything must run on the same host (or adjust `--dbus-host`/`Export.lua` HOST).

## Notes

- swift is the source of truth for radios/transponder: DCS cockpit radios are not
  read in this PoC (per-module radio export is the SRS-style follow-up work).
- `isPaused` reports true when the UDP feed goes stale for >3 s (mission paused/ended),
  which stops swift extrapolating your position.
- Pressure altitude is reported as true altitude for now.
- License: GPL-2.0-or-later (contains code derived from FlightGear's FGSwiftBus).
