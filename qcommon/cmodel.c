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

typedef struct
{
	cplane_t	*plane;
	int			children[2];		// negative numbers are leafs
} cnode_t;

typedef struct
{
	cplane_t		*plane;
	csurface_t		*surface;
} cbrushside_t;

typedef struct
{
	vec3_t		xyz;
	vec3_t		normal;
} cvertex_t;

typedef struct
{
	int			shadernum;
	int			facetype;
	int			numverts;
	int			firstvert;
	int			mesh_cp[2];

	vec3_t		mins, maxs;
} cface_t;

typedef struct
{
	int			contents;
	int			cluster;
	int			area;
	int			firstleafface;
	int			numleaffaces;
	int			firstleafbrush;
	int			numleafbrushes;

	int			numfacets;
	int			firstfacet;
} cleaf_t;

typedef struct
{
	int			contents;
	int			numsides;
	int			firstbrushside;
	int			checkcount;		// to avoid repeated testings
} cbrush_t;

typedef struct
{
	int			numareaportals[MAX_MAP_AREAS];
} carea_t;

int			checkcount;

char		map_name[MAX_QPATH];

int			numbrushsides;
cbrushside_t map_brushsides[MAX_MAP_BRUSHSIDES];

int			numshaderrefs;
csurface_t	map_surfaces[MAX_MAP_SHADERS];

int			numplanes;
cplane_t	map_planes[MAX_MAP_PLANES+6];		// extra for box hull

int			numnodes;
cnode_t		map_nodes[MAX_MAP_NODES+6];		// extra for box hull

int			numleafs = 1;	// allow leaf funcs to be called without a map
cleaf_t		map_leafs[MAX_MAP_LEAFS];

int			numleafbrushes;
int			map_leafbrushes[MAX_MAP_LEAFBRUSHES];

int			numverts;
cvertex_t	map_verts[MAX_MAP_VERTS];

int			numfaces;
cface_t		map_faces[MAX_MAP_FACES];

int			numleaffaces;
cface_t		*map_leaffaces[MAX_MAP_LEAFFACES];

int			numcmodels;
cmodel_t	map_cmodels[MAX_MAP_MODELS];

int			numbrushes;
cbrush_t	map_brushes[MAX_MAP_BRUSHES];

int			numshaderrefs;
shaderref_t	map_shaderrefs[MAX_MAP_SHADERS];

byte		map_hearability[MAX_MAP_VISIBILITY];

int			numvisibility;
byte		map_visibility[MAX_MAP_VISIBILITY];
dvis_t		*map_pvs = (dvis_t *)map_visibility;

dvis_t		*map_phs = (dvis_t *)map_hearability;

byte		nullrow[MAX_MAP_LEAFS/8];

int			numentitychars;
char		map_entitystring[MAX_MAP_ENTSTRING];

int			numareas = 1;
carea_t		map_areas[MAX_MAP_AREAS];

csurface_t	nullsurface;

int			emptyleaf;

cvar_t		*map_noareas;

int			numfacets;
cbrushside_t map_facets[MAX_MAP_BRUSHSIDES];

void	CM_InitBoxHull (void);
void	CM_CreateMeshes (void);
void	FloodAreaConnections (void);

int		c_pointcontents;
int		c_traces, c_brush_traces;


/*
===============================================================================

					MAP LOADING

===============================================================================
*/

byte	*cmod_base;

/*
=================
CMod_LoadSubmodels
=================
*/
void CMod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	cmodel_t	*out;
	cleaf_t		*bleaf;
	int			*leafbrush;
	int			i, j, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no models");
	if (count > MAX_MAP_MODELS)
		Com_Error (ERR_DROP, "Map has too many models");

	numcmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out = &map_cmodels[i];

		if ( !i ) {
			out->headnode = 0;
		} else {
			out->headnode = -1 - numleafs;

			bleaf = &map_leafs[numleafs++];
			bleaf->numleafbrushes = LittleLong ( in->numbrushes );
			bleaf->firstleafbrush = numleafbrushes;
			bleaf->contents = 0;

			leafbrush = &map_leafbrushes[numleafbrushes];
			for ( j = 0; j < bleaf->numleafbrushes; j++, leafbrush++ ) {
				*leafbrush = LittleLong ( in->firstbrush ) + j;
				bleaf->contents |= map_brushes[*leafbrush].contents;
			}

			numleafbrushes += bleaf->numleafbrushes;
		}

		for (j=0 ; j<3 ; j++)
		{ // spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
		}
	}
}


/*
=================
CMod_LoadSurfaces
=================
*/
void CMod_LoadSurfaces (lump_t *l)
{
	shaderref_t	*in;
	csurface_t	*out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no surfaces");
	if (count > MAX_MAP_SHADERS)
		Com_Error (ERR_DROP, "Map has too many surfaces");

	numshaderrefs = count;
	out = map_surfaces;

	for ( i=0 ; i<count ; i++, in++, out++ ) {
		out->flags = LittleLong ( in->flags );
		out->contents = LittleLong ( in->contents );
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
	int			child;
	cnode_t		*out;
	int			i, j, count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map has no nodes");
	if (count > MAX_MAP_NODES)
		Com_Error (ERR_DROP, "Map has too many nodes");

	out = map_nodes;

	numnodes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->plane = map_planes + LittleLong(in->planenum);
		for (j=0 ; j<2 ; j++)
		{
			child = LittleLong (in->children[j]);
			out->children[j] = child;
		}
	}

}

