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
static	vec3_t			poly_xyz[MAX_ARRAY_VERTS];
static	vec2_t			poly_st[MAX_ARRAY_VERTS];
static	byte_vec4_t		poly_color[MAX_ARRAY_VERTS];

static	mesh_t			poly_mesh;

/*
=================
R_PolyOverflow
=================
*/
qboolean R_PolyOverflow( meshbuffer_t *mb )
{
	poly_t *p;

	p = &r_polys[-mb->infokey-1];
	return( numVerts + p->numverts > MAX_ARRAY_VERTS/* || 
		numIndexes + (p->numverts - 2) * 3 > MAX_ARRAY_INDEXES*/ );
}

/*
=================
R_PushPoly
=================
*/
void R_PushPoly( meshbuffer_t *mb )
{
	int i;
	poly_t *p;

	p = &r_polys[-mb->infokey-1];
	if( p->numverts > MAX_ARRAY_VERTS )
		return;

	for( i = 0; i < p->numverts; i++ ) {
		VectorCopy( p->verts[i], poly_xyz[i] );
		Vector2Copy( p->stcoords[i], poly_st[i] );
		*(int *)poly_color[i] = *(int *)p->colors[i];
	}

	poly_mesh.numVertexes = p->numverts;
	poly_mesh.numIndexes = (p->numverts - 2) * 3;
	poly_mesh.xyzArray = poly_xyz;
	poly_mesh.stArray = poly_st;
	poly_mesh.colorsArray = poly_color;
	poly_mesh.indexes = r_trifan_indexes;

	R_PushMesh( &poly_mesh, mb->shader->features );
}

/*
=================
R_DrawPoly
=================
*/
void R_DrawPoly( meshbuffer_t *mb )
{
	R_TranslateForEntity( &r_worldent );
	R_RenderMeshBuffer( mb, qfalse );
}

