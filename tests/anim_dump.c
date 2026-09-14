/* Host harness: run the ported animateActor over the same fixed script the
   reference uses (tests/model_ref.js --anim) and print the resulting world
   matrices, so the two can be diffed frame-exactly. See `make animcheck`.

   The script is 90 frames at 1/60s: moving for the first 60, idle after,
   with a weapon swing triggered on frame 20. */
#include <stdio.h>
#include <stdlib.h>
#include "anim.h"

int main(void) {
    int i, f, j, k;
    for (i = 0; i < MODEL_COUNT; i++) {
        const ModelDef *m = model_get(i);
        ActorAnim a;
        M4 root, *w;
        float pos[3], rot[3] = { 0.f, 0.f, 0.f }, sc[3] = { 1.f, 1.f, 1.f };
        const float dt = 1.f / 60.f;

        anim_init(&a, m);
        for (f = 0; f < 90; f++) {
            if (f == 20) anim_swing(&a, 1.4f, 0.3f);
            anim_update(&a, dt, f < 60, 0, 0.f);
        }

        /* the browser writes the bob onto the actor's position.y, so the
           root matrix has to carry it for the comparison to line up */
        pos[0] = 0.f; pos[1] = a.rootY; pos[2] = 0.f;
        m4_compose(pos, rot, sc, &root);

        w = (M4 *)malloc(sizeof(M4) * (size_t)m->nodeCount);
        if (!w) { fprintf(stderr, "out of memory\n"); return 1; }
        model_world(m, &a.pose, &root, w);

        printf("model %s %d %.6f %.6f %.6f\n", m->name, m->nodeCount,
               a.walkT, a.floatT, a.rootY);
        for (j = 0; j < m->nodeCount; j++) {
            printf("n %d", j);
            for (k = 0; k < 16; k++) printf(" %.6f", w[j].m[k]);
            printf("\n");
        }
        free(w);
    }
    return 0;
}
