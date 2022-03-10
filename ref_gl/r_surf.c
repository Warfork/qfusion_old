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

// r_surf.c: surface-related refresh code

#include "r_local.h"

static vec3_t modelorg;       // relative to viewpoint
static vec3_t modelmins;
static vec3_t modelmaxs;

/*
=============================================================

BRUSH MODELS

=============================================================
*/

/*
=================
R_SurfPotentiallyVisible
=================
*/
qboolean R_SurfPotentiallyVisible( msurface_t *surf )
{
	if( surf->facetype == FACETYPE_FLARE )
		return qtrue;
	if( surf->flags & SURF_NODRAW )
		return qfalse;
	if( !surf->mesh || R_InvalidMesh( surf->mesh ) )
		return qfalse;
	return qtrue;
}

/*
=================
R_CullSurface
=================
*/
qboolean R_CullSurface( msurface_t *surf, unsigned int clipflags )
{
	shader_t *shader = surf->shader;

	if( ( shader->flags & SHADER_SKY ) && r_fastsky->integer )
		return qtrue;
	if( r_nocull->integer )
		return qfalse;
	if( shader->flags & SHADER_AUTOSPRITE )
		return qfalse;

	// flare
	if( surf->facetype == FACETYPE_FLARE )
	{
		if( r_flares->integer && r_flarefade->value )
		{
			vec3_t origin;

			if( ri.currentmodel != r_worldmodel )
			{
				Matrix_TransformVector( ri.currententity->axis, surf->origin, origin );
				VectorAdd( origin, ri.currententity->origin, origin );
			}
			else
			{
				VectorCopy( surf->origin, origin );
			}

			// cull it because we don't want to sort unneeded things
			if( ( origin[0] - ri.viewOrigin[0] ) * ri.vpn[0] +
				( origin[1] - ri.viewOrigin[1] ) * ri.vpn[1] +
				( origin[2] - ri.viewOrigin[2] ) * ri.vpn[2] < 0 )
				return qtrue;

			return ( clipflags && R_CullSphere( origin, 1, clipflags ) );
		}
		return qtrue;
	}

	if( surf->facetype == FACETYPE_PLANAR && r_faceplanecull->integer
#ifdef HARDWARE_OUTLINES
		&& !ri.currententity->outlineHeight
#endif
		&& ( shader->flags & ( SHADER_CULL_FRONT|SHADER_CULL_BACK ) ) )
	{
		// Vic: I hate q3map2. I really do.
		if( !VectorCompare( surf->plane->normal, vec3_origin ) )
		{
			float dist;

			dist = PlaneDiff( modelorg, surf->plane );
			if( ( shader->flags & SHADER_CULL_FRONT ) || ( ri.params & RP_MIRRORVIEW ) )
			{
				if( dist <= BACKFACE_EPSILON )
					return qtrue;
			}
			else
			{
				if( dist >= -BACKFACE_EPSILON )
					return qtrue;
			}
		}
	}

	return ( clipflags && R_CullBox( surf->mins, surf->maxs, clipflags ) );
}

/*
=================
R_AddSurfaceToList
=================
*/
static meshbuffer_t *R_AddSurfaceToList( msurface_t *surf, unsigned int clipflags )
{
	shader_t *shader;
	meshbuffer_t *mb;

	if( R_CullSurface( surf, clipflags ) )
		return NULL;

	shader = ((r_drawworld->integer == 2) ? R_OcclusionShader() : surf->shader);
	if( shader->flags & SHADER_SKY )
	{
		qboolean vis = R_AddSkySurface( surf );
		if( ( ri.params & RP_NOSKY ) && vis )
		{
			R_AddMeshToList( MB_MODEL, surf->fog, shader, surf - r_worldbrushmodel->surfaces + 1 );
			ri.params &= ~RP_NOSKY;
		}
		return NULL;
	}

	if( OCCLUSION_QUERIES_ENABLED( ri ) )
	{
		if( shader->flags & SHADER_PORTAL )
			R_SurfOcclusionQueryKey( ri.currententity, surf );
		if( OCCLUSION_OPAQUE_SHADER( shader ) )
			R_AddOccludingSurface( surf, shader );
	}

	c_brush_polys++;
	mb = R_AddMeshToList( surf->facetype == FACETYPE_FLARE ? MB_SPRITE : MB_MODEL,
		surf->fog, shader, surf - r_worldbrushmodel->surfaces + 1 );
	ri.surfmbuffers[surf - r_worldbrushmodel->surfaces] = mb;
	return mb;
}

