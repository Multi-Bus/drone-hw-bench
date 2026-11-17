import unittest
from log_twr import (captured_enough, parse_delays, parse_line, peer_cycle,
                     summarize, summarize_by_peer)


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


def rows8(*lines):
    return [parse_line(line) for line in lines]


class MultiPeerTests(unittest.TestCase):
    def test_eight_columns_carry_the_peer(self):
        good = parse_line('TWR,246,2,2,3,83,OK,0\r\n')
        self.assertEqual((good['peer'], good['distance_m'], good['attempt']), (3, .83, 2))
        timeout = parse_line('TWR,1146,3,3,1,65535,TIMEOUT,0')
        self.assertEqual((timeout['peer'], timeout['success'], timeout['distance_m']), (1, 0, ''))

    def test_seven_column_lines_still_parse_without_a_peer(self):
        self.assertEqual(parse_line('TWR,146,1,1,80,OK,0')['peer'], '')

    def test_rejects_wrong_column_count_or_status(self):
        self.assertIsNone(parse_line('TWR,1,2,3,4,5,OK,0,9'))
        self.assertIsNone(parse_line('TWR,1,2,3,4,5,BAD,0'))
        self.assertIsNone(parse_line('TWR,1,2,3,x,5,OK,0'))

    def test_statistics_are_split_by_peer(self):
        rows = rows8('TWR,0,1,1,1,100,OK,0', 'TWR,100,2,2,3,110,OK,0',
                     'TWR,200,3,3,1,102,OK,0', 'TWR,300,4,4,3,65535,FAIL,0',
                     'TWR,400,5,5,1,98,OK,0', 'TWR,500,6,6,3,112,OK,0')
        by_peer = summarize_by_peer(rows, None)
        self.assertEqual(list(by_peer), [1, 3])
        self.assertEqual((by_peer[1]['observed_rows'], by_peer[1]['successes']), (3, 3))
        self.assertAlmostEqual(by_peer[1]['mean_m'], 1.0)
        self.assertEqual((by_peer[3]['observed_rows'], by_peer[3]['successes']), (3, 2))
        self.assertAlmostEqual(by_peer[3]['success_rate'], 2 / 3)
        self.assertAlmostEqual(by_peer[3]['mean_m'], 1.11)
        self.assertTrue(all(s['missing_rows'] == 0 for s in by_peer.values()))

    def test_a_skipped_attempt_is_charged_to_the_peer_whose_turn_it_was(self):
        # attempt 4 (peer 3's turn) never arrived; peer 1 lost nothing
        rows = rows8('TWR,0,1,1,1,100,OK,0', 'TWR,100,2,2,3,110,OK,0',
                     'TWR,200,3,3,1,100,OK,0', 'TWR,400,5,5,1,100,OK,0',
                     'TWR,500,6,6,3,110,OK,0')
        by_peer = summarize_by_peer(rows, None)
        self.assertEqual((by_peer[1]['missing_rows'], by_peer[3]['missing_rows']), (0, 1))
        self.assertAlmostEqual(by_peer[3]['success_rate'], 2 / 3)

    def test_peer_that_never_answers_still_has_timeout_rows(self):
        rows = rows8('TWR,0,1,1,1,100,OK,0', 'TWR,1000,2,2,3,65535,TIMEOUT,0',
                     'TWR,1100,3,3,1,100,OK,0', 'TWR,2100,4,4,3,65535,TIMEOUT,0')
        by_peer = summarize_by_peer(rows, None)
        self.assertEqual(by_peer[3]['success_rate'], 0)
        self.assertEqual(by_peer[1]['success_rate'], 1)

    def test_explicit_peer_order_keeps_a_silent_peer_in_the_report(self):
        rows = rows8('TWR,0,1,1,1,100,OK,0', 'TWR,200,3,3,1,100,OK,0')
        by_peer = summarize_by_peer(rows, None, peers=[1, 3])
        self.assertEqual(by_peer[3]['observed_rows'], 0)
        self.assertEqual(by_peer[3]['missing_rows'], 1)  # attempt 2 was peer 3's turn

    def test_attempt_ids_must_stay_continuous_overall(self):
        row = parse_line('TWR,0,1,1,1,100,OK,0')
        with self.assertRaises(ValueError):
            summarize_by_peer([row, row], None)

    def test_peers_that_do_not_alternate_are_rejected(self):
        rows = rows8('TWR,0,1,1,1,100,OK,0', 'TWR,100,2,2,1,100,OK,0',
                     'TWR,200,3,3,3,100,OK,0')
        with self.assertRaises(ValueError):
            peer_cycle(rows)

    def test_samples_are_counted_per_peer(self):
        rows = rows8('TWR,0,1,1,1,100,OK,0', 'TWR,100,2,2,3,100,OK,0',
                     'TWR,200,3,3,1,100,OK,0')
        self.assertFalse(captured_enough(rows, 2))   # peer 3 has only 1
        rows += rows8('TWR,300,4,4,3,100,OK,0')
        self.assertTrue(captured_enough(rows, 2))
        self.assertFalse(captured_enough(rows[:1], 2, peers=[1, 3]))

    def test_legacy_capture_still_counts_total_rows(self):
        rows = rows8('TWR,0,1,1,48,OK,0', 'TWR,100,2,2,50,OK,0')
        self.assertTrue(captured_enough(rows, 2))
        self.assertFalse(captured_enough(rows, 3))

    def test_responder_delay_spec(self):
        self.assertEqual(parse_delays('16400'), (None, 16400))
        self.assertEqual(parse_delays('1=16400,3=16368'), ({1: 16400, 3: 16368}, None))


if __name__ == '__main__':
    unittest.main()
