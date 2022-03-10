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

#define MAX_CM_AREAPORTALS		(MAX_EDICTS)
#define MAX_CM_LEAFS			(MAX_MAP_LEAFS)

#define CM_SUBDIV_LEVEL			(15)

typedef struct
{
	int				contents;
	int				flags;
} csurface_t;

typedef struct
{
	cplane_t		*plane;
	int				children[2];	// negative numbers are leafs
} cnode_t;

typedef struct
{
	cplane_t		*plane;
	int				surfFlags;
} cbrushside_t;

typedef struct
{
	int				contents;

	int				numsides;
	cbrushside_t	*brushsides;

	int				checkcount;		// to avoid repeated testings
} cbrush_t;

typedef struct
{
	int				contents;

	int				numbrushes;
	cbrush_t		*brushes;

	vec3_t			mins, maxs;

	int				checkcount;		// to avoid repeated testings
} cface_t;

typedef struct
{
	int				contents;
	int				cluster;

	int				area;

	int				nummarkbrushes;
	cbrush_t		**markbrushes;

	int				nummarkfaces;
	cface_t			**markfaces;
} cleaf_t;

typedef struct cmodel_s
{
	vec3_t			mins, maxs;

	int				nummarkfaces;
	cface_t			**markfaces;

	int				nummarkbrushes;
    cbrush_t		**markbrushes;
} cmodel_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct
{
	qboolean		open;
	int				area;
	int				otherarea;
} careaportal_t;

typedef struct
{
	int				numareaportals;
	int				areaportals[MAX_CM_AREAPORTALS];
	int				floodnum;		// if two areas have equal floodnums, they are connected
	int				floodvalid;
} carea_t;

int			checkcount;

mempool_t		*cmap_mempool = NULL;

char			map_name[MAX_QPATH];
qboolean		map_clientload;

int			numbrushsides;
cbrushside_t		*map_brushsides;

int			numshaderrefs;
csurface_t		*map_surfaces;

int			numplanes;
cplane_t		*map_planes;

int			numnodes;
cnode_t			*map_nodes;

int			numleafs = 1;
cleaf_t			map_leaf_empty;		// allow leaf funcs to be called without a map
cleaf_t			*map_leafs = &map_leaf_empty;

int			nummarkbrushes;
cbrush_t		**map_markbrushes;

int			numcmodels;
cmodel_t		map_cmodel_empty;
cmodel_t		*map_cmodels = &map_cmodel_empty;

int			numbrushes;
cbrush_t		*map_brushes;

int			numfaces;
cface_t			*map_faces;

int			nummarkfaces;
cface_t			**map_markfaces;

vec4_t			*map_verts;			// this will be freed
int			numvertexes;

int			numareaportals = 1;
careaportal_t		map_areaportals[MAX_CM_AREAPORTALS];

int			numareas = 1;
carea_t			map_area_empty;
carea_t			*map_areas = &map_area_empty;

dvis_t			*map_pvs, *map_phs;
int			map_visdatasize;

qbyte			nullrow[MAX_CM_LEAFS/8];

int			numentitychars;
char			map_entitystring_empty;
char			*map_entitystring = &map_entitystring_empty;

int			floodvalid;

cvar_t			*cm_noAreas;
cvar_t			*cm_noCurves;

int			c_pointcontents;
int			c_traces, c_brush_traces;

qbyte			*cmod_base;

void	CM_InitBoxHull (void);
void	FloodAreaConnections (void);

/*
===============================================================================

					PATCH LOADING

===============================================================================
*/

/*
=================
CM_CreateBrushFromPoints
=================
*/
static int CM_CreateBrushFromPoints ( cbrush_t *brush, vec3_t *verts, csurface_t *surface )
{
	int				i, j, k;
	int				axis, dir;
	vec3_t			mins, maxs;
	cbrushside_t	*s;
	cplane_t		*planes;
	vec3_t			normal;
	float			d, dist;
	cplane_t		mainplane;
	vec3_t			vec, vec2;
	int				numbrushplanes = 0;
	cplane_t		brushplanes[25];

	// set default values for brush
	brush->numsides = 0;
	brush->brushsides = NULL;
	brush->contents = surface->contents;

	// calculate plane for this triangle
	PlaneFromPoints ( verts, &mainplane );
	if ( ComparePlanes (mainplane.normal, mainplane.dist, vec3_origin, 0) ) {
		return 0;
	}
	SnapPlane ( mainplane.normal, &mainplane.dist );

	// calculate mins & maxs
	ClearBounds ( mins, maxs );
	for ( i = 0; i < 3; i++ ) {
		AddPointToBounds ( verts[i], mins, maxs );
	}

	// add the axial planes
	for ( axis = 0; axis < 3; axis++ ) {
		for ( dir = -1; dir <= 1; dir += 2 ) {
			VectorClear ( normal );
			normal[axis] = dir;
			if (dir == 1)
				dist = maxs[axis];
			else
				dist = -mins[axis];

			if ( !ComparePlanes (mainplane.normal, mainplane.dist, normal, dist) ) {
				VectorCopy ( normal, brushplanes[numbrushplanes].normal );
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
			}
		}
	}

	// add front plane
	VectorCopy ( mainplane.normal, brushplanes[numbrushplanes].normal );
	brushplanes[numbrushplanes].dist = mainplane.dist; numbrushplanes++;

	// add the edge bevels
	for ( i = 0; i < 3; i++ ) {
		j = (i + 1) % 3;
		k = (i + 2) % 3;

		VectorSubtract ( verts[i], verts[j], vec );
		if ( VectorNormalize (vec) < 0.5 ) {
			continue;
		}

		SnapVector ( vec );
		for ( j = 0; j < 3; j++ ) {
			if ( vec[j] == 1 || vec[j] == -1 ) {
				break;	// axial
			}
		}
		if ( j != 3 ) {
			continue;	// only test non-axial edges
		}

		// try the six possible slanted axials from this edge
		for ( axis = 0; axis < 3; axis++ ) {
			for ( dir = -1; dir <= 1; dir += 2 ) {
				// construct a plane
				VectorClear ( vec2 );
				vec2[axis] = dir;
				CrossProduct ( vec, vec2, normal );
				if ( VectorNormalize (normal) < 0.5 ) {
					continue;
				}

				dist = DotProduct ( verts[i], normal );

				for ( j = 0; j < numbrushplanes; j++ ) {
					// if this plane has already been used, skip it
					if ( ComparePlanes (brushplanes[j].normal, brushplanes[j].dist, normal, dist) ) {
						break;
					}
				}

				if ( j != numbrushplanes ) {
					continue;
				}

				// if third point of the triangle is
				// behind this plane, it is a proper edge bevel
				d = DotProduct ( verts[k], normal ) - dist;
				if ( d > 0.1 ) {
					continue;	// point in front: this plane isn't part of the outer hull
				}

				// add this plane
				VectorCopy ( normal, brushplanes[numbrushplanes].normal );
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
			}
		}
	}

	// add brushsides
	s = brush->brushsides = Mem_Alloc ( cmap_mempool, numbrushplanes * sizeof (*s) );
	planes = Mem_Alloc ( cmap_mempool, numbrushplanes * sizeof(cplane_t) );
	for ( i = 0; i < numbrushplanes; i++, s++ ) {
		planes[i] = brushplanes[i];
		SnapPlane ( planes[i].normal, &planes[i].dist );
		CategorizePlane ( &planes[i] );

		s->plane = &planes[i];
		s->surfFlags = surface->flags;
	}

	return (brush->numsides = numbrushplanes);
}

