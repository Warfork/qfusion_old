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

index_t			*indexesArray;

#ifdef SHADOW_VOLUMES
int				*neighborsArray;
vec3_t			*trNormalsArray;
#endif

vec2_t			*coordsArray;
vec2_t			*lightmapCoordsArray;

#ifdef SHADOW_VOLUMES
vec4_t			vertexArray[MAX_ARRAY_VERTS*2];
#else
vec4_t			vertexArray[MAX_ARRAY_VERTS];
#endif

byte_vec4_t		colorArray[MAX_ARRAY_VERTS];
static	vec2_t	tUnitCoordsArray[MAX_TEXTURE_UNITS][MAX_ARRAY_VERTS];

vec3_t			normalsArray[MAX_ARRAY_VERTS];

vec4_t			tempVertexArray[MAX_ARRAY_VERTS];
vec3_t			tempNormalsArray[MAX_ARRAY_VERTS];
index_t			tempIndexesArray[MAX_ARRAY_INDEXES];

index_t			inIndexesArray[MAX_ARRAY_INDEXES];

#ifdef SHADOW_VOLUMES
int				inNeighborsArray[MAX_ARRAY_NEIGHBORS];
vec3_t			inTrNormalsArray[MAX_ARRAY_TRIANGLES];
#endif

vec2_t			inCoordsArray[MAX_ARRAY_VERTS];
vec2_t			inLightmapCoordsArray[MAX_ARRAY_VERTS];
byte_vec4_t		inColorsArray[MAX_ARRAY_VERTS];

int				numVerts, numIndexes, numColors;

qboolean		r_arrays_locked;
qboolean		r_blocked;
qboolean		r_triangleOutlines;

int				r_features;

static	int		r_lightmapTexNum;
static	shader_t *r_currentShader;
static	float	r_currentShaderTime;
static	unsigned int r_patchWidth, r_patchHeight;
static	mfog_t	*r_texFog, *r_colorFog;

static	int		r_texNums[SHADER_PASS_MAX];
static	int		r_numUnits;

index_t			*currentIndex;
int				*currentTrNeighbor;
float			*currentTrNormal;
float			*currentVertex;
float			*currentNormal;
float			*currentCoords;
float			*currentLightmapCoords;
qbyte			*currentColor;

static	int		r_identityLighting;

unsigned int	r_numverts;
unsigned int	r_numtris;
unsigned int	r_numflushes;

index_t			r_quad_indexes[6] = { 0, 1, 2, 0, 2, 3 };

void R_RenderFogOnMesh (void);
void R_DrawTriangles (void);
void R_DrawNormals (void);

/*
==============
R_BackendInit
==============
*/
void R_BackendInit (void)
{
	int i;
	double t;

	numVerts = 0;
	numIndexes = 0;
    numColors = 0;

	indexesArray = inIndexesArray;
	currentIndex = indexesArray;

#ifdef SHADOW_VOLUMES
	neighborsArray = inNeighborsArray;
	trNormalsArray = inTrNormalsArray;

	currentTrNeighbor = neighborsArray;
	currentTrNormal = trNormalsArray[0];
#endif

	currentVertex = vertexArray[0];
	currentNormal = normalsArray[0];

	coordsArray = inCoordsArray;
	lightmapCoordsArray = inLightmapCoordsArray;

	currentCoords = coordsArray[0];
	currentLightmapCoords = lightmapCoordsArray[0];

	currentColor = inColorsArray[0];

	r_arrays_locked = qfalse;
	r_blocked = qfalse;
	r_triangleOutlines = qfalse;

	qglVertexPointer( 3, GL_FLOAT, 16, vertexArray );	// padded for SIMD
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

	qglEnableClientState( GL_VERTEX_ARRAY );

	if ( !r_ignorehwgamma->value )
		r_identityLighting = (int)(255.0f / pow(2, max(0, floor(r_overbrightbits->value))));
	else
		r_identityLighting = 255;

	for ( i = 0; i < FTABLE_SIZE; i++ ) {
		t = (double)i / (double)FTABLE_SIZE;

		r_sintable[i] = sin ( t * M_TWOPI );

		if (t < 0.25) 
			r_triangletable[i] = t * 4.0;
		else if (t < 0.75)
			r_triangletable[i] = 2 - 4.0 * t;
		else
			r_triangletable[i] = (t - 0.75) * 4.0 - 1.0;

		if (t < 0.5) 
			r_squaretable[i] = 1.0f;
		else
			r_squaretable[i] = -1.0f;

		r_sawtoothtable[i] = t;
		r_inversesawtoothtable[i] = 1.0 - t;
	}
}

/*
==============
R_BackendShutdown
==============
*/
void R_BackendShutdown (void)
{
}

/*
==============
R_FastSin
==============
*/
float R_FastSin ( float t ) {
	return FTABLE_EVALUATE ( r_sintable, t );
}

