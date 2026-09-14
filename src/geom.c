/* ==========================================================
   geom.c : Three.js r128 geometry generators, ported.

   Vertex order, UV layout and index order all follow the originals so
   the Vita tessellates the same triangles the browser does. Where the
   original walks a `grid` of row index arrays, the port computes the
   same indices arithmetically - the rows are contiguous, so
   grid[y][x] == rowStart(y) + x.
   ========================================================== */
#include "geom.h"
#include <math.h>

#define GPI 3.14159265358979323846

typedef struct {
    float *pos, *uv;
    unsigned short *idx;
    int nv, ni, maxV, maxI, overflow;
} Buf;

static void push_v(Buf *b, float x, float y, float z, float u, float v) {
    if (b->nv >= b->maxV) { b->overflow = 1; return; }
    b->pos[b->nv * 3] = x; b->pos[b->nv * 3 + 1] = y; b->pos[b->nv * 3 + 2] = z;
    b->uv[b->nv * 2] = u; b->uv[b->nv * 2 + 1] = v;
    b->nv++;
}
static void push_i(Buf *b, int a, int c, int d) {
    if (b->ni + 3 > b->maxI) { b->overflow = 1; return; }
    b->idx[b->ni++] = (unsigned short)a;
    b->idx[b->ni++] = (unsigned short)c;
    b->idx[b->ni++] = (unsigned short)d;
}
static int iseg(float v) { int n = (int)floorf(v + 0.5f); return n < 1 ? 1 : n; }

/* ---------------- BoxGeometry ----------------
   Three builds six planes in the order px, nx, py, ny, pz, nz. buildPlane
   is parameterised by which axes map to u, v and w. */
static void box_plane(Buf *b, int u, int v, int w,
                      float udir, float vdir,
                      float width, float height, float depth,
                      int gridX, int gridY) {
    float segW = width / gridX, segH = height / gridY;
    float wHalf = width / 2, hHalf = height / 2, dHalf = depth / 2;
    int gridX1 = gridX + 1, gridY1 = gridY + 1;
    int ix, iy;
    int base = b->nv;
    for (iy = 0; iy < gridY1; iy++) {
        float y = iy * segH - hHalf;
        for (ix = 0; ix < gridX1; ix++) {
            float x = ix * segW - wHalf;
            float vec[3];
            vec[u] = x * udir;
            vec[v] = y * vdir;
            vec[w] = dHalf;
            push_v(b, vec[0], vec[1], vec[2],
                   (float)ix / gridX, 1.f - (float)iy / gridY);
        }
    }
    for (iy = 0; iy < gridY; iy++)
        for (ix = 0; ix < gridX; ix++) {
            int a = base + ix + gridX1 * iy;
            int bb = base + ix + gridX1 * (iy + 1);
            int c = base + (ix + 1) + gridX1 * (iy + 1);
            int d = base + (ix + 1) + gridX1 * iy;
            push_i(b, a, bb, d);
            push_i(b, bb, c, d);
        }
}
static void gen_box(Buf *b, const float *p) {
    float w = p[0], h = p[1], d = p[2];
    int ws = iseg(p[3]), hs = iseg(p[4]), ds = iseg(p[5]);
    box_plane(b, 2, 1, 0, -1, -1, d, h,  w, ds, hs);   /* px */
    box_plane(b, 2, 1, 0,  1, -1, d, h, -w, ds, hs);   /* nx */
    box_plane(b, 0, 2, 1,  1,  1, w, d,  h, ws, ds);   /* py */
    box_plane(b, 0, 2, 1,  1, -1, w, d, -h, ws, ds);   /* ny */
    box_plane(b, 0, 1, 2,  1, -1, w, h,  d, ws, hs);   /* pz */
    box_plane(b, 0, 1, 2, -1, -1, w, h, -d, ws, hs);   /* nz */
}

