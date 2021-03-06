/*
Copyright (C) 1999 Stephen C. Taylor
Copyright (C) 2002-2007 Victor Luchits

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

// r_shader.c

#include "r_local.h"

#define SHADERS_HASH_SIZE	128
#define SHADERCACHE_HASH_SIZE	128

typedef struct
{
	char *keyword;
	void ( *func )( shader_t *shader, shaderpass_t *pass, const char **ptr );
} shaderkey_t;

typedef struct shadercache_s
{
	char *name;
	char *buffer;
	const char *filename;
	size_t offset;
	struct shadercache_s *hash_next;
} shadercache_t;

shader_t r_shaders[MAX_SHADERS];
int r_numShaders;
skydome_t *r_skydomes[MAX_SHADERS];

static char *shaderPaths;
static shader_t	*shaders_hash[SHADERS_HASH_SIZE];
static shadercache_t *shadercache_hash[SHADERCACHE_HASH_SIZE];

static deformv_t r_currentDeforms[MAX_SHADER_DEFORMVS];
static shaderpass_t r_currentPasses[MAX_SHADER_PASSES];
static float r_currentRGBgenArgs[MAX_SHADER_PASSES][3], r_currentAlphagenArgs[MAX_SHADER_PASSES][2];
static shaderfunc_t r_currentRGBgenFuncs[MAX_SHADER_PASSES], r_currentAlphagenFuncs[MAX_SHADER_PASSES];
static tcmod_t r_currentTcmods[MAX_SHADER_PASSES][MAX_SHADER_TCMODS];
static vec4_t r_currentTcGen[MAX_SHADER_PASSES][2];

static qboolean	r_shaderNoMipMaps;
static qboolean	r_shaderNoPicMip;
static qboolean	r_shaderNoCompress;
static qboolean	r_shaderHasDlightPass;

mempool_t *r_shadersmempool;

static qboolean Shader_Parsetok( shader_t *shader, shaderpass_t *pass, const shaderkey_t *keys, const char *token, const char **ptr );
static void Shader_MakeCache( qboolean silent, const char *filename );
static unsigned int Shader_GetCache( const char *name, shadercache_t **cache );
#define Shader_FreePassCinematics(pass) if( (pass)->cin ) { R_FreeCinematics( (pass)->cin ); (pass)->cin = 0; }

//===========================================================================

static char *Shader_ParseString( const char **ptr )
{
	char *token;

	if( !ptr || !( *ptr ) )
		return "";
	if( !**ptr || **ptr == '}' )
		return "";

	token = COM_ParseExt( ptr, qfalse );
	return Q_strlwr( token );
}

static float Shader_ParseFloat( const char **ptr )
{
	if( !ptr || !( *ptr ) )
		return 0;
	if( !**ptr || **ptr == '}' )
		return 0;

	return atof( COM_ParseExt( ptr, qfalse ) );
}

static void Shader_ParseVector( const char **ptr, float *v, unsigned int size )
{
	unsigned int i;
	char *token;
	qboolean bracket;

	if( !size )
		return;
	if( size == 1 )
	{
		Shader_ParseFloat( ptr );
		return;
	}

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "(" ) )
	{
		bracket = qtrue;
		token = Shader_ParseString( ptr );
	}
	else if( token[0] == '(' )
	{
		bracket = qtrue;
		token = &token[1];
	}
	else
	{
		bracket = qfalse;
	}

	v[0] = atof( token );
	for( i = 1; i < size-1; i++ )
		v[i] = Shader_ParseFloat( ptr );

	token = Shader_ParseString( ptr );
	if( !token[0] )
	{
		v[i] = 0;
	}
	else if( token[strlen( token )-1] == ')' )
	{
		token[strlen( token )-1] = 0;
		v[i] = atof( token );
	}
	else
	{
		v[i] = atof( token );
		if( bracket )
			Shader_ParseString( ptr );
	}
}

static void Shader_SkipLine( const char **ptr )
{
	while( ptr )
	{
		const char *token = COM_ParseExt( ptr, qfalse );
		if( !token[0] )
			return;
	}
}

static void Shader_SkipBlock( const char **ptr )
{
	const char *tok;
	int brace_count;

	// Opening brace
	tok = COM_ParseExt( ptr, qtrue );
	if( tok[0] != '{' )
		return;

	for( brace_count = 1; brace_count > 0; )
	{
		tok = COM_ParseExt( ptr, qtrue );
		if( !tok[0] )
			return;
		if( tok[0] == '{' )
			brace_count++;
		else if( tok[0] == '}' )
			brace_count--;
	}
}

#define MAX_CONDITIONS		8
typedef enum { COP_LS, COP_LE, COP_EQ, COP_GR, COP_GE, COP_NE } conOp_t;
typedef enum { COP2_AND, COP2_OR } conOp2_t;
typedef struct { int operand; conOp_t op; qboolean negative; int val; conOp2_t logic; } shaderCon_t;

char *conOpStrings[] = { "<", "<=", "==", ">", ">=", "!=", NULL };
char *conOpStrings2[] = { "&&", "||", NULL };

static qboolean Shader_ParseConditions( const char **ptr, shader_t *shader )
{
	int i;
	char *tok;
	int numConditions;
	shaderCon_t conditions[MAX_CONDITIONS];
	qboolean result = qfalse, val = qfalse, skip, expectingOperator;
	static const int falseCondition = 0;

	numConditions = 0;
	memset( conditions, 0, sizeof( conditions ) );

	skip = qfalse;
	expectingOperator = qfalse;
	while( 1 )
	{
		tok = Shader_ParseString( ptr );
		if( !tok[0] )
		{
			if( expectingOperator )
				numConditions++;
			break;
		}
		if( skip )
			continue;

		for( i = 0; conOpStrings[i]; i++ )
		{
			if( !strcmp( tok, conOpStrings[i] ) )
				break;
		}

		if( conOpStrings[i] )
		{
			if( !expectingOperator )
			{
				Com_Printf( S_COLOR_YELLOW "WARNING: Bad syntax in condition (shader %s)\n", shader->name );
				skip = qtrue;
			}
			else
			{
				conditions[numConditions].op = i;
				expectingOperator = qfalse;
			}
			continue;
		}

		for( i = 0; conOpStrings2[i]; i++ )
		{
			if( !strcmp( tok, conOpStrings2[i] ) )
				break;
		}

		if( conOpStrings2[i] )
		{
			if( !expectingOperator )
			{
				Com_Printf( S_COLOR_YELLOW "WARNING: Bad syntax in condition (shader %s)\n", shader->name );
				skip = qtrue;
			}
			else
			{
				conditions[numConditions++].logic = i;
				if( numConditions == MAX_CONDITIONS )
					skip = qtrue;
				else
					expectingOperator = qfalse;
			}
			continue;
		}

		if( expectingOperator )
		{
			Com_Printf( S_COLOR_YELLOW "WARNING: Bad syntax in condition (shader %s)\n", shader->name );
			skip = qtrue;
			continue;
		}

		if( !strcmp( tok, "!" ) )
		{
			conditions[numConditions].negative = !conditions[numConditions].negative;
			continue;
		}

		if( !conditions[numConditions].operand )
		{
			if( !Q_stricmp( tok, "maxTextureSize" ) )
				conditions[numConditions].operand = ( int  )glConfig.maxTextureSize;
			else if( !Q_stricmp( tok, "maxTextureCubemapSize" ) )
				conditions[numConditions].operand = ( int )glConfig.maxTextureCubemapSize;
			else if( !Q_stricmp( tok, "maxTextureUnits" ) )
				conditions[numConditions].operand = ( int )glConfig.maxTextureUnits;
			else if( !Q_stricmp( tok, "textureCubeMap" ) )
				conditions[numConditions].operand = ( int )glConfig.ext.texture_cube_map;
			else if( !Q_stricmp( tok, "textureEnvCombine" ) )
				conditions[numConditions].operand = ( int )glConfig.ext.texture_env_combine;
			else if( !Q_stricmp( tok, "textureEnvDot3" ) )
				conditions[numConditions].operand = ( int )glConfig.ext.GLSL;
			else if( !Q_stricmp( tok, "GLSL" ) )
				conditions[numConditions].operand = ( int )glConfig.ext.GLSL;
			else if( !Q_stricmp( tok, "deluxeMaps" ) || !Q_stricmp( tok, "deluxe" ) )
				conditions[numConditions].operand = ( int )mapConfig.deluxeMappingEnabled;
			else if( !Q_stricmp( tok, "portalMaps" ) )
				conditions[numConditions].operand = ( int )r_portalmaps->integer;
			else
			{
				Com_Printf( S_COLOR_YELLOW "WARNING: Unknown expression '%s' in shader %s\n", tok, shader->name );
//				skip = qtrue;
				conditions[numConditions].operand = ( int )falseCondition;
			}

			conditions[numConditions].operand++;
			if( conditions[numConditions].operand < 0 )
				conditions[numConditions].operand = 0;

			if( !skip )
			{
				conditions[numConditions].op = COP_NE;
				expectingOperator = qtrue;
			}
			continue;
		}

		if( !strcmp( tok, "false" ) )
			conditions[numConditions].val = 0;
		else if( !strcmp( tok, "true" ) )
			conditions[numConditions].val = 1;
		else
			conditions[numConditions].val = atoi( tok );
		expectingOperator = qtrue;
	}

	if( skip )
		return qfalse;

	if( !conditions[0].operand )
	{
		Com_Printf( S_COLOR_YELLOW "WARNING: Empty 'if' statement in shader %s\n", shader->name );
		return qfalse;
	}


	for( i = 0; i < numConditions; i++ )
	{
		conditions[i].operand--;

		switch( conditions[i].op )
		{
		case COP_LS:
			val = ( conditions[i].operand < conditions[i].val );
			break;
		case COP_LE:
			val = ( conditions[i].operand <= conditions[i].val );
			break;
		case COP_EQ:
			val = ( conditions[i].operand == conditions[i].val );
			break;
		case COP_GR:
			val = ( conditions[i].operand > conditions[i].val );
			break;
		case COP_GE:
			val = ( conditions[i].operand >= conditions[i].val );
			break;
		case COP_NE:
			val = ( conditions[i].operand != conditions[i].val );
			break;
		default:
			break;
		}

		if( conditions[i].negative )
			val = !val;
		if( i )
		{
			switch( conditions[i-1].logic )
			{
			case COP2_AND:
				result = result && val;
				break;
			case COP2_OR:
				result = result || val;
				break;
			}
		}
		else
		{
			result = val;
		}
	}

	return result;
}

static qboolean Shader_SkipConditionBlock( const char **ptr )
{
	const char *tok;
	int condition_count;

	for( condition_count = 1; condition_count > 0; )
	{
		tok = COM_ParseExt( ptr, qtrue );
		if( !tok[0] )
			return qfalse;
		if( !Q_stricmp( tok, "if" ) )
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

static void Shader_ParseSkySides( const char **ptr, shader_t **shaders, qboolean farbox )
{
	int i, j;
	char *token;
	image_t *image;
	qboolean noskybox = qfalse;

	token = Shader_ParseString( ptr );
	if( token[0] == '-' )
	{
		noskybox = qtrue;
	}
	else
	{
		struct cubemapSufAndFlip
		{
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

		for( i = 0; i < 2; i++ )
		{
			memset( shaders, 0, sizeof( shader_t * ) * 6 );

			for( j = 0; j < 6; j++ )
			{
				image = R_FindImage( token, cubemapSides[i][j].suf, IT_NOMIPMAP|IT_CLAMP|cubemapSides[i][j].flags, 0 );
				if( !image )
					break;

				shaders[j] = R_LoadShader( image->name, ( farbox ? SHADER_FARBOX : SHADER_NEARBOX ), qtrue, image->flags, SHADER_INVALID );
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

static void Shader_ParseFunc( const char **ptr, shaderfunc_t *func )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "sin" ) )
		func->type = SHADER_FUNC_SIN;
	else if( !strcmp( token, "triangle" ) )
		func->type = SHADER_FUNC_TRIANGLE;
	else if( !strcmp( token, "square" ) )
		func->type = SHADER_FUNC_SQUARE;
	else if( !strcmp( token, "sawtooth" ) )
		func->type = SHADER_FUNC_SAWTOOTH;
	else if( !strcmp( token, "inversesawtooth" ) )
		func->type = SHADER_FUNC_INVERSESAWTOOTH;
	else if( !strcmp( token, "noise" ) )
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
	if( r_shaderNoMipMaps )
		flags |= IT_NOMIPMAP;
	if( r_shaderNoPicMip )
		flags |= IT_NOPICMIP;
	if( r_shaderNoCompress )
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
	if( !Q_stricmp( name, "$blankbumpimage" ) || !Q_stricmp( name, "*blankbump" ) )
		return r_blankbumptexture;
	if( !Q_stricmp( name, "$particleimage" ) || !Q_stricmp( name, "*particle" ) )
		return r_particletexture;
	if( !Q_strnicmp( name, "*lm", 3 ) )
	{
		Com_DPrintf( S_COLOR_YELLOW "WARNING: shader %s has a stage with explicit lightmap image.\n", shader->name );
		return r_whitetexture;
	}

	image = R_FindImage( name, NULL, flags, bumpScale );
	if( !image )
	{
		Com_DPrintf( S_COLOR_YELLOW "WARNING: shader %s has a stage with no image: %s.\n", shader->name, name );
		return r_notexture;
	}

	return image;
}

/****************** shader keyword functions ************************/