/*
==============
R_TableForFunc
==============
*/
static float *R_TableForFunc ( unsigned int func )
{
	switch ( func )
	{
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
	Com_Error ( ERR_DROP, "R_TableForFunc: unknown function" );

	return NULL;
}

/*
==============
R_BackendStartFrame
==============
*/
void R_BackendStartFrame (void)
{
	static int prevupdate;
	static int rupdate = 300;

	r_numverts = 0;
	r_numtris = 0;
	r_numflushes = 0;

	if ( prevupdate > (curtime % rupdate) ) {
		int i, j, k;
		float t;

		j = random()*(FTABLE_SIZE/4);
		k = random()*(FTABLE_SIZE/2);

		for ( i = 0; i < FTABLE_SIZE; i++ ) {
			if ( i >= j && i < (j+k) ) {
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
void R_BackendEndFrame (void)
{
	if (r_speeds->value && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		Com_Printf( "%4i wpoly %4i leafs %4i verts %4i tris %4i flushes\n",
			c_brush_polys, 
			c_world_leafs,
			r_numverts,
			r_numtris,
			r_numflushes ); 
	}
}

/*
==============
R_LockArrays
==============
*/
void R_LockArrays ( int numverts )
{
	if ( r_arrays_locked )
		return;

	if ( qglLockArraysEXT != 0 ) {
		qglLockArraysEXT( 0, numverts );
		r_arrays_locked = qtrue;
	}
}

/*
==============
R_UnlockArrays
==============
*/
void R_UnlockArrays (void)
{
	if ( !r_arrays_locked )
		return;

	if ( qglUnlockArraysEXT != 0 ) {
		qglUnlockArraysEXT();
		r_arrays_locked = qfalse;
	}
}

/*
==============
R_DrawTriangleStrips

This function looks for and sends tristrips.
Original code by Stephen C. Taylor (Aftershock 3D rendering engine)
==============
*/
void R_DrawTriangleStrips (index_t *indexes, int numindexes)
{
	int toggle;
	index_t a, b, c, *index;

	c = 0;
	index = indexes;
	while ( c < numindexes ) {
		toggle = 1;

		qglBegin( GL_TRIANGLE_STRIP );
		
		qglArrayElement( index[0] );
		qglArrayElement( b = index[1] );
		qglArrayElement( a = index[2] );

		c += 3;
		index += 3;

		while ( c < numindexes ) {
			if ( a != index[0] || b != index[1] ) {
				break;
			}

			if ( toggle ) {
				qglArrayElement( b = index[2] );
			} else {
				qglArrayElement( a = index[2] );
			}

			c += 3;
			index += 3;
			toggle = !toggle;
		}

		qglEnd();
    }
}

/*
==============
R_ClearArrays
==============
*/
void R_ClearArrays (void)
{
	numVerts = 0;
	numIndexes = 0;

	indexesArray = inIndexesArray;
	currentIndex = indexesArray;
#ifdef SHADOW_VOLUMES
	neighborsArray = inNeighborsArray;
	trNormalsArray = inTrNormalsArray;

	currentTrNeighbor = neighborsArray;
	currentTrNormal = trNormalsArray[0];
#endif

	currentVertex = vertexArray[0];
	currentNormal = normalsArray[0];

	R_ResetTexState ();

	r_blocked = qfalse;
}

/*
==============
R_FlushArrays
==============
*/
void R_FlushArrays (void)
{
	if ( !numVerts || !numIndexes ) {
		return;
	}

	if ( numColors > 1 ) {
		qglEnableClientState( GL_COLOR_ARRAY );
	} else if ( numColors == 1 ) {
		qglColor4ubv ( colorArray[0] );
	}

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	if ( !r_arrays_locked ) {
		R_DrawTriangleStrips ( indexesArray, numIndexes );
	} else {
		qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,	indexesArray );
	}

	r_numtris += numIndexes / 3;

	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	if ( numColors > 1 ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	}

	r_numflushes++;
}

/*
==============
R_FlushArraysMtex
==============
*/
void R_FlushArraysMtex (void)
{
	int i;

	if ( !numVerts || !numIndexes ) {
		return;
	}

	GL_MBind( GL_TEXTURE_0, r_texNums[0] );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	if ( numColors > 1 ) {
		qglEnableClientState( GL_COLOR_ARRAY );
	} else if ( numColors == 1 ) {
		qglColor4ubv ( colorArray[0] );
	}

	for ( i = 1; i < r_numUnits; i++ )
	{
		GL_MBind( GL_TEXTURE_0 + i, r_texNums[i] );
		qglEnable ( GL_TEXTURE_2D );
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	if ( !r_arrays_locked ) {
		R_DrawTriangleStrips ( indexesArray, numIndexes );
	} else {
		qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT,	indexesArray );
	}

	r_numtris += numIndexes / 3;

	for ( i = r_numUnits - 1; i >= 0; i-- )
	{
		GL_SelectTexture ( GL_TEXTURE_0 + i );
		if ( i ) {
			qglDisable ( GL_TEXTURE_2D );
		}
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	if ( numColors > 1 ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	}

	r_numflushes++;
}

/*
================
R_DeformVertices
================
*/
void R_DeformVertices (void)
{
	int i, j, k, p;
	float args[4], deflect;
	float *quad[4], *table;
	deformv_t *deformv;
	vec3_t tv, rot_centre;

	deformv = &r_currentShader->deforms[0];
	for (i = 0; i < r_currentShader->numdeforms; i++, deformv++)
	{
		switch (deformv->type)
		{
			case DEFORMV_NONE:
				break;

			case DEFORMV_WAVE:
				args[0] = deformv->func.args[0];
				args[1] = deformv->func.args[1];
				args[3] = deformv->func.args[2] + deformv->func.args[3] * r_currentShaderTime;
				table = R_TableForFunc ( deformv->func.type );

				for ( j = 0; j < numVerts; j++ ) {
					deflect = deformv->args[0] * (vertexArray[j][0]+vertexArray[j][1]+vertexArray[j][2]) + args[3];
					deflect = FTABLE_EVALUATE ( table, deflect ) * args[1] + args[0];

					// Deflect vertex along its normal by wave amount
					VectorMA ( vertexArray[j], deflect, normalsArray[j], vertexArray[j] );
				}
				break;

			case DEFORMV_NORMAL:
				args[0] = deformv->args[1] * r_currentShaderTime;

				for ( j = 0; j < numVerts; j++ ) {
					args[1] = normalsArray[j][2] * args[0];

					deflect = deformv->args[0] * R_FastSin ( args[1] );
					normalsArray[j][0] *= deflect;
					deflect = deformv->args[0] * R_FastSin ( args[1] + 0.25 );
					normalsArray[j][1] *= deflect;
					VectorNormalizeFast ( normalsArray[j] );
				}
				break;

			case DEFORMV_MOVE:
				table = R_TableForFunc ( deformv->func.type );
				deflect = deformv->func.args[2] + r_currentShaderTime * deformv->func.args[3];
				deflect = FTABLE_EVALUATE (table, deflect) * deformv->func.args[1] + deformv->func.args[0];

				for ( j = 0; j < numVerts; j++ )
					VectorMA ( vertexArray[j], deflect, deformv->args, vertexArray[j] );
				break;

			case DEFORMV_BULGE:
				args[0] = deformv->args[0] / (float)r_patchHeight;
				args[1] = deformv->args[1];
				args[2] = r_currentShaderTime / (deformv->args[2]*r_patchWidth);

				for ( k = 0, p = 0; k < r_patchHeight; k++ ) {
					deflect = R_FastSin ( (float)k * args[0] + args[2] ) * args[1];

					for ( j = 0; j < r_patchWidth; j++, p++ )
						VectorMA ( vertexArray[p], deflect, normalsArray[p], vertexArray[p] );
				}
				break;

			case DEFORMV_AUTOSPRITE:
				if ( numIndexes < 6 )
					break;

				for ( k = 0; k < numIndexes; k += 6 )
				{
					mat3_t m0, m1, m2, result;

					quad[0] = (float *)(vertexArray + indexesArray[k+0]);
					quad[1] = (float *)(vertexArray + indexesArray[k+1]);
					quad[2] = (float *)(vertexArray + indexesArray[k+2]);

					for ( j = 2; j >= 0; j-- ) {
						quad[3] = (float *)(vertexArray + indexesArray[k+3+j]);

						if ( !VectorCompare (quad[3], quad[0]) && 
							!VectorCompare (quad[3], quad[1]) &&
							!VectorCompare (quad[3], quad[2]) ) {
							break;
						}
					}

					VectorSubtract ( quad[0], quad[1], m0[0] );
					VectorSubtract ( quad[2], quad[1], m0[1] );
					CrossProduct ( m0[0], m0[1], m0[2] );
					VectorNormalizeFast ( m0[2] );
					MakeNormalVectors ( m0[2], m0[1], m0[0] );

					if ( currententity && (currentmodel != r_worldmodel) ) {
						Matrix4_Matrix3 ( r_modelview_matrix, m1 );
					} else {
						Matrix4_Matrix3 ( r_worldview_matrix, m1 );
					}

					Matrix3_Transpose ( m1, m2 );
					Matrix3_Multiply ( m2, m0, result );

					for ( j = 0; j < 3; j++ )
						rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

					for ( j = 0; j < 4; j++ ) {
						VectorSubtract ( quad[j], rot_centre, tv );
						Matrix3_Multiply_Vec3 ( result, tv, quad[j] );
						VectorAdd ( rot_centre, quad[j], quad[j] );
					}
				}
				break;

			case DEFORMV_AUTOSPRITE2:
				if ( numIndexes < 6 )
					break;

				for ( k = 0; k < numIndexes; k += 6 )
				{
					int long_axis, short_axis;
					vec3_t axis, tmp;
					float len[3];
					mat3_t m0, m1, m2, result;

					quad[0] = (float *)(vertexArray + indexesArray[k+0]);
					quad[1] = (float *)(vertexArray + indexesArray[k+1]);
					quad[2] = (float *)(vertexArray + indexesArray[k+2]);

					for ( j = 2; j >= 0; j-- ) {
						quad[3] = (float *)(vertexArray + indexesArray[k+3+j]);

						if ( !VectorCompare (quad[3], quad[0]) && 
							!VectorCompare (quad[3], quad[1]) &&
							!VectorCompare (quad[3], quad[2]) ) {
							break;
						}
					}

					// build a matrix were the longest axis of the billboard is the Y-Axis
					VectorSubtract ( quad[1], quad[0], m0[0] );
					VectorSubtract ( quad[2], quad[0], m0[1] );
					VectorSubtract ( quad[2], quad[1], m0[2] );
					len[0] = DotProduct ( m0[0], m0[0] );
					len[1] = DotProduct ( m0[1], m0[1] );
					len[2] = DotProduct ( m0[2], m0[2] );

					if ( (len[2] > len[1]) && (len[2] > len[0]) ) {
						if ( len[1] > len[0] ) {
							long_axis = 1;
							short_axis = 0;
						} else {
							long_axis = 0;
							short_axis = 1;
						}
					} else if ( (len[1] > len[2]) && (len[1] > len[0]) ) {
						if ( len[2] > len[0] ) {
							long_axis = 2;
							short_axis = 0;
						} else {
							long_axis = 0;
							short_axis = 2;
						}
					} else if ( (len[0] > len[1]) && (len[0] > len[2]) ) {
						if ( len[2] > len[1] ) {
							long_axis = 2;
							short_axis = 1;
						} else {
							long_axis = 1;
							short_axis = 2;
						}
					}

					if ( DotProduct (m0[long_axis], m0[short_axis]) ) {
						VectorNormalize2 ( m0[long_axis], axis );
						VectorCopy ( axis, m0[1] );

						if ( axis[0] || axis[1] ) {
							MakeNormalVectors ( m0[1], m0[0], m0[2] );
						} else {
							MakeNormalVectors ( m0[1], m0[2], m0[0] );
						}
					} else {
						VectorNormalize2 ( m0[long_axis], axis );
						VectorNormalize2 ( m0[short_axis], m0[0] );
						VectorCopy ( axis, m0[1] );
						CrossProduct ( m0[0], m0[1], m0[2] );
					}

					for ( j = 0; j < 3; j++ )
						rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

					if ( currententity && (currentmodel != r_worldmodel) ) {
						VectorAdd ( currententity->origin, rot_centre, tv );
						VectorSubtract ( r_origin, tv, tmp );
						Matrix3_Multiply_Vec3 ( currententity->axis, tmp, tv );
					} else {
						VectorCopy ( rot_centre, tv );
						VectorSubtract ( r_origin, tv, tv );
					}

					// filter any longest-axis-parts off the camera-direction
					deflect = -DotProduct ( tv, axis );

					VectorMA ( tv, deflect, axis, m1[2] );
					VectorNormalizeFast ( m1[2] );
					VectorCopy ( axis, m1[1] );
					CrossProduct ( m1[1], m1[2], m1[0] );

					Matrix3_Transpose ( m1, m2 );
					Matrix3_Multiply ( m2, m0, result );

					for ( j = 0; j < 4; j++ ) {
						VectorSubtract ( quad[j], rot_centre, tv );
						Matrix3_Multiply_Vec3 ( result, tv, quad[j] );
						VectorAdd ( rot_centre, quad[j], quad[j] );
					}
				}
				break;

			case DEFORMV_PROJECTION_SHADOW:
				break;

			case DEFORMV_AUTOPARTICLE:
				if ( numIndexes < 6 )
					break;

				for ( k = 0; k < numIndexes; k += 6 )
				{
					float scale;
					mat3_t m0, m1, m2, result;

					quad[0] = (float *)(vertexArray + indexesArray[k+0]);
					quad[1] = (float *)(vertexArray + indexesArray[k+1]);
					quad[2] = (float *)(vertexArray + indexesArray[k+2]);

					for ( j = 2; j >= 0; j-- ) {
						quad[3] = (float *)(vertexArray + indexesArray[k+3+j]);

						if ( !VectorCompare (quad[3], quad[0]) && 
							!VectorCompare (quad[3], quad[1]) &&
							!VectorCompare (quad[3], quad[2]) ) {
							break;
						}
					}

					VectorSubtract ( quad[0], quad[1], m0[0] );
					VectorSubtract ( quad[2], quad[1], m0[1] );
					CrossProduct ( m0[0], m0[1], m0[2] );
					VectorNormalizeFast ( m0[2] );
					MakeNormalVectors ( m0[2], m0[1], m0[0] );

					if ( currententity && (currentmodel != r_worldmodel) ) {
						Matrix4_Matrix3 ( r_modelview_matrix, m1 );
					} else {
						Matrix4_Matrix3 ( r_worldview_matrix, m1 );
					}

					Matrix3_Transpose ( m1, m2 );
					Matrix3_Multiply ( m2, m0, result );

					// hack a scale up to keep particles from disappearing
					scale = (quad[0][0] - r_origin[0]) * vpn[0] + (quad[0][1] - r_origin[1]) * vpn[1] + (quad[0][2] - r_origin[2]) * vpn[2];

					if ( scale < 20 ) {
						scale = 1.5;
					} else {
						scale = 1.5 + scale * 0.006f;
					}

					for ( j = 0; j < 3; j++ )
						rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

					for ( j = 0; j < 4; j++ ) {
						VectorSubtract ( quad[j], rot_centre, tv );
						Matrix3_Multiply_Vec3 ( result, tv, quad[j] );
						VectorMA ( rot_centre, scale, quad[j], quad[j] );
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
void R_VertexTCBase ( shaderpass_t *pass, int unit )
{
	int	i;
	float *outCoords, depth;
	vec3_t transform;
	vec3_t n, projection;
	mat3_t inverse_axis;

	outCoords = tUnitCoordsArray[unit][0];
	qglTexCoordPointer( 2, GL_FLOAT, 0, outCoords );

	if ( pass->tcgen == TC_GEN_BASE ) {
		memcpy ( outCoords, coordsArray[0], sizeof(float) * 2 * numVerts );
	} else if ( pass->tcgen == TC_GEN_LIGHTMAP ) {
		memcpy ( outCoords, lightmapCoordsArray[0], sizeof(float) * 2 * numVerts );
	} else if ( pass->tcgen == TC_GEN_ENVIRONMENT ) {
		if ( !currentmodel ) {
			return;
		} else if ( currentmodel == r_worldmodel ) {
			VectorSubtract ( vec3_origin, r_origin, transform );
		} else if ( currentmodel->type == mod_brush ) {
			VectorSubtract ( currententity->origin, r_origin, transform );
			Matrix3_Transpose ( currententity->axis, inverse_axis );
		} else {
			VectorSubtract ( currententity->origin, r_origin, transform );
			Matrix3_Transpose ( currententity->axis, inverse_axis );
		}

		for ( i = 0; i < numVerts; i++, outCoords += 2 ) {
			VectorAdd ( vertexArray[i], transform, projection );
			VectorNormalize ( projection );

			// project vector
			if ( currentmodel == r_worldmodel ) {
				n[0] = normalsArray[i][0];
				n[1] = normalsArray[i][1];
				n[2] = normalsArray[i][2];
			} else {
				n[0] = DotProduct ( normalsArray[i], inverse_axis[0] );
				n[1] = DotProduct ( normalsArray[i], inverse_axis[1] );
				n[2] = DotProduct ( normalsArray[i], inverse_axis[2] );
			}

			depth = -2.0f * DotProduct ( n, projection );
			VectorMA ( projection, depth, n, projection );
			depth = Q_RSqrt ( DotProduct(projection, projection) * 4 );

			outCoords[0] = -((projection[1] * depth) + 0.5f);
			outCoords[1] =  ((projection[2] * depth) + 0.5f);
		}
	} else if ( pass->tcgen == TC_GEN_VECTOR ) {
		for ( i = 0; i < numVerts; i++, outCoords += 2 ) {
			outCoords[0] = DotProduct ( pass->tcgenVec[0], vertexArray[i] ) + pass->tcgenVec[0][3];
			outCoords[1] = DotProduct ( pass->tcgenVec[1], vertexArray[i] ) + pass->tcgenVec[1][3];
		}
	}
}

/*
==============
R_ShaderpassTex
==============
*/
int R_ShaderpassTex ( shaderpass_t *pass )
{
	if ( pass->flags & SHADER_PASS_ANIMMAP ) {
		return pass->anim_frames[(int)(pass->anim_fps * r_currentShaderTime) % pass->anim_numframes]->texnum;
	} else if ( (pass->flags & SHADER_PASS_LIGHTMAP) && r_lightmapTexNum >= 0 ) {
		return gl_state.lightmap_textures + r_lightmapTexNum;
	}

	return pass->anim_frames[0] ? pass->anim_frames[0]->texnum : r_notexture->texnum;
}

/*
================
R_ModifyTextureCoords
================
*/
void R_ModifyTextureCoords ( shaderpass_t *pass, int unit )
{
	int i, j;
	float *table;
	float t1, t2, sint, cost;
	float *tcArray;
	tcmod_t	*tcmod;

	r_texNums[unit] = R_ShaderpassTex ( pass );

	// we're smart enough not to copy data and simply switch the pointer
	if ( !pass->numtcmods ) {
		if ( pass->tcgen == TC_GEN_BASE ) {
			qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArray );
		} else if ( pass->tcgen == TC_GEN_LIGHTMAP ) {
			qglTexCoordPointer( 2, GL_FLOAT, 0, lightmapCoordsArray );
		} else {
			R_VertexTCBase ( pass, unit );
		}
		return;
	}

	R_VertexTCBase ( pass, unit );

	for (i = 0, tcmod = pass->tcmods; i < pass->numtcmods; i++, tcmod++)
	{
		tcArray = tUnitCoordsArray[unit][0];

		switch (tcmod->type)
		{
			case SHADER_TCMOD_ROTATE:
				cost = tcmod->args[0] * r_currentShaderTime;
				sint = R_FastSin( cost );
				cost = R_FastSin( cost + 0.25 );

				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					t1 = tcArray[0];
					t2 = tcArray[1];
					tcArray[0] = cost * (t1 - 0.5f) - sint * (t2 - 0.5f) + 0.5f;
					tcArray[1] = cost * (t2 - 0.5f) + sint * (t1 - 0.5f) + 0.5f;
				}
				break;

			case SHADER_TCMOD_SCALE:
				t1 = tcmod->args[0];
				t2 = tcmod->args[1];

				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					tcArray[0] = tcArray[0] * t1;
					tcArray[1] = tcArray[1] * t2;
				}
				break;

			case SHADER_TCMOD_TURB:
				t1 = tcmod->args[1];
				t2 = tcmod->args[2] + r_currentShaderTime * tcmod->args[3];

				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					tcArray[0] = tcArray[0] + t1 * R_FastSin (tcArray[0]*t1+t2);
					tcArray[1] = tcArray[1] + t1 * R_FastSin (tcArray[1]*t1+t2);
				}
				break;

			case SHADER_TCMOD_STRETCH:
				table = R_TableForFunc ( tcmod->args[0] );
				t2 = tcmod->args[3] + r_currentShaderTime * tcmod->args[4];
				t1 = FTABLE_EVALUATE ( table, t2 ) * tcmod->args[2] + tcmod->args[1];
				t1 = t1 ? 1.0f / t1 : 1.0f;
				t2 = 0.5f - 0.5f * t1;

				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					tcArray[0] = tcArray[0] * t1 + t2;
					tcArray[1] = tcArray[1] * t1 + t2;
				}
				break;

			case SHADER_TCMOD_SCROLL:
				t1 = tcmod->args[0] * r_currentShaderTime; t1 = t1 - floor ( t1 );
				t2 = tcmod->args[1] * r_currentShaderTime; t2 = t2 - floor ( t2 );

				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					tcArray[0] = tcArray[0] + t1;
					tcArray[1] = tcArray[1] + t2;
				}
				break;

			case SHADER_TCMOD_TRANSFORM:
				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					t1 = tcArray[0];
					t2 = tcArray[1];
					tcArray[0] = t1 * tcmod->args[0] + t2 * tcmod->args[2] + tcmod->args[4];
					tcArray[1] = t2 * tcmod->args[1] + t1 * tcmod->args[3] + tcmod->args[5];
				}
				break;

			default:
				break;
		}
	}
}


/*
================
R_ModifyColor
================
*/
void R_ModifyColor ( shaderpass_t *pass )
{
	int i;
	float *table, c, a;
	vec3_t t, v;
	int r, g, b;
	qbyte *bArray, *vArray;
	qboolean noArray;
	shaderfunc_t *rgbgenfunc, *alphagenfunc;

	noArray = ( pass->flags & SHADER_PASS_NOCOLORARRAY ) && !r_colorFog;
	rgbgenfunc = &pass->rgbgen.func;
	alphagenfunc = &pass->alphagen.func;

	if ( noArray ) {
		numColors = 1;
	} else {
		numColors = numVerts;
	}

	bArray = colorArray[0];
	vArray = inColorsArray[0];

	switch (pass->rgbgen.type)
	{
		case RGB_GEN_IDENTITY:
			memset ( bArray, 255, sizeof(byte_vec4_t)*numColors );
			break;

		case RGB_GEN_IDENTITY_LIGHTING:
			memset ( bArray, r_identityLighting, sizeof(byte_vec4_t)*numColors );
			break;

		case RGB_GEN_CONST:
			r = R_FloatToByte ( pass->rgbgen.args[0] );
			g = R_FloatToByte ( pass->rgbgen.args[1] );
			b = R_FloatToByte ( pass->rgbgen.args[2] );

			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[0] = r; bArray[1] = g; bArray[2] = b;
			}
			break;

		case RGB_GEN_COLORWAVE:
			table = R_TableForFunc ( rgbgenfunc->type );
			c = rgbgenfunc->args[2] + r_currentShaderTime * rgbgenfunc->args[3];
			c = FTABLE_EVALUATE ( table, c ) * rgbgenfunc->args[1] + rgbgenfunc->args[0];
			r = R_FloatToByte ( bound (0, pass->rgbgen.args[0] * c, 1) );
			g = R_FloatToByte ( bound (0, pass->rgbgen.args[1] * c, 1) );
			b = R_FloatToByte ( bound (0, pass->rgbgen.args[2] * c, 1) );

			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[0] = r; bArray[1] = g; bArray[2] = b;
			}
			break;

		case RGB_GEN_ENTITY:
			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				*(int *)bArray = *(int *)currententity->color;
			}
			break;

		case RGB_GEN_ONE_MINUS_ENTITY:
			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[0] = 255 - currententity->color[0];
				bArray[1] = 255 - currententity->color[1];
				bArray[2] = 255 - currententity->color[2];
			}
			break;

		case RGB_GEN_VERTEX:
		case RGB_GEN_EXACT_VERTEX:
			memcpy ( bArray, vArray, sizeof(byte_vec4_t)*numColors );
			break;

		case RGB_GEN_ONE_MINUS_VERTEX:
			for ( i = 0; i < numColors; i++, bArray += 4, vArray += 4 ) {
				bArray[0] = 255 - vArray[0];
				bArray[1] = 255 - vArray[1];
				bArray[2] = 255 - vArray[2];
			}
			break;

		case RGB_GEN_LIGHTING_DIFFUSE:
			if ( !currententity ) {
				memset ( bArray, 255, sizeof(byte_vec4_t)*numColors );
			} else {
				R_LightForEntity ( currententity, bArray );
			}
			break;

		default:
			break;
	}

	bArray = colorArray[0];
	vArray = inColorsArray[0];

	switch (pass->alphagen.type)
	{
		case ALPHA_GEN_IDENTITY:
			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[3] = 255;
			}
			break;

		case ALPHA_GEN_CONST:
			b = R_FloatToByte ( pass->alphagen.args[0] );

			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[3] = b;
			}
			break;

		case ALPHA_GEN_WAVE:
			table = R_TableForFunc ( alphagenfunc->type );
			a = alphagenfunc->args[2] + r_currentShaderTime * alphagenfunc->args[3];
			a = FTABLE_EVALUATE ( table, a ) * alphagenfunc->args[1] + alphagenfunc->args[0];
			b = R_FloatToByte ( bound (0.0f, a, 1.0f) );

			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[3] = b;
			}
			break;

		case ALPHA_GEN_PORTAL:
			VectorAdd ( vertexArray[0], currententity->origin, v );
			VectorSubtract ( r_origin, v, t );
			a = VectorLength ( t ) * pass->alphagen.args[0];
			clamp ( a, 0.0f, 1.0f );
			b = R_FloatToByte ( a );

			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[3] = b;
			}
			break;

		case ALPHA_GEN_VERTEX:
			for ( i = 0; i < numColors; i++, bArray += 4, vArray += 4 ) {
				bArray[3] = vArray[3];
			}
			break;

		case ALPHA_GEN_ENTITY:
			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				bArray[3] = currententity->color[3];
			}
			break;

		case ALPHA_GEN_SPECULAR:
			VectorSubtract ( r_origin, currententity->origin, t );

			if ( !Matrix3_Compare (currententity->axis, axis_identity) ) {
				Matrix3_Multiply_Vec3 ( currententity->axis, t, v );
			} else {
				VectorCopy ( t, v );
			}

			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				VectorSubtract ( v, vertexArray[i], t );
				a = DotProduct( t, normalsArray[i] ) * Q_RSqrt ( DotProduct(t,t) );
				a = a * a * a * a * a;
				bArray[3] = R_FloatToByte ( bound (0.0f, a, 1.0f) );
			}
			break;

		case ALPHA_GEN_DOT:
			if ( !Matrix3_Compare (currententity->axis, axis_identity) ) {
				Matrix3_Multiply_Vec3 ( currententity->axis, vpn, v );
			} else {
				VectorCopy ( vpn, v );
			}

			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				a = DotProduct( v, normalsArray[i] ); if ( a < 0 ) a = -a;
				bArray[3] = R_FloatToByte ( bound (pass->alphagen.args[0], a, pass->alphagen.args[1]) );
			}
			break;

		case ALPHA_GEN_ONE_MINUS_DOT:
			if ( !Matrix3_Compare (currententity->axis, axis_identity) ) {
				Matrix3_Multiply_Vec3 ( currententity->axis, vpn, v );
			} else {
				VectorCopy ( vpn, v );
			}

			for ( i = 0; i < numColors; i++, bArray += 4 ) {
				a = DotProduct( v, normalsArray[i] ); if ( a < 0 ) a = -a; a = 1.0f - a;
				bArray[3] = R_FloatToByte ( bound (pass->alphagen.args[0], a, pass->alphagen.args[1]) );
			}
			break;
	}
	
	if ( r_colorFog ) {
		float dist, vdist;
		cplane_t *fogplane;
		vec3_t diff, viewtofog, fog_vpn;

		fogplane = r_colorFog->visibleplane;
		dist = PlaneDiff ( r_origin, fogplane );

		if ( r_currentShader->flags & SHADER_SKY ) 
		{
			if ( dist > 0 )
				VectorScale( fogplane->normal, -dist, viewtofog );
			else
				VectorClear( viewtofog );
		}
		else
		{
			VectorCopy ( currententity->origin, viewtofog );
		}

		VectorScale ( vpn, r_colorFog->shader->fog_dist, fog_vpn );

		bArray = colorArray[0];
		for ( i = 0; i < numColors; i++, bArray += 4 )
		{
			VectorAdd ( vertexArray[i], viewtofog, diff );

			// camera is inside the fog
			if ( dist < 0 ) {
				VectorSubtract ( diff, r_origin, diff );

				c = DotProduct ( diff, fog_vpn );
				a = (1.0f - bound ( 0, c, 1.0f )) * (1.0 / 255.0);
			} else {
				vdist = PlaneDiff ( diff, fogplane );

				if ( vdist < 0 ) {
					VectorSubtract ( diff, r_origin, diff );

					c = vdist / ( vdist - dist );
					c *= DotProduct ( diff, fog_vpn );
					a = (1.0f - bound ( 0, c, 1.0f )) * (1.0 / 255.0);
				} else {
					a = 1.0 / 255.0;
				}
			}

			if ( pass->blendmode == GL_ADD || 
				((pass->blendsrc == GL_ZERO) && (pass->blenddst == GL_ONE_MINUS_SRC_COLOR)) ) {
				bArray[0] = R_FloatToByte ( (float)bArray[0]*a );
				bArray[1] = R_FloatToByte ( (float)bArray[1]*a );
				bArray[2] = R_FloatToByte ( (float)bArray[2]*a );
			} else {
				bArray[3] = R_FloatToByte ( (float)bArray[3]*a );
			}
		}
	}
}

