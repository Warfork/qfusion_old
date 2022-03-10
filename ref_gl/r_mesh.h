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
#define MAX_RENDER_MESHES			16384
#define MAX_RENDER_ADDITIVE_MESHES	MAX_RENDER_MESHES >> 1

typedef struct mesh_s
{
    int				numvertexes;
	vec4_t			*xyz_array;
	vec3_t			*normals_array;
	vec2_t			*st_array;
	vec2_t			*lmst_array;
	byte_vec4_t		*colors_array;

    int				numindexes;
    index_t			*indexes;

#ifdef SHADOW_VOLUMES
	int				*trneighbors;
	vec3_t			*trnormals;
#endif
} mesh_t;

typedef struct meshbuffer_s
{
	int					sortkey;
	int					infokey;		// surface number or mesh number
	unsigned int		dlightbits;
	entity_t			*entity;
	struct shader_s		*shader;
	struct mfog_s		*fog;
} meshbuffer_t;

typedef struct
{
	int					num_meshes;
	meshbuffer_t		meshbuffer[MAX_RENDER_MESHES];

	int					num_additive_meshes;
	meshbuffer_t		meshbuffer_additives[MAX_RENDER_ADDITIVE_MESHES];

	qboolean			skyDrawn;

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
	mesh_t			meshes[6];
	vec2_t			*sphereStCoords[5];
	vec2_t			*linearStCoords[6];

	struct shader_s	*farbox_shaders[6];
	struct shader_s *nearbox_shaders[6];
} skydome_t;

meshbuffer_t *R_AddMeshToList ( struct mfog_s *fog, struct shader_s *shader, int infokey );

void R_SortMeshes (void);
void R_DrawMeshes ( qboolean triangleOutlines );
void R_DrawTriangleOutlines (void);

extern	meshlist_t	r_worldlist;
extern	meshlist_t	*r_currentlist;
