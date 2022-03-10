/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
** QGL.H
*/
#ifndef QGL_H
#define QGL_H

QGL_EXTERN	qboolean QGL_Init( const char *dllname );
QGL_EXTERN	void     QGL_Shutdown( void );

QGL_EXTERN	void	*qglGetProcAddress( const GLubyte * );

typedef int GLintptrARB;
typedef int GLsizeiptrARB;

#endif

#ifndef APIENTRY
# define APIENTRY
#endif

/*
** extension constants
*/

#define GL_TEXTURE0_SGIS									0x835E
#define GL_TEXTURE1_SGIS									0x835F
#define GL_TEXTURE0_ARB										0x84C0
#define GL_TEXTURE1_ARB										0x84C1
#define GL_MAX_TEXTURE_UNITS								0x84E2
#define GL_MAX_TEXTURE_UNITS_ARB							0x84E2

#ifndef GL_POLYGON_OFFSET
#define GL_POLYGON_OFFSET									0x8037

#endif /* GL_POLYGON_OFFSET */

#ifndef GL_ARB_texture_env_combine
#define GL_ARB_texture_env_combine

#define GL_COMBINE_ARB										0x8570
#define GL_COMBINE_RGB_ARB									0x8571
#define GL_COMBINE_ALPHA_ARB								0x8572
#define GL_RGB_SCALE_ARB									0x8573
#define GL_ADD_SIGNED_ARB									0x8574
#define GL_INTERPOLATE_ARB									0x8575
#define GL_CONSTANT_ARB										0x8576
#define GL_PRIMARY_COLOR_ARB								0x8577
#define GL_PREVIOUS_ARB										0x8578
#define GL_SOURCE0_RGB_ARB									0x8580
#define GL_SOURCE1_RGB_ARB									0x8581
#define GL_SOURCE2_RGB_ARB									0x8582
#define GL_SOURCE0_ALPHA_ARB								0x8588
#define GL_SOURCE1_ALPHA_ARB								0x8589
#define GL_SOURCE2_ALPHA_ARB								0x858A
#define GL_OPERAND0_RGB_ARB									0x8590
#define GL_OPERAND1_RGB_ARB									0x8591
#define GL_OPERAND2_RGB_ARB									0x8592
#define GL_OPERAND0_ALPHA_ARB								0x8598
#define GL_OPERAND1_ALPHA_ARB								0x8599
#define GL_OPERAND2_ALPHA_ARB								0x859A

#endif /* GL_ARB_texture_env_combine */

/* GL_ARB_texture_env_dot3 */
#ifndef GL_ARB_texture_env_dot3
#define GL_ARB_texture_env_dot3

#define GL_DOT3_RGB_ARB										0x86AE
#define GL_DOT3_RGBA_ARB									0x86AF
#endif /* GL_ARB_texture_env_dot3 */

/* NV_texture_env_combine4 */
#ifndef GL_NV_texture_env_combine4
#define GL_NV_texture_env_combine4

#define GL_COMBINE4_NV										0x8503
#define GL_SOURCE3_RGB_NV									0x8583
#define GL_SOURCE3_ALPHA_NV									0x858B
#define GL_OPERAND3_RGB_NV									0x8593
#define GL_OPERAND3_ALPHA_NV								0x859B

#endif /* NV_texture_env_combine4 */

/* GL_ARB_texture_compression */
#ifndef GL_ARB_texture_compression
#define GL_ARB_texture_compression

#define GL_COMPRESSED_ALPHA_ARB								0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB							0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB					0x84EB
#define GL_COMPRESSED_INTENSITY_ARB							0x84EC
#define GL_COMPRESSED_RGB_ARB								0x84ED
#define GL_COMPRESSED_RGBA_ARB								0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB						0x84EF
#define GL_TEXTURE_IMAGE_SIZE_ARB							0x86A0
#define GL_TEXTURE_COMPRESSED_ARB							0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB				0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB					0x86A3
#endif /* GL_ARB_texture_compression */

/* GL_EXT_texture_filter_anisotropic */
#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic

#define GL_TEXTURE_MAX_ANISOTROPY_EXT						0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT					0x84FF
#endif /* GL_EXT_texture_filter_anisotropic */

/* GL_EXT_texture_edge_clamp */
#ifndef GL_EXT_texture_edge_clamp
#define GL_EXT_texture_edge_clamp

#define GL_CLAMP_TO_EDGE									0x812F
#endif /* GL_EXT_texture_edge_clamp */

/* GL_ARB_vertex_buffer_object */
#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object

