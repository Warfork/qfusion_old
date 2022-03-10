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
#include "r_local.h"

#define FTABLE_SIZE		1024
#define FTABLE_CLAMP(x)	(((int)((x)*FTABLE_SIZE) & (FTABLE_SIZE-1)))
#define FTABLE_EVALUATE(table,x) (table[FTABLE_CLAMP(x)])

static	float	r_sintable[FTABLE_SIZE];
static	float	r_triangletable[FTABLE_SIZE];
static	float	r_squaretable[FTABLE_SIZE];
static	float	r_sawtoothtable[FTABLE_SIZE];
static	float	r_inversesawtoothtable[FTABLE_SIZE];
static	float	r_noisetable[FTABLE_SIZE];

#if SHADOW_VOLUMES
vec3_t			inVertsArray[MAX_ARRAY_VERTS*2];
#else
vec3_t			inVertsArray[MAX_ARRAY_VERTS];
#endif

vec3_t			inNormalsArray[MAX_ARRAY_VERTS];
index_t			inIndexesArray[MAX_ARRAY_INDEXES];
vec2_t			inCoordsArray[MAX_ARRAY_VERTS];
vec2_t			inLightmapCoordsArray[MAX_ARRAY_VERTS];
byte_vec4_t		inColorsArray[MAX_ARRAY_VERTS];

vec2_t			tUnitCoordsArray[MAX_TEXTURE_UNITS][MAX_ARRAY_VERTS];

vec3_t			tempVertexArray[MAX_ARRAY_VERTS];
vec3_t			tempNormalsArray[MAX_ARRAY_VERTS];

index_t			*indexesArray;
vec3_t			*vertsArray;
vec3_t			*normalsArray;
vec2_t			*coordsArray;
vec2_t			*lightmapCoordsArray;
byte_vec4_t		colorArray[MAX_ARRAY_VERTS];

#if SHADOW_VOLUMES
int				inNeighborsArray[MAX_ARRAY_NEIGHBORS];
vec3_t			inTrNormalsArray[MAX_ARRAY_TRIANGLES];

int				*neighborsArray;
vec3_t			*trNormalsArray;

int				*currentTrNeighbor;
float			*currentTrNormal;
#endif

int				r_numVertexBufferObjects;
GLuint			r_vertexBufferObjects[MAX_VERTEX_BUFFER_OBJECTS];

int				numVerts, numIndexes, numColors;

qboolean		r_arraysLocked;
qboolean		r_blocked;
qboolean		r_triangleOutlines;
qboolean		r_enableNormals;

int				r_features;

static	int		r_lightmapNum;
static	shader_t *r_currentShader;
static	float	r_currentShaderTime;
static	unsigned int r_patchWidth, r_patchHeight;
static	mfog_t	*r_texFog, *r_colorFog;

static	image_t	*r_texPointers[MAX_SHADER_PASSES];

static	shaderpass_t	*r_accumPasses[MAX_TEXTURE_UNITS];
static	int		r_numAccumPasses;

static	int		r_identityLighting;

unsigned int	r_numverts;
unsigned int	r_numtris;
unsigned int	r_numflushes;

index_t			r_quad_indexes[6] = { 0, 1, 2, 0, 2, 3 };
index_t			r_trifan_indexes[(MAX_ARRAY_VERTS-2)*3];

void R_DrawTriangles( void );
void R_DrawNormals( void );
void R_RenderFogOnMesh( void );

/*
==============
R_BackendInit
==============
*/
void R_BackendInit( void )
{
	int i;
	double t;
	index_t *index;

	numVerts = 0;
	numIndexes = 0;
    numColors = 0;

	indexesArray = inIndexesArray;
	vertsArray = inVertsArray;
	normalsArray = inNormalsArray;
	coordsArray = inCoordsArray;
	lightmapCoordsArray = inLightmapCoordsArray;

#if SHADOW_VOLUMES
	neighborsArray = inNeighborsArray;
	trNormalsArray = inTrNormalsArray;

	currentTrNeighbor = neighborsArray;
	currentTrNormal = trNormalsArray[0];
#endif

	r_numAccumPasses = 0;

	r_arraysLocked = qfalse;
	r_blocked = qfalse;
	r_triangleOutlines = qfalse;
	r_numVertexBufferObjects = 0;

	if( glConfig.vertexBufferObject ) {
		r_numVertexBufferObjects = VBO_ENDMARKER + glConfig.maxTextureUnits - 1;
		if( r_numVertexBufferObjects > MAX_VERTEX_BUFFER_OBJECTS )
			r_numVertexBufferObjects = MAX_VERTEX_BUFFER_OBJECTS;

		qglGenBuffersARB( r_numVertexBufferObjects, r_vertexBufferObjects );

		for( i = 0; i < r_numVertexBufferObjects; i++ ) {
			if( i == VBO_INDEXES )
				qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, r_vertexBufferObjects[i] );
			else
				qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[i] );

			if( i == VBO_VERTS ) {
#if SHADOW_VOLUMES
				qglBufferDataARB( GL_ARRAY_BUFFER_ARB, MAX_ARRAY_VERTS * 2 * sizeof( vec3_t ), NULL, GL_STREAM_DRAW_ARB );
#else
				qglBufferDataARB( GL_ARRAY_BUFFER_ARB, MAX_ARRAY_VERTS * sizeof( vec3_t ), NULL, GL_STREAM_DRAW_ARB );
#endif
				qglVertexPointer( 3, GL_FLOAT, 0, 0 );
			} else if( i == VBO_NORMALS ) {
				qglBufferDataARB( GL_ARRAY_BUFFER_ARB, MAX_ARRAY_VERTS * sizeof( vec3_t ), NULL, GL_STREAM_DRAW_ARB );
				qglNormalPointer( GL_FLOAT, 12, 0 );
			} else if( i == VBO_COLORS ) {
				qglBufferDataARB( GL_ARRAY_BUFFER_ARB, MAX_ARRAY_VERTS * sizeof( byte_vec4_t ), NULL, GL_STREAM_DRAW_ARB );
				qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, 0 );
			} else if( i == VBO_INDEXES ){
				qglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, MAX_ARRAY_INDEXES * sizeof( int ), NULL, GL_STREAM_DRAW_ARB );
			} else {
				qglBufferDataARB( GL_ARRAY_BUFFER_ARB, MAX_ARRAY_VERTS * sizeof( vec2_t ), NULL, GL_STREAM_DRAW_ARB );

				GL_SelectTexture( i - VBO_TC0 );
				qglTexCoordPointer( 2, GL_FLOAT, 0, 0 );
				if( i > VBO_TC0 )
					GL_SelectTexture( 0 );
			}
		}

		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	}

	qglEnableClientState( GL_VERTEX_ARRAY );

	if( !r_ignorehwgamma->integer )
		r_identityLighting = (int)(255.0f / pow(2, max(0, floor(r_overbrightbits->value))));
	else
		r_identityLighting = 255;

	// build lookup tables
	for( i = 0; i < FTABLE_SIZE; i++ ) {
		t = (double)i / (double)FTABLE_SIZE;

		r_sintable[i] = sin( t * M_TWOPI );

		if( t < 0.25 ) 
			r_triangletable[i] = t * 4.0;
		else if( t < 0.75 )
			r_triangletable[i] = 2 - 4.0 * t;
		else
			r_triangletable[i] = (t - 0.75) * 4.0 - 1.0;

		if( t < 0.5 )
			r_squaretable[i] = 1.0f;
		else
			r_squaretable[i] = -1.0f;

		r_sawtoothtable[i] = t;
		r_inversesawtoothtable[i] = 1.0 - t;
	}

	// build trifan indexes
	index = r_trifan_indexes;
	for ( i = 0; i < MAX_ARRAY_VERTS-2; i++, index += 3 ) {
		index[0] = 0;
		index[1] = i + 1;
		index[2] = i + 2;
	}
}

/*
==============
R_BackendShutdown
==============
*/
void R_BackendShutdown( void )
{
	if( r_numVertexBufferObjects )
		qglDeleteBuffersARB( r_numVertexBufferObjects, r_vertexBufferObjects );
}

/*
==============
R_ResetBackendPointers
==============
*/
inline void R_ResetBackendPointers( void )
{
	numColors = 0;

	vertsArray = inVertsArray;
	normalsArray = inNormalsArray;
	coordsArray = inCoordsArray;
	lightmapCoordsArray = inLightmapCoordsArray;
}

/*
==============
R_FastSin
==============
*/
float R_FastSin( float t ) {
	return FTABLE_EVALUATE( r_sintable, t );
}

