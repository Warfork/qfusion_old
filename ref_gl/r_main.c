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
// r_main.c
#include "r_local.h"

void R_Clear (void);

viddef_t	vid;

int			GL_TEXTURE_0, GL_TEXTURE_1;

model_t		*r_worldmodel;
bmodel_t	*r_worldbmodel;

meshlist_t	r_worldlist;

float		gldepthmin, gldepthmax;

glconfig_t	gl_config;
glstate_t	gl_state;

image_t		*r_notexture;		// use for bad textures
image_t		*r_particletexture;	// little dot for particles
image_t		*r_whitetexture;
image_t		*r_dlighttexture;
image_t		*r_fogtexture;

entity_t	*currententity;
model_t		*currentmodel;

int			r_numnullentities;
entity_t	*r_nullentities[MAX_EDICTS];

cplane_t	frustum[4];

mleaf_t		*r_vischain;		// linked list of visible leafs
int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_world_leafs;

float		v_blend[4];			// final blending color

void GL_Strings_f( void );

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

qboolean	r_portalview;	// if true, get vis data at
vec3_t		r_portalorg;	// portalorg instead of vieworg

qboolean	r_mirrorview;	// if true, lock pvs

cplane_t	r_clipplane;

mat4_t		r_worldview_matrix;
mat4_t		r_modelview_matrix;
mat4_t		r_projection_matrix;

//
// screen size info
//
refdef_t	r_newrefdef;

int			r_viewcluster, r_oldviewcluster;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_fastsky;
cvar_t	*r_ignorehwgamma;
cvar_t	*r_overbrightbits;
cvar_t	*r_mapoverbrightbits;
cvar_t	*r_vertexlight;
cvar_t	*r_flares;
cvar_t	*r_flaresize;
cvar_t	*r_flarefade;
cvar_t	*r_dynamiclight;
cvar_t	*r_detailtextures;
cvar_t	*r_subdivisions;
cvar_t	*r_faceplanecull;
cvar_t	*r_showtris;
cvar_t	*r_shownormals;
cvar_t	*r_ambientscale;
cvar_t	*r_directedscale;

cvar_t	*r_allow_software;
cvar_t	*r_3dlabs_broken;

cvar_t	*r_shadows;	
cvar_t	*r_shadows_alpha;
cvar_t	*r_shadows_nudge;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;

cvar_t	*r_stencilbits;
cvar_t	*r_colorbits;
cvar_t	*r_texturebits;
cvar_t	*r_texturemode;
cvar_t	*r_mode;
cvar_t	*r_picmip;
cvar_t	*r_skymip;
cvar_t	*r_nobind;
cvar_t	*r_clear;
cvar_t	*r_polyblend;
cvar_t	*r_playermip;
cvar_t	*r_lockpvs;
cvar_t	*r_screenshot_jpeg;
cvar_t	*r_screenshot_jpeg_quality;

cvar_t	*gl_extensions;
cvar_t	*gl_ext_swapinterval;
cvar_t	*gl_ext_multitexture;
cvar_t	*gl_ext_compiled_vertex_array;
cvar_t	*gl_ext_sgis_mipmap;
cvar_t	*gl_ext_texture_env_combine;
cvar_t	*gl_ext_NV_texture_env_combine4;
cvar_t	*gl_ext_compressed_textures;
cvar_t	*gl_ext_bgra;
cvar_t	*gl_ext_texture_edge_clamp;
cvar_t	*gl_ext_texture_filter_anisotropic;
cvar_t	*gl_ext_max_texture_filter_anisotropic;

cvar_t	*gl_log;
cvar_t	*gl_drawbuffer;
cvar_t  *gl_driver;
cvar_t	*gl_finish;
cvar_t	*gl_cull;
cvar_t	*gl_swapinterval;

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox (const vec3_t mins, const vec3_t maxs, const int clipflags)
{
	int		i;
	cplane_t *p;

	if (r_nocull->value)
		return qfalse;

	for (i=0,p=frustum ; i<4; i++,p++)
	{
		if ( !(clipflags & (1<<i)) ) {
			continue;
		}

		switch (p->signbits)
		{
		case 0:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return qtrue;
			break;
		case 1:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return qtrue;
			break;
		case 2:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return qtrue;
			break;
		case 3:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return qtrue;
			break;
		case 4:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return qtrue;
			break;
		case 5:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return qtrue;
			break;
		case 6:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return qtrue;
			break;
		case 7:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return qtrue;
			break;
		default:
			assert( 0 );
			return qfalse;
		}
	}

	return qfalse;
}

/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
qboolean R_CullSphere (const vec3_t centre, const float radius, const int clipflags)
{
	int		i;
	cplane_t *p;

	if (r_nocull->value)
		return qfalse;

	for (i=0,p=frustum ; i<4; i++,p++)
	{
		if ( !(clipflags & (1<<i)) ) {
			continue;
		}

		if ( DotProduct ( centre, p->normal ) - p->dist <= -radius )
			return qtrue;
	}

	return qfalse;
}

/*
=============
R_FogForSphere
=============
*/
mfog_t *R_FogForSphere( const vec3_t centre, const float radius )
{
	int			i, j;
	mfog_t		*fog;
	cplane_t	**plane;

	if ( !r_worldmodel || (r_newrefdef.rdflags & RDF_NOWORLDMODEL) 
		|| !r_worldbmodel || !r_worldbmodel->numfogs )
		return NULL;

	fog = r_worldbmodel->fogs;
	for ( i = 0; i < r_worldbmodel->numfogs; i++, fog++ ) {
		if ( !fog->visibleplane ) {
			continue;
		}

		plane = fog->planes;
		for ( j = 0; j < fog->numplanes; j++, plane++ ) {
			// if completely in front of face, no intersection
			if ( PlaneDiff ( centre, *plane ) > radius ) {
				break;
			}
		}

		if ( j == fog->numplanes ) {
			return fog;
		}
	}

	return NULL;
}

void R_RotateForEntity (entity_t *e)
{
	mat4_t obj_m;

	if ( e->scale != 1.0f && e->model && (e->model->type == mod_brush) ) {
		obj_m[0] = e->axis[0][0] * e->scale;
		obj_m[1] = e->axis[0][1] * e->scale;
		obj_m[2] = e->axis[0][2] * e->scale;
		obj_m[4] = e->axis[1][0] * e->scale;
		obj_m[5] = e->axis[1][1] * e->scale;
		obj_m[6] = e->axis[1][2] * e->scale;
		obj_m[8] = e->axis[2][0] * e->scale;
		obj_m[9] = e->axis[2][1] * e->scale;
		obj_m[10] = e->axis[2][2] * e->scale;
	} else {
		obj_m[0] = e->axis[0][0];
		obj_m[1] = e->axis[0][1];
		obj_m[2] = e->axis[0][2];
		obj_m[4] = e->axis[1][0];
		obj_m[5] = e->axis[1][1];
		obj_m[6] = e->axis[1][2];
		obj_m[8] = e->axis[2][0];
		obj_m[9] = e->axis[2][1];
		obj_m[10] = e->axis[2][2];
	}

	obj_m[3] = 0;
	obj_m[7] = 0;
	obj_m[11] = 0;
	obj_m[12] = e->origin[0];
	obj_m[13] = e->origin[1];
	obj_m[14] = e->origin[2];
	obj_m[15] = 1.0;

	Matrix4_MultiplyFast ( r_worldview_matrix, obj_m, r_modelview_matrix );

	qglLoadMatrixf ( r_modelview_matrix );
}