/*
=================
R_CullBrushModel
=================
*/
qboolean R_CullBrushModel( entity_t *e )
{
	int i;
	qboolean rotated;
	model_t	*model = e->model;
	mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;

	if( bmodel->nummodelsurfaces == 0 )
		return qtrue;

	if( !Matrix_Compare( e->axis, axis_identity ) )
	{
		rotated = qtrue;
		for( i = 0; i < 3; i++ )
		{
			modelmins[i] = e->origin[i] - model->radius * e->scale;
			modelmaxs[i] = e->origin[i] + model->radius * e->scale;
		}
		if( R_CullSphere( e->origin, model->radius * e->scale, ri.clipFlags ) )
			return qtrue;
	}
	else
	{
		rotated = qfalse;
		VectorMA( e->origin, e->scale, model->mins, modelmins );
		VectorMA( e->origin, e->scale, model->maxs, modelmaxs );
		if( R_CullBox( modelmins, modelmaxs, ri.clipFlags ) )
			return qtrue;
	}

	if( ri.refdef.rdflags & ( RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW ) || ( ri.params & RP_SKYPORTALVIEW ) )
	{
		if( rotated )
		{
			if( R_VisCullSphere( e->origin, model->radius * e->scale ) )
				return qtrue;
		}
		else
		{
			if( R_VisCullBox( modelmins, modelmaxs ) )
				return qtrue;
		}
	}

	return qfalse;
}

/*
=================
R_AddBrushModelToList
=================
*/
void R_AddBrushModelToList( entity_t *e )
{
	unsigned int i;
	qboolean rotated;
	model_t	*model = e->model;
	mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;
	msurface_t *psurf;
	unsigned int dlightbits;
	meshbuffer_t *mb;

#ifdef HARDWARE_OUTLINES
	e->outlineHeight = r_worldent->outlineHeight;
	Vector4Copy( r_worldent->outlineRGBA, e->outlineColor );
#endif

	rotated = !Matrix_Compare( e->axis, axis_identity );
	VectorSubtract( ri.refdef.vieworg, e->origin, modelorg );
	if( rotated )
	{
		vec3_t temp;

		VectorCopy( modelorg, temp );
		Matrix_TransformVector( e->axis, temp, modelorg );
	}

	dlightbits = 0;
	if( ( r_dynamiclight->integer == 1 ) && !r_fullbright->integer && !( ri.params & RP_SHADOWMAPVIEW ) )
	{
		for( i = 0; i < r_numDlights; i++ )
		{
			if( BoundsIntersect( modelmins, modelmaxs, r_dlights[i].mins, r_dlights[i].maxs ) )
				dlightbits |= ( 1<<i );
		}
	}

	for( i = 0, psurf = bmodel->firstmodelsurface; i < (unsigned)bmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( !R_SurfPotentiallyVisible( psurf ) )
			continue;

		if( ri.params & RP_SHADOWMAPVIEW )
		{
			if( psurf->visframe != r_framecount )
				continue;
			if( ( psurf->shader->sort >= SHADER_SORT_OPAQUE ) && ( psurf->shader->sort <= SHADER_SORT_BANNER ) )
			{
				if( prevRI.surfmbuffers[psurf - r_worldbrushmodel->surfaces] )
				{
					if( !R_CullSurface( psurf, 0 ) )
					{
						ri.params |= RP_WORLDSURFVISIBLE;
						prevRI.surfmbuffers[psurf - r_worldbrushmodel->surfaces]->shadowbits |= ri.shadowGroup->bit;
					}
				}
			}
			continue;
		}

		psurf->visframe = r_framecount;
		mb = R_AddSurfaceToList( psurf, 0 );
		if( mb )
		{
			mb->sortkey |= ( ( psurf->superLightStyle+1 ) << 10 );
			if( R_SurfPotentiallyLit( psurf ) )
				mb->dlightbits = dlightbits;
		}
	}
}

