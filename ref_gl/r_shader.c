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
// r_shader.c - based on code by Stephen C. Taylor
#include "r_local.h"

#define HASH_SIZE	128

typedef struct
{
    char			*keyword;
    void			(*func)( shader_t *shader, shaderpass_t *pass, char **ptr );
} shaderkey_t;

typedef struct shadercache_s {
	char name[MAX_QPATH];
	char *path;
	qboolean basedir;
	unsigned int offset;
	struct shadercache_s *hash_next;
} shadercache_t;

static shadercache_t *shader_hash[HASH_SIZE];

shader_t		r_shaders[MAX_SHADERS];

static char		r_skyboxname[MAX_QPATH];
static float	r_skyheight;

mempool_t		*r_shadersmempool;

char *Shader_Skip( char *ptr );
static qboolean Shader_Parsetok( shader_t *shader, shaderpass_t *pass, shaderkey_t *keys,
		char *token, char **ptr );
static void Shader_ParseFunc( char **args, shaderfunc_t *func );
static void Shader_MakeCache( char *name, char *path );
static unsigned int Shader_GetCache( char *name, shadercache_t **cache );

//===========================================================================

static char *Shader_ParseString ( char **ptr )
{
	char *token;

	if ( !ptr || !(*ptr) ) {
		return "";
	}
	if ( !**ptr || **ptr == '}' ) {
		return "";
	}

	token = COM_ParseExt ( ptr, qfalse );
	strlwr ( token );
	
	return token;
}

static float Shader_ParseFloat ( char **ptr )
{
	if ( !ptr || !(*ptr) ) {
		return 0;
	}
	if ( !**ptr || **ptr == '}' ) {
		return 0;
	}

	return atof ( COM_ParseExt ( ptr, qfalse ) );
}

static void Shader_ParseVector ( char **ptr, float *v, unsigned int size )
{
	int i;
	char *token;
	qboolean bracket;

	if ( !size ) {
		return;
	} else if ( size == 1 ) {
		Shader_ParseFloat ( ptr );
		return;
	}

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "(") ) {
		bracket = qtrue;
		token = Shader_ParseString ( ptr );
	} else if ( token[0] == '(' ) {
		bracket = qtrue;
		token = &token[1];
	} else {
		bracket = qfalse;
	}

	v[0] = atof ( token );
	for ( i = 1; i < size-1; i++ ) {
		v[i] = Shader_ParseFloat ( ptr );
	}

	token = Shader_ParseString ( ptr );
	if ( !token[0] ) {
		v[i] = 0;
	} else if ( token[strlen(token)-1] == ')' ) {
		token[strlen(token)-1] = 0;
		v[i] = atof ( token );
	} else {
		v[i] = atof ( token );
		if ( bracket ) {
			Shader_ParseString ( ptr );
		}
	}
}

static void Shader_ParseSkySides ( char **ptr, shader_t **shaders, qboolean farbox )
{
	int i;
	char *token;
	char path[MAX_QPATH];
	qboolean noskybox = qfalse;
	static char	*suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

	token = Shader_ParseString ( ptr );
	if ( token[0] == '-' ) { 
		noskybox = qtrue;
	} else {
		for ( i = 0; i < 6; i++ ) {
			Com_sprintf ( path, sizeof(path), "%s_%s", token, suf[i] );
			shaders[i] = R_Shader_Load ( path, farbox ? SHADER_FARBOX : SHADER_NEARBOX, qtrue );
			shaders[i]->registration_sequence = registration_sequence;

			if ( !shaders[i]->passes[0].anim_frames[0] ) {
				noskybox = qtrue;
				break;
			}
		}
	}

	if ( noskybox ) {
		for ( i = 0; i < 6; i++ ) {
			shaders[i] = NULL;
		}
	}
}

static void Shader_ParseFunc ( char **ptr, shaderfunc_t *func )
{
	char *token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "sin") ) {
	    func->type = SHADER_FUNC_SIN;
	} else if ( !Q_stricmp (token, "triangle") ) {
	    func->type = SHADER_FUNC_TRIANGLE;
	} else if ( !Q_stricmp (token, "square") ) {
	    func->type = SHADER_FUNC_SQUARE;
	} else if ( !Q_stricmp (token, "sawtooth") ) {
	    func->type = SHADER_FUNC_SAWTOOTH;
	} else if (!Q_stricmp (token, "inversesawtooth") ) {
	    func->type = SHADER_FUNC_INVERSESAWTOOTH;
	} else if (!Q_stricmp (token, "noise") ) {
	    func->type = SHADER_FUNC_NOISE;
	}

	func->args[0] = Shader_ParseFloat ( ptr );
	func->args[1] = Shader_ParseFloat ( ptr );
	func->args[2] = Shader_ParseFloat ( ptr );
	func->args[3] = Shader_ParseFloat ( ptr );
}

//===========================================================================

static int Shader_SetImageFlags ( shader_t *shader )
{
	int flags = 0;

	if ( shader->flags & SHADER_SKY ) {
		flags |= IT_SKY;
	}
	if ( shader->flags & SHADER_NOMIPMAPS ) {
		flags |= IT_NOMIPMAP;
	}
	if ( shader->flags & SHADER_NOPICMIP ) {
		flags |= IT_NOPICMIP;
	}

	return flags;
}

static image_t *Shader_FindImage ( char *name, int flags )
{
	if ( !Q_stricmp (name, "$whiteimage") || !Q_stricmp (name, "*white") ) {
		return r_whitetexture;
	} else if ( !Q_stricmp (name, "$particleimage") || !Q_stricmp (name, "*particle") ) {
		return r_particletexture;
	} else {
		return GL_FindImage ( name, flags );
    }
}

/****************** shader keyword functions ************************/