/* ---------------- CylinderGeometry (and ConeGeometry) ---------------- */
static void cyl_cap(Buf *b, int top, float rTop, float rBot, float height,
                    int radSeg, float thetaStart, float thetaLength) {
    float halfH = height / 2;
    float radius = top ? rTop : rBot;
    float sign = top ? 1.f : -1.f;
    int centerStart = b->nv, centerEnd, x;
    for (x = 1; x <= radSeg; x++)
        push_v(b, 0, halfH * sign, 0, 0.5f, 0.5f);
    centerEnd = b->nv;
    for (x = 0; x <= radSeg; x++) {
        float u = (float)x / radSeg;
        float theta = u * thetaLength + thetaStart;
        float cosT = cosf(theta), sinT = sinf(theta);
        push_v(b, radius * sinT, halfH * sign, radius * cosT,
               cosT * 0.5f + 0.5f, sinT * 0.5f * sign + 0.5f);
    }
    for (x = 0; x < radSeg; x++) {
        int c = centerStart + x, i = centerEnd + x;
        if (top) push_i(b, i, i + 1, c);
        else     push_i(b, i + 1, i, c);
    }
}
static void gen_cylinder(Buf *b, float rTop, float rBot, float height,
                         int radSeg, int hSeg, int openEnded,
                         float thetaStart, float thetaLength) {
    float halfH = height / 2;
    int x, y, rowLen = radSeg + 1;
    int base = b->nv;
    for (y = 0; y <= hSeg; y++) {
        float v = (float)y / hSeg;
        float radius = v * (rBot - rTop) + rTop;
        for (x = 0; x <= radSeg; x++) {
            float u = (float)x / radSeg;
            float theta = u * thetaLength + thetaStart;
            push_v(b, radius * sinf(theta), -v * height + halfH,
                   radius * cosf(theta), u, 1.f - v);
        }
    }
    for (x = 0; x < radSeg; x++)
        for (y = 0; y < hSeg; y++) {
            int a = base + rowLen * y + x;
            int bb = base + rowLen * (y + 1) + x;
            int c = base + rowLen * (y + 1) + (x + 1);
            int d = base + rowLen * y + (x + 1);
            push_i(b, a, bb, d);
            push_i(b, bb, c, d);
        }
    if (!openEnded) {
        if (rTop > 0) cyl_cap(b, 1, rTop, rBot, height, radSeg, thetaStart, thetaLength);
        if (rBot > 0) cyl_cap(b, 0, rTop, rBot, height, radSeg, thetaStart, thetaLength);
    }
}

/* ---------------- SphereGeometry ---------------- */
static void gen_sphere(Buf *b, const float *p) {
    float radius = p[0];
    int wSeg = iseg(p[1]) < 3 ? 3 : iseg(p[1]);
    int hSeg = iseg(p[2]) < 2 ? 2 : iseg(p[2]);
    float phiStart = p[3], phiLength = p[4];
    float thetaStart = p[5], thetaLength = p[6];
    float thetaEnd = thetaStart + thetaLength;
    int ix, iy, rowLen = wSeg + 1;
    int base = b->nv;
    for (iy = 0; iy <= hSeg; iy++) {
        float v = (float)iy / hSeg;
        float uOffset = 0.f;
        if (iy == 0 && thetaStart == 0) uOffset = 0.5f / wSeg;
        else if (iy == hSeg && thetaEnd >= GPI) uOffset = -0.5f / wSeg;
        for (ix = 0; ix <= wSeg; ix++) {
            float u = (float)ix / wSeg;
            float st = sinf(thetaStart + v * thetaLength);
            push_v(b,
                   -radius * cosf(phiStart + u * phiLength) * st,
                    radius * cosf(thetaStart + v * thetaLength),
                    radius * sinf(phiStart + u * phiLength) * st,
                    u + uOffset, 1.f - v);
        }
    }
    for (iy = 0; iy < hSeg; iy++)
        for (ix = 0; ix < wSeg; ix++) {
            int a = base + rowLen * iy + (ix + 1);
            int bb = base + rowLen * iy + ix;
            int c = base + rowLen * (iy + 1) + ix;
            int d = base + rowLen * (iy + 1) + (ix + 1);
            if (iy != 0 || thetaStart > 0) push_i(b, a, bb, d);
            if (iy != hSeg - 1 || thetaEnd < GPI) push_i(b, bb, c, d);
        }
}

/* ---------------- TorusGeometry ---------------- */
static void gen_torus(Buf *b, const float *p) {
    float radius = p[0], tube = p[1], arc = p[4];
    int radSeg = iseg(p[2]), tubSeg = iseg(p[3]);
    int i, j, rowLen = tubSeg + 1;
    int base = b->nv;
    for (j = 0; j <= radSeg; j++)
        for (i = 0; i <= tubSeg; i++) {
            float u = (float)i / tubSeg * arc;
            float v = (float)j / radSeg * GPI * 2.f;
            push_v(b,
                   (radius + tube * cosf(v)) * cosf(u),
                   (radius + tube * cosf(v)) * sinf(u),
                   tube * sinf(v),
                   (float)i / tubSeg, (float)j / radSeg);
        }
    for (j = 1; j <= radSeg; j++)
        for (i = 1; i <= tubSeg; i++) {
            int a = base + rowLen * j + i - 1;
            int bb = base + rowLen * (j - 1) + i - 1;
            int c = base + rowLen * (j - 1) + i;
            int d = base + rowLen * j + i;
            push_i(b, a, bb, d);
            push_i(b, bb, c, d);
        }
}