#define GL_ARRAY_BUFFER_ARB									0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB							0x8893
#define GL_ARRAY_BUFFER_BINDING_ARB							0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB					0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING_ARB					0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING_ARB					0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING_ARB					0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING_ARB					0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB			0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB				0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB			0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB			0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB					0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB			0x889F
#define GL_STREAM_DRAW_ARB									0x88E0
#define GL_STREAM_READ_ARB									0x88E1
#define GL_STREAM_COPY_ARB									0x88E2
#define GL_STATIC_DRAW_ARB									0x88E4
#define GL_STATIC_READ_ARB									0x88E5
#define GL_STATIC_COPY_ARB									0x88E6
#define GL_DYNAMIC_DRAW_ARB									0x88E8
#define GL_DYNAMIC_READ_ARB									0x88E9
#define GL_DYNAMIC_COPY_ARB									0x88EA
#define GL_READ_ONLY_ARB									0x88B8
#define GL_WRITE_ONLY_ARB									0x88B9
#define GL_READ_WRITE_ARB									0x88BA
#define GL_BUFFER_SIZE_ARB									0x8764
#define GL_BUFFER_USAGE_ARB									0x8765
#define GL_BUFFER_ACCESS_ARB								0x88BB
#define GL_BUFFER_MAPPED_ARB								0x88BC
#define GL_BUFFER_MAP_POINTER_ARB							0x88BD
#endif /* GL_ARB_vertex_buffer_object */

/* GL_ARB_texture_cube_map */
#ifndef GL_ARB_texture_cube_map
#define GL_ARB_texture_cube_map

#define GL_NORMAL_MAP_ARB									0x8511
#define GL_REFLECTION_MAP_ARB								0x8512
#define GL_TEXTURE_CUBE_MAP_ARB								0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP_ARB						0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB					0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB					0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB					0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB					0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB					0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB					0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP_ARB						0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB					0x851C
#endif /* GL_ARB_texture_cube_map */

/* GL_EXT_bgra */
#ifndef GL_EXT_bgra
#define GL_EXT_bgra

#define GL_BGR_EXT											0x80E0
#define GL_BGRA_EXT											0x80E1
#endif

QGL_FUNC(void, glAlphaFunc, (GLenum func, GLclampf ref));
QGL_FUNC(void, glArrayElement, (GLint i));
QGL_FUNC(void, glBegin, (GLenum mode));
QGL_FUNC(void, glBindTexture, (GLenum target, GLuint texture));
QGL_FUNC(void, glBlendFunc, (GLenum sfactor, GLenum dfactor));
QGL_FUNC(void, glClear, (GLbitfield mask));
QGL_FUNC(void, glClearColor, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha));
QGL_FUNC(void, glClearDepth, (GLclampd depth));
QGL_FUNC(void, glClearStencil, (GLint s));
QGL_FUNC(void, glClipPlane, (GLenum plane, const GLdouble *equation));
QGL_FUNC(void, glColor3f, (GLfloat red, GLfloat green, GLfloat blue));
QGL_FUNC(void, glColor3fv, (const GLfloat *v));
QGL_FUNC(void, glColor4f, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha));
QGL_FUNC(void, glColor4fv, (const GLfloat *v));
QGL_FUNC(void, glColor4ubv, (const GLubyte *v));
QGL_FUNC(void, glColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha));
QGL_FUNC(void, glColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer));
QGL_FUNC(void, glCullFace, (GLenum mode));
QGL_FUNC(void, glDeleteTextures, (GLsizei n, const GLuint *textures));
QGL_FUNC(void, glDepthFunc, (GLenum func));
QGL_FUNC(void, glDepthMask, (GLboolean flag));
QGL_FUNC(void, glDepthRange, (GLclampd zNear, GLclampd zFar));
QGL_FUNC(void, glDisable, (GLenum cap));
QGL_FUNC(void, glDisableClientState, (GLenum array));
QGL_FUNC(void, glDrawBuffer, (GLenum mode));
QGL_FUNC(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices));
QGL_FUNC(void, glEnable, (GLenum cap));
QGL_FUNC(void, glEnableClientState, (GLenum array));
QGL_FUNC(void, glEnd, (void));
QGL_FUNC(void, glFinish, (void));
QGL_FUNC(void, glFlush, (void));
QGL_FUNC(void, glFrontFace, (GLenum mode));
QGL_FUNC(GLenum, glGetError, (void));
QGL_FUNC(void, glGetIntegerv, (GLenum pname, GLint *params));
QGL_FUNC(const GLubyte *, glGetString, (GLenum name));
QGL_FUNC(void, glLoadIdentity, (void));
QGL_FUNC(void, glLoadMatrixf, (const GLfloat *m));
QGL_FUNC(void, glMatrixMode, (GLenum mode));
QGL_FUNC(void, glNormalPointer, (GLenum type, GLsizei stride, const GLvoid *pointer));
QGL_FUNC(void, glOrtho, (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar));
QGL_FUNC(void, glPolygonMode, (GLenum face, GLenum mode));
QGL_FUNC(void, glPolygonOffset, (GLfloat factor, GLfloat units));
QGL_FUNC(void, glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels));
QGL_FUNC(void, glScissor, (GLint x, GLint y, GLsizei width, GLsizei height));
QGL_FUNC(void, glShadeModel, (GLenum mode));
QGL_FUNC(void, glStencilFunc, (GLenum func, GLint ref, GLuint mask));
QGL_FUNC(void, glStencilMask, (GLuint mask));
QGL_FUNC(void, glStencilOp, (GLenum fail, GLenum zfail, GLenum zpass));
QGL_FUNC(void, glTexCoord2f, (GLfloat s, GLfloat t));
QGL_FUNC(void, glTexCoordPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer));
QGL_FUNC(void, glTexEnvfv, (GLenum target, GLenum pname, const GLfloat *params));
QGL_FUNC(void, glTexEnvi, (GLenum target, GLenum pname, GLint param));
QGL_FUNC(void, glTexGenfv, (GLenum coord, GLenum pname, const GLfloat *params));
QGL_FUNC(void, glTexGeni, (GLenum coord, GLenum pname, GLint param));
QGL_FUNC(void, glTexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels));
QGL_FUNC(void, glTexParameteri, (GLenum target, GLenum pname, GLint param));
QGL_FUNC(void, glTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels));
QGL_FUNC(void, glVertex2f, (GLfloat x, GLfloat y));
QGL_FUNC(void, glVertex3f, (GLfloat x, GLfloat y, GLfloat z));
QGL_FUNC(void, glVertex3fv, (const GLfloat *v));
QGL_FUNC(void, glVertexPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer));
QGL_FUNC(void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height));

