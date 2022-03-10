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

#include <GL/gl.h>

#include "../qcommon/qcommon.h"

#include "../client/cin.h"

#include "qgl.h"
#include "render.h"

#define	REF_VERSION	"GL 0.01"

typedef unsigned int index_t;

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
#define IT_SKY			8
#define IT_CUBEMAP		16

typedef struct image_s
{
	char		name[MAX_QPATH];				// game path, not including extension
	char		extension[8];					// file extension
	int			flags;
	int			width, height;					// source image
	int			upload_width, upload_height;	// after power of two and picmip
	int			texnum;							// gl texture binding
	struct image_s *hash_next;
} image_t;

enum
{
	TEXTURE_UNIT0,
	TEXTURE_UNIT1,
	TEXTURE_UNIT2,
	TEXTURE_UNIT3,
	TEXTURE_UNIT4,
	TEXTURE_UNIT5,
	TEXTURE_UNIT6,
	TEXTURE_UNIT7,
	MAX_TEXTURE_UNITS
};

#define FOG_TEXTURE_WIDTH	256
#define FOG_TEXTURE_HEIGHT	32

#define SHADOW_VOLUMES		0
//#define SHADOW_VOLUMES		2

//===================================================================

enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

#include "r_math.h"
#include "r_mesh.h"
#include "r_shader.h"
#include "r_backend.h"
#include "r_shadow.h"
#include "r_model.h"
#include "r_skin.h"

#define BACKFACE_EPSILON	0.01

#define	ON_EPSILON			0.1			// point on plane side epsilon

#define	SIDE_FRONT			0
#define	SIDE_BACK			1
#define	SIDE_ON				2

//====================================================

extern	char		*r_cubemapSuff[6];
extern	vec3_t		r_cubemapAngles[6];

extern qboolean		r_ligtmapsPacking;

extern	image_t		*r_cintexture;
extern	image_t		*r_notexture;
extern	image_t		*r_whitetexture;
extern	image_t		*r_particletexture;
extern	image_t		*r_dlighttexture;
extern	image_t		*r_fogtexture;
extern	image_t		*r_lightmapTextures[];

extern	entity_t	*currententity;
extern	model_t		*currentmodel;

extern	int			r_visframecount;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys, c_world_leafs;

extern	int			gl_filter_min, gl_filter_max;

extern	float		gldepthmin, gldepthmax;

//
// view origin
//
extern	vec3_t		vup;
extern	vec3_t		vpn;
extern	vec3_t		vright;
extern	vec3_t		r_origin;

extern	qboolean	r_portalview;	// if true, get vis data at
extern	vec3_t		r_portalorg;	// portalorg instead of vieworg

extern	qboolean	r_mirrorview;	// if true, lock pvs

extern	qboolean	r_envview;

extern	cplane_t	r_clipplane;

//
// screen size info
//
extern	int			r_numEntities;
extern	entity_t	r_entities[MAX_ENTITIES];

extern	int			r_numDlights;
extern	dlight_t	r_dlights[MAX_DLIGHTS];

extern	int			r_numPolys;
extern	poly_t		r_polys[MAX_POLYS];

extern	refdef_t	r_refdef;
extern	refdef_t	r_lastRefdef;

extern	int			r_viewcluster, r_oldviewcluster;

extern	entity_t	r_worldent;
extern	model_t		*r_worldmodel;

extern	cvar_t		*r_norefresh;
extern	cvar_t		*r_drawentities;
extern	cvar_t		*r_drawworld;
extern	cvar_t		*r_speeds;
extern	cvar_t		*r_fullbright;
extern	cvar_t		*r_novis;
extern	cvar_t		*r_nocull;
extern	cvar_t		*r_lerpmodels;
extern	cvar_t		*r_fastsky;
extern	cvar_t		*r_ignorehwgamma;
extern	cvar_t		*r_overbrightbits;
extern	cvar_t		*r_mapoverbrightbits;
extern	cvar_t		*r_vertexlight;
extern	cvar_t		*r_flares;
extern	cvar_t		*r_flaresize;
extern	cvar_t		*r_flarefade;
extern	cvar_t		*r_dynamiclight;
extern	cvar_t		*r_detailtextures;
extern	cvar_t		*r_subdivisions;
extern	cvar_t		*r_faceplanecull;
extern	cvar_t		*r_showtris;
extern	cvar_t		*r_shownormals;
extern	cvar_t		*r_ambientscale;
extern	cvar_t		*r_directedscale;
extern	cvar_t		*r_draworder;
extern	cvar_t		*r_packlightmaps;
extern	cvar_t		*r_spherecull;

