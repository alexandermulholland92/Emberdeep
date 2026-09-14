/* ==========================================================
   render.c : vitaGL drawing.

   Everything is transformed on the CPU into one big vertex batch and
   submitted as a handful of draw calls - the Vita's GPU is far happier
   with that than with thousands of small state changes.
   ========================================================== */
#include "game.h"
#include <vitaGL.h>
#include <stdio.h>
#include "texture.h"
#include "model.h"

#define MAX_BATCH_V   36000
#define MAX_WORLD_V   70000

static float  bpos[MAX_BATCH_V * 3];
static float  bcol[MAX_BATCH_V * 4];
static float  btex[MAX_BATCH_V * 2];
static int    bcount;

/* the world is split by material so each half can bind its own texture */
static float *fpos, *fcol, *ftex;   int fcount;   /* floor slabs   */
static float *kpos, *kcol, *ktex;   int kcount;   /* wall blocks   */
static int    wfloor = -1;

extern const char *fnt_glyph_rows(char c);   /* font.c: 7 rows of 5 bits */

/* ---------------- matrices ---------------- */
typedef struct { float m[16]; } Mat4;

static Mat4 mat_identity(void) {
    Mat4 r; int i;
    for (i = 0; i < 16; i++) r.m[i] = 0.f;
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
    return r;
}
static Mat4 mat_mul(Mat4 a, Mat4 b) {
    Mat4 r; int i, j, k;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            float s = 0.f;
            for (k = 0; k < 4; k++) s += a.m[k * 4 + j] * b.m[i * 4 + k];
            r.m[i * 4 + j] = s;
        }
    return r;
}
static Mat4 mat_perspective(float fovyDeg, float aspect, float zn, float zf) {
    Mat4 r = mat_identity();
    float f = 1.f / tanf(fovyDeg * 3.14159265f / 360.f);
    r.m[0] = f / aspect; r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1.f;
    r.m[14] = (2.f * zf * zn) / (zn - zf); r.m[15] = 0.f;
    return r;
}
static Mat4 mat_look_at(V3 eye, V3 at, V3 up) {
    Mat4 r = mat_identity();
    V3 f, s, u;
    float len;
    f.x = at.x - eye.x; f.y = at.y - eye.y; f.z = at.z - eye.z;
    len = sqrtf(f.x * f.x + f.y * f.y + f.z * f.z); if (len < 1e-6f) len = 1.f;
    f.x /= len; f.y /= len; f.z /= len;
    s.x = f.y * up.z - f.z * up.y; s.y = f.z * up.x - f.x * up.z; s.z = f.x * up.y - f.y * up.x;
    len = sqrtf(s.x * s.x + s.y * s.y + s.z * s.z); if (len < 1e-6f) len = 1.f;
    s.x /= len; s.y /= len; s.z /= len;
    u.x = s.y * f.z - s.z * f.y; u.y = s.z * f.x - s.x * f.z; u.z = s.x * f.y - s.y * f.x;
    r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
    r.m[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
    r.m[14] =  (f.x * eye.x + f.y * eye.y + f.z * eye.z);
    return r;
}
static Mat4 mat_ortho(float l, float r_, float b, float t, float zn, float zf) {
    Mat4 r = mat_identity();
    r.m[0] = 2.f / (r_ - l); r.m[5] = 2.f / (t - b); r.m[10] = -2.f / (zf - zn);
    r.m[12] = -(r_ + l) / (r_ - l); r.m[13] = -(t + b) / (t - b);
    r.m[14] = -(zf + zn) / (zf - zn);
    return r;
}

/* ---------------- unit cube, faces with baked shading ---------------- */
/* face order: +X -X +Y -Y +Z -Z ; shade approximates a high side light   */
static const float kFaceShade[6] = { 0.82f, 0.62f, 1.00f, 0.40f, 0.76f, 0.55f };
static const signed char kCubeFace[6][12] = {
  {  1,-1,-1,  1, 1,-1,  1, 1, 1,  1,-1, 1 },   /* +X */
  { -1,-1, 1, -1, 1, 1, -1, 1,-1, -1,-1,-1 },   /* -X */
  { -1, 1,-1, -1, 1, 1,  1, 1, 1,  1, 1,-1 },   /* +Y */
  { -1,-1, 1, -1,-1,-1,  1,-1,-1,  1,-1, 1 },   /* -Y */
  { -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1 },   /* +Z */
  {  1,-1,-1, -1,-1,-1, -1, 1,-1,  1, 1,-1 }    /* -Z */
};

static void batch_reset(void) { bcount = 0; }

/* submit whatever is in the dynamic batch under one texture, then clear it */
static void batch_flush(GLuint tex) {
    if (bcount <= 0) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    glVertexPointer(3, GL_FLOAT, 0, bpos);
    glColorPointer(4, GL_FLOAT, 0, bcol);
    glTexCoordPointer(2, GL_FLOAT, 0, btex);
    glDrawArrays(GL_TRIANGLES, 0, bcount);
    bcount = 0;
}

static void push_vert(float *vp, float *cp, float *tp, int *n,
                      float x, float y, float z,
                      float r, float g, float b, float a,
                      float u, float v) {
    int i = *n;
    vp[i * 3] = x; vp[i * 3 + 1] = y; vp[i * 3 + 2] = z;
    cp[i * 4] = r; cp[i * 4 + 1] = g; cp[i * 4 + 2] = b; cp[i * 4 + 3] = a;
    if (tp) { tp[i * 2] = u; tp[i * 2 + 1] = v; }
    *n = i + 1;
}

/* an axis-aligned box, optionally spun about Y, written into a target buffer */
static void emit_box(float *vp, float *cp, float *tp, int *n, int cap,
                     float cx, float cy, float cz,
                     float sx, float sy, float sz, float rotY,
                     float r, float g, float b, float a, float lightMul,
                     float uvScale) {
    float ca = cosf(rotY), sa = sinf(rotY);
    static const float qu[4] = { 0.f, 1.f, 1.f, 0.f };
    static const float qv[4] = { 0.f, 0.f, 1.f, 1.f };
    int f, k;
    if (*n + 36 > cap) return;
    for (f = 0; f < 6; f++) {
        float sh = kFaceShade[f] * lightMul;
        float fr = r * sh, fg = g * sh, fb = b * sh;
        float px[4], py[4], pz[4];
        for (k = 0; k < 4; k++) {
            float lx = kCubeFace[f][k * 3]     * sx * 0.5f;
            float ly = kCubeFace[f][k * 3 + 1] * sy * 0.5f;
            float lz = kCubeFace[f][k * 3 + 2] * sz * 0.5f;
            px[k] = cx + lx * ca + lz * sa;
            py[k] = cy + ly;
            pz[k] = cz - lx * sa + lz * ca;
        }
        push_vert(vp, cp, tp, n, px[0], py[0], pz[0], fr, fg, fb, a, qu[0]*uvScale, qv[0]*uvScale);
        push_vert(vp, cp, tp, n, px[1], py[1], pz[1], fr, fg, fb, a, qu[1]*uvScale, qv[1]*uvScale);
        push_vert(vp, cp, tp, n, px[2], py[2], pz[2], fr, fg, fb, a, qu[2]*uvScale, qv[2]*uvScale);
        push_vert(vp, cp, tp, n, px[0], py[0], pz[0], fr, fg, fb, a, qu[0]*uvScale, qv[0]*uvScale);
        push_vert(vp, cp, tp, n, px[2], py[2], pz[2], fr, fg, fb, a, qu[2]*uvScale, qv[2]*uvScale);
        push_vert(vp, cp, tp, n, px[3], py[3], pz[3], fr, fg, fb, a, qu[3]*uvScale, qv[3]*uvScale);
    }
}
static void box(float cx, float cy, float cz, float sx, float sy, float sz,
                float rotY, float r, float g, float b, float lightMul) {
    emit_box(bpos, bcol, btex, &bcount, MAX_BATCH_V, cx, cy, cz, sx, sy, sz, rotY,
             r, g, b, 1.f, lightMul, 1.f);
}
/* flat ground quad, used for shadows, the portal disc and telegraph rings */
static void ground_quad(float cx, float cz, float size,
                        float r, float g, float b, float a) {
    float h = size * 0.5f, y = 0.06f;
    if (bcount + 6 > MAX_BATCH_V) return;
    push_vert(bpos, bcol, btex, &bcount, cx - h, y, cz - h, r, g, b, a, 0.f, 0.f);
    push_vert(bpos, bcol, btex, &bcount, cx + h, y, cz - h, r, g, b, a, 1.f, 0.f);
    push_vert(bpos, bcol, btex, &bcount, cx + h, y, cz + h, r, g, b, a, 1.f, 1.f);
    push_vert(bpos, bcol, btex, &bcount, cx - h, y, cz - h, r, g, b, a, 0.f, 0.f);
    push_vert(bpos, bcol, btex, &bcount, cx + h, y, cz + h, r, g, b, a, 1.f, 1.f);
    push_vert(bpos, bcol, btex, &bcount, cx - h, y, cz + h, r, g, b, a, 0.f, 1.f);
}

/* ---------------- palettes ---------------- */
typedef struct { float r, g, b; } Col;
static Col col_of(float r, float g, float b) { Col c; c.r = r; c.g = g; c.b = b; return c; }

static const float kFloorTint[N_FLOORS][3] = {
    { 0.42f, 0.37f, 0.30f }, { 0.40f, 0.40f, 0.36f }, { 0.34f, 0.17f, 0.15f }
};
static const float kWallTint[N_FLOORS][3] = {
    { 0.20f, 0.18f, 0.22f }, { 0.19f, 0.20f, 0.23f }, { 0.20f, 0.12f, 0.12f }
};
static const float kTorch[N_FLOORS][3] = {
    { 1.00f, 0.60f, 0.27f }, { 0.50f, 0.77f, 1.00f }, { 1.00f, 0.42f, 0.13f }
};

static Col fx_colour(int fx) {
    switch (fx) {
        case FX_FIRE:  return col_of(1.f, 0.48f, 0.13f);
        case FX_BLOOD: return col_of(0.56f, 0.12f, 0.12f);
        case FX_BONE:  return col_of(0.85f, 0.82f, 0.74f);
        case FX_DUST:  return col_of(0.42f, 0.38f, 0.44f);
        case FX_HEAL:  return col_of(0.40f, 0.88f, 0.58f);
        case FX_GOLD:  return col_of(1.f, 0.84f, 0.42f);
        case FX_FROST: return col_of(0.62f, 0.86f, 1.f);
        case FX_VENOM: return col_of(0.66f, 0.88f, 0.35f);
        default:       return col_of(1.f, 0.76f, 0.28f);
    }
}

/* torch falloff: the carried light is what actually reveals the room */
static float light_at(float x, float z) {
    float dx = x - G.pl.pos.x, dz = z - G.pl.pos.z;
    float d = sqrtf(dx * dx + dz * dz);
    float k = 1.25f - d * 0.075f;
    if (k < 0.16f) k = 0.16f;
    if (k > 1.25f) k = 1.25f;
    return k;
}

/* ---------------- static world mesh ---------------- */
/* brazier positions: the inset corners of every room, matching the layout */
static float sTorchX[MAX_ROOMS * 4], sTorchZ[MAX_ROOMS * 4];
static int   sTorchN;

static void collect_torches(void) {
    int i;
    sTorchN = 0;
    for (i = 0; i < gRoomCount; i++) {
        Room *r = &gRooms[i];
        int sx[4], sy[4], k;
        sx[0] = r->x + 1;          sy[0] = r->y + 1;
        sx[1] = r->x + r->w - 2;   sy[1] = r->y + 1;
        sx[2] = r->x + 1;          sy[2] = r->y + r->h - 2;
        sx[3] = r->x + r->w - 2;   sy[3] = r->y + r->h - 2;
        for (k = 0; k < 4 && sTorchN < MAX_ROOMS * 4; k++) {
            sTorchX[sTorchN] = dg_world_of(sx[k]);
            sTorchZ[sTorchN] = dg_world_of(sy[k]);
            sTorchN++;
        }
    }
}
/* static light: brightest near a brazier, never fully black */
static float baked_light(float x, float z) {
    float best = 0.f;
    int i;
    for (i = 0; i < sTorchN; i++) {
        float dx = x - sTorchX[i], dz = z - sTorchZ[i];
        float d = sqrtf(dx * dx + dz * dz);
        float k = 1.15f - d * 0.115f;
        if (k > best) best = k;
    }
    if (best < 0.30f) best = 0.30f;
    if (best > 1.15f) best = 1.15f;
    return best;
}

static void build_world(void) {
    int gx, gy;
    const float *ft = kFloorTint[G.depth];
    const float *wt = kWallTint[G.depth];
    const float *tc = kTorch[G.depth];
    fcount = 0; kcount = 0;
    collect_torches();
    for (gx = 0; gx < GRID; gx++) {
        for (gy = 0; gy < GRID; gy++) {
            float wx = dg_world_of(gx), wz = dg_world_of(gy);
            if (gGrid[gx][gy] > 0) {
                float j = 0.86f + ((gx * 7 + gy * 13) % 11) * 0.026f;
                float L = baked_light(wx, wz);
                float wr = ft[0] * j * L + tc[0] * (L - 0.30f) * 0.10f;
                float wg = ft[1] * j * L + tc[1] * (L - 0.30f) * 0.10f;
                float wb = ft[2] * j * L + tc[2] * (L - 0.30f) * 0.10f;
                emit_box(fpos, fcol, ftex, &fcount, MAX_WORLD_V,
                         wx, -0.04f, wz, CELL - 0.09f, 0.10f, CELL - 0.09f, 0.f,
                         wr, wg, wb, 1.f, 1.f, 1.f);
            } else {
                int touching = 0, ox, oy;
                for (ox = -1; ox <= 1 && !touching; ox++)
                    for (oy = -1; oy <= 1; oy++) {
                        int nx = gx + ox, ny = gy + oy;
                        if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID) continue;
                        if (gGrid[nx][ny] > 0) { touching = 1; break; }
                    }
                if (touching) {
                    float j = 0.88f + ((gx * 5 + gy * 11) % 9) * 0.028f;
                    float L = baked_light(wx, wz);
                    /* walls tile the masonry twice over their height */
                    emit_box(kpos, kcol, ktex, &kcount, MAX_WORLD_V,
                             wx, WALL_H * 0.5f, wz, CELL, WALL_H, CELL, 0.f,
                             wt[0] * j * L, wt[1] * j * L, wt[2] * j * L, 1.f, 1.f, 2.f);
                }
            }
        }
    }
    wfloor = G.depth;
}

