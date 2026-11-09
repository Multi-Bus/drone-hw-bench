"""Task 4 Lv2: round-trip time PC -> dongle -> XIAO -> echo -> PC.

Setup: XIAO runs xiao_throughput.ino (USB serial = control port, echoes everything the central writes).
       Dongle runs central_uart (USB serial = the path under test).
For every connection interval the script sends N pings one at a time, "<n>,xxxx...\n" of the given
length, and measures the time until the same line comes back on the dongle port.
A ping with no reply within --timeout counts as lost (and is not part of the RTT statistics).
The measured time includes the PC serial stack and the USB path, not only the radio link.

Output per condition: <outdir>/t4_rtt_len<L>_ci<C>ms.csv   (n, rtt_ms; empty = lost)
Output summary:       <outdir>/t4_rtt_summary.csv          (one row per condition)

Usage: python rtt.py --xiao COM3 --dongle COM8 --outdir ../data
"""
import argparse
import csv
import os
import re
import statistics
import time
from datetime import datetime

import serial

from throughput import kv, send, wait_for     # same XIAO control-port helpers

REPLY_RE = re.compile(r"^(\d+),x*$")


def ping_once(dongle, n, length, timeout):
    """Send ping n, return RTT in ms or None. Replies for older pings (late) are skipped."""
    head = f"{n},"
    line = (head + "x" * (length - 1 - len(head)) + "\n").encode()
    t0 = time.perf_counter()
    dongle.write(line)
    buf = b""
    while time.perf_counter() - t0 < timeout:
        buf += dongle.read(256)
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            m = REPLY_RE.match(raw.decode(errors="replace"))
            if m and int(m.group(1)) == n:
                return (time.perf_counter() - t0) * 1000.0
    return None


def run_condition(xiao, dongle, length, ci_ms, count, timeout, outdir):
    send(xiao, f"CFG {length} {ci_ms} 0")
    wait_for(xiao, "OK CFG")
    time.sleep(3.0)                       # let the central accept the new connection parameters
    send(xiao, "STAT")
    stat = wait_for(xiao, "STAT")
    if "disconnected" in stat:
        raise RuntimeError("XIAO is not connected to the dongle")
    link = kv(stat)

    dongle.reset_input_buffer()
    rtts = []
    for n in range(count):
        rtts.append(ping_once(dongle, n, length, timeout))
        time.sleep(0.02)                  # let a late echo of this ping arrive before the next one

    ok = [r for r in rtts if r is not None]
    ordered = sorted(ok)
    result = {
        "run_time": datetime.now().isoformat(timespec="seconds"),
        "len": length,
        "ci_req_ms": ci_ms,
        "ci_actual_ms": round(link["ci_units"] * 1.25, 2),
        "mtu": link["mtu"],
        "sent": count,
        "received": len(ok),
        "success_pct": round(100.0 * len(ok) / count, 2),
        "mean_ms": round(statistics.mean(ok), 2) if ok else "",
        "std_ms": round(statistics.pstdev(ok), 2) if ok else "",
        "min_ms": round(ordered[0], 2) if ok else "",
        "p50_ms": round(ordered[len(ordered) // 2], 2) if ok else "",
        "p95_ms": round(ordered[int(len(ordered) * 0.95)], 2) if ok else "",
        "max_ms": round(ordered[-1], 2) if ok else "",
    }
    with open(os.path.join(outdir, f"t4_rtt_len{length}_ci{ci_ms:g}ms.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["n", "rtt_ms"])
        for n, r in enumerate(rtts):
            w.writerow([n, "" if r is None else round(r, 3)])
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xiao", required=True, help="XIAO control port, e.g. COM3")
    ap.add_argument("--dongle", required=True, help="dongle data port, e.g. COM8")
    ap.add_argument("--len", type=int, default=64, help="ping length in bytes")
    ap.add_argument("--cis", default="7.5,30,100", help="connection intervals in ms")
    ap.add_argument("--count", type=int, default=1000, help="pings per condition")
    ap.add_argument("--timeout", type=float, default=1.0, help="seconds to wait for one echo")
    ap.add_argument("--outdir", default=os.path.join(os.path.dirname(__file__), "..", "data"))
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)
    summary_path = os.path.join(args.outdir, "t4_rtt_summary.csv")

    with serial.Serial(args.xiao, 115200, timeout=0.5) as xiao, \
            serial.Serial(args.dongle, 115200, timeout=0.005) as dongle:
        for ci_ms in [float(x) for x in args.cis.split(",")]:
            print(f"len={args.len} B, ci={ci_ms:g} ms, {args.count} pings ...", flush=True)
            r = run_condition(xiao, dongle, args.len, ci_ms, args.count, args.timeout, args.outdir)
            print(f"  actual ci={r['ci_actual_ms']} ms, mean={r['mean_ms']} ms, "
                  f"max={r['max_ms']} ms, success={r['success_pct']} %", flush=True)
            new = not os.path.exists(summary_path)
            with open(summary_path, "a", newline="") as f:
                w = csv.DictWriter(f, fieldnames=list(r))
                if new:
                    w.writeheader()
                w.writerow(r)


if __name__ == "__main__":
    main()
