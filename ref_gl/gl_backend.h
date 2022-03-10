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
typedef struct
{
	vec3_t		position;
    float		tex_st[2];			// texture coords
	float		lm_st[2];			// lightmap texture coords
    vec3_t		normal;				// normal
    float		colour[4];			// colour used for vertex lighting?

	float		patchRel;			// FIXME: this is a bad place
									// for this stuff

	vec3_t		rot_centre;			// for autosprites
} mvertex_t;

typedef struct
{
	shader_t	*shader;

    int			numverts;
	mvertex_t	*firstvert;

    int			numindexes;
    unsigned	*firstindex;

	vec2_t		tex_centre_tc;

	int			patchWidth;
	int			patchHeight;
	int			lightmaptexturenum;

	vec3_t		*lm_mins;			// FACETYPE_MESH, arrays of 'right' and
									// and up vectors for each vertex
	vec3_t		*lm_maxs;
} mesh_t;

void R_InitArrays (void);
void R_LockArrays (void);
void R_UnlockArrays (void);
void R_FlushArrays (void);
void R_ClearArrays (void);
void R_PushElem (unsigned int elem);
void R_PushVertex (float *vertex);
void R_PushCoord (float *tc);
void R_PushColor (float *color);
void R_PushCoordMtex ( float *tc );
void R_PushColorByte ( byte *color );
void R_PushColorByteMtex ( byte *color );
void R_PushColorMtex ( float *color );
void R_FlushArraysMtex (int tex1, int tex2);

void R_DeformVertices ( mesh_t *mesh, vec3_t *out, int outlen );
void R_ModifyTextureCoords ( mesh_t *mesh, int pass, qboolean mtex, vec2_t *out, int outlen );
void R_ModifyColour ( mesh_t *mesh, int pass, qboolean mtex, vec4_t *out, int outlen );

void R_RenderMeshGeneric ( mesh_t *mesh );
void R_RenderMeshMultitextured ( mesh_t *mesh );
void R_RenderMeshCombined ( mesh_t *mesh );


