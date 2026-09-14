#!/usr/bin/env python3
"""Diff the C model traversal against Three.js's own world matrices.

    tests/model_check.py <models.json> <path-to-model_dump> [tolerance]
"""
import json
import subprocess
import sys


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    models = json.load(open(sys.argv[1]))
    tol = float(sys.argv[3]) if len(sys.argv) > 3 else 2e-5

    run = subprocess.run([sys.argv[2]], capture_output=True, text=True)
    if run.returncode != 0:
        print(run.stderr.strip())
        return 1

    got = {}
    order = []
    cur = None
    for line in run.stdout.splitlines():
        f = line.split()
        if f[0] == 'model':
            cur = {'name': f[1], 'nodeCount': int(f[2]), 'tris': int(f[3]), 'nodes': []}
            got[f[1]] = cur
            order.append(f[1])
        elif f[0] == 'n':
            cur['nodes'].append([float(x) for x in f[2:]])

    bad = []
    worst = 0.0
    total_nodes = total_tris = 0
    for m in models:
        g = got.get(m['name'])
        if not g:
            bad.append((m['name'], 'missing from the C tables'))
            continue
        if g['nodeCount'] != m['nodeCount']:
            bad.append((m['name'], 'node count %d vs %d'
                        % (g['nodeCount'], m['nodeCount'])))
            continue
        total_nodes += g['nodeCount']
        total_tris += g['tris']
        for i, n in enumerate(m['nodes']):
            for k in range(16):
                d = abs(g['nodes'][i][k] - n['world'][k])
                worst = max(worst, d)
                if d > tol:
                    bad.append((m['name'],
                                'node %d element %d: %.6f vs %.6f'
                                % (i, k, g['nodes'][i][k], n['world'][k])))
                    break
            if bad and bad[-1][0] == m['name']:
                break

    print('%d models, %d nodes, %d triangles' % (len(models), total_nodes, total_tris))
    if bad:
        print('%d MISMATCHED:' % len(bad))
        for name, why in bad[:10]:
            print('  %-12s %s' % (name, why))
        return 1
    print('every world matrix matches Three.js r128 (worst element delta %.2e)' % worst)
    return 0


if __name__ == '__main__':
    sys.exit(main())
