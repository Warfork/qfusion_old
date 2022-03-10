/*
Copyright (C) 2002-2003 Victor Luchits

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

// r_poly.c - handles fragments and arbitrary polygons
static	vec4_t			poly_xyz[MAX_POLY_VERTS];
static	vec2_t			poly_st[MAX_POLY_VERTS];
static	byte_vec4_t		poly_color[MAX_POLY_VERTS];
static	index_t			poly_indexes[(MAX_POLY_VERTS-2)*3];

entity_t				r_polyent;
static	mesh_t			poly_mesh;
static	meshbuffer_t	poly_mbuffer;

/*
=================
R_InitPolys
=================
*/
void R_InitPolys (void)
{
	int i;
	index_t *index;

	// build trifan indexes
	index = poly_indexes;
	for ( i = 0; i < MAX_POLY_VERTS-2; i++, index += 3 ) {
		index[0] = 0;
		index[1] = i + 1;
		index[2] = i + 2;
	}

	poly_mesh.xyz_array = poly_xyz;
	poly_mesh.st_array = poly_st;
	poly_mesh.colors_array = poly_color;
	poly_mesh.indexes = poly_indexes;

	r_polyent.type = ET_POLY;
	Matrix3_Identity ( r_polyent.axis );

	poly_mbuffer.mesh = &poly_mesh;
	poly_mbuffer.entity = &r_polyent;
}

/*
=================
R_DrawPoly
=================
*/
void R_DrawPoly ( meshbuffer_t *mb )
{
	int i;
	poly_t *p;

	p = &r_newrefdef.polys[-mb->infokey-1];

	for ( i = 0; i < p->numverts; i++ ) {
		VectorCopy ( p->verts[i], poly_mesh.xyz_array[i] );
		Vector2Copy ( p->stcoords[i], poly_mesh.st_array[i] );
		*(int *)poly_mesh.colors_array[i] = *(int *)p->colors[i];
	}

	poly_mesh.numvertexes = p->numverts;
	poly_mesh.numindexes = (p->numverts - 2) * 3;

	poly_mbuffer.fog = mb->fog;
	poly_mbuffer.shader = mb->shader;

	qglLoadMatrixf ( r_modelview_matrix );

	R_PushMesh ( &poly_mesh, MF_NONBATCHED | mb->shader->features );
	R_RenderMeshBuffer ( &poly_mbuffer, false );
}

/*
=================
R_AddPolysToList
=================
*/
void R_AddPolysToList (void)
{
	int i;
	poly_t *p;
	mfog_t *fog;

	currententity = &r_polyent;
	for ( i = 0, p = r_newrefdef.polys; i < r_newrefdef.num_polys; i++, p++ ) {
		fog = R_FogForSphere ( p->verts[0], 0 );	// FIXME: this is a gross hack
		R_AddMeshToBuffer ( NULL, fog, NULL, p->shader, -(i+1) );		
	}
}

static int numFragmentVerts;
static int maxFragmentVerts;
static vec3_t *fragmentVerts;

static int numClippedFragments;
static int maxClippedFragments;
static fragment_t *clippedFragments;

static int		fragmentFrame;
static cplane_t fragmentPlanes[6];

#define	MAX_FRAGMENT_VERTS	128

/*
=================
R_ClipPoly
=================
*/
void R_ClipPoly ( int nump, vec4_t vecs, int stage, fragment_t *fr )
{
	cplane_t *plane;
	qboolean	front, back;
	float	*v, d;
	float	dists[MAX_FRAGMENT_VERTS];
	int		sides[MAX_FRAGMENT_VERTS];
	vec4_t	newv[MAX_FRAGMENT_VERTS];
	int		newc;
	int		i, j;

	if (nump > MAX_FRAGMENT_VERTS-2)
		Com_Error (ERR_DROP, "R_ClipPoly: MAX_FRAGMENT_VERTS");

	if (stage == 6)
	{	// fully clipped
		if ( nump > 2 ) {
			fr->numverts = nump;
			fr->firstvert = numFragmentVerts;

			if ( numFragmentVerts+nump >= maxFragmentVerts ) {
				nump = maxFragmentVerts - numFragmentVerts;
			}

			for ( i = 0, v = vecs; i < nump; i++, v += 4 ) {
				VectorCopy ( v, fragmentVerts[numFragmentVerts+i] );
			}

			numFragmentVerts += nump;
		}

		return;
	}

	front = back = false;
	plane = &fragmentPlanes[stage];
	for ( i = 0, v = vecs; i < nump; i++, v += 4 )
	{
		d = PlaneDiff ( v, plane );
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
		{
			sides[i] = SIDE_ON;
		}

		dists[i] = d;
	}

	if ( !front ) {
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*4)) );
	newc = 0;

	for ( i = 0, v = vecs; i < nump; i++, v += 4 )
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[newc]);
			newc++;
			break;
		case SIDE_BACK:
			break;
		case SIDE_ON:
			VectorCopy (v, newv[newc]);
			newc++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
			newv[newc][j] = v[j] + d * (v[j+4] - v[j]);
		newc++;
	}

	// continue
	R_ClipPoly ( newc, newv[0], stage+1, fr );
}