/* ---------------- PlaneGeometry ---------------- */
static void gen_plane(Buf *b, const float *p) {
    float width = p[0], height = p[1];
    int gridX = iseg(p[2]), gridY = iseg(p[3]);
    float halfW = width / 2, halfH = height / 2;
    float segW = width / gridX, segH = height / gridY;
    int ix, iy, rowLen = gridX + 1;
    int base = b->nv;
    for (iy = 0; iy <= gridY; iy++) {
        float y = iy * segH - halfH;
        for (ix = 0; ix <= gridX; ix++) {
            float x = ix * segW - halfW;
            push_v(b, x, -y, 0, (float)ix / gridX, 1.f - (float)iy / gridY);
        }
    }
    for (iy = 0; iy < gridY; iy++)
        for (ix = 0; ix < gridX; ix++) {
            int a = base + ix + rowLen * iy;
            int bb = base + ix + rowLen * (iy + 1);
            int c = base + (ix + 1) + rowLen * (iy + 1);
            int d = base + (ix + 1) + rowLen * iy;
            push_i(b, a, bb, d);
            push_i(b, bb, c, d);
        }
}

/* ---------------- IcosahedronGeometry ----------------
   PolyhedronGeometry output: non-indexed triangle soup, vertices pushed
   face by face and projected onto the sphere. Only detail 0 is used by
   the models, which is the 20 base faces untouched. */
static const float kIcoV[12 * 3] = {
    -1, 1.618033988749895f, 0,   1, 1.618033988749895f, 0,
    -1,-1.618033988749895f, 0,   1,-1.618033988749895f, 0,
     0,-1, 1.618033988749895f,   0, 1, 1.618033988749895f,
     0,-1,-1.618033988749895f,   0, 1,-1.618033988749895f,
     1.618033988749895f, 0,-1,   1.618033988749895f, 0, 1,
    -1.618033988749895f, 0,-1,  -1.618033988749895f, 0, 1
};
static const unsigned char kIcoI[20 * 3] = {
    0,11, 5,  0, 5, 1,  0, 1, 7,  0, 7,10,  0,10,11,
    1, 5, 9,  5,11, 4, 11,10, 2, 10, 7, 6,  7, 1, 8,
    3, 9, 4,  3, 4, 2,  3, 2, 6,  3, 6, 8,  3, 8, 9,
    4, 9, 5,  2, 4,11,  6, 2,10,  8, 6, 7,  9, 8, 1
};
static float ico_azimuth(float x, float z) { return atan2f(z, -x); }
static float ico_inclination(float x, float y, float z) {
    return atan2f(-y, sqrtf(x * x + z * z));
}
/* PolyhedronGeometry emits each face as (b, c, a) - that falls out of the
   subdivision walk, which pushes v[0][1], v[1][0], v[0][0]. */
static void gen_icosa(Buf *b, const float *p) {
    float radius = p[0];
    int f, k, i;
    for (f = 0; f < 20; f++) {
        static const int kOrder[3] = { 1, 2, 0 };
        for (k = 0; k < 3; k++) {
            const float *v = &kIcoV[kIcoI[f * 3 + kOrder[k]] * 3];
            float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            float x = v[0] / len * radius;
            float y = v[1] / len * radius;
            float z = v[2] / len * radius;
            float uu = ico_azimuth(x, z) / (2.f * GPI) + 0.5f;
            float vv = ico_inclination(x, y, z) / GPI + 0.5f;
            push_v(b, x, y, z, uu, 1.f - vv);
        }
    }
    /* correctUVs(): poles have no meaningful azimuth, so take the face's */
    for (i = 0; i + 3 <= b->nv; i += 3) {
        float cx = 0, cz = 0, azi;
        int t;
        for (t = 0; t < 3; t++) { cx += b->pos[(i + t) * 3]; cz += b->pos[(i + t) * 3 + 2]; }
        azi = ico_azimuth(cx / 3.f, cz / 3.f);
        for (t = 0; t < 3; t++) {
            float *uvp = &b->uv[(i + t) * 2];
            float px = b->pos[(i + t) * 3], pz = b->pos[(i + t) * 3 + 2];
            if (azi < 0 && uvp[0] == 1.f) uvp[0] -= 1.f;
            if (px == 0.f && pz == 0.f) uvp[0] = azi / (2.f * GPI) + 0.5f;
        }
    }
    /* correctSeam(): pull the wrapped corner of a seam-crossing face across */
    for (i = 0; i + 3 <= b->nv; i += 3) {
        float x0 = b->uv[i * 2], x1 = b->uv[(i + 1) * 2], x2 = b->uv[(i + 2) * 2];
        float mx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
        float mn = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
        if (mx > 0.9f && mn < 0.1f) {
            if (x0 < 0.2f) b->uv[i * 2] += 1.f;
            if (x1 < 0.2f) b->uv[(i + 1) * 2] += 1.f;
            if (x2 < 0.2f) b->uv[(i + 2) * 2] += 1.f;
        }
    }
}