/* the world mesh is static, so relight it per frame by scaling its colours */
/* lighting is baked into the vertex colours; the texture modulates on top,
   so each material is one draw call */
static void draw_world(void) {
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glBindTexture(GL_TEXTURE_2D, gTex.surf[SURF_FLOOR_STONE]);
    glVertexPointer(3, GL_FLOAT, 0, fpos);
    glColorPointer(4, GL_FLOAT, 0, fcol);
    glTexCoordPointer(2, GL_FLOAT, 0, ftex);
    glDrawArrays(GL_TRIANGLES, 0, fcount);

    glBindTexture(GL_TEXTURE_2D, gTex.surf[SURF_WALL_MASON]);
    glVertexPointer(3, GL_FLOAT, 0, kpos);
    glColorPointer(4, GL_FLOAT, 0, kcol);
    glTexCoordPointer(2, GL_FLOAT, 0, ktex);
    glDrawArrays(GL_TRIANGLES, 0, kcount);
}

/* ---------------- actors ---------------- */
/* Which baked model each actor uses. The browser picks these in its
   roster; the ids come from model_data.h, generated from that same roster. */
static int model_for_enemy(int type) {
    switch (type) {
        case E_GOBLIN:   return MODEL_GOBLIN;
        case E_ARCHER:   return MODEL_ARCHER;
        case E_SPIDER:   return MODEL_SPIDER;
        case E_SKELETON: return MODEL_SKELETON;
        case E_WRAITH:   return MODEL_WRAITH;
        case E_HOUND:    return MODEL_HOUND;
        case E_IMP:      return MODEL_IMP;
        case E_GOLEM:    return MODEL_GOLEM;
        case B_WARCHIEF: return MODEL_WARCHIEF;
        case B_COLOSSUS: return MODEL_COLOSSUS;
        default:         return MODEL_WARDEN;
    }
}
static int model_for_class(int cls) {
    if (cls == CLS_VANGUARD) return MODEL_VANGUARD;
    if (cls == CLS_PYRO)     return MODEL_PYROMANCER;
    return MODEL_RANGER;
}

