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
// gl_shader.c - based on code by Stephen C. Taylor
#include "gl_local.h"

static int numshaders = 0;
extern int *ids;

typedef struct {
	char name[MAX_QPATH];
	int offset;
} cache_t;

/* Maps shader keywords to functions */
static int shader_lookup(const char *name);
void Shader_Skip (char **ptr);
void Shader_Parsetok(shader_t *shader, shaderpass_t *pass, shaderkey_t *keys,
		char *token, char **ptr);
void Shader_MakeCache (void);

static void shader_makedefaults(void);
static int shader_gettexref(const char *fname);
static char *nexttok(void);
static char *nextarg(void);
static void Syntax(void);
static void shader_parsefunc(char **args, shaderfunc_t *func);

char *shaderbuf, *curpos, *endpos;
int bufsize;

shader_t	r_shaders[MAX_SHADERS];
char		r_skyname[MAX_QPATH];
shader_t	*r_skyshader;
cache_t		shadercache[MAX_SHADERS];

float r_shadertime;

/****************** shader keyword functions ************************/
static void shader_cull (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	if ( numargs != 0 ) {
		if (!Q_stricmp(args[0], "disable") || !Q_stricmp(args[0], "none")) {
			shader->cull = SHADER_CULL_DISABLE;
		} else if (!Q_stricmp (args[0], "front")) {
			shader->cull = SHADER_CULL_FRONT;
			shader->flags |= SHADER_DOCULL;
		} else if (!Q_stricmp (args[0], "back")) {
			shader->cull = SHADER_CULL_BACK;
			shader->flags |= SHADER_DOCULL;
		} else {
			shader->cull = SHADER_CULL_FRONT;
			shader->flags |= SHADER_DOCULL;
		}
	} else {
		shader->cull = SHADER_CULL_FRONT;
		shader->flags |= SHADER_DOCULL;
	}
}

static void shader_surfaceparm(shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	if (!strcmp(args[0], "nomipmaps"))
		shader->flags |= SHADER_NOMIPMAPS;
}

static void shader_skyparms(shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	Com_sprintf ( r_skyname, sizeof(r_skyname)-1, args[0] );
	R_SetSky ( r_skyname );

    shader->skyheight = atof(args[1]);

	if ( !shader->skyheight )
		shader->skyheight = 512;

	shader->flags |= SHADER_SKY;
}

static void shader_nomipmaps(shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
    shader->flags |= SHADER_NOMIPMAPS;
}

static void shader_nopicmip(shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
    shader->flags |= SHADER_NOPICMIP;
	shader->flags |= SHADER_NOMIPMAPS;
}

static void shader_deformvertexes(shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	if (shader->numdeforms >= SHADER_DEFORM_MAX)
		return;

	if (!Q_stricmp(args[0], "wave")) {
		shader->deform_params[shader->numdeforms][0] = atof (args[1]);
		if ( shader->deform_params[shader->numdeforms][0] )
			shader->deform_params[shader->numdeforms][0] = 
			1.0f / shader->deform_params[shader->numdeforms][0];
		shader->deform_vertices[shader->numdeforms] = DEFORMV_WAVE;
		shader_parsefunc(&args[2], &shader->deformv_func[shader->numdeforms]);		
		shader->deform_params[shader->numdeforms][0] *= shader->deformv_func[shader->numdeforms].args[3];
	} else if (!Q_stricmp(args[0], "normal")) {
		shader->deform_vertices[shader->numdeforms] = DEFORMV_NORMAL;
		shader->deform_params[shader->numdeforms][0] = atof (args[1]); // Div
		shader->deform_params[shader->numdeforms][1] = atof (args[2]); // base
	} else if (!Q_stricmp(args[0], "bulge")) {
		shader->deform_vertices[shader->numdeforms] = DEFORMV_BULGE;
		shader->deform_params[shader->numdeforms][0] = atof (args[1]); // Width 
		shader->deform_params[shader->numdeforms][1] = atof (args[2]); // Height
		shader->deform_params[shader->numdeforms][2] = atof (args[3]); // Speed 
	} else if (!Q_stricmp(args[0], "move")) {
		shader->deform_vertices[shader->numdeforms] = DEFORMV_MOVE;
		shader->deform_params[shader->numdeforms][0] = atof (args[1]); // x 
		shader->deform_params[shader->numdeforms][1] = atof (args[2]); // y
		shader->deform_params[shader->numdeforms][2] = atof (args[3]); // z
		shader_parsefunc(&args[4], &shader->deformv_func[shader->numdeforms]);
	} else if (!Q_stricmp (args[0], "autosprite")) {
		shader->deform_vertices[shader->numdeforms] = DEFORMV_AUTOSPRITE;
	} else if (!Q_stricmp (args[0], "autosprite2")) {
		shader->deform_vertices[shader->numdeforms] = DEFORMV_AUTOSPRITE2;
	} else {
		shader->deform_vertices[shader->numdeforms] = DEFORMV_NONE;
		Com_Printf ( S_COLOR_YELLOW "WARNING: Unknown deformv param: %s\n", args[0] );
		return;
	}

	shader->numdeforms++;
	shader->flags |= SHADER_DEFORMVERTS;
}

static void shader_fogparms (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	shader->flags |= SHADER_FOG;

	shader->fog_color[0] = (byte)(atof(args[1]) * 0.5f * 255.0f); // R
	shader->fog_color[1] = (byte)(atof(args[2]) * 0.5f * 255.0f); // G
	shader->fog_color[2] = (byte)(atof(args[3]) * 0.5f * 255.0f); // B

	shader->fog_dist = atof(args[5]); // Dist

	if ( !shader->fog_dist )
		shader->fog_dist = 128.0f;

	shader->fog_dist = 1.0f / shader->fog_dist;
}