static void Shader_Cull( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;

	shader->flags &= ~( SHADER_CULL_FRONT|SHADER_CULL_BACK );

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "disable" ) || !strcmp( token, "none" ) || !strcmp( token, "twosided" ) )
		;
	else if( !strcmp( token, "back" ) || !strcmp( token, "backside" ) || !strcmp( token, "backsided" ) )
		shader->flags |= SHADER_CULL_BACK;
	else
		shader->flags |= SHADER_CULL_FRONT;
}

static void Shader_shaderNoMipMaps( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	r_shaderNoMipMaps = r_shaderNoPicMip = qtrue;
}

static void Shader_shaderNoPicMip( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	r_shaderNoPicMip = qtrue;
}

static void Shader_shaderNoCompress( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	r_shaderNoCompress = qtrue;
}

static void Shader_DeformVertexes( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;
	deformv_t *deformv;

	if( shader->numdeforms == MAX_SHADER_DEFORMVS )
	{
		Com_Printf( S_COLOR_YELLOW "WARNING: shader %s has too many deforms\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	deformv = &r_currentDeforms[shader->numdeforms];

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "wave" ) )
	{
		deformv->type = DEFORMV_WAVE;
		deformv->args[0] = Shader_ParseFloat( ptr );
		if( deformv->args[0] )
			deformv->args[0] = 1.0f / deformv->args[0];
		else
			deformv->args[0] = 100.0f;
		Shader_ParseFunc( ptr, &deformv->func );
	}
	else if( !strcmp( token, "normal" ) )
	{
		shader->flags |= SHADER_DEFORMV_NORMAL;
		deformv->type = DEFORMV_NORMAL;
		deformv->args[0] = Shader_ParseFloat( ptr );
		deformv->args[1] = Shader_ParseFloat( ptr );
	}
	else if( !strcmp( token, "bulge" ) )
	{
		deformv->type = DEFORMV_BULGE;
		Shader_ParseVector( ptr, deformv->args, 3 );
	}
	else if( !strcmp( token, "move" ) )
	{
		deformv->type = DEFORMV_MOVE;
		Shader_ParseVector( ptr, deformv->args, 3 );
		Shader_ParseFunc( ptr, &deformv->func );
	}
	else if( !strcmp( token, "autosprite" ) )
	{
		deformv->type = DEFORMV_AUTOSPRITE;
		shader->flags |= SHADER_AUTOSPRITE;
	}
	else if( !strcmp( token, "autosprite2" ) )
	{
		deformv->type = DEFORMV_AUTOSPRITE2;
		shader->flags |= SHADER_AUTOSPRITE;
	}
	else if( !strcmp( token, "projectionShadow" ) )
		deformv->type = DEFORMV_PROJECTION_SHADOW;
	else if( !strcmp( token, "autoparticle" ) )
		deformv->type = DEFORMV_AUTOPARTICLE;
#ifdef HARDWARE_OUTLINES
	else if( !strcmp( token, "outline" ) )
		deformv->type = DEFORMV_OUTLINE;
#endif
	else
	{
		Shader_SkipLine( ptr );
		return;
	}

	shader->numdeforms++;
}

static void Shader_SkyParms( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	int shaderNum;
	float skyheight;
	shader_t *farboxShaders[6];
	shader_t *nearboxShaders[6];

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

static void Shader_FogParms( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	float div;
	vec3_t color, fcolor;

	if( !r_ignorehwgamma->integer )
		div = 1.0f / pow( 2, max( 0, floor( r_overbrightbits->value ) ) );
	else
		div = 1.0f;

	Shader_ParseVector( ptr, color, 3 );
	ColorNormalize( color, fcolor );
	VectorScale( fcolor, div, fcolor );

	shader->fog_color[0] = R_FloatToByte( fcolor[0] );
	shader->fog_color[1] = R_FloatToByte( fcolor[1] );
	shader->fog_color[2] = R_FloatToByte( fcolor[2] );
	shader->fog_color[3] = 255;
	shader->fog_dist = Shader_ParseFloat( ptr );
	if( shader->fog_dist <= 0.1 )
		shader->fog_dist = 128.0;

	shader->fog_clearDist = Shader_ParseFloat( ptr );
	if( shader->fog_clearDist > shader->fog_dist - 128 )
		shader->fog_clearDist = shader->fog_dist - 128;
	if( shader->fog_clearDist <= 0.0 )
		shader->fog_clearDist = 0;
}

static void Shader_Sort( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "portal" ) )
		shader->sort = SHADER_SORT_PORTAL;
	else if( !strcmp( token, "sky" ) )
		shader->sort = SHADER_SORT_SKY;
	else if( !strcmp( token, "opaque" ) )
		shader->sort = SHADER_SORT_OPAQUE;
	else if( !strcmp( token, "banner" ) )
		shader->sort = SHADER_SORT_BANNER;
	else if( !strcmp( token, "underwater" ) )
		shader->sort = SHADER_SORT_UNDERWATER;
	else if( !strcmp( token, "additive" ) )
		shader->sort = SHADER_SORT_ADDITIVE;
	else if( !strcmp( token, "nearest" ) )
		shader->sort = SHADER_SORT_NEAREST;
	else
	{
		shader->sort = atoi( token );
		if( shader->sort > SHADER_SORT_NEAREST )
			shader->sort = SHADER_SORT_NEAREST;
	}
}

static void Shader_Portal( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	shader->flags |= SHADER_PORTAL;
	shader->sort = SHADER_SORT_PORTAL;
}

static void Shader_PolygonOffset( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	shader->flags |= SHADER_POLYGONOFFSET;
}

static void Shader_EntityMergable( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	shader->flags |= SHADER_ENTITY_MERGABLE;
}

static void Shader_If( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	if( !Shader_ParseConditions( ptr, shader ) )
	{
		if( !Shader_SkipConditionBlock( ptr ) )
			Com_Printf( S_COLOR_YELLOW "WARNING: Mismatched if/endif pair in shader %s\n", shader->name );
	}
}

static void Shader_Endif( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
}

static void Shader_NoModulativeDlights( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	shader->flags |= SHADER_NO_MODULATIVE_DLIGHTS;
}

static void Shader_OffsetMappingScale( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	shader->offsetmapping_scale = Shader_ParseFloat( ptr );
	if( shader->offsetmapping_scale < 0 )
		shader->offsetmapping_scale = 0;
}

static const shaderkey_t shaderkeys[] =
{
	{ "cull", Shader_Cull },
	{ "skyparms", Shader_SkyParms },
	{ "fogparms", Shader_FogParms },
	{ "nomipmaps", Shader_shaderNoMipMaps },
	{ "nopicmip", Shader_shaderNoPicMip },
	{ "polygonoffset", Shader_PolygonOffset },
	{ "sort", Shader_Sort },
	{ "deformvertexes", Shader_DeformVertexes },
	{ "portal", Shader_Portal },
	{ "entitymergable", Shader_EntityMergable },
	{ "if",	Shader_If },
	{ "endif", Shader_Endif },
	{ "nomodulativedlights", Shader_NoModulativeDlights },
	{ "nocompress",	Shader_shaderNoCompress },
	{ "offsetmappingscale", Shader_OffsetMappingScale },
	{ NULL,	NULL }
};

// ===============================================================

static qboolean Shaderpass_LoadMaterial( image_t **normalmap, image_t **glossmap, image_t **decalmap, const char *name, int addFlags, float bumpScale )
{
	image_t *images[3];

	// set defaults
	images[0] = images[1] = images[2] = NULL;

	// load normalmap image
	images[0] = R_FindImage( name, "bump", addFlags|IT_HEIGHTMAP, bumpScale );
	if( !images[0] )
	{
		images[0] = R_FindImage( name, "norm", (addFlags|IT_NORMALMAP) & ~IT_HEIGHTMAP , 0 );

		if( !images[0] )
		{
			if( !r_lighting_diffuse2heightmap->integer )
				return qfalse;
			images[0] = R_FindImage( name, NULL, addFlags|IT_HEIGHTMAP, 2 );
			if( !images[0] )
				return qfalse;
		}
	}

	// load glossmap image
	if( r_lighting_specular->integer )
		images[1] = R_FindImage( name, "gloss", addFlags & ~IT_HEIGHTMAP, 0 );

	images[2] = R_FindImage( name, "decal", addFlags & ~IT_HEIGHTMAP, 0 );

	*normalmap = images[0];
	*glossmap = images[1];
	*decalmap = images[2];

	return qtrue;
}

