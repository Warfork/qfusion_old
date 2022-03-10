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
// cmodel.c -- model loading

#include "qcommon.h"
#include "cm_local.h"

int				checkcount;

mempool_t		*cmap_mempool = NULL;

static bspFormatDesc_t *cmap_bspFormat;

char			map_name[MAX_QPATH];
qboolean		map_clientload;

int				numbrushsides;
cbrushside_t	*map_brushsides;

int				numshaderrefs;
cshaderref_t	*map_shaderrefs;

int				numplanes;
cplane_t		*map_planes;

int				numnodes;
cnode_t			*map_nodes;

int				numleafs = 1;
static cleaf_t	map_leaf_empty;		// allow leaf funcs to be called without a map
cleaf_t			*map_leafs = &map_leaf_empty;

int				nummarkbrushes;
cbrush_t		**map_markbrushes;

int				numcmodels;
static cmodel_t	map_cmodel_empty;
cmodel_t		*map_cmodels = &map_cmodel_empty;
vec3_t			world_mins, world_maxs;

int				numbrushes;
cbrush_t		*map_brushes;

int				numfaces;
cface_t			*map_faces;

int				nummarkfaces;
cface_t			**map_markfaces;

vec3_t			*map_verts;			// this will be freed
int				numvertexes;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
int				numareaportals = 1;
careaportal_t	map_areaportals[MAX_CM_AREAPORTALS];

int				numareas = 1;
static carea_t	map_area_empty;
carea_t			*map_areas = &map_area_empty;

dvis_t			*map_pvs, *map_phs;
int				map_visdatasize;

qbyte			nullrow[MAX_CM_LEAFS/8];

int				numentitychars;
static char		map_entitystring_empty;
char			*map_entitystring = &map_entitystring_empty;

int				floodvalid;

cvar_t			*cm_noAreas;
cvar_t			*cm_noCurves;

int				c_pointcontents;
int				c_traces, c_brush_traces;

qbyte			*cmod_base;

/*
===============================================================================

					PATCH LOADING

===============================================================================
*/

/*
=================
CM_CreateFacetFromPoints
=================
*/
static int CM_CreateFacetFromPoints( cbrush_t *facet, vec3_t *verts, int numverts, cshaderref_t *shaderref )
{
	int				i, j, k;
	int				axis, dir;
	cbrushside_t	*s;
	cplane_t		*planes;
	vec3_t			normal, mins, maxs;
	float			d, dist;
	cplane_t		mainplane;
	vec3_t			vec, vec2;
	int				numbrushplanes;
	cplane_t		brushplanes[32];

	// set default values for brush
	facet->numsides = 0;
	facet->brushsides = NULL;
	facet->contents = shaderref->contents;

	// calculate plane for this triangle
	PlaneFromPoints( verts, &mainplane );
	if( ComparePlanes( mainplane.normal, mainplane.dist, vec3_origin, 0 ) )
		return 0;

	// test a quad case
	if( numverts > 3 ) {
		vec3_t v[3];
		cplane_t plane;

		d = DotProduct( verts[3], mainplane.normal ) - mainplane.dist;
		if( d < -0.1 || d > 0.1 )
			return 0;

		// try different combinations of planes
		for( i = 1; i < 4; i++ ) {
			VectorCopy( verts[i], v[0] );
			VectorCopy( verts[(i+1)%4], v[1] );
			VectorCopy( verts[(i+2)%4], v[2] );
			PlaneFromPoints ( verts, &plane );

			if( DotProduct( mainplane.normal, plane.normal ) < 0.9 )
				return 0;
		}
	}

	numbrushplanes = 0;

	// add front plane
	SnapPlane ( mainplane.normal, &mainplane.dist );
	VectorCopy( mainplane.normal, brushplanes[numbrushplanes].normal );
	brushplanes[numbrushplanes].dist = mainplane.dist; numbrushplanes++;

	// calculate mins & maxs
	ClearBounds( mins, maxs );
	for( i = 0; i < numverts; i++ )
		AddPointToBounds( verts[i], mins, maxs );

	// add the axial planes
	for( axis = 0; axis < 3; axis++ ) {
		for( dir = -1; dir <= 1; dir += 2 ) {
			for( i = 0; i < numbrushplanes; i++ ) {
				if( brushplanes[i].normal[axis] == dir )
					break;
			}

			if( i == numbrushplanes ) {
				VectorClear( normal );
				normal[axis] = dir;
				if (dir == 1)
					dist = maxs[axis];
				else
					dist = -mins[axis];

				VectorCopy( normal, brushplanes[numbrushplanes].normal );
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
			}
		}
	}

	// add the edge bevels
	for( i = 0; i < numverts; i++ ) {
		j = (i + 1) % numverts;
		k = (i + 2) % numverts;

		VectorSubtract( verts[i], verts[j], vec );
		if( VectorNormalize( vec ) < 0.5 )
			continue;

		SnapVector( vec );
		for( j = 0; j < 3; j++ ) {
			if( vec[j] == 1 || vec[j] == -1 )
				break;	// axial
		}
		if( j != 3 )
			continue;	// only test non-axial edges

		// try the six possible slanted axials from this edge
		for( axis = 0; axis < 3; axis++ ) {
			for( dir = -1; dir <= 1; dir += 2 ) {
				// construct a plane
				VectorClear( vec2 );
				vec2[axis] = dir;
				CrossProduct( vec, vec2, normal );
				if( VectorNormalize( normal ) < 0.5 )
					continue;
				dist = DotProduct( verts[i], normal );

				for( j = 0; j < numbrushplanes; j++ ) {
					// if this plane has already been used, skip it
					if( ComparePlanes( brushplanes[j].normal, brushplanes[j].dist, normal, dist ) )
						break;
				}
				if( j != numbrushplanes )
					continue;

				// if all other points are behind this plane, it is a proper edge bevel
				for( j = 0; j < numverts; j++ ) {
					if( j != i ) {
						d = DotProduct( verts[j], normal ) - dist;
						if( d > 0.1 )
							break;	// point in front: this plane isn't part of the outer hull
					}
				}
				if( j != numverts )
					continue;

				// add this plane
				VectorCopy( normal, brushplanes[numbrushplanes].normal );
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
			}
		}
	}

	if( !numbrushplanes )
		return 0;

	// add brushsides
	s = facet->brushsides = Mem_Alloc( cmap_mempool, numbrushplanes * sizeof (*s) );
	planes = Mem_Alloc( cmap_mempool, numbrushplanes * sizeof(cplane_t) );
	for( i = 0; i < numbrushplanes; i++, s++ ) {
		planes[i] = brushplanes[i];
		SnapPlane( planes[i].normal, &planes[i].dist );
		CategorizePlane( &planes[i] );

		s->plane = &planes[i];
		s->surfFlags = shaderref->flags;
	}

	return( facet->numsides = numbrushplanes );
}

