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
//	VBO_INDEXES,
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
extern vec4_t		inSVectorsArray[MAX_ARRAY_VERTS];
extern index_t		inIndexesArray[MAX_ARRAY_INDEXES];
extern vec2_t		inCoordsArray[MAX_ARRAY_VERTS];
extern vec2_t		inLightmapCoordsArray[MAX_LIGHTMAPS][MAX_ARRAY_VERTS];
extern byte_vec4_t	inColorsArray[MAX_LIGHTMAPS][MAX_ARRAY_VERTS];

extern index_t		*indexesArray;
extern vec3_t		*vertsArray;
extern vec3_t		*normalsArray;
extern vec4_t		*sVectorsArray;
extern vec2_t		*coordsArray;
extern vec2_t		*lightmapCoordsArray[MAX_LIGHTMAPS];
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

void R_BackendInit( void );
void R_BackendShutdown( void );
void R_BackendStartFrame( void );
void R_BackendEndFrame( void );

void R_BackendBeginTriangleOutlines( void );
void R_BackendEndTriangleOutlines( void );

void R_BackendCleanUpTextureUnits( void );

void R_LockArrays( int numverts );
void R_UnlockArrays( void );
void R_UnlockArrays( void );
void R_FlushArrays( void );
void R_FlushArraysMtex( void );
void R_ClearArrays( void );

#if SHADOW_VOLUMES
static inline void R_PushIndexes( index_t *indexes, int *neighbors, vec3_t *trnormals, int count, int features )
{
	int numTris;
#else
static inline void R_PushIndexes( index_t *indexes, int count, int features )
{
#endif
	index_t	*currentIndex;

	// this is a fast path for non-batched geometry, use carefully 
	// used on pics, sprites, .dpm, .md3 and .md2 models
	if( features & MF_NONBATCHED ) {
		// simply change indexesArray to point at indexes
		numIndexes = count;
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
#if SHADOW_VOLUMES
		numTris = numIndexes / 3;
#endif
		currentIndex = indexesArray + numIndexes;
		numIndexes += count;

		// the following code assumes that R_PushIndexes is fed with triangles...
		for( ; count > 0; count -= 3, indexes += 3, currentIndex += 3 ) {
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
				VectorCopy( *trnormals, currentTrNormal );
				trnormals++;
				currentTrNormal += 3;
			}
#endif
		}
	}
}

static inline void R_PushTrifanIndexes( int numverts )
{
	int count;
	index_t	*currentIndex;

	currentIndex = indexesArray + numIndexes;
	numIndexes += numverts + numverts + numverts - 6;

	for( count = 2; count < numverts; count++, currentIndex += 3 ) {
		currentIndex[0] = numVerts;
		currentIndex[1] = numVerts + count - 1;
		currentIndex[2] = numVerts + count;
	}
}

