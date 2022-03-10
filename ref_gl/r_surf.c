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
// r_surf.c: surface-related refresh code
#include "r_local.h"

static vec3_t	modelorg;		// relative to viewpoint
static vec3_t	modelmins;
static vec3_t	modelmaxs;

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
R_AddSurfaceToList
=================
*/
meshbuffer_t *R_AddSurfaceToList( msurface_t *surf, int clipflags )
{
	shader_t *shader;
	meshbuffer_t *mb;

	surf->visframe = r_framecount;
	shader = surf->shader;

	if( (shader->flags & SHADER_SKY) && r_fastsky->integer )
		return NULL;

	// flare
	if( surf->facetype == FACETYPE_FLARE ) {
		if( r_flares->integer && r_flarefade->value ) {
			if( !r_nocull->integer ) {
				vec3_t origin;

				if( ri.currentmodel != r_worldmodel ) {
					Matrix_TransformVector( ri.currententity->axis, surf->origin, origin );
					VectorAdd( origin, ri.currententity->origin, origin );
				} else {
					VectorCopy( surf->origin, origin );
				}

				// cull it because we don't want to sort unneeded things
				if( ( origin[0] - ri.viewOrigin[0] ) * ri.vpn[0] +
					( origin[1] - ri.viewOrigin[1] ) * ri.vpn[1] + 
					( origin[2] - ri.viewOrigin[2] ) * ri.vpn[2] < 0 )
					return NULL;
				if( R_CullSphere( origin, 1, clipflags ) )
					return NULL;
			}
			return R_AddMeshToList( MB_SPRITE, surf->fog, shader, surf - ri.currentmodel->surfaces + 1 );
		}
		return NULL;
	}

	if( !r_nocull->integer ) {
		if( surf->facetype == FACETYPE_PLANAR && r_faceplanecull->integer && (shader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK)) ) {
			// Vic: I hate q3map2. I really do.
			if( !VectorCompare( surf->plane->normal, vec3_origin ) ) {
				float dist;

				dist = PlaneDiff( modelorg, surf->plane );
				if( (shader->flags & SHADER_CULL_FRONT) || (ri.params & RP_MIRRORVIEW) ) {
					if( dist <= BACKFACE_EPSILON )
						return NULL;
				} else {
					if( dist >= -BACKFACE_EPSILON )
						return NULL;
				}
			}
		}

		if( clipflags && R_CullBox( surf->mins, surf->maxs, clipflags ) )
			return NULL;
	}

	mb = R_AddMeshToList( MB_MODEL, surf->fog, shader, surf - ri.currentmodel->surfaces + 1 );
	if( mb ) {
		if( shader->flags & SHADER_SKY )
			R_AddSkySurface( surf );
		c_brush_polys++;
	}

	return mb;
}

