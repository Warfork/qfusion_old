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

model_t		*r_worldmodel;

meshlist_t	r_worldlist;

float		gldepthmin, gldepthmax;

glconfig_t	glConfig;
glstate_t	glState;

qboolean	r_firstTime;

image_t		*r_cintexture;		// cinematic texture
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

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_world_leafs;

int			r_mark_leaves, r_mark_lights, r_world_node;
int			r_add_polys, r_add_entities;
int			r_sort_meshes, r_draw_meshes;

void		R_GfxInfo_f( void );

//
// view origin
//
vec3_t		vup;
vec3_t		vpn;
vec3_t		vright;
vec3_t		r_origin;

qboolean	r_portalview;	// if true, get vis data at
vec3_t		r_portalorg;	// portalorg instead of vieworg

qboolean	r_mirrorview;	// if true, lock pvs
qboolean	r_envview;

cplane_t	r_clipplane;

mat4x4_t	r_worldview_matrix;
mat4x4_t	r_modelview_matrix;
mat4x4_t	r_projection_matrix;

//
// screen size info
//
int			r_numEntities;
entity_t	r_entities[MAX_ENTITIES];

int			r_numDlights;
dlight_t	r_dlights[MAX_DLIGHTS];

int			r_numPolys;
poly_t		r_polys[MAX_POLYS];

lightstyle_t	r_lightStyles[MAX_LIGHTSTYLES];

refdef_t	r_refdef;
refdef_t	r_lastRefdef;

int			r_viewcluster, r_oldviewcluster;

float		r_farclip, r_farclip_min, r_farclip_bias = 256.0f;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_lightmap;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_fastsky;
cvar_t	*r_ignorehwgamma;
cvar_t	*r_overbrightbits;
cvar_t	*r_mapoverbrightbits;
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
cvar_t	*r_draworder;
cvar_t	*r_packlightmaps;
cvar_t	*r_spherecull;
cvar_t	*r_bumpscale;
cvar_t	*r_maxlmblocksize;

cvar_t	*r_allow_software;
cvar_t	*r_3dlabs_broken;

cvar_t	*r_shadows;	
cvar_t	*r_shadows_alpha;
cvar_t	*r_shadows_nudge;
cvar_t	*r_shadows_projection_distance;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;

cvar_t	*r_stencilbits;
cvar_t	*r_gamma;
cvar_t	*r_colorbits;
cvar_t	*r_texturebits;
cvar_t	*r_texturemode;
cvar_t	*r_mode;
cvar_t	*r_picmip;
cvar_t	*r_skymip;
cvar_t	*r_nobind;
cvar_t	*r_clear;
cvar_t	*r_polyblend;
cvar_t	*r_lockpvs;
cvar_t	*r_screenshot_jpeg;
cvar_t	*r_screenshot_jpeg_quality;
cvar_t	*r_swapinterval;

cvar_t	*gl_extensions;
cvar_t	*gl_ext_multitexture;
cvar_t	*gl_ext_compiled_vertex_array;
cvar_t	*gl_ext_texture_env_add;
cvar_t	*gl_ext_texture_env_combine;
cvar_t	*gl_ext_texture_env_dot3;
cvar_t	*gl_ext_NV_texture_env_combine4;
cvar_t	*gl_ext_compressed_textures;
cvar_t	*gl_ext_texture_edge_clamp;
cvar_t	*gl_ext_texture_filter_anisotropic;
cvar_t	*gl_ext_max_texture_filter_anisotropic;
cvar_t	*gl_ext_draw_range_elements;
cvar_t	*gl_ext_vertex_buffer_object;
cvar_t	*gl_ext_texture_cube_map;
cvar_t	*gl_ext_bgra;

cvar_t	*gl_drawbuffer;
cvar_t  *gl_driver;
cvar_t	*gl_finish;
cvar_t	*gl_delayfinish;
cvar_t	*gl_cull;

