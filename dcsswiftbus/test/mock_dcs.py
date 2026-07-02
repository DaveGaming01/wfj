#!/usr/bin/env python3
"""Mock DCS Export.lua feed: flies an F/A-18C in a left-hand orbit over Batumi
and sends the same datagrams Export.lua sends, at 10 Hz."""

import math
import socket
import sys
import time

UDP_PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 47788

CENTER_LAT = 41.6103
CENTER_LON = 41.5997
RADIUS_DEG = 0.05
ALT_M = 2000.0
GS_MS = 150.0

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
t0 = time.time()
print(f"mock_dcs: sending to udp://127.0.0.1:{UDP_PORT} at 10 Hz")

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
    }
    payload = ";".join(f"{k}={v}" for k, v in fields.items())
    sock.sendto(payload.encode("ascii"), ("127.0.0.1", UDP_PORT))
    time.sleep(0.1)