QGL_EXT(void, glLockArraysEXT, (int , int ));
QGL_EXT(void, glUnlockArraysEXT, (void));
QGL_EXT(void, glSelectTextureSGIS, (GLenum ));
QGL_EXT(void, glActiveTextureARB, (GLenum ));
QGL_EXT(void, glClientActiveTextureARB, (GLenum ));
QGL_EXT(void, glDrawRangeElementsEXT, (GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *));
QGL_EXT(void, glBindBufferARB, (GLenum target, GLuint buffer));
QGL_EXT(void, glDeleteBuffersARB, (GLsizei n, const GLuint *buffers));
QGL_EXT(void, glGenBuffersARB, (GLsizei n, GLuint *buffers));
QGL_EXT(void, glBufferDataARB, (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage));
QGL_EXT(void, glBufferDataARB, (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage));

// WGL Functions
QGL_WGL(int, wglChoosePixelFormat, (HDC, CONST PIXELFORMATDESCRIPTOR *));
QGL_WGL(int, wglDescribePixelFormat, (HDC, int, UINT, LPPIXELFORMATDESCRIPTOR));
QGL_WGL(BOOL, wglSetPixelFormat, (HDC, int, CONST PIXELFORMATDESCRIPTOR *));
QGL_WGL(BOOL, wglSwapBuffers, (HDC));
QGL_WGL(HGLRC, wglCreateContext, (HDC));
QGL_WGL(BOOL, wglDeleteContext, (HGLRC));
QGL_WGL(BOOL, wglMakeCurrent, (HDC, HGLRC));
QGL_WGL(PROC, wglGetProcAddress, (LPCSTR));

// WGL_EXT Functions
QGL_WGL_EXT(BOOL, wglSwapIntervalEXT, (int interval));
QGL_WGL_EXT(BOOL, wglGetDeviceGammaRamp3DFX, (HDC, WORD *));
QGL_WGL_EXT(BOOL, wglSetDeviceGammaRamp3DFX, (HDC, WORD *));

// GLX Functions
QGL_GLX(XVisualInfo *, glXChooseVisual, (Display *dpy, int screen, int *attribList));
QGL_GLX(GLXContext, glXCreateContext, (Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct));
QGL_GLX(void, glXDestroyContext, (Display *dpy, GLXContext ctx));
QGL_GLX(Bool, glXMakeCurrent, (Display *dpy, GLXDrawable drawable, GLXContext ctx));
QGL_GLX(Bool, glXCopyContext, (Display *dpy, GLXContext src, GLXContext dst, GLuint mask));
QGL_GLX(Bool, glXSwapBuffers, (Display *dpy, GLXDrawable drawable));
QGL_GLX(void *, glXGetProcAddressARB, (const GLubyte *procName));

// GLX_EXT Functions
