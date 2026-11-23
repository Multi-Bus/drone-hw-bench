import csv
import json
from pathlib import Path
import tempfile
import unittest

from capture import tag_records, words_in_dump, write_results


class CaptureTest(unittest.TestCase):
    def test_split_jlink_words(self):
        dump = "J-Link>20004000 = 55574233 00000001 \n20004008 = 00000001 00000000 \nJ-Link>"
        self.assertEqual(words_in_dump(dump, 0x20004000, 4), [0x55574233, 1, 1, 0])

    def test_strict_join_and_incomplete_rounds(self):
        def tag(session, round_number, anchor):
            return f"2026-11-23T01:00:00+09:00\tTWR3,{session},{round_number},{anchor},1,1,1,3528,-20000,-23000,-20100,-23100,00004000,00004080"

        lines = [tag(7, 1, anchor) for anchor in (1, 2, 3)]
        lines.extend((tag(7, 2, 1), tag(7, 2, 2), tag(8, 2, 3)))
        records = tag_records(lines, 7)
        attempts = []
        for round_number, anchor in ((1, 1), (1, 2), (1, 3), (2, 1), (2, 2), (2, 3), (3, 1)):
            attempts.append({"time": "2026-11-23T01:00:00+09:00", "anchor": anchor, "round": round_number,
                             "stages": 7 if (round_number, anchor) in records else 1,
                             "response_status": 0x20000, "final_status": 0,
                             "response_rssi": -20000, "response_fp": -23000})
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            write_results(output, 7, attempts, records, 3)
            summary = json.loads((output / "summary.json").read_text())
            self.assertEqual(summary["complete_rounds"], 2)
            self.assertEqual(summary["successful_rounds"], 1)
            self.assertEqual(summary["max_consecutive_failed_rounds"], 1)
            self.assertEqual(summary["anchors"]["3"]["poll"]["lost_after_tx"], 1)
            with (output / "packets.csv").open(newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 21)
            self.assertEqual(rows[2]["distance_m"], "3.528")
            self.assertEqual(rows[15]["status"], "no_tag_record")
            with (output / "rounds.csv").open(newline="") as stream:
                round_rows = list(csv.DictReader(stream))
            self.assertEqual([row["status"] for row in round_rows], ["ok", "failed", "partial"])
            self.assertEqual(round_rows[0]["a3_distance_m"], "3.528")

    def test_duplicate_tag_row_is_rejected(self):
        line = "TWR3,7,1,1,1,1,1,3528,-20000,-23000,-20100,-23100,00004000,00004080"
        with self.assertRaises(ValueError):
            tag_records([line, line], 7)


if __name__ == "__main__":
    unittest.main()