/* one actor's shading state while its triangles stream out of the model */
typedef struct { float light; int flash; } ActorCtx;

static void actor_tri(void *ctx, int surf, const float *col,
                      const float *pos, const float *uv) {
    ActorCtx *a = (ActorCtx *)ctx;
    float r, g, b;
    int k;
    (void)surf;
    if (bcount + 3 > MAX_BATCH_V) return;
    if (a->flash) { r = 1.f; g = 1.f; b = 1.f; }
    else {
        r = col[0] * a->light;
        g = col[1] * a->light;
        b = col[2] * a->light;
    }
    for (k = 0; k < 3; k++)
        push_vert(bpos, bcol, btex, &bcount,
                  pos[k * 3], pos[k * 3 + 1], pos[k * 3 + 2],
                  r, g, b, 1.f, uv[k * 2], uv[k * 2 + 1]);
}

/* Pose the model once, then draw it one surface at a time so each
   material costs a single bind and a single draw. */
static void draw_model(int modelId, float x, float y, float z,
                       float facing, float scale, float light, int flash) {
    const ModelDef *md = model_get(modelId);
    M4 root, world[MODEL_MAX_NODES];
    float p[3], rot[3], sc[3];
    ActorCtx actx;
    int s;
    if (!md || md->nodeCount > MODEL_MAX_NODES) return;

    p[0] = x; p[1] = y; p[2] = z;
    rot[0] = 0.f; rot[1] = facing; rot[2] = 0.f;
    sc[0] = sc[1] = sc[2] = scale;
    m4_compose(p, rot, sc, &root);
    /* rest pose for now - the joint animation lands in the next stage */
    model_world(md, 0, &root, world);

    actx.light = light;
    actx.flash = flash;
    for (s = 0; s < SURF_COUNT; s++) {
        batch_reset();
        model_emit_world(md, world, s, actor_tri, &actx);
        batch_flush(gTex.surf[s]);
    }
}

