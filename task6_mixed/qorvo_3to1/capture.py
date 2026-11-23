"""Run three serialized DS-TWR exchanges per measurement round and save packet CSV."""

import argparse
import csv
from dataclasses import dataclass
from datetime import datetime
from hashlib import sha256
import json
from pathlib import Path
import re
import secrets
import subprocess
from threading import Event, Thread
import time


HERE = Path(__file__).resolve().parent
MAGIC = 0x55574233
ENTRY = 0x20004000
PACKETS = ("poll", "response", "final")
RX_TIMEOUT = 0x00220000  # DW3110 RXPTO | RXFTO


def now():
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def words_in_dump(text, address, count):
    words = {}
    for match in re.finditer(r"^(?:J-Link>\s*)?([0-9A-Fa-f]{8})\s*=\s*((?:[0-9A-Fa-f]{8}\s*)+)$", text, re.M):
        start = int(match[1], 16)
        for index, word in enumerate(match[2].split()):
            words[start + 4 * index] = int(word, 16)
    return [words[address + 4 * index] for index in range(count)]


@dataclass
class Probe:
    address: int
    serial: str
    jlink: str
    build: Path

    def __post_init__(self):
        folder = self.build / f"A{self.address}"
        self.image = (folder / "cdk-initiator.bin").resolve()
        self.manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8-sig"))
        if (self.manifest["anchor_id"] != self.address or self.manifest["channel"] != 5 or
                self.manifest["image_sha256"] != sha256(self.image.read_bytes()).hexdigest()):
            raise ValueError(f"A{self.address} image does not match its manifest")
        link_map = (folder / "cdk-initiator.map").read_text(encoding="utf-8", errors="replace")
        symbols = {name: int(value, 16) for value, name in re.findall(
            r"^\s*(0x[0-9a-fA-F]+)[ \t]+(start|mailbox)[ \t]*$", link_map, re.M
        )}
        self.mailbox = symbols["mailbox"]
        if symbols["start"] != ENTRY or not ENTRY + self.image.stat().st_size <= self.mailbox <= 0x2001FFC8:
            raise ValueError(f"A{self.address} has unexpected RAM image addresses")

    def call(self, *commands):
        result = subprocess.run(
            [self.jlink, "-USB", self.serial, "-Device", "NRF52833_XXAA", "-If", "SWD",
             "-Speed", "4000", "-AutoConnect", "1", "-NoGui", "1"],
            input="\n".join(("ExitOnError 1", *commands, "exit", "")),
            capture_output=True, text=True, errors="replace", timeout=20,
        )
        if result.returncode != 0:
            raise RuntimeError(f"A{self.address} J-Link failed: {result.stdout[-1000:]}")
        return result.stdout

    def read(self, *commands):
        report = self.call(*commands, f"mem32 0x{self.mailbox:08X} 0xE")
        try:
            words = words_in_dump(report, self.mailbox, 14)
        except KeyError as error:
            raise RuntimeError(f"A{self.address} mailbox could not be read: {report[-1000:]}") from error
        if words[:3] != [MAGIC, self.address, 1]:
            raise RuntimeError(f"A{self.address} is not ready or has the wrong image: {words[:3]}")
        return words

    def load(self):
        self.read("r", f'loadbin "{self.image}", 0x{ENTRY:X}', f'verifybin "{self.image}", 0x{ENTRY:X}',
                  f"SetPC 0x{ENTRY:X}", "g", "Sleep 100")

    def range_once(self, session, round_number, guard_ms):
        timestamp = now()
        words = self.read(
            f"w4 0x{self.mailbox + 12:08X} 0x{session:08X}",
            f"w4 0x{self.mailbox + 16:08X} 0x{round_number:08X}",
            f"w4 0x{self.mailbox + 20:08X} 0x{round_number:08X}",
            f"Sleep {guard_ms}",
        )
        if words[6:9] != [round_number, session, round_number]:
            raise RuntimeError(f"A{self.address} did not acknowledge round {round_number}: {words[6:9]}")
        return {"time": timestamp, "anchor": self.address, "round": round_number, "stages": words[9],
                "response_status": words[10], "final_status": words[11],
                "response_rssi": signed32(words[12]), "response_fp": signed32(words[13])}


def signed32(word):
    return word - (1 << 32) if word & (1 << 31) else word