/*
================
R_SetCurrentShaderState
================
*/
void R_SetCurrentShaderState (void)
{
// Face culling
	if ( !gl_cull->value || (r_features & MF_NOCULL) ) {
		qglDisable ( GL_CULL_FACE );
	} else {
		if ( r_currentShader->flags & SHADER_CULL_FRONT ) {
			qglEnable ( GL_CULL_FACE );
			qglCullFace ( GL_FRONT );
		} else if ( r_currentShader->flags & SHADER_CULL_BACK ) {
			qglEnable ( GL_CULL_FACE );
			qglCullFace ( GL_BACK );
		} else {
			qglDisable ( GL_CULL_FACE );
		}
	}

	if ( r_currentShader->flags & SHADER_POLYGONOFFSET ) {
		qglEnable ( GL_POLYGON_OFFSET_FILL );
	} else {
		qglDisable ( GL_POLYGON_OFFSET_FILL );
	}
}

/*
================
R_SetShaderpassState
================
*/
void R_SetShaderpassState ( shaderpass_t *pass, qboolean mtex )
{
	if ( (mtex && (pass->blendmode != GL_REPLACE)) || (pass->flags & SHADER_PASS_BLEND) ) {
		qglEnable ( GL_BLEND );
		qglBlendFunc ( pass->blendsrc, pass->blenddst );
	} else {
		qglDisable ( GL_BLEND );
	}

	if ( pass->flags & SHADER_PASS_ALPHAFUNC ) {
		qglEnable ( GL_ALPHA_TEST );

		if ( pass->alphafunc == SHADER_ALPHA_GT0 ) {
			qglAlphaFunc ( GL_GREATER, 0 );
		} else if ( pass->alphafunc == SHADER_ALPHA_LT128 ) {
			qglAlphaFunc ( GL_LESS, 0.5f );
		} else if ( pass->alphafunc == SHADER_ALPHA_GE128 ) {
			qglAlphaFunc ( GL_GEQUAL, 0.5f );
		}
	} else {
		qglDisable ( GL_ALPHA_TEST );
	}

	// nasty hack!!!
	if ( !gl_state.in2d ) {
		qglDepthFunc ( pass->depthfunc );

		if ( pass->flags & SHADER_PASS_DEPTHWRITE ) {
			qglDepthMask ( GL_TRUE );
		} else {
			qglDepthMask ( GL_FALSE );
		}
	}
}