static void Shaderpass_MapExt( shader_t *shader, shaderpass_t *pass, int addFlags, const char **ptr )
{
	int flags;
	char *token;

	Shader_FreePassCinematics( pass );

	token = Shader_ParseString( ptr );
	if( token[0] == '$' )
	{
		token++;
		if( !strcmp( token, "lightmap" ) )
		{
			pass->tcgen = TC_GEN_LIGHTMAP;
			pass->flags = ( pass->flags & ~( SHADERPASS_PORTALMAP|SHADERPASS_DLIGHT ) ) | SHADERPASS_LIGHTMAP;
			pass->anim_fps = 0;
			pass->anim_frames[0] = NULL;
			return;
		}
		else if( !strcmp( token, "dlight" ) )
		{
			pass->tcgen = TC_GEN_BASE;
			pass->flags = ( pass->flags & ~( SHADERPASS_LIGHTMAP|SHADERPASS_PORTALMAP ) ) | SHADERPASS_DLIGHT;
			pass->anim_fps = 0;
			pass->anim_frames[0] = NULL;
			r_shaderHasDlightPass = qtrue;
			return;
		}
		else if( !strcmp( token, "portalmap" ) || !strcmp( token, "mirrormap" ) )
		{
			pass->tcgen = TC_GEN_PROJECTION;
			pass->flags = ( pass->flags & ~( SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT ) ) | SHADERPASS_PORTALMAP;
			pass->anim_fps = 0;
			pass->anim_frames[0] = NULL;
			if( ( shader->flags & SHADER_PORTAL ) && ( shader->sort == SHADER_SORT_PORTAL ) )
				shader->sort = 0; // reset sorting so we can figure it out later. FIXME?
			shader->flags |= SHADER_PORTAL|( r_portalmaps->integer ? SHADER_PORTAL_CAPTURE : 0 );
			return;
		}
		else if( !strcmp( token, "rgb" ) )
		{
			addFlags |= IT_NOALPHA;
			token = Shader_ParseString( ptr );
		}
		else if( !strcmp( token, "alpha" ) )
		{
			addFlags |= IT_NORGB;
			token = Shader_ParseString( ptr );
		}
		else
		{
			token--;
		}
	}

	flags = Shader_SetImageFlags( shader ) | addFlags;
	pass->tcgen = TC_GEN_BASE;
	pass->flags &= ~( SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT|SHADERPASS_PORTALMAP );
	pass->anim_fps = 0;
	pass->anim_frames[0] = Shader_FindImage( shader, token, flags, 0 );
	if( !pass->anim_frames[0] )
		Com_DPrintf( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", shader->name, token );
}

static void Shaderpass_AnimMapExt( shader_t *shader, shaderpass_t *pass, int addFlags, const char **ptr )
{
	int flags;
	char *token;

	Shader_FreePassCinematics( pass );

	flags = Shader_SetImageFlags( shader ) | addFlags;

	pass->tcgen = TC_GEN_BASE;
	pass->flags &= ~( SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT|SHADERPASS_PORTALMAP );
	pass->anim_fps = Shader_ParseFloat( ptr );
	pass->anim_numframes = 0;

	for(;; )
	{
		token = Shader_ParseString( ptr );
		if( !token[0] )
			break;
		if( pass->anim_numframes < MAX_SHADER_ANIM_FRAMES )
			pass->anim_frames[pass->anim_numframes++] = Shader_FindImage( shader, token, flags, 0 );
	}

	if( pass->anim_numframes == 0 )
		pass->anim_fps = 0;
}

static void Shaderpass_CubeMapExt( shader_t *shader, shaderpass_t *pass, int addFlags, int tcgen, const char **ptr )
{
	int flags;
	char *token;

	Shader_FreePassCinematics( pass );

	token = Shader_ParseString( ptr );
	flags = Shader_SetImageFlags( shader ) | addFlags;
	pass->anim_fps = 0;
	pass->flags &= ~( SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT|SHADERPASS_PORTALMAP );

	if( !glConfig.ext.texture_cube_map )
	{
		Com_DPrintf( S_COLOR_YELLOW "Shader %s has an unsupported cubemap stage: %s.\n", shader->name );
		pass->anim_frames[0] = r_notexture;
		pass->tcgen = TC_GEN_BASE;
		return;
	}

	pass->anim_frames[0] = R_FindImage( token, NULL, flags|IT_CUBEMAP, 0 );
	if( pass->anim_frames[0] )
	{
		pass->tcgen = tcgen;
	}
	else
	{
		Com_DPrintf( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", shader->name, token );
		pass->anim_frames[0] = r_notexture;
		pass->tcgen = TC_GEN_BASE;
	}
}

static void Shaderpass_Map( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	Shaderpass_MapExt( shader, pass, 0, ptr );
}

static void Shaderpass_ClampMap( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	Shaderpass_MapExt( shader, pass, IT_CLAMP, ptr );
}

static void Shaderpass_AnimMap( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	Shaderpass_AnimMapExt( shader, pass, 0, ptr );
}

static void Shaderpass_AnimClampMap( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	Shaderpass_AnimMapExt( shader, pass, IT_CLAMP, ptr );
}

static void Shaderpass_CubeMap( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	Shaderpass_CubeMapExt( shader, pass, IT_CLAMP, TC_GEN_REFLECTION, ptr );
}

static void Shaderpass_ShadeCubeMap( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	Shaderpass_CubeMapExt( shader, pass, IT_CLAMP, TC_GEN_REFLECTION_CELLSHADE, ptr );
}

static void Shaderpass_VideoMap( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;

	Shader_FreePassCinematics( pass );

	token = Shader_ParseString( ptr );

	pass->cin = R_StartCinematics( token );
	pass->tcgen = TC_GEN_BASE;
	pass->anim_fps = 0;
	pass->flags &= ~(SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT|SHADERPASS_PORTALMAP);
}

static void Shaderpass_NormalMap( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	int flags;
	char *token;
	float bumpScale = 0;

	if( !glConfig.ext.GLSL )
	{
		Com_DPrintf( S_COLOR_YELLOW "WARNING: shader %s has a normalmap stage, while GLSL is not supported\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	Shader_FreePassCinematics( pass );

	flags = Shader_SetImageFlags( shader );
	token = Shader_ParseString( ptr );

	if( !strcmp( token, "$heightmap" ) )
	{
		flags |= IT_HEIGHTMAP;
		bumpScale = Shader_ParseFloat( ptr );
		token = Shader_ParseString( ptr );
	}

	pass->tcgen = TC_GEN_BASE;
	pass->flags &= ~( SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT|SHADERPASS_PORTALMAP );
	pass->anim_frames[1] = Shader_FindImage( shader, token, flags, bumpScale );
	if( pass->anim_frames[1] )
	{
		pass->program = DEFAULT_GLSL_PROGRAM;
		pass->program_type = PROGRAM_TYPE_MATERIAL;
	}

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "$noimage" ) )
		pass->anim_frames[0] = r_whitetexture;
	else
		pass->anim_frames[0] = Shader_FindImage( shader, token, Shader_SetImageFlags( shader ), 0 );
}

static void Shaderpass_Material( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	int flags;
	char *token;
	float bumpScale = 0;

	if( !glConfig.ext.GLSL )
	{
		Com_DPrintf( S_COLOR_YELLOW "WARNING: shader %s has a normalmap stage, while GLSL is not supported\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	Shader_FreePassCinematics( pass );

	flags = Shader_SetImageFlags( shader );
	token = Shader_ParseString( ptr );

	if( token[0] == '$' )
	{
		token++;
		if( !strcmp( token, "rgb" ) )
		{
			flags |= IT_NOALPHA;
			token = Shader_ParseString( ptr );
		}
		else if( !strcmp( token, "alpha" ) )
		{
			flags |= IT_NORGB;
			token = Shader_ParseString( ptr );
		}
		else
		{
			token--;
		}
	}

	pass->anim_frames[0] = Shader_FindImage( shader, token, flags, 0 );
	if( !pass->anim_frames[0] )
	{
		Com_DPrintf( S_COLOR_YELLOW "WARNING: failed to load base/diffuse image for material %s in shader %s.\n", token, shader->name );
		return;
	}

	pass->anim_frames[1] = pass->anim_frames[2] = pass->anim_frames[3] = NULL;

	pass->tcgen = TC_GEN_BASE;
	pass->flags &= ~( SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT|SHADERPASS_PORTALMAP );
	flags &= ~(IT_NOALPHA|IT_NORGB);

	while( 1 )
	{
		token = Shader_ParseString( ptr );
		if( !*token )
			break;

		if( Q_isdigit( token ) )
		{
			flags |= IT_HEIGHTMAP;
			bumpScale = atoi( token );
		}
		else if( !pass->anim_frames[1] )
		{
			pass->anim_frames[1] = Shader_FindImage( shader, token, flags, bumpScale );
			if( !pass->anim_frames[1] )
			{
				Com_DPrintf( S_COLOR_YELLOW "WARNING: missing normalmap image %s in shader %s.\n", token, shader->name );
				pass->anim_frames[1] = r_blankbumptexture;
			}
			else
			{
				pass->program = DEFAULT_GLSL_PROGRAM;
				pass->program_type = PROGRAM_TYPE_MATERIAL;
			}
			flags &= ~IT_HEIGHTMAP;
		}
		else if( !pass->anim_frames[2] )
		{
			if( strcmp( token, "-" ) && r_lighting_specular->integer )
			{
				pass->anim_frames[2] = Shader_FindImage( shader, token, flags, 0 );
				if( !pass->anim_frames[2] )
					Com_DPrintf( S_COLOR_YELLOW "WARNING: missing glossmap image %s in shader %s.\n", token, shader->name );
			}
			
			// set gloss to r_blacktexture so we know we have already parsed the gloss image
			if( pass->anim_frames[2] == NULL )
				pass->anim_frames[2] = r_blacktexture;
		}
		else
		{
			pass->anim_frames[3] = Shader_FindImage( shader, token, flags, 0 );
			if( !pass->anim_frames[3] )
				Com_DPrintf( S_COLOR_YELLOW "WARNING: missing decal image %s in shader %s.\n", token, shader->name );
		}
	}

	// black texture => no gloss, so don't waste time in the GLSL program
	if( pass->anim_frames[2] == r_blacktexture )
		pass->anim_frames[2] = NULL;

	if( pass->anim_frames[1] )
		return;

	// try loading default images
	if( Shaderpass_LoadMaterial( &pass->anim_frames[1], &pass->anim_frames[2], &pass->anim_frames[3], pass->anim_frames[0]->name, flags, bumpScale ) )
	{
		pass->program = DEFAULT_GLSL_PROGRAM;
		pass->program_type = PROGRAM_TYPE_MATERIAL;
	}
	else
	{
		Com_DPrintf( S_COLOR_YELLOW "WARNING: failed to load default images for material %s in shader %s.\n", pass->anim_frames[0]->name, shader->name );
	}
}

static void Shaderpass_Distortion( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	int flags;
	char *token;
	float bumpScale = 0;

	if( !glConfig.ext.GLSL || !r_portalmaps->integer )
	{
		Com_DPrintf( S_COLOR_YELLOW "WARNING: shader %s has a distortion stage, while GLSL is not supported\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	Shader_FreePassCinematics( pass );

	flags = Shader_SetImageFlags( shader );
	pass->flags &= ~( SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT|SHADERPASS_PORTALMAP );
	pass->anim_frames[0] = pass->anim_frames[1] = NULL;

	while( 1 )
	{
		token = Shader_ParseString( ptr );
		if( !*token )
			break;

		if( Q_isdigit( token ) )
		{
			flags |= IT_HEIGHTMAP;
			bumpScale = atoi( token );
		}
		else if( !pass->anim_frames[0] )
		{
			pass->anim_frames[0] = Shader_FindImage( shader, token, flags, 0 );
			if( !pass->anim_frames[0] )
			{
				Com_DPrintf( S_COLOR_YELLOW "WARNING: missing dudvmap image %s in shader %s.\n", token, shader->name );
				pass->anim_frames[0] = r_blacktexture;
			}

			pass->program = DEFAULT_GLSL_DISTORTION_PROGRAM;
			pass->program_type = PROGRAM_TYPE_DISTORTION;
		}
		else
		{
			pass->anim_frames[1] = Shader_FindImage( shader, token, flags, bumpScale );
			if( !pass->anim_frames[1] )
				Com_DPrintf( S_COLOR_YELLOW "WARNING: missing normalmap image %s in shader.\n", token, shader->name );
			flags &= ~IT_HEIGHTMAP;
		}
	}

	if( pass->rgbgen.type == RGB_GEN_UNKNOWN )
	{
		pass->rgbgen.type = RGB_GEN_CONST;
		VectorClear( pass->rgbgen.args );
	}

	shader->flags |= SHADER_PORTAL|SHADER_PORTAL_CAPTURE|SHADER_PORTAL_CAPTURE2;
}

static void Shaderpass_RGBGen( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "identitylighting" ) )
		pass->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
	else if( !strcmp( token, "identity" ) )
		pass->rgbgen.type = RGB_GEN_IDENTITY;
	else if( !strcmp( token, "wave" ) )
	{
		pass->rgbgen.type = RGB_GEN_WAVE;
		pass->rgbgen.args[0] = 1.0f;
		pass->rgbgen.args[1] = 1.0f;
		pass->rgbgen.args[2] = 1.0f;
		Shader_ParseFunc( ptr, pass->rgbgen.func );
	}
	else if( !strcmp( token, "colorwave" ) )
	{
		pass->rgbgen.type = RGB_GEN_WAVE;
		Shader_ParseVector( ptr, pass->rgbgen.args, 3 );
		Shader_ParseFunc( ptr, pass->rgbgen.func );
	}
	else if( !strcmp( token, "entity" ) )
		pass->rgbgen.type = RGB_GEN_ENTITY;
	else if( !strcmp( token, "oneminusentity" ) )
		pass->rgbgen.type = RGB_GEN_ONE_MINUS_ENTITY;
	else if( !strcmp( token, "vertex" ) )
		pass->rgbgen.type = RGB_GEN_VERTEX;
	else if( !strcmp( token, "oneminusvertex" ) )
		pass->rgbgen.type = RGB_GEN_ONE_MINUS_VERTEX;
	else if( !strcmp( token, "lightingdiffuse" ) )
		pass->rgbgen.type = RGB_GEN_LIGHTING_DIFFUSE;
	else if( !strcmp( token, "lightingdiffuseonly" ) )
		pass->rgbgen.type = RGB_GEN_LIGHTING_DIFFUSE_ONLY;
	else if( !strcmp( token, "lightingambientonly" ) )
		pass->rgbgen.type = RGB_GEN_LIGHTING_AMBIENT_ONLY;
	else if( !strcmp( token, "exactvertex" ) )
		pass->rgbgen.type = RGB_GEN_EXACT_VERTEX;
	else if( !strcmp( token, "const" ) || !strcmp( token, "constant" ) )
	{
		float div;
		vec3_t color;

		if( !r_ignorehwgamma->integer )
			div = 1.0f / pow( 2, max( 0, floor( r_overbrightbits->value ) ) );
		else
			div = 1.0f;

		pass->rgbgen.type = RGB_GEN_CONST;
		Shader_ParseVector( ptr, color, 3 );
		ColorNormalize( color, pass->rgbgen.args );
		VectorScale( pass->rgbgen.args, div, pass->rgbgen.args );
	}
	else if( !strcmp( token, "custom" ) || !strcmp( token, "teamcolor" ) )
	{
		// the "teamcolor" thing comes from warsow
		pass->rgbgen.type = RGB_GEN_CUSTOM;
		pass->rgbgen.args[0] = (int)Shader_ParseFloat( ptr );
		if( pass->rgbgen.args[0] < 0 || pass->rgbgen.args[0] >= NUM_CUSTOMCOLORS )
			pass->rgbgen.args[0] = 0;
	}
}

static void Shaderpass_AlphaGen( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "portal" ) )
	{
		pass->alphagen.type = ALPHA_GEN_PORTAL;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphagen.args[0] )
			pass->alphagen.args[0] = 256;
		pass->alphagen.args[0] = 1.0f / pass->alphagen.args[0];
	}
	else if( !strcmp( token, "vertex" ) )
		pass->alphagen.type = ALPHA_GEN_VERTEX;
	else if( !strcmp( token, "oneminusvertex" ) )
		pass->alphagen.type = ALPHA_GEN_ONE_MINUS_VERTEX;
	else if( !strcmp( token, "entity" ) )
		pass->alphagen.type = ALPHA_GEN_ENTITY;
	else if( !strcmp( token, "wave" ) )
	{
		pass->alphagen.type = ALPHA_GEN_WAVE;
		Shader_ParseFunc( ptr, pass->alphagen.func );
	}
	else if( !strcmp( token, "lightingspecular" ) )
	{
		pass->alphagen.type = ALPHA_GEN_SPECULAR;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphagen.args[0] )
			pass->alphagen.args[0] = 5.0f;
	}
	else if( !strcmp( token, "const" ) || !strcmp( token, "constant" ) )
	{
		pass->alphagen.type = ALPHA_GEN_CONST;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
	}
	else if( !strcmp( token, "dot" ) )
	{
		pass->alphagen.type = ALPHA_GEN_DOT;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		pass->alphagen.args[1] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphagen.args[1] )
			pass->alphagen.args[1] = 1.0f;
	}
	else if( !strcmp( token, "oneminusdot" ) )
	{
		pass->alphagen.type = ALPHA_GEN_ONE_MINUS_DOT;
		pass->alphagen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		pass->alphagen.args[1] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphagen.args[1] )
			pass->alphagen.args[1] = 1.0f;
	}
}

