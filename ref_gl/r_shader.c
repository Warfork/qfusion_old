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

#define SHADERS_HASH_SIZE		128
#define SHADERCACHE_HASH_SIZE	128

typedef struct
{
    char			*keyword;
    void			(*func)( shader_t *shader, shaderpass_t *pass, char **ptr );
} shaderkey_t;

typedef struct shadercache_s {
	char			*name;
	char			*path;
	unsigned int	offset;
	struct shadercache_s *hash_next;
} shadercache_t;

shader_t				r_shaders[MAX_SHADERS];
int						r_numShaders;
skydome_t				*r_skydomes[MAX_SHADERS];

static char				*shaderPaths;
static shader_t			*shaders_hash[SHADERS_HASH_SIZE];
static shadercache_t	*shadercache_hash[SHADERCACHE_HASH_SIZE];

static shader_t			*r_cinematicShaders[MAX_SHADERS];
static int				r_numCinematicShaders;

static deformv_t		r_currentDeforms[MAX_SHADER_DEFORMVS];
static shaderpass_t		r_currentPasses[MAX_SHADER_PASSES];
static tcmod_t			r_currentTcmods[MAX_SHADER_PASSES][MAX_SHADER_TCMODS];
static vec4_t			r_currentTcGen[MAX_SHADER_PASSES][2];

mempool_t				*r_shadersmempool;

static qboolean Shader_Parsetok( shader_t *shader, shaderpass_t *pass, shaderkey_t *keys, char *token, char **ptr );
static void Shader_ParseFunc( char **args, shaderfunc_t *func );
static void Shader_MakeCache( qboolean silent, char *name );
static unsigned int Shader_GetCache( char *name, shadercache_t **cache );

//===========================================================================

static char *Shader_ParseString( char **ptr )
{
	char *token;

	if( !ptr || !(*ptr) ) 
		return "";
	if( !**ptr || **ptr == '}' )
		return "";

	token = COM_ParseExt( ptr, qfalse );

	return Q_strlwr( token );
}

static float Shader_ParseFloat( char **ptr )
{
	if( !ptr || !(*ptr) )
		return 0;
	if( !**ptr || **ptr == '}' )
		return 0;

	return atof( COM_ParseExt( ptr, qfalse ) );
}

static void Shader_ParseVector( char **ptr, float *v, unsigned int size )
{
	int i;
	char *token;
	qboolean bracket;

	if( !size ) {
		return;
	} else if( size == 1 ) {
		Shader_ParseFloat( ptr );
		return;
	}

	token = Shader_ParseString( ptr );
	if( !Q_stricmp( token, "(" ) ) {
		bracket = qtrue;
		token = Shader_ParseString( ptr );
	} else if( token[0] == '(' ) {
		bracket = qtrue;
		token = &token[1];
	} else {
		bracket = qfalse;
	}

	v[0] = atof ( token );
	for( i = 1; i < size-1; i++ )
		v[i] = Shader_ParseFloat( ptr );

	token = Shader_ParseString( ptr );
	if( !token[0] ) {
		v[i] = 0;
	} else if( token[strlen(token)-1] == ')' ) {
		token[strlen(token)-1] = 0;
		v[i] = atof ( token );
	} else {
		v[i] = atof( token );
		if( bracket )
			Shader_ParseString( ptr );
	}
}

static void Shader_SkipLine( char **ptr )
{
	while( ptr ) {
		char *token = COM_ParseExt( ptr, qfalse );
		if( !token[0] )
			return;
	}
}

static void Shader_SkipBlock( char **ptr )
{
	char *tok;
    int brace_count;

    // Opening brace
	tok = COM_ParseExt( ptr, qtrue );
	if( tok[0] != '{'  )
		return;

	for( brace_count = 1; brace_count > 0; ) {
		tok = COM_ParseExt( ptr, qtrue );
		if( !tok[0] )
			return;
		else if( tok[0] == '{' )
			brace_count++;
		else if( tok[0] == '}' )
			brace_count--;
	}
}

#define MAX_CONDITIONS			8
typedef enum { COP_LS, COP_LE, COP_EQ, COP_GR, COP_GE, COP_NE } conOp_t;
typedef enum { COP2_AND, COP2_OR } conOp2_t;
typedef struct { int *operand; conOp_t op; qboolean negative; int val; conOp2_t logic; } shaderCon_t;

char *conOpStrings[] = { "<", "<=", "==", ">", ">=", "!=", NULL };
char *conOpStrings2[] = { "&&", "||", NULL };

static qboolean Shader_ParseConditions( char **ptr, shader_t *shader )
{
	int i;
	char *tok;
	int	numConditions;
	shaderCon_t conditions[MAX_CONDITIONS];
	qboolean result, val, skip, expectingOperator;

	numConditions = 0;
	memset( conditions, 0, sizeof( conditions ) );

	skip = qfalse;
	expectingOperator = qfalse;
	while( 1 ) {
		tok = Shader_ParseString( ptr );
		if( !tok[0] ) {
			if( expectingOperator )
				numConditions++;
			break;
		}
		if( skip )
			continue;

		for( i = 0; conOpStrings[i]; i++ ) {
			if( !strcmp( tok, conOpStrings[i] ) )
				break;
		}

		if( conOpStrings[i] ) {
			if( !expectingOperator ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: Bad syntax in condition (shader %s)\n", shader->name );
				skip = qtrue;
			} else {
				conditions[numConditions].op = i;
					expectingOperator = qfalse;
			}
			continue;
		}

		for( i = 0; conOpStrings2[i]; i++ ) {
			if( !strcmp( tok, conOpStrings2[i] ) )
				break;
		}

		if( conOpStrings2[i] ) {
			if( !expectingOperator ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: Bad syntax in condition (shader %s)\n", shader->name );
				skip = qtrue;
			} else {
				conditions[numConditions++].logic = i;
				if( numConditions == MAX_CONDITIONS )
					skip = qtrue;
				else
					expectingOperator = qfalse;
			}
			continue;
		}

		if( expectingOperator ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: Bad syntax in condition (shader %s)\n", shader->name );
			skip = qtrue;
			continue;
		}

		if( !strcmp( tok, "!" ) ) {
			conditions[numConditions].negative = !conditions[numConditions].negative;
			continue;
		}

		if( !conditions[numConditions].operand ) {
			if( !Q_stricmp( tok, "maxTextureSize" ) ) {
				conditions[numConditions].operand = &glConfig.maxTextureSize;
			} else if( !Q_stricmp( tok, "maxTextureCubemapSize" ) ) {
				conditions[numConditions].operand = &glConfig.maxTextureCubemapSize;
			} else if( !Q_stricmp( tok, "maxTextureUnits" ) ) {
				conditions[numConditions].operand = &glConfig.maxTextureUnits;
			} else if( !Q_stricmp( tok, "textureCubeMap" ) ) {
				conditions[numConditions].operand = ( int * )&glConfig.textureCubeMap;
			} else if( !Q_stricmp( tok, "textureEnvCombine" ) ) {
				conditions[numConditions].operand = ( int * )&glConfig.textureEnvCombine;
			} else if( !Q_stricmp( tok, "textureEnvDot3" ) ) {
				conditions[numConditions].operand = ( int * )&glConfig.GLSL;
			} else if( !Q_stricmp( tok, "GLSL" ) ) {
				conditions[numConditions].operand = ( int * )&glConfig.GLSL;
			} else {
				Com_Printf( S_COLOR_YELLOW "WARNING: Unknown expression '%s' in shader %s\n", tok, shader->name );
				skip = qtrue;
			}
			if( !skip ) {
				conditions[numConditions].op = COP_NE;
				expectingOperator = qtrue;
			}
			continue;
		}

		if( !Q_stricmp( tok, "false" ) )
			conditions[numConditions].val = 0;
		else if( !Q_stricmp( tok, "true" ) )
			conditions[numConditions].val = 1;
		else
			conditions[numConditions].val = atoi( tok );
		expectingOperator = qtrue;
	}

	if( skip )
		return qfalse;

	if( !conditions[0].operand ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Empty 'if' statement in shader %s\n", shader->name );
		return qfalse;
	}

	for( i = 0; i < numConditions; i++ ) {
		switch( conditions[i].op ) {
			case COP_LS:
				val = ( *conditions[i].operand < conditions[i].val );
				break;
			case COP_LE:
				val = ( *conditions[i].operand <= conditions[i].val );
				break;
			case COP_EQ:
				val = ( *conditions[i].operand == conditions[i].val );
				break;
			case COP_GR:
				val = ( *conditions[i].operand > conditions[i].val );
				break;
			case COP_GE:
				val = ( *conditions[i].operand >= conditions[i].val );
				break;
			case COP_NE:
				val = ( *conditions[i].operand != conditions[i].val );
				break;
			default:
				break;
		}

		if( conditions[i].negative )
			val = !val;
		if( i ) {
			switch( conditions[i-1].logic ) {
				case COP2_AND:
					result = result && val;
					break;
				case COP2_OR:
					result = result || val;
					break;
			}
		} else {
			result = val;
		}
	}

	return result;
}

