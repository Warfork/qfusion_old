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

#define FTABLE_SIZE_POW	10
#define FTABLE_SIZE		(1<<FTABLE_SIZE_POW)
#define FTABLE_CLAMP(x)	(((int)((x)*FTABLE_SIZE) & (FTABLE_SIZE-1)))
#define FTABLE_EVALUATE(table,x) ((table)[FTABLE_CLAMP(x)])

static	float	r_sintable[FTABLE_SIZE];
static	float	r_sintableByte[256];
static	float	r_triangletable[FTABLE_SIZE];
static	float	r_squaretable[FTABLE_SIZE];
static	float	r_sawtoothtable[FTABLE_SIZE];
static	float	r_inversesawtoothtable[FTABLE_SIZE];

#define NOISE_SIZE		256
#define NOISE_VAL(a)	r_noiseperm[(a) & (NOISE_SIZE - 1)]
#define NOISE_INDEX(x, y, z, t) NOISE_VAL(x + NOISE_VAL(y + NOISE_VAL(z + NOISE_VAL(t))))
#define NOISE_LERP(a, b, w) (a * (1.0f - w) + b * w)

static	float	r_noisetable[NOISE_SIZE];
static	int		r_noiseperm[NOISE_SIZE];

#if SHADOW_VOLUMES
vec3_t			inVertsArray[MAX_ARRAY_VERTS*2];
#else
vec3_t			inVertsArray[MAX_ARRAY_VERTS];
#endif

vec3_t			inNormalsArray[MAX_ARRAY_VERTS];
vec4_t			inSVectorsArray[MAX_ARRAY_VERTS];
index_t			inIndexesArray[MAX_ARRAY_INDEXES];
vec2_t			inCoordsArray[MAX_ARRAY_VERTS];
vec2_t			inLightmapCoordsArray[MAX_LIGHTMAPS][MAX_ARRAY_VERTS];
byte_vec4_t		inColorsArray[MAX_LIGHTMAPS][MAX_ARRAY_VERTS];

vec2_t			tUnitCoordsArray[MAX_TEXTURE_UNITS][MAX_ARRAY_VERTS];

index_t			*indexesArray;
vec3_t			*vertsArray;
vec3_t			*normalsArray;
vec4_t			*sVectorsArray;
vec2_t			*coordsArray;
vec2_t			*lightmapCoordsArray[MAX_LIGHTMAPS];
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

static	int		r_lightmapStyleNum[MAX_TEXTURE_UNITS];
static	superLightStyle_t *r_superLightStyle;

static	const	meshbuffer_t *r_currentMeshBuffer;
static	const	shader_t *r_currentShader;
static	float	r_currentShaderTime;
static	int		r_currentShaderState;
static	const	mfog_t *r_texFog, *r_colorFog;
static	qboolean r_breakEarly;

static	shaderpass_t r_dlightsPass, r_fogPass;
static	shaderpass_t r_lightmapPasses[MAX_TEXTURE_UNITS+1];

static	shaderpass_t r_GLSLpasses[2];		// dlights and base

static	const shaderpass_t *r_accumPasses[MAX_TEXTURE_UNITS];
static	int		r_numAccumPasses;

static	int		r_identityLighting;

unsigned int	r_numverts;
unsigned int	r_numtris;
unsigned int	r_numflushes;
unsigned int	r_numkeptlocks;

static void R_DrawTriangles( void );
static void R_DrawNormals( void );
static void R_CleanUpTextureUnits( int last );
static void R_AccumulatePass( const shaderpass_t *pass );

/*
==============
R_BackendInit
==============
*/
void R_BackendInit( void )
{
	int i;
	float t;

	numVerts = 0;
	numIndexes = 0;
    numColors = 0;

	indexesArray = inIndexesArray;
	vertsArray = inVertsArray;
	normalsArray = inNormalsArray;
	sVectorsArray = inSVectorsArray;
	coordsArray = inCoordsArray;
	for( i = 0; i < MAX_LIGHTMAPS; i++ )
		lightmapCoordsArray[i] = inLightmapCoordsArray[i];

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

#ifdef VERTEX_BUFFER_OBJECTS
	if( glConfig.vertexBufferObject ) {
		r_numVertexBufferObjects = VBO_ENDMARKER + glConfig.maxTextureUnits - 1;
		if( r_numVertexBufferObjects > MAX_VERTEX_BUFFER_OBJECTS )
			r_numVertexBufferObjects = MAX_VERTEX_BUFFER_OBJECTS;

		qglGenBuffersARB( r_numVertexBufferObjects, r_vertexBufferObjects );

		for( i = 0; i < r_numVertexBufferObjects; i++ ) {
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
#endif

	qglEnableClientState( GL_VERTEX_ARRAY );

	if( !r_ignorehwgamma->integer )
		r_identityLighting = (int)(255.0f / pow(2, max(0, floor(r_overbrightbits->value))));
	else
		r_identityLighting = 255;

	// build lookup tables
	for( i = 0; i < FTABLE_SIZE; i++ ) {
		t = (float)i / (float)FTABLE_SIZE;

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

	for( i = 0; i < 256; i++ )
		r_sintableByte[i] = sin( (float)i / 255.0 * M_TWOPI );

	// init the noise table
	srand( 1001 );

	for( i = 0; i < NOISE_SIZE; i++ ) {
		r_noisetable[i] = (float)(((rand() / (float)RAND_MAX) * 2.0 - 1.0));
		r_noiseperm[i] = (unsigned char)(rand() / (float)RAND_MAX * 255);
	}

	// init dynamic lights pass
	memset( &r_dlightsPass, 0, sizeof( shaderpass_t ) );
	r_dlightsPass.flags = SHADERPASS_DLIGHT|GLSTATE_DEPTHFUNC_EQ|GLSTATE_SRCBLEND_DST_COLOR|GLSTATE_DSTBLEND_ONE;

	// init fog pass
	memset( &r_fogPass, 0, sizeof( shaderpass_t ) );
	r_fogPass.tcgen = TC_GEN_FOG;
	r_fogPass.rgbgen.type = RGB_GEN_FOG;
	r_fogPass.alphagen.type = ALPHA_GEN_IDENTITY;
	r_fogPass.flags = SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_DECAL|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;

	// the very first lightmap pass is reserved for GL_REPLACE or GL_MODULATE
	memset( r_lightmapPasses, 0, sizeof( r_lightmapPasses ) );

	// the rest are GL_ADD
	for( i = 1; i < MAX_TEXTURE_UNITS+1; i++ ) {
		r_lightmapPasses[i].flags = SHADERPASS_LIGHTMAP|SHADERPASS_NOCOLORARRAY|GLSTATE_DEPTHFUNC_EQ
			|SHADERPASS_BLEND_ADD|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
		r_lightmapPasses[i].tcgen = TC_GEN_LIGHTMAP;
		r_lightmapPasses[i].alphagen.type = ALPHA_GEN_IDENTITY;
	}
	
	// init optional GLSL program passes
	memset( r_GLSLpasses, 0, sizeof( r_GLSLpasses ) );
	r_GLSLpasses[0].flags = SHADERPASS_DLIGHT|GLSTATE_DEPTHFUNC_EQ|SHADERPASS_BLEND_ADD|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
	r_GLSLpasses[1].flags = SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_MODULATE|GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
	r_GLSLpasses[1].tcgen = TC_GEN_BASE;
	r_GLSLpasses[1].rgbgen.type = RGB_GEN_IDENTITY;
	r_GLSLpasses[1].alphagen.type = ALPHA_GEN_IDENTITY;
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
	int i;

	numColors = 0;

	vertsArray = inVertsArray;
	normalsArray = inNormalsArray;
	sVectorsArray = inSVectorsArray;
	coordsArray = inCoordsArray;
	for( i = 0; i < MAX_LIGHTMAPS; i++ )
		lightmapCoordsArray[i] = inLightmapCoordsArray[i];
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
void R_LatLongToNorm( const qbyte latlong[2], vec3_t out )
{
	float sin_a, sin_b, cos_a, cos_b;

	cos_a = r_sintableByte[(latlong[0] + 64) & 255];
	sin_a = r_sintableByte[latlong[0]];
	cos_b = r_sintableByte[(latlong[1] + 64) & 255];
	sin_b = r_sintableByte[latlong[1]];

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
			return r_sintable;		// default to sintable
	}

	// assume error
	Com_Error( ERR_DROP, "R_TableForFunc: unknown function" );

	return NULL;
}

/*
==============
R_BackendGetNoiseValue
==============
*/
float R_BackendGetNoiseValue( float x, float y, float z, float t )
{
	int i;
	int ix, iy, iz, it;
	float fx, fy, fz, ft;
	float front[4], back[4];
	float fvalue, bvalue, value[2], finalvalue;

	ix = ( int )floor( x );
	fx = x - ix;
	iy = ( int )floor( y );
	fy = y - iy;
	iz = ( int )floor( z );
	fz = z - iz;
	it = ( int )floor( t );
	ft = t - it;

	for( i = 0; i < 2; i++ ) {
		front[0] = r_noisetable[NOISE_INDEX( ix, iy, iz, it + i )];
		front[1] = r_noisetable[NOISE_INDEX( ix+1, iy, iz, it + i )];
		front[2] = r_noisetable[NOISE_INDEX( ix, iy+1, iz, it + i )];
		front[3] = r_noisetable[NOISE_INDEX( ix+1, iy+1, iz, it + i )];

		back[0] = r_noisetable[NOISE_INDEX( ix, iy, iz + 1, it + i )];
		back[1] = r_noisetable[NOISE_INDEX( ix+1, iy, iz + 1, it + i )];
		back[2] = r_noisetable[NOISE_INDEX( ix, iy+1, iz + 1, it + i )];
		back[3] = r_noisetable[NOISE_INDEX( ix+1, iy+1, iz + 1, it + i )];

		fvalue = NOISE_LERP( NOISE_LERP( front[0], front[1], fx ), NOISE_LERP( front[2], front[3], fx ), fy );
		bvalue = NOISE_LERP( NOISE_LERP( back[0], back[1], fx ), NOISE_LERP( back[2], back[3], fx ), fy );
		value[i] = NOISE_LERP( fvalue, bvalue, fz );
	}

	finalvalue = NOISE_LERP( value[0], value[1], ft );

	return finalvalue;
}

/*
==============
R_BackendStartFrame
==============
*/
void R_BackendStartFrame( void )
{
	r_numverts = 0;
	r_numtris = 0;
	r_numflushes = 0;
	r_numkeptlocks = 0;
}

/*
==============
R_BackendEndFrame
==============
*/
void R_BackendEndFrame( void )
{
	// unlock arrays if any
	R_UnlockArrays ();

	// clean up texture units
	R_CleanUpTextureUnits( 1 );

	if( r_speeds->integer && !(ri.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		Com_Printf( "%4i wpoly %4i leafs %4i verts %4i tris %4i flushes %3i locks\n",
			c_brush_polys, 
			c_world_leafs,
			r_numverts,
			r_numtris,
			r_numflushes,
			r_numkeptlocks );
		Com_Printf( "lvs: %5i  node: %5i  farclip: %6.f\n",
			r_mark_leaves,
			r_world_node,
			ri.farClip );
		Com_Printf( "polys\\ents: %5i  sort\\draw: %5i\\%i\n",
			r_add_polys, r_add_entities,
			r_sort_meshes, r_draw_meshes );
	}
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
void R_UnlockArrays( void )
{
	if( !r_arraysLocked )
		return;

	qglUnlockArraysEXT ();
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

	if( r_enableNormals ) {
		qglEnableClientState( GL_NORMAL_ARRAY );
#ifdef VERTEX_BUFFER_OBJECTS
		if( glConfig.vertexBufferObject ) {
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_NORMALS] );
			qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec3_t ), normalsArray, GL_STREAM_DRAW_ARB );
			qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		}
		else
#endif
		{
			qglNormalPointer( GL_FLOAT, 12, normalsArray );
		}
	}

	if( numColors ) {
		if( numColors == 1 ) {
			qglColor4ubv( colorArray[0] );
		} else {
			qglEnableClientState( GL_COLOR_ARRAY );
#ifdef VERTEX_BUFFER_OBJECTS
			if( glConfig.vertexBufferObject ) {
				qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_COLORS] );
				qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( byte_vec4_t ), colorArray, GL_STREAM_DRAW_ARB );
				qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
			}
			else
#endif
			{
				qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
			}
		}
	}

	if( glConfig.drawRangeElements )
		qglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVerts, numIndexes, GL_UNSIGNED_INT, indexesArray );
	else
		qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,	indexesArray );

	if( numColors > 1 )
		qglDisableClientState( GL_COLOR_ARRAY );
	if( r_enableNormals ) {
		qglDisableClientState( GL_NORMAL_ARRAY );
		r_enableNormals = qfalse;
	}

	r_numtris += numIndexes / 3;
	r_numflushes++;
}