/*
================
R_RenderMeshGeneric
================
*/
void R_RenderMeshGeneric ( shaderpass_t *pass )
{
	R_SetShaderpassState ( pass, qfalse );
	R_ModifyTextureCoords ( pass, 0 );
	R_ModifyColor ( pass );

	if ( pass->blendmode == GL_REPLACE )
		GL_TexEnv( GL_REPLACE );
	else
		GL_TexEnv ( GL_MODULATE );
	GL_Bind ( r_texNums[0] );

	R_FlushArrays ();
}

/*
================
R_RenderMeshMultitextured
================
*/
void R_RenderMeshMultitextured ( shaderpass_t *pass )
{
	int	i;

	R_SetShaderpassState ( pass, qtrue );
	R_ModifyColor ( pass );
	R_ModifyTextureCoords ( pass, 0 );

	GL_SelectTexture( GL_TEXTURE_0 );
	GL_TexEnv( GL_MODULATE );

	for ( i = 1, pass++; i < r_numUnits; i++, pass++ )
	{
		GL_SelectTexture( GL_TEXTURE_0 + i );
		GL_TexEnv( pass->blendmode );
		R_ModifyTextureCoords ( pass, i );
	}

	R_FlushArraysMtex ();
}

/*
================
R_RenderMeshCombined
================
*/
void R_RenderMeshCombined ( shaderpass_t *pass )
{
	int	i;

	R_SetShaderpassState ( pass, qtrue );
	R_ModifyColor ( pass );
	R_ModifyTextureCoords ( pass, 0 );

	GL_SelectTexture( GL_TEXTURE_0 );
	GL_TexEnv( GL_MODULATE );

	for ( i = 1, pass++; i < r_numUnits; i++, pass++ )
	{
		GL_SelectTexture( GL_TEXTURE_0 + i );

		if ( pass->blendmode )
		{
			switch ( pass->blendmode )
			{
			case GL_REPLACE:
			case GL_MODULATE:
				GL_TexEnv (GL_MODULATE);
				break;

			case GL_ADD:
				// these modes are best set with TexEnv, Combine4 would need much more setup
				GL_TexEnv (pass->blendmode);
				break;

			case GL_DECAL:
				// mimics Alpha-Blending in upper texture stage, but instead of multiplying the alpha-channel, they´re added
				// this way it can be possible to use GL_DECAL in both texture-units, while still looking good
				// normal mutlitexturing would multiply the alpha-channel which looks ugly
				GL_TexEnv (GL_COMBINE_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_INTERPOLATE_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_ADD);

				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);
							
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_SRC_ALPHA);

				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_TEXTURE);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_SRC_ALPHA);
				break;
			}
		} else {
			GL_TexEnv (GL_COMBINE4_NV);
			qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_ADD);
			qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_ADD);

			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);

			switch ( pass->blendsrc )
			{
			case GL_ONE:
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_ZERO);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_ONE_MINUS_SRC_COLOR);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_ZERO);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_ONE_MINUS_SRC_ALPHA);
				break;
			case GL_ZERO:
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_ZERO);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_ZERO);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_SRC_ALPHA);
				break;
			case GL_DST_COLOR:
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_SRC_ALPHA);
				break;
			case GL_ONE_MINUS_DST_COLOR:
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_ONE_MINUS_SRC_COLOR);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_ONE_MINUS_SRC_ALPHA);
				break;
			case GL_SRC_ALPHA:
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_ALPHA);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_TEXTURE);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_SRC_ALPHA);
				break;
			case GL_ONE_MINUS_SRC_ALPHA:
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_ONE_MINUS_SRC_ALPHA);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_TEXTURE);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_ONE_MINUS_SRC_ALPHA);
				break;
			case GL_DST_ALPHA:
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_ALPHA);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_SRC_ALPHA);
				break;
			case GL_ONE_MINUS_DST_ALPHA:
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_ONE_MINUS_SRC_ALPHA);
				qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_EXT, GL_ONE_MINUS_SRC_ALPHA);
				break;
			}

			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_PREVIOUS_EXT);
			qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_SRC_COLOR);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_EXT, GL_PREVIOUS_EXT);	
			qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_EXT, GL_SRC_ALPHA);

			switch (pass->blenddst)
			{
				case GL_ONE:
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_ZERO);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_COLOR);
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA);
					break;
				case GL_ZERO:
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_ZERO);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_COLOR);
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA);
					break;
				case GL_SRC_COLOR:
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_COLOR);
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA);
					break;
				case GL_ONE_MINUS_SRC_COLOR:
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_COLOR);
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA);
					break;
				case GL_SRC_ALPHA:
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_ALPHA);
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA);
					break;
				case GL_ONE_MINUS_SRC_ALPHA:
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_TEXTURE);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_ALPHA);
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_TEXTURE);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA);
					break;
				case GL_DST_ALPHA:
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_PREVIOUS_EXT);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_ALPHA);
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_PREVIOUS_EXT);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA);
					break;
				case GL_ONE_MINUS_DST_ALPHA:
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_PREVIOUS_EXT);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_ONE_MINUS_SRC_ALPHA);
					qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_PREVIOUS_EXT);
					qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_ONE_MINUS_SRC_ALPHA);
					break;
			}
		}

		R_ModifyTextureCoords ( pass, i );
	}

	R_FlushArraysMtex ();
}

