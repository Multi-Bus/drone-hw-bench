"""DWM3001CDK CLI 레인징 출력을 CSV로 저장한다. 기록만 하고 보드에는 아무것도 보내지 않는다.

사용 예:
    python log_ranging.py COM6 t1_dist_1m_los.csv -n 300

INITF는 다른 터미널에서 직접 실행하거나, 이 스크립트를 먼저 실행한 뒤 INITF를 친다.
단, Tera Term 같은 프로그램이 같은 COM 포트를 열고 있으면 이 스크립트가 포트를 열 수 없다.
"""
import argparse
import csv
import re
import time

import serial

NTF = re.compile(
    r'sequence_number=(?P<seq>\d+).*?'
    r'mac_address=(?P<mac>0x[0-9a-fA-F]+),\s*'
    r'status="(?P<status>\w+)",\s*'
    r'distance\[cm\]=(?P<dist>\d+)',
    re.S,
)


def parse(buffer):
    """buffer에서 완성된 측정을 모두 꺼내고, 남은 문자열을 함께 돌려준다."""
    parts = buffer.split('SESSION_INFO_NTF')
    rows = []
    for part in parts[:-1]:
        m = NTF.search(part)
        if m:
            rows.append((int(m['seq']), m['mac'], m['status'], int(m['dist'])))
    return rows, parts[-1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('port')
    ap.add_argument('out')
    ap.add_argument('-n', type=int, default=300, help='저장할 측정 개수')
    args = ap.parse_args()

    ser = serial.Serial(args.port, 115200, timeout=0.2)
    buf = ''
    count = 0
    with open(args.out, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['timestamp', 'sequence_number', 'mac_address', 'status', 'distance_cm', 'success'])
        try:
            while count < args.n:
                buf += ser.read(4096).decode('utf-8', 'replace')
                rows, buf = parse(buf)
                for seq, mac, status, dist in rows:
                    w.writerow([f'{time.time():.3f}', seq, mac, status, dist, int(status == 'SUCCESS')])
                    count += 1
                    if count >= args.n:
                        break
        finally:
            ser.close()
    print(f'{count}개 저장: {args.out}')


if __name__ == '__main__':
    main()
