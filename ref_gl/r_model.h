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
} mmodel_t;

typedef struct
{
	char			name[MAX_QPATH];
	int				flags;
	int				contents;
	shader_t		*shader;
} mshaderref_t;

typedef struct
{
	cplane_t		*plane;
} mbrushside_t;

typedef struct
{
	int				numsides;
	mbrushside_t	*firstside;
} mbrush_t;

typedef struct mfog_s
{
	shader_t		*shader;

	cplane_t		*visibleplane;

	int				numplanes;
	cplane_t		**planes;
} mfog_t;

typedef struct msurface_s
{
	int				visframe;		// should be drawn when node is crossed
	int				facetype;

	mshaderref_t	*shaderref;

	mesh_t			*mesh;
	mfog_t			*fog;

    float			origin[3];

	int				fragmentframe;		// for R_GetClippedFragments

	int				lightmaptexturenum;
	unsigned int	dlightbits;
	int				dlightframe;
} msurface_t;

typedef struct mnode_s
{
// common with leaf
	cplane_t		*plane;

// node specific
	struct mnode_s	*children[2];
} mnode_t;

typedef struct mleaf_s
{
// common with node
	cplane_t		*plane;

// leaf specific
	int				visframe;
	struct mleaf_s	*vischain;

	float			mins[3];
	float			maxs[3];		// for bounding box culling

	int				cluster;
	int				area;

	msurface_t		**firstmarksurface;
	int				nummarksurfaces;
} mleaf_t;

typedef struct
{
	vec3_t			origin;
	vec3_t			color;
	float			intensity;
} mlight_t;

typedef struct
{
	byte			ambient[3];
	byte			diffuse[3];
	byte			direction[2];
} mgridlight_t;

typedef struct
{
	int				numsubmodels;
	mmodel_t		*submodels;

	int				numplanes;
	cplane_t		*planes;

	int				numleafs;			// number of visible leafs, not counting 0
	mleaf_t			*leafs;

	int				numvertexes;
	vec4_t			*xyz_array;
	vec3_t			*normals_array;		// normals
	vec2_t			*st_array;			// texture coords		
	vec2_t			*lmst_array;		// lightmap texture coords
	byte_vec4_t		*colors_array;		// colors used for vertex lighting

	int				numnodes;
	mnode_t			*nodes;

	int				numsurfaces;
	msurface_t		*surfaces;

	int				numsurfindexes;
	int				*surfindexes;

	int				nummarksurfaces;
	msurface_t		**marksurfaces;

	int				numshaderrefs;
	mshaderref_t	*shaderrefs;

	int				numbrushsides;
	mbrushside_t	*brushsides;

	int				numbrushes;
	mbrush_t		*brushes;

	int				numlightgridelems;
	mgridlight_t	*lightgrid;

	int				numfogs;
	mfog_t			*fogs;

	int				numworldlights;
	mlight_t		*worldlights;

	dvis_t			*vis;

	int				numlightmaps;
	byte			*lightdata;
} bmodel_t;

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

//
// in memory representation
//
typedef struct maliascoord_s
{
	vec2_t			st;
} maliascoord_t;

typedef struct maliasvertex_s
{
	vec3_t			point;
	vec3_t			normal;
} maliasvertex_t;

typedef struct
{
    vec3_t			mins, maxs;
    vec3_t			translate;
    float			radius;
} maliasframe_t;

typedef struct
{
	char			name[MD3_MAX_PATH];
	orientation_t	orient;
} maliastag_t;

typedef struct 
{
	char			name[MD3_MAX_PATH];
	shader_t		*shader;
} maliasskin_t;

typedef struct
{
    int				numverts;
	maliasvertex_t	*vertexes;
	maliascoord_t	*stcoords;

    int				numtris;
    index_t			*indexes;
	int				*trneighbors;

    int				numskins;
	maliasskin_t	*skins;
} maliasmesh_t;

typedef struct maliasmodel_s
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

DARKPLACES MODELS

==============================================================================
*/

//
// in memory representation
//

typedef struct
{
	float			matrix[3][4];
} dpmbonepose_t;

typedef struct dpmbonevert_s
{
	vec3_t			origin;
	float			influence;
	vec3_t			normal;
	unsigned int	bonenum;
} dpmbonevert_t;

typedef struct
{
	unsigned int	numbones;
	dpmbonevert_t	*verts;
} dpmvertex_t;

typedef struct
{
	char			shadername[DPM_MAX_NAME];		// name of the shader to use
	shader_t		*shader;
} dpmskin_t;

typedef struct
{
	float			st[2];
} dpmcoord_t;

typedef struct
{
	unsigned int	numverts;
	dpmvertex_t		*vertexes;
	dpmcoord_t		*stcoords;

	unsigned int	numtris;
	index_t			*indexes;
	int				*trneighbors;
	vec3_t			*trnormals;

	dpmskin_t		skin;
} dpmmesh_t;

typedef struct
{
	char			name[DPM_MAX_NAME];
	signed int		parent;
	unsigned int	flags;
} dpmbone_t;

typedef struct
{
	dpmbonepose_t	*boneposes;

	float			mins[3], maxs[3];
	float			yawradius, allradius;	// for clipping uses
} dpmframe_t;

typedef struct
{
	unsigned int	numbones;
	dpmbone_t		*bones;

	unsigned int	nummeshes;
	dpmmesh_t		*meshes;

	unsigned int	numframes;
	dpmframe_t		*frames;

	float			mins[3], maxs[3];
	float			yawradius, allradius;	// for clipping uses
} dpmmodel_t;

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

typedef enum { mod_bad, mod_brush, mod_sprite, mod_alias, mod_dpm } modtype_t;

typedef struct model_s
{
	char			name[MAX_QPATH];

	int				registration_sequence;

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
	int				nummodelsurfaces;
	msurface_t		*firstmodelsurface;
	bmodel_t		*bmodel;

	vec3_t			gridSize;
	vec3_t			gridMins;
	int				gridBounds[4];

//
// alias model
//
	maliasmodel_t	*aliasmodel;

//
// dpm model
//
	dpmmodel_t		*dpmmodel;

//
// sprite model
//
	smodel_t		*smodel;

	int				numlods;
	struct model_s	**lods;	

	int				extradatasize;
	void			*extradata;
} model_t;

typedef struct
{
	const char		*header;
	int				headerLen;
	int				maxSize;
	int				maxLods;
	void			(*loader) ( model_t *mod, void *buffer );
} modelformatdescriptor_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (char *name, qboolean crash);
mleaf_t *Mod_PointInLeaf (float *p, bmodel_t *bmodel);
byte	*Mod_ClusterPVS (int cluster, bmodel_t *bmodel);

void	Mod_Modellist_f (void);
void	Mod_FreeAll (void);
void	Mod_Free (model_t *mod);
