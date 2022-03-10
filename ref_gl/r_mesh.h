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

enum
{
	MF_NONE				= 0,
	MF_NONBATCHED		= 1 << 0,
	MF_NORMALS			= 1 << 1,
	MF_STCOORDS			= 1 << 2,
	MF_LMCOORDS			= 1 << 3,
	MF_COLORS			= 1 << 4,
	MF_TRNORMALS		= 1 << 5,
	MF_NOCULL			= 1 << 6,
	MF_DEFORMVS			= 1 << 7
};

enum
{
	MB_MODEL,
	MB_SPRITE,
	MB_POLY,

	MB_MAXTYPES	= 4
};

typedef struct mesh_s
{
    int					numVertexes;
	vec3_t				*xyzArray;
	vec3_t				*normalsArray;
	vec2_t				*stArray;
	vec2_t				*lmstArray;
	byte_vec4_t			*colorsArray;

    int					numIndexes;
    index_t				*indexes;

#if SHADOW_VOLUMES
	int					*trneighbors;
	vec3_t				*trnormals;
#endif
} mesh_t;

typedef struct
{
	unsigned int		sortkey;
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
	mesh_t			*meshes;
	vec2_t			*sphereStCoords[5];
	vec2_t			*linearStCoords[6];

	struct shader_s	*farboxShaders[6];
	struct shader_s *nearboxShaders[6];
} skydome_t;

meshbuffer_t *R_AddMeshToList( int type, struct mfog_s *fog, struct shader_s *shader, int infokey );

void R_SortMeshes( void );
void R_DrawMeshes( qboolean triangleOutlines );
void R_DrawTriangleOutlines( void );

void R_DrawPortalSurface( meshbuffer_t *mb );
void R_DrawCubemapView( vec3_t origin, vec3_t angles, int size );

extern	meshlist_t	r_worldlist;
extern	meshlist_t	*r_currentlist;
