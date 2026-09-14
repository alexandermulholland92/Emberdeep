/* ==========================================================
   model.c : model tree traversal and Three-compatible transforms.
   ========================================================== */
#include "model.h"
#include <math.h>
#include <stdlib.h>

const ModelDef *model_get(int id) {
    if (id < 0 || id >= MODEL_COUNT) return 0;
    return &kModels[id];
}

void m4_identity(M4 *out) {
    int i;
    for (i = 0; i < 16; i++) out->m[i] = 0.f;
    out->m[0] = out->m[5] = out->m[10] = out->m[15] = 1.f;
}

/* THREE.Quaternion.setFromEuler, order 'XYZ', then Matrix4.compose */
void m4_compose(const float pos[3], const float rot[3], const float scale[3],
                M4 *out) {
    float c1 = cosf(rot[0] * 0.5f), c2 = cosf(rot[1] * 0.5f), c3 = cosf(rot[2] * 0.5f);
    float s1 = sinf(rot[0] * 0.5f), s2 = sinf(rot[1] * 0.5f), s3 = sinf(rot[2] * 0.5f);
    float x = s1 * c2 * c3 + c1 * s2 * s3;
    float y = c1 * s2 * c3 - s1 * c2 * s3;
    float z = c1 * c2 * s3 + s1 * s2 * c3;
    float w = c1 * c2 * c3 - s1 * s2 * s3;

    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;
    float sx = scale[0], sy = scale[1], sz = scale[2];
    float *t = out->m;

    t[0] = (1.f - (yy + zz)) * sx;
    t[1] = (xy + wz) * sx;
    t[2] = (xz - wy) * sx;
    t[3] = 0.f;
    t[4] = (xy - wz) * sy;
    t[5] = (1.f - (xx + zz)) * sy;
    t[6] = (yz + wx) * sy;
    t[7] = 0.f;
    t[8] = (xz + wy) * sz;
    t[9] = (yz - wx) * sz;
    t[10] = (1.f - (xx + yy)) * sz;
    t[11] = 0.f;
    t[12] = pos[0];
    t[13] = pos[1];
    t[14] = pos[2];
    t[15] = 1.f;
}

void m4_mul(const M4 *a, const M4 *b, M4 *out) {
    const float *ae = a->m, *be = b->m;
    float r[16];
    int c;
    for (c = 0; c < 4; c++) {
        float b0 = be[c * 4], b1 = be[c * 4 + 1], b2 = be[c * 4 + 2], b3 = be[c * 4 + 3];
        r[c * 4]     = ae[0] * b0 + ae[4] * b1 + ae[8]  * b2 + ae[12] * b3;
        r[c * 4 + 1] = ae[1] * b0 + ae[5] * b1 + ae[9]  * b2 + ae[13] * b3;
        r[c * 4 + 2] = ae[2] * b0 + ae[6] * b1 + ae[10] * b2 + ae[14] * b3;
        r[c * 4 + 3] = ae[3] * b0 + ae[7] * b1 + ae[11] * b2 + ae[15] * b3;
    }
    for (c = 0; c < 16; c++) out->m[c] = r[c];
}

void m4_apply(const M4 *m, const float p[3], float out[3]) {
    const float *t = m->m;
    float x = p[0], y = p[1], z = p[2];
    out[0] = t[0] * x + t[4] * y + t[8]  * z + t[12];
    out[1] = t[1] * x + t[5] * y + t[9]  * z + t[13];
    out[2] = t[2] * x + t[6] * y + t[10] * z + t[14];
}

void pose_init(const ModelDef *model, Pose *pose) {
    int i, k;
    if (!model || !pose) return;
    for (i = 0; i < JOINT_COUNT; i++)
        pose->rot[i][0] = pose->rot[i][1] = pose->rot[i][2] = 0.f;
    pose->bodyZ = 0.f;
    for (i = 0; i < model->nodeCount; i++) {
        const ModelNode *n = &model->nodes[i];
        if (n->joint == JOINT_NONE) continue;
        for (k = 0; k < 3; k++) pose->rot[n->joint][k] = n->rot[k];
        if (n->joint == JOINT_BODY) pose->bodyZ = n->pos[2];
    }
}

void model_world(const ModelDef *model, const Pose *pose, const M4 *root,
                 M4 *out) {
    int i;
    if (!model) return;
    for (i = 0; i < model->nodeCount; i++) {
        const ModelNode *n = &model->nodes[i];
        float rot[3], pos[3];
        M4 local;
        rot[0] = n->rot[0]; rot[1] = n->rot[1]; rot[2] = n->rot[2];
        pos[0] = n->pos[0]; pos[1] = n->pos[1]; pos[2] = n->pos[2];
        /* a posed joint's rotation replaces its rest value outright */
        if (pose && n->joint != JOINT_NONE) {
            rot[0] = pose->rot[n->joint][0];
            rot[1] = pose->rot[n->joint][1];
            rot[2] = pose->rot[n->joint][2];
            if (n->joint == JOINT_BODY) pos[2] = pose->bodyZ;
        }
        m4_compose(pos, rot, n->scale, &local);
        if (n->parent < 0) {
            if (root) m4_mul(root, &local, &out[i]);
            else out[i] = local;
        } else {
            m4_mul(&out[n->parent], &local, &out[i]);
        }
    }
}

