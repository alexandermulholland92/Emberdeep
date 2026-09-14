/* ==========================================================
   texture.c : procedural textures generated at load.

   A port of the browser build's generators, trimmed for the Vita:
   colour only, no normal or roughness maps. Lighting still comes from
   the baked vertex colours, and the texture modulates on top.
   ========================================================== */
#include "texture.h"
#include <stdlib.h>
#include <math.h>

/* ---------------- noise ---------------- */
/* integer hash: no trig, which matters a great deal at 444 MHz */
static unsigned int imul(unsigned int a, unsigned int b) { return a * b; }

static float ihash(int x, int y, int s) {
    unsigned int h = imul((unsigned int)x, 374761393u)
                   ^ imul((unsigned int)y, 668265263u)
                   ^ imul((unsigned int)s, 1274126177u);
    h = imul(h ^ (h >> 13), 1274126177u);
    return (float)((h ^ (h >> 16)) & 0xFFFFFFu) / (float)0x1000000;
}
static float smooth5(float t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }

/* periodic value noise, so the result tiles with no seam */
static float pnoise(float x, float y, int per, int seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float xf = x - xi, yf = y - yi;
    int p = per < 1 ? 1 : per;
    int x0 = ((xi % p) + p) % p, x1 = ((xi + 1) % p + p) % p;
    int y0 = ((yi % p) + p) % p, y1 = ((yi + 1) % p + p) % p;
    float u = smooth5(xf), v = smooth5(yf);
    float a = ihash(x0, y0, seed), b = ihash(x1, y0, seed);
    float c = ihash(x0, y1, seed), d = ihash(x1, y1, seed);
    return (a * (1.f - u) + b * u) * (1.f - v) + (c * (1.f - u) + d * u) * v;
}
static float pfbm(float x, float y, int per, int oct, int seed) {
    float amp = 0.5f, f = 1.f, sum = 0.f, norm = 0.f;
    int i;
    for (i = 0; i < oct; i++) {
        sum += pnoise(x * f, y * f, (int)(per * f), seed + i * 17) * amp;
        norm += amp; amp *= 0.5f; f *= 2.f;
    }
    return norm > 0.f ? sum / norm : 0.f;
}
static float pridge(float x, float y, int per, int oct, int seed) {
    float v = pfbm(x, y, per, oct, seed) * 2.f - 1.f;
    return 1.f - (v < 0.f ? -v : v);
}
/* cellular noise: distance to the nearest scattered point */
static float worley(float x, float y, int per, int seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    int p = per < 1 ? 1 : per;
    float best = 9.f;
    int ox, oy;
    for (ox = -1; ox <= 1; ox++) {
        for (oy = -1; oy <= 1; oy++) {
            int cx = xi + ox, cy = yi + oy;
            int wx = ((cx % p) + p) % p, wy = ((cy % p) + p) % p;
            float fx = cx + ihash(wx, wy, seed);
            float fy = cy + ihash(wx, wy, seed + 91);
            float dx = fx - x, dy = fy - y;
            float d = dx * dx + dy * dy;
            if (d < best) best = d;
        }
    }
    best = sqrtf(best);
    return best > 1.f ? 1.f : best;
}

static unsigned char clamp8(float v) {
    if (v < 0.f) return 0;
    if (v > 255.f) return 255;
    return (unsigned char)v;
}
static void put(unsigned char *px, int i, float k, float r, float g, float b) {
    px[i * 4 + 0] = clamp8(k * r);
    px[i * 4 + 1] = clamp8(k * g);
    px[i * 4 + 2] = clamp8(k * b);
    px[i * 4 + 3] = 255;
}

/* ---------------- recipes ----------------
   Near-white on purpose: the vertex colour carries the tint and the
   baked lighting, and GL_MODULATE multiplies the two together.        */

