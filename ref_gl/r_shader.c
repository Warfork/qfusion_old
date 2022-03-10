/*
Copyright (C) 1997-2001 Victor Luchits

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
// r_shader.c - based on code by Stephen C. Taylor
#include "r_local.h"

#define HASH_SIZE	128

typedef struct shadercache_s {
	char name[MAX_QPATH];
	char *path;
	unsigned int offset;
	struct shadercache_s *hash_next;
} shadercache_t;

static shadercache_t *shader_hash[HASH_SIZE];
static char shaderbuf[MAX_QPATH * 256];

shader_t	r_shaders[MAX_SHADERS];
double		r_shadertime;

static char		r_skyboxname[MAX_QPATH];
static float	r_skyheight;

static qboolean	r_cinblocked;

char *Shader_Skip( char *ptr );
static qboolean Shader_Parsetok( shader_t *shader, shaderpass_t *pass, shaderkey_t *keys,
		char *token, char **ptr );
static void Shader_Parsefunc( char **args, shaderfunc_t *func );
static void Shader_MakeCache( char *path );
static void Shader_GetPathAndOffset( char *name, char **path, unsigned int *offset );

static void Shader_Parsefunc (char **args, shaderfunc_t *func)
{
	if (!Q_stricmp(args[0], "sin"))
	    func->type = SHADER_FUNC_SIN;
	else if (!Q_stricmp(args[0], "triangle"))
	    func->type = SHADER_FUNC_TRIANGLE;
	else if (!Q_stricmp(args[0], "square"))
	    func->type = SHADER_FUNC_SQUARE;
	else if (!Q_stricmp(args[0], "sawtooth"))
	    func->type = SHADER_FUNC_SAWTOOTH;
	else if (!Q_stricmp(args[0], "inversesawtooth"))
	    func->type = SHADER_FUNC_INVERSESAWTOOTH;
	else if (!Q_stricmp(args[0], "noise"))
	    func->type = SHADER_FUNC_NOISE;

	func->args[0] = atof (args[1]);
	func->args[1] = atof (args[2]);
	func->args[2] = atof (args[3]);
	func->args[3] = atof (args[4]);
}

static void Shader_SetImageFlags (shader_t *shader, int *flags)
{
	if ( shader->flags & SHADER_SKY ) {
		*flags |= IT_SKY;
	}

	if ( shader->flags & SHADER_NOMIPMAPS ) {
		*flags |= IT_NOMIPMAP;
	}

	if ( shader->flags & SHADER_NOPICMIP ) {
		*flags |= IT_NOPICMIP;
	}
}

/****************** shader keyword functions ************************/
static void Shader_Cull (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	shader->flags &= ~(SHADER_CULL_FRONT|SHADER_CULL_BACK);

	if ( numargs != 0 ) {
		if (!Q_stricmp (args[0], "disable") || !Q_stricmp (args[0], "none") || !Q_stricmp (args[0], "twosided")) {
		} else if (!Q_stricmp (args[0], "front")) {
			shader->flags |= SHADER_CULL_FRONT;
		} else if (!Q_stricmp (args[0], "back") || !Q_stricmp (args[0], "backside") || !Q_stricmp (args[0], "backsided")) {
			shader->flags |= SHADER_CULL_BACK;
		} else {
			shader->flags |= SHADER_CULL_FRONT;
		}
	} else {
		shader->flags |= SHADER_CULL_FRONT;
	}
}

static void Shader_Skyparms (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	int	i;
	skydome_t *skydome;
	char path[MAX_QPATH];
	float skyheight;
	static char	*suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

	if ( shader->skydome ) {
		for ( i = 0; i < 5; i++ ) {
			Z_Free ( shader->skydome->meshes[i].xyz_array );
			Z_Free ( shader->skydome->meshes[i].normals_array );
			Z_Free ( shader->skydome->meshes[i].st_array );
		}

		Z_Free ( shader->skydome );
	}

	skydome = (skydome_t *)Z_Malloc ( sizeof(skydome_t) );
	shader->skydome = skydome;

	skyheight = atof(args[1]);
	if ( !skyheight )
		skyheight = 512.0f;

	if (args[0][0] == '-')
	{
		for (i=0 ; i<6 ; i++)
			skydome->farbox_textures[i] = NULL;
	}
	else
	{
		for (i=0 ; i<6 ; i++)
		{
			Com_sprintf (path, sizeof(path), "%s_%s", args[0], suf[i]);
			skydome->farbox_textures[i] = GL_FindImage ( path, IT_NOMIPMAP|IT_SKY );
		}
	}

	if (args[2][0] == '-')
	{
		for (i=0 ; i<6 ; i++)
			skydome->nearbox_textures[i] = NULL;
	}
	else
	{
		for (i=0 ; i<6 ; i++)
		{
			Com_sprintf (path, sizeof(path), "%s_%s", args[2], suf[i]);
			skydome->nearbox_textures[i] = GL_FindImage ( path, IT_NOMIPMAP|IT_SKY );
		}
	}

	R_CreateSkydome ( shader, skyheight );

	shader->flags |= SHADER_SKY;
	shader->sort = SHADER_SORT_SKY;
}

static void Shader_Nomipmaps (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
    shader->flags |= (SHADER_NOMIPMAPS|SHADER_NOPICMIP);
}

static void Shader_Nopicmip (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
    shader->flags |= SHADER_NOPICMIP;
}

static void Shader_Deformvertexes (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	deformv_t *deformv;

	if (shader->numdeforms >= SHADER_DEFORM_MAX)
		return;

	deformv = &shader->deforms[shader->numdeforms];

	if (!Q_stricmp (args[0], "wave")) {
		deformv->type = DEFORMV_WAVE;
		Shader_Parsefunc(&args[2], &deformv->func);

		deformv->args[0] = atof (args[1]);
		if ( deformv->args[0] ) {
			deformv->args[0] = 1.0f / deformv->args[0];
		}
	} else if (!Q_stricmp (args[0], "normal")) {
		deformv->type = DEFORMV_NORMAL;
		deformv->args[0] = atof (args[1]); // Div
		deformv->args[1] = atof (args[2]); // base
	} else if (!Q_stricmp (args[0], "bulge")) {
		deformv->type = DEFORMV_BULGE;
		deformv->args[0] = atof (args[1]); // Width 
		deformv->args[1] = atof (args[2]); // Height
		deformv->args[2] = atof (args[3]); // Speed 
		shader->flags |= SHADER_DEFORMV_BULGE;
	} else if (!Q_stricmp (args[0], "move")) {
		deformv->type = DEFORMV_MOVE;
		deformv->args[0] = atof (args[1]); // x 
		deformv->args[1] = atof (args[2]); // y
		deformv->args[2] = atof (args[3]); // z
		Shader_Parsefunc (&args[4], &deformv->func);
	} else if (!Q_stricmp (args[0], "autosprite")) {
		deformv->type = DEFORMV_AUTOSPRITE;
	} else if (!Q_stricmp (args[0], "autosprite2")) {
		deformv->type = DEFORMV_AUTOSPRITE2;
	} else if (!Q_stricmp (args[0], "projectionShadow")) {
		deformv->type = DEFORMV_PROJECTION_SHADOW;
	} else {
		return;
	}

	shader->numdeforms++;
}

