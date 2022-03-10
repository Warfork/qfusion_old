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
// gl_warp.c

#include "gl_local.h"

float	skymins[2][6], skymaxs[2][6];
float	sky_min, sky_max;

image_t	*skybox_textures[6];
skydome_t r_skydome;

static void Gen_BoxSide (skydome_t *skydome, int side, vec3_t orig, vec3_t drow, vec3_t dcol);
static void Gen_Box (skydome_t *skydome);
static void Gen_Indexes (skydome_t *skydome);

void R_DrawSkyBox (void);
void R_DrawFastSkyBox (void);

void R_CreateSkydome (void)
{
    int i;
	mesh_t *mesh;
	skydome_t *skydome = &r_skydome;

    // alloc space for skydome meshes, etc.
	mesh = skydome->meshes;
	for ( i = 0; i < 5; i++, mesh++ ) {
		mesh->numverts = POINTS_LEN;
		mesh->firstvert = (mvertex_t *)Hunk_Alloc(POINTS_LEN * sizeof(mvertex_t));

		mesh->numindexes = ELEM_LEN;
		mesh->firstindex = skydome->firstindex;

		mesh->lightmaptexturenum = -1;
		mesh->shader = r_skyshader;
	}

    Gen_Box ( skydome );
    Gen_Indexes ( skydome );
}
    
static void Gen_Indexes ( skydome_t *skydome )
{
    int u, v;
    unsigned int *e = skydome->firstindex;

    // Box elems in tristrip order
    for (v = 0; v < SIDE_SIZE-1; v++)
    {
		for (u = 0; u < SIDE_SIZE-1; u++)
		{
			*e++ = v * SIDE_SIZE + u;
			*e++ = (v+1) * SIDE_SIZE + u;
			*e++ = v * SIDE_SIZE + u + 1;
			*e++ = v * SIDE_SIZE + u + 1;
			*e++ = (v+1) * SIDE_SIZE + u;
			*e++ = (v+1) * SIDE_SIZE + u + 1;	    
		}
    }
}
    
static void Gen_Box ( skydome_t *skydome )
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
    Gen_BoxSide (skydome, SKYBOX_TOP, orig, drow, dcol);

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
    Gen_BoxSide (skydome, SKYBOX_FRONT, orig, drow, dcol);

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
    Gen_BoxSide (skydome, SKYBOX_RIGHT, orig, drow, dcol);

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
    Gen_BoxSide (skydome, SKYBOX_BACK, orig, drow, dcol);

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
    Gen_BoxSide (skydome, SKYBOX_LEFT, orig, drow, dcol);
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
static void Gen_BoxSide ( skydome_t *skydome, int side, vec3_t orig, vec3_t drow, vec3_t dcol )
{
    vec3_t pos, w, row, norm;
	mvertex_t *v;
    int r, c;
    float d, b, t;
	extern float r_skyheight;

    d = EYE_RAD;     // sphere center to camera distance
    b = SPHERE_RAD;  // sphere radius
    
    v = skydome->meshes[side].firstvert;
    VectorCopy (orig, row);

	CrossProduct ( drow, dcol, norm );
	VectorNormalize ( norm );

    for (r = 0; r < SIDE_SIZE; r++)
    {
		VectorCopy (row, pos);

		for (c = 0; c < SIDE_SIZE; c++)
		{
			// pos points from eye to vertex on box
			VectorScale ( pos, r_skyheight, v->position );
			VectorCopy ( pos, w );

			// Normalize pos -> w
			VectorNormalizeFast ( w );

			// Find distance along w to sphere
			t = sqrt(d*d*(w[2]*w[2]-1.0) + b*b) - d*w[2];
			w[0] *= t;
			w[1] *= t;

			t = 1 / (2.0 * SCALE_S);

			// use x and y on sphere as s and t
			// minus is here so skies scoll in correct (Q3A's) direction
			v->tex_st[0] = -w[0] * t;
			v->tex_st[1] = -w[1] * t;

			// avoid bilerp seam
			v->tex_st[0] = (v->tex_st[0]+1)*0.5;
			v->tex_st[1] = (v->tex_st[1]+1)*0.5;

			VectorAdd ( pos, dcol, pos );
			VectorCopy ( norm, v->normal );
			v++;
		}

		VectorAdd ( row, drow, row );
    }
}