static qboolean Shader_SkipConditionBlock( char **ptr )
{
	char *tok;
    int condition_count;

	for( condition_count = 1; condition_count > 0; ) {
		tok = COM_ParseExt( ptr, qtrue );
		if( !tok[0] )
			return qfalse;
		else if( !Q_stricmp( tok, "if" ) )
			condition_count++;
		else if( !Q_stricmp( tok, "endif" ) )
			condition_count--;
// Vic: commented out for now
//		else if( !Q_stricmp( tok, "else" ) && (condition_count == 1) )
//			return qtrue;
	}

	return qtrue;
}

//===========================================================================

static void Shader_ParseSkySides( char **ptr, shader_t **shaders, qboolean farbox )
{
	int i, j;
	char *token;
	image_t *image;
	char path[MAX_QPATH];
	qboolean noskybox = qfalse;

	token = Shader_ParseString( ptr );
	if( token[0] == '-' ) { 
		noskybox = qtrue;
	} else {
		int flags;
		struct cubemapSufAndFlip { 
			char *suf; int flags; 
		} cubemapSides[2][6] = {
			{
				{ "px", IT_FLIPDIAGONAL },
				{ "py", IT_FLIPY },
				{ "nx", IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL },
				{ "ny", IT_FLIPX },
				{ "pz", IT_FLIPDIAGONAL },
				{ "nz", IT_FLIPDIAGONAL }
			},
			{
				{ "rt", 0 },
				{ "bk", 0 },
				{ "lf", 0 },
				{ "ft", 0 },
				{ "up", 0 },
				{ "dn", 0 }
			}
		};

		for( i = 0; i < 2; i++ ) {
			memset( shaders, 0, sizeof( shader_t * ) * 6 );

			for( j = 0; j < 6; j++ ) {
				Q_snprintfz( path, sizeof( path ), "%s_%s", token, cubemapSides[i][j].suf );
				flags = IT_NOMIPMAP|IT_CLAMP|cubemapSides[i][j].flags;
				image = R_FindImage( path, flags, 0 );

				if( image )
					shaders[j] = R_LoadShader( path, (farbox ? SHADER_FARBOX : SHADER_NEARBOX), qtrue, flags, SHADER_INVALID );
				else
					break;
			}
			if( j == 6 )
				break;
		}
		if( i == 2 )
			noskybox = qtrue;
	}

	if( noskybox )
		memset( shaders, 0, sizeof( shader_t * ) * 6 );
}

static void Shader_ParseFunc( char **ptr, shaderfunc_t *func )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !Q_stricmp( token, "sin" ) )
	    func->type = SHADER_FUNC_SIN;
	else if ( !Q_stricmp(token, "triangle" ) )
	    func->type = SHADER_FUNC_TRIANGLE;
	else if ( !Q_stricmp(token, "square" ) )
	    func->type = SHADER_FUNC_SQUARE;
	else if ( !Q_stricmp(token, "sawtooth" ) )
	    func->type = SHADER_FUNC_SAWTOOTH;
	else if( !Q_stricmp( token, "inversesawtooth" ) )
	    func->type = SHADER_FUNC_INVERSESAWTOOTH;
	else if( !Q_stricmp( token, "noise" ) )
	    func->type = SHADER_FUNC_NOISE;

	func->args[0] = Shader_ParseFloat( ptr );
	func->args[1] = Shader_ParseFloat( ptr );
	func->args[2] = Shader_ParseFloat( ptr );
	func->args[3] = Shader_ParseFloat( ptr );
}

//===========================================================================

static int Shader_SetImageFlags( shader_t *shader )
{
	int flags = 0;

	if( shader->flags & SHADER_SKY )
		flags |= IT_SKY;
	if( shader->flags & SHADER_NOMIPMAPS )
		flags |= IT_NOMIPMAP;
	if( shader->flags & SHADER_NOPICMIP )
		flags |= IT_NOPICMIP;
	if( shader->flags & SHADER_NOCOMPRESS )
		flags |= IT_NOCOMPRESS;

	return flags;
}

static image_t *Shader_FindImage( shader_t *shader, char *name, int flags, float bumpScale )
{
	image_t *image;

	if( !Q_stricmp( name, "$whiteimage" ) || !Q_stricmp( name, "*white" ) )
		return r_whitetexture;
	if( !Q_stricmp( name, "$blackimage" ) || !Q_stricmp( name, "*black" ) )
		return r_blacktexture;
	if( !Q_stricmp( name, "$particleimage" ) || !Q_stricmp( name, "*particle" ) )
		return r_particletexture;
	if( !Q_strnicmp( name, "*lm", 3 ) ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: shader %s has a stage with explicit lightmap image.\n", shader->name );
		return r_whitetexture;
	}

	image = R_FindImage( name, flags, bumpScale );
	if( !image ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: shader %s has a stage with no image: %s.\n", shader->name, name );
		return r_notexture;
	}

	return image;
}

/****************** shader keyword functions ************************/

static void Shader_Cull( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	shader->flags &= ~(SHADER_CULL_FRONT|SHADER_CULL_BACK);

	token = Shader_ParseString ( ptr );
	if( !Q_stricmp( token, "disable" ) || !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) ) {
	} else if( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) ) {
		shader->flags |= SHADER_CULL_BACK;
	} else {
		shader->flags |= SHADER_CULL_FRONT;
	}
}

static void Shader_NoMipMaps( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	shader->flags |= (SHADER_NOMIPMAPS|SHADER_NOPICMIP);
}

static void Shader_NoPicMip( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	shader->flags |= SHADER_NOPICMIP;
}

static void Shader_NoCompress( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	shader->flags |= SHADER_NOCOMPRESS;
}

static void Shader_DeformVertexes ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;
	deformv_t *deformv;

	if( shader->numdeforms == MAX_SHADER_DEFORMVS ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: shader %s has too many deforms\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	deformv = &r_currentDeforms[shader->numdeforms];

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp( token, "wave" ) ) {
		deformv->type = DEFORMV_WAVE;
		deformv->args[0] = Shader_ParseFloat( ptr );
		if( deformv->args[0] )
			deformv->args[0] = 1.0f / deformv->args[0];
		else
			deformv->args[0] = 100.0f;
		Shader_ParseFunc( ptr, &deformv->func );
	} else if( !Q_stricmp( token, "normal" ) ) {
		shader->flags |= SHADER_DEFORMV_NORMAL;
		deformv->type = DEFORMV_NORMAL;
		deformv->args[0] = Shader_ParseFloat( ptr );
		deformv->args[1] = Shader_ParseFloat( ptr );
	} else if( !Q_stricmp( token, "bulge" ) ) {
		deformv->type = DEFORMV_BULGE;
		Shader_ParseVector ( ptr, deformv->args, 3 );
	} else if( !Q_stricmp( token, "move" ) ) {
		deformv->type = DEFORMV_MOVE;
		Shader_ParseVector( ptr, deformv->args, 3 );
		Shader_ParseFunc( ptr, &deformv->func );
	} else if( !Q_stricmp( token, "autosprite" ) ) {
		deformv->type = DEFORMV_AUTOSPRITE;
		shader->flags |= SHADER_AUTOSPRITE;
	} else if( !Q_stricmp( token, "autosprite2" ) ) {
		deformv->type = DEFORMV_AUTOSPRITE2;
		shader->flags |= SHADER_AUTOSPRITE;
	} else if( !Q_stricmp( token, "projectionShadow" ) ) {
		deformv->type = DEFORMV_PROJECTION_SHADOW;
	} else if( !Q_stricmp (token, "autoparticle" ) ) {
		deformv->type = DEFORMV_AUTOPARTICLE;
	} else {
		Shader_SkipLine( ptr );
		return;
	}

	shader->numdeforms++;
}

static void Shader_SkyParms( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	int			shaderNum;
	float		skyheight;
	shader_t	*farboxShaders[6];
	shader_t	*nearboxShaders[6];

	shaderNum = shader - r_shaders;
	if( r_skydomes[shaderNum] )
		R_FreeSkydome( r_skydomes[shaderNum] );

	Shader_ParseSkySides( ptr, farboxShaders, qtrue );

	skyheight = Shader_ParseFloat( ptr );
	if( !skyheight )
		skyheight = 512.0f;

//	if( skyheight*sqrt(3) > r_farclip_min )
//		r_farclip_min = skyheight*sqrt(3);
	if( skyheight*2 > r_farclip_min )
		r_farclip_min = skyheight*2;

	Shader_ParseSkySides( ptr, nearboxShaders, qfalse );

	r_skydomes[shaderNum] = R_CreateSkydome( skyheight, farboxShaders, nearboxShaders );
	shader->flags |= SHADER_SKY;
	shader->sort = SHADER_SORT_SKY;
}

