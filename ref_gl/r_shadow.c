/*
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

#include "r_local.h"

/*
=============================================================

PLANAR STENCIL SHADOWS

=============================================================
*/

static shader_t *r_planarShadowShader;

/*
===============
R_InitPlanarShadows
===============
*/
static void R_InitPlanarShadows( void )
{
	r_planarShadowShader = R_LoadShader( "***r_planarShadow***", SHADER_PLANAR_SHADOW, qtrue, 0, 0 );
}

/*
===============
R_PlanarShadowShader
===============
*/
shader_t *R_PlanarShadowShader( void )
{
	return r_planarShadowShader;
}

/*
===============
R_GetShadowImpactAndDir
===============
*/
static void R_GetShadowImpactAndDir( entity_t *e, trace_t *tr, vec3_t lightdir )
{
	vec3_t point;

	R_LightForOrigin( e->lightingOrigin, lightdir, NULL, NULL, e->model->radius * e->scale );

	VectorSet( lightdir, -lightdir[0], -lightdir[1], -1 );
	VectorNormalizeFast( lightdir );
	VectorMA( e->origin, /*(e->model->radius*e->scale*4 + r_shadows_projection_distance->value)*/ 1024.0f, lightdir, point );

	R_TraceLine( tr, e->origin, point, SURF_NONSOLID );
}

/*
===============
R_CullPlanarShadow
===============
*/
qboolean R_CullPlanarShadow( entity_t *e, vec3_t mins, vec3_t maxs, qboolean occlusion_query )
{
	int i;
	float planedist, dist;
	vec3_t lightdir, point;
	vec3_t bbox[8], newmins, newmaxs;
	trace_t tr;

	if( e->flags & ( RF_NOSHADOW|RF_WEAPONMODEL ) )
		return qtrue;
	if( e->flags & RF_VIEWERMODEL )
		return qfalse;

	R_GetShadowImpactAndDir( e, &tr, lightdir );
	if( tr.fraction == 1.0f )
		return qtrue;

	R_TransformEntityBBox( e, mins, maxs, bbox, qtrue );

	VectorSubtract( tr.endpos, e->origin, point );
	planedist = DotProduct( point, tr.plane.normal ) + 1;
	dist = -1.0f / DotProduct( lightdir, tr.plane.normal );
	VectorScale( lightdir, dist, lightdir );

	ClearBounds( newmins, newmaxs );
	for( i = 0; i < 8; i++ )
	{
		VectorSubtract( bbox[i], e->origin, bbox[i] );
		dist = DotProduct( bbox[i], tr.plane.normal ) - planedist;
		if( dist > 0 )
			VectorMA( bbox[i], dist, lightdir, bbox[i] );
		AddPointToBounds( bbox[i], newmins, newmaxs );
	}
	VectorAdd( newmins, e->origin, newmins );
	VectorAdd( newmaxs, e->origin, newmaxs );

	if( R_CullBox( newmins, newmaxs, ri.clipFlags ) )
		return qtrue;

	// mins/maxs are pretransfomed so use r_worldent here
	if( occlusion_query && OCCLUSION_QUERIES_ENABLED( ri ) )
		R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_PLANARSHADOW, e - r_entities ), r_worldent, newmins, newmaxs );

	return qfalse;
}

/*
===============
R_DeformVPlanarShadow
===============
*/
void R_DeformVPlanarShadow( int numV, float *v )
{
	entity_t *e = ri.currententity;
	float planedist, dist;
	vec3_t planenormal, lightdir, lightdir2, point;
	trace_t tr;

	R_GetShadowImpactAndDir( e, &tr, lightdir );

	Matrix_TransformVector( e->axis, lightdir, lightdir2 );
	Matrix_TransformVector( e->axis, tr.plane.normal, planenormal );
	VectorScale( planenormal, e->scale, planenormal );

	VectorSubtract( tr.endpos, e->origin, point );
	planedist = DotProduct( point, tr.plane.normal ) + 1;
	dist = -1.0f / DotProduct( lightdir2, planenormal );
	VectorScale( lightdir2, dist, lightdir2 );

	for( ; numV > 0; numV--, v += 4 )
	{
		dist = DotProduct( v, planenormal ) - planedist;
		if( dist > 0 )
			VectorMA( v, dist, lightdir2, v );
	}
}