/*
=================
CMod_LoadShaders

=================
*/
void CMod_LoadShaders (lump_t *l)
{
	shaderref_t *in;
	shaderref_t *out;
	int 	i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);
	out = map_shaderrefs;	

	for ( i=0 ; i<count ; i++, in++, out++)
	{
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
	cvertex_t	*out;
	int			i, j, count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_VERTS)
		Com_Error (ERR_DROP, "Map has too many vertexes");

	out = map_verts;
	numverts = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		for ( j = 0; j < 3; j++ ) 
		{
			out->xyz[j] = LittleFloat ( in->point[j] );
			out->normal[j] = LittleFloat ( in->normal[j] );
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
	int			i, count, j;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_FACES)
		Com_Error (ERR_DROP, "Map has too many faces");

	out = map_faces;
	numfaces = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->firstvert = LittleLong( in->firstvert );
		out->numverts = LittleLong ( in->numverts );
		out->shadernum = LittleLong ( in->shadernum );
		out->facetype = LittleLong ( in->facetype );

		for ( j = 0; j < 2; j++ ) {
			out->mesh_cp[j] = LittleLong ( in->mesh_cp[j] );
		}

		for ( j = 0; j < 3; j++ ) {
			out->mins[j] = LittleFloat ( in->mins[j] );
			out->maxs[j] = LittleFloat ( in->maxs[j] );
		}
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
	shaderref_t *shaderref;
	int			i, count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_BRUSHES)
		Com_Error (ERR_DROP, "Map has too many brushes");

	out = map_brushes;

	numbrushes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		shaderref = map_shaderrefs + LittleLong ( in->shadernum );
		out->contents = shaderref->contents;
		out->firstbrushside = LittleLong ( in->firstside );
		out->numsides = LittleLong ( in->numsides );
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
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leafs");

	// need to save space for box planes
	if (count > MAX_MAP_LEAFS)
		Com_Error (ERR_DROP, "Map has too many leafs");

	out = map_leafs;	
	numleafs = count;
	emptyleaf = -1;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->cluster = LittleLong ( in->cluster );
		out->area = LittleLong ( in->area ) + 1;
		out->firstleafface = LittleLong ( in->firstleafface );
		out->numleaffaces = LittleLong ( in->numleaffaces );
		out->firstleafbrush = LittleLong ( in->firstleafbrush );
		out->numleafbrushes = LittleLong ( in->numleafbrushes );
		out->contents = 0;

		for ( j=0 ; j<out->numleafbrushes ; j++)
		{
			brush = &map_brushes[map_leafbrushes[out->firstleafbrush + j]];
			out->contents |= brush->contents;
		}

		if ( out->area >= numareas ) {
			numareas = out->area + 1;
		}

		if ( !out->contents ) {
			emptyleaf = i;
		}
	}

	// if map doesn't have an empty leaf - force one
	if ( emptyleaf == -1 ) {
		if (numleafs >= MAX_MAP_LEAFS-1)
			Com_Error (ERR_DROP, "Map does not have an empty leaf");

		out->cluster = -1;
		out->area = -1;
		out->numleafbrushes = 0;
		out->contents = 0;
		out->firstleafbrush = 0;

		Com_DPrintf ( "Forcing an empty leaf: %i\n", numleafs );
		emptyleaf = numleafs++;
	}
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
	int			bits;
	byte		type;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");
	// need to save space for box planes
	if (count > MAX_MAP_PLANES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_planes;	
	numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		type = PLANE_ANYZ;

		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
			if (out->normal[j] == 1.0f)
				type = j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = type;
		out->signbits = bits;
	}
}

/*
=================
CMod_LoadLeafFaces
=================
*/
void CMod_LoadLeafFaces (lump_t *l)
{
	int			i;
	cface_t		**out;
	int		 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no faces");
	// need to save space for box planes
	if (count > MAX_MAP_LEAFFACES)
		Com_Error (ERR_DROP, "Map has too many leaffaces");

	out = map_leaffaces;
	numleaffaces = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		*out = map_faces + LittleLong (*in);
}

/*
=================
CMod_LoadLeafBrushes
=================
*/
void CMod_LoadLeafBrushes (lump_t *l)
{
	int			i;
	int			*out;
	int		 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");
	// need to save space for box planes
	if (count > MAX_MAP_LEAFBRUSHES)
		Com_Error (ERR_DROP, "Map has too many leafbrushes");

	out = map_leafbrushes;
	numleafbrushes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		*out = LittleLong (*in);
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

	// need to save space for box planes
	if (count > MAX_MAP_BRUSHSIDES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_brushsides;	
	numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->plane = map_planes + LittleLong (in->planenum);
		j = LittleLong (in->shadernum);
		if (j >= numshaderrefs)
			Com_Error (ERR_DROP, "Bad brushside texinfo");
		out->surface = &map_surfaces[j];
	}
}