/*
=================
R_PlanarSurfClipFragment
=================
*/
void R_PlanarSurfClipFragment ( msurface_t *surf, vec3_t normal )
{
	int			i;
	fragment_t	*fr;
	vec4_t		*vert;
	index_t		*index;
	mesh_t		*mesh;
	vec4_t		verts[MAX_FRAGMENT_VERTS];

	// greater than 60 degrees
	if ( DotProduct (normal, surf->origin) < 0.5 ) {
		return;
	}

	// copy vertex data and clip to each triangle
	mesh = surf->mesh;
	index = mesh->indexes;
	vert = mesh->xyz_array;
	for ( i = 0; i < mesh->numindexes; i += 3, index += 3 ) {
		fr = &clippedFragments[numClippedFragments];
		fr->numverts = 0;

		VectorCopy ( vert[index[0]], verts[0] );
		VectorCopy ( vert[index[1]], verts[1] );
		VectorCopy ( vert[index[2]], verts[2] );
		R_ClipPoly ( 3, verts[0], 0, fr );

		if ( fr->numverts ) {
			numClippedFragments++;

			if ( numFragmentVerts >= maxFragmentVerts ||
				numClippedFragments >= maxClippedFragments ) {
				return;
			}
		}
	}
}

/*
=================
R_PatchSurfClipFragment
=================
*/
void R_PatchSurfClipFragment ( msurface_t *surf, vec3_t normal )
{
	int			i;
	fragment_t	*fr;
	vec4_t		*vert;
	index_t		*index;
	mesh_t		*mesh;
	vec4_t		verts[MAX_FRAGMENT_VERTS];
	vec3_t		dir1, dir2, snorm;

	// copy vertex data and clip to each triangle
	mesh = surf->mesh;
	index = mesh->indexes;
	vert = mesh->xyz_array;
	for ( i = 0; i < mesh->numindexes; i += 3, index += 3 ) {
		fr = &clippedFragments[numClippedFragments];
		fr->numverts = 0;

		VectorCopy ( vert[index[0]], verts[0] );
		VectorCopy ( vert[index[1]], verts[1] );
		VectorCopy ( vert[index[2]], verts[2] );
		
		// calculate two mostly perpendicular edge directions
		VectorSubtract ( verts[0], verts[1], dir1 );
		VectorSubtract ( verts[2], verts[1], dir2 );
			
		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the surface normal
		CrossProduct ( dir1, dir2, snorm );

		// greater than 60 degrees
		// we multiply 0.5 by length of snorm to avoid normalizing
		if ( DotProduct (normal, snorm) < 0.5 * VectorLength (snorm) ) {
			continue;
		}

		R_ClipPoly ( 3, verts[0], 0, fr );

		if ( fr->numverts ) {
			numClippedFragments++;

			if ( numFragmentVerts >= maxFragmentVerts ||
				numClippedFragments >= maxClippedFragments ) {
				return;
			}
		}
	}
}

/*
=================
R_RecursiveFragmentNode
=================
*/
void R_RecursiveFragmentNode ( mnode_t *node, vec3_t origin, float radius, vec3_t normal )
{
	float dist;
	cplane_t *plane;

mark0:
	if ( numFragmentVerts >= maxFragmentVerts ||
		numClippedFragments >= maxClippedFragments) {
		return;			// already reached the limit somewhere else
	}

	if ( node->plane == NULL ) {	// leaf
		int c;
		mleaf_t *leaf;
		msurface_t *surf, **mark;
		mshaderref_t *shaderref;

		leaf = (mleaf_t *)node;
		if ( !(c = leaf->nummarksurfaces) ) {
			return;
		}

		mark = leaf->firstmarksurface;
		do
		{
			if ( numFragmentVerts >= maxFragmentVerts ||
				numClippedFragments >= maxClippedFragments ) {
				return;
			}

			surf = *mark++;
			if ( surf->fragmentframe == fragmentFrame ) {
				continue;
			}

			surf->fragmentframe = fragmentFrame;
			if ( !(shaderref = surf->shaderref) ) {
				continue;
			}
			if ( shaderref->flags & SURF_NOMARKS ) {
				continue;
			}

			if ( surf->facetype == FACETYPE_PLANAR ) {
				if ( shaderref->contents & CONTENTS_SOLID ) {
					R_PlanarSurfClipFragment ( surf, normal );
				}
			} else if ( surf->facetype == FACETYPE_PATCH ) {
				R_PatchSurfClipFragment ( surf, normal );
			}
		} while (--c);

		return;
	}

	plane = node->plane;
	dist = PlaneDiff ( origin, plane );

	if ( dist > radius )
	{
		node = node->children[0];
		goto mark0;
	}
	if ( dist < -radius )
	{
		node = node->children[1];
		goto mark0;
	}

	R_RecursiveFragmentNode ( node->children[0], origin, radius, normal );
	R_RecursiveFragmentNode ( node->children[1], origin, radius, normal );
}

/*
=================
R_GetClippedFragments
=================
*/
int R_GetClippedFragments ( vec3_t origin, float radius, mat3_t axis, int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments )
{
	int i;
	float d;

	fragmentFrame++;

	// initialize fragments
	numFragmentVerts = 0;
	maxFragmentVerts = maxfverts;
	fragmentVerts = fverts;

	numClippedFragments = 0;
	maxClippedFragments = maxfragments;
	clippedFragments = fragments;

	// calculate clipping planes
	for ( i = 0; i < 3; i++ ) {
		d = DotProduct ( origin, axis[i] );

		VectorCopy ( axis[i], fragmentPlanes[i*2].normal );
		fragmentPlanes[i*2].dist = d - radius;
		fragmentPlanes[i*2].type = PlaneTypeForNormal ( fragmentPlanes[i*2].normal );

		VectorNegate ( axis[i], fragmentPlanes[i*2+1].normal);
		fragmentPlanes[i*2+1].dist = -d - radius;
		fragmentPlanes[i*2+1].type = PlaneTypeForNormal ( fragmentPlanes[i*2+1].normal );
	}

	R_RecursiveFragmentNode ( r_worldbmodel->nodes, origin, radius, axis[0] );

	return numClippedFragments;
}