/*
=================
CM_CreatePatch
=================
*/
static void CM_CreatePatch( cface_t *patch, cshaderref_t *shaderref, vec3_t *verts, int *patch_cp )
{
	int step[2], size[2], flat[2], i, u, v;
	cbrush_t *facets;
	vec3_t *points;
	vec3_t tverts[4];

	// find the degree of subdivision in the u and v directions
	Patch_GetFlatness( CM_SUBDIV_LEVEL, verts, patch_cp, flat );

	step[0] = 1 << flat[0];
	step[1] = 1 << flat[1];
	size[0] = ( patch_cp[0] >> 1 ) * step[0] + 1;
	size[1] = ( patch_cp[1] >> 1 ) * step[1] + 1;
	if( size[0] <= 0 || size[1] <= 0 )
		return;

	points = Mem_Alloc( cmap_mempool, size[0] * size[1] * sizeof (vec3_t) );
	facets = Mem_Alloc( cmap_mempool, (size[0]-1) * (size[1]-1) * 2 * sizeof (cbrush_t) );

	// fill in
	Patch_Evaluate( verts[0], patch_cp, step, points[0], 3 );

	patch->numfacets = 0;
	patch->facets = NULL;
	ClearBounds ( patch->mins, patch->maxs );

	// create a set of facets
    for( v = 0; v < size[1]-1; v++ ) {
		for( u = 0; u < size[0]-1; u++ ) {
			i = v * size[0] + u;
			VectorCopy( points[i], tverts[0] );
			VectorCopy( points[i + size[0]], tverts[1] );
			VectorCopy( points[i + size[0] + 1], tverts[2] );
			VectorCopy( points[i + 1], tverts[3] );

			for( i = 0; i < 4; i++ )
				AddPointToBounds( tverts[i], patch->mins, patch->maxs );

			// try to create one facet from a quad
			if( CM_CreateFacetFromPoints( &facets[patch->numfacets], tverts, 4, shaderref ) ) {
				patch->numfacets++;
				continue;
			}

			VectorCopy( tverts[3], tverts[2] );

			// create two facets from triangles
			if( CM_CreateFacetFromPoints( &facets[patch->numfacets], tverts, 3, shaderref ) )
				patch->numfacets++;

			VectorCopy( tverts[2], tverts[0] );
			VectorCopy( points[v * size[0] + u + size[0] + 1], tverts[2] );

			if( CM_CreateFacetFromPoints( &facets[patch->numfacets], tverts, 3, shaderref ) )
				patch->numfacets++;
		}
    }

	if( !patch->numfacets ) {
		ClearBounds( patch->mins, patch->maxs );
		Mem_Free( points );
		Mem_Free( facets );
		return;
	}

	patch->contents = shaderref->contents;
	patch->facets = Mem_Alloc( cmap_mempool, patch->numfacets * sizeof(cbrush_t) );
	memcpy( patch->facets, facets, patch->numfacets * sizeof(cbrush_t) );

	Mem_Free( points );
	Mem_Free( facets );

	for( i = 0; i < 3; i++ ) {
		// spread the mins / maxs by a pixel
		patch->mins[i] -= 1;
		patch->maxs[i] += 1;
	}
}