/*
===============
R_PlanarShadowPass
===============
*/
void R_PlanarShadowPass( int state )
{
	GL_EnableTexGen( GL_S, 0 );
	GL_EnableTexGen( GL_T, 0 );
	GL_EnableTexGen( GL_R, 0 );
	GL_EnableTexGen( GL_Q, 0 );
	GL_SetTexCoordArrayMode( 0 );

	GL_SetState( state );
	qglColor4f( 0, 0, 0, bound( 0.0f, r_shadows_alpha->value, 1.0f ) );

	qglDisable( GL_TEXTURE_2D );
	if( glState.stencilEnabled )
		qglEnable( GL_STENCIL_TEST );

	R_FlushArrays();

	if( glState.stencilEnabled )
		qglDisable( GL_STENCIL_TEST );
	qglEnable( GL_TEXTURE_2D );
}

/*
=============================================================

STANDARD PROJECTIVE SHADOW MAPS (SSM)

=============================================================
*/

int r_numShadowGroups;
shadowGroup_t r_shadowGroups[MAX_SHADOWGROUPS];
int r_entShadowBits[MAX_ENTITIES];

//static qboolean r_shadowGroups_sorted;

#define SHADOWGROUPS_HASH_SIZE	8
static shadowGroup_t *r_shadowGroups_hash[SHADOWGROUPS_HASH_SIZE];
static qbyte r_shadowCullBits[MAX_SHADOWGROUPS/8];

/*
===============
R_InitShadowmaps
===============
*/
static void R_InitShadowmaps( void )
{
	// clear all possible values, should be called once per scene
	r_numShadowGroups = 0;
//	r_shadowGroups_sorted = qfalse;

	memset( r_shadowGroups, 0, sizeof( r_shadowGroups ) );
	memset( r_entShadowBits, 0, sizeof( r_entShadowBits ) );
	memset( r_shadowGroups_hash, 0, sizeof( r_shadowGroups_hash ) );
}