/*
==============
R_CleanUpTextureUnits
==============
*/
static void R_CleanUpTextureUnits( int last )
{
	int i;

	for( i = glState.currentTMU; i > last - 1; i-- ) {
		GL_EnableTexGen( GL_S, 0 );
		GL_EnableTexGen( GL_T, 0 );
		GL_EnableTexGen( GL_R, 0 );
		GL_SetTexCoordArrayMode( 0 );

		qglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( i - 1 );
	}
}

/*
================
R_DeformVertices
================
*/
void R_DeformVertices( void )
{
	int i, j, k;
	float args[4], deflect;
	float *quad[4];
	const float *table;
	const deformv_t *deformv;
	vec3_t tv, rot_centre;

	deformv = &r_currentShader->deforms[0];
	for( i = 0; i < r_currentShader->numdeforms; i++, deformv++ ) {
		switch( deformv->type ) {
			case DEFORMV_NONE:
				break;
			case DEFORMV_WAVE:
				table = R_TableForFunc( deformv->func.type );

				// Deflect vertex along its normal by wave amount
				if( deformv->func.args[3] == 0 ) {
					deflect = deformv->func.args[2] + deformv->func.args[3] * r_currentShaderTime;
					deflect = FTABLE_EVALUATE( table, deflect ) * deformv->func.args[1] + deformv->func.args[0];

					for( j = 0; j < numVerts; j++ )
						VectorMA( inVertsArray[j], deflect, inNormalsArray[j], inVertsArray[j] );
				} else {
					args[0] = deformv->func.args[0];
					args[1] = deformv->func.args[1];
					args[2] = deformv->func.args[2] + deformv->func.args[3] * r_currentShaderTime;
					args[3] = deformv->args[0];

					for( j = 0; j < numVerts; j++ ) {
						deflect = args[2] + args[3] * (inVertsArray[j][0] + inVertsArray[j][1] + inVertsArray[j][2]);
						deflect = FTABLE_EVALUATE( table, deflect ) * args[1] + args[0];
						VectorMA( inVertsArray[j], deflect, inNormalsArray[j], inVertsArray[j] );
					}
				}
				break;
			case DEFORMV_NORMAL:
				// without this * 0.1f deformation looks wrong, although q3a doesn't have it
				args[0] = deformv->func.args[3] * r_currentShaderTime * 0.1f;
				args[1] = deformv->func.args[1];

				for( j = 0; j < numVerts; j++ ) {
					VectorScale( inVertsArray[j], 0.98f, tv );
					inNormalsArray[j][0] += args[1] * R_BackendGetNoiseValue( tv[0]      , tv[1], tv[2], args[0] );
					inNormalsArray[j][1] += args[1] * R_BackendGetNoiseValue( tv[0] + 100, tv[1], tv[2], args[0] );
					inNormalsArray[j][2] += args[1] * R_BackendGetNoiseValue( tv[0] + 200, tv[1], tv[2], args[0] );
					VectorNormalizeFast( inNormalsArray[j] );
				}
				break;
			case DEFORMV_MOVE:
				table = R_TableForFunc( deformv->func.type );
				deflect = deformv->func.args[2] + r_currentShaderTime * deformv->func.args[3];
				deflect = FTABLE_EVALUATE( table, deflect ) * deformv->func.args[1] + deformv->func.args[0];

				for( j = 0; j < numVerts; j++ )
					VectorMA( inVertsArray[j], deflect, deformv->args, inVertsArray[j] );
				break;
			case DEFORMV_BULGE:
				args[0] = deformv->args[0];
				args[1] = deformv->args[1];
				args[2] = r_currentShaderTime * deformv->args[2];

				for( j = 0; j < numVerts; j++ ) {
					deflect = (inCoordsArray[j][0] * args[0] + args[2]) / M_TWOPI;
					deflect = R_FastSin( deflect ) * args[1];
					VectorMA( inVertsArray[j], deflect, inNormalsArray[j], inVertsArray[j] );
				}
				break;
			case DEFORMV_AUTOSPRITE:
				{
					vec3_t m0[3], m1[3], m2[3], result[3];

					if( numIndexes % 6 )
						break;

					if( ri.currententity && (ri.currentmodel != r_worldmodel) )
						Matrix4_Matrix ( ri.modelviewMatrix, m1 );
					else
						Matrix4_Matrix ( ri.worldviewMatrix, m1 );

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

					if( ri.currententity && (ri.currentmodel != r_worldmodel) ) {
						VectorAdd( ri.currententity->origin, rot_centre, tv );
						VectorSubtract( ri.viewOrigin, tv, tmp );
						Matrix_TransformVector( ri.currententity->axis, tmp, tv );
					} else {
						VectorCopy( rot_centre, tv );
						VectorSubtract( ri.viewOrigin, tv, tv );
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

					if ( ri.currententity && (ri.currentmodel != r_worldmodel) )
						Matrix4_Matrix( ri.modelviewMatrix, m1 );
					else
						Matrix4_Matrix( ri.worldviewMatrix, m1 );

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
						scale = (quad[0][0] - ri.viewOrigin[0]) * ri.vpn[0] + (quad[0][1] - ri.viewOrigin[1]) * ri.vpn[1] + (quad[0][2] - ri.viewOrigin[2]) * ri.vpn[2];
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
static qboolean R_VertexTCBase( const shaderpass_t *pass, int unit, mat4x4_t matrix )
{
	int	i;
	float *outCoords;
	qboolean identityMatrix = qfalse;

	Matrix4_Identity( matrix );

	switch( pass->tcgen ) {
		case TC_GEN_BASE:
			GL_EnableTexGen( GL_S, 0 );
			GL_EnableTexGen( GL_T, 0 );
			GL_EnableTexGen( GL_R, 0 );

#ifdef VERTEX_BUFFER_OBJECTS
			if( glConfig.vertexBufferObject ) {
				identityMatrix = qtrue;
				outCoords = coordsArray[0];
				break;
			}
			else
#endif
			{
				qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArray );
				return qtrue;
			}
		case TC_GEN_LIGHTMAP:
			GL_EnableTexGen( GL_S, 0 );
			GL_EnableTexGen( GL_T, 0 );
			GL_EnableTexGen( GL_R, 0 );

#ifdef VERTEX_BUFFER_OBJECTS
			if( glConfig.vertexBufferObject ) {
				identityMatrix = qtrue;
				outCoords = lightmapCoordsArray[r_lightmapStyleNum[unit]][0];
				break;
			}
			else
#endif
			{
				qglTexCoordPointer( 2, GL_FLOAT, 0, lightmapCoordsArray[r_lightmapStyleNum[unit]] );
				return qtrue;
			}
		case TC_GEN_ENVIRONMENT:
		{
			float depth, *n;
			vec3_t projection, transform;

			if( glState.in2DMode )
				return qtrue;

			VectorSubtract( ri.viewOrigin, ri.currententity->origin, projection );
			Matrix_TransformVector( ri.currententity->axis, projection, transform );

			outCoords = tUnitCoordsArray[unit][0];
			for( i = 0, n = normalsArray[0]; i < numVerts; i++, outCoords += 2, n += 3 ) {
				VectorSubtract( transform, vertsArray[i], projection );
				VectorNormalizeFast( projection );

				depth = DotProduct( n, projection ); depth += depth;
				outCoords[0] = 0.5 + (n[1] * depth - projection[1]) * 0.5;
				outCoords[1] = 0.5 - (n[2] * depth - projection[2]) * 0.5;
			}

			GL_EnableTexGen( GL_S, 0 );
			GL_EnableTexGen( GL_T, 0 );
			GL_EnableTexGen( GL_R, 0 );

#ifdef VERTEX_BUFFER_OBJECTS
			if( glConfig.vertexBufferObject ) {
				identityMatrix = qfalse;
				outCoords = tUnitCoordsArray[unit][0];
				break;
			}
			else
#endif
			{
				qglTexCoordPointer( 2, GL_FLOAT, 0, tUnitCoordsArray[unit] );
				return qtrue;
			}
		}
		case TC_GEN_VECTOR:
		{
			GLfloat genVector[2][4];

			for( i = 0; i < 3; i++ ) {
				genVector[0][i] = pass->tcgenVec[i];
				genVector[1][i] = pass->tcgenVec[i+4];
			}
			genVector[0][3] = genVector[1][3] = 0;

			matrix[12] = pass->tcgenVec[3];
			matrix[13] = pass->tcgenVec[7];

			GL_SetTexCoordArrayMode( 0 );
			GL_EnableTexGen( GL_S, GL_OBJECT_LINEAR );
			GL_EnableTexGen( GL_T, GL_OBJECT_LINEAR );
			GL_EnableTexGen( GL_R, 0 );
			qglTexGenfv( GL_S, GL_OBJECT_PLANE, genVector[0] );
			qglTexGenfv( GL_T, GL_OBJECT_PLANE, genVector[1] );
			return qfalse;
		}
		case TC_GEN_REFLECTION_CELLSHADE:
			if( ri.currententity ) {
				vec3_t dir, temp;

				R_LightForOrigin( ri.currententity->lightingOrigin, temp, NULL, NULL, ri.currentmodel->radius * ri.currententity->scale );

				// rotate direction
				Matrix_TransformVector( ri.currententity->axis, temp, dir );
				VectorNormalizeFast( dir );

#if 1			// FIXME
				VecToAngles( dir, temp );
				if( temp[2] ) Matrix4_Rotate( matrix, -temp[2], 1, 0, 0 );
				if( temp[0] ) Matrix4_Rotate( matrix, -temp[0], 0, 1, 0 );
				if( temp[1] ) Matrix4_Rotate( matrix, -temp[1], 0, 0, 1 );
#else
				VectorCopy( dir, &matrix[0] );
				MakeNormalVectors( &matrix[0], &matrix[4], &matrix[8] );
#endif
			}
		case TC_GEN_REFLECTION:
			r_enableNormals = qtrue;

			GL_EnableTexGen( GL_S, GL_REFLECTION_MAP_ARB );
			GL_EnableTexGen( GL_T, GL_REFLECTION_MAP_ARB );
			GL_EnableTexGen( GL_R, GL_REFLECTION_MAP_ARB );
			return qtrue;
		case TC_GEN_FOG:
		{
			int		fogPtype;
			cplane_t *fogPlane;
			shader_t *fogShader;
			vec3_t	viewtofog;
			float	fogNormal[3], vpnNormal[3];
			float	dist, vdist, fogDist, vpnDist;

			fogPlane = r_texFog->visibleplane;
			fogShader = r_texFog->shader;

			matrix[0] = matrix[5] = fogShader->fog_dist;
			matrix[13] = 1.5f/(float)FOG_TEXTURE_HEIGHT;

			// distance to fog
			dist = ri.fog_dist_to_eye[r_texFog-r_worldmodel->fogs];

			if( r_currentShader->flags & SHADER_SKY ) {
				if( dist > 0 )
					VectorMA( ri.viewOrigin, -dist, fogPlane->normal, viewtofog );
				else
					VectorCopy( ri.viewOrigin, viewtofog );
			} else {
				VectorCopy( ri.currententity->origin, viewtofog );
			}

			// some math tricks to take entity's rotation matrix into account
			// for fog texture coordinates calculations:
			// M is rotation matrix, v is vertex, t is transform vector
			// n is plane's normal, d is plane's dist, r is view origin
			// (M*v + t)*n - d = (M*n)*v - ((d - t*n))
			// (M*v + t - r)*n = (M*n)*v - ((r - t)*n)
			fogNormal[0] = DotProduct( ri.currententity->axis[0], fogPlane->normal ) * ri.currententity->scale;
			fogNormal[1] = DotProduct( ri.currententity->axis[1], fogPlane->normal ) * ri.currententity->scale;
			fogNormal[2] = DotProduct( ri.currententity->axis[2], fogPlane->normal ) * ri.currententity->scale;
			fogPtype = ( fogNormal[0] == 1.0 ? PLANE_X : (fogNormal[1] == 1.0 ? PLANE_Y : (fogNormal[2] == 1.0 ? PLANE_Z : PLANE_NONAXIAL) ) );
			fogDist = (fogPlane->dist - DotProduct( viewtofog, fogPlane->normal ));

			vpnNormal[0] = DotProduct( ri.currententity->axis[0], ri.vpn ) * ri.currententity->scale;
			vpnNormal[1] = DotProduct( ri.currententity->axis[1], ri.vpn ) * ri.currententity->scale;
			vpnNormal[2] = DotProduct( ri.currententity->axis[2], ri.vpn ) * ri.currententity->scale;
			vpnDist = ((ri.viewOrigin[0] - viewtofog[0]) * ri.vpn[0] + (ri.viewOrigin[1] - viewtofog[1]) * ri.vpn[1] + (ri.viewOrigin[2] - viewtofog[2]) * ri.vpn[2]);

			outCoords = tUnitCoordsArray[unit][0];
			if( dist < 0 )	{ 	// camera is inside the fog brush
				for( i = 0; i < numVerts; i++, outCoords += 2 ) {
					outCoords[0] = DotProduct( vertsArray[i], vpnNormal ) - vpnDist;
                    if( fogPtype < 3 )
	                    outCoords[1] = -( vertsArray[i][fogPtype] - fogDist );
                    else
					    outCoords[1] = -( DotProduct( vertsArray[i], fogNormal ) - fogDist );
				}
			} else {
				for( i = 0; i < numVerts; i++, outCoords += 2 ) {
                     if( fogPtype < 3 )
                         vdist = vertsArray[i][fogPtype] - fogDist;
                     else
                         vdist = DotProduct( vertsArray[i], fogNormal ) - fogDist;
                     outCoords[0] = (( vdist < 0 ) ? ( DotProduct( vertsArray[i], vpnNormal ) - vpnDist ) * vdist / ( vdist - dist ) : 0.0f);
				     outCoords[1] = -vdist;
				}
			}

			GL_EnableTexGen( GL_S, 0 );
			GL_EnableTexGen( GL_T, 0 );
			GL_EnableTexGen( GL_R, 0 );

#ifdef VERTEX_BUFFER_OBJECTS
			if( glConfig.vertexBufferObject ) {
				identityMatrix = qfalse;
				outCoords = tUnitCoordsArray[unit][0];
				break;
			}
			else
#endif
			{
				qglTexCoordPointer( 2, GL_FLOAT, 0, tUnitCoordsArray[unit] );
				return qfalse;
			}
		}
		case TC_GEN_SVECTORS:
			GL_EnableTexGen( GL_S, 0 );
			GL_EnableTexGen( GL_T, 0 );
			GL_EnableTexGen( GL_R, 0 );

			qglTexCoordPointer( 4, GL_FLOAT, 0, sVectorsArray );
			return qtrue;
	}

	// note that non-VBO path never gets to this point
#ifdef VERTEX_BUFFER_OBJECTS
	qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_TC0+unit] );
	qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec2_t ), outCoords, GL_STREAM_DRAW_ARB );
	qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
#endif

	return identityMatrix;
}