/*
=================
R_AddBrushModelToList
=================
*/
void R_AddBrushModelToList( entity_t *e )
{
	int			i;
	qboolean	rotated;
	model_t		*model = e->model;
	msurface_t	*psurf;
	unsigned int dlightbits;
	meshbuffer_t *mb;

	if( model->nummodelsurfaces == 0 )
		return;

	if( !Matrix_Compare( e->axis, axis_identity ) ) {
		rotated = qtrue;
		for( i = 0; i < 3; i++ ) {
			modelmins[i] = e->origin[i] - model->radius * e->scale;
			modelmaxs[i] = e->origin[i] + model->radius * e->scale;
		}

		if( R_CullSphere( e->origin, model->radius * e->scale, 15 ) )
			return;
	}
	else
	{
		rotated = qfalse;
		VectorMA( e->origin, e->scale, model->mins, modelmins );
		VectorMA( e->origin, e->scale, model->maxs, modelmaxs );

		if( R_CullBox( modelmins, modelmaxs, 15 ) )
			return;
	}

	if( ri.refdef.rdflags & RDF_PORTALINVIEW ) {
		if( R_VisCullSphere( e->origin, model->radius * e->scale ) )
			return;
	}

	VectorSubtract( ri.refdef.vieworg, e->origin, modelorg );
	if( rotated ) {
		vec3_t	temp;

		VectorCopy( modelorg, temp );
		Matrix_TransformVector( e->axis, temp, modelorg );
	}

	dlightbits = 0;
	if( r_dynamiclight->integer && !r_fullbright->integer ) {
		for( i = 0; i < r_numDlights; i++ ) {
			if( BoundsIntersect( modelmins, modelmaxs, r_dlights[i].mins, r_dlights[i].maxs ) )
				dlightbits |= (1<<i);
		}
	}

	for( i = 0, psurf = model->firstmodelsurface; i < model->nummodelsurfaces; i++, psurf++ ) {
		if( R_SurfPotentiallyVisible( psurf ) ) {
			mb = R_AddSurfaceToList( psurf, 0 );
			if( mb ) {
				mb->sortkey |= ((psurf->superLightStyle+1) << 10);

				if( R_SurfPotentiallyLit( psurf ) )
					mb->dlightbits = dlightbits;
			}
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
R_RecursiveWorldNode
================
*/
static void R_RecursiveWorldNode( mnode_t *node, int clipflags, unsigned int dlightbits )
{
	int			i, clipped;
	unsigned int newDlightbits;
	cplane_t	*clipplane;
	msurface_t	**mark, *surf;
	mleaf_t		*pleaf;
	meshbuffer_t *mb;

	while( 1 ) {
		if( node->visframe != r_visframecount )
			return;

		if( clipflags ) {
			for( i = 0, clipplane = ri.frustum; i < 4; i++, clipplane++ ) {
				clipped = BoxOnPlaneSide( node->mins, node->maxs, clipplane );
				if( clipped == 2 )
					return;
				else if( clipped == 1 )
					clipflags &= ~(1<<i);	// node is entirely on screen
			}
		}

		if( !node->plane )
			break;

		newDlightbits = 0;
		if( dlightbits ) {
			unsigned int bit;
			float dist;

			for( i = 0, bit = 1; i < r_numDlights; i++, bit<<=1 ) {
				if( !(dlightbits & bit) )
					continue;

				dist = PlaneDiff( r_dlights[i].origin, node->plane );

				if( dist < -r_dlights[i].intensity )
					dlightbits &= ~bit;
				if( dist <  r_dlights[i].intensity )
					newDlightbits |= bit;
			}
		}

		R_RecursiveWorldNode( node->children[0], clipflags, dlightbits );

		node = node->children[1];
		dlightbits = newDlightbits;
	}

	// if a leaf node, draw stuff
	pleaf = ( mleaf_t * )node;
	if( !pleaf->firstVisSurface )
		return;

	mark = pleaf->firstVisSurface;
	do {
		surf = *mark++;

		if( !R_SurfPotentiallyVisible( surf ) )
			continue;

		// note that R_AddSurfaceToList may set meshBuffer to NULL
		// for world ALL surfaces to prevent referencing to freed memory region
		if( surf->visframe != r_framecount ) {
			surf->meshBuffer = R_AddSurfaceToList( surf, clipflags );
			if( surf->meshBuffer )
				surf->meshBuffer->sortkey |= ((surf->superLightStyle+1) << 10);
		}
		mb = surf->meshBuffer;

		newDlightbits = mb ? dlightbits & ~mb->dlightbits : 0;
		if( newDlightbits && R_SurfPotentiallyLit( surf ) )
			mb->dlightbits |= R_AddSurfDlighbits( surf, newDlightbits );
	} while( *mark );

	c_world_leafs++;
}

/*
=============
R_CalcDistancesToFogVolumes
=============
*/
static void R_CalcDistancesToFogVolumes( void )
{
	int i;
	mfog_t *fog;

	for( i = 0, fog = r_worldmodel->fogs; i < r_worldmodel->numfogs; i++, fog++ )
		ri.fog_dist_to_eye[fog - r_worldmodel->fogs] = PlaneDiff( ri.viewOrigin, fog->visibleplane );
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
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

	R_ClearSkyBox ();

	R_CalcDistancesToFogVolumes ();

	if( r_speeds->integer )
		r_world_node = Sys_Milliseconds ();
	R_RecursiveWorldNode( ri.currentmodel->nodes, r_nocull->integer ? 0 : 15, (!r_dynamiclight->integer || r_fullbright->integer ? 0 : r_numDlights < 32 ? (1 << r_numDlights) - 1 : -1) );
	if( r_speeds->integer )
		r_world_node = Sys_Milliseconds () - r_world_node;
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current cluster
===============
*/
void R_MarkLeaves( void )
{
	qbyte	*vis;
	int		i;
	mleaf_t	*leaf;
	mnode_t *node;
	int		cluster;

	if( ri.refdef.rdflags & RDF_NOWORLDMODEL )
		return;
	if( r_oldviewcluster == r_viewcluster && (ri.refdef.rdflags & RDF_OLDAREABITS) && !r_novis->integer && r_viewcluster != -1 )
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if( r_lockpvs->integer )
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;

	if( r_novis->integer || r_viewcluster == -1 || !r_worldmodel->vis ) {
		// mark everything
		for( i = 0; i < r_worldmodel->numleafs; i++ )
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for( i = 0; i < r_worldmodel->numnodes; i++ )
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS( r_viewcluster, r_worldmodel );
	for( i = 0, leaf = r_worldmodel->leafs; i < r_worldmodel->numleafs; i++, leaf++ ) {
		cluster = leaf->cluster;
		if( cluster == -1 )
			continue;

		// check for door connected areas
		if( ri.refdef.areabits ) {
			if(! (ri.refdef.areabits[leaf->area>>3] & (1<<(leaf->area&7)) ) )
				continue;		// not visible
		}

		if( vis[cluster>>3] & (1<<(cluster&7)) ) {
			node = (mnode_t *)leaf;
			do {
				if( node->visframe == r_visframecount )
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while( node );
		}
	}
}

/*
=============================================================================

  AUTOSPRITES

=============================================================================
*/

/*
================
R_FixAutosprites
================
*/
void R_FixAutosprites( msurface_t *surf )
{
	int i, j;
	vec2_t *stArray;
	index_t *quad;
	mesh_t *mesh;
	shader_t *shader;

	if( (surf->facetype != FACETYPE_PLANAR && surf->facetype != FACETYPE_TRISURF) || !surf->shader )
		return;

	mesh = surf->mesh;
	if( !mesh || !mesh->numIndexes || mesh->numIndexes % 6 )
		return;

	shader = surf->shader;
	if( !shader->numdeforms || !(shader->flags & SHADER_AUTOSPRITE) )
		return;

	for( i = 0; i < shader->numdeforms; i++ )
		if( shader->deforms[i].type == DEFORMV_AUTOSPRITE )
			break;

	if( i == shader->numdeforms )
		return;

	stArray = mesh->stArray;
	for( i = 0, quad = mesh->indexes; i < mesh->numIndexes; i += 6, quad += 6 ) {
		for( j = 0; j < 6; j++ ) {
			if( stArray[quad[j]][0] < -0.1 || stArray[quad[j]][0] > 1.1 ||
				stArray[quad[j]][1] < -0.1 || stArray[quad[j]][1] > 1.1 ) {
				stArray[quad[0]][0] = 0;
				stArray[quad[0]][1] = 1;
				stArray[quad[1]][0] = 0;
				stArray[quad[1]][1] = 0;
				stArray[quad[2]][0] = 1;
				stArray[quad[2]][1] = 1;
				stArray[quad[5]][0] = 1;
				stArray[quad[5]][1] = 0;
				break;
			}
		}
	}
}