/*
===============
R_ClearShadowmaps
===============
*/
void R_ClearShadowmaps( void )
{
	r_numShadowGroups = 0;

	if( r_shadows->integer != SHADOW_MAPPING || ri.refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	// clear all possible values, should be called once per scene
//	r_shadowGroups_sorted = qfalse;
	memset( r_shadowGroups, 0, sizeof( r_shadowGroups ) );
	memset( r_entShadowBits, 0, sizeof( r_entShadowBits ) );
	memset( r_shadowGroups_hash, 0, sizeof( r_shadowGroups_hash ) );
}

/*
===============
R_AddShadowCaster
===============
*/
qboolean R_AddShadowCaster( entity_t *ent )
{
	int i;
	float radius;
	vec3_t origin;
	unsigned int hash_key;
	shadowGroup_t *group;
	mleaf_t *leaf;
	vec3_t mins, maxs, bbox[8];

	if( r_shadows->integer != SHADOW_MAPPING || ri.refdef.rdflags & RDF_NOWORLDMODEL )
		return qfalse;
	if( !glConfig.ext.GLSL || !glConfig.ext.depth_texture || !glConfig.ext.shadow )
		return qfalse;

	VectorCopy( ent->lightingOrigin, origin );
	if( VectorCompare( origin, vec3_origin ) )
		return qfalse;

	// find lighting group containing entities with same lightingOrigin as ours
	hash_key = (unsigned int)( origin[0] * 7 + origin[1] * 5 + origin[2] * 3 );
	hash_key &= ( SHADOWGROUPS_HASH_SIZE-1 );

	for( group = r_shadowGroups_hash[hash_key]; group; group = group->hashNext )
	{
		if( VectorCompare( group->origin, origin ) )
			goto add; // found an existing one, add
	}

	if( r_numShadowGroups == MAX_SHADOWGROUPS )
		return qfalse; // no free groups

	leaf = Mod_PointInLeaf( origin, r_worldmodel );

	// start a new group
	group = &r_shadowGroups[r_numShadowGroups];
	group->bit = ( 1<<r_numShadowGroups );
	//	group->cluster = leaf->cluster;
	group->vis = Mod_ClusterPVS( leaf->cluster, r_worldmodel );

	// clear group bounds
	VectorCopy( origin, group->origin );
	ClearBounds( group->mins, group->maxs );

	// add to hash table
	group->hashNext = r_shadowGroups_hash[hash_key];
	r_shadowGroups_hash[hash_key] = group;

	r_numShadowGroups++;
add:
	// get model bounds
	if( ent->model->type == mod_alias )
		R_AliasModelBBox( ent, mins, maxs );
	else
		R_SkeletalModelBBox( ent, mins, maxs );

	for( i = 0; i < 3; i++ )
	{
		if( mins[i] >= maxs[i] )
			return qfalse;
	}

	r_entShadowBits[ent - r_entities] |= group->bit;
	if( ent->flags & RF_WEAPONMODEL )
		return qtrue;

	// rotate local bounding box and compute the full bounding box for this group
	R_TransformEntityBBox( ent, mins, maxs, bbox, qtrue );
	for( i = 0; i < 8; i++ )
		AddPointToBounds( bbox[i], group->mins, group->maxs );

	// increase projection distance if needed
	VectorSubtract( group->mins, origin, mins );
	VectorSubtract( group->maxs, origin, maxs );
	radius = RadiusFromBounds( mins, maxs );
	group->projDist = max( group->projDist, radius * ent->scale * 2 + min( r_shadows_projection_distance->value, 64 ) );

	return qtrue;
}

/*
===============
R_ShadowGroupSort

Make sure current view cluster comes first
===============
*/
/*
static int R_ShadowGroupSort (void const *a, void const *b)
{
	shadowGroup_t *agroup, *bgroup;

	agroup = (shadowGroup_t *)a;
	bgroup = (shadowGroup_t *)b;

	if( agroup->cluster == r_viewcluster )
		return -2;
	if( bgroup->cluster == r_viewcluster )
		return 2;
	if( agroup->cluster < bgroup->cluster )
		return -1;
	if( agroup->cluster > bgroup->cluster )
		return 1;
	return 0;
}
*/

/*
===============
R_CullShadowmapGroups
===============
*/
void R_CullShadowmapGroups( void )
{
	int i, j;
	vec3_t mins, maxs;
	shadowGroup_t *group;

	if( ri.refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	memset( r_shadowCullBits, 0, sizeof( r_shadowCullBits ) );

	for( i = 0, group = r_shadowGroups; i < r_numShadowGroups; i++, group++ ) {
		for( j = 0; j < 3; j++ )
		{
			mins[j] = group->origin[j] - group->projDist * 1.75 * 0.5 * 0.5;
			maxs[j] = group->origin[j] + group->projDist * 1.75 * 0.5 * 0.5;
		}

		// check if view point is inside the bounding box...
		for( j = 0; j < 3; j++ )
			if( ri.viewOrigin[j] < mins[j] || ri.viewOrigin[j] > maxs[j] )
				break;

		if( j == 3 )
			continue;									// ...it is, so trivially accept

		if( R_CullBox( mins, maxs, ri.clipFlags ) )
			r_shadowCullBits[i>>3] |= (1<<(i&7));		// trivially reject
		else if( OCCLUSION_QUERIES_ENABLED( ri ) )
			R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_SHADOWGROUP, i ), r_worldent, mins, maxs );
	}
}

