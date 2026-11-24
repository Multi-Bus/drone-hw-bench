"""Install the tag responder in a dedicated copy of the team's FSBL project."""

import argparse
from pathlib import Path
import re
import shutil


def prepare(fsbl: Path, antenna_delay: int) -> None:
    if not 0 <= antenna_delay <= 65535:
        raise ValueError("antenna delay must be a uint16 in DW time units")
    uwb = fsbl / "Core" / "UWB" / "Src"
    example = uwb / "examples" / "ex_05b_ds_twr_resp"
    selection = uwb / "example_selection.h"
    responder = example / "ds_twr_responder.c"
    if not responder.is_file() or not selection.is_file():
        raise ValueError("expected the handoff project's Qorvo responder and example_selection.h")
    contents = selection.read_bytes().decode("utf-8")
    selector = re.compile(r"^([ \t]*)(?://[ \t]*)?(#define[ \t]+TEST_DS_TWR_RESPONDER)\b", re.M)
    if len(selector.findall(contents)) != 1:
        raise ValueError("the handoff must have exactly one TEST_DS_TWR_RESPONDER selector")
    contents = re.sub(r"^([ \t]*)(#define[ \t]+TEST_[A-Z0-9_]+)\b", r"\1//\2", contents, flags=re.M)
    contents = selector.sub(r"\1\2", contents)
    source = Path(__file__).resolve().parent
    for filename in ("frames.h", "radio.h"):
        shutil.copyfile(source / filename, example / filename)
    (example / "tag_antenna_delay.h").write_text(f"#define UWB3_ANTENNA_DELAY {antenna_delay}U\n", encoding="ascii")
    shutil.copyfile(source / "tag.c", responder)
    selection.write_bytes(contents.encode("utf-8"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fsbl-copy", required=True, type=Path, help="FSBL directory in a disposable handoff copy")
    parser.add_argument("--antenna-delay", type=int, default=16385)
    args = parser.parse_args()
    try:
        prepare(args.fsbl_copy, args.antenna_delay)
    except (OSError, ValueError) as error:
        parser.exit(2, f"{error}\n")