static void Shader_Fogparms (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	double div;
	int args0, dargs;

	if ( !strcmp (args[1], "(") ) {
		args0 = 2;
		dargs = 6;
	} else {
		args0 = 1;
		dargs = 5;
	}

	div = floor( r_mapoverbrightbits->value - r_overbrightbits->value );
	if ( div > 0 ) {
		div = 255.0 / pow( 2, div );
	} else {
		div = 255.0;
	}

	shader->fog_color[0] = (byte)(atof(args[args0+0]) * div);	// R
	shader->fog_color[1] = (byte)(atof(args[args0+1]) * div);	// G
	shader->fog_color[2] = (byte)(atof(args[args0+2]) * div);	// B
	shader->fog_color[3] = 255;									// A

	shader->fog_dist = atof(args[dargs]);		// Dist

	if ( shader->fog_dist <= 0.0f )
		shader->fog_dist = 128.0f;

	shader->fog_dist = 1.0f / shader->fog_dist;
}

static void Shader_Sort (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	if (!Q_stricmp( args[0], "portal" ) ) {
		shader->sort = SHADER_SORT_PORTAL;
	} else if( !Q_stricmp( args[0], "sky" ) ) {
		shader->sort = SHADER_SORT_SKY;
	} else if( !Q_stricmp( args[0], "opaque" ) ) {
		shader->sort = SHADER_SORT_OPAQUE;
	} else if( !Q_stricmp( args[0], "banner" ) ) {
		shader->sort = SHADER_SORT_BANNER;
	} else if( !Q_stricmp( args[0], "underwater" ) ) {
		shader->sort = SHADER_SORT_UNDERWATER;
	} else if( !Q_stricmp( args[0], "additive" ) ) {
		shader->sort = SHADER_SORT_ADDITIVE;
	} else if( !Q_stricmp( args[0], "nearest" ) ) {
		shader->sort = SHADER_SORT_NEAREST;
	} else {
		shader->sort = atoi ( args[0] );

		clamp ( shader->sort, SHADER_SORT_NONE, SHADER_SORT_NEAREST );
	}
}

static void Shader_Portal (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	shader->sort = SHADER_SORT_PORTAL;
}

static void Shader_Polygonoffset (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	shader->flags |= SHADER_POLYGONOFFSET;
}

static void Shader_EntityMergable (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	shader->flags |= SHADER_ENTITY_MERGABLE;
}

static shaderkey_t shaderkeys[] =
{
    {"cull",			1, 1,	Shader_Cull },
    {"skyparms",		3, 3,	Shader_Skyparms },
	{"fogparms",		6, 7,	Shader_Fogparms },
    {"nomipmaps",		0, 0,	Shader_Nomipmaps },
	{"nopicmip",		0, 0,	Shader_Nopicmip },
	{"polygonoffset",	0, 0,	Shader_Polygonoffset },
	{"sort",			1, 1,	Shader_Sort },
    {"deformvertexes",	1, 9,	Shader_Deformvertexes },
	{"portal",			0, 0,	Shader_Portal },
	{"entitymergable",	0, 0,	Shader_EntityMergable },
    {NULL,				0, 0,	NULL}
};

// ===============================================================

static void Shaderpass_Map (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	int flags = 0;

	Shader_SetImageFlags ( shader, &flags );

	if (!Q_stricmp (args[0], "$lightmap")) {
		pass->tc_gen = TC_GEN_LIGHTMAP;
		pass->flags |= SHADER_PASS_LIGHTMAP;
		pass->anim_frames[0] = NULL;
	} else if (!Q_stricmp (args[0], "$whiteimage")) {
		pass->tc_gen = TC_GEN_BASE;
		pass->anim_frames[0] = GL_FindImage ("***r_whitetexture***", flags);
	} else {
		pass->tc_gen = TC_GEN_BASE;
		pass->anim_frames[0] = GL_FindImage (args[0], flags);

		if ( !pass->anim_frames[0] ) {
			Com_DPrintf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n",
				shader->name, args[0] );
			pass->anim_frames[0] = r_notexture;
		}
    }
}

static void Shaderpass_Animmap (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
    int i;
	int flags = 0;

	Shader_SetImageFlags ( shader, &flags );

	pass->tc_gen = TC_GEN_BASE;
    pass->flags |= SHADER_PASS_ANIMMAP;
    pass->anim_fps = (float)atof (args[0]);
    pass->anim_numframes = numargs - 1;

    for (i = 1; i < numargs; i++) {
		pass->anim_frames[i-1] = GL_FindImage (args[i], flags);

		if ( !pass->anim_frames[i-1] ) {
			Com_DPrintf ( S_COLOR_YELLOW "Shader %s has an animmap with no image: %s.\n",
				shader->name, args[i] );
			pass->anim_frames[i-1] = r_notexture;
		}
	}
}

static void Shaderpass_Clampmap (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	int flags = IT_CLAMP;

	Shader_SetImageFlags ( shader, &flags );

	pass->tc_gen = TC_GEN_BASE;
	pass->anim_frames[0] = GL_FindImage (args[0], flags);

	if ( !pass->anim_frames[0] ) {
		Com_DPrintf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n",
			shader->name, args[0] );
		pass->anim_frames[0] = r_notexture;
	}
}

static void Shaderpass_VideoMap (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	char		name[MAX_OSPATH];

	COM_StripExtension ( args[0], name );

	if ( pass->cin )
		Z_Free ( pass->cin );

	pass->cin = (cinematics_t *)Z_Malloc ( sizeof(cinematics_t) );
	pass->cin->frame = -1;
	Com_sprintf ( pass->cin->name, sizeof(pass->cin->name), "video/%s.RoQ", name );

	pass->flags |= SHADER_PASS_VIDEOMAP;
	shader->flags |= SHADER_VIDEOMAP;
}

static void Shaderpass_Rgbgen (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	if (!Q_stricmp(args[0], "identitylighting")) {
		pass->rgbgen = RGB_GEN_IDENTITY_LIGHTING;
	} else if (!Q_stricmp(args[0], "identity")) {
		pass->rgbgen = RGB_GEN_IDENTITY;
	} else if (!Q_stricmp (args[0], "wave")) {
		if (numargs != 6) {
			return;
		}
		
		if ( pass->rgbgen_func )
			Z_Free ( pass->rgbgen_func );

		pass->rgbgen = RGB_GEN_WAVE;
		pass->rgbgen_func = Z_Malloc ( sizeof(shaderfunc_t) );

		Shader_Parsefunc (&args[1], pass->rgbgen_func);
	} else if (!Q_stricmp(args[0], "entity")) {
		pass->rgbgen = RGB_GEN_ENTITY;
	} else if (!Q_stricmp(args[0], "oneMinusEntity")) {
		pass->rgbgen = RGB_GEN_ONE_MINUS_ENTITY;
	} else if (!Q_stricmp(args[0], "vertex")) {
		pass->rgbgen = RGB_GEN_VERTEX;
	} else if (!Q_stricmp(args[0], "oneMinusVertex")) {
		pass->rgbgen = RGB_GEN_ONE_MINUS_VERTEX;
	} else if (!Q_stricmp(args[0], "lightingDiffuse")) {
		pass->rgbgen = RGB_GEN_LIGHTING_DIFFUSE;
	} else if (!Q_stricmp (args[0], "exactvertex")) {
		pass->rgbgen = RGB_GEN_EXACT_VERTEX;
	} else if (!Q_stricmp (args[0], "const") || !Q_stricmp (args[0], "constant")) {
		int args0;

		if ( !strcmp (args[1], "(") ) {
			args0 = 2;
		} else {
			args0 = 1;
		}

		if ( pass->rgbgen_func )
			Z_Free ( pass->rgbgen_func );

		pass->rgbgen = RGB_GEN_CONST;
		pass->rgbgen_func = Z_Malloc ( sizeof(shaderfunc_t) );
		pass->rgbgen_func->type = SHADER_FUNC_CONSTANT;
		pass->rgbgen_func->args[0] = atof (args[args0+0]);
		pass->rgbgen_func->args[1] = atof (args[args0+1]);
		pass->rgbgen_func->args[2] = atof (args[args0+2]);
	}
}

