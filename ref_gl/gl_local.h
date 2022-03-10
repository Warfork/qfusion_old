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

#ifdef _WIN32
#  include <windows.h>
#endif

#include <stdio.h>

#include <GL/gl.h>
#include <math.h>

#ifndef __linux__
#ifndef GL_COLOR_INDEX8_EXT
#define GL_COLOR_INDEX8_EXT GL_COLOR_INDEX
#endif
#endif

#include "../client/ref.h"

#include "qgl.h"

#define	REF_VERSION	"GL 0.01"

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


#ifndef __VIDDEF_T
#define __VIDDEF_T
typedef struct
{
	unsigned		width, height;			// coordinates from main game
} viddef_t;
#endif

extern	viddef_t	vid;


/*

  skins will be outline flood filled and mip mapped
  pics and sprites with alpha will be outline flood filled
  pic won't be mip mapped

  model skin
  sprite frame
  wall texture
  pic

*/

#define IT_CLAMP		1
#define IT_NOMIPMAP		2
#define IT_NOPICMIP		4
#define IT_FLOODFILL	8
#define IT_PIC			16
#define IT_FOG			32

typedef struct image_s
{
	char		name[MAX_QPATH];			// game path, including extension
	int			flags;
	int			width, height;				// source image
	int			upload_width, upload_height;	// after power of two and picmip
	int			registration_sequence;		// 0 = free
	int			texnum;						// gl texture binding
	float		sl, tl, sh, th;				// 0,0 - 1,1 unless part of the scrap
	qboolean	scrap;
} image_t;

#define	TEXNUM_LIGHTMAPS	1024
#define	TEXNUM_SCRAPS		1152
#define	TEXNUM_IMAGES		1153

#define	MAX_GLTEXTURES		1024

//===================================================================

typedef enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

#include "gl_shader.h"
#include "gl_backend.h"
#include "gl_model.h"

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

void GL_SetDefaultState( void );
void GL_UpdateSwapInterval( void );

extern	float	gldepthmin, gldepthmax;

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01


//====================================================

extern	image_t		gltextures[MAX_GLTEXTURES];
extern	int			numgltextures;


extern	image_t		*r_notexture;
extern	image_t		*r_whitetexture;
extern	image_t		*r_particletexture;
extern	image_t		*r_dlighttexture;
extern	image_t		*r_fogtexture;
extern	entity_t	*currententity;
extern	model_t		*currentmodel;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;


extern	int			gl_filter_min, gl_filter_max;

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_newrefdef;
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_lefthand;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_lerpmodels;
extern	cvar_t	*r_fastsky;
extern	cvar_t	*r_ignorehwgamma;
extern	cvar_t	*r_overbrightbits;
extern	cvar_t	*r_mapoverbrightbits;
extern	cvar_t	*r_vertexlight;
extern	cvar_t	*r_flares;
extern	cvar_t	*r_flaresize;
extern	cvar_t	*r_flarefade;
extern	cvar_t	*r_dynamiclight;
extern	cvar_t	*r_detailtextures;
extern	cvar_t	*r_subdivisions;
extern	cvar_t	*r_faceplanecull;

extern cvar_t	*gl_ext_swapinterval;
extern cvar_t	*gl_ext_multitexture;
extern cvar_t	*gl_ext_mtexcombine;
extern cvar_t	*gl_ext_compiled_vertex_array;

extern	cvar_t	*gl_bitdepth;
extern	cvar_t	*gl_mode;
extern	cvar_t	*gl_log;
extern	cvar_t	*gl_lightmap;
extern	cvar_t	*gl_shadows;
extern  cvar_t  *gl_monolightmap;
extern	cvar_t	*gl_nobind;
extern	cvar_t	*gl_round_down;
extern	cvar_t	*gl_picmip;
extern	cvar_t	*gl_skymip;
extern	cvar_t	*gl_showtris;
extern	cvar_t	*gl_finish;
extern	cvar_t	*gl_ztrick;
extern	cvar_t	*gl_clear;
extern	cvar_t	*gl_cull;
extern	cvar_t	*gl_polyblend;
extern	cvar_t	*gl_playermip;
extern	cvar_t	*gl_drawbuffer;
extern	cvar_t	*gl_3dlabs_broken;
extern  cvar_t  *gl_driver;
extern	cvar_t	*gl_swapinterval;
extern	cvar_t	*gl_texturemode;
extern	cvar_t	*gl_texturealphamode;
extern	cvar_t	*gl_texturesolidmode;
extern  cvar_t  *gl_lockpvs;