static void Shader_Cull ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	shader->flags &= ~(SHADER_CULL_FRONT|SHADER_CULL_BACK);

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "disable") || !Q_stricmp (token, "none") || !Q_stricmp (token, "twosided") ) {
	} else if ( !Q_stricmp (token, "front") ) {
		shader->flags |= SHADER_CULL_FRONT;
	} else if ( !Q_stricmp (token, "back") || !Q_stricmp (token, "backside") || !Q_stricmp (token, "backsided") ) {
		shader->flags |= SHADER_CULL_BACK;
	} else {
		shader->flags |= SHADER_CULL_FRONT;
	}
}

static void Shader_NoMipMaps ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->flags |= (SHADER_NOMIPMAPS|SHADER_NOPICMIP);
}

static void Shader_NoPicMip ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->flags |= SHADER_NOPICMIP;
}

static void Shader_DeformVertexes ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;
	deformv_t *deformv;

	if ( shader->numdeforms >= SHADER_DEFORM_MAX ) {
		return;
	}

	deformv = &shader->deforms[shader->numdeforms];

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "wave") ) {
		deformv->type = DEFORMV_WAVE;
		deformv->args[0] = Shader_ParseFloat ( ptr );
		if ( deformv->args[0] ) {
			deformv->args[0] = 1.0f / deformv->args[0];
		}
		Shader_ParseFunc ( ptr, &deformv->func );
	} else if ( !Q_stricmp (token, "normal") ) {
		deformv->type = DEFORMV_NORMAL;
		deformv->args[0] = Shader_ParseFloat ( ptr );
		deformv->args[1] = Shader_ParseFloat ( ptr );
	} else if ( !Q_stricmp (token, "bulge") ) {
		deformv->type = DEFORMV_BULGE;
		Shader_ParseVector ( ptr, deformv->args, 3 );
		shader->flags |= SHADER_DEFORMV_BULGE;
	} else if ( !Q_stricmp (token, "move") ) {
		deformv->type = DEFORMV_MOVE;
		Shader_ParseVector ( ptr, deformv->args, 3 );
		Shader_ParseFunc ( ptr, &deformv->func );
	} else if ( !Q_stricmp (token, "autosprite") ) {
		deformv->type = DEFORMV_AUTOSPRITE;
		shader->flags |= SHADER_AUTOSPRITE;
	} else if ( !Q_stricmp (token, "autosprite2") ) {
		deformv->type = DEFORMV_AUTOSPRITE2;
		shader->flags |= SHADER_AUTOSPRITE;
	} else if ( !Q_stricmp (token, "projectionShadow") ) {
		deformv->type = DEFORMV_PROJECTION_SHADOW;
	} else if ( !Q_stricmp (token, "autoparticle") ) {
		deformv->type = DEFORMV_AUTOPARTICLE;
	} else {
		return;
	}

	shader->numdeforms++;
}


static void Shader_SkyParms ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	float skyheight;
	skydome_t *skydome;

	if ( shader->skydome ) {
		R_FreeSkydome ( shader->skydome );
	}

	skydome = (skydome_t *)Shader_Malloc ( sizeof(skydome_t) );
	shader->skydome = skydome;

	Shader_ParseSkySides ( ptr, skydome->farbox_shaders, qtrue );

	skyheight = Shader_ParseFloat ( ptr );
	if ( !skyheight ) {
		skyheight = 512.0f;
	}

	Shader_ParseSkySides ( ptr, skydome->nearbox_shaders, qfalse );

	R_CreateSkydome ( skydome, skyheight );

	shader->flags |= SHADER_SKY;
	shader->sort = SHADER_SORT_SKY;
}

static void Shader_FogParms ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	float div;
	vec3_t color, fcolor;

	if ( !r_ignorehwgamma->value )
		div = 1.0f / pow(2, max(0, floor(r_overbrightbits->value)));
	else
		div = 1.0f;

	Shader_ParseVector ( ptr, color, 3 );
	VectorScale ( color, div, color );
	ColorNormalize ( color, fcolor );

	shader->fog_color[0] = R_FloatToByte ( fcolor[0] );
	shader->fog_color[1] = R_FloatToByte ( fcolor[1] );
	shader->fog_color[2] = R_FloatToByte ( fcolor[2] );
	shader->fog_color[3] = 255;	
	shader->fog_dist = Shader_ParseFloat ( ptr );

	if ( shader->fog_dist <= 0.0f ) {
		shader->fog_dist = 128.0f;
	}
	shader->fog_dist = 1.0f / shader->fog_dist;
}

static void Shader_Sort ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp( token, "portal" ) ) {
		shader->sort = SHADER_SORT_PORTAL;
	} else if( !Q_stricmp( token, "sky" ) ) {
		shader->sort = SHADER_SORT_SKY;
	} else if( !Q_stricmp( token, "opaque" ) ) {
		shader->sort = SHADER_SORT_OPAQUE;
	} else if( !Q_stricmp( token, "banner" ) ) {
		shader->sort = SHADER_SORT_BANNER;
	} else if( !Q_stricmp( token, "underwater" ) ) {
		shader->sort = SHADER_SORT_UNDERWATER;
	} else if( !Q_stricmp( token, "additive" ) ) {
		shader->sort = SHADER_SORT_ADDITIVE;
	} else if( !Q_stricmp( token, "nearest" ) ) {
		shader->sort = SHADER_SORT_NEAREST;
	} else {
		shader->sort = atoi ( token );
		clamp ( shader->sort, SHADER_SORT_NONE, SHADER_SORT_NEAREST );
	}
}

static void Shader_Portal ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->sort = SHADER_SORT_PORTAL;
}

static void Shader_PolygonOffset ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->flags |= SHADER_POLYGONOFFSET;
}

static void Shader_EntityMergable ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->flags |= SHADER_ENTITY_MERGABLE;
}

