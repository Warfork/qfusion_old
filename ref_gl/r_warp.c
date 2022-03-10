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

index_t			r_skydome_indexes[6][ELEM_LEN];
meshbuffer_t	r_skydome_mbuffer;

mfog_t			*r_skyfog;
msurface_t		*r_warpface;

float			sky_min, sky_max;

static void Gen_BoxSide ( skydome_t *skydome, int side, vec3_t orig, vec3_t drow, vec3_t dcol, float skyheight );
static void Gen_Box ( skydome_t *skydome, float skyheight );

void R_DrawSkyBox (skydome_t *skydome, shader_t **shaders);
void R_DrawFastSkyBox (void);
void MakeSkyVec (float x, float y, float z, int axis, vec3_t v);

void R_InitSkydome (void)
{
	meshbuffer_t *mbuffer = &r_skydome_mbuffer;

	sky_min = 1.0/512;
	sky_max = 511.0/512;

	mbuffer->dlightbits = 0;
	mbuffer->entity = &r_worldent;
}

void R_CreateSkydome ( skydome_t *skydome, float skyheight )
{
    int i;
	mesh_t *mesh;

	mesh = skydome->meshes;
	for ( i = 0; i < 6; i++, mesh++ ) {
		mesh->numvertexes = POINTS_LEN;
		mesh->xyz_array = Shader_Malloc ( sizeof(vec4_t) * POINTS_LEN );
		mesh->normals_array = Shader_Malloc ( sizeof(vec3_t) * POINTS_LEN );

		if ( i != 5 ) {
			skydome->sphereStCoords[i] = Shader_Malloc ( sizeof(vec2_t) * POINTS_LEN );
		}
		skydome->linearStCoords[i] = Shader_Malloc ( sizeof(vec2_t) * POINTS_LEN );

		mesh->numindexes = ELEM_LEN;
		mesh->indexes = r_skydome_indexes[i];
#ifdef SHADOW_VOLUMES
		mesh->trneighbors = NULL;
#endif
	}

	Gen_Box ( skydome, skyheight );
}

void R_FreeSkydome ( skydome_t *skydome )
{
	int i;

	for ( i = 0; i < 6; i++ ) {
		Shader_Free ( skydome->meshes[i].xyz_array );
		Shader_Free ( skydome->meshes[i].normals_array );
		if ( i != 5 )
			Shader_Free ( skydome->sphereStCoords[i] );
		Shader_Free ( skydome->linearStCoords[i] );
	}

	Shader_Free ( skydome );
}

static void Gen_Box ( skydome_t *skydome, float skyheight )
{
	int axis;
	vec3_t orig, drow, dcol;

	for (axis=0 ; axis<6 ; axis++)
	{
		MakeSkyVec ( -BOX_SIZE, -BOX_SIZE, BOX_SIZE, axis, orig );
		MakeSkyVec ( 0, BOX_STEP, 0, axis, drow );
		MakeSkyVec ( BOX_STEP, 0, 0, axis, dcol );

	    Gen_BoxSide ( skydome, axis, orig, drow, dcol, skyheight );
	}
}

/*
================
Gen_BoxSide

I don't know exactly what Q3A does for skybox texturing, but
this is at least fairly close.  We tile the texture onto the 
inside of a large sphere, and put the camera near the top of 
the sphere. We place the box around the camera, and cast rays
through the box verts to the sphere to find the texture coordinates.
================
*/
static void Gen_BoxSide ( skydome_t *skydome, int side, vec3_t orig, vec3_t drow, vec3_t dcol, float skyheight )
{
    vec3_t pos, w, row, norm;
	float *v, *n, *st = NULL, *st2;
    int r, c;
    float t, d, d2, b, b2, q[2], s;

	s = 1.0 / (SIDE_SIZE-1);
	d = EYE_RAD;     // sphere center to camera distance
	d2 = d * d;
	b = SPHERE_RAD;  // sphere radius
	b2 = b * b;
	q[0] = 1.0 / (2.0 * SCALE_S);
	q[1] = 1.0 / (2.0 * SCALE_T);

	v = skydome->meshes[side].xyz_array[0];
	n = skydome->meshes[side].normals_array[0];
	if ( side != 5 )
		st = skydome->sphereStCoords[side][0];
	st2 = skydome->linearStCoords[side][0];

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

			if ( st ) {
				// use x and y on sphere as s and t
				// minus is here so skies scoll in correct (Q3A's) direction
				st[0] = -w[0] * q[0];
				st[1] = -w[1] * q[1];

				// avoid bilerp seam
				st[0] = (st[0] + 1) * 0.5;
				st[1] = (st[1] + 1) * 0.5;
			}

			st2[0] = c * s;
			st2[1] = 1.0 - r * s;

			VectorAdd ( pos, dcol, pos );
			VectorCopy ( norm, n );

			v += 4;
			n += 3;
			if ( st ) st += 2;
			st2 += 2;
		}

		VectorAdd ( row, drow, row );
	}
}

