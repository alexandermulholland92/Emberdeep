/* ==========================================================
   texture.h : procedural textures built at load and uploaded to GL.
   ========================================================== */
#ifndef EMBERDEEP_TEXTURE_H
#define EMBERDEEP_TEXTURE_H

#include <vitaGL.h>

/* 128 is plenty on a 960x544 screen and keeps generation under a second */
#define TEX_SIZE 128

typedef struct {
    GLuint floor;   /* flagstone with grout, chips and fractures */
    GLuint wall;    /* coursed masonry with mortar and per-block tone */
    GLuint actor;   /* cloth/hide grain shared by every body */
    GLuint white;   /* 1x1, for passes that want no texture */
} Textures;

extern Textures gTex;

int  tex_build(void);   /* returns 0 on allocation failure */
void tex_free(void);

#endif
