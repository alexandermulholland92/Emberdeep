/* ==========================================================
   minimap.h : the browser build's drawMinimap, ported.

   Emits the map as plain rectangles through a callback rather than
   drawing directly, so the renderer can push them through its HUD batch
   while the host harness rasterises the same rectangles to an image.
   Nothing here touches GL.
   ========================================================== */
#ifndef EMBERDEEP_MINIMAP_H
#define EMBERDEEP_MINIMAP_H

/* one filled rectangle, in the same units and colour space as hud_rect */
typedef void (*MmRectFn)(void *ctx, float x, float y, float w, float h,
                         float r, float g, float b, float a);

/* Draw the map for the current floor into a `size` x `size` box whose
   top-left corner is (ox, oy). Reads gGrid, gSeen, gRooms and G, so it
   reflects however much of the floor the player has explored. */
void mm_draw(float ox, float oy, float size, MmRectFn fn, void *ctx);

#endif