/*
================
R_ApplyTCMods
================
*/
static void R_ApplyTCMods( const shaderpass_t *pass, mat4x4_t result )
{
	int i;
	const float *table;
	float t1, t2, sint, cost;
	mat4x4_t m1, m2;
	const tcmod_t *tcmod;

	for( i = 0, tcmod = pass->tcmods; i < pass->numtcmods; i++, tcmod++ ) {
		switch( tcmod->type ) {
			case TC_MOD_ROTATE:
				cost = tcmod->args[0] * r_currentShaderTime;
				sint = R_FastSin( cost );
				cost = R_FastSin( cost + 0.25 );
				m2[0] =  cost, m2[1] = sint, m2[12] =  0.5f * (sint - cost + 1);
				m2[4] = -sint, m2[5] = cost, m2[13] = -0.5f * (sint + cost - 1);
				Matrix4_Copy2D( result, m1 );
				Matrix4_Multiply2D( m2, m1, result );
				break;
			case TC_MOD_SCALE:
				Matrix4_Scale2D( result, tcmod->args[0], tcmod->args[1] );
				break;
			case TC_MOD_TURB:
				t1 = (1.0 / 4.0);
				t2 = tcmod->args[2] + r_currentShaderTime * tcmod->args[3];
				Matrix4_Scale2D( result, 1 + (tcmod->args[1] * R_FastSin( t2 ) + tcmod->args[0]) * t1, 1 + (tcmod->args[1] * R_FastSin( t2 + 0.25 ) + tcmod->args[0]) * t1 );
				break;
			case TC_MOD_STRETCH:
				table = R_TableForFunc( tcmod->args[0] );
				t2 = tcmod->args[3] + r_currentShaderTime * tcmod->args[4];
				t1 = FTABLE_EVALUATE( table, t2 ) * tcmod->args[2] + tcmod->args[1];
				t1 = t1 ? 1.0f / t1 : 1.0f;
				t2 = 0.5f - 0.5f * t1;
				Matrix4_Stretch2D( result, t1, t2 );
				break;
			case TC_MOD_SCROLL:
				t1 = tcmod->args[0] * r_currentShaderTime;
				t2 = tcmod->args[1] * r_currentShaderTime;
				Matrix4_Translate2D( result, t1 - floor( t1 ), t2 - floor( t2 ) );
				break;
			case TC_MOD_TRANSFORM:
				m2[0] = tcmod->args[0], m2[1] = tcmod->args[2], m2[12] = tcmod->args[4],
				m2[5] = tcmod->args[1], m2[4] = tcmod->args[3], m2[13] = tcmod->args[5]; 
				Matrix4_Copy2D( result, m1 );
				Matrix4_Multiply2D( m2, m1, result );
				break;
			default:
				break;
		}
	}
}