/*
================
R_RenderMeshBuffer
================
*/
void R_RenderMeshBuffer ( meshbuffer_t *mb, qboolean shadowpass )
{
	int	i;
	msurface_t *surf;
	shaderpass_t *pass;
	unsigned int dlightBits;

	if ( !numVerts ) {
		return;
	}

	surf = mb->infokey > 0 ? &currentmodel->bmodel->surfaces[mb->infokey-1] : NULL;
	if ( surf ) {
		r_patchWidth = surf->patchWidth;
		r_patchHeight = surf->patchHeight;
		r_lightmapTexNum = surf->lightmaptexturenum;
	} else {
		r_patchWidth = 0;
		r_patchHeight = 0;
		r_lightmapTexNum = -1;
	}

	r_currentShader = mb->shader;

	if ( gl_state.in2d ) {
		r_currentShaderTime = curtime * 0.001f;
	} else {
		r_currentShaderTime = r_newrefdef.time;

		if ( currententity ) {
			r_currentShaderTime -= currententity->shaderTime;
			if ( r_currentShaderTime < 0 ) {
				r_currentShaderTime = 0;
			}
		}
	}

	R_SetCurrentShaderState ();

	if ( r_currentShader->numdeforms ) {
		R_DeformVertices ();
	}

	if ( !numIndexes || shadowpass ) {
		return;
	}

	if ( r_triangleOutlines ) {
		R_LockArrays ( numVerts );

		if ( r_showtris->value ) {
			R_ResetTexState ();
			R_DrawTriangles ();
		}
		if ( r_shownormals->value ) {
			R_ResetTexState ();
			R_DrawNormals ();
		}

		R_UnlockArrays ();
		R_ClearArrays ();

		return;
	}

	// can we fog the geometry with alpha texture?
	r_texFog = (mb->fog && ((r_currentShader->sort <= (SHADER_SORT_OPAQUE+1) && 
		(r_currentShader->flags & (SHADER_DEPTHWRITE|SHADER_SKY))) || r_currentShader->fog_dist)) ? mb->fog : NULL;

	// check if the fog volume is present but we can't use alpha texture
	r_colorFog = (mb->fog && !r_texFog) ? mb->fog : NULL;

	R_LockArrays ( numVerts );
	pass = r_currentShader->passes;

	// render the shader
	if ( r_currentShader->numpasses == 1 ) {
		r_numUnits = 1;

		if ( !(r_currentShader->flags & SHADER_PASS_DETAIL) || r_detailtextures->value ) {
			pass->flush ( pass );
		}
	} else {
		for ( i = 0; i < r_currentShader->numpasses; ) {
			r_numUnits = pass->numMergedPasses;

			if ( !(pass->flags & SHADER_PASS_DETAIL) || r_detailtextures->value ) {
				pass->flush ( pass );
			}

			i += r_numUnits;
			pass += r_numUnits;
		}
	}

	// render dynamic lights, fog, triangle outlines, normals and clear arrays
	dlightBits = mb->dlightbits;
	if ( r_currentShader->flags & SHADER_FLARE ) {
		dlightBits = 0;
	}

	if ( dlightBits || r_texFog ) {
		GL_EnableMultitexture ( qfalse );
		qglTexCoordPointer( 2, GL_FLOAT, 0, inCoordsArray[0] );

		qglEnable ( GL_BLEND );
		qglDisable ( GL_ALPHA_TEST );
		qglDepthMask ( GL_FALSE );

		if ( dlightBits ) {
			R_AddDynamicLights ( dlightBits );
		}
		if ( r_texFog ) {
			R_RenderFogOnMesh ();
		}
	}

	R_UnlockArrays ();
	R_ClearArrays ();
}