extern	cvar_t	*gl_screenshot_jpeg;			// Heffo - JPEG Screenshots
extern	cvar_t	*gl_screenshot_jpeg_quality;	// Heffo - JPEG Screenshots

extern	cvar_t	*vid_fullscreen;
extern	cvar_t	*vid_gamma;

extern	cvar_t	*intensity;

extern	int		gl_lightmap_format;
extern	int		gl_solid_format;
extern	int		gl_alpha_format;
extern	int		gl_tex_solid_format;
extern	int		gl_tex_alpha_format;

extern	int		c_visible_lightmaps;
extern	int		c_visible_textures;

extern	mat4_t		r_world_matrix;
extern	mat3x3_t	r_inverse_world_matrix;

void R_TranslatePlayerSkin (int playernum);
void GL_Bind (int texnum);
void GL_MBind( GLenum target, int texnum );
void GL_TexEnv( GLenum value );
void GL_EnableMultitexture( qboolean enable );
void GL_SelectTexture( GLenum );

void R_LightForPoint ( vec3_t p, vec3_t ambient, vec3_t diffuse, vec3_t direction );

//====================================================================

extern	model_t	*r_worldmodel;

extern	unsigned	d_8to24table[256];
extern	float		d_8to24floattable[256][3];

extern	int		registration_sequence;

extern	vec3_t	r_uvnorms[255][255];

void V_AddBlend (float r, float g, float b, float a, float *v_blend);

int 	R_Init( void *hinstance, void *hWnd );
void	R_Shutdown( void );

void R_RenderView (refdef_t *fd);
void GL_ScreenShot_f (void);
void R_DrawAliasModel (entity_t *e);
void R_DrawBrushModel (entity_t *e);
void R_DrawSpriteModel (entity_t *e);
void R_DrawMd3Model (entity_t *e);
void R_DrawBeam( entity_t *e );
void R_DrawWorld (void);
void R_DrawSortedPolys (void);
void R_AddMd3ToList (entity_t *e);
void R_RenderBrushPoly (msurface_t *fa);
void R_InitParticleTexture (void);
void R_InitBubble(void);
void R_InitMd3 (void);
void Draw_InitLocal (void);
void GL_SubdivideSurface (msurface_t *fa);
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
void R_RotateForEntity (entity_t *e);
void R_MarkLeaves (void);
void R_AddDynamicLights (msurface_t *s, vec4_t *vertexArray);
void R_DrawFastSky (void);

void R_AddSkySurface (msurface_t *fa);
void R_ClearSkyBox (void);
void R_SetSky (char *name);
void R_CreateSkydome (void);
void R_DrawSkydome (void);

void COM_StripExtension (char *in, char *out);

void	Draw_Pic (int x, int y, char *name);
void	Draw_StretchPic ( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *colour, shader_t *shader );
void	Draw_Char (int x, int y, int num, fontstyle_t fntstl, vec4_t colour);
void	Draw_StringLen (int x, int y, char *str, int len, fontstyle_t fntstl, vec4_t colour);
void	Draw_TileClear (int x, int y, int w, int h, char *name);
void	Draw_Fill (int x, int y, int w, int h, int c);
void	Draw_FadeScreen (void);
void	Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);

void	R_BeginFrame( float camera_separation );
void	R_SwapBuffers( int );
void	R_SetPalette ( const unsigned char *palette);

int		Draw_GetPalette (void);

void	GL_ResampleTexture (unsigned *indata, int inwidth, int inheight, unsigned *outdata, int outwidth, int outheight);

void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height);
image_t *GL_LoadPic (char *name, byte *pic, int width, int height, int flags, int bits);
image_t	*GL_FindImage (char *name, int flags);
image_t	*GL_LoadImage (char *name, int flags);
void	GL_TextureMode( char *string );
void	GL_ImageList_f (void);