/*
=============
R_LatLongToNorm
=============
*/
void R_LatLongToNorm( qbyte latlong[2], vec3_t out )
{
	float sin_a, sin_b, cos_a, cos_b;

	sin_a = (float)latlong[0] * (1.0 / 255.0);
	cos_a = R_FastSin( sin_a + 0.25 );
	sin_a = R_FastSin( sin_a );
	sin_b = (float)latlong[1] * (1.0 / 255.0);
	cos_b = R_FastSin( sin_b + 0.25 );
	sin_b = R_FastSin( sin_b );

	VectorSet( out, cos_b * sin_a, sin_b * sin_a, cos_a );
}

/*
==============
R_TableForFunc
==============
*/
static float *R_TableForFunc( unsigned int func )
{
	switch( func ) {
		case SHADER_FUNC_SIN:
			return r_sintable;
		case SHADER_FUNC_TRIANGLE:
			return r_triangletable;
		case SHADER_FUNC_SQUARE:
			return r_squaretable;
		case SHADER_FUNC_SAWTOOTH:
			return r_sawtoothtable;
		case SHADER_FUNC_INVERSESAWTOOTH:
			return r_inversesawtoothtable;
		case SHADER_FUNC_NOISE:
			return r_noisetable;
	}

	// assume error
	Com_Error( ERR_DROP, "R_TableForFunc: unknown function" );

	return NULL;
}

/*
==============
R_BackendStartFrame
==============
*/
void R_BackendStartFrame( void )
{
	static int prevupdate;
	static int rupdate = 300;

	r_numverts = 0;
	r_numtris = 0;
	r_numflushes = 0;

	if( prevupdate > (curtime % rupdate) ) {
		int i, j, k;
		float t;

		j = random () * (FTABLE_SIZE/4);
		k = random () * (FTABLE_SIZE/2);

		for( i = 0; i < FTABLE_SIZE; i++ ) {
			if( i >= j && i < (j+k) ) {
				t = (double)((i-j)) / (double)(k);
				r_noisetable[i] = R_FastSin ( t + 0.25 );
			} else {
				r_noisetable[i] = 1;
			}
		}
		rupdate = 300 + rand() % 300;
	}
	prevupdate = (curtime % rupdate);
}

/*
==============
R_BackendEndFrame
==============
*/
void R_BackendEndFrame( void )
{
	if( r_speeds->integer && !(r_refdef.rdflags & RDF_NOWORLDMODEL) )
		Com_Printf( "%4i wpoly %4i leafs %4i verts %4i tris %4i flushes\n",
			c_brush_polys, 
			c_world_leafs,
			r_numverts,
			r_numtris,
			r_numflushes ); 
}

/*
==============
R_LockArrays
==============
*/
void R_LockArrays( int numverts )
{
	if( r_arraysLocked )
		return;
	if( glConfig.compiledVertexArray ) {
		qglLockArraysEXT( 0, numverts );
		r_arraysLocked = qtrue;
	}
}

/*
==============
R_UnlockArrays
==============
*/
void R_UnlockArrays (void)
{
	if( !r_arraysLocked )
		return;

	qglUnlockArraysEXT();
	r_arraysLocked = qfalse;
}

/*
==============
R_ClearArrays
==============
*/
void R_ClearArrays( void )
{
	numVerts = 0;
	numIndexes = 0;
	numColors = 0;

	indexesArray = inIndexesArray;
#if SHADOW_VOLUMES
	neighborsArray = inNeighborsArray;
	trNormalsArray = inTrNormalsArray;

	currentTrNeighbor = neighborsArray;
	currentTrNormal = trNormalsArray[0];
#endif

	R_ResetBackendPointers ();

	r_blocked = qfalse;
}

/*
==============
R_FlushArrays
==============
*/
void R_FlushArrays( void )
{
	if( !numVerts || !numIndexes )
		return;

	if( glConfig.vertexBufferObject ) {
		if( r_enableNormals ) {
			qglEnableClientState( GL_NORMAL_ARRAY );
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_NORMALS] );
			qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec3_t ), normalsArray, GL_STREAM_DRAW_ARB );
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		}

		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

		if( numColors > 1 ) {
			qglEnableClientState( GL_COLOR_ARRAY );
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_COLORS] );
			qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( byte_vec4_t ), colorArray, GL_STREAM_DRAW_ARB );
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		} else if( numColors == 1 ) {
			qglColor4ubv( colorArray[0] );
		}

		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_INDEXES] );
		if( glConfig.drawRangeElements )
			qglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVerts, numIndexes, GL_UNSIGNED_INT, 0 );
		else
			qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,	0 );
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	} else {
		if( r_enableNormals ) {
			qglEnableClientState( GL_NORMAL_ARRAY );
			qglNormalPointer( GL_FLOAT, 12, normalsArray );
		}

		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

		if( numColors > 1 ) {
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
		} else if( numColors == 1 ) {
			qglColor4ubv( colorArray[0] );
		}

		if( glConfig.drawRangeElements )
			qglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVerts, numIndexes, GL_UNSIGNED_INT, indexesArray );
		else
			qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,	indexesArray );
	}

	r_numtris += numIndexes / 3;

	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	if( numColors > 1 )
		qglDisableClientState( GL_COLOR_ARRAY );

	if( r_enableNormals ) {
		qglDisableClientState( GL_NORMAL_ARRAY );
		r_enableNormals = qfalse;
	}

	r_numflushes++;
}

/*
==============
R_FlushArraysMtex
==============
*/
void R_FlushArraysMtex( void )
{
	int i;

	if( !numVerts || !numIndexes )
		return;

	if( glConfig.vertexBufferObject ) {
		if( r_enableNormals ) {
			qglEnableClientState( GL_NORMAL_ARRAY );
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_NORMALS] );
			qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec3_t ), normalsArray, GL_STREAM_DRAW_ARB );
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		}

		GL_Bind( 0, r_texPointers[0] );
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

		if( numColors > 1 ) {
			qglEnableClientState( GL_COLOR_ARRAY );
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_COLORS] );
			qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( byte_vec4_t ), colorArray, GL_STREAM_DRAW_ARB );
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		} else if( numColors == 1 ) {
			qglColor4ubv( colorArray[0] );
		}

		for( i = 1; i < r_numAccumPasses; i++ ) {
			GL_Bind( i, r_texPointers[i] );
			qglEnable( GL_TEXTURE_2D );
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		}

		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_INDEXES] );
		if( glConfig.drawRangeElements )
			qglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVerts, numIndexes, GL_UNSIGNED_INT, 0 );
		else
			qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,	0 );
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	} else {
		if( r_enableNormals ) {
			qglEnableClientState( GL_NORMAL_ARRAY );
			qglNormalPointer( GL_FLOAT, 12, normalsArray );
		}

		GL_Bind( 0, r_texPointers[0] );
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

		if( numColors > 1 ) {
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
		} else if( numColors == 1 ) {
			qglColor4ubv( colorArray[0] );
		}

		for( i = 1; i < r_numAccumPasses; i++ ) {
			GL_Bind( i, r_texPointers[i] );
			qglEnable( GL_TEXTURE_2D );
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		}

		if( glConfig.drawRangeElements )
			qglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVerts, numIndexes, GL_UNSIGNED_INT, indexesArray );
		else
			qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,	indexesArray );
	}

	r_numtris += numIndexes / 3;

	for( i = r_numAccumPasses - 1; i > 0; i-- ) {
		GL_SelectTexture( i );
		qglDisable( GL_TEXTURE_2D );
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	GL_SelectTexture( 0 );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	if( numColors > 1 )
		qglDisableClientState( GL_COLOR_ARRAY );

	if( r_enableNormals ) {
		qglDisableClientState( GL_NORMAL_ARRAY );
		r_enableNormals = qfalse;
	}

	r_numflushes++;
}