void R_TranslateForEntity (entity_t *e)
{
	mat4_t obj_m;

	Matrix4_Identity ( obj_m );

	obj_m[12] = e->origin[0];
	obj_m[13] = e->origin[1];
	obj_m[14] = e->origin[2];

	Matrix4_MultiplyFast ( r_worldview_matrix, obj_m, r_modelview_matrix );

	qglLoadMatrixf ( r_modelview_matrix );
}

qboolean R_LerpAttachment ( orientation_t *orient, model_t *mod, int frame, int oldframe, float backlerp, char *name )
{
	if ( !orient ) {
		return qfalse;
	}

	VectorClear ( orient->origin );
	Matrix3_Identity ( orient->axis );

	if ( !name ) {
		return qfalse;
	}

	if ( mod->type == mod_alias ) {
		return R_AliasModelLerpTag ( orient, mod->aliasmodel, frame, oldframe, backlerp, name );
	} else if ( mod->type == mod_skeletal ) {
		return R_SkeletalModelLerpAttachment ( orient, mod->skmodel, frame, oldframe, backlerp, name );
	}

	return qfalse;
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

static	vec4_t			spr_xyz[4];
static	vec2_t			spr_st[4];
static	byte_vec4_t		spr_color[4];

static	mesh_t			spr_mesh;
static	meshbuffer_t	spr_mbuffer;

/*
=================
R_DrawSprite

=================
*/
void R_DrawSprite ( meshbuffer_t *mb, float rotation, float right, float left, float up, float down )
{
	vec3_t		point;
	vec3_t		v_right, v_up;
	int			features;
	entity_t	*e = mb->entity;

	if ( rotation ) {
		RotatePointAroundVector ( v_right, vpn, vright, rotation );
		CrossProduct ( vpn, v_right, v_up );
	} else {
		VectorCopy ( vright, v_right );
		VectorCopy ( vup, v_up );
	}

	VectorScale ( v_up, down, point );
	VectorMA ( point, -left, v_right, spr_mesh.xyz_array[0] );
	VectorMA ( point, -right, v_right, spr_mesh.xyz_array[3] );

	VectorScale ( v_up, up, point );
	VectorMA ( point, -left, v_right, spr_mesh.xyz_array[1] );
	VectorMA ( point, -right, v_right, spr_mesh.xyz_array[2] );

	if ( e->scale != 1.0f ) {
		VectorScale ( spr_mesh.xyz_array[0], e->scale, spr_mesh.xyz_array[0] );
		VectorScale ( spr_mesh.xyz_array[1], e->scale, spr_mesh.xyz_array[1] );
		VectorScale ( spr_mesh.xyz_array[2], e->scale, spr_mesh.xyz_array[2] );
		VectorScale ( spr_mesh.xyz_array[3], e->scale, spr_mesh.xyz_array[3] );
	}

	// the code below is disgusting, but some q3a shaders use 'rgbgen vertex'
	// and 'alphagen vertex' for effects instead of 'rgbgen entity' and 'alphagen entity'
	if ( mb->shader->features & MF_COLORS ) {
		Vector4Copy ( e->color, spr_mesh.colors_array[0] );
		Vector4Copy ( e->color, spr_mesh.colors_array[1] );
		Vector4Copy ( e->color, spr_mesh.colors_array[2] );
		Vector4Copy ( e->color, spr_mesh.colors_array[3] );
	}

	features = MF_NOCULL | mb->shader->features;
	if ( r_shownormals->value ) {
		features |= MF_NORMALS;
	}

	if ( !(mb->shader->flags & SHADER_ENTITY_MERGABLE) ) {
		R_TranslateForEntity ( e );
		R_PushMesh ( &spr_mesh, MF_NONBATCHED | features );
		R_RenderMeshBuffer ( mb, qfalse );
	} else {
		VectorAdd ( spr_mesh.xyz_array[0], e->origin, spr_mesh.xyz_array[0] );
		VectorAdd ( spr_mesh.xyz_array[1], e->origin, spr_mesh.xyz_array[1] );
		VectorAdd ( spr_mesh.xyz_array[2], e->origin, spr_mesh.xyz_array[2] );
		VectorAdd ( spr_mesh.xyz_array[3], e->origin, spr_mesh.xyz_array[3] );
		R_PushMesh ( &spr_mesh, features );
	}
}

/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (meshbuffer_t *mb)
{
	sframe_t	*frame;
	smodel_t	*psprite;
	entity_t	*e = mb->entity;
	model_t		*model = e->model;

	psprite = model->smodel;
	frame = psprite->frames + e->frame;

	R_DrawSprite ( mb, e->rotation, frame->origin_x, frame->origin_x - frame->width, frame->height - frame->origin_y, -frame->origin_y );
}

/*
=================
R_AddSpriteModelToList
=================
*/
void R_AddSpriteModelToList (entity_t *e)
{
	sframe_t	*frame;
	smodel_t	*psprite;
	model_t		*model = e->model;

	if ( !(psprite = model->smodel) ) {
		return;
	}

	// cull it because we don't want to sort unneeded things
	if ((e->origin[0] - r_newrefdef.vieworg[0]) * vpn[0] +
		(e->origin[1] - r_newrefdef.vieworg[1]) * vpn[1] + 
		(e->origin[2] - r_newrefdef.vieworg[2]) * vpn[2] < 0) {
		return;
	}

	e->frame %= psprite->numframes;
	frame = psprite->frames + e->frame;

	// select skin
	if ( e->customShader ) {
		R_AddMeshToList ( R_FogForSphere ( e->origin, frame->radius ), e->customShader, -1 );
	} else {
		R_AddMeshToList ( R_FogForSphere ( e->origin, frame->radius ), frame->shader, -1 );
	}
}

/*
=================
R_DrawSpritePoly

=================
*/
void R_DrawSpritePoly (meshbuffer_t *mb)
{
	entity_t	*e = mb->entity;

	R_DrawSprite ( mb, e->rotation, -e->radius, e->radius, e->radius, -e->radius );
}

/*
=================
R_AddSpritePolyToList
=================
*/
void R_AddSpritePolyToList (entity_t *e)
{
	// select skin
	if ( !e->customShader ) {
		return;
	}

	// cull it because we don't want to sort unneeded things
	if ((e->origin[0] - r_newrefdef.vieworg[0]) * vpn[0] +
		(e->origin[1] - r_newrefdef.vieworg[1]) * vpn[1] + 
		(e->origin[2] - r_newrefdef.vieworg[2]) * vpn[2] < 0) {
		return;
	}

	R_AddMeshToList ( R_FogForSphere ( e->origin, e->radius ), e->customShader, -1 );
}

/*
=================
R_SpriteOverflow
=================
*/
qboolean R_SpriteOverflow ( void )
{
	return R_BackendOverflow ( &spr_mesh );
}

/*
=================
R_InitSpriteModels
=================
*/
void R_InitSpriteModels (void)
{
	spr_mesh.numindexes = 6;
	spr_mesh.indexes = r_quad_indexes;

	spr_mesh.numvertexes = 4;
	spr_mesh.xyz_array = spr_xyz;
	spr_mesh.st_array = spr_st;
	spr_mesh.colors_array = spr_color;

	spr_mesh.st_array[0][0] = 0;
	spr_mesh.st_array[0][1] = 1;
	spr_mesh.st_array[1][0] = 0;
	spr_mesh.st_array[1][1] = 0;
	spr_mesh.st_array[2][0] = 1;
	spr_mesh.st_array[2][1] = 0;
	spr_mesh.st_array[3][0] = 1;
	spr_mesh.st_array[3][1] = 1;
}

/*
=============================================================

  LENS FLARES

=============================================================
*/


/*
=================
R_PushFlareSurf
=================
*/
void R_PushFlareSurf ( meshbuffer_t *mb )
{
	vec4_t color;
	vec3_t origin, point;
	float radius = r_flaresize->value, flarescale;
	float up = radius, down = -radius, left = -radius, right = radius;
	msurface_t *surf;

	surf = &currentmodel->bmodel->surfaces[mb->infokey];
	VectorCopy (surf->origin, origin);

	VectorMA ( origin, down, vup, point );
	VectorMA ( point, -left, vright, spr_mesh.xyz_array[0] );
	VectorMA ( point, -right, vright, spr_mesh.xyz_array[3] );

	VectorMA ( origin, up, vup, point );
	VectorMA ( point, -left, vright, spr_mesh.xyz_array[1] );
	VectorMA ( point, -right, vright, spr_mesh.xyz_array[2] );

	flarescale = 1.0 / r_flarefade->value;
	Vector4Set ( color, 
		COLOR_R ( surf->dlightbits ) * flarescale,
		COLOR_G ( surf->dlightbits ) * flarescale,
		COLOR_B ( surf->dlightbits ) * flarescale, 255 );

	Vector4Copy ( color, spr_mesh.colors_array[0] );
	Vector4Copy ( color, spr_mesh.colors_array[1] );
	Vector4Copy ( color, spr_mesh.colors_array[2] );
	Vector4Copy ( color, spr_mesh.colors_array[3] );

	R_PushMesh  ( &spr_mesh, MF_NOCULL | mb->shader->features );
}


//==================================================================================

/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel (void)
{
	qglDepthMask ( GL_FALSE );
	qglBegin ( GL_LINES );

	qglColor4f ( 1, 0, 0, 0.5 );
	qglVertex3fv ( currententity->origin );
	qglVertex3f ( currententity->origin[0] + currententity->axis[0][0] * 15,
		currententity->origin[1] + currententity->axis[0][1] * 15, 
		currententity->origin[2] + currententity->axis[0][2] * 15);

	qglColor4f ( 0, 1, 0, 0.5 );
	qglVertex3fv ( currententity->origin );
	qglVertex3f ( currententity->origin[0] - currententity->axis[1][0] * 15,
		currententity->origin[1] - currententity->axis[1][1] * 15, 
		currententity->origin[2] - currententity->axis[1][2] * 15);

	qglColor4f ( 0, 0, 1, 0.5 );
	qglVertex3fv ( currententity->origin );
	qglVertex3f ( currententity->origin[0] + currententity->axis[2][0] * 15,
		currententity->origin[1] + currententity->axis[2][1] * 15, 
		currententity->origin[2] + currententity->axis[2][2] * 15);

	qglEnd ();
	qglDepthMask ( GL_TRUE );
}

/*
=============
R_AddEntitiesToList
=============
*/
void R_AddEntitiesToList (void)
{
	int		i;

	r_numnullentities = 0;

	if (!r_drawentities->value)
		return;

	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if ( r_mirrorview ) {
			if ( currententity->flags & RF_WEAPONMODEL ) 
				continue;
		}

		switch ( currententity->rtype )
		{
			case RT_MODEL:
				if ( !(currentmodel = currententity->model) ) {
					r_nullentities[r_numnullentities++] = currententity;
					break;
				}

				switch (currentmodel->type)
				{
				case mod_alias:
					R_AddAliasModelToList (currententity);
					break;
				case mod_skeletal:
					R_AddSkeletalModelToList (currententity);
					break;
				case mod_brush:
					R_AddBrushModelToList (currententity);
					break;
				case mod_sprite:
					R_AddSpriteModelToList (currententity);
					break;
				default:
					Com_Error (ERR_DROP, "%s: bad modeltype", currentmodel->name);
					break;
				}
				break;
			
			case RT_SPRITE:
				if ( currententity->radius ) {
					R_AddSpritePolyToList ( currententity );
				}
				break;

			case RT_PORTALSURFACE:
				break;

			default:
				Com_Error (ERR_DROP, "%s: bad entitytype", currententity->rtype);
				break;
		}
	}
}