static inline int Shaderpass_SrcBlendBits( char *token )
{
	if( !strcmp( token, "gl_zero" ) )
		return GLSTATE_SRCBLEND_ZERO;
	if( !strcmp( token, "gl_one" ) )
		return GLSTATE_SRCBLEND_ONE;
	if( !strcmp( token, "gl_dst_color" ) )
		return GLSTATE_SRCBLEND_DST_COLOR;
	if( !strcmp( token, "gl_one_minus_dst_color" ) )
		return GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR;
	if( !strcmp( token, "gl_src_alpha" ) )
		return GLSTATE_SRCBLEND_SRC_ALPHA;
	if( !strcmp( token, "gl_one_minus_src_alpha" ) )
		return GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	if( !strcmp( token, "gl_dst_alpha" ) )
		return GLSTATE_SRCBLEND_DST_ALPHA;
	if( !strcmp( token, "gl_one_minus_dst_alpha" ) )
		return GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA;
	return GLSTATE_SRCBLEND_ONE;
}

static inline int Shaderpass_DstBlendBits( char *token )
{
	if( !strcmp( token, "gl_zero" ) )
		return GLSTATE_DSTBLEND_ZERO;
	if( !strcmp( token, "gl_one" ) )
		return GLSTATE_DSTBLEND_ONE;
	if( !strcmp( token, "gl_src_color" ) )
		return GLSTATE_DSTBLEND_SRC_COLOR;
	if( !strcmp( token, "gl_one_minus_src_color" ) )
		return GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR;
	if( !strcmp( token, "gl_src_alpha" ) )
		return GLSTATE_DSTBLEND_SRC_ALPHA;
	if( !strcmp( token, "gl_one_minus_src_alpha" ) )
		return GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	if( !strcmp( token, "gl_dst_alpha" ) )
		return GLSTATE_DSTBLEND_DST_ALPHA;
	if( !strcmp( token, "gl_one_minus_dst_alpha" ) )
		return GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA;
	return GLSTATE_DSTBLEND_ONE;
}

static void Shaderpass_BlendFunc( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );

	pass->flags &= ~(GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK);
	if( !strcmp( token, "blend" ) )
		pass->flags |= GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	else if( !strcmp( token, "filter" ) )
		pass->flags |= GLSTATE_SRCBLEND_DST_COLOR|GLSTATE_DSTBLEND_ZERO;
	else if( !strcmp( token, "add" ) )
		pass->flags |= GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
	else
	{
		pass->flags |= Shaderpass_SrcBlendBits( token );
		pass->flags |= Shaderpass_DstBlendBits( Shader_ParseString( ptr ) );
	}
}

static void Shaderpass_AlphaFunc( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );

	pass->flags &= ~(GLSTATE_ALPHAFUNC);
	if( !strcmp( token, "gt0" ) )
		pass->flags |= GLSTATE_AFUNC_GT0;
	else if( !strcmp( token, "lt128" ) )
		pass->flags |= GLSTATE_AFUNC_LT128;
	else if( !strcmp( token, "ge128" ) )
		pass->flags |= GLSTATE_AFUNC_GE128;
}

static void Shaderpass_DepthFunc( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );

	pass->flags &= ~GLSTATE_DEPTHFUNC_EQ;
	if( !strcmp( token, "equal" ) )
		pass->flags |= GLSTATE_DEPTHFUNC_EQ;
}