static void Shader_FogParms( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	float div;
	vec3_t color, fcolor;

	if ( !r_ignorehwgamma->integer )
		div = 1.0f / pow(2, max(0, floor(r_overbrightbits->value)));
	else
		div = 1.0f;

	Shader_ParseVector( ptr, color, 3 );
	ColorNormalize( color, fcolor );
	VectorScale( fcolor, div, fcolor );

	shader->fog_color[0] = R_FloatToByte( fcolor[0] );
	shader->fog_color[1] = R_FloatToByte( fcolor[1] );
	shader->fog_color[2] = R_FloatToByte( fcolor[2] );
	shader->fog_color[3] = 255;	
	shader->fog_dist = ( double )Shader_ParseFloat( ptr );

	if ( shader->fog_dist <= 0.1 )
		shader->fog_dist = 128.0;
	shader->fog_dist = 1.0 / shader->fog_dist;
}

static void Shader_Sort( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString ( ptr );
	if( !Q_stricmp( token, "portal" ) ) {
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
		shader->sort = atoi( token );
		if( shader->sort > SHADER_SORT_NEAREST )
			shader->sort = SHADER_SORT_NEAREST;
	}
}

static void Shader_Portal( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	shader->sort = SHADER_SORT_PORTAL;
}

static void Shader_PolygonOffset( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	shader->flags |= SHADER_POLYGONOFFSET;
}

static void Shader_EntityMergable( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	shader->flags |= SHADER_ENTITY_MERGABLE;
}

static void Shader_If( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	if( !Shader_ParseConditions( ptr, shader ) ) {
		if( !Shader_SkipConditionBlock( ptr ) )
			Com_Printf( S_COLOR_YELLOW "WARNING: Mismatched if/endif pair in shader %s\n", shader->name );
	}
}

static void Shader_Endif( shader_t *shader, shaderpass_t *pass, char **ptr ) {
}

static void Shader_NoModulativeDlights( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	shader->flags |= SHADER_NO_MODULATIVE_DLIGHTS;
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
	{"if",				Shader_If },
	{"endif",			Shader_Endif },
	{"nomodulativedlights", Shader_NoModulativeDlights },
	{"nocompress",		Shader_NoCompress },
    {NULL,				NULL}
};

// ===============================================================

static qboolean Shaderpass_LoadMaterial( image_t **normalmap, image_t **glossmap, char *name, int addFlags, float bumpScale )
{
	image_t *images[2];

	images[0] = images[1] = NULL;

	// load normalmap image
	images[0] = R_FindImage( va( "%s_bump.jpg", name ), addFlags|IT_HEIGHTMAP, bumpScale );
	if( !images[0] ) {
		images[0] = R_FindImage( va( "%s_norm.jpg", name ), addFlags & ~IT_HEIGHTMAP, 0 );

		if( !images[0] ) {
			if( !r_lighting_diffuse2heightmap->integer )
				return qfalse;
			images[0] = R_FindImage( name, addFlags|IT_HEIGHTMAP, 2 );
			if( !images[0] )
				return qfalse;
		}
	}

	// load glossmap image
	if( r_lighting_specular->integer )
		images[1] = R_FindImage( va( "%s_gloss.jpg", name ), addFlags & ~IT_HEIGHTMAP, 0 );

	*normalmap = images[0];
	*glossmap = images[1];

	return qtrue;
}

static void Shaderpass_MapExt( shader_t *shader, shaderpass_t *pass, int addFlags, char **ptr )
{
	int flags;
	char *token;

	token = Shader_ParseString( ptr );
	if( !Q_stricmp( token, "$lightmap" ) ) {
		pass->tcgen = TC_GEN_LIGHTMAP;
		pass->flags |= SHADERPASS_LIGHTMAP;
		pass->flags &= ~(SHADERPASS_DLIGHT | SHADERPASS_VIDEOMAP);
		pass->anim_fps = 0;
		pass->anim_frames[0] = NULL;
	} else if( !Q_stricmp( token, "$dlight" ) ) {
		pass->tcgen = TC_GEN_BASE;
		pass->flags |= SHADERPASS_DLIGHT;
		pass->flags &= ~(SHADERPASS_LIGHTMAP | SHADERPASS_VIDEOMAP);
		pass->anim_fps = 0;
		pass->anim_frames[0] = NULL;
	} else {
		if( !Q_stricmp( token, "$rgb" ) ) {
			addFlags |= IT_NOALPHA;
			token = Shader_ParseString( ptr );
		} else if( !Q_stricmp( token, "$alpha" ) ) {
			addFlags |= IT_NORGB;
			token = Shader_ParseString( ptr );
		}

		flags = Shader_SetImageFlags( shader ) | addFlags;
		pass->tcgen = TC_GEN_BASE;
		pass->flags &= ~(SHADERPASS_LIGHTMAP | SHADERPASS_DLIGHT | SHADERPASS_VIDEOMAP);
		pass->anim_fps = 0;
		pass->anim_frames[0] = Shader_FindImage( shader, token, flags, 0 );
    }
}

static void Shaderpass_AnimMapExt( shader_t *shader, shaderpass_t *pass, int addFlags, char **ptr )
{
    int flags;
	char *token;

	flags = Shader_SetImageFlags( shader ) | addFlags;

	pass->tcgen = TC_GEN_BASE;
	pass->flags &= ~(SHADERPASS_LIGHTMAP | SHADERPASS_DLIGHT | SHADERPASS_VIDEOMAP);
    pass->anim_fps = Shader_ParseFloat( ptr );
	pass->anim_numframes = 0;

    for( ; ; ) {
		token = Shader_ParseString( ptr );
		if( !token[0] )
			break;
		if( pass->anim_numframes < MAX_SHADER_ANIM_FRAMES )
			pass->anim_frames[pass->anim_numframes++] = Shader_FindImage( shader, token, flags, 0 );
	}

	if( pass->anim_numframes == 0 )
		pass->anim_fps = 0;
}

