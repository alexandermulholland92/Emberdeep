/* ==========================================================
   minimap.c : drawMinimap, ported from the browser build.

   Same layers in the same order: a dark panel, the explored floor
   (corridors darker than rooms), a tint over a discovered boss room, the
   portal, then enemies, then the player on top.

   The browser draws a rect per explored cell into a 2D canvas. Here the
   cells are merged into horizontal runs first - identical output, but a
   fully explored floor costs a few dozen quads in the HUD batch instead
   of several hundred.

   Its small pixel fudges (a rect drawn 0.6 wider than its cell to close
   hairline seams, markers inset by 0.5) are in the canvas's internal
   resolution, which is twice the size it is displayed at. They are
   carried here as fractions of a cell so they mean the same thing at
   whatever size the panel is drawn.
   ========================================================== */
#include "minimap.h"
#include "game.h"

/* the browser's canvas clips for free; the HUD batch does not, so markers
   near the edge are trimmed to the panel instead of bleeding over the HUD */
typedef struct { float x0, y0, x1, y1; MmRectFn fn; void *ctx; } Clip;

static void put(const Clip *c, float x, float y, float w, float h,
                float r, float g, float b, float a) {
    float x1 = x + w, y1 = y + h;
    if (x < c->x0) x = c->x0;
    if (y < c->y0) y = c->y0;
    if (x1 > c->x1) x1 = c->x1;
    if (y1 > c->y1) y1 = c->y1;
    if (x1 <= x || y1 <= y) return;
    c->fn(c->ctx, x, y, x1 - x, y1 - y, r, g, b, a);
}

void mm_draw(float ox, float oy, float size, MmRectFn fn, void *ctx) {
    float s = size / (float)GRID;
    float bleed = s * 0.131f;   /* the browser's 0.6 of a 4.57px cell */
    float inset = s * 0.109f;   /* and its 0.5                        */
    Clip c;
    int x, y, i;

    if (!fn || size <= 0.f) return;
    c.x0 = ox; c.y0 = oy; c.x1 = ox + size; c.y1 = oy + size;
    c.fn = fn; c.ctx = ctx;

    /* panel */
    put(&c, ox, oy, size, size, 0.039f, 0.035f, 0.055f, 0.55f);

    /* explored floor, merged along each row */
    for (y = 0; y < GRID; y++) {
        x = 0;
        while (x < GRID) {
            int kind, run;
            if (!gSeen[x][y] || gGrid[x][y] == 0) { x++; continue; }
            kind = gGrid[x][y];
            run = 1;
            while (x + run < GRID && gSeen[x + run][y] && gGrid[x + run][y] == kind)
                run++;
            if (kind == 2)   /* corridor */
                put(&c, ox + x * s, oy + y * s, run * s + bleed, s + bleed,
                    0.231f, 0.227f, 0.267f, 1.f);
            else             /* room floor */
                put(&c, ox + x * s, oy + y * s, run * s + bleed, s + bleed,
                    0.376f, 0.349f, 0.271f, 1.f);
            x += run;
        }
    }

    /* boss room, once its corner has been seen */
    for (i = 0; i < gRoomCount; i++) {
        const Room *r = &gRooms[i];
        if (r->kind != ROOM_BOSS) continue;
        if (r->x < 0 || r->y < 0 || r->x >= GRID || r->y >= GRID) continue;
        if (!gSeen[r->x][r->y]) continue;
        put(&c, ox + r->x * s, oy + r->y * s, r->w * s, r->h * s,
            0.784f, 0.196f, 0.157f, 0.34f);
    }

    /* portal out */
    if (G.portalOn) {
        int px = dg_cell_of(G.portalPos.x), py = dg_cell_of(G.portalPos.z);
        put(&c, ox + px * s - inset * 2.f, oy + py * s - inset * 2.f,
            s + inset * 4.f, s + inset * 4.f, 0.498f, 0.847f, 1.f, 1.f);
    }

    /* enemies, but only on ground the player has actually explored */
    for (i = 0; i < MAX_ENEMY; i++) {
        const Enemy *e = &G.en[i];
        int ex, ey;
        if (!e->active || !e->alive) continue;
        ex = dg_cell_of(e->pos.x);
        ey = dg_cell_of(e->pos.z);
        if (ex < 0 || ey < 0 || ex >= GRID || ey >= GRID) continue;
        if (!gSeen[ex][ey]) continue;
        if (e->isBoss)
            put(&c, ox + ex * s - inset, oy + ey * s - inset,
                s + inset * 2.f, s + inset * 2.f, 1.f, 0.353f, 0.227f, 1.f);
        else
            put(&c, ox + ex * s - inset, oy + ey * s - inset,
                s + inset * 2.f, s + inset * 2.f, 0.816f, 0.337f, 0.290f, 1.f);
    }

    /* the player last, so nothing hides the marker. The browser draws a
       filled circle of radius s*1.05; a quad of the same extent is what
       the HUD batch can express, and at this scale it reads the same. */
    {
        float px = dg_cell_of(G.pl.pos.x) * s;
        float py = dg_cell_of(G.pl.pos.z) * s;
        float rad = s * 1.05f;
        put(&c, ox + px + s * 0.5f - rad, oy + py + s * 0.5f - rad,
            rad * 2.f, rad * 2.f, 1.f, 0.843f, 0.416f, 1.f);
    }
}
