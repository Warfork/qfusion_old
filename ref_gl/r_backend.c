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
#include "r_local.h"

#define FTABLE_SIZE		1024
#define FTABLE_CLAMP(x)	(((int)((x)*FTABLE_SIZE) & (FTABLE_SIZE-1)))
#define FTABLE_EVALUATE(table,x) (table ? table[FTABLE_CLAMP(x)] : frand()*((x)-floor(x)))

static	float	r_sintable[FTABLE_SIZE];
static	float	r_triangletable[FTABLE_SIZE];
static	float	r_squaretable[FTABLE_SIZE];
static	float	r_sawtoothtable[FTABLE_SIZE];
static	float	r_inversesawtoothtable[FTABLE_SIZE];

index_t			*indexesArray;
int				*neighborsArray;
vec3_t			*trNormalsArray;

vec2_t			*coordsArray;
vec2_t			*lightmapCoordsArray;

vec4_t			vertexArray[MAX_ARRAY_VERTS*2];
vec3_t			normalsArray[MAX_ARRAY_VERTS];

vec4_t			tempVertexArray[MAX_ARRAY_VERTS];
vec3_t			tempNormalsArray[MAX_ARRAY_VERTS];

index_t			inIndexesArray[MAX_ARRAY_INDEXES];
int				inNeighborsArray[MAX_ARRAY_NEIGHBORS];
vec3_t			inTrNormalsArray[MAX_ARRAY_TRIANGLES];
vec2_t			inCoordsArray[MAX_ARRAY_VERTS];
vec2_t			inLightmapCoordsArray[MAX_ARRAY_VERTS];
vec4_t			inColorsArray[MAX_ARRAY_VERTS];

static	vec2_t		tUnitCoordsArray[MAX_TEXTURE_UNITS][MAX_ARRAY_VERTS];
static	byte_vec4_t	colorArray[MAX_ARRAY_VERTS];

int				numVerts, numIndexes, numColors;

qboolean		r_arrays_locked;
qboolean		r_blocked;

static	int		r_lmtex;

static	int		r_texNums[SHADER_PASS_MAX];
static	int		r_numUnits;

index_t			*currentIndex;
int				*currentTrNeighbor;
float			*currentTrNormal;
float			*currentVertex;
float			*currentNormal;
float			*currentCoords;
float			*currentLightmapCoords;
float			*currentColor;

static	float	r_identityLighting;
static	float	r_localShaderTime;

unsigned int	r_numverts;
unsigned int	r_numtris;
unsigned int	r_numflushes;

index_t			r_quad_indexes[6] = { 0, 1, 2, 0, 2, 3 };

void R_FinishMeshBuffer ( meshbuffer_t *mb );

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
	neighborsArray = inNeighborsArray;
	trNormalsArray = inTrNormalsArray;
	coordsArray = inCoordsArray;
	lightmapCoordsArray = inLightmapCoordsArray;

	currentTrNeighbor = neighborsArray;
	currentTrNormal = trNormalsArray[0];

	currentVertex = vertexArray[0];
	currentNormal = normalsArray[0];

	currentCoords = coordsArray[0];
	currentLightmapCoords = lightmapCoordsArray[0];

	currentColor = inColorsArray[0];

	r_arrays_locked = false;
	r_blocked = false;

	qglVertexPointer( 3, GL_FLOAT, 16, vertexArray );	// padded for SIMD
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

	qglEnableClientState( GL_VERTEX_ARRAY );

	if ( !r_ignorehwgamma->value )
		r_identityLighting = 1.0f / pow(2, max(0, (int)floor(r_overbrightbits->value)));
	else
		r_identityLighting = 1.0f;

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
float R_FastSin ( float t )
{
	return r_sintable[FTABLE_CLAMP(t)];
}

/*
==============
R_TableForFunc
==============
*/
static float *R_TableForFunc ( unsigned int func )
{
	switch (func)
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
	}

	// assume noise
	return NULL;
}