/*
================
R_RenderFogOnMesh
================
*/
void R_RenderFogOnMesh (void)
{
	int		i;
	vec3_t	diff, viewtofog, fog_vpn;
	float	dist, vdist;
	shader_t *fogshader;
	cplane_t *fogplane;

	if ( !r_texFog->numplanes || !r_texFog->shader || !r_texFog->visibleplane ) {
		return;
	}

	R_ResetTexState ();

	fogshader = r_texFog->shader;
	fogplane = r_texFog->visibleplane;

	GL_Bind( r_fogtexture->texnum );

	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	if ( !r_currentShader->numpasses || r_currentShader->fog_dist || (r_currentShader->flags & SHADER_SKY) ) {
		qglDepthFunc ( GL_LEQUAL );
	} else {
		qglDepthFunc ( GL_EQUAL );
	}

	qglColor4ubv ( fogshader->fog_color );

	// distance to fog
	dist = PlaneDiff ( r_origin, fogplane );

	if ( r_currentShader->flags & SHADER_SKY ) {
		if ( dist > 0 )
			VectorMA( r_origin, -dist, fogplane->normal, viewtofog );
		else
			VectorCopy( r_origin, viewtofog );
	}
	else
	{
		VectorCopy( currententity->origin, viewtofog );
	}

	VectorScale ( vpn, fogshader->fog_dist, fog_vpn );

	for ( i = 0; i < numVerts; i++, currentCoords += 2 )
	{
		VectorAdd ( viewtofog, vertexArray[i], diff );
		vdist = PlaneDiff ( diff, fogplane );
		VectorSubtract ( diff, r_origin, diff );

		if ( dist < 0 ) {	// camera is inside the fog brush
			currentCoords[0] = DotProduct ( diff, fog_vpn );
		} else {
			if ( vdist < 0 ) {
				currentCoords[0] = vdist / ( vdist - dist );
				currentCoords[0] *= DotProduct ( diff, fog_vpn );
			} else {
				currentCoords[0] = 0.0f;
			}
		}

		currentCoords[1] = -vdist * fogshader->fog_dist + 1.5f/(float)FOG_TEXTURE_HEIGHT;
	}

	if ( !r_currentShader->numpasses ) {
		R_LockArrays ( numVerts );
	}

	R_FlushArrays ();
}