/*
===============================================================================

					MAP LOADING

===============================================================================
*/

/*
=================
CMod_LoadSurfaces
=================
*/
static void CMod_LoadSurfaces( lump_t *l )
{
	int				i;
	int				count;
	dshaderref_t	*in;
	cshaderref_t	*out;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadSurfaces: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "CMod_LoadSurfaces: map with no shaders" );

	out = map_shaderrefs = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numshaderrefs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->flags = LittleLong( in->flags );
		out->contents = LittleLong( in->contents );
	}
}

/*
=================
CMod_LoadVertexes
=================
*/
static void CMod_LoadVertexes( lump_t *l )
{
	int			i;
	int			count;
	dvertex_t	*in;
	vec3_t		*out;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMOD_LoadVertexes: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no vertexes" );

	out = map_verts = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numvertexes = count;

	for( i = 0; i < count; i++, in++ ) {
		out[i][0] = LittleFloat( in->point[0] );
		out[i][1] = LittleFloat( in->point[1] );
		out[i][2] = LittleFloat( in->point[2] );
	}
}

/*
=================
CMod_LoadVertexes_RBSP
=================
*/
static void CMod_LoadVertexes_RBSP( lump_t *l )
{
	int			i;
	int			count;
	rdvertex_t	*in;
	vec3_t		*out;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadVertexes_RBSP: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no vertexes" );

	out = map_verts = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numvertexes = count;

	for( i = 0; i < count; i++, in++ ) {
		out[i][0] = LittleFloat( in->point[0] );
		out[i][1] = LittleFloat( in->point[1] );
		out[i][2] = LittleFloat( in->point[2] );
	}
}

/*
=================
CMod_LoadFace
=================
*/
static inline void CMod_LoadFace( cface_t *out, int shadernum, int firstvert, int numverts, int *patch_cp )
{
	cshaderref_t	*shaderref;

	shadernum = LittleLong( shadernum );
	if( shadernum < 0 || shadernum >= numshaderrefs )
		return;

	shaderref = &map_shaderrefs[shadernum];
	if( !shaderref->contents || (shaderref->flags & SURF_NONSOLID) )
		return;

	patch_cp[0] = LittleLong( patch_cp[0] );
	patch_cp[1] = LittleLong( patch_cp[1] );
	if( patch_cp[0] <= 0 || patch_cp[1] <= 0 )
		return;

	firstvert = LittleLong( firstvert );
	if( numverts <= 0 || firstvert < 0 || firstvert >= numvertexes )
		return;

	CM_CreatePatch( out, shaderref, map_verts + firstvert, patch_cp );
}

/*
=================
CMod_LoadFaces
=================
*/
static void CMod_LoadFaces( lump_t *l )
{
	int				i, count;
	dface_t			*in;
	cface_t			*out;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadFaces: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no faces" );

	out = map_faces = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numfaces = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->contents = 0;
		out->numfacets = 0;
		out->facets = NULL;
		if( LittleLong( in->facetype ) != FACETYPE_PATCH )
			continue;
		CMod_LoadFace( out, in->shadernum, in->firstvert, in->numverts, in->patch_cp );
	}
}

/*
=================
CMod_LoadFaces_RBSP
=================
*/
static void CMod_LoadFaces_RBSP( lump_t *l )
{
	int				i, count;
	rdface_t		*in;
	cface_t			*out;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadFaces_RBSP: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no faces" );

	out = map_faces = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numfaces = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->contents = 0;
		out->numfacets = 0;
		out->facets = NULL;
		if( LittleLong( in->facetype ) != FACETYPE_PATCH )
			continue;
		CMod_LoadFace( out, in->shadernum, in->firstvert, in->numverts, in->patch_cp );
	}
}

/*
=================
CMod_LoadSubmodels
=================
*/
static void CMod_LoadSubmodels( lump_t *l )
{
	int			i, j;
	int			count;
	dmodel_t	*in;
	cmodel_t	*out;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadSubmodels: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no models" );

	out = map_cmodels = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numcmodels = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->nummarkfaces = LittleLong( in->numfaces );
		out->markfaces = Mem_Alloc( cmap_mempool, out->nummarkfaces * sizeof( cface_t * ) );
		out->nummarkbrushes = LittleLong( in->numbrushes );
		out->markbrushes = Mem_Alloc( cmap_mempool, out->nummarkbrushes * sizeof( cbrush_t * ) );

		for( j = 0; j < out->nummarkfaces; j++ )
			out->markfaces[j] = map_faces + LittleLong( in->firstface ) + j;
		for( j = 0; j < out->nummarkbrushes; j++ )
			out->markbrushes[j] = map_brushes + LittleLong( in->firstbrush ) + j;

		for( j = 0; j < 3; j++ ) {
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
		}
	}
}

