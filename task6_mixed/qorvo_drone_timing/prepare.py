"""Install the timing bench in a dedicated copy of the N6 handoff FSBL."""

import argparse
from pathlib import Path
import re
import shutil


def integer(text):
    return int(text, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fsbl-copy", required=True, type=Path)
    parser.add_argument("--role", required=True, choices=("initiator", "responder"))
    parser.add_argument("--address", required=True, type=integer)
    parser.add_argument("--peers", type=integer, nargs="+", default=[])
    parser.add_argument("--session", type=integer)
    parser.add_argument("--rounds-per-peer", type=int, default=10_000)
    parser.add_argument("--payload", type=int, choices=(0, 20, 40), default=20)
    parser.add_argument("--channel", type=int, choices=(5, 9), default=5)
    parser.add_argument("--period-ms", type=int, default=20)
    parser.add_argument("--response-uus", type=int, default=900)
    parser.add_argument("--final-uus", type=int, default=700)
    args = parser.parse_args()
    initiator = args.role == "initiator"
    if any(not 1 <= address <= 65534 for address in [args.address, *args.peers]):
        parser.error("addresses must be between 1 and 65534")
    if args.address in args.peers or len(set(args.peers)) != len(args.peers):
        parser.error("peer addresses must be unique and different from the local address")
    if initiator and (not args.peers or not 1 <= (args.session or 0) <= 0xFFFFFFFF):
        parser.error("initiator requires at least one peer and a nonzero uint32 session ID")
    if not initiator and (args.peers or args.session is not None):
        parser.error("only the initiator sets peers and session")
    if not 1 <= args.rounds_per_peer * max(1, len(args.peers)) <= 0xFFFFFFFE:
        parser.error("total attempts must be between 1 and 4294967294")
    if not 0 <= args.period_ms <= 1000:
        parser.error("period must be 0 to 1000 ms")
    if not all(250 <= delay <= 4000 for delay in (args.response_uus, args.final_uus)):
        parser.error("response and final delay must be 250 to 4000 UUS")

    uwb = args.fsbl_copy / "Core" / "UWB" / "Src"
    selection = uwb / "example_selection.h"
    example = uwb / "examples" / "ex_05b_ds_twr_resp"
    target = example / "ds_twr_responder.c"
    source = Path(__file__).resolve().parent
    try:
        contents = selection.read_text(encoding="utf-8")
        if not target.is_file():
            raise ValueError("expected a dedicated copy of the N6 handoff FSBL")
        selector = re.compile(r"^([ \t]*)(?://[ \t]*)?(#define[ \t]+TEST_DS_TWR_RESPONDER)\b", re.M)
        if len(selector.findall(contents)) != 1:
            raise ValueError("expected exactly one TEST_DS_TWR_RESPONDER selector")
        contents = re.sub(r"^([ \t]*)(#define[ \t]+TEST_[A-Z0-9_]+)\b", r"\1//\2", contents, flags=re.M)
        contents = selector.sub(r"\1\2", contents)

        config = {
            "INITIATOR": int(initiator), "ADDRESS": args.address,
            "SESSION": args.session or 0, "ROUNDS": args.rounds_per_peer * len(args.peers),
            "PAYLOAD": args.payload, "CHANNEL": args.channel, "PERIOD_MS": args.period_ms,
            "RESPONSE_DELAY": args.response_uus, "FINAL_DELAY": args.final_uus,
            "RESPONSE_RX_DELAY": args.response_uus - 200,
            "FINAL_RX_DELAY": args.final_uus - 200, "ANTENNA_DELAY": 16385,
        }
        header = "".join(f"#define BENCH_{key} {value}U\n" for key, value in config.items())
        header += "#define BENCH_PEERS { " + ", ".join(map(str, args.peers or [0])) + " }\n"
        for name in ("frames.h", "radio.h"):
            shutil.copyfile(source.parent / "qorvo_3to1" / name, example / name)
        (example / "bench_config.h").write_text(header, encoding="ascii")
        shutil.copyfile(source / "bench.c", target)
        selection.write_text(contents, encoding="utf-8")
    except (OSError, ValueError) as error:
        parser.exit(2, f"{error}\n")


if __name__ == "__main__":
    main()
