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

#define MAX_ARRAY_VERTS			4096
#define MAX_ARRAY_INDEXES		MAX_ARRAY_VERTS*6
#define MAX_ARRAY_TRIANGLES		MAX_ARRAY_INDEXES/3
#define MAX_ARRAY_NEIGHBORS		MAX_ARRAY_TRIANGLES*3

enum
{
	VBO_VERTS,
	VBO_NORMALS,
	VBO_COLORS,
	VBO_INDEXES,
	VBO_TC0,

	VBO_ENDMARKER
};

#define MAX_VERTEX_BUFFER_OBJECTS	VBO_ENDMARKER+MAX_TEXTURE_UNITS-1

#if SHADOW_VOLUMES
extern vec3_t		inVertsArray[MAX_ARRAY_VERTS*2];	// the second half is for shadow volumes
#else
extern vec3_t		inVertsArray[MAX_ARRAY_VERTS];
#endif

extern vec3_t		inNormalsArray[MAX_ARRAY_VERTS];
extern index_t		inIndexesArray[MAX_ARRAY_INDEXES];
extern vec2_t		inCoordsArray[MAX_ARRAY_VERTS];
extern vec2_t		inLightmapCoordsArray[MAX_ARRAY_VERTS];
extern byte_vec4_t	inColorsArray[MAX_ARRAY_VERTS];

extern vec3_t		tempVertexArray[MAX_ARRAY_VERTS];
extern vec3_t		tempNormalsArray[MAX_ARRAY_VERTS];

extern index_t		*indexesArray;
extern vec3_t		*vertsArray;
extern vec3_t		*normalsArray;
extern vec2_t		*coordsArray;
extern vec2_t		*lightmapCoordsArray;
extern byte_vec4_t	colorArray[MAX_ARRAY_VERTS];

#if SHADOW_VOLUMES
extern int			inNeighborsArray[MAX_ARRAY_NEIGHBORS];
extern vec3_t		inTrNormalsArray[MAX_ARRAY_TRIANGLES];

extern int			*neighborsArray;
extern vec3_t		*trNormalsArray;

extern int			*currentTrNeighbor;
extern float		*currentTrNormal;
#endif

extern	int			r_numVertexBufferObjects;
extern	GLuint		r_vertexBufferObjects[MAX_VERTEX_BUFFER_OBJECTS];

extern int			numVerts, numIndexes, numColors;

extern unsigned int	r_numverts;
extern unsigned int	r_numtris;

extern qboolean		r_blocked;
extern qboolean		r_arraysLocked;

extern int			r_features;

extern index_t		r_quad_indexes[];
extern index_t		r_trifan_indexes[];

void R_BackendInit( void );
void R_BackendShutdown( void );
void R_BackendStartFrame( void );
void R_BackendEndFrame( void );

void R_BackendBeginTriangleOutlines( void );
void R_BackendEndTriangleOutlines( void );

void R_LockArrays( int numverts );
void R_UnlockArrays( void );
void R_UnlockArrays( void );
void R_FlushArrays( void );
void R_FlushArraysMtex( void );
void R_ClearArrays( void );