static shaderkey_t shaderkeys[] =
{
    {"cull",			Shader_Cull },
    {"skyparms",		Shader_SkyParms },
	{"fogparms",		Shader_FogParms },
    {"nomipmaps",		Shader_NoMipMaps },
	{"nopicmip",		Shader_NoPicMip },
	{"polygonoffset",	Shader_PolygonOffset },
	{"sort",			Shader_Sort },
    {"deformvertexes",	Shader_DeformVertexes },
	{"portal",			Shader_Portal },
	{"entitymergable",	Shader_EntityMergable },
    {NULL,				NULL}
};

// ===============================================================

static void Shaderpass_MapExt ( shader_t *shader, shaderpass_t *pass, int addFlags, char **ptr )
{
	int flags;
	char *token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "$lightmap") ) {
		pass->tcgen = TC_GEN_LIGHTMAP;
		pass->flags |= SHADER_PASS_LIGHTMAP;
		pass->anim_frames[0] = NULL;
	} else {
		flags = Shader_SetImageFlags ( shader ) | addFlags;
		pass->tcgen = TC_GEN_BASE;
		pass->anim_frames[0] = Shader_FindImage ( token, flags );

		if ( !pass->anim_frames[0] ) {
			pass->anim_frames[0] = r_notexture;
			Com_DPrintf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", shader->name, token );
		}
    }
}

static void Shaderpass_AnimMapExt ( shader_t *shader, shaderpass_t *pass, int addFlags, char **ptr )
{
    int flags;
	char *token;
	image_t *image;

	flags = Shader_SetImageFlags ( shader ) | addFlags;

	pass->tcgen = TC_GEN_BASE;
    pass->flags |= SHADER_PASS_ANIMMAP;
    pass->anim_fps = (int)Shader_ParseFloat ( ptr );
	pass->anim_numframes = 0;

    for ( ; ; ) {
		token = Shader_ParseString ( ptr );
		if ( !token[0] ) {
			break;
		}

		if ( pass->anim_numframes < SHADER_ANIM_FRAMES_MAX ) {
			image = Shader_FindImage ( token, flags );

			if ( !image ) {
				pass->anim_frames[pass->anim_numframes++] = r_notexture;
				Com_DPrintf ( S_COLOR_YELLOW "Shader %s has an animmap with no image: %s.\n", shader->name, token );
			} else {
				pass->anim_frames[pass->anim_numframes++] = image;
			}
		}
	}
}

static void Shaderpass_Map ( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_MapExt ( shader, pass, 0, ptr );
}

static void Shaderpass_ClampMap ( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_MapExt ( shader, pass, IT_CLAMP, ptr );
}

static void Shaderpass_AnimMap ( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_AnimMapExt ( shader, pass, 0, ptr );
}

static void Shaderpass_AnimClampMap ( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_AnimMapExt ( shader, pass, IT_CLAMP, ptr );
}

static void Shaderpass_VideoMap ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char		*token;
	char		name[MAX_OSPATH];

	token = Shader_ParseString ( ptr );
	COM_StripExtension ( token, name );

	if ( pass->cin )
		Shader_Free ( pass->cin );

	pass->cin = (cinematics_t *)Shader_Malloc ( sizeof(cinematics_t) );
	pass->cin->frame = -1;
	Com_sprintf ( pass->cin->name, sizeof(pass->cin->name), "video/%s.RoQ", name );

	pass->flags |= SHADER_PASS_VIDEOMAP;
	shader->flags |= SHADER_VIDEOMAP;
}

static void Shaderpass_RGBGen ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char		*token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "identitylighting") ) {
		pass->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
	} else if ( !Q_stricmp (token, "identity") ) {
		pass->rgbgen.type = RGB_GEN_IDENTITY;
	} else if ( !Q_stricmp (token, "wave") ) {
		pass->rgbgen.type = RGB_GEN_COLORWAVE;
		pass->rgbgen.args[0] = 1.0f;
		pass->rgbgen.args[1] = 1.0f;
		pass->rgbgen.args[2] = 1.0f;
		Shader_ParseFunc ( ptr, &pass->rgbgen.func );
	} else if ( !Q_stricmp (token, "colorwave") ) {
		pass->rgbgen.type = RGB_GEN_COLORWAVE;
		Shader_ParseVector ( ptr, pass->rgbgen.args, 3 );
		Shader_ParseFunc ( ptr, &pass->rgbgen.func );
	} else if ( !Q_stricmp(token, "entity") ) {
		pass->rgbgen.type = RGB_GEN_ENTITY;
	} else if ( !Q_stricmp (token, "oneMinusEntity") ) {
		pass->rgbgen.type = RGB_GEN_ONE_MINUS_ENTITY;
	} else if ( !Q_stricmp (token, "vertex")) {
		pass->rgbgen.type = RGB_GEN_VERTEX;
	} else if ( !Q_stricmp (token, "oneMinusVertex") ) {
		pass->rgbgen.type = RGB_GEN_ONE_MINUS_VERTEX;
	} else if ( !Q_stricmp (token, "lightingDiffuse") ) {
		pass->rgbgen.type = RGB_GEN_LIGHTING_DIFFUSE;
	} else if ( !Q_stricmp (token, "exactvertex") ) {
		pass->rgbgen.type = RGB_GEN_EXACT_VERTEX;
	} else if ( !Q_stricmp (token, "const") || !Q_stricmp (token, "constant") ) {
		pass->rgbgen.type = RGB_GEN_CONST;
		Shader_ParseVector ( ptr, pass->rgbgen.args, 3 );
	}
}