static void Shaderpass_Alphagen (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	if (!Q_stricmp (args[0], "portal")) {
		pass->alphagen = ALPHA_GEN_PORTAL;
		shader->flags |= SHADER_AGEN_PORTAL;
	} else if (!Q_stricmp (args[0], "vertex")) {
		pass->alphagen = ALPHA_GEN_VERTEX;
	} else if (!Q_stricmp (args[0], "entity")) {
		pass->alphagen = ALPHA_GEN_ENTITY;
	} else if (!Q_stricmp (args[0], "wave")) {
		if (numargs != 6) {
			return;
		}

		if ( pass->alphagen_func )
			Z_Free ( pass->alphagen_func );

		pass->alphagen = ALPHA_GEN_WAVE;
		pass->alphagen_func = Z_Malloc ( sizeof(shaderfunc_t) );

		Shader_Parsefunc (&args[1], pass->alphagen_func);
	} else if (!Q_stricmp (args[0], "lightingspecular")) {
		pass->alphagen = ALPHA_GEN_SPECULAR;
	} else if (!Q_stricmp (args[0], "const") || !Q_stricmp (args[0], "constant")) {
		if ( pass->alphagen_func )
			Z_Free ( pass->alphagen_func );

		pass->alphagen = ALPHA_GEN_CONST;
		pass->alphagen_func = Z_Malloc ( sizeof(shaderfunc_t) );

		pass->alphagen_func->type = SHADER_FUNC_CONSTANT;
		pass->alphagen_func->args[0] = atof(args[1]);
	}
}

static void Shaderpass_Blendfunc (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	if ( numargs == 1 )
    {
		if (!Q_stricmp (args[0], "blend")) {
			pass->blendsrc = GL_SRC_ALPHA;
			pass->blenddst = GL_ONE_MINUS_SRC_ALPHA;
		} else if (!Q_stricmp (args[0], "filter")) {
			pass->blendsrc = GL_DST_COLOR;
			pass->blenddst = GL_ZERO;
		} else if (!Q_stricmp (args[0], "add")) {
			pass->blendsrc = pass->blenddst = GL_ONE;
		}
	} else {
		int i;
		unsigned int *blend;

		for (i = 0; i < 2; i++)
		{
			blend = (i == 0) ? &pass->blendsrc : &pass->blenddst;

			if (!Q_stricmp (args[i], "gl_zero"))
				*blend = GL_ZERO;
			else if (!Q_stricmp (args[i], "gl_one"))
				*blend = GL_ONE;
			else if (!Q_stricmp (args[i], "gl_dst_color"))
				*blend = GL_DST_COLOR;
			else if (!Q_stricmp (args[i], "gl_one_minus_src_alpha"))
				*blend = GL_ONE_MINUS_SRC_ALPHA;
			else if (!Q_stricmp (args[i], "gl_src_alpha"))
				*blend = GL_SRC_ALPHA;
			else if (!Q_stricmp (args[i], "gl_src_color"))
				*blend = GL_SRC_COLOR;
			else if (!Q_stricmp (args[i], "gl_one_minus_dst_color"))
				*blend = GL_ONE_MINUS_DST_COLOR;
			else if (!Q_stricmp (args[i], "gl_one_minus_src_color"))
				*blend = GL_ONE_MINUS_SRC_COLOR;
			else if (!Q_stricmp (args[i], "gl_dst_alpha"))
				*blend = GL_DST_ALPHA;
			else if (!Q_stricmp (args[i], "gl_one_minus_dst_alpha"))
				*blend = GL_ONE_MINUS_DST_ALPHA;
			else
				*blend = GL_ONE;
		}
    }

	pass->flags |= SHADER_PASS_BLEND;
}

static void Shaderpass_Alphafunc (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
    if (!Q_stricmp (args[0], "gt0")) {
		pass->alphafunc = SHADER_ALPHA_GT0;
	} else if (!Q_stricmp (args[0], "lt128")) {
		pass->alphafunc = SHADER_ALPHA_LT128;
	} else if (!Q_stricmp (args[0], "ge128")) {
		pass->alphafunc = SHADER_ALPHA_GE128;
    } else {
		return;
	}

    pass->flags |= SHADER_PASS_ALPHAFUNC;
}

static void Shaderpass_Depthfunc (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
    if (!Q_stricmp (args[0], "equal"))
		pass->depthfunc = GL_EQUAL;
    else if (!Q_stricmp (args[0], "lequal"))
		pass->depthfunc = GL_LEQUAL;
	else if (!Q_stricmp (args[0], "gequal"))
		pass->depthfunc = GL_GEQUAL;
}

static void Shaderpass_Depthwrite (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
    shader->flags |= SHADER_DEPTHWRITE;
    pass->flags |= SHADER_PASS_DEPTHWRITE;
}

static void Shaderpass_Tcmod (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	int i;
	tcmod_t *tcmod;

	if (pass->numtcmods >= SHADER_TCMOD_MAX) {
		return;
	}

	tcmod = &pass->tcmods[pass->numtcmods];

	if (!Q_stricmp (args[0], "rotate")) {
		tcmod->args[0] = -atof (args[1]) / 360.0f;

		if ( !tcmod->args[0] ) {
			return;
		}

		tcmod->type = SHADER_TCMOD_ROTATE;
	} else if (!Q_stricmp (args[0], "scale")) {
		if (numargs != 3) {
			return;
		}

		tcmod->args[0] = atof (args[1]);
		tcmod->args[1] = atof (args[2]);
		tcmod->type = SHADER_TCMOD_SCALE;
	} else if (!Q_stricmp (args[0], "scroll")) {
		if (numargs != 3) {
			return;
		}

		tcmod->args[0] = atof (args[1]);
		tcmod->args[1] = atof (args[2]);
		tcmod->type = SHADER_TCMOD_SCROLL;
	} else if (!Q_stricmp (args[0], "stretch")) {
		shaderfunc_t func;

		if (numargs != 6) {
			return;
		}

		Shader_Parsefunc (&args[1], &func);

		tcmod->args[0] = func.type;
		for (i = 1; i < 5; ++i)
			tcmod->args[i] = func.args[i-1];

		tcmod->type = SHADER_TCMOD_STRETCH;
	} else if (!Q_stricmp (args[0], "transform")) {
		if (numargs != 7) { 
			return;
		}

		for (i = 0; i < 6; ++i)
			tcmod->args[i] = atof (args[i + 1]);
		tcmod->type = SHADER_TCMOD_TRANSFORM;
	} else if (!Q_stricmp (args[0], "turb")) {
		if (numargs != 5) {
			return;
		}

		for (i = 0; i < 4; i++)
			tcmod->args[i] = atof (args[i+1]);
		tcmod->type = SHADER_TCMOD_TURB;
	} else {
		return;
	}

	pass->numtcmods++;
}