/*
==============
R_ShaderpassTex
==============
*/
static inline image_t *R_ShaderpassTex( const shaderpass_t *pass, int unit )
{
	if( pass->anim_fps )
		return pass->anim_frames[(int)(pass->anim_fps * r_currentShaderTime) % pass->anim_numframes];
	if( pass->flags & SHADERPASS_LIGHTMAP )
		return r_lightmapTextures[r_superLightStyle->lightmapNum[r_lightmapStyleNum[unit]]];
	return ( pass->anim_frames[0] ? pass->anim_frames[0] : r_notexture );
}

/*
================
R_BindShaderpass
================
*/
static void R_BindShaderpass( const shaderpass_t *pass, image_t *tex, int unit )
{
	mat4x4_t m1, m2, result;
	qboolean identityMatrix;

	if( !tex )
		tex = R_ShaderpassTex( pass, unit );

	GL_Bind( unit, tex );
	if( unit && !pass->program )
		qglEnable( GL_TEXTURE_2D );
	GL_SetTexCoordArrayMode( (tex->flags & IT_CUBEMAP ? GL_TEXTURE_CUBE_MAP_ARB : GL_TEXTURE_COORD_ARRAY) );

	identityMatrix = R_VertexTCBase( pass, unit, result );

	if( pass->numtcmods ) {
		identityMatrix = qfalse;
		R_ApplyTCMods( pass, result );
	}

	if( pass->tcgen == TC_GEN_REFLECTION || pass->tcgen == TC_GEN_REFLECTION_CELLSHADE ) {
		Matrix4_Transpose( ri.modelviewMatrix, m1 );
		Matrix4_Copy( result, m2 );
		Matrix4_Multiply( m2, m1, result );
		GL_LoadTexMatrix( result );
		return;
	}

	if( identityMatrix )
		GL_LoadIdentityTexMatrix ();
	else
		GL_LoadTexMatrix( result );
}