/*
================
R_DeformVertices
================
*/
void R_DeformVertices( void )
{
	int i, j, k, p;
	float args[4], deflect;
	float *quad[4], *table;
	deformv_t *deformv;
	vec3_t tv, rot_centre;

	deformv = &r_currentShader->deforms[0];
	for( i = 0; i < r_currentShader->numdeforms; i++, deformv++ ) {
		switch( deformv->type ) {
			case DEFORMV_NONE:
				break;
			case DEFORMV_WAVE:
				args[0] = deformv->func.args[0];
				args[1] = deformv->func.args[1];
				args[3] = deformv->func.args[2] + deformv->func.args[3] * r_currentShaderTime;
				table = R_TableForFunc( deformv->func.type );

				for ( j = 0; j < numVerts; j++ ) {
					deflect = deformv->args[0] * (inVertsArray[j][0] + inVertsArray[j][1] + inVertsArray[j][2]) + args[3];
					deflect = FTABLE_EVALUATE( table, deflect ) * args[1] + args[0];

					// Deflect vertex along its normal by wave amount
					VectorMA( inVertsArray[j], deflect, inNormalsArray[j], inVertsArray[j] );
				}
				break;
			case DEFORMV_NORMAL:
				args[0] = deformv->args[1] * r_currentShaderTime;

				for( j = 0; j < numVerts; j++ ) {
					args[1] = inNormalsArray[j][2] * args[0];

					deflect = deformv->args[0] * R_FastSin( args[1] );
					inNormalsArray[j][0] *= deflect;
					deflect = deformv->args[0] * R_FastSin( args[1] + 0.25 );
					inNormalsArray[j][1] *= deflect;
					VectorNormalizeFast( inNormalsArray[j] );
				}
				break;
			case DEFORMV_MOVE:
				table = R_TableForFunc( deformv->func.type );
				deflect = deformv->func.args[2] + r_currentShaderTime * deformv->func.args[3];
				deflect = FTABLE_EVALUATE( table, deflect ) * deformv->func.args[1] + deformv->func.args[0];

				for( j = 0; j < numVerts; j++ )
					VectorMA ( inVertsArray[j], deflect, deformv->args, inVertsArray[j] );
				break;
			case DEFORMV_BULGE:
				args[0] = deformv->args[0] / (float)r_patchHeight;
				args[1] = deformv->args[1];
				args[2] = r_currentShaderTime / (deformv->args[2] * r_patchWidth);

				for( k = 0, p = 0; k < r_patchHeight; k++ ) {
					deflect = R_FastSin ( (float)k * args[0] + args[2] ) * args[1];
					for( j = 0; j < r_patchWidth; j++, p++ )
						VectorMA ( inVertsArray[p], deflect, inNormalsArray[p], inVertsArray[p] );
				}
				break;
			case DEFORMV_AUTOSPRITE:
				{
					vec3_t m0[3], m1[3], m2[3], result[3];

					if( numIndexes % 6 )
						break;

					if( currententity && (currentmodel != r_worldmodel) )
						Matrix4_Matrix ( r_modelview_matrix, m1 );
					else
						Matrix4_Matrix ( r_worldview_matrix, m1 );

					Matrix_Transpose( m1, m2 );

					for( k = 0; k < numIndexes; k += 6 ) {
						quad[0] = ( float * )( inVertsArray + indexesArray[k+0] );
						quad[1] = ( float * )( inVertsArray + indexesArray[k+1] );
						quad[2] = ( float * )( inVertsArray + indexesArray[k+2] );

						for( j = 2; j >= 0; j-- ) {
							quad[3] = (float *)(inVertsArray + indexesArray[k+3+j]);
							if( !VectorCompare( quad[3], quad[0] ) && 
								!VectorCompare( quad[3], quad[1] ) &&
								!VectorCompare( quad[3], quad[2] ) ) {
								break;
							}
						}

						Matrix_FromPoints( quad[0], quad[1], quad[2], m0 );

						// swap m0[0] an m0[1] - FIXME?
						VectorCopy( m0[1], rot_centre );
						VectorCopy( m0[0], m0[1] );
						VectorCopy( rot_centre, m0[0] );

						Matrix_Multiply( m2, m0, result );

						for( j = 0; j < 3; j++ )
							rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

						for( j = 0; j < 4; j++ ) {
							VectorSubtract( quad[j], rot_centre, tv );
							Matrix_TransformVector( result, tv, quad[j] );
							VectorAdd( rot_centre, quad[j], quad[j] );
						}
					}
				}
				break;
			case DEFORMV_AUTOSPRITE2:
				if ( numIndexes % 6 )
					break;

				for( k = 0; k < numIndexes; k += 6 ) {
					int long_axis, short_axis;
					vec3_t axis, tmp;
					float len[3];
					vec3_t m0[3], m1[3], m2[3], result[3];

					quad[0] = ( float * )( inVertsArray + indexesArray[k+0] );
					quad[1] = ( float * )( inVertsArray + indexesArray[k+1] );
					quad[2] = ( float * )( inVertsArray + indexesArray[k+2] );

					for( j = 2; j >= 0; j-- ) {
						quad[3] = ( float * )( inVertsArray + indexesArray[k+3+j] );

						if( !VectorCompare( quad[3], quad[0] ) && 
							!VectorCompare( quad[3], quad[1] ) &&
							!VectorCompare( quad[3], quad[2] ) ) {
							break;
						}
					}

					// build a matrix were the longest axis of the billboard is the Y-Axis
					VectorSubtract( quad[1], quad[0], m0[0] );
					VectorSubtract( quad[2], quad[0], m0[1] );
					VectorSubtract( quad[2], quad[1], m0[2] );
					len[0] = DotProduct( m0[0], m0[0] );
					len[1] = DotProduct( m0[1], m0[1] );
					len[2] = DotProduct( m0[2], m0[2] );

					if( (len[2] > len[1]) && (len[2] > len[0]) ) {
						if( len[1] > len[0] ) {
							long_axis = 1;
							short_axis = 0;
						} else {
							long_axis = 0;
							short_axis = 1;
						}
					} else if( (len[1] > len[2]) && (len[1] > len[0]) ) {
						if( len[2] > len[0] ) {
							long_axis = 2;
							short_axis = 0;
						} else {
							long_axis = 0;
							short_axis = 2;
						}
					} else if( (len[0] > len[1]) && (len[0] > len[2]) ) {
						if( len[2] > len[1] ) {
							long_axis = 2;
							short_axis = 1;
						} else {
							long_axis = 1;
							short_axis = 2;
						}
					}

					if( !len[long_axis] )
						break;
					len[long_axis] = Q_RSqrt( len[long_axis] );
					VectorScale( m0[long_axis], len[long_axis], axis );

					if( DotProduct( m0[long_axis], m0[short_axis] ) ) {
						VectorCopy( axis, m0[1] );
						if( axis[0] || axis[1] )
							MakeNormalVectors( m0[1], m0[0], m0[2] );
						else
							MakeNormalVectors( m0[1], m0[2], m0[0] );
					} else {
						if( !len[short_axis] )
							break;
						len[short_axis] = Q_RSqrt( len[short_axis] );
						VectorScale( m0[short_axis], len[short_axis], m0[0] );
						VectorCopy( axis, m0[1] );
						CrossProduct( m0[0], m0[1], m0[2] );
					}

					for( j = 0; j < 3; j++ )
						rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

					if( currententity && (currentmodel != r_worldmodel) ) {
						VectorAdd( currententity->origin, rot_centre, tv );
						VectorSubtract( r_origin, tv, tmp );
						Matrix_TransformVector( currententity->axis, tmp, tv );
					} else {
						VectorCopy( rot_centre, tv );
						VectorSubtract( r_origin, tv, tv );
					}

					// filter any longest-axis-parts off the camera-direction
					deflect = -DotProduct( tv, axis );

					VectorMA( tv, deflect, axis, m1[2] );
					VectorNormalizeFast( m1[2] );
					VectorCopy( axis, m1[1] );
					CrossProduct( m1[1], m1[2], m1[0] );

					Matrix_Transpose( m1, m2 );
					Matrix_Multiply( m2, m0, result );

					for( j = 0; j < 4; j++ ) {
						VectorSubtract( quad[j], rot_centre, tv );
						Matrix_TransformVector( result, tv, quad[j] );
						VectorAdd( rot_centre, quad[j], quad[j] );
					}
				}
				break;
			case DEFORMV_PROJECTION_SHADOW:
				break;
			case DEFORMV_AUTOPARTICLE:
				{
					float scale;
					vec3_t m0[3], m1[3], m2[3], result[3];

					if( numIndexes % 6 )
						break;

					if ( currententity && (currentmodel != r_worldmodel) )
						Matrix4_Matrix( r_modelview_matrix, m1 );
					else
						Matrix4_Matrix( r_worldview_matrix, m1 );

					Matrix_Transpose( m1, m2 );

					for( k = 0; k < numIndexes; k += 6 ) {
						quad[0] = ( float * )( inVertsArray + indexesArray[k+0] );
						quad[1] = ( float * )( inVertsArray + indexesArray[k+1] );
						quad[2] = ( float * )( inVertsArray + indexesArray[k+2] );

						for( j = 2; j >= 0; j-- ) {
							quad[3] = ( float * )( inVertsArray + indexesArray[k+3+j] );

							if( !VectorCompare( quad[3], quad[0] ) && 
								!VectorCompare( quad[3], quad[1] ) &&
								!VectorCompare( quad[3], quad[2] ) ) {
								break;
							}
						}

						Matrix_FromPoints( quad[0], quad[1], quad[2], m0 );
						Matrix_Multiply( m2, m0, result );

						// hack a scale up to keep particles from disappearing
						scale = (quad[0][0] - r_origin[0]) * vpn[0] + (quad[0][1] - r_origin[1]) * vpn[1] + (quad[0][2] - r_origin[2]) * vpn[2];
						if ( scale < 20 )
							scale = 1.5;
						else
							scale = 1.5 + scale * 0.006f;

						for( j = 0; j < 3; j++ )
							rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

						for( j = 0; j < 4; j++ ) {
							VectorSubtract( quad[j], rot_centre, tv );
							Matrix_TransformVector( result, tv, quad[j] );
							VectorMA( rot_centre, scale, quad[j], quad[j] );
						}
					}
				}
				break;
			default:
				break;
		}
	}
}