def tag_records(lines, session):
    records = {}
    for line in lines:
        text = line.strip().split("\t", 1)[-1]
        if not text.startswith("TWR3,"):
            continue
        columns = text.split(",")
        if len(columns) != 14:
            raise ValueError(f"Invalid tag record: {text}")
        values = [int(column) for column in columns[1:12]] + [int(column, 16) for column in columns[12:]]
        tag_session, round_number, anchor, *event = values
        if tag_session != session:
            continue
        key = (round_number, anchor)
        if key in records or anchor not in (1, 2, 3) or round_number < 1:
            raise ValueError(f"Duplicate or invalid tag exchange: {key}")
        records[key] = event
    return records


def dbm(value):
    return "" if value == 32767 else f"{value / 256:.2f}"


def packet_rows(attempt, tag):
    response_sent, final_rx, distance_valid, distance, poll_rssi, poll_fp, final_rssi, final_fp, poll_status, final_status = (
        tag if tag is not None else (0, 0, 0, 0, 32767, 32767, 32767, 32767, 0, 0)
    )
    stages = attempt["stages"]
    return (
        ("poll", bool(stages & 1), tag is not None, poll_rssi, poll_fp, "", poll_status),
        ("response", bool(response_sent), bool(stages & 2), attempt["response_rssi"], attempt["response_fp"], "",
         attempt["response_status"]),
        ("final", bool(stages & 4), bool(final_rx), final_rssi, final_fp,
         f"{distance / 1000:.3f}" if distance_valid else "", final_status),
    )