static void draw_actors(void) {
    int i;

    /* blob shadows and the portal disc share the untextured pass */
    batch_reset();
    if (G.pl.alive)
        ground_quad(G.pl.pos.x, G.pl.pos.z, 0.9f, 0.f, 0.f, 0.f, 0.34f);
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &G.en[i];
        float sc;
        if (!e->active) continue;
        sc = gEnemyDef[e->type].scale;
        if (e->dying) sc *= (e->deathT / e->deathDur);
        if (sc < 0.05f) continue;
        ground_quad(e->pos.x, e->pos.z, 0.9f * sc, 0.f, 0.f, 0.f, 0.34f);
    }
    if (G.portalOn) {
        float pulse = 0.6f + sinf(G.elapsed * 3.f) * 0.25f;
        ground_quad(G.portalPos.x, G.portalPos.z, 2.6f, 0.35f, 0.75f, 1.f, pulse);
    }
    batch_flush(gTex.white);

    /* bodies */
    if (G.pl.alive) {
        float k = light_at(G.pl.pos.x, G.pl.pos.z);
        if (G.pl.invuln > 0.f) k *= 1.35f;
        draw_model(model_for_class(G.pl.cls), G.pl.pos.x, 0.f, G.pl.pos.z,
                   G.pl.facing, 1.f, k, 0);
    }
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &G.en[i];
        float sc;
        if (!e->active) continue;
        sc = gEnemyDef[e->type].scale;
        if (e->dying) sc *= (e->deathT / e->deathDur);
        if (sc < 0.05f) continue;
        draw_model(model_for_enemy(e->type), e->pos.x, e->baseY, e->pos.z,
                   e->facing, sc, light_at(e->pos.x, e->pos.z), e->flash > 0.f);
    }
}