void R_DrawSkydome (void)
{
    int i;
	skydome_t *skydome = &r_skydome;

	if ( !r_skyshader )
		return;

	GL_EnableMultitexture ( false );

	qglDepthMask ( GL_FALSE );
	R_DrawSkyBox ();

    // center skydome on camera to give the illusion of a larger space
	qglPushMatrix ();
	qglTranslatef ( r_origin[0], r_origin[1], r_origin[2] );

	if ( r_skyshader->numpasses ) {
		for (i = 0; i < 5; i++)
		{
			if (skymins[0][i] >= skymaxs[0][i] || 
				skymins[1][i] >= skymaxs[1][i]) {
				continue;
			}

			r_skyshader->flush ( &skydome->meshes[i], NULL, NULL );
		}
	}

    // restore world space
    qglPopMatrix();
}

void R_DrawFastSky (void)
{
	GL_EnableMultitexture ( false );

    // center skydome on camera to give the illusion of a larger space
	qglPushMatrix ();
	qglDepthMask ( GL_FALSE );

	R_DrawFastSkyBox ();

    // restore world space
    qglPopMatrix();
}

//===================================================================

vec3_t	skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
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

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
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
	mvertex_t	*vert;
	mesh_t		*mesh;
	vec3_t		verts[MAX_CLIP_VERTS];

	// calculate vertex values for sky box
	mesh = &fa->mesh;
	vert = mesh->firstvert;
	for ( i = 0; i < mesh->numverts; i++, vert++ )
		VectorSubtract ( vert->position, r_origin, verts[i] );

	ClipSkyPolygon ( mesh->numverts, verts[0], 0 );
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
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}


void MakeSkyVec (vec5_t skyvec)
{
	vec3_t		v;
	float		tc[2];

	v[0] = skyvec[0] * 4096.0f + r_origin[0];
	v[1] = skyvec[1] * 4096.0f + r_origin[1];
	v[2] = skyvec[2] * 4096.0f + r_origin[2];
	tc[0] = skyvec[3] * (254.0f/256.0f) + (1.0f/256.0f);
	tc[1] = skyvec[4] * (254.0f/256.0f) + (1.0f/256.0f);

	R_PushCoord (tc);
	R_PushVertex (v);
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

void R_DrawSkyBox (void)
{
	int		i;

	if ( !skybox_textures[0] )
		return;

	for (i=0 ; i<6 ; i++)
	{
		GL_Bind ( skybox_textures[i]->texnum );

		R_PushElem (0);
		R_PushElem (1);
		R_PushElem (2);
		R_PushElem (0);
		R_PushElem (2);
		R_PushElem (3);

		MakeSkyVec (skyvecs[i*4+0]);
		MakeSkyVec (skyvecs[i*4+1]);
		MakeSkyVec (skyvecs[i*4+2]);
		MakeSkyVec (skyvecs[i*4+3]);

		R_LockArrays ();
		R_FlushArrays ();
		R_UnlockArrays ();
		R_ClearArrays ();
	}
}

void R_DrawFastSkyBox (void)
{
	int		i;

	qglColor3f(0,0,0);
	qglDisable (GL_TEXTURE_2D);

	for (i=0 ; i<6 ; i++)
	{
		R_PushElem (0);
		R_PushElem (1);
		R_PushElem (2);
		R_PushElem (0);
		R_PushElem (2);
		R_PushElem (3);

		MakeSkyVec (skyvecs[i*4+0]);
		MakeSkyVec (skyvecs[i*4+1]);
		MakeSkyVec (skyvecs[i*4+2]);
		MakeSkyVec (skyvecs[i*4+3]);
	}

	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();

	qglEnable (GL_TEXTURE_2D);
}

/*
============
R_SetSky
============
*/
// 3dstudio environment map names
char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
void R_SetSky (char *skyname)
{
	int		i;
	char	pathname[MAX_QPATH];

	if (skyname[0] == '-')
	{
		for (i=0 ; i<6 ; i++)
			skybox_textures[i] = NULL;

		return;
	}

	for (i=0 ; i<6 ; i++)
	{
		Com_sprintf (pathname, sizeof(pathname), "%s_%s", skyname, suf[i]);

		skybox_textures[i] = GL_FindImage ( pathname, IT_NOMIPMAP );

		sky_min = 1.0/512.0;
		sky_max = 511.0/512.0;
	}
}