extern	cvar_t		*gl_extensions;
extern	cvar_t		*gl_ext_swapinterval;
extern	cvar_t		*gl_ext_multitexture;
extern	cvar_t		*gl_ext_compiled_vertex_array;
extern	cvar_t		*gl_ext_texture_env_add;
extern	cvar_t		*gl_ext_texture_env_combine;
extern	cvar_t		*gl_ext_NV_texture_env_combine4;
extern	cvar_t		*gl_ext_compressed_textures;
extern	cvar_t		*gl_ext_texture_edge_clamp;
extern	cvar_t		*gl_ext_texture_filter_anisotropic;
extern	cvar_t		*gl_ext_max_texture_filter_anisotropic;
extern	cvar_t		*gl_ext_max_texture_filter_anisotropic;
extern	cvar_t		*gl_ext_draw_range_elements;
extern	cvar_t		*gl_ext_vertex_buffer_object;
extern	cvar_t		*gl_ext_texture_cube_map;

extern	cvar_t		*r_shadows;
extern	cvar_t		*r_shadows_alpha;
extern	cvar_t		*r_shadows_nudge;

extern	cvar_t		*r_lodbias;
extern	cvar_t		*r_lodscale;

extern	cvar_t		*r_gamma;
extern	cvar_t		*r_colorbits;
extern	cvar_t		*r_texturebits;
extern	cvar_t		*r_texturemode;
extern	cvar_t		*r_mode;
extern	cvar_t		*r_nobind;
extern	cvar_t		*r_picmip;
extern	cvar_t		*r_skymip;
extern	cvar_t		*r_clear;
extern	cvar_t		*r_polyblend;
extern  cvar_t		*r_lockpvs;
extern	cvar_t		*r_screenshot_jpeg;
extern	cvar_t		*r_screenshot_jpeg_quality;

extern	cvar_t		*gl_log;
extern	cvar_t		*gl_finish;
extern	cvar_t		*gl_cull;
extern	cvar_t		*gl_drawbuffer;
extern  cvar_t		*gl_driver;
extern	cvar_t		*gl_swapinterval;

extern	cvar_t		*vid_fullscreen;

extern	mat4x4_t	r_worldview_matrix;
extern	mat4x4_t	r_modelview_matrix;
extern	mat4x4_t	r_projection_matrix;

//====================================================================

static inline qbyte R_FloatToByte( float x )
{
	union {
		float			f;
		unsigned int	i;
	} f2i;

	// shift float to have 8bit fraction at base of number
	f2i.f = x + 32768.0f;
	f2i.i &= 0x7FFFFF;

	// then read as integer and kill float bits...
	return ( qbyte )min( f2i.i, 255 );
}

float R_FastSin( float t );
void R_LatLongToNorm( qbyte latlong[2], vec3_t out );

//====================================================================

//
// r_alias.c
//
void	R_AddAliasModelToList( entity_t *e );
void	R_DrawAliasModel( meshbuffer_t *mb, qboolean shadow );
qboolean R_AliasModelLerpTag( orientation_t *orient, maliasmodel_t *aliasmodel, int framenum, int oldframenum, float lerpfrac, char *name );

//
// r_cin.c
//
void	R_PlayCinematic( cinematics_t *cin );
void	R_RunCinematic( cinematics_t *cin );
void	R_StopCinematic( cinematics_t *cin );
image_t *R_ResampleCinematicFrame( shaderpass_t *pass );

