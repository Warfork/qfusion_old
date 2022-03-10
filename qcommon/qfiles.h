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

//
// qfiles.h: quake file formats
// This file must be identical in the quake and utils directories
//

/*
========================================================================

The .pak files are just a linear collapse of a directory tree

========================================================================
*/

typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;


/*
========================================================================

.MD2 triangle model file format

========================================================================
*/

#define IDMD2HEADER			"IDP2"

#define MD2_ALIAS_VERSION	8

#define	MD2_MAX_TRIANGLES	4096
#define MD2_MAX_VERTS		4096
#define MD2_MAX_FRAMES		1024
#define MD2_MAX_SKINS		32
#define	MD2_MAX_SKINNAME	64

typedef struct
{
	short	s;
	short	t;
} dstvert_t;

typedef struct 
{
	short	index_xyz[3];
	short	index_st[3];
} dtriangle_t;

typedef struct
{
	qbyte	v[3];				// scaled byte to fit in frame mins/maxs
	qbyte	lightnormalindex;
} dtrivertx_t;

typedef struct
{
	float		scale[3];		// multiply byte verts by this
	float		translate[3];	// then add this
	char		name[16];		// frame name from grabbing
	dtrivertx_t	verts[1];		// variable sized
} daliasframe_t;

// the glcmd format:
// a positive integer starts a tristrip command, followed by that many
// vertex structures.
// a negative integer starts a trifan command, followed by -x vertexes
// a zero indicates the end of the command list.
// a vertex consists of a floating point s, a floating point t,
// and an integer vertex index.


typedef struct
{
	int			ident;
	int			version;

	int			skinwidth;
	int			skinheight;
	int			framesize;		// byte size of each frame

	int			num_skins;
	int			num_xyz;
	int			num_st;			// greater than num_xyz for seams
	int			num_tris;
	int			num_glcmds;		// dwords in strip/fan command list
	int			num_frames;

	int			ofs_skins;		// each skin is a MAX_SKINNAME string
	int			ofs_st;			// byte offset from start for stverts
	int			ofs_tris;		// offset for dtriangles
	int			ofs_frames;		// offset for first frame
	int			ofs_glcmds;	
	int			ofs_end;		// end of file

} dmd2_t;


/*
========================================================================

.MD3 model file format

========================================================================
*/

#define IDMD3HEADER			"IDP3"

#define MD3_ALIAS_VERSION	15
#define MD3_ALIAS_MAX_LODS	4

#define	MD3_MAX_TRIANGLES	8192	// per mesh
#define MD3_MAX_VERTS		4096	// per mesh
#define MD3_MAX_SHADERS		256		// per mesh
#define MD3_MAX_FRAMES		1024	// per model
#define	MD3_MAX_MESHES		32		// per model
#define MD3_MAX_TAGS		16		// per frame
#define MD3_MAX_PATH		64

// vertex scales
#define	MD3_XYZ_SCALE		(1.0/64)

typedef struct
{
	float			st[2];
} dmd3coord_t;

typedef struct
{
	short			point[3];
	unsigned char	norm[2];
} dmd3vertex_t;

typedef struct
{
    float			mins[3];
	float			maxs[3];
    float			translate[3];
    float			radius;
    char			creator[16];
} dmd3frame_t;

typedef struct
{
	char			name[MD3_MAX_PATH];		// tag name
	float			origin[3];
	float			axis[3][3];
} dmd3tag_t;

typedef struct 
{
	char			name[MD3_MAX_PATH];
	int				unused;					// shader
} dmd3skin_t;

typedef struct
{
    char			id[4];

    char			name[MD3_MAX_PATH];

	int				flags;

    int				num_frames;
    int				num_skins;
    int				num_verts;
    int				num_tris;

    int				ofs_indexes;
    int				ofs_skins;
    int				ofs_tcs;
    int				ofs_verts;

    int				meshsize;
} dmd3mesh_t;

typedef struct
{
    int				id;
    int				version;

    char			filename[MD3_MAX_PATH];

	int				flags;

    int				num_frames;
    int				num_tags;
    int				num_meshes;
    int				num_skins;

    int				ofs_frames;
    int				ofs_tags;
    int				ofs_meshes;
    int				ofs_end;
} dmd3header_t;

/*
========================================================================

.SKM and .SKP models file formats

========================================================================
*/

#define SKMHEADER				"SKM1"

#define SKM_MAX_NAME			64
#define SKM_MAX_MESHES			32
#define SKM_MAX_FRAMES			65536
#define SKM_MAX_TRIS			65536
#define SKM_MAX_VERTS			(SKM_MAX_TRIS * 3)
#define SKM_MAX_BONES			256
#define SKM_MAX_SHADERS			256
#define SKM_MAX_FILESIZE		16777216
#define SKM_MAX_ATTACHMENTS		DPM_MAX_BONES
#define SKM_MAX_LODS			4

// model format related flags
#define SKM_BONEFLAG_ATTACH		1
#define SKM_MODELTYPE			2	// (hierarchical skeletal pose)