/* ---------------- tessellation cache ----------------
   Every mesh node's primitive is tessellated once and kept, so a frame
   only has to transform vertices rather than rebuild circles and spheres. */
typedef struct { int vOff, vCount, iOff, iCount; } CacheEntry;

static float *gPos, *gUv;
static unsigned short *gIdx;
static CacheEntry *gEntry[MODEL_COUNT];
static int gCacheBuilt;

void model_cache_free(void) {
    int i;
    free(gPos); free(gUv); free(gIdx);
    gPos = 0; gUv = 0; gIdx = 0;
    for (i = 0; i < MODEL_COUNT; i++) { free(gEntry[i]); gEntry[i] = 0; }
    gCacheBuilt = 0;
}

int model_cache_build(void) {
    int m, i, totV = 0, totI = 0, atV = 0, atI = 0;
    if (gCacheBuilt) return 1;

    for (m = 0; m < MODEL_COUNT; m++) {
        const ModelDef *d = &kModels[m];
        for (i = 0; i < d->nodeCount; i++) {
            const ModelNode *n = &d->nodes[i];
            GeoSpec g;
            int v = 0, ix = 0, k;
            if (n->geom < 0) continue;
            g.kind = (unsigned char)n->geom;
            for (k = 0; k < GEO_MAX_PARAMS; k++) g.p[k] = n->gp[k];
            geo_bounds(&g, &v, &ix);
            totV += v; totI += ix;
        }
    }
    gPos = (float *)malloc(sizeof(float) * 3 * (size_t)totV);
    gUv  = (float *)malloc(sizeof(float) * 2 * (size_t)totV);
    gIdx = (unsigned short *)malloc(sizeof(unsigned short) * (size_t)(totI ? totI : 1));
    if (!gPos || !gUv || !gIdx) { model_cache_free(); return 0; }

    for (m = 0; m < MODEL_COUNT; m++) {
        const ModelDef *d = &kModels[m];
        gEntry[m] = (CacheEntry *)calloc((size_t)d->nodeCount, sizeof(CacheEntry));
        if (!gEntry[m]) { model_cache_free(); return 0; }
        for (i = 0; i < d->nodeCount; i++) {
            const ModelNode *n = &d->nodes[i];
            CacheEntry *e = &gEntry[m][i];
            GeoSpec g;
            int nv = 0, ni = 0, k, maxV = 0, maxI = 0;
            e->vCount = e->iCount = 0;
            if (n->geom < 0) continue;
            g.kind = (unsigned char)n->geom;
            for (k = 0; k < GEO_MAX_PARAMS; k++) g.p[k] = n->gp[k];
            geo_bounds(&g, &maxV, &maxI);
            if (!geo_build(&g, &gPos[atV * 3], &gUv[atV * 2], &gIdx[atI],
                           maxV, maxI, &nv, &ni))
                continue;
            e->vOff = atV; e->vCount = nv;
            e->iOff = atI; e->iCount = ni;
            atV += nv; atI += ni;
        }
    }
    gCacheBuilt = 1;
    return 1;
}

void model_emit_world(const ModelDef *model, const M4 *world, int surfFilter,
                      ModelTriFn fn, void *ctx) {
    int mid, i;
    if (!model || !world || !fn) return;
    if (!gCacheBuilt && !model_cache_build()) return;

    mid = (int)(model - kModels);
    if (mid < 0 || mid >= MODEL_COUNT) return;

    for (i = 0; i < model->nodeCount; i++) {
        const ModelNode *n = &model->nodes[i];
        const CacheEntry *e = &gEntry[mid][i];
        int t, k;
        if (n->geom < 0 || e->vCount == 0) continue;
        if (surfFilter >= 0 && n->surf != surfFilter) continue;

        if (e->iCount > 0) {
            for (t = 0; t + 2 < e->iCount; t += 3) {
                float p9[9], uv6[6];
                for (k = 0; k < 3; k++) {
                    int v = e->vOff + gIdx[e->iOff + t + k];
                    m4_apply(&world[i], &gPos[v * 3], &p9[k * 3]);
                    uv6[k * 2] = gUv[v * 2];
                    uv6[k * 2 + 1] = gUv[v * 2 + 1];
                }
                fn(ctx, n->surf, n->col, p9, uv6);
            }
        } else {
            /* non-indexed: the vertex list is already triangle soup */
            for (t = 0; t + 2 < e->vCount; t += 3) {
                float p9[9], uv6[6];
                for (k = 0; k < 3; k++) {
                    int v = e->vOff + t + k;
                    m4_apply(&world[i], &gPos[v * 3], &p9[k * 3]);
                    uv6[k * 2] = gUv[v * 2];
                    uv6[k * 2 + 1] = gUv[v * 2 + 1];
                }
                fn(ctx, n->surf, n->col, p9, uv6);
            }
        }
    }
}

void model_emit(const ModelDef *model, const Pose *pose, const M4 *root,
                ModelTriFn fn, void *ctx) {
    M4 world[MODEL_MAX_NODES];
    if (!model || model->nodeCount > MODEL_MAX_NODES) return;
    model_world(model, pose, root, world);
    model_emit_world(model, world, -1, fn, ctx);
}
