"""Task 4: raw CSV -> result tables and graphs.

Reads   data/lv1/pc_rx, data/lv2/{throughput,rate_check,rtt}
Writes  the tables between <!-- BEGIN:table --> and <!-- END:table --> in results/*.md
        and the graphs in results/figures/*.png
Run:    python measure/analyze.py

The text around the tables in results/*.md (method, observations, limits, conclusion) is written
by hand and is never touched. Everything inside the markers is regenerated from the raw files, so
the tables always match the data.
"""
import csv
import statistics as st
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent          # task4_ble/
DATA = ROOT / "data"
RES = ROOT / "results"
FIG = RES / "figures"
FIG.mkdir(parents=True, exist_ok=True)

REQ_BPS = 3200                                           # 50 Hz x 64 B drone requirement


def read_csv(path):
    with open(path, newline="", encoding="utf-8-sig") as f:   # -sig: the Lv1 file was saved with a BOM
        return list(csv.DictReader(f))


def write_block(md_name, table):
    """Replace the text between the table markers in results/<md_name>."""
    path = RES / md_name
    s = path.read_text(encoding="utf-8")
    begin, end = "<!-- BEGIN:table -->", "<!-- END:table -->"
    i, j = s.index(begin) + len(begin), s.index(end)
    path.write_text(s[:i] + "\n" + table.strip() + "\n" + s[j:], encoding="utf-8")
    print("updated", md_name)


def md_table(header, rows):
    out = ["| " + " | ".join(header) + " |", "| " + " | ".join("---" for _ in header) + " |"]
    out += ["| " + " | ".join(str(c) for c in r) + " |" for r in rows]
    return "\n".join(out)


def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG / name, dpi=130)
    plt.close(fig)
    print("saved", name)


# ---------------------------------------------------------------- Lv1: PC reception
def lv1():
    rows = read_csv(DATA / "lv1" / "pc_rx" / "t4_lv1_dongle_rx_15s.csv")
    seq = [int(r["seq"]) for r in rows]
    ms = [int(r["millis"]) for r in rows]
    dt = [b - a for a, b in zip(ms, ms[1:])]
    gaps = sum(1 for a, b in zip(seq, seq[1:]) if b - a != 1)
    table = md_table(["항목", "값"], [
        ["수신 줄 수", len(rows)],
        ["seq 범위", f"{seq[0]} ~ {seq[-1]}"],
        ["seq 끊김", gaps],
        ["줄 간격 평균 / 표준편차 (ms)", f"{st.mean(dt):.1f} / {st.pstdev(dt):.1f}"],
        ["줄 간격 최소 / 최대 (ms)", f"{min(dt)} / {max(dt)}"],
    ])
    write_block("lv1.md", table)
    fig, ax = plt.subplots(figsize=(5.5, 3.2))
    ax.hist(dt, bins=range(min(dt), max(dt) + 2, 5), color="#4c78a8")
    ax.set_xlabel("interval between lines (XIAO millis, ms)")
    ax.set_ylabel("count")
    ax.set_title("Lv1: PC reception, 15 s")
    save(fig, "lv1_rx_interval.png")


# ---------------------------------------------------------------- Lv2: throughput
def throughput():
    d = DATA / "lv2" / "throughput"
    summary = read_csv(d / "summary.csv")
    conds = {}
    for r in summary:
        conds.setdefault((int(r["len_req"]), float(r["ci_req_ms"])), []).append(r)
    rows, stats = [], {}
    for (ln, ci), rs in sorted(conds.items()):
        bins = []
        run_avgs = []
        for r in rs:
            b = [int(x["rx_bytes"]) for x in read_csv(d / f"t4_tp_len{ln}_ci{ci:g}ms_r{r['run']}.csv")]
            bins += b
            run_avgs.append(st.mean(b) / 1000)
        loss = [float(r["loss_pct"]) for r in rs]
        stats[(ln, ci)] = (st.mean(bins) / 1000, min(run_avgs), max(run_avgs), st.mean(loss))
        rows.append([f"{ln} B", f"{ci:g} ms", f"{len(rs)}회 × {rs[0]['duration_s']} s",
                     f"{st.mean(bins) / 1000:.1f}", f"{st.pstdev(bins) / 1000:.1f}", f"{max(bins) / 1000:.1f}",
                     " / ".join(f"{a:.1f}" for a in run_avgs),
                     f"{100 - st.mean(loss):.2f}% (손실 평균 {st.mean(loss):.2f}%, 최대 {max(loss):g}%)"])
    write_block("lv2_throughput.md", md_table(
        ["패킷", "연결 간격", "측정 횟수", "평균 (kB/s)", "표준편차", "최대", "회차별 평균", "성공률"], rows))

    lens = sorted({k[0] for k in stats})
    cis = sorted({k[1] for k in stats})
    fig, ax = plt.subplots(figsize=(6.5, 4))
    for ln in lens:
        mean = [stats[(ln, c)][0] for c in cis]
        lo = [m - stats[(ln, c)][1] for m, c in zip(mean, cis)]
        hi = [stats[(ln, c)][2] - m for m, c in zip(mean, cis)]
        ax.errorbar(range(len(cis)), mean, yerr=[lo, hi], marker="o", capsize=3, label=f"{ln} B")
    ax.axhline(REQ_BPS / 1000, color="red", ls="--", lw=1, label="drone requirement 3.2 kB/s")
    ax.set_xticks(range(len(cis)), [f"{c:g}" for c in cis])
    ax.set_xlabel("connection interval (ms)")
    ax.set_ylabel("throughput (kB/s)")
    ax.set_yscale("log")
    ax.set_title("Lv2 throughput (mean of 3 runs, bars = min..max of the runs)")
    ax.legend()
    ax.grid(alpha=0.3, which="both")
    save(fig, "lv2_throughput.png")

    fig, ax = plt.subplots(figsize=(6.5, 3.6))
    w = 0.25
    for k, ln in enumerate(lens):
        ax.bar([i + (k - 1) * w for i in range(len(cis))], [stats[(ln, c)][3] for c in cis], w, label=f"{ln} B")
    ax.set_xticks(range(len(cis)), [f"{c:g}" for c in cis])
    ax.set_xlabel("connection interval (ms)")
    ax.set_ylabel("packet loss (%, mean of 3 runs)")
    ax.set_title("Lv2 throughput: packet loss at maximum send rate")
    ax.legend()
    ax.grid(alpha=0.3, axis="y")
    save(fig, "lv2_throughput_loss.png")