/*
================
R_ModifyColor
================
*/
void R_ModifyColor( const shaderpass_t *pass )
{
	int i, c, bits;
	float *table, temp, a;
	vec3_t t, v, style;
	qbyte *bArray, *inArray, rgba[4] = { 255, 255, 255, 255 };
	qboolean noArray, identityAlpha, entityAlpha;
	const shaderfunc_t *rgbgenfunc, *alphagenfunc;

	noArray = ( pass->flags & SHADERPASS_NOCOLORARRAY ) && !r_colorFog;
	rgbgenfunc = &pass->rgbgen.func;
	alphagenfunc = &pass->alphagen.func;

	if( noArray )
		numColors = 1;
	else
		numColors = numVerts;

	if( (r_overbrightbits->integer > 0) && !(r_ignorehwgamma->integer) )
		bits = r_overbrightbits->integer;
	else
		bits = 0;

	bArray = colorArray[0];
	inArray = inColorsArray[0][0];

	if( pass->rgbgen.type == RGB_GEN_IDENTITY_LIGHTING ) {
		entityAlpha = identityAlpha = qfalse;
		memset( bArray, r_identityLighting, sizeof( byte_vec4_t ) * numColors );
	} else if( pass->rgbgen.type == RGB_GEN_EXACT_VERTEX ) {
		entityAlpha = identityAlpha = qfalse;
		memcpy( bArray, inArray, sizeof( byte_vec4_t ) * numColors );
	} else {
		entityAlpha = qfalse;
		identityAlpha = qtrue;
		memset( bArray, 255, sizeof( byte_vec4_t ) * numColors );

		switch( pass->rgbgen.type ) {
			case RGB_GEN_IDENTITY:
				break;
			case RGB_GEN_CONST:
				rgba[0] = R_FloatToByte( pass->rgbgen.args[0] );
				rgba[1] = R_FloatToByte( pass->rgbgen.args[1] );
				rgba[2] = R_FloatToByte( pass->rgbgen.args[2] );

				for( i = 0, c = *(int *)rgba; i < numColors; i++, bArray += 4 )
					*(int *)bArray = c;
				break;
			case RGB_GEN_COLORWAVE:
				if( rgbgenfunc->type == SHADER_FUNC_NOISE ) {
					temp = R_BackendGetNoiseValue( 0, 0, 0, (r_currentShaderTime + rgbgenfunc->args[2]) * rgbgenfunc->args[3] );
				} else {
					table = R_TableForFunc( rgbgenfunc->type );
					temp = r_currentShaderTime * rgbgenfunc->args[3] + rgbgenfunc->args[2];
					temp = FTABLE_EVALUATE( table, temp ) * rgbgenfunc->args[1] + rgbgenfunc->args[0];
				}

				temp = temp * rgbgenfunc->args[1] + rgbgenfunc->args[0];
				a = pass->rgbgen.args[0] * temp; rgba[0] = a <= 0 ? 0 : R_FloatToByte( a );
				a = pass->rgbgen.args[1] * temp; rgba[1] = a <= 0 ? 0 : R_FloatToByte( a );
				a = pass->rgbgen.args[2] * temp; rgba[2] = a <= 0 ? 0 : R_FloatToByte( a );

				for( i = 0, c = *(int *)rgba; i < numColors; i++, bArray += 4 )
					*(int *)bArray = c;
				break;
			case RGB_GEN_ENTITY:
				entityAlpha = qtrue;
				identityAlpha = (ri.currententity->color[3] == 255);

				for( i = 0, c = *(int *)ri.currententity->color; i < numColors; i++, bArray += 4 )
					*(int *)bArray = c;
				break;
			case RGB_GEN_ONE_MINUS_ENTITY:
				rgba[0] = 255 - ri.currententity->color[0];
				rgba[1] = 255 - ri.currententity->color[1];
				rgba[2] = 255 - ri.currententity->color[2];

				for( i = 0, c = *(int *)rgba; i < numColors; i++, bArray += 4 )
					*(int *)bArray = c;
				break;
			case RGB_GEN_VERTEX:
				VectorSet( style, -1, -1, -1 );

				if( !r_superLightStyle || r_superLightStyle->vertexStyles[1] == 255 ) {
					VectorSet( style, 1, 1, 1 );
					if( r_superLightStyle && r_superLightStyle->vertexStyles[0] != 255 )
						VectorCopy( r_lightStyles[r_superLightStyle->vertexStyles[0]].rgb, style );
				}

				if( style[0] == style[1] && style[1] == style[2] && style[2] == 1 ) {
					for( i = 0; i < numColors; i++, bArray += 4, inArray += 4 ) {
						bArray[0] = inArray[0] >> bits;
						bArray[1] = inArray[1] >> bits;
						bArray[2] = inArray[2] >> bits;
					}
				} else {
					int j;
					float *tc;
					vec3_t temp[MAX_ARRAY_VERTS];

					memset( temp, 0, sizeof( vec3_t ) * numColors );

					for( j = 0; j < MAX_LIGHTMAPS && r_superLightStyle->vertexStyles[j] != 255; j++ ) {
						VectorCopy( r_lightStyles[r_superLightStyle->vertexStyles[j]].rgb, style );
						if( VectorCompare( style, vec3_origin ) )
							continue;

						inArray = inColorsArray[j][0];
						for( i = 0, tc = temp[0]; i < numColors; i++, tc += 3, inArray += 4 ) {
							tc[0] += (inArray[0] >> bits) * style[0];
							tc[1] += (inArray[1] >> bits) * style[1];
							tc[2] += (inArray[2] >> bits) * style[2];
						}
					}

					for( i = 0, tc = temp[0]; i < numColors; i++, tc += 3, bArray += 4 ) {
						bArray[0] = bound( 0, tc[0], 255 );
						bArray[1] = bound( 0, tc[1], 255 );
						bArray[2] = bound( 0, tc[2], 255 );
					}
				}
				break;
			case RGB_GEN_ONE_MINUS_VERTEX:
				for( i = 0; i < numColors; i++, bArray += 4, inArray += 4 ) {
					bArray[0] = 255 - (inArray[0] >> bits);
					bArray[1] = 255 - (inArray[1] >> bits);
					bArray[2] = 255 - (inArray[2] >> bits);
				}
				break;
			case RGB_GEN_LIGHTING_DIFFUSE:
				if( ri.currententity )
					R_LightForEntity( ri.currententity, bArray );
				break;
			case RGB_GEN_LIGHTING_DIFFUSE_ONLY:
				if( ri.currententity ) {
					vec4_t diffuse;

					if( ri.currententity->flags & RF_FULLBRIGHT )
						VectorSet( diffuse, 1, 1, 1 );
					else
						R_LightForOrigin( ri.currententity->lightingOrigin, t, NULL, diffuse, ri.currentmodel->radius * ri.currententity->scale );

					rgba[0] = R_FloatToByte( diffuse[0] );
					rgba[1] = R_FloatToByte( diffuse[1] );
					rgba[2] = R_FloatToByte( diffuse[2] );

					for( i = 0, c = *(int *)rgba; i < numColors; i++, bArray += 4 )
						*(int *)bArray = c;
				}
				break;
			case RGB_GEN_LIGHTING_AMBIENT_ONLY:
				if( ri.currententity ) {
					vec4_t ambient;

					if( ri.currententity->flags & RF_FULLBRIGHT )
						VectorSet( ambient, 1, 1, 1 );
					else
						R_LightForOrigin( ri.currententity->lightingOrigin, t, ambient, NULL, ri.currentmodel->radius * ri.currententity->scale );

					rgba[0] = R_FloatToByte( ambient[0] );
					rgba[1] = R_FloatToByte( ambient[1] );
					rgba[2] = R_FloatToByte( ambient[2] );

					for( i = 0, c = *(int *)rgba; i < numColors; i++, bArray += 4 )
						*(int *)bArray = c;
				}
				break;
			case RGB_GEN_FOG:
				for( i = 0, c = *(int *)r_texFog->shader->fog_color; i < numColors; i++, bArray += 4 )
					*(int *)bArray = c;
				break;
			case RGB_GEN_CUSTOM:
				c = (int)pass->rgbgen.args[0];
				for( i = 0, c = *(int *)r_customColors[c]; i < numColors; i++, bArray += 4 )
					*(int *)bArray = c;
				break;
			default:
				break;
		}
	}

	bArray = colorArray[0];
	inArray = inColorsArray[0][0];

	switch( pass->alphagen.type ) {
		case ALPHA_GEN_IDENTITY:
			if( identityAlpha )
				break;
			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = 255;
			break;
		case ALPHA_GEN_CONST:
			c = R_FloatToByte( pass->alphagen.args[0] );
			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = c;
			break;
		case ALPHA_GEN_WAVE:
			if( alphagenfunc->type == SHADER_FUNC_NOISE ) {
				a = R_BackendGetNoiseValue( 0, 0, 0, (r_currentShaderTime + alphagenfunc->args[2]) * alphagenfunc->args[3] );
			} else {
				table = R_TableForFunc( alphagenfunc->type );
				a = alphagenfunc->args[2] + r_currentShaderTime * alphagenfunc->args[3];
				a = FTABLE_EVALUATE( table, a );
			}

			a = a * alphagenfunc->args[1] + alphagenfunc->args[0];
			c = a <= 0 ? 0 : R_FloatToByte( a );

			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = c;
			break;
		case ALPHA_GEN_PORTAL:
			VectorAdd( vertsArray[0], ri.currententity->origin, v );
			VectorSubtract( ri.viewOrigin, v, t );
			a = VectorLength( t ) * pass->alphagen.args[0];
			clamp ( a, 0.0f, 1.0f );
			c = R_FloatToByte( a );

			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = c;
			break;
		case ALPHA_GEN_VERTEX:
			for( i = 0; i < numColors; i++, bArray += 4, inArray += 4 )
				bArray[3] = inArray[3];
			break;
		case ALPHA_GEN_ONE_MINUS_VERTEX:
			for( i = 0; i < numColors; i++, bArray += 4, inArray += 4 )
				bArray[3] = 255 - inArray[3];
			break;
		case ALPHA_GEN_ENTITY:
			if( entityAlpha )
				break;
			for( i = 0; i < numColors; i++, bArray += 4 )
				bArray[3] = ri.currententity->color[3];
			break;
		case ALPHA_GEN_SPECULAR:
			VectorSubtract( ri.viewOrigin, ri.currententity->origin, t );
			if( !Matrix_Compare( ri.currententity->axis, axis_identity ) )
				Matrix_TransformVector( ri.currententity->axis, t, v );
			else
				VectorCopy( t, v );

			for( i = 0; i < numColors; i++, bArray += 4 ) {
				VectorSubtract( v, vertsArray[i], t );
				c = VectorLength( t );
				a = DotProduct( t, normalsArray[i] ) / max( 0.1, c );
				a = pow( a, pass->alphagen.args[0] );
				bArray[3] = a <= 0 ? 0 : R_FloatToByte( a );
			}
			break;
		case ALPHA_GEN_DOT:
			if( !Matrix_Compare( ri.currententity->axis, axis_identity ) )
				Matrix_TransformVector( ri.currententity->axis, ri.vpn, v );
			else
				VectorCopy ( ri.vpn, v );

			for( i = 0; i < numColors; i++, bArray += 4 ) {
				a = DotProduct( v, inNormalsArray[i] ); if ( a < 0 ) a = -a;
				bArray[3] = R_FloatToByte( bound( pass->alphagen.args[0], a, pass->alphagen.args[1] ) );
			}
			break;
		case ALPHA_GEN_ONE_MINUS_DOT:
			if( !Matrix_Compare( ri.currententity->axis, axis_identity ) )
				Matrix_TransformVector( ri.currententity->axis, ri.vpn, v );
			else
				VectorCopy( ri.vpn, v );

			for( i = 0; i < numColors; i++, bArray += 4 ) {
				a = DotProduct( v, inNormalsArray[i] ); if ( a < 0 ) a = -a; a = 1.0f - a;
				bArray[3] = R_FloatToByte( bound( pass->alphagen.args[0], a, pass->alphagen.args[1] ) );
			}
		default:
			break;
	}

	if( r_colorFog ) {
		float	dist, vdist;
		cplane_t *fogPlane;
		vec3_t	viewtofog;
		float	fogNormal[3], vpnNormal[3];
		float	fogDist, vpnDist, fogShaderDist;
		int		fogptype;
		qboolean alphaFog;
		int		blendsrc, blenddst;

		blendsrc = pass->flags & GLSTATE_SRCBLEND_MASK;
		blenddst = pass->flags & GLSTATE_DSTBLEND_MASK;
		if( (blendsrc != GLSTATE_SRCBLEND_SRC_ALPHA && blenddst != GLSTATE_DSTBLEND_SRC_ALPHA) && 
			(blendsrc != GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA && blenddst != GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA) )
			alphaFog = qfalse;
		else
			alphaFog = qtrue;

		fogPlane = r_colorFog->visibleplane;
		fogShaderDist = r_colorFog->shader->fog_dist;
		dist = ri.fog_dist_to_eye[r_colorFog-r_worldmodel->fogs];

		if( r_currentShader->flags & SHADER_SKY ) {
			if( dist > 0 )
				VectorScale( fogPlane->normal, -dist, viewtofog );
			else
				VectorClear( viewtofog );
		} else {
			VectorCopy( ri.currententity->origin, viewtofog );
		}

		vpnNormal[0] = DotProduct( ri.currententity->axis[0], ri.vpn ) * fogShaderDist * ri.currententity->scale;
		vpnNormal[1] = DotProduct( ri.currententity->axis[1], ri.vpn ) * fogShaderDist * ri.currententity->scale;
		vpnNormal[2] = DotProduct( ri.currententity->axis[2], ri.vpn ) * fogShaderDist * ri.currententity->scale;
		vpnDist = ((ri.viewOrigin[0] - viewtofog[0]) * ri.vpn[0] + (ri.viewOrigin[1] - viewtofog[1]) * ri.vpn[1] + (ri.viewOrigin[2] - viewtofog[2]) * ri.vpn[2]) * fogShaderDist;

		bArray = colorArray[0];
		if( dist < 0 ) { // camera is inside the fog
			for( i = 0; i < numColors; i++, bArray += 4 ) {
				temp = DotProduct( vertsArray[i], vpnNormal ) - vpnDist;
				c = (1.0f - bound( 0, temp, 1.0f )) * 0xFFFF;

				if( alphaFog ) {
					bArray[3] = (bArray[3] * c) >> 16;
				} else {
					bArray[0] = (bArray[0] * c) >> 16;
					bArray[1] = (bArray[1] * c) >> 16;
					bArray[2] = (bArray[2] * c) >> 16;
				}
			}
		} else {
			fogNormal[0] = DotProduct( ri.currententity->axis[0], fogPlane->normal ) * ri.currententity->scale;
			fogNormal[1] = DotProduct( ri.currententity->axis[1], fogPlane->normal ) * ri.currententity->scale;
			fogNormal[2] = DotProduct( ri.currententity->axis[2], fogPlane->normal ) * ri.currententity->scale;
			fogptype = ( fogNormal[0] == 1.0 ? PLANE_X : (fogNormal[1] == 1.0 ? PLANE_Y : (fogNormal[2] == 1.0 ? PLANE_Z : PLANE_NONAXIAL) ) );
			if( fogptype > 2 )
				VectorScale( fogNormal, fogShaderDist, fogNormal );
			fogDist = (fogPlane->dist - DotProduct( viewtofog, fogPlane->normal )) * fogShaderDist;
			dist *= fogShaderDist;

			for( i = 0; i < numColors; i++, bArray += 4 ) {
				if( fogptype < 3 )
					vdist = vertsArray[i][fogptype] * fogShaderDist - fogDist;
				else
					vdist = DotProduct( vertsArray[i], fogNormal ) - fogDist;

				if( vdist < 0 ) {
					temp = ( DotProduct( vertsArray[i], vpnNormal ) - vpnDist ) * vdist / ( vdist - dist );
					c = (1.0f - bound( 0, temp, 1.0f )) * 0xFFFF;

   					if( alphaFog ) {
						bArray[3] = (bArray[3] * c) >> 16;
					} else {
						bArray[0] = (bArray[0] * c) >> 16;
						bArray[1] = (bArray[1] * c) >> 16;
						bArray[2] = (bArray[2] * c) >> 16;
					}
				}
			}
		}
	}
}