static void Shaderpass_Tcgen (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	if (!Q_stricmp (args[0], "base")) {
		pass->tc_gen = TC_GEN_BASE;
	} else if (!Q_stricmp (args[0], "lightmap")) {
		pass->tc_gen = TC_GEN_LIGHTMAP;
	} else if (!Q_stricmp (args[0], "environment")) {
		pass->tc_gen = TC_GEN_ENVIRONMENT;
	} else if (!Q_stricmp (args[0], "vector")) {
		pass->tc_gen = TC_GEN_BASE;

//		pass->tc_gen = TC_GEN_VECTOR;

//		pass->tc_gen_s[0] = atof(args[1]);
//		pass->tc_gen_s[1] = atof(args[2]);
//		pass->tc_gen_s[2] = atof(args[3]);
//		pass->tc_gen_t[0] = atof(args[4]);
//		pass->tc_gen_t[1] = atof(args[5]);
//		pass->tc_gen_t[2] = atof(args[6]);
	}
}

static void Shaderpass_Detail (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	pass->flags |= SHADER_PASS_DETAIL;
}

static shaderkey_t shaderpasskeys[] =
{
    {"map",			1, 1,				Shaderpass_Map },
    {"rgbgen",		1, 6,				Shaderpass_Rgbgen },
    {"blendfunc",	1, 2,				Shaderpass_Blendfunc },
    {"depthfunc",	1, 1,				Shaderpass_Depthfunc },
    {"depthwrite",	0, 0,				Shaderpass_Depthwrite },
    {"alphafunc",	1, 1,				Shaderpass_Alphafunc },
    {"tcmod",		2, 7,				Shaderpass_Tcmod },
    {"animmap",		3, SHADER_ARGS_MAX, Shaderpass_Animmap },
    {"clampmap",	1, 1,				Shaderpass_Clampmap },
	{"videomap",	1, 1,				Shaderpass_VideoMap },
    {"tcgen",		1, 10,				Shaderpass_Tcgen },
	{"alphagen",	0, 10,				Shaderpass_Alphagen },
	{"detail",		0, 0,				Shaderpass_Detail },
    {NULL,			0, 0,				NULL }
};

// ===============================================================

qboolean Shader_Init (void)
{
	char *dirptr;
	int i, dirlen, numdirs;

	Com_Printf ( "Initializing Shaders:\n" );

	numdirs = FS_GetFileList ( "scripts", "shader", shaderbuf, sizeof(shaderbuf) );
	if ( !numdirs ) {
		Com_Error ( ERR_DROP, "Could not find any shaders!");
		return false;
	}

	// now load all the scripts
	dirptr = shaderbuf;
	memset ( shader_hash, 0, sizeof(shadercache_t *)*HASH_SIZE );

	for (i=0; i<numdirs; i++, dirptr += dirlen+1) {
		dirlen = strlen(dirptr);
		if ( !dirlen ) {
			continue;
		}

		Shader_MakeCache ( dirptr );
	}

	return true;
}

static void Shader_MakeCache ( char *path )
{
	unsigned int key, i;
	char filename[MAX_QPATH];
	char *buf, *ptr, *token, *t;
	shadercache_t *cache;
	int size;

	Com_sprintf( filename, sizeof(filename), "scripts/%s", path );
	Com_Printf ( "...loading '%s'\n", filename );

	size = FS_LoadFile ( filename, (void **)&buf );
	if ( !buf || size <= 0 ) {
		return;
	}

	ptr = buf;
	do
	{
		if ( ptr - buf >= size )
			break;

		token = COM_ParseExt ( &ptr, true );
		if ( !token[0] || ptr - buf >= size )
			break;

		t = NULL;
		Shader_GetPathAndOffset ( token, &t, &i );
		if ( t ) {
			ptr = Shader_Skip ( ptr );
			continue;
		}

		key = Com_HashKey ( token, HASH_SIZE );

		cache = ( shadercache_t * )Z_Malloc ( sizeof(shadercache_t) );
		cache->hash_next = shader_hash[key];
		cache->path = path;
		cache->offset = ptr - buf;
		Com_sprintf ( cache->name, MAX_QPATH, token );
		shader_hash[key] = cache;

		ptr = Shader_Skip ( ptr );
	} while ( ptr );

	FS_FreeFile ( buf );
}

char *Shader_Skip ( char *ptr )
{	
	char *tok;
    int brace_count;

    // Opening brace
    tok = COM_ParseExt ( &ptr, true );

	if (!ptr)
		return NULL;
    
	if ( tok[0] != '{' ) {
		tok = COM_ParseExt ( &ptr, true );
	}

    for (brace_count = 1; brace_count > 0 ; ptr++)
    {
		tok = COM_ParseExt ( &ptr, true );

		if ( !tok[0] )
			return NULL;

		if (tok[0] == '{') {
			brace_count++;
		} else if (tok[0] == '}') {
			brace_count--;
		}
    }

	return ptr;
}

static void Shader_GetPathAndOffset ( char *name, char **path, unsigned int *offset )
{
	unsigned int key;
	shadercache_t *cache;

	key = Com_HashKey ( name, HASH_SIZE );
	cache = shader_hash[key];

	for ( ; cache; cache = cache->hash_next ) {
		if ( !Q_stricmp (cache->name, name) ) {
			*path = cache->path;
			*offset = cache->offset;
			return;
		}
	}

	path = NULL;
}

void Shader_FreePass (shaderpass_t *pass)
{
	if ( pass->flags & SHADER_PASS_VIDEOMAP ) {
		GL_StopCinematic ( pass->cin );
		Z_Free ( pass->cin );
		pass->cin = NULL;
	}
	
	if ( pass->rgbgen_func ) {
		Z_Free ( pass->rgbgen_func );
		pass->rgbgen_func = NULL;
	}
	
	if ( pass->alphagen_func ) {
		Z_Free ( pass->alphagen_func );
		pass->alphagen_func = NULL;
	}
}

void Shader_Free (shader_t *shader)
{
	int i;
	shaderpass_t *pass;

	if ( shader->flags & SHADER_SKY ) {
		for ( i = 0; i < 5; i++ ) {
			Z_Free ( shader->skydome->meshes[i].xyz_array );
			Z_Free ( shader->skydome->meshes[i].normals_array );
			Z_Free ( shader->skydome->meshes[i].st_array );
		}

		Z_Free ( shader->skydome );
	}

	pass = shader->passes;
	for ( i = 0; i < shader->numpasses; i++, pass++ ) {
		Shader_FreePass ( pass );
	}
}