static void shader_sort (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
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
		shader->sort = atoi(args[0]);
	}
}

static void shader_q3map (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	// Just do nothing 
}

static void shader_portal(shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	shader->sort = SHADER_SORT_PORTAL;
}

static void shader_entitymergable (shader_t *shader, shaderpass_t *pass, int numargs,
		      char **args)
{
	// TODO! (?)

}

static void shader_polygonoffset (shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	shader->flags |= SHADER_POLYGONOFFSET;
}

static shaderkey_t shaderkeys[] =
{
    {"cull", 1, 1, shader_cull},
    {"surfaceparm", 1, 1, shader_surfaceparm},
    {"skyparms", 3, 3, shader_skyparms},
	{"fogparms", 6, 6, shader_fogparms},
    {"nomipmaps", 0, 0, shader_nomipmaps},
	{"nopicmip", 0, 0, shader_nopicmip},
	{"polygonoffset", 0, 0, shader_polygonoffset },
	{"sort", 1, 1, shader_sort},
    {"deformvertexes", 1, 9, shader_deformvertexes},
	{"q3map_lightimage", 0, 9, shader_q3map},
	{"q3map_globaltexture", 0, 9, shader_q3map},
	{"q3map_surfacelight", 0, 9, shader_q3map},
	{"q3map_flare", 0, 9, shader_q3map},
	{"tesssize", 0, 9, shader_q3map},
	{"qer_editorimage", 0, 9, shader_q3map},
	{"qer_trans", 0, 9, shader_q3map},
	{"qer_nocarve", 0, 9, shader_q3map},
	{"q3map_sun", 0, 9, shader_q3map},
	{"q3map_lightsubdivide", 0, 9, shader_q3map},
	{"light", 0, 1, shader_q3map },
	{"portal", 0, 0, shader_portal},
	{"entitymergable", 0, 0, shader_entitymergable},
    {NULL, 0, 0, NULL}
};

// ===============================================================

static void shaderpass_map(shader_t *shader, shaderpass_t *pass, int numargs, char **args)
{
	int flags = 0;

	if (shader->flags & SHADER_NOMIPMAPS)
		flags |= IT_NOMIPMAP;
	if (shader->flags & SHADER_NOPICMIP)
		flags |= IT_NOPICMIP;
	if (pass->flags & SHADER_PASS_CLAMP)
		flags |= IT_CLAMP;

	if (!Q_stricmp(args[0], "$lightmap")) {
		pass->tc_gen = TC_GEN_LIGHTMAP;
		pass->flags |= SHADER_PASS_LIGHTMAP;
	} else if (!Q_stricmp (args[0], "$whiteimage")) {
		pass->tc_gen = TC_GEN_BASE;
		pass->texref = GL_FindImage ("***r_whitetexture***", flags);
	} else {
		pass->tc_gen = TC_GEN_BASE;
		pass->texref = GL_FindImage (args[0], flags);

		if ( !pass->texref ) {
			Com_Printf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n",
				shader->name, args[0] );
			pass->texref = r_notexture;
		}
    }
}

static void shaderpass_animmap (shader_t *shader, shaderpass_t *pass, int numargs,
		   char **args)
{
    int i;
	int flags = 0;

	if (shader->flags & SHADER_NOMIPMAPS)
		flags |= IT_NOMIPMAP;
	if (shader->flags & SHADER_NOPICMIP)
		flags |= IT_NOPICMIP;
	if (pass->flags & SHADER_PASS_CLAMP)
		flags |= IT_CLAMP;

	pass->tc_gen = TC_GEN_BASE;
    pass->flags |= SHADER_PASS_ANIMMAP;
    pass->anim_fps = (float)atof(args[0]);
    pass->anim_numframes = numargs - 1;

    for (i = 1; i < numargs; i++) {
		pass->anim_frames[i-1] = GL_FindImage (args[i], flags);

		if ( !pass->anim_frames[i-1] ) {
			Com_Printf ( S_COLOR_YELLOW "Shader %s has an animmap with no image: %s.\n",
				shader->name, args[i] );
			pass->anim_frames[i-1] = r_notexture;
		}
	}
}

static void
shaderpass_clampmap(shader_t *shader, shaderpass_t *pass, int numargs,
		    char **args)
{
	int flags = IT_CLAMP;

	if (shader->flags & SHADER_NOMIPMAPS)
		flags |= IT_NOMIPMAP;
	if (shader->flags & SHADER_NOPICMIP)
		flags |= IT_NOPICMIP;

	pass->flags |= SHADER_PASS_CLAMP;
	pass->tc_gen = TC_GEN_BASE;
	pass->texref = GL_FindImage (args[0], flags);

	if ( !pass->texref ) {
		Com_Printf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n",
			shader->name, args[0] );
		pass->texref = r_notexture;
	}
}

