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

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
typedef struct
{
	vec3_t			mins, maxs;
	float			radius;
	int				firstface, numfaces;
	vec3_t			gridSize;
	vec3_t			gridMins;
	int				gridBounds[4];
} mmodel_t;

typedef struct
{
	char			name[MAX_QPATH];
	int				flags;
	shader_t		*shader;
} mshaderref_t;

typedef struct
{
	shader_t		*shader;
	cplane_t		*visibleplane;

	int				numplanes;
	cplane_t		*planes;
} mfog_t;

typedef struct
{
	int				visframe;		// should be drawn when node is crossed
	int				facetype;
	int				flags;

	shader_t		*shader;
	mesh_t			*mesh;
	mfog_t			*fog;
	cplane_t		*plane;

	union
	{
		float		origin[3];
		float		mins[3];
	};
	union
	{
		float		maxs[3];
		float		color[3];
	};
	int				fragmentframe;		// for R_GetClippedFragments

	int				superLightStyle;

	meshbuffer_t	*meshBuffer;		// needed to OR dlightbits
} msurface_t;

typedef struct mnode_s
{
// common with leaf
	cplane_t		*plane;

	int				visframe;

	float			mins[3];
	float			maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

// node specific
	struct mnode_s	*children[2];
} mnode_t;

typedef struct mleaf_s
{
// common with node
	cplane_t		*plane;

	int				visframe;

	float			mins[3];
	float			maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

// leaf specific
	int				cluster;
	int				area;

	msurface_t		**firstVisSurface;
	msurface_t		**firstFragmentSurface;
} mleaf_t;

typedef struct
{
	vec3_t			origin;
	vec3_t			color;
	float			intensity;
} mlight_t;

typedef struct
{
	qbyte			ambient[MAX_LIGHTMAPS][3];
	qbyte			diffuse[MAX_LIGHTMAPS][3];
	qbyte			styles[MAX_LIGHTMAPS];
	qbyte			direction[2];
} mgridlight_t;

typedef struct
{
	int				texNum;
	float			texMatrix[2][2];
} mlightmapRect_t;

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

//
// in memory representation
//
typedef struct
{
	short			point[3];
	qbyte			latlong[2];		// use bytes to keep 8-byte alignment
} maliasvertex_t;

typedef struct
{
    vec3_t			mins, maxs;
	vec3_t			scale;
    vec3_t			translate;
    float			radius;
} maliasframe_t;

typedef struct
{
	char			name[MD3_MAX_PATH];
	quat_t			quat;
	vec3_t			origin;
} maliastag_t;

typedef struct 
{
	shader_t		*shader;
} maliasskin_t;

typedef struct
{
    char			name[MD3_MAX_PATH];

    int				numverts;
	maliasvertex_t	*vertexes;
	vec2_t			*stArray;

	vec3_t			*xyzArray;
	vec3_t			*normalsArray;
	vec4_t			*sVectorsArray;

    int				numtris;
    index_t			*indexes;

#if SHADOW_VOLUMES
	int				*trneighbors;
#endif

    int				numskins;
	maliasskin_t	*skins;
} maliasmesh_t;

typedef struct
{
    int				numframes;
	maliasframe_t	*frames;

    int				numtags;
	maliastag_t		*tags;

    int				nummeshes;
	maliasmesh_t	*meshes;

	int				numskins;
	maliasskin_t	*skins;
} maliasmodel_t;

/*
==============================================================================

SKELETAL MODELS

==============================================================================
*/

//
// in memory representation
//
#define SKM_MAX_WEIGHTS	4

//
// in memory representation
//
typedef struct
{
	shader_t		*shader;
} mskskin_t;

typedef struct
{
	char			name[SKM_MAX_NAME];

	float			*influences;
	unsigned int	*bones;

	unsigned int	numverts;
	vec3_t			*xyzArray, *normalsArray;
	vec2_t			*stArray;
	vec4_t			*sVectorsArray;

	unsigned int	numtris;
	index_t			*indexes;

	unsigned int	numreferences;
	unsigned int	*references;

#if SHADOW_VOLUMES
	int				*trneighbors;
#endif

	mskskin_t		skin;
} mskmesh_t;

