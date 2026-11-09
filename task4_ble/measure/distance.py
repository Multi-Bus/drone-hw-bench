"""Task 4 Lv2: RSSI and packet loss by distance (XIAO -> dongle -> PC).

Setup: XIAO runs xiao_throughput.ino in auto mode (power bank, no PC): it sends 64 B x 50 packets/s
       with seq as soon as the dongle connects. Dongle runs central_uart built with RSSI output
       (USB serial = data port: packet lines "<seq>,<ms>,xxxx" and "RSSI,<ms>,<dBm>" once a second).
The script only reads the dongle port. A background thread reads it all the time, also while you
move the XIAO: if nobody reads, the dongle queues old data and dumps it when the port is read again.
Per distance: move the XIAO, press Enter, wait SETTLE s (link settles), then measure --duration s.
Output per distance: <outdir>/t4_dist_<D>m.csv   (per second: rx packets, RSSI sample)
Output summary:      <outdir>/summary.csv        (one row per distance)

Loss is counted from seq gaps. BLE retransmits, so a weak link usually shows up as fewer packets per
second (shortfall_pct) or a disconnect (rssi_err lines), not as seq gaps.

Usage: python distance.py --dongle COM8 --dists 1,5,10,20
"""
import argparse
import csv
import os
import re
import statistics
import threading
import time
from datetime import datetime

import serial

PKT_RE = re.compile(r"^(\d+),(\d+),x*$")        # "<seq>,<millis>,xxxx"
RSSI_RE = re.compile(r"^RSSI,(\d+),(-?\d+)$")   # dongle RSSI line
SETTLE = 3.0


class Reader(threading.Thread):
    """Reads the dongle port forever; keeps (arrival time, seq) and (arrival time, dongle ms, dBm)."""

    def __init__(self, port):
        super().__init__(daemon=True)
        self.port = port
        self.lock = threading.Lock()
        self.pkts = []
        self.rssi = []
        self.errs = []          # arrival times of RSSI_ERR lines (e.g. no_conn = link down)
        self.stop = threading.Event()

    def run(self):
        rest = b""
        while not self.stop.is_set():
            data = self.port.read(4096)
            if not data:
                continue
            now = time.monotonic()
            rest += data
            *lines, rest = rest.split(b"\n")
            with self.lock:
                for raw in lines:
                    text = raw.decode(errors="replace").strip()
                    m = PKT_RE.match(text)
                    if m:
                        self.pkts.append((now, int(m.group(1))))
                    elif RSSI_RE.match(text):
                        ms, dbm = RSSI_RE.match(text).groups()
                        self.rssi.append((now, int(ms), int(dbm)))
                    elif text.startswith("RSSI_ERR"):
                        self.errs.append(now)


def run_distance(rd, dist_m, rate, duration, outdir):
    input(f"[{dist_m} m] XIAO를 {dist_m} m에 두고 Enter (안정화 {SETTLE:g} s + 측정 {duration} s) ")
    time.sleep(SETTLE)
    t0 = time.monotonic()
    time.sleep(duration)
    t1 = t0 + duration
    with rd.lock:
        pk = [(t - t0, s) for t, s in rd.pkts if t0 <= t < t1]
        rs = [(t - t0, ms, d) for t, ms, d in rd.rssi if t0 <= t < t1]
        errs = [t for t in rd.errs if t0 <= t < t1]

    seqs = [s for _, s in pk]
    rx_pkts = len(set(seqs))
    gaps = (max(seqs) - min(seqs) + 1 - rx_pkts) if seqs else 0
    expected = rate * duration
    vals = [d for _, _, d in rs]

    pkts_s = [0] * duration
    for t, _ in pk:
        pkts_s[min(int(t), duration - 1)] += 1
    rssi_s = [""] * duration
    for t, _, d in rs:
        rssi_s[min(int(t), duration - 1)] = d
    with open(os.path.join(outdir, f"t4_dist_{dist_m}m.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_s", "rx_pkts", "rssi_dbm"])
        for i in range(duration):
            w.writerow([i, pkts_s[i], rssi_s[i]])

    return {
        "dist_m": dist_m,
        "run_time": datetime.now().isoformat(timespec="seconds"),
        "rate_pps": rate,
        "duration_s": duration,
        "expected_pkts": expected,
        "rx_pkts": rx_pkts,
        "seq_gaps": gaps,
        "loss_pct": round(100.0 * gaps / (rx_pkts + gaps), 2) if rx_pkts else "",
        "shortfall_pct": round(100.0 * (expected - rx_pkts) / expected, 2),
        "rssi_n": len(vals),
        "rssi_mean": round(statistics.fmean(vals), 1) if vals else "",
        "rssi_std": round(statistics.pstdev(vals), 1) if len(vals) > 1 else "",
        "rssi_min": min(vals) if vals else "",
        "rssi_max": max(vals) if vals else "",
        "rssi_err_lines": len(errs),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dongle", required=True, help="dongle data port, e.g. COM8")
    ap.add_argument("--dists", default="1,5,10,20", help="distances in m, measured in this order")
    ap.add_argument("--rate", type=int, default=50, help="packets per second the XIAO sends (auto mode: 50)")
    ap.add_argument("--duration", type=int, default=60, help="seconds per distance")
    ap.add_argument("--outdir", default=os.path.join(os.path.dirname(__file__), "..", "data", "lv2", "distance"))
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)

    summary_path = os.path.join(args.outdir, "summary.csv")
    dists = [int(x) for x in args.dists.split(",")]

    with serial.Serial(args.dongle, 115200, timeout=0.1) as dongle:
        rd = Reader(dongle)
        rd.start()
        try:
            for d in dists:
                r = run_distance(rd, d, args.rate, args.duration, args.outdir)
                print(f"  {d} m: rx {r['rx_pkts']}/{r['expected_pkts']}, seq gaps={r['seq_gaps']}, "
                      f"RSSI mean={r['rssi_mean']} dBm (n={r['rssi_n']}), err lines={r['rssi_err_lines']}",
                      flush=True)
                new = not os.path.exists(summary_path)
                with open(summary_path, "a", newline="") as f:
                    w = csv.DictWriter(f, fieldnames=list(r))
                    if new:
                        w.writeheader()
                    w.writerow(r)
        finally:
            rd.stop.set()
            rd.join()


if __name__ == "__main__":
    main()