static void Shaderpass_AlphaGen ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char		*token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "portal") ) {
		pass->alphagen.type = ALPHA_GEN_PORTAL;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat (ptr) );
		if ( !pass->alphagen.args[0] ) {
			pass->alphagen.args[0] = 256;
		}
		pass->alphagen.args[0] = 1.0f / pass->alphagen.args[0];
		shader->flags |= SHADER_AGEN_PORTAL;
	} else if ( !Q_stricmp (token, "vertex") ) {
		pass->alphagen.type = ALPHA_GEN_VERTEX;
	} else if ( !Q_stricmp (token, "entity") ) {
		pass->alphagen.type = ALPHA_GEN_ENTITY;
	} else if ( !Q_stricmp (token, "wave") ) {
		pass->alphagen.type = ALPHA_GEN_WAVE;
		Shader_ParseFunc ( ptr, &pass->alphagen.func );
	} else if ( !Q_stricmp (token, "lightingspecular") ) {
		pass->alphagen.type = ALPHA_GEN_SPECULAR;
	} else if ( !Q_stricmp (token, "const") || !Q_stricmp (token, "constant") ) {
		pass->alphagen.type = ALPHA_GEN_CONST;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat (ptr) );
	} else if ( !Q_stricmp (token, "dot") ) {
		pass->alphagen.type = ALPHA_GEN_DOT;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat (ptr) );
		pass->alphagen.args[1] = fabs( Shader_ParseFloat (ptr) );
		if ( !pass->alphagen.args[1] ) {
			pass->alphagen.args[1] = 1.0f;
		}
	} else if ( !Q_stricmp (token, "oneMinusDot") ) {
		pass->alphagen.type = ALPHA_GEN_ONE_MINUS_DOT;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat (ptr) );
		pass->alphagen.args[1] = fabs( Shader_ParseFloat (ptr) );
		if ( !pass->alphagen.args[1] ) {
			pass->alphagen.args[1] = 1.0f;
		}
	}
}

static void Shaderpass_BlendFunc ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char		*token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "blend") ) {
		pass->blendsrc = GL_SRC_ALPHA;
		pass->blenddst = GL_ONE_MINUS_SRC_ALPHA;
	} else if ( !Q_stricmp (token, "filter") ) {
		pass->blendsrc = GL_DST_COLOR;
		pass->blenddst = GL_ZERO;
	} else if ( !Q_stricmp (token, "add") ) {
		pass->blendsrc = pass->blenddst = GL_ONE;
	} else {
		int i;
		unsigned int *blend;

		for ( i = 0; i < 2; i++ )
		{
			blend = (i == 0) ? &pass->blendsrc : &pass->blenddst;

			if ( !Q_stricmp ( token, "gl_zero") )
				*blend = GL_ZERO;
			else if ( !Q_stricmp (token, "gl_one") )
				*blend = GL_ONE;
			else if ( !Q_stricmp (token, "gl_dst_color") )
				*blend = GL_DST_COLOR;
			else if ( !Q_stricmp (token, "gl_one_minus_src_alpha") )
				*blend = GL_ONE_MINUS_SRC_ALPHA;
			else if ( !Q_stricmp (token, "gl_src_alpha") )
				*blend = GL_SRC_ALPHA;
			else if ( !Q_stricmp (token, "gl_src_color") )
				*blend = GL_SRC_COLOR;
			else if ( !Q_stricmp (token, "gl_one_minus_dst_color") )
				*blend = GL_ONE_MINUS_DST_COLOR;
			else if ( !Q_stricmp (token, "gl_one_minus_src_color") )
				*blend = GL_ONE_MINUS_SRC_COLOR;
			else if ( !Q_stricmp (token, "gl_dst_alpha") )
				*blend = GL_DST_ALPHA;
			else if ( !Q_stricmp (token, "gl_one_minus_dst_alpha") )
				*blend = GL_ONE_MINUS_DST_ALPHA;
			else
				*blend = GL_ONE;

			if ( !i ) {
				token = Shader_ParseString ( ptr );
			}
		}
    }

	pass->flags |= SHADER_PASS_BLEND;
}

static void Shaderpass_AlphaFunc ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString ( ptr );
    if ( !Q_stricmp (token, "gt0") ) {
		pass->alphafunc = SHADER_ALPHA_GT0;
	} else if ( !Q_stricmp (token, "lt128") ) {
		pass->alphafunc = SHADER_ALPHA_LT128;
	} else if ( !Q_stricmp (token, "ge128") ) {
		pass->alphafunc = SHADER_ALPHA_GE128;
    } else {
		return;
	}

    pass->flags |= SHADER_PASS_ALPHAFUNC;
}

static void Shaderpass_DepthFunc ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString ( ptr );
    if ( !Q_stricmp (token, "equal") )
		pass->depthfunc = GL_EQUAL;
    else if ( !Q_stricmp (token, "lequal") )
		pass->depthfunc = GL_LEQUAL;
	else if ( !Q_stricmp (token, "gequal") )
		pass->depthfunc = GL_GEQUAL;
}

static void Shaderpass_DepthWrite ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
    shader->flags |= SHADER_DEPTHWRITE;
    pass->flags |= SHADER_PASS_DEPTHWRITE;
}

static void Shaderpass_TcMod ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	int i;
	tcmod_t *tcmod;
	char *token;

	if (pass->numtcmods >= SHADER_TCMOD_MAX) {
		return;
	}

	tcmod = &pass->tcmods[pass->numtcmods];

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "rotate") ) {
		tcmod->args[0] = -Shader_ParseFloat ( ptr ) / 360.0f;
		if ( !tcmod->args[0] ) {
			return;
		}
		tcmod->type = SHADER_TCMOD_ROTATE;
	} else if ( !Q_stricmp (token, "scale") ) {
		tcmod->args[0] = Shader_ParseFloat ( ptr );
		tcmod->args[1] = Shader_ParseFloat ( ptr );
		tcmod->type = SHADER_TCMOD_SCALE;
	} else if ( !Q_stricmp (token, "scroll") ) {
		tcmod->args[0] = Shader_ParseFloat ( ptr );
		tcmod->args[1] = Shader_ParseFloat ( ptr );
		tcmod->type = SHADER_TCMOD_SCROLL;
	} else if ( !Q_stricmp (token, "stretch") ) {
		shaderfunc_t func;

		Shader_ParseFunc ( ptr, &func );

		tcmod->args[0] = func.type;
		for (i = 1; i < 5; ++i)
			tcmod->args[i] = func.args[i-1];
		tcmod->type = SHADER_TCMOD_STRETCH;
	} else if ( !Q_stricmp (token, "transform") ) {
		for (i = 0; i < 6; ++i)
			tcmod->args[i] = Shader_ParseFloat ( ptr );
		tcmod->type = SHADER_TCMOD_TRANSFORM;
	} else if ( !Q_stricmp (token, "turb") ) {
		for (i = 0; i < 4; i++)
			tcmod->args[i] = Shader_ParseFloat ( ptr );
		tcmod->type = SHADER_TCMOD_TURB;
	} else {
		return;
	}

	pass->numtcmods++;
}