/*
=================
CM_CreatePatch
=================
*/
static void CM_CreatePatch ( cface_t *face, csurface_t *surface, int numverts, vec4_t *verts, int *patch_cp )
{
	int step[2], size[2], flat[2], i, u, v;
	cbrush_t *brushes;
	vec4_t *points;
	vec3_t tverts[4], tverts2[4];

// find the degree of subdivision in the u and v directions
	Patch_GetFlatness ( CM_SUBDIV_LEVEL, verts, patch_cp, flat );

	step[0] = 1 << flat[0];
	step[1] = 1 << flat[1];
	size[0] = ( patch_cp[0] >> 1 ) * step[0] + 1;
	size[1] = ( patch_cp[1] >> 1 ) * step[1] + 1;
	if ( size[0] <= 0 || size[1] <= 0 ) {
		return;
	}

	points = Mem_Alloc ( cmap_mempool, size[0] * size[1] * sizeof (vec4_t) );
	brushes = Mem_Alloc ( cmap_mempool, (size[0]-1) * (size[1]-1) * 2 * sizeof (cbrush_t) );

// fill in
	Patch_Evaluate ( verts, patch_cp, step, points );

	face->numbrushes = 0;
	face->brushes = NULL;
	ClearBounds ( face->mins, face->maxs );

// create a set of brushes
    for ( v = 0; v < size[1]-1; v++ ) {
		for ( u = 0; u < size[0]-1; u++ ) {
			i = v * size[0] + u;
			VectorCopy ( points[i], tverts[0] );
			VectorCopy ( points[i + size[0]], tverts[1] );
			VectorCopy ( points[i + 1], tverts[2] );
			VectorCopy ( points[i + size[0] + 1], tverts[3] );

			for ( i = 0; i < 4; i++ ) {
				AddPointToBounds ( tverts[i], face->mins, face->maxs );
			}

			// create two brushes
			if ( CM_CreateBrushFromPoints (&brushes[face->numbrushes], tverts, surface) ) {
				face->numbrushes++;
			}

			VectorCopy ( tverts[2], tverts2[0] );
			VectorCopy ( tverts[1], tverts2[1] );
			VectorCopy ( tverts[3], tverts2[2] );

			if ( CM_CreateBrushFromPoints (&brushes[face->numbrushes], tverts2, surface) ) {
				face->numbrushes++;
			}
		}
    }

	if ( !face->numbrushes ) {
		ClearBounds ( face->mins, face->maxs );
		Mem_Free ( points );
		Mem_Free ( brushes );
		return;
	}

	face->contents = surface->contents;
	face->brushes = Mem_Alloc ( cmap_mempool, face->numbrushes * sizeof(cbrush_t) );
	memcpy ( face->brushes, brushes, face->numbrushes * sizeof(cbrush_t) );

	Mem_Free ( points );
	Mem_Free ( brushes );

	for ( i = 0; i < 3; i++ ) {
		// spread the mins / maxs by a pixel
		face->mins[i] -= 1;
		face->maxs[i] += 1;
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
void CMod_LoadSurfaces (lump_t *l)
{
	dshaderref_t	*in;
	csurface_t		*out;
	int				i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no shaders");

	out = map_surfaces = Mem_Alloc ( cmap_mempool, count * sizeof(*out) );
	numshaderrefs = count;

	for ( i=0 ; i<count ; i++, in++, out++ ) {
		out->flags = LittleLong ( in->flags );
		out->contents = LittleLong ( in->contents );
	}
}

/*
=================
CMod_LoadVertexes
=================
*/
void CMod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	vec4_t		*out;
	int			i, count, j;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMOD_LoadVertexes: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no vertexes");

	out = Mem_Alloc ( cmap_mempool, count*sizeof(*out) );
	map_verts = out;
	numvertexes = count;

	for ( i=0 ; i<count ; i++, in++)
	{
		for ( j=0 ; j < 3 ; j++)
		{
			out[i][j] = LittleFloat ( in->point[j] );
		}
	}
}


/*
=================
CMod_LoadFaces
=================
*/
void CMod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	cface_t		*out;
	csurface_t	*surface;
	int			i, count, shadernum;
	int			numverts, firstvert, patch_cp[2];

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no faces");

	out = map_faces = Mem_Alloc ( cmap_mempool, count * sizeof (*out) );
	numfaces = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->contents = 0;
		out->numbrushes = 0;
		out->brushes = NULL;

		if ( LittleLong (in->facetype) != FACETYPE_PATCH ) {
			continue;
		}

		shadernum = LittleLong ( in->shadernum );
		if ( shadernum < 0 || shadernum >= numshaderrefs ) {
			continue;
		}
		
		surface = &map_surfaces[shadernum];
		if ( !surface->contents || (surface->flags & SURF_NONSOLID) ) {
			continue;
		}

		patch_cp[0] = LittleLong ( in->patch_cp[0] );
		patch_cp[1] = LittleLong ( in->patch_cp[1] );
		if ( patch_cp[0] <= 0 || patch_cp[1] <= 0 ) {
			continue;
		}

		numverts = LittleLong ( in->numverts );
		firstvert = LittleLong ( in->firstvert );
		if ( numverts <= 0 || firstvert < 0 || firstvert >= numvertexes ) {
			continue;
		}

		CM_CreatePatch ( out, surface, numverts, map_verts + firstvert, patch_cp );
	}
}

/*
=================
CMod_LoadSubmodels
=================
*/
void CMod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no models");

	out = map_cmodels = Mem_Alloc ( cmap_mempool, count * sizeof(*out) );
	numcmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->nummarkfaces = LittleLong ( in->numfaces );
		out->markfaces = Mem_Alloc ( cmap_mempool, out->nummarkfaces * sizeof(cface_t *) );
		out->nummarkbrushes = LittleLong ( in->numbrushes );
		out->markbrushes = Mem_Alloc ( cmap_mempool, out->nummarkbrushes * sizeof(cbrush_t *) );

		for ( j = 0; j < out->nummarkfaces; j++ ) {
			out->markfaces[j] = map_faces + LittleLong ( in->firstface ) + j;
		}
		for ( j = 0; j < out->nummarkbrushes; j++ ) {
			out->markbrushes[j] = map_brushes + LittleLong ( in->firstbrush ) + j;
		}

		for ( j = 0; j < 3; j++ ) {
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
		}
	}
}

