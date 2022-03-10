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

int			r_entvisframe[MAX_ENTITIES][2];

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

cplane_t	frustum[4];

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

mat4_t		r_modelview_matrix;
mat4_t		r_projection_matrix;

//
// screen size info
//
refdef_t	r_newrefdef;

int			r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;
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

cvar_t	*r_colorbits;
cvar_t	*r_stencilbits;
cvar_t	*r_mode;
cvar_t	*r_picmip;
cvar_t	*r_skymip;
cvar_t	*r_lightmap;
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

cvar_t	*gl_log;
cvar_t	*gl_drawbuffer;
cvar_t  *gl_driver;
cvar_t	*gl_finish;
cvar_t	*gl_cull;
cvar_t	*gl_swapinterval;
cvar_t	*gl_texturemode;
cvar_t	*gl_texturealphamode;
cvar_t	*gl_texturesolidmode;

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;
	cplane_t *p;

	if (r_nocull->value)
		return false;

	for (i=0,p=frustum ; i<4; i++,p++)
	{
		switch (p->signbits)
		{
		case 0:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		default:
			assert( 0 );
			return false;
		}
	}

	return false;
}

/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
qboolean R_CullSphere (vec3_t centre, float radius)
{
	int		i;
	cplane_t *p;

	if (r_nocull->value)
		return false;

	for (i=0,p=frustum ; i<4; i++,p++)
	{
		if ( DotProduct ( centre, p->normal ) - p->dist <= -radius )
			return true;
	}

	return false;
}

/*
=============
R_FogForSphere
=============
*/
mfog_t *R_FogForSphere( vec3_t centre, float radius )
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
	mat4_t obj_m, m;

	if ( e->scale != 1.0f ) {
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

	Matrix4_MultiplyFast ( r_modelview_matrix, obj_m, m );

	qglLoadMatrixf ( m );
}