/*
================
R_ShaderpassBlendmode
================
*/
static int R_ShaderpassBlendmode( int passFlags )
{
	if( passFlags & SHADERPASS_BLEND_REPLACE )
		return GL_REPLACE;
	if( passFlags & SHADERPASS_BLEND_MODULATE )
		return GL_MODULATE;
	if( passFlags & SHADERPASS_BLEND_ADD )
		return GL_ADD;
	if( passFlags & SHADERPASS_BLEND_DECAL )
		return GL_DECAL;
	return 0;
}

/*
================
R_SetShaderState
================
*/
static void R_SetShaderState( void )
{
	int state;

// Face culling
	if( !gl_cull->integer || (r_features & MF_NOCULL) )
		GL_Cull( 0 );
	else if( r_currentShader->flags & SHADER_CULL_FRONT )
		GL_Cull( GL_FRONT );
	else if( r_currentShader->flags & SHADER_CULL_BACK )
		GL_Cull( GL_BACK );
	else
		GL_Cull( 0 );

	state = 0;
	if( r_currentShader->flags & SHADER_POLYGONOFFSET )
		state |= GLSTATE_OFFSET_FILL;
	if( r_currentShader->flags & SHADER_FLARE )
		state |= GLSTATE_NO_DEPTH_TEST;
	r_currentShaderState = state;
}

/*
================
R_RenderMeshGeneric
================
*/
void R_RenderMeshGeneric( void )
{
	const shaderpass_t *pass = r_accumPasses[0];

	R_BindShaderpass( pass, NULL, 0 );
	R_ModifyColor( pass );

	if( pass->flags & SHADERPASS_BLEND_REPLACE )
		GL_TexEnv( GL_REPLACE );
	else
		GL_TexEnv( GL_MODULATE );
	GL_SetState( r_currentShaderState | (pass->flags & GLSTATE_MASK) );

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
	const shaderpass_t *pass = r_accumPasses[0];

	R_BindShaderpass( pass, NULL, 0 );
	R_ModifyColor( pass );

	GL_TexEnv( GL_MODULATE );
	GL_SetState( r_currentShaderState | (pass->flags & GLSTATE_MASK) | GLSTATE_BLEND_MTEX );

	for( i = 1; i < r_numAccumPasses; i++ ) {
		pass = r_accumPasses[i];
		R_BindShaderpass( pass, NULL, i );
		GL_TexEnv( R_ShaderpassBlendmode( pass->flags ) );
	}

	R_FlushArrays ();
}

/*
================
R_RenderMeshCombined
================
*/
void R_RenderMeshCombined( void )
{
	int	i;
	const shaderpass_t *pass = r_accumPasses[0];

	R_BindShaderpass( pass, NULL, 0 );
	R_ModifyColor( pass );

	GL_TexEnv( GL_MODULATE );
	GL_SetState( r_currentShaderState | (pass->flags & GLSTATE_MASK) | GLSTATE_BLEND_MTEX );

	for( i = 1; i < r_numAccumPasses; i++ ) {
		pass = r_accumPasses[i];
		R_BindShaderpass( pass, NULL, i );

		if( pass->flags & (SHADERPASS_BLEND_REPLACE|SHADERPASS_BLEND_MODULATE) ) {
			GL_TexEnv( GL_MODULATE );
		} else if( pass->flags & SHADERPASS_BLEND_ADD ) {
			// these modes are best set with TexEnv, Combine4 would need much more setup
			GL_TexEnv( GL_ADD );
		} else if( pass->flags & SHADERPASS_BLEND_DECAL ) {
			// mimics Alpha-Blending in upper texture stage, but instead of multiplying the alpha-channel, they're added
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
		} else {
			int blendsrc, blenddst;

			GL_TexEnv( GL_COMBINE4_NV );

			qglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_ADD );
			qglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_ADD );

			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR );
			qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE );
			qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA );

			blendsrc = pass->flags & GLSTATE_SRCBLEND_MASK;
			blenddst = pass->flags & GLSTATE_DSTBLEND_MASK;

			switch( blendsrc ) {
			case GLSTATE_SRCBLEND_ONE:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GLSTATE_SRCBLEND_ZERO:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );
				break;
			case GLSTATE_SRCBLEND_DST_COLOR:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GLSTATE_SRCBLEND_SRC_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GLSTATE_SRCBLEND_DST_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA:
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

			switch( blenddst ) {
			case GLSTATE_DSTBLEND_ONE:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GLSTATE_DSTBLEND_ZERO:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA );
				break;
			case GLSTATE_DSTBLEND_SRC_COLOR:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA );
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GLSTATE_DSTBLEND_SRC_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA );
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case GLSTATE_DSTBLEND_DST_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA );
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA:
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_ALPHA);
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA );
				break;
			}
		}
	}

	R_FlushArrays ();
}