def write_results(out, session, attempts, tags, planned_rounds, capture_error=None):
    counts = {str(anchor): {packet: {"expected": 0, "sent": 0, "received": 0, "valid": 0, "lost_after_tx": 0}
                            for packet in PACKETS} for anchor in (1, 2, 3)}
    successes = {}
    with (out / "packets.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("timestamp", "session", "round", "anchor_id", "packet", "tx_ok", "rx_ok", "success",
                         "status", "rssi_dbm", "first_path_dbm", "distance_m", "rx_sys_status"))
        for attempt in attempts:
            anchor, round_number = attempt["anchor"], attempt["round"]
            tag = tags.get((round_number, anchor))
            exchange_ok = tag is not None and bool(tag[2])
            for packet, sent, received, rssi, first_path, distance, status in packet_rows(attempt, tag):
                success = sent and received
                exchange_ok &= success
                counters = counts[str(anchor)][packet]
                counters["expected"] += 1
                counters["sent"] += sent
                counters["received"] += received
                counters["valid"] += success
                counters["lost_after_tx"] += sent and not received
                if success:
                    outcome = "ok"
                elif tag is None and packet != "response":
                    outcome = "no_tag_record"
                elif not sent:
                    outcome = "no_tx"
                else:
                    outcome = "rx_timeout" if status & RX_TIMEOUT else "not_received"
                writer.writerow((attempt["time"], session, round_number, anchor, packet, int(sent), int(received),
                                 int(success), outcome, dbm(rssi), dbm(first_path), distance, f"0x{status:08X}"))
            successes.setdefault(round_number, {})[anchor] = exchange_ok
    complete = [all(row.values()) for row in successes.values() if len(row) == 3]
    with (out / "rounds.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("session", "round", "a1_distance_m", "a2_distance_m", "a3_distance_m", "status"))
        for round_number, results in successes.items():
            distances = [f"{tags[(round_number, anchor)][3] / 1000:.3f}" if results.get(anchor) else ""
                         for anchor in (1, 2, 3)]
            status = "partial" if len(results) != 3 else "ok" if all(results.values()) else "failed"
            writer.writerow((session, round_number, *distances, status))
    max_failures = failures = 0
    for success in complete:
        failures = 0 if success else failures + 1
        max_failures = max(max_failures, failures)
    for packets in counts.values():
        for counters in packets.values():
            sent = counters["sent"]
            expected = counters["expected"]
            counters["loss_after_tx_rate"] = counters["lost_after_tx"] / sent if sent else None
            counters["miss_rate_per_expected_packet"] = 1 - counters["valid"] / expected if expected else None
    summary = {"mode": "9-packet", "channel": 5, "session": session, "planned_rounds": planned_rounds,
               "complete_rounds": len(complete), "successful_rounds": sum(complete),
               "all_three_ranges_rate": sum(complete) / len(complete) if complete else None,
               "max_consecutive_failed_rounds": max_failures, "tag_records": len(tags),
               "capture_error": capture_error,
               "anchors": counts}
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")


def capture(args):
    import serial  # pyserial is only required for the live bench.

    session = secrets.randbelow(0xFFFFFFFF) + 1
    probes = [Probe(index, serial_id, args.jlink, args.build) for index, serial_id in enumerate(args.probes, 1)]
    tag_sha = sha256(args.tag_elf.read_bytes()).hexdigest() if args.tag_elf else None
    args.out.mkdir(parents=True)
    (args.out / "run.json").write_text(json.dumps({
        "start_time": now(), "session": session, "mode": "9-packet", "channel": 5,
        "rounds": args.rounds, "guard_ms": args.guard_ms, "tag_port": args.tag_port,
        "tag_elf_sha256": tag_sha, "anchors": [probe.manifest for probe in probes],
    }, indent=2) + "\n", encoding="utf-8")
    attempts = []
    errors = []
    failure = None
    stop = Event()
    with serial.Serial(args.tag_port, 115200, timeout=0.1) as tag, \
            (args.out / "tag.raw.log").open("w", encoding="utf-8") as raw, \
            (args.out / "anchor.jsonl").open("w", encoding="utf-8") as anchors:
        def read_tag():
            try:
                buffer = bytearray()
                while not stop.is_set():
                    buffer.extend(tag.read(tag.in_waiting or 1))
                    while b"\n" in buffer:
                        line, _, remainder = buffer.partition(b"\n")
                        buffer = bytearray(remainder)
                        raw.write(f"{now()}\t{line.rstrip().decode('utf-8', errors='replace')}\n")
                        raw.flush()
                    if len(buffer) > 1024:
                        raise RuntimeError("Tag stream exceeded the expected line length")
            except (serial.SerialException, OSError, RuntimeError) as error:
                errors.append(error)

        reader = Thread(target=read_tag, daemon=True)
        reader.start()
        try:
            for probe in probes:
                probe.load()
            for round_number in range(1, args.rounds + 1):
                for probe in probes:
                    if errors:
                        raise RuntimeError("Tag capture failed") from errors[0]
                    attempt = probe.range_once(session, round_number, args.guard_ms)
                    attempts.append(attempt)
                    anchors.write(json.dumps(attempt) + "\n")
                    anchors.flush()
                if round_number % 25 == 0:
                    print(f"round {round_number}/{args.rounds}", flush=True)
            time.sleep(0.25)
        except (OSError, RuntimeError, subprocess.TimeoutExpired, KeyboardInterrupt) as error:
            failure = error
        finally:
            stop.set()
            reader.join()
    failure = failure or (errors[0] if errors else None)
    with (args.out / "tag.raw.log").open(encoding="utf-8") as raw:
        tags = tag_records(raw, session)
    write_results(args.out, session, attempts, tags, args.rounds,
                  f"{type(failure).__name__}: {failure}" if failure else None)
    if failure:
        raise RuntimeError("Capture stopped early. Partial results were saved.") from failure
    if not tags:
        raise RuntimeError("No matching tag records. Check the tag firmware, VCP and setup before using the RF totals.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probes", nargs=3, metavar=("A1", "A2", "A3"), required=True)
    parser.add_argument("--tag-port", required=True)
    parser.add_argument("--tag-elf", type=Path, help="record the SHA-256 of the tag ELF used for this run")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--rounds", type=int, default=1000)
    parser.add_argument("--guard-ms", type=int, default=50)
    parser.add_argument("--jlink", default="JLink.exe")
    parser.add_argument("--build", type=Path, default=HERE.parent / "anchor_3001" / "qorvo_1to1" / "build")
    args = parser.parse_args()
    if (not 1 <= args.rounds <= 0xFFFFFFFF or not 25 <= args.guard_ms <= 1000 or
            len(set(args.probes)) != 3 or not all(re.fullmatch(r"[0-9]+", probe) for probe in args.probes)):
        parser.error("supply three distinct numeric J-Link serials, positive rounds, and guard-ms between 25 and 1000")
    try:
        capture(args)
    except (OSError, KeyError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"{error}\n")
