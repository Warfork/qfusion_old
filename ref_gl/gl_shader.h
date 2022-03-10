/* Aftershock 3D rendering engine
 * Copyright (C) 1999 Stephen C. Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef __SHADER_H__
#define __SHADER_H__

#define MAX_SHADERS				2048

#define SHADER_PASS_MAX			8
#define SHADER_DEFORM_MAX		8
#define SHADER_ANIM_FRAMES_MAX	8
#define SHADER_ARGS_MAX			(SHADER_ANIM_FRAMES_MAX+1)
#define SHADER_TCMOD_MAX		8

#define SHADER_BSP				0
#define SHADER_MD3				1
#define SHADER_2D				2

// Shader flags
enum
{
	SHADER_DEPTHWRITE    = 1 << 0,
	SHADER_SKY           = 1 << 1,
	SHADER_POLYGONOFFSET = 1 << 2,
	SHADER_NOMIPMAPS     = 1 << 3,
	SHADER_NOPICMIP      = 1 << 4,
	SHADER_CULL_FRONT	 = 1 << 5,
	SHADER_CULL_BACK	 = 1 << 6
};

// Shaderpass flags
enum
{
	SHADER_PASS_DEPTHWRITE	= 1 << 0,
    SHADER_PASS_LIGHTMAP	= 1 << 1,
    SHADER_PASS_BLEND		= 1 << 2,
    SHADER_PASS_ALPHAFUNC	= 1 << 3,
    SHADER_PASS_ANIMMAP		= 1 << 4,
	SHADER_PASS_DETAIL		= 1 << 5,
	SHADER_PASS_NOCOLORARRAY = 1 << 6
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


// RedBlueGreen pal generation
enum 
{
	RGB_GEN_UNKNOWN,
	RGB_GEN_IDENTITY,
	RGB_GEN_IDENTITY_LIGHTING,
	RGB_GEN_CONST,
	RGB_GEN_WAVE,
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
	ALPHA_GEN_IDENTITY,
	ALPHA_GEN_PORTAL,
	ALPHA_GEN_VERTEX,
	ALPHA_GEN_ENTITY,
	ALPHA_GEN_SPECULAR,
	ALPHA_GEN_WAVE
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
	DEFORMV_AUTOSPRITE2
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
    int				func;				// SHADER_FUNC enum
    float			args[4];			// offset, amplitude, phase_offset, rate
} shaderfunc_t;

typedef struct 
{
	int				type;
	float			args[6];
} tc_mod_t;

// Per-pass rendering state information
typedef struct
{
    unsigned int	flags;

    unsigned int	blendsrc, blenddst; // glBlend args
	unsigned int	blendmode;

    unsigned int	depthfunc;			// glDepthFunc arg
    unsigned int	alphafunc;

    int				rgbgen;             
    shaderfunc_t	rgbgen_func;

	int				alphagen;
    shaderfunc_t	alphagen_func;

	int				tc_gen;
	vec3_t			tc_gen_s;
	vec3_t			tc_gen_t;

    int				num_tc_mod;               
	tc_mod_t		tc_mod[SHADER_TCMOD_MAX];

    float			anim_fps;			// animation frames per sec
    int				anim_numframes;
    image_t			*anim_frames[SHADER_ANIM_FRAMES_MAX];  // texture refs
} shaderpass_t;

// Shader information
typedef struct shader_s
{	
	char			name[MAX_QPATH];

	void			(*flush)(struct mesh_s *mesh, struct mfog_s *fog, struct msurface_s *s);

    int				flags;
	int				sort;
	int				registration_sequence;

    int				numpasses;
    shaderpass_t	pass[SHADER_PASS_MAX];

	int				numdeforms;
	int				deform_vertices[SHADER_DEFORM_MAX];
    float			deform_params[SHADER_DEFORM_MAX][4];
    shaderfunc_t	deformv_func[SHADER_DEFORM_MAX];

	byte			fog_color[4];
	float			fog_dist;
} shader_t;

typedef struct shaderkey_s
{
    char			*keyword;
    int				minargs, maxargs;
    void			(*func)(shader_t *shader, shaderpass_t *pass,
						int numargs, char **args);
	struct shaderkey_s *hash_next;
	struct shaderkey_s *next;
} shaderkey_t;

extern shader_t r_shaders[MAX_SHADERS];
extern shader_t *r_skyshader;

extern double r_shadertime;

qboolean Shader_Init (void);
void Shader_Shutdown (void);
void Shader_UpdateRegistration (void);

int R_LoadShader (char *name, int type, struct msurface_s *surf);
shader_t *R_RegisterShaderNoMip (char *name);
shader_t *R_RegisterShader (char *name);
shader_t *R_RegisterShaderMD3 (char *name);

#endif /*__SHADER_H__*/