/*
=================
CMod_LoadNodes

=================
*/
void CMod_LoadNodes (lump_t *l)
{
	dnode_t		*in;
	cnode_t		*out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map has no nodes");

	out = map_nodes = Mem_Alloc ( cmap_mempool, count * sizeof(*out) );
	numnodes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->plane = map_planes + LittleLong (in->planenum);
		out->children[0] = LittleLong (in->children[0]);
		out->children[1] = LittleLong (in->children[1]);
	}
}

/*
=================
CMod_LoadMarkFaces
=================
*/
void CMod_LoadMarkFaces (lump_t *l)
{	
	int		i, j, count;
	int		*in;
	cface_t **out;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leaffaces");

	map_markfaces = out = Mem_Alloc (cmap_mempool, count * sizeof(*out));
	nummarkfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleLong ( in[i] );
		if (j < 0 ||  j >= numfaces)
			Com_Error (ERR_DROP, "CMod_LoadLeafFaces: bad surface number");

		out[i] = map_faces + j;
	}
}

/*
=================
CMod_LoadLeafs
=================
*/
void CMod_LoadLeafs (lump_t *l)
{
	int			i, j;
	cleaf_t		*out;
	dleaf_t 	*in;
	cbrush_t	*brush;
	cface_t		*face;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leafs");

	out = map_leafs = Mem_Alloc ( cmap_mempool, count * sizeof(*out) );
	numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->cluster = LittleLong ( in->cluster );
		out->area = LittleLong ( in->area ) + 1;
		out->markbrushes = map_markbrushes + LittleLong ( in->firstleafbrush );
		out->nummarkbrushes = LittleLong ( in->numleafbrushes );

		out->contents = 0;

		// OR brushes' contents
		for ( j = 0; j < out->nummarkbrushes; j++ ) {
			brush = out->markbrushes[j];
			out->contents |= brush->contents;
		}

		out->markfaces = map_markfaces + LittleLong ( in->firstleafface );
		out->nummarkfaces = LittleLong ( in->numleaffaces );

		// OR patches' contents
		for ( j = 0; j < out->nummarkfaces; j++ ) {
			face = out->markfaces[j];
			out->contents |= face->contents;
		}

		if ( out->area >= numareas ) {
			numareas = out->area + 1;
		}
	}

	map_areas = Mem_Alloc ( cmap_mempool, numareas * sizeof (*map_areas) );
}

/*
=================
CMod_LoadPlanes
=================
*/
void CMod_LoadPlanes (lump_t *l)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");

	out = map_planes = Mem_Alloc ( cmap_mempool, count * sizeof(*out) );
	numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->signbits = 0;
		out->type = PLANE_NONAXIAL;

		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				out->signbits |= 1<<j;
			if (out->normal[j] == 1.0f)
				out->type = j;
		}

		out->dist = LittleFloat (in->dist);
	}
}

/*
=================
CMod_LoadMarkBrushes
=================
*/
void CMod_LoadMarkBrushes (lump_t *l)
{
	int			i;
	cbrush_t	**out;
	int		 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leafbrushes");

	out = map_markbrushes = Mem_Alloc ( cmap_mempool, count * sizeof (*out) );
	nummarkbrushes = count;

	for ( i=0 ; i<count ; i++, in++)
		out[i] = map_brushes + LittleLong (*in);
}

/*
=================
CMod_LoadBrushSides
=================
*/
void CMod_LoadBrushSides (lump_t *l)
{
	int			i, j;
	cbrushside_t	*out;
	dbrushside_t 	*in;
	int			count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no brushsides");

	out = map_brushsides = Mem_Alloc ( cmap_mempool, count * sizeof(*out) );
	numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->plane = map_planes + LittleLong (in->planenum);
		j = LittleLong (in->shadernum);
		if (j >= numshaderrefs)
			Com_Error (ERR_DROP, "Bad brushside texinfo");
		out->surfFlags = map_surfaces[j].flags;
	}
}

/*
=================
CMod_LoadBrushes

=================
*/
void CMod_LoadBrushes (lump_t *l)
{
	dbrush_t	*in;
	cbrush_t	*out;
	int			i, count;
	int			shaderref;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no brushes");

	out = map_brushes = Mem_Alloc ( cmap_mempool, count * sizeof(*out) );
	numbrushes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		shaderref = LittleLong ( in->shadernum );
		out->contents = map_surfaces[shaderref].contents;
		out->numsides = LittleLong ( in->numsides );
		out->brushsides = map_brushsides + LittleLong ( in->firstside );
	}
}

/*
=================
CMod_LoadVisibility
=================
*/
void CMod_LoadVisibility (lump_t *l)
{
	map_visdatasize = l->filelen;
	if (!map_visdatasize)
	{
		map_pvs = NULL;
		return;
	}

	map_pvs = Mem_Alloc ( cmap_mempool, map_visdatasize );
	memcpy (map_pvs, cmod_base + l->fileofs, map_visdatasize);

	map_pvs->numclusters = LittleLong (map_pvs->numclusters);
	map_pvs->rowsize = LittleLong (map_pvs->rowsize);
}


/*
=================
CMod_LoadEntityString
=================
*/
void CMod_LoadEntityString (lump_t *l)
{
	numentitychars = l->filelen;
	if (!l->filelen)
		return;

	map_entitystring = Mem_Alloc ( cmap_mempool, numentitychars );
	memcpy (map_entitystring, cmod_base + l->fileofs, l->filelen);
}

/*
=================
CM_CalcPHS
=================
*/
void CM_CalcPHS (void)
{
	int		rowbytes, rowwords;
	int		i, j, k, l, index;
	int		bitbyte;
	unsigned	*dest, *src;
	qbyte	*scan;
	int		count, vcount;

	if ( !map_pvs ) {
		map_phs = NULL;
		return;
	}

	Com_DPrintf ("Building PHS...\n");

	map_phs = Mem_Alloc ( cmap_mempool, map_visdatasize );
	map_phs->rowsize = map_pvs->rowsize;
	map_phs->numclusters = map_pvs->numclusters;

	rowbytes = map_pvs->rowsize;
	rowwords = rowbytes / sizeof(long);

	vcount = 0;
	for (i=0 ; i<map_pvs->numclusters ; i++)
	{
		scan = CM_ClusterPVS ( i );
		for (j=0 ; j<map_pvs->numclusters ; j++)
		{
			if ( scan[j>>3] & (1<<(j&7)) )
			{
				vcount++;
			}
		}
	}

	count = 0;
	scan = (qbyte *)map_pvs->data;
	dest = (unsigned *)((qbyte *)map_phs->data);

	for (i=0 ; i<map_phs->numclusters ; i++, dest += rowwords, scan += rowbytes)
	{
		memcpy (dest, scan, rowbytes);

		for (j=0 ; j<rowbytes ; j++)
		{
			bitbyte = scan[j];
			if (!bitbyte)
				continue;
			for (k=0 ; k<8 ; k++)
			{
				if (! (bitbyte & (1<<k)) )
					continue;

				// OR this pvs row into the phs
				index = (j<<3) + k;

				if (index >= map_phs->numclusters)
					Com_Error (ERR_DROP, "CM_CalcPHS: Bad bit in PVS");	// pad bits should be 0

				src = (unsigned *)((qbyte *)map_pvs->data) + index*rowwords;
				for (l=0 ; l<rowwords ; l++)
					dest[l] |= src[l];
			}
		}
		for (j=0 ; j<map_phs->numclusters ; j++)
			if ( ((qbyte *)dest)[j>>3] & (1<<(j&7)) )
				count++;
	}

	Com_DPrintf ("Average clusters visible / hearable / total: %i / %i / %i\n"
		, vcount/map_phs->numclusters, count/map_phs->numclusters, map_phs->numclusters);
}