/*
=================
CMod_LoadNodes
=================
*/
static void CMod_LoadNodes( lump_t *l )
{
	int			i;
	int			count;
	dnode_t		*in;
	cnode_t		*out;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadNodes: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map has no nodes" );

	out = map_nodes = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numnodes = count;

	for( i = 0; i < 3; i++ ) {
		world_mins[i] = LittleFloat( in->mins[i] );
		world_maxs[i] = LittleFloat( in->maxs[i] );
	}

	for( i = 0; i < count; i++, out++, in++ ) {
		out->plane = map_planes + LittleLong( in->planenum );
		out->children[0] = LittleLong( in->children[0] );
		out->children[1] = LittleLong( in->children[1] );
	}
}

/*
=================
CMod_LoadMarkFaces
=================
*/
static void CMod_LoadMarkFaces( lump_t *l )
{
	int			i, j;
	int			count;
	cface_t		**out;
	int			*in;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadMarkFaces: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no leaffaces" );

	map_markfaces = out = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	nummarkfaces = count;

	for( i = 0; i < count; i++ ) {
		j = LittleLong( in[i] );
		if( j < 0 ||  j >= numfaces )
			Com_Error( ERR_DROP, "CMod_LoadMarkFaces: bad surface number" );
		out[i] = map_faces + j;
	}
}

/*
=================
CMod_LoadLeafs
=================
*/
static void CMod_LoadLeafs( lump_t *l )
{
	int			i, j, k;
	int			count;
	cleaf_t		*out;
	dleaf_t 	*in;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadLeafs: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no leafs" );

	out = map_leafs = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numleafs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->contents = 0;
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area ) + 1;
		out->markbrushes = map_markbrushes + LittleLong( in->firstleafbrush );
		out->nummarkbrushes = LittleLong( in->numleafbrushes );
		out->markfaces = map_markfaces + LittleLong( in->firstleafface );
		out->nummarkfaces = LittleLong( in->numleaffaces );

		// OR brushes' contents
		for( j = 0; j < out->nummarkbrushes; j++ )
			out->contents |= out->markbrushes[j]->contents;

		// exclude markfaces that have no facets
		// so we don't perform this check at runtime
		for( j = 0; j < out->nummarkfaces; ) {
			k = j;
			if( !out->markfaces[j]->facets ) {
				for( ; (++j < out->nummarkfaces) && !out->markfaces[j]->facets; );
				if( j < out->nummarkfaces )
					memmove( &out->markfaces[k], &out->markfaces[j], (out->nummarkfaces - j) * sizeof( *out->markfaces ) );
				out->nummarkfaces -= j - k;

			}
			j = k + 1;
		}

		// OR patches' contents
		for( j = 0; j < out->nummarkfaces; j++ )
			out->contents |= out->markfaces[j]->contents;

		if( out->area >= numareas )
			numareas = out->area + 1;
	}

	map_areas = Mem_Alloc( cmap_mempool, numareas * sizeof( *map_areas ) );
}

/*
=================
CMod_LoadPlanes
=================
*/
static void CMod_LoadPlanes( lump_t *l )
{
	int			i, j;
	int			count;
	cplane_t	*out;
	dplane_t 	*in;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadPlanes: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no planes" );

	out = map_planes = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numplanes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->signbits = 0;
		out->type = PLANE_NONAXIAL;

		for( j = 0; j < 3; j++ ) {
			out->normal[j] = LittleFloat( in->normal[j] );
			if( out->normal[j] < 0 )
				out->signbits |= (1 << j);
			if( out->normal[j] == 1.0f )
				out->type = j;
		}

		out->dist = LittleFloat( in->dist );
	}
}

/*
=================
CMod_LoadMarkBrushes
=================
*/
static void CMod_LoadMarkBrushes( lump_t *l )
{
	int			i;
	int			count;
	cbrush_t	**out;
	int		 	*in;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadMarkBrushes: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no leafbrushes" );

	out = map_markbrushes = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	nummarkbrushes = count;

	for( i = 0; i < count; i++, in++ )
		out[i] = map_brushes + LittleLong( *in );
}

/*
=================
CMod_LoadBrushSides
=================
*/
static void CMod_LoadBrushSides( lump_t *l )
{
	int				i, j;
	int				count;
	cbrushside_t	*out;
	dbrushside_t 	*in;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadBrushSides: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no brushsides" );

	out = map_brushsides = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numbrushsides = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->plane = map_planes + LittleLong( in->planenum );
		j = LittleLong( in->shadernum );
		if( j >= numshaderrefs )
			Com_Error( ERR_DROP, "Bad brushside texinfo" );
		out->surfFlags = map_shaderrefs[j].flags;
	}
}

/*
=================
CMod_LoadBrushSides_RBSP
=================
*/
static void CMod_LoadBrushSides_RBSP( lump_t *l )
{
	int				i, j;
	int				count;
	cbrushside_t	*out;
	rdbrushside_t 	*in;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadBrushSides_RBSP: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no brushsides" );

	out = map_brushsides = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numbrushsides = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->plane = map_planes + LittleLong( in->planenum );
		j = LittleLong( in->shadernum );
		if( j >= numshaderrefs )
			Com_Error( ERR_DROP, "Bad brushside texinfo" );
		out->surfFlags = map_shaderrefs[j].flags;
	}
}