/*
=================
R_AddPolysToList
=================
*/
void R_AddPolysToList( void )
{
	int i;
	poly_t *p;
	mfog_t *fog;

	currententity = &r_worldent;
	for( i = 0, p = r_polys; i < r_numPolys; i++, p++ ) {
		fog = R_FogForSphere( p->verts[0], 0 );	// FIXME: this is a gross hack
		R_AddMeshToList( MB_POLY, fog, p->shader, -(i+1) );		
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

#define	MAX_FRAGMENT_VERTS	64

/*
=================
R_ClipTriangle
=================
*/
void R_ClipTriangle( vec3_t a, vec3_t b, vec3_t c, fragment_t *fr )
{
	int			i, j;
	int			stage, newc, numv;
	cplane_t	*plane;
	qboolean	front;
	float		*v, d;
	float		dists[MAX_FRAGMENT_VERTS+1];
	int			sides[MAX_FRAGMENT_VERTS+1];
	vec3_t		*verts, *newverts, newv[2][MAX_FRAGMENT_VERTS+1];

	numv = 3;
	verts = newv[1];
	VectorCopy( a, verts[0] );
	VectorCopy( b, verts[1] );
	VectorCopy( c, verts[2] );

	for( stage = 0, plane = fragmentPlanes; stage < 6; stage++, plane++ ) {
		for( i = 0, v = verts[0], front = qfalse; i < numv; i++, v += 3 ) {
			d = PlaneDiff( v, plane );

			if( d > ON_EPSILON ) {
				front = qtrue;
				sides[i] = SIDE_FRONT;
			} else if( d < -ON_EPSILON ) {
				sides[i] = SIDE_BACK;
			} else {
				front = qtrue;
				sides[i] = SIDE_ON;
			}
			dists[i] = d;
		}

		if( !front )
			return;

		// clip it
		sides[i] = sides[0];
		dists[i] = dists[0];
		VectorCopy( verts[0], (verts[0] + (i * 3)) );

		newc = 0;
		newverts = newv[stage & 1];

		for( i = 0, v = verts[0]; i < numv; i++, v += 3 ) {
			switch( sides[i] ) {
				case SIDE_FRONT:
					if( newc == MAX_FRAGMENT_VERTS )
						return;
					VectorCopy( v, newverts[newc] );
					newc++;
					break;
				case SIDE_BACK:
					break;
				case SIDE_ON:
					if( newc == MAX_FRAGMENT_VERTS )
						return;
					VectorCopy( v, newverts[newc] );
					newc++;
					break;
			}

			if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
				continue;
			if( newc == MAX_FRAGMENT_VERTS )
				return;

			d = dists[i] / (dists[i] - dists[i+1]);
			for( j = 0; j < 3; j++ )
				newverts[newc][j] = v[j] + d * (v[j+3] - v[j]);
			newc++;
		}

		if( newc <= 2 )
			return;

		// continue with new verts
		numv = newc;
		verts = newverts;
	}

	// fully clipped
	if( numFragmentVerts + numv > maxFragmentVerts )
		return;

	fr->numverts = numv;
	fr->firstvert = numFragmentVerts;

	for( i = 0, v = verts[0]; i < numv; i++, v += 3 )
		VectorCopy( v, fragmentVerts[numFragmentVerts + i] );
	numFragmentVerts += numv;
}

/*
=================
R_PlanarSurfClipFragment
=================
*/
void R_PlanarSurfClipFragment( msurface_t *surf, vec3_t normal )
{
	int			i;
	fragment_t	*fr;
	vec3_t		*vert;
	index_t		*index;
	mesh_t		*mesh;

	// greater than 60 degrees
	if( DotProduct( normal, surf->origin ) < 0.5 )
		return;

	// copy vertex data and clip to each triangle
	mesh = surf->mesh;
	index = mesh->indexes;
	vert = mesh->xyzArray;

	fr = &clippedFragments[numClippedFragments];
	fr->numverts = 0;

	for( i = 0; i < mesh->numIndexes; i += 3, index += 3 ) {
		R_ClipTriangle( vert[index[0]], vert[index[1]], vert[index[2]], fr );

		if( fr->numverts ) {
			if( numFragmentVerts == maxFragmentVerts ||	++numClippedFragments == maxClippedFragments )
				return;
			(++fr)->numverts = 0;
		}
	}
}

/*
=================
R_PatchSurfClipFragment
=================
*/
void R_PatchSurfClipFragment( msurface_t *surf, vec3_t normal )
{
	int			i;
	fragment_t	*fr;
	vec3_t		*vert;
	index_t		*index;
	mesh_t		*mesh;
	vec3_t		verts[3];
	vec3_t		dir1, dir2, snorm;

	// copy vertex data and clip to each triangle
	mesh = surf->mesh;
	index = mesh->indexes;
	vert = mesh->xyzArray;

	fr = &clippedFragments[numClippedFragments];
	fr->numverts = 0;

	for( i = 0; i < mesh->numIndexes; i += 3, index += 3 ) {
		VectorCopy( vert[index[0]], verts[0] );
		VectorCopy( vert[index[1]], verts[1] );
		VectorCopy( vert[index[2]], verts[2] );

		// calculate two mostly perpendicular edge directions
		VectorSubtract( verts[0], verts[1], dir1 );
		VectorSubtract( verts[2], verts[1], dir2 );

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the surface normal
		CrossProduct( dir1, dir2, snorm );

		// greater than 60 degrees
		// we multiply 0.5 by length of snorm to avoid normalizing
		if( DotProduct( normal, snorm ) < 0.5 * VectorLength( snorm ) )
			continue;

		R_ClipTriangle( verts[0], verts[1], verts[2], fr );

		if( fr->numverts ) {
			if( numFragmentVerts == maxFragmentVerts ||	++numClippedFragments == maxClippedFragments )
				return;
			(++fr)->numverts = 0;
		}
	}
}

/*
=================
R_RecursiveFragmentNode
=================
*/
void R_RecursiveFragmentNode( mnode_t *node, vec3_t origin, float radius, vec3_t normal )
{
	int				c;
	float			dist;
	mleaf_t			*leaf;
	msurface_t		*surf, **mark;

	while( node->plane != NULL ) {
		if( numFragmentVerts == maxFragmentVerts ||	numClippedFragments == maxClippedFragments )
			return;		// already reached the limit somewhere else

		dist = PlaneDiff( origin, node->plane );
		if( dist > radius ) {
			node = node->children[0];
			continue;
		}

		if( dist >= -radius )
			R_RecursiveFragmentNode( node->children[0], origin, radius, normal );
		node = node->children[1];
	}

	leaf = (mleaf_t *)node;
	if( !( c = leaf->nummarksurfaces ) )
		return;

	mark = leaf->firstmarksurface;
	do {
		if( numFragmentVerts == maxFragmentVerts ||	numClippedFragments == maxClippedFragments )
			return;

		surf = *mark++;
		if( surf->fragmentframe == fragmentFrame )
			continue;

		surf->fragmentframe = fragmentFrame;
		if( surf->shaderref->flags & (SURF_NOMARKS|SURF_NOIMPACT|SURF_NODRAW) )
			continue;

		if( surf->facetype == FACETYPE_PLANAR ) {
			if( surf->shaderref->contents & CONTENTS_SOLID )
				R_PlanarSurfClipFragment( surf, normal );
		} else if( surf->facetype == FACETYPE_PATCH ) {
			R_PatchSurfClipFragment( surf, normal );
		}
	} while( --c );
}

/*
=================
R_GetClippedFragments
=================
*/
int R_GetClippedFragments( vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments )
{
	int		i;
	float	d;

	fragmentFrame++;

	// initialize fragments
	numFragmentVerts = 0;
	maxFragmentVerts = maxfverts;
	fragmentVerts = fverts;

	numClippedFragments = 0;
	maxClippedFragments = maxfragments;
	clippedFragments = fragments;

	// calculate clipping planes
	for( i = 0; i < 3; i++ ) {
		d = DotProduct( origin, axis[i] );

		VectorCopy( axis[i], fragmentPlanes[i*2].normal );
		fragmentPlanes[i*2].dist = d - radius;
		fragmentPlanes[i*2].type = PlaneTypeForNormal( fragmentPlanes[i*2].normal );

		VectorNegate( axis[i], fragmentPlanes[i*2+1].normal );
		fragmentPlanes[i*2+1].dist = -d - radius;
		fragmentPlanes[i*2+1].type = PlaneTypeForNormal( fragmentPlanes[i*2+1].normal );
	}

	R_RecursiveFragmentNode( r_worldmodel->nodes, origin, radius, axis[0] );

	return numClippedFragments;
}