static void Shaderpass_CubeMapExt( shader_t *shader, shaderpass_t *pass, int addFlags, int tcgen, char **ptr )
{
	int flags;
	char *token;

	token = Shader_ParseString( ptr );
	flags = Shader_SetImageFlags( shader ) | addFlags;
	pass->anim_fps = 0;
	pass->flags &= ~(SHADERPASS_LIGHTMAP | SHADERPASS_DLIGHT | SHADERPASS_VIDEOMAP);

	if( !glConfig.textureCubeMap ) { 
		Com_DPrintf( S_COLOR_YELLOW "Shader %s has an unsupported cubemap stage: %s.\n", shader->name );
		pass->anim_frames[0] = r_notexture;
		pass->tcgen = TC_GEN_BASE;
		return;
	}

	pass->anim_frames[0] = R_FindCubemapImage( token, flags );
	if( pass->anim_frames[0] ) {
		pass->tcgen = tcgen;
	} else {
		Com_DPrintf( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", shader->name, token );
		pass->anim_frames[0] = r_notexture;
		pass->tcgen = TC_GEN_BASE;
	}
}

static void Shaderpass_Map( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_MapExt( shader, pass, 0, ptr );
}

static void Shaderpass_ClampMap( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_MapExt( shader, pass, IT_CLAMP, ptr );
}

static void Shaderpass_AnimMap( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_AnimMapExt( shader, pass, 0, ptr );
}

static void Shaderpass_AnimClampMap( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_AnimMapExt( shader, pass, IT_CLAMP, ptr );
}

static void Shaderpass_CubeMap( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_CubeMapExt( shader, pass, IT_CLAMP, TC_GEN_REFLECTION, ptr );
}

static void Shaderpass_ShadeCubeMap( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	Shaderpass_CubeMapExt( shader, pass, IT_CLAMP, TC_GEN_REFLECTION_CELLSHADE, ptr );
}

static void Shaderpass_VideoMap( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char		*token;
	char		name[MAX_OSPATH];

	token = Shader_ParseString( ptr );
	COM_StripExtension( token, name );

	if( pass->cin )
		Shader_Free( pass->cin );

	pass->cin = (cinematics_t *)Shader_Malloc( sizeof(cinematics_t) );
	pass->cin->frame = -1;
	Q_snprintfz( pass->cin->name, sizeof(pass->cin->name), "video/%s.RoQ", name );

	pass->tcgen = TC_GEN_BASE;
	pass->anim_fps = 0;
	pass->flags |= SHADERPASS_VIDEOMAP;
	pass->flags &= ~(SHADERPASS_LIGHTMAP | SHADERPASS_DLIGHT);
	shader->flags |= SHADER_VIDEOMAP;
	r_cinematicShaders[r_numCinematicShaders++] = shader;
}

static void Shaderpass_NormalMap( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	int flags;
	char *token;
	float bumpScale = 0;

	if( !glConfig.GLSL ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: shader %s has a normalmap stage, while GLSL is not supported\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	flags = Shader_SetImageFlags( shader );
	token = Shader_ParseString( ptr );

	if( !Q_stricmp( token, "$heightmap" ) ) {
		flags |= IT_HEIGHTMAP;
		bumpScale = Shader_ParseFloat( ptr );
		token = Shader_ParseString( ptr );
	}

	pass->tcgen = TC_GEN_BASE;
	pass->flags &= ~(SHADERPASS_LIGHTMAP | SHADERPASS_DLIGHT);
	pass->anim_frames[1] = R_FindImage( token, flags, bumpScale );
	if( pass->anim_frames[1] )
		pass->program = DEFAULT_GLSL_PROGRAM;

	token = Shader_ParseString( ptr );
	if( !Q_stricmp( token, "$noimage" ) )
		pass->anim_frames[0] = r_whitetexture;
	else
		pass->anim_frames[0] = Shader_FindImage( shader, token, Shader_SetImageFlags( shader ), 0 );
}

static void Shaderpass_Material( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	int flags;
	char *token;
	char base[MAX_QPATH], norm[MAX_QPATH], gloss[MAX_QPATH];
	float bumpScale = 0;

	if( !glConfig.GLSL ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: shader %s has a normalmap stage, while GLSL is not supported\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	flags = Shader_SetImageFlags( shader );
	token = Shader_ParseString( ptr );
	Q_strncpyz( base, token, sizeof( base ) );
	pass->anim_frames[0] = Shader_FindImage( shader, base, flags, 0 );
	pass->tcgen = TC_GEN_BASE;
	pass->flags &= ~(SHADERPASS_LIGHTMAP | SHADERPASS_DLIGHT);

	norm[0] = gloss[0] = '\0';
	while( 1 ) {
		token = Shader_ParseString( ptr );
		if( !*token )
			break;

		if( Q_isdigit( token ) ) {
			flags |= IT_HEIGHTMAP;
			bumpScale = atoi( token );
		} else if( norm[0] == '\0' ) {
			Q_strncpyz( norm, token, sizeof( norm ) );
		} else {
			Q_strncpyz( gloss, token, sizeof( gloss ) );
		}
	}

	if( !norm[0] ) {
		if( Shaderpass_LoadMaterial( &pass->anim_frames[1], &pass->anim_frames[2], base, flags, bumpScale ) )
			pass->program = DEFAULT_GLSL_PROGRAM;
		else
			Com_DPrintf( S_COLOR_YELLOW "WARNING: failed to load default images for material %s in shader.\n", base, shader->name );
		return;
	}

	pass->program = DEFAULT_GLSL_PROGRAM;

	pass->anim_frames[1] = R_FindImage( norm, flags, bumpScale );
	if( !pass->anim_frames[1] ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: missing normalmap image %s in shader.\n", norm, shader->name );
		pass->anim_frames[1] = r_blanknormalmaptexture;
	}
	if( gloss[0] && r_lighting_specular->integer ) {
		pass->anim_frames[2] = R_FindImage( gloss, flags & ~IT_HEIGHTMAP, 0 );
		if( !pass->anim_frames[2] )
			Com_DPrintf( S_COLOR_YELLOW "WARNING: missing glossmap image %s in shader %s.\n", gloss, shader->name );
	}
}

static void Shaderpass_RGBGen( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char		*token;

	token = Shader_ParseString ( ptr );
	if( !Q_stricmp( token, "identitylighting" ) ) {
		pass->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
	} else if( !Q_stricmp( token, "identity" ) ) {
		pass->rgbgen.type = RGB_GEN_IDENTITY;
	} else if( !Q_stricmp( token, "wave" ) ) {
		pass->rgbgen.type = RGB_GEN_COLORWAVE;
		pass->rgbgen.args[0] = 1.0f;
		pass->rgbgen.args[1] = 1.0f;
		pass->rgbgen.args[2] = 1.0f;
		Shader_ParseFunc( ptr, &pass->rgbgen.func );
	} else if( !Q_stricmp( token, "colorwave" ) ) {
		pass->rgbgen.type = RGB_GEN_COLORWAVE;
		Shader_ParseVector( ptr, pass->rgbgen.args, 3 );
		Shader_ParseFunc( ptr, &pass->rgbgen.func );
	} else if( !Q_stricmp( token, "entity" ) ) {
		pass->rgbgen.type = RGB_GEN_ENTITY;
	} else if( !Q_stricmp( token, "oneMinusEntity" ) ) {
		pass->rgbgen.type = RGB_GEN_ONE_MINUS_ENTITY;
	} else if( !Q_stricmp( token, "vertex" ) ) {
		pass->rgbgen.type = RGB_GEN_VERTEX;
	} else if( !Q_stricmp( token, "oneMinusVertex" ) ) {
		pass->rgbgen.type = RGB_GEN_ONE_MINUS_VERTEX;
	} else if( !Q_stricmp( token, "lightingDiffuse" ) ) {
		pass->rgbgen.type = RGB_GEN_LIGHTING_DIFFUSE;
	} else if( !Q_stricmp( token, "lightingDiffuseOnly" ) ) {
		pass->rgbgen.type = RGB_GEN_LIGHTING_DIFFUSE_ONLY;
	} else if( !Q_stricmp( token, "lightingAmbientOnly" ) ) {
		pass->rgbgen.type = RGB_GEN_LIGHTING_AMBIENT_ONLY;
	} else if( !Q_stricmp( token, "exactVertex" ) ) {
		pass->rgbgen.type = RGB_GEN_EXACT_VERTEX;
	} else if( !Q_stricmp( token, "const" ) || !Q_stricmp( token, "constant" ) ) {
		float div;
		vec3_t color;

		if ( !r_ignorehwgamma->integer )
			div = 1.0f / pow(2, max(0, floor(r_overbrightbits->value)));
		else
			div = 1.0f;

		pass->rgbgen.type = RGB_GEN_CONST;
		Shader_ParseVector( ptr, color, 3 );
		ColorNormalize( color, pass->rgbgen.args );
		VectorScale( pass->rgbgen.args, div, pass->rgbgen.args );
	} else if( !Q_stricmp( token, "custom" ) || !Q_stricmp( token, "teamcolor" ) ) {
		// the "teamcolor" thing comes from warsow
		pass->rgbgen.type = RGB_GEN_CUSTOM;
		pass->rgbgen.args[0] = (int)Shader_ParseFloat( ptr );
		if( pass->rgbgen.args[0] < 0 || pass->rgbgen.args[0] >= NUM_CUSTOMCOLORS )
			pass->rgbgen.args[0] = 0;
	}
}

static void Shaderpass_AlphaGen( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char		*token;

	token = Shader_ParseString( ptr );
	if( !Q_stricmp( token, "portal" ) ) {
		pass->alphagen.type = ALPHA_GEN_PORTAL;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphagen.args[0] )
			pass->alphagen.args[0] = 256;
		pass->alphagen.args[0] = 1.0f / pass->alphagen.args[0];
		shader->flags |= SHADER_AGEN_PORTAL;
	} else if( !Q_stricmp( token, "vertex" ) ) {
		pass->alphagen.type = ALPHA_GEN_VERTEX;
	} else if( !Q_stricmp( token, "oneMinusVertex" ) ) {
		pass->alphagen.type = ALPHA_GEN_ONE_MINUS_VERTEX;
	} else if( !Q_stricmp( token, "entity" ) ) {
		pass->alphagen.type = ALPHA_GEN_ENTITY;
	} else if( !Q_stricmp( token, "wave" ) ) {
		pass->alphagen.type = ALPHA_GEN_WAVE;
		Shader_ParseFunc( ptr, &pass->alphagen.func );
	} else if( !Q_stricmp( token, "lightingSpecular" ) ) {
		pass->alphagen.type = ALPHA_GEN_SPECULAR;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphagen.args[0] )
			pass->alphagen.args[0] = 5.0f;
	} else if( !Q_stricmp( token, "const" ) || !Q_stricmp( token, "constant" ) ) {
		pass->alphagen.type = ALPHA_GEN_CONST;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
	} else if( !Q_stricmp( token, "dot" ) ) {
		pass->alphagen.type = ALPHA_GEN_DOT;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		pass->alphagen.args[1] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphagen.args[1] )
			pass->alphagen.args[1] = 1.0f;
	} else if( !Q_stricmp( token, "oneMinusDot" ) ) {
		pass->alphagen.type = ALPHA_GEN_ONE_MINUS_DOT;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		pass->alphagen.args[1] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphagen.args[1] )
			pass->alphagen.args[1] = 1.0f;
	}
}

static inline int Shaderpass_SrcBlendBits( char *token )
{
	if( !Q_stricmp( token, "gl_zero") )
		return GLSTATE_SRCBLEND_ZERO;
	if( !Q_stricmp( token, "gl_one" ) )
		return GLSTATE_SRCBLEND_ONE;
	if( !Q_stricmp( token, "gl_dst_color" ) )
		return GLSTATE_SRCBLEND_DST_COLOR;
	if( !Q_stricmp( token, "gl_one_minus_dst_color" ) )
		return GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR;
	if( !Q_stricmp( token, "gl_src_alpha" ) )
		return GLSTATE_SRCBLEND_SRC_ALPHA;
	if( !Q_stricmp( token, "gl_one_minus_src_alpha" ) )
		return GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	if( !Q_stricmp( token, "gl_dst_alpha" ) )
		return GLSTATE_SRCBLEND_DST_ALPHA;
	if( !Q_stricmp( token, "gl_one_minus_dst_alpha" ) )
		return GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA;
	return GLSTATE_SRCBLEND_ONE;
}

