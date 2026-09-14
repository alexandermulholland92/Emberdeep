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
    a->started = 0;
    for (i = 0; i < MODEL_MAX_LEGS; i++)
        a->legRestZ[i] = model ? a->pose.rot[JOINT_LEG_HIP(i)][2] : 0.f;
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
        /* biped / quadruped stride: hips swing, knees only fold one way */
        float rate = (m->kind == MKIND_QUAD) ? 11.f : 8.5f;
        a->walkT += dt * (moving ? rate : 2.2f);
        for (i = 0; i < legs; i++) {
            float phase = (m->kind == MKIND_QUAD)
                ? a->walkT + ((i == 0 || i == 3) ? 0.f : API)   /* diagonal gait */
                : a->walkT + (i ? API : 0.f);
            p->rot[JOINT_LEG_HIP(i)][0] = sinf(phase) * 0.55f * amp;
            p->rot[JOINT_LEG_KNEE(i)][0] =
                -maxf(0.f, sinf(phase - 0.7f)) * 0.85f * amp;
        }
        a->rootY = baseY + (moving ? fabsf(sinf(a->walkT)) * 0.045f
                                   : sinf(a->walkT) * 0.008f);
        p->rot[JOINT_CHEST][1] = sinf(a->walkT) * 0.11f * amp;
    }

    /* ---- off-hand arm counter-swings while walking ---- */
    {
        float swingA = moving ? sinf(a->walkT + API) * 0.42f : 0.f;
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