/*
==============
R_VertexTCBase
==============
*/
void R_VertexTCBase( shaderpass_t *pass, int unit )
{
	int	i;
	float *outCoords, depth;
	vec3_t transform;
	vec3_t n, projection;
	vec3_t inverse_axis[3];

	if( pass->tcgen == TC_GEN_REFLECTION ) {
		r_enableNormals = qtrue;
		qglEnable( GL_TEXTURE_GEN_S );
		qglEnable( GL_TEXTURE_GEN_T );
		qglEnable( GL_TEXTURE_GEN_R );
		qglTexGeni( GL_S, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );
		qglTexGeni( GL_T, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );
		qglTexGeni( GL_R, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );
		return;
	}

	qglDisable( GL_TEXTURE_GEN_S );
	qglDisable( GL_TEXTURE_GEN_T );
	qglDisable( GL_TEXTURE_GEN_R );

	if( glConfig.vertexBufferObject )
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_TC0+unit] );

	if( pass->tcgen == TC_GEN_BASE ) {
		if( glConfig.vertexBufferObject )
			qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec2_t ), coordsArray, GL_STREAM_DRAW_ARB );
		else
			qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArray );
	} else if( pass->tcgen == TC_GEN_LIGHTMAP ) {
		if( glConfig.vertexBufferObject )
			qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec2_t ), lightmapCoordsArray, GL_STREAM_DRAW_ARB );
		else
			qglTexCoordPointer( 2, GL_FLOAT, 0, lightmapCoordsArray );
	} else if( pass->tcgen == TC_GEN_ENVIRONMENT ) {
		outCoords = tUnitCoordsArray[unit][0];

		if( currentmodel ) {
			if( currentmodel == r_worldmodel ) {
				VectorSubtract( vec3_origin, r_origin, transform );
			} else if( currentmodel->type == mod_brush ) {
				VectorSubtract( currententity->origin, r_origin, transform );
				Matrix_Transpose( currententity->axis, inverse_axis );
			} else {
				VectorSubtract( currententity->origin, r_origin, transform );
				Matrix_Transpose( currententity->axis, inverse_axis );
			}
		} else {
			VectorClear( transform );
			Matrix_Transpose( axis_identity, inverse_axis );
		}

		for( i = 0; i < numVerts; i++, outCoords += 2 ) {
			VectorAdd( vertsArray[i], transform, projection );
			VectorNormalize( projection );

			// project vector
			if( currentmodel == r_worldmodel )
				VectorCopy( normalsArray[i], n );
			else
				Matrix_TransformVector( inverse_axis, normalsArray[i], n );

			depth = -2.0f * DotProduct( n, projection );
			VectorMA( projection, depth, n, projection );
			depth = Q_RSqrt( DotProduct(projection, projection) * 4 );

			outCoords[0] = -((projection[1] * depth) + 0.5f);
			outCoords[1] =  ((projection[2] * depth) + 0.5f);
		}

		if( glConfig.vertexBufferObject )
			qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec2_t ), tUnitCoordsArray[unit][0], GL_STREAM_DRAW_ARB );
		else
			qglTexCoordPointer( 2, GL_FLOAT, 0, tUnitCoordsArray[unit][0] );
	} else if( pass->tcgen == TC_GEN_VECTOR ) {
		outCoords = tUnitCoordsArray[unit][0];

		for( i = 0; i < numVerts; i++, outCoords += 2 ) {
			outCoords[0] = DotProduct( pass->tcgenVec[0], vertsArray[i] ) + pass->tcgenVec[0][3];
			outCoords[1] = DotProduct( pass->tcgenVec[1], vertsArray[i] ) + pass->tcgenVec[1][3];
		}

		if( glConfig.vertexBufferObject )
			qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec2_t ), tUnitCoordsArray[unit][0], GL_STREAM_DRAW_ARB );
		else
			qglTexCoordPointer( 2, GL_FLOAT, 0, tUnitCoordsArray[unit][0] );
	}

	if( glConfig.vertexBufferObject )
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

/*
==============
R_ShaderpassTex
==============
*/
static image_t *R_ShaderpassTex( shaderpass_t *pass )
{
	if( pass->flags & SHADER_PASS_ANIMMAP )
		return pass->anim_frames[(int)(pass->anim_fps * r_currentShaderTime) % pass->anim_numframes];
	else if( pass->flags & SHADER_PASS_LIGHTMAP )
		return r_lightmapTextures[r_lightmapNum];
	return ( pass->anim_frames[0] ? pass->anim_frames[0] : r_notexture );
}

/*
================
R_ModifyTextureCoords
================
*/
void R_ModifyTextureCoords( shaderpass_t *pass, int unit )
{
	int i;
	float *table;
	float t1, t2, sint, cost;
	mat4x4_t m1, m2, result;
	tcmod_t	*tcmod;

	r_texPointers[unit] = R_ShaderpassTex ( pass );

	R_VertexTCBase( pass, unit );

	qglMatrixMode( GL_TEXTURE );

	Matrix4_Identity( result );

	if( pass->numtcmods ) {
		Matrix4_Identity( m2 );

		for( i = 0, tcmod = pass->tcmods; i < pass->numtcmods; i++, tcmod++ ) {
			switch( tcmod->type ) {
				case TC_MOD_ROTATE:
					cost = tcmod->args[0] * r_currentShaderTime;
					sint = R_FastSin( cost );
					cost = R_FastSin( cost + 0.25 );
					m2[0] = 1, m2[1] = 0, m2[4] = 0, m2[5] = 1, m2[12] = -0.5f, m2[13] = -0.5f;
					Matrix4_Copy( result, m1 );
					Matrix4_MultiplyFast2( m2, m1, result );
					m2[0] = cost, m2[1] = sint, m2[4] = -sint, m2[5] = cost, m2[12] = 0, m2[13] = 0;
					Matrix4_Copy( result, m1 );
					Matrix4_MultiplyFast2( m2, m1, result );
					m2[0] = 1, m2[1] = 0, m2[4] = 0, m2[5] = 1, m2[12] = 0.5f, m2[13] = 0.5f;
					Matrix4_Copy( result, m1 );
					Matrix4_MultiplyFast2( m2, m1, result );
					break;
				case TC_MOD_SCALE:
					m2[0] = tcmod->args[0], m2[1] = 0, m2[4] = 0, m2[5] = tcmod->args[1], m2[12] = 0, m2[13] = 0;
					Matrix4_Copy( result, m1 );
					Matrix4_MultiplyFast2( m2, m1, result );
					break;
				case TC_MOD_TURB:
					t1 = (1.0 / 4.0);
					t2 = tcmod->args[2] + r_currentShaderTime * tcmod->args[3];
					m2[0] = 1 + (tcmod->args[1] * R_FastSin( t2 ) + tcmod->args[0]) * t1, m2[1] = 0, m2[4] = 0, 
						m2[5] = 1 + (tcmod->args[1] * R_FastSin( t2 + 0.25 ) + tcmod->args[0]) * t1, m2[12] = 0, m2[13] = 0;
					Matrix4_Copy( result, m1 );
					Matrix4_MultiplyFast2( m2, m1, result );
					break;
				case TC_MOD_STRETCH:
					table = R_TableForFunc( tcmod->args[0] );
					t2 = tcmod->args[3] + r_currentShaderTime * tcmod->args[4];
					t1 = FTABLE_EVALUATE( table, t2 ) * tcmod->args[2] + tcmod->args[1];
					t1 = t1 ? 1.0f / t1 : 1.0f;
					t2 = 0.5f - 0.5f * t1;
					m2[0] = t1, m2[1] = 0, m2[4] = 0, m2[5] = t1, m2[12] = t2, m2[13] = t2;
					Matrix4_Copy( result, m1 );
					Matrix4_MultiplyFast2( m2, m1, result );
					break;
				case TC_MOD_SCROLL:
					t1 = tcmod->args[0] * r_currentShaderTime;
					t2 = tcmod->args[1] * r_currentShaderTime;
					m2[0] = 1, m2[1] = 0, m2[4] = 0, m2[5] = 1, m2[12] = t1 - floor ( t1 ), m2[13] = t2 - floor ( t2 );
					Matrix4_Copy( result, m1 );
					Matrix4_MultiplyFast2( m2, m1, result );
					break;
				case TC_MOD_TRANSFORM:
					m2[0] = tcmod->args[0], m2[1] = tcmod->args[3], m2[4] = tcmod->args[2], 
						m2[5] = tcmod->args[1], m2[12] = tcmod->args[4], m2[13] = tcmod->args[5];
					Matrix4_Copy( result, m1 );
					Matrix4_MultiplyFast2( m2, m1, result );
					break;
				default:
					break;
			}
		}
	}

	if( pass->flags & SHADER_PASS_CUBEMAP )
		qglEnable( GL_TEXTURE_CUBE_MAP_ARB );
	else
		qglDisable( GL_TEXTURE_CUBE_MAP_ARB );

	if( pass->tcgen == TC_GEN_REFLECTION ) {
		Matrix4_Transpose( r_modelview_matrix, m1 );
		Matrix4_Copy( result, m2 );
		Matrix4_Multiply( m2, m1, result );
		qglLoadMatrixf( result );
		return;
	}

	if( pass->numtcmods )
		qglLoadMatrixf( result );
	else
		qglLoadIdentity ();
}

