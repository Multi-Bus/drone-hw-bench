"""DWM3001CDK CLI 레인징 출력을 CSV로 저장한다. 기록만 하고 보드에는 아무것도 보내지 않는다.

사용 예:
    python log_ranging.py COM6 t1_dist_1m_los.csv -n 500

INITF는 다른 터미널에서 직접 실행하거나, 이 스크립트를 먼저 실행한 뒤 INITF를 친다.
단, Tera Term 같은 프로그램이 같은 COM 포트를 열고 있으면 이 스크립트가 포트를 열 수 없다.

-n은 저장할 레인징 시도 횟수다. 성공이든 실패(RX_TIMEOUT 등, 거리 없음)든 한 번이면 한 줄이다.
같은 이름의 출력 파일이 이미 있으면 덮어쓰지 않고 멈춘다.
"""
import argparse
import csv
import os
import re
import time

import serial

SEQ = re.compile(r'sequence_number=(\d+)')
MAC = re.compile(r'mac_address=(0x[0-9a-fA-F]+)')
STATUS = re.compile(r'status="(\w+)"')
DIST = re.compile(r'distance\[cm\]=(-?\d+)')  # NLOS나 가림에서는 음수 거리도 나온다


def parse(buffer):
    """buffer에서 완성된 측정을 모두 꺼내고, 남은 문자열을 함께 돌려준다.

    반환하는 행은 (sequence_number, mac_address, status, distance_cm)이다.
    실패한 시도처럼 거리가 없으면 distance_cm은 None, mac_address나 status가 없으면 빈 문자열이다.
    """
    parts = buffer.split('SESSION_INFO_NTF')
    rows = []
    for part in parts[:-1]:
        seq = SEQ.search(part)
        if not seq:
            continue
        mac = MAC.search(part)
        status = STATUS.search(part)
        dist = DIST.search(part)
        rows.append((
            int(seq[1]),
            mac[1] if mac else '',
            status[1] if status else '',
            int(dist[1]) if dist else None,
        ))
    return rows, parts[-1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('port')
    ap.add_argument('out')
    ap.add_argument('-n', type=int, default=300, help='저장할 레인징 시도 횟수(성공과 실패 모두)')
    args = ap.parse_args()

    if os.path.exists(args.out):
        raise SystemExit(f'{args.out}이(가) 이미 있다. 다른 이름을 쓴다.')

    # 포트를 먼저 연다. 포트 오류로 멈췄을 때 빈 CSV가 남지 않게 한다.
    ser = serial.Serial(args.port, 115200, timeout=0.2)
    f = open(args.out, 'x', newline='', encoding='utf-8')
    buf = ''
    count = 0
    ok = 0
    with f:
        w = csv.writer(f)
        w.writerow(['timestamp', 'sequence_number', 'mac_address', 'status', 'distance_cm', 'success'])
        try:
            while count < args.n:
                buf += ser.read(4096).decode('utf-8', 'replace')
                rows, buf = parse(buf)
                for seq, mac, status, dist in rows:
                    success = int(status == 'SUCCESS' and dist is not None)
                    w.writerow([f'{time.time():.3f}', seq, mac, status, '' if dist is None else dist, success])
                    count += 1
                    ok += success
                    if count >= args.n:
                        break
        finally:
            ser.close()
    print(f'{count}개 저장(성공 {ok}): {args.out}')


if __name__ == '__main__':
    main()
