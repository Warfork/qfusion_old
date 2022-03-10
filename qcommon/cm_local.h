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

#define MAX_CM_AREAPORTALS		(MAX_EDICTS)
#define MAX_CM_LEAFS			(MAX_MAP_LEAFS)

#define CM_SUBDIV_LEVEL			(16)

typedef struct
{
	int				contents;
	int				flags;
} cshaderref_t;

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
	int				checkcount;		// to avoid repeated testings

	int				numsides;
	cbrushside_t	*brushsides;
} cbrush_t;

typedef struct
{
	int				contents;
	int				checkcount;		// to avoid repeated testings

	vec3_t			mins, maxs;

	int				numfacets;
	cbrush_t		*facets;
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

//=======================================================================

extern	int				checkcount;

extern	mempool_t		*cmap_mempool;

extern	int				numbrushsides;
extern	cbrushside_t	*map_brushsides;

extern	int				numshaderrefs;
extern	cshaderref_t	*map_shaderrefs;

extern	int				numplanes;
extern	cplane_t		*map_planes;

extern	int				numnodes;
extern	cnode_t			*map_nodes;

extern	int				numleafs;
extern	cleaf_t			*map_leafs;

extern	int				nummarkbrushes;
extern	cbrush_t		**map_markbrushes;

extern	int				numcmodels;
extern	cmodel_t		*map_cmodels;

extern	int				numbrushes;
extern	cbrush_t		*map_brushes;

extern	int				numfaces;
extern	cface_t			*map_faces;

extern	int				nummarkfaces;
extern	cface_t			**map_markfaces;

extern	vec3_t			*map_verts;
extern	int				numvertexes;

extern	int				numareaportals;
extern	careaportal_t	map_areaportals[MAX_CM_AREAPORTALS];

extern	int				numareas;
extern	carea_t			*map_areas;

extern	dvis_t			*map_pvs, *map_phs;
extern	int				map_visdatasize;

extern	int				numentitychars;
extern	char			*map_entitystring;

//=======================================================================

void	CM_CalcPHS( void );
void	CM_InitBoxHull( void );
void	CM_FloodAreaConnections( void );