/*
=================
CMod_LoadBrushes
=================
*/
static void CMod_LoadBrushes( lump_t *l )
{
	int			i;
	int			count;
	dbrush_t	*in;
	cbrush_t	*out;
	int			shaderref;

	in = ( void * )(cmod_base + l->fileofs);
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "CMod_LoadBrushes: funny lump size" );
	count = l->filelen / sizeof( *in );
	if( count < 1 )
		Com_Error( ERR_DROP, "Map with no brushes" );

	out = map_brushes = Mem_Alloc( cmap_mempool, count * sizeof( *out ) );
	numbrushes = count;

	for( i = 0; i < count; i++, out++, in++ ) {
		shaderref = LittleLong( in->shadernum );
		out->contents = map_shaderrefs[shaderref].contents;
		out->numsides = LittleLong( in->numsides );
		out->brushsides = map_brushsides + LittleLong( in->firstside );
	}
}

/*
=================
CMod_LoadVisibility
=================
*/
static void CMod_LoadVisibility( lump_t *l )
{
	map_visdatasize = l->filelen;
	if( !map_visdatasize ) {
		map_pvs = NULL;
		return;
	}

	map_pvs = Mem_Alloc( cmap_mempool, map_visdatasize );
	memcpy( map_pvs, cmod_base + l->fileofs, map_visdatasize );

	map_pvs->numclusters = LittleLong( map_pvs->numclusters );
	map_pvs->rowsize = LittleLong( map_pvs->rowsize );
}

/*
=================
CMod_LoadEntityString
=================
*/
static void CMod_LoadEntityString( lump_t *l )
{
	numentitychars = l->filelen;
	if( !l->filelen )
		return;

	map_entitystring = Mem_Alloc( cmap_mempool, numentitychars );
	memcpy( map_entitystring, cmod_base + l->fileofs, l->filelen );
}

/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
cmodel_t *CM_LoadMap( char *name, qboolean clientload, unsigned *checksum )
{
	unsigned		*buf;
	int				i;
	dheader_t		header;
	int				length, version;
	static unsigned	last_checksum;

	map_clientload = clientload;

	if( !strcmp( map_name, name ) && ( clientload || !Cvar_VariableValue ("flushmap")) ) {
		*checksum = last_checksum;
		if( !clientload ) {
			memset( map_areaportals, 0, sizeof( map_areaportals ) );
			CM_FloodAreaConnections ();
		}
		return map_cmodels;		// still have the right version
	}

	// free old stuff
	if( !cmap_mempool )
		cmap_mempool = Mem_AllocPool( NULL, "Collision Map" );
	else
		Mem_EmptyPool( cmap_mempool );

	numplanes = 0;
	numnodes = 0;
	numleafs = 0;
	numbrushes = 0;
	numbrushsides = 0;
	numcmodels = 0;
	numentitychars = 0;
	numvertexes = 0;
	numfaces = 0;
	nummarkfaces = 0;
	nummarkbrushes = 0;
	numareas = 1;
	numareaportals = 1;
	map_name[0] = 0;

	map_pvs = map_phs = NULL;
	map_leafs = &map_leaf_empty;
	map_cmodels = &map_cmodel_empty;
	map_areas = &map_area_empty;
	map_entitystring = &map_entitystring_empty;

	ClearBounds( world_mins, world_maxs );

	if( !name || !name[0] ) {
		numleafs = 1;
		numcmodels = 2;
		*checksum = 0;
		return map_cmodels;			// cinematic servers won't have anything at all
	}

	//
	// load the file
	//
	length = FS_LoadFile( name, ( void ** )&buf, NULL, 0 );
	if( !buf )
		Com_Error( ERR_DROP, "Couldn't load %s", name );

	last_checksum = LittleLong( Com_BlockChecksum( buf, length ) );
	*checksum = last_checksum;

	header = *(dheader_t *)buf;
	version = LittleLong( header.version );
	for( i = 0, cmap_bspFormat = bspFormats; i < numBspFormats; i++, cmap_bspFormat++ ) {
		if( !strncmp( (char *)buf, cmap_bspFormat->header, 4 ) && (version == cmap_bspFormat->version) )
			break;
	}
	if( i == numBspFormats )
		Com_Error( ERR_DROP, "CM_LoadMap: %s: unknown bsp format, version %i", name, version );

	for( i = 0; i < sizeof(dheader_t) / 4; i++ )
		((int *)&header)[i] = LittleLong( ((int *)&header)[i]);
	cmod_base = ( qbyte * )buf;

	// load into heap
	CMod_LoadSurfaces( &header.lumps[LUMP_SHADERREFS] );
	CMod_LoadPlanes( &header.lumps[LUMP_PLANES] );
	if( cmap_bspFormat->flags & BSP_RAVEN )
		CMod_LoadBrushSides_RBSP( &header.lumps[LUMP_BRUSHSIDES] );
	else
		CMod_LoadBrushSides( &header.lumps[LUMP_BRUSHSIDES] );
	CMod_LoadBrushes( &header.lumps[LUMP_BRUSHES] );
	CMod_LoadMarkBrushes( &header.lumps[LUMP_LEAFBRUSHES] );
	if( cmap_bspFormat->flags & BSP_RAVEN ) {
		CMod_LoadVertexes_RBSP( &header.lumps[LUMP_VERTEXES] );
		CMod_LoadFaces_RBSP( &header.lumps[LUMP_FACES] );
	} else {
		CMod_LoadVertexes( &header.lumps[LUMP_VERTEXES] );
		CMod_LoadFaces( &header.lumps[LUMP_FACES] );
	}
	CMod_LoadMarkFaces( &header.lumps[LUMP_LEAFFACES] );
	CMod_LoadLeafs( &header.lumps[LUMP_LEAFS] );
	CMod_LoadNodes( &header.lumps[LUMP_NODES] );
	CMod_LoadSubmodels( &header.lumps[LUMP_MODELS] );
	CMod_LoadVisibility( &header.lumps[LUMP_VISIBILITY] );
	CMod_LoadEntityString( &header.lumps[LUMP_ENTITIES] );

	FS_FreeFile( buf );

	CM_InitBoxHull ();

	memset( map_areaportals, 0, sizeof( map_areaportals ) );
	CM_FloodAreaConnections ();

	CM_CalcPHS ();

	if( numvertexes )
		Mem_Free( map_verts );

	memset( nullrow, 255, MAX_CM_LEAFS / 8 );

	Q_strncpyz( map_name, name, sizeof(map_name) );

	return map_cmodels;
}

