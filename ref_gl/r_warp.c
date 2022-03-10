/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// r_warp.c

#include "r_local.h"


#define SIDE_SIZE	9
#define POINTS_LEN	(SIDE_SIZE*SIDE_SIZE)
#define ELEM_LEN	((SIDE_SIZE-1)*(SIDE_SIZE-1)*6)

#define SPHERE_RAD	10.0
#define EYE_RAD     9.0

#define SCALE_S		4.0  // Arbitrary (?) texture scaling factors
#define SCALE_T		4.0 

#define BOX_SIZE	1.0f
#define BOX_STEP	BOX_SIZE / (SIDE_SIZE-1) * 2.0f

index_t			r_skydome_indexes[ELEM_LEN];
meshbuffer_t	r_skydome_mbuffer;

mfog_t			*r_skyfog;
msurface_t		*r_warpface;

static void Gen_BoxSide ( skydome_t *skydome, int side, vec3_t orig, vec3_t drow, vec3_t dcol, float skyheight );
static void Gen_Box ( skydome_t *skydome, float skyheight );

void R_DrawSkyBox (image_t **skybox_textures, int texenv);
void R_DrawFastSkyBox (void);

void R_InitSkydome (void)
{
    int u, v;
    index_t *e = r_skydome_indexes;

    // Box elems in tristrip order
    for (v = 0; v < SIDE_SIZE-1; v++)
    {
		for (u = 0; u < SIDE_SIZE-1; u++)
		{
			e[0] = v * SIDE_SIZE + u;
			e[1] = e[4] = e[0] + SIDE_SIZE;
			e[2] = e[3] = e[0] + 1;
			e[5] = e[1] + 1;
			e += 6;
		}
    }

	r_skydome_mbuffer.entity = &r_worldent;
}

void R_CreateSkydome (shader_t *shader, float skyheight)
{
    int i;
	mesh_t *mesh;
	skydome_t *skydome;
	meshbuffer_t *mbuffer = &r_skydome_mbuffer;

	mbuffer->dlightbits = 0;
	mbuffer->infokey = -1;
	mbuffer->shader = shader;

	skydome = shader->skydome;
	mesh = skydome->meshes;
	for ( i = 0; i < 5; i++, mesh++ ) {
		mesh->numvertexes = POINTS_LEN;
		mesh->xyz_array = Z_Malloc ( sizeof(vec4_t) * POINTS_LEN );
		mesh->normals_array = Z_Malloc ( sizeof(vec3_t) * POINTS_LEN );
		mesh->st_array = Z_Malloc ( sizeof(vec2_t) * POINTS_LEN );

		mesh->numindexes = ELEM_LEN;
		mesh->indexes = r_skydome_indexes;
		mesh->trneighbors = NULL;
	}

	Gen_Box ( skydome, skyheight );
}
    