/*
================
R_ModifyColor
================
*/
void R_ModifyColor( shaderpass_t *pass )
{
	int i;
	float *table, c, a;
	int r, g, b;
	vec3_t t, v;
	qbyte *bArray, *inArray;
	qboolean noArray;
	shaderfunc_t *rgbgenfunc, *alphagenfunc;

	noArray = ( pass->flags & SHADER_PASS_NOCOLORARRAY ) && !r_colorFog;
	rgbgenfunc = &pass->rgbgen.func;
	alphagenfunc = &pass->alphagen.func;

	if( noArray )
		numColors = 1;
	else
		numColors = numVerts;

	bArray = colorArray[0];
	inArray = inColorsArray[0];

	switch( pass->rgbgen.type ) {
		case RGB_GEN_IDENTITY:
			memset( bArray, 255, sizeof( byte_vec4_t ) * numColors );
			break;
		case RGB_GEN_IDENTITY_LIGHTING:
			memset( bArray, r_identityLighting, sizeof( byte_vec4_t ) * numColors );
			break;
		case RGB_GEN_CONST:
			r = R_FloatToByte( pass->rgbgen.args[0] );
			g = R_FloatToByte( pass->rgbgen.args[1] );
			b = R_FloatToByte( pass->rgbgen.args[2] );

			for( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[0] = r;
				bArray[1] = g;
				bArray[2] = b;
			}
			break;
		case RGB_GEN_COLORWAVE:
			table = R_TableForFunc( rgbgenfunc->type );
			c = r_currentShaderTime * rgbgenfunc->args[3] + rgbgenfunc->args[2];
			c = FTABLE_EVALUATE( table, c ) * rgbgenfunc->args[1] + rgbgenfunc->args[0];
			a = pass->rgbgen.args[0] * c; r = R_FloatToByte( bound( 0, a, 1 ) );
			a = pass->rgbgen.args[1] * c; g = R_FloatToByte( bound( 0, a, 1 ) );
			a = pass->rgbgen.args[2] * c; b = R_FloatToByte( bound( 0, a, 1 ) );

			for( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[0] = r;
				bArray[1] = g;
				bArray[2] = b;
			}
			break;
		case RGB_GEN_ENTITY:
			r = *(int *)currententity->color;
			for( i = 0; i < numColors; i++, bArray += 4 )
				*(int *)bArray = r;
			break;
		case RGB_GEN_ONE_MINUS_ENTITY:
			for( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[0] = 255 - currententity->color[0];
				bArray[1] = 255 - currententity->color[1];
				bArray[2] = 255 - currententity->color[2];
			}
			break;
		case RGB_GEN_VERTEX:
			if( (r_overbrightbits->integer > 0) && !(r_ignorehwgamma->integer) ) {
				for( i = 0; i < numColors; i++, bArray += 4, inArray += 4 ) {
					bArray[0] = inArray[0] >> r_overbrightbits->integer;
					bArray[1] = inArray[1] >> r_overbrightbits->integer;
					bArray[2] = inArray[2] >> r_overbrightbits->integer;
				}
				break;
			}
		case RGB_GEN_EXACT_VERTEX:
			memcpy( bArray, inArray, sizeof( byte_vec4_t ) * numColors );
			break;
		case RGB_GEN_ONE_MINUS_VERTEX:
			if( (r_overbrightbits->integer > 0) && !(r_ignorehwgamma->integer) ) {
				for( i = 0; i < numColors; i++, bArray += 4, inArray += 4 ) {
					bArray[0] = 255 - (inArray[0] >> r_overbrightbits->integer);
					bArray[1] = 255 - (inArray[1] >> r_overbrightbits->integer);
					bArray[2] = 255 - (inArray[2] >> r_overbrightbits->integer);
				}
			} else {
				for( i = 0; i < numColors; i++, bArray += 4, inArray += 4 ) {
					bArray[0] = 255 - inArray[0];
					bArray[1] = 255 - inArray[1];
					bArray[2] = 255 - inArray[2];
				}
			}
			break;
		case RGB_GEN_LIGHTING_DIFFUSE:
			if( !currententity )
				memset( bArray, 255, sizeof( byte_vec4_t ) * numColors );
			else
				R_LightForEntity( currententity, bArray );
			break;
		default:
			break;
	}

	bArray = colorArray[0];
	inArray = inColorsArray[0];

	switch( pass->alphagen.type ) {
		case ALPHA_GEN_IDENTITY:
			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = 255;
			break;
		case ALPHA_GEN_CONST:
			b = R_FloatToByte( pass->alphagen.args[0] );
			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = b;
			break;
		case ALPHA_GEN_WAVE:
			table = R_TableForFunc( alphagenfunc->type );
			a = alphagenfunc->args[2] + r_currentShaderTime * alphagenfunc->args[3];
			a = FTABLE_EVALUATE( table, a ) * alphagenfunc->args[1] + alphagenfunc->args[0];
			b = R_FloatToByte( bound( 0.0f, a, 1.0f ) );
			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = b;
			break;
		case ALPHA_GEN_PORTAL:
			VectorAdd( vertsArray[0], currententity->origin, v );
			VectorSubtract( r_origin, v, t );
			a = VectorLength( t ) * pass->alphagen.args[0];
			clamp ( a, 0.0f, 1.0f );
			b = R_FloatToByte( a );

			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = b;
			break;
		case ALPHA_GEN_VERTEX:
			for( i = 0; i < numColors; i++, bArray += 4, inArray += 4 )
				bArray[3] = inArray[3];
			break;
		case ALPHA_GEN_ENTITY:
			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = currententity->color[3];
			break;
		case ALPHA_GEN_SPECULAR:
			VectorSubtract( r_origin, currententity->origin, t );
			if( !Matrix_Compare( currententity->axis, axis_identity ) )
				Matrix_TransformVector( currententity->axis, t, v );
			else
				VectorCopy( t, v );

			for( i = 0; i < numColors; i++, bArray += 4 ) {
				VectorSubtract( v, vertsArray[i], t );
				a = DotProduct( t, normalsArray[i] ) * Q_RSqrt( DotProduct( t, t ) );
				a = a * a * a * a * a;
				bArray[3] = R_FloatToByte( bound( 0.0f, a, 1.0f ) );
			}
			break;
		case ALPHA_GEN_DOT:
			if( !Matrix_Compare( currententity->axis, axis_identity ) )
				Matrix_TransformVector( currententity->axis, vpn, v );
			else
				VectorCopy ( vpn, v );

			for( i = 0; i < numColors; i++, bArray += 4 ) {
				a = DotProduct( v, inNormalsArray[i] ); if ( a < 0 ) a = -a;
				bArray[3] = R_FloatToByte( bound( pass->alphagen.args[0], a, pass->alphagen.args[1] ) );
			}
			break;
		case ALPHA_GEN_ONE_MINUS_DOT:
			if( !Matrix_Compare( currententity->axis, axis_identity ) )
				Matrix_TransformVector( currententity->axis, vpn, v );
			else
				VectorCopy( vpn, v );

			for( i = 0; i < numColors; i++, bArray += 4 ) {
				a = DotProduct( v, inNormalsArray[i] ); if ( a < 0 ) a = -a; a = 1.0f - a;
				bArray[3] = R_FloatToByte( bound( pass->alphagen.args[0], a, pass->alphagen.args[1] ) );
			}
			break;
	}

	if ( r_colorFog ) {
		double	dist, vdist;
		cplane_t *fogplane;
		vec3_t	viewtofog;
		double	fognormal[3], vpnnormal[3];
		double	fogdist, vpndist, fogShaderDist;
		int		fogptype;

		fogplane = r_colorFog->visibleplane;
		fogShaderDist = r_colorFog->shader->fog_dist;
		dist = PlaneDiff( r_origin, fogplane );

		if( r_currentShader->flags & SHADER_SKY ) {
			if( dist > 0 )
				VectorScale( fogplane->normal, -dist, viewtofog );
			else
				VectorClear( viewtofog );
		} else {
			VectorCopy( currententity->origin, viewtofog );
		}

		fognormal[0] = DotProduct( currententity->axis[0], fogplane->normal ) * currententity->scale;
		fognormal[1] = DotProduct( currententity->axis[1], fogplane->normal ) * currententity->scale;
		fognormal[2] = DotProduct( currententity->axis[2], fogplane->normal ) * currententity->scale;
		fogptype = ( fognormal[0] == 1.0 ? PLANE_X : (fognormal[1] == 1.0 ? PLANE_Y : (fognormal[2] == 1.0 ? PLANE_Z : PLANE_NONAXIAL) ) );
		if( fogptype > 2 )
			VectorScale( fognormal, fogShaderDist, fognormal );
		fogdist = (fogplane->dist - DotProduct( viewtofog, fogplane->normal )) * fogShaderDist;

		vpnnormal[0] = DotProduct( currententity->axis[0], vpn ) * fogShaderDist * currententity->scale;
		vpnnormal[1] = DotProduct( currententity->axis[1], vpn ) * fogShaderDist * currententity->scale;
		vpnnormal[2] = DotProduct( currententity->axis[2], vpn ) * fogShaderDist * currententity->scale;
		vpndist = ((r_origin[0] - viewtofog[0]) * vpn[0] + (r_origin[1] - viewtofog[1]) * vpn[1] + (r_origin[2] - viewtofog[2]) * vpn[2]) * fogShaderDist;

		bArray = colorArray[0];
		if( dist < 0 ) { // camera is inside the fog
			for( i = 0; i < numColors; i++, bArray += 4 ) {
				c = DotProduct( vertsArray[i], vpnnormal ) - vpndist;
				a = (1.0f - bound ( 0, c, 1.0f )) * (1.0 / 255.0);

				if( pass->blendmode == GL_ADD || 
					((pass->blendsrc == GL_ZERO) && (pass->blenddst == GL_ONE_MINUS_SRC_COLOR)) ) {
					bArray[0] = R_FloatToByte( (float)bArray[0]*a );
					bArray[1] = R_FloatToByte( (float)bArray[1]*a );
					bArray[2] = R_FloatToByte( (float)bArray[2]*a );
				} else {
					bArray[3] = R_FloatToByte( (float)bArray[3]*a );
				}
			}
		} else {
			dist *= fogShaderDist;

			if( fogptype < 3 ) {
				for( i = 0; i < numColors; i++, bArray += 4 ) {
					vdist = vertsArray[i][fogptype] * fogShaderDist - fogdist;

					if( vdist < 0 ) {
						c = ( DotProduct( vertsArray[i], vpnnormal ) - vpndist ) * vdist / ( vdist - dist );
						a = (1.0f - bound( 0, c, 1.0f )) * (1.0 / 255.0);
					} else {
						a = 1.0 / 255.0;
					}

					if( pass->blendmode == GL_ADD || 
						((pass->blendsrc == GL_ZERO) && (pass->blenddst == GL_ONE_MINUS_SRC_COLOR)) ) {
						bArray[0] = R_FloatToByte( (float)bArray[0]*a );
						bArray[1] = R_FloatToByte( (float)bArray[1]*a );
						bArray[2] = R_FloatToByte( (float)bArray[2]*a );
					} else {
						bArray[3] = R_FloatToByte( (float)bArray[3]*a );
					}
				}
			} else {
				for( i = 0; i < numColors; i++, bArray += 4 ) {
					vdist = DotProduct( vertsArray[i], fognormal ) - fogdist;

					if( vdist < 0 ) {
						c = ( DotProduct( vertsArray[i], vpnnormal ) - vpndist ) * vdist / ( vdist - dist );
						a = (1.0f - bound( 0, c, 1.0f )) * (1.0 / 255.0);
					} else {
						a = 1.0 / 255.0;
					}

					if( pass->blendmode == GL_ADD || 
						((pass->blendsrc == GL_ZERO) && (pass->blenddst == GL_ONE_MINUS_SRC_COLOR)) ) {
						bArray[0] = R_FloatToByte( (float)bArray[0]*a );
						bArray[1] = R_FloatToByte( (float)bArray[1]*a );
						bArray[2] = R_FloatToByte( (float)bArray[2]*a );
					} else {
						bArray[3] = R_FloatToByte( (float)bArray[3]*a );
					}
				}
			}
		}
	}
}

