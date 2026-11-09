"""Task 1 Lv2 분석: data/의 조건별 CSV로 결과 표와 그래프 2개를 만든다.

실행 (task1_uwb/ 폴더에서):
    python analyze.py

입력: data/ 안에서 아래 이름 규칙에 맞는 CSV만 읽는다. (Lv1의 t1_dist_1m_los_run1.csv 같은 파일은 읽지 않는다.)
    t1_dist_{N}m_los.csv, t1_alu_back_{N}cm.csv, t1_alu_side_{N}cm.csv,
    t1_angle_{N}deg.csv, t1_place_{lab,hallway,outdoor}.csv, t1_nlos_{body,wall}.csv
출력: results/results.md, results/results.csv, results/fig_distance_error.png, results/fig_alu.png

정의 (LV2.MD 5절):
    성공 = status가 SUCCESS이고 거리값이 있는 행(CSV의 success == 1). 성공률 = 성공 행 / 전체 행(시도).
    평균 오차 = 성공 행 거리의 평균 - 실제 거리. 표준편차는 모표준편차. 95% 절대오차 = |오차|의 95번째 백분위수.
    오차 통계는 성공 행으로 계산하며 음수나 엉뚱한 거리도 포함한다.
    누락 = sequence_number가 끊긴 구간의 빠진 개수. 실패로 세지 않고 따로 표시한다.
"""
import argparse
import csv
import re
import statistics
from pathlib import Path

HERE = Path(__file__).resolve().parent
PLACE = {'lab': '실험실', 'hallway': '복도', 'outdoor': '야외'}
NLOS = {'body': '사람 몸', 'wall': '벽 1개'}
# 방향: 파일 이름의 각도는 시계 방향. 오른쪽 45°, 90°는 45, 90이고 왼쪽 45°, 90°는 315, 270이다.
ANGLE = {'45': '오른쪽 45°', '90': '오른쪽 90°', '270': '왼쪽 90°', '315': '왼쪽 45°'}
ROUND_S = 0.2  # 레인징 한 번의 간격(INITF 기본 블록 200 ms)

# (그룹, 파일 이름 규칙, 이름 -> 조건 표시, 이름 -> 실제 거리 cm). 순서는 PDF의 조건 순서다.
PATTERNS = [
    ('dist', r't1_dist_(\d+)m_los\.csv', lambda v: f'거리 {v} m', lambda v: int(v) * 100),
    ('alu_back', r't1_alu_back_(\d+)cm\.csv', lambda v: f'알루미늄(뒤) {v} cm', lambda v: 300),
    ('alu_side', r't1_alu_side_(\d+)cm\.csv', lambda v: f'알루미늄(옆) {v} cm', lambda v: 300),
    ('angle', r't1_angle_(\d+)deg\.csv', lambda v: f'방향 {ANGLE.get(v, v + "°")}', lambda v: 300),
    ('place', r't1_place_(lab|hallway|outdoor)\.csv', lambda v: f'장소 {PLACE[v]}', lambda v: 500),
    ('nlos', r't1_nlos_(body|wall)\.csv', lambda v: f'NLOS {NLOS[v]}', lambda v: 300),
]


def sort_value(group, v):
    if group == 'angle':  # 왼쪽 90°, 왼쪽 45°, 오른쪽 45°, 오른쪽 90° 순서
        return sorted(ANGLE, key=lambda a: (int(a) + 180) % 360).index(v) if v in ANGLE else int(v)
    if v.isdigit():
        return int(v)
    return list({'place': PLACE, 'nlos': NLOS}[group]).index(v)


def find_conditions(data_dir):
    conds = []
    for path in sorted(data_dir.glob('t1_*.csv')):
        for gi, (group, pat, label, true_cm) in enumerate(PATTERNS):
            m = re.fullmatch(pat, path.name)
            if m:
                v = m[1]
                conds.append(dict(path=path, group=group, value=v, label=label(v), true_cm=true_cm(v),
                                  key=(gi, sort_value(group, v))))
                break
    return sorted(conds, key=lambda c: c['key'])


def read_rows(path):
    rows = []
    with open(path, newline='', encoding='utf-8') as f:
        for r in csv.DictReader(f):
            rows.append(dict(ts=float(r['timestamp']), seq=int(r['sequence_number']),
                             dist=int(r['distance_cm']) if r['distance_cm'] != '' else None,
                             ok=int(r['success']) == 1))
    return rows


def percentile(xs, q):
    xs = sorted(xs)
    k = (len(xs) - 1) * q / 100
    lo = int(k)
    hi = min(lo + 1, len(xs) - 1)
    return xs[lo] + (xs[hi] - xs[lo]) * (k - lo)


def analyze(rows, true_cm):
    n = len(rows)
    d = [r['dist'] for r in rows if r['ok']]
    err = [x - true_cm for x in d]
    seq = [r['seq'] for r in rows]
    t = [r['ts'] for r in rows]
    out = dict(n=n, ok=len(d), rate=100 * len(d) / n if n else 0.0,
               missing=sum(b - a - 1 for a, b in zip(seq, seq[1:]) if b > a + 1))
    if d:
        out.update(mean=statistics.fmean(d), err=statistics.fmean(err), std=statistics.pstdev(d),
                   abs95=percentile([abs(e) for e in err], 95), max=max(d))
    # 저장이 보드의 송신 속도보다 빠르면, 포트를 열기 전에 쌓여 있던 출력이 한꺼번에 들어온 것이다.
    # 조건을 배치한 뒤 INITF를 새로 시작했다면 같은 배치의 데이터라 문제없다. 시작하지 않았다면 이전 배치의 값이 섞여 있다.
    note = ''
    queued = round((ROUND_S * (n - 1) - (t[-1] - t[0])) / ROUND_S) if n > 1 else 0
    if queued >= 10:
        note = f'앞 약 {queued}행은 포트를 열기 전에 쌓인 출력'
    out['note'] = note
    return out


