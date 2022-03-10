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

#define MAX_SHADERS				4096

#define SHADER_PASS_MAX			8
#define SHADER_DEFORM_MAX		8
#define SHADER_ANIM_FRAMES_MAX	16
#define SHADER_TCMOD_MAX		8

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

// Shader flags
enum
{
	SHADER_DEPTHWRITE				= 1 << 0,
	SHADER_SKY						= 1 << 1,
	SHADER_POLYGONOFFSET			= 1 << 2,
	SHADER_NOMIPMAPS				= 1 << 3,
	SHADER_NOPICMIP					= 1 << 4,
	SHADER_CULL_FRONT				= 1 << 5,
	SHADER_CULL_BACK				= 1 << 6,
	SHADER_VIDEOMAP					= 1 << 7,
	SHADER_AGEN_PORTAL				= 1 << 8,
	SHADER_DEFORMV_BULGE			= 1 << 9,
	SHADER_ENTITY_MERGABLE			= 1 << 10,
	SHADER_FLARE					= 1 << 11,
	SHADER_AUTOSPRITE				= 1 << 12
};

// Shaderpass flags
enum
{
	SHADER_PASS_DEPTHWRITE		= 1 << 0,
    SHADER_PASS_LIGHTMAP		= 1 << 1,
	SHADER_PASS_VIDEOMAP		= 1 << 2,
    SHADER_PASS_BLEND			= 1 << 3,
    SHADER_PASS_ALPHAFUNC		= 1 << 4,
    SHADER_PASS_ANIMMAP			= 1 << 5,
	SHADER_PASS_DETAIL			= 1 << 6,
	SHADER_PASS_NOCOLORARRAY	= 1 << 7
};	

// Transform functions
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

// tcmod functions
enum 
{
	SHADER_TCMOD_NONE			= 0,
	SHADER_TCMOD_SCALE			= 1,
	SHADER_TCMOD_SCROLL			= 2,
	SHADER_TCMOD_ROTATE			= 3,
	SHADER_TCMOD_TRANSFORM		= 4,
	SHADER_TCMOD_TURB			= 5,
	SHADER_TCMOD_STRETCH		= 6
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


// RedGreenBlue pal generation
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
	RGB_GEN_EXACT_VERTEX 
};

// alpha channel generation
enum 
{
	ALPHA_GEN_UNKNOWN,
	ALPHA_GEN_IDENTITY,
	ALPHA_GEN_CONST,
	ALPHA_GEN_PORTAL,
	ALPHA_GEN_VERTEX,
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
	TC_GEN_VECTOR
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

// The flushing functions
enum 
{
	SHADER_FLUSH_GENERIC,
	SHADER_FLUSH_MULTITEXTURE_2,
	SHADER_FLUSH_MULTITEXTURE_COMBINE
};

// AlphaFunc
enum
{
	SHADER_ALPHA_GT0,
	SHADER_ALPHA_LT128,
	SHADER_ALPHA_GE128
};

// Periodic functions
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
typedef struct shaderpass_s
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

	int				numMergedPasses;
	void			(*flush) (struct shaderpass_s *pass);

    int				numtcmods;               
	tcmod_t			tcmods[SHADER_TCMOD_MAX];

	cinematics_t	*cin;

    float			anim_fps;			// animation frames per sec
    int				anim_numframes;
    image_t			*anim_frames[SHADER_ANIM_FRAMES_MAX];  // texture refs
} shaderpass_t;

// Shader information
typedef struct shader_s
{	
	char			name[MAX_QPATH];

    int				flags;
	int				features;
	unsigned int	sort;
	int				registration_sequence;

    int				numpasses;
    shaderpass_t	passes[SHADER_PASS_MAX];

	int				numdeforms;
	deformv_t		deforms[SHADER_DEFORM_MAX];

	skydome_t		*skydome;

	qbyte			fog_color[4];
	float			fog_dist;
} shader_t;

extern shader_t r_shaders[MAX_SHADERS];

// memory management
extern mempool_t *r_shadersmempool;

#define Shader_Malloc(size) Mem_Alloc(r_shadersmempool,size)
#define Shader_Free(data) Mem_Free(data)

qboolean Shader_Init (void);
void Shader_Shutdown (void);
void Shader_UpdateRegistration (void);
void Shader_RunCinematic (void);
void Shader_UploadCinematic (shader_t *shader);

shader_t *R_Shader_Load (char *name, int type, qboolean forceDefault);

shader_t *R_RegisterPic (char *name);
shader_t *R_RegisterShader (char *name);
shader_t *R_RegisterSkin (char *name);

#endif /*__SHADER_H__*/
