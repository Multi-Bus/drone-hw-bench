"""Capture the libdeca STM32 runner as CSV, retaining failed attempts.

Serial: 115200, 7 data bits, odd parity, one stop bit (ST UART example).
Use --input to convert/review an existing raw log without opening hardware.

Two line formats are read:
  TWR,tick_ms,attempt,sequence,distance_cm,status,queue_dropped          (7 columns, one peer)
  TWR,tick_ms,attempt,sequence,peer,distance_cm,status,queue_dropped     (8 columns, multi-peer)
With the 8-column format the initiator alternates over its peers (attempt n asks
peers[(n-1) % count]), so statistics, missing rows and --samples are counted per peer.
"""
import argparse
import csv
import datetime as dt
import json
import math
import statistics
import time
from collections import Counter
from pathlib import Path

FIELDS = ['timestamp', 'tick_ms', 'attempt', 'sequence', 'peer', 'distance_m',
          'rssi', 'first_path_power', 'success', 'status', 'queue_dropped',
          'condition', 'nominal_distance_m', 'reference_quality', 'actual_distance_m', 'antenna_delay_initiator',
          'antenna_delay_responder']


def parse_line(line):
    parts = line.strip().split(',')
    if parts[0] != 'TWR' or len(parts) not in (7, 8):
        return None
    status = parts[-2]
    if status not in ('OK', 'FAIL', 'TIMEOUT'):
        return None
    try:
        nums = [int(v) for v in parts[1:-2]]
        dropped = int(parts[-1])
    except ValueError:
        return None
    if len(parts) == 7:
        tick, attempt, sequence, cm = nums
        peer = ''
    else:
        tick, attempt, sequence, peer, cm = nums
    ok = status == 'OK' and cm < 65534
    return dict(tick_ms=tick, attempt=attempt, sequence=sequence, peer=peer,
                distance_m=cm / 100 if ok else '', success=int(ok),
                status=status, queue_dropped=dropped)


def _attempt_ids(rows):
    """Attempt IDs of the whole capture must be strictly increasing and positive."""
    attempts = [r['attempt'] for r in rows]
    if attempts and min(attempts) <= 0:
        raise ValueError('Use the initiator port for attempt/success-rate statistics')
    if len(attempts) != len(set(attempts)) or any(b <= a for a, b in zip(attempts, attempts[1:])):
        raise ValueError('Duplicate/restarted/out-of-order attempt IDs; start a new capture')
    return attempts


def _stats(rows, span, actual):
    good = [float(r['distance_m']) for r in rows if r['success']]
    errors = sorted(abs(v - actual) for v in good) if actual is not None else []
    return {'observed_rows': len(rows), 'attempt_span': span,
            'missing_rows': span - len(rows), 'successes': len(good),
            'success_rate': len(good) / span if span else None,
            'mean_m': statistics.mean(good) if good else None,
            'mean_error_m': statistics.mean(good) - actual if good and actual is not None else None,
            'sample_std_m': statistics.stdev(good) if len(good) > 1 else None,
            'max_m': max(good) if good else None,
            'p95_absolute_error_m': errors[math.ceil(.95 * len(errors)) - 1] if errors else None,
            'max_queue_dropped': max((r['queue_dropped'] for r in rows), default=0),
            'rssi_available': False}


def summarize(rows, actual):
    """Single-peer statistics over the whole capture (7-column format)."""
    attempts = _attempt_ids(rows)
    span = max(attempts) - min(attempts) + 1 if attempts else 0
    return _stats(rows, span, actual)


def peer_cycle(rows, peers=None):
    """Peer order of the initiator: cycle[(attempt - 1) % len(cycle)] is asked in that attempt.

    Taken from `peers` when given, otherwise inferred from the rows. The rows must follow
    a fixed alternation, which is what lets a missing attempt ID be charged to its peer.
    """
    if peers:
        cycle = list(peers)
    else:
        n = len({r['peer'] for r in rows})
        cycle = [None] * n
        for r in rows:
            i = (r['attempt'] - 1) % n
            if cycle[i] is None:
                cycle[i] = r['peer']
        if None in cycle:
            raise ValueError('Cannot infer the peer order; pass --peers')
    for r in rows:
        if cycle[(r['attempt'] - 1) % len(cycle)] != r['peer']:
            raise ValueError('Peers do not alternate in a fixed order (or --peers is wrong); '
                             'start a new capture')
    return cycle


