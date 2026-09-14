/* ==========================================================
   texture.c : procedural surfaces generated at load.

   A line-for-line port of the browser build's surface recipes. Each
   recipe is a height function plus a shade function, evaluated exactly
   as buildSurface() does: fill a height field first, then shade from it.

   Only the colour map is produced. The browser build also derives a
   normal map and a roughness map from the same height field and feeds
   them to a PBR material; the Vita draws through the fixed-function
   pipeline with lighting baked into vertex colours, so there is no
   per-pixel lighting for those maps to drive.

   Arithmetic is done in `treal` (double) to match the browser's numbers
   bit for bit - the recipes threshold on noise (chips, pits, flecks,
   knots), so a float rounding difference does not just shift a value,
   it flips a feature on or off.
   ========================================================== */
#include "texture.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef double treal;

#define TPI 3.14159265358979323846

static treal cl01(treal v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static treal tmin2(treal a, treal b) { return a < b ? a : b; }

/* ---------------- noise ---------------- */
/* integer hash, matching the browser's Math.imul chain exactly: the
   result is the full 32-bit word scaled by 2^32-1, not a truncation */
static treal ihash(int x, int y, int s) {
    unsigned int h = (unsigned int)x * 374761393u
                   ^ (unsigned int)y * 668265263u
                   ^ (unsigned int)s * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (treal)(h ^ (h >> 16)) / 4294967295.0;
}
static treal smooth5(treal t) { return t * t * t * (t * (t * 6 - 15) + 10); }

static int per_of(treal per) {
    treal r = floor(per + 0.5);      /* Math.round */
    int p = (int)r;
    return p < 1 ? 1 : p;
}

/* periodic value noise, so every texture tiles without a visible seam */
static treal pnoise(treal x, treal y, treal per, int seed) {
    int xi = (int)floor(x), yi = (int)floor(y);
    treal xf = x - xi, yf = y - yi;
    int p = per_of(per);
    int x0 = ((xi % p) + p) % p, x1 = ((xi + 1) % p + p) % p;
    int y0 = ((yi % p) + p) % p, y1 = ((yi + 1) % p + p) % p;
    treal u = smooth5(xf), v = smooth5(yf);
    treal a = ihash(x0, y0, seed), b = ihash(x1, y0, seed);
    treal c = ihash(x0, y1, seed), d = ihash(x1, y1, seed);
    return (a * (1 - u) + b * u) * (1 - v) + (c * (1 - u) + d * u) * v;
}
static treal pfbm(treal x, treal y, treal per, int oct, int seed) {
    treal amp = 0.5, f = 1, sum = 0, norm = 0;
    int i;
    for (i = 0; i < oct; i++) {
        sum += pnoise(x * f, y * f, per * f, seed + i * 17) * amp;
        norm += amp; amp *= 0.5; f *= 2;
    }
    return sum / norm;
}
/* ridged noise reads as cracks and fractures */
static treal pridge(treal x, treal y, treal per, int oct, int seed) {
    return 1 - fabs(pfbm(x, y, per, oct, seed) * 2 - 1);
}
/* cellular noise: distance to the nearest scattered feature point */
static treal worley(treal x, treal y, treal per, int seed) {
    int xi = (int)floor(x), yi = (int)floor(y);
    int p = per_of(per);
    treal best = 9;
    int ox, oy;
    for (ox = -1; ox <= 1; ox++) {
        for (oy = -1; oy <= 1; oy++) {
            int cx = xi + ox, cy = yi + oy;
            int wx = ((cx % p) + p) % p, wy = ((cy % p) + p) % p;
            treal fx = cx + ihash(wx, wy, seed);
            treal fy = cy + ihash(wx, wy, seed + 91);
            treal dx = fx - x, dy = fy - y;
            treal d = dx * dx + dy * dy;
            if (d < best) best = d;
        }
    }
    return cl01(sqrt(best));
}

/* Uint8ClampedArray assignment semantics: clamp, then round half to even */
static unsigned char to_u8(treal v) {
    treal f, d;
    long i;
    if (!(v > 0)) return 0;          /* also catches NaN */
    if (v >= 255) return 255;
    f = floor(v); d = v - f; i = (long)f;
    if (d > 0.5) return (unsigned char)(i + 1);
    if (d < 0.5) return (unsigned char)i;
    return (unsigned char)((i & 1) ? i + 1 : i);
}
static void tint(treal k, treal r, treal g, treal b, unsigned char *out) {
    out[0] = to_u8(cl01(k * r / 255) * 255);
    out[1] = to_u8(cl01(k * g / 255) * 255);
    out[2] = to_u8(cl01(k * b / 255) * 255);
    out[3] = 255;
}

/* ---------------- recipes ----------------
   All are near-white so the vertex colour does the tinting. */

/* ---- stoneFloorSurface ---- */
static treal h_floor(treal u, treal v, int seed, int coarse) {
    treal edge = tmin2(tmin2(u, v), tmin2(1 - u, 1 - v));
    treal grout = edge < 0.055 ? -0.9 * (1 - edge / 0.055) : 0;
    treal slab  = pfbm(u * 5, v * 5, 5, 4, seed) * 0.55;
    treal grain = pfbm(u * 34, v * 34, 34, 2, seed + 5) * 0.13;
    treal chip  = (1 - worley(u * 7, v * 7, 7, seed + 11)) > 0.82 ? -0.28 : 0;
    treal crack = pridge(u * 4, v * 4, 4, 3, seed + 23);
    (void)coarse;
    crack = crack > 0.86 ? -(crack - 0.86) * 4.2 : 0;
    return grout + slab + grain + chip + crack;
}
static void s_floor(treal h, treal u, treal v, int seed, int coarse, int size,
                    unsigned char *out) {
    treal edge = tmin2(tmin2(u, v), tmin2(1 - u, 1 - v));
    treal k = 0.72 + h * 0.5;
    (void)coarse;
    if (edge < 0.055) k *= 0.44;                  /* grout line, trodden dirt */
    if (h < -0.15) k *= 0.7;                      /* crevices hold grime */
    if (ihash((int)(u * size), (int)(v * size), seed + 3) > 0.986) k += 0.26;
    tint(k, 220, 214, 206, out);
}

/* ---- masonrySurface ---- */
#define MASON_ROWS 5
#define MASON_COLS 3
static void brick_of(treal u, treal v, int *rx, int *ry, treal *fx, treal *fy) {
    int r = (int)floor(v * MASON_ROWS);
    treal off = (r % 2) * 0.5;
    treal t = u * MASON_COLS + off;
    *ry = r;
    *rx = (int)floor(t);
    *fx = t - floor(t);
    *fy = v * MASON_ROWS - r;
}
static treal mason_m(treal fx, treal fy) {
    return tmin2(tmin2(fx, 1 - fx), tmin2(fy, 1 - fy));
}
static treal h_mason(treal u, treal v, int seed, int coarse) {
    int rx, ry; treal fx, fy, m, mortar, face, pit;
    (void)coarse;
    brick_of(u, v, &rx, &ry, &fx, &fy);
    m = mason_m(fx, fy);
    mortar = m < 0.06 ? -1.0 * (1 - m / 0.06) : 0;
    face = pfbm(u * 10, v * 10, 10, 4, seed) * 0.5;
    pit = (1 - worley(u * 12, v * 12, 12, seed + 7)) > 0.86 ? -0.3 : 0;
    return mortar + face + pit;
}
static void s_mason(treal h, treal u, treal v, int seed, int coarse, int size,
                    unsigned char *out) {
    int rx, ry; treal fx, fy, m, tone, streak, k;
    (void)coarse; (void)size;
    brick_of(u, v, &rx, &ry, &fx, &fy);
    m = mason_m(fx, fy);
    tone = 0.86 + ihash(rx, ry, seed + 31) * 0.34;   /* every block differs */
    streak = pfbm(u * 3, v * 14, 14, 2, seed + 41) * 0.16;
    k = (0.6 + h * 0.55) * tone - streak;
    if (m < 0.06) k *= 0.52;
    tint(k, 218, 213, 208, out);
}

/* ---- rockSurface ---- */
static treal h_rock(treal u, treal v, int seed, int coarse) {
    treal mass = pfbm(u * 6, v * 6, 6, 4, seed) * 0.8;
    treal frac = pridge(u * 5, v * 5, 5, 3, seed + 9);
    (void)coarse;
    return mass + (frac > 0.8 ? -(frac - 0.8) * 3.0 : 0);
}
static void s_rock(treal h, treal u, treal v, int seed, int coarse, int size,
                   unsigned char *out) {
    (void)u; (void)v; (void)seed; (void)coarse; (void)size;
    tint(0.66 + h * 0.6, 216, 214, 214, out);
}

/* ---- boneSurface ---- */
static treal h_bone(treal u, treal v, int seed, int coarse) {
    treal stri = pfbm(u * 3, v * 22, 22, 3, seed) * 0.6;   /* lengthwise striation */
    treal pore = (1 - worley(u * 16, v * 16, 16, seed + 13)) > 0.8 ? -0.35 : 0;
    (void)coarse;
    return stri + pore;
}
static void s_bone(treal h, treal u, treal v, int seed, int coarse, int size,
                   unsigned char *out) {
    treal k = 0.82 + h * 0.34;
    (void)u; (void)v; (void)seed; (void)coarse; (void)size;
    if (h < -0.1) k *= 0.78;
    tint(k, 232, 226, 205, out);
}

/* ---- clothSurface ---- */
static treal h_cloth(treal u, treal v, int seed, int coarse) {
    treal warp = sin(u * TPI * 2 * 26) * 0.5 + 0.5;
    treal weft = sin(v * TPI * 2 * 26) * 0.5 + 0.5;
    treal weave = warp * weft * 0.5;
    treal fibre = pfbm(u * 30, v * 30, 30, 2, seed) * 0.28;
    treal fold = pfbm(u * 3, v * 3, 3, 3, seed + 19) * 0.3;
    (void)coarse;
    return weave + fibre + fold;
}
static void s_cloth(treal h, treal u, treal v, int seed, int coarse, int size,
                    unsigned char *out) {
    (void)u; (void)v; (void)seed; (void)coarse; (void)size;
    tint(0.74 + h * 0.42, 222, 218, 210, out);
}

/* ---- leatherSurface ---- */
static treal h_leather(treal u, treal v, int seed, int coarse) {
    treal grain = worley(u * 14, v * 14, 14, seed) * 0.55;   /* pebbled hide */
    treal crease = pridge(u * 4, v * 4, 4, 2, seed + 27);
    (void)coarse;
    return grain + (crease > 0.84 ? -(crease - 0.84) * 2.4 : 0)
         + pfbm(u * 20, v * 20, 20, 2, seed + 5) * 0.16;
}
static void s_leather(treal h, treal u, treal v, int seed, int coarse, int size,
                      unsigned char *out) {
    (void)u; (void)v; (void)seed; (void)coarse; (void)size;
    tint(0.66 + h * 0.5, 220, 210, 198, out);
}

/* ---- metalSurface ---- */
static treal h_metal(treal u, treal v, int seed, int coarse) {
    treal brush = sin(v * 260 + pfbm(u * 4, v * 4, 4, 2, seed) * 14);
    treal scratch = brush > 0.94 ? -0.4 : 0;                 /* brushed lines */
    treal pit = (1 - worley(u * 18, v * 18, 18, seed + 3)) > 0.9 ? -0.5 : 0;
    (void)coarse;
    return scratch + pit + pfbm(u * 9, v * 9, 9, 3, seed + 7) * 0.3;
}
static void s_metal(treal h, treal u, treal v, int seed, int coarse, int size,
                    unsigned char *out) {
    treal k = 0.78 + h * 0.42;
    (void)u; (void)v; (void)seed; (void)coarse; (void)size;
    if (h < -0.2) k *= 0.72;                                 /* pits read darker */
    tint(k, 224, 226, 230, out);
}

/* ---- woodSurface ---- */
static treal h_wood(treal u, treal v, int seed, int coarse) {
    treal rings = fabs(sin((v * 9 + pfbm(u * 2, v * 2, 2, 2, seed) * 2.4) * TPI));
    treal grain = pfbm(u * 4, v * 40, 40, 2, seed + 11) * 0.3;
    treal knot = worley(u * 3, v * 3, 3, seed + 17) < 0.16 ? -0.4 : 0;
    (void)coarse;
    return rings * 0.45 + grain + knot;
}
static void s_wood(treal h, treal u, treal v, int seed, int coarse, int size,
                   unsigned char *out) {
    (void)u; (void)v; (void)seed; (void)coarse; (void)size;
    tint(0.62 + h * 0.55, 226, 205, 176, out);
}

/* ---- skinSurface(seed, coarse) ---- */
static treal h_skin(treal u, treal v, int seed, int coarse) {
    treal sc = coarse ? 26 : 40;
    treal pore = (1 - worley(u * sc, v * sc, sc, seed)) * 0.3;
    treal mottle = pfbm(u * 6, v * 6, 6, 3, seed + 9) * 0.6;
    return pore + mottle;
}
static void s_skin(treal h, treal u, treal v, int seed, int coarse, int size,
                   unsigned char *out) {
    (void)u; (void)v; (void)seed; (void)size;
    tint((coarse ? 0.72 : 0.8) + h * (coarse ? 0.46 : 0.34), 236, 222, 212, out);
}

/* ---------------- recipe table ---------------- */
typedef treal (*HeightFn)(treal u, treal v, int seed, int coarse);
typedef void  (*ShadeFn)(treal h, treal u, treal v, int seed, int coarse,
                         int size, unsigned char *out);

typedef struct {
    HeightFn height;
    ShadeFn  shade;
    int      seed;
    int      size;
    int      coarse;
} Recipe;

/* same order and seeds as the browser build's buildSurfaces() */
static const Recipe kRecipe[SURF_COUNT] = {
    { h_floor,   s_floor,   101, TEX_WORLD, 0 },
    { h_mason,   s_mason,   111, TEX_WORLD, 0 },
    { h_rock,    s_rock,     71, TEX_ACTOR, 0 },
    { h_bone,    s_bone,     21, TEX_ACTOR, 0 },
    { h_cloth,   s_cloth,    31, TEX_ACTOR, 0 },
    { h_leather, s_leather,  41, TEX_ACTOR, 0 },
    { h_metal,   s_metal,    51, TEX_ACTOR, 0 },
    { h_wood,    s_wood,     61, TEX_ACTOR, 0 },
    { h_skin,    s_skin,      3, TEX_ACTOR, 0 },
    { h_skin,    s_skin,     11, TEX_ACTOR, 1 }
};

int tex_size_of(int surf) {
    if (surf < 0 || surf >= SURF_COUNT) return 0;
    return kRecipe[surf].size;
}

void tex_gen(int surf, unsigned char *rgba) {
    const Recipe *rc;
    int S, x, y;
    treal *H;
    if (surf < 0 || surf >= SURF_COUNT) return;
    rc = &kRecipe[surf];
    S = rc->size;
    H = (treal *)malloc(sizeof(treal) * (size_t)S * S);
    if (!H) return;
    for (y = 0; y < S; y++)
        for (x = 0; x < S; x++)
            H[y * S + x] = rc->height((treal)x / S, (treal)y / S,
                                      rc->seed, rc->coarse);
    for (y = 0; y < S; y++)
        for (x = 0; x < S; x++) {
            int i = y * S + x;
            rc->shade(H[i], (treal)x / S, (treal)y / S, rc->seed, rc->coarse,
                      S, &rgba[i * 4]);
        }
    free(H);
}

/* ---------------- upload ---------------- */
#ifndef TEX_HOST_HARNESS
Textures gTex;

static GLuint upload(const unsigned char *px, int S) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return id;
}

int tex_build(void) {
    unsigned char *px = (unsigned char *)malloc((size_t)TEX_WORLD * TEX_WORLD * 4);
    unsigned char white[4];
    int i;
    if (!px) return 0;

    for (i = 0; i < SURF_COUNT; i++) {
        tex_gen(i, px);
        gTex.surf[i] = upload(px, tex_size_of(i));
    }
    free(px);

    /* a 1x1 white texture lets untextured passes keep the same pipeline */
    white[0] = white[1] = white[2] = white[3] = 255;
    glGenTextures(1, &gTex.white);
    glBindTexture(GL_TEXTURE_2D, gTex.white);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return 1;
}

void tex_free(void) {
    glDeleteTextures(SURF_COUNT, gTex.surf);
    glDeleteTextures(1, &gTex.white);
    memset(&gTex, 0, sizeof gTex);
}
#endif