typedef struct
{
	char id[4];				// SKMHEADER
	unsigned int type;
	unsigned int filesize;	// size of entire model file

	unsigned int num_bones;
	unsigned int num_meshes;

	// this offset is relative to the file
	unsigned int ofs_meshes;
} dskmheader_t;

// there may be more than one of these
typedef struct
{
	// these offsets are relative to the file
	char shadername[SKM_MAX_NAME];		// name of the shader to use
	char meshname[SKM_MAX_NAME];

	unsigned int num_verts;
	unsigned int num_tris;
	unsigned int num_references;
	unsigned int ofs_verts;	
	unsigned int ofs_texcoords;
	unsigned int ofs_indices;
	unsigned int ofs_references;
} dskmmesh_t;

// one or more of these per vertex
typedef struct
{
	float origin[3];		// vertex location (these blend)
	float influence;		// influence fraction (these must add up to 1)
	float normal[3];		// surface normal (these blend)
	unsigned int bonenum;	// number of the bone
} dskmbonevert_t;

// variable size, parsed sequentially
typedef struct
{
	unsigned int numbones;
	// immediately followed by 1 or more ddpmbonevert_t structures
	dskmbonevert_t verts[1];
} dskmvertex_t;

typedef struct
{
	float			st[2];
} dskmcoord_t;

typedef struct
{
	char id[4];				// SKMHEADER
	unsigned int type;
	unsigned int filesize;	// size of entire model file

	unsigned int num_bones;
	unsigned int num_frames;

	// these offsets are relative to the file
	unsigned int ofs_bones;
	unsigned int ofs_frames;
} dskpheader_t;

// one per bone
typedef struct
{
	// name examples: upperleftarm leftfinger1 leftfinger2 hand, etc
	char name[SKM_MAX_NAME];
	signed int parent;		// parent bone number
	unsigned int flags;		// flags for the bone
} dskpbone_t;

typedef struct
{
	float quat[4];
	float origin[3];
} dskpbonepose_t;

// immediately followed by bone positions for the frame
typedef struct
{
	// name examples: idle_1 idle_2 idle_3 shoot_1 shoot_2 shoot_3, etc
	char name[SKM_MAX_NAME];
	unsigned int ofs_bonepositions;
} dskpframe_t;

/*
========================================================================

.SP2 sprite file format

========================================================================
*/

#define IDSP2HEADER			"IDS2"

#define SPRITE_VERSION		2

#define SPRITE_MAX_NAME		64
#define SPRITE_MAX_FRAMES	32

typedef struct
{
	int		width, height;
	int		origin_x, origin_y;			// raster coordinates inside pic
	char	name[SPRITE_MAX_NAME];		// name of shader file
} dsprframe_t;

typedef struct {
	int			ident;
	int			version;
	int			numframes;
	dsprframe_t	frames[1];				// variable sized
} dsprite_t;

/*
==============================================================================

  .BSP file format

==============================================================================
*/

#define IDBSPHEADER			"IBSP"

#define Q3BSPVERSION		46
#define RTCWBSPVERSION		47

// there shouldn't be any problem with increasing these values at the
// expense of more memory allocation in the utilities
#define	MAX_MAP_MODELS		0x400
#define	MAX_MAP_BRUSHES		0x8000
#define	MAX_MAP_ENTITIES	0x800
#define	MAX_MAP_ENTSTRING	0x40000
#define	MAX_MAP_SHADERS		0x400

#define	MAX_MAP_AREAS		0x100
#define	MAX_MAP_FOGS		0x100
#define	MAX_MAP_PLANES		0x20000
#define	MAX_MAP_NODES		0x20000
#define	MAX_MAP_BRUSHSIDES	0x30000
#define	MAX_MAP_LEAFS		0x20000
#define	MAX_MAP_VERTEXES	0x80000
#define	MAX_MAP_FACES		0x20000
#define	MAX_MAP_LEAFFACES	0x20000
#define	MAX_MAP_LEAFBRUSHES 0x40000
#define	MAX_MAP_PORTALS		0x20000
#define	MAX_MAP_INDICES		0x80000
#define	MAX_MAP_LIGHTING	0x800000
#define	MAX_MAP_VISIBILITY	0x200000

// lightmaps
#define LIGHTMAP_BYTES			3
#define	LIGHTMAP_WIDTH			128
#define	LIGHTMAP_HEIGHT			128
#define LIGHTMAP_SIZE			(LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT*LIGHTMAP_BYTES)

// key / value pair sizes

#define	MAX_KEY		32
#define	MAX_VALUE	1024

//=============================================================================