/*
==================
CM_ClientLoad

FIXME!
==================
*/
qboolean CM_ClientLoad (void)
{
	return map_clientload;
}

/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
cmodel_t *CM_LoadMap (char *name, qboolean clientload, unsigned *checksum)
{
	unsigned		*buf;
	int				i;
	dheader_t		header;
	int				length;
	static unsigned	last_checksum;

	cm_noAreas = Cvar_Get ("cm_noAreas", "0", CVAR_CHEAT);
	cm_noCurves = Cvar_Get ("cm_noCurves", "0", CVAR_CHEAT);

	map_clientload = clientload;

	if ( !strcmp (map_name, name) && (clientload || !Cvar_VariableValue ("flushmap")) )
	{
		*checksum = last_checksum;
		if (!clientload)
		{
			memset (map_areaportals, 0, sizeof(map_areaportals));
			FloodAreaConnections ();
		}
		return map_cmodels;		// still have the right version
	}

	// free old stuff
	if ( !cmap_mempool ) {
		cmap_mempool = Mem_AllocPool ( NULL, "Collision Map" );
	} else {
		Mem_EmptyPool ( cmap_mempool );
	}

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

	map_leafs = &map_leaf_empty;
	map_cmodels = &map_cmodel_empty;
	map_areas = &map_area_empty;
	map_entitystring = &map_entitystring_empty;

	if (!name || !name[0])
	{
		numleafs = 1;
		numcmodels = 2;
		*checksum = 0;
		return map_cmodels;			// cinematic servers won't have anything at all
	}

	//
	// load the file
	//
	length = FS_LoadFile (name, (void **)&buf);
	if (!buf)
		Com_Error (ERR_DROP, "Couldn't load %s", name);

	last_checksum = LittleLong (Com_BlockChecksum (buf, length));
	*checksum = last_checksum;

	header = *(dheader_t *)buf;
	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)&header)[i] = LittleLong ( ((int *)&header)[i]);

	if ((header.version != Q3BSPVERSION) && (header.version != RTCWBSPVERSION))
		Com_Error (ERR_DROP, "CMod_LoadBrushModel: %s has wrong version number (%i should be %i or %i)"
		, name, header.version, Q3BSPVERSION, RTCWBSPVERSION);

	cmod_base = (qbyte *)buf;

	// load into heap
	CMod_LoadSurfaces (&header.lumps[LUMP_SHADERREFS]);
	CMod_LoadPlanes (&header.lumps[LUMP_PLANES]);
	CMod_LoadBrushSides (&header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadBrushes (&header.lumps[LUMP_BRUSHES]);
	CMod_LoadMarkBrushes (&header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadVertexes (&header.lumps[LUMP_VERTEXES]);
	CMod_LoadFaces (&header.lumps[LUMP_FACES]);
	CMod_LoadMarkFaces (&header.lumps[LUMP_LEAFFACES]);
	CMod_LoadLeafs (&header.lumps[LUMP_LEAFS]);
	CMod_LoadNodes (&header.lumps[LUMP_NODES]);
	CMod_LoadSubmodels (&header.lumps[LUMP_MODELS]);
	CMod_LoadVisibility (&header.lumps[LUMP_VISIBILITY]);
	CMod_LoadEntityString (&header.lumps[LUMP_ENTITIES]);

	FS_FreeFile (buf);

	CM_InitBoxHull ();

	memset (map_areaportals, 0, sizeof(map_areaportals));
	FloodAreaConnections ();

	CM_CalcPHS ();

	if (map_verts) {
		Mem_Free (map_verts);
	}

	memset ( nullrow, 255, MAX_CM_LEAFS / 8 );

	Q_strncpyz ( map_name, name, sizeof(map_name) );

	return map_cmodels;
}

/*
==================
CM_InlineModel
==================
*/
cmodel_t	*CM_InlineModel (int num)
{
	if (num < 0 || num >= numcmodels)
		Com_Error (ERR_DROP, "CM_InlineModel: bad number");

	return &map_cmodels[num];
}

int		CM_ClusterSize (void)
{
	return map_pvs ? map_pvs->rowsize : MAX_CM_LEAFS / 8;
}

int		CM_NumClusters (void)
{
	return map_pvs->numclusters;
}

int		CM_NumInlineModels (void)
{
	return numcmodels;
}

char	*CM_EntityString (void)
{
	return map_entitystring;
}

int		CM_LeafContents (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafContents: bad number");
	return map_leafs[leafnum].contents;
}

int		CM_LeafCluster (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafCluster: bad number");
	return map_leafs[leafnum].cluster;
}

int		CM_LeafArea (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafArea: bad number");
	return map_leafs[leafnum].area;
}

void	CM_InlineModelBounds (cmodel_t *cmodel, vec3_t mins, vec3_t maxs)
{
	if (!cmodel)
		return;

	VectorCopy ( cmodel->mins, mins );
	VectorCopy ( cmodel->maxs, maxs );
}

//=======================================================================

cplane_t		box_planes[6];
cbrushside_t	box_brushsides[6];
cbrush_t		box_brush[1];
cbrush_t		*box_markbrushes[1];
cmodel_t		box_cmodel[1];

/*
===================
CM_InitBoxHull

Set up the planes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_InitBoxHull (void)
{
	int			i;
	int			side;
	cplane_t	*p;
	cbrushside_t	*s;

	box_brush->numsides = 6;
	box_brush->brushsides = box_brushsides;
	box_brush->contents = CONTENTS_BODY;

	box_markbrushes[0] = box_brush;

	box_cmodel->nummarkfaces = 0;
	box_cmodel->markfaces = NULL;
	box_cmodel->markbrushes = box_markbrushes;
	box_cmodel->nummarkbrushes = 1;

	for (i=0 ; i<6 ; i++)
	{
		side = i&1;

		// brush sides
		s = box_brushsides + i;
		s->plane = box_planes + i;
		s->surfFlags = 0;

		// planes
		p = &box_planes[i];
		VectorClear (p->normal);

		if ( i&1 ) {
			p->type = PLANE_NONAXIAL;
			p->normal[i>>1] = -1;
			p->signbits = (1<<(i>>1));
		} else {
			p->type = i>>1;
			p->normal[i>>1] = 1;
			p->signbits = 0;
		}
	}
}


/*
===================
CM_ModelForBBox

To keep everything totally uniform, bounding boxes are turned into inline models
===================
*/
cmodel_t *CM_ModelForBBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = -mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = -mins[2];

	VectorCopy (mins, box_cmodel->mins);
	VectorCopy (maxs, box_cmodel->maxs);

	return box_cmodel;
}