static void Gen_Box ( skydome_t *skydome, float skyheight )
{
    vec3_t orig, drow, dcol;
    
    // Top
    orig[0] = -BOX_SIZE;
    orig[1] = BOX_SIZE;
    orig[2] = BOX_SIZE;
    drow[0] = 0.0f;
    drow[1] = -BOX_STEP;
    drow[2] = 0.0f;
    dcol[0] = BOX_STEP;
    dcol[1] = 0.0f;
    dcol[2] = 0.0f;
    Gen_BoxSide ( skydome, SKYBOX_TOP, orig, drow, dcol, skyheight );

    // Front
    orig[0] = BOX_SIZE;
    orig[1] = BOX_SIZE;
    orig[2] = BOX_SIZE;
    drow[0] = 0.0f;
    drow[1] = 0.0f;
    drow[2] = -BOX_STEP;
    dcol[0] = -BOX_STEP;
    dcol[1] = 0.0f;
    dcol[2] = 0.0f;
    Gen_BoxSide ( skydome, SKYBOX_FRONT, orig, drow, dcol, skyheight );

    // Right
    orig[0] = BOX_SIZE;
    orig[1] = -BOX_SIZE;
    orig[2] = BOX_SIZE;
    drow[0] = 0.0f;
    drow[1] = 0.0f;
    drow[2] = -BOX_STEP;
    dcol[0] = 0.0f;
    dcol[1] = BOX_STEP;
    dcol[2] = 0.0f;
    Gen_BoxSide ( skydome, SKYBOX_RIGHT, orig, drow, dcol, skyheight );

    // Back
    orig[0] = -BOX_SIZE;
    orig[1] = -BOX_SIZE;
    orig[2] = BOX_SIZE;
    drow[0] = 0.0f;
    drow[1] = 0.0f;
    drow[2] = -BOX_STEP;
    dcol[0] = BOX_STEP;
    dcol[1] = 0.0f;
    dcol[2] = 0.0f;
    Gen_BoxSide ( skydome, SKYBOX_BACK, orig, drow, dcol, skyheight );

    // Left
    orig[0] = -BOX_SIZE;
    orig[1] = BOX_SIZE;
    orig[2] = BOX_SIZE;
    drow[0] = 0.0f;
    drow[1] = 0.0f;
    drow[2] = -BOX_STEP;
    dcol[0] = 0.0f;
    dcol[1] = -BOX_STEP;
    dcol[2] = 0.0f;
    Gen_BoxSide ( skydome, SKYBOX_LEFT, orig, drow, dcol, skyheight );
}

/*
================
Gen_BoxSide

I don't know exactly what Q3A does for skybox 
texturing, but this is at least fairly close.  
We tile the texture onto the inside of a large 
sphere, and put the camera near the top of the sphere.
We place the box around the camera, and cast rays
through the box verts to the sphere to find the 
texture coordinates.
================
*/
static void Gen_BoxSide ( skydome_t *skydome, int side, vec3_t orig, vec3_t drow, vec3_t dcol, float skyheight )
{
    vec3_t pos, w, row, norm;
	float *v, *n, *st;
    int r, c;
	double t;
    float d, d2, b, b2, q;

	d = EYE_RAD;     // sphere center to camera distance
	d2 = d * d;
	b = SPHERE_RAD;  // sphere radius
	b2 = b * b;
	q = 1.0 / (2.0 * SCALE_S);

	v = skydome->meshes[side].xyz_array[0];
	n = skydome->meshes[side].normals_array[0];
	st = skydome->meshes[side].st_array[0];

	VectorCopy (orig, row);

	CrossProduct ( drow, dcol, norm );
	VectorNormalize ( norm );

	for (r = 0; r < SIDE_SIZE; r++)
	{
		VectorCopy (row, pos);

		for (c = 0; c < SIDE_SIZE; c++)
		{
			// pos points from eye to vertex on box
			VectorScale ( pos, skyheight, v );
			VectorCopy ( pos, w );

			// Normalize pos -> w
			VectorNormalize ( w );

			// Find distance along w to sphere
			t = sqrt ( d2 * (w[2] * w[2] - 1.0) + b2 ) - d * w[2];
			w[0] *= t;
			w[1] *= t;

			// use x and y on sphere as s and t
			// minus is here so skies scoll in correct (Q3A's) direction
			st[0] = -w[0] * q;
			st[1] = -w[1] * q;

			// avoid bilerp seam
			st[0] = (st[0] + 1) * 0.5;
			st[1] = (st[1] + 1) * 0.5;

			VectorAdd ( pos, dcol, pos );
			VectorCopy ( norm, n );

			v += 4;
			n += 3;
			st += 2;
		}

		VectorAdd ( row, drow, row );
	}
}