/*
===============
R_DrawShadowmaps
===============
*/
void R_DrawShadowmaps( void )
{
	int i, j;
	int width, height, textureWidth, textureHeight;
	float lod_scale;
	vec3_t angles;
	vec3_t lightdir, M[3];
	refinst_t oldRI;
	shadowGroup_t *group;

	if( !r_numShadowGroups )
		return;

	width = r_lastRefdef.width;
	height = r_lastRefdef.height;

	ri.previousentity = NULL;
	memcpy( &oldRI, &prevRI, sizeof( refinst_t ) );
	memcpy( &prevRI, &ri, sizeof( refinst_t ) );
	ri.refdef.rdflags &= ~RDF_SKYPORTALINVIEW;
	lod_scale = tan( ri.refdef.fov_x * ( M_PI/180 ) * 0.5f );

/*
	// sort by clusternum (not really needed anymore, but oh well)
	if( !r_shadowGroups_sorted ) {		// note: this breaks hash pointers
		r_shadowGroups_sorted = qtrue;
		qsort( r_shadowGroups, r_numShadowGroups, sizeof(shadowGroup_t), R_ShadowGroupSort );
	}
*/

	// find lighting group containing entities with same lightingOrigin as ours
	for( i = 0, group = r_shadowGroups; i < r_numShadowGroups; i++, group++ )
	{
		if( r_shadowCullBits[i>>3] & ( 1<<( i&7 ) ) )
			continue;

		if( OCCLUSION_QUERIES_ENABLED( prevRI ) )
		{
			if( !R_GetOcclusionQueryResultBool( OQ_SHADOWGROUP, i, qtrue ) )
				continue;
		}

		ri.farClip = group->projDist;
		ri.lod_dist_scale_for_fov = lod_scale;
		ri.clipFlags |= ( 1<<4 ); // clip by far plane too
		ri.shadowBits = 0;      // no shadowing yet
		ri.meshlist = &r_shadowlist;
		ri.shadowGroup = group;
		ri.params = RP_SHADOWMAPVIEW|RP_FLIPFRONTFACE|RP_OLDVIEWCLUSTER; // make sure RP_WORLDSURFVISIBLE isn't set

		// allocate/resize the texture if needed
		R_InitShadowmapTexture( &( r_shadowmapTextures[i] ), i, width, height );

		assert( r_shadowmapTextures[i] && r_shadowmapTextures[i]->upload_width && r_shadowmapTextures[i]->upload_height );

		group->depthTexture = r_shadowmapTextures[i];
		textureWidth = group->depthTexture->upload_width;
		textureHeight = group->depthTexture->upload_height;

		// default to fov 90, R_SetupFrame will most likely alter the values to give depth more precision
		ri.refdef.width = textureWidth;
		ri.refdef.height = textureHeight;
		ri.refdef.fov_x = 90;
		ri.refdef.fov_y = CalcFov( ri.refdef.fov_x, ri.refdef.width, ri.refdef.height );
		Vector4Set( ri.viewport, ri.refdef.x, ri.refdef.y, textureWidth, textureHeight );
		Vector4Set( ri.scissor, ri.refdef.x, ri.refdef.y, textureWidth, textureHeight );

		// set the view transformation matrix according to lightgrid
		R_LightForOrigin( group->origin, lightdir, NULL, NULL, group->projDist * 0.5 );
		VectorSet( lightdir, -lightdir[0], -lightdir[1], -lightdir[2] );
		VectorNormalizeFast( lightdir );

		NormalVectorToAxis( lightdir, M );
		Matrix_EulerAngles( M, angles );

		for( j = 0; j < 3; j++ )
			ri.refdef.viewangles[j] = anglemod( angles[j] );

		// position the light source in the opposite direction
		VectorMA( group->origin, -group->projDist * 0.5, lightdir, ri.refdef.vieworg );

		R_RenderView( &ri.refdef );

		if( !( ri.params & RP_WORLDSURFVISIBLE ) )
			continue; // we didn't cast any shadows on opaque meshes so discard this group

		if( !( prevRI.shadowBits & group->bit ) )
		{	// capture results from framebuffer into depth texture
			prevRI.shadowBits |= group->bit;
			GL_Bind( 0, group->depthTexture );
			qglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, ri.refdef.x, ri.refdef.y, textureWidth, textureHeight );
		}

		Matrix4_Copy( ri.worldviewProjectionMatrix, group->worldviewProjectionMatrix );
	}

	oldRI.shadowBits |= prevRI.shadowBits;  // set shadowBits for all RI's so that we won't
	memcpy( &ri, &prevRI, sizeof( refinst_t ) );
	memcpy( &prevRI, &oldRI, sizeof( refinst_t ) );
}

//==================================================================================

/*
===============
R_InitShadows
===============
*/
void R_InitShadows( void )
{
	R_InitPlanarShadows();

	R_InitShadowmaps();
}

/*
===============
R_ShutdownShadows
===============
*/
void R_ShutdownShadows( void )
{
	r_planarShadowShader = NULL;
}
