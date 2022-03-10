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

entity_t		r_worldent;

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
R_AddSurfaceToList
=================
*/
void R_AddSurfaceToList( msurface_t *surf, int clipflags )
{
	mesh_t *mesh;
	shader_t *shader;
	meshbuffer_t *mb;

	surf->visframe = r_framecount;

	if( surf->shaderref->flags & SURF_NODRAW )
		return;

	shader = surf->shaderref->shader;

	// flare
	if( surf->facetype == FACETYPE_FLARE ) {
		if ( r_flares->integer )
			R_AddMeshToList( MB_MODEL, surf->fog, shader, surf - currentmodel->surfaces + 1 );
		return;
	}

	if( !(mesh = surf->mesh) || R_InvalidMesh( mesh ) )
		return;

	if( !r_nocull->integer ) {
		if ( surf->facetype == FACETYPE_PLANAR && r_faceplanecull->integer && !VectorCompare(surf->origin, vec3_origin) && (shader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK)) ) {
			float dot;
			float *vert;

			vert = mesh->xyzArray[0];
			if( surf->origin[0] == 1.0f )
				dot = modelorg[0] - vert[0];
			else if( surf->origin[1] == 1.0f )
				dot = modelorg[1] - vert[1];
			else if( surf->origin[2] == 1.0f )
				dot = modelorg[2] - vert[2];
			else
				dot = 
					(modelorg[0] - vert[0]) * surf->origin[0] +
					(modelorg[1] - vert[1]) * surf->origin[1] + 
					(modelorg[2] - vert[2]) * surf->origin[2];

			if( (shader->flags & SHADER_CULL_FRONT) || r_mirrorview ) {
				if( dot <= BACKFACE_EPSILON )
					return;
			} else {
				if( dot >= -BACKFACE_EPSILON )
					return;
			}
		}

		if( clipflags && R_CullBox( surf->mins, surf->maxs, clipflags ) )
			return;
	}

	mb = R_AddMeshToList( MB_MODEL, surf->fog, shader, surf - currentmodel->surfaces + 1 );
	if( mb ) {
		if( (shader->flags & SHADER_SKY) && !r_fastsky->integer )
			R_AddSkySurface( surf );
		c_brush_polys++;
	}
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

	if( model->nummodelsurfaces == 0 )
		return;

	if( !Matrix_Compare( e->axis, axis_identity ) ) {
		rotated = qtrue;
		for( i = 0; i < 3; i++ ) {
			modelmins[i] = e->origin[i] - model->radius * e->scale;
			modelmaxs[i] = e->origin[i] + model->radius * e->scale;
		}

		if( R_CullSphere( e->origin, model->radius, 15 ) )
			return;
		if( R_VisCullSphere( e->origin, model->radius ) )
			return;
	}
	else
	{
		rotated = qfalse;
		VectorMA( e->origin, e->scale, model->mins, modelmins );
		VectorMA( e->origin, e->scale, model->maxs, modelmaxs );

		if( R_CullBox( modelmins, modelmaxs, 15 ) )
			return;
		if( R_VisCullBox( modelmins, modelmaxs ) )
			return;
	}

	VectorSubtract( r_refdef.vieworg, e->origin, modelorg );
	if( rotated ) {
		vec3_t	temp;

		VectorCopy( modelorg, temp );
		Matrix_TransformVector( e->axis, temp, modelorg );
	}

	if( r_dynamiclight->integer && r_numDlights && !(r_vertexlight->integer || r_fullbright->integer) ) {
		int			k, bit;
		dlight_t	*lt;

		lt = r_dlights;
		for( k = 0; k < r_numDlights; k++, lt++ ) {
			if( !BoundsAndSphereIntersect( modelmins, modelmaxs, lt->origin, lt->intensity ) )
				continue;

			bit = 1<<k;
			psurf = model->firstmodelsurface;
			for( i = 0; i < model->nummodelsurfaces; i++, psurf++ ) {
				if( psurf->mesh && (psurf->facetype != FACETYPE_FLARE) )
					R_SurfMarkLight( bit, psurf );
			}
		}
	}

	psurf = model->firstmodelsurface;
	for( i = 0; i < model->nummodelsurfaces; i++, psurf++ ) {
		if( psurf->visframe != r_framecount )
			R_AddSurfaceToList( psurf, 0 );
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
void R_RecursiveWorldNode( mnode_t *node, int clipflags )
{
	int			i, clipped;
	cplane_t	*clipplane;
	msurface_t	**mark, *surf;
	mleaf_t		*pleaf;

	while( 1 ) {
		if( node->visframe != r_visframecount )
			return;

		if( clipflags ) {
			for( i = 0, clipplane = frustum; i < 4; i++, clipplane++ ) {
				clipped = BoxOnPlaneSide( node->mins, node->maxs, clipplane );
				if( clipped == 2 )
					return;
				else if( clipped == 1 )
					clipflags &= ~(1<<i);	// node is entirely on screen
			}
		}

		if( !node->plane )
			break;

		R_RecursiveWorldNode( node->children[0], clipflags );
		node = node->children[1];
	}

	// if a leaf node, draw stuff
	pleaf = ( mleaf_t * )node;

	// check for door connected areas
	if( r_refdef.areabits ) {
		if(! (r_refdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
			return;		// not visible
	}

	if( !(i = pleaf->nummarksurfaces) )
		return;

	mark = pleaf->firstmarksurface;
	do {
		surf = *mark++;
		if( surf->visframe != r_framecount )
			R_AddSurfaceToList( surf, clipflags );
	} while( --i );

	c_world_leafs++;
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
	if( r_refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	VectorCopy( r_refdef.vieworg, modelorg );

	currententity = &r_worldent;
	currentmodel = currententity->model;

	R_ClearSkyBox ();
	R_MarkLights ();

	if( r_nocull->integer )
		R_RecursiveWorldNode( currentmodel->nodes, 0 );
	else
		R_RecursiveWorldNode( currentmodel->nodes, 15 );
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

	if( r_oldviewcluster == r_viewcluster && !r_novis->integer && r_viewcluster != -1 )
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

	if( (surf->facetype != FACETYPE_PLANAR && surf->facetype != FACETYPE_TRISURF) || !surf->shaderref )
		return;

	mesh = surf->mesh;
	if( !mesh || !mesh->numIndexes || mesh->numIndexes % 6 )
		return;

	shader = surf->shaderref->shader;
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
