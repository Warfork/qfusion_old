/*
Copyright (C) 2002-2003 Victor Luchits

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
#ifndef __SHADER_H__
#define __SHADER_H__

#define MAX_SHADERS					4096
#define MAX_SHADER_PASSES			8
#define MAX_SHADER_DEFORMVS			8
#define MAX_SHADER_ANIM_FRAMES		16
#define MAX_SHADER_TCMODS			8

// shader types
enum
{
	SHADER_INVALID = -1,
	SHADER_UNKNOWN,
	SHADER_BSP,
	SHADER_BSP_VERTEX,
	SHADER_BSP_FLARE,
	SHADER_MD3,
	SHADER_2D,
	SHADER_FARBOX,
	SHADER_NEARBOX
};

// shader flags
enum
{
	SHADER_DEPTHWRITE			= 1 << 0,
	SHADER_SKY					= 1 << 1,
	SHADER_POLYGONOFFSET		= 1 << 2,
	SHADER_NOMIPMAPS			= 1 << 3,
	SHADER_NOPICMIP				= 1 << 4,
	SHADER_CULL_FRONT			= 1 << 5,
	SHADER_CULL_BACK			= 1 << 6,
	SHADER_VIDEOMAP				= 1 << 7,
	SHADER_AGEN_PORTAL			= 1 << 8,
	SHADER_DEFORMV_NORMAL		= 1 << 9,
	SHADER_ENTITY_MERGABLE		= 1 << 10,
	SHADER_FLARE				= 1 << 11,
	SHADER_AUTOSPRITE			= 1 << 12,
	SHADER_NO_MODULATIVE_DLIGHTS= 1 << 13,
	SHADER_LIGHTMAP				= 1 << 14,
	SHADER_NOCOMPRESS			= 1 << 15
};

// sorting
enum 
{
	SHADER_SORT_NONE			= 0,
	SHADER_SORT_PORTAL			= 1,
	SHADER_SORT_SKY				= 2,
	SHADER_SORT_OPAQUE			= 3,
	SHADER_SORT_DECAL			= 4,
	SHADER_SORT_ALPHATEST		= 5,
	SHADER_SORT_BANNER			= 6,
	SHADER_SORT_UNDERWATER		= 8,
	SHADER_SORT_ADDITIVE		= 9,
	SHADER_SORT_NEAREST			= 16
};

// shaderpass flags
#define SHADERPASS_MARK_BEGIN	0x10000	// same as GLSTATE_MARK_END
enum
{
    SHADERPASS_LIGHTMAP			= SHADERPASS_MARK_BEGIN,
	SHADERPASS_VIDEOMAP			= SHADERPASS_MARK_BEGIN << 1,
	SHADERPASS_DETAIL			= SHADERPASS_MARK_BEGIN << 2,
	SHADERPASS_NOCOLORARRAY		= SHADERPASS_MARK_BEGIN << 3,
	SHADERPASS_DLIGHT			= SHADERPASS_MARK_BEGIN << 4,
	SHADERPASS_DELUXEMAP		= SHADERPASS_MARK_BEGIN << 5,

	SHADERPASS_BLEND_REPLACE	= SHADERPASS_MARK_BEGIN << 6,
	SHADERPASS_BLEND_MODULATE	= SHADERPASS_MARK_BEGIN << 7,
	SHADERPASS_BLEND_ADD		= SHADERPASS_MARK_BEGIN << 8,
	SHADERPASS_BLEND_DECAL		= SHADERPASS_MARK_BEGIN << 9
};

#define SHADERPASS_BLENDMODE (SHADERPASS_BLEND_REPLACE|SHADERPASS_BLEND_MODULATE|SHADERPASS_BLEND_ADD|SHADERPASS_BLEND_DECAL)

// transform functions
enum
{
    SHADER_FUNC_SIN             = 1,
    SHADER_FUNC_TRIANGLE        = 2,
    SHADER_FUNC_SQUARE          = 3,
    SHADER_FUNC_SAWTOOTH        = 4,
    SHADER_FUNC_INVERSESAWTOOTH = 5,
	SHADER_FUNC_NOISE			= 6,
	SHADER_FUNC_CONSTANT		= 7
};

// RGB colors generation
enum 
{
	RGB_GEN_UNKNOWN,
	RGB_GEN_IDENTITY,
	RGB_GEN_IDENTITY_LIGHTING,
	RGB_GEN_CONST,
	RGB_GEN_COLORWAVE,
	RGB_GEN_ENTITY,
	RGB_GEN_ONE_MINUS_ENTITY,
	RGB_GEN_VERTEX,
	RGB_GEN_ONE_MINUS_VERTEX,
	RGB_GEN_LIGHTING_DIFFUSE,
	RGB_GEN_EXACT_VERTEX,
	RGB_GEN_LIGHTING_DIFFUSE_ONLY,
	RGB_GEN_LIGHTING_AMBIENT_ONLY,
	RGB_GEN_FOG,
	RGB_GEN_CUSTOM
};

// alpha channel generation
enum 
{
	ALPHA_GEN_UNKNOWN,
	ALPHA_GEN_IDENTITY,
	ALPHA_GEN_CONST,
	ALPHA_GEN_PORTAL,
	ALPHA_GEN_VERTEX,
	ALPHA_GEN_ONE_MINUS_VERTEX,
	ALPHA_GEN_ENTITY,
	ALPHA_GEN_SPECULAR,
	ALPHA_GEN_WAVE,
	ALPHA_GEN_DOT,
	ALPHA_GEN_ONE_MINUS_DOT
};

// texture coordinates generation
enum 
{
	TC_GEN_BASE,
	TC_GEN_LIGHTMAP,
	TC_GEN_ENVIRONMENT,
	TC_GEN_VECTOR,
	TC_GEN_REFLECTION,
	TC_GEN_FOG,
	TC_GEN_REFLECTION_CELLSHADE,
	TC_GEN_SVECTORS
};

// tcmod functions
enum 
{
	TC_MOD_NONE,
	TC_MOD_SCALE,
	TC_MOD_SCROLL,
	TC_MOD_ROTATE,
	TC_MOD_TRANSFORM,
	TC_MOD_TURB,
	TC_MOD_STRETCH
};

// vertices deformation
enum 
{
	DEFORMV_NONE,
	DEFORMV_WAVE,
	DEFORMV_NORMAL,
	DEFORMV_BULGE,
	DEFORMV_MOVE,
	DEFORMV_AUTOSPRITE,
	DEFORMV_AUTOSPRITE2,
	DEFORMV_PROJECTION_SHADOW,
	DEFORMV_AUTOPARTICLE
};

typedef struct
{
    unsigned short	type;				// SHADER_FUNC enum
    float			args[4];			// offset, amplitude, phase_offset, rate
} shaderfunc_t;

typedef struct 
{
	unsigned short	type;
	float			args[6];
} tcmod_t;

typedef struct 
{
	unsigned short	type;
	float			args[3];
    shaderfunc_t	func;
} rgbgen_t;

typedef struct 
{
	unsigned short	type;
	float			args[2];
    shaderfunc_t	func;
} alphagen_t;

typedef struct
{
	unsigned short	type;
    float			args[4];
    shaderfunc_t	func;
} deformv_t;

// Per-pass rendering state information
typedef struct
{
	unsigned int	flags;

	rgbgen_t		rgbgen;
	alphagen_t		alphagen;

	unsigned short	tcgen;
	vec_t			*tcgenVec;

	unsigned short	numtcmods;
	tcmod_t			*tcmods;

	cinematics_t	*cin;

	char			*program;

	float			anim_fps;			// animation frames per sec
	unsigned short	anim_numframes;
	image_t			*anim_frames[MAX_SHADER_ANIM_FRAMES];  // texture refs
} shaderpass_t;

// Shader information
typedef struct shader_s
{	
	char			name[MAX_QPATH];

    unsigned short	flags;
	unsigned short	features;
	unsigned int	sort;
	unsigned int	sortkey;

	int				type;

    unsigned short	numpasses;
    shaderpass_t	*passes;

	unsigned short	numdeforms;
	deformv_t		*deforms;

	qbyte			fog_color[4];
	float			fog_dist;

	struct shader_s *hash_next;
} shader_t;

// memory management
extern mempool_t *r_shadersmempool;

extern	shader_t	r_shaders[MAX_SHADERS];
extern	int			r_numShaders;
extern	skydome_t	*r_skydomes[MAX_SHADERS];

#define Shader_Malloc(size) Mem_Alloc(r_shadersmempool,size)
#define Shader_Free(data) Mem_Free(data)
#define Shader_Sortkey(shader,sort) (((sort)<<26)|(shader-r_shaders))

void R_InitShaders( qboolean silent );
void R_ShutdownShaders( void );

void R_RunCinematicShaders( void );
void R_UploadCinematicShader( shader_t *shader );

shader_t *R_LoadShader( char *name, int type, qboolean forceDefault, int addFlags, int ignoreType );

shader_t *R_RegisterPic( char *name );
shader_t *R_RegisterShader( char *name );
shader_t *R_RegisterSkin( char *name );

void R_ShaderList_f( void );

#endif /*__SHADER_H__*/