void Shader_Shutdown (void)
{
	int i;
	shader_t *shader;
	shadercache_t *cache, *cache_next;

	shader = r_shaders;
	for (i = 0; i < MAX_SHADERS; i++, shader++)
	{
		if ( !shader->registration_sequence )
			continue;
		
		Shader_Free ( shader );
	}

	for ( i = 0; i < HASH_SIZE; i++ ) {
		cache = shader_hash[i];

		for ( ; cache; cache = cache_next ) {
			cache_next = cache->hash_next;
			cache->hash_next = NULL;
			Z_Free ( cache );
		}
	}

	memset (r_shaders, 0, sizeof(shader_t)*MAX_SHADERS);
}

void Shader_SetBlendmode ( shaderpass_t *pass )
{
	if ( !pass->anim_frames[0] && !(pass->flags & SHADER_PASS_LIGHTMAP) ) {
		pass->blendmode = 0;
		return;
	}

	if ( !(pass->flags & SHADER_PASS_BLEND) && qglMTexCoord2fSGIS ) {
		if ( (pass->rgbgen == RGB_GEN_IDENTITY) && (pass->alphagen == ALPHA_GEN_IDENTITY) ) {
			pass->blendmode = GL_REPLACE;
		} else {
			pass->blendsrc = GL_ONE;
			pass->blenddst = GL_ZERO;
			pass->blendmode = GL_MODULATE;
		}
		return;
	}

	if ( pass->blendsrc == GL_ZERO && pass->blenddst == GL_SRC_COLOR ||
		pass->blendsrc == GL_DST_COLOR && pass->blenddst == GL_ZERO )
		pass->blendmode = GL_MODULATE;
	else if ( pass->blendsrc == GL_ONE && pass->blenddst == GL_ONE )
		pass->blendmode = GL_ADD;
	else if ( pass->blendsrc == GL_SRC_ALPHA && pass->blenddst == GL_ONE_MINUS_SRC_ALPHA )
		pass->blendmode = GL_DECAL;
	else
		pass->blendmode = 0;
}

void Shader_Readpass (shader_t *shader, char **ptr)
{
    char *token;
	shaderpass_t *pass;
	qboolean finish, ignore;
	static shader_t dummy;

	if ( shader->numpasses >= SHADER_PASS_MAX ) {
		ignore = true;
		shader = &dummy;
		shader->numpasses = 1;
		pass = shader->passes;
	} else {
		ignore = false;
		pass = &shader->passes[shader->numpasses++];
	}
	
    // Set defaults
    pass->flags = 0;
    pass->anim_frames[0] = NULL;
	pass->anim_numframes = 0;
    pass->depthfunc = GL_LEQUAL;
    pass->rgbgen = RGB_GEN_UNKNOWN;
	pass->rgbgen_func = NULL;
	pass->alphagen = ALPHA_GEN_IDENTITY;
	pass->alphagen_func = NULL;
	pass->tc_gen = TC_GEN_BASE;
	pass->numtcmods = 0;

	// default to R_RenderMeshGeneric
	pass->numMergedPasses = 1;
	pass->flush = R_RenderMeshGeneric;

	finish = false;
	while (ptr)
	{
		if ( finish )
			break;

		token = COM_ParseExt (ptr, true);

		if (!token[0]) 
			continue;

		if ( token[0] == '}' ) {
			break;
		} else {
			finish = Shader_Parsetok (shader, pass, shaderpasskeys, token, ptr);
		}
	}

	// check some things 
	if ( ignore ) {
		Shader_Free ( shader );
		return;
	}

	if ( (pass->blendsrc == GL_ONE) && (pass->blenddst == GL_ZERO) ) {
		pass->flags |= SHADER_PASS_DEPTHWRITE;
		shader->flags |= SHADER_DEPTHWRITE;
	}

	switch (pass->rgbgen)
	{
		case RGB_GEN_IDENTITY_LIGHTING:
		case RGB_GEN_IDENTITY:
		case RGB_GEN_CONST:
		case RGB_GEN_WAVE:
		case RGB_GEN_ENTITY:
		case RGB_GEN_ONE_MINUS_ENTITY:
		case RGB_GEN_UNKNOWN:	// assume RGB_GEN_IDENTITY or RGB_GEN_IDENTITY_LIGHTING

			switch (pass->alphagen)
			{
				case ALPHA_GEN_IDENTITY:
				case ALPHA_GEN_CONST:
				case ALPHA_GEN_WAVE:
				case ALPHA_GEN_ENTITY:
					pass->flags |= SHADER_PASS_NOCOLORARRAY;
					break;
				default:
					break;
			}

			break;
		default:
			break;
	}

	Shader_SetBlendmode ( pass );

	if ( (shader->flags & SHADER_SKY) && (shader->flags & SHADER_DEPTHWRITE) ) {
		if ( pass->flags & SHADER_PASS_DEPTHWRITE ) {
			pass->flags &= ~SHADER_PASS_DEPTHWRITE;
		}
	}
}

static qboolean Shader_Parsetok (shader_t *shader, shaderpass_t *pass, shaderkey_t *keys, char *token, char **ptr)
{
    shaderkey_t *key;
    char *c, *args[SHADER_ARGS_MAX];
	static char buf[SHADER_ARGS_MAX][128];
    int numargs;

	for (key = keys; key->keyword != NULL; key++)
	{
		if (!Q_stricmp (token, key->keyword))
		{
			numargs = 0;

			while (ptr)
			{
				c = COM_ParseExt (ptr, false);

				if (!c[0] || c[0] == '}') // NEW LINE 
					break;
	
				strlwr ( c );
				strcpy ( buf[numargs], c );
				args[numargs] = buf[numargs++];
			}

			if (numargs < key->minargs || numargs > key->maxargs) {
				continue;
			}

			if (key->func)
				key->func (shader, pass, numargs, args);

			return (c[0] == '}');
		}
	}
   
	// Next Line
	while (ptr)
	{
		token = COM_ParseExt (ptr, false);
		if (!token[0])
			break;
	}

	return false;
}

