/*
Copyright (C) 1997-2001 Id Software, Inc.
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

// r_main.c

#include "r_local.h"

model_t	*r_worldmodel;
mbrushmodel_t *r_worldbrushmodel;

float gldepthmin, gldepthmax;

mapconfig_t mapConfig;

refinst_t ri, prevRI;
refdef_t r_lastRefdef;

image_t	*r_cintexture;      // cinematic texture
image_t	*r_portaltexture;   // portal view
image_t	*r_portaltexture2;  // refraction image for distortions
image_t	*r_notexture;       // use for bad textures
image_t	*r_particletexture; // little dot for particles
image_t	*r_whitetexture;
image_t	*r_blacktexture;
image_t *r_blankbumptexture;
image_t	*r_dlighttexture;
image_t	*r_fogtexture;
image_t	*r_coronatexture;
image_t	*r_shadowmapTextures[MAX_SHADOWGROUPS];

static int r_numnullentities;
static entity_t	*r_nullentities[MAX_EDICTS];

static int r_numbmodelentities;
static entity_t	*r_bmodelentities[MAX_EDICTS];

static qbyte r_entVisBits[MAX_EDICTS/8];

int r_pvsframecount;    // bumped when going to a new PVS
int r_framecount;       // used for dlight push checking

int c_brush_polys, c_world_leafs;

int r_mark_leaves, r_world_node;
int r_add_polys, r_add_entities;
int r_sort_meshes, r_draw_meshes;

msurface_t *r_debug_surface;

char r_speeds_msg[MAX_RSPEEDSMSGSIZE];

//
// screen size info
//
unsigned int r_numEntities;
entity_t r_entities[MAX_ENTITIES];
entity_t *r_worldent = &r_entities[0];

unsigned int r_numDlights;
dlight_t r_dlights[MAX_DLIGHTS];

unsigned int r_numPolys;
poly_t r_polys[MAX_POLYS];

lightstyle_t r_lightStyles[MAX_LIGHTSTYLES];

int r_viewcluster, r_oldviewcluster;

float r_farclip_min, r_farclip_bias = 64.0f;

/*
=================
GL_Cull
=================
*/
void GL_Cull( int cull )
{
	if( glState.faceCull == cull )
		return;

	if( !cull )
	{
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
		state &= ~( GLSTATE_DEPTHWRITE|GLSTATE_DEPTHFUNC_EQ );

	diff = glState.flags ^ state;
	if( !diff )
		return;

	if( diff & ( GLSTATE_BLEND_MTEX|GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ) )
	{
		if( state & ( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ) )
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
			if( !( glState.flags & GLSTATE_ALPHAFUNC ) )
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
GL_FrontFace
=================
*/
void GL_FrontFace( int front )
{
	qglFrontFace( front ? GL_CW : GL_CCW );
	glState.frontFace = front;
}

/*
=============
R_TransformEntityBBox
=============
*/
void R_TransformEntityBBox( entity_t *e, vec3_t mins, vec3_t maxs, vec3_t bbox[8], qboolean local )
{
	int i;
	vec3_t axis[3], tmp;

	if( e == r_worldent )
		local = qfalse;
	if( local )
		Matrix_Transpose( e->axis, axis );	// switch row-column order

	// rotate local bounding box and compute the full bounding box
	for( i = 0; i < 8; i++ )
	{
		vec_t *corner = bbox[i];

		corner[0] = ( ( i & 1 ) ? mins[0] : maxs[0] );
		corner[1] = ( ( i & 2 ) ? mins[1] : maxs[1] );
		corner[2] = ( ( i & 4 ) ? mins[2] : maxs[2] );

		if( local )
		{
			Matrix_TransformVector( axis, corner, tmp );
			VectorAdd( tmp, e->origin, corner );
		}
	}
}

/*
=============
R_LoadIdentity
=============
*/
void R_LoadIdentity( void )
{
	Matrix4_Identity( ri.objectMatrix );
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
	if( e == r_worldent )
	{
		R_LoadIdentity();
		return;
	}

	if( e->scale != 1.0f )
	{
		ri.objectMatrix[0] = e->axis[0][0] * e->scale;
		ri.objectMatrix[1] = e->axis[0][1] * e->scale;
		ri.objectMatrix[2] = e->axis[0][2] * e->scale;
		ri.objectMatrix[4] = e->axis[1][0] * e->scale;
		ri.objectMatrix[5] = e->axis[1][1] * e->scale;
		ri.objectMatrix[6] = e->axis[1][2] * e->scale;
		ri.objectMatrix[8] = e->axis[2][0] * e->scale;
		ri.objectMatrix[9] = e->axis[2][1] * e->scale;
		ri.objectMatrix[10] = e->axis[2][2] * e->scale;
	}
	else
	{
		ri.objectMatrix[0] = e->axis[0][0];
		ri.objectMatrix[1] = e->axis[0][1];
		ri.objectMatrix[2] = e->axis[0][2];
		ri.objectMatrix[4] = e->axis[1][0];
		ri.objectMatrix[5] = e->axis[1][1];
		ri.objectMatrix[6] = e->axis[1][2];
		ri.objectMatrix[8] = e->axis[2][0];
		ri.objectMatrix[9] = e->axis[2][1];
		ri.objectMatrix[10] = e->axis[2][2];
	}

	ri.objectMatrix[3] = 0;
	ri.objectMatrix[7] = 0;
	ri.objectMatrix[11] = 0;
	ri.objectMatrix[12] = e->origin[0];
	ri.objectMatrix[13] = e->origin[1];
	ri.objectMatrix[14] = e->origin[2];
	ri.objectMatrix[15] = 1.0;

	Matrix4_MultiplyFast( ri.worldviewMatrix, ri.objectMatrix, ri.modelviewMatrix );
	qglLoadMatrixf( ri.modelviewMatrix );
}

/*
=============
R_TranslateForEntity
=============
*/
void R_TranslateForEntity( entity_t *e )
{
	if( e == r_worldent )
	{
		R_LoadIdentity();
		return;
	}

	Matrix4_Identity( ri.objectMatrix );

	ri.objectMatrix[12] = e->origin[0];
	ri.objectMatrix[13] = e->origin[1];
	ri.objectMatrix[14] = e->origin[2];

	Matrix4_MultiplyFast( ri.worldviewMatrix, ri.objectMatrix, ri.modelviewMatrix );
	qglLoadMatrixf( ri.modelviewMatrix );
}

/*
=============
R_LerpTag
=============
*/
qboolean R_LerpTag( orientation_t *orient, const model_t *mod, int oldframe, int frame, float lerpfrac, const char *name )
{
	if( !orient )
		return qfalse;

	VectorClear( orient->origin );
	Matrix_Identity( orient->axis );

	if( !name )
		return qfalse;

	if( mod->type == mod_alias )
		return R_AliasModelLerpTag( orient, mod->extradata, oldframe, frame, lerpfrac, name );

	return qfalse;
}

/*
=============
R_FogForSphere
=============
*/
mfog_t *R_FogForSphere( const vec3_t centre, const float radius )
{
	int i, j;
	mfog_t *fog;
	cplane_t *plane;

	if( !r_worldmodel || ( ri.refdef.rdflags & RDF_NOWORLDMODEL ) || !r_worldbrushmodel->numfogs )
		return NULL;
	if( ri.params & RP_SHADOWMAPVIEW )
		return NULL;
	if( r_worldbrushmodel->globalfog )
		return r_worldbrushmodel->globalfog;

	fog = r_worldbrushmodel->fogs;
	for( i = 0; i < r_worldbrushmodel->numfogs; i++, fog++ )
	{
		if( !fog->shader )
			continue;

		plane = fog->planes;
		for( j = 0; j < fog->numplanes; j++, plane++ )
		{
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
	if( fog && fog->shader && ri.fog_dist_to_eye[fog-r_worldbrushmodel->fogs] < 0 )
	{
		float vpnDist = ( ( ri.viewOrigin[0] - origin[0] ) * ri.vpn[0] + ( ri.viewOrigin[1] - origin[1] ) * ri.vpn[1] + ( ri.viewOrigin[2] - origin[2] ) * ri.vpn[2] );
		return ( ( vpnDist + radius ) / fog->shader->fog_dist ) < -1;
	}

	return qfalse;
}

/*
=============================================================

CUSTOM COLORS

=============================================================
*/

static byte_vec4_t r_customColors[NUM_CUSTOMCOLORS];

/*
=================
R_InitCustomColors
=================
*/
void R_InitCustomColors( void )
{
	memset( r_customColors, 255, sizeof( r_customColors ) );
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
R_GetCustomColor
=================
*/
int R_GetCustomColor( int num )
{
	if( num < 0 || num >= NUM_CUSTOMCOLORS )
		return COLOR_RGBA( 255, 255, 255, 255 );
	return *(int *)r_customColors[num];
}

/*
=============================================================

SPRITE MODELS AND FLARES

=============================================================
*/

static vec4_t spr_xyz[4] = { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1} };
static vec2_t spr_st[4] = { {0, 1}, {0, 0}, {1,0}, {1,1} };
static byte_vec4_t spr_color[4];
static mesh_t spr_mesh = { 4, spr_xyz, spr_xyz, NULL, spr_st, { 0, 0, 0, 0 }, { spr_color, spr_color, spr_color, spr_color }, 6, NULL };

/*
=================
R_PushSprite
=================
*/
static qboolean R_PushSprite( const meshbuffer_t *mb, float rotation, float right, float left, float up, float down )
{
	int i, features;
	vec3_t point;
	vec3_t v_right, v_up;
	entity_t *e = ri.currententity;
	shader_t *shader;

	if( rotation )
	{
		RotatePointAroundVector( v_right, ri.vpn, ri.vright, rotation );
		CrossProduct( ri.vpn, v_right, v_up );
	}
	else
	{
		VectorCopy( ri.vright, v_right );
		VectorCopy( ri.vup, v_up );
	}

	VectorScale( v_up, down, point );
	VectorMA( point, -left, v_right, spr_xyz[0] );
	VectorMA( point, -right, v_right, spr_xyz[3] );

	VectorScale( v_up, up, point );
	VectorMA( point, -left, v_right, spr_xyz[1] );
	VectorMA( point, -right, v_right, spr_xyz[2] );

	if( e->scale != 1.0f )
	{
		for( i = 0; i < 4; i++ )
			VectorScale( spr_xyz[i], e->scale, spr_xyz[i] );
	}

	MB_NUM2SHADER( mb->shaderkey, shader );

	// the code below is disgusting, but some q3a shaders use 'rgbgen vertex'
	// and 'alphagen vertex' for effects instead of 'rgbgen entity' and 'alphagen entity'
	if( shader->features & MF_COLORS )
	{
		for( i = 0; i < 4; i++ )
			Vector4Copy( e->color, spr_color[i] );
	}

	features = MF_NOCULL | MF_TRIFAN | shader->features;
	if( r_shownormals->integer )
		features |= MF_NORMALS;

	if( shader->flags & SHADER_ENTITY_MERGABLE )
	{
		for( i = 0; i < 4; i++ )
			VectorAdd( spr_xyz[i], e->origin, spr_xyz[i] );
		R_PushMesh( &spr_mesh, features );
		return qfalse;
	}

	R_PushMesh( &spr_mesh, MF_NONBATCHED | features );
	return qtrue;
}

/*
=================
R_PushFlareSurf
=================
*/
static void R_PushFlareSurf( const meshbuffer_t *mb )
{
	int i;
	vec4_t color;
	vec3_t origin, point, v;
	float radius = r_flaresize->value, colorscale, depth;
	float up = radius, down = -radius, left = -radius, right = radius;
	mbrushmodel_t *bmodel = ( mbrushmodel_t * )ri.currentmodel->extradata;
	msurface_t *surf = &bmodel->surfaces[mb->infokey - 1];
	shader_t *shader;

	if( ri.currentmodel != r_worldmodel )
	{
		Matrix_TransformVector( ri.currententity->axis, surf->origin, origin );
		VectorAdd( origin, ri.currententity->origin, origin );
	}
	else
	{
		VectorCopy( surf->origin, origin );
	}
	R_TransformToScreen_Vec3( origin, v );

	if( v[0] < ri.refdef.x || v[0] > ri.refdef.x + ri.refdef.width )
		return;
	if( v[1] < ri.refdef.y || v[1] > ri.refdef.y + ri.refdef.height )
		return;

	qglReadPixels( (int)( v[0] /* + 0.5f*/ ), (int)( v[1] /* + 0.5f*/ ), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );
	if( depth + 1e-4 < v[2] )
		return; // occluded

	VectorCopy( origin, origin );

	VectorMA( origin, down, ri.vup, point );
	VectorMA( point, -left, ri.vright, spr_xyz[0] );
	VectorMA( point, -right, ri.vright, spr_xyz[3] );

	VectorMA( origin, up, ri.vup, point );
	VectorMA( point, -left, ri.vright, spr_xyz[1] );
	VectorMA( point, -right, ri.vright, spr_xyz[2] );

	colorscale = 255.0 / r_flarefade->value;
	Vector4Set( color,
		surf->color[0] * colorscale,
		surf->color[1] * colorscale,
		surf->color[2] * colorscale,
		255 );
	for( i = 0; i < 4; i++ )
		clamp( color[i], 0, 255 );

	for( i = 0; i < 4; i++ )
		Vector4Copy( color, spr_color[i] );

	MB_NUM2SHADER( mb->shaderkey, shader );

	R_PushMesh( &spr_mesh, MF_NOCULL | MF_TRIFAN | shader->features );
}

/*
=================
R_PushCorona
=================
*/
static void R_PushCorona( const meshbuffer_t *mb )
{
	int i;
	vec4_t color;
	vec3_t origin, point;
	dlight_t *light = r_dlights + ( -mb->infokey - 1 );
	float radius = light->intensity, colorscale;
	float up = radius, down = -radius, left = -radius, right = radius;
	shader_t *shader;

	VectorCopy( light->origin, origin );

	VectorMA( origin, down, ri.vup, point );
	VectorMA( point, -left, ri.vright, spr_xyz[0] );
	VectorMA( point, -right, ri.vright, spr_xyz[3] );

	VectorMA( origin, up, ri.vup, point );
	VectorMA( point, -left, ri.vright, spr_xyz[1] );
	VectorMA( point, -right, ri.vright, spr_xyz[2] );

	colorscale = 255.0 * bound( 0, r_coronascale->value, 1.0 );
	Vector4Set( color,
		light->color[0] * colorscale,
		light->color[1] * colorscale,
		light->color[2] * colorscale,
		255 );
	for( i = 0; i < 4; i++ )
		clamp( color[i], 0, 255 );

	for( i = 0; i < 4; i++ )
		Vector4Copy( color, spr_color[i] );

	MB_NUM2SHADER( mb->shaderkey, shader );

	R_PushMesh( &spr_mesh, MF_NOCULL | MF_TRIFAN | shader->features );
}

#ifdef QUAKE2_JUNK
/*
=================
R_PushSpriteModel
=================
*/
qboolean R_PushSpriteModel( const meshbuffer_t *mb )
{
	sframe_t *frame;
	smodel_t *psprite;
	entity_t *e = ri.currententity;
	model_t	*model = e->model;

	psprite = ( smodel_t * )model->extradata;
	frame = psprite->frames + e->frame;

	return R_PushSprite( mb, e->rotation, frame->origin_x, frame->origin_x - frame->width, frame->height - frame->origin_y, -frame->origin_y );
}
#endif

/*
=================
R_PushSpritePoly
=================
*/
qboolean R_PushSpritePoly( const meshbuffer_t *mb )
{
	entity_t *e = ri.currententity;

	if( ( mb->sortkey & 3 ) == MB_CORONA )
	{
		R_PushCorona( mb );
		return qfalse;
	}
	if( mb->infokey > 0 )
	{
		R_PushFlareSurf( mb );
		return qfalse;
	}

	return R_PushSprite( mb, e->rotation, -e->radius, e->radius, e->radius, -e->radius );
}

#ifdef QUAKE2_JUNK
/*
=================
R_AddSpriteModelToList
=================
*/
static void R_AddSpriteModelToList( entity_t *e )
{
	sframe_t *frame;
	smodel_t *psprite;
	model_t	*model = e->model;
	float dist;
	meshbuffer_t *mb;

	if( !( psprite = ( ( smodel_t * )model->extradata ) ) )
		return;

	dist =
		( e->origin[0] - ri.refdef.vieworg[0] ) * ri.vpn[0] +
		( e->origin[1] - ri.refdef.vieworg[1] ) * ri.vpn[1] +
		( e->origin[2] - ri.refdef.vieworg[2] ) * ri.vpn[2];
	if( dist < 0 )
		return; // cull it because we don't want to sort unneeded things

	e->frame %= psprite->numframes;
	frame = psprite->frames + e->frame;

	if( ri.refdef.rdflags & ( RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW ) || ( ri.params & RP_SKYPORTALVIEW ) )
	{
		if( R_VisCullSphere( e->origin, frame->radius ) )
			return;
	}

	// select skin
	if( e->customShader )
		mb = R_AddMeshToList( MB_MODEL, R_FogForSphere( e->origin, frame->radius ), e->customShader, -1 );
	else
		mb = R_AddMeshToList( MB_MODEL, R_FogForSphere( e->origin, frame->radius ), frame->shader, -1 );
	if( mb )
		mb->shaderkey |= ( bound( 1, 0x4000 - (unsigned int)dist, 0x4000 - 1 ) << 12 );
}
#endif

/*
=================
R_AddSpritePolyToList
=================
*/
static void R_AddSpritePolyToList( entity_t *e )
{
	float dist;
	meshbuffer_t *mb;

	dist =
		( e->origin[0] - ri.refdef.vieworg[0] ) * ri.vpn[0] +
		( e->origin[1] - ri.refdef.vieworg[1] ) * ri.vpn[1] +
		( e->origin[2] - ri.refdef.vieworg[2] ) * ri.vpn[2];
	if( dist < 0 )
		return; // cull it because we don't want to sort unneeded things
	if( ri.refdef.rdflags & ( RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW ) || ( ri.params & RP_SKYPORTALVIEW ) )
	{
		if( R_VisCullSphere( e->origin, e->radius ) )
			return;
	}

	mb = R_AddMeshToList( MB_SPRITE, R_FogForSphere( e->origin, e->radius ), e->customShader, -1 );
	if( mb )
		mb->shaderkey |= ( bound( 1, 0x4000 - (unsigned int)dist, 0x4000 - 1 ) << 12 );
}

/*
=================
R_SpriteOverflow
=================
*/
qboolean R_SpriteOverflow( void )
{
	return R_MeshOverflow( &spr_mesh );
}

//==================================================================================

static vec4_t pic_xyz[4] = { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1} };
static vec2_t pic_st[4];
static byte_vec4_t pic_colors[4];
static mesh_t pic_mesh = { 4, pic_xyz, pic_xyz, NULL, pic_st, { 0, 0, 0, 0 }, { pic_colors, pic_colors, pic_colors, pic_colors }, 6, NULL };
static meshbuffer_t pic_mbuffer;

/*
===============
R_Set2DMode
===============
*/
void R_Set2DMode( qboolean enable )
{
	if( enable )
	{
		if( glState.in2DMode )
			return;

		// set 2D virtual screen size
		qglScissor( 0, 0, glState.width, glState.height );
		qglViewport( 0, 0, glState.width, glState.height );
		qglMatrixMode( GL_PROJECTION );
		qglLoadIdentity();
		qglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );
		qglMatrixMode( GL_MODELVIEW );
		qglLoadIdentity();

		GL_Cull( 0 );
		GL_SetState( GLSTATE_NO_DEPTH_TEST );

		qglColor4f( 1, 1, 1, 1 );

		glState.in2DMode = qtrue;
		ri.currententity = ri.previousentity = NULL;
		ri.currentmodel = NULL;

		pic_mbuffer.infokey = -1;
		pic_mbuffer.shaderkey = 0;
	}
	else
	{
		if( pic_mbuffer.infokey != -1 )
		{
			R_RenderMeshBuffer( &pic_mbuffer );
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
void R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const shader_t *shader )
{
	int bcolor;

	if( !shader )
		return;

	// lower-left
	Vector2Set( pic_xyz[0], x, y );
	Vector2Set( pic_st[0], s1, t1 );
	Vector4Set( pic_colors[0], R_FloatToByte( color[0] ), R_FloatToByte( color[1] ), 
		R_FloatToByte( color[2] ), R_FloatToByte( color[3] ) );
	bcolor = *(int *)pic_colors[0];

	// lower-right
	Vector2Set( pic_xyz[1], x+w, y );
	Vector2Set( pic_st[1], s2, t1 );
	*(int *)pic_colors[1] = bcolor;

	// upper-right
	Vector2Set( pic_xyz[2], x+w, y+h );
	Vector2Set( pic_st[2], s2, t2 );
	*(int *)pic_colors[2] = bcolor;

	// upper-left
	Vector2Set( pic_xyz[3], x, y+h );
	Vector2Set( pic_st[3], s1, t2 );
	*(int *)pic_colors[3] = bcolor;

	if( pic_mbuffer.shaderkey != (int)shader->sortkey || -pic_mbuffer.infokey-1+4 > MAX_ARRAY_VERTS )
	{
		if( pic_mbuffer.shaderkey )
		{
			pic_mbuffer.infokey = -1;
			R_RenderMeshBuffer( &pic_mbuffer );
		}
	}

	pic_mbuffer.infokey -= 4;
	pic_mbuffer.shaderkey = shader->sortkey;

	// upload video right before rendering
	if( shader->flags & SHADER_VIDEOMAP )
		R_UploadCinematicShader( shader );

	R_PushMesh( &pic_mesh, MF_TRIFAN | shader->features | ( r_shownormals->integer ? MF_NORMALS : 0 ) );
}

/*
=============
R_DrawStretchRaw
=============
*/
void R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, int frame, qbyte *data )
{
	int samples = 3;

	GL_Bind( 0, r_cintexture );

	R_Upload32( &data, cols, rows, IT_CINEMATIC, NULL, NULL, &samples, ( cols == r_cintexture->width && rows == r_cintexture->height ) );

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
	qglEnd();
}

/*
============
R_PolyBlend
============
*/
static void R_PolyBlend( void )
{
	if( !r_polyblend->integer )
		return;
	if( ri.refdef.blend[3] < 0.01f )
		return;

	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity();
	qglOrtho( 0, 1, 1, 0, -99999, 99999 );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	GL_Cull( 0 );
	GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	qglDisable( GL_TEXTURE_2D );

	qglColor4fv( ri.refdef.blend );

	qglBegin( GL_TRIANGLES );
	qglVertex2f( -5, -5 );
	qglVertex2f( 10, -5 );
	qglVertex2f( -5, 10 );
	qglEnd();

	qglEnable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );
}

/*
===============
R_ApplySoftwareGamma
===============
*/
static void R_ApplySoftwareGamma( void )
{
	double f, div;

	// apply software gamma
	if( !r_ignorehwgamma->integer )
		return;

	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity();
	qglOrtho( 0, 1, 1, 0, -99999, 99999 );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

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

	while( f >= 1.01f )
	{
		if( f >= 2 )
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		else
			qglColor4f( f - 1.0f, f - 1.0f, f - 1.0f, 1.0f );

		qglVertex2f( -5, -5 );
		qglVertex2f( 10, -5 );
		qglVertex2f( -5, 10 );
		f *= 0.5;
	}

	qglEnd();

	qglEnable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );
}

//=============================================================================

#ifdef HARDWARE_OUTLINES

static shader_t *r_outlineShader;

/*
===============
R_InitOutlines
===============
*/
void R_InitOutlines( void )
{
	r_outlineShader = R_LoadShader( "celloutline/default", SHADER_OUTLINE, qfalse, 0, SHADER_INVALID );
}

/*
===============
R_AddModelMeshOutline
===============
*/
void R_AddModelMeshOutline( unsigned int modhandle, mfog_t *fog, int meshnum )
{
	meshbuffer_t *mb = R_AddMeshToList( MB_MODEL, fog, r_outlineShader, -( meshnum+1 ) );
	if( mb )
		mb->LODModelHandle = modhandle;
}
#endif

//=======================================================================

/*
===============
R_SetupFrustum
===============
*/
static void R_SetupFrustum( void )
{
	int i;
	vec3_t farPoint;

	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up
	// 4 - farclip

	// rotate ri.vpn right by FOV_X/2 degrees
	RotatePointAroundVector( ri.frustum[0].normal, ri.vup, ri.vpn, -( 90-ri.refdef.fov_x / 2 ) );
	// rotate ri.vpn left by FOV_X/2 degrees
	RotatePointAroundVector( ri.frustum[1].normal, ri.vup, ri.vpn, 90-ri.refdef.fov_x / 2 );
	// rotate ri.vpn up by FOV_X/2 degrees
	RotatePointAroundVector( ri.frustum[2].normal, ri.vright, ri.vpn, 90-ri.refdef.fov_y / 2 );
	// rotate ri.vpn down by FOV_X/2 degrees
	RotatePointAroundVector( ri.frustum[3].normal, ri.vright, ri.vpn, -( 90 - ri.refdef.fov_y / 2 ) );
	// negate forward vector
	VectorNegate( ri.vpn, ri.frustum[4].normal );

	for( i = 0; i < 4; i++ )
	{
		ri.frustum[i].type = PLANE_NONAXIAL;
		ri.frustum[i].dist = DotProduct( ri.viewOrigin, ri.frustum[i].normal );
		ri.frustum[i].signbits = SignbitsForPlane( &ri.frustum[i] );
	}

	VectorMA( ri.viewOrigin, ri.farClip, ri.vpn, farPoint );
	ri.frustum[i].type = PLANE_NONAXIAL;
	ri.frustum[i].dist = DotProduct( farPoint, ri.frustum[i].normal );
	ri.frustum[i].signbits = SignbitsForPlane( &ri.frustum[i] );
}

/*
===============
R_FarClip
===============
*/
static float R_FarClip( void )
{
	float farclip_dist;

	if( r_worldmodel && !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		int i;
		float dist;
		vec3_t tmp;

		farclip_dist = 0;
		for( i = 0; i < 8; i++ )
		{
			tmp[0] = ( ( i & 1 ) ? ri.visMins[0] : ri.visMaxs[0] );
			tmp[1] = ( ( i & 2 ) ? ri.visMins[1] : ri.visMaxs[1] );
			tmp[2] = ( ( i & 4 ) ? ri.visMins[2] : ri.visMaxs[2] );

			dist = DistanceSquared( tmp, ri.viewOrigin );
			farclip_dist = max( farclip_dist, dist );
		}

		farclip_dist = sqrt( farclip_dist );

		if( r_worldbrushmodel->globalfog )
		{
			float fogdist = r_worldbrushmodel->globalfog->shader->fog_dist;
			if( farclip_dist > fogdist )
				farclip_dist = fogdist;
			else
				ri.clipFlags &= ~16;
		}
	}
	else
	{
		farclip_dist = 2048;
	}

	return max( r_farclip_min, farclip_dist ) + r_farclip_bias;
}

/*
=============
R_SetupProjectionMatrix
=============
*/
static void R_SetupProjectionMatrix( const refdef_t *rd, mat4x4_t m )
{
	GLdouble xMin, xMax, yMin, yMax, zNear, zFar;

	if( rd->rdflags & RDF_NOWORLDMODEL )
		ri.farClip = 2048;
	else
		ri.farClip = R_FarClip();

	zNear = Z_NEAR;
	zFar = ri.farClip;

	yMax = zNear *tan( rd->fov_y *M_PI / 360.0 );
	yMin = -yMax;

	xMax = zNear *tan( rd->fov_x *M_PI / 360.0 );
	xMin = -xMax;

	xMin += -( 2 * glState.cameraSeparation ) / zNear;
	xMax += -( 2 * glState.cameraSeparation ) / zNear;

	m[0] = ( 2.0 * zNear ) / ( xMax - xMin );
	m[1] = 0.0f;
	m[2] = 0.0f;
	m[3] = 0.0f;
	m[4] = 0.0f;
	m[5] = ( 2.0 * zNear ) / ( yMax - yMin );
	m[6] = 0.0f;
	m[7] = 0.0f;
	m[8] = ( xMax + xMin ) / ( xMax - xMin );
	m[9] = ( yMax + yMin ) / ( yMax - yMin );
	m[10] = -( zFar + zNear ) / ( zFar - zNear );
	m[11] = -1.0f;
	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = -( 2.0 * zFar * zNear ) / ( zFar - zNear );
	m[15] = 0.0f;
}

/*
=============
R_SetupModelviewMatrix
=============
*/
static void R_SetupModelviewMatrix( const refdef_t *rd, mat4x4_t m )
{
#if 0
	Matrix4_Identity( m );
	Matrix4_Rotate( m, -90, 1, 0, 0 );
	Matrix4_Rotate( m, 90, 0, 0, 1 );
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
static void R_SetupFrame( void )
{
	mleaf_t *leaf;

	// build the transformation matrix for the given view angles
	VectorCopy( ri.refdef.vieworg, ri.viewOrigin );
	AngleVectors( ri.refdef.viewangles, ri.viewAxis[0], ri.viewAxis[1], ri.viewAxis[2] );
	ri.vpn = ri.viewAxis[0];
	ri.vright = ri.viewAxis[1];
	ri.vup = ri.viewAxis[2];

	if( ri.params & RP_SHADOWMAPVIEW )
		return;

	r_framecount++;

	ri.lod_dist_scale_for_fov = tan( ri.refdef.fov_x * ( M_PI/180 ) * 0.5f );

	// current viewcluster
	if( !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		VectorCopy( r_worldmodel->mins, ri.visMins );
		VectorCopy( r_worldmodel->maxs, ri.visMaxs );

		if( !( ri.params & RP_OLDVIEWCLUSTER ) )
		{
			r_oldviewcluster = r_viewcluster;
			leaf = Mod_PointInLeaf( ri.pvsOrigin, r_worldmodel );
			r_viewcluster = leaf->cluster;
		}
	}
}

/*
===============
R_SetupViewMatrices
===============
*/
static void R_SetupViewMatrices( void )
{
	R_SetupModelviewMatrix( &ri.refdef, ri.worldviewMatrix );
	if( ri.params & RP_SHADOWMAPVIEW )
	{
		int i;
		float x1, x2, y1, y2;
		int ix1, ix2, iy1, iy2;
		int sizex = ri.refdef.width, sizey = ri.refdef.height;
		int diffx, diffy;
		shadowGroup_t *group = ri.shadowGroup;

		R_SetupProjectionMatrix( &ri.refdef, ri.projectionMatrix );
		Matrix4_Multiply( ri.projectionMatrix, ri.worldviewMatrix, ri.worldviewProjectionMatrix );

		// compute optimal fov to increase depth precision (so that shadow group objects are
		// as close to the nearplane as possible)
		// note that it's suboptimal to use bbox calculated in worldspace (FIXME)
		x1 = y1 = 999999;
		x2 = y2 = -999999;
		for( i = 0; i < 8; i++ )
		{                   // compute and rotate a full bounding box
			vec3_t v, tmp;

			tmp[0] = ( ( i & 1 ) ? group->mins[0] : group->maxs[0] );
			tmp[1] = ( ( i & 2 ) ? group->mins[1] : group->maxs[1] );
			tmp[2] = ( ( i & 4 ) ? group->mins[2] : group->maxs[2] );

			// transform to screen
			R_TransformToScreen_Vec3( tmp, v );
			x1 = min( x1, v[0] ); y1 = min( y1, v[1] );
			x2 = max( x2, v[0] ); y2 = max( y2, v[1] );
		}

		// give it 1 pixel gap on both sides
		ix1 = x1 - 1.0f; ix2 = x2 + 1.0f;
		iy1 = y1 - 1.0f; iy2 = y2 + 1.0f;

		diffx = sizex - min( ix1, sizex - ix2 ) * 2;
		diffy = sizey - min( iy1, sizey - iy2 ) * 2;

		// adjust fov
		ri.refdef.fov_x = 2 * RAD2DEG( atan( (float)diffx / (float)sizex ) );
		ri.refdef.fov_y = 2 * RAD2DEG( atan( (float)diffy / (float)sizey ) );
	}
	R_SetupProjectionMatrix( &ri.refdef, ri.projectionMatrix );
	if( ri.params & RP_MIRRORVIEW )
		ri.projectionMatrix[0] = -ri.projectionMatrix[0];
	Matrix4_Multiply( ri.projectionMatrix, ri.worldviewMatrix, ri.worldviewProjectionMatrix );
}

/*
=============
R_Clear
=============
*/
static void R_Clear( int bitMask )
{
	int bits;

	bits = GL_DEPTH_BUFFER_BIT;

	if( !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) && r_fastsky->integer )
		bits |= GL_COLOR_BUFFER_BIT;
	if( glState.stencilEnabled && ( r_shadows->integer >= SHADOW_PLANAR ) )
		bits |= GL_STENCIL_BUFFER_BIT;

	bits &= bitMask;

	if( bits & GL_STENCIL_BUFFER_BIT )
		qglClearStencil( 128 );

	if( bits & GL_COLOR_BUFFER_BIT )
	{
		qbyte *color = r_worldmodel && !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) && r_worldbrushmodel->globalfog ?
			r_worldbrushmodel->globalfog->shader->fog_color : mapConfig.environmentColor;
		qglClearColor( (float)color[0]*( 1.0/255.0 ), (float)color[1]*( 1.0/255.0 ), (float)color[2]*( 1.0/255.0 ), 1 );
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
static void R_SetupGL( void )
{
	qglScissor( ri.scissor[0], ri.scissor[1], ri.scissor[2], ri.scissor[3] );
	qglViewport( ri.viewport[0], ri.viewport[1], ri.viewport[2], ri.viewport[3] );

	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( ri.projectionMatrix );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadMatrixf( ri.worldviewMatrix );

	if( ri.params & RP_CLIPPLANE )
	{
		GLdouble clip[4];
		cplane_t *p = &ri.clipPlane;

		clip[0] = p->normal[0];
		clip[1] = p->normal[1];
		clip[2] = p->normal[2];
		clip[3] = -p->dist;

		qglClipPlane( GL_CLIP_PLANE0, clip );
		qglEnable( GL_CLIP_PLANE0 );
	}

	if( ri.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !glState.frontFace );

	if( ri.params & RP_SHADOWMAPVIEW )
	{
		qglShadeModel( GL_FLAT );
		qglColorMask( 0, 0, 0, 0 );
		qglPolygonOffset( 1, 4 );
		if( prevRI.params & RP_CLIPPLANE )
			qglDisable( GL_CLIP_PLANE0 );
	}

	GL_Cull( GL_FRONT );
	GL_SetState( GLSTATE_DEPTHWRITE );
}

/*
=============
R_EndGL
=============
*/
static void R_EndGL( void )
{
	if( ri.params & RP_SHADOWMAPVIEW )
	{
		qglPolygonOffset( -1, -2 );
		qglColorMask( 1, 1, 1, 1 );
		qglShadeModel( GL_SMOOTH );
		if( prevRI.params & RP_CLIPPLANE )
			qglEnable( GL_CLIP_PLANE0 );
	}

	if( ri.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !glState.frontFace );

	if( ri.params & RP_CLIPPLANE )
		qglDisable( GL_CLIP_PLANE0 );
}


/*
=============
R_CategorizeEntities
=============
*/
static void R_CategorizeEntities( void )
{
	unsigned int i;

	r_numnullentities = 0;
	r_numbmodelentities = 0;

	if( !r_drawentities->integer )
		return;

	for( i = 1; i < r_numEntities; i++ )
	{
		ri.previousentity = ri.currententity;
		ri.currententity = &r_entities[i];

		if( ri.currententity->rtype != RT_MODEL )
			continue;

		ri.currentmodel = ri.currententity->model;
		if( !ri.currentmodel )
		{
			r_nullentities[r_numnullentities++] = ri.currententity;
			continue;
		}

		switch( ri.currentmodel->type )
		{
		case mod_brush:
			r_bmodelentities[r_numbmodelentities++] = ri.currententity;
			break;
		case mod_alias:
		case mod_skeletal:
			if( !( ri.currententity->renderfx & ( RF_NOSHADOW|RF_PLANARSHADOW ) ) )
				R_AddShadowCaster( ri.currententity ); // build groups and mark shadow casters
			break;
#ifdef QUAKE2_JUNK
		case mod_sprite:
			break;
#endif
		default:
			Com_Error( ERR_DROP, "%s: bad modeltype", ri.currentmodel->name );
			break;
		}
	}
}

/*
=============
R_CullEntities
=============
*/
static void R_CullEntities( void )
{
	unsigned int i;
	entity_t *e;
	qboolean culled;

	memset( r_entVisBits, 0, sizeof( r_entVisBits ) );
	if( !r_drawentities->integer )
		return;

	for( i = 1; i < r_numEntities; i++ )
	{
		ri.previousentity = ri.currententity;
		ri.currententity = e = &r_entities[i];
		culled = qtrue;

		switch( e->rtype )
		{
		case RT_MODEL:
			if( !e->model )
				break;
			switch( e->model->type )
			{
			case mod_alias:
				culled = R_CullAliasModel( e );
				break;
			case mod_skeletal:
				culled = R_CullSkeletalModel( e );
				break;
			case mod_brush:
				culled = R_CullBrushModel( e );
				break;
#ifdef QUAKE2_JUNK
			case mod_sprite:
				culled = qfalse;
				break;
#endif
			default:
				break;
			}
			break;
		case RT_SPRITE:
			culled = ( e->radius <= 0 ) || ( e->customShader == NULL );
			break;
		default:
			break;
		}

		if( !culled )
			r_entVisBits[i>>3] |= ( 1<<( i&7 ) );
	}
}

/*
=============
R_DrawNullModel
=============
*/
static void R_DrawNullModel( void )
{
	qglBegin( GL_LINES );

	qglColor4f( 1, 0, 0, 0.5 );
	qglVertex3fv( ri.currententity->origin );
	qglVertex3f( ri.currententity->origin[0] + ri.currententity->axis[0][0] * 15,
		ri.currententity->origin[1] + ri.currententity->axis[0][1] * 15,
		ri.currententity->origin[2] + ri.currententity->axis[0][2] * 15 );

	qglColor4f( 0, 1, 0, 0.5 );
	qglVertex3fv( ri.currententity->origin );
	qglVertex3f( ri.currententity->origin[0] - ri.currententity->axis[1][0] * 15,
		ri.currententity->origin[1] - ri.currententity->axis[1][1] * 15,
		ri.currententity->origin[2] - ri.currententity->axis[1][2] * 15 );

	qglColor4f( 0, 0, 1, 0.5 );
	qglVertex3fv( ri.currententity->origin );
	qglVertex3f( ri.currententity->origin[0] + ri.currententity->axis[2][0] * 15,
		ri.currententity->origin[1] + ri.currententity->axis[2][1] * 15,
		ri.currententity->origin[2] + ri.currententity->axis[2][2] * 15 );

	qglEnd();
}

/*
=============
R_DrawBmodelEntities
=============
*/
static void R_DrawBmodelEntities( void )
{
	int i, j;

	for( i = 0; i < r_numbmodelentities; i++ )
	{
		ri.previousentity = ri.currententity;
		ri.currententity = r_bmodelentities[i];
		j = ri.currententity - r_entities;
		if( r_entVisBits[j>>3] & ( 1<<( j&7 ) ) )
			R_AddBrushModelToList( ri.currententity );
	}
}

/*
=============
R_DrawRegularEntities
=============
*/
static void R_DrawRegularEntities( void )
{
	unsigned int i;
	entity_t *e;
	qboolean shadowmap = ( ( ri.params & RP_SHADOWMAPVIEW ) != 0 );

	for( i = 1; i < r_numEntities; i++ )
	{
		ri.previousentity = ri.currententity;
		ri.currententity = e = &r_entities[i];

		if( shadowmap )
		{
			if( e->flags & RF_NOSHADOW )
				continue;
			if( r_entShadowBits[i] & ri.shadowGroup->bit )
				goto add; // shadow caster
		}
		if( !( r_entVisBits[i>>3] & ( 1<<( i&7 ) ) ) )
			continue;

add:
		switch( e->rtype )
		{
		case RT_MODEL:
			ri.currentmodel = e->model;
			switch( ri.currentmodel->type )
			{
			case mod_alias:
				R_AddAliasModelToList( e );
				break;
			case mod_skeletal:
				R_AddSkeletalModelToList( e );
				break;
#ifdef QUAKE2_JUNK
			case mod_sprite:
				if( !shadowmap )
					R_AddSpriteModelToList( e );
				break;
#endif
			default:
				break;
			}
			break;
		case RT_SPRITE:
			if( !shadowmap )
				R_AddSpritePolyToList( e );
			break;
		default:
			break;
		}
	}
}

/*
=============
R_DrawNullEntities
=============
*/
static void R_DrawNullEntities( void )
{
	int i;

	if( !r_numnullentities )
		return;

	qglDisable( GL_TEXTURE_2D );
	GL_SetState( GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	// draw non-transparent first
	for( i = 0; i < r_numnullentities; i++ )
	{
		ri.previousentity = ri.currententity;
		ri.currententity = r_nullentities[i];

		if( ri.params & RP_MIRRORVIEW )
		{
			if( ri.currententity->flags & RF_WEAPONMODEL )
				continue;
		}
		else
		{
			if( ri.currententity->flags & RF_VIEWERMODEL )
				continue;
		}
		R_DrawNullModel();
	}

	qglEnable( GL_TEXTURE_2D );
}

/*
=============
R_DrawEntities
=============
*/
static void R_DrawEntities( void )
{
	qboolean shadowmap = ( ( ri.params & RP_SHADOWMAPVIEW ) != 0 );

	if( !r_drawentities->integer )
		return;

	if( !shadowmap )
	{
		R_CullEntities(); // mark visible entities in r_entVisBits

		R_CullShadowmapGroups();
	}

	// we don't mark bmodel entities in RP_SHADOWMAPVIEW, only individual surfaces
	R_DrawBmodelEntities();

	if( OCCLUSION_QUERIES_ENABLED( ri ) )
	{
		R_EndOcclusionPass();
	}

	if( ri.params & RP_ENVVIEW )
		return;

	if( !shadowmap )
		R_DrawShadowmaps(); // render to depth textures, mark shadowed entities and surfaces
	else if( !( ri.params & RP_WORLDSURFVISIBLE ) || ( prevRI.shadowBits & ri.shadowGroup->bit ) )
		return; // we're supposed to cast shadows but there are no visible surfaces for this light, so stop
	// or we've already drawn and captured textures for this group

	R_DrawRegularEntities();
}


/*
===============
R_RenderDebugSurface
===============
*/
void R_RenderDebugSurface( void )
{
	trace_t tr;
	vec3_t forward;
	vec3_t start, end;

	if( ri.params & RP_NONVIEWERREF || ri.refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	r_debug_surface = NULL;
	if( r_speeds->integer != 4 )
		return;

	VectorCopy( ri.vpn, forward );
	VectorCopy( ri.viewOrigin, start );
	VectorMA( start, 4096, forward, end );

	r_debug_surface = R_TraceLine( &tr, start, end, 0 );
	if( r_debug_surface && r_debug_surface->mesh && !r_showtris->integer )
	{
		ri.previousentity = NULL;
		ri.currententity = &r_entities[tr.ent];

		R_ClearMeshList( ri.meshlist );
		R_AddMeshToList( MB_MODEL, NULL, r_debug_surface->shader, r_debug_surface - r_worldbrushmodel->surfaces + 1 );
		R_DrawTriangleOutlines( qtrue, qfalse );
	}
}

/*
================
R_RenderView

ri.refdef must be set before the first call
================
*/
void R_RenderView( const refdef_t *fd )
{
	int msec = 0;
	qboolean shadowMap = ri.params & RP_SHADOWMAPVIEW ? qtrue : qfalse;

	ri.refdef = *fd;

	R_ClearMeshList( ri.meshlist );

	if( !r_worldmodel && !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) )
		Com_Error( ERR_DROP, "R_RenderView: NULL worldmodel" );

	R_SetupFrame();

	// we know the farclip so adjust fov before setting up the frustum
	if( shadowMap )
	{
		R_SetupViewMatrices();
	}
	else if( OCCLUSION_QUERIES_ENABLED( ri ) )
	{
		R_SetupViewMatrices();

		R_SetupGL();

		R_Clear( ~( GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT ) );

		R_BeginOcclusionPass();
	}

	R_SetupFrustum();

	if( r_speeds->integer )
		msec = Sys_Milliseconds();
	R_MarkLeaves();
	if( r_speeds->integer )
		r_mark_leaves += ( Sys_Milliseconds() - msec );

	R_DrawWorld();

	// we know the the farclip at this point after determining visible world leafs
	if( !shadowMap )
	{
		R_SetupViewMatrices();

		R_DrawCoronas();

		if( r_speeds->integer )
			msec = Sys_Milliseconds();
		R_AddPolysToList();
		if( r_speeds->integer )
			r_add_polys += ( Sys_Milliseconds() - msec );
	}

	if( r_speeds->integer )
		msec = Sys_Milliseconds();
	R_DrawEntities();
	if( r_speeds->integer )
		r_add_entities += ( Sys_Milliseconds() - msec );

	if( shadowMap )
	{
		if( !( ri.params & RP_WORLDSURFVISIBLE ) )
			return; // we didn't cast shadows on anything, so stop
		if( prevRI.shadowBits & ri.shadowGroup->bit )
			return; // already drawn
	}

	if( r_speeds->integer )
		msec = Sys_Milliseconds();
	R_SortMeshes();
	if( r_speeds->integer )
		r_sort_meshes += ( Sys_Milliseconds() - msec );

	R_DrawPortals();

	if( r_portalonly->integer && !( ri.params & ( RP_MIRRORVIEW|RP_PORTALVIEW ) ) )
		return;

	R_SetupGL();

	R_Clear( shadowMap ? ~( GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT ) : ~0 );

	if( r_speeds->integer )
		msec = Sys_Milliseconds();
	R_DrawMeshes();
	if( r_speeds->integer )
		r_draw_meshes += ( Sys_Milliseconds() - msec );

	R_BackendCleanUpTextureUnits();

	R_DrawTriangleOutlines( r_showtris->integer ? qtrue : qfalse, r_shownormals->integer ? qtrue : qfalse );

	R_RenderDebugSurface ();

	R_DrawNullEntities();

	R_EndGL();
}

//=======================================================================

/*
===============
R_UpdateSwapInterval
===============
*/
static void R_UpdateSwapInterval( void )
{
	if( r_swapinterval->modified )
	{
		r_swapinterval->modified = qfalse;

		if( !glState.stereoEnabled )
		{
			if( qglSwapInterval )
				qglSwapInterval( r_swapinterval->integer );
		}
	}
}

/*
===============
R_UpdateHWGamma
===============
*/
static void R_UpdateHWGamma( void )
{
	int i, v;
	double invGamma, div;
	unsigned short gammaRamp[3*256];

	if( !glState.hwGamma )
		return;

	invGamma = 1.0 / bound( 0.5, r_gamma->value, 3 );
	div = (double)( 1 << max( 0, r_overbrightbits->integer ) ) / 255.5;

	for( i = 0; i < 256; i++ )
	{
		v = ( int )( 65535.0 * pow( ( (double)i + 0.5 ) * div, invGamma ) + 0.5 );
		gammaRamp[i] = gammaRamp[i + 256] = gammaRamp[i + 512] = ( ( unsigned short )bound( 0, v, 65535 ) );
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

	if( gl_finish->integer && gl_delayfinish->integer )
	{
		// flush any remaining 2D bits
//		R_Set2DMode( qfalse );

		// apply software gamma
//		R_ApplySoftwareGamma ();

		qglFinish();

		GLimp_EndFrame();
	}

	GLimp_BeginFrame();

	if( r_environment_color->modified )
	{
		VectorClear( mapConfig.environmentColor );
		mapConfig.environmentColor[3] = 255;

		if( r_environment_color->string[0] )
		{
			int r, g, b;

			if( sscanf( r_environment_color->string, "%i %i %i", &r, &g, &b ) == 3 )
			{
				mapConfig.environmentColor[0] = bound( 0, r, 255 );
				mapConfig.environmentColor[1] = bound( 0, g, 255 );
				mapConfig.environmentColor[2] = bound( 0, b, 255 );
			}
			else
			{
				Cvar_ForceSet( "r_environment_color", "" );
			}
		}

		r_environment_color->modified = qfalse;
	}

	if( r_clear->integer || forceClear )
	{
		byte_vec4_t color;
		
		Vector4Copy( mapConfig.environmentColor, color );
		qglClearColor( color[0]*( 1.0/255.0 ), color[1]*( 1.0/255.0 ), color[2]*( 1.0/255.0 ), 1 );
		qglClear( GL_COLOR_BUFFER_BIT );
	}

	// update gamma
	if( r_gamma->modified )
	{
		r_gamma->modified = qfalse;
		R_UpdateHWGamma();
	}

	// run cinematic passes on shaders
	R_RunAllCinematics();

	// go into 2D mode
	R_Set2DMode( qtrue );

	// draw buffer stuff
	if( gl_drawbuffer->modified )
	{
		gl_drawbuffer->modified = qfalse;

		if( glState.cameraSeparation == 0 || !glState.stereoEnabled )
		{
			if( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 )
				qglDrawBuffer( GL_FRONT );
			else
				qglDrawBuffer( GL_BACK );
		}
	}

	// texturemode stuff
	if( r_texturemode->modified )
	{
		R_TextureMode( r_texturemode->string );
		r_texturemode->modified = qfalse;
	}

	// swapinterval stuff
	R_UpdateSwapInterval();
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
void R_AddEntityToScene( const entity_t *ent )
{
	if( ( r_numEntities < MAX_ENTITIES ) && ent )
	{
		entity_t *de = &r_entities[r_numEntities++];
		*de = *ent;
	}
}

/*
=====================
R_AddLightToScene
=====================
*/
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, const shader_t *shader )
{
	if( ( r_numDlights < MAX_DLIGHTS ) && intensity && ( r != 0 || g != 0 || b != 0 ) )
	{
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
void R_AddPolyToScene( const poly_t *poly )
{
	if( ( r_numPolys < MAX_POLYS ) && poly && poly->numverts )
	{
		poly_t *dp = &r_polys[r_numPolys++];

		*dp = *poly;
		if( dp->numverts > MAX_POLY_VERTS )
			dp->numverts = MAX_POLY_VERTS;
	}
}

/*
=====================
R_AddLightStyleToScene
=====================
*/
void R_AddLightStyleToScene( int style, float r, float g, float b )
{
	lightstyle_t *ls;

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
void R_RenderScene( const refdef_t *fd )
{
	// flush any remaining 2D bits
	R_Set2DMode( qfalse );

	if( r_norefresh->integer )
		return;

	R_BackendStartFrame();

	if( !( fd->rdflags & RDF_NOWORLDMODEL ) )
	{
		r_lastRefdef = *fd;
	}

	c_brush_polys = 0;
	c_world_leafs = 0;

	r_mark_leaves =
		r_add_polys =
		r_add_entities =
		r_sort_meshes =
		r_draw_meshes =
		r_world_node = 0;

	ri.params = RP_NONE;
	ri.refdef = *fd;
	ri.farClip = 0;
	ri.clipFlags = 15;
	if( r_worldmodel && !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) && r_worldbrushmodel->globalfog )
	{
		ri.farClip = r_worldbrushmodel->globalfog->shader->fog_dist;
		ri.farClip = max( r_farclip_min, ri.farClip ) + r_farclip_bias;
		ri.clipFlags |= 16;
	}
	ri.meshlist = &r_worldlist;
	ri.shadowBits = 0;
	ri.shadowGroup = NULL;

	// adjust field of view for widescreen
	if( glState.wideScreen && !( fd->rdflags & RDF_NOFOVADJUSTMENT ) )
		AdjustFov( &ri.refdef.fov_x, &ri.refdef.fov_y, glState.width, glState.height, qfalse );

	Vector4Set( ri.scissor, fd->x, glState.height - fd->height - fd->y, fd->width, fd->height );
	Vector4Set( ri.viewport, fd->x, glState.height - fd->height - fd->y, fd->width, fd->height );
	VectorCopy( fd->vieworg, ri.pvsOrigin );
	VectorCopy( fd->vieworg, ri.lodOrigin );

	if( gl_finish->integer && !gl_delayfinish->integer && !( fd->rdflags & RDF_NOWORLDMODEL ) )
		qglFinish();

	R_ClearShadowmaps();

	R_CategorizeEntities();

	R_RenderView( fd );

	R_BloomBlend( fd );

	R_PolyBlend();

	R_BackendEndFrame();

	R_Set2DMode( qtrue );
}

/*
===============
R_BeginFrame
===============
*/
void R_EndFrame( void )
{
	// flush any remaining 2D bits
	R_Set2DMode( qfalse );

	// cleanup texture units
	R_BackendCleanUpTextureUnits();

	// apply software gamma
	R_ApplySoftwareGamma();

	// free temporary image buffers
	R_FreeImageBuffers ();

	if( gl_finish->integer && gl_delayfinish->integer )
	{
		qglFlush();
		return;
	}

	GLimp_EndFrame();
}

/*
===============
R_SpeedsMessage
===============
*/
const char *R_SpeedsMessage( char *out, size_t size )
{
	if( out )
		Q_strncpyz( out, r_speeds_msg, size );
	return out;
}

//==================================================================================

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
	Matrix4_Multiply_Vector( ri.worldviewProjectionMatrix, temp, temp2 );

	if( !temp2[3] )
		return;

	out[0] = ( temp2[0] / temp2[3] + 1.0f ) * 0.5f * ri.refdef.width;
	out[1] = ( temp2[1] / temp2[3] + 1.0f ) * 0.5f * ri.refdef.height;
	out[2] = ( temp2[2] / temp2[3] + 1.0f ) * 0.5f;
}

/*
=============
R_TransformVectorToScreen
=============
*/
void R_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out )
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

	out[0] = rd->x + ( temp[0] / temp[3] + 1.0f ) * rd->width * 0.5f;
	out[1] = rd->y + ( temp[1] / temp[3] + 1.0f ) * rd->height * 0.5f;
}

//==================================================================================

/*
=============
R_TraceLine
=============
*/
msurface_t *R_TraceLine( trace_t *tr, const vec3_t start, const vec3_t end, int surfumask )
{
	int i;
	msurface_t *surf;

	// trace against world
	surf = R_TransformedTraceLine( tr, start, end, r_worldent, surfumask );

	// trace against bmodels
	for( i = 0; i < r_numbmodelentities; i++ )
	{
		trace_t t2;
		msurface_t *s2;

		s2 = R_TransformedTraceLine( &t2, start, end, r_bmodelentities[i], surfumask );
		if( t2.fraction < tr->fraction )
		{
			*tr = t2;	// closer impact point
			surf = s2;
		}
	}

	return surf;
}