cvar_t	*vid_fullscreen;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox( const vec3_t mins, const vec3_t maxs, const int clipflags )
{
	int		i;
	cplane_t *p;

	if (r_nocull->integer)
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
qboolean R_CullSphere( const vec3_t centre, const float radius, const int clipflags )
{
	int		i;
	cplane_t *p;

	if (r_nocull->integer)
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
===================
R_VisCullBox
===================
*/
qboolean R_VisCullBox( const vec3_t mins, const vec3_t maxs )
{
	int		s, stackdepth = 0;
	vec3_t	extmins, extmaxs;
	mnode_t *node, *localstack[2048];

	if( !r_worldmodel || ( r_refdef.rdflags & RDF_NOWORLDMODEL ) )
		return qfalse;
	if( r_novis->integer )
		return qfalse;

	for( s = 0; s < 3; s++ ) {
		extmins[s] = mins[s] - 4;
		extmaxs[s] = maxs[s] + 4;
	}

	for( node = r_worldmodel->nodes; ; ) {
		if( node->visframe != r_visframecount )
			goto nextNodeOnStack;
		if( !node->plane ) {
			mleaf_t *pleaf = ( mleaf_t * )node;

			if( r_refdef.areabits ) {
				if(! (r_refdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) ) {
nextNodeOnStack:
					if( !stackdepth )
						return qtrue;
					node = localstack[--stackdepth];
					continue;
				}
			}
			return qfalse;
		}

		s = BOX_ON_PLANE_SIDE( extmins, extmaxs, node->plane ) - 1;
		if( s < 2 ) {
			node = node->children[s];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof(localstack)/sizeof(mnode_t *) )
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}

	return qtrue;
}

/*
===================
R_VisCullSphere
===================
*/
qboolean R_VisCullSphere( const vec3_t origin, float radius )
{
	float	dist;
	int		stackdepth = 0;
	mnode_t *node, *localstack[2048];

	if( !r_worldmodel || ( r_refdef.rdflags & RDF_NOWORLDMODEL ) )
		return qfalse;
	if( r_novis->integer )
		return qfalse;

	radius += 4;
	for( node = r_worldmodel->nodes; ; ) {
		if( node->visframe != r_visframecount )
			goto nextNodeOnStack;
		if( !node->plane ) {
			mleaf_t *pleaf = ( mleaf_t * )node;

			if( r_refdef.areabits ) {
				if(! (r_refdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) ) {
nextNodeOnStack:
					if( !stackdepth )
						return qtrue;
					node = localstack[--stackdepth];
					continue;
				}
			}
			return qfalse;
		}

		dist = PlaneDiff( origin, node->plane );
		if( dist > radius ) {
			node = node->children[0];
			continue;
		} else if( dist < -radius ) {
			node = node->children[1];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof(localstack)/sizeof(mnode_t *) )
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}

	return qtrue;
}

/*
=============
R_FogForSphere
=============
*/
mfog_t *R_FogForSphere( const vec3_t centre, const float radius )
{
	int			i, j;
	mfog_t		*fog, *defaultFog;
	cplane_t	*plane;

	if( !r_worldmodel || (r_refdef.rdflags & RDF_NOWORLDMODEL) || !r_worldmodel->numfogs )
		return NULL;

	defaultFog = NULL;
	fog = r_worldmodel->fogs;
	for( i = 0; i < r_worldmodel->numfogs; i++, fog++ ) {
		if( !fog->shader )
			continue;
		if( !fog->visibleplane ) {
			defaultFog = fog;
			continue;
		}

		plane = fog->planes;
		for( j = 0; j < fog->numplanes; j++, plane++ ) {
			// if completely in front of face, no intersection
			if( PlaneDiff( centre, plane ) > radius )
				break;
		}

		if( j == fog->numplanes )
			return fog;
	}

	return defaultFog;
}

void R_LoadIdentity( void )
{
	Matrix4_Copy( r_worldview_matrix, r_modelview_matrix );
	qglLoadMatrixf( r_worldview_matrix );
}

void R_RotateForEntity( entity_t *e )
{
	mat4x4_t obj_m;

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

	Matrix4_MultiplyFast( r_worldview_matrix, obj_m, r_modelview_matrix );

	qglLoadMatrixf( r_modelview_matrix );
}

void R_TranslateForEntity( entity_t *e )
{
	mat4x4_t obj_m;

	Matrix4_Identity( obj_m );

	obj_m[12] = e->origin[0];
	obj_m[13] = e->origin[1];
	obj_m[14] = e->origin[2];

	Matrix4_MultiplyFast( r_worldview_matrix, obj_m, r_modelview_matrix );

	qglLoadMatrixf( r_modelview_matrix );
}

qboolean R_LerpTag( orientation_t *orient, model_t *mod, int oldframe, int frame, float lerpfrac, char *name )
{
	if( !orient )
		return qfalse;

	VectorClear( orient->origin );
	Matrix_Identity( orient->axis );

	if( !name )
		return qfalse;

	if( mod->type == mod_alias )
		return R_AliasModelLerpTag( orient, mod->aliasmodel, oldframe, frame, lerpfrac, name );

	return qfalse;
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

static	vec3_t			spr_xyz[4];
static	vec2_t			spr_st[4] = { {0, 1}, {0, 0}, {1,0}, {1,1} };
static	byte_vec4_t		spr_color[4];

static	mesh_t			spr_mesh;
static	meshbuffer_t	spr_mbuffer;

/*
=================
R_DrawSprite
=================
*/
void R_DrawSprite( const meshbuffer_t *mb, float rotation, float right, float left, float up, float down )
{
	vec3_t		point;
	vec3_t		v_right, v_up;
	int			features;
	entity_t	*e = mb->entity;

	if ( rotation ) {
		RotatePointAroundVector( v_right, vpn, vright, rotation );
		CrossProduct( vpn, v_right, v_up );
	} else {
		VectorCopy( vright, v_right );
		VectorCopy( vup, v_up );
	}

	VectorScale( v_up, down, point );
	VectorMA( point, -left, v_right, spr_xyz[0] );
	VectorMA( point, -right, v_right, spr_xyz[3] );

	VectorScale( v_up, up, point );
	VectorMA( point, -left, v_right, spr_xyz[1] );
	VectorMA( point, -right, v_right, spr_xyz[2] );

	if( e->scale != 1.0f ) {
		VectorScale( spr_xyz[0], e->scale, spr_xyz[0] );
		VectorScale( spr_xyz[1], e->scale, spr_xyz[1] );
		VectorScale( spr_xyz[2], e->scale, spr_xyz[2] );
		VectorScale( spr_xyz[3], e->scale, spr_xyz[3] );
	}

	// the code below is disgusting, but some q3a shaders use 'rgbgen vertex'
	// and 'alphagen vertex' for effects instead of 'rgbgen entity' and 'alphagen entity'
	if ( mb->shader->features & MF_COLORS ) {
		Vector4Copy( e->color, spr_color[0] );
		Vector4Copy( e->color, spr_color[1] );
		Vector4Copy( e->color, spr_color[2] );
		Vector4Copy( e->color, spr_color[3] );
	}

	features = MF_NOCULL | MF_TRIFAN | mb->shader->features;
	if( r_shownormals->integer )
		features |= MF_NORMALS;

	spr_mesh.numIndexes = 6;
	spr_mesh.numVertexes = 4;
	spr_mesh.xyzArray = spr_xyz;
	spr_mesh.stArray = spr_st;
	spr_mesh.colorsArray[0] = spr_color;

	if( !(mb->shader->flags & SHADER_ENTITY_MERGABLE) ) {
		R_TranslateForEntity( e );
		R_PushMesh( &spr_mesh, MF_NONBATCHED | features );
		R_RenderMeshBuffer( mb, qfalse );
	} else {
		VectorAdd( spr_xyz[0], e->origin, spr_xyz[0] );
		VectorAdd( spr_xyz[1], e->origin, spr_xyz[1] );
		VectorAdd( spr_xyz[2], e->origin, spr_xyz[2] );
		VectorAdd( spr_xyz[3], e->origin, spr_xyz[3] );
		R_PushMesh( &spr_mesh, features );
	}
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel( const meshbuffer_t *mb )
{
	sframe_t	*frame;
	smodel_t	*psprite;
	entity_t	*e = mb->entity;
	model_t		*model = e->model;

	psprite = model->smodel;
	frame = psprite->frames + e->frame;

	R_DrawSprite( mb, e->rotation, frame->origin_x, frame->origin_x - frame->width, frame->height - frame->origin_y, -frame->origin_y );
}

/*
=================
R_AddSpriteModelToList
=================
*/
void R_AddSpriteModelToList( entity_t *e )
{
	sframe_t	*frame;
	smodel_t	*psprite;
	model_t		*model = e->model;

	if( !(psprite = model->smodel) )
		return;

	// cull it because we don't want to sort unneeded things
	if ((e->origin[0] - r_refdef.vieworg[0]) * vpn[0] +
		(e->origin[1] - r_refdef.vieworg[1]) * vpn[1] + 
		(e->origin[2] - r_refdef.vieworg[2]) * vpn[2] < 0) {
		return;
	}

	e->frame %= psprite->numframes;
	frame = psprite->frames + e->frame;

	// select skin
	if( e->customShader )
		R_AddMeshToList( MB_MODEL, R_FogForSphere( e->origin, frame->radius ), e->customShader, -1 );
	else
		R_AddMeshToList( MB_MODEL, R_FogForSphere( e->origin, frame->radius ), frame->shader, -1 );
}

/*
=================
R_DrawSpritePoly
=================
*/
void R_DrawSpritePoly( const meshbuffer_t *mb )
{
	entity_t *e = mb->entity;

	R_DrawSprite( mb, e->rotation, -e->radius, e->radius, e->radius, -e->radius );
}

/*
=================
R_AddSpritePolyToList
=================
*/
void R_AddSpritePolyToList (entity_t *e)
{
	// select skin
	if( !e->customShader )
		return;

	// cull it because we don't want to sort unneeded things
	if ((e->origin[0] - r_refdef.vieworg[0]) * vpn[0] +
		(e->origin[1] - r_refdef.vieworg[1]) * vpn[1] + 
		(e->origin[2] - r_refdef.vieworg[2]) * vpn[2] < 0) {
		return;
	}

	R_AddMeshToList( MB_SPRITE, R_FogForSphere( e->origin, e->radius ), e->customShader, -1 );
}

/*
=================
R_SpriteOverflow
=================
*/
qboolean R_SpriteOverflow( void ) {
	return R_MeshOverflow( &spr_mesh );
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
void R_PushFlareSurf( const meshbuffer_t *mb )
{
	vec4_t color;
	vec3_t origin, point, v;
	float radius = r_flaresize->value, flarescale, depth;
	float up = radius, down = -radius, left = -radius, right = radius;
	msurface_t *surf = &currentmodel->surfaces[mb->infokey - 1];

	if( currentmodel != r_worldmodel ) {
		Matrix_TransformVector( currententity->axis, surf->origin, origin );
		VectorAdd( origin, currententity->origin, origin );
	} else {
		VectorCopy( surf->origin, origin );
	}
	R_TransformToScreen_Vec3( origin, v );

	if( v[0] < r_refdef.x || v[0] > r_refdef.x + r_refdef.width )
		return;
	if( v[1] < r_refdef.y || v[1] > r_refdef.y + r_refdef.height )
		return;

	qglReadPixels( (int)(v[0]/* + 0.5f*/), (int)(v[1]/* + 0.5f*/), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );
	if( depth + 1e-4 < v[2] )
		return;		// occluded

	VectorCopy( origin, origin );

	VectorMA( origin, down, vup, point );
	VectorMA( point, -left, vright, spr_xyz[0] );
	VectorMA( point, -right, vright, spr_xyz[3] );

	VectorMA( origin, up, vup, point );
	VectorMA( point, -left, vright, spr_xyz[1] );
	VectorMA( point, -right, vright, spr_xyz[2] );

	flarescale = 1.0 / r_flarefade->value;
	Vector4Set( color, 
		COLOR_R( surf->dlightbits ) * flarescale,
		COLOR_G( surf->dlightbits ) * flarescale,
		COLOR_B( surf->dlightbits ) * flarescale, 255 );

	Vector4Copy( color, spr_color[0] );
	Vector4Copy( color, spr_color[1] );
	Vector4Copy( color, spr_color[2] );
	Vector4Copy( color, spr_color[3] );

	spr_mesh.numIndexes = 6;
	spr_mesh.numVertexes = 4;
	spr_mesh.xyzArray = spr_xyz;
	spr_mesh.stArray = spr_st;
	spr_mesh.colorsArray[0] = spr_color;
	spr_mesh.colorsArray[1] = spr_color;
	spr_mesh.colorsArray[2] = spr_color;
	spr_mesh.colorsArray[3] = spr_color;

	R_PushMesh( &spr_mesh, MF_NOCULL | MF_TRIFAN | mb->shader->features );
}


//==================================================================================

/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel( void )
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
void R_AddEntitiesToList( void )
{
	int		i;

	r_numnullentities = 0;

	if( !r_drawentities->integer )
		return;

	if( r_envview ) {
		for( i = 1; i < r_numEntities; i++ ) {
			currententity = &r_entities[i];

			if( currententity->rtype != RT_MODEL || !(currentmodel = currententity->model) )
				continue;
			if( currentmodel->type == mod_brush )
				R_AddBrushModelToList( currententity );
		}
	} else {
		for( i = 1; i < r_numEntities; i++ ) {
			currententity = &r_entities[i];

			if( r_mirrorview ) {
				if( currententity->flags & RF_WEAPONMODEL ) 
					continue;
			}

			switch( currententity->rtype ) {
				case RT_MODEL:
					if( !(currentmodel = currententity->model) ) {
						r_nullentities[r_numnullentities++] = currententity;
						break;
					}

					switch( currentmodel->type ) {
						case mod_alias:
							R_AddAliasModelToList( currententity );
							break;
						case mod_skeletal:
							R_AddSkeletalModelToList( currententity );
							break;
						case mod_brush:
							R_AddBrushModelToList( currententity );
							break;
						case mod_sprite:
							R_AddSpriteModelToList( currententity );
							break;
						default:
							Com_Error( ERR_DROP, "%s: bad modeltype", currentmodel->name );
							break;
					}
					break;
				case RT_SPRITE:
					if( currententity->radius )
						R_AddSpritePolyToList( currententity );
					break;
				case RT_PORTALSURFACE:
					break;
				default:
					break;
			}
		}
	}
}

/*
=============
R_DrawNullEntities
=============
*/
void R_DrawNullEntities( void )
{
	int		i;

	if( !r_numnullentities )
		return;

	qglDepthFunc( GL_LEQUAL );
	qglDisable( GL_TEXTURE_2D );
	qglDisable( GL_ALPHA_TEST );
	qglEnable( GL_BLEND );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	// draw non-transparent first
	for( i = 0; i < r_numnullentities; i++ ) {
		currententity = r_nullentities[i];

		if( r_mirrorview ) {
			if( currententity->flags & RF_WEAPONMODEL ) 
				continue;
		} else {
			if( currententity->flags & RF_VIEWERMODEL ) 
				continue;
		}
		R_DrawNullModel ();
	}

	qglDisable( GL_BLEND );
	qglEnable( GL_TEXTURE_2D );
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if( !r_polyblend->integer )
		return;
	if( r_refdef.blend[3] < 0.01f )
		return;

	qglEnable( GL_BLEND );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_TEXTURE_2D );

	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity ();
	qglOrtho( 0, 1, 1, 0, -99999, 99999 );

	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity ();

	qglColor4fv( r_refdef.blend );

	qglBegin( GL_TRIANGLES );
	qglVertex2f( -5, -5 );
	qglVertex2f( 10, -5 );
	qglVertex2f( -5, 10 );
	qglEnd ();

	qglDisable( GL_BLEND );
	qglEnable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );
}

//=======================================================================

/*
===============
R_SetFrustum
===============
*/
void R_SetFrustum( void )
{
	int		i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );

	for( i = 0; i < 4; i++ ) {
		frustum[i].type = PLANE_NONAXIAL;
		frustum[i].dist = DotProduct( r_origin, frustum[i].normal );
		frustum[i].signbits = SignbitsForPlane( &frustum[i] );
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	mleaf_t *leaf;

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	if ( !r_portalview )
		AngleVectors( r_refdef.viewangles, vpn, vright, vup );

	// current viewcluster
	if( !( r_refdef.rdflags & RDF_NOWORLDMODEL ) && !r_mirrorview ) {
		if( r_portalview ) {
			r_oldviewcluster = -1;
			leaf = Mod_PointInLeaf( r_portalorg, r_worldmodel );
		} else {
			r_oldviewcluster = r_viewcluster;
			leaf = Mod_PointInLeaf( r_origin, r_worldmodel );
		}
		r_viewcluster = leaf->cluster;
	}
}

/*
===============
R_FarClip
===============
*/
float R_FarClip( void )
{
	float farclip, farclip_dist;

	farclip_dist = DotProduct( r_origin, vpn );
	farclip = max( r_farclip_min, 256.0f ) + farclip_dist;

	if( r_worldmodel && !(r_refdef.rdflags & RDF_NOWORLDMODEL) ) {
		vec_t *mins, *maxs, dist;

		mins = r_worldmodel->nodes[0].mins;
		maxs = r_worldmodel->nodes[0].maxs;
		dist = (vpn[0] < 0 ? mins[0] : maxs[0]) * vpn[0] + 
			(vpn[1] < 0 ? mins[1] : maxs[1]) * vpn[1] +
			(vpn[2] < 0 ? mins[2] : maxs[2]) * vpn[2];
		if( dist > farclip )
			farclip = dist;
	}

	return max( farclip - farclip_dist + r_farclip_bias, r_farclip );
}

/*
=============
R_SetupProjectionMatrix
=============
*/
void R_SetupProjectionMatrix( refdef_t *rd, mat4x4_t m )
{
	GLdouble xMin, xMax, yMin, yMax, zNear, zFar;

	r_farclip = R_FarClip ();

	zNear = 4;
	zFar = r_farclip;

	yMax = zNear * tan( rd->fov_y * M_PI / 360.0 );
	yMin = -yMax;

	xMin = yMin * rd->width / rd->height;
	xMax = yMax * rd->width / rd->height;

	xMin += -( 2 * glState.cameraSeparation ) / zNear;
	xMax += -( 2 * glState.cameraSeparation ) / zNear;

	m[0] = (2.0 * zNear) / (xMax - xMin);
	m[1] = 0.0f;
	m[2] = 0.0f;
	m[3] = 0.0f;
	m[4] = 0.0f;
	m[5] = (2.0 * zNear) / (yMax - yMin);
	m[6] = 0.0f;
	m[7] = 0.0f;
	m[8] = (xMax + xMin) / (xMax - xMin);
	m[9] = (yMax + yMin) / (yMax - yMin);
	m[10] = -(zFar + zNear) / (zFar - zNear);
	m[11] = -1.0f;
	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = -(2.0 * zFar * zNear) / (zFar - zNear);
	m[15] = 0.0f;
}

/*
=============
R_SetupModelviewMatrix
=============
*/
void R_SetupModelviewMatrix( refdef_t *rd, mat4x4_t m )
{
#if 0
	Matrix4_Identity( m );
	Matrix4_Rotate( m, -90, 1, 0, 0 );
	Matrix4_Rotate( m,  90, 0, 0, 1 );
#else
	Vector4Set( &m[0], 0, 0, -1, 0 );
	Vector4Set( &m[4], -1, 0, 0, 0 );
	Vector4Set( &m[8], 0, 1, 0, 0 );
	Vector4Set( &m[12], 0, 0, 0, 1 );
#endif
	Matrix4_Rotate( m, -rd->viewangles[2], 1, 0, 0 );
	Matrix4_Rotate( m, -rd->viewangles[0], 0, 1, 0 );
	Matrix4_Rotate( m, -rd->viewangles[1], 0, 0, 1 );
	Matrix4_Translate( m, -rd->vieworg[0], -rd->vieworg[1], -rd->vieworg[2] );
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL( void )
{
	if( !r_mirrorview && !r_portalview ) {
		qglScissor( r_refdef.x, glState.height - r_refdef.height - r_refdef.y, r_refdef.width, r_refdef.height );
		qglViewport( r_refdef.x, glState.height - r_refdef.height - r_refdef.y, r_refdef.width, r_refdef.height );
		qglClear( GL_DEPTH_BUFFER_BIT );
	}

	// set up projection matrix
	R_SetupProjectionMatrix( &r_refdef, r_projection_matrix );
	if( r_mirrorview )
		r_projection_matrix[0] = -r_projection_matrix[0];

	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( r_projection_matrix );

	R_SetupModelviewMatrix( &r_refdef, r_worldview_matrix );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadMatrixf( r_worldview_matrix );

	if( r_portalview || r_mirrorview ) {
		GLdouble clip[4];

		clip[0] = r_clipplane.normal[0];
		clip[1] = r_clipplane.normal[1];
		clip[2] = r_clipplane.normal[2];
		clip[3] = -r_clipplane.dist;

		qglClipPlane( GL_CLIP_PLANE0, clip );
		qglEnable( GL_CLIP_PLANE0 );
	}

	qglEnable( GL_DEPTH_TEST );
	qglDepthMask( GL_TRUE );
}

/*
=============
R_TransformToScreen_Vec3
=============
*/
void R_TransformToScreen_Vec3( vec3_t in, vec3_t out )
{
	vec4_t temp, temp2;

	temp[0] = in[0];
	temp[1] = in[1];
	temp[2] = in[2];
	temp[3] = 1.0f;
	Matrix4_Multiply_Vector( r_worldview_matrix, temp, temp2 );
	Matrix4_Multiply_Vector( r_projection_matrix, temp2, temp );

	if( !temp[3] )
		return;
	out[0] = r_refdef.x + (temp[0] / temp[3] + 1.0f) * r_refdef.width * 0.5f;
	out[1] = r_refdef.y + (temp[1] / temp[3] + 1.0f) * r_refdef.height * 0.5f;
	out[2] = (temp[2] / temp[3] + 1.0f) * 0.5f;
}

/*
=============
R_TransformVectorToScreen
=============
*/
void R_TransformVectorToScreen( refdef_t *rd, vec3_t in, vec2_t out )
{
	mat4x4_t p, m;
	vec4_t temp, temp2;

	if( !rd || !in || !out )
		return;

	temp[0] = in[0];
	temp[1] = in[1];
	temp[2] = in[2];
	temp[3] = 1.0f;

	R_SetupProjectionMatrix( rd, p );
	R_SetupModelviewMatrix( rd, m );

	Matrix4_Multiply_Vector( m, temp, temp2 );
	Matrix4_Multiply_Vector( p, temp2, temp );

	if( !temp[3] )
		return;
	out[0] = rd->x + (temp[0] / temp[3] + 1.0f) * rd->width * 0.5f;
	out[1] = rd->y + (temp[1] / temp[3] + 1.0f) * rd->height * 0.5f;
}

/*
=============
R_Clear
=============
*/
void R_Clear( void )
{
	int	bits;

	bits = GL_DEPTH_BUFFER_BIT;

	if( r_clear->integer ) {
		qglClearColor( 0.5, 0.5, 0.5, 1 );
		bits |= GL_COLOR_BUFFER_BIT;
	}
	if( glState.stencilEnabled && r_shadows->integer ) {
		qglClearStencil( 128 );
		bits |= GL_STENCIL_BUFFER_BIT;
	}

	qglClear( bits );

	gldepthmin = 0;
	gldepthmax = 1;
	qglDepthRange( gldepthmin, gldepthmax );
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView( refdef_t *fd, meshlist_t *list )
{
	r_refdef = *fd;
	r_currentlist = list;

	if( !r_worldmodel && !( r_refdef.rdflags & RDF_NOWORLDMODEL ) )
		Com_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	if( (r_mirrorview || r_portalview) && r_fastsky->integer ) {
		R_DrawSky( NULL );
		goto done;
	}

	if( r_speeds->integer )
		r_mark_leaves = Sys_Milliseconds ();
	R_MarkLeaves ();
	if( r_speeds->integer )
		r_mark_leaves = Sys_Milliseconds () - r_mark_leaves;

	R_DrawWorld ();

	if( r_speeds->integer )
		r_add_polys = Sys_Milliseconds ();
	R_AddPolysToList ();
	if( r_speeds->integer )
		r_add_polys = Sys_Milliseconds () - r_add_polys;

	if( r_speeds->integer )
		r_add_entities = Sys_Milliseconds ();
	R_AddEntitiesToList ();
	if( r_speeds->integer )
		r_add_entities = Sys_Milliseconds () - r_add_entities;

	if( r_speeds->integer )
		r_sort_meshes = Sys_Milliseconds ();
	R_SortMeshes ();
	if( r_speeds->integer )
		r_sort_meshes = Sys_Milliseconds () - r_sort_meshes;

	if( r_speeds->integer )
		r_draw_meshes = Sys_Milliseconds ();
	R_DrawMeshes( qfalse );
	if( r_speeds->integer )
		r_draw_meshes = Sys_Milliseconds () - r_draw_meshes;

	R_DrawTriangleOutlines ();

	R_DrawNullEntities ();

	if( r_mirrorview || r_portalview )
done:
		qglDisable ( GL_CLIP_PLANE0 );
}

//=======================================================================

vec3_t			pic_xyz[4];
vec3_t			pic_normals[4] = { {0,1,0}, {0,1,0}, {0,1,0}, {0,1,0} };
vec2_t			pic_st[4];
byte_vec4_t		pic_colors[4];

mesh_t			pic_mesh;
meshbuffer_t	pic_mbuffer;

/*
===============
R_Set2DMode
===============
*/
void R_Set2DMode( void )
{
	// set 2D virtual screen size
	qglViewport( 0, 0, glState.width, glState.height );
	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity ();
	qglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );
	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity ();
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_CULL_FACE );
	qglColor4f( 1, 1, 1, 1 );
	glState.in2DMode = qtrue;
}

/*
===============
R_DrawStretchPic
===============
*/
void R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, shader_t *shader )
{
	int bcolor;

	if( !shader )
		return;

	// lower-left
	pic_xyz[0][0] = x;
	pic_xyz[0][1] = y;
	pic_xyz[0][2] = 1;
	pic_st[0][0] = s1;
	pic_st[0][1] = t1;
	pic_colors[0][0] = R_FloatToByte( color[0] );
	pic_colors[0][1] = R_FloatToByte( color[1] );
	pic_colors[0][2] = R_FloatToByte( color[2] );
	pic_colors[0][3] = R_FloatToByte( color[3] );
	bcolor = *(int *)pic_colors[0];

	// lower-right
	pic_xyz[1][0] = x+w;
	pic_xyz[1][1] = y;
	pic_xyz[1][2] = 1;
	pic_st[1][0] = s2;
	pic_st[1][1] = t1;
	*(int *)pic_colors[1] = bcolor;

	// upper-right
	pic_xyz[2][0] = x+w;
	pic_xyz[2][1] = y+h;
	pic_xyz[2][2] = 1;
	pic_st[2][0] = s2;
	pic_st[2][1] = t2;
	*(int *)pic_colors[2] = bcolor;

	// upper-left
	pic_xyz[3][0] = x;
	pic_xyz[3][1] = y+h;
	pic_xyz[3][2] = 1;
	pic_st[3][0] = s1;
	pic_st[3][1] = t2;
	*(int *)pic_colors[3] = bcolor;

#if SHADOW_VOLUMES
	pic_mesh.trneighbors = NULL;
#endif

	pic_mesh.numVertexes = 4;
	pic_mesh.xyzArray = pic_xyz;
	pic_mesh.stArray = pic_st;
	pic_mesh.normalsArray = pic_normals;
	pic_mesh.colorsArray[0] = pic_colors;

	pic_mbuffer.shader = shader;

	// upload video right before rendering
	if( shader->flags & SHADER_VIDEOMAP )
		R_UploadCinematicShader( shader );

	R_PushMesh( &pic_mesh, MF_NONBATCHED | MF_TRIFAN | shader->features | (r_shownormals->integer ? MF_NORMALS : 0) );
	R_RenderMeshBuffer( &pic_mbuffer, qfalse );
}

/*
=============
R_DrawStretchRaw
=============
*/
void R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, int frame, qbyte *data )
{
	GL_Bind( 0, r_cintexture );

	if( cols != r_cintexture->width || rows != r_cintexture->height )
		R_Upload32( &data, cols, rows, IT_CINEMATIC, NULL, NULL, 3, qfalse );
	else
		R_Upload32( &data, cols, rows, IT_CINEMATIC, NULL, NULL, 3, qtrue );
	r_cintexture->width = cols;
	r_cintexture->height = rows;

	qglBegin( GL_QUADS );
	qglTexCoord2f( 0, 0 );
	qglVertex2f( x, y );
	qglTexCoord2f( 1, 0 );
	qglVertex2f( x + w, y );
	qglTexCoord2f( 1, 1 );
	qglVertex2f( x + w, y + h );
	qglTexCoord2f( 0, 1 );
	qglVertex2f( x, y + h );
	qglEnd ();
}

//=============================================================================

/*
===============
R_UpdateSwapInterval
===============
*/
void R_UpdateSwapInterval( void )
{
	if( r_swapinterval->modified ) {
		r_swapinterval->modified = qfalse;

		if( !glState.stereoEnabled ) {
#ifdef _WIN32
			if( qwglSwapIntervalEXT )
				qwglSwapIntervalEXT( r_swapinterval->integer );
#endif
		}
	}
}

/*
===============
R_UpdateHWGamma
===============
*/
void R_UpdateHWGamma( void )
{
	int i, v;
	double invGamma, div;
	unsigned short gammaRamp[3*256];

	if( !glState.hwGamma )
		return;

	invGamma = 1.0 / bound( 0.5, r_gamma->value, 3 );
	div = (double)(1 << max( 0, r_overbrightbits->integer )) / 255.5;

	for( i = 0; i < 256; i++ ) {
		v = ( int )( 65535.0 * pow( ((double)i + 0.5) * div, invGamma ) + 0.5 );
		gammaRamp[i] = gammaRamp[i + 256] = gammaRamp[i + 512] = (( unsigned short )bound( 0, v, 65535 ));
	}

	GLimp_SetGammaRamp( 256, gammaRamp );
}

/*
===============
R_BeginFrame
===============
*/
void R_BeginFrame( float cameraSeparation )
{
	glState.cameraSeparation = cameraSeparation;

	if( gl_finish->integer && gl_delayfinish->integer ) {
		R_ApplySoftwareGamma ();
		qglFinish ();
		GLimp_EndFrame ();
	}

	GLimp_BeginFrame ();

	// update gamma
	if( r_gamma->modified ) {
		r_gamma->modified = qfalse;
		R_UpdateHWGamma ();
	}

	// run cinematic passes on shaders
	R_RunCinematicShaders ();

	// go into 2D mode
	R_Set2DMode ();

	// draw buffer stuff
	if( gl_drawbuffer->modified ) {
		gl_drawbuffer->modified = qfalse;

		if ( glState.cameraSeparation == 0 || !glState.stereoEnabled ) {
			if( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 )
				qglDrawBuffer( GL_FRONT );
			else
				qglDrawBuffer( GL_BACK );
		}
	}

	// texturemode stuff
	if( r_texturemode->modified ) {
		R_TextureMode( r_texturemode->string );
		r_texturemode->modified = qfalse;
	}

	// swapinterval stuff
	R_UpdateSwapInterval ();

	// clear screen if desired
	R_Clear ();
}


/*
====================
R_ClearScene
====================
*/
void R_ClearScene( void )
{
	r_numEntities = 1;
	r_entities[0] = r_worldent;
	r_numDlights = 0;
	r_numPolys = 0;
	currententity = &r_worldent;
	currentmodel = r_worldmodel;
}

/*
=====================
R_AddEntityToScene
=====================
*/
void R_AddEntityToScene( entity_t *ent )
{
	if( (r_numEntities < MAX_ENTITIES) && ent )
		r_entities[r_numEntities++] = *ent;
}

/*
=====================
R_AddLightToScene
=====================
*/
void R_AddLightToScene( vec3_t org, float intensity, float r, float g, float b, shader_t *shader )
{
	if( (r_numDlights < MAX_DLIGHTS) && intensity && (r != 0 || g != 0 || b != 0) ) {
		dlight_t *dl = &r_dlights[r_numDlights++];

		VectorCopy( org, dl->origin );
		dl->intensity = intensity * DLIGHT_SCALE;
		dl->color[0] = r;
		dl->color[1] = g;
		dl->color[2] = b;
		dl->shader = shader;

		R_LightBounds( org, dl->intensity, dl->mins, dl->maxs );
	}
}

/*
=====================
R_AddPolyToScene
=====================
*/
void R_AddPolyToScene( poly_t *poly )
{
	if( (r_numPolys < MAX_POLYS) && poly && poly->numverts ) {
		r_polys[r_numPolys] = *poly;
		if( r_polys[r_numPolys].numverts > MAX_POLY_VERTS )
			r_polys[r_numPolys].numverts = MAX_POLY_VERTS;
		r_numPolys++;
	}
}

/*
=====================
R_AddLightStyleToScene
=====================
*/
void R_AddLightStyleToScene( int style, float r, float g, float b )
{
	lightstyle_t	*ls;

	if( style < 0 || style > MAX_LIGHTSTYLES )
		Com_Error( ERR_DROP, "R_AddLightStyleToScene: bad light style %i", style );

	ls = &r_lightStyles[style];
	ls->rgb[0] = max( 0, r );
	ls->rgb[1] = max( 0, g );
	ls->rgb[2] = max( 0, b );
}

/*
===============
R_RenderScene
===============
*/
void R_RenderScene( refdef_t *fd )
{
	glState.in2DMode = qfalse;

	if( !r_norefresh->integer ) {
		R_BackendStartFrame ();

		if( !(fd->rdflags & RDF_NOWORLDMODEL ) )
			r_lastRefdef = *fd;

		r_farclip = 0;

		r_mirrorview = qfalse;
		r_portalview = qfalse;

		c_brush_polys = 0;
		c_world_leafs = 0;

		r_worldlist.skyDrawn = qfalse;
		r_worldlist.num_meshes = 0;
		r_worldlist.num_additive_meshes = 0;

		if( gl_finish->integer && !gl_delayfinish->integer )
			qglFinish ();

		R_RenderView( fd, &r_worldlist );

		R_PolyBlend ();

#if SHADOW_VOLUMES
		R_ShadowBlend ();
#endif

		R_BackendEndFrame ();

		R_Set2DMode ();
	}
}

/*
===============
R_ApplySoftwareGamma
===============
*/
void R_ApplySoftwareGamma( void )
{
	double f, div;

	// apply software gamma
	if( !r_ignorehwgamma->integer )
		return;

	qglEnable( GL_BLEND );
	qglBlendFunc( GL_DST_COLOR, GL_ONE );
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_TEXTURE_2D );

	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity ();
	qglOrtho( 0, 1, 1, 0, -99999, 99999 );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity ();

	if( r_overbrightbits->integer > 0 )
		div = 0.5 * (double)( 1 << r_overbrightbits->integer );
	else
		div = 0.5;
	f = div + r_gamma->value;
	clamp( f, 0.1, 5.0 );

	qglBegin( GL_TRIANGLES );

	while( f >= 1.01f ) {
		if( f >= 2 )
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		else
			qglColor4f( f - 1.0f, f - 1.0f, f - 1.0f, 1.0f );

		qglVertex2f( -5, -5 );
		qglVertex2f( 10, -5 );
		qglVertex2f( -5, 10 );
		f *= 0.5;
	}

	qglEnd ();

	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDisable( GL_BLEND );
	qglEnable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );
}