/*
================
R_SetCurrentShaderState
================
*/
void R_SetCurrentShaderState( void )
{
// Face culling
	if( !gl_cull->integer || (r_features & MF_NOCULL) ) {
		qglDisable( GL_CULL_FACE );
	} else {
		if( r_currentShader->flags & SHADER_CULL_FRONT ) {
			qglEnable( GL_CULL_FACE );
			qglCullFace( GL_FRONT );
		} else if( r_currentShader->flags & SHADER_CULL_BACK ) {
			qglEnable( GL_CULL_FACE );
			qglCullFace( GL_BACK );
		} else {
			qglDisable( GL_CULL_FACE );
		}
	}

	if( r_currentShader->flags & SHADER_POLYGONOFFSET )
		qglEnable( GL_POLYGON_OFFSET_FILL );
	else
		qglDisable( GL_POLYGON_OFFSET_FILL );
}

/*
================
R_SetShaderpassState
================
*/
void R_SetShaderpassState( shaderpass_t *pass, qboolean mtex )
{
	if( ( mtex && ( pass->blendmode != GL_REPLACE ) ) || ( pass->flags & SHADER_PASS_BLEND ) ) {
		qglEnable( GL_BLEND );
		qglBlendFunc( pass->blendsrc, pass->blenddst );
	} else {
		qglDisable( GL_BLEND );
	}

	if( pass->flags & SHADER_PASS_ALPHAFUNC ) {
		qglEnable( GL_ALPHA_TEST );
		if( pass->alphafunc == ALPHA_FUNC_GT0 )
			qglAlphaFunc( GL_GREATER, 0 );
		else if( pass->alphafunc == ALPHA_FUNC_LT128 )
			qglAlphaFunc( GL_LESS, 0.5f );
		else if( pass->alphafunc == ALPHA_FUNC_GE128 )
			qglAlphaFunc( GL_GEQUAL, 0.5f );
	} else {
		qglDisable( GL_ALPHA_TEST );
	}

	// nasty hack!!!
	if( !glState.in2DMode ) {
		qglDepthFunc( pass->depthfunc );
		if( pass->flags & SHADER_PASS_DEPTHWRITE )
			qglDepthMask( GL_TRUE );
		else
			qglDepthMask( GL_FALSE );
	}
}

/*
================
R_RenderMeshGeneric
================
*/
void R_RenderMeshGeneric( void )
{
	shaderpass_t *pass = r_accumPasses[0];

	R_SetShaderpassState( pass, qfalse );
	R_ModifyTextureCoords( pass, 0 );
	R_ModifyColor( pass );

	if ( pass->blendmode == GL_REPLACE )
		GL_TexEnv( GL_REPLACE );
	else
		GL_TexEnv( GL_MODULATE );
	GL_Bind( 0, r_texPointers[0] );

	R_FlushArrays ();
}

