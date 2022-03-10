/*
Copyright (C) 1997-2001 Victor Luchits

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
#define MAX_RENDER_MESHES			16384
#define MAX_RENDER_ADDITIVE_MESHES	MAX_RENDER_MESHES >> 1

struct mfog_s;
struct shader_s;
struct msurface_s;

#if _MSC_VER || __BORLANDC__
typedef unsigned __int64 msortkey_t;
#else
typedef unsigned long long msortkey_t;
#endif

typedef struct mesh_s
{
    int				numvertexes;
	vec4_t			*xyz_array;
	vec3_t			*normals_array;
	vec2_t			*st_array;
	vec2_t			*lmst_array;
	vec4_t			*colors_array;

    int				numindexes;
    index_t			*indexes;
	int				*trneighbors;
	vec3_t			*trnormals;

	vec3_t			mins, maxs;
	float			radius;

	unsigned int	patchWidth;
	unsigned int	patchHeight;
} mesh_t;

typedef struct meshbuffer_s
{
	msortkey_t			sortkey;
	int					infokey;		// lightmap number or mesh number
	unsigned int		dlightbits;
	entity_t			*entity;
	struct shader_s		*shader;
	mesh_t				*mesh;
	struct mfog_s		*fog;
} meshbuffer_t;

typedef struct
{
	int					num_meshes;
	meshbuffer_t		meshbuffer[MAX_RENDER_MESHES];

	int					num_additive_meshes;
	meshbuffer_t		meshbuffer_additives[MAX_RENDER_ADDITIVE_MESHES];

	float				skymins[2][6];
	float				skymaxs[2][6];
} meshlist_t;

enum
{
    SKYBOX_RIGHT,
    SKYBOX_LEFT,
    SKYBOX_FRONT,
    SKYBOX_BACK,
    SKYBOX_TOP,
	SKYBOX_BOTTOM		// not used for skydome, but is used for skybox
};

typedef struct
{
	mesh_t			meshes[5];

	image_t			*farbox_textures[6];
	image_t			*nearbox_textures[6];
} skydome_t;

meshbuffer_t *R_AddMeshToBuffer ( mesh_t *mesh, struct mfog_s *fog, struct msurface_s *surf, struct shader_s *shader, int infokey );

void R_DrawSortedMeshes (void);

extern	meshlist_t	*currentlist;
