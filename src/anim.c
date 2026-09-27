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

/* the stride's shape, shared by the walk and the measurement of it */
#define HIP_SWING  0.55f   /* thigh either side of vertical               */
#define KNEE_FOLD  0.85f   /* knee at its most bent                        */
#define KNEE_LEAD  0.3f    /* how far ahead of the hip the knee runs       */
#define TOE_DOWN   0.35f   /* foot tipped toe-down as it leaves the floor  */
#define TOE_UP     0.4f    /* and toe-up as it comes down, heel first      */

/* The thigh's swing through one cycle, -1 (forward) .. 1 (back). While
   the foot is planted - from the front of the stride round to the back -
   the thigh sweeps back at an even rate, so the foot travels back under
   the body at the same steady speed the body moves over it. The swing
   forward eases out and in. */
static float hip_wave(float phase) {
    float u = phase - 6.2831853f * floorf(phase / 6.2831853f);
    if (u < 0.5f * API) return u / (0.5f * API);
    if (u > 1.5f * API) return (u - 2.f * API) / (0.5f * API);
    return sinf(u);
}

/* One leg at `phase`: the hip, the knee, and the foot's pitch against
   the floor (positive tips the toe down).

   The knee folds positive - shin back, heel up, the way a knee bends -
   and only while the leg swings through, on a real walk's timing: it
   starts just before the foot leaves the floor, is most bent early in the
   swing, and is straight again before the foot reaches out to land.

   The foot stays flat while it is planted. It trails toe-down for the
   first half of the swing, is level again by mid-swing so the toe clears
   the floor, and comes in toe-up to land on its heel before settling
   flat. That heel-and-toe timing is also what lets one foot take the
   weight exactly as the other lets it go. */
static void stride_pose(float phase, float stride,
                        float *hip, float *knee, float *tilt) {
    float u = phase - 6.2831853f * floorf(phase / 6.2831853f);
    float down = (u > 0.5f * API && u < API)
               ? sinf(2.f * (u - 0.5f * API)) : 0.f;
    float up = fabsf(u - 1.5f * API) < 0.25f * API
             ? cosf(2.f * (u - 1.5f * API)) : 0.f;
    *hip = hip_wave(phase) * HIP_SWING * stride;
    *knee = maxf(0.f, -cosf(phase + KNEE_LEAD)) * KNEE_FOLD * stride;
    *tilt = (down * TOE_DOWN - up * TOE_UP) * stride;
}

/* A foot's lowest corner for a leg posed that way: how far it has risen
   off the floor, and where it is along z relative to the hip. The foot
   box turns about its own centre, which hangs below the knee. */