static void Shaderpass_TcGen ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "base") ) {
		pass->tcgen = TC_GEN_BASE;
	} else if ( !Q_stricmp (token, "lightmap") ) {
		pass->tcgen = TC_GEN_LIGHTMAP;
	} else if ( !Q_stricmp (token, "environment") ) {
		pass->tcgen = TC_GEN_ENVIRONMENT;
	} else if ( !Q_stricmp (token, "vector") ) {
		pass->tcgen = TC_GEN_VECTOR;
		Shader_ParseVector ( ptr, pass->tcgenVec[0], 4 );
		Shader_ParseVector ( ptr, pass->tcgenVec[1], 4 );
	}
}

static void Shaderpass_Detail ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	pass->flags |= SHADER_PASS_DETAIL;
}

static shaderkey_t shaderpasskeys[] =
{
    {"rgbgen",		Shaderpass_RGBGen },
    {"blendfunc",	Shaderpass_BlendFunc },
    {"depthfunc",	Shaderpass_DepthFunc },
    {"depthwrite",	Shaderpass_DepthWrite },
    {"alphafunc",	Shaderpass_AlphaFunc },
    {"tcmod",		Shaderpass_TcMod },
    {"map",			Shaderpass_Map },
    {"animmap",		Shaderpass_AnimMap },
    {"clampmap",	Shaderpass_ClampMap },
    {"animclampmap",Shaderpass_AnimClampMap },
	{"videomap",	Shaderpass_VideoMap },
    {"tcgen",		Shaderpass_TcGen },
	{"alphagen",	Shaderpass_AlphaGen },
	{"detail",		Shaderpass_Detail },
    {NULL,			NULL }
};

// ===============================================================

qboolean Shader_Init (void)
{
	char *dirptr, *fileptr;
	int i, filelen, dirlen, numfiles;
	char *shaderbuf, *shaderbuf2;

	Com_Printf ( "Initializing Shaders:\n" );

	r_shadersmempool = Mem_AllocPool ( NULL, "Shaders" );

	shaderbuf = Mem_TempMalloc (MAX_QPATH * 256);
	shaderbuf2 = Mem_TempMalloc (MAX_QPATH * 256);

	numfiles = FS_GetFileListExt ( "scripts", "shader", shaderbuf, MAX_QPATH * 256, shaderbuf2, MAX_QPATH * 256 );
	if ( !numfiles ) {
		Mem_TempFree (shaderbuf);
		Mem_TempFree (shaderbuf2);
		Com_Error ( ERR_DROP, "Could not find any shaders!");
		return qfalse;
	}

	// now load all the scripts
	fileptr = shaderbuf;
	dirptr = shaderbuf2;
	memset ( shader_hash, 0, sizeof(shadercache_t *)*HASH_SIZE );

	for (i=0; i<numfiles; i++, fileptr += filelen+1, dirptr += dirlen+1) {
		filelen = strlen (fileptr);
		dirlen = strlen (dirptr);

		if ( !fileptr ) {
			continue;
		}

		Shader_MakeCache ( fileptr, dirptr );
	}

	Mem_TempFree (shaderbuf);
	Mem_TempFree (shaderbuf2);

	return qtrue;
}