typedef struct
{
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES			0
#define	LUMP_SHADERREFS			1
#define	LUMP_PLANES				2
#define LUMP_NODES				3
#define LUMP_LEAFS				4
#define	LUMP_LEAFFACES			5
#define	LUMP_LEAFBRUSHES		6
#define	LUMP_MODELS				7
#define	LUMP_BRUSHES			8
#define	LUMP_BRUSHSIDES			9
#define LUMP_VERTEXES			10
#define LUMP_INDEXES			11
#define LUMP_FOGS				12
#define LUMP_FACES				13
#define LUMP_LIGHTING			14
#define LUMP_LIGHTGRID			15
#define LUMP_VISIBILITY			16
#define	HEADER_LUMPS			17

typedef struct
{
	int			ident;
	int			version;	
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	float		mins[3], maxs[3];
	int			firstface, numfaces;	// submodels just draw faces
										// without walking the bsp tree
    int			firstbrush, numbrushes;
} dmodel_t;

typedef struct
{
	float			point[3];
    float			tex_st[2];		// texture coords
	float			lm_st[2];		// lightmap texture coords
    float			normal[3];		// normal
    unsigned char	color[4];		// color used for vertex lighting?
} dvertex_t;

// planes (x&~1) and (x&~1)+1 are always opposites

typedef struct
{
	float	normal[3];
	float	dist;
} dplane_t;


// contents flags are separate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

// these definitions also need to be in q_shared.h!

// lower bits are stronger, and will eat weaker brushes completely
#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
#define	CONTENTS_FOG			64

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000

// bot specific contents types
#define	CONTENTS_TELEPORTER		0x40000
#define	CONTENTS_JUMPPAD		0x80000
#define CONTENTS_CLUSTERPORTAL	0x100000
#define CONTENTS_DONOTENTER		0x200000

#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	CONTENTS_BODY			0x2000000	// should never be on a brush, only in game
#define	CONTENTS_CORPSE			0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes not used for the bsp
#define	CONTENTS_STRUCTURAL		0x10000000	// brushes used for the bsp
#define	CONTENTS_TRANSLUCENT	0x20000000	// don't consume surface fragments inside
#define	CONTENTS_TRIGGER		0x40000000
#define	CONTENTS_NODROP			0x80000000	// don't leave bodies or items (death fog, lava)

#define	SURF_NODAMAGE			0x1		// never give falling damage
#define	SURF_SLICK				0x2		// effects game physics
#define	SURF_SKY				0x4		// lighting from environment map
#define	SURF_LADDER				0x8
#define	SURF_NOIMPACT			0x10	// don't make missile explosions
#define	SURF_NOMARKS			0x20	// don't leave missile marks
#define	SURF_FLESH				0x40	// make flesh sounds and effects
#define	SURF_NODRAW				0x80	// don't generate a drawsurface at all
#define	SURF_HINT				0x100	// make a primary bsp splitter
#define	SURF_SKIP				0x200	// completely ignore, allowing non-closed brushes
#define	SURF_NOLIGHTMAP			0x400	// surface doesn't need a lightmap
#define	SURF_POINTLIGHT			0x800	// generate lighting info at vertexes
#define	SURF_METALSTEPS			0x1000	// clanking footsteps
#define	SURF_NOSTEPS			0x2000	// no footstep sounds
#define	SURF_NONSOLID			0x4000	// don't collide against curves with this set
#define SURF_LIGHTFILTER		0x8000	// act as a light filter during q3map -light
#define	SURF_ALPHASHADOW		0x10000	// do per-pixel light shadow casting in q3map
#define	SURF_NODLIGHT			0x20000	// never add dynamic lights
#define SURF_DUST				0x40000 // leave a dust trail when walking on this surface


typedef struct
{
	int				planenum;
	int				children[2];	// negative numbers are -(leafs+1), not nodes
	int				mins[3];		// for frustum culling
	int				maxs[3];
} dnode_t;


typedef struct shaderref_s
{
	char			name[MAX_QPATH];
	int				flags;
	int				contents;
} dshaderref_t;

enum
{
    FACETYPE_PLANAR   = 1,
    FACETYPE_PATCH    = 2,
    FACETYPE_TRISURF  = 3,
    FACETYPE_FLARE    = 4
};

typedef struct
{
	int				shadernum;
	int				fognum;
	int				facetype;

    int				firstvert;
	int				numverts;
	unsigned		firstindex;
	int				numindexes;

    int				lm_texnum;		// lightmap info
    int				lm_offset[2];
    int				lm_size[2];

    float			origin[3];		// FACETYPE_FLARE only

    float			mins[3];
    float			maxs[3];		// FACETYPE_PATCH and FACETYPE_TRISURF only
    float			normal[3];		// FACETYPE_PLANAR only

    int				patch_cp[2];	// patch control point dimensions
} dface_t;

typedef struct
{
	int				cluster;
	int				area;

	int				mins[3];
	int				maxs[3];

	int				firstleafface;
	int				numleaffaces;

	int				firstleafbrush;
	int				numleafbrushes;
} dleaf_t;

typedef struct
{
	int				planenum;
	int				shadernum;
} dbrushside_t;

typedef struct
{
	int				firstside;
	int				numsides;
	int				shadernum;
} dbrush_t;

typedef struct {
	char			shader[MAX_QPATH];
	int				brushnum;
	int				visibleside;
} dfog_t;

typedef struct
{
	int				numclusters;
	int				rowsize;
	unsigned char	data[1];
} dvis_t;

typedef struct
{
	unsigned char	ambient[3];
	unsigned char	diffuse[3];
	unsigned char	direction[2];
} dgridlight_t;