static inline int Shaderpass_DstBlendBits( char *token )
{
	if( !Q_stricmp( token, "gl_zero") )
		return GLSTATE_DSTBLEND_ZERO;
	if( !Q_stricmp( token, "gl_one" ) )
		return GLSTATE_DSTBLEND_ONE;
	if( !Q_stricmp( token, "gl_src_color" ) )
		return GLSTATE_DSTBLEND_SRC_COLOR;
	if( !Q_stricmp( token, "gl_one_minus_src_color" ) )
		return GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR;
	if( !Q_stricmp( token, "gl_src_alpha" ) )
		return GLSTATE_DSTBLEND_SRC_ALPHA;
	if( !Q_stricmp( token, "gl_one_minus_src_alpha" ) )
		return GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	if( !Q_stricmp( token, "gl_dst_alpha" ) )
		return GLSTATE_DSTBLEND_DST_ALPHA;
	if( !Q_stricmp( token, "gl_one_minus_dst_alpha" ) )
		return GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA;
	return GLSTATE_DSTBLEND_ONE;
}

static void Shaderpass_BlendFunc( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char		*token;

	token = Shader_ParseString( ptr );
	if( !Q_stricmp( token, "blend" ) ) {
		pass->flags |= GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	} else if( !Q_stricmp( token, "filter" ) ) {
		pass->flags |= GLSTATE_SRCBLEND_DST_COLOR|GLSTATE_DSTBLEND_ZERO;
	} else if( !Q_stricmp( token, "add" ) ) {
		pass->flags |= GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
	} else {
		pass->flags |= Shaderpass_SrcBlendBits( token );
		pass->flags |= Shaderpass_DstBlendBits( Shader_ParseString( ptr ) );
    }
}

static void Shaderpass_AlphaFunc( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
    if( !Q_stricmp( token, "gt0" ) )
		pass->flags |= GLSTATE_AFUNC_GT0;
	else if( !Q_stricmp( token, "lt128" ) )
		pass->flags |= GLSTATE_AFUNC_LT128;
	else if( !Q_stricmp( token, "ge128" ) )
		pass->flags |= GLSTATE_AFUNC_GE128;
}

static void Shaderpass_DepthFunc( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
    if( !Q_stricmp( token, "equal" ) )
		pass->flags |= GLSTATE_DEPTHFUNC_EQ;
}

static void Shaderpass_DepthWrite( shader_t *shader, shaderpass_t *pass, char **ptr )
{
    shader->flags |= SHADER_DEPTHWRITE;
    pass->flags |= GLSTATE_DEPTHWRITE;
}

static void Shaderpass_TcMod( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	int i;
	tcmod_t *tcmod;
	char *token;

	if( pass->numtcmods == MAX_SHADER_TCMODS ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: shader %s has too many tcmods\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	tcmod = &r_currentTcmods[shader->numpasses][r_currentPasses[shader->numpasses].numtcmods];

	token = Shader_ParseString ( ptr );
	if( !Q_stricmp( token, "rotate" ) ) {
		tcmod->args[0] = -Shader_ParseFloat( ptr ) / 360.0f;
		if( !tcmod->args[0] )
			return;
		tcmod->type = TC_MOD_ROTATE;
	} else if( !Q_stricmp( token, "scale" ) ) {
		Shader_ParseVector( ptr, tcmod->args, 2 );
		tcmod->type = TC_MOD_SCALE;
	} else if( !Q_stricmp( token, "scroll" ) ) {
		Shader_ParseVector( ptr, tcmod->args, 2 );
		tcmod->type = TC_MOD_SCROLL;
	} else if( !Q_stricmp( token, "stretch" ) ) {
		shaderfunc_t func;

		Shader_ParseFunc( ptr, &func );

		tcmod->args[0] = func.type;
		for( i = 1; i < 5; i++ )
			tcmod->args[i] = func.args[i-1];
		tcmod->type = TC_MOD_STRETCH;
	} else if( !Q_stricmp( token, "transform" ) ) {
		Shader_ParseVector( ptr, tcmod->args, 6 );
		tcmod->args[4] = tcmod->args[4] - floor( tcmod->args[4] );
		tcmod->args[5] = tcmod->args[5] - floor( tcmod->args[5] );
		tcmod->type = TC_MOD_TRANSFORM;
	} else if( !Q_stricmp( token, "turb" ) ) {
		Shader_ParseVector( ptr, tcmod->args, 4 );
		tcmod->type = TC_MOD_TURB;
	} else {
		Shader_SkipLine( ptr );
		return;
	}

	r_currentPasses[shader->numpasses].numtcmods++;
}

static void Shaderpass_TcGen( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !Q_stricmp( token, "base" ) ) {
		pass->tcgen = TC_GEN_BASE;
	} else if( !Q_stricmp( token, "lightmap" ) ) {
		pass->tcgen = TC_GEN_LIGHTMAP;
	} else if( !Q_stricmp( token, "environment" ) ) {
		pass->tcgen = TC_GEN_ENVIRONMENT;
	} else if( !Q_stricmp( token, "vector" ) ) {
		pass->tcgen = TC_GEN_VECTOR;
		Shader_ParseVector( ptr, r_currentTcGen[shader->numpasses][0], 4 );
		Shader_ParseVector( ptr, r_currentTcGen[shader->numpasses][1], 4 );
	} else if( !Q_stricmp( token, "reflection" ) ) {
		pass->tcgen = TC_GEN_REFLECTION;
	} else if( !Q_stricmp( token, "cellshade" ) ) {
		pass->tcgen = TC_GEN_REFLECTION_CELLSHADE;
	}
}

static void Shaderpass_Detail( shader_t *shader, shaderpass_t *pass, char **ptr ) {
	pass->flags |= SHADERPASS_DETAIL;
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
    {"cubemap",		Shaderpass_CubeMap },
	{"shadecubemap",Shaderpass_ShadeCubeMap },
	{"videomap",	Shaderpass_VideoMap },
    {"clampmap",	Shaderpass_ClampMap },
    {"animclampmap",Shaderpass_AnimClampMap },
	{"normalmap",	Shaderpass_NormalMap },
	{"material",	Shaderpass_Material },
    {"tcgen",		Shaderpass_TcGen },
	{"alphagen",	Shaderpass_AlphaGen },
	{"detail",		Shaderpass_Detail },
    {NULL,			NULL }
};

// ===============================================================

/*
===============
R_ShaderList_f
===============
*/
void R_ShaderList_f( void )
{
	int			i;
	shader_t	*shader;

	Com_Printf ("------------------\n");
	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
		Com_Printf( " %2i %2i: %s\n", shader->numpasses, shader->sort, shader->name);
	Com_Printf ("%i shaders total\n", r_numShaders);
}

void R_InitShaders( qboolean silent )
{
	int i, numfiles;
	char *fileptr;
	size_t filelen, shaderbuflen;

	if( !silent )
		Com_Printf( "Initializing Shaders:\n" );

	r_shadersmempool = Mem_AllocPool( NULL, "Shaders" );

	numfiles = FS_GetFileListExt( "scripts", ".shader", NULL, &shaderbuflen, 0, 0 );
	if ( !numfiles ) {
		Mem_FreePool( &r_shadersmempool );
		Com_Error( ERR_DROP, "Could not find any shaders!");
	}

	shaderPaths = Shader_Malloc( shaderbuflen );
	FS_GetFileListExt( "scripts", ".shader", shaderPaths, &shaderbuflen, 0, 0 );

	// now load all the scripts
	fileptr = shaderPaths;
	memset( shadercache_hash, 0, sizeof(shadercache_t *)*SHADERCACHE_HASH_SIZE );

	for( i = 0; i < numfiles; i++, fileptr += filelen + 1 ) {
		filelen = strlen( fileptr );
		Shader_MakeCache( silent, fileptr );
	}

	if( !silent )
		Com_Printf( "--------------------------------------\n\n" );
}

static void Shader_MakeCache ( qboolean silent, char *name )
{
	unsigned int key;
	char filename[MAX_QPATH];
	char *buf, *ptr, *token, *t;
	shadercache_t *cache;
	int size;

	Q_snprintfz( filename, sizeof(filename), "scripts/%s", name );

	if( !silent )
		Com_Printf( "...loading '%s'\n", filename );

	size = FS_LoadFile( filename, (void **)&buf, NULL, 0 );
	for( ptr = buf; ptr; ) {
		if( ptr - buf >= size )
			break;

		token = COM_ParseExt( &ptr, qtrue );
		if( !token[0] || !ptr || ptr - buf >= size )
			break;

		t = NULL;
		key = Shader_GetCache( token, &cache );
		if ( cache ) {
			Shader_SkipBlock( &ptr );
			continue;
		}

		cache = ( shadercache_t * )Shader_Malloc( sizeof(shadercache_t) + strlen(token) + 1 );
		cache->hash_next = shadercache_hash[key];
		cache->name = ( char * )( (qbyte *)cache + sizeof(shadercache_t) );
		strcpy ( cache->name, token );
		cache->path = name;
		cache->offset = ptr - buf;
		shadercache_hash[key] = cache;

		Shader_SkipBlock( &ptr );
	}

	FS_FreeFile( buf );
}

