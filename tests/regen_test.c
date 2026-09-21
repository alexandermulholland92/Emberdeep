/* Regression guard for the stale-world bug.
 *
 * The renderer caches the static world mesh and rebuilds it only when its
 * cache key changes. That key used to be G.depth, which is wrong: every
 * dg_generate() randomises a fresh layout, including for a depth just
 * visited. Starting a new run after dying on Floor I regenerates at depth
 * 0 while the cached mesh was also built at depth 0, so the walls stayed
 * from the previous run while collision used the new grid - paths that
 * ran straight into walls.
 *
 * These checks pin the two facts the fix relies on: regenerating bumps
 * gDungeonGen, and the same depth does not mean the same layout. */
#include <stdio.h>
#include <string.h>
#include "game.h"

/* the simulation's RNG lives in main.c, which needs the Vita SDK */
static unsigned int sRng = 20240921u;
static unsigned int xr(void) {
    sRng ^= sRng << 13; sRng ^= sRng >> 17; sRng ^= sRng << 5;
    return sRng;
}
float rnd01(void)            { return (float)(xr() & 0xFFFFFF) / (float)0x1000000; }
float rndr(float a, float b) { return a + rnd01() * (b - a); }
int   rndi(int a, int b)     { return a + (int)(rnd01() * (float)(b - a + 1)); }

Game G;

int main(void) {
    unsigned char first[GRID][GRID];
    unsigned int gen0, gen1;
    int i, differing = 0, fail = 0;

    /* 1. every regenerate advances the counter the renderer keys on */
    dg_generate(0);
    gen0 = gDungeonGen;
    memcpy(first, gGrid, sizeof first);

    dg_generate(0);
    gen1 = gDungeonGen;

    if (gen1 == gen0) {
        printf("FAIL  dg_generate() did not advance gDungeonGen (%u -> %u)\n",
               gen0, gen1);
        fail = 1;
    } else {
        printf("ok    gDungeonGen advances on regenerate (%u -> %u)\n", gen0, gen1);
    }

    /* 2. the same depth does not imply the same layout, which is exactly
          why depth cannot serve as the cache key */
    if (memcmp(first, gGrid, sizeof first) != 0) differing++;
    for (i = 0; i < 6; i++) {
        dg_generate(0);
        if (memcmp(first, gGrid, sizeof first) != 0) differing++;
    }
    if (differing == 0) {
        printf("FAIL  seven layouts at depth 0 were all identical\n");
        fail = 1;
    } else {
        printf("ok    depth 0 regenerates a different layout (%d of 7 differ)\n",
               differing);
    }

    /* 3. and the counter keeps moving across a floor change and back,
          the sequence a second playthrough actually walks */
    gen0 = gDungeonGen;
    dg_generate(1);
    dg_generate(0);
    if (gDungeonGen != gen0 + 2) {
        printf("FAIL  counter skipped a regenerate (%u -> %u, wanted %u)\n",
               gen0, gDungeonGen, gen0 + 2);
        fail = 1;
    } else {
        printf("ok    counter tracks every regenerate across floors\n");
    }

    /* 4. the bug itself, as a decision table: walk the sequence a second
          playthrough takes - start Floor I, die, start Floor I again - and
          ask each candidate cache key whether it would rebuild the mesh.
          The old key (depth) misses the restart; the new one catches it. */
    {
        const int depths[] = { 0, 0, 1, 2, 0 };   /* run 1, restart, descend, restart */
        const char *what[] = { "first run, Floor I", "restart after dying on I",
                               "descend to II", "descend to III",
                               "restart after winning" };
        int lastDepth = -1, missedByDepth = 0, missedByGen = 0;
        unsigned int lastGen = 0;
        size_t n;

        for (n = 0; n < sizeof depths / sizeof depths[0]; n++) {
            unsigned char before[GRID][GRID];
            int depthWouldRebuild, genWouldRebuild, layoutChanged;

            memcpy(before, gGrid, sizeof before);
            dg_generate(depths[n]);
            layoutChanged = memcmp(before, gGrid, sizeof before) != 0;

            depthWouldRebuild = (lastDepth != depths[n]);
            genWouldRebuild   = (lastGen != gDungeonGen);

            if (layoutChanged && !depthWouldRebuild) {
                missedByDepth++;
                printf("      depth key goes stale: %s\n", what[n]);
            }
            if (layoutChanged && !genWouldRebuild) missedByGen++;

            lastDepth = depths[n];
            lastGen = gDungeonGen;
        }

        if (missedByDepth == 0) {
            printf("FAIL  the depth key never went stale - this test proves nothing\n");
            fail = 1;
        } else {
            printf("ok    depth key would have missed %d rebuild(s) - the reported bug\n",
                   missedByDepth);
        }
        if (missedByGen != 0) {
            printf("FAIL  generation key missed %d rebuild(s)\n", missedByGen);
            fail = 1;
        } else {
            printf("ok    generation key rebuilds on every layout change\n");
        }
    }

    printf(fail ? "regen: FAILED\n" : "regen: passed\n");
    return fail;
}
