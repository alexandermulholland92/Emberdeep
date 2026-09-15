/* Host harness: render each procedural surface through the shipped
   generator in src/texture.c and write raw RGBA, so the bytes can be
   diffed against the browser build's own output (see tests/tex_ref.js).

   Built with -DTEX_HOST_HARNESS, which compiles texture.c without its
   GL upload path so no Vita toolchain or GL context is needed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "texture.h"

static const char *kName[SURF_COUNT] = {
    "floorStone", "wallMason", "rock", "bone", "cloth",
    "leather", "metal", "wood", "skin", "hide"
};

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[512];
    int i, rc = 0;

    for (i = 0; i < SURF_COUNT; i++) {
        int S = tex_size_of(i);
        size_t bytes = (size_t)S * S * 4;
        unsigned char *px = (unsigned char *)malloc(bytes);
        FILE *f;
        if (!px) { fprintf(stderr, "out of memory\n"); return 1; }
        memset(px, 0, bytes);
        tex_gen(i, px);

        snprintf(path, sizeof path, "%s/%s.raw", dir, kName[i]);
        f = fopen(path, "wb");
        if (!f) { fprintf(stderr, "cannot write %s\n", path); free(px); return 1; }
        if (fwrite(px, 1, bytes, f) != bytes) { fprintf(stderr, "short write\n"); rc = 1; }
        fclose(f);
        printf("%-11s %dx%d  %s\n", kName[i], S, S, path);
        free(px);
    }
    return rc;
}
