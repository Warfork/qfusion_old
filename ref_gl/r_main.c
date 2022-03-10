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

float		gldepthmin, gldepthmax;

mapconfig_t	mapConfig;
glconfig_t	glConfig;
glstate_t	glState;

refinst_t   ri;
refdef_t	r_lastRefdef;

qboolean	r_firstTime;

image_t		*r_cintexture;		// cinematic texture
image_t		*r_notexture;		// use for bad textures
image_t		*r_particletexture;	// little dot for particles
image_t		*r_whitetexture;
image_t		*r_blacktexture;
image_t		*r_dlighttexture;
image_t		*r_fogtexture;
image_t		*r_blanknormalmaptexture;

int			r_numnullentities;
entity_t	*r_nullentities[MAX_EDICTS];

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_world_leafs;

int			r_mark_leaves, r_world_node;
int			r_add_polys, r_add_entities;
int			r_sort_meshes, r_draw_meshes;

void		R_GfxInfo_f( void );

//
// screen size info
//
int			r_numEntities;
entity_t	r_entities[MAX_ENTITIES];
entity_t	*r_worldent = &r_entities[0];

int			r_numDlights;
dlight_t	r_dlights[MAX_DLIGHTS];

int			r_numPolys;
poly_t		r_polys[MAX_POLYS];

lightstyle_t	r_lightStyles[MAX_LIGHTSTYLES];

byte_vec4_t	r_customColors[NUM_CUSTOMCOLORS];

int			r_viewcluster, r_oldviewcluster;

float		r_farclip_min, r_farclip_bias = 64.0f;

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
cvar_t	*r_draworder;
cvar_t	*r_packlightmaps;
cvar_t	*r_spherecull;
cvar_t	*r_maxlmblocksize;
cvar_t	*r_portalonly;

cvar_t	*r_lighting_bumpscale;
cvar_t	*r_lighting_deluxemapping;
cvar_t	*r_lighting_diffuse2heightmap;
cvar_t	*r_lighting_specular;
cvar_t	*r_lighting_glossintensity;
cvar_t	*r_lighting_glossexponent;
cvar_t	*r_lighting_models_followdeluxe;
cvar_t	*r_lighting_ambientscale;
cvar_t	*r_lighting_directedscale;

cvar_t	*r_shadows;	
cvar_t	*r_shadows_alpha;
cvar_t	*r_shadows_nudge;
cvar_t	*r_shadows_projection_distance;

cvar_t	*r_bloom;
cvar_t	*r_bloom_alpha;
cvar_t	*r_bloom_diamond_size;
cvar_t	*r_bloom_intensity;
cvar_t	*r_bloom_darken;
cvar_t	*r_bloom_sample_size;
cvar_t	*r_bloom_fast_sample;

cvar_t	*r_allow_software;
cvar_t	*r_3dlabs_broken;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;

cvar_t	*r_environment_color;
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
cvar_t	*gl_ext_NV_texture_env_combine4;
cvar_t	*gl_ext_compressed_textures;
cvar_t	*gl_ext_texture_edge_clamp;
cvar_t	*gl_ext_texture_filter_anisotropic;
cvar_t	*gl_ext_max_texture_filter_anisotropic;
cvar_t	*gl_ext_draw_range_elements;
cvar_t	*gl_ext_vertex_buffer_object;
cvar_t	*gl_ext_texture_cube_map;
cvar_t	*gl_ext_BGRA;
cvar_t	*gl_ext_texture3D;
cvar_t	*gl_ext_GLSL;

cvar_t	*gl_drawbuffer;
cvar_t  *gl_driver;
cvar_t	*gl_finish;
cvar_t	*gl_delayfinish;
cvar_t	*gl_cull;

cvar_t	*vid_fullscreen;

/*
=================
GL_Cull
=================
*/
void GL_Cull( int cull )
{
	if( glState.faceCull == cull )
		return;

	if( !cull ) {
		qglDisable( GL_CULL_FACE );
		glState.faceCull = 0;
		return;
	}

	if( !glState.faceCull )
		qglEnable( GL_CULL_FACE );
	qglCullFace( cull );
	glState.faceCull = cull;
}

