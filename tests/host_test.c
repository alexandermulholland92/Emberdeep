/* ==========================================================
   host_test.c : desktop harness for the Vita simulation.

   Compiles dungeon.c + actors.c on the host and drives them with
   a pathfinding bot, so the game logic is verified even though the
   Vita toolchain is not available here.

   build:  cc -std=c99 -Isrc tests/host_test.c src/dungeon.c src/actors.c -lm -o /tmp/ht
   ========================================================== */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- RNG normally provided by main.c ---- */
static unsigned int sRng = 12345u;
static unsigned int xr(void) {
    sRng ^= sRng << 13; sRng ^= sRng >> 17; sRng ^= sRng << 5;
    return sRng;
}
float rnd01(void)            { return (float)(xr() & 0xFFFFFF) / (float)0x1000000; }
float rndr(float a, float b) { return a + rnd01() * (b - a); }
int   rndi(int a, int b)     { return a + (int)(rnd01() * (float)(b - a + 1)); }

/* ---- breadth-first path step, so the bot can follow corridors ---- */
typedef struct { int x, y; } Cell;
static int bfs_step(float fx, float fz, float tx, float tz, float *ox, float *oz) {
    static int prevX[GRID][GRID], prevY[GRID][GRID];
    static unsigned char seen[GRID][GRID];
    static Cell q[GRID * GRID];
    int head = 0, tail = 0, found = 0, cx, cy;
    int sx = dg_cell_of(fx), sy = dg_cell_of(fz);
    int gx = dg_cell_of(tx), gy = dg_cell_of(tz);
    const int dx4[4] = { 1, -1, 0, 0 }, dy4[4] = { 0, 0, 1, -1 };

    if (sx == gx && sy == gy) { *ox = tx; *oz = tz; return 1; }
    memset(seen, 0, sizeof(seen));
    if (sx < 0 || sy < 0 || sx >= GRID || sy >= GRID) return 0;
    seen[sx][sy] = 1;
    q[tail].x = sx; q[tail].y = sy; tail++;
    while (head < tail) {
        int k;
        cx = q[head].x; cy = q[head].y; head++;
        if (cx == gx && cy == gy) { found = 1; break; }
        for (k = 0; k < 4; k++) {
            int nx = cx + dx4[k], ny = cy + dy4[k];
            if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID) continue;
            if (seen[nx][ny] || gGrid[nx][ny] == 0) continue;
            seen[nx][ny] = 1;
            prevX[nx][ny] = cx; prevY[nx][ny] = cy;
            q[tail].x = nx; q[tail].y = ny; tail++;
        }
    }
    if (!found) return 0;
    cx = gx; cy = gy;
    while (!(prevX[cx][cy] == sx && prevY[cx][cy] == sy)) {
        int px = prevX[cx][cy], py = prevY[cx][cy];
        if (px == cx && py == cy) break;
        if (px == sx && py == sy) break;
        cx = px; cy = py;
        if (cx == sx && cy == sy) break;
    }
    *ox = dg_world_of(cx); *oz = dg_world_of(cy);
    return 1;
}

int main(int argc, char **argv) {
    int cls = argc > 1 ? atoi(argv[1]) : 0;
    int cheat = (argc > 2 && atoi(argv[2]) == 1);
    unsigned int seed = argc > 3 ? (unsigned int)atoi(argv[3]) : 2024u;
    int iter, lastFloor = -1, frames = 0;
    const float DT = 1.f / 60.f;
    char progress[256] = { 0 };

    sRng = seed;
    memset(&G, 0, sizeof(G));
    ac_start_run(cls);

    for (iter = 0; iter < 60000; iter++) {
        Input in;
        float tx, tz, wx, wz, dx, dz, len;
        int i, best = -1;
        float bestD = 1e9f;

        if (G.state == ST_DEAD || G.state == ST_WIN) break;
        if (cheat) {
            G.pl.hpMax = 9999.f; G.pl.hp = 9999.f;
            G.pl.atk = 400.f; G.pl.invuln = 9.f;
            G.pl.res = G.pl.resMax;
        }
        if (G.depth != lastFloor) {
            char buf[32];
            lastFloor = G.depth;
            snprintf(buf, sizeof buf, "%sF%d@lv%d", progress[0] ? " -> " : "", G.depth + 1, G.pl.level);
            strncat(progress, buf, sizeof(progress) - strlen(progress) - 1);
        }

        /* pick a goal: nearest awake enemy, else the portal, else an unvisited room */
        for (i = 0; i < MAX_ENEMY; i++) {
            Enemy *e = &G.en[i];
            float d;
            if (!e->active || !e->alive || e->dying || !e->awake) continue;
            d = v_dist(e->pos, G.pl.pos);
            if (d < bestD) { bestD = d; best = i; }
        }
        if (best >= 0)      { tx = G.en[best].pos.x; tz = G.en[best].pos.z; }
        else if (G.portalOn) { tx = G.portalPos.x;   tz = G.portalPos.z; }
        else {
            Room *target = 0;
            float bd = 1e9f;
            for (i = 0; i < gRoomCount; i++) {
                V3 c;
                float d;
                if (gRooms[i].spawned) continue;
                c = dg_room_center(&gRooms[i]);
                d = v_dist(c, G.pl.pos);
                if (d < bd) { bd = d; target = &gRooms[i]; }
            }
            if (!target) for (i = 0; i < gRoomCount; i++)
                if (gRooms[i].kind == ROOM_BOSS) target = &gRooms[i];
            if (target) { V3 c = dg_room_center(target); tx = c.x; tz = c.z; }
            else        { tx = G.pl.pos.x; tz = G.pl.pos.z; }
        }

        if (!bfs_step(G.pl.pos.x, G.pl.pos.z, tx, tz, &wx, &wz)) { wx = tx; wz = tz; }
        dx = wx - G.pl.pos.x; dz = wz - G.pl.pos.z;
        len = sqrtf(dx * dx + dz * dz);
        memset(&in, 0, sizeof in);
        if (len > 0.001f) { in.mx = dx / len; in.mz = dz / len; }

        /* step a few frames, then mash every ability */
        for (i = 0; i < 8; i++) { ac_update_player(DT, &in); ac_update(DT); frames++; }
        for (i = 0; i < 4; i++) ac_use_ability(i);
        for (i = 0; i < 5; i++) { ac_update_player(DT, &in); ac_update(DT); frames++; }
    }

    printf("class %-10s | frames %6d | %s\n", gClassDef[cls].name, frames, progress);
    printf("  level %d | kills %d | depth %d | state %s\n",
           G.pl.level, G.kills, G.depth,
           G.state == ST_WIN ? "WIN" : (G.state == ST_DEAD ? "DEAD" : "running"));
    return 0;
}