static void Shaderpass_DepthWrite( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	shader->flags |= SHADER_DEPTHWRITE;
	pass->flags |= GLSTATE_DEPTHWRITE;
}

static void Shaderpass_TcMod( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	int i;
	tcmod_t *tcmod;
	char *token;

	if( pass->numtcmods == MAX_SHADER_TCMODS )
	{
		Com_Printf( S_COLOR_YELLOW "WARNING: shader %s has too many tcmods\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	tcmod = &pass->tcmods[pass->numtcmods];

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "rotate" ) )
	{
		tcmod->args[0] = -Shader_ParseFloat( ptr ) / 360.0f;
		if( !tcmod->args[0] )
			return;
		tcmod->type = TC_MOD_ROTATE;
	}
	else if( !strcmp( token, "scale" ) )
	{
		Shader_ParseVector( ptr, tcmod->args, 2 );
		tcmod->type = TC_MOD_SCALE;
	}
	else if( !strcmp( token, "scroll" ) )
	{
		Shader_ParseVector( ptr, tcmod->args, 2 );
		tcmod->type = TC_MOD_SCROLL;
	}
	else if( !strcmp( token, "stretch" ) )
	{
		shaderfunc_t func;

		Shader_ParseFunc( ptr, &func );

		tcmod->args[0] = func.type;
		for( i = 1; i < 5; i++ )
			tcmod->args[i] = func.args[i-1];
		tcmod->type = TC_MOD_STRETCH;
	}
	else if( !strcmp( token, "transform" ) )
	{
		Shader_ParseVector( ptr, tcmod->args, 6 );
		tcmod->args[4] = tcmod->args[4] - floor( tcmod->args[4] );
		tcmod->args[5] = tcmod->args[5] - floor( tcmod->args[5] );
		tcmod->type = TC_MOD_TRANSFORM;
	}
	else if( !strcmp( token, "turb" ) )
	{
		Shader_ParseVector( ptr, tcmod->args, 4 );
		tcmod->type = TC_MOD_TURB;
	}
	else
	{
		Shader_SkipLine( ptr );
		return;
	}

	r_currentPasses[shader->numpasses].numtcmods++;
}

static void Shaderpass_TcGen( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "base" ) )
		pass->tcgen = TC_GEN_BASE;
	else if( !strcmp( token, "lightmap" ) )
		pass->tcgen = TC_GEN_LIGHTMAP;
	else if( !strcmp( token, "environment" ) )
		pass->tcgen = TC_GEN_ENVIRONMENT;
	else if( !strcmp( token, "vector" ) )
	{
		pass->tcgen = TC_GEN_VECTOR;
		Shader_ParseVector( ptr, &pass->tcgenVec[0], 4 );
		Shader_ParseVector( ptr, &pass->tcgenVec[4], 4 );
	}
	else if( !strcmp( token, "reflection" ) )
		pass->tcgen = TC_GEN_REFLECTION;
	else if( !strcmp( token, "cellshade" ) )
		pass->tcgen = TC_GEN_REFLECTION_CELLSHADE;
}

static void Shaderpass_Detail( shader_t *shader, shaderpass_t *pass, const char **ptr )
{
	pass->flags |= SHADERPASS_DETAIL;
}

static const shaderkey_t shaderpasskeys[] =
{
	{ "rgbgen", Shaderpass_RGBGen },
	{ "blendfunc", Shaderpass_BlendFunc },
	{ "depthfunc", Shaderpass_DepthFunc },
	{ "depthwrite",	Shaderpass_DepthWrite },
	{ "alphafunc", Shaderpass_AlphaFunc },
	{ "tcmod", Shaderpass_TcMod },
	{ "map", Shaderpass_Map },
	{ "animmap", Shaderpass_AnimMap },
	{ "cubemap", Shaderpass_CubeMap },
	{ "shadecubemap", Shaderpass_ShadeCubeMap },
	{ "videomap", Shaderpass_VideoMap },
	{ "clampmap", Shaderpass_ClampMap },
	{ "animclampmap", Shaderpass_AnimClampMap },
	{ "normalmap", Shaderpass_NormalMap },
	{ "material", Shaderpass_Material },
	{ "distortion",	Shaderpass_Distortion },
	{ "tcgen", Shaderpass_TcGen },
	{ "alphagen", Shaderpass_AlphaGen },
	{ "detail", Shaderpass_Detail },
	{ NULL,	NULL }
};

// ===============================================================

/*
===============
R_ShaderList_f
===============
*/
void R_ShaderList_f( void )
{
	int i;
	shader_t *shader;

	Com_Printf( "------------------\n" );
	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
		Com_Printf( " %2i %2i: %s\n", shader->numpasses, shader->sort, shader->name );
	Com_Printf( "%i shaders total\n", r_numShaders );
}

/*
===============
R_ShaderDump_f
===============
*/
void R_ShaderDump_f( void )
{
	char backup, *start;
	const char *name, *ptr;
	shadercache_t *cache;
	
	if( (Cmd_Argc() < 2) && !r_debug_surface )
	{
		Com_Printf( "Usage: %s [name]\n", Cmd_Argv(0) );
		return;
	}

	if( Cmd_Argc() < 2 )
		name = r_debug_surface->shader->name;
	else
		name = Cmd_Argv( 1 );

	Shader_GetCache( name, &cache );
	if( !cache )
	{
		Com_Printf( "Could not find shader %s in cache.\n", name );
		return;
	}

	start = cache->buffer + cache->offset;

	// temporarily hack in the zero-char
	ptr = start;
	Shader_SkipBlock( &ptr );
	backup = cache->buffer[ptr - cache->buffer];
	cache->buffer[ptr - cache->buffer] = '\0';

	Com_Printf( "Found in %s:\n\n", cache->filename );
	Com_Printf( S_COLOR_YELLOW "%s%s\n", name, start );

	cache->buffer[ptr - cache->buffer] = backup;
}

void R_InitShaders( qboolean silent )
{
	int i, numfiles;
	const char *fileptr;
	size_t filelen, shaderbuflen;

	if( !silent )
		Com_Printf( "Initializing Shaders:\n" );

	r_shadersmempool = Mem_AllocPool( NULL, "Shaders" );

	numfiles = FS_GetFileListExt( "scripts", ".shader", NULL, &shaderbuflen, 0, 0 );
	if( !numfiles )
	{
		Mem_FreePool( &r_shadersmempool );
		Com_Error( ERR_DROP, "Could not find any shaders!" );
	}

	shaderPaths = Shader_Malloc( shaderbuflen );
	FS_GetFileList( "scripts", ".shader", shaderPaths, shaderbuflen, 0, 0 );

	// now load all the scripts
	fileptr = shaderPaths;
	memset( shadercache_hash, 0, sizeof( shadercache_t * )*SHADERCACHE_HASH_SIZE );

	for( i = 0; i < numfiles; i++, fileptr += filelen + 1 )
	{
		filelen = strlen( fileptr );
		Shader_MakeCache( silent, fileptr );
	}

	if( !silent )
		Com_Printf( "--------------------------------------\n\n" );
}

static void Shader_MakeCache( qboolean silent, const char *filename )
{
	int size;
	unsigned int key;
	char *pathName = NULL;
	size_t pathNameSize;
	char *buf, *temp = NULL;
	const char *token, *ptr;
	shadercache_t *cache;
	qbyte *cacheMemBuf;
	size_t cacheMemSize;

	pathNameSize = strlen( "scripts/" ) + strlen( filename ) + 1;
	pathName = Shader_Malloc( pathNameSize );
	assert( pathName );
	Q_snprintfz( pathName, pathNameSize, "scripts/%s", filename );

	if( !silent )
		Com_Printf( "...loading '%s'\n", pathName );

	size = FS_LoadFile( pathName, ( void ** )&temp, NULL, 0 );
	if( !temp || size <= 0 )
		goto done;

	size = COM_Compress( temp );
	if( !size )
		goto done;

	buf = Shader_Malloc( size+1 );
	strcpy( buf, temp );
	FS_FreeFile( temp );
	temp = NULL;

	// calculate buffer size to allocate our cache objects all at once (we may leak
	// insignificantly here because of duplicate entries)
	for( ptr = buf, cacheMemSize = 0; ptr; )
	{
		token = COM_ParseExt( &ptr, qtrue );
		if( !token[0] )
			break;

		cacheMemSize += sizeof( shadercache_t ) + strlen( token ) + 1;
		Shader_SkipBlock( &ptr );
	}

	if( !cacheMemSize )
	{
		Shader_Free( buf );
		goto done;
	}

	cacheMemBuf = Shader_Malloc( cacheMemSize );
	memset( cacheMemBuf, 0, cacheMemSize );
	for( ptr = buf; ptr; )
	{
		token = COM_ParseExt( &ptr, qtrue );
		if( !token[0] )
			break;

		key = Shader_GetCache( token, &cache );
		if( cache )
			goto set_path_and_offset;

		cache = ( shadercache_t * )cacheMemBuf; cacheMemBuf += sizeof( shadercache_t ) + strlen( token ) + 1;
		cache->hash_next = shadercache_hash[key];
		cache->name = ( char * )( (qbyte *)cache + sizeof( shadercache_t ) );
		strcpy( cache->name, token );
		shadercache_hash[key] = cache;

set_path_and_offset:
		cache->filename = filename;
		cache->buffer = buf;
		cache->offset = ptr - buf;

		Shader_SkipBlock( &ptr );
	}

done:
	if( temp )
		FS_FreeFile( temp );
	if( pathName )
		Shader_Free( pathName );
}