/*
================
R_RenderMeshMultitextured
================
*/
void R_RenderMeshMultitextured( void )
{
	int	i;
	shaderpass_t *pass = r_accumPasses[0];

	GL_SelectTexture( 0 );
	GL_TexEnv( GL_MODULATE );

	R_SetShaderpassState( pass, qtrue );
	R_ModifyColor( pass );
	R_ModifyTextureCoords( pass, 0 );

	for( i = 1; i < r_numAccumPasses; i++ ) {
		pass = r_accumPasses[i];

		GL_SelectTexture( i );
		GL_TexEnv( pass->blendmode );
		R_ModifyTextureCoords( pass, i );
	}

	R_FlushArraysMtex ();
}

/*
================
R_RenderMeshCombined
================
*/
void R_RenderMeshCombined( void )
{
	int	i;
	shaderpass_t *pass = r_accumPasses[0];

	GL_SelectTexture( 0 );
	GL_TexEnv( GL_MODULATE );

	R_SetShaderpassState( pass, qtrue );
	R_ModifyColor( pass );
	R_ModifyTextureCoords( pass, 0 );

	for( i = 1; i < r_numAccumPasses; i++ ) {
		pass = r_accumPasses[i];

		GL_SelectTexture( i );

		switch ( pass->blendmode ) {
		case GL_REPLACE:
		case GL_MODULATE:
			GL_TexEnv( GL_MODULATE );
			break;
		case GL_ADD:
			// these modes are best set with TexEnv, Combine4 would need much more setup
			GL_TexEnv( pass->blendmode );
			break;
		case GL_DECAL:
			// mimics Alpha-Blending in upper texture stage, but instead of multiplying the alpha-channel, they´re added
			// this way it can be possible to use GL_DECAL in both texture-units, while still looking good
			// normal mutlitexturing would multiply the alpha-channel which looks ugly
			GL_TexEnv( GL_COMBINE_ARB );
			qglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB );
			qglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_ADD );

			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR );
			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA );

			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR );
			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );

			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA );
			break;
		default:
			GL_TexEnv( GL_COMBINE4_NV );

			qglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_ADD );
			qglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_ADD );

			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR );
			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA );

			switch( pass->blendsrc ) {
			case GL_ONE:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GL_ZERO:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );
				break;
			case GL_DST_COLOR:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );
				break;
			case GL_ONE_MINUS_DST_COLOR:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GL_SRC_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );
				break;
			case GL_ONE_MINUS_SRC_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GL_DST_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );
				break;
			case GL_ONE_MINUS_DST_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_ONE_MINUS_SRC_ALPHA );
				break;
			}

			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_PREVIOUS_ARB );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_COLOR );
			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, GL_PREVIOUS_ARB );	
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, GL_SRC_ALPHA );

			switch( pass->blenddst ) {
			case GL_ONE:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GL_ZERO:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA );
				break;
			case GL_SRC_COLOR:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA );
				break;
			case GL_ONE_MINUS_SRC_COLOR:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GL_SRC_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA );
				break;
			case GL_ONE_MINUS_SRC_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GL_DST_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA );
				break;
			case GL_ONE_MINUS_DST_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_ALPHA);
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA );
				break;
			}
		}

		R_ModifyTextureCoords ( pass, i );
	}

	R_FlushArraysMtex ();
}

/*
================
R_RenderAccumulatedPasses
================
*/
void R_RenderAccumulatedPasses( void )
{
	if( r_numAccumPasses == 1 )
		R_RenderMeshGeneric ();
	else if( glConfig.textureEnvCombine )
		R_RenderMeshCombined ();
	else
		R_RenderMeshMultitextured ();
	r_numAccumPasses = 0;
}

/*
================
R_AccumulatePass
================
*/
void R_AccumulatePass( shaderpass_t *pass )
{
	qboolean accumulate;
	shaderpass_t *prevPass;

	if( !r_numAccumPasses ) {
		r_accumPasses[r_numAccumPasses++] = pass;
		return;
	}

	// see if there are any free texture units
	accumulate = ( r_numAccumPasses < glConfig.maxTextureUnits );

	if( accumulate ) {
		// ok, we've got several passes, diff against the previous
		prevPass = r_accumPasses[r_numAccumPasses-1];

		// see if depthfuncs and colors are good
		if ( 
			(prevPass->flags & SHADER_PASS_VIDEOMAP) ||
			(pass->flags & SHADER_PASS_VIDEOMAP) ||
			(prevPass->depthfunc != pass->depthfunc) ||
			(pass->flags & SHADER_PASS_ALPHAFUNC) ||
			(pass->rgbgen.type != RGB_GEN_IDENTITY) ||
			(pass->alphagen.type != ALPHA_GEN_IDENTITY) ||
			((prevPass->flags & SHADER_PASS_ALPHAFUNC) && (pass->depthfunc != GL_EQUAL))
			)
			accumulate = qfalse;

		// see if blendmodes are good
		if( accumulate ) {
			if( !pass->blendmode ) {
				accumulate = ( prevPass->blendmode == GL_REPLACE ) && glConfig.NVTextureEnvCombine4;
			} else if( glConfig.textureEnvCombine ) {
				if( prevPass->blendmode == GL_REPLACE )
					accumulate = ( pass->blendmode == GL_ADD ) ? glConfig.textureEnvAdd : qtrue;
				else if( prevPass->blendmode == GL_ADD )
					accumulate = ( pass->blendmode == GL_ADD ) && glConfig.textureEnvAdd;
				else if( prevPass->blendmode == GL_MODULATE )
					accumulate = ( pass->blendmode == GL_MODULATE || pass->blendmode == GL_REPLACE );
				else
					accumulate = qfalse;
			} else/* if( glConfig.multiTexture )*/ {
				if ( prevPass->blendmode == GL_REPLACE )
					accumulate = ( pass->blendmode == GL_ADD ) ? glConfig.textureEnvAdd : ( pass->blendmode != GL_DECAL );
				else if( prevPass->blendmode == GL_ADD )
					accumulate = ( pass->blendmode == GL_ADD ) && glConfig.textureEnvAdd;
				else if( prevPass->blendmode == GL_MODULATE )
					accumulate = ( pass->blendmode == GL_MODULATE || pass->blendmode == GL_REPLACE );
				else
					accumulate = qfalse;
			}
		}
	}

	// no, failed to accumulate
	if( !accumulate )
		R_RenderAccumulatedPasses ();
	r_accumPasses[r_numAccumPasses++] = pass;
}