void R_DrawSky (shader_t *shader)
{
    int i, features;
	mat4_t m;
	skydome_t *skydome = shader ? shader->skydome : NULL;
	meshbuffer_t *mbuffer = &r_skydome_mbuffer;

	if ( !skydome && !r_fastsky->value ) {
		return;
	}

    // center skydome on camera to give the illusion of a larger space
	Matrix4_Copy ( r_modelview_matrix, m );
	m[12] = 0;
	m[13] = 0;
	m[14] = 0;
	m[15] = 1.0;

	qglLoadMatrixf ( m );

	gldepthmin = 1;
	gldepthmax = 1;
	qglDepthRange( gldepthmin, gldepthmax );

	GL_EnableMultitexture ( false );
	qglDepthMask ( GL_FALSE );
	GLSTATE_DISABLE_ALPHATEST;
	GLSTATE_DISABLE_BLEND;

	if ( r_fastsky->value ) {
		R_DrawFastSkyBox ();
	} else {
		qboolean flush = false;

		R_DrawSkyBox ( skydome->farbox_textures, GL_REPLACE );

		features = mbuffer->shader->features;
		if ( r_shownormals->value ) {
			features |= MF_NORMALS;
		}

		if ( shader->numpasses ) {
			for ( i = 0; i < 5; i++ )
			{
				if (currentlist->skymins[0][i] >= currentlist->skymaxs[0][i] || 
					currentlist->skymins[1][i] >= currentlist->skymaxs[1][i]) {
					continue;
				}

				flush = true;
				mbuffer->fog = r_skyfog;
				mbuffer->mesh = skydome->meshes + i;

				R_PushMesh ( &skydome->meshes[i], features );
			}
		}

		if ( flush ) {
			R_RenderMeshBuffer ( mbuffer, false );
		}

		if ( skydome->nearbox_textures[0] ) {
			GL_EnableMultitexture ( false );
			qglDepthMask ( GL_FALSE );
			GLSTATE_ENABLE_ALPHATEST;
			R_DrawSkyBox ( skydome->nearbox_textures, GL_MODULATE );
			GLSTATE_DISABLE_ALPHATEST;
		}
	}

	qglLoadMatrixf ( r_modelview_matrix );

	gldepthmin = 0;
	gldepthmax = 1;
	qglDepthRange( gldepthmin, gldepthmax );

	r_skyfog = NULL;
}

//===================================================================

vec3_t	skyclip[6] = {
	{  1,  1,  0  },
	{  1, -1,  0  },
	{  0, -1,  1  },
	{  0,  1,  1  },
	{  1,  0,  1  },
	{ -1,  0,  1  } 
};

// 1 = s, 2 = t, 3 = 2048
int	st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down
};

// s = [0]/[2], t = [1]/[2]
int	vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}
};

void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i, j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	// decide which face it maps to
	VectorClear ( v );

	for ( i = 0, vp = vecs; i < nump; i++, vp += 3 )
		VectorAdd ( vp, v, v );

	av[0] = fabs( v[0] );
	av[1] = fabs( v[1] );
	av[2] = fabs( v[2] );

	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	if ( !r_skyfog )
		r_skyfog = r_warpface->fog;

	// project new texture coords
	for ( i = 0; i < nump; i++, vecs += 3 )
	{
		j = vec_to_st[axis][2];

		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		if (dv < 0.001)
			continue;	// don't divide by zero

		j = vec_to_st[axis][0];

		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;

		j = vec_to_st[axis][1];

		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (s < currentlist->skymins[0][axis])
			currentlist->skymins[0][axis] = s;
		if (t < currentlist->skymins[1][axis])
			currentlist->skymins[1][axis] = t;
		if (s > currentlist->skymaxs[0][axis])
			currentlist->skymaxs[0][axis] = s;
		if (t > currentlist->skymaxs[1][axis])
			currentlist->skymaxs[1][axis] = t;
	}
}

#define	ON_EPSILON		0.1			// point on plane side epsilon
#define	MAX_CLIP_VERTS	64