static unsigned int Shader_GetCache( const char *name, shadercache_t **cache )
{
	unsigned int key;
	shadercache_t *c;

	*cache = NULL;

	key = Com_HashKey( name, SHADERCACHE_HASH_SIZE );
	for( c = shadercache_hash[key]; c; c = c->hash_next )
	{
		if( !Q_stricmp( c->name, name ) )
		{
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
	if( ( shader->flags & SHADER_SKY ) && r_skydomes[shaderNum] )
	{
		R_FreeSkydome( r_skydomes[shaderNum] );
		r_skydomes[shaderNum] = NULL;
	}

	if( shader->flags & SHADER_VIDEOMAP )
	{
		for( i = 0, pass = shader->passes; i < shader->numpasses; i++, pass++ )
			Shader_FreePassCinematics( pass );
	}

	Shader_Free( shader->name );
}

void R_ShutdownShaders( void )
{
	int i;
	shader_t *shader;

	if( !r_shadersmempool )
		return;

	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
		Shader_FreeShader( shader );

	Mem_FreePool( &r_shadersmempool );

	r_numShaders = 0;

	shaderPaths = NULL;
	memset( r_shaders, 0, sizeof( r_shaders ) );
	memset( shaders_hash, 0, sizeof( shaders_hash ) );
	memset( shadercache_hash, 0, sizeof( shadercache_hash ) );
}

void Shader_SetBlendmode( shaderpass_t *pass )
{
	int blendsrc, blenddst;

	if( pass->flags & SHADERPASS_BLENDMODE )
		return;
	if( !pass->anim_frames[0] && !( pass->flags & ( SHADERPASS_LIGHTMAP|SHADERPASS_DLIGHT ) ) )
		return;

	if( !( pass->flags & ( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ) ) )
	{
		if( ( pass->rgbgen.type == RGB_GEN_IDENTITY ) && ( pass->alphagen.type == ALPHA_GEN_IDENTITY ) )
			pass->flags |= SHADERPASS_BLEND_REPLACE;
		else
			pass->flags |= SHADERPASS_BLEND_MODULATE;
		return;
	}

	blendsrc = pass->flags & GLSTATE_SRCBLEND_MASK;
	blenddst = pass->flags & GLSTATE_DSTBLEND_MASK;

	if( blendsrc == GLSTATE_SRCBLEND_ONE && blenddst == GLSTATE_DSTBLEND_ZERO )
		pass->flags |= SHADERPASS_BLEND_MODULATE;
	else if( ( blendsrc == GLSTATE_SRCBLEND_ZERO && blenddst == GLSTATE_DSTBLEND_SRC_COLOR ) || ( blendsrc == GLSTATE_SRCBLEND_DST_COLOR && blenddst == GLSTATE_DSTBLEND_ZERO ) )
		pass->flags |= SHADERPASS_BLEND_MODULATE;
	else if( blendsrc == GLSTATE_SRCBLEND_ONE && blenddst == GLSTATE_DSTBLEND_ONE )
		pass->flags |= SHADERPASS_BLEND_ADD;
	else if( blendsrc == GLSTATE_SRCBLEND_SRC_ALPHA && blenddst == GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA )
		pass->flags |= SHADERPASS_BLEND_DECAL;
}

static void Shader_Readpass( shader_t *shader, const char **ptr )
{
	int n = shader->numpasses;
	const char *token;
	shaderpass_t *pass;

	if( n == MAX_SHADER_PASSES )
	{
		Com_Printf( S_COLOR_YELLOW "WARNING: shader %s has too many passes\n", shader->name );

		while( ptr )
		{	// skip
			token = COM_ParseExt( ptr, qtrue );
			if( !token[0] || token[0] == '}' )
				break;
		}
		return;
	}

	// Set defaults
	pass = &r_currentPasses[n];
	memset( pass, 0, sizeof( shaderpass_t ) );
	pass->rgbgen.type = RGB_GEN_UNKNOWN;
	pass->rgbgen.args = r_currentRGBgenArgs[n];
	pass->rgbgen.func = &r_currentRGBgenFuncs[n];
	pass->alphagen.type = ALPHA_GEN_UNKNOWN;
	pass->alphagen.args = r_currentAlphagenArgs[n];
	pass->alphagen.func = &r_currentAlphagenFuncs[n];
	pass->tcgenVec = r_currentTcGen[n][0];
	pass->tcgen = TC_GEN_BASE;
	pass->tcmods = r_currentTcmods[n];

	while( ptr )
	{
		token = COM_ParseExt( ptr, qtrue );

		if( !token[0] )
			break;
		else if( token[0] == '}' )
			break;
		else if( Shader_Parsetok( shader, pass, shaderpasskeys, token, ptr ) )
			break;
	}

	if( ( ( pass->flags & GLSTATE_SRCBLEND_MASK ) == GLSTATE_SRCBLEND_ONE )
		&& ( ( pass->flags & GLSTATE_DSTBLEND_MASK ) == GLSTATE_DSTBLEND_ZERO ) )
	{
		pass->flags &= ~( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK );
		pass->flags |= SHADERPASS_BLEND_MODULATE;
	}

	if( !( pass->flags & ( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ) ) )
		pass->flags |= GLSTATE_DEPTHWRITE;
	if( pass->flags & GLSTATE_DEPTHWRITE )
		shader->flags |= SHADER_DEPTHWRITE;

	switch( pass->rgbgen.type )
	{
	case RGB_GEN_IDENTITY_LIGHTING:
	case RGB_GEN_IDENTITY:
	case RGB_GEN_CONST:
	case RGB_GEN_WAVE:
	case RGB_GEN_ENTITY:
	case RGB_GEN_ONE_MINUS_ENTITY:
	case RGB_GEN_LIGHTING_DIFFUSE_ONLY:
	case RGB_GEN_LIGHTING_AMBIENT_ONLY:
	case RGB_GEN_CUSTOM:
#ifdef HARDWARE_OUTLINES
	case RGB_GEN_OUTLINE:
#endif
	case RGB_GEN_UNKNOWN:   // assume RGB_GEN_IDENTITY or RGB_GEN_IDENTITY_LIGHTING
		switch( pass->alphagen.type )
		{
		case ALPHA_GEN_UNKNOWN:
		case ALPHA_GEN_IDENTITY:
		case ALPHA_GEN_CONST:
		case ALPHA_GEN_WAVE:
		case ALPHA_GEN_ENTITY:
#ifdef HARDWARE_OUTLINES
		case ALPHA_GEN_OUTLINE:
#endif
			pass->flags |= SHADERPASS_NOCOLORARRAY;
			break;
		default:
			break;
		}

		break;
	default:
		break;
	}

	if( ( shader->flags & SHADER_SKY ) && ( shader->flags & SHADER_DEPTHWRITE ) )
	{
		if( pass->flags & GLSTATE_DEPTHWRITE )
			pass->flags &= ~GLSTATE_DEPTHWRITE;
	}

	shader->numpasses++;
}

static qboolean Shader_Parsetok( shader_t *shader, shaderpass_t *pass, const shaderkey_t *keys, const char *token, const char **ptr )
{
	const shaderkey_t *key;

	for( key = keys; key->keyword != NULL; key++ )
	{
		if( !Q_stricmp( token, key->keyword ) )
		{
			if( key->func )
				key->func( shader, pass, ptr );
			if( *ptr && **ptr == '}' )
			{
				*ptr = *ptr + 1;
				return qtrue;
			}
			return qfalse;
		}
	}

	Shader_SkipLine( ptr );

	return qfalse;
}

void Shader_SetFeatures( shader_t *s )
{
	int i;
	shaderpass_t *pass;

	if( s->numdeforms )
		s->features |= MF_DEFORMVS;
	if( s->flags & SHADER_AUTOSPRITE )
		s->features |= MF_NOCULL;

	for( i = 0; i < s->numdeforms; i++ )
	{
		switch( s->deforms[i].type )
		{
		case DEFORMV_BULGE:
			s->features |= MF_STCOORDS;
		case DEFORMV_WAVE:
		case DEFORMV_NORMAL:
			s->features |= MF_NORMALS;
			break;
		case DEFORMV_MOVE:
			break;
		default:
			break;
		}
	}

	for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ )
	{
		if( pass->program && ( pass->program_type == PROGRAM_TYPE_MATERIAL || pass->program_type == PROGRAM_TYPE_DISTORTION ) )
			s->features |= MF_NORMALS|MF_SVECTORS|MF_LMCOORDS|MF_ENABLENORMALS;

		switch( pass->rgbgen.type )
		{
		case RGB_GEN_LIGHTING_DIFFUSE:
			s->features |= MF_NORMALS;
			break;
		case RGB_GEN_VERTEX:
		case RGB_GEN_ONE_MINUS_VERTEX:
		case RGB_GEN_EXACT_VERTEX:
			s->features |= MF_COLORS;
			break;
		}

		switch( pass->alphagen.type )
		{
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

		switch( pass->tcgen )
		{
		case TC_GEN_LIGHTMAP:
			s->features |= MF_LMCOORDS;
			break;
		case TC_GEN_ENVIRONMENT:
			s->features |= MF_NORMALS;
			break;
		case TC_GEN_REFLECTION:
		case TC_GEN_REFLECTION_CELLSHADE:
			s->features |= MF_NORMALS|MF_ENABLENORMALS;
			break;
		default:
			s->features |= MF_STCOORDS;
			break;
		}
	}
}

void Shader_Finish( shader_t *s )
{
	int i, j;
	const char *oldname = s->name;
	size_t size = strlen( oldname ) + 1;
	shaderpass_t *pass;
	qbyte *buffer;

	// if the portal capture texture hasn't been initialized yet, do that
	if( ( s->flags & SHADER_PORTAL_CAPTURE ) && !r_portaltexture )
		R_InitPortalTexture( &r_portaltexture, 1, glState.width, glState.height );
	if( ( s->flags & SHADER_PORTAL_CAPTURE2 ) && !r_portaltexture2 )
		R_InitPortalTexture( &r_portaltexture2, 2, glState.width, glState.height );

	if( !s->numpasses && !s->sort )
	{
		if( s->numdeforms )
		{
			s->deforms = Shader_Malloc( s->numdeforms * sizeof( deformv_t ) );
			memcpy( s->deforms, r_currentDeforms, s->numdeforms * sizeof( deformv_t ) );
		}
		if( s->flags & SHADER_PORTAL )
			s->sort = SHADER_SORT_PORTAL;
		else
			s->sort = SHADER_SORT_ADDITIVE;
	}

	if( ( s->flags & SHADER_POLYGONOFFSET ) && !s->sort )
		s->sort = SHADER_SORT_DECAL;

	size += s->numdeforms * sizeof( deformv_t ) + s->numpasses * sizeof( shaderpass_t );
	for( i = 0, pass = r_currentPasses; i < s->numpasses; i++, pass++ )
	{
		// rgbgen args
		if( pass->rgbgen.type == RGB_GEN_WAVE ||
			pass->rgbgen.type == RGB_GEN_CONST ||
			pass->rgbgen.type == RGB_GEN_CUSTOM )
			size += sizeof( float ) * 3;

		// alphagen args
		if( pass->alphagen.type == ALPHA_GEN_PORTAL ||
			pass->alphagen.type == ALPHA_GEN_SPECULAR ||
			pass->alphagen.type == ALPHA_GEN_CONST ||
			pass->alphagen.type == ALPHA_GEN_DOT || pass->alphagen.type == ALPHA_GEN_ONE_MINUS_DOT )
			size += sizeof( float ) * 2;

		if( pass->rgbgen.type == RGB_GEN_WAVE )
			size += sizeof( shaderfunc_t );
		if( pass->alphagen.type == ALPHA_GEN_WAVE )
			size += sizeof( shaderfunc_t );
		size += pass->numtcmods * sizeof( tcmod_t );
		if( pass->tcgen == TC_GEN_VECTOR )
			size += sizeof( vec4_t ) * 2;
	}

	buffer = Shader_Malloc( size );

	s->name = ( char * )buffer; buffer += strlen( oldname ) + 1;
	s->passes = ( shaderpass_t * )buffer; buffer += s->numpasses * sizeof( shaderpass_t );

	strcpy( s->name, oldname );
	memcpy( s->passes, r_currentPasses, s->numpasses * sizeof( shaderpass_t ) );

	for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ )
	{
		if( pass->rgbgen.type == RGB_GEN_WAVE ||
			pass->rgbgen.type == RGB_GEN_CONST ||
			pass->rgbgen.type == RGB_GEN_CUSTOM )
		{
			pass->rgbgen.args = ( float * )buffer; buffer += sizeof( float ) * 3;
			memcpy( pass->rgbgen.args, r_currentPasses[i].rgbgen.args, sizeof( float ) * 3 );
		}

		if( pass->alphagen.type == ALPHA_GEN_PORTAL ||
			pass->alphagen.type == ALPHA_GEN_SPECULAR ||
			pass->alphagen.type == ALPHA_GEN_CONST ||
			pass->alphagen.type == ALPHA_GEN_DOT || pass->alphagen.type == ALPHA_GEN_ONE_MINUS_DOT )
		{
			pass->alphagen.args = ( float * )buffer; buffer += sizeof( float ) * 2;
			memcpy( pass->alphagen.args, r_currentPasses[i].alphagen.args, sizeof( float ) * 2 );
		}

		if( pass->rgbgen.type == RGB_GEN_WAVE )
		{
			pass->rgbgen.func = ( shaderfunc_t * )buffer; buffer += sizeof( shaderfunc_t );
			memcpy( pass->rgbgen.func, r_currentPasses[i].rgbgen.func, sizeof( shaderfunc_t ) );
		}
		else
		{
			pass->rgbgen.func = NULL;
		}

		if( pass->alphagen.type == ALPHA_GEN_WAVE )
		{
			pass->alphagen.func = ( shaderfunc_t * )buffer; buffer += sizeof( shaderfunc_t );
			memcpy( pass->alphagen.func, r_currentPasses[i].alphagen.func, sizeof( shaderfunc_t ) );
		}
		else
		{
			pass->alphagen.func = NULL;
		}

		if( pass->numtcmods )
		{
			pass->tcmods = ( tcmod_t * )buffer; buffer += r_currentPasses[i].numtcmods * sizeof( tcmod_t );
			pass->numtcmods = r_currentPasses[i].numtcmods;
			memcpy( pass->tcmods, r_currentPasses[i].tcmods, r_currentPasses[i].numtcmods * sizeof( tcmod_t ) );
		}

		if( pass->tcgen == TC_GEN_VECTOR )
		{
			pass->tcgenVec = ( vec_t * )buffer; buffer += sizeof( vec4_t ) * 2;
			Vector4Copy( &r_currentPasses[i].tcgenVec[0], &pass->tcgenVec[0] );
			Vector4Copy( &r_currentPasses[i].tcgenVec[4], &pass->tcgenVec[4] );
		}
	}

	if( s->numdeforms )
	{
		s->deforms = ( deformv_t * )buffer;
		memcpy( s->deforms, r_currentDeforms, s->numdeforms * sizeof( deformv_t ) );
	}

	if( s->flags & SHADER_AUTOSPRITE )
		s->flags &= ~( SHADER_CULL_FRONT|SHADER_CULL_BACK );
	if( r_shaderHasDlightPass )
		s->flags |= SHADER_NO_MODULATIVE_DLIGHTS;

	for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ )
	{
		if( pass->cin )
			s->flags |= SHADER_VIDEOMAP;
		if( pass->flags & SHADERPASS_LIGHTMAP )
			s->flags |= SHADER_LIGHTMAP;
		if( pass->program )
		{
			s->flags |= SHADER_NO_MODULATIVE_DLIGHTS;
			if( pass->program_type == PROGRAM_TYPE_MATERIAL )
				s->flags |= SHADER_MATERIAL;
			if( r_shaderHasDlightPass )
				pass->anim_frames[5] = ( (image_t *)1 ); // no dlights (HACK HACK HACK)
		}
		Shader_SetBlendmode( pass );
	}

	for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ )
	{
		if( !( pass->flags & ( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ) ) )
			break;
	}

	// all passes have blendfuncs
	if( i == s->numpasses )
	{
		int opaque;

		opaque = -1;
		for( i = 0, pass = s->passes; i < s->numpasses; i++, pass++ )
		{
			if( ( ( pass->flags & GLSTATE_SRCBLEND_MASK ) == GLSTATE_SRCBLEND_ONE )
				&& ( ( pass->flags & GLSTATE_DSTBLEND_MASK ) == GLSTATE_DSTBLEND_ZERO ) )
				opaque = i;

			if( pass->rgbgen.type == RGB_GEN_UNKNOWN )
			{
				if( !s->fog_dist && !( pass->flags & SHADERPASS_LIGHTMAP ) )
					pass->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
				else
					pass->rgbgen.type = RGB_GEN_IDENTITY;
			}

			if( pass->alphagen.type == ALPHA_GEN_UNKNOWN )
			{
				if( pass->rgbgen.type == RGB_GEN_VERTEX /* || pass->rgbgen.type == RGB_GEN_EXACT_VERTEX*/ )
					pass->alphagen.type = ALPHA_GEN_VERTEX;
				else
					pass->alphagen.type = ALPHA_GEN_IDENTITY;
			}
		}

		if( !( s->flags & SHADER_SKY ) && !s->sort )
		{
			if( s->flags & SHADER_DEPTHWRITE || ( opaque != -1 && s->passes[opaque].flags & GLSTATE_ALPHAFUNC ) )
				s->sort = SHADER_SORT_ALPHATEST;
			else if( opaque == -1 )
				s->sort = SHADER_SORT_ADDITIVE;
			else
				s->sort = SHADER_SORT_OPAQUE;
		}
	}
	else
	{
		shaderpass_t *sp;

		for( j = 0, sp = s->passes; j < s->numpasses; j++, sp++ )
		{
			if( sp->rgbgen.type == RGB_GEN_UNKNOWN )
			{
				if( sp->flags & GLSTATE_ALPHAFUNC && !( j && s->passes[j-1].flags & SHADERPASS_LIGHTMAP ) )  // FIXME!
					sp->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
				else
					sp->rgbgen.type = RGB_GEN_IDENTITY;
			}

			if( sp->alphagen.type == ALPHA_GEN_UNKNOWN )
			{
				if( sp->rgbgen.type == RGB_GEN_VERTEX /* || sp->rgbgen.type == RGB_GEN_EXACT_VERTEX*/ )
					sp->alphagen.type = ALPHA_GEN_VERTEX;
				else
					sp->alphagen.type = ALPHA_GEN_IDENTITY;
			}
		}

		if( !s->sort )
		{
			if( pass->flags & GLSTATE_ALPHAFUNC )
				s->sort = SHADER_SORT_ALPHATEST;
		}

		if( !( pass->flags & GLSTATE_DEPTHWRITE ) && !( s->flags & SHADER_SKY ) )
		{
			pass->flags |= GLSTATE_DEPTHWRITE;
			s->flags |= SHADER_DEPTHWRITE;
		}
	}

	if( !s->sort )
		s->sort = SHADER_SORT_OPAQUE;

	if( ( s->flags & SHADER_SKY ) && ( s->flags & SHADER_DEPTHWRITE ) )
		s->flags &= ~SHADER_DEPTHWRITE;

	Shader_SetFeatures( s );
}

