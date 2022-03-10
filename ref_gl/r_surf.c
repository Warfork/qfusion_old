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
// R_SURF.C: surface-related refresh code
#include "r_local.h"

#define GL_LIGHTMAP_FORMAT GL_RGBA

entity_t	r_worldent;

static vec3_t	modelorg;		// relative to viewpoint
static vec3_t	modelmins;
static vec3_t	modelmaxs;

void R_BuildLightMap (byte *data, byte *dest);

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
void R_AddSurfaceToList ( msurface_t *surf, int clipflags )
{
	mesh_t *mesh;
	shader_t *shader;
	meshbuffer_t *mb;

	surf->visframe = r_framecount;

	// don't even bother culling
	if ( !(mesh = surf->mesh) || !surf->shaderref ) {
		return;
	}

	shader = surf->shaderref->shader;

	// flare
	if ( surf->facetype == FACETYPE_FLARE ) {
		if ( r_flares->value ) {
			R_AddMeshToBuffer ( surf->mesh, surf->fog, surf, shader, -1 );
		}
		return;
	}

	if ( !r_nocull->value ) {
		if ( surf->facetype == FACETYPE_PLANAR && r_faceplanecull->value && !VectorCompare(surf->origin, vec3_origin) && (shader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK)) ) {
			float dot;

			if ( surf->origin[0] == 1.0f ) {
				dot = modelorg[0] - mesh->xyz_array[0][0];
			} else if ( surf->origin[1] == 1.0f ) {
				dot = modelorg[1] - mesh->xyz_array[0][1];
			} else if ( surf->origin[2] == 1.0f ) {
				dot = modelorg[2] - mesh->xyz_array[0][2];
			} else {
				dot = 
					(modelorg[0] - mesh->xyz_array[0][0]) * surf->origin[0] +
					(modelorg[1] - mesh->xyz_array[0][1]) * surf->origin[1] + 
					(modelorg[2] - mesh->xyz_array[0][2]) * surf->origin[2];
			}

			if ( (shader->flags & SHADER_CULL_FRONT) || r_mirrorview ) {
				if ( dot <= BACKFACE_EPSILON )
					return;
			} else {
				if ( dot > BACKFACE_EPSILON )
					return;
			}
		}

		if ( clipflags && R_CullBox ( mesh->mins, mesh->maxs, clipflags ) ) {
			return;
		}
	}

	mb = R_AddMeshToBuffer ( mesh, surf->fog, surf, shader, surf->lightmaptexturenum );
	if ( mb ) {
		if ( (shader->flags & SHADER_SKY) && !r_fastsky->value ) {
			R_AddSkySurface ( surf );
		}

		c_brush_polys++;
	}
}

/*
=================
R_DrawInlineBModel
=================
*/
void R_DrawInlineBModel (void)
{
	int			i;
	msurface_t	*psurf;

	if ( r_dynamiclight->value == 1 && r_newrefdef.num_dlights && !r_vertexlight->value ) 
	{
		int			k, bit;
		dlight_t	*lt;

		lt = r_newrefdef.dlights;
		for (k=0 ; k<r_newrefdef.num_dlights ; k++, lt++)
		{
			if ( !BoundsAndSphereIntersect (modelmins, modelmaxs, lt->origin, lt->intensity) ) {
				continue;
			}

			bit = 1<<k;
			psurf = currentmodel->firstmodelsurface;
			for ( i=0 ; i<currentmodel->nummodelsurfaces ; i++, psurf++ ) {
				if ( psurf->mesh && (psurf->facetype != FACETYPE_FLARE) ) {
					R_SurfMarkLight (lt, bit, psurf);
				}
			}
		}
	}

	psurf = currentmodel->firstmodelsurface;
	for ( i=0 ; i<currentmodel->nummodelsurfaces ; i++, psurf++ ) {
		if ( psurf->visframe != r_framecount ) {
			R_AddSurfaceToList ( psurf, 0 );
		}
	}
}

/*
=================
R_AddBrushModelToList
=================
*/
void R_AddBrushModelToList (entity_t *e)
{
	int			i;
	qboolean	rotated;

	if (currentmodel->nummodelsurfaces == 0)
		return;

	if ( !Matrix3_Compare (currententity->axis, axis_identity) )
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			modelmins[i] = e->origin[i] - currentmodel->radius;
			modelmaxs[i] = e->origin[i] + currentmodel->radius;
		}

		if (R_CullSphere (e->origin, currentmodel->radius, 15))
			return;
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, currentmodel->mins, modelmins);
		VectorAdd (e->origin, currentmodel->maxs, modelmaxs);

		if (R_CullBox (modelmins, modelmaxs, 15))
			return;
	}

	VectorSubtract (r_newrefdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;

		VectorCopy (modelorg, temp);
		Matrix3_Multiply_Vec3 ( e->axis, temp, modelorg );
	}

	R_DrawInlineBModel ();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_LeafWorldNode