static void draw_projectiles(void) {
    int i;
    for (i = 0; i < MAX_PROJ; i++) {
        Proj *p = &G.pr[i];
        Col c;
        if (!p->active) continue;
        c = fx_colour(p->fx);
        box(p->pos.x, p->pos.y > 0.1f ? p->pos.y : 1.f, p->pos.z,
            0.3f, 0.3f, 0.3f, G.elapsed * 4.f, c.r, c.g, c.b, 1.3f);
    }
}

static void draw_particles(void) {
    int i;
    for (i = 0; i < MAX_PART; i++) {
        Particle *p = &G.pa[i];
        Col c;
        float s;
        if (!p->active) continue;
        c = fx_colour(p->fx);
        s = p->size * (p->life / p->maxLife);
        box(p->pos.x, p->pos.y, p->pos.z, s, s, s, 0.f, c.r, c.g, c.b, 1.25f);
    }
}

/* ---------------- HUD ---------------- */
static void hud_rect(float x, float y, float w, float h,
                     float r, float g, float b, float a) {
    if (bcount + 6 > MAX_BATCH_V) return;
    push_vert(bpos, bcol, btex, &bcount, x,     y,     0.f, r, g, b, a, 0.f, 0.f);
    push_vert(bpos, bcol, btex, &bcount, x + w, y,     0.f, r, g, b, a, 1.f, 0.f);
    push_vert(bpos, bcol, btex, &bcount, x + w, y + h, 0.f, r, g, b, a, 1.f, 1.f);
    push_vert(bpos, bcol, btex, &bcount, x,     y,     0.f, r, g, b, a, 0.f, 0.f);
    push_vert(bpos, bcol, btex, &bcount, x + w, y + h, 0.f, r, g, b, a, 1.f, 1.f);
    push_vert(bpos, bcol, btex, &bcount, x,     y + h, 0.f, r, g, b, a, 0.f, 1.f);
}