/*
================
R_RenderMeshGLSLProgrammed
================
*/
static qboolean R_RenderMeshGLSLProgrammed( void )
{
	int i, tcgen;
	int state;
	qboolean breakIntoPasses = qfalse;
	int program, object, programFeatures = 0;
	image_t *base;
	mat4x4_t unused;
	vec3_t lightDir = { 0.0f, 0.0f, 0.0f };
	vec4_t ambient = { 0.0f, 0.0f, 0.0f, 0.0f }, diffuse = { 0.0f, 0.0f, 0.0f, 0.0f };
	superLightStyle_t *lightStyle;
	shaderpass_t *pass = ( shaderpass_t * )r_accumPasses[0];

	r_enableNormals = qtrue;

	if( r_currentMeshBuffer->infokey > 0 )
	{	// world surface
		if( r_lightmap->integer || r_currentMeshBuffer->dlightbits )
			base = r_whitetexture;			// white
		else
			base = pass->anim_frames[0];	// base

		// we use multipass for dynamic lights, so bind the white texture
		// instead of base in GLSL program and add another modulative pass (diffusemap)
		if( !r_lightmap->integer && r_currentMeshBuffer->dlightbits )
		{
			breakIntoPasses = qtrue;
			r_GLSLpasses[1] = *pass;
			r_GLSLpasses[1].flags = (pass->flags & SHADERPASS_NOCOLORARRAY)|SHADERPASS_BLEND_MODULATE|GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
			r_GLSLpasses[1].program = NULL;
		}
	}
	else
	{	// models
		base = pass->anim_frames[0];		// base
	}

	tcgen = pass->tcgen;					// store the original tcgen
	pass->tcgen = TC_GEN_BASE;
	R_BindShaderpass( pass, base, 0 );
	if( !breakIntoPasses )
	{	// calculate the fragment color
		R_ModifyColor( pass );
	}
	else
	{	// rgbgen identity (255,255,255,255)
		numColors = 1;
		colorArray[0][0] = colorArray[0][1] = colorArray[0][2] = colorArray[0][3] = 255;
	}

	// set shaderpass state (blending, depthwrite, etc)
	state = r_currentShaderState | (pass->flags & GLSTATE_MASK) | GLSTATE_BLEND_MTEX;
	GL_SetState( state );

	// we only send S-vectors to GPU and recalc T-vectors as cross product
	// in vertex shader
	pass->tcgen = TC_GEN_SVECTORS;
	GL_Bind( 1, pass->anim_frames[1] );				// normalmap
	GL_SetTexCoordArrayMode( GL_TEXTURE_COORD_ARRAY );
	R_VertexTCBase( pass, 1, unused );

	if( pass->anim_frames[2] && r_lighting_glossintensity->value ) {
		programFeatures |= PROGRAM_APPLY_SPECULAR;
		GL_Bind( 2, pass->anim_frames[2] );			// gloss
	}

	if( r_currentMeshBuffer->infokey > 0 ) {		// world surface
		lightStyle = r_superLightStyle;

		// bind lightmap textures and set program's features for lightstyles
		if( r_superLightStyle && r_superLightStyle->lightmapNum[0] >= 0 ) {
			pass->tcgen = TC_GEN_LIGHTMAP;

			for( i = 0; i < MAX_LIGHTMAPS && r_superLightStyle->lightmapStyles[i] != 255; i++ ) {
				programFeatures |= (PROGRAM_APPLY_LIGHTSTYLE0 << i);

				r_lightmapStyleNum[i+3] = i;
				GL_Bind( i+3, r_lightmapTextures[r_superLightStyle->lightmapNum[i]] );		// lightmap
				GL_SetTexCoordArrayMode( GL_TEXTURE_COORD_ARRAY );
				R_VertexTCBase( pass, i+3, unused );
			}

			if( i == 1 ) {
				vec_t *rgb = r_lightStyles[r_superLightStyle->lightmapStyles[0]].rgb;

				// PROGRAM_APPLY_FB_LIGHTMAP indicates that there's no need to renormalize
				// the lighting vector for specular (saves 3 adds, 3 muls and 1 normalize per pixel)
				if( rgb[0] == 1 && rgb[1] == 1 && rgb[2] == 1 )
					programFeatures |= PROGRAM_APPLY_FB_LIGHTMAP;
			}
		}
	} else {
		vec3_t temp;

		lightStyle = NULL;
		programFeatures |= PROGRAM_APPLY_DIRECTIONAL_LIGHT;

		// get weighted incoming direction of world and dynamic lights
		R_LightForOrigin( ri.currententity->lightingOrigin, temp, ambient, diffuse, 
			ri.currententity->model ? ri.currententity->model->radius * ri.currententity->scale : 0 );

		if( ri.currententity->flags & RF_MINLIGHT ) {
			if( ambient[0] <= 0.1f || ambient[1] <= 0.1f || ambient[2] <= 0.1f )
				VectorSet( ambient, 0.1f, 0.1f, 0.1f );
		}

		// rotate direction
		Matrix_TransformVector( ri.currententity->axis, temp, lightDir );
	}

	pass->tcgen = tcgen;		// restore original tcgen

	// update uniforms
	program = R_RegisterGLSLProgram( pass->program, NULL, programFeatures );
	if( !program )
		return qfalse;

	object = R_GetProgramObject( program );

	qglUseProgramObjectARB( object );

	R_UpdateProgramUniforms( program, ri.viewOrigin, vec3_origin, lightDir, ambient, diffuse, lightStyle );

	R_FlushArrays ();

	qglUseProgramObjectARB( 0 );

	if( breakIntoPasses ) {
		R_AccumulatePass( &r_GLSLpasses[0] );		// dynamic lighting pass
		R_AccumulatePass( &r_GLSLpasses[1] );		// modulate (diffusemap)
	}

	return qfalse;
}

/*
================
R_RenderAccumulatedPasses
================ 
*/
static void R_RenderAccumulatedPasses( void )
{
	R_CleanUpTextureUnits( r_numAccumPasses );

	if( r_accumPasses[0]->program ) {
		r_numAccumPasses = 0;
		R_RenderMeshGLSLProgrammed ();
		return;
	}

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
static void R_AccumulatePass( const shaderpass_t *pass )
{
	qboolean accumulate;
	const shaderpass_t *prevPass;

	// see if there are any free texture units
	accumulate = ( r_numAccumPasses < glConfig.maxTextureUnits ) && !( pass->flags & SHADERPASS_DLIGHT ) && !pass->program;

	if( accumulate ) {
		if( !r_numAccumPasses ) {
			r_accumPasses[r_numAccumPasses++] = pass;
			return;
		}

		// ok, we've got several passes, diff against the previous
		prevPass = r_accumPasses[r_numAccumPasses-1];

		// see if depthfuncs and colors are good
		if(
			((prevPass->flags ^ pass->flags) & GLSTATE_DEPTHFUNC_EQ) ||
			(pass->flags & GLSTATE_ALPHAFUNC) ||
			(pass->rgbgen.type != RGB_GEN_IDENTITY) ||
			(pass->alphagen.type != ALPHA_GEN_IDENTITY) ||
			((prevPass->flags & GLSTATE_ALPHAFUNC) && !(pass->flags & GLSTATE_DEPTHFUNC_EQ))
			)
			accumulate = qfalse;

		// see if blendmodes are good
		if( accumulate ) {
			int mode, prevMode;
			
			prevMode = R_ShaderpassBlendmode( prevPass->flags );
			mode = R_ShaderpassBlendmode( pass->flags );

			if( !mode ) {
				accumulate = ( prevMode == GL_REPLACE ) && glConfig.NVTextureEnvCombine4;
			} else if( glConfig.textureEnvCombine ) {
				if( prevMode == GL_REPLACE )
					accumulate = ( mode == GL_ADD ) ? glConfig.textureEnvAdd : qtrue;
				else if( prevMode == GL_ADD )
					accumulate = ( mode == GL_ADD ) && glConfig.textureEnvAdd;
				else if( prevMode == GL_MODULATE )
					accumulate = ( mode == GL_MODULATE || mode == GL_REPLACE );
				else
					accumulate = qfalse;
			} else/* if( glConfig.multiTexture )*/ {
				if ( prevMode == GL_REPLACE )
					accumulate = ( mode == GL_ADD ) ? glConfig.textureEnvAdd : ( mode != GL_DECAL );
				else if( prevMode == GL_ADD )
					accumulate = ( mode == GL_ADD ) && glConfig.textureEnvAdd;
				else if( prevMode == GL_MODULATE )
					accumulate = ( mode == GL_MODULATE || mode == GL_REPLACE );
				else
					accumulate = qfalse;
			}
		}
	}

	// no, failed to accumulate
	if( !accumulate ) {
		if( r_numAccumPasses )
			R_RenderAccumulatedPasses ();

		if( pass->flags & SHADERPASS_DLIGHT ) {
			R_CleanUpTextureUnits( 1 );
			R_AddDynamicLights( r_currentMeshBuffer->dlightbits, r_currentShaderState | (pass->flags & GLSTATE_MASK) );
			return;
		}
	}

	r_accumPasses[r_numAccumPasses++] = pass;
	if( pass->program )
		R_RenderAccumulatedPasses ();
}

/*
================
R_SetupLightmapMode
================
*/
void R_SetupLightmapMode( void )
{
	r_lightmapPasses[0].tcgen = TC_GEN_LIGHTMAP;
	r_lightmapPasses[0].rgbgen.type = RGB_GEN_IDENTITY;
	r_lightmapPasses[0].alphagen.type = ALPHA_GEN_IDENTITY;
	r_lightmapPasses[0].flags &= ~(SHADERPASS_BLENDMODE|SHADERPASS_DELUXEMAP|GLSTATE_ALPHAFUNC|GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK|GLSTATE_DEPTHFUNC_EQ);
	r_lightmapPasses[0].flags |= SHADERPASS_LIGHTMAP|SHADERPASS_NOCOLORARRAY|SHADERPASS_BLEND_MODULATE/*|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO*/;
	if( r_lightmap->integer )
		r_lightmapPasses[0].flags |= GLSTATE_DEPTHWRITE;
}

/*
================
R_RenderMeshBuffer
================
*/
void R_RenderMeshBuffer( const meshbuffer_t *mb, qboolean shadowpass )
{
	int	i;
	msurface_t *surf;
	shaderpass_t *pass;
	mfog_t *fog;
	unsigned int dlightBits;
	qboolean breakEarly = qfalse;

	if( !numVerts )
		return;

	surf = mb->infokey > 0 ? &ri.currentmodel->surfaces[mb->infokey-1] : NULL;
	if( surf )
		r_superLightStyle = &r_superLightStyles[surf->superLightStyle];
	else
		r_superLightStyle = NULL;
	r_currentMeshBuffer = mb;
			
	MB_NUM2SHADER( mb->shaderkey, r_currentShader );

	if( glState.in2DMode ) {
		r_currentShaderTime = curtime * 0.001f;
	} else {
		r_currentShaderTime = ri.refdef.time;

		if( ri.currententity ) {
			r_currentShaderTime -= ri.currententity->shaderTime;
			if ( r_currentShaderTime < 0 )
				r_currentShaderTime = 0;
		}
	}

	if( !r_triangleOutlines )
		R_SetShaderState ();

	if( r_currentShader->numdeforms )
		R_DeformVertices ();

	if( r_features & MF_KEEPLOCK )
		r_numkeptlocks++;
	else
		R_UnlockArrays ();

#ifdef VERTEX_BUFFER_OBJECTS
	if( glConfig.vertexBufferObject ) {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_VERTS] );
		qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( vec3_t ), vertsArray, GL_STREAM_DRAW_ARB );
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	}
	else
