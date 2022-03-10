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
R_PushPoly
=================
*/
void R_PushPoly( const meshbuffer_t *mb )
{
	int i;
	poly_t *p;
	shader_t *shader;
	int features;

	MB_NUM2SHADER( mb->shaderkey, shader );

	features = shader->features | MF_TRIFAN;
	for( i = -mb->infokey-1, p = r_polys + i; i < mb->lastPoly; i++, p++ ) {
		poly_mesh.numVertexes = p->numverts;
		poly_mesh.xyzArray = p->verts;
		poly_mesh.stArray = p->stcoords;
		poly_mesh.colorsArray[0] = p->colors;
		R_PushMesh( &poly_mesh, features );
	}
}


/*
=================
R_AddPolysToList
=================
*/
void R_AddPolysToList( void )
{
	int i, nverts, fognum;
	poly_t *p;
	mfog_t *fog, *lastFog = NULL;
	meshbuffer_t *mb = NULL;
	shader_t *shader;

	ri.currententity = r_worldent;
	for( i = 0, p = r_polys; i < r_numPolys; nverts += p->numverts, mb->lastPoly++, i++, p++ ) {
		shader = p->shader;
		if( p->fognum < 0 )
			fognum = -1;
		else if( p->fognum )
			fognum = bound( 1, p->fognum, r_worldmodel->numfogs + 1 );
		else
			fognum = r_worldmodel->numfogs ? 0 : -1;

		if( fognum == -1 )
			fog = NULL;
		else if( !fognum )
			fog = R_FogForSphere( p->verts[0], 0 );
		else
			fog = r_worldmodel->fogs + fognum - 1;

		// we ignore SHADER_ENTITY_MERGABLE here because polys are just regular trifans
		if( !mb || mb->shaderkey != shader->sortkey || lastFog != fog || nverts + p->numverts > MAX_ARRAY_VERTS ) {
			nverts = 0;
			lastFog = fog;

			mb = R_AddMeshToList( MB_POLY, fog, shader, -(i+1) );
			mb->lastPoly = i;
		}
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
static float	fragmentDiameterSquared;

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
static qboolean R_WindingClipFragment( vec3_t *wVerts, int numVerts, int fognum )
{
	int			i, j;
	int			stage, newc, numv;
	cplane_t	*plane;
	qboolean	front;
	float		*v, *nextv, d;
	float		dists[MAX_FRAGMENT_VERTS+1];
	int			sides[MAX_FRAGMENT_VERTS+1];
	vec3_t		*verts, *newverts, newv[2][MAX_FRAGMENT_VERTS], t;
	fragment_t	*fr;

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
			return qfalse;

		// clip it
		sides[i] = sides[0];
		dists[i] = dists[0];

		newc = 0;
		newverts = newv[stage & 1];
		for( i = 0, v = verts[0]; i < numv; i++, v += 3 ) {
			switch( sides[i] ) {
				case SIDE_FRONT:
					if( newc == MAX_FRAGMENT_VERTS )
						return qfalse;
					VectorCopy( v, newverts[newc] );
					newc++;
					break;
				case SIDE_BACK:
					break;
				case SIDE_ON:
					if( newc == MAX_FRAGMENT_VERTS )
						return qfalse;
					VectorCopy( v, newverts[newc] );
					newc++;
					break;
			}

			if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
				continue;
			if( newc == MAX_FRAGMENT_VERTS )
				return qfalse;

			d = dists[i] / (dists[i] - dists[i+1]);
			nextv = ( i == numv - 1 ) ? verts[0] : v + 3;
			for( j = 0; j < 3; j++ )
				newverts[newc][j] = v[j] + d * (nextv[j] - v[j]);
			newc++;
		}

		if( newc <= 2 )
			return qfalse;

		// continue with new verts
		numv = newc;
		verts = newverts;
	}

	// fully clipped
	if( numFragmentVerts + numv > maxFragmentVerts )
		return qfalse;

	fr = &clippedFragments[numClippedFragments++];
	fr->numverts = numv;
	fr->firstvert = numFragmentVerts;
	fr->fognum = fognum;
	for( i = 0, v = verts[0], nextv = fragmentVerts[numFragmentVerts]; i < numv; i++, v += 3, nextv += 3 )
		VectorCopy( v, nextv );

	numFragmentVerts += numv;
	if( numFragmentVerts == maxFragmentVerts && numClippedFragments == maxClippedFragments )
		return qtrue;

	// if all of the following is true:
	// a) all clipping planes are perpendicular
	// b) there are 4 in a clipped fragment
	// c) all sides of the fragment are equal (it is a quad)
	// d) all sides are radius*2 +- epsilon (0.001)
	// then it is safe to assume there's only one fragment possible
	// not sure if it's 100% correct, but sounds convincing
	if( numv == 4 ) {
		for( i = 0, v = verts[0]; i < numv; i++, v += 3 ) {
			nextv = ( i == 3 ) ? verts[0] : v + 3;
			VectorSubtract( v, nextv, t );

			d = fragmentDiameterSquared - DotProduct( t, t );
			if( d > 0.01 || d < -0.01 )
				return qfalse;
		}
		return qtrue;
	}

	return qfalse;
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
static qboolean R_PlanarSurfClipFragment( msurface_t *surf, vec3_t normal )
{
	int			i;
	mesh_t		*mesh;
	index_t		*index;
	vec3_t		*verts, poly[4];
	int			fognum;

	if( DotProduct( normal, surf->plane->normal ) < 0.5 )
		return qfalse;		// greater than 60 degrees

	mesh = surf->mesh;
	index = mesh->indexes;
	verts = mesh->xyzArray;
	fognum = surf->fog ? surf->fog - r_worldmodel->fogs + 1 : -1;

	// clip each triangle individually
	for( i = 0; i < mesh->numIndexes; i += 3, index += 3 ) {
		VectorCopy( verts[index[0]], poly[0] );
		VectorCopy( verts[index[1]], poly[1] );
		VectorCopy( verts[index[2]], poly[2] );

		if( R_WindingClipFragment( poly, 3, fognum ) )
			return qtrue;
	}

	return qfalse;
}

/*
=================
R_PatchSurfClipFragment
=================
*/
static qboolean R_PatchSurfClipFragment( msurface_t *surf, vec3_t normal )
{
	int			i, j;
	mesh_t		*mesh;
	index_t		*index;
	vec3_t		*verts, poly[3];
	vec3_t		dir1, dir2, snorm;
	int			fognum;

	mesh = surf->mesh;
	index = mesh->indexes;
	verts = mesh->xyzArray;
	fognum = surf->fog ? surf->fog - r_worldmodel->fogs + 1 : -1;

	// clip each triangle individually
	for( i = j = 0; i < mesh->numIndexes; i += 6, index += 6, j = 0 ) {
		VectorCopy( verts[index[1]], poly[1] );

		if( !j ) 
		{
			VectorCopy( verts[index[0]], poly[0] );
			VectorCopy( verts[index[2]], poly[2] );
		} 
		else 
		{
tri2:
			j++;
			VectorCopy( poly[2], poly[0] );
			VectorCopy( verts[index[5]], poly[2] );
		}

		// calculate two mostly perpendicular edge directions
		VectorSubtract( poly[0], poly[1], dir1 );
		VectorSubtract( poly[2], poly[1], dir2 );

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the triangle normal
		CrossProduct( dir1, dir2, snorm );

		// we multiply 0.5 by length of snorm to avoid normalizing
		if( DotProduct( normal, snorm ) < 0.5 * VectorLength( snorm ) )
			continue;	// greater than 60 degrees

		if( R_WindingClipFragment( poly, 3, fognum ) )
			return qtrue;

		if( !j )
			goto tri2;
	}

	return qfalse;
}

/*
=================
R_SurfPotentiallyFragmented
=================
*/
qboolean R_SurfPotentiallyFragmented( msurface_t *surf )
{
	if( surf->flags & (SURF_NOMARKS|SURF_NOIMPACT|SURF_NODRAW) )
		return qfalse;
	return ( (surf->facetype == FACETYPE_PLANAR) || (surf->facetype == FACETYPE_PATCH) );
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
	qboolean		inside;
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
					inside = R_PlanarSurfClipFragment( surf, normal );
				else
					inside = R_PatchSurfClipFragment( surf, normal );

				if( inside )
					return;
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

	fragmentDiameterSquared = radius*radius*4;

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
