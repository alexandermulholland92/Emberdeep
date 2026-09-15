/* Host harness: compute every model node's world matrix through
   src/model.c and print it, so it can be diffed against the matrixWorld
   values Three.js computed for the same tree (tests/model_ref.js).
   Also reports each model's triangle count. See `make modelcheck`. */
#include <stdio.h>
#include <stdlib.h>
#include "model.h"

static long gTris;
static void count_tri(void *ctx, int surf, const float *col,
                      const float *pos, const float *uv) {
    (void)ctx; (void)surf; (void)col; (void)pos; (void)uv;
    gTris++;
}

int main(void) {
    int i, j, k;
    for (i = 0; i < MODEL_COUNT; i++) {
        const ModelDef *m = model_get(i);
        M4 *w = (M4 *)malloc(sizeof(M4) * (size_t)m->nodeCount);
        if (!w) { fprintf(stderr, "out of memory\n"); return 1; }
        model_world(m, 0, 0, w);           /* rest pose, identity root */

        gTris = 0;
        model_emit(m, 0, 0, count_tri, 0);

        printf("model %s %d %ld\n", m->name, m->nodeCount, gTris);
        for (j = 0; j < m->nodeCount; j++) {
            printf("n %d", j);
            for (k = 0; k < 16; k++) printf(" %.6f", w[j].m[k]);
            printf("\n");
        }
        free(w);
    }
    return 0;
}