static float foot_contact(const ActorAnim *a, float hip, float knee,
                          float tilt, float *z) {
    float sa = sinf(hip + knee), ca = cosf(hip + knee);
    float sb = sinf(tilt), cb = cosf(tilt);
    float corner = (a->toe * sb > a->heel * sb) ? a->toe : a->heel;
    float y = -a->thigh * cosf(hip) - a->footY * ca - a->footZ * sa
            - a->half * cb - corner * sb;
    if (z) *z = -a->thigh * sinf(hip) - a->footY * sa + a->footZ * ca
              - a->half * sb + corner * cb;
    return y + a->thigh + a->footY + a->half;
}

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
    a->thigh = a->footY = a->footZ = a->half = a->heel = a->toe = 0.f;
    a->cycle = 0.f;
    a->speed = a->pace = 0.f;
    a->scale = 1.f;
    a->started = 0;
    for (i = 0; i < MODEL_MAX_LEGS; i++)
        a->legRestZ[i] = model ? a->pose.rot[JOINT_LEG_HIP(i)][2] : 0.f;

    /* Every stepping rig's legs are alike, so the first one is measured:
       the thigh, and the foot box hung below the knee. */
    if (model && model->legs > 0 && MODEL_HAS(model, JOINT_LEG_FOOT(0))) {
        int hip = -1, knee = -1, foot = -1;
        for (i = 0; i < model->nodeCount; i++) {
            if (model->nodes[i].joint == JOINT_LEG_HIP(0)) hip = i;
            if (model->nodes[i].joint == JOINT_LEG_KNEE(0)) knee = i;
            if (model->nodes[i].joint == JOINT_LEG_FOOT(0)) foot = i;
        }
        if (hip >= 0 && knee >= 0 && foot >= 0
            && model->nodes[knee].parent == hip
            && model->nodes[foot].parent == knee) {
            const ModelNode *n = &model->nodes[foot];
            a->thigh = -model->nodes[knee].pos[1];
            a->footY = -n->pos[1];
            a->footZ = n->pos[2];
            a->half = n->gp[1] * 0.5f * n->scale[1];
            a->heel = -n->gp[2] * 0.5f * n->scale[2];
            a->toe = n->gp[2] * 0.5f * n->scale[2];
        }
    }

    /* Then walk it once, on paper: through a full cycle, whichever corner
       is lowest is the one on the floor, and the body has to travel by
       however far that corner slides back relative to the hip for it to
       stay put. The total is the ground one cycle covers, which is what
       lets the stride keep pace with the actor instead of skating. */
    if (a->half > 0.f) {
        const int n = 128;
        float prevZ = 0.f;
        int prevCorner = -1;
        for (i = 0; i <= n; i++) {
            float phase = 6.2831853f * i / n, best = 1e9f, zz = 0.f;
            int leg, corner = -1;
            for (leg = 0; leg < 2; leg++) {
                float hip, knee, tilt, z, lift;
                stride_pose(phase + leg * API, 1.f, &hip, &knee, &tilt);
                lift = foot_contact(a, hip, knee, tilt, &z);
                if (lift < best) {
                    best = lift;
                    zz = z;
                    corner = leg * 2 + (a->toe * sinf(tilt) > a->heel * sinf(tilt));
                }
            }
            if (corner == prevCorner && prevZ > zz) a->cycle += prevZ - zz;
            prevCorner = corner;
            prevZ = zz;
        }
    }
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
        /* legs trail back as it flies, knees folded the way knees fold
           (the browser's -0.45 bent them backwards) */
        for (i = 0; i < legs; i++) {
            p->rot[JOINT_LEG_HIP(i)][0] = 0.3f + sinf(a->floatT + i) * 0.12f;
            p->rot[JOINT_LEG_KNEE(i)][0] = 0.45f;
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
        /* biped / quadruped stride (see stride_pose). Rigs face +z and a
           positive x rotation swings a limb's far end backwards, so a leg
           swings forward while cos(phase) < 0 and is planted while
           cos(phase) > 0. (The browser folded the knee negative, which
           kicks the shin forward like a knee bending backwards, and swung
           the leg forward straight - together they read as walking
           backwards.)

           The body settles onto whichever foot is lowest, so it dips as
           the legs spread and rides highest over a straight planted leg,
           instead of lifting the feet off the floor mid-step. */
        float rate = (m->kind == MKIND_QUAD) ? 11.f : 8.5f;
        float lock = 1e9f, walking;
        a->stride = approach(a->stride, moving ? 1.f : 0.05f, dt * 12.f);

        /* Step as fast as the ground goes by: with the actor's speed known,
           one cycle has to cover exactly the ground it moves, or the
           planted foot skates. (A shorter stride while it is still
           getting going covers proportionally less.) Without a speed, the
           browser's fixed cadence. */
        a->pace = approach(a->pace, moving ? a->speed : 0.f, dt * 10.f);
        if (moving && a->pace > 0.f && a->cycle > 0.f)
            rate = a->pace * 6.2831853f
                 / (a->cycle * a->scale * maxf(a->stride, 0.25f));
        rate = minf(maxf(rate, 2.2f), 24.f);
        a->walkT += dt * (moving ? rate : 2.2f) * (moving < 0 ? -1.f : 1.f);

        for (i = 0; i < legs; i++) {
            float phase = (m->kind == MKIND_QUAD)
                ? a->walkT + ((i == 0 || i == 3) ? 0.f : API)   /* diagonal gait */
                : a->walkT + (i ? API : 0.f);
            float hip, knee, tilt;
            stride_pose(phase, a->stride, &hip, &knee, &tilt);
            p->rot[JOINT_LEG_HIP(i)][0] = hip;
            p->rot[JOINT_LEG_KNEE(i)][0] = knee;
            /* the ankle undoes the leg's slope, so a planted foot is flat */
            p->rot[JOINT_LEG_FOOT(i)][0] = tilt - hip - knee;
            lock = minf(lock, foot_contact(a, hip, knee, tilt, 0));
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