/*
=============================================================

WORLD MODEL

=============================================================
*/

/*
================
R_MarkLeafSurfaces
================
*/
static void R_MarkLeafSurfaces( msurface_t **mark, unsigned int clipflags, unsigned int dlightbits )
{
	unsigned int newDlightbits;
	msurface_t *surf;
	meshbuffer_t *mb;

	do
	{
		surf = *mark++;

		// note that R_AddSurfaceToList may set meshBuffer to NULL
		// for world ALL surfaces to prevent referencing to freed memory region
		if( surf->visframe != r_framecount )
		{
			surf->visframe = r_framecount;
			mb = R_AddSurfaceToList( surf, clipflags );
			if( mb )
				mb->sortkey |= ( ( surf->superLightStyle+1 ) << 10 );
		}
		else
		{
			mb = ri.surfmbuffers[surf - r_worldbrushmodel->surfaces];
		}

		newDlightbits = mb ? dlightbits & ~mb->dlightbits : 0;
		if( newDlightbits && R_SurfPotentiallyLit( surf ) )
			mb->dlightbits |= R_AddSurfDlighbits( surf, newDlightbits );
	} while( *mark );
}

/*
================
R_RecursiveWorldNode
================
*/
static void R_RecursiveWorldNode( mnode_t *node, unsigned int clipflags, unsigned int dlightbits )
{
	unsigned int i, newDlightbits;
	unsigned int bit;
	const cplane_t *clipplane;
	mleaf_t	*pleaf;

	while( 1 )
	{
		if( node->pvsframe != r_pvsframecount )
			return;

		if( clipflags )
		{
			for( i = sizeof( ri.frustum )/sizeof( ri.frustum[0] ), bit = 1, clipplane = ri.frustum; i > 0; i--, bit<<=1, clipplane++ )
			{
				if( clipflags & bit )
				{
					int clipped = BoxOnPlaneSide( node->mins, node->maxs, clipplane );
					if( clipped == 2 )
						return;
					if( clipped == 1 )
						clipflags &= ~bit; // node is entirely on screen
				}
			}
		}

		if( !node->plane )
			break;

		newDlightbits = 0;
		if( dlightbits )
		{
			float dist;

			for( i = 0, bit = 1; i < r_numDlights; i++, bit <<= 1 )
			{
				if( !( dlightbits & bit ) )
					continue;

				dist = PlaneDiff( r_dlights[i].origin, node->plane );
				if( dist < -r_dlights[i].intensity )
					dlightbits &= ~bit;
				if( dist < r_dlights[i].intensity )
					newDlightbits |= bit;
			}
		}

		R_RecursiveWorldNode( node->children[0], clipflags, dlightbits );

		node = node->children[1];
		dlightbits = newDlightbits;
	}

	// if a leaf node, draw stuff
	pleaf = ( mleaf_t * )node;
	pleaf->visframe = r_framecount;

	// add leaf bounds to view bounds
	for( i = 0; i < 3; i++ )
	{
		ri.visMins[i] = min( ri.visMins[i], pleaf->mins[i] );
		ri.visMaxs[i] = max( ri.visMaxs[i], pleaf->maxs[i] );
	}

	R_MarkLeafSurfaces( pleaf->firstVisSurface, clipflags, dlightbits );
	c_world_leafs++;
}