/*
==============
R_BackendStartFrame
==============
*/
void R_BackendStartFrame (void)
{
	r_numverts = 0;
	r_numtris = 0;
	r_numflushes = 0;
}

/*
==============
R_BackendEndFrame
==============
*/
void R_BackendEndFrame (void)
{
	if (r_speeds->value)
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
		r_arrays_locked = true;
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
		r_arrays_locked = false;
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
	neighborsArray = inNeighborsArray;
	trNormalsArray = inTrNormalsArray;

	currentTrNeighbor = neighborsArray;
	currentTrNormal = trNormalsArray[0];

	currentVertex = vertexArray[0];
	currentNormal = normalsArray[0];

	R_ResetTexState ();

	r_blocked = false;
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
void R_DeformVertices ( meshbuffer_t *mb )
{
	int i, j, k, pw, ph, p;
	float args[4], deflect;
	float *quad[4], *table;
	shader_t *shader;
	deformv_t *deformv;
	vec3_t tv, rot_centre;

	shader = mb->shader;
	deformv = &shader->deforms[0];

	for (i = 0; i < shader->numdeforms; i++, deformv++)
	{
		switch (deformv->type)
		{
			case DEFORMV_NONE:
				break;

			case DEFORMV_WAVE:
				args[0] = deformv->func.args[0];
				args[1] = deformv->func.args[1];
				args[3] = deformv->func.args[2] + deformv->func.args[3] * r_localShaderTime;
				table = R_TableForFunc ( deformv->func.type );

				for ( j = 0; j < numVerts; j++ ) {
					deflect = deformv->args[0] * (vertexArray[j][0]+vertexArray[j][1]+vertexArray[j][2]) + args[3];
					deflect = FTABLE_EVALUATE ( table, deflect ) * args[1] + args[0];

					// Deflect vertex along its normal by wave amount
					VectorMA ( vertexArray[j], deflect, normalsArray[j], vertexArray[j] );
				}
				break;

			case DEFORMV_NORMAL:
				args[0] = deformv->args[1] * r_localShaderTime;

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
				deflect = deformv->func.args[2] + r_localShaderTime * deformv->func.args[3];
				deflect = FTABLE_EVALUATE (table, deflect) * deformv->func.args[1] + deformv->func.args[0];

				for ( j = 0; j < numVerts; j++ )
					VectorMA ( vertexArray[j], deflect, deformv->args, vertexArray[j] );
				break;

			case DEFORMV_BULGE:
				pw = mb->mesh->patchWidth;
				ph = mb->mesh->patchHeight;

				args[0] = deformv->args[0] / (float)ph;
				args[1] = deformv->args[1];
				args[2] = r_localShaderTime / (deformv->args[2]*pw);

				for ( k = 0, p = 0; k < ph; k++ ) {
					deflect = R_FastSin ( (float)k * args[0] + args[2] ) * args[1];

					for ( j = 0; j < pw; j++, p++ )
						VectorMA ( vertexArray[p], deflect, normalsArray[p], vertexArray[p] );
				}
				break;

			case DEFORMV_AUTOSPRITE:
				if ( numIndexes < 6 )
					break;

				for ( k = 0; k < numIndexes; k += 6 )
				{
					quad[0] = (float *)(vertexArray + indexesArray[k+0]);
					quad[1] = (float *)(vertexArray + indexesArray[k+3]);
					quad[2] = (float *)(vertexArray + indexesArray[k+4]);
					quad[3] = (float *)(vertexArray + indexesArray[k+5]);

					for ( j = 0; j < 3; j++ )
						rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25 + currententity->origin[j];

					for ( j = 0; j < 4; j++ ) {
						VectorSubtract ( quad[j], rot_centre, tv );
						Matrix4_Multiply_Vec3 ( r_modelview_matrix, tv, quad[j] );
						VectorAdd ( rot_centre, quad[j], quad[j] );
					}
				}
				break;

			case DEFORMV_AUTOSPRITE2:
				if ( numIndexes < 6 )
					break;

				for ( k = 0; k < numIndexes; k += 6 )
				{
					vec3_t axis;
					mat3_t matrix, fmatrix, result;

					quad[0] = (float *)(vertexArray + indexesArray[k+0]);
					quad[1] = (float *)(vertexArray + indexesArray[k+1]);
					quad[2] = (float *)(vertexArray + indexesArray[k+2]);
					quad[3] = (float *)(vertexArray + indexesArray[k+5]);

					// build a matrix were the longest axis of the billboard is the Y-Axis
					VectorSubtract ( quad[1], quad[0], result[0] );
					VectorSubtract ( quad[2], quad[0], result[1] );
					VectorNormalizeFast ( result[0] );
					VectorNormalizeFast ( result[1] );
					CrossProduct ( result[0], result[1], result[2] );
					VectorCopy ( result[1], axis );

					for ( j = 0; j < 3; j++ )
						rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

					if ( currententity ) {
						VectorAdd ( currententity->origin, rot_centre, tv );
					} else {
						VectorCopy ( rot_centre, tv );
					}
					VectorSubtract ( r_origin, tv, tv );

					// filter any longest-axis-parts off the camera-direction
					deflect = -DotProduct ( tv, axis );

					VectorMA ( tv, deflect, axis, fmatrix[2] );
					VectorNormalizeFast ( fmatrix[2] );
					VectorCopy ( axis, fmatrix[1] );
					CrossProduct ( fmatrix[1], fmatrix[2], fmatrix[0] );

					Matrix3_Transpose ( result, matrix );
					Matrix3_Multiply ( matrix, fmatrix, result );

					for ( j = 0; j < 4; j++ ) {
						VectorSubtract ( quad[j], rot_centre, tv );
						Matrix3_Multiply_Vec3 ( result, tv, quad[j] );
						VectorAdd ( rot_centre, quad[j], quad[j] );
					}
				}
				break;

			case DEFORMV_PROJECTION_SHADOW:
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
void R_VertexTCBase ( int tc_gen, int unit )
{
	int	i;
	vec3_t t, n;
	float *outCoords;
	vec3_t transform;
	mat3_t inverse_axis;

	outCoords = tUnitCoordsArray[unit][0];
	qglTexCoordPointer( 2, GL_FLOAT, 0, outCoords );

	if ( tc_gen == TC_GEN_BASE ) {
		memcpy ( outCoords, coordsArray[0], sizeof(float) * 2 * numVerts );
	} else if ( tc_gen == TC_GEN_LIGHTMAP ) {
		memcpy ( outCoords, lightmapCoordsArray[0], sizeof(float) * 2 * numVerts );
	} else if ( tc_gen == TC_GEN_ENVIRONMENT ) {
		if ( !currentmodel ) {
			VectorSubtract ( vec3_origin, currententity->origin, transform );
			Matrix3_Transpose ( currententity->axis, inverse_axis );
		} else if ( currentmodel == r_worldmodel ) {
			VectorSubtract ( vec3_origin, r_origin, transform );
		} else if ( currentmodel->type == mod_brush ) {
			VectorNegate ( currententity->origin, t );
			VectorSubtract ( t, r_origin, transform );
			Matrix3_Transpose ( currententity->axis, inverse_axis );
		} else {
			VectorSubtract ( vec3_origin, currententity->origin, transform );
			Matrix3_Transpose ( currententity->axis, inverse_axis );
		}

		for ( i = 0; i < numVerts; i++, outCoords += 2 ) {
			VectorAdd ( vertexArray[i], transform, t );

			// project vector
			if ( currentmodel && (currentmodel == r_worldmodel) ) {
				n[0] = normalsArray[i][0];
				n[1] = normalsArray[i][1];
				n[2] = Q_RSqrt ( DotProduct(t,t) );
			} else {
				n[0] = DotProduct ( normalsArray[i], inverse_axis[0] );
				n[1] = DotProduct ( normalsArray[i], inverse_axis[1] );
				n[2] = Q_RSqrt ( DotProduct(t,t) );
			}

			outCoords[0] = t[0]*n[2] - n[0];
			outCoords[1] = t[1]*n[2] - n[1];
		}
	} else if ( tc_gen == TC_GEN_VECTOR ) {
		for ( i = 0; i < numVerts; i++, outCoords += 2 ) {
			static vec3_t tc_gen_s = { 1.0f, 0.0f, 0.0f };
			static vec3_t tc_gen_t = { 0.0f, 1.0f, 0.0f };
			
			outCoords[0] = DotProduct ( tc_gen_s, vertexArray[i] );
			outCoords[1] = DotProduct ( tc_gen_t, vertexArray[i] );
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
		return pass->anim_frames[(int)(pass->anim_fps * r_localShaderTime) % pass->anim_numframes]->texnum;
	} else if ( (pass->flags & SHADER_PASS_LIGHTMAP) && r_lmtex >= 0 ) {
		return gl_state.lightmap_textures + r_lmtex;
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
		if ( pass->tc_gen == TC_GEN_BASE ) {
			qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArray );
		} else if ( pass->tc_gen == TC_GEN_LIGHTMAP ) {
			qglTexCoordPointer( 2, GL_FLOAT, 0, lightmapCoordsArray );
		} else {
			R_VertexTCBase ( pass->tc_gen, unit );
		}
		return;
	}

	R_VertexTCBase ( pass->tc_gen, unit );

	for (i = 0, tcmod = pass->tcmods; i < pass->numtcmods; i++, tcmod++)
	{
		tcArray = tUnitCoordsArray[unit][0];

		switch (tcmod->type)
		{
			case SHADER_TCMOD_ROTATE:
				cost = tcmod->args[0] * r_localShaderTime;
				sint = R_FastSin( cost );
				cost = R_FastSin( cost + 0.25 );

				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					t1 = cost * (tcArray[0] - 0.5f) - sint * (tcArray[1] - 0.5f) + 0.5f;
					t2 = cost * (tcArray[1] - 0.5f) + sint * (tcArray[0] - 0.5f) + 0.5f;
					tcArray[0] = t1;
					tcArray[1] = t2;
				}
				break;

			case SHADER_TCMOD_SCALE:
				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					tcArray[0] = tcArray[0] * tcmod->args[0];
					tcArray[1] = tcArray[1] * tcmod->args[1];
				}
				break;

			case SHADER_TCMOD_TURB:
				t1 = tcmod->args[2] + r_localShaderTime * tcmod->args[3];
				t2 = tcmod->args[1];

				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					tcArray[0] = tcArray[0] + R_FastSin (tcArray[0]*t2+t1) * t2;
					tcArray[1] = tcArray[1] + R_FastSin (tcArray[1]*t2+t1) * t2;
				}
				break;
			
			case SHADER_TCMOD_STRETCH:
				table = R_TableForFunc ( tcmod->args[0] );
				t2 = tcmod->args[3] + r_localShaderTime * tcmod->args[4];
				t1 = FTABLE_EVALUATE ( table, t2 ) * tcmod->args[2] + tcmod->args[1];
				t1 = t1 ? 1.0f / t1 : 1.0f;
				t2 = 0.5f - 0.5f * t1;

				for ( j = 0; j < numVerts; j++, tcArray += 2 ) {
					tcArray[0] = tcArray[0] * t1 + t2;
					tcArray[1] = tcArray[1] * t1 + t2;
				}
				break;
						
			case SHADER_TCMOD_SCROLL:
				t1 = tcmod->args[0] * r_localShaderTime;
				t2 = tcmod->args[1] * r_localShaderTime;

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
void R_ModifyColor ( meshbuffer_t *mb, shaderpass_t *pass )
{
	int i;
	float *table, c, a;
	vec3_t t, v;
	shader_t *shader;
	byte *bArray;
	float *cArray, *vArray;
	qboolean fogged, noArray;
	vec4_t tempcArray[MAX_ARRAY_VERTS];

	cArray = tempcArray[0];
	vArray = inColorsArray[0];
	bArray = colorArray[0];

	shader = mb->shader;
	fogged = mb->fog && (shader->sort >= SHADER_SORT_ADDITIVE) &&
		!(pass->flags & SHADER_PASS_DEPTHWRITE) && !shader->fog_dist;
	noArray = (pass->flags & SHADER_PASS_NOCOLORARRAY) && !fogged;

	if ( noArray ) {
		numColors = 1;
	} else {
		numColors = numVerts;
	}

	if ( !fogged ) {
		// three most common cases
		if ( pass->rgbgen == RGB_GEN_VERTEX && pass->alphagen == ALPHA_GEN_VERTEX ) {
			for ( i = 0; i < numColors; i++, vArray += 4, bArray += 4 ) 
			{
				bArray[0] = FloatToByte ( vArray[0] );
				bArray[1] = FloatToByte ( vArray[1] );
				bArray[2] = FloatToByte ( vArray[2] );
				bArray[3] = FloatToByte ( vArray[3] );
			}

			return;
		} else if ( pass->rgbgen == RGB_GEN_VERTEX && pass->alphagen == ALPHA_GEN_IDENTITY ) {
			for ( i = 0; i < numColors; i++, vArray += 4, bArray += 4 ) 
			{
				bArray[0] = FloatToByte ( vArray[0] );
				bArray[1] = FloatToByte ( vArray[1] );
				bArray[2] = FloatToByte ( vArray[2] );
				bArray[3] = 255;
			}

			return;
		} else if ( pass->rgbgen == RGB_GEN_IDENTITY && pass->alphagen == ALPHA_GEN_IDENTITY ) {
			memset ( bArray, (byte)255, numColors*sizeof(byte_vec4_t) );
			return;
		}
	}

	switch (pass->rgbgen)
	{
		case RGB_GEN_IDENTITY_LIGHTING:
			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[0] = cArray[1] = cArray[2] = r_identityLighting;
			}
			break;

		case RGB_GEN_IDENTITY:
			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[0] = cArray[1] = cArray[2] = 1.0f;
			}
			break;

		case RGB_GEN_CONST:
			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[0] = pass->rgbgen_func->args[0];
				cArray[1] = pass->rgbgen_func->args[1];
				cArray[2] = pass->rgbgen_func->args[2];
			}
			break;

		case RGB_GEN_WAVE:
			table = R_TableForFunc ( pass->rgbgen_func->type );
			c = pass->rgbgen_func->args[2] + r_localShaderTime * pass->rgbgen_func->args[3];
			c = FTABLE_EVALUATE ( table, c ) * pass->rgbgen_func->args[1] + pass->rgbgen_func->args[0];
			clamp ( c, 0.0f, 1.0f );

			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[0] = cArray[1] = cArray[2] = c;
			}
			break;

		case RGB_GEN_ENTITY:
			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[0] = currententity->color[0] * (1.0 / 255.0);
				cArray[1] = currententity->color[1] * (1.0 / 255.0);
				cArray[2] = currententity->color[2] * (1.0 / 255.0);
			}
			break;

		case RGB_GEN_ONE_MINUS_ENTITY:
			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[0] = 1.0 - currententity->color[0] * (1.0 / 255.0);
				cArray[1] = 1.0 - currententity->color[1] * (1.0 / 255.0);
				cArray[2] = 1.0 - currententity->color[2] * (1.0 / 255.0);
			}
			break;

		case RGB_GEN_VERTEX:
		case RGB_GEN_EXACT_VERTEX:
			for ( i = 0; i < numColors; i++, cArray += 4, vArray += 4 ) {
				cArray[0] = vArray[0];
				cArray[1] = vArray[1];
				cArray[2] = vArray[2];
			}
			break;

		case RGB_GEN_ONE_MINUS_VERTEX:
			for ( i = 0; i < numColors; i++, cArray += 4, vArray += 4 ) {
				cArray[0] = 1.0f - vArray[0];
				cArray[1] = 1.0f - vArray[1];
				cArray[2] = 1.0f - vArray[2];
			}
			break;

		case RGB_GEN_LIGHTING_DIFFUSE:
			if ( !currententity ) {
				for ( i = 0; i < numColors; i++, cArray += 4 ) {
					cArray[0] = cArray[1] = cArray[2] = 1.0f;
				}
			} else {
				R_LightForEntity ( currententity, tempcArray );
			}
			break;

		default:
			break;
	}

	cArray = tempcArray[0];
	vArray = inColorsArray[0];

	switch (pass->alphagen)
	{
		case ALPHA_GEN_IDENTITY:
			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[3] = 1.0f;
			}
			break;

		case ALPHA_GEN_CONST:
			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[3] = pass->alphagen_func->args[0];
			}
			break;

		case ALPHA_GEN_WAVE:
			table = R_TableForFunc ( pass->alphagen_func->type );
			a = pass->alphagen_func->args[2] + r_localShaderTime * pass->alphagen_func->args[3];
			a = FTABLE_EVALUATE ( table, a ) * pass->alphagen_func->args[1] + pass->alphagen_func->args[0];
			clamp ( a, 0.0f, 1.0f );

			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[3] = a;
			}
			break;

		case ALPHA_GEN_PORTAL:
			VectorAdd ( vertexArray[0], currententity->origin, v );
			VectorSubtract ( r_origin, v, t );
			a = VectorLength ( t ) * (1.0/255.0);
			clamp ( a, 0.0f, 1.0f );

			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[3] = a;
			}
			break;

		case ALPHA_GEN_VERTEX:
			for ( i = 0; i < numColors; i++, cArray += 4, vArray += 4 ) {
				cArray[3] = vArray[3];
			}
			break;

		case ALPHA_GEN_ENTITY:
			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				cArray[3] = currententity->color[3] * (1.0 / 255.0);
			}
			break;

		case ALPHA_GEN_SPECULAR:
			VectorSubtract ( r_origin, currententity->origin, t );

			if ( !Matrix3_Compare (currententity->axis, axis_identity) ) {
				Matrix3_Multiply_Vec3 ( currententity->axis, t, v );
			} else {
				VectorCopy ( t, v );
			}

			for ( i = 0; i < numColors; i++, cArray += 4 ) {
				VectorSubtract ( v, vertexArray[i], t );
				a = DotProduct( t, normalsArray[i] ) * Q_RSqrt ( DotProduct(t,t) );
				a = a * a * a * a * a;
				cArray[3] = bound ( 0.0f, a, 1.0f );
			}
			break;
	}
	
	cArray = tempcArray[0];

	if ( fogged ) {
		float dist, vdist;
		cplane_t *fogplane;
		vec3_t diff, viewtofog, fog_vpn;

		fogplane = mb->fog->visibleplane;
		dist = PlaneDiff ( r_origin, fogplane );

		if ( shader->flags & SHADER_SKY ) 
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

		VectorScale ( vpn, mb->fog->shader->fog_dist, fog_vpn );

		for ( i = 0; i < numColors; i++, cArray += 4, bArray += 4 ) 
		{
			VectorAdd ( vertexArray[i], viewtofog, diff );

			// camera is inside the fog
			if ( dist < 0 ) {
				VectorSubtract ( diff, r_origin, diff );

				c = DotProduct ( diff, fog_vpn );
				a = 1.0f - bound ( 0, c, 1.0f );
			} else {
				vdist = PlaneDiff ( diff, fogplane );

				if ( vdist < 0 ) {
					VectorSubtract ( diff, r_origin, diff );

					c = vdist / ( vdist - dist );
					c *= DotProduct ( diff, fog_vpn );
					a = 1.0f - bound ( 0, c, 1.0f );
				} else {
					a = 1.0f;
				}
			}

			if ( pass->blendmode == GL_ADD ) {
				bArray[0] = FloatToByte ( cArray[0]*a );
				bArray[1] = FloatToByte ( cArray[1]*a );
				bArray[2] = FloatToByte ( cArray[2]*a );
				bArray[3] = FloatToByte ( cArray[3] );
			} else {
				bArray[0] = FloatToByte ( cArray[0] );
				bArray[1] = FloatToByte ( cArray[1] );
				bArray[2] = FloatToByte ( cArray[2] );
				bArray[3] = FloatToByte ( cArray[3]*a );
			}
		}
	} else {
		for ( i = 0; i < numColors; i++, cArray += 4, bArray += 4 ) 
		{
			bArray[0] = FloatToByte ( cArray[0] );
			bArray[1] = FloatToByte ( cArray[1] );
			bArray[2] = FloatToByte ( cArray[2] );
			bArray[3] = FloatToByte ( cArray[3] );
		}
	}
}