static unsigned int Shader_GetCache( char *name, shadercache_t **cache )
{
	unsigned int key;
	shadercache_t *c;

	*cache = NULL;

	key = Com_HashKey( name, SHADERCACHE_HASH_SIZE );
	for( c = shadercache_hash[key]; c; c = c->hash_next ) {
		if( !Q_stricmp( c->name, name ) ) {
			*cache = c;
			return key;
		}
	}

	return key;
}

void Shader_FreeShader( shader_t *shader )
{
	int i;
	int shaderNum;
	shaderpass_t *pass;

	shaderNum = shader - r_shaders;
	if( (shader->flags & SHADER_SKY) && r_skydomes[shaderNum] ) {
		R_FreeSkydome( r_skydomes[shaderNum] );
		r_skydomes[shaderNum] = NULL;
	}

	for( i = 0, pass = shader->passes; i < shader->numpasses; i++, pass++ ) {
		if( pass->flags & SHADERPASS_VIDEOMAP ) {
			R_StopCinematic( pass->cin );
			Shader_Free( pass->cin );
			pass->cin = NULL;
		}
	}
}

void R_ShutdownShaders( void )
{
	int i;
	shader_t *shader;

	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
		Shader_FreeShader( shader );

	Mem_FreePool( &r_shadersmempool );

	r_numShaders = 0;
	r_numCinematicShaders = 0;

	shaderPaths = NULL;
	memset( r_shaders, 0, sizeof( r_shaders ) );
	memset( r_cinematicShaders, 0, sizeof( r_cinematicShaders ) );
	memset( shaders_hash, 0, sizeof( shaders_hash ) );
	memset( shadercache_hash, 0, sizeof( shadercache_hash ) );
}

void Shader_SetBlendmode( shaderpass_t *pass )
{
	int blendsrc, blenddst;

	if( pass->flags & SHADERPASS_BLENDMODE )
		return;
	if( !pass->anim_frames[0] && !(pass->flags & (SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT)) )
		return;

	if( !(pass->flags & (GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK)) ) {
		if( (pass->rgbgen.type == RGB_GEN_IDENTITY) && (pass->alphagen.type == ALPHA_GEN_IDENTITY) )
			pass->flags |= SHADERPASS_BLEND_REPLACE;
		else
			pass->flags |= SHADERPASS_BLEND_MODULATE;
		return;
	}

	blendsrc = pass->flags & GLSTATE_SRCBLEND_MASK;
	blenddst = pass->flags & GLSTATE_DSTBLEND_MASK;

	if( blendsrc == GLSTATE_SRCBLEND_ONE && blenddst == GLSTATE_DSTBLEND_ZERO )
		pass->flags |= SHADERPASS_BLEND_MODULATE;
	else if( (blendsrc == GLSTATE_SRCBLEND_ZERO && blenddst == GLSTATE_DSTBLEND_SRC_COLOR) || (blendsrc == GLSTATE_SRCBLEND_DST_COLOR && blenddst == GLSTATE_DSTBLEND_ZERO) )
		pass->flags |= SHADERPASS_BLEND_MODULATE;
	else if( blendsrc == GLSTATE_SRCBLEND_ONE && blenddst == GLSTATE_DSTBLEND_ONE )
		pass->flags |= SHADERPASS_BLEND_ADD;
	else if( blendsrc == GLSTATE_SRCBLEND_SRC_ALPHA && blenddst == GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA )
		pass->flags |= SHADERPASS_BLEND_DECAL;
}

static void Shader_Readpass( shader_t *shader, char **ptr )
{
    char *token;
	shaderpass_t *pass;

	if( shader->numpasses == MAX_SHADER_PASSES ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: shader %s has too many passes\n", shader->name );

		while( ptr ) {	// skip
			token = COM_ParseExt( ptr, qtrue );
			if( !token[0] || token[0] == '}' )
				break;
		}
		return;
	}

    // Set defaults
	pass = &r_currentPasses[shader->numpasses];
	memset( pass, 0, sizeof( shaderpass_t ) );
    pass->rgbgen.type = RGB_GEN_UNKNOWN;
	pass->alphagen.type = ALPHA_GEN_UNKNOWN;
	pass->tcgen = TC_GEN_BASE;

	while( ptr ) {
		token = COM_ParseExt( ptr, qtrue );

		if( !token[0] )
			break;
		else if( token[0] == '}' )
			break;
		else if( Shader_Parsetok( shader, pass, shaderpasskeys, token, ptr ) )
			break;
	}

	if( ((pass->flags & GLSTATE_SRCBLEND_MASK) == GLSTATE_SRCBLEND_ONE) 
		&& ((pass->flags & GLSTATE_DSTBLEND_MASK) == GLSTATE_DSTBLEND_ZERO) ) {
		pass->flags &= ~(GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK);
		pass->flags |= SHADERPASS_BLEND_MODULATE;
	}
	if( !(pass->flags & (GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK)) ) {
		pass->flags |= GLSTATE_DEPTHWRITE;
	}
	if( pass->flags & GLSTATE_DEPTHWRITE ) {
		shader->flags |= SHADER_DEPTHWRITE;
	}

	switch( pass->rgbgen.type ) {
		case RGB_GEN_IDENTITY_LIGHTING:
		case RGB_GEN_IDENTITY:
		case RGB_GEN_CONST:
		case RGB_GEN_COLORWAVE:
		case RGB_GEN_ENTITY:
		case RGB_GEN_ONE_MINUS_ENTITY:
		case RGB_GEN_LIGHTING_DIFFUSE_ONLY:
		case RGB_GEN_LIGHTING_AMBIENT_ONLY:
		case RGB_GEN_CUSTOM:
		case RGB_GEN_UNKNOWN:	// assume RGB_GEN_IDENTITY or RGB_GEN_IDENTITY_LIGHTING
			switch( pass->alphagen.type ) {
				case ALPHA_GEN_UNKNOWN:
				case ALPHA_GEN_IDENTITY:
				case ALPHA_GEN_CONST:
				case ALPHA_GEN_WAVE:
				case ALPHA_GEN_ENTITY:
					pass->flags |= SHADERPASS_NOCOLORARRAY;
					break;
				default:
					break;
			}

			break;
		default:
			break;
	}

	if( (shader->flags & SHADER_SKY) && (shader->flags & SHADER_DEPTHWRITE) ) {
		if( pass->flags & GLSTATE_DEPTHWRITE )
			pass->flags &= ~GLSTATE_DEPTHWRITE;
	}

	shader->numpasses++;
}

static qboolean Shader_Parsetok( shader_t *shader, shaderpass_t *pass, shaderkey_t *keys, char *token, char **ptr )
{
    shaderkey_t *key;

	for( key = keys; key->keyword != NULL; key++ ) {
		if( !Q_stricmp( token, key->keyword ) ) {
			if( key->func )
				key->func( shader, pass, ptr );
			return( *ptr && **ptr == '}' );
		}
	}

	Shader_SkipLine( ptr );

	return qfalse;
}

