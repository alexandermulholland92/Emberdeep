#!/usr/bin/env python3
"""Diff the C tessellator against Three.js's own geometry output.

Reads the reference produced by `node tests/model_ref.js <html> --geom`,
feeds the same specs through the compiled tests/geom_dump harness, and
compares vertex positions, UVs and triangle indices.

    tests/geom_check.py <geoms.json> <path-to-geom_dump> [tolerance]
"""
import json
import subprocess
import sys

TYPE_TO_KIND = {
    'Box': 'box', 'Cylinder': 'cylinder', 'Cone': 'cone', 'Sphere': 'sphere',
    'Torus': 'torus', 'Plane': 'plane', 'Icosahedron': 'icosa',
}


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    geoms = json.load(open(sys.argv[1]))
    dump_bin = sys.argv[2]
    tol = float(sys.argv[3]) if len(sys.argv) > 3 else 2e-5

    specs = []
    for g in geoms:
        params = ['%.17g' % (1.0 if p is True else 0.0 if p is False else (p or 0))
                  for p in g['params']]
        specs.append('%s %s' % (TYPE_TO_KIND[g['type']], ' '.join(params)))

    out = subprocess.run([dump_bin], input='\n'.join(specs) + '\n',
                         capture_output=True, text=True)
    if out.returncode != 0:
        print(out.stderr.strip())
        return 1

    lines = out.stdout.splitlines()
    at = 0
    bad = []
    worst = 0.0
    for g in geoms:
        assert lines[at].startswith('#'), lines[at]
        nv, ni = (int(x) for x in lines[at].split()[1:])
        at += 1
        verts = [tuple(float(x) for x in lines[at + i].split()[1:]) for i in range(nv)]
        at += nv
        tris = [tuple(int(x) for x in lines[at + i].split()[1:]) for i in range(ni // 3)]
        at += ni // 3

        why = []
        if nv != g['vertexCount']:
            why.append('vertex count %d vs %d' % (nv, g['vertexCount']))
        if ni != g['indexCount']:
            why.append('index count %d vs %d' % (ni, g['indexCount']))
        if not why:
            rp, ru, ri = g['pos'], g['uv'], g['index']
            for i, (x, y, z, u, v) in enumerate(verts):
                for got, want, what in ((x, rp[i * 3], 'x'), (y, rp[i * 3 + 1], 'y'),
                                        (z, rp[i * 3 + 2], 'z')):
                    d = abs(got - want)
                    worst = max(worst, d)
                    if d > tol:
                        why.append('vertex %d %s: %.6f vs %.6f' % (i, what, got, want))
                if ru:
                    for got, want, what in ((u, ru[i * 2], 'u'), (v, ru[i * 2 + 1], 'v')):
                        d = abs(got - want)
                        worst = max(worst, d)
                        if d > tol:
                            why.append('vertex %d %s: %.6f vs %.6f' % (i, what, got, want))
                if why:
                    break
            if not why and ri:
                flat = [i for t in tris for i in t]
                if flat != list(ri):
                    n = sum(1 for a, b in zip(flat, ri) if a != b)
                    why.append('%d of %d indices differ' % (n, len(ri)))
        if why:
            bad.append((g['key'], why[:2]))

    print('%d geometries checked, %d vertices' %
          (len(geoms), sum(g['vertexCount'] for g in geoms)))
    if bad:
        print('%d MISMATCHED:' % len(bad))
        for key, why in bad[:12]:
            print('  %-46s %s' % (key, '; '.join(why)))
        return 1
    print('all geometries match Three.js r128 (worst coordinate delta %.2e)' % worst)
    return 0


if __name__ == '__main__':
    sys.exit(main())