/*
================
R_SetShaderState
================
*/
void R_SetShaderState ( shader_t *shader )
{
// Face culling
	if ( !gl_cull->value ) {
		GLSTATE_DISABLE_CULL;
	} else {
		if ( shader->flags & SHADER_CULL_FRONT ) {
			GLSTATE_ENABLE_CULL;
			qglCullFace ( GL_FRONT );
		} else if ( shader->flags & SHADER_CULL_BACK ) {
			GLSTATE_ENABLE_CULL;
			qglCullFace ( GL_BACK );
		} else {
			GLSTATE_DISABLE_CULL;
		}
	}

	if ( shader->flags & SHADER_POLYGONOFFSET ) {
		GLSTATE_ENABLE_OFFSET;
	} else {
		GLSTATE_DISABLE_OFFSET;
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
		GLSTATE_ENABLE_BLEND;
		qglBlendFunc ( pass->blendsrc, pass->blenddst );
	} else {
		GLSTATE_DISABLE_BLEND;
	}

	if ( pass->flags & SHADER_PASS_ALPHAFUNC ) {
		GLSTATE_ENABLE_ALPHATEST;

		if ( pass->alphafunc == SHADER_ALPHA_GT0 ) {
			qglAlphaFunc ( GL_GREATER, 0 );
		} else if ( pass->alphafunc == SHADER_ALPHA_LT128 ) {
			qglAlphaFunc ( GL_LESS, 0.5f );
		} else if ( pass->alphafunc == SHADER_ALPHA_GE128 ) {
			qglAlphaFunc ( GL_GEQUAL, 0.5f );
		}
	} else {
		GLSTATE_DISABLE_ALPHATEST;
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
void R_RenderMeshGeneric ( meshbuffer_t *mb, shaderpass_t *pass )
{
	R_SetShaderpassState ( pass, false );
	R_ModifyTextureCoords ( pass, 0 );
	R_ModifyColor ( mb, pass );

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
void R_RenderMeshMultitextured ( meshbuffer_t *mb, shaderpass_t *pass )
{
	int	i;

	r_numUnits = pass->numMergedPasses;

	R_SetShaderpassState ( pass, true );
	R_ModifyColor ( mb, pass );
	R_ModifyTextureCoords ( pass, 0 );

	GL_SelectTexture( GL_TEXTURE_0 );
	if ( pass->blendmode == GL_REPLACE )
		GL_TexEnv( GL_REPLACE );
	else
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
void R_RenderMeshCombined ( meshbuffer_t *mb, shaderpass_t *pass )
{
	int	i;

	r_numUnits = pass->numMergedPasses;

	R_SetShaderpassState ( pass, true );
	R_ModifyColor ( mb, pass );
	R_ModifyTextureCoords ( pass, 0 );

	GL_SelectTexture( GL_TEXTURE_0 );
	if ( pass->blendmode == GL_REPLACE )
		GL_TexEnv( GL_REPLACE );
	else
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
	shader_t *shader;
	shaderpass_t *pass;

	shader = mb->shader;
	r_lmtex = mb->infokey;

	if ( currententity && !gl_state.in2d ) {
		r_localShaderTime = r_shadertime - currententity->shaderTime;
	} else {
		r_localShaderTime = r_shadertime;
	}

	R_SetShaderState ( shader );

	if ( shader->numdeforms ) {
		R_DeformVertices ( mb );
	}

	if ( shadowpass ) {
		return;
	}

	R_LockArrays ( numVerts );

	for ( i = 0, pass = shader->passes; i < shader->numpasses; )
	{
		if ( !(pass->flags & SHADER_PASS_DETAIL) || r_detailtextures->value ) {
			pass->flush ( mb, pass );
		}

		i += pass->numMergedPasses;
		pass += pass->numMergedPasses;
	}

	R_FinishMeshBuffer ( mb );
}


/*
================
R_RenderFogOnMesh
================
*/
void R_RenderFogOnMesh ( shader_t *shader, mfog_t *fog )
{
	int		i;
	vec3_t	diff, viewtofog, fog_vpn;
	float	dist, vdist;
	shader_t *fogshader;
	cplane_t *fogplane;

	if ( !fog->numplanes || !fog->shader || !fog->visibleplane ) {
		return;
	}

	R_ResetTexState ();

	fogshader = fog->shader;
	fogplane = fog->visibleplane;

	GL_Bind( r_fogtexture->texnum );

	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	if ( shader->numpasses && (shader->passes[shader->numpasses-1].flags & SHADER_PASS_DEPTHWRITE) ) {
		qglDepthFunc ( GL_EQUAL );
	} else {
		qglDepthFunc ( GL_LEQUAL );
	}

	qglColor4ubv ( fogshader->fog_color );

	// distance to fog
	dist = PlaneDiff ( r_origin, fogplane );

	if ( shader->flags & SHADER_SKY ) {
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

		currentCoords[1] = -vdist*fogshader->fog_dist + 1.5f/256.0f;
	}

	if ( !shader->numpasses ) {
		R_LockArrays ( numVerts );
	}

	R_FlushArrays ();
}

/*
================
R_DrawTriangleOutlines
================
*/
void R_DrawTriangleOutlines (void)
{
	qglDisable( GL_TEXTURE_2D );
	qglDisable( GL_DEPTH_TEST );
	qglColor4f( 1, 1, 1, 1 );
	GLSTATE_DISABLE_BLEND;
	qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);

	R_FlushArrays ();

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_TEXTURE_2D );
}

/*
================
R_DrawNormals
================
*/
void R_DrawNormals (void)
{
	int i;

	qglDisable( GL_TEXTURE_2D );
	qglColor4f( 1, 1, 1, 1 );
	GLSTATE_DISABLE_BLEND;

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

	qglEnable( GL_TEXTURE_2D );
}

/*
================
R_FinishMeshBuffer
Render dynamic lights, fog, triangle outlines, normals and clear arrays
================
*/
void R_FinishMeshBuffer ( meshbuffer_t *mb )
{
	shader_t	*shader;
	qboolean	fogged;

	shader = mb->shader;
	fogged = mb->fog && ((shader->sort < SHADER_SORT_ADDITIVE && 
		(shader->flags & (SHADER_DEPTHWRITE|SHADER_SKY))) || shader->fog_dist);

	if ( mb->dlightbits || fogged ) {
		GL_EnableMultitexture ( false );
		qglTexCoordPointer( 2, GL_FLOAT, 0, inCoordsArray[0] );

		GLSTATE_ENABLE_BLEND;
		GLSTATE_DISABLE_ALPHATEST;
		qglDepthMask ( GL_FALSE );

		if ( mb->dlightbits ) {
			R_AddDynamicLights ( mb );
		}

		if ( fogged ) {
			R_RenderFogOnMesh ( shader, mb->fog );
		}
	}

	if ( r_showtris->value || r_shownormals->value ) {
		GL_EnableMultitexture ( false );

		if ( r_showtris->value ) {
			R_DrawTriangleOutlines ();
		}

		if ( r_shownormals->value ) {
			R_DrawNormals ();
		}
	}

	R_UnlockArrays ();
	R_ClearArrays ();
}
