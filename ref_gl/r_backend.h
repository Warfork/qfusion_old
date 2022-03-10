/*
Copyright (C) 2001-2002 Victor Luchits

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

extern vec4_t	vertexArray[MAX_ARRAY_VERTS*2];	// the second half is for shadow volumes
extern vec3_t	normalsArray[MAX_ARRAY_VERTS];

extern vec4_t	tempVertexArray[MAX_ARRAY_VERTS];
extern vec3_t	tempNormalsArray[MAX_ARRAY_VERTS];

extern index_t	*indexesArray;
extern int		*neighborsArray;
extern vec3_t	*trNormalsArray;
extern vec2_t	*coordsArray;
extern vec2_t	*lightmapCoordsArray;

extern index_t	inIndexesArray[MAX_ARRAY_INDEXES];
extern int		inNeighborsArray[MAX_ARRAY_NEIGHBORS];
extern vec3_t	inTrNormalsArray[MAX_ARRAY_TRIANGLES];
extern vec2_t	inCoordsArray[MAX_ARRAY_VERTS];
extern vec2_t	inLightmapCoordsArray[MAX_ARRAY_VERTS];
extern vec4_t	inColorsArray[MAX_ARRAY_VERTS];

extern int		numVerts, numIndexes, numColors;

extern index_t	*currentIndex;
extern int		*currentTrNeighbor;
extern float	*currentTrNormal;
extern float	*currentVertex;
extern float	*currentNormal;
extern float	*currentCoords;
extern float	*currentLightmapCoords;
extern float	*currentColor;

extern unsigned int	r_numverts;
extern unsigned int	r_numtris;

extern qboolean r_blocked;
extern qboolean r_arrays_locked;

extern unsigned int r_quad_indexes[6];

void R_BackendInit (void);
void R_BackendShutdown (void);
void R_BackendStartFrame (void);
void R_BackendEndFrame (void);

void R_LockArrays (int numverts);
void R_UnlockArrays (void);
void R_UnlockArrays (void);
void R_FlushArrays (void);
void R_FlushArraysMtex (void);
void R_ClearArrays (void);

void R_DrawTriangleStrips (index_t *indexes, int numindexes);

#define MF_NONE			0
#define	MF_NONBATCHED	1
#define MF_NORMALS		2
#define MF_STCOORDS		4
#define MF_LMCOORDS		8
#define MF_COLORS		16
#define MF_TRNORMALS	32

static inline void R_ResetTexState (void)
{
	coordsArray = inCoordsArray;
	lightmapCoordsArray = inLightmapCoordsArray;

	currentCoords = coordsArray[0];
	currentLightmapCoords = lightmapCoordsArray[0];

	numColors = 0;
	currentColor = inColorsArray[0];
}

static inline void R_PushIndexes ( index_t *indexes, int *neighbors, vec3_t *trnormals, int numindexes, int features )
{
	int i;
	int numTris;


	// this is a fast path for non-batched geometry, use carefully 
	// used on pics, sprites, .dpm, .md3 and .md2 models
	if ( features & MF_NONBATCHED ) {
		if ( numindexes > MAX_ARRAY_INDEXES ) {
			numindexes = MAX_ARRAY_INDEXES;
		}

		// simply change indexesArray to point at indexes
		numIndexes = numindexes;
		indexesArray = indexes;
		currentIndex = indexesArray + numIndexes;

		if ( neighbors ) {
			neighborsArray = neighbors;
			currentTrNeighbor = neighborsArray + numIndexes;
		}

		if ( trnormals && (features & MF_TRNORMALS) ) {
			numTris = numIndexes / 3;

			trNormalsArray = trnormals;
			currentTrNormal = trNormalsArray[0] + numTris;
		}
	} else {
		// clamp
		if ( numIndexes + numindexes > MAX_ARRAY_INDEXES ) {
			numindexes = MAX_ARRAY_INDEXES - numIndexes;
		}

		numTris = numindexes / 3;
		numIndexes += numindexes;

		// the following code assumes that R_PushIndexes is fed with triangles...
		for ( i=0; i<numTris; i++, indexes += 3, currentIndex += 3 )
		{
			currentIndex[0] = numVerts + indexes[0];
			currentIndex[1] = numVerts + indexes[1];
			currentIndex[2] = numVerts + indexes[2];

			if ( neighbors ) {
				currentTrNeighbor[0] = numTris + neighbors[0];
				currentTrNeighbor[1] = numTris + neighbors[1];
				currentTrNeighbor[2] = numTris + neighbors[2];

				neighbors += 3;
				currentTrNeighbor += 3;
			}

			if ( trnormals && (features & MF_TRNORMALS) ) {
				VectorCopy ( trnormals[i], currentTrNormal );
				currentTrNormal += 3;
			}
		}
	}
}

static inline void R_PushMesh ( mesh_t *mesh, int features )
{
	int numverts;

	if ( !mesh->indexes || !mesh->xyz_array ) {
		return;
	}

	R_PushIndexes ( mesh->indexes, mesh->trneighbors, mesh->trnormals, mesh->numindexes, features );

	numverts = mesh->numvertexes;
	if ( numVerts + numverts > MAX_ARRAY_VERTS ) {
		numverts = MAX_ARRAY_VERTS - numVerts;
	}

	memcpy ( currentVertex, mesh->xyz_array, numverts * sizeof(vec4_t) );
	currentVertex += numverts * 4;

	if ( mesh->normals_array && (features & MF_NORMALS) ) {
		memcpy ( currentNormal, mesh->normals_array, numverts * sizeof(vec3_t) );
		currentNormal += numverts * 3;
	}

	if ( mesh->st_array && (features & MF_STCOORDS) ) {
		if ( features & MF_NONBATCHED ) {
			coordsArray = mesh->st_array;
			currentCoords = coordsArray[0];
		} else {
			memcpy ( currentCoords, mesh->st_array, numverts * sizeof(vec2_t) );
		}

		currentCoords += numverts * 2;
	}

	if ( mesh->lmst_array && (features & MF_LMCOORDS) ) {
		if ( features & MF_NONBATCHED ) {
			lightmapCoordsArray = mesh->lmst_array;
			currentLightmapCoords = lightmapCoordsArray[0];
		} else {
			memcpy ( currentLightmapCoords, mesh->lmst_array, numverts * sizeof(vec2_t) );
		}

		currentLightmapCoords += numverts * 2;
	}

	if ( mesh->colors_array && (features & MF_COLORS) ) {
		memcpy ( currentColor, mesh->colors_array, numverts * sizeof(vec4_t) );
		currentColor += numverts * 4;
	}

	numVerts += numverts;
	r_numverts += numverts;
}

static inline qboolean R_BackendOverflow ( mesh_t *mesh )
{
	return ( numVerts + mesh->numvertexes > MAX_ARRAY_VERTS || 
		numIndexes + mesh->numindexes > MAX_ARRAY_INDEXES );
}

static inline qboolean R_MeshIsInvalid ( mesh_t *mesh )
{
	return ( !mesh->numvertexes || !mesh->numindexes || 
		mesh->numvertexes > MAX_ARRAY_VERTS || mesh->numindexes > MAX_ARRAY_INDEXES );
}

void R_RenderMeshBuffer ( meshbuffer_t *mb, qboolean shadowpass );
void R_RenderMeshGeneric ( meshbuffer_t *mb, shaderpass_t *pass );
void R_RenderMeshMultitextured ( meshbuffer_t *mb, shaderpass_t *pass );
void R_RenderMeshCombined ( meshbuffer_t *mb, shaderpass_t *pass );


