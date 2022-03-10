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
	SHADER_DEFORMV_BULGE		= 1 << 9,
	SHADER_ENTITY_MERGABLE		= 1 << 10,
	SHADER_FLARE				= 1 << 11,
	SHADER_AUTOSPRITE			= 1 << 12,
	SHADER_NO_MODULATIVE_DLIGHTS= 1 << 13,
	SHADER_LIGHTMAP				= 1 << 14
};

// shaderpass flags
enum
{
	SHADER_PASS_DEPTHWRITE		= 1 << 0,
    SHADER_PASS_LIGHTMAP		= 1 << 1,
	SHADER_PASS_VIDEOMAP		= 1 << 2,
    SHADER_PASS_BLEND			= 1 << 3,
    SHADER_PASS_ALPHAFUNC		= 1 << 4,
    SHADER_PASS_ANIMMAP			= 1 << 5,
	SHADER_PASS_DETAIL			= 1 << 6,
	SHADER_PASS_NOCOLORARRAY	= 1 << 7,
	SHADER_PASS_DLIGHT			= 1 << 8
};	

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

// sorting
enum 
{
	SHADER_SORT_NONE			= 0,
	SHADER_SORT_PORTAL			= 1,
	SHADER_SORT_SKY				= 2,
	SHADER_SORT_OPAQUE			= 3,
	SHADER_SORT_BANNER			= 6,
	SHADER_SORT_UNDERWATER		= 8,
	SHADER_SORT_ADDITIVE		= 9,
	SHADER_SORT_NEAREST			= 16
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
	RGB_GEN_FOG
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
	ALPHA_GEN_ONE_MINUS_DOT,
	ALPHA_GEN_FOG
};

// alphaFunc
enum
{
	ALPHA_FUNC_GT0,
	ALPHA_FUNC_LT128,
	ALPHA_FUNC_GE128
};

// texture coordinates generation
enum 
{
	TC_GEN_BASE,
	TC_GEN_LIGHTMAP,
	TC_GEN_ENVIRONMENT,
	TC_GEN_VECTOR,
	TC_GEN_REFLECTION,
	TC_GEN_FOG
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
    int				type;				// SHADER_FUNC enum
    float			args[4];			// offset, amplitude, phase_offset, rate
} shaderfunc_t;

typedef struct 
{
	int				type;
	float			args[6];
} tcmod_t;

typedef struct 
{
	int				type;
	float			args[3];
    shaderfunc_t	func;
} rgbgen_t;

typedef struct 
{
	int				type;
	float			args[2];
    shaderfunc_t	func;
} alphagen_t;

typedef struct
{
	int				type;
    float			args[4];
    shaderfunc_t	func;
} deformv_t;

// Per-pass rendering state information
typedef struct
{
    unsigned int	flags;

    unsigned int	blendsrc, blenddst; // glBlendFunc args
	unsigned int	blendmode;

    unsigned int	depthfunc;			// glDepthFunc arg
    unsigned int	alphafunc;

    rgbgen_t		rgbgen;
	alphagen_t		alphagen;

	int				tcgen;
	vec4_t			tcgenVec[2];

    int				numtcmods;
	tcmod_t			*tcmods;

	cinematics_t	*cin;

    float			anim_fps;			// animation frames per sec
    int				anim_numframes;
    image_t			*anim_frames[MAX_SHADER_ANIM_FRAMES];  // texture refs
} shaderpass_t;

// Shader information
typedef struct shader_s
{	
	char			name[MAX_QPATH];

    int				flags;
	int				features;
	unsigned int	sort;
	unsigned int	sortkey;

    int				numpasses;
    shaderpass_t	*passes;

	int				numdeforms;
	deformv_t		*deforms;

	skydome_t		*skydome;

	qbyte			fog_color[4];
	double			fog_dist;

	struct shader_s *hash_next;
} shader_t;

// memory management
extern mempool_t *r_shadersmempool;

#define Shader_Malloc(size) Mem_Alloc(r_shadersmempool,size)
#define Shader_Free(data) Mem_Free(data)

void R_InitShaders( qboolean silent );
void R_ShutdownShaders( void );

void R_RunCinematicShaders( void );
void R_UploadCinematicShader( shader_t *shader );

shader_t *R_LoadShader( char *name, int type, qboolean forceDefault, int addFlags );

shader_t *R_RegisterPic (char *name);
shader_t *R_RegisterShader (char *name);
shader_t *R_RegisterSkin (char *name);

void R_ShaderList_f( void );

#endif /*__SHADER_H__*/