static void hud_text(float x, float y, float px, const char *s,
                     float r, float g, float b, float a) {
    float cx = x;
    for (; *s; s++) {
        const char *rows = fnt_glyph_rows(*s);
        int ry, rx;
        if (rows) {
            for (ry = 0; ry < 7; ry++) {
                unsigned char bits = (unsigned char)rows[ry];
                for (rx = 0; rx < 5; rx++)
                    if (bits & (1 << (4 - rx)))
                        hud_rect(cx + rx * px, y + ry * px, px, px, r, g, b, a);
            }
        }
        cx += px * 6.f;
    }
}
static float text_w(const char *s, float px) { return (float)strlen(s) * px * 6.f; }

static void hud_bar(float x, float y, float w, float h, float frac,
                    float r, float g, float b) {
    if (frac < 0.f) frac = 0.f;
    if (frac > 1.f) frac = 1.f;
    hud_rect(x - 1.f, y - 1.f, w + 2.f, h + 2.f, 0.75f, 0.68f, 0.50f, 0.55f);
    hud_rect(x, y, w, h, 0.05f, 0.04f, 0.06f, 0.85f);
    hud_rect(x, y, w * frac, h, r, g, b, 1.f);
}

static void draw_hud(void) {
    const ClassDef *c = &gClassDef[G.pl.cls];
    char buf[64];
    int i;
    float bx = 96.f, by = 18.f, bw = 250.f;

    hud_rect(0.f, 0.f, (float)SCR_W, 84.f, 0.02f, 0.02f, 0.03f, 0.55f);

    snprintf(buf, sizeof buf, "LV %d", G.pl.level);
    hud_text(18.f, 20.f, 2.6f, buf, 1.f, 0.90f, 0.62f, 1.f);
    hud_text(18.f, 46.f, 1.7f, c->name, 0.70f, 0.66f, 0.55f, 1.f);

    hud_bar(bx, by, bw, 13.f, G.pl.hp / G.pl.hpMax, 0.78f, 0.22f, 0.17f);
    hud_bar(bx, by + 20.f, bw, 9.f, G.pl.res / G.pl.resMax, 0.26f, 0.55f, 0.82f);
    hud_bar(bx, by + 34.f, bw, 6.f, G.pl.xp / G.pl.xpNext, 0.72f, 0.60f, 0.28f);

    snprintf(buf, sizeof buf, "%d/%d", (int)G.pl.hp, (int)G.pl.hpMax);
    hud_text(bx + bw + 12.f, by + 1.f, 1.7f, buf, 0.95f, 0.85f, 0.80f, 1.f);

    hud_text((float)SCR_W - 250.f, 20.f, 1.9f, ac_floor_name(G.depth), 0.78f, 0.70f, 0.52f, 1.f);
    snprintf(buf, sizeof buf, "KILLS %d", G.kills);
    hud_text((float)SCR_W - 250.f, 44.f, 1.6f, buf, 0.60f, 0.56f, 0.46f, 1.f);

    /* ability slots: X, square, triangle, circle */
    {
        const char *keys[4] = { "X", "[]", "/\\", "O" };
        for (i = 0; i < 4; i++) {
            float ax = (float)SCR_W - 360.f + i * 88.f, ay = (float)SCR_H - 66.f;
            int locked = G.pl.level < c->unlock[i];
            float cd = c->cd[i] > 0.f ? G.pl.cds[i] / c->cd[i] : 0.f;
            float a = locked ? 0.22f : 0.85f;
            hud_rect(ax, ay, 80.f, 48.f, 0.10f, 0.09f, 0.12f, a);
            if (!locked && cd > 0.f)
                hud_rect(ax, ay, 80.f, 48.f * cd, 0.f, 0.f, 0.f, 0.62f);
            hud_text(ax + 6.f, ay + 4.f, 1.5f, keys[i], 1.f, 0.86f, 0.55f, a + 0.15f);
            hud_text(ax + 6.f, ay + 26.f, 1.2f, c->abil[i], 0.85f, 0.80f, 0.68f, a);
        }
    }

    if (G.toastT > 0.f && G.toastText)
        hud_text(((float)SCR_W - text_w(G.toastText, 2.2f)) * 0.5f, 120.f, 2.2f,
                 G.toastText, 1.f, 0.84f, 0.42f, 1.f);
    if (G.bannerT > 0.f && G.bannerText)
        hud_text(((float)SCR_W - text_w(G.bannerText, 3.4f)) * 0.5f, 210.f, 3.4f,
                 G.bannerText, 0.95f, 0.88f, 0.72f, 1.f);
}

