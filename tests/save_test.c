/* Round-trip guard for the save format.
 *
 * sv_pack/sv_unpack are kept free of the Vita's file I/O so the format
 * can be exercised here: pack a run, scribble over the live state, unpack,
 * and check every field came back. Also checks the header actually
 * refuses a save it should not trust, since the snapshot is a raw image
 * of the live structs and reading a stale one would be silent corruption.
 *
 * Built with -DSV_HOST_HARNESS, which compiles save.c without its ux0:
 * path so no Vita SDK is needed. */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "save.h"

/* the simulation's RNG lives in main.c, which needs the Vita SDK */
static unsigned int sRng = 20260924u;
static unsigned int xr(void) {
    sRng ^= sRng << 13; sRng ^= sRng >> 17; sRng ^= sRng << 5;
    return sRng;
}
float rnd01(void)            { return (float)(xr() & 0xFFFFFF) / (float)0x1000000; }
float rndr(float a, float b) { return a + rnd01() * (b - a); }
int   rndi(int a, int b)     { return a + (int)(rnd01() * (float)(b - a + 1)); }

/* G itself lives in actors.c, which this harness links */

static unsigned char sBuf[SV_MAX_BYTES];
static int fail;

static void check(int ok, const char *what) {
    printf("%-5s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) fail = 1;
}

int main(void) {
    Game before;
    unsigned char gridBefore[GRID][GRID], seenBefore[GRID][GRID];
    Room roomsBefore[MAX_ROOMS];
    int roomCountBefore, timerBytes = 0, n, i;
    void *timers = ac_timer_state(&timerBytes);
    unsigned char timersBefore[4096];
    unsigned int genBefore;

    /* ---- build a run worth saving ---- */
    dg_generate(1);
    dg_reveal_room(&gRooms[0]);
    G.depth = 1;
    G.kills = 37;
    G.elapsed = 123.5f;
    G.portalOn = 1;
    G.portalPos = dg_room_center(&gRooms[0]);
    G.pl.alive = 1;
    G.pl.cls = CLS_PYRO;
    G.pl.level = 5;
    G.pl.hp = 42.5f;
    G.pl.hpMax = 90.f;
    G.pl.pos = dg_room_center(&gRooms[0]);
    for (i = 0; i < 6; i++) {
        G.en[i].active = 1;
        G.en[i].alive = 1;
        G.en[i].type = E_SKELETON;
        G.en[i].hp = 12.f + i;
        G.en[i].pos = dg_room_center(&gRooms[0]);
        G.en[i].pos.x += i;
    }
    memset(timers, 0xA5, (size_t)timerBytes);   /* pending deferred effects */

    /* these must never ride along: they point into this build's strings */
    G.bannerText = "THE OSSUARY";
    G.toastText = "LEVEL 5";
    G.pauseNote = "RUN SAVED";
    G.state = ST_PAUSE;

    before = G;
    memcpy(gridBefore, gGrid, sizeof gridBefore);
    memcpy(seenBefore, gSeen, sizeof seenBefore);
    memcpy(roomsBefore, gRooms, sizeof roomsBefore);
    memcpy(timersBefore, timers, (size_t)timerBytes);
    roomCountBefore = gRoomCount;
    genBefore = gDungeonGen;

    /* ---- pack ---- */
    n = sv_pack(sBuf, (int)sizeof sBuf);
    check(n > 0, "sv_pack produced a snapshot");
    if (n <= 0) return 1;
    printf("      %d bytes\n", n);

    check(sv_pack(sBuf, 8) == 0, "sv_pack refuses a buffer that is too small");

    /* ---- lose the live state completely ---- */
    memset(&G, 0, sizeof G);
    memset(gGrid, 0, sizeof gGrid);
    memset(gSeen, 0, sizeof gSeen);
    memset(gRooms, 0, sizeof gRooms);
    memset(timers, 0, (size_t)timerBytes);
    gRoomCount = 0;
    dg_generate(0);            /* and generate a different floor over the top */

    /* ---- unpack ---- */
    check(sv_unpack(sBuf, n) == 1, "sv_unpack accepted its own snapshot");

    check(G.depth == before.depth && G.kills == before.kills
          && G.elapsed == before.elapsed, "run counters restored");
    check(G.pl.cls == before.pl.cls && G.pl.level == before.pl.level
          && G.pl.hp == before.pl.hp && G.pl.hpMax == before.pl.hpMax,
          "player restored");
    check(G.pl.pos.x == before.pl.pos.x && G.pl.pos.z == before.pl.pos.z,
          "player position restored");
    check(memcmp(G.en, before.en, sizeof G.en) == 0, "every enemy restored");
    check(G.portalOn == before.portalOn
          && G.portalPos.x == before.portalPos.x, "portal restored");
    check(memcmp(gGrid, gridBefore, sizeof gridBefore) == 0, "layout restored");
    check(memcmp(gSeen, seenBefore, sizeof seenBefore) == 0,
          "explored map restored");
    check(memcmp(gRooms, roomsBefore, sizeof roomsBefore) == 0
          && gRoomCount == roomCountBefore, "rooms restored");
    check(memcmp(timers, timersBefore, (size_t)timerBytes) == 0,
          "deferred effects restored");

    check(G.bannerText == 0 && G.toastText == 0 && G.pauseNote == 0,
          "string pointers dropped rather than restored");
    check(G.state == ST_PLAY, "a run saved while paused resumes unpaused");
    check(gDungeonGen != genBefore,
          "world mesh marked for rebuild after a load");

    /* ---- and the header has to be worth something ---- */
    {
        unsigned char bad[SV_MAX_BYTES];
        memcpy(bad, sBuf, (size_t)n);
        bad[0] ^= 0xFF;                                  /* magic */
        check(sv_unpack(bad, n) == 0, "a wrong magic is refused");

        memcpy(bad, sBuf, (size_t)n);
        bad[4] ^= 0xFF;                                  /* version */
        check(sv_unpack(bad, n) == 0, "a wrong version is refused");

        memcpy(bad, sBuf, (size_t)n);
        bad[8] ^= 0xFF;                                  /* sizeof(Game) */
        check(sv_unpack(bad, n) == 0, "a changed struct size is refused");

        check(sv_unpack(sBuf, 12) == 0, "a truncated file is refused");
    }

    printf(fail ? "save: FAILED\n" : "save: passed\n");
    return fail;
}
