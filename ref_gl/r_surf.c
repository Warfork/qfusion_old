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

entity_t	r_worldent;

static vec3_t	modelorg;		// relative to viewpoint
static vec3_t	modelmins;
static vec3_t	modelmaxs;

#define GL_LIGHTMAP_FORMAT GL_RGBA

typedef struct
{
	int internal_format;
	int	current_lightmap_texture;
} gllightmapstate_t;

static gllightmapstate_t gl_lms;

static void		LM_InitBlock( void );
static void		LM_UploadBlock( void );
static qboolean	LM_AllocBlock (int w, int h, int *x, int *y);

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
void R_AddSurfaceToList ( msurface_t *surf )
{
	mesh_t *mesh;
	shader_t *shader;
	meshbuffer_t *mb;

	// don't even bother culling
	if ( surf->visframe == r_framecount ) {
		return;
	}

	surf->visframe = r_framecount;
	
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

	if ( surf->facetype == FACETYPE_PLANAR ) {
		if ( !r_nocull->value && ((r_faceplanecull->value && (shader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK)))) ) {
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
	} else if ( surf->facetype == FACETYPE_MESH || surf->facetype == FACETYPE_TRISURF ) {
		if ( R_CullBox ( mesh->mins, mesh->maxs ) ) {
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

	psurf = currentmodel->firstmodelsurface;
	for ( i=0 ; i<currentmodel->nummodelsurfaces ; i++, psurf++ ) {
		R_AddSurfaceToList ( psurf );
	}

	if ( r_dynamiclight->value == 1 && r_newrefdef.num_dlights && !r_vertexlight->value ) 
	{
		int			k, bit;
		vec3_t		point, lorigin;
		dlight_t	*lt;

		lt = r_newrefdef.dlights;
		for (k=0 ; k<r_newrefdef.num_dlights ; k++, lt++)
		{
			if ( !BoundsAndSphereIntersect (modelmins, modelmaxs, lt->origin, lt->intensity) ) {
				continue;
			}

			VectorCopy ( lt->origin, lorigin );
			VectorSubtract ( lorigin, currententity->origin, lt->origin );
			if ( !Matrix3_Compare (currententity->axis, axis_identity) )
			{
				VectorCopy ( lt->origin, point );
				Matrix3_Multiply_Vec3 ( currententity->axis, point, lt->origin );
			}

			bit = 1<<k;
			psurf = currentmodel->firstmodelsurface;
			for ( i=0 ; i<currentmodel->nummodelsurfaces ; i++, psurf++ ) {
				R_SurfMarkLight (lt, bit, psurf);
			}

			VectorCopy ( lorigin, lt->origin );
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

		if (R_CullSphere (e->origin, currentmodel->radius))
			return;
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, currentmodel->mins, modelmins);
		VectorAdd (e->origin, currentmodel->maxs, modelmaxs);

		if (R_CullBox (modelmins, modelmaxs))
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
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode (mnode_t *node, int clipflags)
{
	int			c;
	msurface_t	**mark;
	mleaf_t		*pleaf;
	int			clipped;

	if ( node->visframe != r_visframecount )
		return;

#if 0			// FIXME: this is buggy
	if ( clipflags & (1<<4) ) {
		clipped = BoxOnPlaneSide ( node->mins, node->maxs, &r_clipplane );
		if (clipped == 2)
			return;
		else if (clipped == 1)
			clipflags &= ~(1<<4);	// node is entirely on screen
	}
#endif

	if ( !r_nocull->value && clipflags )
	{
		int i;
		cplane_t *clipplane;

		for (i=0,clipplane=frustum ; i<4 ; i++,clipplane++)
		{
			if (! (clipflags & (1<<i)) )
				continue;	// don't need to clip against it

			clipped = BoxOnPlaneSide ( node->mins, node->maxs, clipplane );
			if (clipped == 2)
				return;
			else if (clipped == 1)
				clipflags &= ~(1<<i);	// node is entirely on screen
		}
	}

// if a leaf node, draw stuff
	if ( node->contents != CONTENTS_NODE )
	{
		pleaf = (mleaf_t *)node;

		// check for door connected areas
		if (r_newrefdef.areabits)
		{
			if (! (r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) ) {
				return;		// not visible
			}
		}

		mark = pleaf->firstmarksurface;
		if ( !(c = pleaf->nummarksurfaces) )
			return;

		do
		{
			R_AddSurfaceToList ( *mark++ );
		} while (--c);

		c_world_leafs++;
		return;
	}

	R_RecursiveWorldNode (node->children[0], clipflags);
	R_RecursiveWorldNode (node->children[1], clipflags);
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
	currententity = &r_worldent;
	currentmodel = r_worldmodel;
	Matrix3_Identity (r_worldent.axis);

	R_ClearSkyBox ();

	if ( r_mirrorview || r_portalview ) {
		R_RecursiveWorldNode( r_worldbmodel->nodes, 15 );
	} else {
		R_RecursiveWorldNode( r_worldbmodel->nodes, 31 );
	}

	if ( r_dynamiclight->value == 1 && r_newrefdef.num_dlights && !r_vertexlight->value ) 
	{
		int			k;
		dlight_t	*lt;

		lt = r_newrefdef.dlights;
		for (k=0 ; k<r_newrefdef.num_dlights ; k++, lt++)
		{
			R_MarkLightWorldNode (lt, 1<<k, r_worldbmodel->nodes);
		}
	}
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
	byte	fatvis[MAX_MAP_LEAFS/8];
	mnode_t	*node;
	int		i, c;
	mleaf_t	*leaf;
	int		cluster;

	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2 && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if (r_lockpvs->value)
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if (r_novis->value || r_viewcluster == -1 || !r_worldbmodel->vis )
	{
		// mark everything
		for (i=0 ; i<r_worldbmodel->numleafs ; i++)
			r_worldbmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_worldbmodel->numnodes ; i++)
			r_worldbmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS (r_viewcluster, r_worldbmodel);
	// may have to combine two clusters because of solid water boundaries
	if (r_viewcluster2 != r_viewcluster)
	{
		memcpy (fatvis, vis, r_worldbmodel->vis->rowsize);
		vis = Mod_ClusterPVS (r_viewcluster2, r_worldbmodel);
		c = r_worldbmodel->vis->rowsize/4;
		for (i=0 ; i<c ; i++)
			((int *)fatvis)[i] |= ((int *)vis)[i];
		vis = fatvis;
	}
	
	for (i=0,leaf=r_worldbmodel->leafs ; i<r_worldbmodel->numleafs ; i++, leaf++)
	{
		cluster = leaf->cluster;
		if (cluster == -1)
			continue;

		if (vis[cluster>>3] & (1<<(cluster&7)))
		{
			node = (mnode_t *)leaf;
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

/*
=============================================================================

  MESHES

=============================================================================
*/

void GL_CreateMesh ( model_t *mod, dface_t *in, msurface_t *out )
{
    int numverts, firstvert, mesh_cp[2], step[2], size[2], flat[2], i, u, v, p;
	vec4_t colors[MAX_ARRAY_VERTS], points[MAX_ARRAY_VERTS], normals[MAX_ARRAY_VERTS],
		lm_st[MAX_ARRAY_VERTS], tex_st[MAX_ARRAY_VERTS];
	vec4_t normals2[MAX_ARRAY_VERTS], lm_st2[MAX_ARRAY_VERTS], tex_st2[MAX_ARRAY_VERTS];
	mesh_t *mesh;
	index_t	*indexes;
	float subdivlevel;

	mesh_cp[0] = LittleLong ( in->mesh_cp[0] );
	mesh_cp[1] = LittleLong ( in->mesh_cp[1] );

	if ( !mesh_cp[0] || !mesh_cp[1] ) {
		return;
	}

	subdivlevel = r_subdivisions->value;
	if ( subdivlevel < 4 )
		subdivlevel = 4;

	numverts = LittleLong ( in->numverts );
	firstvert = LittleLong ( in->firstvert );
	for ( i = 0; i < numverts; i++ ) {
		VectorCopy ( mod->bmodel->xyz_array[firstvert + i], points[i] );
		VectorCopy ( mod->bmodel->normals_array[firstvert + i], normals[i] );
		Vector4Copy ( mod->bmodel->colors_array[firstvert + i], colors[i] );
		Vector2Copy ( mod->bmodel->st_array[firstvert + i], tex_st[i] );
		Vector2Copy ( mod->bmodel->lmst_array[firstvert + i], lm_st[i] );
	}

// find the degree of subdivision in the u and v directions
	Mesh_GetFlatness ( subdivlevel, points, mesh_cp, flat );

// allocate space for mesh
	step[0] = (1 << flat[0]);
	step[1] = (1 << flat[1]);
	size[0] = (mesh_cp[0] / 2) * step[0] + 1;
	size[1] = (mesh_cp[1] / 2) * step[1] + 1;
	numverts = size[0] * size[1];

	if ( numverts > MAX_ARRAY_VERTS ) {
		return;
	}

	out->mesh = mesh = Hunk_AllocName ( sizeof(mesh_t), mod->name );
	mesh->numvertexes = numverts;
	mesh->xyz_array = Hunk_AllocName ( mesh->numvertexes * sizeof(vec4_t), mod->name );
	mesh->normals_array = Hunk_AllocName ( mesh->numvertexes * sizeof(vec3_t), mod->name );
	mesh->st_array = Hunk_AllocName ( mesh->numvertexes * sizeof(vec2_t), mod->name );
	mesh->lmst_array = Hunk_AllocName ( mesh->numvertexes * sizeof(vec2_t), mod->name );
	mesh->colors_array = Hunk_AllocName ( mesh->numvertexes * sizeof(vec4_t), mod->name );

	mesh->patchWidth = size[0];
	mesh->patchHeight = size[1];

// fill in
	Mesh_EvalQuadricBezierPatch ( points, mesh_cp, step, mesh->xyz_array );
	Mesh_EvalQuadricBezierPatch ( tex_st, mesh_cp, step, mesh->colors_array );
	Mesh_EvalQuadricBezierPatch ( normals, mesh_cp, step, normals2 );
	Mesh_EvalQuadricBezierPatch ( lm_st, mesh_cp, step, lm_st2 );
	Mesh_EvalQuadricBezierPatch ( tex_st, mesh_cp, step, tex_st2 );

	for (i = 0; i < numverts; i++)
	{
		VectorNormalize2 ( normals2[i], mesh->normals_array[i] );
		Vector2Copy ( tex_st2[i], mesh->st_array[i] );
		Vector2Copy ( lm_st2[i], mesh->lmst_array[i] );
    }

// allocate and fill index table
	mesh->numindexes = (size[0]-1) * (size[1]-1) * 6;
	mesh->indexes = (index_t *)Hunk_AllocName ( mesh->numindexes * sizeof(index_t), mod->name );

	indexes = mesh->indexes;
    for (v = 0, i = 0; v < size[1]-1; v++)
	{
		for (u = 0; u < size[0]-1; u++, i += 6, indexes += 6)
		{
			indexes[0] = p = v * size[0] + u;
			indexes[1] = p + size[0];
			indexes[2] = p + 1;
			indexes[3] = p + 1;
			indexes[4] = p + size[0];
			indexes[5] = p + size[0] + 1;
		}
	}
}

//===========================================================

/*
================
GL_PretransformAutosprites
================
*/
void GL_PretransformAutosprites (msurface_t *surf)
{
	int i, j;
	mesh_t *mesh;
	shader_t *shader;
	float *v[4];
	float *n[4];
	vec3_t centre, right, up, tv, tempvec;
	mat3_t matrix;

	if ( (surf->facetype != FACETYPE_PLANAR && surf->facetype != FACETYPE_TRISURF) ||
		!surf->mesh || surf->mesh->numindexes < 6 || !surf->shaderref )
		return;

	shader = surf->shaderref->shader;
	if ( !shader->numdeforms )
		return;

	for ( i = 0; i < shader->numdeforms; i++ )
	{
		if ( shader->deforms[i].type == DEFORMV_AUTOSPRITE ||
			shader->deforms[i].type == DEFORMV_AUTOSPRITE2 )
			break;
	}

	if ( i == shader->numdeforms )
		return;

	mesh = surf->mesh;
	if ( shader->deforms[i].type == DEFORMV_AUTOSPRITE ) {
		for ( i = 0; i < mesh->numindexes; i += 6 )
		{
			v[0] = (mesh->xyz_array + mesh->indexes[i+0])[0];
			v[1] = (mesh->xyz_array + mesh->indexes[i+3])[0];
			v[2] = (mesh->xyz_array + mesh->indexes[i+4])[0];
			v[3] = (mesh->xyz_array + mesh->indexes[i+5])[0];

			n[0] = (mesh->normals_array + mesh->indexes[i+0])[0];
			n[1] = (mesh->normals_array + mesh->indexes[i+3])[0];
			n[2] = (mesh->normals_array + mesh->indexes[i+4])[0];
			n[3] = (mesh->normals_array + mesh->indexes[i+5])[0];

			VectorCopy ( n[0], matrix[2] );
			MakeNormalVectors ( matrix[2], matrix[0], matrix[1] );

			for ( j = 0; j < 3; j++ )
				centre[j] = (v[0][j]+v[1][j]+v[2][j]+v[3][j]) * 0.25;

			for ( j = 0; j < 4; j++ )
			{
				VectorSubtract ( v[j], centre, tempvec );
				Matrix3_Multiply_Vec3 ( matrix, tempvec, tv );
				VectorAdd ( centre, tv, v[j] );
			}
		}
	} else {
		for ( i = 0; i < mesh->numindexes; i += 6 )
		{
			v[0] = (mesh->xyz_array + mesh->indexes[i+0])[0];
			v[1] = (mesh->xyz_array + mesh->indexes[i+1])[0];
			v[2] = (mesh->xyz_array + mesh->indexes[i+2])[0];

			VectorSubtract ( v[1], v[0], right );
			VectorSubtract ( v[2], v[0], up );

			if ( DotProduct (right, right) > DotProduct (up, up) ) {
				j = mesh->indexes[i];
				mesh->indexes[i] = mesh->indexes[i+2];
				mesh->indexes[i+2] = mesh->indexes[i+3] = mesh->indexes[i+5];
				mesh->indexes[i+5] = mesh->indexes[i+1];
				mesh->indexes[i+1] = mesh->indexes[i+4] = j;
			}
		}
	}
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

	gl_lms.current_lightmap_texture = 1;
	gl_lms.internal_format = gl_tex_solid_format;
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
						   gl_lms.internal_format,
						   LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 
						   0, 
						   GL_LIGHTMAP_FORMAT, 
						   GL_UNSIGNED_BYTE, 
						   lightmap_buffer );
	}

	GL_EndBuildingLightmaps ();
}