void R_DrawSkyBox (skydome_t *skydome, shader_t **shaders)
{
	int i, features;
	meshbuffer_t *mbuffer = &r_skydome_mbuffer;
	int	skytexorder[6] = { SKYBOX_RIGHT, SKYBOX_FRONT, SKYBOX_LEFT, SKYBOX_BACK, SKYBOX_TOP, SKYBOX_BOTTOM };

	features = shaders[0]->features;
	if ( r_shownormals->value ) {
		features |= MF_NORMALS;
	}


	for ( i = 0; i < 6; i++ ) {
		if (r_currentlist->skymins[0][i] >= r_currentlist->skymaxs[0][i] || 
			r_currentlist->skymins[1][i] >= r_currentlist->skymaxs[1][i]) {
			continue;
		}

		mbuffer->fog = r_skyfog;
		mbuffer->shader = shaders[skytexorder[i]];
		
		skydome->meshes[i].st_array = skydome->linearStCoords[i];
		R_PushMesh ( &skydome->meshes[i], features );
		R_RenderMeshBuffer ( mbuffer, qfalse );
	}
}

void R_DrawSky (shader_t *shader)
{
    int i;
	mat4_t m;
    index_t *index;
	skydome_t *skydome = shader ? shader->skydome : NULL;
	meshbuffer_t *mbuffer = &r_skydome_mbuffer;
    int u, v, umin, umax, vmin, vmax;

	if ( !skydome && !r_fastsky->value ) {
		return;
	}

    // center skydome on camera to give the illusion of a larger space
	Matrix4_Copy ( r_worldview_matrix, m );
	m[12] = 0;
	m[13] = 0;
	m[14] = 0;
	m[15] = 1.0;

	qglLoadMatrixf ( m );

	gldepthmin = 1;
	gldepthmax = 1;
	qglDepthRange( gldepthmin, gldepthmax );

	GL_EnableMultitexture ( qfalse );
	qglDepthMask ( GL_FALSE );
	qglDisable ( GL_ALPHA_TEST );
	qglDisable ( GL_BLEND );

	if ( r_fastsky->value ) {
		R_DrawFastSkyBox ();
	} else {
		if ( r_portalview || r_mirrorview ) {
			qglDisable ( GL_CLIP_PLANE0 );
		}

		for ( i = 0; i < 6; i++ )
		{
			if (r_currentlist->skymins[0][i] >= r_currentlist->skymaxs[0][i] || 
				r_currentlist->skymins[1][i] >= r_currentlist->skymaxs[1][i]) {
				continue;
			}

			umin = (int)((r_currentlist->skymins[0][i]+1.0f)*0.5f*(float)(SIDE_SIZE-1));
			umax = (int)((r_currentlist->skymaxs[0][i]+1.0f)*0.5f*(float)(SIDE_SIZE-1)) + 1;
			vmin = (int)((r_currentlist->skymins[1][i]+1.0f)*0.5f*(float)(SIDE_SIZE-1));
			vmax = (int)((r_currentlist->skymaxs[1][i]+1.0f)*0.5f*(float)(SIDE_SIZE-1)) + 1;

			clamp ( umin, 0, SIDE_SIZE-1 );
			clamp ( umax, 0, SIDE_SIZE-1 );
			clamp ( vmin, 0, SIDE_SIZE-1 );
			clamp ( vmax, 0, SIDE_SIZE-1 );

			// Box indexes in tristrip order
			index = skydome->meshes[i].indexes;
			for (v = vmin; v < vmax; v++)
			{
				for (u = umin; u < umax; u++)
				{
					index[0] = v * SIDE_SIZE + u;
					index[1] = index[4] = index[0] + SIDE_SIZE;
					index[2] = index[3] = index[0] + 1;
					index[5] = index[1] + 1;
					index += 6;
				}
			}

			skydome->meshes[i].numindexes = (vmax-vmin)*(umax-umin)*6;
		}

		if ( skydome->farbox_shaders[0] ) {
			R_DrawSkyBox ( skydome, skydome->farbox_shaders );
		}

		if ( shader->numpasses ) {
			qboolean flush = qfalse;
			int features = shader->features;

			if ( r_shownormals->value ) {
				features |= MF_NORMALS;
			}

			for ( i = 0; i < 5; i++ )
			{
				if (r_currentlist->skymins[0][i] >= r_currentlist->skymaxs[0][i] || 
					r_currentlist->skymins[1][i] >= r_currentlist->skymaxs[1][i]) {
					continue;
				}

				flush = qtrue;
				mbuffer->fog = r_skyfog;
				mbuffer->shader = shader;

				skydome->meshes[i].st_array = skydome->sphereStCoords[i];
				R_PushMesh ( &skydome->meshes[i], features );
			}

			if ( flush ) {
				R_RenderMeshBuffer ( mbuffer, qfalse );
			}
		}

		if ( skydome->nearbox_shaders[0] ) {
			R_DrawSkyBox ( skydome, skydome->nearbox_shaders );
		}

		if ( r_portalview || r_mirrorview ) {
			qglEnable ( GL_CLIP_PLANE0 );
		}
	}

	qglLoadMatrixf ( r_worldview_matrix );

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

	for ( i = 0, vp = vecs; i < nump; i++, vp += 3 ) {
		VectorAdd ( vp, v, v );
	}

	av[0] = fabs( v[0] );
	av[1] = fabs( v[1] );
	av[2] = fabs( v[2] );

	if ( (av[0] > av[1]) && (av[0] > av[2]) ) {
		axis = (v[0] < 0) ? 1 : 0;
	} else if ( (av[1] > av[2]) && (av[1] > av[0]) ) {
		axis = (v[1] < 0) ? 3 : 2;
	} else {
		axis = (v[2] < 0) ? 5 : 4;
	}

	if ( !r_skyfog )
		r_skyfog = r_warpface->fog;

	// project new texture coords
	for ( i = 0; i < nump; i++, vecs += 3 )
	{
		j = vec_to_st[axis][2];
		dv = (j > 0) ? vecs[j - 1] : -vecs[-j - 1];

		if (dv < 0.001)
			continue;	// don't divide by zero

		dv = 1.0f / dv;

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j -1] * dv : vecs[j-1] * dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j -1] * dv : vecs[j-1] * dv;

		if (s < r_currentlist->skymins[0][axis])
			r_currentlist->skymins[0][axis] = s;
		if (t < r_currentlist->skymins[1][axis])
			r_currentlist->skymins[1][axis] = t;
		if (s > r_currentlist->skymaxs[0][axis])
			r_currentlist->skymaxs[0][axis] = s;
		if (t > r_currentlist->skymaxs[1][axis])
			r_currentlist->skymaxs[1][axis] = t;
	}
}

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

	front = back = qfalse;
	norm = skyclip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = qtrue;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = qtrue;
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
	index_t		*index;
	mesh_t		*mesh;
	vec3_t		verts[MAX_CLIP_VERTS];

	// calculate vertex values for sky box
	r_warpface = fa;
	mesh = fa->mesh;
	index = mesh->indexes;
	vert = mesh->xyz_array;
	for ( i = 0; i < mesh->numindexes; i += 3, index += 3 ) {
		VectorSubtract ( vert[index[0]], r_origin, verts[0] );
		VectorSubtract ( vert[index[1]], r_origin, verts[1] );
		VectorSubtract ( vert[index[2]], r_origin, verts[2] );
		ClipSkyPolygon ( 3, verts[0], 0 );
	}
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
		r_currentlist->skymins[0][i] = r_currentlist->skymins[1][i] = 9999999;
		r_currentlist->skymaxs[0][i] = r_currentlist->skymaxs[1][i] = -9999999;
	}
}