void R_TranslateForEntity (entity_t *e)
{
	mat4_t obj_m, m;

	Matrix4_Identity ( obj_m );

	obj_m[12] = e->origin[0];
	obj_m[13] = e->origin[1];
	obj_m[14] = e->origin[2];

	Matrix4_MultiplyFast ( r_modelview_matrix, obj_m, m );

	qglLoadMatrixf ( m );
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

static	vec4_t			spr_xyz[4];
static	vec2_t			spr_st[4];

static	vec3_t			spr_mins, spr_maxs;
static	mesh_t			spr_mesh;
static	meshbuffer_t	spr_mbuffer;

/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (meshbuffer_t *mb)
{
	vec3_t		point;
	int			features;
	float		*up, *right;
	sframe_t	*frame;
	smodel_t	*psprite;
	entity_t	*e = mb->entity;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = e->model->smodel;
	frame = psprite->frames + e->frame;

	{	// normal sprite
		up = vup;
		right = vright;
	}

	VectorScale (up, -frame->origin_y, point);
	VectorMA (point, -frame->origin_x, right, spr_mesh.xyz_array[0]);

	VectorScale (up, frame->height - frame->origin_y, point);
	VectorMA (point, -frame->origin_x, right, spr_mesh.xyz_array[1]);

	VectorScale (up, frame->height - frame->origin_y, point);
	VectorMA (point, frame->width - frame->origin_x, right, spr_mesh.xyz_array[2]);

	VectorScale (up, -frame->origin_y, point);
	VectorMA (point, frame->width - frame->origin_x, right, spr_mesh.xyz_array[3]);
	
	spr_mbuffer.shader = mb->shader;
	spr_mbuffer.entity = e;
	spr_mbuffer.fog = mb->fog;

	features = MF_NONBATCHED | mb->shader->features;
	if ( r_shownormals->value ) {
		features |= MF_NORMALS;
	}

	R_TranslateForEntity ( e );

	R_PushMesh ( &spr_mesh, features );
	R_RenderMeshBuffer ( &spr_mbuffer, false );
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

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	if ( !(psprite = e->model->smodel) ) {
		return;
	}

	e->frame %= psprite->numframes;
	frame = psprite->frames + e->frame;

	// select skin
	if ( e->customShader ) {
		R_AddMeshToBuffer ( NULL, R_FogForSphere ( e->origin, frame->radius ), NULL, e->customShader, 0 );
	} else {
		R_AddMeshToBuffer ( NULL, R_FogForSphere ( e->origin, frame->radius ), NULL, frame->shader, 0 );
	}
}

/*
=================
R_DrawSpritePoly

=================
*/
void R_DrawSpritePoly (meshbuffer_t *mb)
{
	vec3_t		point;
	int			features;
	float		*up, *right;
	entity_t	*e = mb->entity;

	{	// normal sprite
		up = vup;
		right = vright;
	}

	VectorAdd (up, right, point);
	VectorNormalizeFast (point);
	VectorScale (point, e->radius, point);
	VectorCopy (point, spr_mesh.xyz_array[0]);

	VectorNegate (point, spr_mesh.xyz_array[2]);

	VectorSubtract ( up, right, point );
	VectorNormalizeFast ( point );
	VectorScale ( point, e->radius, point );
	VectorCopy ( point, spr_mesh.xyz_array[1] );

	VectorNegate ( point, spr_mesh.xyz_array[3] );
	
	spr_mbuffer.shader = mb->shader;
	spr_mbuffer.entity = e;
	spr_mbuffer.fog = mb->fog;

	features = MF_NONBATCHED | mb->shader->features;
	if ( r_shownormals->value ) {
		features |= MF_NORMALS;
	}

	R_TranslateForEntity ( e );

	R_PushMesh ( &spr_mesh, features );
	R_RenderMeshBuffer ( &spr_mbuffer, false );
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

	R_AddMeshToBuffer ( NULL, R_FogForSphere ( e->origin, e->radius ), NULL, e->customShader, 0 );
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

	spr_mesh.st_array[0][0] = 0;
	spr_mesh.st_array[0][1] = 0;
	spr_mesh.st_array[1][0] = 1;
	spr_mesh.st_array[1][1] = 0;
	spr_mesh.st_array[2][0] = 1;
	spr_mesh.st_array[2][1] = 1;
	spr_mesh.st_array[3][0] = 0;
	spr_mesh.st_array[3][1] = 1;

	spr_mbuffer.mesh = &spr_mesh;
}

/*
=============================================================

  LENS FLARES

=============================================================
*/

static	vec4_t			flare_xyz[4];
static	vec2_t			flare_st[4];
static	vec4_t			flare_color[4];

static	mesh_t			flare_mesh;
static	meshbuffer_t	flare_mbuffer;

/*
=================
R_PushFlare
=================
*/
void R_PushFlare ( meshbuffer_t *mb )
{
	vec3_t origin, v;
	vec4_t color;
	float *up, *right;
	float radius = r_flaresize->value, flarescale;

	VectorCopy (mb->mesh->xyz_array[0], origin);

	{	// normal sprite
		up = vup;
		right = vright;
	}

	VectorAdd (up, right, v);
	VectorNormalizeFast (v);
	VectorScale (v, radius, v);
	VectorAdd (origin, v, flare_mesh.xyz_array[0]);

	VectorSubtract (origin, v, flare_mesh.xyz_array[2]);

	VectorSubtract (up, right, v);
	VectorNormalizeFast (v);
	VectorScale (v, radius, v);
	VectorAdd (origin, v, flare_mesh.xyz_array[1]);

	VectorSubtract (origin, v, flare_mesh.xyz_array[3]);

	flarescale = (1.0 / 255.0) / r_flarefade->value;
	color[0] = (( mb->dlightbits	   ) & 0xFF) * flarescale;
	color[1] = (( mb->dlightbits >> 8  ) & 0xFF) * flarescale;
	color[2] = (( mb->dlightbits >> 16 ) & 0xFF) * flarescale;
	color[3] = 1.0f;

	VectorCopy ( color, flare_mesh.colors_array[0] );
	VectorCopy ( color, flare_mesh.colors_array[1] );
	VectorCopy ( color, flare_mesh.colors_array[2] );
	VectorCopy ( color, flare_mesh.colors_array[3] );

	flare_mbuffer.fog = mb->fog;
	flare_mbuffer.shader = mb->shader;
	flare_mbuffer.entity = mb->entity;

	R_PushMesh  ( &flare_mesh, mb->shader->features );
}

/*
=================
R_InitFlares
=================
*/
void R_InitFlares (void)
{
	flare_mesh.numindexes = 6;
	flare_mesh.indexes = r_quad_indexes;

	flare_mesh.numvertexes = 4;
	flare_mesh.xyz_array = flare_xyz;
	flare_mesh.st_array = flare_st;
	flare_mesh.colors_array = flare_color;

	flare_mesh.st_array[0][0] = 0;
	flare_mesh.st_array[0][1] = 0;
	flare_mesh.st_array[1][0] = 1;
	flare_mesh.st_array[1][1] = 0;
	flare_mesh.st_array[2][0] = 1;
	flare_mesh.st_array[2][1] = 1;
	flare_mesh.st_array[3][0] = 0;
	flare_mesh.st_array[3][1] = 1;

	flare_mbuffer.mesh = &flare_mesh;
}


/*
** R_DrawBeam
*/
#define NUM_BEAM_SEGS	6

void R_DrawBeam( entity_t *e )
{
	int	i;
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, e->radius, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	qglColor4ubv( e->color );

	qglBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		qglVertex3fv( start_points[i] );
		qglVertex3fv( end_points[i] );
		qglVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] );
		qglVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] );
	}
	qglEnd();
}