#endif
	{
		if( !r_arraysLocked )
			qglVertexPointer( 3, GL_FLOAT, 0, vertsArray );
	}

	if( !numIndexes || shadowpass )
		return;

	if( r_triangleOutlines ) {
		R_LockArrays( numVerts );

		if( r_showtris->integer )
			R_DrawTriangles ();
		if( r_shownormals->integer )
			R_DrawNormals ();

		R_ClearArrays ();
		return;
	}

	// extract the fog volume number from sortkey
	if( !r_worldmodel )
		fog = NULL;
	else
		MB_NUM2FOG( mb->sortkey, fog );

	if( fog && !fog->shader )
		return;

	// can we fog the geometry with alpha texture?
	r_texFog = (fog && fog->shader && ((r_currentShader->sort <= SHADER_SORT_ALPHATEST && 
		(r_currentShader->flags & (SHADER_DEPTHWRITE|SHADER_SKY))) || r_currentShader->fog_dist)) ? fog : NULL;

	// check if the fog volume is present but we can't use alpha texture
	r_colorFog = (fog && fog->shader && !r_texFog) ? fog : NULL;

	if( r_currentShader->flags & SHADER_FLARE )
		dlightBits = 0;
	else
		dlightBits = mb->dlightbits;

	R_LockArrays( numVerts );

	// accumulate passes for dynamic merging
	for ( i = 0, pass = r_currentShader->passes; i < r_currentShader->numpasses; i++, pass++ ) {
		if( !pass->program ) {
			if( pass->flags & SHADERPASS_LIGHTMAP ) {
				int j, k, l, u;

				// no valid lightmaps, goodbye
				if( !r_superLightStyle || r_superLightStyle->lightmapNum[0] < 0 || r_superLightStyle->lightmapStyles[0] == 255 )
					continue;

				// try to apply lightstyles
				if( (!(pass->flags & (GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK)) || (pass->flags & SHADERPASS_BLEND_MODULATE)) && (pass->rgbgen.type == RGB_GEN_IDENTITY) && (pass->alphagen.type == ALPHA_GEN_IDENTITY) ) {
					vec3_t colorSum, color;

					// the first pass is always GL_MODULATE or GL_REPLACE
					// other passes are GL_ADD
					r_lightmapPasses[0] = *pass;

					for( j = 0, l = 0, u = 0; j < MAX_LIGHTMAPS && r_superLightStyle->lightmapStyles[j] != 255; j++ ) {
						VectorCopy( r_lightStyles[r_superLightStyle->lightmapStyles[j]].rgb, colorSum );
						VectorClear( color );

						for( ; ; l++ ) {
							for( k = 0; k < 3; k++ ) {
								colorSum[k] -= color[k];
								color[k] = bound( 0, colorSum[k], 1 );
							}

							if( l ) {
								if( !color[0] && !color[1] && !color[2] )
									break;
								if( l == MAX_TEXTURE_UNITS+1 )
									r_lightmapPasses[0] = r_lightmapPasses[1];
								u = l % (MAX_TEXTURE_UNITS+1);
							}

							if( VectorCompare( color, colorWhite ) ) {
								r_lightmapPasses[u].rgbgen.type = RGB_GEN_IDENTITY;
							} else {
								if( !l ) {
									r_lightmapPasses[0].flags &= ~SHADERPASS_BLENDMODE;
									r_lightmapPasses[0].flags |= SHADERPASS_BLEND_MODULATE;
								}
								r_lightmapPasses[u].rgbgen.type = RGB_GEN_CONST;
								VectorCopy( color, r_lightmapPasses[u].rgbgen.args );
							}

							if( r_lightmap->integer && !l )
								R_SetupLightmapMode ();
							R_AccumulatePass( &r_lightmapPasses[u] );
							r_lightmapStyleNum[r_numAccumPasses - 1] = j;
						}
					}
				} else {
					if( r_lightmap->integer ) {
						R_SetupLightmapMode ();
						pass = r_lightmapPasses;
					}
					R_AccumulatePass( pass );
					r_lightmapStyleNum[r_numAccumPasses - 1] = 0;
				}
				continue;
			} else if( r_lightmap->integer && (r_currentShader->flags & SHADER_LIGHTMAP) )
				continue;
			if( (pass->flags & SHADERPASS_DETAIL) && !r_detailtextures->integer )
				continue;
			if( (pass->flags & SHADERPASS_DLIGHT) && !dlightBits )
				continue;
		}

		R_AccumulatePass( pass );
	}

	// accumulate dynamic lights pass and fog pass if any
	if( !r_lightmap->integer || !(r_currentShader->flags & SHADER_LIGHTMAP) ) {
		if( dlightBits && !(r_currentShader->flags & SHADER_NO_MODULATIVE_DLIGHTS) )
			R_AccumulatePass( &r_dlightsPass );

		if( r_texFog && r_texFog->shader ) {
			r_fogPass.anim_frames[0] = r_fogtexture;
			if( !r_currentShader->numpasses || r_currentShader->fog_dist || (r_currentShader->flags & SHADER_SKY) )
				r_fogPass.flags &= ~GLSTATE_DEPTHFUNC_EQ;
			else
				r_fogPass.flags |= GLSTATE_DEPTHFUNC_EQ;
			R_AccumulatePass( &r_fogPass );
		}
	}

	// flush any remaining passes
	if( r_numAccumPasses )
		R_RenderAccumulatedPasses ();

	R_ClearArrays ();

	qglMatrixMode( GL_MODELVIEW );
}

/*
================
R_BackendCleanUpTextureUnits
================
*/
void R_BackendCleanUpTextureUnits( void ) {
	R_CleanUpTextureUnits( 1 );

	GL_LoadIdentityTexMatrix ();
	qglMatrixMode( GL_MODELVIEW );

	GL_EnableTexGen( GL_S, 0 );
	GL_EnableTexGen( GL_T, 0 );
	GL_EnableTexGen( GL_R, 0 );
	GL_SetTexCoordArrayMode( 0 );
}

/*
================
R_BackendBeginTriangleOutlines
================
*/
void R_BackendBeginTriangleOutlines( void )
{
	r_triangleOutlines = qtrue;
	qglColor4fv( colorWhite );

	GL_Cull( 0 );
	GL_SetState( GLSTATE_NO_DEPTH_TEST );
	qglDisable( GL_TEXTURE_2D );
	qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
}

/*
================
R_BackendEndTriangleOutlines
================
*/
void R_BackendEndTriangleOutlines( void )
{
	r_triangleOutlines = qfalse;
	qglColor4fv( colorWhite );
	GL_SetState( 0 );
	qglEnable( GL_TEXTURE_2D );
	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

/*
================
R_SetColorForOutlines
================
*/
static inline void R_SetColorForOutlines( void )
{
	int type = r_currentMeshBuffer->sortkey & 3;

	switch( type ) {
		case MB_MODEL:
			if( r_currentMeshBuffer->infokey < 0 )
				qglColor4fv( colorRed );
			else
				qglColor4fv( colorWhite );
			break;
		case MB_SPRITE:
			qglColor4fv( colorBlue );
			break;
		case MB_POLY:
			qglColor4fv( colorGreen );
			break;
	}
}

/*
================
R_DrawTriangles
================
*/
static void R_DrawTriangles( void )
{
	if( r_showtris->integer == 2 )
		R_SetColorForOutlines ();

	if( glConfig.drawRangeElements )
		qglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVerts, numIndexes, GL_UNSIGNED_INT, indexesArray );
	else
		qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,	indexesArray );
}

/*
================
R_DrawNormals
================
*/
static void R_DrawNormals( void )
{
	int i;

	if( r_shownormals->integer == 2 )
		R_SetColorForOutlines ();

	qglBegin( GL_LINES );
	for( i = 0; i < numVerts; i++ ) {
		qglVertex3fv( vertsArray[i] );
		qglVertex3f( vertsArray[i][0] + normalsArray[i][0], 
			vertsArray[i][1] + normalsArray[i][1], 
			vertsArray[i][2] + normalsArray[i][2] );
	}
	qglEnd ();
}
