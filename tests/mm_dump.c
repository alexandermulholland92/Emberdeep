/* Host harness: rasterise the minimap through the shipped src/minimap.c
   and write it as raw RGBA, so the panel can be looked at without a Vita.

   Composites the same three layers render.c does - border, wrap backing,
   then mm_draw - at the exact size the HUD uses, so what comes out is
   what the device draws.

       tests/mm_dump <out.raw> [explored-rooms]

   with explored-rooms 0 meaning "reveal the whole floor". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "minimap.h"

#define PAD  12                     /* margin around the panel in the image */
#define MM   96
#define IMG  (MM + PAD * 2)

static unsigned char gImg[IMG * IMG * 4];

/* the simulation's RNG lives in main.c, which needs the Vita SDK */
static unsigned int sRng = 20260924u;
static unsigned int xr(void) {
    sRng ^= sRng << 13; sRng ^= sRng >> 17; sRng ^= sRng << 5;
    return sRng;
}
float rnd01(void)            { return (float)(xr() & 0xFFFFFF) / (float)0x1000000; }
float rndr(float a, float b) { return a + rnd01() * (b - a); }
int   rndi(int a, int b)     { return a + (int)(rnd01() * (float)(b - a + 1)); }

Game G;

/* src-over, matching what the HUD's blend func does on device */
static void blend(void *ctx, float x, float y, float w, float h,
                  float r, float g, float b, float a) {
    int px, py;
    (void)ctx;
    for (py = (int)y; py < (int)(y + h + 0.999f); py++) {
        for (px = (int)x; px < (int)(x + w + 0.999f); px++) {
            unsigned char *d;
            if (px < 0 || py < 0 || px >= IMG || py >= IMG) continue;
            d = &gImg[(py * IMG + px) * 4];
            d[0] = (unsigned char)(r * 255.f * a + d[0] * (1.f - a));
            d[1] = (unsigned char)(g * 255.f * a + d[1] * (1.f - a));
            d[2] = (unsigned char)(b * 255.f * a + d[2] * (1.f - a));
            d[3] = 255;
        }
    }
}

/* walk from one room to the next the way the player would, so the map
   shows a believable trail of explored corridor rather than whole rooms */
static void walk_between(const Room *a, const Room *b) {
    V3 from = dg_room_center(a), to = dg_room_center(b);
    int step, steps = 90;
    for (step = 0; step <= steps; step++) {
        float t = (float)step / steps;
        dg_mark_seen(from.x + (to.x - from.x) * t,
                     from.z + (to.z - from.z) * t, 3);
    }
}

int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "mm.raw";
    int rooms = argc > 2 ? atoi(argv[2]) : 3;
    FILE *f;
    int i;

    /* a dark ground so the panel's own alpha is visible */
    for (i = 0; i < IMG * IMG; i++) {
        gImg[i * 4 + 0] = 26; gImg[i * 4 + 1] = 22;
        gImg[i * 4 + 2] = 30; gImg[i * 4 + 3] = 255;
    }

    dg_generate(0);

    if (rooms <= 0 || rooms > gRoomCount) rooms = gRoomCount;
    for (i = 0; i < rooms; i++) {
        dg_reveal_room(&gRooms[i]);
        if (i > 0) walk_between(&gRooms[i - 1], &gRooms[i]);
    }

    /* player in the last room reached */
    G.pl.pos = dg_room_center(&gRooms[rooms - 1]);
    G.pl.alive = 1;

    /* a handful of enemies scattered through the explored rooms, one boss */
    for (i = 0; i < rooms && i < MAX_ENEMY; i++) {
        V3 c = dg_room_center(&gRooms[i]);
        G.en[i].active = 1;
        G.en[i].alive = 1;
        G.en[i].pos = c;
        G.en[i].pos.x += 1.5f;
        G.en[i].isBoss = (gRooms[i].kind == ROOM_BOSS);
    }

    /* and the way down, once the floor is cleared */
    if (rooms == gRoomCount) {
        G.portalOn = 1;
        /* in the first room, well away from the player, so the marker is
           not hidden under them when eyeballing the output */
        G.portalPos = dg_room_center(&gRooms[0]);
    }

    /* exactly what render.c's draw_minimap() composites */
    blend(0, PAD - 1.f, PAD - 1.f, MM + 2.f, MM + 2.f,
          0.784f, 0.698f, 0.502f, 0.35f);
    blend(0, PAD, PAD, MM, MM, 0.024f, 0.020f, 0.039f, 0.62f);
    mm_draw((float)PAD, (float)PAD, (float)MM, blend, 0);

    f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", out); return 1; }
    fwrite(gImg, 1, sizeof gImg, f);
    fclose(f);
    printf("%s  %dx%d  %d of %d rooms explored\n", out, IMG, IMG,
           rooms, gRoomCount);
    return 0;
}
