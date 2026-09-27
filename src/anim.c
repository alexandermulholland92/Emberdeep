/* ==========================================================
   anim.c : animateActor, ported from the browser build.

   Joint rotations are written into the Pose, which persists between
   frames - several of them ease toward a target rather than being
   recomputed from scratch, exactly as the original does.
   ========================================================== */
#include "anim.h"
#include <math.h>

#define API 3.14159265358979323846f

static float clampf01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

/* approach() from the browser: a clamped per-frame lerp that snaps once
   it is close enough, so a value actually reaches its target */
static float approach(float cur, float target, float rate) {
    float d = target - cur;
    return fabsf(d) < 1e-4f ? target : cur + d * clampf01(rate);
}
static float maxf(float a, float b) { return a > b ? a : b; }
static float minf(float a, float b) { return a < b ? a : b; }

void anim_init(ActorAnim *a, const ModelDef *model) {
    int i;
    if (!a) return;
    a->model = model;
    pose_init(model, &a->pose);
    a->walkT = 0.f;
    a->floatT = 0.f;
    a->atkAnim = 0.f;
    a->atkAnimDur = 0.3f;
    a->atkSwing = 1.4f;
    a->rootY = 0.f;
    a->lastX = a->lastZ = 0.f;
    a->stride = 0.05f;
    a->thigh = a->sole = a->heel = a->toe = 0.f;
    a->scale = 1.f;
    a->started = 0;
    for (i = 0; i < MODEL_MAX_LEGS; i++)
        a->legRestZ[i] = model ? a->pose.rot[JOINT_LEG_HIP(i)][2] : 0.f;

    /* Every stepping rig's legs are alike, so the first one is measured:
       the thigh, and the foot - the lowest box hung from the knee - whose
       heel or toe is what meets the floor once the shin tilts. */
    if (model && model->legs > 0) {
        int hip = -1, knee = -1;
        for (i = 0; i < model->nodeCount; i++) {
            if (model->nodes[i].joint == JOINT_LEG_HIP(0)) hip = i;
            if (model->nodes[i].joint == JOINT_LEG_KNEE(0)) knee = i;
        }
        if (hip >= 0 && knee >= 0 && model->nodes[knee].parent == hip) {
            a->thigh = -model->nodes[knee].pos[1];
            for (i = 0; i < model->nodeCount; i++) {
                const ModelNode *n = &model->nodes[i];
                float depth;
                if (n->parent != knee || n->geom != GEO_BOX) continue;
                depth = -n->pos[1] + n->gp[1] * 0.5f * n->scale[1];
                if (depth > a->sole) {
                    a->sole = depth;
                    a->heel = n->pos[2] - n->gp[2] * 0.5f * n->scale[2];
                    a->toe = n->pos[2] + n->gp[2] * 0.5f * n->scale[2];
                }
            }
        }
    }
}

/* How far a foot's lowest corner rises off the floor with the hip at `hip`
   and the knee at `knee` (both about x, the knee's added to the thigh's):
   the thigh and shin lift it as they swing off vertical, and the toe dips
   as the shin tilts back, the heel as it tilts forward. */
static float foot_lift(const ActorAnim *a, float hip, float knee) {
    float shin = hip + knee, s = sinf(shin);
    return a->thigh * (1.f - cosf(hip)) + a->sole * (1.f - cosf(shin))
         - maxf(a->toe * s, a->heel * s);
}

void anim_swing(ActorAnim *a, float amount, float dur) {
    if (!a) return;
    a->atkAnim = dur > 0.f ? dur : 0.3f;
    a->atkAnimDur = a->atkAnim;
    a->atkSwing = amount != 0.f ? amount : 1.4f;
}