//==================================================================================

/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel (void)
{
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
}

/*
=============
R_SortEntitiesOnList
=============
*/
void R_SortEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->value)
		return;

	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if ( r_mirrorview ) {
			if ( currententity->flags & RF_WEAPONMODEL ) 
				continue;
		}

		switch ( currententity->type )
		{
			case ET_MODEL:
				if ( !(currentmodel = currententity->model) ) {
					break;
				}

				switch (currentmodel->type)
				{
				case mod_alias:
					R_AddAliasModelToList (currententity);
					break;
				case mod_dpm:
					R_AddDarkPlacesModelToList (currententity);
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
			
			case ET_SPRITE:
				if ( currententity->radius ) {
					R_AddSpritePolyToList ( currententity );
				}
				break;

			case ET_BEAM:
				break;

			case ET_PORTALSURFACE:
				break;

			default:
				Com_Error (ERR_DROP, "%s: bad entitytype", currententity->type);
				break;
		}
	}
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->value)
		return;

	GL_EnableMultitexture ( false );
	qglDepthFunc ( GL_LEQUAL );
	qglDepthMask ( GL_FALSE );
	qglDisable (GL_TEXTURE_2D);
	GLSTATE_DISABLE_ALPHATEST;
	GLSTATE_ENABLE_BLEND;
	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	// draw non-transparent first
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if ( !r_portalview && !r_mirrorview ) {
			r_entvisframe[currententity->number][0] = r_entvisframe[currententity->number][1] = 0;
		}
		
		if ( r_mirrorview ) {
			if ( currententity->flags & RF_WEAPONMODEL ) 
				continue;
		} else {
			if ( currententity->flags & RF_VIEWERMODEL ) 
				continue;
		}

		switch ( currententity->type )
		{
			case ET_MODEL:
				if ( !(currentmodel = currententity->model) ) {
					R_DrawNullModel ();
				}
				break;

			case ET_SPRITE:
				break;

			case ET_BEAM:
				R_DrawBeam( currententity );
				break;

			case ET_PORTALSURFACE:
				break;
		}
	}

	GLSTATE_DISABLE_BLEND;
	qglEnable (GL_TEXTURE_2D);
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	const particle_t *p;
	int				i;
	vec3_t			r_pup, r_pright;
	float			scale;
	byte_vec4_t		pcolor;

	if( !r_newrefdef.particles )
		return;

	VectorScale ( vup, 1.5f, r_pup );
	VectorScale ( vright, 1.5f, r_pright );

	GL_EnableMultitexture ( false );
    GL_Bind( r_particletexture->texnum );
	qglDepthMask( GL_FALSE );		// no z buffering
	GLSTATE_DISABLE_ALPHATEST;
	GLSTATE_ENABLE_BLEND;
	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglBegin ( GL_TRIANGLES );

	for ( i=0,p = r_newrefdef.particles ; i < r_newrefdef.num_particles ; i++, p++ )
	{
		// hack a scale up to keep particles from disapearing
		scale = ( p->origin[0] - r_origin[0] ) * vpn[0] + 
			( p->origin[1] - r_origin[1] ) * vpn[1] +
			( p->origin[2] - r_origin[2] ) * vpn[2];

		if (scale <= 0) {
			continue;
		} else if (scale < 20) {
			scale = 1;
		} else {
			scale = 1 + scale * 0.004f;
		}

		*(int *)pcolor = d_8to24table[p->color];
		pcolor[3] = p->alpha*255;

		qglColor4ubv ( pcolor );

		qglTexCoord2f( 0.0625, 0.0625 );
		qglVertex3fv( p->origin );

		qglTexCoord2f( 1.0625, 0.0625 );
		qglVertex3f( p->origin[0] + r_pup[0]*scale, 
					 p->origin[1] + r_pup[1]*scale, 
					 p->origin[2] + r_pup[2]*scale);

		qglTexCoord2f( 0.0625, 1.0625 );
		qglVertex3f( p->origin[0] + r_pright[0]*scale, 
					 p->origin[1] + r_pright[1]*scale, 
					 p->origin[2] + r_pright[2]*scale);
	}

	qglEnd ();
	qglColor4f ( 1, 1, 1, 1 );
	GLSTATE_DISABLE_BLEND;
	qglDepthMask( GL_TRUE );		// back to normal Z buffering
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

	GLSTATE_ENABLE_BLEND;
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

	GLSTATE_DISABLE_BLEND;
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

	GLSTATE_ENABLE_BLEND;
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
	GLSTATE_DISABLE_BLEND;
	qglEnable (GL_TEXTURE_2D);

	qglColor4f(1,1,1,1);
}