static inline void R_PushMesh( const mesh_t *mesh, int features )
{
	int numverts;

	if( !(mesh->indexes || (features & MF_TRIFAN)) || !mesh->xyzArray )
		return;

	r_features = features;

	if( features & MF_TRIFAN )
		R_PushTrifanIndexes( mesh->numVertexes );
	else
#if SHADOW_VOLUMES
		R_PushIndexes( mesh->indexes, mesh->trneighbors, mesh->trnormals, mesh->numIndexes, features );
#else
		R_PushIndexes( mesh->indexes, mesh->numIndexes, features );
#endif

	numverts = mesh->numVertexes;

	if( features & MF_NONBATCHED ) {
		if( features & MF_DEFORMVS ) {
			if( mesh->xyzArray != inVertsArray )
				memcpy( inVertsArray, mesh->xyzArray, numverts * sizeof(vec3_t) );

			if( (features & MF_NORMALS) && mesh->normalsArray && (mesh->normalsArray != inNormalsArray ) )
				memcpy( inNormalsArray, mesh->normalsArray, numverts * sizeof(vec3_t) );
		} else {
			vertsArray = mesh->xyzArray;

			if( (features & MF_NORMALS) && mesh->normalsArray )
				normalsArray = mesh->normalsArray;
		}

		if( (features & MF_STCOORDS) && mesh->stArray )
			coordsArray = mesh->stArray;

		if( (features & MF_LMCOORDS) && mesh->lmstArray[0] ) {
			lightmapCoordsArray[0] = mesh->lmstArray[0];
			if( features & MF_LMCOORDS1 ) {
				lightmapCoordsArray[1] = mesh->lmstArray[1];
				if( features & MF_LMCOORDS2 ) {
					lightmapCoordsArray[2] = mesh->lmstArray[2];
					if( features & MF_LMCOORDS3 )
						lightmapCoordsArray[3] = mesh->lmstArray[3];
				}
			}
		}

		if( (features & MF_SVECTORS) && mesh->sVectorsArray )
			sVectorsArray = mesh->sVectorsArray;
	} else {
		memcpy( inVertsArray[numVerts], mesh->xyzArray, numverts * sizeof(vec3_t) );

		if( (features & MF_NORMALS) && mesh->normalsArray )
			memcpy( inNormalsArray[numVerts], mesh->normalsArray, numverts * sizeof(vec3_t) );

		if( (features & MF_STCOORDS) && mesh->stArray )
			memcpy( inCoordsArray[numVerts], mesh->stArray, numverts * sizeof(vec2_t) );

		if( (features & MF_LMCOORDS) && mesh->lmstArray[0] ) {
			memcpy( inLightmapCoordsArray[0][numVerts], mesh->lmstArray[0], numverts * sizeof(vec2_t) );
			if( features & MF_LMCOORDS1 ) {
				memcpy( inLightmapCoordsArray[1][numVerts], mesh->lmstArray[1], numverts * sizeof(vec2_t) );
				if( features & MF_LMCOORDS2 ) {
					memcpy( inLightmapCoordsArray[2][numVerts], mesh->lmstArray[2], numverts * sizeof(vec2_t) );
					if( features & MF_LMCOORDS3 )
						memcpy( inLightmapCoordsArray[3][numVerts], mesh->lmstArray[3], numverts * sizeof(vec2_t) );
				}
			}
		}

		if( (features & MF_SVECTORS) && mesh->sVectorsArray )
			memcpy( inSVectorsArray[numVerts], mesh->sVectorsArray, numverts * sizeof(vec4_t) );
	}

	if( (features & MF_COLORS) && mesh->colorsArray[0] ) {
		memcpy( inColorsArray[0][numVerts], mesh->colorsArray[0], numverts * sizeof(byte_vec4_t) );
		if( features & MF_COLORS1 ) {
			memcpy( inColorsArray[1][numVerts], mesh->colorsArray[1], numverts * sizeof(byte_vec4_t) );
			if( features & MF_COLORS2 ) {
				memcpy( inColorsArray[2][numVerts], mesh->colorsArray[2], numverts * sizeof(byte_vec4_t) );
				if( features & MF_COLORS3 )
					memcpy( inColorsArray[3][numVerts], mesh->colorsArray[3], numverts * sizeof(byte_vec4_t) );
			}
		}
	}

	numVerts += numverts;
	r_numverts += numverts;
}

static inline qboolean R_MeshOverflow( const mesh_t *mesh )
{
	return ( numVerts + mesh->numVertexes > MAX_ARRAY_VERTS || 
		numIndexes + mesh->numIndexes > MAX_ARRAY_INDEXES );
}

static inline qboolean R_InvalidMesh( const mesh_t *mesh )
{
	return ( !mesh->numVertexes || !mesh->numIndexes || 
		mesh->numVertexes > MAX_ARRAY_VERTS || mesh->numIndexes > MAX_ARRAY_INDEXES );
}

void R_RenderMeshBuffer( const meshbuffer_t *mb, qboolean shadowpass );