static void gen_floor_stone(unsigned char *px, int S, int seed) {
    int x, y;
    for (y = 0; y < S; y++) {
        for (x = 0; x < S; x++) {
            float u = (float)x / S, v = (float)y / S;
            float eu = u < 1.f - u ? u : 1.f - u;
            float ev = v < 1.f - v ? v : 1.f - v;
            float edge = eu < ev ? eu : ev;
            float slab = pfbm(u * 5.f, v * 5.f, 5, 4, seed) * 0.55f;
            float grain = pfbm(u * 34.f, v * 34.f, 34, 2, seed + 5) * 0.13f;
            float crack = pridge(u * 4.f, v * 4.f, 4, 3, seed + 23);
            float k = 0.72f + slab + grain;
            if (crack > 0.86f) k -= (crack - 0.86f) * 2.2f;         /* fractures */
            if ((1.f - worley(u * 7.f, v * 7.f, 7, seed + 11)) > 0.82f) k -= 0.16f;
            if (edge < 0.055f) k *= 0.44f;                          /* grout line */
            if (ihash(x, y, seed + 3) > 0.986f) k += 0.26f;         /* mica fleck */
            put(px, y * S + x, k, 220.f, 214.f, 206.f);
        }
    }
}

static void gen_wall_masonry(unsigned char *px, int S, int seed) {
    const int ROWS = 5, COLS = 3;
    int x, y;
    for (y = 0; y < S; y++) {
        for (x = 0; x < S; x++) {
            float u = (float)x / S, v = (float)y / S;
            int ry = (int)(v * ROWS);
            float off = (ry % 2) * 0.5f;
            float t = u * COLS + off;
            int rx = (int)floorf(t);
            float fx = t - floorf(t);
            float fy = v * ROWS - ry;
            float m1 = fx < 1.f - fx ? fx : 1.f - fx;
            float m2 = fy < 1.f - fy ? fy : 1.f - fy;
            float m = m1 < m2 ? m1 : m2;
            float face = pfbm(u * 10.f, v * 10.f, 10, 4, seed) * 0.5f;
            float tone = 0.86f + ihash(rx, ry, seed + 31) * 0.34f;  /* per block */
            float streak = pfbm(u * 3.f, v * 14.f, 14, 2, seed + 41) * 0.16f;
            float k = (0.6f + face) * tone - streak;
            if ((1.f - worley(u * 12.f, v * 12.f, 12, seed + 7)) > 0.86f) k -= 0.15f;
            if (m < 0.06f) k *= 0.52f;                              /* mortar */
            put(px, y * S + x, k, 218.f, 213.f, 208.f);
        }
    }
}

/* one grain shared by bodies: cloth weave crossed with a hide mottle */
static void gen_actor_grain(unsigned char *px, int S, int seed) {
    int x, y;
    for (y = 0; y < S; y++) {
        for (x = 0; x < S; x++) {
            float u = (float)x / S, v = (float)y / S;
            float warp = sinf(u * 6.2831853f * 14.f) * 0.5f + 0.5f;
            float weft = sinf(v * 6.2831853f * 14.f) * 0.5f + 0.5f;
            float weave = warp * weft * 0.22f;
            float mottle = pfbm(u * 7.f, v * 7.f, 7, 3, seed) * 0.42f;
            float pore = (1.f - worley(u * 18.f, v * 18.f, 18, seed + 13)) * 0.12f;
            float k = 0.80f + weave + mottle * 0.5f - pore;
            put(px, y * S + x, k, 232.f, 226.f, 218.f);
        }
    }
}

/* ---------------- upload ---------------- */
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

Textures gTex;

int tex_build(void) {
    const int S = TEX_SIZE;
    unsigned char *px = (unsigned char *)malloc((size_t)S * S * 4);
    unsigned char white[4];
    if (!px) return 0;

    gen_floor_stone(px, S, 101);   gTex.floor = upload(px, S);
    gen_wall_masonry(px, S, 111);  gTex.wall  = upload(px, S);
    gen_actor_grain(px, S, 31);    gTex.actor = upload(px, S);
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
    GLuint ids[4];
    ids[0] = gTex.floor; ids[1] = gTex.wall; ids[2] = gTex.actor; ids[3] = gTex.white;
    glDeleteTextures(4, ids);
    gTex.floor = gTex.wall = gTex.actor = gTex.white = 0;
}