/*
================
R_RenderMeshBuffer
================
*/
void R_RenderMeshBuffer( meshbuffer_t *mb, qboolean shadowpass )
{
	int	i;
	msurface_t *surf;
	shaderpass_t *pass;
	unsigned int dlightBits;

	if( !numVerts )
		return;

	surf = mb->infokey > 0 ? &currentmodel->surfaces[mb->infokey-1] : NULL;
	if( surf ) {
		r_patchWidth = surf->patchWidth;
		r_patchHeight = surf->patchHeight;
		r_lightmapNum = surf->lightmapnum;
	} else {
		r_patchWidth = 0;
		r_patchHeight = 0;
		r_lightmapNum = -1;
	}

	r_currentShader = mb->shader;

	if( glState.in2DMode ) {
		r_currentShaderTime = curtime * 0.001f;
	} else {
		r_currentShaderTime = r_refdef.time;

		if( currententity ) {
			r_currentShaderTime -= currententity->shaderTime;
			if ( r_currentShaderTime < 0 )
				r_currentShaderTime = 0;
		}
	}

	if( !r_triangleOutlines )
		R_SetCurrentShaderState ();

	if( r_currentShader->numdeforms )
		R_DeformVertices ();

	if( glConfig.vertexBufferObject ) {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_VERTS] );
		qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec3_t ), vertsArray, GL_STREAM_DRAW_ARB );
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	} else {
		qglVertexPointer( 3, GL_FLOAT, 0, vertsArray );
	}

	if( !numIndexes || shadowpass )
		return;

	if( glConfig.vertexBufferObject ) {
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_INDEXES] );
		qglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, numIndexes * sizeof( int ), indexesArray, GL_STREAM_DRAW_ARB );
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	}

	if( r_triangleOutlines ) {
		R_LockArrays( numVerts );

		if ( r_showtris->integer )
			R_DrawTriangles ();
		if ( r_shownormals->integer )
			R_DrawNormals ();

		R_UnlockArrays ();
		R_ClearArrays ();
		R_ResetBackendPointers ();
		return;
	}

	// can we fog the geometry with alpha texture?
	r_texFog = (mb->fog && ((r_currentShader->sort <= (SHADER_SORT_OPAQUE+1) && 
		(r_currentShader->flags & (SHADER_DEPTHWRITE|SHADER_SKY))) || r_currentShader->fog_dist)) ? mb->fog : NULL;

	// check if the fog volume is present but we can't use alpha texture
	r_colorFog = (mb->fog && !r_texFog) ? mb->fog : NULL;

	R_LockArrays( numVerts );
	pass = r_currentShader->passes;

	// accumulate passes for dynamic merging
	for ( i = 0; i < r_currentShader->numpasses; i++, pass++ ) {
		if( (pass->flags & SHADER_PASS_DETAIL) && !r_detailtextures->integer )
			continue;
		if( (pass->flags & SHADER_PASS_LIGHTMAP) && (r_lightmapNum < 0) )
			continue;
		R_AccumulatePass( pass );
	}

	// flush any remaining passes
	if( r_numAccumPasses )
		R_RenderAccumulatedPasses ();

	// render dynamic lights, fog, triangle outlines, normals and clear arrays
	dlightBits = mb->dlightbits;
	if( r_currentShader->flags & SHADER_FLARE )
		dlightBits = 0;

	qglDisable( GL_TEXTURE_GEN_S );
	qglDisable( GL_TEXTURE_GEN_T );
	qglDisable( GL_TEXTURE_GEN_R );
	if( glConfig.textureCubeMap )
		qglDisable( GL_TEXTURE_CUBE_MAP_ARB );
	qglMatrixMode( GL_TEXTURE );

	if( dlightBits || r_texFog ) {
		qglEnable( GL_BLEND );
		qglDisable( GL_ALPHA_TEST );
		qglDepthMask( GL_FALSE );

		if( dlightBits )
			R_AddDynamicLights( dlightBits );
		if( r_texFog )
			R_RenderFogOnMesh ();
	}

	qglLoadIdentity ();
	qglMatrixMode( GL_MODELVIEW );

	R_UnlockArrays ();
	R_ClearArrays ();
	R_ResetBackendPointers ();
}

/*
================
R_RenderFogOnMesh
================
*/
void R_RenderFogOnMesh( void )
{
	int		i;
	vec3_t	viewtofog;
	float	*coords;
	double	dist, vdist;
	shader_t *fogshader;
	double	fognormal[3], vpnnormal[3];
	double	fogdist, vpndist;
	int		fogptype;
	cplane_t *fogplane;
	mat4x4_t m = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1.5/(float)FOG_TEXTURE_HEIGHT, 0, 1 };

	if ( !r_texFog->numplanes || !r_texFog->shader || !r_texFog->visibleplane )
		return;

	numColors = 0;
	coords = inCoordsArray[0];

	fogshader = r_texFog->shader;
	fogplane = r_texFog->visibleplane;

	GL_Bind( 0, r_fogtexture );
	if( !glConfig.vertexBufferObject )
		qglTexCoordPointer( 2, GL_FLOAT, 0, inCoordsArray[0] );
	else
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_TC0] );

	m[0] = m[5] = fogshader->fog_dist;
	qglLoadMatrixf( m );

	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglColor4ubv( fogshader->fog_color );

	if( !r_currentShader->numpasses || r_currentShader->fog_dist || (r_currentShader->flags & SHADER_SKY) )
		qglDepthFunc( GL_LEQUAL );
	else
		qglDepthFunc( GL_EQUAL );

	// distance to fog
	dist = PlaneDiff ( r_origin, fogplane );

	if( r_currentShader->flags & SHADER_SKY ) {
		if ( dist > 0 )
			VectorMA( r_origin, -dist, fogplane->normal, viewtofog );
		else
			VectorCopy( r_origin, viewtofog );
	} else {
		VectorCopy( currententity->origin, viewtofog );
	}

	// some math tricks to take entity's rotation matrix into consideration
	// for fog texture coordinates calculations:
	// M is a rotation matrix, v is a vertex, t is a transform vector
	// n is a plane's normal, d is a plane's dist, r is a view origin
	// (M*v + t)*n - d = (M*n)*v - ((d - t*n))
	// (M*v + t - r)*n = (M*n)*v - ((r - t)*n)

	fognormal[0] = DotProduct( currententity->axis[0], fogplane->normal ) * currententity->scale;
	fognormal[1] = DotProduct( currententity->axis[1], fogplane->normal ) * currententity->scale;
	fognormal[2] = DotProduct( currententity->axis[2], fogplane->normal ) * currententity->scale;
	fogptype = ( fognormal[0] == 1.0 ? PLANE_X : (fognormal[1] == 1.0 ? PLANE_Y : (fognormal[2] == 1.0 ? PLANE_Z : PLANE_NONAXIAL) ) );
	fogdist = (fogplane->dist - DotProduct( viewtofog, fogplane->normal ));

	vpnnormal[0] = DotProduct( currententity->axis[0], vpn ) * currententity->scale;
	vpnnormal[1] = DotProduct( currententity->axis[1], vpn ) * currententity->scale;
	vpnnormal[2] = DotProduct( currententity->axis[2], vpn ) * currententity->scale;
	vpndist = ((r_origin[0] - viewtofog[0]) * vpn[0] + (r_origin[1] - viewtofog[1]) * vpn[1] + (r_origin[2] - viewtofog[2]) * vpn[2]);

	if( dist < 0 )	{ 	// camera is inside the fog brush
		if( fogptype < 3 ) {
			for( i = 0; i < numVerts; i++, coords += 2 ) {
				coords[0] = DotProduct( vertsArray[i], vpnnormal ) - vpndist;
				coords[1] = -( vertsArray[i][fogptype] - fogdist );
			}
		} else {
			for( i = 0; i < numVerts; i++, coords += 2 ) {
				coords[0] = DotProduct( vertsArray[i], vpnnormal ) - vpndist;
				coords[1] = -( DotProduct( vertsArray[i], fognormal ) - fogdist );
			}
		}
	} else {
		if( fogptype < 3 ) {
			for( i = 0; i < numVerts; i++, coords += 2 ) {
				vdist = vertsArray[i][fogptype] - fogdist;

				if( vdist < 0 )
					coords[0] = ( DotProduct( vertsArray[i], vpnnormal ) - vpndist ) * vdist / ( vdist - dist );
				else
					coords[0] = 0.0f;
				coords[1] = -vdist;
			}
		} else {
			for( i = 0; i < numVerts; i++, coords += 2 ) {
				vdist = DotProduct( vertsArray[i], fognormal ) - fogdist;

				if( vdist < 0 )
					coords[0] = ( DotProduct( vertsArray[i], vpnnormal ) - vpndist ) * vdist / ( vdist - dist );
				else
					coords[0] = 0.0f;
				coords[1] = -vdist;
			}
		}
	}

	if( glConfig.vertexBufferObject ) {
		qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec2_t ), inCoordsArray[0], GL_STREAM_DRAW_ARB );
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	}

	if ( !r_currentShader->numpasses )
		R_LockArrays( numVerts );

	R_FlushArrays ();
}

/*
================
R_BackendBeginTriangleOutlines
================
*/
void R_BackendBeginTriangleOutlines( void )
{
	r_triangleOutlines = qtrue;
	qglDisable( GL_TEXTURE_2D );
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_BLEND );
	qglDisable( GL_CULL_FACE );
	qglColor4f( 1, 1, 1, 1 );
}

/*
================
R_BackendEndTriangleOutlines
================
*/
void R_BackendEndTriangleOutlines( void )
{
	r_triangleOutlines = qfalse;
	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_TEXTURE_2D );
}

/*
================
R_DrawTriangles
================
*/
void R_DrawTriangles( void )
{
	int i;

	for( i = 0; i < numIndexes; i += 3 ) {
		qglBegin( GL_LINE_STRIP );
		qglArrayElement( indexesArray[i] );
		qglArrayElement( indexesArray[i+1] );
		qglArrayElement( indexesArray[i+2] );
		qglArrayElement( indexesArray[i] );
		qglEnd ();
	}
}

/*
================
R_DrawNormals
================
*/
void R_DrawNormals( void )
{
	int i;

	qglBegin( GL_LINES );
	for( i = 0; i < numVerts; i++ ) {
		qglVertex3fv( vertsArray[i] );
		qglVertex3f( vertsArray[i][0] + normalsArray[i][0], 
			vertsArray[i][1] + normalsArray[i][1], 
			vertsArray[i][2] + normalsArray[i][2] );
	}
	qglEnd ();
}