def fmt(x, nd=1):
    return '-' if x is None else f'{x:.{nd}f}'


COLUMNS = ['조건', '실제 거리(cm)', '시도', '성공', '성공률(%)', '평균(cm)', '평균 오차(cm)',
           '표준편차(cm)', '95% 절대오차(cm)', '최대(cm)', '누락', '비고']


def table_row(c, s):
    return [c['label'], str(c['true_cm']), str(s['n']), str(s['ok']), fmt(s['rate']),
            fmt(s.get('mean')), fmt(s.get('err')), fmt(s.get('std')), fmt(s.get('abs95')),
            fmt(s.get('max'), 0), str(s['missing']), s['note']]


def write_tables(rows, out_dir):
    head = '| ' + ' | '.join(COLUMNS) + ' |\n| ' + ' | '.join('---' for _ in COLUMNS) + ' |\n'
    md = head + ''.join('| ' + ' | '.join(r) + ' |\n' for r in rows)
    (out_dir / 'results.md').write_text(md, encoding='utf-8')
    with open(out_dir / 'results.csv', 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(COLUMNS)
        w.writerows(rows)
    return md


def plot(results, out_dir):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib import font_manager
    installed = {f.name for f in font_manager.fontManager.ttflist}
    korean = next((n for n in ('Malgun Gothic', 'AppleGothic', 'NanumGothic') if n in installed), None)
    if korean:
        plt.rcParams['font.family'] = korean
    else:
        print('한글 글꼴을 찾지 못해 그래프의 한글이 깨질 수 있다.')
    plt.rcParams['axes.unicode_minus'] = False

    def series(group):
        return [(int(c['value']), s) for c, s in results if c['group'] == group and 'err' in s]

    dist = series('dist')
    if dist:
        fig, ax = plt.subplots(figsize=(6, 4))
        ax.errorbar([v for v, _ in dist], [s['err'] for _, s in dist], yerr=[s['std'] for _, s in dist],
                    marker='o', capsize=4)
        ax.axhline(0, color='gray', lw=0.8)
        ax.set_xlabel('실제 거리 (m)')
        ax.set_ylabel('평균 오차 (cm) = 측정 - 실제')
        ax.set_title('거리별 오차 (에러바 = 표준편차)')
        ax.set_xticks([v for v, _ in dist])
        fig.tight_layout()
        fig.savefig(out_dir / 'fig_distance_error.png', dpi=150)
        plt.close(fig)
    else:
        print('거리 조건 데이터가 없어 fig_distance_error.png를 만들지 않았다.')

    back, side = series('alu_back'), series('alu_side')
    if back or side:
        fig, (a1, a2) = plt.subplots(1, 2, figsize=(10, 4))
        for name, data in (('뒤', back), ('옆', side)):
            if not data:
                continue
            xs = [v for v, _ in data]
            a1.errorbar(xs, [s['err'] for _, s in data], yerr=[s['std'] for _, s in data],
                        marker='o', capsize=4, label=f'알루미늄({name})')
            a2.plot(xs, [s['rate'] for _, s in data], marker='o', label=f'알루미늄({name})')
        base = next((s for v, s in dist if v == 3), None)  # 같은 3 m, 판 없음
        if base:
            a1.axhline(base['err'], color='gray', ls='--', lw=1, label='판 없음(거리 3 m)')
            a2.axhline(base['rate'], color='gray', ls='--', lw=1, label='판 없음(거리 3 m)')
        a1.axhline(0, color='gray', lw=0.8)
        a1.set_xlabel('판 이격 거리 (cm)')
        a1.set_ylabel('평균 오차 (cm)')
        a1.set_title('알루미늄 이격별 오차 (3 m, 에러바 = 표준편차)')
        a2.set_xlabel('판 이격 거리 (cm)')
        a2.set_ylabel('성공률 (%)')
        a2.set_ylim(0, 105)
        a2.set_title('알루미늄 이격별 성공률')
        a1.legend()
        a2.legend()
        fig.tight_layout()
        fig.savefig(out_dir / 'fig_alu.png', dpi=150)
        plt.close(fig)
    else:
        print('알루미늄 조건 데이터가 없어 fig_alu.png를 만들지 않았다.')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', type=Path, default=HERE / 'data', help='조건별 CSV가 있는 폴더')
    ap.add_argument('--out', type=Path, default=HERE / 'results', help='결과를 쓸 폴더')
    args = ap.parse_args()

    conds = find_conditions(args.data)
    if not conds:
        raise SystemExit(f'{args.data}에 이름 규칙에 맞는 CSV가 없다.')
    args.out.mkdir(parents=True, exist_ok=True)
    results = [(c, analyze(read_rows(c['path']), c['true_cm'])) for c in conds]
    print(write_tables([table_row(c, s) for c, s in results], args.out))
    plot(results, args.out)
    print(f'결과: {args.out}')


if __name__ == '__main__':
    main()