/*
================
R_BackendBeginTriangleOutlines
================
*/
void R_BackendBeginTriangleOutlines (void)
{
	r_triangleOutlines = qtrue;

	GL_EnableMultitexture ( qfalse );
	qglDisable( GL_TEXTURE_2D );
	qglDisable( GL_DEPTH_TEST );
	qglDisable ( GL_BLEND );
	qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	qglColor4f( 1, 1, 1, 1 );
}

/*
================
R_BackendEndTriangleOutlines
================
*/
void R_BackendEndTriangleOutlines (void)
{
	r_triangleOutlines = qfalse;
	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_TEXTURE_2D );
}


/*
================
R_DrawTriangles
================
*/
void R_DrawTriangles (void)
{
	R_FlushArrays ();
}

/*
================
R_DrawNormals
================
*/
void R_DrawNormals (void)
{
	int i;

	if ( gl_state.in2d ) {
		qglBegin ( GL_POINTS );
		for ( i = 0; i < numVerts; i++ ) { 
			qglVertex3fv ( vertexArray[i] );
		}
		qglEnd ();
	} else {
		qglDisable( GL_DEPTH_TEST );
		qglBegin ( GL_LINES );
		for ( i = 0; i < numVerts; i++ ) { 
			qglVertex3fv ( vertexArray[i] );
			qglVertex3f ( vertexArray[i][0] + normalsArray[i][0], 
				vertexArray[i][1] + normalsArray[i][1], 
				vertexArray[i][2] + normalsArray[i][2] );
		}
		qglEnd ();
		qglEnable( GL_DEPTH_TEST );
	}
}
