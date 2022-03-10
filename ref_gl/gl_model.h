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
	vec3_t		mins, maxs;
	float		radius;
	int			firstface, numfaces;
    int			firstbrush, numbrushes;
} mmodel_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

#define SIDE_SIZE 9
#define POINTS_LEN (SIDE_SIZE*SIDE_SIZE)
#define ELEM_LEN ((SIDE_SIZE-1)*(SIDE_SIZE-1)*6)

#define SPHERE_RAD  10.0
#define EYE_RAD      9.0

#define SCALE_S		4.0  // Arbitrary (?) texture scaling factors
#define SCALE_T		4.0 

#define BOX_SIZE	1.0f
#define BOX_STEP	0.25f

// the skydome has 5 sides (no bottom)
// {"rt", "bk", "lf", "ft", "up", "dn"};
enum
{
    SKYBOX_RIGHT = 0,
    SKYBOX_LEFT,
    SKYBOX_FRONT,
    SKYBOX_BACK,
    SKYBOX_TOP
};

typedef struct
{
	mesh_t			meshes[5];
    unsigned		firstindex[ELEM_LEN];
} skydome_t;

typedef struct 
{
	int				flags;
	int				contents;
	char			name[MAX_QPATH];
} mshaderref_t;

typedef struct
{
	float			pdist;
	byte			ptype;
	shader_t		*shader;
} mfog_t;

typedef struct msurface_s
{
	int				visframe;		// should be drawn when node is crossed

	int				facetype;

	mesh_t			mesh;

    int				lm_offset[2];
    int				lm_size[2];

    float			origin[3];		// FACETYPE_TRISURF only

    float			mins[3];
    float			maxs[3];		// FACETYPE_MESH only

	mshaderref_t	*shaderref;
	mfog_t			*fog;

    cplane_t		*plane;			// FACETYPE_PLANAR only
	
	struct	msurface_s	*texturechain;
	struct  msurface_s	*fogchain;
} msurface_t;

typedef struct
{
	cplane_t		*plane;
} mbrushside_t;

typedef struct
{
	mbrushside_t	*firstside;
	int				numsides;
} mbrush_t;

typedef struct mnode_s
{
// common with leaf
	int			contents;		// -1, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	
	float		mins[3];
	float		maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

// node specific
	cplane_t		*plane;
	struct mnode_s	*children[2];
} mnode_t;


typedef struct mleaf_s
{
// common with node
	int			contents;		// will be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		mins[3];
	float		maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

// leaf specific
	int			cluster;
	int			area;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;

	int			*firstleafbrush;
	int			numleafbrushes;
} mleaf_t;

typedef struct
{
	float		lightAmbient[3];
	float		lightDiffuse[3];
	float		lightPosition[3];
} mlightgrid_t;

//===================================================================

//
// Whole model
//

typedef enum {mod_bad, mod_brush, mod_sprite, mod_alias } modtype_t;

typedef enum { ALIASTYPE_MD2, ALIASTYPE_MD3 } aliastype_t;

typedef struct model_s
{
	char		name[MAX_QPATH];

	int			registration_sequence;

	modtype_t	type;
	int			numframes;
	
	int			flags;
	aliastype_t aliastype;

//
// volume occupied by the model graphics
//		
	vec3_t		mins, maxs;
	float		radius;

//
// brush model
//
	int			firstmodelsurface, nummodelsurfaces;
	int			lightmap;		// only for submodels

	int			numsubmodels;
	mmodel_t	*submodels;

	int			numplanes;
	cplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numnodes;
	mnode_t		*nodes;

	int			numshaderrefs;
	mshaderref_t	*shaderrefs;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfindexes;
	int			*surfindexes;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	int			numbrushsides;
	mbrushside_t *brushsides;

	int			numbrushes;
	mbrush_t	*brushes;

	int			numleafbrushes;
	int			*leafbrushes;

	int			numlightgridelems;
	mlightgrid_t *lightgrid;

	int			numfogs;
	mfog_t		*fogs;

	dvis_t		*vis;

	skydome_t	skydome;

	int			numlightmaps;
	byte		*lightdata;

	// for alias models and skins
	shader_t	*skins[MAX_MD3_MESHES][MAX_MD2SKINS];

	int			extradatasize;
	void		*extradata;
} model_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (char *name, qboolean crash);
mleaf_t *Mod_PointInLeaf (float *p, model_t *model);
byte	*Mod_ClusterPVS (int cluster, model_t *model);

void	Mod_Modellist_f (void);

void	*Hunk_Begin (int maxsize);
void	*Hunk_Alloc (int size);
int		Hunk_End (void);
void	Hunk_Free (void *base);

void	Mod_FreeAll (void);
void	Mod_Free (model_t *mod);
