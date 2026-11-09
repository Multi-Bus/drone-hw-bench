"""Quick link check: read the dongle port for a few seconds and print what arrives.

Usage: python measure/check_link.py --dongle COM8 [--seconds 10]
Healthy link at 50 packets/s: packets ~ 50 x seconds, RSSI lines ~ 1 per second, no RSSI_ERR.
"""
import argparse
import re
import time

import serial

ap = argparse.ArgumentParser()
ap.add_argument("--dongle", required=True)
ap.add_argument("--seconds", type=int, default=10)
args = ap.parse_args()

with serial.Serial(args.dongle, 115200, timeout=0.1) as s:
    s.read(65536)                       # old queued data comes out first; wait it out
    time.sleep(2)
    s.reset_input_buffer()
    raw = b""
    end = time.monotonic() + args.seconds
    while time.monotonic() < end:
        raw += s.read(4096)

lines = [l.decode(errors="replace").strip() for l in raw.split(b"\n")]
pkts = [l for l in lines if re.match(r"^\d+,\d+,x*$", l)]
rssi = [l for l in lines if l.startswith("RSSI,")]
errs = [l for l in lines if l.startswith("RSSI_ERR")]
print(f"packets: {len(pkts)} (expect ~{50 * args.seconds})")
print(f"RSSI lines: {len(rssi)}, last: {rssi[-1] if rssi else '-'}")
print(f"RSSI_ERR lines: {len(errs)}, last: {errs[-1] if errs else '-'}")
print("LINK OK" if pkts and rssi and not errs else "LINK NOT OK (no packets, or RSSI_ERR such as no_conn)")
