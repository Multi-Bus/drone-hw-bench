"""Join N6 UART logs by session and round, and report radio timing and failures."""

import argparse
from collections import Counter, defaultdict
import csv
from dataclasses import dataclass
import json
import math
from pathlib import Path
import statistics


@dataclass(frozen=True)
class Attempt:
    session: int
    round: int
    source: int
    peer: int
    payload: int
    result: int
    poll: int
    final: int
    status: str
    started_ms: int
    completed_ms: int

    @property
    def key(self):
        return self.session, self.round, self.source, self.peer


def read_log(path, role):
    attempts = {}
    ready = None
    done = False
    for line in path.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        fields = line.strip().split(",")
        if fields[0] == "DWB_READY":
            if ready is not None or len(fields) != 11 or fields[1] != role:
                raise ValueError(f"invalid or repeated startup marker in {path}")
            ready = list(map(int, fields[2:]))
        elif fields[0] == "DWB_DONE":
            done = True
        elif fields[0] == "DWB":
            if len(fields) != 13 or fields[1] != role or done:
                raise ValueError(f"invalid bench row in {path}: {line}")
            row = Attempt(*map(int, fields[2:8]), int(fields[8], 16), int(fields[9], 16),
                          fields[10], int(fields[11]), int(fields[12]))
            if row.key in attempts:
                raise ValueError(f"duplicate attempt {row.key} in {path}")
            if ready and (row.payload != ready[2] or (row.source if role == 'I' else row.peer) != ready[0]):
                raise ValueError(f"row does not match startup configuration in {path}: {line}")
            attempts[row.key] = row
    return attempts, ready, done


def packet_us(psdu_bytes):
    # Qorvo DW3000 datasheet, Frame Format and Symbol Timings. PSDU includes FCS.
    bits = psdu_bytes * 8
    return (128 + 8) * 1.01763 + 21 * 1.02564 + (bits + math.ceil(bits / 330) * 48) * 0.12821


def delta(value, previous, bits):
    return (value - previous) % (1 << bits)


def distribution(values):
    if not values:
        return None
    ordered = sorted(values)
    return {"samples": len(values), "mean": statistics.mean(values),
            "p95": ordered[math.ceil(len(values) * .95) - 1], "max": ordered[-1]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--initiator", required=True, type=Path)
    parser.add_argument("--responders", required=True, nargs="+", type=Path)
    parser.add_argument("--out", required=True, type=Path, help="new CSV output path")
    args = parser.parse_args()
    try:
        attempts, config, done = read_log(args.initiator, "I")
        rows = sorted(attempts.values(), key=lambda row: row.round)
        if not rows or not config or not done or len(rows) != config[-1] or any(
            row.session != rows[0].session or row.round != index for index, row in enumerate(rows, 1)
        ):
            raise ValueError("initiator log must contain startup, every round of one session, and DWB_DONE")
        replies = {}
        captured_peers = set()
        for path in args.responders:
            observed, responder, _ = read_log(path, "R")
            if not responder or responder[0] in captured_peers or (
                responder[1:3] != config[1:3] or responder[4:8] != config[4:8]
            ):
                raise ValueError(f"responder startup configuration differs or address is duplicated: {path}")
            captured_peers.add(responder[0])
            replies.update(observed)
        if {row.peer for row in rows} != captured_peers:
            raise ValueError("provide one complete responder log per peer")
        peers = [row.peer for row in rows[:len(captured_peers)]]
        if len(set(peers)) != len(captured_peers) or len(rows) % len(peers) or any(
            row.peer != peers[index % len(peers)] for index, row in enumerate(rows)
        ):
            raise ValueError("initiator rounds do not follow a complete, repeating peer order")

        fields = ("session", "round", "source", "peer", "payload_bytes", "initiator_result",
                  "responder_result", "confirmed", "poll_tx_dtu", "final_tx_dtu", "exchange_us",
                  "slot_estimate_us", "airtime_estimate_us", "started_ms", "completed_ms",
                  "initiator_status", "responder_status")
        stats = defaultdict(Counter)
        timing = defaultdict(list)
        gaps = defaultdict(list)
        last_success = {}
        sweep_durations = []
        sweep_periods = []
        previous_sweep = None
        shr_us = (128 + 8) * 1.01763
        final_us = packet_us(32)
        airtime = packet_us(20) + packet_us(20 + config[2]) + final_us
        with args.out.open("x", newline="", encoding="utf-8") as output:
            writer = csv.writer(output)
            writer.writerow(fields)
            for row in rows:
                reply = replies.get(row.key)
                confirmed = row.result == 0 and reply is not None and reply.result == 0
                exchange = delta(row.final, row.poll, 40) / 63897.6 if row.result == 0 else None
                writer.writerow((row.session, row.round, row.source, row.peer, row.payload, row.result,
                                 reply.result if reply else None, int(confirmed), f"{row.poll:010x}",
                                 f"{row.final:010x}", exchange, exchange + final_us if exchange is not None else None,
                                 airtime if exchange is not None else None, row.started_ms, row.completed_ms, row.status,
                                 reply.status if reply else None))
                counts = stats[row.peer]
                counts["attempts"] += 1
                counts["confirmed"] += confirmed
                counts["missing_responder_row"] += reply is None
                if row.peer == peers[0]:
                    if previous_sweep is not None:
                        sweep_periods.append(delta(row.started_ms, previous_sweep, 32))
                    previous_sweep = row.started_ms
                if row.peer == peers[-1]:
                    sweep_durations.append(delta(row.completed_ms, previous_sweep, 32))
                if confirmed:
                    timing[row.peer].append(exchange)
                    if row.peer in last_success:
                        gaps[row.peer].append(delta(row.started_ms, last_success[row.peer], 32))
                    last_success[row.peer] = row.started_ms
        result = {
            "channel": config[1], "payload_bytes": config[2], "configured_period_ms": config[3],
            "response_final_uus": config[4:6], "shr_estimate_us": shr_us,
            "sweep_first_start_to_last_finish_ms": distribution(sweep_durations),
            "sweep_start_to_start_ms": distribution(sweep_periods),
            "peers": {str(peer): {**counts, "unconfirmed_rate": 1 - counts["confirmed"] / counts["attempts"],
                                  "exchange_us": distribution(timing[peer]),
                                  "successful_update_gap_ms": distribution(gaps[peer])}
                      for peer, counts in stats.items()},
        }
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(2, f"{error}\n")


if __name__ == "__main__":
    main()