void	GL_InitImages (void);
void	GL_ShutdownImages (void);

void	GL_FreeUnusedImages (void);

void GL_TextureAlphaMode( char *string );
void GL_TextureSolidMode( char *string );

/*
** GL extension emulation functions
*/
void GL_DrawParticles( int n, const particle_t particles[], const unsigned colortable[768] );

/*
** GL config stuff
*/
#define GL_RENDERER_VOODOO		0x00000001
#define GL_RENDERER_VOODOO2   	0x00000002
#define GL_RENDERER_VOODOO_RUSH	0x00000004
#define GL_RENDERER_BANSHEE		0x00000008
#define		GL_RENDERER_3DFX		0x0000000F

#define GL_RENDERER_PCX1		0x00000010
#define GL_RENDERER_PCX2		0x00000020
#define GL_RENDERER_PMX			0x00000040
#define		GL_RENDERER_POWERVR		0x00000070

#define GL_RENDERER_PERMEDIA2	0x00000100
#define GL_RENDERER_GLINT_MX	0x00000200
#define GL_RENDERER_GLINT_TX	0x00000400
#define GL_RENDERER_3DLABS_MISC	0x00000800
#define		GL_RENDERER_3DLABS	0x00000F00

#define GL_RENDERER_REALIZM		0x00001000
#define GL_RENDERER_REALIZM2	0x00002000
#define		GL_RENDERER_INTERGRAPH	0x00003000

#define GL_RENDERER_3DPRO		0x00004000
#define GL_RENDERER_REAL3D		0x00008000
#define GL_RENDERER_RIVA128		0x00010000
#define GL_RENDERER_DYPIC		0x00020000

#define GL_RENDERER_V1000		0x00040000
#define GL_RENDERER_V2100		0x00080000
#define GL_RENDERER_V2200		0x00100000
#define		GL_RENDERER_RENDITION	0x001C0000

#define GL_RENDERER_O2          0x00100000
#define GL_RENDERER_IMPACT      0x00200000
#define GL_RENDERER_RE			0x00400000
#define GL_RENDERER_IR			0x00800000
#define		GL_RENDERER_SGI			0x00F00000

#define GL_RENDERER_MCD			0x01000000
#define GL_RENDERER_OTHER		0x80000000

typedef struct
{
	int         renderer;
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;

	qboolean	allow_cds;

	qboolean	env_add;

	qboolean	mtexcombine;
	qboolean	nvtexcombine4;
} glconfig_t;

typedef struct
{
	qboolean fullscreen;

	int prev_mode;

	int lightmap_textures;

	int	currenttextures[2];
	int currenttmu;

	float camera_separation;
	qboolean stereo_enabled;

	qboolean stencil_enabled;

	qboolean gammaramp;

	qboolean blend;
	qboolean alphatest;

	qboolean in2d;
} glstate_t;

extern glconfig_t  gl_config;
extern glstate_t   gl_state;

#define GLSTATE_DISABLE_ALPHATEST	if ( gl_state.alphatest ) { qglDisable(GL_ALPHA_TEST); gl_state.alphatest = false; }
#define GLSTATE_ENABLE_ALPHATEST	if ( !gl_state.alphatest ) { qglEnable(GL_ALPHA_TEST); gl_state.alphatest = true; }

#define GLSTATE_DISABLE_BLEND		if ( gl_state.blend ) { qglDisable(GL_BLEND); gl_state.blend = false; }
#define GLSTATE_ENABLE_BLEND		if ( !gl_state.blend ) { qglEnable(GL_BLEND); gl_state.blend = true; }

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_BeginFrame( float camera_separation );
void		GLimp_EndFrame( void );
int 		GLimp_Init( void *hinstance, void *hWnd );
void		GLimp_Shutdown( void );
int     	GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen );
void		GLimp_AppActivate( qboolean active );
void		GLimp_EnableLogging( qboolean enable );
void		GLimp_LogNewFrame( void );

void		VID_NewWindow ( int width, int height);
qboolean	VID_GetModeInfo( int *width, int *height, int mode );