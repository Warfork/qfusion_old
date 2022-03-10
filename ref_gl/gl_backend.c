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

#define MAX_ARRAY		1024

extern float r_fastsin[256];
#define FASTSIN_SCALE (256.0 / M_TWOPI)
#define TURBSIN(f, s, t) r_fastsin[((int)(((f)*(s)+t) * FASTSIN_SCALE) & 255)]

static	vec4_t	vertexArray[MAX_ARRAY];
static	float	coordsArray[MAX_ARRAY][2];
static	float	coordsArrayMtex[MAX_ARRAY][2];
unsigned int	elemsArray [MAX_ARRAY*3];
static	byte_vec4_t	colorArray[MAX_ARRAY];
static	byte_vec4_t	colorArrayMtex[MAX_ARRAY];
static	byte_vec4_t	colorArrayTemp[MAX_ARRAY];
int		numVerts, numElems, numCoords, numCoordsMtex, numColors, numColorsMtex;
static	qboolean	locked;

void R_InitArrays (void)
{
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

	if ( numColors ) {
		qglEnableClientState( GL_COLOR_ARRAY );
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

	if ( numColors ) {
		numColors = 0;
		qglDisableClientState( GL_COLOR_ARRAY );
	}

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

	if ( numColors ) {
		qglEnableClientState( GL_COLOR_ARRAY );
	}

	if ( numCoords ) {
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	GL_MBind( GL_TEXTURE1, tex2 );

	if ( numCoordsMtex ) {
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	if ( numColorsMtex ) {
		qglEnableClientState( GL_COLOR_ARRAY );
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

	if ( numColorsMtex ) {
		numColorsMtex = 0;
		qglDisableClientState( GL_COLOR_ARRAY );
	}

	GL_SelectTexture ( GL_TEXTURE0 );

	if ( numColors ) {
		numColors = 0;
		qglDisableClientState( GL_COLOR_ARRAY );
	}

	if ( numCoords ) {
		numCoords = 0;
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}
}

static float render_func_eval ( unsigned int func, float *args )
{
    float x = args[2] + r_shadertime * args[3];
	x -= floor(x);

	// Evaluate a number of time based periodic functions
	switch (func)
	{
	case SHADER_FUNC_SIN:
		return sin (x * M_TWOPI) * args[1] + args[0];

	case SHADER_FUNC_TRIANGLE:
		return ((x < 0.5f) ? (2.0f * x - 1.0f) : (-2.0f * x + 2.0f)) * args[1] + args[0];
		
	case SHADER_FUNC_SQUARE:
		return (x < 0.5f) ? args[1] + args[0] : args[0] - args[1];
		
	case SHADER_FUNC_SAWTOOTH:
		return x * args[1] + args[0];
		
	case SHADER_FUNC_INVERSESAWTOOTH:
		return (1.0f - x) * args[1] + args[0];
	}

    return 0.0;
}

/*
================
R_DeformVertices
================
*/
void R_DeformVertices ( mesh_t *mesh, vec3_t *out, int outlen )
{
	int i, n;
	float args[4], deflect;
	vec3_t tv;
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
					deflect = render_func_eval (shader->deformv_func[n].func, args);
						
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
				deflect = 0.1*render_func_eval (shader->deformv_func[n].func, shader->deformv_func[n].args);

				for ( i = 0; i < numverts; i++ ) {
					VectorMA ( vertexArray[i], deflect, shader->deform_params[n], vertexArray[i] );
				}

				break;

			case DEFORMV_BULGE:
				if ( !in[i].patchRel )
					break;

				args[0] = shader->deform_params[n][0];
				args[1] = shader->deform_params[n][1];

				for ( i = 0; i < numverts; i++ ) {
					args[2] = shader->deform_params[n][2];
					args[2] = r_shadertime / (args[2]*mesh->patchWidth);
					args[2] -= floor(args[2]);

					deflect = sin(((float)in[i].patchRel*args[0]+args[2])*M_TWOPI)*args[1];
					VectorMA ( vertexArray[i], deflect, in[i].normal, vertexArray[i] );
				}

				break;

			case DEFORMV_AUTOSPRITE:
				for ( i = 0; i < numverts; i++ ) {
					VectorSubtract ( vertexArray[i], in[i].rot_centre, tv );
					Matrix3_Multiply_Vec3 ( r_inverse_world_matrix, tv, vertexArray[i] );
					VectorAdd ( in[i].rot_centre, vertexArray[i], vertexArray[i] );
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

		VectorAdd ( vert->position, currententity->origin, t );

		// project vector
		if ( currententity == &r_worldent ) {
			VectorCopy ( vert->normal, n );
		} else {
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
		out[1] = DotProduct ( pass->tc_gen_s, vert->position );
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
	vec2_t c_tc, t;
	vec2_t *tcArray;
	int numverts = mesh->numverts;
	mvertex_t *in = mesh->firstvert;
	shaderpass_t *shaderPass = &mesh->shader->pass[pass];

	VectorCopy ( mesh->tex_centre_tc, c_tc );

	if ( !mtex ) {
		for ( i = 0; i < numverts; i++  ) {
			R_VertexTCBase( shaderPass, &in[i], t );
			R_PushCoord ( t );
		}

		tcArray = coordsArray;
	} else {
		for ( i = 0; i < numverts; i++ ) {
			R_VertexTCBase( shaderPass, &in[i], t );
			R_PushCoordMtex ( t );
		}

		tcArray = coordsArrayMtex;
	}

	for (n = 0; n < shaderPass->num_tc_mod; n++)
	{
		switch (shaderPass->tc_mod[n].type)
		{
			case SHADER_TCMOD_ROTATE:
				cost = DEG2RAD ( shaderPass->tc_mod[n].args[0] * r_shadertime );
				sint = sin(cost);
				cost = cos(cost);

				for ( i = 0; i < numverts; i++ ) {
					t1 = cost * (tcArray[i][0] - c_tc[0]) + sint * (c_tc[1] - tcArray[i][1]) + c_tc[0];
					t2 = cost * (tcArray[i][1] - c_tc[1]) + sint * (tcArray[i][0] - c_tc[0]) + c_tc[1];
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
					tcArray[i][0] = tcArray[i][0] + TURBSIN (tcArray[i][1], 0.125f, t1) * t2;
					tcArray[i][1] = tcArray[i][1] + TURBSIN (tcArray[i][0], 0.125f, t1) * t2;
				}
				break;
			
			case SHADER_TCMOD_STRETCH:
				t1 = 1.0f / render_func_eval ( shaderPass->tc_mod_stretch.func, shaderPass->tc_mod_stretch.args );
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
	int i;
	float c;
	byte_vec4_t *cArray;
	vec3_t t;
	vec4_t tColor;
	int numverts = mesh->numverts;
	mvertex_t *in = mesh->firstvert;
	shaderpass_t *shaderPass = &mesh->shader->pass[pass];

	cArray = colorArrayTemp;

	if ( shaderPass->rgbgen == RGB_GEN_IDENTITY && shaderPass->alphagen == ALPHA_GEN_IDENTITY ) {
		tColor[0] = tColor[1] = tColor[2] = tColor[3] = 1.0f;
		cArray[0][0] = cArray[0][1] = cArray[0][2] = cArray[0][3] = 255;

		for ( i = 0; i < numverts; i++ ) {
			if ( !mtex ) {
				R_PushColorByte ( cArray[0] );
			} else {
				R_PushColorByteMtex ( cArray[0] );
			}

			if ( out && (i < outlen) ) {
				Vector4Copy ( tColor, out[i] );
			}
		}

		return;
	}

	switch (shaderPass->rgbgen)
	{
		case RGB_GEN_IDENTITY_LIGHTING:
			for ( i = 0; i < numverts; i++ ) {
				cArray[i][0] = cArray[i][1] = cArray[i][2] = 128;
			}
			break;

		case RGB_GEN_IDENTITY:
			for ( i = 0; i < numverts; i++ ) {
				cArray[i][0] = cArray[i][1] = cArray[i][2] = 255;
			}
			break;

		case RGB_GEN_WAVE:
			c = render_func_eval ( shaderPass->rgbgen_func.func, shaderPass->rgbgen_func.args ) * 255.0f;
			if ( c < 0.0f ) c = 0.0f; else if ( c > 255.0f ) c = 255.0f;
			c += (float)0xC00000;

			for ( i = 0; i < numverts; i++ ) {
				cArray[i][0] = cArray[i][1] = cArray[i][2] = (*(byte *)&c);
			}
			break;

		case RGB_GEN_ENTITY:
			for ( i = 0; i < numverts; i++ ) {
				cArray[i][0] = cArray[i][1] = cArray[i][2] = 255;
			}
			break;

		case RGB_GEN_ONE_MINUS_ENTITY:
			for ( i = 0; i < numverts; i++ ) {
				cArray[i][0] = cArray[i][1] = cArray[i][2] = 0;
			}
			break;

		case RGB_GEN_VERTEX:
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
				c = in[i].colour[0]*255.0f + (float)0xC00000;
				cArray[i][0] = 255 - (*(byte *)&c);
				c = in[i].colour[1]*255.0f + (float)0xC00000;
				cArray[i][1] = 255 - (*(byte *)&c);
				c = in[i].colour[2]*255.0f + (float)0xC00000;
				cArray[i][2] = 255 - (*(byte *)&c);
			}
			break;

		case RGB_GEN_LIGHTING_DIFFUSE:	// TODO
			for ( i = 0; i < numverts; i++ ) {
				R_SetVertexColour ( in[i].position, in[i].normal, tColor );

				c = tColor[0]*255.0f + (float)0xC00000;
				cArray[i][0] = (*(byte *)&c);
				c = tColor[1]*255.0f + (float)0xC00000;
				cArray[i][1] = (*(byte *)&c);
				c = tColor[2]*255.0f + (float)0xC00000;
				cArray[i][2] = (*(byte *)&c);
			}
			break;

		default:
			break;
	}

	switch (shaderPass->alphagen)
	{
		case ALPHA_GEN_WAVE:
			c = render_func_eval ( shaderPass->alphagen_func.func, shaderPass->alphagen_func.args ) * 255.0f;
			if ( c < 0.0f ) c = 0.0f; else if ( c > 255.0f ) c = 255.0f;
			c += (float)0xC00000;

			for ( i = 0; i < numverts; i++ ) {
				cArray[i][3] = (*(byte *)&c);
			}
			break;

		case ALPHA_GEN_PORTAL:
			for ( i = 0; i < numverts; i++ ) {
				VectorSubtract ( in[i].position, r_origin, t );
				c = sqrt(DotProduct ( t, t ));
				c = min ( c, 255.0f );
				c += (float)0xC00000;
				cArray[i][3] = (*(byte *)&c);
			}
			break;

		case ALPHA_GEN_VERTEX:
			for ( i = 0; i < numverts; i++ ) {
				c = in[i].colour[3] * 255.0f;
				c += (float)0xC00000;
				cArray[i][3] = (*(byte *)&c);
			}
			break;

		case ALPHA_GEN_ENTITY:
			for ( i = 0; i < numverts; i++ ) {
				c = in[i].colour[3] * 255.0f;
				c += (float)0xC00000;
				cArray[i][3] = (*(byte *)&c);
			}
			break;

		case ALPHA_GEN_SPECULAR:
			for ( i = 0; i < numverts; i++ ) {
				VectorSubtract ( in[i].position, r_origin, t );
				VectorNormalizeFast ( t );
				c = DotProduct( t, in[i].normal ); if ( c < 0 ) c = -c;
				c = c * c * c * c * c * 255.0f;
				c += (float)0xC00000;
				cArray[i][3] = (*(byte *)&c);
			}
			break;

		case ALPHA_GEN_IDENTITY:
		default:
			for ( i = 0; i < numverts; i++ ) {
				cArray[i][3] = 255;
			}
			break;
	}

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

int R_ShaderpassTex ( shaderpass_t *pass, int lmtex )
{
	if ( pass->flags & SHADER_PASS_ANIMMAP ) {
		int frame = (int)(pass->anim_fps * r_shadertime) % pass->anim_numframes;
		return pass->anim_frames[frame]->texnum;
	} else if ( pass->flags & SHADER_PASS_LIGHTMAP ) {
		return gl_state.lightmap_textures + lmtex;
	}

	return pass->texref ? pass->texref->texnum : -1;
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
		switch ( shader->cull )
		{
			case SHADER_CULL_DISABLE:
				qglDisable (GL_CULL_FACE);
				break;

			case SHADER_CULL_FRONT:
				qglEnable (GL_CULL_FACE);
				qglCullFace (GL_FRONT);
				break;

			case SHADER_CULL_BACK:
				qglEnable (GL_CULL_FACE);
				qglCullFace (GL_BACK);
				break;
		}
	}

	if (shader->flags & SHADER_POLYGONOFFSET)
		qglEnable (GL_POLYGON_OFFSET);
	else
		qglDisable (GL_POLYGON_OFFSET);
}

/*
================
R_SetShaderpassState
================
*/
void R_SetShaderpassState ( shaderpass_t *pass )
{
	if ( pass->flags & SHADER_PASS_BLEND ) {
		GLSTATE_ENABLE_BLEND
		qglBlendFunc (pass->blendsrc, pass->blenddst);
	}
	else {
		GLSTATE_DISABLE_BLEND
	}

	if (pass->flags & SHADER_PASS_ALPHAFUNC) {
		GLSTATE_ENABLE_ALPHATEST
		qglAlphaFunc (pass->alphafunc, pass->alphafuncref);
	} else {
		GLSTATE_DISABLE_ALPHATEST
	}

	// nasty hack!!!
	if ( !gl_state.in2d ) {
		qglDepthFunc (pass->depthfunc);

		if (pass->flags & SHADER_PASS_DEPTHWRITE) {
			qglDepthMask (GL_TRUE);
		} else {
			qglDepthMask (GL_FALSE);
		}
	}
}

/*
================
R_RenderMeshGeneric
================
*/
void R_RenderMeshGeneric ( mesh_t *mesh )
{
	int			i, j, tex;
	int			*lindex;
	shader_t	*shader = mesh->shader;
	shaderpass_t *pass;

	GL_TexEnv ( GL_MODULATE );

	R_SetShaderState ( shader );

	lindex = mesh->firstindex;
	for (i = 0; i < mesh->numindexes; i++, lindex++)
		R_PushElem ( *lindex );
	
	R_DeformVertices ( mesh, NULL, 0 );

	if ( shader->numpasses != 1 ) {
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

		R_SetShaderpassState ( pass );
		R_ModifyTextureCoords ( mesh, j, false, NULL, 0 );
		R_ModifyColour ( mesh, j, false, NULL, 0 );

		if ( shader->numpasses == 1 ) {
			R_LockArrays ();
		}

		R_FlushArrays ();
	}

	R_UnlockArrays ();
	R_ClearArrays ();
}

/*
================
R_RenderMeshMultitextured
================
*/
void R_RenderMeshMultitextured ( mesh_t *mesh )
{
	int			i;
	int			*lindex;
	shader_t	*shader = mesh->shader;
	shaderpass_t *pass1, *pass2;

	pass1 = &shader->pass[0];
	pass2 = &shader->pass[1];

	R_SetShaderState ( shader );

	GL_SelectTexture( GL_TEXTURE0 );
	GL_TexEnv( pass1->blendmode );

	if (pass1->flags & SHADER_PASS_ALPHAFUNC) {
		GLSTATE_ENABLE_ALPHATEST
		qglAlphaFunc (pass1->alphafunc, pass1->alphafuncref);
	}
	else {
		GLSTATE_DISABLE_ALPHATEST
	}

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
	R_UnlockArrays ();
	R_ClearArrays ();

	if (pass1->flags & SHADER_PASS_ALPHAFUNC)
		GLSTATE_DISABLE_ALPHATEST
}

/*
================
R_RenderMeshCombined
================
*/
void R_RenderMeshCombined ( mesh_t *mesh )
{
	int			i;
	int			*lindex;
	shader_t	*shader = mesh->shader;
	shaderpass_t *pass1, *pass2;

	pass1 = &shader->pass[0];
	pass2 = &shader->pass[1];

	R_SetShaderState ( shader );

	GL_SelectTexture( GL_TEXTURE0 );

	if (pass1->flags & SHADER_PASS_ALPHAFUNC) {
		GLSTATE_ENABLE_ALPHATEST
		qglAlphaFunc (pass1->alphafunc, pass1->alphafuncref);
	}
	else {
		GLSTATE_DISABLE_ALPHATEST
	}

	GL_TexEnv ( GL_COMBINE_EXT );
	qglTexEnvi ( GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_REPLACE );
	qglTexEnvi ( GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE );
	qglTexEnvi ( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE );
	qglTexEnvi ( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE );

	GL_SelectTexture( GL_TEXTURE1 );

	if ( pass2->blendmode )
	{
		switch ( pass2->blendmode )
		{
		case GL_MODULATE:
			GL_TexEnv ( GL_COMBINE_EXT );
			qglTexEnvi ( GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE );
			qglTexEnvi ( GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE );
			qglTexEnvi ( GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT );
			qglTexEnvi ( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE );
			qglTexEnvi ( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE );
			qglTexEnvi ( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT );

			if ( r_overbrightbits->value )
				qglTexEnvi ( GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, r_overbrightbits->value );

			break;

		case GL_ADD:
		case GL_BLEND:
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

			if ( r_overbrightbits->value )
				qglTexEnvi ( GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, r_overbrightbits->value );

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

		if ( r_overbrightbits->value )
			qglTexEnvi ( GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, r_overbrightbits->value );
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
	R_UnlockArrays ();
	R_ClearArrays ();

	if (pass1->flags & SHADER_PASS_ALPHAFUNC)
		GLSTATE_DISABLE_ALPHATEST
}