static void Shader_MakeCache ( char *name, char *path )
{
	unsigned int key;
	char filename[MAX_QPATH];
	char *buf, *ptr, *token, *t;
	shadercache_t *cache;
	int size;
	char basepath[MAX_OSPATH];
	qboolean basedir = qfalse;
	extern cvar_t *fs_basepath, *fs_basedir;

	Com_sprintf ( basepath, sizeof(basepath), "%s/%s", fs_basepath->string, fs_basedir->string );
	if ( !Q_strnicmp (path, basepath, strlen (basepath)) ) {
		basedir = qtrue;
	}

	Com_sprintf( filename, sizeof(filename), "scripts/%s", name );
	Com_Printf ( "...loading '%s'\n", filename );

	size = FS_LoadFile ( filename, (void **)&buf );
	if ( !buf || size <= 0 ) {
		return;
	}

	ptr = buf;
	do
	{
		if ( !ptr || ptr - buf >= size )
			break;

		token = COM_ParseExt ( &ptr, qtrue );
		if ( !token[0] || !ptr || ptr - buf >= size )
			break;

		t = NULL;
		key = Shader_GetCache ( token, &cache );
		if ( cache ) {
			// replace only if original file is in the basedir
			if ( cache->basedir ) {
				Shader_Free (cache->path);
				cache->path = Shader_Malloc ( strlen(name)+1 );
				strcpy ( cache->path, name );
				cache->offset = ptr - buf;
			}

			ptr = Shader_Skip ( ptr );
			continue;
		}

		cache = ( shadercache_t * )Shader_Malloc ( sizeof(shadercache_t) );
		cache->hash_next = shader_hash[key];
		cache->path = Shader_Malloc ( strlen(name)+1 );
		strcpy ( cache->path, name );
		cache->basedir = basedir;
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
    tok = COM_ParseExt ( &ptr, qtrue );

	if (!ptr)
		return NULL;
    
	if ( tok[0] != '{' ) {
		tok = COM_ParseExt ( &ptr, qtrue );
	}

    for (brace_count = 1; brace_count > 0 && ptr; )
    {
		tok = COM_ParseExt ( &ptr, qtrue );
		if ( !tok[0] ) {
			return NULL;
		}

		if (tok[0] == '{') {
			brace_count++;
		} else if (tok[0] == '}') {
			brace_count--;
		}
    }

	return ptr;
}

static unsigned int Shader_GetCache ( char *name, shadercache_t **cache )
{
	unsigned int key;
	shadercache_t *c;

	*cache = NULL;

	key = Com_HashKey ( name, HASH_SIZE );
	c = shader_hash[key];

	for ( ; c; c = c->hash_next ) {
		if ( !Q_stricmp (c->name, name) ) {
			*cache = c;
			return key;
		}
	}

	return key;
}

void Shader_FreePass (shaderpass_t *pass)
{
	if ( pass->flags & SHADER_PASS_VIDEOMAP ) {
		GL_StopCinematic ( pass->cin );
		Shader_Free ( pass->cin );
		pass->cin = NULL;
	}
}

void Shader_FreeShader (shader_t *shader)
{
	int i;
	shaderpass_t *pass;

	if ( (shader->flags & SHADER_SKY) && shader->skydome ) {
		R_FreeSkydome ( shader->skydome );
		shader->skydome = NULL;
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

	shader = r_shaders;
	for (i = 0; i < MAX_SHADERS; i++, shader++)
	{
		if ( !shader->name[0] )
			continue;
		
		Shader_FreeShader ( shader );
	}

	Mem_FreePool ( &r_shadersmempool );

	memset (r_shaders, 0, sizeof(r_shaders));
	memset (shader_hash, 0, sizeof(shader_hash));
}

void Shader_SetBlendmode ( shaderpass_t *pass )
{
	if ( !pass->anim_frames[0] && !(pass->flags & SHADER_PASS_LIGHTMAP) ) {
		pass->blendmode = 0;
		return;
	}

	if ( !(pass->flags & SHADER_PASS_BLEND) && qglMTexCoord2fSGIS ) {
		if ( (pass->rgbgen.type == RGB_GEN_IDENTITY) && (pass->alphagen.type == ALPHA_GEN_IDENTITY) ) {
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
	qboolean ignore;
	static shader_t dummy;

	if ( shader->numpasses >= SHADER_PASS_MAX ) {
		ignore = qtrue;
		shader = &dummy;
		shader->numpasses = 1;
		pass = shader->passes;
	} else {
		ignore = qfalse;
		pass = &shader->passes[shader->numpasses++];
	}
	
    // Set defaults
    pass->flags = 0;
    pass->anim_frames[0] = NULL;
	pass->anim_numframes = 0;
    pass->depthfunc = GL_LEQUAL;
    pass->rgbgen.type = RGB_GEN_UNKNOWN;
	pass->alphagen.type = ALPHA_GEN_UNKNOWN;
	pass->tcgen = TC_GEN_BASE;
	pass->numtcmods = 0;

	// default to R_RenderMeshGeneric
	pass->numMergedPasses = 1;
	pass->flush = R_RenderMeshGeneric;

	while ( ptr )
	{
		token = COM_ParseExt (ptr, qtrue);

		if ( !token[0] ) {
			continue;
		} else if ( token[0] == '}' ) {
			break;
		} else if ( Shader_Parsetok (shader, pass, shaderpasskeys, token, ptr) ) {
			break;
		}
	}

	// check some things 
	if ( ignore ) {
		Shader_FreeShader ( shader );
		return;
	}

	if ( (pass->blendsrc == GL_ONE) && (pass->blenddst == GL_ZERO) ) {
		pass->flags |= SHADER_PASS_DEPTHWRITE;
		shader->flags |= SHADER_DEPTHWRITE;
	}

	switch (pass->rgbgen.type)
	{
		case RGB_GEN_IDENTITY_LIGHTING:
		case RGB_GEN_IDENTITY:
		case RGB_GEN_CONST:
		case RGB_GEN_COLORWAVE:
		case RGB_GEN_ENTITY:
		case RGB_GEN_ONE_MINUS_ENTITY:
		case RGB_GEN_UNKNOWN:	// assume RGB_GEN_IDENTITY or RGB_GEN_IDENTITY_LIGHTING

			switch (pass->alphagen.type)
			{
				case ALPHA_GEN_UNKNOWN:
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

	for (key = keys; key->keyword != NULL; key++)
	{
		if (!Q_stricmp (token, key->keyword))
		{
			if (key->func)
				key->func ( shader, pass, ptr );

			return ( ptr && *ptr && **ptr == '}' );
		}
	}

	// Next Line
	while (ptr)
	{
		token = COM_ParseExt ( ptr, qfalse );
		if ( !token[0] ) {
			break;
		}
	}

	return qfalse;
}

void Shader_SetPassFlush ( shaderpass_t *pass, shaderpass_t *pass2 )
{
	if ( ((pass->flags & SHADER_PASS_DETAIL) && !r_detailtextures->value) ||
		((pass2->flags & SHADER_PASS_DETAIL) && !r_detailtextures->value) ||
		 (pass->flags & SHADER_PASS_VIDEOMAP) || (pass2->flags & SHADER_PASS_VIDEOMAP) || 
		 ((pass->flags & SHADER_PASS_ALPHAFUNC) && (pass2->depthfunc != GL_EQUAL)) ) {
		return;
	}
	if ( (pass->depthfunc != pass2->depthfunc) || (pass2->flags & SHADER_PASS_ALPHAFUNC) ) {
		return;
	}
	if ( pass2->rgbgen.type != RGB_GEN_IDENTITY || pass2->alphagen.type != ALPHA_GEN_IDENTITY ) {
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
	qboolean trnormals;
	shaderpass_t *pass;

	s->features = MF_NONE;
	
	for ( i = 0, trnormals = qtrue; i < s->numdeforms; i++ ) {
		switch ( s->deforms[i].type ) {
			case DEFORMV_BULGE:
			case DEFORMV_WAVE:
				trnormals = qfalse;
			case DEFORMV_NORMAL:
				s->features |= MF_NORMALS;
				break;
			case DEFORMV_MOVE:
				break;
			default:
				trnormals = qfalse;
				break;
		}
	}

	if ( trnormals ) {
		s->features |= MF_TRNORMALS;
	}

	for ( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ ) {
		switch ( pass->rgbgen.type ) {
			case RGB_GEN_LIGHTING_DIFFUSE:
				s->features |= MF_NORMALS;
				break;
			case RGB_GEN_VERTEX:
			case RGB_GEN_ONE_MINUS_VERTEX:
			case RGB_GEN_EXACT_VERTEX:
				s->features |= MF_COLORS;
				break;
		}

		switch ( pass->alphagen.type ) {
			case ALPHA_GEN_SPECULAR:
			case ALPHA_GEN_DOT:
			case ALPHA_GEN_ONE_MINUS_DOT:
				s->features |= MF_NORMALS;
				break;
			case ALPHA_GEN_VERTEX:
				s->features |= MF_COLORS;
				break;
		}

		switch ( pass->tcgen ) {
			default:
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

	if ( (s->flags & SHADER_POLYGONOFFSET) && !s->sort ) {
		s->sort = SHADER_SORT_OPAQUE + 1;
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
			if ( pass->rgbgen.type == RGB_GEN_VERTEX )
				break;
		}

		if ( i < s->numpasses ) {		// we found it
			pass->flags |= SHADER_CULL_FRONT;
			pass->flags &= ~(SHADER_PASS_BLEND|SHADER_PASS_ANIMMAP);
			pass->blendmode = 0;
			pass->flags |= SHADER_PASS_DEPTHWRITE;
			pass->alphagen.type = ALPHA_GEN_IDENTITY;
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
			
			s->passes[0].rgbgen.type = RGB_GEN_VERTEX;
			s->passes[0].alphagen.type = ALPHA_GEN_IDENTITY;
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

			if ( pass->rgbgen.type == RGB_GEN_UNKNOWN ) { 
				if ( !s->fog_dist && !(pass->flags & SHADER_PASS_LIGHTMAP) ) 
					pass->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
				else
					pass->rgbgen.type = RGB_GEN_IDENTITY;
			}
			if ( pass->alphagen.type == ALPHA_GEN_UNKNOWN ) {
				if ( pass->rgbgen.type == RGB_GEN_VERTEX || pass->rgbgen.type == RGB_GEN_EXACT_VERTEX ) {
					pass->alphagen.type = ALPHA_GEN_VERTEX;
				} else {
					pass->alphagen.type = ALPHA_GEN_IDENTITY;
				}
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
			if ( sp->rgbgen.type == RGB_GEN_UNKNOWN ) { 
				sp->rgbgen.type = RGB_GEN_IDENTITY;
			}
			if ( sp->alphagen.type == ALPHA_GEN_UNKNOWN ) {
				if ( sp->rgbgen.type == RGB_GEN_VERTEX || sp->rgbgen.type == RGB_GEN_EXACT_VERTEX ) {
					sp->alphagen.type = ALPHA_GEN_VERTEX;
				} else {
					sp->alphagen.type = ALPHA_GEN_IDENTITY;
				}
			}
		}

		if ( !s->sort ) {
			if ( pass->flags & SHADER_PASS_ALPHAFUNC )
				s->sort = SHADER_SORT_OPAQUE + 1;
		}

		if ( !(pass->flags & SHADER_PASS_DEPTHWRITE) && !(s->flags & SHADER_SKY) )
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

	shader = r_shaders;
	for (i = 0; i < MAX_SHADERS; i++, shader++)
	{
		if ( shader->registration_sequence != registration_sequence ) {
			Shader_FreeShader ( shader );
			memset ( shader, 0, sizeof(shader_t) );
			continue;
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
					Shader_Free ( pass->cin );
				}

				continue;
			}

			GL_RunCinematic ( pass->cin );
		}
	}
}

shader_t *R_Shader_Load ( char *name, int type, qboolean forceDefault )
{
	int i, f = -1;
	unsigned int length = 0;
	char shortname[MAX_QPATH], path[MAX_QPATH];
	char *buf = NULL;
	shader_t *s;
	shadercache_t *cache;
	shaderpass_t *pass;

	COM_StripExtension ( name, shortname );

	// test if already loaded
	for (i = 0; i < MAX_SHADERS; i++)
	{
		if (!r_shaders[i].name[0])
		{
			if ( f == -1 )	// free shader
				f = i;
			continue;
		}

		if ( !Q_stricmp (shortname, r_shaders[i].name) ) {
			return &r_shaders[i];
		}
	}

	if ( f == -1 )
	{
		Com_Error (ERR_FATAL, "R_Shader_Load: Shader limit exceeded.");
		return NULL;
	}

	s = &r_shaders[f];
	memset ( s, 0, sizeof( shader_t ) );

	Com_sprintf ( s->name, MAX_QPATH, shortname );

	Shader_GetCache ( shortname, &cache );

	if ( cache ) {
		Com_sprintf ( path, sizeof(path), "scripts/%s", cache->path );
		length = FS_LoadFile ( path, (void **)&buf );
	}

	// the shader is in the shader scripts
	if ( !forceDefault && cache && buf && (cache->offset < length) )
	{
		char *ptr, *token;

		// set defaults
		s->flags = SHADER_CULL_FRONT;

		ptr = buf + cache->offset;
		token = COM_ParseExt (&ptr, qtrue);

		if ( !ptr || token[0] != '{' ) {
			goto create_default;
		}

		while ( ptr )
		{
			token = COM_ParseExt (&ptr, qtrue);

			if ( !token[0] ) {
				continue;
			} else if ( token[0] == '}' ) {
				break;
			} else if ( token[0] == '{' ) {
				Shader_Readpass ( s, &ptr );
			} else if ( Shader_Parsetok (s, NULL, shaderkeys, token, &ptr ) ) {
				break;
			}
		}

		Shader_Finish ( s );
		FS_FreeFile ( buf );
	}
	else		// make a default shader
	{
		switch (type)
		{
			case SHADER_BSP_VERTEX:
				pass = &s->passes[0];
				pass->tcgen = TC_GEN_BASE;
				pass->anim_frames[0] = GL_FindImage (shortname, 0);
				pass->depthfunc = GL_LEQUAL;
				pass->flags = SHADER_PASS_DEPTHWRITE;
				pass->rgbgen.type = RGB_GEN_VERTEX;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
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
				break;

			case SHADER_BSP_FLARE:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_BLEND | SHADER_PASS_NOCOLORARRAY;
				pass->blendsrc = GL_ONE;
				pass->blenddst = GL_ONE;
				pass->blendmode = GL_MODULATE;
				pass->anim_frames[0] = GL_FindImage (shortname, 0);
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen.type = RGB_GEN_VERTEX;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->numtcmods = 0;
				pass->tcgen = TC_GEN_BASE;
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
				break;

			case SHADER_MD3:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_DEPTHWRITE;
				pass->anim_frames[0] = GL_FindImage (shortname, 0);
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen.type = RGB_GEN_LIGHTING_DIFFUSE;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->numtcmods = 0;
				pass->tcgen = TC_GEN_BASE;
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
				break;

			case SHADER_2D:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_BLEND | SHADER_PASS_NOCOLORARRAY;
				pass->blendsrc = GL_SRC_ALPHA;
				pass->blenddst = GL_ONE_MINUS_SRC_ALPHA;
				pass->blendmode = GL_MODULATE;
				pass->anim_frames[0] = GL_FindImage (shortname, IT_NOPICMIP|IT_NOMIPMAP);
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen.type = RGB_GEN_VERTEX;
				pass->alphagen.type = ALPHA_GEN_VERTEX;
				pass->numtcmods = 0;
				pass->tcgen = TC_GEN_BASE;
				pass->numMergedPasses = 1;
				pass->flush = R_RenderMeshGeneric;

				s->numpasses = 1;
				s->numdeforms = 0;
				s->flags = SHADER_NOPICMIP|SHADER_NOMIPMAPS;
				s->features = MF_STCOORDS|MF_COLORS;
				s->sort = SHADER_SORT_ADDITIVE;
				break;

			case SHADER_FARBOX:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_NOCOLORARRAY;
				pass->blendsrc = GL_ONE;
				pass->blenddst = GL_ZERO;
				pass->blendmode = GL_REPLACE;
				pass->anim_frames[0] = GL_FindImage (shortname, IT_NOMIPMAP|IT_CLAMP);
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen.type = RGB_GEN_IDENTITY;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->numtcmods = 0;
				pass->tcgen = TC_GEN_BASE;
				pass->numMergedPasses = 1;
				pass->flush = R_RenderMeshGeneric;

				s->numpasses = 1;
				s->numdeforms = 0;
				s->flags = 0;
				s->features = MF_STCOORDS;
				s->sort = SHADER_SORT_SKY;
				break;

			case SHADER_NEARBOX:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_ALPHAFUNC|SHADER_PASS_NOCOLORARRAY;
				pass->blendsrc = GL_ONE;
				pass->blenddst = GL_ZERO;
				pass->blendmode = GL_MODULATE;
				pass->alphafunc = SHADER_ALPHA_LT128;
				pass->anim_frames[0] = GL_FindImage (shortname, IT_NOMIPMAP|IT_CLAMP);
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen.type = RGB_GEN_IDENTITY;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->numtcmods = 0;
				pass->tcgen = TC_GEN_BASE;
				pass->numMergedPasses = 1;
				pass->flush = R_RenderMeshGeneric;

				if ( !pass->anim_frames[0] ) {
					Com_DPrintf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", s->name, shortname );
					pass->anim_frames[0] = r_notexture;
				}

				s->numpasses = 1;
				s->numdeforms = 0;
				s->flags = 0;
				s->features = MF_STCOORDS;
				s->sort = SHADER_SORT_SKY;
				break;

			case SHADER_BSP:
			default:
create_default:
				pass = &s->passes[0];
				pass->flags = SHADER_PASS_LIGHTMAP | SHADER_PASS_DEPTHWRITE | SHADER_PASS_NOCOLORARRAY;
				pass->tcgen = TC_GEN_LIGHTMAP;
				pass->anim_frames[0] = NULL;
				pass->depthfunc = GL_LEQUAL;
				pass->blendmode = GL_REPLACE;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->rgbgen.type = RGB_GEN_IDENTITY;
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
				pass->tcgen = TC_GEN_BASE;
				pass->anim_frames[0] = GL_FindImage (shortname, 0);
				pass->blendsrc = GL_ZERO;
				pass->blenddst = GL_SRC_COLOR;
				pass->blendmode = GL_MODULATE;
				pass->depthfunc = GL_LEQUAL;
				pass->rgbgen.type = RGB_GEN_IDENTITY;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;

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
				break;
		}
	}

	return s;
}

shader_t *R_RegisterPic (char *name) 
{
	shader_t *shader;
	
	shader = R_Shader_Load ( name, SHADER_2D, qfalse );
	shader->registration_sequence = registration_sequence;

	return shader;
}

shader_t *R_RegisterShader (char *name)
{
	shader_t *shader;

	shader = R_Shader_Load ( name, SHADER_BSP, qfalse );
	shader->registration_sequence = registration_sequence;

	return shader;
}

shader_t *R_RegisterSkin (char *name)
{
	shader_t *shader;

	shader = R_Shader_Load ( name, SHADER_MD3, qfalse );
	shader->registration_sequence = registration_sequence;

	return shader;
}