/*
================
R_MarkShadowLeafSurfaces
================
*/
static void R_MarkShadowLeafSurfaces( msurface_t **mark, unsigned int clipflags )
{
	msurface_t *surf;
	meshbuffer_t *mb;
	const unsigned int bit = ri.shadowGroup->bit;

	do
	{
		surf = *mark++;
		if( surf->flags & ( SURF_NOIMPACT|SURF_NODRAW ) )
			continue;

		mb = prevRI.surfmbuffers[surf - r_worldbrushmodel->surfaces];
		if( !mb || (mb->shadowbits & bit) )
			continue;

		// this surface is visible in previous RI, not marked as shadowed...
		if( ( surf->shader->sort >= SHADER_SORT_OPAQUE ) && ( surf->shader->sort <= SHADER_SORT_ALPHATEST ) )
		{	// ...is opaque
			if( !R_CullSurface( surf, clipflags ) )
			{	// and is visible to the light source too
				ri.params |= RP_WORLDSURFVISIBLE;
				mb->shadowbits |= bit;
			}
		}
	} while( *mark );
}

/*
================
R_LinearShadowLeafs
================
*/
static void R_LinearShadowLeafs( void )
{
	unsigned int i, j;
	unsigned int cpf, bit;
	const cplane_t *clipplane;
	mleaf_t	*pleaf;

	for( j = r_worldbrushmodel->numleafs, pleaf = r_worldbrushmodel->leafs; j > 0; j--, pleaf++ )
	{
		if( pleaf->visframe != r_framecount )
			continue;
		if( !( ri.shadowGroup->vis[pleaf->cluster>>3] & ( 1<<( pleaf->cluster&7 ) ) ) )
			continue;

		cpf = ri.clipFlags;
		for( i = sizeof( ri.frustum )/sizeof( ri.frustum[0] ), bit = 1, clipplane = ri.frustum; i > 0; i--, bit<<=1, clipplane++ )
		{
			int clipped = BoxOnPlaneSide( pleaf->mins, pleaf->maxs, clipplane );
			if( clipped == 2 )
				break;
			if( clipped == 1 )
				cpf &= ~bit;	// leaf is entirely on screen
		}

		if( !i ) {
			R_MarkShadowLeafSurfaces( pleaf->firstVisSurface, cpf );
			c_world_leafs++;
		}
	}
}

//==================================================================================

int r_surfQueryKeys[MAX_SURF_QUERIES];

/*
===============
R_ClearSurfOcclusionQueryKeys
===============
*/
void R_ClearSurfOcclusionQueryKeys( void )
{
	memset( r_surfQueryKeys, -1, sizeof( r_surfQueryKeys ) );
}

/*
===============
R_SurfOcclusionQueryKey
===============
*/
int R_SurfOcclusionQueryKey( entity_t *e, msurface_t *surf )
{
	int i;
	int *keys = r_surfQueryKeys;
	int key = surf - r_worldbrushmodel->surfaces;

	if( e != r_worldent )
		return -1;

	for( i = 0; i < MAX_SURF_QUERIES; i++ )
	{
		if( keys[i] >= 0 )
		{
			if( keys[i] == key )
				return i;
		}
		else
		{
			keys[i] = key;
			return i;
		}
	}

	return -1;
}

/*
===============
R_SurfIssueOcclusionQueries
===============
*/
void R_SurfIssueOcclusionQueries( void )
{
	int i, *keys = r_surfQueryKeys;
	msurface_t *surf;

	for( i = 0; keys[i] >= 0; i++ )
	{
		surf = &r_worldbrushmodel->surfaces[keys[i]];
		R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_CUSTOM, i ), r_worldent, surf->mins, surf->maxs );
	}
}

//==================================================================================