void R_UploadCinematicShader( const shader_t *shader )
{
	int j;
	shaderpass_t *pass;

	// upload cinematics
	for( j = 0, pass = shader->passes; j < shader->numpasses; j++, pass++ )
	{
		if( pass->cin )
			pass->anim_frames[0] = R_UploadCinematics( pass->cin );
	}
}

void R_DeformvBBoxForShader( const shader_t *shader, vec3_t ebbox )
{
	int dv;

	if( !shader )
		return;
	for( dv = 0; dv < shader->numdeforms; dv++ )
	{
		switch( shader->deforms[dv].type )
		{
		case DEFORMV_WAVE:
			ebbox[0] = max( ebbox[0], fabs( shader->deforms[dv].func.args[1] ) + shader->deforms[dv].func.args[0] );
			ebbox[1] = ebbox[0];
			ebbox[2] = ebbox[0];
			break;
		default:
			break;
		}
	}
}

shader_t *R_LoadShader( const char *name, int type, qboolean forceDefault, int addFlags, int ignoreType )
{
	int i, lastDot = -1;
	unsigned int key, length;
	char shortname[MAX_QPATH];
	shader_t *s;
	shadercache_t *cache;
	shaderpass_t *pass;
	image_t *materialImages[MAX_SHADER_ANIM_FRAMES];

	if( !name || !name[0] )
		return NULL;

	if( r_numShaders == MAX_SHADERS )
		Com_Error( ERR_FATAL, "R_LoadShader: Shader limit exceeded" );

	for( i = ( name[0] == '/' || name[0] == '\\' ), length = 0; name[i] && ( length < sizeof( shortname )-1 ); i++ )
	{
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
	for( s = shaders_hash[key]; s; s = s->hash_next )
	{
		if( !strcmp( s->name, shortname ) && ( s->type != ignoreType ) )
			return s;
	}

	s = &r_shaders[r_numShaders++];
	memset( s, 0, sizeof( shader_t ) );
	s->name = shortname;
	s->offsetmapping_scale = 1;

	if( ignoreType == SHADER_UNKNOWN )
		forceDefault = qtrue;

	r_shaderNoMipMaps =	qfalse;
	r_shaderNoPicMip = qfalse;
	r_shaderNoCompress = qfalse;
	r_shaderHasDlightPass = qfalse;

	cache = NULL;
	if( !forceDefault )
		Shader_GetCache( shortname, &cache );

	// the shader is in the shader scripts
	if( cache )
	{
		const char *ptr, *token;

		Com_DPrintf( "Loading shader %s from cache...\n", name );

		// set defaults
		s->type = SHADER_UNKNOWN;
		s->flags = SHADER_CULL_FRONT;
		s->features = MF_NONE;

		ptr = cache->buffer + cache->offset;
		token = COM_ParseExt( &ptr, qtrue );

		if( !ptr || token[0] != '{' )
			goto create_default;

		while( ptr )
		{
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
	}
	else
	{           // make a default shader
		switch( type )
		{
		case SHADER_BSP_VERTEX:
			s->type = SHADER_BSP_VERTEX;
			s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS;
			s->features = MF_STCOORDS|MF_COLORS;
			s->sort = SHADER_SORT_OPAQUE;
			s->numpasses = 3;
			s->name = Shader_Malloc( length + 1 + sizeof( shaderpass_t ) * s->numpasses );
			strcpy( s->name, shortname );
			s->passes = ( shaderpass_t * )( ( qbyte * )s->name + length + 1 );
			pass = &s->passes[0];
			pass->anim_frames[0] = r_whitetexture;
			pass->flags = GLSTATE_DEPTHWRITE|SHADERPASS_BLEND_MODULATE /*|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO*/;
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
			s->name = Shader_Malloc( length + 1 + sizeof( shaderpass_t ) * s->numpasses );
			strcpy( s->name, shortname );
			s->passes = ( shaderpass_t * )( ( qbyte * )s->name + length + 1 );
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
			s->features = MF_STCOORDS|MF_NORMALS;
			s->sort = SHADER_SORT_OPAQUE;
			s->numpasses = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( shaderpass_t ) * s->numpasses );
			strcpy( s->name, shortname );
			s->passes = ( shaderpass_t * )( ( qbyte * )s->name + length + 1 );
			pass = &s->passes[0];
			pass->flags = GLSTATE_DEPTHWRITE|SHADERPASS_BLEND_MODULATE;
			pass->rgbgen.type = RGB_GEN_LIGHTING_DIFFUSE;
			pass->alphagen.type = ALPHA_GEN_IDENTITY;
			pass->tcgen = TC_GEN_BASE;
			pass->anim_frames[0] = Shader_FindImage( s, shortname, addFlags, 0 );

			// load default GLSL program if there's a bumpmap was found
			if( ( r_lighting_models_followdeluxe->integer ? mapConfig.deluxeMappingEnabled : glConfig.ext.GLSL )
				&& Shaderpass_LoadMaterial( &materialImages[0], &materialImages[1], &materialImages[2], shortname, addFlags, 1 ) )
			{
				pass->rgbgen.type = RGB_GEN_IDENTITY;
				pass->program = DEFAULT_GLSL_PROGRAM;
				pass->program_type = PROGRAM_TYPE_MATERIAL;
				pass->anim_frames[1] = materialImages[0]; // normalmap
				pass->anim_frames[2] = materialImages[1]; // glossmap
				pass->anim_frames[3] = materialImages[2]; // decalmap
				s->features |= MF_SVECTORS|MF_ENABLENORMALS;
				s->flags |= SHADER_MATERIAL;
			}
			break;
		case SHADER_2D:
			s->type = SHADER_2D;
			s->features = MF_STCOORDS|MF_COLORS;
			s->sort = SHADER_SORT_ADDITIVE;
			s->numpasses = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( shaderpass_t ) * s->numpasses );
			strcpy( s->name, shortname );
			s->passes = ( shaderpass_t * )( ( qbyte * )s->name + length + 1 );
			pass = &s->passes[0];
			pass->flags = SHADERPASS_BLEND_MODULATE|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA /* | SHADERPASS_NOCOLORARRAY*/;
			pass->anim_frames[0] = Shader_FindImage( s, shortname, IT_CLAMP|IT_NOPICMIP|IT_NOMIPMAP|addFlags, 0 );
			pass->rgbgen.type = RGB_GEN_VERTEX;
			pass->alphagen.type = ALPHA_GEN_VERTEX;
			pass->tcgen = TC_GEN_BASE;
			break;
		case SHADER_FARBOX:
			s->type = SHADER_FARBOX;
			s->features = MF_STCOORDS;
			s->sort = SHADER_SORT_SKY;
			s->flags = SHADER_SKY;
			s->numpasses = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( shaderpass_t ) * s->numpasses );
			strcpy( s->name, shortname );
			s->passes = ( shaderpass_t * )( ( qbyte * )s->name + length + 1 );
			pass = &s->passes[0];
			pass->flags = SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_MODULATE /*|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO*/;
			pass->anim_frames[0] = R_FindImage( shortname, NULL, IT_NOMIPMAP|IT_CLAMP|addFlags, 0 );
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
			s->name = Shader_Malloc( length + 1 + sizeof( shaderpass_t ) * s->numpasses );
			strcpy( s->name, shortname );
			s->passes = ( shaderpass_t * )( ( qbyte * )s->name + length + 1 );
			pass = &s->passes[0];
			pass->flags = GLSTATE_ALPHAFUNC|SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_DECAL|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			pass->anim_frames[0] = R_FindImage( shortname, NULL, IT_NOMIPMAP|IT_CLAMP|addFlags, 0 );
			pass->rgbgen.type = RGB_GEN_IDENTITY_LIGHTING;
			pass->alphagen.type = ALPHA_GEN_IDENTITY;
			pass->tcgen = TC_GEN_BASE;
			break;
		case SHADER_PLANAR_SHADOW:
			s->type = SHADER_PLANAR_SHADOW;
			s->features = MF_DEFORMVS;
			s->sort = SHADER_SORT_DECAL;
			s->flags = 0;
			s->numdeforms = 1;
			s->numpasses = 1;
			s->name = Shader_Malloc( length + 1 + s->numdeforms * sizeof( deformv_t ) + sizeof( shaderpass_t ) * s->numpasses );
			strcpy( s->name, shortname );
			s->deforms = ( deformv_t * )( ( qbyte * )s->name + length + 1 );
			s->deforms[0].type = DEFORMV_PROJECTION_SHADOW;
			s->passes = ( shaderpass_t * )( ( qbyte * )s->deforms + s->numdeforms * sizeof( deformv_t ) );
			pass = &s->passes[0];
			pass->flags = SHADERPASS_NOCOLORARRAY|SHADERPASS_STENCILSHADOW|SHADERPASS_BLEND_DECAL|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			pass->rgbgen.type = RGB_GEN_IDENTITY;
			pass->alphagen.type = ALPHA_GEN_IDENTITY;
			pass->tcgen = TC_GEN_NONE;
			break;
		case SHADER_OPAQUE_OCCLUDER:
			s->type = SHADER_OPAQUE_OCCLUDER;
			s->sort = SHADER_SORT_OPAQUE;
			s->flags = SHADER_CULL_FRONT|SHADER_DEPTHWRITE;
			s->numpasses = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( shaderpass_t ) * s->numpasses + 3 * sizeof( float ) );
			strcpy( s->name, shortname );
			s->passes = ( shaderpass_t * )( ( qbyte * )s->name + length + 1 );
			pass = &s->passes[0];
			pass->anim_frames[0] = r_whitetexture;
			pass->flags = SHADERPASS_NOCOLORARRAY|GLSTATE_DEPTHWRITE;
			pass->rgbgen.type = RGB_GEN_ENVIRONMENT;
			pass->rgbgen.args = ( float * )( ( qbyte * )s->passes + sizeof( shaderpass_t ) * s->numpasses );
			VectorClear( pass->rgbgen.args );
			pass->alphagen.type = ALPHA_GEN_IDENTITY;
			pass->tcgen = TC_GEN_NONE;
			break;
#ifdef HARDWARE_OUTLINES
		case SHADER_OUTLINE:
			s->type = SHADER_OUTLINE;
			s->features = MF_NORMALS|MF_DEFORMVS;
			s->sort = SHADER_SORT_OPAQUE;
			s->flags = SHADER_CULL_BACK|SHADER_DEPTHWRITE;
			s->numdeforms = 1;
			s->numpasses = 1;
			s->name = Shader_Malloc( length + 1 + s->numdeforms * sizeof( deformv_t ) + sizeof( shaderpass_t ) * s->numpasses );
			strcpy( s->name, shortname );
			s->deforms = ( deformv_t * )( ( qbyte * )s->name + length + 1 );
			s->deforms[0].type = DEFORMV_OUTLINE;
			s->passes = ( shaderpass_t * )( ( qbyte * )s->deforms + s->numdeforms * sizeof( deformv_t ) );
			pass = &s->passes[0];
			pass->anim_frames[0] = r_whitetexture;
			pass->flags = SHADERPASS_NOCOLORARRAY|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO|SHADERPASS_BLEND_MODULATE|GLSTATE_DEPTHWRITE;
			pass->rgbgen.type = RGB_GEN_OUTLINE;
			pass->alphagen.type = ALPHA_GEN_OUTLINE;
			pass->tcgen = TC_GEN_NONE;
			break;
#endif
		case SHADER_BSP:
		default:
create_default:
			if( mapConfig.deluxeMappingEnabled
				&& Shaderpass_LoadMaterial( &materialImages[0], &materialImages[1], &materialImages[2], shortname, addFlags, 1 ) )
			{
				s->type = SHADER_BSP;
				s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS|SHADER_LIGHTMAP|SHADER_MATERIAL;
				s->features = MF_STCOORDS|MF_LMCOORDS|MF_NORMALS|MF_SVECTORS|MF_ENABLENORMALS;
				s->sort = SHADER_SORT_OPAQUE;
				s->numpasses = 1;
				s->name = Shader_Malloc( length + 1 + sizeof( shaderpass_t ) * s->numpasses );
				strcpy( s->name, shortname );
				s->passes = ( shaderpass_t * )( ( qbyte * )s->name + length + 1 );
				pass = &s->passes[0];
				pass->flags = SHADERPASS_LIGHTMAP|SHADERPASS_DELUXEMAP|GLSTATE_DEPTHWRITE|SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_REPLACE;
				pass->tcgen = TC_GEN_BASE;
				pass->rgbgen.type = RGB_GEN_IDENTITY;
				pass->alphagen.type = ALPHA_GEN_IDENTITY;
				pass->program = DEFAULT_GLSL_PROGRAM;
				pass->program_type = PROGRAM_TYPE_MATERIAL;
				pass->anim_frames[0] = Shader_FindImage( s, shortname, addFlags, 0 );
				pass->anim_frames[1] = materialImages[0]; // normalmap
				pass->anim_frames[2] = materialImages[1]; // glossmap
				pass->anim_frames[3] = materialImages[2]; // glossmap
			}
			else
			{
				s->type = SHADER_BSP;
				s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS|SHADER_LIGHTMAP;
				s->features = MF_STCOORDS|MF_LMCOORDS;
				s->sort = SHADER_SORT_OPAQUE;
				s->numpasses = 3;
				s->name = Shader_Malloc( length + 1 + sizeof( shaderpass_t ) * s->numpasses );
				strcpy( s->name, shortname );
				s->passes = ( shaderpass_t * )( ( qbyte * )s->name + length + 1 );
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

shader_t *R_RegisterPic( const char *name )
{
	return R_LoadShader( name, SHADER_2D, qfalse, 0, SHADER_INVALID );
}

shader_t *R_RegisterShader( const char *name )
{
	return R_LoadShader( name, SHADER_BSP, qfalse, 0, SHADER_INVALID );
}

shader_t *R_RegisterSkin( const char *name )
{
	return R_LoadShader( name, SHADER_MD3, qfalse, 0, SHADER_INVALID );
}