void Shader_SetFeatures( shader_t *s )
{
	int i;
	qboolean trnormals;
	shaderpass_t *pass;

	if( s->numdeforms )
		s->features |= MF_DEFORMVS;

	for( i = 0, trnormals = qtrue; i < s->numdeforms; i++ ) {
		switch( s->deforms[i].type ) {
			case DEFORMV_BULGE:
				s->features |= MF_STCOORDS;
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

	if( trnormals )
		s->features |= MF_TRNORMALS;

	for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ ) {
		if( pass->program )
			s->features |= MF_NORMALS|MF_SVECTORS|MF_LMCOORDS;

		switch( pass->rgbgen.type ) {
			case RGB_GEN_LIGHTING_DIFFUSE:
				s->features |= MF_NORMALS;
				break;
			case RGB_GEN_VERTEX:
			case RGB_GEN_ONE_MINUS_VERTEX:
			case RGB_GEN_EXACT_VERTEX:
				s->features |= MF_COLORS;
				break;
		}

		switch( pass->alphagen.type ) {
			case ALPHA_GEN_SPECULAR:
			case ALPHA_GEN_DOT:
			case ALPHA_GEN_ONE_MINUS_DOT:
				s->features |= MF_NORMALS;
				break;
			case ALPHA_GEN_VERTEX:
			case ALPHA_GEN_ONE_MINUS_VERTEX:
				s->features |= MF_COLORS;
				break;
		}

		switch( pass->tcgen ) {
			case TC_GEN_LIGHTMAP:
				s->features |= MF_LMCOORDS;
				break;
			case TC_GEN_ENVIRONMENT:
			case TC_GEN_REFLECTION:
			case TC_GEN_REFLECTION_CELLSHADE:
				s->features |= MF_NORMALS;
				break;
			default:
				s->features |= MF_STCOORDS;
				break;
		}
	}
}

void Shader_Finish( shader_t *s )
{
	int i, j, size;
	shaderpass_t *pass;
	qbyte *buffer;

	if( !s->numpasses && !s->sort ) {
		if( s->numdeforms ) {
			s->deforms = Shader_Malloc( s->numdeforms * sizeof( deformv_t ) );
			memcpy( s->deforms, r_currentDeforms, s->numdeforms * sizeof( deformv_t ) );
		}
		s->sort = SHADER_SORT_ADDITIVE;
		return;
	}

	if( (s->flags & SHADER_POLYGONOFFSET) && !s->sort )
		s->sort = SHADER_SORT_DECAL;

	size = s->numdeforms * sizeof( deformv_t ) + s->numpasses * sizeof( shaderpass_t );
	for( i = 0; i < s->numpasses; i++ ) {
		size += r_currentPasses[i].numtcmods * sizeof( tcmod_t );
		if( r_currentPasses[i].tcgen == TC_GEN_VECTOR )
			size += sizeof( vec4_t ) * 2;
	}

	buffer = Shader_Malloc( size );
	s->passes = ( shaderpass_t * )buffer;
	memcpy( s->passes, r_currentPasses, s->numpasses * sizeof( shaderpass_t ) );

	buffer += s->numpasses * sizeof( shaderpass_t );
	for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ ) {
		if( r_currentPasses[i].numtcmods ) {
			pass->tcmods = ( tcmod_t * )buffer;
			pass->numtcmods = r_currentPasses[i].numtcmods;
			memcpy( pass->tcmods, r_currentTcmods[i], r_currentPasses[i].numtcmods * sizeof( tcmod_t ) );
			buffer += r_currentPasses[i].numtcmods * sizeof( tcmod_t );
		}

		if( r_currentPasses[i].tcgen == TC_GEN_VECTOR ) {
			pass->tcgenVec = ( vec_t * )buffer;
			Vector4Copy( r_currentTcGen[i][0], &pass->tcgenVec[0] );
			Vector4Copy( r_currentTcGen[i][1], &pass->tcgenVec[4] );
			buffer += sizeof( vec4_t ) * 2;
		}
	}

	if( s->numdeforms ) {
		s->deforms = ( deformv_t * )buffer;
		memcpy( s->deforms, r_currentDeforms, s->numdeforms * sizeof( deformv_t ) );
	}

	for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ ) {
		if( pass->flags & SHADERPASS_LIGHTMAP )
			s->flags |= SHADER_LIGHTMAP;
		if( pass->program )
			s->flags |= SHADER_NO_MODULATIVE_DLIGHTS;
		Shader_SetBlendmode( pass );
	}

	for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ ) {
		if( !(pass->flags & (GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK)) )
			break;
	}

	// all passes have blendfuncs
	if( i == s->numpasses ) {
		int opaque;

		opaque = -1;
		for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ ) {
			if( ((pass->flags & GLSTATE_SRCBLEND_MASK) == GLSTATE_SRCBLEND_ONE) 
				&& ((pass->flags & GLSTATE_DSTBLEND_MASK) == GLSTATE_DSTBLEND_ZERO) )
				opaque = i;

			if( pass->rgbgen.type == RGB_GEN_UNKNOWN ) {
				if( !s->fog_dist && !(pass->flags & SHADERPASS_LIGHTMAP) ) 
					pass->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
				else
					pass->rgbgen.type = RGB_GEN_IDENTITY;
			}
			if( pass->alphagen.type == ALPHA_GEN_UNKNOWN ) {
				if( pass->rgbgen.type == RGB_GEN_VERTEX/* || pass->rgbgen.type == RGB_GEN_EXACT_VERTEX*/ )
					pass->alphagen.type = ALPHA_GEN_VERTEX;
				else
					pass->alphagen.type = ALPHA_GEN_IDENTITY;
			}
		}

		if( !( s->flags & SHADER_SKY ) && !s->sort ) {
			if( s->flags & SHADER_DEPTHWRITE || (opaque != -1 && s->passes[opaque].flags & GLSTATE_ALPHAFUNC) )
				s->sort = SHADER_SORT_ALPHATEST;
			else if( opaque == -1 )
				s->sort = SHADER_SORT_ADDITIVE;
			else
				s->sort = SHADER_SORT_OPAQUE;
		}
	} else {
		shaderpass_t *sp;

		for( j = 0, sp = s->passes; j < s->numpasses; j++, sp++ ) {
			if( sp->rgbgen.type == RGB_GEN_UNKNOWN ) {
				if( sp->flags & GLSTATE_ALPHAFUNC && !(j && s->passes[j-1].flags & SHADERPASS_LIGHTMAP) ) // FIXME!
					sp->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
				else
					sp->rgbgen.type = RGB_GEN_IDENTITY;
			}
			if( sp->alphagen.type == ALPHA_GEN_UNKNOWN ) {
				if( sp->rgbgen.type == RGB_GEN_VERTEX/* || sp->rgbgen.type == RGB_GEN_EXACT_VERTEX*/ )
					sp->alphagen.type = ALPHA_GEN_VERTEX;
				else
					sp->alphagen.type = ALPHA_GEN_IDENTITY;
			}
		}

		if( !s->sort ) {
			if( pass->flags & GLSTATE_ALPHAFUNC )
				s->sort = SHADER_SORT_ALPHATEST;
		}

		if( !(pass->flags & GLSTATE_DEPTHWRITE) && !(s->flags & SHADER_SKY) ) {
			pass->flags |= GLSTATE_DEPTHWRITE;
			s->flags |= SHADER_DEPTHWRITE;
		}
	}

	if( !s->sort )
		s->sort = SHADER_SORT_OPAQUE;

	if( (s->flags & SHADER_SKY) && (s->flags & SHADER_DEPTHWRITE) )
		s->flags &= ~SHADER_DEPTHWRITE;

	Shader_SetFeatures( s );
}

void R_UploadCinematicShader( shader_t *shader )
{
	int j;
	shaderpass_t *pass;

	// upload cinematics
	for( j = 0, pass = shader->passes; j < shader->numpasses; j++, pass++ ) {
		if( pass->flags & SHADERPASS_VIDEOMAP )
			pass->anim_frames[0] = R_ResampleCinematicFrame( pass );
	}
}

void R_RunCinematicShaders( void )
{
	int i, j;
	shader_t *shader;
	shaderpass_t *pass;

	for( i = 0; i < r_numCinematicShaders; i++ ) {
		shader = r_cinematicShaders[i];

		for ( j = 0, pass = shader->passes; j < shader->numpasses; j++, pass++ ) {
			if( !(pass->flags & SHADERPASS_VIDEOMAP) )
				continue;

			// reinitialize
			if( pass->cin->frame == -1 ) {
				R_StopCinematic( pass->cin );
				R_PlayCinematic( pass->cin );

				if( pass->cin->time == 0 ) {		// not found
					pass->flags &= ~SHADERPASS_VIDEOMAP;
					Shader_Free( pass->cin );
				}
				continue;
			}

			R_RunCinematic( pass->cin );
		}
	}
}

