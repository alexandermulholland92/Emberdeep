/* Walk-cycle guard.
 *
 * The browser's stride bent every knee the wrong way - the shin kicked
 * forward - and swung each leg forward straight, so characters looked like
 * they were walking backwards. That is one sign away from coming back, and
 * animcheck cannot see it (it only proves the port matches the reference),
 * so this checks the walk itself on every biped and quadruped:
 *
 *   - every knee folds the natural way (shin back, never forward)
 *   - the knee is bent as the leg swings through under the body, and
 *     straight by the time the foot reaches out in front to land
 *   - the planted foot stays on the floor: no hovering, no sinking
 *   - the off-hand swings against its own leg, not with it
 *   - backing away runs the cycle in reverse
 *   - stopping settles back to standing
 *
 * Needs nothing but a C compiler; part of `make test`. */
#include <math.h>
#include <stdio.h>
#include "anim.h"

static int fail;

static void check(int ok, const char *model, const char *what) {
    if (!ok) {
        printf("FAIL  %-10s %s\n", model, what);
        fail = 1;
    }
}

static int node_of(const ModelDef *m, int joint) {
    int i;
    for (i = 0; i < m->nodeCount; i++)
        if (m->nodes[i].joint == joint) return i;
    return -1;
}

static float gLow;
static void lowest(void *ctx, int surf, const float *col, const float *p,
                   const float *uv) {
    int k;
    (void)ctx; (void)surf; (void)col; (void)uv;
    for (k = 0; k < 3; k++)
        if (p[k * 3 + 1] < gLow) gLow = p[k * 3 + 1];
}

/* the swing of a limb about x, from its node's world axes */
static float pitch(const M4 *w) { return atan2f(w->m[6], w->m[5]); }

#define STEPS 480

static void walk_test(const ModelDef *m) {
    static M4 w[MODEL_MAX_NODES];
    ActorAnim a;
    const float dt = 1.f / 60.f;
    float rate = (m->kind == MKIND_QUAD) ? 11.f : 8.5f;
    float fine = 6.2831853f / rate / STEPS;       /* one stride, finely */
    float rest, floorLo = 1e9f, floorHi = -1e9f, wrongKnee = 0.f;
    float hip[STEPS], knee[STEPS], hand[STEPS], lead[STEPS];
    float mh = 0.f, ml = 0.f, sxy = 0.f, sxx = 0.f, syy = 0.f;
    float midSwing = -1.f, atReach = 1e9f, reach = 1e9f;
    int hip0 = node_of(m, JOINT_LEG_HIP(0));
    int knee0 = node_of(m, JOINT_LEG_KNEE(0)), knee1 = node_of(m, JOINT_LEG_KNEE(1));
    int handL = node_of(m, JOINT_ARML_H);
    int i, l;

    model_world(m, 0, 0, w);
    gLow = 1e9f;
    model_emit_world(m, w, -1, lowest, 0);
    rest = gLow;

    anim_init(&a, m);
    for (i = 0; i < 120; i++) anim_update(&a, dt, 1, 0, 0.f);

    for (i = 0; i < STEPS; i++) {
        anim_update(&a, fine, 1, 0, 0.f);
        model_world(m, &a.pose, 0, w);

        for (l = 0; l < m->legs; l++) {
            int h = node_of(m, JOINT_LEG_HIP(l)), k = node_of(m, JOINT_LEG_KNEE(l));
            float bend = pitch(&w[k]) - pitch(&w[h]);
            if (-bend > wrongKnee) wrongKnee = -bend;
        }
        hip[i] = pitch(&w[hip0]);
        knee[i] = pitch(&w[knee0]) - hip[i];

        gLow = 1e9f;
        model_emit_world(m, w, -1, lowest, 0);
        gLow += a.rootY - rest;
        if (gLow < floorLo) floorLo = gLow;
        if (gLow > floorHi) floorHi = gLow;

        if (handL >= 0) {
            /* the lead of the leg on the off-hand's side over the other */
            int same = (w[handL].m[12] * w[hip0].m[12] > 0.f);
            hand[i] = w[handL].m[14];
            lead[i] = same ? w[knee0].m[14] - w[knee1].m[14]
                           : w[knee1].m[14] - w[knee0].m[14];
            mh += hand[i];
            ml += lead[i];
        }
    }

    /* a negative pitch is the thigh forward of vertical: the moment it
       passes vertical on its way forward is mid-swing, and its most
       negative is the reach before the foot lands */
    for (i = 1; i < STEPS; i++) {
        if (hip[i - 1] >= 0.f && hip[i] < 0.f) midSwing = knee[i];
        if (hip[i] < reach) { reach = hip[i]; atReach = knee[i]; }
    }

    check(wrongKnee < 0.01f, m->name, "a knee bends backwards");
    check(midSwing > 0.3f, m->name,
          "the knee is not bent as the leg swings through");
    check(atReach < 0.05f, m->name,
          "the knee is still bent when the foot reaches out to land");
    check(floorLo > -0.003f && floorHi < 0.003f, m->name,
          "the planted foot leaves the floor or sinks into it");

    if (handL >= 0) {
        mh /= STEPS;
        ml /= STEPS;
        for (i = 0; i < STEPS; i++) {
            sxy += (hand[i] - mh) * (lead[i] - ml);
            sxx += (hand[i] - mh) * (hand[i] - mh);
            syy += (lead[i] - ml) * (lead[i] - ml);
        }
        check(sxy / sqrtf(sxx * syy + 1e-12f) < -0.5f, m->name,
              "the off-hand swings with its own leg");
    }

    /* backing away: the clock runs the other way */
    {
        float t0 = a.walkT;
        for (i = 0; i < 30; i++) anim_update(&a, dt, -1, 0, 0.f);
        check(a.walkT < t0, m->name, "backing away does not reverse the stride");
    }

    /* and stopping settles to standing */
    for (i = 0; i < 60; i++) anim_update(&a, dt, 0, 0, 0.f);
    for (l = 0; l < m->legs; l++)
        check(fabsf(a.pose.rot[JOINT_LEG_HIP(l)][0]) < 0.03f
              && a.pose.rot[JOINT_LEG_KNEE(l)][0] < 0.05f, m->name,
              "the legs are still mid-stride a second after stopping");
    check(fabsf(a.rootY) < 0.01f, m->name,
          "the body has not settled a second after stopping");
}

int main(void) {
    int id, tested = 0;
    for (id = 0; id < MODEL_COUNT; id++) {
        const ModelDef *m = model_get(id);
        if (!m || m->legs < 2) continue;
        if (m->kind != MKIND_BIPED && m->kind != MKIND_QUAD) continue;
        walk_test(m);
        tested++;
    }
    if (fail) printf("gait: FAILED\n");
    else printf("gait: %d walking rigs checked, passed\n", tested);
    return fail;
}