void R_ShadowBlend (void)
{
	if ( r_shadows->value != 1 || !gl_state.stencil_enabled ) {
		return;
	}

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, 1, 1, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	GLSTATE_DISABLE_ALPHATEST;
	GLSTATE_ENABLE_BLEND;
	GLSTATE_DISABLE_CULL;
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

	qglColor4f ( 0, 0, 0, bound (0.0f, r_shadows_alpha->value, 1.0f) );

	GLSTATE_ENABLE_STENCIL
	qglStencilFunc (GL_NOTEQUAL, 128, 0xFF);
	qglStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);

	qglBegin (GL_TRIANGLES);
	qglVertex2f (-5, -5);
	qglVertex2f (10, -5);
	qglVertex2f (-5, 10);
	qglEnd ();

	GLSTATE_DISABLE_STENCIL;
	GLSTATE_DISABLE_BLEND;
	qglEnable (GL_TEXTURE_2D);
	GLSTATE_ENABLE_ALPHATEST;

	qglColor4f(1,1,1,1);
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
		frustum[i].type = PLANE_ANYZ;
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
	mleaf_t	*leaf;

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_newrefdef.vieworg, r_origin);

	if ( !r_portalview )
		AngleVectors (r_newrefdef.viewangles, vpn, vright, vup);

