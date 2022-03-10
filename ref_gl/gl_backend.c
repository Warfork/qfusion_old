/*
Copyright (C) 2001-2002 Victor "Vic" Luchits

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
#include "gl_local.h"

#define MAX_POLYS		2048
#define MAX_ARRAY		MAX_POLYS*3

static	float	r_fastsin[1024];

static	vec4_t	vertexArray[MAX_ARRAY];
static	float	coordsArray[MAX_ARRAY][2];
static	float	coordsArrayMtex[MAX_ARRAY][2];
unsigned int	elemsArray [MAX_ARRAY*3];
static	byte_vec4_t	colorArray[MAX_ARRAY];
static	byte_vec4_t	colorArrayMtex[MAX_ARRAY];
static	byte_vec4_t	colorArrayTemp[MAX_ARRAY];
int		numVerts, numElems, numCoords, numCoordsMtex, numColors, numColorsMtex;
static	qboolean	locked;

static	byte	r_identityLighting;

void R_InitArrays (void)
{
	int i;

	numVerts = 0;
	numElems = 0;
	numCoords = 0;
    numColors = 0;
	numCoordsMtex = 0;
	numColorsMtex = 0;
	locked = false;

	memset (vertexArray, 0, MAX_ARRAY * sizeof(vec4_t));
	memset (coordsArray, 0, MAX_ARRAY * 2 * sizeof(float));
	memset (coordsArrayMtex, 0, MAX_ARRAY * 2 * sizeof(float));
	memset (elemsArray, 0, MAX_ARRAY * 3 * sizeof(int));
	memset (colorArray, 0, MAX_ARRAY * 4 * sizeof(byte));
	memset (colorArrayMtex, 0, MAX_ARRAY * 4 * sizeof(byte));

	GL_SelectTexture ( GL_TEXTURE1 );
	qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArrayMtex );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArrayMtex );

	GL_SelectTexture ( GL_TEXTURE0 );
	qglVertexPointer( 3, GL_FLOAT, 16, vertexArray );	// padded for SIMD
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
	qglTexCoordPointer( 2, GL_FLOAT, 0, coordsArray );

	qglEnableClientState( GL_VERTEX_ARRAY );

	r_identityLighting = 255 >> (int)max(0, r_overbrightbits->value);

	for ( i = 0; i < 1024; i++ ) {
		r_fastsin[i] = sin ( i * M_TWOPI / 1024.0 );
	}
}

void R_LockArrays (void)
{
	if ( locked )
		return;

	if ( !gl_ext_compiled_vertex_array->value )
		return;

	if ( qglLockArraysEXT != 0 ) {
		qglLockArraysEXT( 0, numVerts );
		locked = true;
	}
}

void R_UnlockArrays (void)
{
	if ( !locked )
		return;

	if ( qglUnlockArraysEXT != 0 ) {
		qglUnlockArraysEXT();
		locked = false;
	}
}

void R_PushElem ( unsigned int elem )
{
	if( numElems >= MAX_ARRAY*3 ) {
		return;
	}

	elemsArray[numElems] = numVerts + elem;
	numElems++;
}

void R_PushVertex ( float *vertex )
{
	if( numVerts >= MAX_ARRAY ) {
		return;
	}

	vertexArray[numVerts][0] = vertex[0];
	vertexArray[numVerts][1] = vertex[1];
	vertexArray[numVerts][2] = vertex[2];

	numVerts++;
}

void R_PushCoord ( float *tc )
{
	if( numCoords >= MAX_ARRAY ) {
		return;
	}

	coordsArray[numCoords][0] = tc[0];
	coordsArray[numCoords][1] = tc[1];

	numCoords++;
}

void R_PushCoordMtex ( float *tc )
{
	coordsArrayMtex[numCoordsMtex][0] = tc[0];
	coordsArrayMtex[numCoordsMtex][1] = tc[1];

	numCoordsMtex++;
}

void R_PushColor ( float *color )
{
	int i;
	float c;

	if( numColors >= MAX_ARRAY ) {
		return;
	}

	for ( i = 0; i < 4; i++ ) {
		c = color[i] * 255.0f + (float)0xC00000;
		colorArray[numColors][i] = (*(byte *)&c);
	}

	numColors++;
}

void R_PushColorMtex ( float *color )
{
	int i;
	float c;

	for ( i = 0; i < 4; i++ ) {
		c = color[i] * 255.0f + (float)0xC00000;
		colorArrayMtex[numColorsMtex][i] = (*(byte *)&c);
	}

	numColorsMtex++;
}

void R_PushColorByte ( byte *color )
{
	int i;

	if( numColors >= MAX_ARRAY ) {
		return;
	}

	for ( i = 0; i < 4; i++ ) {
		colorArray[numColors][i] = color[i];
	}

	numColors++;
}

void R_PushColorByteMtex ( byte *color )
{
	int i;

	for ( i = 0; i < 4; i++ ) {
		colorArrayMtex[numColorsMtex][i] = color[i];
	}

	numColorsMtex++;
}

/*
==============
R_stripmine

This function looks for and sends tristrips.
Code by Stephen C. Taylor (Aftershock 3D rendering engine)
==============
*/
static void R_stripmine( int numelems, int *elems )
{
    int toggle;
    unsigned int a, b, elem;

    elem = 0;
    while ( elem < numelems ) {
		toggle = 1;
		qglBegin( GL_TRIANGLE_STRIP );
		
		qglArrayElement( elems[elem++] );
		b = elems[elem++];
		qglArrayElement( b );
		a = elems[elem++];
		qglArrayElement( a );
		
		while ( elem < numelems ) {
			if ( a != elems[elem] || b != elems[elem+1] ) {
				break;
			}

			if ( toggle ) {
				b = elems[elem+2];
				qglArrayElement( b );
			} else {
				a = elems[elem+2];
				qglArrayElement( a );
			}

			elem += 3;
			toggle = !toggle;
		}

		qglEnd();
    }
}