/*
==================
CM_LoadMapMessage
==================
*/
char *CM_LoadMapMessage( char *name, char *message, int size )
{
	int i, file, len;
	char ident[4], *data, *entitystring;
	int version;
	bspFormatDesc_t *format;
	lump_t l;
	qboolean isworld;
	char key[MAX_KEY], value[MAX_VALUE], *token;

	*message = '\0';

	len = FS_FOpenFile( name, &file, FS_READ );
	if( file == -1 )
		return message;

	FS_Read( ident, sizeof( ident ), file );
	FS_Read( &version, sizeof( version ), file );

	version = LittleLong( version );
	for( i = 0, format = bspFormats; i < numBspFormats; i++, format++ ) {
		if( !strncmp( ident, format->header, 4 ) && (version == format->version) )
			break;
	}
	if( i == numBspFormats ) {
		FS_FCloseFile( file );
		Com_Printf( "CM_LoadMapMessage: %s: unknown bsp format, version %i", name, version );
		return message;
	}

	FS_Seek( file, sizeof( lump_t ) * LUMP_ENTITIES, FS_SEEK_CUR );

	FS_Read( &l.fileofs, sizeof( l.fileofs ), file );
	l.fileofs = LittleLong( l.fileofs );

	FS_Read( &l.filelen, sizeof( l.filelen ), file );
	l.filelen = LittleLong( l.filelen );

	if( !l.filelen ) {
		FS_FCloseFile( file );
		return message;
	}

	FS_Seek( file, l.fileofs, FS_SEEK_SET );

	entitystring = Mem_TempMalloc( l.filelen );
	FS_Read( entitystring, l.filelen, file );

	FS_FCloseFile( file );

	for( data = entitystring; (token = COM_Parse( &data )) && token[0] == '{'; ) {
		isworld = qtrue;

		while( 1 ) {
			if( !(token = COM_Parse( &data )) )
				break; // error
			if( token[0] == '}' )
				break; // end of entity

			Q_strncpyz( key, token, sizeof(key) );
			while( key[strlen(key)-1] == ' ' )	// remove trailing spaces
				key[strlen(key)-1] = 0;

			if( !(token = COM_Parse( &data )) )
				break; // error

			Q_strncpyz( value, token, sizeof(value) );

			// now that we have the key pair worked out...
			if( !strcmp( key, "classname" ) ) {
				if( strcmp( value, "worldspawn" ) )
					isworld = qfalse;
			} else if( isworld && !strcmp( key, "message" ) ) {
				Q_strncpyz( message, token, size );
				break;
			}
		}

		if( isworld )
			break;
	}

	Mem_Free( entitystring );

	return message;
}

/*
==================
CM_ClientLoad

FIXME!
==================
*/
qboolean CM_ClientLoad( void ) {
	return map_clientload;
}

/*
==================
CM_InlineModel
==================
*/
cmodel_t *CM_InlineModel( int num )
{
	if( num < 0 || num >= numcmodels )
		Com_Error( ERR_DROP, "CM_InlineModel: bad number %i (%i)", num, numcmodels );
	return &map_cmodels[num];
}

/*
=================
CM_NumInlineModels
=================
*/
int	CM_NumInlineModels( void ) {
	return numcmodels;
}

/*
=================
CM_InlineModelBounds
=================
*/
void CM_InlineModelBounds( cmodel_t *cmodel, vec3_t mins, vec3_t maxs )
{
	if( cmodel == map_cmodels ) {
		VectorCopy( world_mins, mins );
		VectorCopy( world_maxs, maxs );
	} else {
		VectorCopy( cmodel->mins, mins );
		VectorCopy( cmodel->maxs, maxs );
	}
}