static inline void R_PushIndexes( index_t *indexes, int *neighbors, vec3_t *trnormals, int numindexes, int features )
{
	int i;
	int numTris;
	index_t	*currentIndex;

	// this is a fast path for non-batched geometry, use carefully 
	// used on pics, sprites, .dpm, .md3 and .md2 models
	if( features & MF_NONBATCHED ) {
		if( numindexes > MAX_ARRAY_INDEXES )
			numindexes = MAX_ARRAY_INDEXES;

		// simply change indexesArray to point at indexes
		numIndexes = numindexes;
		indexesArray = indexes;

#if SHADOW_VOLUMES
		if( neighbors ) {
			neighborsArray = neighbors;
			currentTrNeighbor = neighborsArray + numIndexes;
		}

		if( (features & MF_TRNORMALS) && trnormals ) {
			numTris = numIndexes / 3;
			trNormalsArray = trnormals;
			currentTrNormal = trNormalsArray[0] + numTris;
		}
#endif
	} else {
		// clamp
		if( numIndexes + numindexes > MAX_ARRAY_INDEXES )
			numindexes = MAX_ARRAY_INDEXES - numIndexes;

		numTris = numindexes / 3;
		currentIndex = indexesArray + numIndexes;
		numIndexes += numindexes;

		// the following code assumes that R_PushIndexes is fed with triangles...
		for( i = 0; i < numTris; i++, indexes += 3, currentIndex += 3 ) {
			currentIndex[0] = numVerts + indexes[0];
			currentIndex[1] = numVerts + indexes[1];
			currentIndex[2] = numVerts + indexes[2];

#if SHADOW_VOLUMES
			if( neighbors ) {
				currentTrNeighbor[0] = numTris + neighbors[0];
				currentTrNeighbor[1] = numTris + neighbors[1];
				currentTrNeighbor[2] = numTris + neighbors[2];

				neighbors += 3;
				currentTrNeighbor += 3;
			}

			if( (features & MF_TRNORMALS) && trnormals ) {
				VectorCopy ( trnormals[i], currentTrNormal );
				currentTrNormal += 3;
			}
#endif
		}
	}
}

static inline void R_PushMesh( mesh_t *mesh, int features )
{
	int numverts;

	if( !mesh->indexes || !mesh->xyzArray )
		return;

	r_features = features;

#if SHADOW_VOLUMES
	R_PushIndexes( mesh->indexes, mesh->trneighbors, mesh->trnormals, mesh->numIndexes, features );
#else
	R_PushIndexes( mesh->indexes, NULL, NULL, mesh->numIndexes, features );
#endif

	numverts = mesh->numVertexes;

	if( features & MF_NONBATCHED ) {
		if( features & MF_DEFORMVS ) {
			memcpy( inVertsArray[numVerts], mesh->xyzArray, numverts * sizeof(vec3_t) );

			if( (features & MF_NORMALS) && mesh->normalsArray )
				memcpy( inNormalsArray[numVerts], mesh->normalsArray, numverts * sizeof(vec3_t) );
		} else {
			vertsArray = mesh->xyzArray;

			if( (features & MF_NORMALS) && mesh->normalsArray )
				normalsArray = mesh->normalsArray;
		}

		if( (features & MF_STCOORDS) && mesh->stArray )
			coordsArray = mesh->stArray;

		if( (features & MF_LMCOORDS) && mesh->lmstArray )
			lightmapCoordsArray = mesh->lmstArray;
	} else {
		memcpy( inVertsArray[numVerts], mesh->xyzArray, numverts * sizeof(vec3_t) );

		if( (features & MF_NORMALS) && mesh->normalsArray )
			memcpy( inNormalsArray[numVerts], mesh->normalsArray, numverts * sizeof(vec3_t) );

		if( (features & MF_STCOORDS) && mesh->stArray )
			memcpy( inCoordsArray[numVerts], mesh->stArray, numverts * sizeof(vec2_t) );

		if( (features & MF_LMCOORDS) && mesh->lmstArray )
			memcpy( inLightmapCoordsArray[numVerts], mesh->lmstArray, numverts * sizeof(vec2_t) );
	}

	if( (features & MF_COLORS) && mesh->colorsArray )
		memcpy( inColorsArray[numVerts], mesh->colorsArray, numverts * sizeof(byte_vec4_t) );

	numVerts += numverts;
	r_numverts += numverts;
}

static inline qboolean R_BackendOverflow( mesh_t *mesh )
{
	return ( numVerts + mesh->numVertexes > MAX_ARRAY_VERTS || 
		numIndexes + mesh->numIndexes > MAX_ARRAY_INDEXES );
}

static inline qboolean R_InvalidMesh( mesh_t *mesh )
{
	return ( !mesh->numVertexes || !mesh->numIndexes || 
		mesh->numVertexes > MAX_ARRAY_VERTS || mesh->numIndexes > MAX_ARRAY_INDEXES );
}

void R_RenderMeshBuffer( meshbuffer_t *mb, qboolean shadowpass );