typedef struct
{
	char			name[SKM_MAX_NAME];
	signed int		parent;
	unsigned int	flags;
} mskbone_t;

typedef struct
{
	bonepose_t		*boneposes;
} mskframe_t;

typedef struct
{
	unsigned int	numbones;
	mskbone_t		*bones;

	unsigned int	nummeshes;
	mskmesh_t		*meshes;

	unsigned int	numframes;
	mskframe_t		*frames;
	bonepose_t		*invbaseposes;
} mskmodel_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

//
// in memory representation
//
typedef struct
{
	int			width, height;
	int			origin_x, origin_y;			// raster coordinates inside pic

	char		name[SPRITE_MAX_NAME];
	shader_t	*shader;

	float		mins[3], maxs[3];
	float		radius;
} sframe_t;

typedef struct 
{
	int			numframes;
	sframe_t	*frames;
} smodel_t;

//===================================================================

//
// Whole model
//

typedef enum { mod_bad, mod_brush, mod_sprite, mod_alias, mod_skeletal } modtype_t;

#define MOD_MAX_LODS	4

typedef struct model_s
{
	char			name[MAX_QPATH];

	modtype_t		type;
	int				flags;

//
// volume occupied by the model graphics
//		
	vec3_t			mins, maxs;
	float			radius;

//
// brush model
//
	int				numsubmodels;
	mmodel_t		*submodels;

	int				nummodelsurfaces;
	msurface_t		*firstmodelsurface;

	int				numplanes;
	cplane_t		*planes;

	int				numleafs;			// number of visible leafs, not counting 0
	mleaf_t			*leafs;

	int				numvertexes;
	vec3_t			*xyz_array;
	vec3_t			*normals_array;		// normals
	vec2_t			*st_array;			// texture coords		
	vec2_t			*lmst_array[MAX_LIGHTMAPS];			// lightmap texture coords
	byte_vec4_t		*colors_array[MAX_LIGHTMAPS];		// colors used for vertex lighting

	int				numnodes;
	mnode_t			*nodes;

	int				numsurfaces;
	msurface_t		*surfaces;

	int				numsurfindexes;
	int				*surfindexes;

	int				numshaderrefs;
	mshaderref_t	*shaderrefs;

	int				numlightgridelems;
	mgridlight_t	*lightgrid;

	int				numlightarrayelems;
	mgridlight_t	**lightarray;

	int				numfogs;
	mfog_t			*fogs;
	mfog_t			*globalfog;

	int				numworldlights;
	mlight_t		*worldlights;

	int				numlightmaps;
	mlightmapRect_t	*lightmapRects;

	dvis_t			*vis;

//
// alias model
//
	maliasmodel_t	*aliasmodel;

//
// skeletal model
//
	mskmodel_t		*skmodel;

//
// sprite model
//
	smodel_t		*smodel;

	int				numlods;
	struct model_s	*lods[MOD_MAX_LODS];

	mempool_t		*extradata;
} model_t;

typedef struct
{
	const char		*header;
	int				headerLen;
	int				maxLods;
	void			(*loader) ( model_t *mod, model_t *parent, void *buffer );
} modelformatdescriptor_t;

//============================================================================

void	R_InitModels (void);
void	R_ShutdownModels (void);

void	Mod_ClearAll (void);
model_t *Mod_ForName (char *name, qboolean crash);
mleaf_t *Mod_PointInLeaf (float *p, model_t *model);
qbyte	*Mod_ClusterPVS (int cluster, model_t *model);

#define Mod_Malloc(mod,size) Mem_Alloc((mod)->extradata,size)
#define Mod_Free(data) Mem_Free(data)

void	Mod_StripLODSuffix ( char *name );

void	Mod_Modellist_f (void);