void R_ClearArrays (void)
{
	numElems = 0;
	numVerts = 0;
}

void R_FlushArrays (void)
{
	if ( !numVerts || !numElems ) {
		return;
	}

	if ( numColors > 1 ) {
		qglEnableClientState( GL_COLOR_ARRAY );
	} else if ( numColors == 1 ) {
		qglColor4ubv ( colorArray[0] );
	}

	if ( numCoords ) {
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	if ( !locked ) {
		R_stripmine ( numElems, elemsArray );
	}
	else {
		qglDrawElements( GL_TRIANGLES, numElems, GL_UNSIGNED_INT,
			elemsArray );
	}

	if ( numColors > 1 ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	}
	numColors = 0;

	if ( numCoords ) {
		numCoords = 0;
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}
}

void R_FlushArraysMtex ( int tex1, int tex2 )
{
	if ( !numVerts || !numElems ) {
		return;
	}

	GL_MBind( GL_TEXTURE0, tex1 );

	if ( numColors > 1 ) {
		qglEnableClientState( GL_COLOR_ARRAY );
	} else if ( numColors == 1 ) {
		qglColor4ubv ( colorArray[0] );
	}

	if ( numCoords ) {
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	GL_MBind( GL_TEXTURE1, tex2 );

	if ( numCoordsMtex ) {
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	if ( numColorsMtex > 1 ) {
		qglEnableClientState( GL_COLOR_ARRAY );
	} else if ( numColorsMtex == 1 ) {
		qglColor4ubv ( colorArrayMtex[0] );
	}

	if ( !locked ) {
		R_stripmine ( numElems, elemsArray );
	} else {
		qglDrawElements( GL_TRIANGLES, numElems, GL_UNSIGNED_INT,
			elemsArray );
	}

	if ( numCoordsMtex ) {
		numCoordsMtex = 0;
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	if ( numColorsMtex > 1 ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	}
	numColorsMtex = 0;

	GL_SelectTexture ( GL_TEXTURE0 );

	if ( numColors > 1 ) {
		qglDisableClientState( GL_COLOR_ARRAY );
	}
	numColors = 0;

	if ( numCoords ) {
		numCoords = 0;
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}
}

float R_FastSin ( float t )
{
	if ( t >= 1.0f )
		t -= floor ( t );

	return r_fastsin[((int)(t * 1024) & 1023)];
}

static float R_FuncEval ( unsigned int func, float *args )
{
	float x = args[2] + r_shadertime * args[3];

	if ( x >= 1.0f )
		x -= floor(x);

	// Evaluate a number of time based periodic functions
	switch (func)
	{
		case SHADER_FUNC_SIN:
			return R_FastSin( x ) * args[1] + args[0];

		case SHADER_FUNC_TRIANGLE:
			if (x < 0.25) 
				return (x * 4) * args[1] + args[0];
			else if (x < 0.75)
				return (2 - 4 * x) * args[1] + args[0];
			else
				return ((x-0.75) * 4 - 1.0f) * args[1] + args[0];
			
		case SHADER_FUNC_SQUARE:
			return (x < 0.5) ? args[1] + args[0] : args[0] - args[1];
			
		case SHADER_FUNC_SAWTOOTH:
			return x * args[1] + args[0];
			
		case SHADER_FUNC_INVERSESAWTOOTH:
			return (1.0 - x) * args[1] + args[0];

		case SHADER_FUNC_NOISE:
			return frand() * x * args[1] + args[0];

		case SHADER_FUNC_CONSTANT:
			return 0.0;
	}

	Com_Error ( ERR_DROP, "R_FuncEval: invalid func enum\n" );
    return 0.0;
}

/*
================
R_DeformVertices
================
*/
void R_DeformVertices ( mesh_t *mesh, vec3_t *out, int outlen )
{
	int i, j, n, pw, ph, p;
	float args[4], deflect;
	float *quad[4];
	vec3_t tv, rot_centre;
	shader_t *shader = mesh->shader;
	mvertex_t *in = mesh->firstvert;
	int numverts = mesh->numverts;

	for ( i = 0; i < numverts; i++ ) {
		R_PushVertex ( in[i].position );
	}

	for (n = 0; n < shader->numdeforms; n++)
	{
		switch (shader->deform_vertices[n])
		{
			case DEFORMV_NONE:
				break;

			case DEFORMV_WAVE:
				args[0] = shader->deformv_func[n].args[0];
				args[1] = shader->deformv_func[n].args[1];
				args[3] = shader->deformv_func[n].args[3];

				for ( i = 0; i < numverts; i++ ) {
					args[2] = shader->deformv_func[n].args[2] + 
						shader->deform_params[n][0] * (vertexArray[i][0]+vertexArray[i][1]+vertexArray[i][2]);
					deflect = R_FuncEval (shader->deformv_func[n].func, args);
						
					// Deflect vertex along its normal by wave amount
					VectorMA ( vertexArray[i], deflect, in[i].normal, vertexArray[i] );
				}
				break;

			case DEFORMV_NORMAL:
				for ( i = 0; i < numverts; i++ ) {
					deflect = shader->deform_params[n][0];
					VectorMA ( vertexArray[i], deflect, in[i].normal, vertexArray[i] );
				}
				break;

			case DEFORMV_MOVE:		// should this depend on the framerate?
				deflect = R_FuncEval (shader->deformv_func[n].func, shader->deformv_func[n].args);

				for ( i = 0; i < numverts; i++ ) {
					VectorMA ( vertexArray[i], deflect, shader->deform_params[n], vertexArray[i] );
				}
				break;

			case DEFORMV_BULGE:
				pw = mesh->patchWidth;
				ph = mesh->patchHeight;
				p = 0;

				args[0] = shader->deform_params[n][0];
				args[1] = shader->deform_params[n][1];
				args[2] = shader->deform_params[n][2];
				args[2] = r_shadertime / (args[2]*pw);
				args[2] -= floor(args[2]);

				for ( i = 0; i < ph; i++ ) {
					deflect = R_FastSin(((float)i/(float)ph * args[0]+args[2]))*args[1];

					for ( j = 0; j < pw; j++ ) {
						VectorMA ( vertexArray[p], deflect, in[p].normal, vertexArray[p] );
						p++;
					}
				}
				break;

			case DEFORMV_AUTOSPRITE:
				for ( i = 0; i < numElems; i += 6 )
				{
					quad[0] = (float *)(vertexArray + elemsArray[i+0]);
					quad[1] = (float *)(vertexArray + elemsArray[i+3]);
					quad[2] = (float *)(vertexArray + elemsArray[i+4]);
					quad[3] = (float *)(vertexArray + elemsArray[i+5]);

					for ( j = 0; j < 3; j++ )
						rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

					for ( j = 0; j < 4; j++ ) {
						VectorSubtract ( quad[j], rot_centre, tv );
						Matrix3_Multiply_Vec3 ( r_inverse_world_matrix, tv, quad[j] );
						VectorAdd ( rot_centre, quad[j], quad[j] );
					}
				}
				break;

			case DEFORMV_AUTOSPRITE2:
				break;

			default:
				break;
		}
	}

	if ( !out ) {
		return;
	}

	for ( i = 0; (i < numverts) && (i < outlen); i++ ) {
		VectorCopy ( vertexArray[i], out[i] );
	}
}

void R_VertexTCBase ( shaderpass_t *pass, mvertex_t *vert, vec2_t out )
{
	extern entity_t	r_worldent;

	if ( pass->tc_gen == TC_GEN_LIGHTMAP ) {
		out[0] = vert->lm_st[0];
		out[1] = vert->lm_st[1];
	} else if ( pass->tc_gen == TC_GEN_BASE ) {
		out[0] = vert->tex_st[0];
		out[1] = vert->tex_st[1];
	} else if ( pass->tc_gen == TC_GEN_ENVIRONMENT ) {
		vec3_t t, n;

		// project vector
		if ( !currententity || currententity == &r_worldent ) {
			VectorCopy ( vert->position, t );
			VectorCopy ( vert->normal, n );
		} else {
			VectorAdd ( vert->position, currententity->origin, t );
	
			// rotate normal
			n[1] = -DotProduct ( vert->normal, currententity->angleVectors[1] );
			n[2] = DotProduct ( vert->normal, currententity->angleVectors[2] );
		}

		VectorSubtract ( t, r_origin, t );
		VectorNormalizeFast ( t );

		out[0] = t[1] - n[1];
		out[1] = t[2] - n[2];
	}  else if ( pass->tc_gen == TC_GEN_VECTOR ) {
		out[0] = DotProduct ( pass->tc_gen_s, vert->position );
		out[1] = DotProduct ( pass->tc_gen_t, vert->position );
	}
}

/*
================
R_ModifyTextureCoords
================
*/
void R_ModifyTextureCoords ( mesh_t *mesh, int pass, qboolean mtex, vec2_t *out, int outlen )
{
	int i, n;
	float t1, t2, sint, cost;
	vec2_t t;
	vec2_t *tcArray;
	int numverts = mesh->numverts;
	mvertex_t *in = mesh->firstvert;
	shaderpass_t *shaderPass = &mesh->shader->pass[pass];

	if ( !mtex ) {
		for ( i = 0; i < numverts; i++  ) {
			R_VertexTCBase( shaderPass, in + i, t );
			R_PushCoord ( t );
		}

		tcArray = coordsArray;
	} else {
		for ( i = 0; i < numverts; i++ ) {
			R_VertexTCBase( shaderPass, in + i, t );
			R_PushCoordMtex ( t );
		}

		tcArray = coordsArrayMtex;
	}

	for (n = 0; n < shaderPass->num_tc_mod; n++)
	{
		switch (shaderPass->tc_mod[n].type)
		{
			case SHADER_TCMOD_ROTATE:
				cost = shaderPass->tc_mod[n].args[0] * r_shadertime;
				sint = R_FastSin( cost );
				cost = R_FastSin( cost + 0.25 );

				for ( i = 0; i < numverts; i++ ) {
					t1 = cost * (tcArray[i][0] - 0.5f) + sint * (0.5f - tcArray[i][1]) + 0.5f;
					t2 = cost * (tcArray[i][1] - 0.5f) + sint * (tcArray[i][0] - 0.5f) + 0.5f;
					tcArray[i][0] = t1;
					tcArray[i][1] = t2;
				}
				break;

			case SHADER_TCMOD_SCALE:
				for ( i = 0; i < numverts; i++ ) {
					tcArray[i][0] = tcArray[i][0] * shaderPass->tc_mod[n].args[0];
					tcArray[i][1] = tcArray[i][1] * shaderPass->tc_mod[n].args[1];
				}
				break;

			case SHADER_TCMOD_TURB:
				t1 = shaderPass->tc_mod[n].args[2] + r_shadertime * shaderPass->tc_mod[n].args[3];
				t1 -= floor(t1);
				t2 = shaderPass->tc_mod[n].args[1];

				for ( i = 0; i < numverts; i++ ) {
					tcArray[i][0] = tcArray[i][0] + R_FastSin (tcArray[i][0]*t2+t1) * t2;
					tcArray[i][1] = tcArray[i][1] + R_FastSin (tcArray[i][1]*t2+t1) * t2;
				}
				break;
			
			case SHADER_TCMOD_STRETCH:
				t1 = 1.0f / R_FuncEval ( shaderPass->tc_mod[n].args[0], &shaderPass->tc_mod[n].args[1] );
				t2 = 0.5f - 0.5f*t1;

				for ( i = 0; i < numverts; i++ ) {
					tcArray[i][0] = tcArray[i][0] * t1 + t2;
					tcArray[i][1] = tcArray[i][1] * t1 + t2;
				}
				break;
						
			case SHADER_TCMOD_SCROLL:
				for ( i = 0; i < numverts; i++ ) {
					tcArray[i][0] = tcArray[i][0] + shaderPass->tc_mod[n].args[0] * r_shadertime;
					tcArray[i][1] = tcArray[i][1] + shaderPass->tc_mod[n].args[1] * r_shadertime;
				}
				break;
					
			case SHADER_TCMOD_TRANSFORM:
				for ( i = 0; i < numverts; i++ ) {
					t1 = tcArray[i][0];
					t2 = tcArray[i][1];
					tcArray[i][0] = t1 * shaderPass->tc_mod[n].args[0] + t2 * shaderPass->tc_mod[n].args[2] + 
						shaderPass->tc_mod[n].args[4];
					tcArray[i][1] = t2 * shaderPass->tc_mod[n].args[1] + t1 * shaderPass->tc_mod[n].args[3] + 
						shaderPass->tc_mod[n].args[5];
				}
				break;

			default:
				break;
		}
	}

	if ( !out ) {
		return;
	}

	for ( i = 0; (i < numverts) && (i < outlen); i++ ) {
		out[i][0] = tcArray[i][0];
		out[i][1] = tcArray[i][0];
	}
}


/*
================
R_ModifyColour
================
*/
void R_ModifyColour ( mesh_t *mesh, int pass, qboolean mtex, vec4_t *out, int outlen )
{
	int i, j;
	float c, a, dot;
	vec3_t t;
	vec3_t ambient, diffuse, direction, dir;
	int numverts = mesh->numverts;
	mvertex_t *in = mesh->firstvert;
	shaderpass_t *shaderPass = &mesh->shader->pass[pass];
	byte_vec4_t *cArray = colorArrayTemp;
	qboolean noArray = shaderPass->flags & SHADER_PASS_NOCOLORARRAY;

	switch (shaderPass->rgbgen)
	{
		case RGB_GEN_IDENTITY_LIGHTING:
			if ( noArray ) {
				cArray[0][0] = cArray[0][1] = cArray[0][2] = r_identityLighting;
			} else {
				for ( i = 0; i < numverts; i++ ) {
					cArray[i][0] = cArray[i][1] = cArray[i][2] = r_identityLighting;
				}
			}
			break;

		case RGB_GEN_IDENTITY:
			if ( noArray ) {
				cArray[0][0] = cArray[0][1] = cArray[0][2] = 255;
			} else {
				for ( i = 0; i < numverts; i++ ) {
					cArray[i][0] = cArray[i][1] = cArray[i][2] = 255;
				}
			}
			break;

		case RGB_GEN_CONST:
			if ( noArray ) {
				c = shaderPass->rgbgen_func.args[0] + (float)0xC00000;
				cArray[0][0] = (*(byte *)&c);
				c = shaderPass->rgbgen_func.args[1] + (float)0xC00000;
				cArray[0][1] = (*(byte *)&c);
				c = shaderPass->rgbgen_func.args[2] + (float)0xC00000;
				cArray[0][2] = (*(byte *)&c);
			} else {
				for ( i = 0; i < numverts; i++ ) {
					c = shaderPass->rgbgen_func.args[0] + (float)0xC00000;
					cArray[i][0] = (*(byte *)&c);
					c = shaderPass->rgbgen_func.args[1] + (float)0xC00000;
					cArray[i][1] = (*(byte *)&c);
					c = shaderPass->rgbgen_func.args[2] + (float)0xC00000;
					cArray[i][2] = (*(byte *)&c);
				}
			}
			break;

		case RGB_GEN_WAVE:
			c = R_FuncEval ( shaderPass->rgbgen_func.func, shaderPass->rgbgen_func.args );
			if ( c < 0.0f ) c = 0.0f; else if ( c > 255.0f ) c = 255.0f;
			c += (float)0xC00000;

			if ( noArray ) {
				cArray[0][0] = cArray[0][1] = cArray[0][2] = (*(byte *)&c);
			} else {
				for ( i = 0; i < numverts; i++ ) {
					cArray[i][0] = cArray[i][1] = cArray[i][2] = (*(byte *)&c);
				}
			}
			break;

		case RGB_GEN_ENTITY:
			if ( noArray ) {
				cArray[0][0] = cArray[0][1] = cArray[0][2] = 255;
			} else {
				for ( i = 0; i < numverts; i++ ) {
					cArray[i][0] = cArray[i][1] = cArray[i][2] = 255;
				}
			}
			break;

		case RGB_GEN_ONE_MINUS_ENTITY:
			if ( noArray ) {
				cArray[0][0] = cArray[0][1] = cArray[0][2] = 0;
			} else {
				for ( i = 0; i < numverts; i++ ) {
					cArray[i][0] = cArray[i][1] = cArray[i][2] = 0;
				}
			}
			break;

		case RGB_GEN_VERTEX:
		case RGB_GEN_EXACT_VERTEX:
			for ( i = 0; i < numverts; i++ ) {
				c = in[i].colour[0]*255.0f + (float)0xC00000;
				cArray[i][0] = (*(byte *)&c);
				c = in[i].colour[1]*255.0f + (float)0xC00000;
				cArray[i][1] = (*(byte *)&c);
				c = in[i].colour[2]*255.0f + (float)0xC00000;
				cArray[i][2] = (*(byte *)&c);
			}
			break;

		case RGB_GEN_ONE_MINUS_VERTEX:
			for ( i = 0; i < numverts; i++ ) {
				c = (1.0f - in[i].colour[0])*255.0f + (float)0xC00000;
				cArray[i][0] = (*(byte *)&c);
				c = (1.0f - in[i].colour[1])*255.0f + (float)0xC00000;
				cArray[i][1] = (*(byte *)&c);
				c = (1.0f - in[i].colour[2])*255.0f + (float)0xC00000;
				cArray[i][2] = (*(byte *)&c);
			}
			break;

		case RGB_GEN_LIGHTING_DIFFUSE:
			if ( !currententity ) {
				for ( i = 0; i < numverts; i++ ) {
					c = 255.0f + (float)0xC00000;
					cArray[i][0] = (*(byte *)&c);
					c = 255.0f + (float)0xC00000;
					cArray[i][1] = (*(byte *)&c);
					c = 255.0f + (float)0xC00000;
					cArray[i][2] = (*(byte *)&c);
				}
			} else {
				VectorAdd ( vertexArray[0], currententity->origin, t );
				R_LightForPoint ( t, ambient, diffuse, dir );

				for ( j = 0; j < 3; j++ ) {
					ambient[j] *= 255;
					diffuse[j] *= 255;
				}

				// rotate normal
				direction[0] = DotProduct ( dir, currententity->angleVectors[0] );
				direction[1] = -DotProduct ( dir, currententity->angleVectors[1] );
				direction[2] = DotProduct ( dir, currententity->angleVectors[2] );

				for ( i = 0; i < numverts; i++ ) {
					dot = DotProduct ( in[i].normal, direction );
					
					if ( dot <= 0 ) {
						for ( j = 0; j < 3; j++ ) {
							c = ambient[j] + (float)0xC00000;
							cArray[i][j] = (*(byte *)&c);
						}
					} else {
						for ( j = 0; j < 3; j++ ) {
							c = ambient[j] + dot * diffuse[j];
							c = max ( 0, min( c, 255.0f ) ) + (float)0xC00000;
							cArray[i][j] = (*(byte *)&c);
						}
					}
				}
			}
			break;

		default:
			break;
	}

	switch (shaderPass->alphagen)
	{
		case ALPHA_GEN_WAVE:
			a = R_FuncEval ( shaderPass->alphagen_func.func, shaderPass->alphagen_func.args );
			if ( a < 0.0f ) a = 0.0f; else if ( a > 255.0f ) a = 255.0f;
			a += (float)0xC00000;

			if ( noArray ) {
				cArray[0][3] = (*(byte *)&a);
			} else {
				for ( i = 0; i < numverts; i++ ) {
					cArray[i][3] = (*(byte *)&a);
				}
			}
			break;

		case ALPHA_GEN_PORTAL:
			for ( i = 0; i < numverts; i++ ) {
				VectorAdd ( vertexArray[i], currententity->origin, t );
				VectorSubtract ( t, r_origin, t );
				a = sqrt(DotProduct ( t, t ));
				a = min ( a, 255.0f );
				a += (float)0xC00000;
				cArray[i][3] = (*(byte *)&a);
			}
			break;

		case ALPHA_GEN_VERTEX:
			for ( i = 0; i < numverts; i++ ) {
				a = in[i].colour[3] * 255.0f;
				a += (float)0xC00000;
				cArray[i][3] = (*(byte *)&a);
			}
			break;

		case ALPHA_GEN_ENTITY:
			if ( noArray ) {
				cArray[0][3] = 0;
			} else {
				for ( i = 0; i < numverts; i++ ) {
					cArray[i][3] = 0;
				}
			}
			break;

		case ALPHA_GEN_SPECULAR:
			for ( i = 0; i < numverts; i++ ) {
				VectorAdd ( vertexArray[i], currententity->origin, t );
				VectorSubtract ( r_origin, t, t );
				VectorNormalizeFast ( t );
				a = DotProduct( t, in[i].normal );
				a *= a; a *= a * 255.0f;
				a += (float)0xC00000;
				cArray[i][3] = (*(byte *)&a);
			}
			break;

		case ALPHA_GEN_IDENTITY:
			if ( noArray ) {
				cArray[0][3] = 255;
			} else {
				for ( i = 0; i < numverts; i++ ) {
					cArray[i][3] = 255;
				}
			}
			break;
	}

	if ( noArray ) {
		if ( !mtex ) {
			R_PushColorByte ( cArray[0] );
		} else {
			R_PushColorByteMtex ( cArray[0] );
		}
	}
	else {
		for ( i = 0; i < numverts; i++ ) {
			if ( !mtex ) {
				R_PushColorByte ( cArray[i] );
			} else {
				R_PushColorByteMtex ( cArray[i] );
			}

			if ( out && (i < outlen) ) {
				Vector4Copy ( cArray[i], out[i] );
			}
		}
	}
}

int R_ShaderpassTex ( shaderpass_t *pass, int lmtex )
{
	if ( pass->flags & SHADER_PASS_ANIMMAP ) {
		int frame = (int)(pass->anim_fps * r_shadertime) % pass->anim_numframes;
		return pass->anim_frames[frame]->texnum;
	} else if ( pass->flags & SHADER_PASS_LIGHTMAP ) {
		return gl_state.lightmap_textures + lmtex;
	}

	return pass->anim_frames[0] ? pass->anim_frames[0]->texnum : -1;
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
		qglDisable (GL_CULL_FACE);
	} else {
		if ( shader->flags & SHADER_CULL_FRONT ) {
			qglEnable (GL_CULL_FACE);
			qglCullFace (GL_FRONT);
		} else if ( shader->flags & SHADER_CULL_BACK ) {
			qglEnable (GL_CULL_FACE);
			qglCullFace (GL_BACK);
		} else {
			qglDisable (GL_CULL_FACE);
		}
	}

	if (shader->flags & SHADER_POLYGONOFFSET)
		qglEnable (GL_POLYGON_OFFSET_FILL);
	else
		qglDisable (GL_POLYGON_OFFSET_FILL);
}

/*
================
R_SetShaderpassState
================
*/
void R_SetShaderpassState ( shaderpass_t *pass, qboolean mtex )
{
	if ( mtex ) {
		if ( pass->blendmode != GL_REPLACE ) {
			GLSTATE_ENABLE_BLEND
			qglBlendFunc ( pass->blendsrc, pass->blenddst );
		} else {
			GLSTATE_DISABLE_BLEND
		}
	} else {
		if ( pass->flags & SHADER_PASS_BLEND ) {
			GLSTATE_ENABLE_BLEND
			qglBlendFunc ( pass->blendsrc, pass->blenddst );
		}
		else {
			GLSTATE_DISABLE_BLEND
		}
	}

	if (pass->flags & SHADER_PASS_ALPHAFUNC) {
		GLSTATE_ENABLE_ALPHATEST

		if ( pass->alphafunc == SHADER_ALPHA_GT0 ) {
			qglAlphaFunc ( GL_GREATER, 0 );
		} else if ( pass->alphafunc == SHADER_ALPHA_LT128 ) {
			qglAlphaFunc ( GL_LESS, 0.5f );
		} else if ( pass->alphafunc == SHADER_ALPHA_GE128 ) {
			qglAlphaFunc ( GL_GEQUAL, 0.5f );
		}
	} else {
		GLSTATE_DISABLE_ALPHATEST
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
R_RenderFogOnMesh
================
*/
void R_RenderFogOnMesh ( shader_t *mshader, mfog_t *fog )
{
	int		i;
	float	tc[2];
	vec3_t	diff;
	float	dist, vdist;
	shader_t *fogshader;
	cplane_t *fogplane;

	if ( !fog->brush || !fog->shader ) {
		return;
	}

	fogshader = fog->shader;
	fogplane = fog->plane;

	GL_Bind ( r_fogtexture->texnum );
	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	if ( mshader->numpasses && (mshader->pass[mshader->numpasses-1].flags & SHADER_PASS_DEPTHWRITE) ) {
		qglDepthFunc ( GL_EQUAL );
	} else {
		qglDepthFunc ( GL_LEQUAL );
	}

	// upside-down
	if ( fogplane->type < 3 ) {
		dist = r_origin[fogplane->type] - fogplane->dist;
	} else {
		dist = DotProduct (r_origin, fogplane->normal) - fogplane->dist;
	}

	qglColor4ubv ( fogshader->fog_color );

	for ( i = 0; i < numVerts; i++ )
	{
		VectorAdd ( currententity->origin, vertexArray[i], diff );

		if ( fogplane->type < 3 ) {
			vdist = diff[fogplane->type] - fogplane->dist;
		} else {
			vdist = DotProduct (diff, fogplane->normal) - fogplane->dist;
		}

		VectorSubtract ( diff, r_origin, diff );

		if ( dist < 0 ) {	// camera is inside the fog brush
			tc[0] = DotProduct ( diff, vpn );
		} else {
			if ( vdist < 0 ) {
				tc[0] = vdist / ( vdist - dist );
				tc[0] *= DotProduct ( diff, vpn );
			} else {
				tc[0] = 0.0f;
			}
		}
	
		tc[0] *= fogshader->fog_dist;
		tc[1] = -vdist*fogshader->fog_dist + 1.5f/256.0f;

		R_PushCoord ( tc );
	}

	if ( !mshader->numpasses ) {
		R_LockArrays ();
	}

	R_FlushArrays ();
}

/*
** R_DrawTriangleOutlines
*/
void R_DrawTriangleOutlines (void)
{
	if ( !Com_ServerState() || !developer->value ) {
		return;
	}

	qglDisable (GL_TEXTURE_2D);
	qglDisable (GL_DEPTH_TEST);
	qglColor4f (1, 1, 1, 1);
	qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);

	R_FlushArrays ();

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglEnable (GL_DEPTH_TEST);
	qglEnable (GL_TEXTURE_2D);
}

/*
================
R_RenderSkySurface
================
*/
void R_RenderSkySurface ( mesh_t *mesh, mfog_t *fog )
{
	int			i;
	int			*lindex;

	if ( !fog ) {
		return;
	}

	lindex = mesh->firstindex;
	for (i = 0; i < mesh->numindexes; i++, lindex++)
		R_PushElem ( *lindex );
	
	R_DeformVertices ( mesh, NULL, 0 );
	R_RenderFogOnMesh ( mesh->shader, fog );
	R_UnlockArrays ();
	R_ClearArrays ();
}

/*
================
R_RenderMeshGeneric
================
*/
void R_RenderMeshGeneric ( mesh_t *mesh, mfog_t *fog, msurface_t *s )
{
	int			i, j, tex;
	int			*lindex;
	shader_t	*shader = mesh->shader;
	shaderpass_t *pass;

	if ( qglMTexCoord2fSGIS ) {
		GL_SelectTexture( GL_TEXTURE1 );
		qglDisable( GL_TEXTURE_2D );
	}

	GL_SelectTexture( GL_TEXTURE0 );
	GL_TexEnv ( GL_MODULATE );

	R_SetShaderState ( shader );

	lindex = mesh->firstindex;
	for (i = 0; i < mesh->numindexes; i++, lindex++)
		R_PushElem ( *lindex );
	
	R_DeformVertices ( mesh, NULL, 0 );

	if ( shader->numpasses > 1 ) {
		R_LockArrays ();
	}

	pass = shader->pass;
	for ( j = 0; j < shader->numpasses; j++, pass++ )
	{
		if ( pass->flags & SHADER_PASS_DETAIL ) {
			if ( !r_detailtextures->value )
				continue;
		}

		tex = R_ShaderpassTex ( pass, mesh->lightmaptexturenum );

		if ( tex != -1 ) {
			GL_Bind ( tex );
		} else {
			continue;
		}

		R_SetShaderpassState ( pass, false );
		R_ModifyTextureCoords ( mesh, j, false, NULL, 0 );
		R_ModifyColour ( mesh, j, false, NULL, 0 );

		if ( shader->numpasses == 1 ) {
			R_LockArrays ();
		}

		R_FlushArrays ();
	}

	if ( fog || s ) {
		GLSTATE_ENABLE_BLEND
		GLSTATE_DISABLE_ALPHATEST
		qglDepthMask ( GL_FALSE );
	}

	if ( s ) {
		R_AddDynamicLights ( s, vertexArray );
	}

	if ( fog ) {
		R_RenderFogOnMesh ( shader, fog );
	}

	if ( gl_showtris->value/* && !gl_state.in2d*/ ) {
		R_DrawTriangleOutlines ();
	}

	R_UnlockArrays ();
	R_ClearArrays ();
}

/*
================
R_RenderMeshMultitextured
================
*/
void R_RenderMeshMultitextured ( mesh_t *mesh, mfog_t *fog, msurface_t *s )
{
	int			i;
	int			*lindex;
	shader_t	*shader = mesh->shader;
	shaderpass_t *pass1, *pass2;

	pass1 = &shader->pass[0];
	pass2 = &shader->pass[1];

	R_SetShaderState ( shader );

	GL_SelectTexture( GL_TEXTURE1 );
	qglEnable( GL_TEXTURE_2D );
	GL_SelectTexture( GL_TEXTURE0 );

	R_SetShaderpassState ( pass1, true );
	if ( pass1->blendmode == GL_REPLACE )
		GL_TexEnv ( GL_REPLACE );
	else
		GL_TexEnv ( GL_MODULATE );

	GL_SelectTexture( GL_TEXTURE1 );
	GL_TexEnv( pass2->blendmode );

	lindex = mesh->firstindex;
	for (i = 0; i <  mesh->numindexes; i++, lindex++)
		R_PushElem (*lindex);

	R_DeformVertices ( mesh, NULL, 0 );
	R_LockArrays ();
	R_ModifyTextureCoords ( mesh, 0, false, NULL, 0 );
	R_ModifyTextureCoords ( mesh, 1, true, NULL, 0 );
	R_ModifyColour ( mesh, 0, false, NULL, 0 );
	R_ModifyColour ( mesh, 1, true, NULL, 0 );
	R_FlushArraysMtex ( R_ShaderpassTex ( pass1, mesh->lightmaptexturenum ), R_ShaderpassTex ( pass2, mesh->lightmaptexturenum ) );

	if ( fog || s ) {
		GL_EnableMultitexture ( false );
		GL_TexEnv ( GL_MODULATE );
		GLSTATE_ENABLE_BLEND
		GLSTATE_DISABLE_ALPHATEST
		qglDepthMask ( GL_FALSE );
	}

	if ( s ) {
		R_AddDynamicLights ( s, vertexArray );
	}

	if ( fog ) {
		R_RenderFogOnMesh ( shader, fog );
	}

	if ( gl_showtris->value/* && !gl_state.in2d*/ ) {
		GL_EnableMultitexture ( false );
		R_DrawTriangleOutlines ();
	}

	R_UnlockArrays ();
	R_ClearArrays ();
}

/*
================
R_RenderMeshCombined
================
*/
void R_RenderMeshCombined ( mesh_t *mesh, mfog_t *fog, msurface_t *s )
{
	int			i;
	int			*lindex;
	shader_t	*shader = mesh->shader;
	shaderpass_t *pass1, *pass2;

	pass1 = &shader->pass[0];
	pass2 = &shader->pass[1];

	R_SetShaderState ( shader );

	GL_SelectTexture( GL_TEXTURE1 );
	qglEnable( GL_TEXTURE_2D );
	GL_SelectTexture( GL_TEXTURE0 );

	R_SetShaderpassState ( pass1, true );
	if ( pass1->blendmode == GL_REPLACE )
		GL_TexEnv ( GL_REPLACE );
	else
		GL_TexEnv ( GL_MODULATE );

	GL_SelectTexture( GL_TEXTURE1 );

	if ( pass2->blendmode )
	{
		switch ( pass2->blendmode )
		{
		case GL_REPLACE:
		case GL_MODULATE:
		case GL_BLEND:
		case GL_ADD:
			// these modes are best set with TexEnv, Combine4 would need much more setup
			GL_TexEnv ( pass2->blendmode );
			break;

		case GL_DECAL:
			// mimics Alpha-Blending in upper texture stage, but instead of multiplying the alpha-channel, they´re added
			// this way it can be possible to use GL_DECAL in both texture-units, while still looking good
			// normal mutlitexturing would multiply the alpha-channel which looks ugly
			GL_TexEnv ( GL_COMBINE_EXT );
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
		GL_TexEnv ( GL_COMBINE4_NV );

		qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_ADD);
		qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_ADD);

		qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
		qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
		qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
		qglTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);

		switch ( pass2->blendsrc )
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

		switch (pass2->blenddst)
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

	lindex = mesh->firstindex;
	for (i = 0; i < mesh->numindexes; i++, lindex++)
		R_PushElem (*lindex);

	R_DeformVertices ( mesh, NULL, 0 );
	R_LockArrays ();
	R_ModifyTextureCoords ( mesh, 0, false, NULL, 0 );
	R_ModifyTextureCoords ( mesh, 1, true, NULL, 0 );
	R_ModifyColour ( mesh, 0, false, NULL, 0 );
	R_ModifyColour ( mesh, 1, true, NULL, 0 );
	R_FlushArraysMtex ( R_ShaderpassTex ( pass1, mesh->lightmaptexturenum ), R_ShaderpassTex ( pass2, mesh->lightmaptexturenum ) );

	if ( fog || s ) {
		GL_EnableMultitexture ( false );
		GL_TexEnv ( GL_MODULATE );
		GLSTATE_ENABLE_BLEND
		GLSTATE_DISABLE_ALPHATEST
		qglDepthMask ( GL_FALSE );
	}

	if ( s ) {
		R_AddDynamicLights ( s, vertexArray );
	}

	if ( fog ) {
		R_RenderFogOnMesh ( shader, fog );
	}

	if ( gl_showtris->value/* && !gl_state.in2d*/ ) {
		GL_EnableMultitexture ( false );
		R_DrawTriangleOutlines ();
	}

	R_UnlockArrays ();
	R_ClearArrays ();
}