/*
=============
R_CalcDistancesToFogVolumes
=============
*/
static void R_CalcDistancesToFogVolumes( void )
{
	int i;
	mfog_t *fog;

	for( i = 0, fog = r_worldbrushmodel->fogs; i < r_worldbrushmodel->numfogs; i++, fog++ )
		ri.fog_dist_to_eye[fog - r_worldbrushmodel->fogs] = PlaneDiff( ri.viewOrigin, fog->visibleplane );
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	int clipflags, msec = 0;
	unsigned int dlightbits;

	if( !r_drawworld->integer )
		return;
	if( !r_worldmodel )
		return;
	if( ri.refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	VectorCopy( ri.refdef.vieworg, modelorg );

	ri.previousentity = NULL;
	ri.currententity = r_worldent;
	ri.currentmodel = ri.currententity->model;
#ifdef HARDWARE_OUTLINES
	if( (ri.refdef.rdflags & RDF_WORLDOUTLINES) && (r_viewcluster != -1) )
		ri.currententity->outlineHeight = max( 0.0f, r_outlines_world->value );
	else
		ri.currententity->outlineHeight = 0.0f;
	Vector4Copy( mapConfig.outlineColor, ri.currententity->outlineColor );
#endif

	if( !( ri.params & RP_SHADOWMAPVIEW ) )
	{
		R_AllocMeshbufPointers( &ri );
		memset( ri.surfmbuffers, 0, r_worldbrushmodel->numsurfaces * sizeof( meshbuffer_t * ) );

		R_CalcDistancesToFogVolumes();
	}

	ClearBounds( ri.visMins, ri.visMaxs );

	R_ClearSkyBox();

	if( r_nocull->integer )
		clipflags = 0;
	else
		clipflags = ri.clipFlags;

	if( r_dynamiclight->integer != 1 || r_fullbright->integer )
		dlightbits = 0;
	else
		dlightbits = r_numDlights < 32 ? ( 1 << r_numDlights ) - 1 : -1;

	if( r_speeds->integer )
		msec = Sys_Milliseconds();
	if( ri.params & RP_SHADOWMAPVIEW )
		R_LinearShadowLeafs ();
	else
		R_RecursiveWorldNode( r_worldbrushmodel->nodes, clipflags, dlightbits );
	if( r_speeds->integer )
		r_world_node += Sys_Milliseconds() - msec;
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current cluster
===============
*/
void R_MarkLeaves( void )
{
	qbyte *vis;
	int i;
	mleaf_t	*leaf, **pleaf;
	mnode_t *node;
	int cluster;

	if( ri.refdef.rdflags & RDF_NOWORLDMODEL )
		return;
	if( r_oldviewcluster == r_viewcluster && ( ri.refdef.rdflags & RDF_OLDAREABITS ) && !r_novis->integer && r_viewcluster != -1 )
		return;
	if( ri.params & RP_SHADOWMAPVIEW )
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if( r_lockpvs->integer )
		return;

	r_pvsframecount++;
	r_oldviewcluster = r_viewcluster;

	if( r_novis->integer || r_viewcluster == -1 || !r_worldbrushmodel->vis )
	{
		// mark everything
		for( pleaf = r_worldbrushmodel->visleafs, leaf = *pleaf; leaf; leaf = *pleaf++ )
			leaf->pvsframe = r_pvsframecount;
		for( i = 0, node = r_worldbrushmodel->nodes; i < r_worldbrushmodel->numnodes; i++, node++ )
			node->pvsframe = r_pvsframecount;
		return;
	}

	vis = Mod_ClusterPVS( r_viewcluster, r_worldmodel );
	for( pleaf = r_worldbrushmodel->visleafs, leaf = *pleaf; leaf; leaf = *pleaf++ )
	{
		cluster = leaf->cluster;

		// check for door connected areas
		if( ri.refdef.areabits )
		{
			if( !( ri.refdef.areabits[leaf->area>>3] & ( 1<<( leaf->area&7 ) ) ) )
				continue; // not visible
		}

		if( vis[cluster>>3] & ( 1<<( cluster&7 ) ) )
		{
			node = (mnode_t *)leaf;
			do
			{
				if( node->pvsframe == r_pvsframecount )
					break;
				node->pvsframe = r_pvsframecount;
				node = node->parent;
			}
			while( node );
		}
	}
}
