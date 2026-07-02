#!/usr/bin/env python3
"""Mock DCS: flies an F/A-18C in a left-hand orbit over Batumi and sends
 1) the dcsswiftbus Export.lua datagrams (position etc.) at 10 Hz
 2) DCS-SRS-style radio JSON broadcasts (cockpit radios) at 5 Hz
"""

import json
import math
import socket
import sys
import time

UDP_PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 47788
SRS_PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 9084

CENTER_LAT = 41.6103
CENTER_LON = 41.5997
RADIUS_DEG = 0.05
ALT_M = 2000.0
GS_MS = 150.0
QNH_MMHG = 745.0  # -> QNH ~993.2 hPa, pressure altitude ~165 m above true

# what the "cockpit" is tuned to (SRS radios: index 0 intercom, 1 = COMM1, 2 = COMM2)
SRS_STATE = {
    "name": "mock",
    "unit": "FA-18C_hornet",
    "radios": [
        {"name": "Intercom", "freq": 1.0, "modulation": 2},
        {"name": "AN/ARC-210 - COMM1", "freq": 305000000.0, "modulation": 0},
        {"name": "AN/ARC-210 - COMM2", "freq": 127500000.0, "modulation": 0},
    ],
    "iff": {"status": 1, "mode1": 0, "mode2": -1, "mode3": 4520, "mode4": False},
}

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
t0 = time.time()
last_srs = 0.0
print(f"mock_dcs: position -> udp:{UDP_PORT}, SRS radios -> udp:{SRS_PORT}")

while True:
    t = time.time() - t0
    phase = t * 0.05  # slow orbit
    lat = CENTER_LAT + RADIUS_DEG * math.sin(phase)
    lon = CENTER_LON + RADIUS_DEG * math.cos(phase)
    hdg = (math.degrees(phase) + 270.0) % 360.0
    ve = GS_MS * math.sin(math.radians(hdg))
    vn = GS_MS * math.cos(math.radians(hdg))
    fields = {
        "name": "FA-18C_hornet",
        "lat": f"{lat:.7f}",
        "lon": f"{lon:.7f}",
        "alt": f"{ALT_M:.1f}",
        "agl": f"{ALT_M - 30.0:.1f}",
        "gs": f"{GS_MS:.1f}",
        "pitch": "2.0",
        "roll": "-15.0",
        "hdg": f"{hdg:.2f}",
        "ve": f"{ve:.2f}",
        "vu": "0.0",
        "vn": f"{vn:.2f}",
        "pr": "0.0",
        "rr": "0.0",
        "yr": "0.009",
        "gear": "0.0",
        "flaps": "0.0",
        "brk": "0.0",
        "qnh": f"{QNH_MMHG:.2f}",
    }
    payload = ";".join(f"{k}={v}" for k, v in fields.items())
    sock.sendto(payload.encode("ascii"), ("127.0.0.1", UDP_PORT))

    if time.time() - last_srs >= 0.2:
        sock.sendto((json.dumps(SRS_STATE) + " \n").encode("ascii"), ("127.0.0.1", SRS_PORT))
        last_srs = time.time()

    time.sleep(0.1)