shader_t *R_LoadShader( char *name, int type, qboolean forceDefault, int addFlags, int ignoreType )
{
	int i, length, lastDot = -1;
	unsigned int key;
	char shortname[MAX_QPATH], path[MAX_QPATH];
	char *buf = NULL;
	shader_t *s;
	shadercache_t *cache;
	shaderpass_t *pass;
	image_t *materialImages[MAX_SHADER_ANIM_FRAMES];

	if( !name || !name[0] )
		return NULL;

	if( r_numShaders == MAX_SHADERS )
		Com_Error( ERR_FATAL, "R_LoadShader: Shader limit exceeded" );

	for( i = ( name[0] == '/' || name[0] == '\\' ), length = 0; name[i] && (length < sizeof(shortname)-1); i++ ) {
		if( name[i] == '.' )
			lastDot = length;
		if( name[i] == '\\' ) 
			shortname[length++] = '/';
		else
			shortname[length++] = tolower( name[i] );
	}

	if( !length )
		return NULL;
	if( lastDot != -1 )
		length = lastDot;
	shortname[length] = 0;

	// test if already loaded
	key = Com_HashKey( shortname, SHADERS_HASH_SIZE );
	for( s = shaders_hash[key]; s; s = s->hash_next ) {
		if( !strcmp( s->name, shortname ) && (s->type != ignoreType) )
			return s;
	}

	s = &r_shaders[r_numShaders++];
	memset( s, 0, sizeof( shader_t ) );
	Q_strncpyz( s->name, shortname, sizeof(s->name) );

	if( ignoreType == SHADER_UNKNOWN )
		forceDefault = qtrue;

	cache = NULL;
	if( !forceDefault ) {
		Shader_GetCache( shortname, &cache );
		if( cache ) {
			Q_snprintfz( path, sizeof(path), "scripts/%s", cache->path );
			length = FS_LoadFile( path, (void **)&buf, NULL, 0 );
		}
	}

	// the shader is in the shader scripts
	if( cache && buf && (cache->offset < length) ) {
		char *ptr, *token;

		// set defaults
		s->type = SHADER_UNKNOWN;
		s->flags = SHADER_CULL_FRONT;
		s->features = MF_NONE;

		ptr = buf + cache->offset;
		token = COM_ParseExt( &ptr, qtrue );

		if( !ptr || token[0] != '{' )
			goto create_default;

		while( ptr ) {
			token = COM_ParseExt( &ptr, qtrue );

			if( !token[0] )
				break;
			else if( token[0] == '}' )
				break;
			else if( token[0] == '{' )
				Shader_Readpass( s, &ptr );
			else if( Shader_Parsetok( s, NULL, shaderkeys, token, &ptr ) )
				break;
		}

		Shader_Finish( s );
		FS_FreeFile( buf );
	} else {		// make a default shader
		switch( type ) {
			case SHADER_BSP_VERTEX:
				s->type = SHADER_BSP_VERTEX;
				s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS;
				s->features = MF_STCOORDS|MF_COLORS|MF_TRNORMALS;
				s->sort = SHADER_SORT_OPAQUE;
				s->numpasses = 3;
				s->passes = Shader_Malloc( sizeof( shaderpass_t ) * 3 );
				pass = &s->passes[0];
				pass->anim_frames[0] = r_whitetexture;
				pass->flags = GLSTATE_DEPTHWRITE|SHADERPASS_BLEND_MODULATE/*|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO*/;
				pass->tcgen = TC_GEN_BASE;
				pass->rgbgen.type = RGB_GEN_VERTEX;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass = &s->passes[1];
				pass->flags = SHADERPASS_DLIGHT|GLSTATE_DEPTHFUNC_EQ|SHADERPASS_BLEND_ADD|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
				pass->tcgen = TC_GEN_BASE;
				pass = &s->passes[2];
				pass->flags = SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_MODULATE|GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
				pass->tcgen = TC_GEN_BASE;
				pass->anim_frames[0] = Shader_FindImage( s, shortname, addFlags, 0 );
				pass->rgbgen.type = RGB_GEN_IDENTITY;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				break;
			case SHADER_BSP_FLARE:
				s->type = SHADER_BSP_FLARE;
				s->flags = SHADER_FLARE;
				s->features = MF_STCOORDS|MF_COLORS;
				s->sort = SHADER_SORT_ADDITIVE;
				s->numpasses = 1;
				s->passes = Shader_Malloc( sizeof( shaderpass_t ) );
				pass = &s->passes[0];
				pass->flags = SHADERPASS_BLEND_ADD|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
				pass->anim_frames[0] = Shader_FindImage( s, shortname, addFlags, 0 );
				pass->rgbgen.type = RGB_GEN_VERTEX;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->tcgen = TC_GEN_BASE;
				break;
			case SHADER_MD3:
				s->type = SHADER_MD3;
				s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT;
				s->features = MF_STCOORDS|MF_NORMALS|MF_SVECTORS;
				s->sort = SHADER_SORT_OPAQUE;
				s->numpasses = 1;
				s->passes = Shader_Malloc( sizeof( shaderpass_t ) );
				pass = &s->passes[0];
				pass->flags = GLSTATE_DEPTHWRITE|SHADERPASS_BLEND_MODULATE;
				pass->rgbgen.type = RGB_GEN_LIGHTING_DIFFUSE;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->tcgen = TC_GEN_BASE;
				pass->anim_frames[0] = Shader_FindImage( s, shortname, addFlags, 0 );

				// load default GLSL program if there's a bumpmap was found
				if( (r_lighting_models_followdeluxe->integer ? mapConfig.deluxeMappingEnabled : glConfig.GLSL) 
					&& Shaderpass_LoadMaterial( &materialImages[0], &materialImages[1], shortname, addFlags, 1 ) ) {
					pass->rgbgen.type = RGB_GEN_IDENTITY;
					pass->program = DEFAULT_GLSL_PROGRAM;
					pass->anim_frames[1] = materialImages[0];		// normalmap
					pass->anim_frames[2] = materialImages[1];		// glossmap
				}
				break;
			case SHADER_2D:
				s->type = SHADER_2D;
				s->flags = SHADER_NOPICMIP|SHADER_NOMIPMAPS;
				s->features = MF_STCOORDS|MF_COLORS;
				s->sort = SHADER_SORT_ADDITIVE;
				s->numpasses = 1;
				s->passes = Shader_Malloc( sizeof( shaderpass_t ) );
				pass = &s->passes[0];
				pass->flags = SHADERPASS_BLEND_MODULATE|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA/* | SHADERPASS_NOCOLORARRAY*/;
				pass->anim_frames[0] = Shader_FindImage( s, shortname, IT_CLAMP|IT_NOPICMIP|IT_NOMIPMAP|addFlags, 0 );
				pass->rgbgen.type = RGB_GEN_VERTEX;
				pass->alphagen.type = ALPHA_GEN_VERTEX;
				pass->tcgen = TC_GEN_BASE;
				break;
			case SHADER_FARBOX:
				s->type = SHADER_FARBOX;
				s->features = MF_STCOORDS;
				s->sort = SHADER_SORT_SKY;
				s->numpasses = 1;
				s->flags = SHADER_SKY;
				s->passes = Shader_Malloc( sizeof( shaderpass_t ) );
				pass = &s->passes[0];
				pass->flags = SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_MODULATE/*|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO*/;
				pass->anim_frames[0] = R_FindImage( shortname, IT_NOMIPMAP|IT_CLAMP|addFlags, 0 );
				pass->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->tcgen = TC_GEN_BASE;
				break;
			case SHADER_NEARBOX:
				s->type = SHADER_NEARBOX;
				s->features = MF_STCOORDS;
				s->sort = SHADER_SORT_SKY;
				s->numpasses = 1;
				s->flags = SHADER_SKY;
				s->passes = Shader_Malloc( sizeof( shaderpass_t ) );
				pass = &s->passes[0];
				pass->flags = GLSTATE_ALPHAFUNC|SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_DECAL|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
				pass->anim_frames[0] = R_FindImage( shortname, IT_NOMIPMAP|IT_CLAMP|addFlags, 0 );
				pass->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->tcgen = TC_GEN_BASE;
				break;
			case SHADER_BSP:
			default:
create_default:
				if( mapConfig.deluxeMappingEnabled 
					&& Shaderpass_LoadMaterial( &materialImages[0], &materialImages[1], shortname, addFlags, 1 ) ) {
					s->type = SHADER_BSP;
					s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS|SHADER_LIGHTMAP;
					s->features = MF_STCOORDS|MF_LMCOORDS|MF_TRNORMALS|MF_NORMALS|MF_SVECTORS;
					s->sort = SHADER_SORT_OPAQUE;
					s->numpasses = 1;
					s->passes = Shader_Malloc( sizeof( shaderpass_t ) );
					pass = &s->passes[0];
					pass->flags = SHADERPASS_LIGHTMAP|SHADERPASS_DELUXEMAP|GLSTATE_DEPTHWRITE|SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_REPLACE;
					pass->tcgen = TC_GEN_BASE;
					pass->rgbgen.type = RGB_GEN_IDENTITY;
					pass->alphagen.type = ALPHA_GEN_IDENTITY;
					pass->program = DEFAULT_GLSL_PROGRAM;
					pass->anim_frames[0] = Shader_FindImage( s, shortname, addFlags, 0 );
					pass->anim_frames[1] = materialImages[0];	// normalmap
					pass->anim_frames[2] = materialImages[1];	// glossmap
				} else {
					s->type = SHADER_BSP;
					s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS|SHADER_LIGHTMAP;
					s->features = MF_STCOORDS|MF_LMCOORDS|MF_TRNORMALS;
					s->sort = SHADER_SORT_OPAQUE;
					s->numpasses = 3;
					s->passes = Shader_Malloc( sizeof( shaderpass_t ) * 3 );
					pass = &s->passes[0];
					pass->flags = SHADERPASS_LIGHTMAP|GLSTATE_DEPTHWRITE|SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_REPLACE;
					pass->tcgen = TC_GEN_LIGHTMAP;
					pass->rgbgen.type = RGB_GEN_IDENTITY;
					pass->alphagen.type = ALPHA_GEN_IDENTITY;
					pass = &s->passes[1];
					pass->flags = SHADERPASS_DLIGHT|GLSTATE_DEPTHFUNC_EQ|SHADERPASS_BLEND_ADD|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
					pass->tcgen = TC_GEN_BASE;
					pass = &s->passes[2];
					pass->flags = SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_MODULATE|GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
					pass->tcgen = TC_GEN_BASE;
					pass->anim_frames[0] = Shader_FindImage( s, shortname, addFlags, 0 );
					pass->rgbgen.type = RGB_GEN_IDENTITY;
					pass->alphagen.type = ALPHA_GEN_IDENTITY;
				}
				break;
		}
	}

	// calculate sortkey
	s->sortkey = Shader_Sortkey( s, s->sort );

	// add to hash table
	s->hash_next = shaders_hash[key];
	shaders_hash[key] = s;

	return s;
}

shader_t *R_RegisterPic( char *name ) {
	return R_LoadShader( name, SHADER_2D, qfalse, 0, SHADER_INVALID );
}

shader_t *R_RegisterShader( char *name ) {
	return R_LoadShader( name, SHADER_BSP, qfalse, 0, SHADER_INVALID );
}

shader_t *R_RegisterSkin( char *name ) {
	return R_LoadShader( name, SHADER_MD3, qfalse, 0, SHADER_INVALID );
}
