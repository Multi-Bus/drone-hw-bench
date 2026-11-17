"""Capture the libdeca STM32 runner as CSV, retaining failed attempts.

Serial: 115200, 7 data bits, odd parity, one stop bit (ST UART example).
Use --input to convert/review an existing raw log without opening hardware.
"""
import argparse
import csv
import datetime as dt
import json
import math
import statistics
import time
from pathlib import Path

FIELDS = ['timestamp', 'tick_ms', 'attempt', 'sequence', 'distance_m',
          'rssi', 'first_path_power', 'success', 'status', 'queue_dropped',
          'condition', 'nominal_distance_m', 'reference_quality', 'actual_distance_m', 'antenna_delay_initiator',
          'antenna_delay_responder']


def parse_line(line):
    parts = line.strip().split(',')
    if len(parts) != 7 or parts[0] != 'TWR':
        return None
    try:
        tick, attempt, sequence, cm = map(int, parts[1:5])
        dropped = int(parts[6])
    except ValueError:
        return None
    if parts[5] not in ('OK', 'FAIL', 'TIMEOUT'):
        return None
    return dict(tick_ms=tick, attempt=attempt, sequence=sequence,
                distance_m=cm / 100 if parts[5] == 'OK' and cm < 65534 else '',
                success=int(parts[5] == 'OK' and cm < 65534),
                status=parts[5], queue_dropped=dropped)


def summarize(rows, actual):
    good = [float(r['distance_m']) for r in rows if r['success']]
    attempts = [r['attempt'] for r in rows]
    if attempts and min(attempts) <= 0:
        raise ValueError('Use the initiator port for attempt/success-rate statistics')
    if len(attempts) != len(set(attempts)) or any(b <= a for a, b in zip(attempts, attempts[1:])):
        raise ValueError('Duplicate/restarted/out-of-order attempt IDs; start a new capture')
    span = max(attempts) - min(attempts) + 1 if attempts else 0
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


def main():
    p = argparse.ArgumentParser(description=__doc__)
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument('--port')
    source.add_argument('--input', type=Path)
    p.add_argument('--samples', type=int, default=300)
    p.add_argument('--timeout', type=float, default=120)
    p.add_argument('--distance', type=float, required=True)
    p.add_argument('--reference-quality', choices=('approximate', 'measured'), default='approximate',
                   help='Only measured references produce accuracy/error statistics')
    p.add_argument('--condition', required=True)
    p.add_argument('--ant-initiator', type=int, default=16368)
    p.add_argument('--ant-responder', type=int, default=16368)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
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
            while len(rows) < a.samples and time.monotonic() < deadline:
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
                row.update(timestamp=dt.datetime.now(dt.timezone.utc).isoformat() if a.port else '',
                           rssi='', first_path_power='', condition=a.condition,
                           nominal_distance_m=a.distance, reference_quality=a.reference_quality,
                           actual_distance_m=a.distance if a.reference_quality == 'measured' else '',
                           antenna_delay_initiator=a.ant_initiator,
                           antenna_delay_responder=a.ant_responder)
                writer.writerow(row)
                csvfile.flush()
                rows.append(row)
    finally:
        stream.close()
    summary = summarize(rows, a.distance if a.reference_quality == 'measured' else None)
    summary.update(nominal_distance_m=a.distance, reference_quality=a.reference_quality,
                   accuracy_validated=False)
    summary['complete'] = len(rows) == a.samples and summary['missing_rows'] == 0
    a.out.with_suffix('.summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))
    if not summary['complete'] or summary['max_queue_dropped']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
