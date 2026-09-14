/* Minimal vitaGL surface used only to type-check render.c on the host.
   Signatures mirror vitaGL/GLES; this file is NOT shipped to the Vita. */
#ifndef STUB_VITAGL_H
#define STUB_VITAGL_H
#include <stdlib.h>
typedef unsigned int GLenum; typedef unsigned int GLbitfield; typedef int GLint;
typedef unsigned int GLuint;
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
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_REPEAT 0x2901
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_MODULATE 0x2100
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
/* mirrors the real vitaGL: init returns GLboolean, and there is no teardown */
GLboolean vglInit(int legacy_pool_size);
void vglSwapBuffers(GLboolean has_commondialog);
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
/* signatures copied from vitaGL/source/vitaGL.h */
void glGenTextures(GLsizei n, GLuint *textures);
void glBindTexture(GLenum target, GLuint texture);
void glDeleteTextures(GLsizei n, const GLuint *textures);
void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width,
                  GLsizei height, GLint border, GLenum format, GLenum type,
                  const GLvoid *data);
void glGenerateMipmap(GLenum target);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexEnvi(GLenum target, GLenum pname, GLint param);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
#endif