void Shader_SetPassFlush ( shaderpass_t *pass, shaderpass_t *pass2 )
{
	if ( ((pass->flags & SHADER_PASS_DETAIL) && !r_detailtextures->value) ||
		((pass2->flags & SHADER_PASS_DETAIL) && !r_detailtextures->value) ||
		 (pass->flags & SHADER_PASS_VIDEOMAP) || (pass2->flags & SHADER_PASS_VIDEOMAP) || 
		 ((pass->flags & SHADER_PASS_ALPHAFUNC) && (pass2->depthfunc != GL_EQUAL)) ) {
		return;
	}

	if ( pass2->rgbgen != RGB_GEN_IDENTITY || pass2->alphagen != ALPHA_GEN_IDENTITY ) {
		return;
	}

	// check if we can use R_RenderMeshCombined
	if ( gl_config.tex_env_combine || gl_config.nv_tex_env_combine4 )
	{
		if ( pass->blendmode == GL_REPLACE ) {
			if ((pass2->blendmode == GL_DECAL && gl_config.tex_env_combine) ||
				(pass2->blendmode == GL_ADD && gl_config.env_add) ||
				(pass2->blendmode && pass2->blendmode != GL_ADD) ||	gl_config.nv_tex_env_combine4 ) {
				pass->flush = R_RenderMeshCombined;
			}
		} else if ( pass->blendmode == GL_ADD && 
			pass2->blendmode == GL_ADD && gl_config.env_add ) {
			pass->flush = R_RenderMeshCombined;
		} else if ( pass->blendmode == GL_MODULATE && 
			pass2->blendmode == GL_MODULATE ) {
			pass->flush = R_RenderMeshCombined;
		}
	} else if ( qglMTexCoord2fSGIS ) {
		// check if we can use R_RenderMeshMultitextured
		if ( pass->blendmode == GL_REPLACE ) {
			if ( pass2->blendmode == GL_ADD && gl_config.env_add ) {
				pass->flush = R_RenderMeshMultitextured;
				pass->numMergedPasses = 2;
			} else if ( pass2->blendmode && pass2->blendmode != GL_DECAL ) {
				pass->flush = R_RenderMeshMultitextured;
				pass->numMergedPasses = 2;
			}
		} else if ( pass->blendmode == GL_MODULATE && 
			pass2->blendmode == GL_MODULATE ) {
			pass->flush = R_RenderMeshMultitextured;
		} else if ( pass->blendmode == GL_ADD && 
			pass2->blendmode == GL_ADD && gl_config.env_add ) {
			pass->flush = R_RenderMeshCombined;
		}
	}

	if ( pass->flush != R_RenderMeshGeneric ) {
		pass->numMergedPasses = 2;
	}
}

void Shader_SetFeatures ( shader_t *s )
{
	int i;
	shaderpass_t *pass;
	qboolean trnormals;

	s->features = MF_NONE;
	
	for ( i = 0, trnormals = true; i < s->numdeforms; i++ ) {
		switch ( s->deforms[i].type ) {
			case DEFORMV_BULGE:
			case DEFORMV_WAVE:
				trnormals = false;
			case DEFORMV_NORMAL:
				s->features |= MF_NORMALS;
				break;
			case DEFORMV_MOVE:
				break;
			default:
				trnormals = false;
				break;
		}
	}

	if ( trnormals ) {
		s->features |= MF_TRNORMALS;
	}

	for ( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ ) {
		switch ( pass->rgbgen ) {
			case RGB_GEN_LIGHTING_DIFFUSE:
				s->features |= MF_NORMALS;
				break;
			case RGB_GEN_VERTEX:
			case RGB_GEN_ONE_MINUS_VERTEX:
			case RGB_GEN_EXACT_VERTEX:
				s->features |= MF_COLORS;
				break;
		}

		switch ( pass->alphagen ) {
			case ALPHA_GEN_SPECULAR:
				s->features |= MF_NORMALS;
				break;
			case ALPHA_GEN_VERTEX:
				s->features |= MF_COLORS;
				break;
		}

		switch ( pass->tc_gen ) {
			case TC_GEN_BASE:
				s->features |= MF_STCOORDS;
				break;
			case TC_GEN_LIGHTMAP:
				s->features |= MF_LMCOORDS;
				break;
			case TC_GEN_ENVIRONMENT:
				s->features |= MF_NORMALS;
				break;
		}
	}
}

void Shader_Finish ( shader_t *s )
{
	int i;
	shaderpass_t *pass;

	if ( !Q_stricmp (s->name, "flareShader") ) {
		s->flags |= SHADER_FLARE;
	}

	if ( !s->numpasses && !s->sort ) {
		s->sort = SHADER_SORT_ADDITIVE;
		return;
	}

	if ( r_vertexlight->value ) {
		// do we have a lightmap pass?
		pass = s->passes;
		for ( i = 0; i < s->numpasses; i++, pass++ ) {
			if ( pass->flags & SHADER_PASS_LIGHTMAP )
				break;
		}

		if ( i == s->numpasses ) {
			goto done;
		}

		// try to find pass with rgbgen set to RGB_GEN_VERTEX
		pass = s->passes;
		for ( i = 0; i < s->numpasses; i++, pass++ ) {
			if ( pass->rgbgen == RGB_GEN_VERTEX )
				break;
		}

		if ( i < s->numpasses ) {		// we found it
			pass->flags |= SHADER_CULL_FRONT;
			pass->flags &= ~(SHADER_PASS_BLEND|SHADER_PASS_ANIMMAP);
			pass->blendmode = 0;
			pass->flags |= SHADER_PASS_DEPTHWRITE;
			pass->alphagen = ALPHA_GEN_IDENTITY;
			pass->numMergedPasses = 1;
			pass->flush = R_RenderMeshGeneric;
			s->flags |= SHADER_DEPTHWRITE;
			s->sort = SHADER_SORT_OPAQUE;
			s->numpasses = 1;
			memcpy ( &s->passes[0], pass, sizeof(shaderpass_t) );
		} else {	// we didn't find it - simply remove all lightmap passes
			pass = s->passes;
			for ( i = 0; i < s->numpasses; i++, pass++ ) {
				if ( pass->flags & SHADER_PASS_LIGHTMAP )
					break;
			}
			
			if ( i == s->numpasses -1 ) {
				s->numpasses--;
			} else if ( i < s->numpasses - 1 ) {
				for ( ; i < s->numpasses - 1; i++, pass++ ) {
					memcpy ( pass, &s->passes[i+1], sizeof(shaderpass_t) );
				}
				s->numpasses--;
			}
			
			if ( s->passes[0].numtcmods ) {
				pass = s->passes;
				for ( i = 0; i < s->numpasses; i++, pass++ ) {
					if ( !pass->numtcmods )
						break;
				}
				
				memcpy ( &s->passes[0], pass, sizeof(shaderpass_t) );
			}
			
			s->passes[0].rgbgen = RGB_GEN_VERTEX;
			s->passes[0].alphagen = ALPHA_GEN_IDENTITY;
			s->passes[0].blendmode = 0;
			s->passes[0].flags &= ~(SHADER_PASS_BLEND|SHADER_PASS_ANIMMAP|SHADER_PASS_NOCOLORARRAY);
			s->passes[0].flags |= SHADER_PASS_DEPTHWRITE;
			s->passes[0].numMergedPasses = 1;
			s->passes[0].flush = R_RenderMeshGeneric;
			s->numpasses = 1;
			s->flags |= SHADER_DEPTHWRITE;
		}
	}
done:;

	pass = s->passes;
	for ( i = 0; i < s->numpasses; i++, pass++ ) {
		if ( !(pass->flags & SHADER_PASS_BLEND) ) {
			break;
		}
	}

	// all passes have blendfuncs
	if ( i == s->numpasses ) {
		int opaque;

		opaque = -1;
		pass = s->passes;
		for ( i = 0; i < s->numpasses; i++, pass++ ) {
			if ( (pass->blendsrc == GL_ONE) && (pass->blenddst == GL_ZERO) ) {
				opaque = i;
			}

			if ( pass->rgbgen == RGB_GEN_UNKNOWN ) { 
				if ( !s->fog_dist && !(pass->flags & SHADER_PASS_LIGHTMAP) ) 
					pass->rgbgen = RGB_GEN_IDENTITY_LIGHTING;
				else
					pass->rgbgen = RGB_GEN_IDENTITY;
			}
		}

		if ( !( s->flags & SHADER_SKY ) && !s->sort ) {
			if ( opaque == -1 )
				s->sort = SHADER_SORT_ADDITIVE;
			else if ( s->passes[opaque].flags & SHADER_PASS_ALPHAFUNC )
				s->sort = SHADER_SORT_OPAQUE + 1;
			else
				s->sort = SHADER_SORT_OPAQUE;
		}
	} else {
		int	j;
		shaderpass_t *sp;

		sp = s->passes;
		for ( j = 0; j < s->numpasses; j++, sp++ ) {
			if ( sp->rgbgen == RGB_GEN_UNKNOWN ) { 
				sp->rgbgen = RGB_GEN_IDENTITY;
			}
		}

		if ( !s->sort ) {
			if ( pass->flags & SHADER_PASS_ALPHAFUNC )
				s->sort = SHADER_SORT_OPAQUE + 1;
		}

		if ( !( s->flags & SHADER_DEPTHWRITE ) &&
			!( s->flags & SHADER_SKY ) )
		{
			pass->flags |= SHADER_PASS_DEPTHWRITE;
			s->flags |= SHADER_DEPTHWRITE;
		}
	}

	if ( s->numpasses >= 2 )
	{
		pass = s->passes;
		for ( i = 0; i < s->numpasses; )
		{
			if ( i == s->numpasses - 1 )
				break;

			pass = s->passes + i;
			Shader_SetPassFlush ( pass, pass + 1 );

			i += pass->numMergedPasses;
		}
	}

	if ( !s->sort ) {
		s->sort = SHADER_SORT_OPAQUE;
	}

	if ( (s->flags & SHADER_SKY) && (s->flags & SHADER_DEPTHWRITE) ) {
		s->flags &= ~SHADER_DEPTHWRITE;
	}

	Shader_SetFeatures ( s );
}