/*
=================
CM_EntityString
=================
*/
char *CM_EntityString( void ) {
	return map_entitystring;
}

/*
=================
CM_LeafCluster
=================
*/
int	CM_LeafCluster( int leafnum )
{
	if( leafnum < 0 || leafnum >= numleafs )
		Com_Error( ERR_DROP, "CM_LeafCluster: bad number" );
	return map_leafs[leafnum].cluster;
}

/*
=================
CM_LeafArea
=================
*/
int	CM_LeafArea( int leafnum )
{
	if( leafnum < 0 || leafnum >= numleafs )
		Com_Error( ERR_DROP, "CM_LeafArea: bad number" );
	return map_leafs[leafnum].area;
}

/*
===============================================================================

PVS / PHS

===============================================================================
*/

/*
=================
CM_CalcPHS
=================
*/
void CM_CalcPHS( void )
{
	int			i, j, k, l, index;
	int			rowbytes, rowwords;
	int			bitbyte;
	unsigned int *dest, *src;
	qbyte		*scan;
	int			count, vcount;

	if( !map_pvs ) {
		map_phs = NULL;
		return;
	}

	Com_DPrintf( "Building PHS...\n" );

	map_phs = Mem_Alloc( cmap_mempool, map_visdatasize );
	map_phs->rowsize = map_pvs->rowsize;
	map_phs->numclusters = map_pvs->numclusters;

	rowbytes = map_pvs->rowsize;
	rowwords = rowbytes / sizeof( int );

	vcount = 0;
	for( i = 0; i < map_pvs->numclusters; i++ ) {
		scan = CM_ClusterPVS( i );
		for( j = 0; j < map_pvs->numclusters; j++ ) {
			if( scan[j>>3] & (1<<(j&7)) )
				vcount++;
		}
	}

	count = 0;
	scan = ( qbyte * )map_pvs->data;
	dest = ( unsigned int * )(( qbyte * )map_phs->data);

	for( i = 0; i < map_phs->numclusters; i++, dest += rowwords, scan += rowbytes ) {
		memcpy( dest, scan, rowbytes );

		for( j = 0; j < rowbytes; j++ )	{
			bitbyte = scan[j];
			if( !bitbyte )
				continue;
			for( k = 0; k < 8; k++ ) {
				if( !( bitbyte & (1<<k) ) )
					continue;

				// OR this pvs row into the phs
				index = (j << 3) + k;
				if( index >= map_phs->numclusters )
					Com_Error( ERR_DROP, "CM_CalcPHS: Bad bit in PVS" );	// pad bits should be 0

				src = ( unsigned int * )(( qbyte * )map_pvs->data) + index * rowwords;
				for( l = 0; l < rowwords; l++ )
					dest[l] |= src[l];
			}
		}
		for( j = 0; j < map_phs->numclusters; j++ )
			if( (( qbyte * )dest)[j>>3] & (1<<(j&7)) )
				count++;
	}

	Com_DPrintf( "Average clusters visible / hearable / total: %i / %i / %i\n"
		, vcount/map_phs->numclusters, count/map_phs->numclusters, map_phs->numclusters );
}

/*
=================
CM_ClusterSize
=================
*/
int	CM_ClusterSize( void ) {
	return map_pvs ? map_pvs->rowsize : MAX_CM_LEAFS / 8;
}

/*
=================
CM_NumClusters
=================
*/
int	CM_NumClusters( void ) {
	return map_pvs->numclusters;
}

/*
=================
CM_VisData
=================
*/
dvis_t *CM_VisData( void ) {
	return map_pvs;
}

/*
=================
CM_ClusterPVS
=================
*/
qbyte *CM_ClusterPVS( int cluster )
{
	if( cluster == -1 || !map_pvs )
		return nullrow;
	return ( qbyte * )map_pvs->data + cluster * map_pvs->rowsize;
}