/*
=============
R_DrawNullEntities
=============
*/
void R_DrawNullEntities (void)
{
	int		i;

	if (!r_numnullentities)
		return;

	GL_EnableMultitexture ( qfalse );
	qglDepthFunc ( GL_LEQUAL );
	qglDisable ( GL_TEXTURE_2D );
	qglDisable ( GL_ALPHA_TEST );
	qglEnable ( GL_BLEND );
	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	// draw non-transparent first
	for (i=0 ; i<r_numnullentities ; i++)
	{
		currententity = r_nullentities[i];

		if ( r_mirrorview ) {
			if ( currententity->flags & RF_WEAPONMODEL ) 
				continue;
		} else {
			if ( currententity->flags & RF_VIEWERMODEL ) 
				continue;
		}

		R_DrawNullModel ();
	}

	qglDisable ( GL_BLEND );
	qglEnable ( GL_TEXTURE_2D );
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!r_polyblend->value)
		return;
	if (v_blend[3] < 0.01f)
		return;

	qglEnable ( GL_BLEND );
	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, 1, 1, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	qglColor4fv (v_blend);

	qglBegin (GL_TRIANGLES);
	qglVertex2f (-5, -5);
	qglVertex2f (10, -5);
	qglVertex2f (-5, 10);
	qglEnd ();

	qglDisable ( GL_BLEND );
	qglEnable (GL_TEXTURE_2D);

	qglColor4f(1,1,1,1);
}