static void
shaderpass_rgbgen(shader_t *shader, shaderpass_t *pass, int numargs,
		  char **args)
{
	if (!Q_stricmp(args[0], "identitylighting")) {
		pass->rgbgen = RGB_GEN_IDENTITY_LIGHTING;
	} else if (!Q_stricmp(args[0], "identity")) {
		pass->rgbgen = RGB_GEN_IDENTITY;
	} else if (!Q_stricmp (args[0], "wave")) {
		if (numargs != 6) {
			Syntax();
			return;
		}

		shader_parsefunc(&args[1], &pass->rgbgen_func);
		pass->rgbgen = RGB_GEN_WAVE;
	} else if (!Q_stricmp(args[0], "entity")) {
		pass->rgbgen = RGB_GEN_ENTITY;
	} else if (!Q_stricmp(args[0], "oneMinusEntity")) {
		pass->rgbgen = RGB_GEN_ONE_MINUS_ENTITY;
	} else if (!Q_stricmp(args[0], "Vertex")) {
		pass->rgbgen = RGB_GEN_VERTEX;
		pass->alphagen = ALPHA_GEN_VERTEX;
	} else if (!Q_stricmp(args[0], "oneMinusVertex")) {
		pass->rgbgen = RGB_GEN_ONE_MINUS_VERTEX;
	} else if (!Q_stricmp(args[0], "lightingDiffuse")) {
		pass->rgbgen = RGB_GEN_LIGHTING_DIFFUSE;
	} else if (!Q_stricmp (args[0], "exactvertex")) {
		pass->rgbgen = RGB_GEN_EXACT_VERTEX;
	} else {
		Com_Printf ( S_COLOR_YELLOW "WARNING: Unknown rgbgen param: %s\n", args[0]);
	}
}

static void shaderpass_blendfunc(shader_t *shader, shaderpass_t *pass, int numargs,
		     char **args)
{
	if ( numargs == 1 )
    {
		if (!strcmp(args[0], "blend")) {
			pass->blendsrc = GL_SRC_ALPHA;
			pass->blenddst = GL_ONE_MINUS_SRC_ALPHA;
		} else if (!strcmp(args[0], "filter")) {
			pass->blendsrc = GL_DST_COLOR;
			pass->blenddst = GL_ZERO;
		} else if (!strcmp(args[0], "add")) {
			pass->blendsrc = pass->blenddst = GL_ONE;
		} else {
			Syntax();
			return;
		}
	} else {
		int i;
		unsigned int *blend;

		for (i = 0; i < 2; i++)
		{
			blend = (i == 0) ? &pass->blendsrc : &pass->blenddst;

			if (!strcmp(args[i], "gl_zero"))
				*blend = GL_ZERO;
			else if (!strcmp(args[i], "gl_one"))
				*blend = GL_ONE;
			else if (!strcmp(args[i], "gl_dst_color"))
				*blend = GL_DST_COLOR;
			else if (!strcmp(args[i], "gl_one_minus_src_alpha"))
				*blend = GL_ONE_MINUS_SRC_ALPHA;
			else if (!strcmp(args[i], "gl_src_alpha"))
				*blend = GL_SRC_ALPHA;
			else if (!strcmp(args[i], "gl_src_color"))
				*blend = GL_SRC_COLOR;
			else if (!strcmp(args[i], "gl_one_minus_dst_color"))
				*blend = GL_ONE_MINUS_DST_COLOR;
			else if (!strcmp(args[i], "gl_one_minus_src_color"))
				*blend = GL_ONE_MINUS_SRC_COLOR;
			else if (!strcmp(args[i], "gl_dst_alpha"))
				*blend = GL_DST_ALPHA;
			else if (!strcmp(args[i], "gl_one_minus_dst_alpha"))
				*blend = GL_ONE_MINUS_DST_ALPHA;
			else {
				Syntax();
				return;
			}
		}
    }

	pass->flags |= SHADER_PASS_BLEND;
}

static void
shaderpass_depthfunc(shader_t *shader, shaderpass_t *pass, int numargs,
		     char **args)
{
    if (!strcmp(args[0], "equal"))
		pass->depthfunc = GL_EQUAL;
	// FIXME: is this needed?
    else if (!strcmp(args[0], "lequal"))
		pass->depthfunc = GL_LEQUAL;
    else
		Syntax();
}

static void
shaderpass_depthwrite(shader_t *shader, shaderpass_t *pass, int numargs,
		      char **args)
{
    // FIXME: Why oh why is depthwrite enabled in the sky shaders ????
	if (shader->flags & SHADER_SKY)
		return;
    
    shader->flags |= SHADER_DEPTHWRITE;
    pass->flags |= SHADER_PASS_DEPTHWRITE;
}

static void shaderpass_alphafunc(shader_t *shader, shaderpass_t *pass, int numargs,
		     char **args)
{
    if (!Q_stricmp(args[0], "gt0")) {
		pass->alphafunc = GL_GREATER;
		pass->alphafuncref = 0.0f;
	} else if (!Q_stricmp (args[0], "lt128")) {
		pass->alphafunc = GL_LESS;
		pass->alphafuncref = 0.5f;
	} else if (!Q_stricmp(args[0], "ge128")) {
		pass->alphafunc = GL_GEQUAL;
		pass->alphafuncref = 0.5f;
    } else {
		Com_Printf ( S_COLOR_YELLOW "WARNING: Unknown alphafunc param: %s\n", args[0]);
		return;
	}

    pass->flags |= SHADER_PASS_ALPHAFUNC;
}