/* ---------------- entry points ---------------- */
void geo_bounds(const GeoSpec *g, int *maxVerts, int *maxIdx) {
    const float *p = g->p;
    int v = 0, i = 0;
    switch (g->kind) {
        case GEO_BOX: {
            int ws = iseg(p[3]), hs = iseg(p[4]), ds = iseg(p[5]);
            v = 2 * ((ds + 1) * (hs + 1) + (ws + 1) * (ds + 1) + (ws + 1) * (hs + 1));
            i = 2 * (ds * hs + ws * ds + ws * hs) * 6;
            break;
        }
        case GEO_CYLINDER: case GEO_CONE: {
            int rs, hs;
            if (g->kind == GEO_CONE) { rs = iseg(p[2]); hs = iseg(p[3]); }
            else                     { rs = iseg(p[3]); hs = iseg(p[4]); }
            v = (rs + 1) * (hs + 1) + 2 * (rs + (rs + 1));
            i = rs * hs * 6 + 2 * rs * 3;
            break;
        }
        case GEO_SPHERE: {
            int ws = iseg(p[1]), hs = iseg(p[2]);
            v = (ws + 1) * (hs + 1);
            i = ws * hs * 6;
            break;
        }
        case GEO_TORUS: {
            int rs = iseg(p[2]), ts = iseg(p[3]);
            v = (rs + 1) * (ts + 1);
            i = rs * ts * 6;
            break;
        }
        case GEO_PLANE: {
            int ws = iseg(p[2]), hs = iseg(p[3]);
            v = (ws + 1) * (hs + 1);
            i = ws * hs * 6;
            break;
        }
        case GEO_ICOSA: v = 60; i = 0; break;
        default: break;
    }
    if (maxVerts) *maxVerts = v;
    if (maxIdx) *maxIdx = i;
}

int geo_build(const GeoSpec *g, float *pos, float *uv, unsigned short *idx,
              int maxVerts, int maxIdx, int *outVerts, int *outIdx) {
    Buf b;
    b.pos = pos; b.uv = uv; b.idx = idx;
    b.nv = 0; b.ni = 0; b.maxV = maxVerts; b.maxI = maxIdx; b.overflow = 0;
    if (outVerts) *outVerts = 0;
    if (outIdx) *outIdx = 0;

    switch (g->kind) {
        case GEO_BOX:      gen_box(&b, g->p); break;
        case GEO_CYLINDER: gen_cylinder(&b, g->p[0], g->p[1], g->p[2],
                                        iseg(g->p[3]), iseg(g->p[4]),
                                        g->p[5] != 0.f, g->p[6], g->p[7]); break;
        /* ConeGeometry is CylinderGeometry with a zero top radius */
        case GEO_CONE:     gen_cylinder(&b, 0.f, g->p[0], g->p[1],
                                        iseg(g->p[2]), iseg(g->p[3]),
                                        g->p[4] != 0.f, g->p[5], g->p[6]); break;
        case GEO_SPHERE:   gen_sphere(&b, g->p); break;
        case GEO_TORUS:    gen_torus(&b, g->p); break;
        case GEO_PLANE:    gen_plane(&b, g->p); break;
        case GEO_ICOSA:    gen_icosa(&b, g->p); break;
        default: return 0;
    }
    if (b.overflow) return 0;
    if (outVerts) *outVerts = b.nv;
    if (outIdx) *outIdx = b.ni;
    return 1;
}