/*
=================
CM_ClusterPHS
=================
*/
qbyte *CM_ClusterPHS( int cluster )
{
	if( cluster == -1 || !map_phs )
		return nullrow;
	return ( qbyte * )map_phs->data + cluster * map_phs->rowsize;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/

/*
=================
CM_AddAreaPortal
=================
*/
qboolean CM_AddAreaPortal( int portalnum, int area, int otherarea )
{
	carea_t *a;
	careaportal_t *ap;

	if( portalnum >= MAX_CM_AREAPORTALS )
		return qfalse;
	if( !area || area > numareas || !otherarea || otherarea > numareas )
		return qfalse;

	ap = &map_areaportals[portalnum];
	ap->area = area;
	ap->otherarea = otherarea;

	a = &map_areas[area];
	a->areaportals[a->numareaportals++] = portalnum;

	a = &map_areas[otherarea];
	a->areaportals[a->numareaportals++] = portalnum;

	numareaportals++;

	return qtrue;
}

/*
=================
CM_FloodArea_r
=================
*/
static void CM_FloodArea_r( int areanum, int floodnum )
{
	int				i;
	carea_t			*area;
	careaportal_t	*p;

	area = &map_areas[areanum];
	if( area->floodvalid == floodvalid ) {
		if( area->floodnum == floodnum )
			return;
		Com_Error( ERR_DROP, "FloodArea_r: reflooded" );
	}

	area->floodnum = floodnum;
	area->floodvalid = floodvalid;
	for( i = 0; i < area->numareaportals; i++ ) {
		p = &map_areaportals[area->areaportals[i]];
		if( !p->open )
			continue;

		if( p->area == areanum )
			CM_FloodArea_r( p->otherarea, floodnum );
		else if( p->otherarea == areanum )
			CM_FloodArea_r( p->area, floodnum );
	}
}

/*
====================
CM_FloodAreaConnections
====================
*/
void CM_FloodAreaConnections( void )
{
	int		i;
	int		floodnum;

	// all current floods are now invalid
	floodvalid++;
	floodnum = 0;

	// area 0 is not used
	for( i = 1; i < numareas ; i++ ) {
		if( map_areas[i].floodvalid == floodvalid )
			continue;		// already flooded into
		floodnum++;
		CM_FloodArea_r( i, floodnum );
	}
}

/*
=================
CM_SetAreaPortalState
=================
*/
void CM_SetAreaPortalState( int portalnum, int area, int otherarea, qboolean open )
{
	if( portalnum >= MAX_CM_AREAPORTALS )
		Com_Error( ERR_DROP, "areaportal >= MAX_CM_AREAPORTALS" );

	if( !map_areaportals[portalnum].area ) {
		// add new areaportal if it doesn't exist
		if( !CM_AddAreaPortal( portalnum, area, otherarea ) )
			return;
	}

	map_areaportals[portalnum].open = open;
	CM_FloodAreaConnections ();
}

/*
=================
CM_AreasConnected
=================
*/
qboolean CM_AreasConnected( int area1, int area2 )
{
	if( cm_noAreas->integer )
		return qtrue;
	if( area1 > numareas || area2 > numareas )
		Com_Error( ERR_DROP, "CM_AreasConnected: area > numareas" );

	if( map_areas[area1].floodnum == map_areas[area2].floodnum )
		return qtrue;
	return qfalse;
}

/*
=================
CM_WriteAreaBits

Writes a length byte followed by a bit vector of all the areas
that area in the same flood as the area parameter

This is used by the client refreshes to cull visibility
=================
*/
int CM_WriteAreaBits( qbyte *buffer, int area )
{
	int		i;
	int		bytes;

	bytes = (numareas + 7) >> 3;

	if( cm_noAreas->integer ) {
		// for debugging, send everything
		memset( buffer, 255, bytes );
	} else {
		memset( buffer, 0, bytes );

		for( i = 1; i < numareas; i++ ) {
			if( !area || i == area || CM_AreasConnected( i, area ) )
				buffer[i>>3] |= 1<<(i&7);
		}
	}

	return bytes;
}

/*
=================
CM_MergeAreaBits
=================
*/
void CM_MergeAreaBits( qbyte *buffer, int area )
{
	int	i;

	for( i = 1; i < numareas; i++ ) {
		if ( CM_AreasConnected( i, area ) || i == area )
			buffer[i>>3] |= 1 << (i&7);
	}
}

/*
===================
CM_WritePortalState

Writes the portal state to a savegame file
===================
*/
void CM_WritePortalState( FILE *f )
{
	int i, j;

	fwrite( &numareaportals, sizeof(int), 1, f );

	for( i = 1; i < MAX_CM_AREAPORTALS; i++ ) {
		if( map_areaportals[i].area ) {
			fwrite( &i, sizeof(int), 1, f );
			fwrite( &map_areaportals[i], sizeof(map_areaportals[0]), 1, f );
		}
	}

	fwrite( &numareas, sizeof(int), 1, f );

	for( i = 1; i < numareas; i++ ) {
		fwrite( &map_areas[i].numareaportals, sizeof(int), 1, f );

		for( j = 0; j < map_areas[i].numareaportals; j++ )
			fwrite( &map_areas[i].areaportals[j], sizeof(int), 1, f );
	}
}

/*
===================
CM_ReadPortalState

Reads the portal state from a savegame file
and recalculates the area connections
===================
*/
void CM_ReadPortalState( FILE *f )
{
	int i, j;

	fread( &numareaportals, sizeof(int), 1, f );
	for( i = 1; i < numareaportals; i++ ) {
		fread( &j, sizeof(int), 1, f );
		fread( &map_areaportals[j], sizeof(map_areaportals[0]), 1, f );
	}

	fread( &numareas, sizeof(int), 1, f );

	for( i = 1; i < numareas; i++ ) {
		fread( &map_areas[i].numareaportals, sizeof(int), 1, f );

		for ( j = 0; j < map_areas[i].numareaportals; j++ )
			fread( &map_areas[i].areaportals[j], sizeof(int), 1, f );
	}

	CM_FloodAreaConnections ();
}