def summarize_by_peer(rows, actual, peers=None):
    """Statistics per peer. A skipped attempt ID counts as a missing row of the peer
    whose turn it was; the attempt IDs themselves must still be continuous overall."""
    attempts = _attempt_ids(rows)
    cycle = peer_cycle(rows, peers)
    first, last = (min(attempts), max(attempts)) if attempts else (1, 0)
    out = {}
    for p in cycle:
        span = sum(1 for a in range(first, last + 1) if cycle[(a - 1) % len(cycle)] == p)
        out[p] = _stats([r for r in rows if r['peer'] == p], span, actual)
    return out


def captured_enough(rows, samples, peers=None):
    """--samples counts rows per peer (every peer must reach it)."""
    counts = Counter(r['peer'] for r in rows)
    if not counts:
        return False
    wanted = list(peers) if peers else list(counts)
    return all(counts[p] >= samples for p in wanted)


def parse_peers(text):
    return [int(v) for v in text.split(',')] if text else None


def parse_delays(text):
    """'16400' for all responders, or '1=16400,3=16368' per responder address."""
    if '=' not in text:
        return None, int(text)
    return {int(k): int(v) for k, v in (item.split('=', 1) for item in text.split(','))}, None


def main():
    p = argparse.ArgumentParser(description=__doc__)
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument('--port')
    source.add_argument('--input', type=Path)
    p.add_argument('--samples', type=int, default=300,
                   help='rows to collect; with a peer column, per peer')
    p.add_argument('--timeout', type=float, default=120)
    p.add_argument('--distance', type=float, required=True)
    p.add_argument('--reference-quality', choices=('approximate', 'measured'), default='approximate',
                   help='Only measured references produce accuracy/error statistics')
    p.add_argument('--condition', required=True)
    p.add_argument('--peers', type=parse_peers,
                   help='peer addresses in the order the initiator asks them, e.g. 1,3 '
                        '(default: inferred from the capture)')
    p.add_argument('--ant-initiator', type=int, default=16368)
    p.add_argument('--ant-responder', default='16368',
                   help="one value for all responders, or per peer: 1=16400,3=16368")
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    delay_by_peer, delay_default = parse_delays(a.ant_responder)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    stream = None
    if a.port:
        import serial
        stream = serial.Serial(a.port, 115200, bytesize=serial.SEVENBITS,
                               parity=serial.PARITY_ODD, stopbits=serial.STOPBITS_ONE,
                               timeout=.2)
        stream.reset_input_buffer()
        # The first readline may be a fragment from the currently transmitting row.
        stream.readline()
    else:
        stream = a.input.open('rb')
    deadline = time.monotonic() + a.timeout
    try:
        with a.out.open('w', newline='', encoding='utf-8') as csvfile, \
                a.out.with_suffix('.raw.txt').open('w', encoding='utf-8') as raw:
            writer = csv.DictWriter(csvfile, fieldnames=FIELDS)
            writer.writeheader()
            while not captured_enough(rows, a.samples, a.peers) and time.monotonic() < deadline:
                data = stream.readline()
                if not data:
                    if a.input:
                        break
                    continue
                line = data.decode('ascii', errors='replace')
                raw.write(line)
                raw.flush()
                row = parse_line(line)
                if row is None:
                    continue
                if delay_by_peer is None:
                    responder_delay = delay_default
                elif row['peer'] in delay_by_peer:
                    responder_delay = delay_by_peer[row['peer']]
                else:
                    raise SystemExit(f"--ant-responder has no value for peer {row['peer']!r}")
                row.update(timestamp=dt.datetime.now(dt.timezone.utc).isoformat() if a.port else '',
                           rssi='', first_path_power='', condition=a.condition,
                           nominal_distance_m=a.distance, reference_quality=a.reference_quality,
                           actual_distance_m=a.distance if a.reference_quality == 'measured' else '',
                           antenna_delay_initiator=a.ant_initiator,
                           antenna_delay_responder=responder_delay)
                writer.writerow(row)
                csvfile.flush()
                rows.append(row)
    finally:
        stream.close()
    actual = a.distance if a.reference_quality == 'measured' else None
    multi = any(r['peer'] != '' for r in rows)
    if multi:
        per_peer = summarize_by_peer(rows, actual, a.peers)
        summary = {'observed_rows': len(rows),
                   'max_queue_dropped': max((r['queue_dropped'] for r in rows), default=0),
                   'peers': {str(k): v for k, v in per_peer.items()}}
        summary['complete'] = all(s['observed_rows'] >= a.samples and s['missing_rows'] == 0
                                  for s in per_peer.values())
    else:
        summary = summarize(rows, actual)
        summary['complete'] = len(rows) == a.samples and summary['missing_rows'] == 0
    summary.update(nominal_distance_m=a.distance, reference_quality=a.reference_quality,
                   accuracy_validated=False)
    a.out.with_suffix('.summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))
    if not summary['complete'] or summary['max_queue_dropped']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