void MakeSkyVec (float x, float y, float z, int axis, vec3_t v)
{
	int		j, k;
	vec3_t 	b;

	b[0] = x;
	b[1] = y;
	b[2] = z;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
	}
}

void MakeSkyVec2 (float s, float t, int axis)
{
	vec3_t	v;

	MakeSkyVec ( s * 4600, t * 4600, 4600, axis, v );

	// avoid bilerp seam
	s = (s + 1) * 0.5;
	t = (t + 1) * 0.5;

	clamp ( s, sky_min, sky_max );
	clamp ( t, sky_min, sky_max );

	qglTexCoord2f ( s, 1.0 - t );
	qglVertex3fv ( v );
}

/*
==============
R_DrawFastSkyBox
==============
*/
void R_DrawFastSkyBox (void)
{
	int		i;

	qglDisable (GL_TEXTURE_2D);
	qglColor3f(0,0,0);
	qglBegin ( GL_QUADS );

	for (i=0 ; i<6 ; i++)
	{
		MakeSkyVec2 (-1, -1, i);
		MakeSkyVec2 (-1, 1, i);
		MakeSkyVec2 (1, 1, i);
		MakeSkyVec2 (1, -1, i);
	}

	qglEnd ();
	qglEnable (GL_TEXTURE_2D);
}
