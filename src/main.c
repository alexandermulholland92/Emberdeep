/* ==========================================================
   main.c : Vita entry point, input and the frame loop.
   ========================================================== */
#include "game.h"
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>

/* ---------------- RNG (declared in game.h, used by dungeon/actors) ------- */
static unsigned int sRng = 2463534242u;
static unsigned int xr(void) {
    sRng ^= sRng << 13; sRng ^= sRng >> 17; sRng ^= sRng << 5;
    return sRng;
}
float rnd01(void)            { return (float)(xr() & 0xFFFFFF) / (float)0x1000000; }
float rndr(float a, float b) { return a + rnd01() * (b - a); }
int   rndi(int a, int b)     { if (b < a) { int t = a; a = b; b = t; } 
                               return a + (int)(rnd01() * (float)(b - a + 1)); }

/* ---------------- input ---------------- */
#define STICK_DEAD 32

static void read_input(Input *in, SceCtrlData *pad, SceCtrlData *prev) {
    float lx = ((float)pad->lx - 128.f), ly = ((float)pad->ly - 128.f);
    unsigned int pressed = pad->buttons & ~prev->buttons;

    if (lx > -STICK_DEAD && lx < STICK_DEAD) lx = 0.f;
    if (ly > -STICK_DEAD && ly < STICK_DEAD) ly = 0.f;

    /* screen-right is +x; pushing the stick up walks away from the camera */
    in->mx = lx / 110.f;
    in->mz = ly / 110.f;
    if (in->mx < -1.f) in->mx = -1.f;
    if (in->mx >  1.f) in->mx =  1.f;
    if (in->mz < -1.f) in->mz = -1.f;
    if (in->mz >  1.f) in->mz =  1.f;

    /* d-pad walks too, for anyone whose stick has drifted */
    if (pad->buttons & SCE_CTRL_LEFT)  in->mx = -1.f;
    if (pad->buttons & SCE_CTRL_RIGHT) in->mx =  1.f;
    if (pad->buttons & SCE_CTRL_UP)    in->mz = -1.f;
    if (pad->buttons & SCE_CTRL_DOWN)  in->mz =  1.f;

    in->b[0] = (pressed & SCE_CTRL_CROSS)    ? 1 : 0;
    in->b[1] = (pressed & SCE_CTRL_SQUARE)   ? 1 : 0;
    in->b[2] = (pressed & SCE_CTRL_TRIANGLE) ? 1 : 0;
    in->b[3] = (pressed & SCE_CTRL_CIRCLE)   ? 1 : 0;
    in->start = (pressed & SCE_CTRL_START)   ? 1 : 0;
}

/* ---------------- menu ---------------- */
static void menu_input(const Input *in, SceCtrlData *pad, SceCtrlData *prev) {
    unsigned int pressed = pad->buttons & ~prev->buttons;
    if (pressed & SCE_CTRL_UP)   G.classPick = (G.classPick + CLS_COUNT - 1) % CLS_COUNT;
    if (pressed & SCE_CTRL_DOWN) G.classPick = (G.classPick + 1) % CLS_COUNT;
    if (in->b[0]) ac_start_run(G.classPick);
}

int main(void) {
    SceCtrlData pad, prev;
    SceRtcTick now, last;
    int i;

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    memset(&pad, 0, sizeof pad);
    memset(&prev, 0, sizeof prev);

    if (!rd_init()) {
        sceKernelExitProcess(0);
        return 0;
    }

    memset(&G, 0, sizeof G);
    G.state = ST_CLASS;
    G.classPick = 0;

    sceRtcGetCurrentTick(&last);
    sRng ^= (unsigned int)last.tick;          /* seed the layout from the clock */

    for (;;) {
        Input in;
        float dt;

        sceCtrlPeekBufferPositive(0, &pad, 1);
        memset(&in, 0, sizeof in);
        read_input(&in, &pad, &prev);

        sceRtcGetCurrentTick(&now);
        dt = (float)((double)(now.tick - last.tick) / 1000000.0);
        last = now;
        if (dt > 0.1f) dt = 0.1f;             /* never let a hitch teleport anyone */
        if (dt <= 0.f) dt = 1.f / 60.f;

        if (G.state == ST_CLASS) {
            menu_input(&in, &pad, &prev);
        } else if (G.state == ST_PLAY) {
            for (i = 0; i < 4; i++) if (in.b[i]) ac_use_ability(i);
            ac_update_player(dt, &in);
            ac_update(dt);
        } else {                              /* dead or victorious */
            if (in.b[0]) { G.state = ST_CLASS; }
        }

        rd_frame(dt);
        prev = pad;
    }

    rd_shutdown();
    sceKernelExitProcess(0);
    return 0;
}
