/* ==========================================================
   model.h : jointed actor models ported from the browser build.

   The browser builds each actor as a Three.js scene graph and animates
   it by rotating named joints. The node trees are lifted out of
   emberdeep.html by tests/model_ref.js and baked into src/model_data.c,
   so the geometry is the browser's own rather than a re-derivation.

   Transforms follow Three exactly: a node's local matrix is
   compose(position, quaternionFromEuler(rot, 'XYZ'), scale), and its
   world matrix is parentWorld * local.
   ========================================================== */
#ifndef EMBERDEEP_MODEL_H
#define EMBERDEEP_MODEL_H

#include "geom.h"
#include "texture.h"   /* SURF_* ids for each part */

/* column-major, same element order as THREE.Matrix4.elements */
typedef struct { float m[16]; } M4;

/* ---- joints the rig exposes, matching instantiate()'s userData ---- */
enum {
    JOINT_NONE = 0,
    JOINT_BODY, JOINT_CHEST, JOINT_HEAD, JOINT_TAIL,
    JOINT_CAPE0, JOINT_WING0, JOINT_WING1,
    JOINT_ARMR_S, JOINT_ARMR_E, JOINT_ARMR_H,
    JOINT_ARML_S, JOINT_ARML_E, JOINT_ARML_H,
    JOINT_LEG_BASE                      /* 8 legs, hip then knee */
};
#define MODEL_MAX_LEGS 8
#define JOINT_LEG_HIP(i)  (JOINT_LEG_BASE + (i) * 2)
#define JOINT_LEG_KNEE(i) (JOINT_LEG_BASE + (i) * 2 + 1)
#define JOINT_COUNT       (JOINT_LEG_BASE + MODEL_MAX_LEGS * 2)

/* ---- body plans, matching the rig's `kind` ---- */
enum { MKIND_BIPED, MKIND_ARACHNID, MKIND_QUAD, MKIND_FLOAT };

/* ---- one node of a model's tree ---- */
typedef struct {
    short parent;          /* index of the parent node, -1 for the root */
    unsigned char joint;   /* JOINT_* if the rig animates this node     */
    signed char geom;      /* GEO_* if this node draws, -1 if a group   */
    signed char surf;      /* SURF_* from texture.h, -1 if untextured   */
    float pos[3];
    float rot[3];          /* Euler XYZ, radians                        */
    float scale[3];
    float col[3];          /* the part's own material colour, 0..1      */
    float gp[GEO_MAX_PARAMS];
} ModelNode;

typedef struct ModelDefTag {
    const char *name;
    unsigned char kind;
    unsigned char legs;
    short nodeCount;
    const ModelNode *nodes;
} ModelDef;

/* model ids and the table itself live in model_data.h */
#include "model_data.h"

const ModelDef *model_get(int id);

/* ---- matrix helpers, mirroring THREE.Matrix4 ---- */
void m4_identity(M4 *out);
void m4_compose(const float pos[3], const float rot[3], const float scale[3],
                M4 *out);
void m4_mul(const M4 *a, const M4 *b, M4 *out);       /* out = a * b */
void m4_apply(const M4 *m, const float p[3], float out[3]);

/* Fill `out` (model->nodeCount entries) with world matrices.
   `jointRot` is JOINT_COUNT * 3 floats of extra Euler XYZ rotation applied
   on top of each joint's rest pose, or NULL for the rest pose. `root` is
   the actor's world placement, or NULL for the identity. */
void model_world(const ModelDef *model, const float *jointRot, const M4 *root,
                 M4 *out);

/* Walk the model and hand every triangle to `fn`, already in world space.
   `col` is the part's material colour, `pos` is 9 floats (three vertices),
   `uv` is 6. */
typedef void (*ModelTriFn)(void *ctx, int surf, const float *col,
                           const float *pos, const float *uv);
void model_emit(const ModelDef *model, const float *jointRot, const M4 *root,
                ModelTriFn fn, void *ctx);

/* Same, but against world matrices the caller already computed, and
   optionally restricted to one surface (surfFilter < 0 draws everything).
   Drawing a model one surface at a time lets each become a single bind and
   draw without recomputing the pose. */
void model_emit_world(const ModelDef *model, const M4 *world, int surfFilter,
                      ModelTriFn fn, void *ctx);

/* Tessellate every mesh in every model once, up front. model_emit builds
   this lazily too, but the Vita wants it done at load rather than mid-frame. */
int  model_cache_build(void);
void model_cache_free(void);

/* Largest nodeCount across all models, for sizing pose buffers. */
#define MODEL_MAX_NODES 68

#endif