/*
============
R_ApplySoftwareGamma
============
*/
void R_ApplySoftwareGamma (void)
{
	float f;
	float div;

	if (!r_ignorehwgamma->value || gl_state.gammaramp)
		return;

	qglEnable ( GL_BLEND );
	qglBlendFunc ( GL_DST_COLOR, GL_ONE );
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, 1, 1, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	div = 0.5f*(double)(1 << (int)floor(r_overbrightbits->value));
	f = 1.3f + div - vid_gamma->value;
	clamp ( f, 0.1f, 5.0f );

	qglBegin (GL_TRIANGLES);

	while (f >= 1.01f)
	{
		if (f >= 2)
			qglColor4f (1.0f, 1.0f, 1.0f, 1.0f);
		else
			qglColor4f (f - 1.0f, f - 1.0f, f - 1.0f, 1.0f);

		qglVertex2f (-5, -5);
		qglVertex2f (10, -5);
		qglVertex2f (-5, 10);
		f *= 0.5;
	}

	qglEnd ();

	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDisable ( GL_BLEND );
	qglEnable (GL_TEXTURE_2D);

	qglColor4f(1,1,1,1);
}

void R_ShadowBlend (void)
{
#ifdef SHADOW_VOLUMES
	if ( r_shadows->value != 2 || !gl_state.stencil_enabled ) {
		return;
	}

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, 1, 1, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	qglDisable ( GL_ALPHA_TEST );
	qglEnable ( GL_BLEND );
	qglDisable ( GL_CULL_FACE );
	qglDisable ( GL_DEPTH_TEST );
	qglDisable ( GL_TEXTURE_2D );

	qglColor4f ( 0, 0, 0, bound (0.0f, r_shadows_alpha->value, 1.0f) );

	qglEnable ( GL_STENCIL_TEST );
	qglStencilFunc (GL_NOTEQUAL, 128, 0xFF);
	qglStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);

	qglBegin (GL_TRIANGLES);
	qglVertex2f (-5, -5);
	qglVertex2f (10, -5);
	qglVertex2f (-5, 10);
	qglEnd ();

	qglDisable ( GL_STENCIL_TEST );
	qglDisable ( GL_BLEND );
	qglEnable ( GL_TEXTURE_2D );

	qglColor4f(1,1,1,1);
#endif
}

//=======================================================================

void R_SetFrustum (void)
{
	int		i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_newrefdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_newrefdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_newrefdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_newrefdef.fov_y / 2 ) );

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_NONAXIAL;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

//=======================================================================

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	int i;
	mleaf_t *leaf;

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_newrefdef.vieworg, r_origin);

	if ( !r_portalview )
		AngleVectors (r_newrefdef.viewangles, vpn, vright, vup);

// current viewcluster
	if ( !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) && !r_mirrorview )
	{
		if ( r_portalview ) {
			r_oldviewcluster = -1;
			leaf = Mod_PointInLeaf (r_portalorg, r_worldbmodel);
		} else {
			r_oldviewcluster = r_viewcluster;
			leaf = Mod_PointInLeaf (r_origin, r_worldbmodel);
		}

		r_viewcluster = leaf->cluster;
	}

	for (i=0 ; i<4 ; i++)
		v_blend[i] = r_newrefdef.blend[i];
}


void MYgluPerspective( GLdouble fovy, GLdouble aspect,
		     GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;
	
	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;
	
	xmin = ymin * aspect;
	xmax = ymax * aspect;
	
	xmin += -( 2 * gl_state.camera_separation ) / zNear;
	xmax += -( 2 * gl_state.camera_separation ) / zNear;
	
	qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	int		x, x2, y2, y, w, h;

	if ( r_portalview ) {
		goto modelview;
	}

	//
	// set up viewport
	//
	x = floor(r_newrefdef.x * vid.width / vid.width);
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	y = floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	w = x2 - x;
	h = y - y2;

	qglViewport (x, y2, w, h);
	qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );

	//
	// clear screen if desired
	//
	R_Clear ();

	//
	// set up projection matrix
	//
    screenaspect = (float)r_newrefdef.width/r_newrefdef.height;
	qglMatrixMode (GL_PROJECTION);
    qglLoadIdentity ();
	MYgluPerspective (r_newrefdef.fov_y, screenaspect, 4, 65536);

	if ( r_mirrorview ) {
		qglScalef ( -1, 1, 1 );
	}

	qglGetFloatv (GL_PROJECTION_MATRIX, r_projection_matrix);

	qglMatrixMode (GL_MODELVIEW);

modelview:
	Matrix4_Identity ( r_worldview_matrix );
	Matrix4_Rotate ( r_worldview_matrix, -90, 1, 0, 0 );
	Matrix4_Rotate ( r_worldview_matrix,  90, 0, 0, 1 );
	Matrix4_Rotate ( r_worldview_matrix, -r_newrefdef.viewangles[2], 1, 0, 0 );
	Matrix4_Rotate ( r_worldview_matrix, -r_newrefdef.viewangles[0], 0, 1, 0 );
	Matrix4_Rotate ( r_worldview_matrix, -r_newrefdef.viewangles[1], 0, 0, 1 );
	Matrix4_Translate ( r_worldview_matrix, -r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2] );

	qglLoadMatrixf ( r_worldview_matrix );

	if ( r_portalview || r_mirrorview ) {
		GLdouble clip[4];

		clip[0] = r_clipplane.normal[0];
		clip[1] = r_clipplane.normal[1];
		clip[2] = r_clipplane.normal[2];
		clip[3] = -r_clipplane.dist;

		qglClipPlane ( GL_CLIP_PLANE0, clip );
		qglEnable ( GL_CLIP_PLANE0 );
	}

	qglEnable (GL_DEPTH_TEST);
	qglDepthMask ( GL_TRUE );
}

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	int	bits;
	
	bits = GL_DEPTH_BUFFER_BIT;

	if ( r_clear->value ) {
		bits |= GL_COLOR_BUFFER_BIT;
	}
	if ( gl_state.stencil_enabled && r_shadows->value ) {
		qglClearStencil ( 128 );
		bits |= GL_STENCIL_BUFFER_BIT;
	}

	qglClear ( bits );

	gldepthmin = 0;
	gldepthmax = 1;
	qglDepthRange ( gldepthmin, gldepthmax );
}

void R_Flash( void )
{
	R_PolyBlend ();
	R_ShadowBlend ();
}

