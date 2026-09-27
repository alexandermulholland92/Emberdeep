/* Weapon guard.
 *
 * The browser carried every weapon pointing up the forearm and back past
 * the shoulder, and its attack swung the arm backwards; a chop in the
 * rigs' plane also led with the flat of the blade. animcheck cannot see
 * any of that (it only proves the port matches the reference), so this
 * checks the weapon itself, on every rig that holds one:
 *
 *   - it is carried pointing up and ahead
 *   - winding up takes it back over the shoulder
 *   - the chop carries the tip forward and ends with it pointing ahead,
 *     the arm out in front
 *   - the edge (or axe head) leads the chop
 *   - a boss's wind-up hauls it back too
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

/* the weapon: a group hung from the right hand */
static int weapon_of(const ModelDef *m) {
    int i, hand = -1;
    for (i = 0; i < m->nodeCount; i++)
        if (m->nodes[i].joint == JOINT_ARMR_H) hand = i;
    for (i = 0; hand >= 0 && i < m->nodeCount; i++)
        if (m->nodes[i].parent == hand && m->nodes[i].geom < 0) return i;
    return -1;
}

/* world direction of the weapon's shaft (its +y) and of its edge (its
   geometry's +x, which the mount turns), and where a point up the shaft is */
static void weapon_at(const ModelDef *m, const ActorAnim *a, int node,
                      float shaft[3], float edge[3], float tip[3]) {
    static M4 w[MODEL_MAX_NODES];
    const float up[3] = { 0.f, 0.5f, 0.f };
    model_world(m, &a->pose, 0, w);
    shaft[0] = w[node].m[4]; shaft[1] = w[node].m[5]; shaft[2] = w[node].m[6];
    edge[0] = w[node].m[0];  edge[1] = w[node].m[1];  edge[2] = w[node].m[2];
    m4_apply(&w[node], up, tip);
}

static void run(ActorAnim *a, int frames, int telegraphing) {
    int i;
    for (i = 0; i < frames; i++)
        anim_update(a, 1.f / 240.f, 0, telegraphing, 0.f);
}

static void swing_test(const ModelDef *m, int node) {
    ActorAnim a;
    float shaft[3], edge[3], tip[3], prevTip[3];
    float lead = 0.f, travel = 0.f;
    int i;

    anim_init(&a, m);
    run(&a, 480, 0);
    weapon_at(m, &a, node, shaft, edge, tip);
    check(shaft[1] > 0.3f && shaft[2] > 0.3f, m->name,
          "the weapon is not carried pointing up and ahead");

    /* 0.3 s swing at 240 Hz: 72 steps, the wind-up peaking at step 21 */
    anim_swing(&a, 1.4f, 0.3f);
    run(&a, 21, 0);
    weapon_at(m, &a, node, shaft, edge, tip);
    check(shaft[2] < -0.3f, m->name,
          "winding up does not take the weapon back over the shoulder");

    for (i = 0; i < 51; i++) {
        prevTip[0] = tip[0]; prevTip[1] = tip[1]; prevTip[2] = tip[2];
        run(&a, 1, 0);
        weapon_at(m, &a, node, shaft, edge, tip);
        {
            float v[3] = { tip[0] - prevTip[0], tip[1] - prevTip[1],
                           tip[2] - prevTip[2] };
            float speed = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            if (speed > 1e-5f) {
                lead += (edge[0] * v[0] + edge[1] * v[1] + edge[2] * v[2]);
                travel += speed;
            }
        }
    }
    check(shaft[2] > 0.8f, m->name,
          "the chop does not end with the weapon pointing ahead");
    check(a.pose.rot[JOINT_ARMR_S][0] < -0.3f, m->name,
          "the chop does not end with the arm out in front");
    /* edge-first scores 0.77-0.94 (the chest's twist carries a big rig's
       arm a little sideways); flat-first scores about 0.02 */
    check(lead > 0.6f * travel, m->name,
          "the chop leads with the flat of the weapon, not its edge");

    /* a boss's wind-up, held for a second */
    anim_init(&a, m);
    run(&a, 240, 0);
    run(&a, 240, 1);
    weapon_at(m, &a, node, shaft, edge, tip);
    check(shaft[2] < -0.3f, m->name,
          "a boss's wind-up does not haul the weapon back");
}

int main(void) {
    int id, tested = 0;
    for (id = 0; id < MODEL_COUNT; id++) {
        const ModelDef *m = model_get(id);
        int node = m ? weapon_of(m) : -1;
        if (node < 0) continue;
        swing_test(m, node);
        tested++;
    }
    if (fail) printf("swing: FAILED\n");
    else printf("swing: %d armed rigs checked, passed\n", tested);
    return fail;
}
