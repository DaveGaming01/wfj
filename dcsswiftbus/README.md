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
| ✅ COM1/COM2 + transponder | managed in the swift GUI (daemon stores what swift sets) |
| ✅ Model matching for others | file your type (e.g. `FA18`) — others see you with their military CSLs |
| ❌ Seeing VATSIM traffic inside DCS | traffic calls are accepted but nothing is rendered — DCS cannot spawn arbitrary aircraft from outside. Use swift's mapping/radar view for awareness. |

You are procedurally blind to other traffic in-sim. Fly accordingly (and per whatever
arrangement you have with VATSIM: stay out of busy airspace, comply with ATC).

## Build (Linux)

```sh
sudo apt install cmake g++ pkg-config libdbus-1-dev libevent-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

On Windows, build under [MSYS2](https://www.msys2.org/)
(`pacman -S mingw-w64-x86_64-{gcc,cmake,pkgconf,dbus,libevent}`) or with vcpkg
(`dbus` + `libevent` ports). The code is plain C++17 + libdbus + libevent, no Qt.

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