void Shader_UpdateRegistration (void)
{
	int i, j, l;
	shader_t *shader;
	shaderpass_t *pass;
	extern shader_t	*chars_shader;
	extern shader_t *propfont1_shader, *propfont1_glow_shader, *propfont2_shader;

	if ( chars_shader )
		chars_shader->registration_sequence = registration_sequence;

	if ( propfont1_shader )
		propfont1_shader->registration_sequence = registration_sequence;

	if ( propfont1_glow_shader )
		propfont1_glow_shader->registration_sequence = registration_sequence;

	if ( propfont2_shader )
		propfont2_shader->registration_sequence = registration_sequence;

	shader = r_shaders;
	for (i = 0; i < MAX_SHADERS; i++, shader++)
	{
		if ( !shader->registration_sequence )
			continue;
		if ( shader->registration_sequence != registration_sequence ) {
			Shader_Free ( shader );
			shader->registration_sequence = 0;
			continue;
		}

		if ( shader->flags & SHADER_SKY && shader->skydome ) {
			if ( shader->skydome->farbox_textures[0] ) {
				for ( j = 0; j < 6; j++ ) {
					if ( shader->skydome->farbox_textures[j] )
						shader->skydome->farbox_textures[j]->registration_sequence = registration_sequence;
				}
			}

			if ( shader->skydome->nearbox_textures[0] ) {
				for ( j = 0; j < 6; j++ ) {
					if ( shader->skydome->nearbox_textures[j] )
						shader->skydome->nearbox_textures[j]->registration_sequence = registration_sequence;
				}
			}
		}

		pass = shader->passes;
		for (j = 0; j < shader->numpasses; j++, pass++)
		{
			if ( pass->flags & SHADER_PASS_ANIMMAP ) {
				for (l = 0; l < pass->anim_numframes; l++) 
				{
					if ( pass->anim_frames[l] )
						pass->anim_frames[l]->registration_sequence = registration_sequence;
				}
			} else if ( pass->flags & SHADER_PASS_VIDEOMAP ) {
				// Shader_RunCinematic will do the job
				pass->cin->frame = -1;
			} else if ( !(pass->flags & SHADER_PASS_LIGHTMAP) ) {
				if ( pass->anim_frames[0] )
					pass->anim_frames[0]->registration_sequence = registration_sequence;
			} 
		}
	}
}

void Shader_UploadCinematic (shader_t *shader)
{
	int j;
	shaderpass_t *pass;

	// upload cinematics
	pass = shader->passes;
	for ( j = 0; j < shader->numpasses; j++, pass++ ) {
		if ( pass->flags & SHADER_PASS_VIDEOMAP ) {
			pass->anim_frames[0] = GL_ResampleCinematicFrame ( pass );
		}
	}
}

void Shader_RunCinematic (void)
{
	int i, j;
	shader_t *shader;
	shaderpass_t *pass;

	shader = r_shaders;
	for ( i = 0; i < MAX_SHADERS; i++, shader++ ) {
		if ( !shader->registration_sequence )
			continue;
		if ( !(shader->flags & SHADER_VIDEOMAP) )
			continue;

		pass = shader->passes;
		for ( j = 0; j < shader->numpasses; j++, pass++ ) {
			if ( !(pass->flags & SHADER_PASS_VIDEOMAP) )
				continue;

			// reinitialize
			if ( pass->cin->frame == -1 ) {
				GL_StopCinematic ( pass->cin );
				GL_PlayCinematic( pass->cin );

				if ( pass->cin->time == 0 ) {		// not found
					pass->flags &= ~SHADER_PASS_VIDEOMAP;
					Z_Free ( pass->cin );
				}

				continue;
			}

			GL_RunCinematic ( pass->cin );
		}
	}
}