static void shaderpass_tcmod(shader_t *shader, shaderpass_t *pass, int numargs,
		 char **args)
{
	if (pass->num_tc_mod >= SHADER_TCMOD_MAX) {
		Com_Printf ( S_COLOR_YELLOW "WARNING: SHADERTCMOD_MAX exceeded!");
		return;
	}

	if (!Q_stricmp(args[0], "rotate")) {
		pass->tc_mod[pass->num_tc_mod].type = SHADER_TCMOD_ROTATE;
		pass->tc_mod[pass->num_tc_mod].args[0] = -atof(args[1]);
	} else if (!Q_stricmp (args[0], "scale")) {
		if (numargs != 3) {
			Syntax();
			pass->tc_mod[pass->num_tc_mod].type = -1;
			return;
		}

		pass->tc_mod[pass->num_tc_mod].type = SHADER_TCMOD_SCALE;
		pass->tc_mod[pass->num_tc_mod].args[0] = atof(args[1]);
		pass->tc_mod[pass->num_tc_mod].args[1] = atof(args[2]);
	} else if (!Q_stricmp(args[0], "scroll")) {
		if (numargs != 3) {
			Syntax();
			pass->tc_mod[pass->num_tc_mod].type = -1;
			return;
		}

		pass->tc_mod[pass->num_tc_mod].type = SHADER_TCMOD_SCROLL;
		pass->tc_mod[pass->num_tc_mod].args[0] = atof (args[1]);
		pass->tc_mod[pass->num_tc_mod].args[1] = atof (args[2]);

	} else if (!Q_stricmp (args[0], "stretch")) {
		if (numargs != 6) {
			Syntax();
			pass->tc_mod[pass->num_tc_mod].type = -1;
			return;
		}

		pass->tc_mod[pass->num_tc_mod].type = SHADER_TCMOD_STRETCH;
		shader_parsefunc(&args[1], &pass->tc_mod_stretch);
	} else if (!Q_stricmp (args[0], "transform")) {
		int i;

		if (numargs != 7) { 
			Syntax();
			pass->tc_mod[pass->num_tc_mod].type = -1;
			return;
		}

		pass->tc_mod[pass->num_tc_mod].type = SHADER_TCMOD_TRANSFORM;

		for (i = 0; i < 6; ++i)
			pass->tc_mod[pass->num_tc_mod].args[i] = atof(args[i + 1]);
	} else if (!Q_stricmp (args[0], "turb")) {
		int i, a1 = 0;

		if (numargs == 5)
			a1 = 1;
		else if (numargs == 6)
			a1 = 2;
		else {
			Syntax();
			pass->tc_mod[pass->num_tc_mod].type = -1;
			return;
		}

		pass->tc_mod[pass->num_tc_mod].type = SHADER_TCMOD_TURB;

		for (i = 0; i < 4; i++)
			pass->tc_mod[pass->num_tc_mod].args[i] = atof(args[i+a1]);
	} else 	{
		Com_Printf ( S_COLOR_YELLOW "WARNING: Unknown tc_mod: %s\n", args[0]);
		return;
	}

    pass->flags |= SHADER_PASS_TCMOD;
	pass->num_tc_mod++;
}


static void shaderpass_tcgen(shader_t *shader, shaderpass_t *pass, int numargs,
		 char **args)
{
	if (!Q_stricmp(args[0], "base")) {
		pass->tc_gen = TC_GEN_BASE;
	} else if (!Q_stricmp(args[0], "lightmap")) {
		pass->tc_gen = TC_GEN_LIGHTMAP;
	} else if (!Q_stricmp(args[0], "environment")) {
		pass->tc_gen = TC_GEN_ENVIRONMENT;
	} else if (!Q_stricmp(args[0], "vector")) {
		pass->tc_gen = TC_GEN_VECTOR;

		pass->tc_gen_s[0] = atof(args[1]);
		pass->tc_gen_s[1] = atof(args[2]);
		pass->tc_gen_s[2] = atof(args[3]);
		pass->tc_gen_t[0] = atof(args[4]);
		pass->tc_gen_t[1] = atof(args[5]);
		pass->tc_gen_t[2] = atof(args[6]);
	} else {
		Com_Printf ( S_COLOR_YELLOW "WARNING: Unknown tcgenparam: %s\n", args[0]);
	}
}

static void shaderpass_alphagen (shader_t *shader, shaderpass_t *pass, int numargs,
		 char **args)
{
	if (!Q_stricmp (args[0], "portal")) {
		pass->alphagen = ALPHA_GEN_PORTAL;
	} else if (!Q_stricmp (args[0], "vertex")) {
		pass->alphagen = ALPHA_GEN_VERTEX;
	} else if (!Q_stricmp (args[0], "entity")) {
		pass->alphagen = ALPHA_GEN_ENTITY;
	} else if (!Q_stricmp (args[0], "wave")) {

		if (numargs != 6) {
			Syntax();
			return;
		}

		shader_parsefunc(&args[1], &pass->alphagen_func);
		pass->alphagen = ALPHA_GEN_WAVE;
	} else if (!Q_stricmp (args[0], "lightingspecular")) {
		pass->alphagen = ALPHA_GEN_SPECULAR;
	} else {
		pass->alphagen = ALPHA_GEN_IDENTITY;
		Com_Printf ( S_COLOR_YELLOW "WARNING: Unknown alphagen param: %s\n", args[0]);
	}
}

static void shaderpass_detail (shader_t *shader, shaderpass_t *pass, int numargs,
		 char **args)
{
	pass->flags |= SHADER_PASS_DETAIL;
}

