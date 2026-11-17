import unittest
from log_twr import parse_line, summarize


class LogTests(unittest.TestCase):
    def test_real_output_and_failed_attempt(self):
        good = parse_line('TWR,33946,339,339,12,OK,0\r\n')
        bad = parse_line('TWR,34060,340,340,65535,FAIL,0\r\n')
        self.assertEqual(good['distance_m'], .12)
        self.assertEqual(bad['distance_m'], '')
        self.assertEqual(summarize([good, bad], .5)['success_rate'], .5)

    def test_missing_attempts_count_in_denominator(self):
        rows = [parse_line('TWR,0,1,1,48,OK,0'), parse_line('TWR,200,3,3,52,OK,0')]
        summary = summarize(rows, .5)
        self.assertEqual(summary['missing_rows'], 1)
        self.assertAlmostEqual(summary['success_rate'], 2 / 3)
        self.assertEqual(summary['mean_error_m'], 0)
        self.assertAlmostEqual(summary['p95_absolute_error_m'], .02)

    def test_rejects_boot_fragment_and_restarted_counter(self):
        self.assertIsNone(parse_line('339,12,OK,0'))
        self.assertIsNone(parse_line('# HEALTH irq=0'))
        row = parse_line('TWR,0,1,1,48,OK,0')
        with self.assertRaises(ValueError):
            summarize([row, row], .5)

    def test_no_success_has_no_distance_statistics(self):
        summary = summarize([parse_line('TWR,0,1,1,65535,TIMEOUT,0')], .5)
        self.assertEqual(summary['success_rate'], 0)
        self.assertIsNone(summary['mean_m'])

    def test_approximate_reference_does_not_claim_accuracy(self):
        summary = summarize([parse_line('TWR,0,1,1,51,OK,0')], None)
        self.assertEqual(summary['mean_m'], .51)
        self.assertIsNone(summary['mean_error_m'])
        self.assertIsNone(summary['p95_absolute_error_m'])


if __name__ == '__main__':
    unittest.main()
