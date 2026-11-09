#!/usr/bin/env python3
"""Task 3 Lv2: turn the Tera Term logs in ../data/t3_lv2_*.txt into one CSV and Markdown tables.

Usage:  python summarize_logs.py            (run from anywhere; paths are relative to this file)
Output: ../data/t3_lv2_summary.csv   all reports of all logs, one row per (file, benchmark)
        Markdown tables on stdout     (pasted into ../README.md)

Log filename = condition: t3_lv2_bench_<opt>_<cache>_board<N>.txt, t3_lv2_isr_<opt>_<cache>_board<N>.txt
"""
import csv
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "data")

RE_HEAD = re.compile(r"^(.+?): n=(\d+), clock=(\d+) Hz\s*$")
RE_STAT = re.compile(r"^\s+(mean|min|max)\s+(\d+) cycles\s+([\d.]+) us\s*$")
RE_MAXAT = re.compile(r"^\s+max at sample #(\d+)\s*$")
RE_OUT = re.compile(r"^\s+samples > min\+(\d+) cycles: (\d+) of (\d+)\s*$")
RE_DIFF = re.compile(r"max \|plain C - CMSIS-DSP\| = (\d+) e-6")
RE_FILE = re.compile(r"t3_lv2_(bench|isr)_(O\d)_(cache(?:on|off))_board(\d+)\.txt$")


def parse(path):
    rows, cur, diff = [], None, None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            m = RE_HEAD.match(line)
            if m:
                cur = {"benchmark": m.group(1), "n": int(m.group(2)), "clock_hz": int(m.group(3))}
                rows.append(cur)
                continue
            if cur is None:
                continue
            m = RE_STAT.match(line)
            if m:
                cur[m.group(1) + "_cycles"] = int(m.group(2))
                cur[m.group(1) + "_us"] = float(m.group(3))
                continue
            m = RE_MAXAT.match(line)
            if m:
                cur["max_at_sample"] = int(m.group(1))
                continue
            m = RE_OUT.match(line)
            if m:
                cur["samples_gt_min_plus_100"] = int(m.group(2))
                continue
            m = RE_DIFF.search(line)
            if m:
                diff = int(m.group(1))
    return rows, diff


def main():
    files = sorted(glob.glob(os.path.join(DATA, "t3_lv2_*.txt")))
    if not files:
        sys.exit("no t3_lv2_*.txt in " + DATA)
    out_rows, by_file, diffs = [], {}, {}
    for p in files:
        name = os.path.basename(p)
        m = RE_FILE.match(name)
        if not m:
            print("skip (name does not match):", name, file=sys.stderr)
            continue
        kind, opt, cache, board = m.groups()
        rows, diff = parse(p)
        by_file[name] = rows
        if diff is not None:
            diffs[name] = diff
        for r in rows:
            out_rows.append({"file": name, "kind": kind, "optimization": opt, "cache": cache,
                             "board": int(board), **r})

    cols = ["file", "kind", "optimization", "cache", "board", "benchmark", "n", "clock_hz",
            "mean_cycles", "mean_us", "min_cycles", "min_us", "max_cycles", "max_us",
            "max_at_sample", "samples_gt_min_plus_100"]
    csv_path = os.path.join(DATA, "t3_lv2_summary.csv")
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore")
        w.writeheader()
        w.writerows(out_rows)
    print("wrote", os.path.relpath(csv_path), "(%d rows)" % len(out_rows), file=sys.stderr)

    # ---- Markdown: condition x benchmark (benchmark logs only; the ISR log's benchmark part is a repeat run)
    conds = [("bench_O0_cacheon", "-O0, 캐시 켬"), ("bench_O2_cacheon", "-O2, 캐시 켬"),
             ("bench_O3_cacheon", "-O3, 캐시 켬"), ("bench_O3_cacheoff", "-O3, 캐시 끔")]
    tables = {}
    for key, _ in conds:
        fn = "t3_lv2_%s_board2.txt" % key
        tables[key] = {r["benchmark"]: r for r in by_file.get(fn, [])}
    names = []
    for key, _ in conds:
        for b in tables[key]:
            if b not in names:
                names.append(b)

    def cell(key, b, field):
        r = tables[key].get(b)
        return "-" if not r else "{:,}".format(r[field + "_cycles"])

    def table(field, title):
        print("\n" + title + "\n")
        print("| 항목 | " + " | ".join(c[1] for c in conds) + " |")
        print("| --- | " + " | ".join("---:" for _ in conds) + " |")
        for b in names:
            print("| " + b + " | " + " | ".join(cell(k, b, field) for k, _ in conds) + " |")

    table("mean", "평균 (사이클)")
    table("max", "최대 (사이클)")

    # ---- repeat of the -O3 / cache-on benchmark inside the ISR log
    a = tables["bench_O3_cacheon"]
    rep = {r["benchmark"]: r for r in by_file.get("t3_lv2_isr_O3_cacheon_board2.txt", [])}
    print("\n반복 측정 (-O3, 캐시 켬): 같은 조건을 두 번 잰 평균 (사이클)\n")
    print("| 항목 | 1회 (`bench` 로그) | 2회 (`isr` 로그의 벤치마크 부분) |")
    print("| --- | ---: | ---: |")
    for b, r in a.items():
        r2 = rep.get(b)
        print("| " + b + " | {:,} | ".format(r["mean_cycles"]) + ("{:,}".format(r2["mean_cycles"]) if r2 else "-") + " |")

    # ---- 1 kHz interrupt test
    isr = {r["benchmark"]: r for r in by_file.get("t3_lv2_isr_O3_cacheon_board2.txt", [])}
    print("\n1 kHz 인터럽트 시험 (-O3, 캐시 켬, n=%s)\n" % next((r["n"] for k, r in isr.items() if k.startswith("ISR period")), "?"))
    print("| 항목 | 평균 | 최소 | 최대 | 최대 (µs) | 최대 위치 |")
    print("| --- | ---: | ---: | ---: | ---: | ---: |")
    for k, r in isr.items():
        if k.startswith(("ISR period", "period dev", "ISR exec", "interrupt lat")):
            print("| {} | {:,} | {:,} | {:,} | {} | #{} |".format(k.replace("|", "\\|"), r["mean_cycles"], r["min_cycles"], r["max_cycles"],
                                                              r["max_us"], r.get("max_at_sample", "?")))

    # ---- board 2 vs board 1, same build (isr logs)
    b2 = isr
    b1 = {r["benchmark"]: r for r in by_file.get("t3_lv2_isr_O3_cacheon_board1.txt", [])}
    if b1:
        print("\n**보드 2 vs 보드 1 (같은 빌드, -O3, 캐시 켬, `isr` 로그, 사이클)**\n")
        print("| 항목 | 보드 2 평균 | 보드 1 평균 | 보드 2 최대 | 보드 1 최대 |")
        print("| --- | ---: | ---: | ---: | ---: |")
        for k, r in b2.items():
            q = b1.get(k)
            if q:
                print("| {} | {:,} | {:,} | {:,} | {:,} |".format(k.replace("|", "\\|"), r["mean_cycles"], q["mean_cycles"],
                                                                 r["max_cycles"], q["max_cycles"]))

    for n, d in diffs.items():
        print("\n%s: max |plain C - CMSIS-DSP| = %d e-6" % (n, d))


if __name__ == "__main__":
    main()