int R_LoadShader ( char *name, int type )
{
	int i, f = -1;
	unsigned int offset;
	char shortname[MAX_QPATH], path[MAX_QPATH];
	char *buf, *ts = NULL;
	shader_t *s;
	shaderpass_t *pass;

	COM_StripExtension ( name, shortname );

	// test if already loaded
	for (i = 0; i < MAX_SHADERS; i++)
	{
		if (!r_shaders[i].registration_sequence)
		{
			if ( f == -1 )	// free shader
				f = i;
			continue;
		}

		if (!Q_stricmp (shortname, r_shaders[i].name) )
		{
			r_shaders[i].registration_sequence = registration_sequence;
			return i;
		}
	}

	if ( f == -1 )
	{
		Com_Error (ERR_FATAL, "R_LoadShader: Shader limit exceeded.");
		return f;
	}

	s = &r_shaders[f];
	memset ( s, 0, sizeof( shader_t ) );

	Com_sprintf ( s->name, MAX_QPATH, shortname );

	Shader_GetPathAndOffset( shortname, &ts, &offset );

	if ( ts ) {
		Com_sprintf ( path, sizeof(path), "scripts/%s", ts );
		if ( FS_LoadFile ( path, (void **)&buf ) <= 0 ) {
			ts = NULL;
		}
	}

	// the shader is in the shader scripts
	if ( ts )
	{
		char *ptr, *token;
		qboolean finish;

		// set defaults
		s->flags = SHADER_CULL_FRONT;
		s->registration_sequence = registration_sequence;

		ptr = buf + offset;
		token = COM_ParseExt (&ptr, true);

		if (!ptr || token[0] != '{')
			return -1;

		finish = false;
		while (ptr)
		{
			if ( finish )
				break;

			token = COM_ParseExt (&ptr, true);

			if (!token[0])
				continue;

			if (token[0] == '{') {
				Shader_Readpass ( s, &ptr );
			} else if (token[0] == '}') {
				break;
			} else {
				finish = Shader_Parsetok ( s, NULL, shaderkeys, token, &ptr );
			}
		}

		Shader_Finish ( s );
		FS_FreeFile ( buf );
	}
	else		// make a default shader
	{
		switch (type)
		{
			case SHADER_BSP:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_LIGHTMAP | SHADER_PASS_DEPTHWRITE | SHADER_PASS_NOCOLORARRAY;
				pass->tc_gen = TC_GEN_LIGHTMAP;
				pass->anim_frames[0] = NULL;
				pass->depthfunc = GL_LEQUAL;
				pass->blendmode = GL_REPLACE;
				pass->alphagen = ALPHA_GEN_IDENTITY;
				pass->rgbgen = RGB_GEN_IDENTITY;
				pass->numMergedPasses = 2;

				if ( qglMTexCoord2fSGIS ) {
					pass->numMergedPasses = 2;
					pass->flush = R_RenderMeshMultitextured;
				} else {
					pass->numMergedPasses = 1;
					pass->flush = R_RenderMeshGeneric;
				}
					
				pass = &s->passes[1];
				pass->flags = SHADER_PASS_BLEND | SHADER_PASS_NOCOLORARRAY;
				pass->tc_gen = TC_GEN_BASE;
				pass->anim_frames[0] = GL_FindImage (shortname, 0);
				pass->blendsrc = GL_ZERO;
				pass->blenddst = GL_SRC_COLOR;
				pass->blendmode = GL_MODULATE;
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen = RGB_GEN_IDENTITY;
				pass->alphagen = ALPHA_GEN_IDENTITY;

				if ( !qglMTexCoord2fSGIS ) {
					pass->numMergedPasses = 1;
					pass->flush = R_RenderMeshGeneric;
				}

				if ( !pass->anim_frames[0] ) {
					Com_DPrintf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", s->name, shortname );
					pass->anim_frames[0] = r_notexture;
				}

				s->numpasses = 2;
				s->numdeforms = 0;
				s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT;
				s->features = MF_STCOORDS|MF_LMCOORDS|MF_TRNORMALS;
				s->sort = SHADER_SORT_OPAQUE;
				s->registration_sequence = registration_sequence;
				break;

			case SHADER_BSP_VERTEX:
				pass = &s->passes[0];
				pass->tc_gen = TC_GEN_BASE;
				pass->anim_frames[0] = GL_FindImage (shortname, 0);
				pass->depthfunc = GL_LEQUAL;
				pass->flags = SHADER_PASS_DEPTHWRITE;
				pass->rgbgen = RGB_GEN_VERTEX;
				pass->alphagen = ALPHA_GEN_IDENTITY;
				pass->blendmode = GL_MODULATE;
				pass->numMergedPasses = 1;
				pass->flush = R_RenderMeshGeneric;

				if ( !pass->anim_frames[0] ) {
					Com_DPrintf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", s->name, shortname );
					pass->anim_frames[0] = r_notexture;
				}

				s->numpasses = 1;
				s->numdeforms = 0;
				s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT;
				s->features = MF_STCOORDS|MF_COLORS|MF_TRNORMALS;
				s->sort = SHADER_SORT_OPAQUE;
				s->registration_sequence = registration_sequence;
				break;

			case SHADER_BSP_FLARE:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_BLEND | SHADER_PASS_NOCOLORARRAY;
				pass->blendsrc = GL_ONE;
				pass->blenddst = GL_ONE;
				pass->blendmode = GL_MODULATE;
				pass->anim_frames[0] = GL_FindImage (shortname, 0);
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen = RGB_GEN_VERTEX;
				pass->alphagen = ALPHA_GEN_IDENTITY;
				pass->numtcmods = 0;
				pass->tc_gen = TC_GEN_BASE;
				pass->numMergedPasses = 1;
				pass->flush = R_RenderMeshGeneric;

				if ( !pass->anim_frames[0] ) {
					Com_DPrintf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", s->name, shortname );
					pass->anim_frames[0] = r_notexture;
				}

				s->numpasses = 1;
				s->numdeforms = 0;
				s->flags = SHADER_FLARE;
				s->features = MF_STCOORDS|MF_COLORS;
				s->sort = SHADER_SORT_ADDITIVE;
				s->registration_sequence = registration_sequence;
				break;

			case SHADER_MD3:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_DEPTHWRITE;
				pass->anim_frames[0] = GL_FindImage (shortname, 0);
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen = RGB_GEN_LIGHTING_DIFFUSE;
				pass->numtcmods = 0;
				pass->tc_gen = TC_GEN_BASE;
				pass->blendmode = GL_MODULATE;
				pass->numMergedPasses = 1;
				pass->flush = R_RenderMeshGeneric;

				if ( !pass->anim_frames[0] ) {
					Com_DPrintf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", s->name, shortname );
					pass->anim_frames[0] = r_notexture;
				}

				s->numpasses = 1;
				s->numdeforms = 0;
				s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT;
				s->features = MF_STCOORDS|MF_NORMALS;
				s->sort = SHADER_SORT_OPAQUE;
				s->registration_sequence = registration_sequence;
				break;

			case SHADER_2D:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_BLEND | SHADER_PASS_NOCOLORARRAY;
				pass->blendsrc = GL_SRC_ALPHA;
				pass->blenddst = GL_ONE_MINUS_SRC_ALPHA;
				pass->blendmode = GL_MODULATE;
				pass->anim_frames[0] = GL_FindImage (shortname, IT_NOPICMIP|IT_NOMIPMAP);
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen = RGB_GEN_VERTEX;
				pass->alphagen = ALPHA_GEN_VERTEX;
				pass->numtcmods = 0;
				pass->tc_gen = TC_GEN_BASE;
				pass->numMergedPasses = 1;
				pass->flush = R_RenderMeshGeneric;

				if ( !pass->anim_frames[0] ) {
					Com_DPrintf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", s->name, shortname );
					pass->anim_frames[0] = r_notexture;
				}

				s->numpasses = 1;
				s->numdeforms = 0;
				s->flags = SHADER_NOPICMIP|SHADER_NOMIPMAPS;
				s->features = MF_STCOORDS|MF_COLORS;
				s->sort = SHADER_SORT_ADDITIVE;
				s->registration_sequence = registration_sequence;
				break;

			default:
				return -1;
		}
	}

	return f;
}

shader_t *R_RegisterPic (char *name) 
{
	return &r_shaders[R_LoadShader (name, SHADER_2D)];
}

shader_t *R_RegisterShader (char *name)
{
	return &r_shaders[R_LoadShader (name, SHADER_BSP)];
}

shader_t *R_RegisterSkin (char *name)
{
	return &r_shaders[R_LoadShader (name, SHADER_MD3)];
}