/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
void R_RenderView ( refdef_t *fd, meshlist_t *list )
{
	r_newrefdef = *fd;
	r_currentlist = list;

	if ( (!r_worldmodel || !r_worldbmodel) && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		Com_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	if ( (r_mirrorview || r_portalview) && r_fastsky->value ) {
		R_DrawSky ( NULL );
		goto done;
	}

	R_MarkLeaves ();

	R_DrawWorld ();

	R_AddPolysToList ();

	R_AddEntitiesToList ();

	R_SortMeshes ();

	R_DrawMeshes ( qfalse );

	R_DrawTriangleOutlines ();

	R_DrawNullEntities ();

	R_RenderDlights ();

	if ( r_mirrorview || r_portalview ) {
done:
		qglDisable ( GL_CLIP_PLANE0 );
	}
}

void R_SetGL2D (void)
{
	// set 2D virtual screen size
	qglViewport (0,0, vid.width, vid.height);
	qglMatrixMode (GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode (GL_MODELVIEW);
    qglLoadIdentity ();
	qglDisable (GL_DEPTH_TEST);
	qglDisable ( GL_CULL_FACE );
	qglColor4f (1, 1, 1, 1);
	gl_state.in2d = qtrue;
}

static void GL_DrawColoredStereoLinePair( float r, float g, float b, float y )
{
	qglColor3f( r, g, b );
	qglVertex2f( 0, y );
	qglVertex2f( vid.width, y );
	qglColor3f( 0, 0, 0 );
	qglVertex2f( 0, y + 1 );
	qglVertex2f( vid.width, y + 1 );
}

static void GL_DrawStereoPattern( void )
{
	int i;

	if ( !( gl_config.renderer & GL_RENDERER_INTERGRAPH ) )
		return;

	if ( !gl_state.stereo_enabled )
		return;

	R_SetGL2D();

	qglDrawBuffer( GL_BACK_LEFT );

	for ( i = 0; i < 20; i++ )
	{
		qglBegin( GL_LINES );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 0 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 2 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 4 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 6 );
			GL_DrawColoredStereoLinePair( 0, 1, 0, 8 );
			GL_DrawColoredStereoLinePair( 1, 1, 0, 10);
			GL_DrawColoredStereoLinePair( 1, 1, 0, 12);
			GL_DrawColoredStereoLinePair( 0, 1, 0, 14);
		qglEnd();
		
		GLimp_EndFrame();
	}
}

void GL_TransformToScreen_Vec3 ( vec3_t in, vec3_t out )
{
	vec3_t temp;

	Matrix4_Multiply_Vec3 ( r_worldview_matrix, in, temp );
	Matrix4_Multiply_Vec3 ( r_projection_matrix, temp, out );

	out[0] = r_newrefdef.x + (out[0] + 1.0f) * r_newrefdef.width * 0.5f;
	out[1] = r_newrefdef.y + (out[1] + 1.0f) * r_newrefdef.height * 0.5f;
}

/*
@@@@@@@@@@@@@@@@@@@@@
R_RenderFrame

@@@@@@@@@@@@@@@@@@@@@
*/
void R_RenderFrame (refdef_t *fd)
{
	gl_state.in2d = qfalse;

	if ( !r_norefresh->value ) {
		R_BackendStartFrame ();

		if (gl_finish->value)
			qglFinish ();

		r_mirrorview = qfalse;
		r_portalview = qfalse;

		c_brush_polys = 0;
		c_world_leafs = 0;

		r_worldlist.skyDrawn = qfalse;
		r_worldlist.num_meshes = 0;
		r_worldlist.num_additive_meshes = 0;

		R_RenderView( fd, &r_worldlist );
		R_Flash();

		R_BackendEndFrame ();
	}

	R_SetGL2D ();
}


void R_Register( void )
{
	Cvar_GetLatchedVars (CVAR_LATCH_VIDEO);

	r_norefresh = Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = Cvar_Get ("r_fullbright", "0", CVAR_CHEAT|CVAR_LATCH_VIDEO);
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_CHEAT);
	r_drawworld = Cvar_Get ("r_drawworld", "1", CVAR_CHEAT);
	r_novis = Cvar_Get ("r_novis", "0", 0);
	r_nocull = Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = Cvar_Get ("r_speeds", "0", 0);

	r_fastsky = Cvar_Get ("r_fastsky", "0", CVAR_ARCHIVE);
	r_ignorehwgamma = Cvar_Get ("r_ignorehwgamma", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_overbrightbits = Cvar_Get ("r_overbrightbits", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_mapoverbrightbits = Cvar_Get ("r_mapoverbrightbits", "2", CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_vertexlight = Cvar_Get ("r_vertexlight", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_detailtextures = Cvar_Get ("r_detailtextures", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_flares = Cvar_Get ("r_flares", "0", CVAR_ARCHIVE);
	r_flaresize = Cvar_Get ("r_flaresize", "40", CVAR_ARCHIVE);
	r_flarefade = Cvar_Get ("r_flarefade", "7", CVAR_ARCHIVE);
	r_dynamiclight = Cvar_Get ("r_dynamiclight", "1", CVAR_ARCHIVE);
	r_subdivisions = Cvar_Get ("r_subdivisions", "4", CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_faceplanecull = Cvar_Get ("r_faceplanecull", "1", CVAR_ARCHIVE);
	r_showtris = Cvar_Get ("r_showtris", "0", CVAR_CHEAT);
	r_shownormals = Cvar_Get ("r_shownormals", "0", CVAR_CHEAT);
	r_ambientscale = Cvar_Get ("r_ambientscale", "0.6", 0);
	r_directedscale = Cvar_Get ("r_directedscale", "1", 0);

	r_allow_software = Cvar_Get( "r_allow_software", "0", 0 );
	r_3dlabs_broken = Cvar_Get( "r_3dlabs_broken", "1", CVAR_ARCHIVE );

	r_shadows = Cvar_Get ("r_shadows", "0", CVAR_ARCHIVE );
	r_shadows_alpha = Cvar_Get ("r_shadows_alpha", "0.4", CVAR_ARCHIVE );
	r_shadows_nudge = Cvar_Get ("r_shadows_nudge", "1", CVAR_ARCHIVE );

	r_lodbias = Cvar_Get( "r_lodbias", "0", CVAR_ARCHIVE );
	r_lodscale = Cvar_Get( "r_lodscale", "5.0", CVAR_ARCHIVE );

	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	r_texturebits = Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	r_texturemode = Cvar_Get( "r_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	r_stencilbits = Cvar_Get( "r_stencilbits", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_mode = Cvar_Get( "r_mode", "3", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_nobind = Cvar_Get ("r_nobind", "0", 0);
	r_picmip = Cvar_Get ("r_picmip", "0", CVAR_LATCH_VIDEO);
	r_skymip = Cvar_Get ("r_skymip", "0", CVAR_LATCH_VIDEO);
	r_clear = Cvar_Get ("r_clear", "0", 0);
	r_polyblend = Cvar_Get ("r_polyblend", "1", 0);
	r_playermip = Cvar_Get ("r_playermip", "0", 0);
	r_lockpvs = Cvar_Get( "r_lockpvs", "0", CVAR_CHEAT );
	r_screenshot_jpeg = Cvar_Get( "r_screenshot_jpeg", "1", CVAR_ARCHIVE );					// Heffo - JPEG Screenshots
	r_screenshot_jpeg_quality = Cvar_Get( "r_screenshot_jpeg_quality", "85", CVAR_ARCHIVE );	// Heffo - JPEG Screenshots

	gl_log = Cvar_Get( "gl_log", "0", 0 );
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_cull = Cvar_Get ("gl_cull", "1", 0);
	gl_driver = Cvar_Get( "gl_driver", GL_DRIVERNAME, CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	gl_extensions = Cvar_Get( "gl_extensions", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_swapinterval = Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_multitexture = Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_compiled_vertex_array = Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_sgis_mipmap = Cvar_Get( "gl_ext_sgis_mipmap", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_env_combine = Cvar_Get( "gl_ext_texture_env_combine", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_NV_texture_env_combine4 = Cvar_Get( "gl_ext_NV_texture_env_combine4", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_compressed_textures = Cvar_Get( "gl_ext_compressed_textures", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_bgra = Cvar_Get( "gl_ext_bgra", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_edge_clamp = Cvar_Get( "gl_ext_texture_edge_clamp", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_filter_anisotropic = Cvar_Get( "gl_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_max_texture_filter_anisotropic = Cvar_Get( "gl_ext_max_texture_filter_anisotropic", "0", CVAR_NOSET );

	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );
	gl_swapinterval = Cvar_Get( "gl_swapinterval", "0", CVAR_ARCHIVE );

	vid_fullscreen = Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );

	Cmd_AddCommand( "imagelist", GL_ImageList_f );
	Cmd_AddCommand( "screenshot", R_ScreenShot_f );
	Cmd_AddCommand( "modellist", Mod_Modellist_f );

	Cmd_AddCommand( "gl_strings", GL_Strings_f );
}

/*
==================
R_SetMode
==================
*/
qboolean R_SetMode (void)
{
	int err;
	qboolean fullscreen;

	if ( vid_fullscreen->modified && !gl_config.allow_cds )
	{
		Com_Printf( "R_SetMode() - CDS not allowed with this driver\n" );
		Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->value );
		vid_fullscreen->modified = qfalse;
	}

	fullscreen = vid_fullscreen->value;

	vid_fullscreen->modified = qfalse;

	if ( r_mode->value < 3 ) {
		Com_Printf ( "Resolutions below 640x480 are not supported\n" );
		Cvar_ForceSet ( "r_mode", "3" );
	}

	r_mode->modified = qfalse;

	if ( ( err = GLimp_SetMode( &vid.width, &vid.height, r_mode->value, fullscreen ) ) == rserr_ok )
	{
		gl_state.prev_mode = r_mode->value;
	}
	else
	{
		if ( err == rserr_invalid_fullscreen )
		{
			Cvar_SetValue( "vid_fullscreen", 0);
			vid_fullscreen->modified = qfalse;
			Com_Printf( "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n" );
			if ( ( err = GLimp_SetMode( &vid.width, &vid.height, r_mode->value, qfalse ) ) == rserr_ok )
				return qtrue;
		}
		else if ( err == rserr_invalid_mode )
		{
			Cvar_SetValue( "r_mode", gl_state.prev_mode );
			r_mode->modified = qfalse;
			Com_Printf( "ref_gl::R_SetMode() - invalid mode\n" );
		}

		// try setting it back to something safe
		if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_state.prev_mode, qfalse ) ) != rserr_ok )
		{
			Com_Printf( "ref_gl::R_SetMode() - could not revert to safe mode\n" );
			return qfalse;
		}
	}
	return qtrue;
}

/*
===============
GL_CheckExtensions
===============
*/
void GL_CheckExtensions (void)
{
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;

#ifdef _WIN32
	qwglSwapIntervalEXT = NULL;
	qwglGetDeviceGammaRamp3DFX = NULL;
	qwglSetDeviceGammaRamp3DFX = NULL;
#endif

	qglSelectTextureSGIS = NULL;
	qglMTexCoord2fSGIS = NULL;

	gl_config.env_add = qfalse;
	gl_config.tex_env_combine = qfalse;
	gl_config.sgis_mipmap = qfalse;
	gl_config.nv_tex_env_combine4 = qfalse;
	gl_config.compressed_textures = qfalse;
	gl_config.bgra = qfalse;
	gl_config.texture_edge_clamp = qfalse;
	gl_config.texture_filter_anisotropic = qfalse;
	gl_config.max_texture_filter_anisotropic = 0.0f;

	if ( !gl_extensions->value ) {
		Com_Printf ( "...ignoring all OpenGL extensions\n" );
		return;
	}

	/*
	** grab extensions
	*/
	if ( strstr( gl_config.extensions_string, "GL_EXT_compiled_vertex_array" ) || 
		 strstr( gl_config.extensions_string, "GL_SGI_compiled_vertex_array" ) )
	{
		if ( gl_ext_compiled_vertex_array->value ) 
		{
			Com_Printf( "...using GL_EXT_compiled_vertex_array\n" );
			qglLockArraysEXT = ( void * ) qwglGetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = ( void * ) qwglGetProcAddress( "glUnlockArraysEXT" );
		}
		else
		{
			Com_Printf( "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}
	else
	{
		Com_Printf( "...GL_EXT_compiled_vertex_array not found\n" );
	}

#ifdef _WIN32
	if ( strstr( gl_config.extensions_string, "WGL_EXT_swap_control" ) )
	{
		if ( gl_ext_swapinterval->value )
		{
			qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qwglGetProcAddress( "wglSwapIntervalEXT" );
			Com_Printf( "...enabling WGL_EXT_swap_control\n" );
		}
		else
		{
			Com_Printf( "...ignoring WGL_EXT_swap_control\n" );
		}
	}
	else
	{
		Com_Printf( "...WGL_EXT_swap_control not found\n" );
	}
#endif

	if ( strstr( gl_config.extensions_string, "GL_ARB_multitexture" ) )
	{
		if ( gl_ext_multitexture->value )
		{
			Com_Printf( "...using GL_ARB_multitexture\n" );
			qglMTexCoord2fSGIS = ( void * ) qwglGetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ( void * ) qwglGetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( void * ) qwglGetProcAddress( "glClientActiveTextureARB" );
			GL_TEXTURE_0 = GL_TEXTURE0_ARB;
			GL_TEXTURE_1 = GL_TEXTURE1_ARB;
		}
		else
		{
			Com_Printf( "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		Com_Printf( "...GL_ARB_multitexture not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_SGIS_multitexture" ) )
	{
		if ( qglActiveTextureARB )
		{
			Com_Printf( "...GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n" );
		}
		else if ( gl_ext_multitexture->value )
		{
			Com_Printf( "...using GL_SGIS_multitexture\n" );
			qglMTexCoord2fSGIS = ( void * ) qwglGetProcAddress( "glMTexCoord2fSGIS" );
			qglSelectTextureSGIS = ( void * ) qwglGetProcAddress( "glSelectTextureSGIS" );
			GL_TEXTURE_0 = GL_TEXTURE0_SGIS;
			GL_TEXTURE_1 = GL_TEXTURE1_SGIS;
		}
		else
		{
			Com_Printf( "...ignoring GL_SGIS_multitexture\n" );
		}
	}
	else
	{
		Com_Printf( "...GL_SGIS_multitexture not found\n" );
	}

#ifdef _WIN32
	if( strstr( gl_config.extensions_string, "WGL_3DFX_gamma_control" )) {
		if( !r_ignorehwgamma->value ) 
		{
			qwglGetDeviceGammaRamp3DFX	= ( BOOL (WINAPI *)(HDC, WORD *)) qwglGetProcAddress( "wglGetDeviceGammaRamp3DFX" );
			qwglSetDeviceGammaRamp3DFX	= ( BOOL (WINAPI *)(HDC, WORD *)) qwglGetProcAddress( "wglSetDeviceGammaRamp3DFX" );
			Com_Printf( "...enabling WGL_3DFX_gamma_control\n" );
		} 
		else 
		{
			Com_Printf( "...ignoring WGL_3DFX_gamma_control\n" );
		}
	} 
	else 
	{
		Com_Printf( "...WGL_3DFX_gamma_control not found\n" );
	}
#endif

	if ( strstr( gl_config.extensions_string, "GL_ARB_texture_env_add" ) )
	{
		Com_Printf( "...using GL_ARB_texture_env_add\n" );
		gl_config.env_add = qtrue;
	}
	else
	{
		Com_Printf( "...GL_ARB_texture_env_add not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_ARB_texture_env_combine" ) )
	{
		if ( gl_ext_texture_env_combine->value && qglMTexCoord2fSGIS )
		{
			Com_Printf( "...using GL_ARB_texture_env_combine\n" );
			gl_config.tex_env_combine = qtrue;
		}
		else
		{
			Com_Printf( "...ignoring GL_ARB_texture_env_combine\n" );
		}
	}
	else
	{
		Com_Printf( "...GL_ARB_texture_env_combine not found\n" );
	}

	if ( !gl_config.tex_env_combine )
	{
		if ( strstr( gl_config.extensions_string, "GL_EXT_texture_env_combine" ) )
		{
			if ( gl_ext_texture_env_combine->value && qglMTexCoord2fSGIS )
			{
				Com_Printf( "...using GL_EXT_texture_env_combine\n" );
				gl_config.tex_env_combine = qtrue;
			}
			else
			{
				Com_Printf( "...ignoring GL_EXT_texture_env_combine\n" );
			}
		}
		else
		{
			Com_Printf( "...GL_EXT_texture_env_combine not found\n" );
		}
	}

	if ( strstr( gl_config.extensions_string, "NV_texture_env_combine4" ) )
	{
		if ( gl_ext_NV_texture_env_combine4->value && qglMTexCoord2fSGIS )
		{
			Com_Printf( "...using NV_texture_env_combine4\n" );
			gl_config.nv_tex_env_combine4 = qtrue;
		}
		else
		{
			Com_Printf( "...ignoring NV_texture_env_combine4\n" );
		}
	}
	else
	{
		Com_Printf( "...NV_texture_env_combine4 not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_SGIS_generate_mipmap" ) )
	{
		if ( gl_ext_sgis_mipmap->value )
		{
			Com_Printf ( "...using GL_SGIS_generate_mipmap\n" );
			gl_config.sgis_mipmap = qtrue;
		}
		else
		{
			Com_Printf( "...ignoring GL_SGIS_generate_mipmap\n" );
		}
	} 
	else 
	{
		Com_Printf ( "...GL_SGIS_generate_mipmap not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_ARB_texture_compression" ) )
	{
		if ( gl_ext_compressed_textures->value )
		{
			Com_Printf ( "...using GL_ARB_texture_compression\n" );
			gl_config.compressed_textures = qtrue;
		}
		else
		{
			Com_Printf ( "...ignoring GL_ARB_texture_compression\n" );
		}
	}
	else
	{
		Com_Printf ( "...GL_ARB_texture_compression not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_EXT_bgra" ) )
	{
		if ( gl_ext_bgra->value )
		{
			Com_Printf ( "...using GL_EXT_bgra\n" );
			gl_config.bgra = qtrue;
		}
		else
		{
			Com_Printf ( "...ignoring GL_EXT_bgra\n" );
		}
	}
	else
	{
		Com_Printf ( "...GL_EXT_bgra not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_EXT_texture_edge_clamp" ) )
	{
		if ( gl_ext_texture_edge_clamp->value )
		{
			Com_Printf ( "...using GL_EXT_texture_edge_clamp\n" );
			gl_config.texture_edge_clamp = qtrue;
		}
		else
		{
			Com_Printf ( "...ignoring GL_EXT_texture_edge_clamp\n" );
		}
	}
	else
	{
		Com_Printf ( "...GL_EXT_texture_edge_clamp not found\n" );

		if ( strstr( gl_config.extensions_string, "GL_SGIS_texture_edge_clamp" ) )
		{
			if ( gl_ext_texture_edge_clamp->value )
			{
				Com_Printf ( "...using GL_SGIS_texture_edge_clamp\n" );
				gl_config.texture_edge_clamp = qtrue;
			}
			else
			{
				Com_Printf ( "...ignoring GL_SGIS_texture_edge_clamp\n" );
			}
		}
		else
		{
			Com_Printf ( "...GL_SGIS_texture_edge_clamp not found\n" );
		}
	}

	qglGetFloatv ( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_config.max_texture_filter_anisotropic );
	Cvar_ForceSet ( "gl_ext_max_texture_filter_anisotropic", va ("%1.1f", gl_config.max_texture_filter_anisotropic) );

	if ( strstr( gl_config.extensions_string, "GL_EXT_texture_filter_anisotropic" ) )
	{
		if ( gl_ext_texture_filter_anisotropic->value )
		{
			Com_Printf ( "...using GL_EXT_texture_filter_anisotropic (max %1.1f)\n", gl_config.max_texture_filter_anisotropic );
			gl_config.texture_filter_anisotropic = qtrue;
		}
		else
		{
			Com_Printf ( "...ignoring GL_EXT_texture_filter_anisotropic\n" );
		}
	}
	else
	{
		Com_Printf ( "...GL_EXT_texture_filter_anisotropic not found\n" );
	}
}

/*
===============
R_Init
===============
*/
int R_Init( void *hinstance, void *hWnd )
{	
	char renderer_buffer[1000];
	char vendor_buffer[1000];
	int	 err;

	Com_Printf( "ref_gl version: "REF_VERSION"\n");

	R_Register();

	// initialize our QGL dynamic bindings
	if ( !QGL_Init( gl_driver->string ) )
	{
		QGL_Shutdown();
        Com_Printf( "ref_gl::R_Init() - could not load \"%s\"\n", gl_driver->string );
		return -1;
	}

	// initialize OS-specific parts of OpenGL
	if ( !GLimp_Init( hinstance, hWnd ) )
	{
		QGL_Shutdown();
		return -1;
	}

	// set our "safe" modes
	gl_state.prev_mode = 3;

	if ( !r_ignorehwgamma->value ) 
		gl_state.inv_pow2_ovrbr = 1.0 / pow ( 2, max(0, floor(r_overbrightbits->value)) );
	else
		gl_state.inv_pow2_ovrbr = 1.0f;

	gl_state.pow2_mapovrbr = floor( r_mapoverbrightbits->value - r_overbrightbits->value );
	if ( gl_state.pow2_mapovrbr > 0 ) {
		gl_state.pow2_mapovrbr = pow( 2, gl_state.pow2_mapovrbr ) / 255.0;
	} else {
		gl_state.pow2_mapovrbr = 1.0 / 255.0;
	}

	// create the window and set up the context
	if ( !R_SetMode () )
	{
		QGL_Shutdown();
        Com_Printf( "ref_gl::R_Init() - could not R_SetMode()\n" );
		return -1;
	}

	/*
	** get our various GL strings
	*/
	gl_config.vendor_string = qglGetString (GL_VENDOR);
	Com_Printf( "GL_VENDOR: %s\n", gl_config.vendor_string );
	gl_config.renderer_string = qglGetString (GL_RENDERER);
	Com_Printf( "GL_RENDERER: %s\n", gl_config.renderer_string );
	gl_config.version_string = qglGetString (GL_VERSION);
	Com_Printf( "GL_VERSION: %s\n", gl_config.version_string );
	gl_config.extensions_string = qglGetString (GL_EXTENSIONS);
	Com_Printf( "GL_EXTENSIONS: %s\n", gl_config.extensions_string );

	strcpy( renderer_buffer, gl_config.renderer_string );
	strlwr( renderer_buffer );

	strcpy( vendor_buffer, gl_config.vendor_string );
	strlwr( vendor_buffer );

	if ( strstr( renderer_buffer, "voodoo" ) )
	{
		if ( !strstr( renderer_buffer, "rush" ) )
			gl_config.renderer = GL_RENDERER_VOODOO;
		else
			gl_config.renderer = GL_RENDERER_VOODOO_RUSH;
	}
	else if ( strstr( vendor_buffer, "sgi" ) )
		gl_config.renderer = GL_RENDERER_SGI;
	else if ( strstr( renderer_buffer, "permedia" ) )
		gl_config.renderer = GL_RENDERER_PERMEDIA2;
	else if ( strstr( renderer_buffer, "glint" ) )
		gl_config.renderer = GL_RENDERER_GLINT_MX;
	else if ( strstr( renderer_buffer, "glzicd" ) )
		gl_config.renderer = GL_RENDERER_REALIZM;
	else if ( strstr( renderer_buffer, "gdi" ) )
		gl_config.renderer = GL_RENDERER_MCD;
	else if ( strstr( renderer_buffer, "pcx2" ) )
		gl_config.renderer = GL_RENDERER_PCX2;
	else if ( strstr( renderer_buffer, "verite" ) )
		gl_config.renderer = GL_RENDERER_RENDITION;
//	else if ( strstr( renderer, "rage pro" ) || strstr( renderer, "Rage Pro" ) )
//		gl_config.renderer = GL_RENDERER_RAGEPRO;
	else
		gl_config.renderer = GL_RENDERER_OTHER;

#ifdef GL_FORCEFINISH
	Cvar_SetValue( "gl_finish", 1 );
#endif

	// MCD has buffering issues
	if ( gl_config.renderer == GL_RENDERER_MCD )
	{
		Cvar_SetValue( "gl_finish", 1 );
	}

	if ( gl_config.renderer & GL_RENDERER_3DLABS )
	{
		if ( r_3dlabs_broken->value )
			gl_config.allow_cds = qfalse;
		else
			gl_config.allow_cds = qtrue;
	}
	else
	{
		gl_config.allow_cds = qtrue;
	}

	if ( gl_config.allow_cds )
		Com_Printf( "...allowing CDS\n" );
	else
		Com_Printf( "...disabling CDS\n" );

	GL_CheckExtensions ();

	GL_SetDefaultState ();

	/*
	** draw our stereo patterns
	*/
#if 0 // commented out until H3D pays us the money they owe us
	GL_DrawStereoPattern ();
#endif

	GL_InitImages ();
	Mod_Init ();

	R_InitBuiltInTextures ();
	R_InitBubble ();

	R_SkinFile_Init ();

	Shader_Init ();

	Draw_InitLocal ();

	R_InitPolys ();

	R_InitSpriteModels ();
	R_InitAliasModels ();
	R_InitSkeletalModels ();

	R_InitSkydome ();

	R_BackendInit ();

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		Com_Printf( "glGetError() = 0x%x\n", err);

	return qfalse;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown (void)
{	
	Cmd_RemoveCommand ("modellist");
	Cmd_RemoveCommand ("screenshot");
	Cmd_RemoveCommand ("imagelist");
	Cmd_RemoveCommand ("gl_strings");

	Mod_Shutdown ();

	R_BackendShutdown ();

	R_SkinFile_Shutdown ();

	Shader_Shutdown ();

	GL_ShutdownImages ();

	/*
	** shut down OS specific OpenGL stuff like contexts, etc.
	*/
	GLimp_Shutdown();

	/*
	** shutdown our QGL subsystem
	*/
	QGL_Shutdown();
}

/*
===============
R_Flush
===============
*/
void R_Flush (void)
{
	qglFlush ();
}

/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginFrame
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginFrame( float camera_separation )
{
	gl_state.camera_separation = camera_separation;

	if ( gl_log->modified )
	{
		GLimp_EnableLogging( gl_log->value );
		gl_log->modified = qfalse;
	}

	if ( gl_log->value )
	{
		GLimp_LogNewFrame();
	}

	/*
	** update gamma
	*/
	if ( vid_gamma->modified )
	{
		vid_gamma->modified = qfalse;
		GLimp_UpdateGammaRamp ();
	}

	GLimp_BeginFrame( camera_separation );

	/*
	** go into 2D mode
	*/
	R_SetGL2D ();

	/*
	** draw buffer stuff
	*/
	if ( gl_drawbuffer->modified )
	{
		gl_drawbuffer->modified = qfalse;

		if ( gl_state.camera_separation == 0 || !gl_state.stereo_enabled )
		{
			if ( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 )
				qglDrawBuffer( GL_FRONT );
			else
				qglDrawBuffer( GL_BACK );
		}
	}

	/*
	** texturemode stuff
	*/
	if ( r_texturemode->modified )
	{
		GL_TextureMode( r_texturemode->string );
		r_texturemode->modified = qfalse;
	}

	/*
	** swapinterval stuff
	*/
	GL_UpdateSwapInterval();
}