static shaderkey_t shaderpasskeys[] =
{
    {"map", 1, 1, shaderpass_map},
    {"rgbgen", 1, 6, shaderpass_rgbgen},
    {"blendfunc", 1, 2, shaderpass_blendfunc},
    {"depthfunc", 1, 1, shaderpass_depthfunc},
    {"depthwrite", 0, 0, shaderpass_depthwrite},
    {"alphafunc", 1, 1, shaderpass_alphafunc},
    {"tcmod", 2, 7, shaderpass_tcmod},
    {"animmap", 3, SHADER_ARGS_MAX, shaderpass_animmap},
    {"clampmap", 1, 1, shaderpass_clampmap},
    {"tcgen", 1, 10, shaderpass_tcgen},
	{"alphagen", 0, 10, shaderpass_alphagen},
	{"detail", 0, 0, shaderpass_detail},

    {NULL, 0, 0, NULL}
};

// ===============================================================

qboolean Shader_Init (void)
{
	int i, size, dirlen, numdirs;
	char dirlist[MAX_QPATH * 256];
	char loadedlist[MAX_QPATH * 256];
	char *dirptr, *pscripts, *buf;
	char filename[MAX_QPATH];

	Com_Printf ( "Initializing Shaders:\n");

	numdirs = FS_GetFileList ("scripts", "shader", dirlist, 256 * MAX_QPATH);

	if (!numdirs) {
		Com_Error ( ERR_DROP, "Could not find any shaders!");
		return false;
	}

	memset (filename, 0, MAX_QPATH);
	memset (loadedlist, 0, MAX_QPATH * 256);

	// find the size of all shader scripts
	dirptr = dirlist;
	bufsize = 0;
	for (i = 0; i < numdirs; i++, dirptr += dirlen+1) {
		dirlen = strlen(dirptr);

		if (!dirlen)
			continue;
		if (strstr (loadedlist, va(";%s", dirptr) ))
			continue;

		Com_sprintf (filename, sizeof(filename), "scripts/%s", dirptr);
		strcat (loadedlist, va(";%s", dirptr) );
		bufsize += FS_FileSize (filename) + 1;
	}

	// space for the terminator
	bufsize++;

	// allocate the memory
	pscripts = shaderbuf = (char * )Q_malloc (bufsize);

	memset (filename, 0, MAX_QPATH);
	memset (loadedlist, 0, MAX_QPATH*256);

	// now load all the scripts
	dirptr = dirlist;
	for (i=0; i<numdirs; i++, dirptr += dirlen+1) {
		dirlen = strlen(dirptr);

		if (!dirlen)
			continue;
		if (strstr (loadedlist, va(";%s", dirptr) ))
			continue;

		Com_sprintf( filename, sizeof(filename), "scripts/%s", dirptr );
		strcat (loadedlist, va(";%s", dirptr) );

		size = FS_LoadFile ( filename, &buf );
		if ( !buf || !size ) continue;

		Com_Printf ( "...loading '%s'\n", filename );

		memcpy ( pscripts, buf, size );

		FS_FreeFile ( buf );
		pscripts += size;

		// make sure there's a whitespace between two files
		*(pscripts++) = '\n';
	}

	// terminate this big string
	*pscripts = 0;

	Shader_MakeCache();

	return true;
}

void Shader_MakeCache (void)
{
	char *ptr;
	char *token;
	int i;

	memset (r_shaders, 0, sizeof(shader_t)*MAX_SHADERS);
	memset (shadercache, 0, sizeof(cache_t)*MAX_SHADERS);

	ptr = shaderbuf;
	numshaders = 0;

	while (ptr)
	{
		token = COM_ParseExt (&ptr, true);
		if (!token[0]) continue;
		Shader_Skip (&ptr);
		
		numshaders++;
	}

	ptr = shaderbuf;
	i = 0;

	while (ptr)
	{
		token = COM_ParseExt (&ptr, true);

		if (!ptr)
			break;
		if (!token[0]) 
			continue;

		shadercache[i].offset = ptr - shaderbuf;

		if (shadercache[i].offset > bufsize)
			shadercache[i].offset = -1;

		strncpy (shadercache[i].name, token, MAX_QPATH);

		Shader_Skip (&ptr);	
		i++;
	}
}

void Shader_Skip (char **ptr)
{	
	char *tok, *tmp;
    int brace_count;

    // Opening brace
    tok = COM_ParseExt (ptr, true);
	
	if (!ptr) 
		return;
    
	if (tok[0] != '{') 
	{
		tok = COM_ParseExt (ptr, true);
	}

	tmp = *ptr;

    for (brace_count = 1; brace_count > 0 ; tmp++)
    {
		if (!tmp[0]) 
			break;

		if (tmp[0] == '{')
			brace_count++;
		else if (tmp[0] == '}')
			brace_count--;
    }

	*ptr = tmp;
}

int Shader_GetOffset (char *name)
{
	int i;

	// HASHTABLE !!!
	for (i = 0; i < numshaders; i++)
	{
		if (!Q_stricmp (shadercache[i].name, name))
			return shadercache[i].offset;
	}

	return -1;
}

void Shader_Shutdown (void)
{
	free (shaderbuf);
	memset (r_shaders, 0, sizeof(shader_t)*MAX_SHADERS);
	memset (shadercache, 0, sizeof(cache_t)*MAX_SHADERS);
}