/*
==================
CM_PointLeafnum_r

==================
*/
int CM_PointLeafnum_r (vec3_t p, int num)
{
	float		d;
	cnode_t		*node;
	cplane_t	*plane;

	while (num >= 0)
	{
		node = map_nodes + num;
		plane = node->plane;
		d = PlaneDiff (p, plane);

		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_pointcontents++;		// optimize counter

	return -1 - num;
}

int CM_PointLeafnum (vec3_t p)
{
	if (!numplanes)
		return 0;		// sound may call this without map loaded
	return CM_PointLeafnum_r (p, 0);
}



/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
int		leaf_count, leaf_maxcount;
int		*leaf_list;
float	*leaf_mins, *leaf_maxs;
int		leaf_topnode;

void CM_BoxLeafnums_r (int nodenum)
{
	cnode_t	*node;
	int		s;

	while (1)
	{
		if (nodenum < 0)
		{
			if (leaf_count >= leaf_maxcount)
			{
				return;
			}
			leaf_list[leaf_count++] = -1 - nodenum;
			return;
		}
	
		node = &map_nodes[nodenum];
		s = BoxOnPlaneSide (leaf_mins, leaf_maxs, node->plane);

		if (s == 1)
			nodenum = node->children[0];
		else if (s == 2)
			nodenum = node->children[1];
		else
		{	// go down both
			if (leaf_topnode == -1)
				leaf_topnode = nodenum;
			CM_BoxLeafnums_r (node->children[0]);
			nodenum = node->children[1];
		}
	}
}

int	CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode)
{
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	CM_BoxLeafnums_r (0);

	if (topnode)
		*topnode = leaf_topnode;

	return leaf_count;
}


/*
==================
CM_BrushContents
==================
*/
static inline int CM_BrushContents ( cbrush_t *brush, vec3_t p )
{
	int				i;
	cbrushside_t	*brushside;

	brushside = brush->brushsides;
	for ( i = 0; i < brush->numsides; i++, brushside++ ) {
		if ( PlaneDiff (p, brushside->plane) > 0 )
			break;
	}

	if ( i == brush->numsides ) {
		return brush->contents;
	}

	return 0;
}

/*
==================
CM_FaceContents
==================
*/
static inline int CM_FaceContents ( cface_t *face, vec3_t p )
{
	int			i, c;
	cbrush_t	*brush;
	
	for ( i = 0, brush = face->brushes; i < face->numbrushes; i++, brush++ ) {
		if ( (c = CM_BrushContents (brush, p)) ) {
			return c;
		}
	}

	return 0;
}

/*
==================
CM_PointContents

==================
*/
int CM_PointContents ( vec3_t p, cmodel_t *cmodel )
{
	int				i, contents;
	cface_t			*face, **markface;
	cbrush_t		*brush, **markbrush;

	if ( !numnodes ) {	// map not loaded
		return 0;
	}

	if ( !cmodel || cmodel == map_cmodels ) {
		cleaf_t	*leaf;
		
		leaf = &map_leafs[CM_PointLeafnum_r (p, 0)];
		contents = ( leaf->contents & CONTENTS_NODROP );

		markbrush = leaf->markbrushes;
		for ( i = 0; i < leaf->nummarkbrushes; i++ ) {
			brush = *markbrush++;

			// check if brush actually adds something to contents
			if ( (contents & brush->contents) == brush->contents ) {
				continue;
			}
			contents |= CM_BrushContents ( brush, p );
		}

		if ( cm_noCurves->value || !leaf->nummarkfaces ) {
			return contents;
		}

		markface = leaf->markfaces;
		for ( i = 0; i < leaf->nummarkfaces; i++ ) {
			face = *markface++;

			// check if patch actually adds something to contents
			if ( !face->numbrushes || (contents & face->contents) == face->contents ) {
				continue;
			}
			contents |= CM_FaceContents ( face, p );
		}
	} else {
		contents = 0;

		markbrush = cmodel->markbrushes;
		for ( i = 0; i < cmodel->nummarkbrushes; i++ ) {
			brush = *markbrush++;

			// check if brush actually adds something to contents
			if ( (contents & brush->contents) == brush->contents ) {
				continue;
			}
			contents |= CM_BrushContents ( brush, p );
		}

		if ( cm_noCurves->value || !cmodel->nummarkfaces ) {
			return contents;
		}

		markface = cmodel->markfaces;
		for ( i = 0; i < cmodel->nummarkfaces; i++ ) {
			face = *markface++;

			// check if patch actually adds something to contents
			if ( !face->numbrushes || (contents & face->contents) == face->contents ) {
				continue;
			}
			contents |= CM_FaceContents ( face, p );
		}
	}

	return contents;
}

