/* ==========================================================
   texture.h : procedural surfaces built at load and uploaded to GL.

   A direct port of the browser build's surface recipes. The generator
   (tex_gen) is deliberately free of any GL dependency so the host-side
   harness can render the same pixels and diff them against the browser
   build's own output - see tests/tex_dump.c and `make texcheck`.
   ========================================================== */
#ifndef EMBERDEEP_TEXTURE_H
#define EMBERDEEP_TEXTURE_H

/* the browser build samples floors and walls at 256 and bodies at 128 */
#define TEX_WORLD 256
#define TEX_ACTOR 128

/* surface ids, in the same order as the browser build's buildSurfaces() */
enum {
    SURF_FLOOR_STONE,   /* stoneFloorSurface(101), 256 */
    SURF_WALL_MASON,    /* masonrySurface(111),    256 */
    SURF_ROCK,          /* rockSurface(71)              */
    SURF_BONE,          /* boneSurface(21)              */
    SURF_CLOTH,         /* clothSurface(31)             */
    SURF_LEATHER,       /* leatherSurface(41)           */
    SURF_METAL,         /* metalSurface(51)             */
    SURF_WOOD,          /* woodSurface(61)              */
    SURF_SKIN,          /* skinSurface(3, false)        */
    SURF_HIDE,          /* skinSurface(11, true)        */
    SURF_COUNT
};

/* edge length of a given surface: TEX_WORLD for the two world surfaces,
   TEX_ACTOR for everything else */
int  tex_size_of(int surf);

/* fill `rgba` (tex_size_of(surf)^2 * 4 bytes) with the surface's colour map.
   Pure computation - callable on the host with no GL context. */
void tex_gen(int surf, unsigned char *rgba);

#ifndef TEX_HOST_HARNESS
#include <vitaGL.h>

typedef struct {
    GLuint surf[SURF_COUNT];   /* indexed by the SURF_* ids above */
    GLuint white;              /* 1x1, for passes that want no texture */
} Textures;

extern Textures gTex;

int  tex_build(void);   /* returns 0 on allocation failure */
void tex_free(void);
#endif

#endif