void Shader_Readpass (shader_t *shader, char **ptr)
{
    char *token;
	shaderpass_t *pass;
	shaderpass_t dummy;

	if ( shader->numpasses >= SHADER_PASS_MAX ) {
		pass = &dummy;
	} else {
		pass = &shader->pass[shader->numpasses++];
	}
	
    // Set defaults
    pass->flags = 0;
    pass->texref = NULL;
	pass->anim_numframes = 0;
    pass->depthfunc = GL_LEQUAL;
    pass->rgbgen = RGB_GEN_IDENTITY;
	pass->alphagen = ALPHA_GEN_IDENTITY;
	pass->tc_gen = TC_GEN_BASE;
	pass->num_tc_mod = 0;
	
	while (ptr)
	{
		token = COM_ParseExt (ptr, true);
		
		if (!token[0]) continue;

		if (token[0] == '}')
			break;
		else {
			Shader_Parsetok (shader, pass, shaderpasskeys, token, ptr);
		}
	}

	// check some things 
	if ( (pass->blendsrc == GL_ONE) && (pass->blenddst == GL_ZERO) ) {
		pass->blendsrc = 0;
		pass->blenddst = 0;
		pass->flags &= ~SHADER_PASS_BLEND;
//		pass->flags |= SHADER_PASS_DEPTHWRITE;
	}
}

void Shader_Parsetok (shader_t *shader, shaderpass_t *pass, shaderkey_t *keys, char *token, char **ptr)
{
    shaderkey_t *key;
    char *c, *args[SHADER_ARGS_MAX];
	static char buf[SHADER_ARGS_MAX][128];
    int numargs;

	for (key = keys; key->keyword != NULL; key++)
	{
		if (!stricmp (token, key->keyword))
		{
			numargs = 0;

			while (ptr)
			{
				c = COM_ParseExt (ptr, false);

				if (!c[0]) // NEW LINE 
					break;

				c = strlwr(c); // Lowercase ( FIXME !)
				
				strcpy(buf[numargs], c);
				args[numargs] = buf[numargs++];
			}

			if (numargs < key->minargs || numargs > key->maxargs) {
				Syntax();
				continue;
			}

			if (key->func)
				key->func (shader, pass, numargs, args);

			return;
		}
	}

	// we could not find the keyword
	Com_Printf ( S_COLOR_YELLOW "Shader_Parsetok: Unknown keyword: %s\n", token);
   
	// Next Line
	while (ptr)
	{
		token = COM_ParseExt (ptr, false);
		if (!token[0])
			break;
	}
}

