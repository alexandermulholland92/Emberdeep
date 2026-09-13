/* Minimal vitaGL surface used only to type-check render.c on the host.
   Signatures mirror vitaGL/GLES; this file is NOT shipped to the Vita. */
#ifndef STUB_VITAGL_H
#define STUB_VITAGL_H
#include <stdlib.h>
typedef unsigned int GLenum; typedef unsigned int GLbitfield; typedef int GLint;
typedef int GLsizei; typedef float GLfloat; typedef unsigned char GLboolean; typedef void GLvoid;
#define GL_FLOAT 0x1406
#define GL_TRIANGLES 0x0004
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_VERTEX_ARRAY 0x8074
#define GL_COLOR_ARRAY 0x8076
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700
#define GL_LEQUAL 0x0203
#define GL_LESS 0x0201
#define GL_FALSE 0
void vglInit(int poolSize);
void vglEnd(void);
void vglSwapBuffers(GLboolean hasCommonDialog);
void glClear(GLbitfield mask);
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glEnable(GLenum cap);
void glDisable(GLenum cap);
void glDepthFunc(GLenum func);
void glBlendFunc(GLenum s, GLenum d);
void glEnableClientState(GLenum a);
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p);
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p);
void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glMatrixMode(GLenum mode);
void glLoadIdentity(void);
void glLoadMatrixf(const GLfloat *m);
#endif