static void draw_overlay_screen(void) {
    const char *title, *hint;
    char buf[64];
    hud_rect(0.f, 0.f, (float)SCR_W, (float)SCR_H, 0.02f, 0.02f, 0.04f, 0.86f);
    if (G.state == ST_CLASS) {
        int i;
        title = "DUNGEONS OF THE EMBERDEEP";
        hud_text(((float)SCR_W - text_w(title, 3.2f)) * 0.5f, 70.f, 3.2f, title,
                 0.93f, 0.81f, 0.55f, 1.f);
        for (i = 0; i < CLS_COUNT; i++) {
            float y = 180.f + i * 96.f;
            int sel = (G.classPick == i);
            hud_rect(200.f, y - 14.f, 560.f, 78.f,
                     sel ? 0.20f : 0.09f, sel ? 0.15f : 0.08f, sel ? 0.10f : 0.10f, 0.9f);
            hud_text(224.f, y, 2.6f, gClassDef[i].name,
                     sel ? 1.f : 0.75f, sel ? 0.88f : 0.70f, sel ? 0.55f : 0.58f, 1.f);
            hud_text(224.f, y + 30.f, 1.5f, gClassDef[i].role, 0.62f, 0.58f, 0.48f, 1.f);
        }
        hint = "UP/DOWN CHOOSE     X BEGIN";
        hud_text(((float)SCR_W - text_w(hint, 1.8f)) * 0.5f, 480.f, 1.8f, hint,
                 0.66f, 0.62f, 0.52f, 1.f);
    } else {
        title = (G.state == ST_WIN) ? "THE EMBERDEEP FALLS" : "YOU DIED";
        hud_text(((float)SCR_W - text_w(title, 3.6f)) * 0.5f, 190.f, 3.6f, title,
                 0.93f, 0.78f, 0.52f, 1.f);
        snprintf(buf, sizeof buf, "LEVEL %d    KILLS %d    FLOOR %d",
                 G.pl.level, G.kills, G.depth + 1);
        hud_text(((float)SCR_W - text_w(buf, 2.f)) * 0.5f, 260.f, 2.f, buf,
                 0.72f, 0.68f, 0.58f, 1.f);
        hint = "X  DESCEND AGAIN";
        hud_text(((float)SCR_W - text_w(hint, 2.f)) * 0.5f, 340.f, 2.f, hint,
                 0.66f, 0.62f, 0.52f, 1.f);
    }
}