// current viewcluster
	if ( !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) && !r_mirrorview )
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		if (r_portalview)
			leaf = Mod_PointInLeaf (r_portalorg, r_worldbmodel);
		else
			leaf = Mod_PointInLeaf (r_origin, r_worldbmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (temp, r_worldbmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (temp, r_worldbmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
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

	if (r_mirrorview)
	{
		if ( r_clipplane.normal[2] )
			qglScalef (  1, -1, 1 );
		else
			qglScalef ( -1,  1, 1 );
	}

	qglGetFloatv (GL_PROJECTION_MATRIX, r_projection_matrix);

	qglMatrixMode (GL_MODELVIEW);

modelview:
#ifndef HARDWARE_TRANSFORMS
	Matrix4_Identity ( r_modelview_matrix );
	Matrix4_Rotate ( r_modelview_matrix, -90, 1, 0, 0 );
	Matrix4_Rotate ( r_modelview_matrix, 90, 0, 0, 1 );
	Matrix4_Rotate ( r_modelview_matrix, -r_newrefdef.viewangles[2], 1, 0, 0 );
	Matrix4_Rotate ( r_modelview_matrix, -r_newrefdef.viewangles[0], 0, 1, 0 );
	Matrix4_Rotate ( r_modelview_matrix, -r_newrefdef.viewangles[1], 0, 0, 1 );
	Matrix4_Translate ( r_modelview_matrix, -r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2] );

	qglLoadMatrixf ( r_modelview_matrix );
#else
	qglLoadIdentity ();

    qglRotatef (-90,1, 0, 0);	    // put Z going up
    qglRotatef (90, 0, 0, 1);	    // put Z going up
    qglRotatef (-r_newrefdef.viewangles[2], 1, 0, 0);
    qglRotatef (-r_newrefdef.viewangles[0], 0, 1, 0);
    qglRotatef (-r_newrefdef.viewangles[1], 0, 0, 1);
    qglTranslatef (-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2]);

	qglGetFloatv (GL_MODELVIEW_MATRIX, r_modelview_matrix);
#endif

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
	if ( gl_state.stencil_enabled ) {
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
	currentlist = list;

	if ( (!r_worldmodel || !r_worldbmodel) && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		Com_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	if ( (r_mirrorview || r_portalview) && r_fastsky->value ) {
		goto done;
	}

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();

	R_SortEntitiesOnList ();

	R_DrawSortedMeshes ();

	R_DrawEntitiesOnList ();

	R_RenderDlights ();

	R_DrawParticles ();

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
	GLSTATE_DISABLE_CULL;
	qglColor4f (1, 1, 1, 1);
	gl_state.in2d = true;
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

	Matrix4_Multiply_Vec3 ( r_modelview_matrix, in, temp );
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
	gl_state.in2d = false;

	if ( !r_norefresh->value ) {
		R_BackendStartFrame ();

		if (gl_finish->value)
			qglFinish ();

		r_mirrorview = false;
		r_portalview = false;

		c_brush_polys = 0;
		c_world_leafs = 0;

		R_RenderView( fd, &r_worldlist );
		R_Flash();

		R_BackendEndFrame ();
	}

	R_SetGL2D ();
}


void R_Register( void )
{
	Cvar_GetLatchedVars (CVAR_LATCH_VIDEO);

	r_lefthand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_norefresh = Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = Cvar_Get ("r_fullbright", "0", CVAR_CHEAT);
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_CHEAT);
	r_drawworld = Cvar_Get ("r_drawworld", "1", CVAR_CHEAT);
	r_lightmap = Cvar_Get ("r_lightmap", "0", CVAR_CHEAT);
	r_novis = Cvar_Get ("r_novis", "0", 0);
	r_nocull = Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = Cvar_Get ("r_speeds", "0", 0);

	r_fastsky = Cvar_Get ("r_fastsky", "0", CVAR_ARCHIVE);
	r_ignorehwgamma = Cvar_Get ("r_ignorehwgamma", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
	r_overbrightbits = Cvar_Get ("r_overbrightbits", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO);
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

	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_LATCH_VIDEO );
	r_stencilbits = Cvar_Get( "r_stencilbits", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_mode = Cvar_Get( "r_mode", "3", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_nobind = Cvar_Get ("r_nobind", "0", 0);
	r_picmip = Cvar_Get ("r_picmip", "0", CVAR_LATCH_VIDEO);
	r_skymip = Cvar_Get ("r_skymip", "1", CVAR_LATCH_VIDEO);
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
	gl_texturemode = Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	gl_texturealphamode = Cvar_Get( "gl_texturealphamode", "default", CVAR_ARCHIVE );
	gl_texturesolidmode = Cvar_Get( "gl_texturesolidmode", "default", CVAR_ARCHIVE );

	gl_extensions = Cvar_Get( "gl_extensions", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_swapinterval = Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_multitexture = Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_compiled_vertex_array = Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_sgis_mipmap = Cvar_Get( "gl_ext_sgis_mipmap", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_env_combine = Cvar_Get( "gl_ext_texture_env_combine", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_NV_texture_env_combine4 = Cvar_Get( "gl_ext_NV_texture_env_combine4", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_compressed_textures = Cvar_Get( "gl_ext_compressed_textures", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );
	gl_swapinterval = Cvar_Get( "gl_swapinterval", "0", CVAR_ARCHIVE );

	vid_fullscreen = Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );

	Cmd_AddCommand( "imagelist", GL_ImageList_f );
	Cmd_AddCommand( "screenshot", GL_ScreenShot_f );
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
		vid_fullscreen->modified = false;
	}

	fullscreen = vid_fullscreen->value;

	vid_fullscreen->modified = false;
	r_mode->modified = false;

	if ( ( err = GLimp_SetMode( &vid.width, &vid.height, r_mode->value, fullscreen ) ) == rserr_ok )
	{
		gl_state.prev_mode = r_mode->value;
	}
	else
	{
		if ( err == rserr_invalid_fullscreen )
		{
			Cvar_SetValue( "vid_fullscreen", 0);
			vid_fullscreen->modified = false;
			Com_Printf( "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n" );
			if ( ( err = GLimp_SetMode( &vid.width, &vid.height, r_mode->value, false ) ) == rserr_ok )
				return true;
		}
		else if ( err == rserr_invalid_mode )
		{
			Cvar_SetValue( "r_mode", gl_state.prev_mode );
			r_mode->modified = false;
			Com_Printf( "ref_gl::R_SetMode() - invalid mode\n" );
		}

		// try setting it back to something safe
		if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_state.prev_mode, false ) ) != rserr_ok )
		{
			Com_Printf( "ref_gl::R_SetMode() - could not revert to safe mode\n" );
			return false;
		}
	}
	return true;
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

	gl_config.env_add = false;
	gl_config.tex_env_combine = false;
	gl_config.sgis_mipmap = false;
	gl_config.nv_tex_env_combine4 = false;
	gl_config.compressed_textures = false;

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
		gl_config.env_add = true;
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
			gl_config.tex_env_combine = true;
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
				gl_config.tex_env_combine = true;
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
			gl_config.nv_tex_env_combine4 = true;
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
			gl_config.sgis_mipmap = true;
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
			gl_config.compressed_textures = true;
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

	Draw_GetPalette ();

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

	gl_state.inv_pow2_mapovrbr = floor( r_mapoverbrightbits->value - r_overbrightbits->value );
	if ( gl_state.inv_pow2_mapovrbr > 0 ) {
		gl_state.inv_pow2_mapovrbr = pow( 2, gl_state.inv_pow2_mapovrbr ) / 255.0;
	} else {
		gl_state.inv_pow2_mapovrbr = 1.0 / 255.0;
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


	// power vr can't have anything stay in the framebuffer, so
	// the screen needs to redraw the tiled background every frame
	if ( gl_config.renderer & GL_RENDERER_POWERVR ) 
	{
		Cvar_Set( "scr_drawall", "1" );
	}
	else
	{
		Cvar_Set( "scr_drawall", "0" );
	}

#ifdef __linux__
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
			gl_config.allow_cds = false;
		else
			gl_config.allow_cds = true;
	}
	else
	{
		gl_config.allow_cds = true;
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

	R_InitFlares ();

	R_InitSpriteModels ();
	R_InitAliasModels ();
	R_InitDarkPlacesModels ();

	R_InitSkydome ();

	Shader_Init ();

	Draw_InitLocal ();

	R_BackendInit ();

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		Com_Printf( "glGetError() = 0x%x\n", err);

	return false;
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

	Mod_FreeAll ();

	R_BackendShutdown ();

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
		gl_log->modified = false;
	}

	if ( gl_log->value )
	{
		GLimp_LogNewFrame();
	}

	/*
	** update 3Dfx gamma -- it is expected that a user will do a vid_restart
	** after tweaking this value
	*/
	if ( vid_gamma->modified )
	{
		vid_gamma->modified = false;
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
		gl_drawbuffer->modified = false;

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
	if ( gl_texturemode->modified )
	{
		GL_TextureMode( gl_texturemode->string );
		gl_texturemode->modified = false;
	}

	if ( gl_texturealphamode->modified )
	{
		GL_TextureAlphaMode( gl_texturealphamode->string );
		gl_texturealphamode->modified = false;
	}

	if ( gl_texturesolidmode->modified )
	{
		GL_TextureSolidMode( gl_texturesolidmode->string );
		gl_texturesolidmode->modified = false;
	}

	/*
	** swapinterval stuff
	*/
	GL_UpdateSwapInterval();
}