================
*/
void R_LeafWorldNode (void)
{
	int			i;
	int			clipflags;
	msurface_t	**mark, *surf;
	mleaf_t		*pleaf;

	for ( pleaf = r_vischain; pleaf; pleaf = pleaf->vischain )
	{
		// check for door connected areas
		if ( r_newrefdef.areabits ) {
			if (! (r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) ) {
				continue;		// not visible
			}
		}

		clipflags = 15;		// 1 | 2 | 4 | 8
		if ( !r_nocull->value ) {
			int clipped;
			cplane_t *clipplane;

			for (i=0,clipplane=frustum ; i<4 ; i++,clipplane++)
			{
				clipped = BoxOnPlaneSide ( pleaf->mins, pleaf->maxs, clipplane );
				if ( clipped == 2 ) {
					break;
				} else if ( clipped == 1 ) {
					clipflags &= ~(1<<i);	// node is entirely on screen
				}
			}

			if ( i != 4 ) {
				continue;
			}
		}

		i = pleaf->nummarksurfaces;
		mark = pleaf->firstmarksurface;

		do
		{
			surf = *mark++;
			if ( surf->visframe != r_framecount ) {
				R_AddSurfaceToList ( surf, clipflags );
			}
		} while (--i);

		c_world_leafs++;
	}
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	if ( !r_drawworld->value )
		return;
	if ( !r_worldmodel || !r_worldbmodel )
		return;
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
		return;

	VectorCopy (r_newrefdef.vieworg, modelorg);

	r_worldent.model = r_worldmodel;
	Matrix3_Identity (r_worldent.axis);

	currententity = &r_worldent;
	currentmodel = r_worldmodel;

	R_ClearSkyBox ();

	R_MarkLights ();

	R_LeafWorldNode ();
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	int		i;
	mleaf_t	*leaf;
	int		cluster;

	if (r_oldviewcluster == r_viewcluster && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if (r_lockpvs->value)
		return;

	r_vischain = NULL;
	r_visframecount++;
	r_oldviewcluster = r_viewcluster;

	if (r_novis->value || r_viewcluster == -1 || !r_worldbmodel->vis )
	{
		// mark everything
		for (i=0,leaf=r_worldbmodel->leafs ; i<r_worldbmodel->numleafs ; i++, leaf++)
		{
			if ( !leaf->nummarksurfaces ) {
				continue;
			}

			leaf->visframe = r_visframecount;
			leaf->vischain = r_vischain;
			r_vischain = leaf;
		}
		return;
	}

	vis = Mod_ClusterPVS (r_viewcluster, r_worldbmodel);
	for (i=0,leaf=r_worldbmodel->leafs ; i<r_worldbmodel->numleafs ; i++, leaf++)
	{
		cluster = leaf->cluster;
		if ( cluster == -1 || !leaf->nummarksurfaces ) {
			continue;
		}
		if ( vis[cluster>>3] & (1<<(cluster&7)) ) {
			leaf->visframe = r_visframecount;
			leaf->vischain = r_vischain;
			r_vischain = leaf;
		}
	}
}

/*
=============================================================================

  PATCHES

=============================================================================
*/

mesh_t *GL_CreateMeshForPatch ( model_t *mod, dface_t *surf )
{
    int numverts, numindexes, firstvert, patch_cp[2], step[2], size[2], flat[2], i, u, v, p;
	vec4_t colors[MAX_ARRAY_VERTS], points[MAX_ARRAY_VERTS], normals[MAX_ARRAY_VERTS],
		lm_st[MAX_ARRAY_VERTS], tex_st[MAX_ARRAY_VERTS];
	vec4_t c, colors2[MAX_ARRAY_VERTS], normals2[MAX_ARRAY_VERTS], lm_st2[MAX_ARRAY_VERTS], tex_st2[MAX_ARRAY_VERTS];
	mesh_t *mesh;
	index_t	*indexes;
	float subdivlevel;

	patch_cp[0] = LittleLong ( surf->patch_cp[0] );
	patch_cp[1] = LittleLong ( surf->patch_cp[1] );

	if ( !patch_cp[0] || !patch_cp[1] ) {
		return NULL;
	}

	subdivlevel = r_subdivisions->value;
	if ( subdivlevel < 1 )
		subdivlevel = 1;

	numverts = LittleLong ( surf->numverts );
	firstvert = LittleLong ( surf->firstvert );
	for ( i = 0; i < numverts; i++ ) {
		VectorCopy ( mod->bmodel->xyz_array[firstvert + i], points[i] );
		VectorCopy ( mod->bmodel->normals_array[firstvert + i], normals[i] );
		Vector4Scale ( mod->bmodel->colors_array[firstvert + i], (1.0 / 255.0), colors[i] );
		Vector2Copy ( mod->bmodel->st_array[firstvert + i], tex_st[i] );
		Vector2Copy ( mod->bmodel->lmst_array[firstvert + i], lm_st[i] );
	}

// find the degree of subdivision in the u and v directions
	Patch_GetFlatness ( subdivlevel, points, patch_cp, flat );

// allocate space for mesh
	step[0] = (1 << flat[0]);
	step[1] = (1 << flat[1]);
	size[0] = (patch_cp[0] / 2) * step[0] + 1;
	size[1] = (patch_cp[1] / 2) * step[1] + 1;
	numverts = size[0] * size[1];

	if ( numverts > MAX_ARRAY_VERTS ) {
		return NULL;
	}

	mesh = (mesh_t *)Hunk_AllocName ( sizeof(mesh_t), mod->name );
	mesh->numvertexes = numverts;
	mesh->xyz_array = Hunk_AllocName ( numverts * sizeof(vec4_t), mod->name );
	mesh->normals_array = Hunk_AllocName ( numverts * sizeof(vec3_t), mod->name );
	mesh->st_array = Hunk_AllocName ( numverts * sizeof(vec2_t), mod->name );
	mesh->lmst_array = Hunk_AllocName ( numverts * sizeof(vec2_t), mod->name );
	mesh->colors_array = Hunk_AllocName ( numverts * sizeof(vec4_t), mod->name );

	mesh->patchWidth = size[0];
	mesh->patchHeight = size[1];

// fill in
	Patch_Evaluate ( points, patch_cp, step, mesh->xyz_array );
	Patch_Evaluate ( colors, patch_cp, step, colors2 );
	Patch_Evaluate ( normals, patch_cp, step, normals2 );
	Patch_Evaluate ( lm_st, patch_cp, step, lm_st2 );
	Patch_Evaluate ( tex_st, patch_cp, step, tex_st2 );

	for (i = 0; i < numverts; i++)
	{
		VectorNormalize2 ( normals2[i], mesh->normals_array[i] );
		ColorNormalize ( colors2[i], c );
		Vector4Scale ( c, 255.0, mesh->colors_array[i] );
		Vector2Copy ( tex_st2[i], mesh->st_array[i] );
		Vector2Copy ( lm_st2[i], mesh->lmst_array[i] );
    }

// compute new indexes avoiding adding invalid triangles
	numindexes = 0;
	indexes = tempIndexesArray;
    for (v = 0, i = 0; v < size[1]-1; v++)
	{
		for (u = 0; u < size[0]-1; u++, i += 6)
		{
			indexes[0] = p = v * size[0] + u;
			indexes[1] = p + size[0];
			indexes[2] = p + 1;

			if ( !VectorCompare(mesh->xyz_array[indexes[0]], mesh->xyz_array[indexes[1]]) && 
				!VectorCompare(mesh->xyz_array[indexes[0]], mesh->xyz_array[indexes[2]]) && 
				!VectorCompare(mesh->xyz_array[indexes[1]], mesh->xyz_array[indexes[2]]) ) {
				indexes += 3;
				numindexes += 3;
			}

			indexes[0] = p + 1;
			indexes[1] = p + size[0];
			indexes[2] = p + size[0] + 1;

			if ( !VectorCompare(mesh->xyz_array[indexes[0]], mesh->xyz_array[indexes[1]]) && 
				!VectorCompare(mesh->xyz_array[indexes[0]], mesh->xyz_array[indexes[2]]) && 
				!VectorCompare(mesh->xyz_array[indexes[1]], mesh->xyz_array[indexes[2]]) ) {
				indexes += 3;
				numindexes += 3;
			}
		}
	}

// allocate and fill index table
	mesh->numindexes = numindexes;
	mesh->indexes = (index_t *)Hunk_AllocName ( numindexes * sizeof(index_t), mod->name );
	memcpy (mesh->indexes, tempIndexesArray, numindexes * sizeof(index_t) );

	return mesh;
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

/*
==================
GL_BeginBuildingLightmaps
==================
*/
void GL_BeginBuildingLightmaps (void)
{
	r_framecount = 1;

	if (!gl_state.lightmap_textures)
		gl_state.lightmap_textures = TEXNUM_LIGHTMAPS;
}

/*
=======================
GL_EndBuildingLightmaps
=======================
*/
void GL_EndBuildingLightmaps (void)
{
}

/*
=======================
R_BuildLightmaps
=======================
*/
void R_BuildLightmaps (bmodel_t *bmodel)
{
	int i;
	byte lightmap_buffer[4*LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT];

	if ( bmodel->numlightmaps > MAX_MAP_LIGHTMAPS )
		Com_Error( ERR_DROP, "R_BuildLightmaps() - too many lightmaps" );

	GL_BeginBuildingLightmaps ();

	for (i = 0; i < bmodel->numlightmaps; i++)
	{
        R_BuildLightMap (bmodel->lightdata + i*LIGHTMAP_SIZE, lightmap_buffer);

		GL_Bind( gl_state.lightmap_textures + i );
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		qglTexImage2D( GL_TEXTURE_2D, 
						   0, 
						   GL_RGBA,
						   LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 
						   0, 
						   GL_LIGHTMAP_FORMAT, 
						   GL_UNSIGNED_BYTE, 
						   lightmap_buffer );
	}

	GL_EndBuildingLightmaps ();
}