//
// r_image.c
//
void	GL_SelectTexture( int tmu );
void	GL_Bind( int tmu, image_t *tex );
void	GL_TexEnv( GLenum mode );

void	R_InitImages( void );
void	R_ShutdownImages( void );

void	R_ScreenShot_f( void );
void	R_EnvShot_f( void );
void	R_ImageList_f( void );
void	R_TextureMode( char *string );

image_t *R_LoadPic( char *name, qbyte **pic, int width, int height, int flags, int samples );
image_t	*R_FindImage( char *name, int flags );
image_t *R_LoadCubemapPic( char *name, qbyte *pic[6], int width, int height, int flags, int samples );
image_t	*R_FindCubemapImage( char *name, int flags );

void	R_Upload32( qbyte **data, int width, int height, int flags, int *upload_width, int *upload_height, int samples );

//
// r_light.c
//
void	R_MarkLights( void );
void	R_SurfMarkLight( int bit, msurface_t *surf );
void	R_AddDynamicLights( unsigned int dlightbits );
void	R_LightForEntity( entity_t *e, qbyte *bArray );
void	R_LightDirForOrigin( vec3_t origin, vec3_t dir );
void	R_BuildLightmaps( int numLightmaps, qbyte *data, mlightmapRect_t *rects );

//
// r_main.c
//
int 	R_Init( void *hinstance, void *hWnd );
void	R_InitMedia( void );
void	R_FreeMedia( void );
void	R_Restart( void );
void	R_Shutdown( void );
void	R_BeginFrame( float cameraSeparation );
void	R_EndFrame( void );

void	R_RenderView( refdef_t *fd, meshlist_t *list );

qboolean R_CullBox( const vec3_t mins, const vec3_t maxs, const int clipflags );
qboolean R_CullSphere( const vec3_t centre, const float radius, const int clipflags );
qboolean R_VisCullBox( const vec3_t mins, const vec3_t maxs );
qboolean R_VisCullSphere( const vec3_t origin, float radius );

mfog_t	*R_FogForSphere( const vec3_t centre, const float radius );
void	R_LoadIdentity( void );
void	R_RotateForEntity( entity_t *e );
void	R_TranslateForEntity( entity_t *e );
void	R_TransformToScreen_Vec2( vec3_t in, vec2_t out );

void	R_PushFlareSurf( meshbuffer_t *mb );
qboolean R_SpriteOverflow( void );
void	R_AddSpriteModelToList( entity_t *e );
void	R_DrawSpriteModel( meshbuffer_t *mb );
void	R_AddSpritePolyToList( entity_t *e );
void	R_DrawSpritePoly( meshbuffer_t *mb );

//
// r_poly.c
//
qboolean R_PolyOverflow( meshbuffer_t *mb );
void	R_PushPoly( meshbuffer_t *mb );
void	R_DrawPoly( meshbuffer_t *mb );
void	R_AddPolysToList( void );

//
// r_surf.c
//
void	R_MarkLeaves( void );
void	R_DrawWorld( void );

void	R_AddBrushModelToList( entity_t *e );

void	R_CreateMeshForPatch( model_t *mod, dface_t *in, msurface_t *out );
void	R_FixAutosprites( msurface_t *surf );

//
// r_skm.c
//
void	R_AddSkeletalModelToList( entity_t *e );
void	R_DrawSkeletalModel( meshbuffer_t *mb, qboolean shadow );
qboolean R_SkeletalModelLerpAttachment( orientation_t *orient, mskmodel_t *skmodel, int framenum, int oldframenum, float lerpfrac, char *name );

//
// r_warp.c
//
skydome_t *R_CreateSkydome( float skyheight, shader_t **farboxShaders, shader_t	**nearboxShaders );
void	R_FreeSkydome( skydome_t *skydome );
void	R_ClearSkyBox( void );
void	R_DrawSky( shader_t *shader );
void	R_AddSkySurface( msurface_t *fa );

//====================================================================

