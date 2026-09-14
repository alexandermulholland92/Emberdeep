#!/usr/bin/env python3
"""Diff the ported animateActor against the browser's own, pose for pose.

    tests/anim_check.py <anim.json> <path-to-anim_dump> [tolerance]
"""
import json
import subprocess
import sys


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ref = json.load(open(sys.argv[1]))
    tol = float(sys.argv[3]) if len(sys.argv) > 3 else 5e-5

    run = subprocess.run([sys.argv[2]], capture_output=True, text=True)
    if run.returncode != 0:
        print(run.stderr.strip())
        return 1

    got, cur = {}, None
    for line in run.stdout.splitlines():
        f = line.split()
        if f[0] == 'model':
            cur = {'nodeCount': int(f[2]), 'walkT': float(f[3]),
                   'floatT': float(f[4]), 'rootY': float(f[5]), 'nodes': []}
            got[f[1]] = cur
        elif f[0] == 'n':
            cur['nodes'].append([float(x) for x in f[2:]])

    bad, worst, clocks = [], 0.0, 0
    for m in ref:
        g = got.get(m['name'])
        if not g:
            bad.append((m['name'], 'missing from the C build'))
            continue
        for key in ('walkT', 'floatT', 'rootY'):
            d = abs(g[key] - m[key])
            if d > 1e-4:
                bad.append((m['name'], '%s %.6f vs %.6f' % (key, g[key], m[key])))
                break
        else:
            clocks += 1
        if bad and bad[-1][0] == m['name']:
            continue
        if g['nodeCount'] != len(m['nodes']):
            bad.append((m['name'], 'node count %d vs %d'
                        % (g['nodeCount'], len(m['nodes']))))
            continue
        for i, world in enumerate(m['nodes']):
            hit = False
            for k in range(16):
                d = abs(g['nodes'][i][k] - world[k])
                worst = max(worst, d)
                if d > tol:
                    bad.append((m['name'], 'node %d element %d: %.6f vs %.6f'
                                % (i, k, g['nodes'][i][k], world[k])))
                    hit = True
                    break
            if hit:
                break

    print('%d models animated 90 frames, %d with matching clocks' % (len(ref), clocks))
    if bad:
        print('%d MISMATCHED:' % len(bad))
        for name, why in bad[:10]:
            print('  %-12s %s' % (name, why))
        return 1
    print('every animated pose matches the browser (worst element delta %.2e)' % worst)
    return 0


if __name__ == '__main__':
    sys.exit(main())
