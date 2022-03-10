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

#ifdef __cplusplus
# define QGL_EXTERN extern "C"
#else
# define QGL_EXTERN extern
#endif

#ifdef _WIN32
# include <windows.h>

# define QGL_WGL(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
# define QGL_WGL_EXT(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
# define QGL_GLX(type,name,params)
# define QGL_GLX_EXT(type,name,params)
#endif

#if defined (__linux__) || defined (__FreeBSD__)
# define QGL_WGL(type,name,params)
# define QGL_WGL_EXT(type,name,params)
# define QGL_GLX(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
# define QGL_GLX_EXT(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
#endif

#if defined (__APPLE__) || defined (MACOSX)
# define QGL_WGL(type,name,params)
# define QGL_WGL_EXT(type,name,params)
# define QGL_GLX(type,name,params)
# define QGL_GLX_EXT(type,name,params)
#endif

#include "../qcommon/qcommon.h"

#define QGL_FUNC(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;
#define QGL_EXT(type,name,params) QGL_EXTERN type (APIENTRY * q##name) params;

#include "qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC

#include "../client/cin.h"
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
#define IT_HEIGHTMAP	32
#define IT_FLIPX		64
#define IT_FLIPY		128
#define IT_FLIPDIAGONAL	256
#define IT_NORGB		512
#define IT_NOALPHA		1024
#define IT_NOCOMPRESS	2048

#define IT_CINEMATIC	(IT_NOPICMIP|IT_NOMIPMAP|IT_CLAMP|IT_NOCOMPRESS)