/*
** GL config stuff
*/
#define GL_RENDERER_VOODOO		0x00000001
#define GL_RENDERER_VOODOO2   	0x00000002
#define GL_RENDERER_VOODOO_RUSH	0x00000004
#define GL_RENDERER_BANSHEE		0x00000008
#define	GL_RENDERER_3DFX		0x0000000F

#define GL_RENDERER_PCX1		0x00000010
#define GL_RENDERER_PCX2		0x00000020
#define GL_RENDERER_PMX			0x00000040
#define	GL_RENDERER_POWERVR		0x00000070

#define GL_RENDERER_PERMEDIA2	0x00000100
#define GL_RENDERER_GLINT_MX	0x00000200
#define GL_RENDERER_GLINT_TX	0x00000400
#define GL_RENDERER_3DLABS_MISC	0x00000800
#define	GL_RENDERER_3DLABS		0x00000F00

#define GL_RENDERER_REALIZM		0x00001000
#define GL_RENDERER_REALIZM2	0x00002000
#define	GL_RENDERER_INTERGRAPH	0x00003000

#define GL_RENDERER_3DPRO		0x00004000
#define GL_RENDERER_REAL3D		0x00008000
#define GL_RENDERER_RIVA128		0x00010000
#define GL_RENDERER_DYPIC		0x00020000

#define GL_RENDERER_V1000		0x00040000
#define GL_RENDERER_V2100		0x00080000
#define GL_RENDERER_V2200		0x00100000
#define	GL_RENDERER_RENDITION	0x001C0000

#define GL_RENDERER_O2          0x00100000
#define GL_RENDERER_IMPACT      0x00200000
#define GL_RENDERER_RE			0x00400000
#define GL_RENDERER_IR			0x00800000
#define	GL_RENDERER_SGI			0x00F00000

#define GL_RENDERER_MCD			0x01000000
#define GL_RENDERER_OTHER		0x80000000

typedef struct
{
	int         renderer;
	const char	*rendererString;
	const char	*vendorString;
	const char	*versionString;
	const char	*extensionsString;

	qboolean	allowCDS;

	int			maxTextureSize;
	int			maxTextureUnits;
	int			maxTextureCubemapSize;

	qboolean	compiledVertexArray;
	qboolean	multiTexture;
	qboolean	swapInterval;
	qboolean	textureCubeMap;
	qboolean	textureEnvAdd;
	qboolean	textureEnvCombine;
	qboolean	NVTextureEnvCombine4;
	qboolean	textureEdgeClamp;
	qboolean	textureFilterAnisotropic;
	float		maxTextureFilterAnisotropic;
	qboolean	compressedTextures;
	qboolean	drawRangeElements;
	qboolean	vertexBufferObject;
} glconfig_t;

typedef struct
{
	int			width, height;
	qboolean	fullScreen;

	qboolean	initializedMedia;

	int			previousMode;

	int			currentTMU;
	int			currentTextures[MAX_TEXTURE_UNITS];
	int			currentEnvModes[MAX_TEXTURE_UNITS];

	float		cameraSeparation;
	qboolean	stereoEnabled;
	qboolean	stencilEnabled;
	qboolean	in2DMode;

	qboolean	hwGamma;
	unsigned short orignalGammaRamp[3*256];

	float		pow2MapOvrbr;
	float		invPow2Ovrbr;
} glstate_t;

extern glconfig_t  glConfig;
extern glstate_t   glState;

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_BeginFrame( void );
void		GLimp_EndFrame( void );
int 		GLimp_Init( void *hinstance, void *hWnd );
void		GLimp_Shutdown( void );
int     	GLimp_SetMode( int mode, qboolean fullscreen );
void		GLimp_AppActivate( qboolean active );
void		GLimp_EnableLogging( qboolean enable );
void		GLimp_LogNewFrame( void );
qboolean	GLimp_GetGammaRamp( size_t stride, unsigned short *ramp );
void		GLimp_SetGammaRamp( size_t stride, unsigned short *ramp );

void		VID_NewWindow( int width, int height);
qboolean	VID_GetModeInfo( int *width, int *height, int mode );