/*
===============
R_BeginFrame
===============
*/
void R_EndFrame( void )
{
	if( gl_finish->integer && gl_delayfinish->integer ) {
		qglFlush ();
		return;
	}

	R_ApplySoftwareGamma ();

	GLimp_EndFrame ();
}

//=======================================================================

void R_Register( void )
{
	Cvar_GetLatchedVars( CVAR_LATCH_VIDEO );

	r_norefresh = Cvar_Get( "r_norefresh", "0", 0 );
	r_fullbright = Cvar_Get( "r_fullbright", "0", CVAR_CHEAT|CVAR_LATCH_VIDEO );
	r_lightmap = Cvar_Get( "r_lightmap", "0", CVAR_CHEAT );
	r_drawentities = Cvar_Get( "r_drawentities", "1", CVAR_CHEAT );
	r_drawworld = Cvar_Get( "r_drawworld", "1", CVAR_CHEAT );
	r_novis = Cvar_Get( "r_novis", "0", 0 );
	r_nocull = Cvar_Get( "r_nocull", "0", 0 );
	r_lerpmodels = Cvar_Get( "r_lerpmodels", "1", 0 );
	r_speeds = Cvar_Get( "r_speeds", "0", 0 );
	r_showtris = Cvar_Get( "r_showtris", "0", CVAR_CHEAT );
	r_lockpvs = Cvar_Get( "r_lockpvs", "0", CVAR_CHEAT );
	r_clear = Cvar_Get( "r_clear", "0", 0 );
	r_mode = Cvar_Get( "r_mode", "3", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_nobind = Cvar_Get( "r_nobind", "0", 0 );
	r_picmip = Cvar_Get( "r_picmip", "0", CVAR_LATCH_VIDEO );
	r_skymip = Cvar_Get( "r_skymip", "0", CVAR_LATCH_VIDEO );
	r_polyblend = Cvar_Get( "r_polyblend", "1", 0 );

	r_fastsky = Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE );
	r_ignorehwgamma = Cvar_Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_overbrightbits = Cvar_Get( "r_overbrightbits", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_mapoverbrightbits = Cvar_Get( "r_mapoverbrightbits", "2", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_detailtextures = Cvar_Get( "r_detailtextures", "1", CVAR_ARCHIVE );
	r_flares = Cvar_Get( "r_flares", "0", CVAR_ARCHIVE );
	r_flaresize = Cvar_Get( "r_flaresize", "40", CVAR_ARCHIVE );
	r_flarefade = Cvar_Get( "r_flarefade", "3", CVAR_ARCHIVE );
	r_dynamiclight = Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE );
	r_subdivisions = Cvar_Get( "r_subdivisions", "4", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_packlightmaps = Cvar_Get( "r_packlightmaps", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_faceplanecull = Cvar_Get( "r_faceplanecull", "1", CVAR_ARCHIVE );
	r_spherecull = Cvar_Get( "r_spherecull", "1", 0 );
	r_shownormals = Cvar_Get( "r_shownormals", "0", CVAR_CHEAT );
	r_ambientscale = Cvar_Get( "r_ambientscale", "0.6", 0 );
	r_directedscale = Cvar_Get( "r_directedscale", "1", 0 );
	r_draworder = Cvar_Get( "r_draworder", "0", CVAR_CHEAT );
	r_bumpscale = Cvar_Get( "r_bumpscale", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_maxlmblocksize = Cvar_Get( "r_maxLMBlockSize", "512", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	r_allow_software = Cvar_Get( "r_allow_software", "0", 0 );
	r_3dlabs_broken = Cvar_Get( "r_3dlabs_broken", "1", CVAR_ARCHIVE );

	r_shadows = Cvar_Get( "r_shadows", "0", CVAR_ARCHIVE );
	r_shadows_alpha = Cvar_Get( "r_shadows_alpha", "0.4", CVAR_ARCHIVE );
	r_shadows_nudge = Cvar_Get( "r_shadows_nudge", "1", CVAR_ARCHIVE );
	r_shadows_projection_distance = Cvar_Get( "r_shadows_projection_distance", "10000", CVAR_ARCHIVE );

	r_lodbias = Cvar_Get( "r_lodbias", "0", CVAR_ARCHIVE );
	r_lodscale = Cvar_Get( "r_lodscale", "5.0", CVAR_ARCHIVE );

	r_gamma = Cvar_Get( "r_gamma", "1.0", CVAR_ARCHIVE );
	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	r_texturebits = Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	r_texturemode = Cvar_Get( "r_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	r_stencilbits = Cvar_Get( "r_stencilbits", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	r_screenshot_jpeg = Cvar_Get( "r_screenshot_jpeg", "1", CVAR_ARCHIVE );					// Heffo - JPEG Screenshots
	r_screenshot_jpeg_quality = Cvar_Get( "r_screenshot_jpeg_quality", "85", CVAR_ARCHIVE );	// Heffo - JPEG Screenshots

	r_swapinterval = Cvar_Get( "r_swapinterval", "0", CVAR_ARCHIVE );
	// make sure r_swapinterval is checked after vid_restart
	r_swapinterval->modified = qtrue;

	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_delayfinish = Cvar_Get( "gl_delayfinish", "1", CVAR_ARCHIVE );
	gl_cull = Cvar_Get ("gl_cull", "1", 0);
	gl_driver = Cvar_Get( "gl_driver", GL_DRIVERNAME, CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	gl_extensions = Cvar_Get( "gl_extensions", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_multitexture = Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_compiled_vertex_array = Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_env_add = Cvar_Get( "gl_ext_texture_env_add", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_env_combine = Cvar_Get( "gl_ext_texture_env_combine", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_env_dot3 = Cvar_Get( "gl_ext_texture_env_dot3", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_NV_texture_env_combine4 = Cvar_Get( "gl_ext_NV_texture_env_combine4", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_compressed_textures = Cvar_Get( "gl_ext_compressed_textures", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_edge_clamp = Cvar_Get( "gl_ext_texture_edge_clamp", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_filter_anisotropic = Cvar_Get( "gl_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_max_texture_filter_anisotropic = Cvar_Get( "gl_ext_max_texture_filter_anisotropic", "0", CVAR_NOSET );
	gl_ext_draw_range_elements = Cvar_Get( "gl_ext_draw_range_elements", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_vertex_buffer_object = Cvar_Get( "gl_ext_vertex_buffer_object", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_cube_map = Cvar_Get( "gl_ext_texture_cube_map", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_bgra = Cvar_Get( "gl_ext_bgra", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );

	vid_fullscreen = Cvar_Get( "vid_fullscreen", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	Cmd_AddCommand( "imagelist", R_ImageList_f );
	Cmd_AddCommand( "shaderlist", R_ShaderList_f );
	Cmd_AddCommand( "screenshot", R_ScreenShot_f );
	Cmd_AddCommand( "envshot", R_EnvShot_f );
	Cmd_AddCommand( "modellist", Mod_Modellist_f );
	Cmd_AddCommand( "gfxinfo", R_GfxInfo_f );
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

	if ( vid_fullscreen->modified && !glConfig.allowCDS ) {
		Com_Printf( "R_SetMode() - CDS not allowed with this driver\n" );
		Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->integer );
		vid_fullscreen->modified = qfalse;
	}

	fullscreen = vid_fullscreen->integer;
	vid_fullscreen->modified = qfalse;

	if ( r_mode->integer < 3 ) {
		Com_Printf ( "Resolutions below 640x480 are not supported\n" );
		Cvar_ForceSet ( "r_mode", "3" );
	}

	r_mode->modified = qfalse;

	if ( ( err = GLimp_SetMode( r_mode->integer, fullscreen ) ) == rserr_ok )
	{
		glState.previousMode = r_mode->integer;
	}
	else
	{
		if ( err == rserr_invalid_fullscreen )
		{
			Cvar_SetValue( "vid_fullscreen", 0);
			vid_fullscreen->modified = qfalse;
			Com_Printf( "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n" );
			if ( ( err = GLimp_SetMode( r_mode->integer, qfalse ) ) == rserr_ok )
				return qtrue;
		}
		else if ( err == rserr_invalid_mode )
		{
			Cvar_SetValue( "r_mode", glState.previousMode );
			r_mode->modified = qfalse;
			Com_Printf( "ref_gl::R_SetMode() - invalid mode\n" );
		}

		// try setting it back to something safe
		if ( ( err = GLimp_SetMode( glState.previousMode, qfalse ) ) != rserr_ok )
		{
			Com_Printf( "ref_gl::R_SetMode() - could not revert to safe mode\n" );
			return qfalse;
		}
	}

	if( r_ignorehwgamma->integer )
		glState.hwGamma = qfalse;
	else
		glState.hwGamma = GLimp_GetGammaRamp( 256, glState.orignalGammaRamp );
	if( glState.hwGamma )
		r_gamma->modified = qtrue;

	return qtrue;
}

/*
===============
R_CheckExtensions
===============
*/
void R_CheckExtensions( void )
{
	glConfig.compiledVertexArray = qfalse;
	glConfig.multiTexture = qfalse;
	glConfig.textureCubeMap = qfalse;
	glConfig.textureEnvAdd = qfalse;
	glConfig.textureEnvCombine = qfalse;
	glConfig.textureEnvDot3 = qfalse;
	glConfig.NVTextureEnvCombine4 = qfalse;
	glConfig.compressedTextures = qfalse;
	glConfig.textureEdgeClamp = qfalse;
	glConfig.textureFilterAnisotropic = qfalse;
	glConfig.maxTextureFilterAnisotropic = 0.0f;
	glConfig.drawRangeElements = qfalse;
	glConfig.vertexBufferObject = qfalse;
	glConfig.bgra = qfalse;

	glConfig.maxTextureCubemapSize = 0;
	glConfig.maxTextureFilterAnisotropic = 0;

#ifdef _WIN32
	if( strstr( glConfig.extensionsString, "WGL_EXT_swap_control" ) ) {
		qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qglGetProcAddress( "wglSwapIntervalEXT" );

		if( !qwglSwapIntervalEXT )
			Com_Printf( "R_CheckExtensions: broken WGL_EXT_swap_control support, contact your video card vendor\n" );
	}
#endif

	if( !gl_extensions->integer )
		return;

	/*
	** grab extensions
	*/
	if( gl_ext_compiled_vertex_array->integer ) {
		if( strstr( glConfig.extensionsString, "GL_EXT_compiled_vertex_array" ) || 
			strstr( glConfig.extensionsString, "GL_SGI_compiled_vertex_array" ) ) {
			qglLockArraysEXT = ( void * ) qglGetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = ( void * ) qglGetProcAddress( "glUnlockArraysEXT" );

			if( !qglLockArraysEXT || !qglUnlockArraysEXT )
				Com_Printf( "R_CheckExtensions: broken CVA support, contact your video card vendor\n" );
			else
				glConfig.compiledVertexArray = qtrue;
		}
	}

	if( gl_ext_vertex_buffer_object->integer ) {
		if( strstr( glConfig.extensionsString, "GL_ARB_vertex_buffer_object" ) ) {
			qglBindBufferARB = ( void * ) qglGetProcAddress( "glBindBufferARB" );
			if( qglBindBufferARB )
				qglDeleteBuffersARB = ( void * ) qglGetProcAddress("glDeleteBuffersARB");
			if( qglDeleteBuffersARB )
				qglGenBuffersARB = ( void * ) qglGetProcAddress("glGenBuffersARB");
			if( qglGenBuffersARB )
				qglBufferDataARB = ( void * ) qglGetProcAddress("glBufferDataARB");

			if( qglBufferDataARB )
				glConfig.vertexBufferObject = qtrue;
			else
				Com_Printf( "R_CheckExtensions: broken GL_ARB_vertex_buffer_object support, contact your video card vendor\n" );
		}
	}

	if( gl_ext_draw_range_elements->integer ) {
		if( strstr( glConfig.extensionsString, "GL_EXT_draw_range_elements" ) ) {
			qglDrawRangeElementsEXT = ( void * ) qglGetProcAddress( "glDrawRangeElementsEXT" );
			if( !qglDrawRangeElementsEXT )
				qglDrawRangeElementsEXT = ( void * ) qglGetProcAddress( "glDrawRangeElements" );
			if( qglDrawRangeElementsEXT )
				glConfig.drawRangeElements = qtrue;
			else
				Com_Printf( "R_CheckExtensions: broken GL_EXT_draw_range_elements support, contact your video card vendor\n" );
		}
	}

	if ( gl_ext_multitexture->integer ) {
		if( strstr( glConfig.extensionsString, "GL_ARB_multitexture" ) ) {
			qglActiveTextureARB = ( void * ) qglGetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( void * ) qglGetProcAddress( "glClientActiveTextureARB" );

			if( !qglActiveTextureARB || !qglClientActiveTextureARB )
				Com_Printf( "R_CheckExtensions: broken multitexture support, contact your video card vendor\n" );
			else
				glConfig.multiTexture = qtrue;
		}

		if( !glConfig.multiTexture ) {
			if( strstr( glConfig.extensionsString, "GL_SGIS_multitexture" ) ) {
				qglSelectTextureSGIS = ( void * ) qglGetProcAddress( "glSelectTextureSGIS" );

				if( !qglSelectTextureSGIS )
					Com_Printf( "R_CheckExtensions: broken multitexture support, contact your video card vendor\n" );
				else
					glConfig.multiTexture = qtrue;
			}
		}
	}

#ifdef _WIN32
	if( !r_ignorehwgamma->integer ) {
		if( strstr( glConfig.extensionsString, "WGL_3DFX_gamma_control" )) {
			qwglGetDeviceGammaRamp3DFX = ( BOOL (WINAPI *)(HDC, WORD *)) qglGetProcAddress( "wglGetDeviceGammaRamp3DFX" );
			qwglSetDeviceGammaRamp3DFX = ( BOOL (WINAPI *)(HDC, WORD *)) qglGetProcAddress( "wglSetDeviceGammaRamp3DFX" );

			if( !qwglGetDeviceGammaRamp3DFX || !qwglSetDeviceGammaRamp3DFX ) {
				Com_Printf( "R_CheckExtensions: broken 3DFX gamma support, contact your video card vendor\n" );
			}
		}
	}
#endif

	if( gl_ext_texture_env_add->integer && glConfig.multiTexture ) {
		if( strstr( glConfig.extensionsString, "GL_ARB_texture_env_add" ) )
			glConfig.textureEnvAdd = qtrue;
	}

	if ( gl_ext_texture_env_combine->integer && glConfig.multiTexture ) {
		if( strstr( glConfig.extensionsString, "GL_ARB_texture_env_combine" ) || 
			strstr( glConfig.extensionsString, "GL_EXT_texture_env_combine" ) )
			glConfig.textureEnvCombine = qtrue;
	}

	if( gl_ext_texture_env_dot3->integer && glConfig.textureEnvCombine ) {
		if( strstr( glConfig.extensionsString, "GL_ARB_texture_env_dot3" ) )
			glConfig.textureEnvDot3 = qtrue;
	}

	if( gl_ext_NV_texture_env_combine4->integer && glConfig.textureEnvCombine ) {
		if ( strstr( glConfig.extensionsString, "NV_texture_env_combine4" ) )
			glConfig.NVTextureEnvCombine4 = qtrue;
	}

	if( gl_ext_compressed_textures->integer ) {
		if( strstr( glConfig.extensionsString, "GL_ARB_texture_compression" ) )
			glConfig.compressedTextures = qtrue;
	}

	if( gl_ext_texture_edge_clamp->integer ) {
		if( strstr( glConfig.extensionsString, "GL_EXT_texture_edge_clamp" ) ||
			strstr( glConfig.extensionsString, "GL_SGIS_texture_edge_clamp" ) )
			glConfig.textureEdgeClamp = qtrue;
	}

	if( gl_ext_texture_filter_anisotropic->integer ) {
		if( strstr( glConfig.extensionsString, "GL_EXT_texture_filter_anisotropic" ) )
			glConfig.textureFilterAnisotropic = qtrue;
	}

	if( gl_ext_texture_cube_map->integer ) {
		if( strstr( glConfig.extensionsString, "GL_ARB_texture_cube_map" ) )
			glConfig.textureCubeMap = qtrue;
	}

	if( gl_ext_bgra->integer ) {
		if( strstr( glConfig.extensionsString, "GL_EXT_bgra" ) )
			glConfig.bgra = qtrue;
	}
}

/*
===============
R_SetDefaultState
===============
*/
void R_SetDefaultState( void )
{
	memset( &glState, 0, sizeof(glState) );

	// set our "safe" modes
	glState.previousMode = 3;
	glState.initializedMedia = qfalse;

	if( r_ignorehwgamma->integer )
		glState.invPow2Ovrbr = 1.0f;
	else
		glState.invPow2Ovrbr = 1.0 / (float)( 1 << max( 0, r_overbrightbits->integer ) );

	if( r_ignorehwgamma->integer )
		glState.pow2MapOvrbr = r_mapoverbrightbits->integer;
	else
		glState.pow2MapOvrbr = r_mapoverbrightbits->integer - r_overbrightbits->integer;

	if( glState.pow2MapOvrbr > 0 )
		glState.pow2MapOvrbr = pow( 2, glState.pow2MapOvrbr ) / 255.0;
	else
		glState.pow2MapOvrbr = 1.0 / 255.0;
}

/*
===============
R_SetGLDefaults
===============
*/
void R_SetGLDefaults( void )
{
	int i;

	qglFinish ();

	qglClearColor( 1, 0, 0.5, 0.5 );

	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_STENCIL_TEST );

	qglColor4f( 1, 1, 1, 1 );

	// enable gouraud shading
	qglShadeModel( GL_SMOOTH );

	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglPolygonOffset( -1, -2 );

	// properly disable multitexturing at startup
	for( i = glConfig.maxTextureUnits-1; i > 0; i-- ) {
		GL_SelectTexture( i );
		GL_TexEnv( GL_MODULATE );
		qglDisable( GL_BLEND );
		qglDisable( GL_TEXTURE_2D );
	}

	GL_SelectTexture( 0 );
	GL_TexEnv( GL_MODULATE );
	qglDisable( GL_BLEND );
	qglEnable( GL_TEXTURE_2D );

	R_TextureMode( r_texturemode->string );

	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );

	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
}

/*
================== 
R_GfxInfo_f
================== 
*/
void R_GfxInfo_f( void )
{
	Com_Printf( "\n" );
	Com_Printf( "GL_VENDOR: %s\n", glConfig.vendorString );
	Com_Printf( "GL_RENDERER: %s\n", glConfig.rendererString );
	Com_Printf( "GL_VERSION: %s\n", glConfig.versionString );
	Com_Printf( "GL_EXTENSIONS: %s\n", glConfig.extensionsString );
	Com_Printf( "GL_MAX_TEXTURE_SIZE: %i\n", glConfig.maxTextureSize );
	Com_Printf( "GL_MAX_TEXTURE_UNITS: %i\n", glConfig.maxTextureUnits );
//	if( glConfig.textureCubeMap )
		Com_Printf( "GL_MAX_CUBE_MAP_TEXTURE_SIZE: %i\n", glConfig.maxTextureCubemapSize );
//	if( glConfig.textureFilterAnisotropic )
		Com_Printf( "GL_MAX_TEXTURE_MAX_ANISOTROPY: %i\n", glConfig.maxTextureFilterAnisotropic );
	Com_Printf( "\n" );

	Com_Printf( "mode: %i, %s\n", r_mode->integer, glState.fullScreen ? "fullscreen" : "" );
	Com_Printf( "CDS: %s\n", glConfig.allowCDS ? "enabled" : "disabled" );
	Com_Printf( "picmip: %i\n", r_picmip->integer );
	Com_Printf( "texturemode: %s\n", r_texturemode->string );
	Com_Printf( "swap interval: %s\n", r_swapinterval->integer ? "enabled" : "disabled" );
	Com_Printf( "compiled vertex array: %s\n", glConfig.compiledVertexArray ? "enabled" : "disabled" );
	Com_Printf( "multitexture: %s\n", glConfig.multiTexture ? "enabled" : "disabled" );
	Com_Printf( "texture cube map: %s\n", glConfig.textureCubeMap ? "enabled" : "disabled" );
	Com_Printf( "texenv add: %s\n", glConfig.textureEnvAdd ? "enabled" : "disabled" );
	Com_Printf( "texenv combine: %s\n", glConfig.textureEnvCombine ? "enabled" : "disabled" );
	Com_Printf( "texenv dot3: %s\n", glConfig.textureEnvDot3 ? "enabled" : "disabled" );
	Com_Printf( "NVtexenv combine4: %s\n", glConfig.NVTextureEnvCombine4 ? "enabled" : "disabled" );
	Com_Printf( "texture edge clamp: %s\n", glConfig.textureEdgeClamp ? "enabled" : "disabled" );
	Com_Printf( "anisotropic filtering: %s\n", glConfig.textureFilterAnisotropic ? "enabled" : "disabled" );
	Com_Printf( "compressed textures: %s\n", glConfig.compressedTextures ? "enabled" : "disabled" );
	Com_Printf( "draw range elements: %s\n", glConfig.drawRangeElements ? "enabled" : "disabled" );
	Com_Printf( "vertex buffer object: %s\n", glConfig.vertexBufferObject ? "enabled" : "disabled" );
	Com_Printf( "BGRA byte order: %s\n", glConfig.bgra ? "enabled" : "disabled" );
}

/*
===============
R_Init
===============
*/
int R_Init( void *hinstance, void *hWnd )
{
	char renderer_buffer[1024];
	char vendor_buffer[1024];
	int	 err;

	r_firstTime = qtrue;

	Com_Printf( "\n----- R_Init -----\n" );

	Com_Printf( "ref_gl version: "REF_VERSION"\n");

	R_Register ();
	R_SetDefaultState ();

	glConfig.allowCDS = qtrue;

	// initialize our QGL dynamic bindings
	if( !QGL_Init( gl_driver->string ) ) {
		QGL_Shutdown ();
		Com_Printf( "ref_gl::R_Init() - could not load \"%s\"\n", gl_driver->string );
		return -1;
	}

	// initialize OS-specific parts of OpenGL
	if( !GLimp_Init( hinstance, hWnd ) ) {
		QGL_Shutdown ();
		return -1;
	}

	// create the window and set up the context
	if( !R_SetMode () ) {
		QGL_Shutdown ();
		Com_Printf( "ref_gl::R_Init() - could not R_SetMode()\n" );
		return -1;
	}

	/*
	** get our various GL strings
	*/
	glConfig.vendorString = qglGetString (GL_VENDOR);
	glConfig.rendererString = qglGetString (GL_RENDERER);
	glConfig.versionString = qglGetString (GL_VERSION);
	glConfig.extensionsString = qglGetString (GL_EXTENSIONS);

	Q_strncpyz( renderer_buffer, glConfig.rendererString, sizeof(renderer_buffer) );
	Q_strlwr( renderer_buffer );

	Q_strncpyz( vendor_buffer, glConfig.vendorString, sizeof(vendor_buffer) );
	Q_strlwr( vendor_buffer );

	if( strstr( renderer_buffer, "voodoo" ) ) {
		if( !strstr( renderer_buffer, "rush" ) )
			glConfig.renderer = GL_RENDERER_VOODOO;
		else
			glConfig.renderer = GL_RENDERER_VOODOO_RUSH;
	} else if( strstr( vendor_buffer, "sgi" ) )
		glConfig.renderer = GL_RENDERER_SGI;
	else if( strstr( renderer_buffer, "permedia" ) )
		glConfig.renderer = GL_RENDERER_PERMEDIA2;
	else if( strstr( renderer_buffer, "glint" ) )
		glConfig.renderer = GL_RENDERER_GLINT_MX;
	else if( strstr( renderer_buffer, "glzicd" ) )
		glConfig.renderer = GL_RENDERER_REALIZM;
	else if( strstr( renderer_buffer, "gdi" ) )
		glConfig.renderer = GL_RENDERER_MCD;
	else if( strstr( renderer_buffer, "pcx2" ) )
		glConfig.renderer = GL_RENDERER_PCX2;
	else if( strstr( renderer_buffer, "verite" ) )
		glConfig.renderer = GL_RENDERER_RENDITION;
	else
		glConfig.renderer = GL_RENDERER_OTHER;

#ifdef GL_FORCEFINISH
	Cvar_SetValue( "gl_finish", 1 );
#endif

	// MCD has buffering issues
	if( glConfig.renderer == GL_RENDERER_MCD )
		Cvar_SetValue( "gl_finish", 1 );

	glConfig.allowCDS = qtrue;
	if( glConfig.renderer & GL_RENDERER_3DLABS ) {
		if( r_3dlabs_broken->integer )
			glConfig.allowCDS = qfalse;
		else
			glConfig.allowCDS = qtrue;
	}

	R_CheckExtensions ();

	R_SetGLDefaults ();

	qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.maxTextureSize );
	if( glConfig.maxTextureSize <= 0 )
		glConfig.maxTextureSize = 256;

	qglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.maxTextureCubemapSize );

	if( !glConfig.multiTexture ) {
		glConfig.maxTextureUnits = 1;
	} else {
		qglGetIntegerv( GL_MAX_TEXTURE_UNITS, &glConfig.maxTextureUnits );
		if( glConfig.maxTextureUnits <= 0 )
			glConfig.maxTextureUnits = 1;
		else if( glConfig.maxTextureUnits > MAX_TEXTURE_UNITS )
			glConfig.maxTextureUnits = MAX_TEXTURE_UNITS;
	}
	
	qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.maxTextureFilterAnisotropic );
	Cvar_ForceSet( "gl_ext_max_texture_filter_anisotropic", va ("%i", glConfig.maxTextureFilterAnisotropic) );

	R_GfxInfo_f ();

	R_BackendInit ();

	R_ClearScene ();

	glState.initializedMedia = qtrue;

	err = qglGetError();
	if( err != GL_NO_ERROR )
		Com_Printf( "glGetError() = 0x%x\n", err);

	Com_Printf( "----- finished R_Init -----\n" );

	return qfalse;
}

/*
===============
R_InitMedia
===============
*/
void R_InitMedia( void )
{
	if( glState.initializedMedia )
		return;

	R_InitLightStyles ();
	R_InitImages ();
	R_InitShaders( !r_firstTime );
	R_InitModels ();
	R_InitSkinFiles ();

	glState.currentTMU = 0;
	memset( glState.currentTextures, -1, sizeof(glState.currentTextures) );
	memset( glState.currentEnvModes, -1, sizeof(glState.currentEnvModes) );

	glState.initializedMedia = qtrue;
}

/*
===============
R_FreeMedia
===============
*/
void R_FreeMedia( void )
{
	if( !glState.initializedMedia )
		return;

	R_ShutdownSkinFiles ();
	R_ShutdownModels ();
	R_ShutdownShaders ();
	R_ShutdownImages ();

	glState.initializedMedia = qfalse;
}

/*
===============
R_Restart
===============
*/
void R_Restart( void )
{
	if( r_firstTime )
		Com_Printf( "\n" );

	R_FreeMedia ();
	R_InitMedia ();

	if( !r_firstTime )
		Com_Printf( "\n" );

	r_firstTime = qfalse;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown( void )
{
	Cmd_RemoveCommand( "modellist" );
	Cmd_RemoveCommand( "screenshot" );
	Cmd_RemoveCommand( "imagelist" );
	Cmd_RemoveCommand( "gfxinfo" );
	Cmd_RemoveCommand( "shaderlist" );

	// free shaders, models, etc.
	R_FreeMedia ();

	// shutdown rendering backend
	R_BackendShutdown ();

	// restore original gamma
	if( glState.hwGamma )
		GLimp_SetGammaRamp( 256, glState.orignalGammaRamp );

	// shut down OS specific OpenGL stuff like contexts, etc.
	GLimp_Shutdown ();

	// shutdown our QGL subsystem
	QGL_Shutdown ();
}