typedef struct image_s
{
	char		name[MAX_QPATH];				// game path, not including extension
	char		extension[8];					// file extension
	int			flags;
	int			width, height, depth;			// source image
	int			upload_width, upload_height, upload_depth;	// after power of two and picmip
	int			texnum;							// gl texture binding
	float		bumpScale;
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

#define VID_DEFAULTMODE		"0"

#define SHADOW_PLANAR		1
//#define SHADOW_VOLUMES		0
#define SHADOW_VOLUMES		(SHADOW_PLANAR+1)

//#define VERTEX_BUFFER_OBJECTS

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

#define BACKFACE_EPSILON	0.01

#define	ON_EPSILON			0.1			// point on plane side epsilon

#define Z_NEAR				4

#define	SIDE_FRONT			0
#define	SIDE_BACK			1
#define	SIDE_ON				2

#define RP_NONE				0x0
#define RP_MIRRORVIEW		0x1			// lock pvs at vieworg
#define RP_PORTALVIEW		0x2
#define RP_ENVVIEW			0x4
#define RP_NOSKY			0x8
#define RP_SKYPORTALVIEW	0x10

//====================================================

typedef struct
{
	vec3_t		origin;
	vec3_t		color;
	vec3_t		mins, maxs;
	float		intensity;
	shader_t	*shader;
} dlight_t;

typedef struct
{
	int			features;
	int			lightmapNum[MAX_LIGHTMAPS];
	int			lightmapStyles[MAX_LIGHTMAPS];
	int			vertexStyles[MAX_LIGHTMAPS];
	float		stOffset[MAX_LIGHTMAPS][2];
} superLightStyle_t;

typedef struct
{
	int			params;		// rendering parameters

	refdef_t	refdef;
	int			scissor[4];
	meshlist_t	*meshlist;	// meshes to be rendered

	entity_t	*currententity;
	model_t		*currentmodel;
	entity_t	*previousentity;

	//
	// view origin
	//
	vec3_t		viewOrigin;
	vec3_t		vup, vpn, vright;
	cplane_t	frustum[4];
	float		farClip;

	mat4x4_t	worldviewMatrix;
	mat4x4_t	modelviewMatrix;
	mat4x4_t	projectionMatrix;

	float		skyMins[2][6];
	float		skyMaxs[2][6];

	float		lod_dist_scale_for_fov;

	float		fog_dist_to_eye[MAX_MAP_FOGS];

	vec3_t		pvsOrigin;
	cplane_t	clipPlane;
} refinst_t;

//====================================================

extern	char		*r_cubemapSuff[6];

extern	image_t		*r_cintexture;
extern	image_t		*r_notexture;
extern	image_t		*r_whitetexture;
extern	image_t		*r_blacktexture;
extern	image_t		*r_particletexture;
extern	image_t		*r_dlighttexture;
extern	image_t		*r_fogtexture;
extern	image_t		*r_lightmapTextures[];
extern	image_t		*r_blanknormalmaptexture;

extern	int			r_visframecount;
extern	int			r_framecount;
extern	int			c_brush_polys, c_world_leafs;

extern	int			r_mark_leaves, r_world_node;
extern	int			r_add_polys, r_add_entities;
extern	int			r_sort_meshes, r_draw_meshes;

extern	int			gl_filter_min, gl_filter_max;

extern	float		gldepthmin, gldepthmax;

//
// screen size info
//
extern	int			r_numEntities;
extern	entity_t	r_entities[MAX_ENTITIES];

extern	int			r_numDlights;
extern	dlight_t	r_dlights[MAX_DLIGHTS];

extern	int			r_numPolys;
extern	poly_t		r_polys[MAX_POLYS];

extern	lightstyle_t	r_lightStyles[MAX_LIGHTSTYLES];

#define NUM_CUSTOMCOLORS	16
extern	byte_vec4_t	r_customColors[NUM_CUSTOMCOLORS];

extern	refdef_t	r_lastRefdef;

extern	int			r_viewcluster, r_oldviewcluster;

extern	float		r_farclip_min, r_farclip_bias;

extern	entity_t	*r_worldent;
extern	model_t		*r_worldmodel;

extern	cvar_t		*r_norefresh;
extern	cvar_t		*r_drawentities;
extern	cvar_t		*r_drawworld;
extern	cvar_t		*r_speeds;
extern	cvar_t		*r_fullbright;
extern	cvar_t		*r_lightmap;
extern	cvar_t		*r_novis;
extern	cvar_t		*r_nocull;
extern	cvar_t		*r_lerpmodels;
extern	cvar_t		*r_fastsky;
extern	cvar_t		*r_ignorehwgamma;
extern	cvar_t		*r_overbrightbits;
extern	cvar_t		*r_mapoverbrightbits;
extern	cvar_t		*r_flares;
extern	cvar_t		*r_flaresize;
extern	cvar_t		*r_flarefade;
extern	cvar_t		*r_dynamiclight;
extern	cvar_t		*r_detailtextures;
extern	cvar_t		*r_subdivisions;
extern	cvar_t		*r_faceplanecull;
extern	cvar_t		*r_showtris;
extern	cvar_t		*r_shownormals;
extern	cvar_t		*r_draworder;
extern	cvar_t		*r_packlightmaps;
extern	cvar_t		*r_spherecull;
extern	cvar_t		*r_maxlmblocksize;
extern	cvar_t		*r_portalonly;

extern	cvar_t		*r_lighting_bumpscale;
extern	cvar_t		*r_lighting_deluxemapping;
extern	cvar_t		*r_lighting_diffuse2heightmap;
extern	cvar_t		*r_lighting_specular;
extern	cvar_t		*r_lighting_glossintensity;
extern	cvar_t		*r_lighting_glossexponent;
extern	cvar_t		*r_lighting_models_followdeluxe;
extern	cvar_t		*r_lighting_ambientscale;
extern	cvar_t		*r_lighting_directedscale;

extern	cvar_t		*r_shadows;
extern	cvar_t		*r_shadows_alpha;
extern	cvar_t		*r_shadows_nudge;
extern	cvar_t		*r_shadows_projection_distance;

extern	cvar_t		*r_bloom;
extern	cvar_t		*r_bloom_alpha;
extern	cvar_t		*r_bloom_diamond_size;
extern	cvar_t		*r_bloom_intensity;
extern	cvar_t		*r_bloom_darken;
extern	cvar_t		*r_bloom_sample_size;
extern	cvar_t		*r_bloom_fast_sample;

extern	cvar_t		*gl_extensions;
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
extern	cvar_t		*gl_ext_BGRA;
extern	cvar_t		*gl_ext_texture3D;
extern	cvar_t		*gl_ext_GLSL;

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
extern	cvar_t		*r_swapinterval;

extern	cvar_t		*gl_finish;
extern	cvar_t		*gl_delayfinish;
extern	cvar_t		*gl_cull;
extern	cvar_t		*gl_drawbuffer;
extern  cvar_t		*gl_driver;

extern	cvar_t		*vid_fullscreen;

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
void R_LatLongToNorm( const qbyte latlong[2], vec3_t out );

//====================================================================

//
// r_alias.c
//
void	R_AddAliasModelToList( entity_t *e );
void	R_DrawAliasModel( const meshbuffer_t *mb, qboolean shadow );
qboolean R_AliasModelLerpTag( orientation_t *orient, maliasmodel_t *aliasmodel, int framenum, int oldframenum, float lerpfrac, const char *name );

//
// r_bloom.c
//
void R_InitBloomTextures( void ); 
void R_BloomBlend( refdef_t *fd );

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
void	GL_LoadTexMatrix( mat4x4_t m );
void	GL_LoadIdentityTexMatrix( void );
void	GL_EnableTexGen( int coord, int mode );
void	GL_SetTexCoordArrayMode( int mode );

void	GL_Cull( int cull );

void	R_InitImages( void );
void	R_ShutdownImages( void );

void	R_ScreenShot_f( void );
void	R_EnvShot_f( void );
void	R_ImageList_f( void );
void	R_TextureMode( char *string );

image_t *R_LoadPic( char *name, qbyte **pic, int width, int height, int flags, int samples );
image_t	*R_FindImage( char *name, int flags, float bumpScale );
image_t *R_LoadCubemapPic( char *name, qbyte *pic[6], int width, int height, int flags, int samples );
image_t	*R_FindCubemapImage( char *name, int flags );

void	R_Upload32( qbyte **data, int width, int height, int flags, int *upload_width, int *upload_height, int samples, qboolean subImage );

//
// r_light.c
//
#define DLIGHT_SCALE		0.5f
#define MAX_SUPER_STYLES	1023

extern	int		r_numSuperLightStyles;
extern	superLightStyle_t	r_superLightStyles[MAX_SUPER_STYLES];

void	R_LightBounds( const vec3_t origin, float intensity, vec3_t mins, vec3_t maxs );
qboolean R_SurfPotentiallyLit( msurface_t *surf );
unsigned int R_AddSurfDlighbits( msurface_t *surf, unsigned int dlightbits );
void	R_AddDynamicLights( unsigned int dlightbits, int state );
void	R_LightForEntity( entity_t *e, qbyte *bArray );
void	R_LightForOrigin( vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius );
void	R_BuildLightmaps( int numLightmaps, int w, int h, const qbyte *data, mlightmapRect_t *rects );
void	R_InitLightStyles( void );
int		R_AddSuperLightStyle( const int *lightmaps, const qbyte *lightmapStyles, const qbyte *vertexStyles, mlightmapRect_t **lmRects );
void	R_SortSuperLightStyles( void );

//
// r_main.c
//
int 	R_Init( void *hinstance, void *hWnd );
void	R_InitMedia( void );
void	R_FreeMedia( void );
void	R_Restart( void );
void	R_Shutdown( void );
void	R_BeginFrame( float cameraSeparation, qboolean forceClear );
void	R_EndFrame( void );
void	R_RenderView( refdef_t *fd );

qboolean R_CullBox( const vec3_t mins, const vec3_t maxs, const int clipflags );
qboolean R_CullSphere( const vec3_t centre, const float radius, const int clipflags );
qboolean R_VisCullBox( const vec3_t mins, const vec3_t maxs );
qboolean R_VisCullSphere( const vec3_t origin, float radius );
qboolean R_CullModel( entity_t *e, vec3_t mins, vec3_t maxs, float radius );

mfog_t	*R_FogForSphere( const vec3_t centre, const float radius );
qboolean R_CompletelyFogged( mfog_t *fog, vec3_t origin, float radius );

void	R_LoadIdentity( void );
void	R_RotateForEntity( entity_t *e );
void	R_TranslateForEntity( entity_t *e );
void	R_TransformToScreen_Vec3( vec3_t in, vec3_t out );
void	R_TransformVectorToScreen( refdef_t *rd, vec3_t in, vec2_t out );

void	R_PushFlareSurf( const meshbuffer_t *mb );

qboolean R_SpriteOverflow( void );

qboolean R_PushSpriteModel( const meshbuffer_t *mb );
qboolean R_PushSpritePoly( const meshbuffer_t *mb );

void	R_AddSpriteModelToList( entity_t *e );
void	R_AddSpritePolyToList( entity_t *e );

//
// r_mesh.c
//

extern	meshlist_t	r_worldlist;

void R_InitMeshLists( void );
void R_FreeMeshLists( void );

meshbuffer_t *R_AddMeshToList( int type, mfog_t *fog, shader_t *shader, int infokey );

void R_SortMeshes( void );
void R_DrawMeshes( qboolean triangleOutlines );
void R_DrawTriangleOutlines( void );
void R_DrawPortals( void );

void R_DrawPortalSurface( const meshbuffer_t *mb );
void R_DrawCubemapView( vec3_t origin, vec3_t angles, int size );
void R_DrawSkyPortal( skyportal_t *skyportal, vec3_t mins, vec3_t maxs );

void R_BuildTangentVectors( int numVertexes, vec3_t *xyzArray, vec3_t *normalsArray, vec2_t *stArray, int numTris, index_t *indexes, vec4_t *sVectorsArray, vec3_t *tVectorsArray );

//
// r_program.c
//
#define DEFAULT_GLSL_PROGRAM	"*r_defaultProgram"

enum
{
	PROGRAM_APPLY_LIGHTSTYLE0			= 1 << 0,
	PROGRAM_APPLY_LIGHTSTYLE1			= 1 << 1,
	PROGRAM_APPLY_LIGHTSTYLE2			= 1 << 2,
	PROGRAM_APPLY_LIGHTSTYLE3			= 1 << 3,
	PROGRAM_APPLY_SPECULAR				= 1 << 4,
	PROGRAM_APPLY_DIRECTIONAL_LIGHT		= 1 << 5,
	PROGRAM_APPLY_FB_LIGHTMAP			= 1 << 6
};

void R_InitGLSLPrograms( void );
int R_FindGLSLProgram( const char *name );
int R_RegisterGLSLProgram( const char *name, const char *string, int features );
int R_GetProgramObject( int index );
void R_UpdateProgramUniforms( int index, vec3_t eyeOrigin,  vec3_t lightOrigin, vec3_t lightDir, vec4_t ambient, vec4_t diffuse, superLightStyle_t *superLightStyle );
void R_ShutdownGLSLPrograms( void );
void R_ProgramList_f( void );

//
// r_poly.c
//
void	R_PushPoly( const meshbuffer_t *mb );
void	R_AddPolysToList( void );
qboolean R_SurfPotentiallyFragmented( msurface_t *surf );
int		R_GetClippedFragments( vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments );

//
// r_surf.c
//
void	R_MarkLeaves( void );
void	R_DrawWorld( void );
qboolean R_SurfPotentiallyVisible( msurface_t *surf );
void	R_AddBrushModelToList( entity_t *e );
void	R_FixAutosprites( msurface_t *surf );

//
// r_skin.c
//
void R_InitSkinFiles( void );
void R_ShutdownSkinFiles( void );
struct skinfile_s *R_SkinFile_Load( char *name );
struct skinfile_s *R_RegisterSkinFile( char *name );
shader_t *R_FindShaderForSkinFile( struct skinfile_s *skinfile, char *meshname );

//
// r_skm.c
//
void	R_AddSkeletalModelToList( entity_t *e );
void	R_DrawSkeletalModel( const meshbuffer_t *mb, qboolean shadow );

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

enum
{
	GLSTATE_NONE = 0,

	//
	// glBlendFunc args
	//
	GLSTATE_SRCBLEND_ZERO					= 1,
	GLSTATE_SRCBLEND_ONE					= 2,
	GLSTATE_SRCBLEND_DST_COLOR				= 1|2,
	GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR	= 4,
	GLSTATE_SRCBLEND_SRC_ALPHA				= 1|4,
	GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA	= 2|4,
	GLSTATE_SRCBLEND_DST_ALPHA				= 1|2|4,
	GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA	= 8,

	GLSTATE_DSTBLEND_ZERO					= 16,
	GLSTATE_DSTBLEND_ONE					= 32,
	GLSTATE_DSTBLEND_SRC_COLOR				= 16|32,
	GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR	= 64,
	GLSTATE_DSTBLEND_SRC_ALPHA				= 16|64,
	GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA	= 32|64,
	GLSTATE_DSTBLEND_DST_ALPHA				= 16|32|64,
	GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA	= 128,

	GLSTATE_BLEND_MTEX						= 0x100,

	GLSTATE_AFUNC_GT0						= 0x200,
	GLSTATE_AFUNC_LT128						= 0x400,
	GLSTATE_AFUNC_GE128						= 0x800,

	GLSTATE_DEPTHWRITE						= 0x1000,
	GLSTATE_DEPTHFUNC_EQ					= 0x2000,

	GLSTATE_OFFSET_FILL						= 0x4000,
	GLSTATE_NO_DEPTH_TEST					= 0x8000,

	GLSTATE_MARK_END						= 0x10000	// SHADERPASS_MARK_BEGIN
};

#define GLSTATE_MASK			(GLSTATE_MARK_END-1)

// #define SHADERPASS_SRCBLEND_MASK (((GLSTATE_SRCBLEND_DST_ALPHA)<<1)-GLSTATE_SRCBLEND_ZERO)
#define GLSTATE_SRCBLEND_MASK	0xF

// #define SHADERPASS_SRCBLEND_MASK (((GLSTATE_DSTBLEND_DST_ALPHA)<<1)-GLSTATE_DSTBLEND_ZERO)
#define GLSTATE_DSTBLEND_MASK	0xF0

#define GLSTATE_ALPHAFUNC		(GLSTATE_AFUNC_GT0|GLSTATE_AFUNC_LT128|GLSTATE_AFUNC_GE128)

typedef struct
{
	int			pow2MapOvrbr;

	qboolean	lightmapsPacking;
	qboolean	deluxeMaps;				// true if there are valid deluxemaps in the .bsp
	qboolean	deluxeMappingEnabled;	// true if deluxeMaps is true and r_lighting_deluxemaps->integer != 0
} mapconfig_t;

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
	int			max3DTextureSize;

	qboolean	compiledVertexArray;
	qboolean	multiTexture;
	qboolean	textureCubeMap;
	qboolean	textureEnvAdd;
	qboolean	textureEnvCombine;
	qboolean	NVTextureEnvCombine4;
	qboolean	textureEdgeClamp;
	qboolean	textureFilterAnisotropic;
	int			maxTextureFilterAnisotropic;
	qboolean	compressedTextures;
	qboolean	drawRangeElements;
#ifdef VERTEX_BUFFER_OBJECTS
	qboolean	vertexBufferObject;
#endif
	qboolean	BGRA;
	qboolean	texture3D;
	qboolean	GLSL;
} glconfig_t;