/*
==================
CM_TransformedPointContents

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
int	CM_TransformedPointContents (vec3_t p, cmodel_t *cmodel, vec3_t origin, vec3_t angles)
{
	vec3_t		p_l;
	vec3_t		temp;
	mat3_t		axis;

	if (!numnodes)	// map not loaded
		return 0;

	// subtract origin offset
	VectorSubtract (p, origin, p_l);

	// rotate start and end into the models frame of reference
	if (cmodel && (cmodel != box_cmodel) && (angles[0] || angles[1] || angles[2]) )
	{
		AnglesToAxis (angles, axis);
		VectorCopy (p_l, temp);
		Matrix3_Multiply_Vec3 (axis, temp, p_l);
	}

	return CM_PointContents (p_l, cmodel);
}


/*
===============================================================================

BOX TRACING

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(1.0f / 32.0f)

vec3_t	trace_start, trace_end;
vec3_t	trace_mins, trace_maxs;
vec3_t	trace_absmins, trace_absmaxs;
vec3_t	trace_extents;

trace_t	*trace_trace;
int		trace_contents;
qboolean	trace_ispoint;		// optimized case


/*
================
CM_ClipBoxToBrush
================
*/
void CM_ClipBoxToBrush (vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2, trace_t *trace, cbrush_t *brush)
{
	int			i, j;
	cplane_t	*plane, *clipplane;
	float		enterfrac, leavefrac;
	vec3_t		ofs;
	float		d1, d2;
	qboolean	getout, startout;
	float		f;
	cbrushside_t	*side, *leadside;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	if (!brush->numsides)
		return;

	c_brush_traces++;

	getout = qfalse;
	startout = qfalse;
	leadside = NULL;
	side = brush->brushsides;

	for (i = 0; i < brush->numsides; i++, side++)
	{
		plane = side->plane;

		if (!trace_ispoint)
		{	// general box case

			// push the plane out apropriately for mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}

			if ( plane->type < 3 ) {
				d1 = p1[plane->type] + ofs[plane->type] - plane->dist;
				d2 = p2[plane->type] + ofs[plane->type] - plane->dist;
			} else {
				d1 = (p1[0] + ofs[0])*plane->normal[0] + (p1[1] + ofs[1])*plane->normal[1] + (p1[2] + ofs[2])*plane->normal[2] - plane->dist;
				d2 = (p2[0] + ofs[0])*plane->normal[0] + (p2[1] + ofs[1])*plane->normal[1] + (p2[2] + ofs[2])*plane->normal[2] - plane->dist;
			}
		}
		else
		{	// special point case
			if ( plane->type < 3 ) {
				d1 = p1[plane->type] - plane->dist;
				d2 = p2[plane->type] - plane->dist;
			} else {
				d1 = DotProduct (p1, plane->normal) - plane->dist;
				d2 = DotProduct (p2, plane->normal) - plane->dist;
			}
		}

		if (d2 > 0)
			getout = qtrue;	// endpoint is not in solid
		if (d1 > 0)
			startout = qtrue;

		// if completely in front of face, no intersection
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		f = d1 - d2;
		// crosses face
		if (f > 0)
		{	// enter
			f = (d1-DIST_EPSILON) / f;
			if (f > enterfrac)
			{
				enterfrac = f;
				clipplane = plane;
				leadside = side;
			}
		}
		else if (f < 0)
		{	// leave
			f = (d1+DIST_EPSILON) / f;
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout)
	{	// original point was inside brush
		trace->startsolid = qtrue;
		if (!getout)
			trace->allsolid = qtrue;
		return;
	}

	if (enterfrac - (1.0f / 1024.0f) <= leavefrac)
	{
		if (enterfrac > -1 && enterfrac < trace->fraction)
		{
			if (enterfrac < 0)
				enterfrac = 0;
			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surfFlags = leadside->surfFlags;
			trace->contents = brush->contents;
		}
	}
}

/*
================
CM_TestBoxInBrush
================
*/
void CM_TestBoxInBrush (vec3_t mins, vec3_t maxs, vec3_t p1,
					  trace_t *trace, cbrush_t *brush)
{
	int			i, j;
	cplane_t	*plane;
	vec3_t		ofs;
	float		d1;
	cbrushside_t	*side;

	if (!brush->numsides)
		return;

	side = brush->brushsides;
	for (i = 0; i<brush->numsides; i++, side++)
	{
		plane = side->plane;

		// general box case

		// push the plane out apropriately for mins/maxs
		for (j=0 ; j<3 ; j++)
		{
			if (plane->normal[j] < 0)
				ofs[j] = maxs[j];
			else
				ofs[j] = mins[j];
		}

		if ( plane->type < 3 ) {
			d1 = p1[plane->type] + ofs[plane->type] - plane->dist;
		} else {
			d1 = (p1[0] + ofs[0])*plane->normal[0] + (p1[1] + ofs[1])*plane->normal[1] + (p1[2] + ofs[2])*plane->normal[2] - plane->dist;
		}

		// if completely in front of face, no intersection
		if (d1 > 0)
			return;
	}

	// inside this brush
	trace->startsolid = trace->allsolid = qtrue;
	trace->fraction = 0;
	trace->contents = brush->contents;
}

/*
================
CM_ClipBox
================
*/
void CM_ClipBox (cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces, int nummarkfaces)
{
	int			i, j;
	cbrush_t	*b;
	cface_t		*face;

	// trace line against all brushes
	for (i=0 ; i<nummarkbrushes ; i++)
	{
		b = markbrushes[i];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush
		b->checkcount = checkcount;
		if ( !(b->contents & trace_contents))
			continue;
		CM_ClipBoxToBrush (trace_mins, trace_maxs, trace_start, trace_end, trace_trace, b);
		if (!trace_trace->fraction)
			return;
	}

	if (cm_noCurves->value || !nummarkfaces)
		return;

	// trace line against all patches
	for (i = 0; i < nummarkfaces; i++)
	{
		face = markfaces[i];
		if (face->checkcount == checkcount)
			continue;	// already checked this patch
		face->checkcount = checkcount;
		if (!face->numbrushes || !(face->contents & trace_contents))
			continue;
		if (!BoundsIntersect (face->mins, face->maxs, trace_absmins, trace_absmaxs))
			continue;
		for (j = 0; j < face->numbrushes; j++)
		{
			CM_ClipBoxToBrush (trace_mins, trace_maxs, trace_start, trace_end, trace_trace, &face->brushes[j]);
			if (!trace_trace->fraction)
				return;
		}
	}
}


/*
================
CM_TraceToLeaf
================
*/
void CM_TraceToLeaf (int leafnum)
{
	cleaf_t		*leaf;

	leaf = &map_leafs[leafnum];
	if ( !(leaf->contents & trace_contents))
		return;
	CM_ClipBox (leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces);
}

/*
================
CM_TraceToInlineModel
================
*/
void CM_TraceToInlineModel (cmodel_t *mod)
{
	if (!BoundsIntersect (mod->mins, mod->maxs, trace_absmins, trace_absmaxs))
		return;
	CM_ClipBox (mod->markbrushes, mod->nummarkbrushes, mod->markfaces, mod->nummarkfaces);
}

/*
================
CM_TestBox
================
*/
void CM_TestBox (cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces, int nummarkfaces)
{
	int			i, j;
	cbrush_t	*b;
	cface_t		*face;

	// trace line against all brushes
	for (i = 0; i < nummarkbrushes; i++)
	{
		b = markbrushes[i];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush
		b->checkcount = checkcount;
		if ( !(b->contents & trace_contents))
			continue;
		CM_TestBoxInBrush (trace_mins, trace_maxs, trace_start, trace_trace, b);
		if (!trace_trace->fraction)
			return;
	}

	if (cm_noCurves->value || !nummarkfaces)
		return;

	// trace line against all patches
	for (i = 0; i < nummarkfaces; i++)
	{
		face = markfaces[i];
		if (face->checkcount == checkcount)
			continue;	// already checked this patch
		face->checkcount = checkcount;
		if (!face->numbrushes || !(face->contents & trace_contents))
			continue;
		if (!BoundsIntersect (face->mins, face->maxs, trace_absmins, trace_absmaxs))
			continue;
		for (j = 0; j < face->numbrushes; j++)
		{
			CM_TestBoxInBrush (trace_mins, trace_maxs, trace_start, trace_trace, &face->brushes[j]);
			if (!trace_trace->fraction)
				return;
		}
	}
}

/*
================
CM_TestInLeaf
================
*/
void CM_TestInLeaf (int leafnum)
{
	cleaf_t		*leaf;

	leaf = &map_leafs[leafnum];
	if ( !(leaf->contents & trace_contents))
		return;
	CM_TestBox (leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces);
}

/*
================
CM_TestInInlineModel
================
*/
void CM_TestInInlineModel (cmodel_t *mod)
{
	if (!BoundsIntersect (mod->mins, mod->maxs, trace_absmins, trace_absmaxs))
		return;
	CM_TestBox (mod->markbrushes, mod->nummarkbrushes, mod->markfaces, mod->nummarkfaces);
}

/*
==================
CM_RecursiveHullCheck

==================
*/
void CM_RecursiveHullCheck (int num, float p1f, float p2f, vec3_t p1, vec3_t p2)
{
	cnode_t		*node;
	cplane_t	*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	int			i;
	vec3_t		mid;
	int			side;
	float		midf;

	if (trace_trace->fraction <= p1f)
		return;		// already hit something nearer

	// if < 0, we are in a leaf node
	if (num < 0)
	{
		CM_TraceToLeaf (-1-num);
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = map_nodes + num;
	plane = node->plane;

	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	}
	else
	{
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
		if (trace_ispoint)
			offset = 0;
		else
			offset = fabs(trace_extents[0]*plane->normal[0]) +
				fabs(trace_extents[1]*plane->normal[1]) +
				fabs(trace_extents[2]*plane->normal[2]);
	}


	// see which sides we need to consider
	if (t1 >= offset && t2 >= offset)
	{
		CM_RecursiveHullCheck (node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if (t1 < -offset && t2 < -offset)
	{
		CM_RecursiveHullCheck (node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < t2)
	{
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON)*idist;
		frac = (t1 - offset + DIST_EPSILON)*idist;
	}
	else if (t1 > t2)
	{
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON)*idist;
		frac = (t1 + offset + DIST_EPSILON)*idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	clamp ( frac, 0, 1 );

	midf = p1f + (p2f - p1f)*frac;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	CM_RecursiveHullCheck (node->children[side], p1f, midf, p1, mid);

	// go past the node
	clamp ( frac2, 0, 1 );

	midf = p1f + (p2f - p1f)*frac2;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac2*(p2[i] - p1[i]);

	CM_RecursiveHullCheck (node->children[side^1], midf, p2f, mid, p2);
}



//======================================================================

/*
==================
CM_BoxTrace
==================
*/
void		CM_BoxTrace (trace_t *tr, vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  cmodel_t *cmodel, int brushmask)
{
	vec3_t	point;
	qboolean notworld = (cmodel && (cmodel != map_cmodels));

	if (!tr)
		return;

	checkcount++;		// for multi-check avoidance

	c_traces++;			// for statistics, may be zeroed

	// fill in a default trace
	memset (tr, 0, sizeof(*tr));
	tr->fraction = 1;

	if (!numnodes)	// map not loaded
		return;

	trace_trace = tr;
	trace_contents = brushmask;
	VectorCopy (start, trace_start);
	VectorCopy (end, trace_end);
	VectorCopy (mins, trace_mins);
	VectorCopy (maxs, trace_maxs);

	// build a bounding box of the entire move
	ClearBounds (trace_absmins, trace_absmaxs);
	VectorAdd (start, trace_mins, point);
	AddPointToBounds (point, trace_absmins, trace_absmaxs);
	VectorAdd (start, trace_maxs, point);
	AddPointToBounds (point, trace_absmins, trace_absmaxs);
	VectorAdd (end, trace_mins, point);
	AddPointToBounds (point, trace_absmins, trace_absmaxs);
	VectorAdd (end, trace_maxs, point);
	AddPointToBounds (point, trace_absmins, trace_absmaxs);

	//
	// check for position test special case
	//
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		int		leafs[1024];
		int		i, numleafs;
		vec3_t	c1, c2;
		int		topnode;

		if (notworld) 
		{
			CM_TestInInlineModel (cmodel);
		}
		else
		{
			VectorAdd (start, mins, c1);
			VectorAdd (start, maxs, c2);
			for (i=0 ; i<3 ; i++)
			{
				c1[i] -= 1;
				c2[i] += 1;
			}

			numleafs = CM_BoxLeafnums (c1, c2, leafs, 1024, &topnode);
			for (i=0 ; i<numleafs ; i++)
			{
				CM_TestInLeaf (leafs[i]);
				if (tr->allsolid)
					break;
			}
		}

		VectorCopy (start, tr->endpos);
		return;
	}

	//
	// check for point special case
	//
	if (mins[0] == 0 && mins[1] == 0 && mins[2] == 0
		&& maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0)
	{
		trace_ispoint = qtrue;
		VectorClear (trace_extents);
	}
	else
	{
		trace_ispoint = qfalse;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	//
	// general sweeping through world
	//
	if (!notworld)
		CM_RecursiveHullCheck (0, 0, 1, start, end);
	else
		CM_TraceToInlineModel (cmodel);

	if (tr->fraction == 1)
	{
		VectorCopy (end, tr->endpos);
	}
	else
	{
		tr->endpos[0] = start[0] + tr->fraction * (end[0] - start[0]);
		tr->endpos[1] = start[1] + tr->fraction * (end[1] - start[1]);
		tr->endpos[2] = start[2] + tr->fraction * (end[2] - start[2]);
	}
}


/*
==================
CM_TransformedBoxTrace

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
void		CM_TransformedBoxTrace (trace_t *tr, vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  cmodel_t *cmodel, int brushmask,
						  vec3_t origin, vec3_t angles)
{
	vec3_t		start_l, end_l;
	vec3_t		a, temp;
	mat3_t		axis;
	qboolean	rotated;

	if (!tr)
		return;

	// subtract origin offset
	VectorSubtract (start, origin, start_l);
	VectorSubtract (end, origin, end_l);

	// rotate start and end into the models frame of reference
	if (cmodel && (cmodel != box_cmodel) && (angles[0] || angles[1] || angles[2]) )
		rotated = qtrue;
	else
		rotated = qfalse;

	if (rotated)
	{
		AnglesToAxis (angles, axis);

		VectorCopy (start_l, temp);
		Matrix3_Multiply_Vec3 (axis, temp, start_l);

		VectorCopy (end_l, temp);
		Matrix3_Multiply_Vec3 (axis, temp, end_l);
	}

	// sweep the box through the model
	CM_BoxTrace (tr, start_l, end_l, mins, maxs, cmodel, brushmask);

	if (rotated && tr->fraction != 1.0)
	{
		VectorNegate (angles, a);
		AnglesToAxis (a, axis);

		VectorCopy (tr->plane.normal, temp);
		Matrix3_Multiply_Vec3 (axis, temp, tr->plane.normal);
	}

	if (tr->fraction == 1)
	{
		VectorCopy (end, tr->endpos);
	}
	else
	{
		tr->endpos[0] = start[0] + tr->fraction * (end[0] - start[0]);
		tr->endpos[1] = start[1] + tr->fraction * (end[1] - start[1]);
		tr->endpos[2] = start[2] + tr->fraction * (end[2] - start[2]);
	}
}


/*
===============================================================================

PVS / PHS

===============================================================================
*/

qbyte	*CM_ClusterPVS (int cluster)
{
	if (cluster == -1 || !map_pvs)
		return nullrow;

	return (qbyte *)map_pvs->data + cluster * map_pvs->rowsize;
}

qbyte	*CM_ClusterPHS (int cluster)
{
	if (cluster == -1 || !map_phs)
		return nullrow;

	return (qbyte *)map_phs->data + cluster * map_phs->rowsize;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/

qboolean CM_AddAreaPortal ( int portalnum, int area, int otherarea )
{
	carea_t *a;
	careaportal_t *ap;

	if ( portalnum >= MAX_CM_AREAPORTALS ) {
		return qfalse;
	}
	if ( !area || area > numareas || !otherarea || otherarea > numareas ) {
		return qfalse;
	}

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

void FloodArea_r (int areanum, int floodnum)
{
	int	i;
	carea_t	*area;
	careaportal_t *p;

	area = &map_areas[areanum];
	if (area->floodvalid == floodvalid)
	{
		if (area->floodnum == floodnum)
			return;
		Com_Error (ERR_DROP, "FloodArea_r: reflooded");
	}

	area->floodnum = floodnum;
	area->floodvalid = floodvalid;
	for (i=0 ; i<area->numareaportals ; i++)
	{
		p = &map_areaportals[area->areaportals[i]];
		if (!p->open)
			continue;

		if (p->area == areanum)
			FloodArea_r (p->otherarea, floodnum);
		else if (p->otherarea == areanum)
			FloodArea_r (p->area, floodnum);
	}
}

/*
====================
FloodAreaConnections

====================
*/
void FloodAreaConnections (void)
{
	int		i;
	int		floodnum;

	// all current floods are now invalid
	floodvalid++;
	floodnum = 0;

	// area 0 is not used
	for (i=1 ; i<numareas ; i++)
	{
		if (map_areas[i].floodvalid == floodvalid)
			continue;		// already flooded into
		floodnum++;
		FloodArea_r (i, floodnum);
	}
}

void CM_SetAreaPortalState (int portalnum, int area, int otherarea, qboolean open)
{
	if (portalnum >= MAX_CM_AREAPORTALS)
		Com_Error (ERR_DROP, "areaportal >= MAX_CM_AREAPORTALS");

	if (!map_areaportals[portalnum].area)
	{
		// add new areaportal if it doesn't exist
		if (!CM_AddAreaPortal (portalnum, area, otherarea))
			return;
	}

	map_areaportals[portalnum].open = open;
	FloodAreaConnections ();
}

qboolean CM_AreasConnected (int area1, int area2)
{
	if (cm_noAreas->value)
		return qtrue;

	if (area1 > numareas || area2 > numareas)
		Com_Error (ERR_DROP, "CM_AreasConnected: area > numareas");

	if (map_areas[area1].floodnum == map_areas[area2].floodnum)
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
int CM_WriteAreaBits (qbyte *buffer, int area)
{
	int		i;
	int		bytes;

	bytes = (numareas+7)>>3;

	if (cm_noAreas->value)
	{	// for debugging, send everything
		memset (buffer, 255, bytes);
	}
	else
	{
		memset (buffer, 0, bytes);

		for (i=1 ; i<numareas ; i++)
		{
			if (!area || CM_AreasConnected ( i, area ) || i == area)
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
void CM_MergeAreaBits (qbyte *buffer, int area)
{
	int		i;

	for (i=1 ; i<numareas ; i++)
	{
		if ( CM_AreasConnected ( i, area ) || i == area)
			buffer[i>>3] |= 1<<(i&7);
	}
}

/*
===================
CM_WritePortalState

Writes the portal state to a savegame file
===================
*/
void CM_WritePortalState (FILE *f)
{
	int i, j;

	fwrite ( &numareaportals, sizeof(int), 1, f );

	for ( i = 1; i < MAX_CM_AREAPORTALS; i++ ) {
		if ( map_areaportals[i].area ) {
			fwrite ( &i, sizeof(int), 1, f );
			fwrite ( &map_areaportals[i], sizeof(map_areaportals[0]), 1, f );
		}
	}

	fwrite ( &numareas, sizeof(int), 1, f );

	for ( i = 1; i < numareas; i++ ) {
		fwrite ( &map_areas[i].numareaportals, sizeof(int), 1, f );

		for ( j = 0; j < map_areas[i].numareaportals; j++ ) {
			fwrite ( &map_areas[i].areaportals[j], sizeof(int), 1, f );
		}
	}
}

/*
===================
CM_ReadPortalState

Reads the portal state from a savegame file
and recalculates the area connections
===================
*/
void CM_ReadPortalState (FILE *f)
{
	int i, j;

	fread ( &numareaportals, sizeof(int), 1, f );
	for ( i = 1; i < numareaportals; i++ ) {
		fread ( &j, sizeof(int), 1, f );
		fread ( &map_areaportals[j], sizeof(map_areaportals[0]), 1, f );
	}

	fread ( &numareas, sizeof(int), 1, f );

	for ( i = 1; i < numareas; i++ ) {
		fread ( &map_areas[i].numareaportals, sizeof(int), 1, f );

		for ( j = 0; j < map_areas[i].numareaportals; j++ ) {
			fread ( &map_areas[i].areaportals[j], sizeof(int), 1, f );
		}
	}

	FloodAreaConnections ();
}

/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
qboolean CM_HeadnodeVisible (int nodenum, qbyte *visbits)
{
	int		leafnum;
	int		cluster;
	cnode_t	*node;

	if (nodenum < 0)
	{
		leafnum = -1-nodenum;
		cluster = map_leafs[leafnum].cluster;
		if (cluster == -1)
			return qfalse;
		if (visbits[cluster>>3] & (1<<(cluster&7)))
			return qtrue;
		return qfalse;
	}

	node = &map_nodes[nodenum];
	if (CM_HeadnodeVisible(node->children[0], visbits))
		return qtrue;
	return CM_HeadnodeVisible(node->children[1], visbits);
}
