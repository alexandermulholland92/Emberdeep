/* ==========================================================
   save.c : snapshot a run to ux0: and read it back.
   ========================================================== */
#include "save.h"
#include "game.h"
#include <string.h>

#ifndef SV_HOST_HARNESS
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>   /* sceIoMkdir lives here, not in dirent.h */
#endif

#define SV_MAGIC   0x524D4245u   /* 'EMBR' */
#define SV_VERSION 1u

/* Sizes of every block go in the header. A struct that grows or shrinks
   between builds then fails the check instead of being read as garbage. */
typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int game;      /* sizeof(Game)        */
    unsigned int timers;    /* the deferred-effect table */
    unsigned int grid;      /* sizeof(gGrid)       */
    unsigned int seen;
    unsigned int rooms;     /* sizeof(gRooms)      */
    int          roomCount;
    int          total;     /* payload bytes after the header */
} SvHeader;

int sv_pack(void *buf, int cap) {
    unsigned char *p = (unsigned char *)buf;
    SvHeader h;
    int timerBytes = 0;
    const void *timers = ac_timer_state(&timerBytes);
    Game snap;
    int at;

    if (!buf) return 0;

    h.magic = SV_MAGIC;
    h.version = SV_VERSION;
    h.game = (unsigned int)sizeof(Game);
    h.timers = (unsigned int)timerBytes;
    h.grid = (unsigned int)sizeof(gGrid);
    h.seen = (unsigned int)sizeof(gSeen);
    h.rooms = (unsigned int)sizeof(gRooms);
    h.roomCount = gRoomCount;
    h.total = (int)(h.game + h.timers + h.grid + h.seen + h.rooms);

    if (cap < (int)sizeof h + h.total) return 0;

    /* bannerText and toastText point into this build's string constants,
       so they mean nothing to a later one - drop them, and resume unpaused */
    snap = G;
    snap.bannerText = 0;
    snap.toastText = 0;
    snap.pauseNote = 0;
    snap.bannerT = 0.f;
    snap.toastT = 0.f;
    snap.state = ST_PLAY;

    memcpy(p, &h, sizeof h);                   at = (int)sizeof h;
    memcpy(p + at, &snap, h.game);             at += (int)h.game;
    memcpy(p + at, timers, h.timers);          at += (int)h.timers;
    memcpy(p + at, gGrid, h.grid);             at += (int)h.grid;
    memcpy(p + at, gSeen, h.seen);             at += (int)h.seen;
    memcpy(p + at, gRooms, h.rooms);           at += (int)h.rooms;
    return at;
}

int sv_unpack(const void *buf, int len) {
    const unsigned char *p = (const unsigned char *)buf;
    SvHeader h;
    int timerBytes = 0;
    void *timers = ac_timer_state(&timerBytes);
    int at;

    if (!buf || len < (int)sizeof h) return 0;
    memcpy(&h, p, sizeof h);

    if (h.magic != SV_MAGIC || h.version != SV_VERSION) return 0;
    if (h.game != (unsigned int)sizeof(Game)) return 0;
    if (h.timers != (unsigned int)timerBytes) return 0;
    if (h.grid != (unsigned int)sizeof(gGrid)) return 0;
    if (h.seen != (unsigned int)sizeof(gSeen)) return 0;
    if (h.rooms != (unsigned int)sizeof(gRooms)) return 0;
    if (h.roomCount < 0 || h.roomCount > MAX_ROOMS) return 0;
    if (len < (int)sizeof h + h.total) return 0;

    at = (int)sizeof h;
    memcpy(&G, p + at, h.game);                at += (int)h.game;
    memcpy(timers, p + at, h.timers);          at += (int)h.timers;
    memcpy(gGrid, p + at, h.grid);             at += (int)h.grid;
    memcpy(gSeen, p + at, h.seen);             at += (int)h.seen;
    memcpy(gRooms, p + at, h.rooms);           at += (int)h.rooms;
    gRoomCount = h.roomCount;

    /* stale pointers never survive the trip */
    G.bannerText = 0;
    G.toastText = 0;
    G.pauseNote = 0;
    G.bannerT = 0.f;
    G.toastT = 0.f;

    /* the layout just changed under the renderer, which caches the world
       mesh - the same counter a restart uses tells it to rebuild */
    gDungeonGen++;
    return 1;
}

#ifndef SV_HOST_HARNESS
static unsigned char sBuf[SV_MAX_BYTES];

int sv_save(void) {
    SceUID fd;
    int n = sv_pack(sBuf, (int)sizeof sBuf);
    if (n <= 0) return 0;

    sceIoMkdir("ux0:data", 0777);            /* both are no-ops if present */
    sceIoMkdir(SV_PATH_DIR, 0777);

    fd = sceIoOpen(SV_PATH_FILE, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return 0;
    if (sceIoWrite(fd, sBuf, n) != n) { sceIoClose(fd); return 0; }
    sceIoClose(fd);
    return 1;
}

int sv_load(void) {
    SceUID fd = sceIoOpen(SV_PATH_FILE, SCE_O_RDONLY, 0777);
    int n;
    if (fd < 0) return 0;
    n = sceIoRead(fd, sBuf, (int)sizeof sBuf);
    sceIoClose(fd);
    if (n <= 0) return 0;
    return sv_unpack(sBuf, n);
}

int sv_exists(void) {
    SceUID fd = sceIoOpen(SV_PATH_FILE, SCE_O_RDONLY, 0777);
    if (fd < 0) return 0;
    sceIoClose(fd);
    return 1;
}

void sv_delete(void) {
    sceIoRemove(SV_PATH_FILE);
}
#endif
