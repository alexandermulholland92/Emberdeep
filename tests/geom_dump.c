/* Host harness: tessellate geometry specs through src/geom.c and write the
   result as text, so it can be diffed against the vertices Three.js itself
   produces (tests/model_ref.js --geom). See `make geomcheck`.

   Reads one spec per line on stdin:
       <kind> <p0> <p1> ... <p7>
   where kind is box|cylinder|cone|sphere|torus|plane|icosa.
   Writes, per spec:
       # <verts> <indices>
       v <x> <y> <z> <u> <v>     (one per vertex)
       i <a> <b> <c>             (one per triangle) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "geom.h"

static int kind_of(const char *s) {
    if (!strcmp(s, "box")) return GEO_BOX;
    if (!strcmp(s, "cylinder")) return GEO_CYLINDER;
    if (!strcmp(s, "cone")) return GEO_CONE;
    if (!strcmp(s, "sphere")) return GEO_SPHERE;
    if (!strcmp(s, "torus")) return GEO_TORUS;
    if (!strcmp(s, "plane")) return GEO_PLANE;
    if (!strcmp(s, "icosa")) return GEO_ICOSA;
    return -1;
}

int main(void) {
    char line[512];
    while (fgets(line, sizeof line, stdin)) {
        char name[32];
        GeoSpec g;
        float *pos, *uv;
        unsigned short *idx;
        int maxV = 0, maxI = 0, nv = 0, ni = 0, i, k;
        int n = sscanf(line, "%31s %f %f %f %f %f %f %f %f", name,
                       &g.p[0], &g.p[1], &g.p[2], &g.p[3],
                       &g.p[4], &g.p[5], &g.p[6], &g.p[7]);
        if (n < 1) continue;
        for (k = n - 1; k < GEO_MAX_PARAMS; k++) g.p[k] = 0.f;
        k = kind_of(name);
        if (k < 0) { fprintf(stderr, "unknown geometry '%s'\n", name); return 1; }
        g.kind = (unsigned char)k;

        geo_bounds(&g, &maxV, &maxI);
        pos = (float *)malloc(sizeof(float) * 3 * (size_t)(maxV + 1));
        uv  = (float *)malloc(sizeof(float) * 2 * (size_t)(maxV + 1));
        idx = (unsigned short *)malloc(sizeof(unsigned short) * (size_t)(maxI + 3));
        if (!pos || !uv || !idx) { fprintf(stderr, "out of memory\n"); return 1; }

        if (!geo_build(&g, pos, uv, idx, maxV, maxI, &nv, &ni)) {
            fprintf(stderr, "geo_build failed for %s\n", line);
            return 1;
        }
        printf("# %d %d\n", nv, ni);
        for (i = 0; i < nv; i++)
            printf("v %.6f %.6f %.6f %.6f %.6f\n",
                   pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2],
                   uv[i * 2], uv[i * 2 + 1]);
        for (i = 0; i < ni; i += 3)
            printf("i %d %d %d\n", idx[i], idx[i + 1], idx[i + 2]);
        free(pos); free(uv); free(idx);
    }
    return 0;
}
