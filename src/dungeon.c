/* ==========================================================
   dungeon.c : layout generation, collision, grid queries
   No platform calls - compiles on desktop for the tests.
   ========================================================== */
#include "game.h"

unsigned char gGrid[GRID][GRID];
unsigned char gSeen[GRID][GRID];
Room  gRooms[MAX_ROOMS];
int   gRoomCount;

float dg_world_of(int g) { return ((float)g - GRID * 0.5f) * CELL; }
int   dg_cell_of(float w) { return (int)floorf(w / CELL + GRID * 0.5f + 0.5f); }

static int cell_floor(int gx, int gy) {
    if (gx < 0 || gy < 0 || gx >= GRID || gy >= GRID) return 0;
    return gGrid[gx][gy] > 0;
}

int dg_walkable(float x, float z, float r) {
    return cell_floor(dg_cell_of(x - r), dg_cell_of(z - r)) &&
           cell_floor(dg_cell_of(x + r), dg_cell_of(z - r)) &&
           cell_floor(dg_cell_of(x - r), dg_cell_of(z + r)) &&
           cell_floor(dg_cell_of(x + r), dg_cell_of(z + r));
}

/* axis-separated so bodies slide along a wall instead of sticking to it */
int dg_move(V3 *p, float dx, float dz, float r) {
    int moved = 0;
    if (dx != 0.f && dg_walkable(p->x + dx, p->z, r)) { p->x += dx; moved = 1; }
    if (dz != 0.f && dg_walkable(p->x, p->z + dz, r)) { p->z += dz; moved = 1; }
    return moved;
}

Room *dg_room_at(float x, float z) {
    int gx = dg_cell_of(x), gy = dg_cell_of(z), i;
    for (i = 0; i < gRoomCount; i++) {
        Room *r = &gRooms[i];
        if (gx >= r->x && gx < r->x + r->w && gy >= r->y && gy < r->y + r->h) return r;
    }
    return 0;
}

V3 dg_room_center(const Room *r) {
    V3 c;
    c.x = dg_world_of(r->x) + (r->w - 1) * 0.5f * CELL;
    c.y = 0.f;
    c.z = dg_world_of(r->y) + (r->h - 1) * 0.5f * CELL;
    return c;
}

void dg_mark_seen(float x, float z, int cells) {
    int gx = dg_cell_of(x), gy = dg_cell_of(z), ix, iy;
    for (ix = gx - cells; ix <= gx + cells; ix++)
        for (iy = gy - cells; iy <= gy + cells; iy++)
            if (ix >= 0 && iy >= 0 && ix < GRID && iy < GRID && gGrid[ix][iy] > 0)
                gSeen[ix][iy] = 1;
}

void dg_reveal_room(const Room *r) {
    int x, y;
    for (x = r->x; x < r->x + r->w; x++)
        for (y = r->y; y < r->y + r->h; y++)
            if (x >= 0 && y >= 0 && x < GRID && y < GRID) gSeen[x][y] = 1;
}

/* ---------- generation ---------- */

static void carve_room(const Room *r) {
    int x, y;
    for (x = r->x; x < r->x + r->w; x++)
        for (y = r->y; y < r->y + r->h; y++)
            if (x >= 0 && y >= 0 && x < GRID && y < GRID) gGrid[x][y] = 1;   /* room floor */
}

static void h_line(int x0, int x1, int y) {
    int s = x0 < x1 ? x0 : x1, e = x0 < x1 ? x1 : x0, x, w;
    for (x = s; x <= e; x++)
        for (w = 0; w < 2; w++)
            if (x >= 0 && x < GRID && y + w >= 0 && y + w < GRID && gGrid[x][y + w] == 0)
                gGrid[x][y + w] = 2;
}
static void v_line(int y0, int y1, int x) {
    int s = y0 < y1 ? y0 : y1, e = y0 < y1 ? y1 : y0, y, w;
    for (y = s; y <= e; y++)
        for (w = 0; w < 2; w++)
            if (x + w >= 0 && x + w < GRID && y >= 0 && y < GRID && gGrid[x + w][y] == 0)
                gGrid[x + w][y] = 2;
}

static void carve_corridor(const Room *a, const Room *b) {
    int ax = a->x + a->w / 2, ay = a->y + a->h / 2;
    int bx = b->x + b->w / 2, by = b->y + b->h / 2;
    if (rnd01() < 0.5f) { h_line(ax, bx, ay); v_line(ay, by, bx); }
    else                { v_line(ay, by, ax); h_line(ax, bx, by); }
}

static int overlaps(const Room *r, int pad) {
    int i;
    for (i = 0; i < gRoomCount; i++) {
        const Room *o = &gRooms[i];
        if (r->x - pad < o->x + o->w && r->x + r->w + pad > o->x &&
            r->y - pad < o->y + o->h && r->y + r->h + pad > o->y) return 1;
    }
    return 0;
}

static int try_place(Room *out, int minS, int maxS, int tries) {
    int t;
    for (t = 0; t < tries; t++) {
        Room r;
        r.w = rndi(minS, maxS);
        r.h = rndi(minS, maxS);
        r.x = rndi(2, GRID - r.w - 3);
        r.y = rndi(2, GRID - r.h - 3);
        r.kind = ROOM_NORMAL;
        r.spawned = 0;
        if (!overlaps(&r, 2)) { *out = r; return 1; }
    }
    return 0;
}

/* Bumped on every regenerate. The layout is freshly randomised each call,
   including for a depth just visited, so anything caching the world has to
   key on this rather than on G.depth. */
unsigned int gDungeonGen = 0;

void dg_generate(int depth) {
    int i, want, hasBoss = 0;

    memset(gGrid, 0, sizeof(gGrid));
    memset(gSeen, 0, sizeof(gSeen));
    gRoomCount = 0;

    want = 6 + (depth < 2 ? depth : 2);
    if (want > MAX_ROOMS) want = MAX_ROOMS;

    if (!try_place(&gRooms[0], 6, 8, 400)) {
        gRooms[0].x = 4; gRooms[0].y = 4; gRooms[0].w = 6; gRooms[0].h = 6;
    }
    gRooms[0].kind = ROOM_START;
    gRooms[0].spawned = 1;          /* never spawn enemies on the player */
    gRoomCount = 1;

    for (i = 1; i < want; i++) {
        Room r;
        int last = (i == want - 1);
        if (!try_place(&r, last ? 9 : 5, last ? 11 : 9, 500)) continue;
        if (last) { r.kind = ROOM_BOSS; hasBoss = 1; }
        gRooms[gRoomCount++] = r;
    }
    if (!hasBoss && gRoomCount > 1) {
        gRooms[gRoomCount - 1].kind = ROOM_BOSS;
    }

    for (i = 0; i < gRoomCount; i++) carve_room(&gRooms[i]);
    for (i = 1; i < gRoomCount; i++) carve_corridor(&gRooms[i - 1], &gRooms[i]);

    gDungeonGen++;
}
