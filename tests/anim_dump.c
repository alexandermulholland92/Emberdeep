/* Host harness: run the ported animateActor over the same fixed script the
   reference uses (tests/model_ref.js --anim) and print the resulting world
   matrices, so the two can be diffed frame-exactly. See `make animcheck`.

   The script is 90 frames at 1/60s: walking forward at 2 units a second
   for the first 60, idle after, with a weapon swing triggered on frame 20,
   then a boss-style wind-up over frames 62-75 and the swing it leads into
   on frame 76 (still under way when the script ends).
   The browser reads the speed off the actor's own motion, so it only sees
   it from the second frame; the speed here follows suit. */
#include <stdio.h>
#include <stdlib.h>
#include "anim.h"

/* the frames tests/model_ref.js snapshots */
static const int kSnap[] = { 10, 25, 45, 59, 70, 79, 89 };

int main(void) {
    int i, f, j, k, s;
    for (i = 0; i < MODEL_COUNT; i++) {
        const ModelDef *m = model_get(i);
        ActorAnim a;
        M4 root, *w;
        float pos[3], rot[3] = { 0.f, 0.f, 0.f }, sc[3] = { 1.f, 1.f, 1.f };
        const float dt = 1.f / 60.f, speed = 2.f;
        float z = 0.f;

        anim_init(&a, m);
        w = (M4 *)malloc(sizeof(M4) * (size_t)m->nodeCount);
        if (!w) { fprintf(stderr, "out of memory\n"); return 1; }
        for (f = 0; f < 90; f++) {
            if (f == 20 || f == 76) anim_swing(&a, 1.4f, 0.3f);
            if (f < 60) z += speed * dt;
            a.speed = (f > 0 && f < 60) ? speed : 0.f;
            anim_update(&a, dt, f < 60, f >= 62 && f < 76, 0.f);

            for (s = 0; s < (int)(sizeof kSnap / sizeof kSnap[0]); s++)
                if (kSnap[s] == f) break;
            if (s == (int)(sizeof kSnap / sizeof kSnap[0])) continue;

            /* the browser writes the bob onto the actor's position.y, so
               the root matrix has to carry it for the comparison to line up */
            pos[0] = 0.f; pos[1] = a.rootY; pos[2] = z;
            m4_compose(pos, rot, sc, &root);
            model_world(m, &a.pose, &root, w);

            printf("model %s@%d %d %.6f %.6f %.6f\n", m->name, f, m->nodeCount,
                   a.walkT, a.floatT, a.rootY);
            for (j = 0; j < m->nodeCount; j++) {
                printf("n %d", j);
                for (k = 0; k < 16; k++) printf(" %.6f", w[j].m[k]);
                printf("\n");
            }
        }
        free(w);
    }
    return 0;
}