/* ---------------- frame ---------------- */
int rd_init(void) {
    fpos = (float *)malloc(sizeof(float) * MAX_WORLD_V * 3);
    fcol = (float *)malloc(sizeof(float) * MAX_WORLD_V * 4);
    ftex = (float *)malloc(sizeof(float) * MAX_WORLD_V * 2);
    kpos = (float *)malloc(sizeof(float) * MAX_WORLD_V * 3);
    kcol = (float *)malloc(sizeof(float) * MAX_WORLD_V * 4);
    ktex = (float *)malloc(sizeof(float) * MAX_WORLD_V * 2);
    if (!fpos || !fcol || !ftex || !kpos || !kcol || !ktex) return 0;

    vglInit(0x800000);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_TEXTURE_2D);
    /* MODULATE multiplies texture by vertex colour, which is where the tint
       and the baked lighting live */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glClearColor(0.03f, 0.03f, 0.05f, 1.f);

    if (!tex_build()) return 0;
    if (!model_cache_build()) return 0;
    return 1;
}
void rd_shutdown(void) {
    /* vitaGL exposes no teardown call - releasing our own buffers is all there
       is to do, and process exit tears GXM down. */
    tex_free();
    model_cache_free();
    free(fpos); free(fcol); free(ftex);
    free(kpos); free(kcol); free(ktex);
    fpos = fcol = ftex = kpos = kcol = ktex = NULL;
}

void rd_frame(float dt) {
    Mat4 proj, view, vp;
    V3 eye, at, up;
    float sh = G.shake;

    (void)dt;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (G.state == ST_PLAY || G.state == ST_DEAD || G.state == ST_WIN) {
        if (wfloor != G.depth) build_world();

        eye.x = G.pl.pos.x + (sh > 0.f ? rndr(-sh, sh) * 0.4f : 0.f);
        eye.y = 10.6f + (sh > 0.f ? rndr(-sh, sh) * 0.3f : 0.f);
        eye.z = G.pl.pos.z + 6.0f;
        at.x = G.pl.pos.x; at.y = 1.1f; at.z = G.pl.pos.z - 3.2f;
        up.x = 0.f; up.y = 1.f; up.z = 0.f;

        proj = mat_perspective(52.f, (float)SCR_W / (float)SCR_H, 0.1f, 90.f);
        view = mat_look_at(eye, at, up);
        vp = mat_mul(proj, view);

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(vp.m);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glEnable(GL_DEPTH_TEST);
        draw_world();

        draw_actors();                    /* binds and flushes per body material */

        batch_reset();
        draw_projectiles();
        draw_particles();
        batch_flush(gTex.white);          /* effects stay flat and bright */
    }

    /* 2D pass */
    {
        Mat4 o = mat_ortho(0.f, (float)SCR_W, (float)SCR_H, 0.f, -1.f, 1.f);
        glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(o.m);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        batch_reset();
        if (G.state == ST_PLAY) draw_hud();
        else draw_overlay_screen();
        batch_flush(gTex.white);
    }

    vglSwapBuffers(GL_FALSE);
}