/*
=================
GL_SetState
=================
*/
void GL_SetState( int state )
{
	int diff;

	if( glState.in2DMode )
		state |= GLSTATE_NO_DEPTH_TEST;
	if( state & GLSTATE_NO_DEPTH_TEST )
		state &= ~(GLSTATE_DEPTHWRITE|GLSTATE_DEPTHFUNC_EQ);

	diff = glState.flags ^ state;
	if( !diff )
		return;

	if( diff & (GLSTATE_BLEND_MTEX|GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK) )
	{
		if( state & (GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK) )
		{
			int blendsrc, blenddst;

			switch( state & GLSTATE_SRCBLEND_MASK )
			{
				case GLSTATE_SRCBLEND_ZERO:
					blendsrc = GL_ZERO;
					break;
				case GLSTATE_SRCBLEND_DST_COLOR:
					blendsrc = GL_DST_COLOR;
					break;
				case GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR:
					blendsrc = GL_ONE_MINUS_DST_COLOR;
					break;
				case GLSTATE_SRCBLEND_SRC_ALPHA:
					blendsrc = GL_SRC_ALPHA;
					break;
				case GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA:
					blendsrc = GL_ONE_MINUS_SRC_ALPHA;
					break;
				case GLSTATE_SRCBLEND_DST_ALPHA:
					blendsrc = GL_DST_ALPHA;
					break;
				case GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA:
					blendsrc = GL_ONE_MINUS_DST_ALPHA;
					break;
				default:
				case GLSTATE_SRCBLEND_ONE:
					blendsrc = GL_ONE;
					break;
			}

			switch( state & GLSTATE_DSTBLEND_MASK )
			{
				case GLSTATE_DSTBLEND_ONE:
					blenddst = GL_ONE;
					break;
				case GLSTATE_DSTBLEND_SRC_COLOR:
					blenddst = GL_SRC_COLOR;
					break;
				case GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR:
					blenddst = GL_ONE_MINUS_SRC_COLOR;
					break;
				case GLSTATE_DSTBLEND_SRC_ALPHA:
					blenddst = GL_SRC_ALPHA;
					break;
				case GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA:
					blenddst = GL_ONE_MINUS_SRC_ALPHA;
					break;
				case GLSTATE_DSTBLEND_DST_ALPHA:
					blenddst = GL_DST_ALPHA;
					break;
				case GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA:
					blenddst = GL_ONE_MINUS_DST_ALPHA;
					break;
				default:
				case GLSTATE_DSTBLEND_ZERO:
					blenddst = GL_ZERO;
					break;
			}

			if( state & GLSTATE_BLEND_MTEX )
			{
				if( glState.currentEnvModes[glState.currentTMU] != GL_REPLACE )
					qglEnable( GL_BLEND );
				else
					qglDisable( GL_BLEND );
			}
			else
			{
				qglEnable( GL_BLEND );
			}

			qglBlendFunc( blendsrc, blenddst );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	if( diff & GLSTATE_ALPHAFUNC )
	{
		if( state & GLSTATE_ALPHAFUNC )
		{
			if( !(glState.flags & GLSTATE_ALPHAFUNC) )
				qglEnable( GL_ALPHA_TEST );

			if( state & GLSTATE_AFUNC_GT0 )
				qglAlphaFunc( GL_GREATER, 0 );
			else if( state & GLSTATE_AFUNC_LT128 )
				qglAlphaFunc( GL_LESS, 0.5f );
			else
				qglAlphaFunc( GL_GEQUAL, 0.5f );
		}
		else
		{
			qglDisable( GL_ALPHA_TEST );
		}
	}

	if( diff & GLSTATE_DEPTHFUNC_EQ )
	{
		if( state & GLSTATE_DEPTHFUNC_EQ )
			qglDepthFunc( GL_EQUAL );
		else
			qglDepthFunc( GL_LEQUAL );
	}

	if( diff & GLSTATE_DEPTHWRITE )
	{
		if( state & GLSTATE_DEPTHWRITE )
			qglDepthMask( GL_TRUE );
		else
			qglDepthMask( GL_FALSE );
	}

	if( diff & GLSTATE_NO_DEPTH_TEST )
	{
		if( state & GLSTATE_NO_DEPTH_TEST )
			qglDisable( GL_DEPTH_TEST );
		else
			qglEnable( GL_DEPTH_TEST );
	}

	if( diff & GLSTATE_OFFSET_FILL )
	{
		if( state & GLSTATE_OFFSET_FILL )
			qglEnable( GL_POLYGON_OFFSET_FILL );
		else
			qglDisable( GL_POLYGON_OFFSET_FILL );
	}

	glState.flags = state;
}

/*
=================
R_SetCustomColor
=================
*/
void R_SetCustomColor( int num, int r, int g, int b )
{
	if( num < 0 || num >= NUM_CUSTOMCOLORS )
		return;
	Vector4Set( r_customColors[num], (qbyte)r, (qbyte)g, (qbyte)b, 255 );
}

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

	for (i=0,p=ri.frustum ; i<4; i++,p++)
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

	for (i=0,p=ri.frustum ; i<4; i++,p++)
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

	if( !r_worldmodel || ( ri.refdef.rdflags & RDF_NOWORLDMODEL ) )
		return qfalse;
	if( r_novis->integer )
		return qfalse;

	for( s = 0; s < 3; s++ ) {
		extmins[s] = mins[s] - 4;
		extmaxs[s] = maxs[s] + 4;
	}

	for( node = r_worldmodel->nodes; ; ) {
		if( node->visframe != r_visframecount ) {
			if( !stackdepth )
				return qtrue;
			node = localstack[--stackdepth];
			continue;
		}

		if( !node->plane )
			return qfalse;

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

	if( !r_worldmodel || ( ri.refdef.rdflags & RDF_NOWORLDMODEL ) )
		return qfalse;
	if( r_novis->integer )
		return qfalse;

	radius += 4;
	for( node = r_worldmodel->nodes; ; ) {
		if( node->visframe != r_visframecount ) {
			if( !stackdepth )
				return qtrue;
			node = localstack[--stackdepth];
			continue;
		}

		if( !node->plane )
			return qfalse;

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
R_CullModel
=============
*/
qboolean R_CullModel( entity_t *e, vec3_t mins, vec3_t maxs, float radius )
{
	int i;
	vec3_t bbox[8];

	if( e->flags & RF_WEAPONMODEL )
		if( !(ri.params & (RP_PORTALVIEW|RP_SKYPORTALVIEW)) )
			return qfalse;
	if( e->flags & RF_VIEWERMODEL )
		if( ri.params & RP_MIRRORVIEW )
			return qfalse;

	if( r_spherecull->integer ) {
		if( R_CullSphere( e->origin, radius, 15 ) )
			return qtrue;
	} else {
		// compute and rotate a full bounding box
		for( i = 0; i < 8; i++ ) {
			vec3_t tmp;

			tmp[0] = ( ( i & 1 ) ? mins[0] : maxs[0] );
			tmp[1] = ( ( i & 2 ) ? mins[1] : maxs[1] );
			tmp[2] = ( ( i & 4 ) ? mins[2] : maxs[2] );

			Matrix_TransformVector( e->axis, tmp, bbox[i] );
			bbox[i][0] += e->origin[0];
			bbox[i][1] = -bbox[i][1] + e->origin[1];
			bbox[i][2] += e->origin[2];
		}

		{
			int p, f, aggregatemask = ~0;

			for( p = 0; p < 8; p++ ) {
				int mask = 0;

				for( f = 0; f < 4; f++ ) {
					if ( DotProduct( ri.frustum[f].normal, bbox[p] ) < ri.frustum[f].dist )
						mask |= ( 1 << f );
				}
				aggregatemask &= mask;
			}

			if ( aggregatemask )
				return qtrue;
			return qfalse;
		}
	}

	if( ri.refdef.rdflags & RDF_PORTALINVIEW ) {
		if( R_VisCullSphere( e->origin, radius ) )
			return qtrue;
	}

	if( (ri.params & (RP_MIRRORVIEW|RP_PORTALVIEW)) && !r_nocull->integer ) {
		if( PlaneDiff( e->origin, &(ri.clipPlane) ) < -radius )
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
	cplane_t	*plane;

	if( !r_worldmodel || (ri.refdef.rdflags & RDF_NOWORLDMODEL) || !r_worldmodel->numfogs )
		return NULL;
	if( r_worldmodel->globalfog )
		return r_worldmodel->globalfog;

	fog = r_worldmodel->fogs;
	for( i = 0; i < r_worldmodel->numfogs; i++, fog++ ) {
		if( !fog->shader )
			continue;

		plane = fog->planes;
		for( j = 0; j < fog->numplanes; j++, plane++ ) {
			// if completely in front of face, no intersection
			if( PlaneDiff( centre, plane ) > radius )
				break;
		}

		if( j == fog->numplanes )
			return fog;
	}

	return NULL;
}

/*
=============
R_CompletelyFogged
=============
*/
qboolean R_CompletelyFogged( mfog_t *fog, vec3_t origin, float radius )
{
	// note that fog->distanceToEye < 0 is always true if 
	// globalfog is not NULL and we're inside the world boundaries
	if( fog && fog->shader && ri.fog_dist_to_eye[fog-r_worldmodel->fogs] < 0 ) {
		float vpnDist = ((ri.viewOrigin[0] - origin[0]) * ri.vpn[0] + (ri.viewOrigin[1] - origin[1]) * ri.vpn[1] + (ri.viewOrigin[2] - origin[2]) * ri.vpn[2]);
		return ((vpnDist + radius) * fog->shader->fog_dist) < -1;
	}

	return qfalse;
}

/*
=============
R_LoadIdentity
=============
*/
void R_LoadIdentity( void )
{
	Matrix4_Copy( ri.worldviewMatrix, ri.modelviewMatrix );
	qglLoadMatrixf( ri.modelviewMatrix );
}

/*
=============
R_RotateForEntity
=============
*/
void R_RotateForEntity( entity_t *e )
{
	mat4x4_t obj_m;

	if( e == r_worldent ) {
		R_LoadIdentity ();
		return;
	}

	if( e->scale != 1.0f && e->model && (e->model->type == mod_brush) ) {
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

	Matrix4_MultiplyFast( ri.worldviewMatrix, obj_m, ri.modelviewMatrix );
	qglLoadMatrixf( ri.modelviewMatrix );
}

/*
=============
R_TranslateForEntity
=============
*/
void R_TranslateForEntity( entity_t *e )
{
	mat4x4_t obj_m;

	if( e == r_worldent ) {
		R_LoadIdentity ();
		return;
	}

	Matrix4_Identity( obj_m );

	obj_m[12] = e->origin[0];
	obj_m[13] = e->origin[1];
	obj_m[14] = e->origin[2];

	Matrix4_MultiplyFast( ri.worldviewMatrix, obj_m, ri.modelviewMatrix );
	qglLoadMatrixf( ri.modelviewMatrix );
}

/*
=============
R_LerpTag
=============
*/
qboolean R_LerpTag( orientation_t *orient, model_t *mod, int oldframe, int frame, float lerpfrac, const char *name )
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

  SPRITE MODELS AND FLARES

=============================================================
*/

static	vec3_t			spr_xyz[4];
static	vec2_t			spr_st[4] = { {0, 1}, {0, 0}, {1,0}, {1,1} };
static	byte_vec4_t		spr_color[4];

static	mesh_t			spr_mesh;
static	meshbuffer_t	spr_mbuffer;

/*
=================
R_PushSprite
=================
*/
static qboolean R_PushSprite( const meshbuffer_t *mb, float rotation, float right, float left, float up, float down )
{
	vec3_t		point;
	vec3_t		v_right, v_up;
	int			features;
	entity_t	*e = ri.currententity;
	shader_t	*shader;

	if( rotation ) {
		RotatePointAroundVector( v_right, ri.vpn, ri.vright, rotation );
		CrossProduct( ri.vpn, v_right, v_up );
	} else {
		VectorCopy( ri.vright, v_right );
		VectorCopy( ri.vup, v_up );
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

	MB_NUM2SHADER( mb->shaderkey, shader );

	// the code below is disgusting, but some q3a shaders use 'rgbgen vertex'
	// and 'alphagen vertex' for effects instead of 'rgbgen entity' and 'alphagen entity'
	if ( shader->features & MF_COLORS ) {
		Vector4Copy( e->color, spr_color[0] );
		Vector4Copy( e->color, spr_color[1] );
		Vector4Copy( e->color, spr_color[2] );
		Vector4Copy( e->color, spr_color[3] );
	}

	features = MF_NOCULL | MF_TRIFAN | shader->features;
	if( r_shownormals->integer )
		features |= MF_NORMALS;

	spr_mesh.numIndexes = 6;
	spr_mesh.numVertexes = 4;
	spr_mesh.xyzArray = spr_xyz;
	spr_mesh.stArray = spr_st;
	spr_mesh.colorsArray[0] = spr_color;

	if( shader->flags & SHADER_ENTITY_MERGABLE ) {
		VectorAdd( spr_xyz[0], e->origin, spr_xyz[0] );
		VectorAdd( spr_xyz[1], e->origin, spr_xyz[1] );
		VectorAdd( spr_xyz[2], e->origin, spr_xyz[2] );
		VectorAdd( spr_xyz[3], e->origin, spr_xyz[3] );
		R_PushMesh( &spr_mesh, features );
		return qfalse;
	}


	R_PushMesh( &spr_mesh, MF_NONBATCHED | features );
	return qtrue;
}

/*
=================
R_PushSpriteModel
=================
*/
qboolean R_PushSpriteModel( const meshbuffer_t *mb )
{
	sframe_t	*frame;
	smodel_t	*psprite;
	entity_t	*e = ri.currententity;
	model_t		*model = e->model;

	psprite = model->smodel;
	frame = psprite->frames + e->frame;

	return R_PushSprite( mb, e->rotation, frame->origin_x, frame->origin_x - frame->width, frame->height - frame->origin_y, -frame->origin_y );
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
	float		dist;
	meshbuffer_t *mb;

	if( !(psprite = model->smodel) )
		return;

	dist = 
		(e->origin[0] - ri.refdef.vieworg[0]) * ri.vpn[0] +
		(e->origin[1] - ri.refdef.vieworg[1]) * ri.vpn[1] + 
		(e->origin[2] - ri.refdef.vieworg[2]) * ri.vpn[2];
	if (dist < 0)
		return;		// cull it because we don't want to sort unneeded things

	e->frame %= psprite->numframes;
	frame = psprite->frames + e->frame;

	// select skin
	if( e->customShader )
		mb = R_AddMeshToList( MB_MODEL, R_FogForSphere( e->origin, frame->radius ), e->customShader, -1 );
	else
		mb = R_AddMeshToList( MB_MODEL, R_FogForSphere( e->origin, frame->radius ), frame->shader, -1 );

	if( mb )	// hack in approx distance for sorting purposes
		mb->shaderkey |= (bound(1, 0x4000 - (unsigned int)dist, 0x4000 - 1) << 12);
}

/*
=================
R_PushSpritePoly
=================
*/
qboolean R_PushSpritePoly( const meshbuffer_t *mb )
{
	entity_t *e = ri.currententity;

	if( mb->infokey > 0 ) {
		R_PushFlareSurf( mb );
		return qfalse;
	}

	return R_PushSprite( mb, e->rotation, -e->radius, e->radius, e->radius, -e->radius );
}

/*
=================
R_AddSpritePolyToList
=================
*/
void R_AddSpritePolyToList (entity_t *e)
{
	float dist;
	meshbuffer_t *mb;

	// select skin
	if( !e->customShader )
		return;

	dist = 
		(e->origin[0] - ri.refdef.vieworg[0]) * ri.vpn[0] +
		(e->origin[1] - ri.refdef.vieworg[1]) * ri.vpn[1] + 
		(e->origin[2] - ri.refdef.vieworg[2]) * ri.vpn[2];
	if (dist < 0)
		return;		// cull it because we don't want to sort unneeded things

	mb = R_AddMeshToList( MB_SPRITE, R_FogForSphere( e->origin, e->radius ), e->customShader, -1 );

	if( mb )	// hack in approx distance for sorting purposes
		mb->shaderkey |= (bound(1, 0x4000 - (unsigned int)dist, 0x4000 - 1) << 12);
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
	msurface_t *surf = &ri.currentmodel->surfaces[mb->infokey - 1];
	shader_t *shader;

	if( ri.currentmodel != r_worldmodel ) {
		Matrix_TransformVector( ri.currententity->axis, surf->origin, origin );
		VectorAdd( origin, ri.currententity->origin, origin );
	} else {
		VectorCopy( surf->origin, origin );
	}
	R_TransformToScreen_Vec3( origin, v );

	if( v[0] < ri.refdef.x || v[0] > ri.refdef.x + ri.refdef.width )
		return;
	if( v[1] < ri.refdef.y || v[1] > ri.refdef.y + ri.refdef.height )
		return;

	qglReadPixels( (int)(v[0]/* + 0.5f*/), (int)(v[1]/* + 0.5f*/), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );
	if( depth + 1e-4 < v[2] )
		return;		// occluded

	VectorCopy( origin, origin );

	VectorMA( origin, down, ri.vup, point );
	VectorMA( point, -left, ri.vright, spr_xyz[0] );
	VectorMA( point, -right, ri.vright, spr_xyz[3] );

	VectorMA( origin, up, ri.vup, point );
	VectorMA( point, -left, ri.vright, spr_xyz[1] );
	VectorMA( point, -right, ri.vright, spr_xyz[2] );

	flarescale = 255.0 / r_flarefade->value;
	Vector4Set( color, 
		surf->color[0] * flarescale,
		surf->color[1] * flarescale,
		surf->color[2] * flarescale,
		255 );
	clamp( color[0], 0, 255 );
	clamp( color[1], 0, 255 );
	clamp( color[2], 0, 255 );

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

	MB_NUM2SHADER( mb->shaderkey, shader );

	R_PushMesh( &spr_mesh, MF_NOCULL | MF_TRIFAN | shader->features );
}


//==================================================================================

/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel( void )
{
	qglBegin ( GL_LINES );

	qglColor4f ( 1, 0, 0, 0.5 );
	qglVertex3fv ( ri.currententity->origin );
	qglVertex3f ( ri.currententity->origin[0] + ri.currententity->axis[0][0] * 15,
		ri.currententity->origin[1] + ri.currententity->axis[0][1] * 15, 
		ri.currententity->origin[2] + ri.currententity->axis[0][2] * 15);

	qglColor4f ( 0, 1, 0, 0.5 );
	qglVertex3fv ( ri.currententity->origin );
	qglVertex3f ( ri.currententity->origin[0] - ri.currententity->axis[1][0] * 15,
		ri.currententity->origin[1] - ri.currententity->axis[1][1] * 15, 
		ri.currententity->origin[2] - ri.currententity->axis[1][2] * 15);

	qglColor4f ( 0, 0, 1, 0.5 );
	qglVertex3fv ( ri.currententity->origin );
	qglVertex3f ( ri.currententity->origin[0] + ri.currententity->axis[2][0] * 15,
		ri.currententity->origin[1] + ri.currententity->axis[2][1] * 15, 
		ri.currententity->origin[2] + ri.currententity->axis[2][2] * 15);

	qglEnd ();
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

	if( ri.params & RP_ENVVIEW ) {
		for( i = 1; i < r_numEntities; i++ ) {
			ri.previousentity = ri.currententity;
			ri.currententity = &r_entities[i];

			if( ri.currententity->rtype != RT_MODEL || !(ri.currentmodel = ri.currententity->model) )
				continue;
			if( ri.currentmodel->type == mod_brush )
				R_AddBrushModelToList( ri.currententity );
		}
	} else {
		for( i = 1; i < r_numEntities; i++ ) {
			ri.previousentity = ri.currententity;
			ri.currententity = &r_entities[i];

			if( ri.params & RP_MIRRORVIEW ) {
				if( ri.currententity->flags & RF_WEAPONMODEL ) 
					continue;
			}

			switch( ri.currententity->rtype ) {
				case RT_MODEL:
					if( !(ri.currentmodel = ri.currententity->model) ) {
						r_nullentities[r_numnullentities++] = ri.currententity;
						break;
					}

					switch( ri.currentmodel->type ) {
						case mod_alias:
							R_AddAliasModelToList( ri.currententity );
							break;
						case mod_skeletal:
							R_AddSkeletalModelToList( ri.currententity );
							break;
						case mod_brush:
							R_AddBrushModelToList( ri.currententity );
							break;
						case mod_sprite:
							R_AddSpriteModelToList( ri.currententity );
							break;
						default:
							Com_Error( ERR_DROP, "%s: bad modeltype", ri.currentmodel->name );
							break;
					}
					break;
				case RT_SPRITE:
					if( ri.currententity->radius )
						R_AddSpritePolyToList( ri.currententity );
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

	qglDisable( GL_TEXTURE_2D );
	GL_SetState( GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	// draw non-transparent first
	for( i = 0; i < r_numnullentities; i++ ) {
		ri.previousentity = ri.currententity;
		ri.currententity = r_nullentities[i];

		if( ri.params & RP_MIRRORVIEW ) {
			if( ri.currententity->flags & RF_WEAPONMODEL ) 
				continue;
		} else {
			if( ri.currententity->flags & RF_VIEWERMODEL ) 
				continue;
		}
		R_DrawNullModel ();
	}

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
	if( ri.refdef.blend[3] < 0.01f )
		return;

	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity ();
	qglOrtho( 0, 1, 1, 0, -99999, 99999 );

	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity ();

	GL_Cull( 0 );
	GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	qglDisable( GL_TEXTURE_2D );

	qglColor4fv( ri.refdef.blend );

	qglBegin( GL_TRIANGLES );
	qglVertex2f( -5, -5 );
	qglVertex2f( 10, -5 );
	qglVertex2f( -5, 10 );
	qglEnd ();

	qglEnable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );
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

	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity ();
	qglOrtho( 0, 1, 1, 0, -99999, 99999 );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity ();

	GL_Cull( 0 );
	GL_SetState( GLSTATE_NO_DEPTH_TEST | GLSTATE_SRCBLEND_DST_COLOR | GLSTATE_DSTBLEND_ONE );

	qglDisable( GL_TEXTURE_2D );

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

	qglEnable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );
}

//=======================================================================

/*
===============
R_SetupFrustum
===============
*/
void R_SetupFrustum( void )
{
	int		i;

	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up

	// rotate ri.vpn right by FOV_X/2 degrees
	RotatePointAroundVector( ri.frustum[0].normal, ri.vup, ri.vpn, -(90-ri.refdef.fov_x / 2 ) );
	// rotate ri.vpn left by FOV_X/2 degrees
	RotatePointAroundVector( ri.frustum[1].normal, ri.vup, ri.vpn, 90-ri.refdef.fov_x / 2 );
	// rotate ri.vpn up by FOV_X/2 degrees
	RotatePointAroundVector( ri.frustum[2].normal, ri.vright, ri.vpn, 90-ri.refdef.fov_y / 2 );
	// rotate ri.vpn down by FOV_X/2 degrees
	RotatePointAroundVector( ri.frustum[3].normal, ri.vright, ri.vpn, -( 90 - ri.refdef.fov_y / 2 ) );

	for( i = 0; i < 4; i++ ) {
		ri.frustum[i].type = PLANE_NONAXIAL;
		ri.frustum[i].dist = DotProduct( ri.viewOrigin, ri.frustum[i].normal );
		ri.frustum[i].signbits = SignbitsForPlane( &ri.frustum[i] );
	}
}

/*
===============
R_FarClip
===============
*/
static float R_FarClip( void )
{
	float farclip_dist = 0;

	if( r_worldmodel && !(ri.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		int i;
		float dist;
		vec3_t tmp;

		for( i = 0; i < 8; i++ ) {
			tmp[0] = ( ( i & 1 ) ? r_worldmodel->nodes[0].mins[0] : r_worldmodel->nodes[0].maxs[0] );
			tmp[1] = ( ( i & 2 ) ? r_worldmodel->nodes[0].mins[1] : r_worldmodel->nodes[0].maxs[1] );
			tmp[2] = ( ( i & 4 ) ? r_worldmodel->nodes[0].mins[2] : r_worldmodel->nodes[0].maxs[2] );

			dist = DistanceSquared( tmp, ri.viewOrigin );
			if( !i )
				farclip_dist = dist;
			else
				farclip_dist = max( farclip_dist, dist );
		}

		farclip_dist = sqrt( farclip_dist );

		if( r_worldmodel->globalfog ) {
			float fogdist = 1.0 / r_worldmodel->globalfog->shader->fog_dist;
			farclip_dist = min( fogdist, farclip_dist );
		}
	}

	return max( max( r_farclip_min, farclip_dist ) + r_farclip_bias, ri.farClip );
}

/*
=============
R_SetupProjectionMatrix
=============
*/
void R_SetupProjectionMatrix( refdef_t *rd, mat4x4_t m )
{
	GLdouble xMin, xMax, yMin, yMax, zNear, zFar;

	if( rd->rdflags & RDF_NOWORLDMODEL )
		ri.farClip = 2048;
	else
		ri.farClip = R_FarClip ();

	zNear = Z_NEAR;
	zFar = ri.farClip;

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
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	mleaf_t *leaf;

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy( ri.refdef.vieworg, ri.viewOrigin );
	AngleVectors( ri.refdef.viewangles, ri.vpn, ri.vright, ri.vup );

	R_SetupProjectionMatrix( &ri.refdef, ri.projectionMatrix );
	if( ri.params & RP_MIRRORVIEW )
		ri.projectionMatrix[0] = -ri.projectionMatrix[0];

	R_SetupModelviewMatrix( &ri.refdef, ri.worldviewMatrix );

	ri.lod_dist_scale_for_fov = tan( ri.refdef.fov_x * (M_PI/180) * 0.5f );

	// current viewcluster
	if( !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) && !(ri.params & RP_MIRRORVIEW) ) {
		if( ri.params & (RP_PORTALVIEW|RP_SKYPORTALVIEW) )
			r_oldviewcluster = -1;
		else
			r_oldviewcluster = r_viewcluster;

		leaf = Mod_PointInLeaf( ri.pvsOrigin, r_worldmodel );
		r_viewcluster = leaf->cluster;
	}
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

	if( !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) && !(ri.params & (RP_MIRRORVIEW|RP_PORTALVIEW|RP_SKYPORTALVIEW)) ) {
		if( r_fastsky->integer ) {
			bits |= GL_COLOR_BUFFER_BIT;
			qglClearColor( 0, 0, 0, 1 );
		}
	}

	if( glState.stencilEnabled && r_shadows->integer >= SHADOW_PLANAR ) {
		qglClearStencil( 128 );
		bits |= GL_STENCIL_BUFFER_BIT;
	}

	qglClear( bits );

	gldepthmin = 0;
	gldepthmax = 1;
	qglDepthRange( gldepthmin, gldepthmax );
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL( void )
{
	qglScissor( ri.scissor[0], ri.scissor[1], ri.scissor[2], ri.scissor[3] );
	qglViewport( ri.refdef.x, glState.height - ri.refdef.height - ri.refdef.y, ri.refdef.width, ri.refdef.height );

	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( ri.projectionMatrix );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadMatrixf( ri.worldviewMatrix );

	if( ri.params & (RP_MIRRORVIEW|RP_PORTALVIEW) ) {
		GLdouble clip[4];

		clip[0] = ri.clipPlane.normal[0];
		clip[1] = ri.clipPlane.normal[1];
		clip[2] = ri.clipPlane.normal[2];
		clip[3] = -ri.clipPlane.dist;

		qglClipPlane( GL_CLIP_PLANE0, clip );
		qglEnable( GL_CLIP_PLANE0 );

		if( ri.params & RP_MIRRORVIEW )
			qglFrontFace( GL_CW );
	}

	GL_Cull( GL_FRONT );
	GL_SetState( GLSTATE_DEPTHWRITE );

	R_Clear ();
}

/*
=============
R_EndGL
=============
*/
void R_EndGL( void )
{
	if( ri.params & (RP_MIRRORVIEW|RP_PORTALVIEW) ) {
		qglDisable( GL_CLIP_PLANE0 );
		if( ri.params & RP_MIRRORVIEW )
			qglFrontFace( GL_CCW );
	}
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
	Matrix4_Multiply_Vector( ri.worldviewMatrix, temp, temp2 );
	Matrix4_Multiply_Vector( ri.projectionMatrix, temp2, temp );

	if( !temp[3] )
		return;

	out[0] = (temp[0] / temp[3] + 1.0f) * 0.5f * ri.refdef.width;
	out[1] = (temp[1] / temp[3] + 1.0f) * 0.5f * ri.refdef.height;
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
================
R_RenderView

ri.refdef must be set before the first call
================
*/
void R_RenderView( refdef_t *fd )
{
	ri.refdef = *fd;
	ri.meshlist->num_meshes = 0;
	ri.meshlist->num_translucent_meshes = 0;

	if( !r_worldmodel && !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) )
		Com_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	R_SetupFrame ();

	R_SetupFrustum ();

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

	R_DrawPortals ();

	if( r_portalonly->integer && !(ri.params & (RP_MIRRORVIEW|RP_PORTALVIEW)) )
		return;

	R_SetupGL ();

	if( r_speeds->integer )
		r_draw_meshes = Sys_Milliseconds ();
	R_DrawMeshes( qfalse );
	if( r_speeds->integer )
		r_draw_meshes = Sys_Milliseconds () - r_draw_meshes;

	R_BackendCleanUpTextureUnits ();

	R_DrawTriangleOutlines ();

	R_DrawNullEntities ();

	R_EndGL ();
}

//=======================================================================

static vec3_t			pic_xyz[4];
static const vec3_t		pic_normals[4] = { {0,1,0}, {0,1,0}, {0,1,0}, {0,1,0} };
static vec2_t			pic_st[4];
static byte_vec4_t		pic_colors[4];

static mesh_t			pic_mesh;
static meshbuffer_t		pic_mbuffer;

/*
===============
R_Set2DMode
===============
*/
void R_Set2DMode( qboolean enable )
{
	if( enable ) {
		if( glState.in2DMode )
			return;

		// set 2D virtual screen size
		qglScissor( 0, 0, glState.width, glState.height );
		qglViewport( 0, 0, glState.width, glState.height );
		qglMatrixMode( GL_PROJECTION );
		qglLoadIdentity ();
		qglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );
		qglMatrixMode( GL_MODELVIEW );
		qglLoadIdentity ();

		GL_Cull( 0 );
		GL_SetState( GLSTATE_NO_DEPTH_TEST );

		qglColor4f( 1, 1, 1, 1 );

		glState.in2DMode = qtrue;

#if SHADOW_VOLUMES
	pic_mesh.trneighbors = NULL;
#endif

		pic_mesh.numVertexes = 4;
		pic_mesh.numIndexes = 6;
		pic_mesh.xyzArray = pic_xyz;
		pic_mesh.stArray = pic_st;
		pic_mesh.normalsArray = (vec3_t *)pic_normals;
		pic_mesh.colorsArray[0] = pic_colors;

		pic_mbuffer.infokey = -1;
		pic_mbuffer.shaderkey = 0;
	} else {
		if( pic_mbuffer.infokey != -1 ) {
			R_RenderMeshBuffer( &pic_mbuffer, qfalse );
			pic_mbuffer.infokey = -1;
		}

		glState.in2DMode = qfalse;
	}
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

	if( pic_mbuffer.shaderkey != shader->sortkey || -pic_mbuffer.infokey-1+4 > MAX_ARRAY_VERTS ) {
		if( pic_mbuffer.shaderkey ) {
			pic_mbuffer.infokey = -1;
			R_RenderMeshBuffer( &pic_mbuffer, qfalse );
		}
	}

	pic_mbuffer.infokey -= 4;
	pic_mbuffer.shaderkey = shader->sortkey;

	// upload video right before rendering
	if( shader->flags & SHADER_VIDEOMAP )
		R_UploadCinematicShader( shader );

	R_PushMesh( &pic_mesh, MF_TRIFAN | shader->features | (r_shownormals->integer ? MF_NORMALS : 0) );
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
void R_BeginFrame( float cameraSeparation, qboolean forceClear )
{
	glState.cameraSeparation = cameraSeparation;

	if( gl_finish->integer && gl_delayfinish->integer ) {
		// flush any remaining 2D bits
		R_Set2DMode( qfalse );

		// apply software gamma
		R_ApplySoftwareGamma ();

		qglFinish ();

		GLimp_EndFrame ();
	}

	GLimp_BeginFrame ();

	if( r_clear->integer || forceClear ) {
		vec4_t color = { 0, 0, 0, 1 };
	
		if( r_environment_color->string[0] ) {
			int r, g, b;

			if( sscanf( r_environment_color->string, "%i %i %i", &r, &g, &b ) == 3 )
			{
				color[0] = bound( 0, r, 255 ) * (1.0/255.0);
				color[1] = bound( 0, g, 255 ) * (1.0/255.0);
				color[2] = bound( 0, b, 255 ) * (1.0/255.0);
			}
			else
			{
				Cvar_ForceSet( "r_environment_color", "" );
			}
		}

		qglClearColor( color[0], color[1], color[2], color[3] );
		qglClear( GL_COLOR_BUFFER_BIT );
	}

	// update gamma
	if( r_gamma->modified ) {
		r_gamma->modified = qfalse;
		R_UpdateHWGamma ();
	}

	// run cinematic passes on shaders
	R_RunCinematicShaders ();

	// go into 2D mode
	R_Set2DMode( qtrue );

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
}


/*
====================
R_ClearScene
====================
*/
void R_ClearScene( void )
{
	r_numEntities = 1;
	r_numDlights = 0;
	r_numPolys = 0;
	ri.previousentity = NULL;
	ri.currententity = r_worldent;
	ri.currentmodel = r_worldmodel;
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
	// flush any remaining 2D bits
	R_Set2DMode( qfalse );

	if( r_norefresh->integer )
		return;

	R_BackendStartFrame ();

	if( !(fd->rdflags & RDF_NOWORLDMODEL ) )
		r_lastRefdef = *fd;

	c_brush_polys = 0;
	c_world_leafs = 0;

	ri.params = RP_NONE;
	ri.refdef = *fd;
	ri.farClip = 0;
	ri.meshlist = &r_worldlist;
	Vector4Set( ri.scissor, fd->x, glState.height - fd->height - fd->y, fd->width, fd->height );
	VectorCopy( fd->vieworg, ri.pvsOrigin );

	if( gl_finish->integer && !gl_delayfinish->integer )
		qglFinish ();

	R_RenderView( fd );

	R_BloomBlend( fd );

	R_PolyBlend ();

#if SHADOW_VOLUMES
	R_ShadowBlend ();
#endif

	R_BackendEndFrame ();

	R_Set2DMode( qtrue );
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

	// flush any remaining 2D bits
	R_Set2DMode( qfalse );

	// cleanup texture units
	R_BackendCleanUpTextureUnits ();

	// apply software gamma
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
	r_mode = Cvar_Get( "r_mode", VID_DEFAULTMODE, CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_nobind = Cvar_Get( "r_nobind", "0", 0 );
	r_picmip = Cvar_Get( "r_picmip", "0", CVAR_LATCH_VIDEO );
	r_skymip = Cvar_Get( "r_skymip", "0", CVAR_LATCH_VIDEO );
	r_polyblend = Cvar_Get( "r_polyblend", "1", 0 );

	r_bloom = Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE );
	r_bloom_alpha = Cvar_Get( "r_bloom_alpha", "0.3", CVAR_ARCHIVE );
	r_bloom_diamond_size = Cvar_Get( "r_bloom_diamond_size", "8", CVAR_ARCHIVE );
	r_bloom_intensity = Cvar_Get( "r_bloom_intensity", "1.3", CVAR_ARCHIVE );
	r_bloom_darken = Cvar_Get( "r_bloom_darken", "4", CVAR_ARCHIVE );
	r_bloom_sample_size = Cvar_Get( "r_bloom_sample_size", "128", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_bloom_fast_sample = Cvar_Get( "r_bloom_fast_sample", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	r_environment_color = Cvar_Get( "r_environment_color", "0 0 0", CVAR_ARCHIVE );
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
	r_draworder = Cvar_Get( "r_draworder", "0", CVAR_CHEAT );
	r_maxlmblocksize = Cvar_Get( "r_maxLMBlockSize", "512", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_portalonly = Cvar_Get( "r_portalonly", "0", 0 );

	r_allow_software = Cvar_Get( "r_allow_software", "0", 0 );
	r_3dlabs_broken = Cvar_Get( "r_3dlabs_broken", "1", CVAR_ARCHIVE );

	r_lighting_bumpscale = Cvar_Get( "r_lighting_bumpscale", "8", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_lighting_deluxemapping = Cvar_Get( "r_lighting_deluxemapping", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_lighting_diffuse2heightmap = Cvar_Get( "r_lighting_diffuse2heightmap", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_lighting_specular = Cvar_Get( "r_lighting_specular", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_lighting_glossintensity = Cvar_Get( "r_lighting_glossintensity", "2", CVAR_ARCHIVE );
	r_lighting_glossexponent = Cvar_Get( "r_lighting_glossexponent", "32", CVAR_ARCHIVE );
	r_lighting_models_followdeluxe = Cvar_Get( "r_lighting_models_followdeluxe", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	r_lighting_ambientscale = Cvar_Get( "r_lighting_ambientscale", "0.6", 0 );
	r_lighting_directedscale = Cvar_Get( "r_lighting_directedscale", "1", 0 );

	r_shadows = Cvar_Get( "r_shadows", "0", CVAR_ARCHIVE );
	r_shadows_alpha = Cvar_Get( "r_shadows_alpha", "0.4", CVAR_ARCHIVE );
	r_shadows_nudge = Cvar_Get( "r_shadows_nudge", "1", CVAR_ARCHIVE );
	r_shadows_projection_distance = Cvar_Get( "r_shadows_projection_distance", "800", CVAR_ARCHIVE );

	r_lodbias = Cvar_Get( "r_lodbias", "0", CVAR_ARCHIVE );
	r_lodscale = Cvar_Get( "r_lodscale", "5.0", CVAR_ARCHIVE );

	r_gamma = Cvar_Get( "r_gamma", "1.0", CVAR_ARCHIVE );
	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	r_texturebits = Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	r_texturemode = Cvar_Get( "r_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	r_stencilbits = Cvar_Get( "r_stencilbits", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	r_screenshot_jpeg = Cvar_Get( "r_screenshot_jpeg", "1", CVAR_ARCHIVE );
	r_screenshot_jpeg_quality = Cvar_Get( "r_screenshot_jpeg_quality", "85", CVAR_ARCHIVE );

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
	gl_ext_NV_texture_env_combine4 = Cvar_Get( "gl_ext_NV_texture_env_combine4", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_compressed_textures = Cvar_Get( "gl_ext_compressed_textures", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_edge_clamp = Cvar_Get( "gl_ext_texture_edge_clamp", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_filter_anisotropic = Cvar_Get( "gl_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_max_texture_filter_anisotropic = Cvar_Get( "gl_ext_max_texture_filter_anisotropic", "0", CVAR_NOSET );
	gl_ext_draw_range_elements = Cvar_Get( "gl_ext_draw_range_elements", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_vertex_buffer_object = Cvar_Get( "gl_ext_vertex_buffer_object", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture_cube_map = Cvar_Get( "gl_ext_texture_cube_map", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_BGRA = Cvar_Get( "gl_ext_BGRA", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_texture3D = Cvar_Get( "gl_ext_texture3D", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	gl_ext_GLSL = Cvar_Get( "gl_ext_GLSL", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );

	vid_fullscreen = Cvar_Get( "vid_fullscreen", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );

	Cmd_AddCommand( "imagelist", R_ImageList_f );
	Cmd_AddCommand( "shaderlist", R_ShaderList_f );
	Cmd_AddCommand( "screenshot", R_ScreenShot_f );
	Cmd_AddCommand( "envshot", R_EnvShot_f );
	Cmd_AddCommand( "modellist", Mod_Modellist_f );
	Cmd_AddCommand( "gfxinfo", R_GfxInfo_f );
	Cmd_AddCommand( "programlist", R_ProgramList_f );
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

	if( r_mode->integer < -1 ) {
		Com_Printf( "Bad mode %i or custom resolution\n", r_mode->integer );
		Cvar_ForceSet( "r_mode", VID_DEFAULTMODE );
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
	glConfig.NVTextureEnvCombine4 = qfalse;
	glConfig.compressedTextures = qfalse;
	glConfig.textureEdgeClamp = qfalse;
	glConfig.textureFilterAnisotropic = qfalse;
	glConfig.maxTextureFilterAnisotropic = 0.0f;
	glConfig.drawRangeElements = qfalse;
#ifdef VERTEX_BUFFER_OBJECTS
	glConfig.vertexBufferObject = qfalse;
#endif
	glConfig.BGRA = qfalse;
	glConfig.texture3D = qfalse;
	glConfig.GLSL = qfalse;

	glConfig.maxTextureCubemapSize = 0;
	glConfig.max3DTextureSize = 0;
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
#ifdef VERTEX_BUFFER_OBJECTS
	if( gl_ext_vertex_buffer_object->integer ) {
		if( strstr( glConfig.extensionsString, "GL_ARB_vertex_buffer_object" ) ) {
			qglBindBufferARB = ( void * ) qglGetProcAddress( "glBindBufferARB" );
			if( qglBindBufferARB )
				qglDeleteBuffersARB = ( void * ) qglGetProcAddress( "glDeleteBuffersARB" );
			if( qglDeleteBuffersARB )
				qglGenBuffersARB = ( void * ) qglGetProcAddress( "glGenBuffersARB" );
			if( qglGenBuffersARB )
				qglBufferDataARB = ( void * ) qglGetProcAddress( "glBufferDataARB" );

			if( qglBufferDataARB )
				glConfig.vertexBufferObject = qtrue;
			else
				Com_Printf( "R_CheckExtensions: broken GL_ARB_vertex_buffer_object support, contact your video card vendor\n" );
		}
	}
#endif
	if( gl_ext_texture3D->integer ) {
		if( strstr( glConfig.extensionsString, "GL_EXT_texture3D" ) ) {
			qglTexImage3D = ( void * ) qglGetProcAddress( "glTexImage3D" );
			if( qglTexImage3D )
				qglTexSubImage3D = ( void * ) qglGetProcAddress( "glTexSubImage3D" );

			if( qglTexSubImage3D )
				glConfig.texture3D = qtrue;
			else
				Com_Printf( "R_CheckExtensions: broken gl_ext_texture3D support, contact your video card vendor\n" );
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

	if( gl_ext_GLSL->integer && glConfig.multiTexture ) {
		if( strstr( glConfig.extensionsString, "GL_ARB_shader_objects" ) &&
			strstr( glConfig.extensionsString, "GL_ARB_shading_language_100" ) &&
			strstr( glConfig.extensionsString, "GL_ARB_vertex_shader" ) &&
			strstr( glConfig.extensionsString, "GL_ARB_fragment_shader" ) ) {

			qglDeleteObjectARB = ( void * ) qglGetProcAddress( "glDeleteObjectARB" );
			qglGetHandleARB = ( void * ) qglGetProcAddress( "glGetHandleARB" );
			qglDetachObjectARB = ( void * ) qglGetProcAddress( "glDetachObjectARB" );
			qglCreateShaderObjectARB = ( void * ) qglGetProcAddress( "glCreateShaderObjectARB" );
			qglShaderSourceARB = ( void * ) qglGetProcAddress( "glShaderSourceARB" );
			qglCompileShaderARB = ( void * ) qglGetProcAddress( "glCompileShaderARB" );
			qglCreateProgramObjectARB = ( void * ) qglGetProcAddress( "glCreateProgramObjectARB" );
			qglAttachObjectARB = ( void * ) qglGetProcAddress( "glAttachObjectARB" );
			qglLinkProgramARB = ( void * ) qglGetProcAddress( "glLinkProgramARB" );
			qglUseProgramObjectARB = ( void * ) qglGetProcAddress( "glUseProgramObjectARB" );
			qglValidateProgramARB = ( void * ) qglGetProcAddress( "glValidateProgramARB" );
			qglUniform1fARB = ( void * ) qglGetProcAddress( "glUniform1fARB" );
			qglUniform2fARB = ( void * ) qglGetProcAddress( "glUniform2fARB" );
			qglUniform3fARB = ( void * ) qglGetProcAddress( "glUniform3fARB" );
			qglUniform4fARB = ( void * ) qglGetProcAddress( "glUniform4fARB" );
			qglUniform1iARB = ( void * ) qglGetProcAddress( "glUniform1iARB" );
			qglUniform2iARB = ( void * ) qglGetProcAddress( "glUniform2iARB" );
			qglUniform3iARB = ( void * ) qglGetProcAddress( "glUniform3iARB" );
			qglUniform4iARB = ( void * ) qglGetProcAddress( "glUniform4iARB" );
			qglUniform1fvARB = ( void * ) qglGetProcAddress( "glUniform1fvARB" );
			qglUniform2fvARB = ( void * ) qglGetProcAddress( "glUniform2fvARB" );
			qglUniform3fvARB = ( void * ) qglGetProcAddress( "glUniform3fvARB" );
			qglUniform4fvARB = ( void * ) qglGetProcAddress( "glUniform4fvARB" );
			qglUniform1ivARB = ( void * ) qglGetProcAddress( "glUniform1ivARB" );
			qglUniform2ivARB = ( void * ) qglGetProcAddress( "glUniform2ivARB" );
			qglUniform3ivARB = ( void * ) qglGetProcAddress( "glUniform3ivARB" );
			qglUniform4ivARB = ( void * ) qglGetProcAddress( "glUniform4ivARB" );
			qglUniformMatrix2fvARB = ( void * ) qglGetProcAddress( "glUniformMatrix2fvARB" );
			qglUniformMatrix3fvARB = ( void * ) qglGetProcAddress( "glUniformMatrix3fvARB" );
			qglUniformMatrix4fvARB = ( void * ) qglGetProcAddress( "glUniformMatrix4fvARB" );
			qglGetObjectParameterfvARB = ( void * ) qglGetProcAddress( "glGetObjectParameterfvARB" );
			qglGetObjectParameterivARB = ( void * ) qglGetProcAddress( "glGetObjectParameterivARB" );
			qglGetInfoLogARB = ( void * ) qglGetProcAddress( "glGetInfoLogARB" );
			qglGetAttachedObjectsARB = ( void * ) qglGetProcAddress( "glGetAttachedObjectsARB" );
			qglGetUniformLocationARB = ( void * ) qglGetProcAddress( "glGetUniformLocationARB" );
			qglGetActiveUniformARB = ( void * ) qglGetProcAddress( "glGetActiveUniformARB" );
			qglGetUniformfvARB = ( void * ) qglGetProcAddress( "glGetUniformfvARB" );
			qglGetUniformivARB = ( void * ) qglGetProcAddress( "glGetUniformivARB" );
			qglGetShaderSourceARB = ( void * ) qglGetProcAddress( "glGetShaderSourceARB" );

			qglVertexAttribPointerARB = ( void * ) qglGetProcAddress( "glVertexAttribPointerARB" );
			qglEnableVertexAttribArrayARB = ( void * ) qglGetProcAddress( "glEnableVertexAttribArrayARB" );
			qglDisableVertexAttribArrayARB = ( void * ) qglGetProcAddress( "glDisableVertexAttribArrayARB" );
			qglBindAttribLocationARB = ( void * ) qglGetProcAddress( "glBindAttribLocationARB" );
			qglGetActiveAttribARB = ( void * ) qglGetProcAddress( "glGetActiveAttribARB" );
			qglGetAttribLocationARB = ( void * ) qglGetProcAddress( "glGetAttribLocationARB" );

			if(	!qglDeleteObjectARB || !qglGetHandleARB || !qglDetachObjectARB || !qglCreateShaderObjectARB ||
				!qglShaderSourceARB || !qglCompileShaderARB || !qglCreateProgramObjectARB || !qglAttachObjectARB ||
				!qglLinkProgramARB || !qglUseProgramObjectARB || !qglValidateProgramARB || !qglUniform1fARB ||
				!qglUniform2fARB ||	!qglUniform3fARB ||	!qglUniform4fARB ||	!qglUniform1iARB ||	!qglUniform2iARB ||
				!qglUniform3iARB ||	!qglUniform4iARB ||	!qglUniform1fvARB || !qglUniform2fvARB || !qglUniform3fvARB ||
				!qglUniform4fvARB || !qglUniform1ivARB || !qglUniform2ivARB || !qglUniform3ivARB || !qglUniform4ivARB ||
				!qglUniformMatrix2fvARB || !qglUniformMatrix3fvARB || !qglUniformMatrix4fvARB || !qglGetObjectParameterfvARB ||
				!qglGetObjectParameterivARB || !qglGetInfoLogARB || !qglGetAttachedObjectsARB || !qglGetUniformLocationARB ||
				!qglGetActiveUniformARB || !qglGetUniformfvARB || !qglGetUniformivARB || !qglGetShaderSourceARB ||
				!qglVertexAttribPointerARB || !qglEnableVertexAttribArrayARB || !qglDisableVertexAttribArrayARB ||
				!qglBindAttribLocationARB || !qglGetActiveAttribARB || !qglGetAttribLocationARB )
				Com_Printf( "R_CheckExtensions: broken GLSL support, contact your video card vendor\n" );
			else
				glConfig.GLSL = qtrue;
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

	if( gl_ext_BGRA->integer ) {
		if( strstr( glConfig.extensionsString, "GL_EXT_bgra" ) )
			glConfig.BGRA = qtrue;
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

	qglEnable( GL_DEPTH_TEST );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_STENCIL_TEST );
	qglEnable( GL_SCISSOR_TEST );
	qglDepthFunc( GL_LEQUAL );
	qglDepthMask( GL_FALSE );

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
	qglDisable( GL_BLEND );
	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_POLYGON_OFFSET_FILL );
	qglEnable( GL_TEXTURE_2D );

	GL_Cull( 0 );
	GL_SetState( GLSTATE_DEPTHWRITE );
	GL_TexEnv( GL_MODULATE );

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
	if( glConfig.textureCubeMap )
		Com_Printf( "GL_MAX_CUBE_MAP_TEXTURE_SIZE: %i\n", glConfig.maxTextureCubemapSize );
	if( glConfig.texture3D )
		Com_Printf( "GL_MAX_3D_TEXTURE_SIZE: %i\n", glConfig.max3DTextureSize );
	if( glConfig.textureFilterAnisotropic )
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
	Com_Printf( "texture3D %s\n", glConfig.texture3D ? "enabled" : "disabled" );
	Com_Printf( "texenv add: %s\n", glConfig.textureEnvAdd ? "enabled" : "disabled" );
	Com_Printf( "texenv combine: %s\n", glConfig.textureEnvCombine ? "enabled" : "disabled" );
	Com_Printf( "NVtexenv combine4: %s\n", glConfig.NVTextureEnvCombine4 ? "enabled" : "disabled" );
	Com_Printf( "texture edge clamp: %s\n", glConfig.textureEdgeClamp ? "enabled" : "disabled" );
	Com_Printf( "anisotropic filtering: %s\n", glConfig.textureFilterAnisotropic ? "enabled" : "disabled" );
	Com_Printf( "compressed textures: %s\n", glConfig.compressedTextures ? "enabled" : "disabled" );
	Com_Printf( "draw range elements: %s\n", glConfig.drawRangeElements ? "enabled" : "disabled" );
#ifdef VERTEX_BUFFER_OBJECTS
	Com_Printf( "vertex buffer object: %s\n", glConfig.vertexBufferObject ? "enabled" : "disabled" );
#endif
	Com_Printf( "BGRA byte order: %s\n", glConfig.BGRA ? "enabled" : "disabled" );
	Com_Printf( "OpenGL Shading Language: %s\n", glConfig.GLSL ? "enabled" : "disabled" );
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
	char *driver;

	r_firstTime = qtrue;

	Com_Printf( "\n----- R_Init -----\n" );

	Com_Printf( "ref_gl version: "REF_VERSION"\n");

	R_Register ();
	R_SetDefaultState ();

	glConfig.allowCDS = qtrue;

	driver = gl_driver->string;

	// initialize our QGL dynamic bindings
init_qgl:
	if( !QGL_Init( gl_driver->string ) ) {
		QGL_Shutdown ();
		Com_Printf( "ref_gl::R_Init() - could not load \"%s\"\n", gl_driver->string );

		if( strcmp( gl_driver->string, GL_DRIVERNAME ) ) {
			Cvar_ForceSet( gl_driver->name, GL_DRIVERNAME );
			goto init_qgl;
		}

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

	if( glConfig.textureCubeMap )
		qglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.maxTextureCubemapSize );
	if( glConfig.texture3D )
		qglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &glConfig.max3DTextureSize );

	if( !glConfig.multiTexture ) {
		glConfig.maxTextureUnits = 1;
	} else {
		qglGetIntegerv( GL_MAX_TEXTURE_UNITS, &glConfig.maxTextureUnits );
		if( glConfig.maxTextureUnits < 2 )
			Com_Error( ERR_DROP, "R_Init: glConfig.maxTextureUnits = %i, broken driver, contact your video card vendor", glConfig.maxTextureUnits );
		else if( glConfig.maxTextureUnits > MAX_TEXTURE_UNITS )
			glConfig.maxTextureUnits = MAX_TEXTURE_UNITS;
	}

	if( glConfig.textureFilterAnisotropic )
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

	R_InitMeshLists ();

	R_InitLightStyles ();
	R_InitGLSLPrograms ();
	R_InitImages ();
	R_InitShaders( !r_firstTime );
	R_InitModels ();
	R_InitSkinFiles ();

	glState.currentTMU = 0;
	memset( glState.currentTextures, -1, sizeof(glState.currentTextures) );
	memset( glState.currentEnvModes, -1, sizeof(glState.currentEnvModes) );

	memset( r_customColors, 255, sizeof( r_customColors ) );

	memset( &spr_mbuffer, 0, sizeof( meshbuffer_t ) );
	memset( &pic_mbuffer, 0, sizeof( meshbuffer_t ) );

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
	R_ShutdownGLSLPrograms ();

	R_FreeMeshLists ();

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
	Cmd_RemoveCommand( "envshot");
	Cmd_RemoveCommand( "imagelist" );
	Cmd_RemoveCommand( "gfxinfo" );
	Cmd_RemoveCommand( "shaderlist" );
	Cmd_RemoveCommand( "programlist" );

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
