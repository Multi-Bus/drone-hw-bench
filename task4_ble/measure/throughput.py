"""Task 4 Lv2: BLE throughput (XIAO -> dongle -> PC).

Setup: XIAO runs xiao_throughput.ino (USB serial = control port).
       Dongle runs central_uart (USB serial = received NUS data).
For every (packet length, connection interval) condition the script
  1. tells the XIAO the condition (CFG) and reads back what the link actually negotiated (STAT),
  2. tells the XIAO to send as fast as it can (START) while counting bytes on the dongle port,
  3. stops (STOP), drains the dongle port, and compares what arrived with what the XIAO sent.
Output per condition: <outdir>/t4_tp_len<L>_ci<C>ms_r<run>.csv  (bytes received in each second)
Output summary:       <outdir>/t4_tp_summary.csv         (one row per condition)

Usage: python throughput.py --xiao COM3 --dongle COM8 --outdir ../data
"""
import argparse
import csv
import os
import re
import statistics
import time
from datetime import datetime

import serial

PKT_RE = re.compile(r"^(\d+),(\d+),x*$")   # "<seq>,<millis>,xxxx" (the "\n" is the line end)


def send(ser, cmd):
    ser.write((cmd + "\n").encode())


def wait_for(ser, prefix, timeout=5.0):
    """Read lines from the XIAO control port until one starts with prefix."""
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        line = ser.readline().decode(errors="replace").strip()
        if line.startswith(prefix):
            return line
    raise RuntimeError(f"no '{prefix}' reply from XIAO within {timeout} s")


def kv(line):
    """'STAT mtu=247 ci_units=12 dl=251' -> {'mtu': 247, ...}"""
    return {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", line)}


class Receiver:
    """Splits the dongle byte stream into lines and tracks sequence numbers."""

    def __init__(self):
        self.rest = b""
        self.seqs = []          # seq of every valid packet, in arrival order
        self.bad = 0            # lines that do not look like a packet

    def feed(self, data):
        self.rest += data
        *lines, self.rest = self.rest.split(b"\n")
        for raw in lines:
            m = PKT_RE.match(raw.decode(errors="replace"))
            if m:
                self.seqs.append(int(m.group(1)))
            else:
                self.bad += 1


def run_condition(xiao, dongle, length, ci_ms, duration, outdir, run):
    send(xiao, f"CFG {length} {ci_ms}")
    wait_for(xiao, "OK CFG")
    time.sleep(3.0)                      # let the central accept the new connection parameters
    send(xiao, "STAT")
    stat = wait_for(xiao, "STAT")
    if "disconnected" in stat:
        raise RuntimeError("XIAO is not connected to the dongle")
    link = kv(stat)

    dongle.reset_input_buffer()
    rx = Receiver()
    bins = [0] * duration
    send(xiao, "START")
    wait_for(xiao, "OK START")
    t0 = time.monotonic()
    while True:
        t = time.monotonic() - t0
        if t >= duration:
            break
        data = dongle.read(4096)
        if data:
            bins[int(t)] += len(data)
            rx.feed(data)
    send(xiao, "STOP")
    done = kv(wait_for(xiao, "DONE"))
    drain_end = time.monotonic() + 2.0   # packets still in flight or in the TX FIFO
    drained = 0
    while time.monotonic() < drain_end:
        data = dongle.read(4096)
        drained += len(data)
        rx.feed(data)

    tx_pkts = done["pkts"]
    rx_pkts = len(set(rx.seqs))
    gaps = (max(rx.seqs) - min(rx.seqs) + 1 - rx_pkts) if rx.seqs else 0
    rx_bytes = sum(bins)
    result = {
        "run": run,
        "run_time": datetime.now().isoformat(timespec="seconds"),
        "len_req": length,
        "ci_req_ms": ci_ms,
        "ci_actual_ms": round(link["ci_units"] * 1.25, 2),
        "mtu": link["mtu"],
        "data_len": link["dl"],
        "duration_s": duration,
        "rx_bytes": rx_bytes,
        "avg_Bps": round(rx_bytes / duration, 1),
        "std_Bps": round(statistics.pstdev(bins), 1),
        "max_Bps": max(bins),
        "tx_pkts": tx_pkts,
        "rx_pkts": rx_pkts,
        "loss_pct": round(100.0 * (tx_pkts - rx_pkts) / tx_pkts, 2) if tx_pkts else "",
        "seq_gaps": gaps,
        "bad_lines": rx.bad,
    }

    name = f"t4_tp_len{length}_ci{ci_ms:g}ms_r{run}.csv"
    with open(os.path.join(outdir, name), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_s", "rx_bytes"])
        for i, b in enumerate(bins):
            w.writerow([i, b])
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xiao", required=True, help="XIAO control port, e.g. COM3")
    ap.add_argument("--dongle", required=True, help="dongle data port, e.g. COM8")
    ap.add_argument("--lens", default="20,100,240", help="packet lengths in bytes")
    ap.add_argument("--cis", default="7.5,15,30,50,100", help="connection intervals in ms")
    ap.add_argument("--duration", type=int, default=30, help="seconds per condition")
    ap.add_argument("--run", type=int, default=1, help="repeat number, goes into file names and the summary")
    ap.add_argument("--outdir", default=os.path.join(os.path.dirname(__file__), "..", "data"))
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)

    lens = [int(x) for x in args.lens.split(",")]
    cis = [float(x) for x in args.cis.split(",")]
    summary_path = os.path.join(args.outdir, "t4_tp_summary.csv")

    with serial.Serial(args.xiao, 115200, timeout=0.5) as xiao, \
            serial.Serial(args.dongle, 115200, timeout=0.05) as dongle:
        for ci_ms in cis:
            for length in lens:
                print(f"len={length} B, ci={ci_ms:g} ms ...", flush=True)
                r = run_condition(xiao, dongle, length, ci_ms, args.duration, args.outdir, args.run)
                print(f"  actual ci={r['ci_actual_ms']} ms, mtu={r['mtu']}, "
                      f"{r['avg_Bps']} B/s, loss={r['loss_pct']} %", flush=True)
                new = not os.path.exists(summary_path)
                with open(summary_path, "a", newline="") as f:
                    w = csv.DictWriter(f, fieldnames=list(r))
                    if new:
                        w.writeheader()
                    w.writerow(r)


if __name__ == "__main__":
    main()