/*
=================
CMod_LoadVisibility
=================
*/
void CMod_LoadVisibility (lump_t *l)
{
	numvisibility = l->filelen;
	if (l->filelen > MAX_MAP_VISIBILITY)
		Com_Error (ERR_DROP, "Map has too large visibility lump");

	memcpy (map_visibility, cmod_base + l->fileofs, l->filelen);

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
	if (l->filelen > MAX_MAP_ENTSTRING)
		Com_Error (ERR_DROP, "Map has too large entity lump");

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
	byte	*scan;
	int		count, vcount;

	Com_DPrintf ("Building PHS...\n");

	rowwords = map_pvs->rowsize / sizeof(long);
	rowbytes = map_pvs->rowsize;

	memset ( map_phs, 0, MAX_MAP_VISIBILITY );

	map_phs->rowsize = map_pvs->rowsize;
	map_phs->numclusters = map_pvs->numclusters;

	vcount = 0;
	for (i=0 ; i<map_pvs->numclusters ; i++)
	{
		scan = CM_ClusterPVS ( i );
		for (j=0 ; j<numleafs ; j++)
		{
			if ( scan[j>>3] & (1<<(j&7)) )
			{
				vcount++;
			}
		}
	}

	count = 0;
	scan = (byte *)map_pvs->data;
	dest = (unsigned *)((byte *)map_phs + 8);

	for (i=0 ; i<map_pvs->numclusters ; i++, dest += rowwords, scan += rowbytes)
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
				// or this pvs row into the phs
				// +1 because pvs is 1 based
				index = (j<<3) + k + 1;
				if (index >= numleafs)
					continue;
				src = (unsigned *)((byte*)map_pvs->data) + index*rowwords;
				for (l=0 ; l<rowwords ; l++)
					dest[l] |= src[l];
			}
		}

		if (i == 0)
			continue;
		for (j=0 ; j<numleafs ; j++)
			if ( ((byte *)dest)[j>>3] & (1<<(j&7)) )
				count++;
	}

	Com_DPrintf ("Average leafs visible / hearable / total: %i / %i / %i\n"
		, vcount/numleafs, count/numleafs, numleafs);
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

	map_noareas = Cvar_Get ("map_noareas", "0", 0);

	if (  !strcmp (map_name, name) && (clientload || !Cvar_VariableValue ("flushmap")) )
	{
		*checksum = last_checksum;
		if (!clientload)
			FloodAreaConnections ();
		return &map_cmodels[0];		// still have the right version
	}

	// free old stuff
	numplanes = 0;
	numnodes = 0;
	numleafs = 0;
	numcmodels = 0;
	numvisibility = 0;
	numentitychars = 0;
	numfacets = 0;
	map_entitystring[0] = 0;
	map_name[0] = 0;

	if (!name || !name[0])
	{
		numleafs = 1;
		numareas = 1;
		*checksum = 0;
		return &map_cmodels[0];			// cinematic servers won't have anything at all
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

	if (header.version != BSPVERSION)
		Com_Error (ERR_DROP, "CMod_LoadBrushModel: %s has wrong version number (%i should be %i)"
		, name, header.version, BSPVERSION);

	cmod_base = (byte *)buf;

	// load into heap
	CMod_LoadSurfaces (&header.lumps[LUMP_SHADERREFS]);
	CMod_LoadPlanes (&header.lumps[LUMP_PLANES]);
	CMod_LoadLeafFaces (&header.lumps[LUMP_LEAFFACES]);
	CMod_LoadLeafBrushes (&header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadShaders (&header.lumps[LUMP_SHADERREFS]);
	CMod_LoadBrushes (&header.lumps[LUMP_BRUSHES]);
	CMod_LoadBrushSides (&header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadVertexes (&header.lumps[LUMP_VERTEXES]);
	CMod_LoadFaces (&header.lumps[LUMP_FACES]);
	CMod_LoadLeafs (&header.lumps[LUMP_LEAFS]);
	CMod_LoadSubmodels (&header.lumps[LUMP_MODELS]);
	CMod_LoadNodes (&header.lumps[LUMP_NODES]);
	CMod_LoadVisibility (&header.lumps[LUMP_VISIBILITY]);
	CMod_LoadEntityString (&header.lumps[LUMP_ENTITIES]);

	FS_FreeFile (buf);

	CM_CreateMeshes ();
	CM_InitBoxHull ();
	FloodAreaConnections ();

	CM_CalcPHS ();

	memset ( nullrow, 0, MAX_MAP_LEAFS / 8 );

	strcpy ( map_name, name );

	return &map_cmodels[0];
}

/*
==================
CM_InlineModel
==================
*/
cmodel_t	*CM_InlineModel (char *name)
{
	int		num;

	if (!name || name[0] != '*')
		Com_Error (ERR_DROP, "CM_InlineModel: bad name");
	num = atoi (name+1);
	if (num < 1 || num >= numcmodels)
		Com_Error (ERR_DROP, "CM_InlineModel: bad number");

	return &map_cmodels[num];
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

//=======================================================================

#define LEVEL_WIDTH(lvl) ((1 << (lvl+1)) + 1)

int r_imaxmeshlevel = 4;
int r_isubdivisions = 4;

static int CM_MeshFindLevel (vec3_t *v)
{
    int level;
    vec3_t a, b, dist;

    // subdivide on the left until tolerance is reached
    for (level = 0; level < r_imaxmeshlevel-1; level++)
    {
		// subdivide on the left
		VectorAvg( v[0], v[1], a );
		VectorAvg( v[1], v[2], b );
		VectorAvg( a, b, v[2] );

		// find distance moved
		VectorSubtract( v[2], v[1], dist );

		// check for tolerance
		if (DotProduct( dist, dist ) < r_isubdivisions * r_isubdivisions)
			break;

		// insert new middle vertex
		VectorCopy( a, v[1] );
    }

    return level;
}

static void CM_MeshFindSize (int *numcp, vec3_t *cp, int *size)
{
    int u, v, found, level;
    float *a, *b;
    vec3_t test[3];
    
    // find non-coincident pairs in u direction
    found = 0;
    for (v=0; v < numcp[1]; v++)
    {
		for (u=0; u < numcp[0]-1; u += 2)
		{
			a = cp[v * numcp[0] + u];
			b = cp[v * numcp[0] + u + 2];
			if (!VectorCompare( a, b ))
			{
				found = 1;
				break;
			}
		}
		if (found) break;
    }
    if (!found) Com_Error( ERR_DROP, "Bad mesh control points" );

    /* Find subdivision level in u */
    VectorCopy( a, test[0] );
    VectorCopy( (a+3), test[1] );
    VectorCopy( b, test[2] );
    level = CM_MeshFindLevel( test );
    size[0] = (LEVEL_WIDTH(level) - 1) * ((numcp[0]-1) / 2) + 1;
    
    // find non-coincident pairs in v direction
    found = 0;
    for (u=0; u < numcp[0]; u++)
    {
		for (v=0; v < numcp[1]-1; v += 2)
		{
			a = cp[v * numcp[0] + u];
			b = cp[(v + 2) * numcp[0] + u];
			if (!VectorCompare( a, b ))
			{
				found = 1;
				break;
			}
		}
		if (found) break;
    }
    if (!found) Com_Error( ERR_DROP, "Bad mesh control points" );

    // find subdivision level in v
    VectorCopy( a, test[0] );
    VectorCopy( (a+numcp[0]*3), test[1] );
    VectorCopy( b, test[2] );
    level = CM_MeshFindLevel(test);
    size[1] = (LEVEL_WIDTH(level) - 1)* ((numcp[1]-1) / 2) + 1;    
}

static void CM_MeshFillCurve_3 (int numcp, int size, int stride, vec3_t *p)
{
    int step, halfstep, i, mid;
    vec3_t a, b;

    step = (size-1) / (numcp-1);

    while (step > 0)
    {
		halfstep = step / 2;
		for (i = 0; i < size-1; i += step*2)
		{
			mid = (i+step)*stride;
			VectorAvg( p[i*stride], p[mid], a );
			VectorAvg( p[mid], p[(i+step*2)*stride], b );
			VectorAvg( a, b, p[mid] );

			if (halfstep > 0)
			{
				VectorCopy( a, p[(i+halfstep)*stride] );
				VectorCopy( b, p[(i+3*halfstep)*stride] );
			}
		}
		
		step /= 2;
    }
}

static void CM_MeshFillPatch_3 (int *numcp, int *size, vec3_t *p)
{
    int step, u, v;

    // fill in control points in v direction
    step = (size[0]-1) / (numcp[0]-1);    
    for (u = 0; u < size[0]; u += step)
    {
		CM_MeshFillCurve_3( numcp[1], size[1], size[0], p + u );
    }

    // fill in the rest in the u direction
    for (v = 0; v < size[1]; v++)
    {
		CM_MeshFillCurve_3( numcp[0], size[0], 1, p + v * size[0] );
    }
}

void CM_CreateMesh (cleaf_t *leaf, cface_t *face)
{
    int step[2], size[2], i, u, v, p, b, bits, type;
	int newnumverts, numverts = face->numverts;
	cvertex_t *vert;
	vec3_t points[MAX_VERTS], normals[MAX_VERTS], normals2[MAX_VERTS];
	cplane_t brushsides[MAX_VERTS];
	cbrushside_t *brushside;

	vert = map_verts + face->firstvert;
	for ( i = 0; i < numverts; i++, vert++ )
		VectorCopy ( vert->xyz, points[i] );
	
// find the degree of subdivision in the u and v directions
	CM_MeshFindSize ( face->mesh_cp, points, size );
	newnumverts = size[0] * size[1];

// fill in sparse mesh control points
    step[0] = (size[0]-1) / (face->mesh_cp[0]-1);
    step[1] = (size[1]-1) / (face->mesh_cp[1]-1);

	vert = map_verts + face->firstvert;
    for (v = 0; v < size[1]; v += step[1])
    {
		for (u = 0; u < size[0]; u += step[0])
		{
			p = v * size[0] + u;
			VectorCopy ( vert->xyz, points[p] );
			VectorCopy ( vert->normal, normals[p] );
			VectorCopy ( vert->normal, normals2[p] );
			vert++;
		}
    }

// fill in each mesh
	CM_MeshFillPatch_3 ( face->mesh_cp, size, normals );
	CM_MeshFillPatch_3 ( face->mesh_cp, size, normals2 );
	CM_MeshFillPatch_3 ( face->mesh_cp, size, points );

	b = 0;
	newnumverts = face->numverts;

	memset ( brushsides, 0, sizeof(cplane_t)*MAX_VERTS );

	for ( i = 0; i < newnumverts; i++ )
		VectorNormalize ( normals[i] );

	for ( i = 0; i < newnumverts; i++ )
	{
//		for ( v = 0; v < b; v++ ) 
//		{
//			if ( VectorCompare( normals[i], brushsides[v].normal ) )
//				break;
//		}
/*
		if ( v == b )
		{
			vert = map_verts + face->firstvert;
			for ( v = 0; v < numverts; v++, vert++ )
			{
				if ( VectorCompare( normals2[i], vert->normal) )
					break;
			}
		}
*/
		v = b;
		if ( v == b )
		{
			bits = 0;
			type = PLANE_ANYZ;

			for (p=0 ; p<3 ; p++)
			{
				if (normals[i][p] < 0)
					bits |= 1<<p;
				if (normals[i][p] == 1.0f)
					type = p;
			}

			VectorCopy ( normals[i], brushsides[b].normal );
			brushsides[b].dist = DotProduct ( points[i], normals[i] );
			brushsides[b].type = type;
			brushsides[b].signbits = bits;
			b++;
		}
	}

	for ( i = 0; i < b; i ++ )
	{
		brushside = &map_facets[numfacets++];
		brushside->surface = &map_surfaces[face->shadernum];
		brushside->plane = &map_planes[numplanes++];
		*brushside->plane = brushsides[i];
	}

	leaf->numfacets += b;
}


void CM_CreateMeshes (void)
{
	int i, j;
	cleaf_t *leaf;
	cface_t **mark, *face;

	leaf = map_leafs;
	for ( i = 1; i < numleafs; i++, leaf++ )
	{
		leaf->numfacets = 0;
		leaf->firstfacet = numfacets;

		mark = map_leaffaces + leaf->firstleafface;
		for ( j = 0; j < leaf->numleaffaces; j++)
		{
			face = *mark++;
//			if ( face->facetype == FACETYPE_MESH )
//				CM_CreateMesh ( leaf, face );
		}

//		Com_Printf ( "leaf %i, %i facets, %i firstfacet\n", i, leaf->numfacets, leaf->firstfacet );
	}
}

//=======================================================================

cplane_t	*box_planes;
int			box_headnode;
cbrush_t	*box_brush;
cleaf_t		*box_leaf;

/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_InitBoxHull (void)
{
	int			i;
	int			side;
	cnode_t		*c;
	cplane_t	*p;
	cbrushside_t	*s;

	box_headnode = numnodes;
	box_planes = &map_planes[numplanes];
	if (numnodes+6 > MAX_MAP_NODES
		|| numbrushes+1 > MAX_MAP_BRUSHES
		|| numleafbrushes+1 > MAX_MAP_LEAFBRUSHES
		|| numbrushsides+6 > MAX_MAP_BRUSHSIDES
		|| numplanes+12 > MAX_MAP_PLANES)
		Com_Error (ERR_DROP, "Not enough room for box tree");

	box_brush = &map_brushes[numbrushes];
	box_brush->numsides = 6;
	box_brush->firstbrushside = numbrushsides;
	box_brush->contents = CONTENTS_BODY;

	box_leaf = &map_leafs[numleafs];
	box_leaf->contents = CONTENTS_BODY;
	box_leaf->firstleafbrush = numleafbrushes;
	box_leaf->numleafbrushes = 1;

	map_leafbrushes[numleafbrushes] = numbrushes;

	for (i=0 ; i<6 ; i++)
	{
		side = i&1;

		// brush sides
		s = &map_brushsides[numbrushsides+i];
		s->plane = map_planes + (numplanes+i*2+side);
		s->surface = &nullsurface;

		// nodes
		c = &map_nodes[box_headnode+i];
		c->plane = map_planes + (numplanes+i*2);
		c->children[side] = -1 - emptyleaf;
		if (i != 5)
			c->children[side^1] = box_headnode+i + 1;
		else
			c->children[side^1] = -1 - numleafs;

		// planes
		p = &box_planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = 1;

		p = &box_planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = -1;
	}	
}


/*
===================
CM_HeadnodeForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
int	CM_HeadnodeForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	return box_headnode;
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
		
		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
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
	cplane_t	*plane;
	cnode_t		*node;
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
		plane = node->plane;

		s = BOX_ON_PLANE_SIDE(leaf_mins, leaf_maxs, plane);
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

int	CM_BoxLeafnums_headnode (vec3_t mins, vec3_t maxs, int *list, int listsize, int headnode, int *topnode)
{
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	CM_BoxLeafnums_r (headnode);

	if (topnode)
		*topnode = leaf_topnode;

	return leaf_count;
}

int	CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode)
{
	return CM_BoxLeafnums_headnode (mins, maxs, list,
		listsize, map_cmodels[0].headnode, topnode);
}



/*
==================
CM_PointContents

==================
*/
int CM_PointContents (vec3_t p, int headnode)
{
	int				i, j, contents;
	cleaf_t			*leaf;
	cplane_t		*plane;
	cbrush_t		*brush;
	cbrushside_t	*brushside;

	if (!numnodes)	// map not loaded
		return 0;

	i = CM_PointLeafnum_r (p, headnode);
	leaf = &map_leafs[i];

	contents = 0;

	for (i = 0; i < leaf->numleafbrushes; i++)
	{
		brush = &map_brushes[map_leafbrushes[leaf->firstleafbrush + i]];
		
		for ( j = 0; j < brush->numsides; j++ )
		{
			brushside = &map_brushsides[brush->firstbrushside + j];
			plane = brushside->plane;

			if (plane->type < 3)
			{
				if (p[plane->type] > plane->dist)
					break;
			}
			else
			{
				if (DotProduct (p, plane->normal) > plane->dist)
					break;
			}
		}

		if (j == brush->numsides) 
			contents |= brush->contents;
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
int	CM_TransformedPointContents (vec3_t p, int headnode, vec3_t origin, vec3_t angles)
{
	vec3_t		p_l;
	vec3_t		temp;
	vec3_t		forward, right, up;

	if (!numnodes)	// map not loaded
		return 0;

	// subtract origin offset
	VectorSubtract (p, origin, p_l);

	// rotate start and end into the models frame of reference
	if (headnode != box_headnode && 
	(angles[0] || angles[1] || angles[2]) )
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (p_l, temp);
		p_l[0] = DotProduct (temp, forward);
		p_l[1] = -DotProduct (temp, right);
		p_l[2] = DotProduct (temp, up);
	}

	return CM_PointContents (p_l, headnode);
}


/*
===============================================================================

BOX TRACING

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

vec3_t	trace_start, trace_end;
vec3_t	trace_mins, trace_maxs;
vec3_t	trace_extents;

trace_t	trace_trace;
int		trace_contents;
qboolean	trace_ispoint;		// optimized case

/*
================
CM_ClipBoxToBrush
================
*/
void CM_ClipBoxToFacet (vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2,
					  trace_t *trace, cbrushside_t	*side)
{
	int			j;
	cplane_t	*plane, *clipplane;
	float		dist;
	float		enterfrac, leavefrac;
	vec3_t		ofs;
	float		d1, d2;
	qboolean	getout, startout;
	float		f;
	cbrushside_t *leadside;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;
	getout = false;
	startout = false;
	leadside = NULL;
	plane = side->plane;

	if (!trace_ispoint)
	{	// general box case
		
		// push the plane out apropriately for mins/maxs
		
		// FIXME: use signbits into 8 way lookup for each mins/maxs
		for (j=0 ; j<3 ; j++)
		{
			if (plane->normal[j] < 0)
				ofs[j] = maxs[j];
			else
				ofs[j] = mins[j];
		}
		
		if ( plane->type < 3 ) {
			dist = ofs[plane->type];
		} else {
			dist = DotProduct (ofs, plane->normal);
		}
		
		dist = plane->dist - dist;
	}
	else
	{	// special point case
		dist = plane->dist;
	}
	
	if ( plane->type < 3 ) {
		d1 = p1[plane->type] - dist;
		d2 = p2[plane->type] - dist;
	} else {
		d1 = DotProduct (p1, plane->normal) - dist;
		d2 = DotProduct (p2, plane->normal) - dist;
	}
	
	if (d2 > 0)
		getout = true;	// endpoint is not in solid
	if (d1 > 0)
		startout = true;
	
	// if completely in front of face, no intersection
	if (d1 > 0 && d2 >= d1)
		return;
	
	if (d1 <= 0 && d2 <= 0)
		return;
	
	// crosses face
	if (d1 > d2)
	{	// enter
		f = (d1-DIST_EPSILON) / (d1-d2);
		if (f > enterfrac)
		{
			enterfrac = f;
			clipplane = plane;
			leadside = side;
		}
	}
	else
	{	// leave
		f = (d1+DIST_EPSILON) / (d1-d2);
		if (f < leavefrac)
			leavefrac = f;
	}

	if (!startout)
	{	// original point was inside brush
		trace->startsolid = true;
		if (!getout)
			trace->allsolid = true;
		return;
	}
	if (enterfrac < leavefrac)
	{
		if (enterfrac > -1 && enterfrac < trace->fraction)
		{
			if (enterfrac < 0)
				enterfrac = 0;
			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = leadside->surface;
			trace->contents = leadside->surface->contents;
		}
	}
}

/*
================
CM_ClipBoxToBrush
================
*/
void CM_ClipBoxToBrush (vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2,
					  trace_t *trace, cbrush_t *brush)
{
	int			i, j;
	cplane_t	*plane, *clipplane;
	float		dist;
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

	getout = false;
	startout = false;
	leadside = NULL;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = &map_brushsides[brush->firstbrushside+i];
		plane = side->plane;

		// FIXME: special case for axial

		if (!trace_ispoint)
		{	// general box case

			// push the plane out apropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}

			if ( plane->type < 3 ) {
				dist = ofs[plane->type];
			} else {
				dist = DotProduct (ofs, plane->normal);
			}

			dist = plane->dist - dist;
		}
		else
		{	// special point case
			dist = plane->dist;
		}

		if ( plane->type < 3 ) {
			d1 = p1[plane->type] - dist;
			d2 = p2[plane->type] - dist;
		} else {
			d1 = DotProduct (p1, plane->normal) - dist;
			d2 = DotProduct (p2, plane->normal) - dist;
		}

		if (d2 > 0)
			getout = true;	// endpoint is not in solid
		if (d1 > 0)
			startout = true;

		// if completely in front of face, no intersection
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		// crosses face
		if (d1 > d2)
		{	// enter
			f = (d1-DIST_EPSILON) / (d1-d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				clipplane = plane;
				leadside = side;
			}
		}
		else
		{	// leave
			f = (d1+DIST_EPSILON) / (d1-d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout)
	{	// original point was inside brush
		trace->startsolid = true;
		if (!getout)
			trace->allsolid = true;
		return;
	}
	if (enterfrac < leavefrac)
	{
		if (enterfrac > -1 && enterfrac < trace->fraction)
		{
			if (enterfrac < 0)
				enterfrac = 0;
			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = leadside->surface;
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
	float		dist;
	vec3_t		ofs;
	float		d1;
	cbrushside_t	*side;

	if (!brush->numsides)
		return;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = &map_brushsides[brush->firstbrushside+i];
		plane = side->plane;

		// FIXME: special case for axial

		// general box case

		// push the plane out apropriately for mins/maxs

		// FIXME: use signbits into 8 way lookup for each mins/maxs
		for (j=0 ; j<3 ; j++)
		{
			if (plane->normal[j] < 0)
				ofs[j] = maxs[j];
			else
				ofs[j] = mins[j];
		}

		if ( plane->type < 3 ) {
			dist = plane->dist - ofs[plane->type];
			d1 = p1[plane->type] - dist;
		} else {
			dist = plane->dist - DotProduct (ofs, plane->normal);
			d1 = DotProduct (p1, plane->normal) - dist;
		}

		// if completely in front of face, no intersection
		if (d1 > 0)
			return;
	}

	// inside this brush
	trace->startsolid = trace->allsolid = true;
	trace->fraction = 0;
	trace->contents = brush->contents;
}


/*
================
CM_TraceToLeaf
================
*/
void CM_TraceToLeaf (int leafnum)
{
	int			k;
	int			brushnum;
	cleaf_t		*leaf;
	cbrush_t	*b;

	leaf = &map_leafs[leafnum];
	if ( !(leaf->contents & trace_contents))
		return;

	// trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush+k];

		b = &map_brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush in another leaf
		b->checkcount = checkcount;
		if ( !(b->contents & trace_contents))
			continue;
		CM_ClipBoxToBrush (trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}

	if ( !leaf->numfacets )
		return;

	for (k=0 ; k<leaf->numfacets ; k++)
	{
//		CM_ClipBoxToFacet ( trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, &map_facets[leaf->firstfacet+k] );
//		Com_Printf ( "%i\n", map_facets[leaf->firstfacet+k].surface->contents );
	}
}


/*
================
CM_TestInLeaf
================
*/
void CM_TestInLeaf (int leafnum)
{
	int			k;
	int			brushnum;
	cleaf_t		*leaf;
	cbrush_t	*b;

	leaf = &map_leafs[leafnum];

	if ( !(leaf->contents & trace_contents))
		return;

	// trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush+k];

		b = &map_brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush in another leaf
		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;
		CM_TestBoxInBrush (trace_mins, trace_maxs, trace_start, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}

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

	if (trace_trace.fraction <= p1f)
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
	if (frac < 0)
		frac = 0;
	if (frac > 1)
		frac = 1;
		
	midf = p1f + (p2f - p1f)*frac;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	CM_RecursiveHullCheck (node->children[side], p1f, midf, p1, mid);


	// go past the node
	if (frac2 < 0)
		frac2 = 0;
	if (frac2 > 1)
		frac2 = 1;
		
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
trace_t		CM_BoxTrace (vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  int headnode, int brushmask)
{
	int		i;

	checkcount++;		// for multi-check avoidance

	c_traces++;			// for statistics, may be zeroed

	// fill in a default trace
	memset (&trace_trace, 0, sizeof(trace_trace));
	trace_trace.fraction = 1;
	trace_trace.surface = &nullsurface;

	if (!numnodes)	// map not loaded
		return trace_trace;

	trace_contents = brushmask;
	VectorCopy (start, trace_start);
	VectorCopy (end, trace_end);
	VectorCopy (mins, trace_mins);
	VectorCopy (maxs, trace_maxs);

	//
	// check for position test special case
	//
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		int		leafs[1024];
		int		i, numleafs;
		vec3_t	c1, c2;
		int		topnode;

		VectorAdd (start, mins, c1);
		VectorAdd (start, maxs, c2);
		for (i=0 ; i<3 ; i++)
		{
			c1[i] -= 1;
			c2[i] += 1;
		}

		numleafs = CM_BoxLeafnums_headnode (c1, c2, leafs, 1024, headnode, &topnode);
		for (i=0 ; i<numleafs ; i++)
		{
			CM_TestInLeaf (leafs[i]);
			if (trace_trace.allsolid)
				break;
		}
		VectorCopy (start, trace_trace.endpos);
		return trace_trace;
	}

	//
	// check for point special case
	//
	if (mins[0] == 0 && mins[1] == 0 && mins[2] == 0
		&& maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0)
	{
		trace_ispoint = true;
		VectorClear (trace_extents);
	}
	else
	{
		trace_ispoint = false;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	//
	// general sweeping through world
	//
	CM_RecursiveHullCheck (headnode, 0, 1, start, end);

	if (trace_trace.fraction == 1)
	{
		VectorCopy (end, trace_trace.endpos);
	}
	else
	{
		for (i=0 ; i<3 ; i++)
			trace_trace.endpos[i] = start[i] + trace_trace.fraction * (end[i] - start[i]);
	}
	return trace_trace;
}


/*
==================
CM_TransformedBoxTrace

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
#ifdef _WIN32
#pragma optimize( "", off )
#endif


trace_t		CM_TransformedBoxTrace (vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  int headnode, int brushmask,
						  vec3_t origin, vec3_t angles)
{
	trace_t		trace;
	vec3_t		start_l, end_l;
	vec3_t		a;
	vec3_t		forward, right, up;
	vec3_t		temp;
	qboolean	rotated;

	// subtract origin offset
	VectorSubtract (start, origin, start_l);
	VectorSubtract (end, origin, end_l);

	// rotate start and end into the models frame of reference
	if (headnode != box_headnode && 
	(angles[0] || angles[1] || angles[2]) )
		rotated = true;
	else
		rotated = false;

	if (rotated)
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (start_l, temp);
		start_l[0] = DotProduct (temp, forward);
		start_l[1] = -DotProduct (temp, right);
		start_l[2] = DotProduct (temp, up);

		VectorCopy (end_l, temp);
		end_l[0] = DotProduct (temp, forward);
		end_l[1] = -DotProduct (temp, right);
		end_l[2] = DotProduct (temp, up);
	}

	// sweep the box through the model
	trace = CM_BoxTrace (start_l, end_l, mins, maxs, headnode, brushmask);

	if (rotated && trace.fraction != 1.0)
	{
		// FIXME: figure out how to do this with existing angles
		VectorNegate (angles, a);
		AngleVectors (a, forward, right, up);

		VectorCopy (trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct (temp, forward);
		trace.plane.normal[1] = -DotProduct (temp, right);
		trace.plane.normal[2] = DotProduct (temp, up);
	}

	trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);

	return trace;
}

#ifdef _WIN32
#pragma optimize( "", on )
#endif



/*
===============================================================================

PVS / PHS

===============================================================================
*/

byte	*CM_ClusterPVS (int cluster)
{
	if (cluster != -1)
		return (byte *)map_pvs->data + cluster * map_pvs->rowsize;

	return nullrow;
}

byte	*CM_ClusterPHS (int cluster)
{
	if (cluster != -1)
		return (byte *)map_phs->data + cluster * map_phs->rowsize;

	return nullrow;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/

/*
====================
FloodAreaConnections


====================
*/
void	FloodAreaConnections (void)
{
	int		i, j;

	// area 0 is not used
	for (i=1 ; i<numareas ; i++)
	{
		for (  j = 1; j < numareas; j++ ) {
			map_areas[i].numareaportals[j] = ( j == i );
		}
	}
}

void	CM_SetAreaPortalState (int area1, int area2, qboolean open)
{
	if (area1 > numareas || area2 > numareas)
		Com_Error (ERR_DROP, "CM_SetAreaPortalState: area > numareas");

	if ( open ) {
		map_areas[area1].numareaportals[area2]++;
		map_areas[area2].numareaportals[area1]++;
	} else {
		map_areas[area1].numareaportals[area2]--;
		map_areas[area2].numareaportals[area1]--;
	}
}

qboolean	CM_AreasConnected (int area1, int area2)
{
	int		i;

	if (map_noareas->value)
		return true;

	if (area1 > numareas || area2 > numareas)
		Com_Error (ERR_DROP, "CM_AreasConnected: area > numareas");

	if ( map_areas[area1].numareaportals[area2] )
		return true;

	// area 0 is not used
	for (i=1 ; i<numareas ; i++)
	{
		if ( map_areas[i].numareaportals[area1] &&
			map_areas[i].numareaportals[area2] )
			return true;
	}

	return false;
}


/*
=================
CM_WriteAreaBits

Writes a length byte followed by a bit vector of all the areas
that area in the same flood as the area parameter

This is used by the client refreshes to cull visibility
=================
*/
int CM_WriteAreaBits (byte *buffer, int area)
{
	int		i;
	int		bytes;

	bytes = (numareas+7)>>3;

	if (map_noareas->value)
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
===================
CM_WritePortalState

Writes the portal state to a savegame file
===================
*/
void	CM_WritePortalState (FILE *f)
{
}

/*
===================
CM_ReadPortalState

Reads the portal state from a savegame file
and recalculates the area connections
===================
*/
void	CM_ReadPortalState (FILE *f)
{
}

/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
qboolean CM_HeadnodeVisible (int nodenum, byte *visbits)
{
	int		leafnum;
	int		cluster;
	cnode_t	*node;

	if (nodenum < 0)
	{
		leafnum = -1-nodenum;
		cluster = map_leafs[leafnum].cluster;
		if (cluster == -1)
			return false;
		if (visbits[cluster>>3] & (1<<(cluster&7)))
			return true;
		return false;
	}

	node = &map_nodes[nodenum];
	if (CM_HeadnodeVisible(node->children[0], visbits))
		return true;
	return CM_HeadnodeVisible(node->children[1], visbits);
}