void Shader_SetBlendmode ( shaderpass_t *pass )
{
	if ( !(pass->flags & SHADER_PASS_BLEND ) ) {
		pass->blendmode = GL_REPLACE;
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

void Shader_Finish ( shader_t *s )
{
	int i;
	shaderpass_t *pass;

	if ( !s->numpasses ) {
		return;
	}

	// kill lightmap pass
	if ( r_vertexlight->value ) {
		pass = s->pass;
		for ( i = 0; i < s->numpasses; i++, pass++ ) {
			if ( pass->flags & SHADER_PASS_LIGHTMAP )
				break;
		}

		if ( i < s->numpasses ) {
			int j;

			for ( j = 0; j < s->numpasses; j++ ) {
				if ( !s->pass[j].num_tc_mod && j != i &&
					s->pass[j].tc_gen == TC_GEN_BASE )
					break;
			}

			if ( j == s->numpasses ) {
				s->pass[i+1].blendsrc = 0;
				s->pass[i+1].blenddst = 0;
				s->pass[i+1].flags &= ~SHADER_PASS_BLEND;
				s->pass[i+1].flags |= SHADER_PASS_DEPTHWRITE;
				s->pass[i+1].rgbgen = RGB_GEN_VERTEX;

				for ( ; i < s->numpasses - 1; i++, pass++ ) {
					memcpy ( pass, &s->pass[i+1], sizeof(shaderpass_t) );
				}
			} else {
				s->pass[j].blendsrc = 0;
				s->pass[j].blenddst = 0;
				s->pass[j].flags &= ~SHADER_PASS_BLEND;
				s->pass[j].flags |= SHADER_PASS_DEPTHWRITE;
				s->pass[j].rgbgen = RGB_GEN_VERTEX;

				memcpy ( &s->pass[0], &s->pass[j], sizeof(shaderpass_t) );
				s->numpasses = 1;
			}
		}
	}

	pass = s->pass;
	for ( i = 0; i < s->numpasses; i++, pass++ )
		Shader_SetBlendmode ( pass );

	if (s->numpasses == 2)
	{
		if ( gl_config.mtexcombine )	// check if we can use SHADER_FLUSH_MULTITEXTURE_COMBINE
		{
			if ( s->pass[0].blendmode == GL_REPLACE )
				if ( ( s->pass[1].blendmode && s->pass[1].blendmode != GL_ADD ) ||
					( s->pass[1].blendmode == GL_ADD && gl_config.env_add ) || 
					gl_config.nvtexcombine4 ) {
					s->flush = SHADER_FLUSH_MULTITEXTURE_COMBINE;
				}
		} else if ( qglMTexCoord2fSGIS ) {	// check if we can use SHADER_FLUSH_MULTITEXTURE_2
			if ( s->pass[0].blendmode == GL_REPLACE ) {
				if ( s->pass[1].blendmode == GL_ADD ) {
					if ( gl_config.env_add )
						s->flush = SHADER_FLUSH_MULTITEXTURE_2;
				} else if ( s->pass[1].blendmode && s->pass[1].blendmode != GL_DECAL && s->pass[1].blendmode != GL_REPLACE ) {
					s->flush = SHADER_FLUSH_MULTITEXTURE_2;
				}
			}
		}
	}

	pass = s->pass;
	for ( i = 0; i < s->numpasses; i++, pass++ ) {
		if (!(pass->flags & SHADER_PASS_BLEND)) {
			break;
		}
	}

	// all passes have blendfuncs
	if ( i == s->numpasses ) {
		if ( s->cull == SHADER_CULL_FRONT ) {
			s->cull = SHADER_CULL_DISABLE;
			s->flags &= ~SHADER_DOCULL;
		}

		if ( !( s->flags & SHADER_SKY ) )
			s->sort = SHADER_SORT_ADDITIVE;
	}

	// TODO: check if we can use R_BackendFlushMultitextur_Combine
		
	if ( 
	!( s->flags & SHADER_DEPTHWRITE ) &&
	!( s->flags & SHADER_SKY ) && 
	 ( i != s->numpasses )
	)
	{
		pass = s->pass;
		for  (i = 0; i < s->numpasses; i++, pass++ ) {
			if (!(pass->flags & SHADER_PASS_BLEND)) {
				pass->flags |= SHADER_PASS_DEPTHWRITE;
				s->flags |= SHADER_DEPTHWRITE;
				break;
			}
		}
	}

	s->sortkey = (s->flags & SHADER_POLYGONOFFSET) + (s->cull << 1) + ( s->flush << 4 );
}

void Shader_UpdateRegistration (void)
{
	int i, j, l;
	shader_t *shader;
	shaderpass_t *pass;
	extern shader_t	*chars_shader;

	chars_shader->registration_sequence = registration_sequence;

	r_skyshader = NULL;
	shader = r_shaders;
	for (i = 0; i < MAX_SHADERS; i++, shader++)
	{
		if ( !shader->registration_sequence )
			continue;

		if ( shader->registration_sequence != registration_sequence )
		{
			memset ( shader, 0, sizeof( shader_t ) );
			continue;
		}

		if ( shader->flags & SHADER_SKY && !r_skyshader)
			r_skyshader = shader;

		pass = shader->pass;
		
		for (j = 0; j < shader->numpasses; j++, pass++)
		{
			if (pass->flags & SHADER_PASS_ANIMMAP)
			{
				for (l = 0; l < pass->anim_numframes; l++) 
				{
					if ( pass->anim_frames[l] )
						pass->anim_frames[l]->registration_sequence = registration_sequence;
				}
			} 
			else if ( !(pass->flags & SHADER_PASS_LIGHTMAP) )
			{
				if ( pass->texref )
					pass->texref->registration_sequence = registration_sequence;
			}
		}
	}

	if ( !r_skyshader ) {
		R_SetSky ( "-" );
	} else {
		R_SetSky ( r_skyname );
	}
}

int R_LoadShader (char *name, int type, msurface_t *surf)
{
	char *ptr;
	int offset, i, f = -1;
	shader_t *s;

	// test if already loaded
	for (i = 0; i < MAX_SHADERS; i++)
	{
		if (!Q_stricmp (name, r_shaders[i].name))
		{
			r_shaders[i].registration_sequence = registration_sequence;
			return i;
		}

		// find a free shader
		if ( f == -1 )
			if ( !r_shaders[i].registration_sequence )
				f = i;
	}

	if ( f == -1 )
	{
		Com_Error (ERR_FATAL, "R_LoadShader: Shader limit exceeded.\n");
		return f;
	}

	s = &r_shaders[f];
	Com_sprintf ( s->name, MAX_QPATH, name );

	offset = Shader_GetOffset(name);

	// the shader is in the shader scripts
	if (offset > -1 && (offset < bufsize))
	{
		char *token;
		ptr = shaderbuf + offset;

		if (!ptr)
			return -1;

		// set defaults
		s->flags = 0;
		s->sort = 0;
		s->numpasses = 0;
		s->deform_vertices[0] = DEFORMV_NONE;
		s->skyheight = 512.0f;
		s->numdeforms = 0;
		s->flush = SHADER_FLUSH_GENERIC;
		s->cull = SHADER_CULL_DISABLE;
		s->registration_sequence = registration_sequence;

		token = COM_ParseExt (&ptr, true);

		if (!ptr || token[0] != '{')
			return -1;

		while (ptr)
		{
			token = COM_ParseExt (&ptr, true);

			if (!token[0])
				continue;

			if (token[0] == '{') {
				Shader_Readpass (s, &ptr);
			} else if (token[0] == '}') {
				break;
			} else {
				Shader_Parsetok (s, NULL, shaderkeys, token, &ptr);
			}
		}

		Shader_Finish(s);
	}
	else		// make a default shader
	{
		switch (type)
		{
		case SHADER_2D:
			s->flags = SHADER_NOPICMIP|SHADER_NOMIPMAPS;
			s->numpasses = 1;
			s->pass[0].flags = SHADER_PASS_BLEND;
			s->pass[0].blendsrc = GL_SRC_ALPHA;
			s->pass[0].blenddst = GL_ONE_MINUS_SRC_ALPHA;
			s->pass[0].blendmode = GL_MODULATE;
			s->pass[0].texref = GL_FindImage (name, IT_NOPICMIP|IT_NOMIPMAP);
			s->pass[0].depthfunc = GL_ALWAYS;
			s->pass[0].rgbgen = RGB_GEN_VERTEX;
			s->pass[0].num_tc_mod = 0;
			s->pass[0].tc_gen = TC_GEN_BASE;
			s->sort = SHADER_SORT_ADDITIVE;
			s->deform_vertices[0] = DEFORMV_NONE;
			s->numdeforms = 0;

			if ( !s->pass[0].texref ) {
				Com_Printf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", s->name, name );
				s->pass[0].texref = r_notexture;
			}

			s->flush = SHADER_FLUSH_GENERIC;
			s->cull = SHADER_CULL_DISABLE;
			s->registration_sequence = registration_sequence;
			break;

		case SHADER_BSP:
			s->flags = SHADER_DEPTHWRITE;

			if ( !surf || surf->mesh.lightmaptexturenum == -1 ||
				r_vertexlight->value ) {
				s->numpasses = 1;
				s->pass[0].tc_gen = TC_GEN_BASE;
				s->pass[0].texref = GL_FindImage (name, 0);
				s->pass[0].alphagen = ALPHA_GEN_IDENTITY;
				s->pass[0].depthfunc = GL_LEQUAL;
				s->pass[0].flags = SHADER_PASS_DEPTHWRITE;
				s->pass[0].rgbgen = RGB_GEN_VERTEX;
				s->pass[0].blendmode = GL_REPLACE;

				if ( !s->pass[0].texref ) {
					Com_Printf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", s->name, name );
					s->pass[0].texref = r_notexture;
				}

				s->flush = SHADER_FLUSH_GENERIC;
			} else 	{
				s->numpasses = 2;

				s->pass[0].flags = SHADER_PASS_LIGHTMAP | SHADER_PASS_BLEND | SHADER_PASS_DEPTHWRITE;
				s->pass[0].tc_gen = TC_GEN_LIGHTMAP;
				s->pass[0].texref = NULL;
				s->pass[0].depthfunc = GL_LEQUAL;
				s->pass[0].blendmode = GL_REPLACE;
    			s->pass[0].alphagen = ALPHA_GEN_IDENTITY;
				s->pass[0].rgbgen = RGB_GEN_IDENTITY;

				s->pass[1].flags = 0;
				s->pass[1].tc_gen = TC_GEN_BASE;
				s->pass[1].texref = GL_FindImage (name, 0);
				s->pass[1].blendsrc = GL_DST_COLOR;
				s->pass[1].blenddst = GL_ZERO;
				s->pass[1].blendmode = GL_MODULATE;
				s->pass[1].depthfunc = GL_LEQUAL;
				s->pass[1].rgbgen = RGB_GEN_IDENTITY;
    			s->pass[1].alphagen = ALPHA_GEN_IDENTITY;

				if ( !s->pass[1].texref ) {
					Com_Printf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n", s->name, name );
					s->pass[1].texref = r_notexture;
				}

				if ( gl_config.mtexcombine )
					s->flush = SHADER_FLUSH_MULTITEXTURE_COMBINE;
				else if ( qglMTexCoord2fSGIS )
					s->flush = SHADER_FLUSH_MULTITEXTURE_2;
				else
					s->flush = SHADER_FLUSH_GENERIC;
			}

		    s->sort = SHADER_SORT_OPAQUE;
			s->deform_vertices[0] = DEFORMV_NONE;
			s->numdeforms = 0;
			s->cull = SHADER_CULL_FRONT;
			s->registration_sequence = registration_sequence;
			break;

		case SHADER_MD3:
			s->flags = SHADER_DEPTHWRITE;
			s->numpasses = 1;
			s->pass[0].flags = SHADER_PASS_DEPTHWRITE;
			s->pass[0].texref = GL_FindImage (name, IT_FLOODFILL);
			s->pass[0].depthfunc = GL_LESS;
			s->pass[0].rgbgen = RGB_GEN_LIGHTING_DIFFUSE;
			s->pass[0].num_tc_mod = 0;
			s->pass[0].tc_gen = TC_GEN_BASE;
			s->pass[0].blendmode = GL_REPLACE;
			s->sort = SHADER_SORT_OPAQUE;
			s->deform_vertices[0] = DEFORMV_NONE;
			s->numdeforms = 0;

			if ( !s->pass[0].texref ) {
				Com_Printf ( S_COLOR_YELLOW "Shader %s has a stage with no image: %s.\n",
					s->name, name );
				s->pass[0].texref = r_notexture;
			}

			s->flush = SHADER_FLUSH_GENERIC;
			s->cull = SHADER_CULL_FRONT;
			s->registration_sequence = registration_sequence;
			break;

		default:
			return -1;
		}
	}

	return f;
}

void Syntax(void)
{
    Com_Printf ( S_COLOR_YELLOW "Syntax error\n");
}

static void shader_parsefunc(char **args, shaderfunc_t *func)
{
	if (!strcmp(args[0], "sin"))
	    func->func = SHADER_FUNC_SIN;
	else if (!strcmp(args[0], "triangle"))
	    func->func = SHADER_FUNC_TRIANGLE;
	else if (!strcmp(args[0], "square"))
	    func->func = SHADER_FUNC_SQUARE;
	else if (!strcmp(args[0], "sawtooth"))
	    func->func = SHADER_FUNC_SAWTOOTH;
	else if (!strcmp(args[0], "inversesawtooth"))
	    func->func = SHADER_FUNC_INVERSESAWTOOTH;
	else
	    Syntax();

	func->args[0] = atof(args[1]);
	func->args[1] = atof(args[2]);
	func->args[2] = atof(args[3]);
	func->args[3] = 1;
}

shader_t *R_RegisterShaderNoMip (char *name) 
{
	return &r_shaders[R_LoadShader (name, SHADER_2D, NULL)];
}

shader_t *R_RegisterShader (char *name)
{
	return &r_shaders[R_LoadShader (name, SHADER_BSP, NULL)];
}

shader_t *R_RegisterShaderMD3 (char *name)
{
	return &r_shaders[R_LoadShader (name, SHADER_MD3, NULL)];
}