void anim_update(ActorAnim *a, float dt, int moving, int telegraphing,
                 float baseY) {
    const ModelDef *m;
    Pose *p;
    float amp;
    int i, legs;

    if (!a || !a->model) return;
    m = a->model;
    p = &a->pose;
    amp = moving ? 1.f : 0.05f;
    legs = m->legs;

    /* ---- locomotion ---- */
    if (m->kind == MKIND_FLOAT) {
        a->floatT += dt * 2.2f;
        a->rootY = baseY + sinf(a->floatT) * 0.15f;
        {
            float f = sinf(a->floatT * 7.f) * 0.8f;
            p->rot[JOINT_WING0][2] = 0.25f + f;
            p->rot[JOINT_WING1][2] = -0.25f - f;
        }
        for (i = 0; i < legs; i++) {
            p->rot[JOINT_LEG_HIP(i)][0] = 0.3f + sinf(a->floatT + i) * 0.12f;
            p->rot[JOINT_LEG_KNEE(i)][0] = -0.45f;
        }
    } else if (m->kind == MKIND_ARACHNID) {
        a->walkT += dt * (moving ? 15.f : 2.f);
        a->rootY = baseY + fabsf(sinf(a->walkT * 2.f)) * 0.028f;
        for (i = 0; i < legs; i++) {
            float ph = a->walkT + i * 0.85f;
            p->rot[JOINT_LEG_HIP(i)][2] = a->legRestZ[i] + sinf(ph) * 0.22f * amp;
            p->rot[JOINT_LEG_HIP(i)][0] = cosf(ph) * 0.2f * amp;
        }
    } else {
        /* biped / quadruped stride. Rigs face +z and a positive x rotation
           swings a limb's far end backwards, so with the hip at
           sin(phase) a leg swings forward while cos(phase) < 0 and is
           planted while cos(phase) > 0.

           The knee folds positive - shin back, heel up, the way a knee
           bends - and only while its leg swings through, on a real
           walk's timing: it starts to lift the heel shortly before the
           hip stops driving back, is most bent a third of the way into
           the swing, and is straight again before the foot reaches out in
           front to land. (The browser folded it negative, which kicks the
           shin forward like a knee bending backwards, and swung the leg
           forward straight - together they read as walking backwards.)

           The body then settles onto whichever foot is lowest, so it dips
           as the legs spread and rides highest over a straight planted
           leg, instead of lifting the feet off the floor mid-step. */
        float rate = (m->kind == MKIND_QUAD) ? 11.f : 8.5f;
        float lock = 1e9f, walking;
        a->stride = approach(a->stride, moving ? 1.f : 0.05f, dt * 12.f);
        a->walkT += dt * (moving ? rate : 2.2f) * (moving < 0 ? -1.f : 1.f);
        for (i = 0; i < legs; i++) {
            float phase = (m->kind == MKIND_QUAD)
                ? a->walkT + ((i == 0 || i == 3) ? 0.f : API)   /* diagonal gait */
                : a->walkT + (i ? API : 0.f);
            float hip = sinf(phase) * 0.55f * a->stride;
            float knee = maxf(0.f, -cosf(phase + 0.5f)) * 0.85f * a->stride;
            p->rot[JOINT_LEG_HIP(i)][0] = hip;
            p->rot[JOINT_LEG_KNEE(i)][0] = knee;
            lock = minf(lock, foot_lift(a, hip, knee));
        }
        if (legs == 0) lock = 0.f;
        /* standing still keeps the browser's gentle breathing bob */
        walking = (a->stride - 0.05f) / 0.95f;
        a->rootY = baseY - lock * a->scale * walking
                 + sinf(a->walkT) * 0.008f * (1.f - walking);
        p->rot[JOINT_CHEST][1] = sinf(a->walkT) * 0.11f * a->stride;
    }

    /* ---- off-hand arm counter-swings while walking ----
       The off-hand is on leg 0's side, so it has to run half a cycle
       behind that leg to swing against it. (The browser's sin(walkT + PI)
       put it in step with its own leg, like a pacing camel.) */
    {
        float swingA = moving ? sinf(a->walkT) * 0.42f : 0.f;
        if (MODEL_HAS(m, JOINT_ARML_S) && !telegraphing) {
            p->rot[JOINT_ARML_S][0] =
                approach(p->rot[JOINT_ARML_S][0], -swingA, dt * 9.f);
            p->rot[JOINT_ARML_E][0] =
                approach(p->rot[JOINT_ARML_E][0],
                         -0.28f - maxf(0.f, swingA) * 0.5f, dt * 9.f);
        }
    }

    /* ---- cape and tail trail behind motion ---- */
    p->rot[JOINT_CAPE0][0] =
        approach(p->rot[JOINT_CAPE0][0],
                 moving ? 0.42f + sinf(a->walkT * 2.f) * 0.07f : 0.1f, dt * 5.f);
    p->rot[JOINT_TAIL][2] =
        sinf(a->walkT * 0.8f) * 0.22f * (moving ? 1.f : 0.35f);

    /* ---- head settles toward level ---- */
    if (!telegraphing)
        p->rot[JOINT_HEAD][0] =
            approach(p->rot[JOINT_HEAD][0], moving ? 0.08f : 0.f, dt * 6.f);

    if (telegraphing) return;

    /* ---- weapon arm: wind up, then swing through ---- */
    if (a->atkAnim > 0.f) {
        float t, sh, elb;
        a->atkAnim = maxf(0.f, a->atkAnim - dt);
        t = 1.f - a->atkAnim / a->atkAnimDur;
        if (MODEL_HAS(m, JOINT_ARMR_S)) {
            if (t < 0.3f) {
                float k = t / 0.3f;
                sh = -0.95f * k;
                elb = -1.35f * k;
            } else {
                float k2 = (t - 0.3f) / 0.7f;
                float e = sinf(k2 * API);
                sh = -0.95f + e * (0.95f + a->atkSwing);
                elb = -1.35f + e * 1.25f;
            }
            p->rot[JOINT_ARMR_S][0] = sh;
            p->rot[JOINT_ARMR_E][0] = minf(0.f, elb);
            p->rot[JOINT_CHEST][1] += sinf(t * API) * 0.22f;
        } else if (MODEL_HAS(m, JOINT_BODY)) {
            p->bodyZ = sinf(t * API) * 0.42f;   /* beasts lunge instead */
        }
    } else {
        if (MODEL_HAS(m, JOINT_ARMR_S)) {
            p->rot[JOINT_ARMR_S][0] =
                approach(p->rot[JOINT_ARMR_S][0], 0.f, dt * 11.f);
            p->rot[JOINT_ARMR_E][0] =
                approach(p->rot[JOINT_ARMR_E][0], -0.2f, dt * 11.f);
        }
        if (MODEL_HAS(m, JOINT_BODY))
            p->bodyZ = approach(p->bodyZ, 0.f, dt * 12.f);
    }
}