typedef struct
{
	int			flags;

	int			width, height;
	qboolean	fullScreen;

	qboolean	initializedMedia;

	int			previousMode;

	int			currentTMU;
	int			currentTextures[MAX_TEXTURE_UNITS];
	int			currentEnvModes[MAX_TEXTURE_UNITS];
	qboolean	texIdentityMatrix[MAX_TEXTURE_UNITS];
	int			genSTEnabled[MAX_TEXTURE_UNITS];			// 0 - disabled, OR 1 - S, OR 2 - T, OR 4 - R
	int			texCoordArrayMode[MAX_TEXTURE_UNITS];	// 0 - disabled, 1 - enabled, 2 - cubemap

	int			faceCull;

	float		cameraSeparation;
	qboolean	stereoEnabled;
	qboolean	stencilEnabled;
	qboolean	in2DMode;

	qboolean	hwGamma;
	unsigned short orignalGammaRamp[3*256];
} glstate_t;

extern mapconfig_t	mapConfig;
extern glconfig_t	glConfig;
extern glstate_t	glState;
extern refinst_t	ri;

void GL_SetState( int state );

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_BeginFrame( void );
void		GLimp_EndFrame( void );
int 		GLimp_Init( void *hinstance, void *wndproc );
void		GLimp_Shutdown( void );
int     	GLimp_SetMode( int mode, qboolean fullscreen );
void		GLimp_AppActivate( qboolean active );
qboolean	GLimp_GetGammaRamp( size_t stride, unsigned short *ramp );
void		GLimp_SetGammaRamp( size_t stride, unsigned short *ramp );

void		VID_NewWindow( int width, int height);
qboolean	VID_GetModeInfo( int *width, int *height, qboolean *wideScreen, int mode );