# ---------------------------------------------------------------- Lv2: fixed-rate check
def rate_check():
    d = DATA / "lv2" / "rate_check"
    summary = read_csv(d / "summary.csv")
    conds = {}
    for r in summary:
        conds.setdefault(float(r["ci_req_ms"]), []).append(r)
    rows, boxes, labels = [], [], []
    for ci, rs in sorted(conds.items()):
        bins = []
        for r in rs:
            bins += [int(x["rx_bytes"]) for x in
                     read_csv(d / f"t4_rate{r['rate_pps']}_len{r['len_req']}_ci{ci:g}ms_r{r['run']}.csv")]
        tx = sum(int(r["tx_pkts"]) for r in rs)
        rx = sum(int(r["rx_pkts"]) for r in rs)
        rows.append([f"{ci:g} ms", f"{len(rs)}회 × {rs[0]['duration_s']} s", tx, rx,
                     f"{100 * rx / tx:.2f}% (손실 {tx - rx})", f"{st.mean(bins):.1f} B/s",
                     f"{min(bins)} / {max(bins)}"])
        boxes.append(bins)
        labels.append(f"{ci:g}")
    write_block("lv2_rate_check.md", md_table(
        ["연결 간격", "측정 횟수", "보낸 패킷", "받은 패킷", "성공률", "수신 평균", "초당 최소 / 최대 (B/s)"], rows))
    fig, ax = plt.subplots(figsize=(5.5, 3.6))
    ax.boxplot(boxes, tick_labels=labels)
    ax.axhline(REQ_BPS, color="red", ls="--", lw=1, label="target 3200 B/s (50 Hz x 64 B)")
    ax.set_xlabel("connection interval (ms)")
    ax.set_ylabel("received bytes per second")
    ax.set_title("Lv2 requirement check (3 runs x 60 s per interval)")
    ax.legend(loc="lower right")
    ax.grid(alpha=0.3, axis="y")
    save(fig, "lv2_rate_check.png")


# ---------------------------------------------------------------- Lv2: RTT
def rtt():
    d = DATA / "lv2" / "rtt"
    cis = sorted(float(r["ci_req_ms"]) for r in read_csv(d / "summary.csv"))
    rows = []
    fig, ax = plt.subplots(figsize=(6.5, 4))
    for ci in cis:
        raw = read_csv(d / f"t4_rtt_len64_ci{ci:g}ms.csv")
        ok = sorted(float(r["rtt_ms"]) for r in raw if r["rtt_ms"] != "")
        rows.append([f"{ci:g} ms", len(raw), f"{st.mean(ok):.1f}", f"{st.pstdev(ok):.1f}", f"{ok[-1]:.1f}",
                     f"{ok[0]:.1f}", f"{ok[len(ok) // 2]:.1f}", f"{ok[int(len(ok) * 0.95)]:.1f}",
                     f"{100 * len(ok) / len(raw):g}%"])
        ax.plot(ok, [(i + 1) / len(ok) for i in range(len(ok))], label=f"{ci:g} ms")
    write_block("lv2_rtt.md", md_table(
        ["연결 간격", "측정 횟수", "평균 (ms)", "표준편차", "최대", "최소", "중앙값", "95%", "성공률"], rows))
    ax.set_xlabel("RTT (ms)")
    ax.set_ylabel("cumulative fraction")
    ax.set_title("Lv2 RTT, 1000 pings per connection interval")
    ax.legend(title="connection interval")
    ax.grid(alpha=0.3)
    save(fig, "lv2_rtt_cdf.png")


if __name__ == "__main__":
    lv1()
    throughput()
    rate_check()
    rtt()