void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	float	*norm;
	float	*v;
	qboolean	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		Com_Error (ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");

	if (stage == 6)
	{	// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

/*
=================
R_AddSkySurface
=================
*/
void R_AddSkySurface ( msurface_t *fa )
{
	int			i;
	vec4_t		*vert;
	mesh_t		*mesh;
	vec3_t		verts[MAX_CLIP_VERTS];

	// calculate vertex values for sky box
	r_warpface = fa;
	mesh = fa->mesh;
	vert = mesh->xyz_array;
	for ( i = 0; i < mesh->numvertexes; i++, vert++ )
		VectorSubtract ( *vert, r_origin, verts[i] );

	ClipSkyPolygon ( mesh->numvertexes, verts[0], 0 );
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void)
{
	int		i;

	for (i=0 ; i<6 ; i++)
	{
		currentlist->skymins[0][i] = currentlist->skymins[1][i] = 9999;
		currentlist->skymaxs[0][i] = currentlist->skymaxs[1][i] = -9999;
	}
}


void MakeSkyVec (vec5_t skyvec)
{
	vec3_t		v;
	float		st[2];

	v[0] = skyvec[0] * 4096.0f;
	v[1] = skyvec[1] * 4096.0f;
	v[2] = skyvec[2] * 4096.0f;
	st[0] = skyvec[3] * (254.0f/256.0f) + (1.0f/256.0f);
	st[1] = skyvec[4] * (254.0f/256.0f) + (1.0f/256.0f);

	qglTexCoord2fv (st);
	qglVertex3fv (v);
}

/*
==============
R_DrawSkyBox
==============
*/
static vec5_t	skyvecs[24] = 
{
	{ 1,  1,  1, 1, 0 }, {  1,  1, -1, 1, 1 }, { -1,  1, -1, 0, 1 }, { -1,  1,  1, 0, 0 },
	{ -1,  1,  1, 1, 0 }, { -1,  1, -1, 1, 1 }, { -1, -1, -1, 0, 1 }, { -1, -1,  1, 0, 0 },
	{ -1, -1,  1, 1, 0 }, { -1, -1, -1, 1, 1 }, {  1, -1, -1, 0, 1 }, {  1, -1,  1, 0, 0 },
	{  1, -1,  1, 1, 0 }, {  1, -1, -1, 1, 1 }, {  1,  1, -1, 0, 1 }, {  1,  1,  1, 0, 0 },
	{  1, -1,  1, 1, 0 }, {  1,  1,  1, 1, 1 }, { -1,  1,  1, 0, 1 }, { -1, -1,  1, 0, 0 },
	{  1,  1, -1, 1, 0 }, {  1, -1, -1, 1, 1 }, { -1, -1, -1, 0, 1 }, { -1,  1, -1, 0, 0 }
};

void R_DrawSkyBox (image_t **skybox_textures, int texenv)
{
	int		i;

	GL_TexEnv ( texenv );

	if ( !skybox_textures[0] ) {
		return;
	}

	for (i=0 ; i<6 ; i++)
	{
		GL_Bind ( skybox_textures[i]->texnum );

		qglBegin ( GL_QUADS );

		MakeSkyVec (skyvecs[i*4+0]);
		MakeSkyVec (skyvecs[i*4+1]);
		MakeSkyVec (skyvecs[i*4+2]);
		MakeSkyVec (skyvecs[i*4+3]);

		qglEnd ();
	}
}

void R_DrawFastSkyBox (void)
{
	int		i;

	qglDisable (GL_TEXTURE_2D);
	qglColor3f(0,0,0);
	qglBegin ( GL_QUADS );

	for (i=0 ; i<6 ; i++)
	{
		MakeSkyVec (skyvecs[i*4+0]);
		MakeSkyVec (skyvecs[i*4+1]);
		MakeSkyVec (skyvecs[i*4+2]);
		MakeSkyVec (skyvecs[i*4+3]);
	}

	qglEnd ();
	qglEnable (GL_TEXTURE_2D);
}
