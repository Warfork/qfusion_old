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
static	mesh_t			poly_mesh;

/*
=================
R_PolyOverflow
=================
*/
qboolean R_PolyOverflow( const meshbuffer_t *mb )
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
void R_PushPoly( const meshbuffer_t *mb )
{
	poly_t *p;

	p = &r_polys[-mb->infokey-1];
	poly_mesh.numVertexes = p->numverts;
	poly_mesh.xyzArray = p->verts;
	poly_mesh.stArray = p->stcoords;
	poly_mesh.colorsArray[0] = p->colors;

	R_PushMesh( &poly_mesh, mb->shader->features | MF_TRIFAN );
}

/*
=================
R_DrawPoly
=================
*/
void R_DrawPoly( const meshbuffer_t *mb ) {
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
R_WindingClipFragment

This function operates on windings (convex polygons without 
any points inside) like triangles, quads, etc. The output is 
a convex fragment (polygon, trifan) which the result of clipping 
the input winding by six fragment planes.
=================
*/
static void R_WindingClipFragment( vec3_t *wVerts, int numVerts, fragment_t *fr )
{
	int			i, j;
	int			stage, newc, numv;
	cplane_t	*plane;
	qboolean	front;
	float		*v, *nextv, d;
	float		dists[MAX_FRAGMENT_VERTS+1];
	int			sides[MAX_FRAGMENT_VERTS+1];
	vec3_t		*verts, *newverts, newv[2][MAX_FRAGMENT_VERTS];

	numv = numVerts;
	verts = wVerts;

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
			nextv = ( i == numv - 1 ) ? verts[0] : v + 3;
			for( j = 0; j < 3; j++ )
				newverts[newc][j] = v[j] + d * (nextv[j] - v[j]);

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

NOTE: one might want to combine this function with 
R_WindingClipFragment for special cases like trifans (q1 and
q2 polys) or tristrips for ultra-fast clipping, providing there's 
enough stack space (depending on MAX_FRAGMENT_VERTS value).
=================
*/
static void R_PlanarSurfClipFragment( msurface_t *surf, vec3_t normal )
{
	int			i;
	mesh_t		*mesh;
	index_t		*index;
	vec3_t		*verts, tri[3];
	fragment_t	*fr;

	if( DotProduct( normal, surf->origin ) < 0.5 )
		return;		// greater than 60 degrees

	mesh = surf->mesh;
	fr = &clippedFragments[numClippedFragments];
	fr->numverts = 0;

	// clip each triangle individually
	index = mesh->indexes;
	verts = mesh->xyzArray;
	for( i = 0; i < mesh->numIndexes; i += 3, index += 3 ) {
		VectorCopy( verts[index[0]], tri[0] );
		VectorCopy( verts[index[1]], tri[1] );
		VectorCopy( verts[index[2]], tri[2] );

		R_WindingClipFragment( tri, 3, fr );

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
static void R_PatchSurfClipFragment( msurface_t *surf, vec3_t normal )
{
	int			i;
	mesh_t		*mesh;
	index_t		*index;
	vec3_t		*verts, tri[3];
	vec3_t		dir1, dir2, snorm;
	fragment_t	*fr;

	mesh = surf->mesh;
	fr = &clippedFragments[numClippedFragments];
	fr->numverts = 0;

	// clip each triangle individually
	index = mesh->indexes;
	verts = mesh->xyzArray;
	for( i = 0; i < mesh->numIndexes; i += 3, index += 3 ) {
		VectorCopy( verts[index[0]], tri[0] );
		VectorCopy( verts[index[1]], tri[1] );
		VectorCopy( verts[index[2]], tri[2] );

		// calculate two mostly perpendicular edge directions
		VectorSubtract( tri[0], tri[1], dir1 );
		VectorSubtract( tri[2], tri[1], dir2 );

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the triangle normal
		CrossProduct( dir1, dir2, snorm );

		// we multiply 0.5 by length of snorm to avoid normalizing
		if( DotProduct( normal, snorm ) < 0.5 * VectorLength( snorm ) )
			continue;	// greater than 60 degrees

		R_WindingClipFragment( tri, 3, fr );

		if( fr->numverts ) {
			if( numFragmentVerts == maxFragmentVerts ||	++numClippedFragments == maxClippedFragments )
				return;
			(++fr)->numverts = 0;
		}
	}
}

/*
=================
R_SurfPotentiallyFragmented
=================
*/
qboolean R_SurfPotentiallyFragmented( msurface_t *surf )
{
	if( surf->shaderref->flags & (SURF_NOMARKS|SURF_NOIMPACT|SURF_NODRAW) )
		return qfalse;

	return ( ((surf->facetype == FACETYPE_PLANAR) && (surf->shaderref->contents & CONTENTS_SOLID)) || 
		(surf->facetype == FACETYPE_PATCH) );
}

/*
=================
R_FragmentNode
=================
*/
static void R_FragmentNode( vec3_t origin, float radius, vec3_t normal )
{
	int				stackdepth = 0;
	float			dist;
	mnode_t			*node, *localstack[2048];
	mleaf_t			*leaf;
	msurface_t		*surf, **mark;

	for( node = r_worldmodel->nodes, stackdepth = 0; ; ) {
		if( node->plane == NULL ) {
			leaf = ( mleaf_t * )node;
			if( !leaf->firstFragmentSurface )
				goto nextNodeOnStack;

			mark = leaf->firstFragmentSurface;
			do {
				if( numFragmentVerts == maxFragmentVerts ||	numClippedFragments == maxClippedFragments )
					return;		// already reached the limit

				surf = *mark++;
				if( surf->fragmentframe == fragmentFrame )
					continue;

				surf->fragmentframe = fragmentFrame;
				if( surf->facetype == FACETYPE_PLANAR )
					R_PlanarSurfClipFragment( surf, normal );
				else
					R_PatchSurfClipFragment( surf, normal );
			} while( *mark );

			if( numFragmentVerts == maxFragmentVerts ||	numClippedFragments == maxClippedFragments )
				return;			// already reached the limit

nextNodeOnStack:
			if( !stackdepth )
				break;
			node = localstack[--stackdepth];
			continue;
		}

		dist = PlaneDiff( origin, node->plane );
		if( dist > radius ) {
			node = node->children[0];
			continue;
		}

		if( (dist >= -radius) && (stackdepth < sizeof(localstack)/sizeof(mnode_t *)) )
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}
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

	R_FragmentNode( origin, radius, axis[0] );

	return numClippedFragments;
}
