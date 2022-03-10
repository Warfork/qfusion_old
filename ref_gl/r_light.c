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
// r_light.c

#include "r_local.h"

#define DLIGHT_SCALE	1.0f

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

void R_RenderDlight (dlight_t *light, float div)
{
	int		i, j;
	vec3_t	v, v_right, v_up;
	float	dist, rad = light->intensity * 0.35;
	extern float bubble_sintable[], bubble_costable[];
	float *bub_sin = bubble_sintable, *bub_cos = bubble_costable;

	VectorSubtract (light->origin, r_origin, v);
	dist = VectorNormalize ( v );

	if ( dist < rad ) {
		return;
	}

	MakeNormalVectors ( v, v_right, v_up );

	if (dist - rad > 8)
		VectorMA ( light->origin, -rad, v, v );
	else {
		// make sure the light bubble will not be clipped by
		// near z clip plane
		VectorMA ( light->origin, 8 - dist, v, v );
	}

	qglBegin ( GL_TRIANGLE_FAN );
	qglColor3f ( light->color[0]*div, light->color[1]*div, light->color[2]*div );
	qglVertex3fv ( v );
	qglColor3f ( 0, 0, 0 );

	for ( i = 32; i >= 0; i--, bub_sin++, bub_cos++ ) {
		for (j = 0; j < 3; j++)
			v[j] = light->origin[j] + (v_right[j]*(*bub_cos) +
				+ v_up[j]*(*bub_sin)) * rad;

		qglVertex3fv ( v );
	}

	qglEnd ();
}

/*
=============
R_RenderDlights
=============
*/
void R_RenderDlights (void)
{
	int			i;
	dlight_t	*l;
	float		div;

	if ( r_dynamiclight->value != 2 || !r_newrefdef.num_dlights || r_vertexlight->value )
		return;

	div = 0.2*gl_state.inv_pow2_ovrbr;

	qglDepthMask ( GL_FALSE );
	qglDisable ( GL_TEXTURE_2D );
	qglEnable ( GL_BLEND );
	qglBlendFunc ( GL_ONE, GL_ONE );

	l = r_newrefdef.dlights;
	for (i=0 ; i<r_newrefdef.num_dlights ; i++, l++)
		R_RenderDlight ( l, div );

	qglColor3f ( 1, 1, 1 );
	qglDisable ( GL_BLEND );
	qglEnable ( GL_TEXTURE_2D );
	qglDisable ( GL_BLEND );
	qglDepthMask ( GL_TRUE );
}

/*
=============
R_SurfMarkLight
=============
*/
void R_SurfMarkLight (dlight_t *light, int bit, msurface_t *surf)
{
	shader_t *shader;
	mshaderref_t *shaderref;

	shaderref = surf->shaderref;
	if ( !shaderref || (shaderref->flags & (SURF_SKY|SURF_NODLIGHT)) ) {
		return;
	}

	shader = shaderref->shader;
	if ( (shader->flags & (SHADER_SKY|SHADER_FLARE)) || !shader->numpasses ) {
		return;
	}

	if ( surf->dlightframe != r_framecount ) {
		surf->dlightbits = 0;
		surf->dlightframe = r_framecount;
	}

	surf->dlightbits |= bit;
}

/*
=============
R_MarkLightWorldNode
=============
*/
void R_MarkLightWorldNode (dlight_t *light, int bit, mnode_t *node)
{
	cplane_t	*splitplane;
	msurface_t	*surf;
	float		dist;

mark0:
	if ( node->plane == NULL )
	{
		msurface_t	**mark;
		mleaf_t		*pleaf;
		mesh_t		*mesh;
		int			c;

		pleaf = (mleaf_t *)node;

		if ( pleaf->visframe != r_visframecount ) {
			return;
		}

		// check for door connected areas
		if (r_newrefdef.areabits)
		{
			if (! (r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) ) {
				return;		// not visible
			}
		}

		if ( !BoundsAndSphereIntersect (pleaf->mins, pleaf->maxs, light->origin, light->intensity) ) {
			return;
		}

		// mark the polygons
		c = pleaf->nummarksurfaces;
		mark = pleaf->firstmarksurface;

		do
		{
			surf = *mark++;

			if ( (mesh = surf->mesh) && (surf->facetype != FACETYPE_FLARE) ) {
				if ( !BoundsAndSphereIntersect (mesh->mins, mesh->maxs, light->origin, light->intensity) ) {
					continue;
				}
			}

			R_SurfMarkLight ( light, bit, surf );
		} while (--c);

		return;
	}

	splitplane = node->plane;
	dist = PlaneDiff (light->origin, splitplane);

	if ( dist > light->intensity )
	{
		node = node->children[0];
		goto mark0;
	}
	if ( dist < -light->intensity )
	{
		node = node->children[1];
		goto mark0;
	}

	R_MarkLightWorldNode (light, bit, node->children[0]);
	R_MarkLightWorldNode (light, bit, node->children[1]);
}

/*
=================
R_MarkLights
=================
*/
void R_MarkLights (void)
{
	int			k;
	dlight_t	*lt;

	if ( r_dynamiclight->value != 1 || !r_newrefdef.num_dlights || r_vertexlight->value ) {
		return;
	}

	lt = r_newrefdef.dlights;
	for (k=0 ; k<r_newrefdef.num_dlights ; k++, lt++)
	{
		R_MarkLightWorldNode (lt, 1<<k, r_worldbmodel->nodes);
	}
}

/*
=================
R_AddDynamicLights
=================
*/
void R_AddDynamicLights ( meshbuffer_t *mb )
{
	dlight_t *light;
	int i, j, lnum;
	vec3_t point, tvec, dlorigin;
	vec3_t vright, vup;
	vec3_t dir1, dir2, normal, right, up, oldnormal;
	float *v[3], dist, scale;
	index_t *oldIndexesArray, index[3];
	int dlightNumIndexes, oldNumIndexes;

	oldNumIndexes = numIndexes;
	oldIndexesArray	= indexesArray;
	VectorClear ( oldnormal );

	GL_Bind ( r_dlighttexture->texnum );

	qglDepthFunc ( GL_EQUAL );
	qglBlendFunc ( GL_DST_COLOR, GL_ONE );

	light = r_newrefdef.dlights;
	for ( lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, light++ )
	{
		if ( !(mb->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		VectorSubtract ( light->origin, currententity->origin, dlorigin );
		if ( !Matrix3_Compare (currententity->axis, axis_identity) )
		{
			VectorCopy ( dlorigin, point );
			Matrix3_Multiply_Vec3 ( currententity->axis, point, dlorigin );
		}

		qglColor3fv ( light->color );

		R_ResetTexState ();
		dlightNumIndexes = 0;

		for ( i = 0; i < oldNumIndexes; i += 3 )
		{
			index[0] = oldIndexesArray[i+0];
			index[1] = oldIndexesArray[i+1];
			index[2] = oldIndexesArray[i+2];

			v[0] = (float *)(vertexArray + index[0]);
			v[1] = (float *)(vertexArray + index[1]);
			v[2] = (float *)(vertexArray + index[2]);

			// calculate two mostly perpendicular edge directions
			VectorSubtract ( v[0], v[1], dir1 );
			VectorSubtract ( v[2], v[1], dir2 );

			// we have two edge directions, we can calculate a third vector from
			// them, which is the direction of the surface normal
			CrossProduct ( dir1, dir2, normal );
			VectorNormalize ( normal );

			VectorSubtract ( v[0], dlorigin, tvec );
			dist = DotProduct ( tvec, normal );
			if ( dist < 0 )
				dist = -dist;
			if ( dist >= light->intensity ) {
				continue;
			}

			VectorMA ( dlorigin, -dist, normal, point );
			scale = DLIGHT_SCALE / (light->intensity - dist);

			if ( !VectorCompare (normal, oldnormal) ) {
				MakeNormalVectors ( normal, right, up );
				VectorCopy ( normal, oldnormal );
			}

			VectorScale ( right, scale, vright );
			VectorScale ( up, scale, vup );

			for ( j = 0; j < 3; j++ )
			{		
				// Get our texture coordinates
				// Project the light image onto the face
				VectorSubtract( v[j], point, tvec );
				
				coordsArray[index[j]][0] = DotProduct( tvec, vright ) + 0.5f;
				coordsArray[index[j]][1] = DotProduct( tvec, vup ) + 0.5f;
			}

			tempIndexesArray[dlightNumIndexes++] = index[0];
			tempIndexesArray[dlightNumIndexes++] = index[1];
			tempIndexesArray[dlightNumIndexes++] = index[2];
		}

		if ( dlightNumIndexes ) {
			R_PushIndexes ( tempIndexesArray, NULL, NULL, dlightNumIndexes, MF_NONBATCHED );
			R_FlushArrays ();
			dlightNumIndexes = 0;
		}
	}

	numIndexes = oldNumIndexes;
	indexesArray = oldIndexesArray;
}

//===================================================================

/*
===============
R_LightDirForOrigin
===============
*/
void R_LightDirForOrigin ( vec3_t origin, vec3_t dir )
{
	vec3_t vf, vf2;
	float t[8], direction_uv[2], dot;
	int vi[3], i, j, index[4];

	if ( !r_worldmodel || (r_newrefdef.rdflags & RDF_NOWORLDMODEL) || !r_worldbmodel ||
		 !r_worldbmodel->lightgrid || !r_worldbmodel->numlightgridelems ) {
		VectorSet ( dir, 0.5, 0.2, -1 );
		VectorNormalizeFast ( dir );
		return;
	}

	for ( i = 0; i < 3; i++ ) {
		vf[i] = (origin[i] - r_worldmodel->gridMins[i]) / r_worldmodel->gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor(vf[i]);
		vf2[i] = 1.0f - vf[i];
	}

	index[0] = vi[2]*r_worldmodel->gridBounds[3] + vi[1]*r_worldmodel->gridBounds[0] + vi[0];
	index[1] = index[0] + r_worldmodel->gridBounds[0];
	index[2] = index[0] + r_worldmodel->gridBounds[3];
	index[3] = index[2] + r_worldmodel->gridBounds[0];

	for ( i = 0; i < 4; i++ ) {
		if ( index[i] < 0 || index[i] >= (r_worldbmodel->numlightgridelems-1) ) {
			VectorSet ( dir, 0.5, 0.2, -1 );
			VectorNormalizeFast ( dir );
			return;
		}
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	for ( j = 0; j < 2; j++ ) {
		direction_uv[j] = 0;

		for ( i = 0; i < 4; i++ ) {
			direction_uv[j] += t[i*2] * r_worldbmodel->lightgrid[ index[i] ].direction[j];
			direction_uv[j] += t[i*2+1] * r_worldbmodel->lightgrid[ index[i] + 1 ].direction[j];
		}

		direction_uv[j] = anglemod ( direction_uv[j] );
	}

	dot = direction_uv[0] * (1.0 / 255.0);
	t[0] = R_FastSin ( dot + 0.25f );
	t[1] = R_FastSin ( dot );

	dot = direction_uv[1] * (1.0 / 255.0);
	t[2] = R_FastSin ( dot + 0.25f );
	t[3] = R_FastSin ( dot );

	VectorSet ( dir, t[2] * t[1], t[3] * t[1], t[0] );
}

/*
===============
R_LightForEntity
===============
*/
void R_LightForEntity ( entity_t *e, byte *bArray )
{
	vec3_t vf, vf2;
	float *cArray;
	float t[8], direction_uv[2], dot;
	int vi[3], i, j, index[4];
	vec3_t dlorigin, ambient, diffuse, dir, direction;
	vec4_t tempColorsArray[MAX_ARRAY_VERTS];

	if ( (e->flags & RF_FULLBRIGHT) || r_fullbright->value ) {
		memset ( bArray, 255, sizeof(byte_vec4_t)*numColors );
		return;
	}

	// probably a weird shader, see mpteam4 for example
	if ( !e->model || (e->model->type == mod_brush) ) {
		memset ( bArray, 0, sizeof(byte_vec4_t)*numColors );
		return;
	}

	VectorSet ( ambient, 0, 0, 0 );
	VectorSet ( diffuse, 0, 0, 0 );
	VectorSet ( direction, 1, 1, 1 );

	if ( !r_worldmodel || (r_newrefdef.rdflags & RDF_NOWORLDMODEL) || !r_worldbmodel ||
		 !r_worldbmodel->lightgrid || !r_worldbmodel->numlightgridelems ) {
		goto dynamic;
	}

	for ( i = 0; i < 3; i++ ) {
		vf[i] = (e->origin[i] - r_worldmodel->gridMins[i]) / r_worldmodel->gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor(vf[i]);
		vf2[i] = 1.0f - vf[i];
	}

	index[0] = vi[2]*r_worldmodel->gridBounds[3] + vi[1]*r_worldmodel->gridBounds[0] + vi[0];
	index[1] = index[0] + r_worldmodel->gridBounds[0];
	index[2] = index[0] + r_worldmodel->gridBounds[3];
	index[3] = index[2] + r_worldmodel->gridBounds[0];

	for ( i = 0; i < 4; i++ ) {
		if ( index[i] < 0 || index[i] >= (r_worldbmodel->numlightgridelems-1) )
			goto dynamic;
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	for ( j = 0; j < 3; j++ ) {
		ambient[j] = 0;
		diffuse[j] = 0;

		for ( i = 0; i < 4; i++ ) {
			ambient[j] += t[i*2] * r_worldbmodel->lightgrid[ index[i] ].ambient[j];
			ambient[j] += t[i*2+1] * r_worldbmodel->lightgrid[ index[i] + 1 ].ambient[j];

			diffuse[j] += t[i*2] * r_worldbmodel->lightgrid[ index[i] ].diffuse[j];
			diffuse[j] += t[i*2+1] * r_worldbmodel->lightgrid[ index[i] + 1 ].diffuse[j];
		}
	}

	for ( j = 0; j < 2; j++ ) {
		direction_uv[j] = 0;

		for ( i = 0; i < 4; i++ ) {
			direction_uv[j] += t[i*2] * r_worldbmodel->lightgrid[ index[i] ].direction[j];
			direction_uv[j] += t[i*2+1] * r_worldbmodel->lightgrid[ index[i] + 1 ].direction[j];
		}

		direction_uv[j] = anglemod ( direction_uv[j] );
	}

	dot = bound(0.0f, r_ambientscale->value, 1.0f) * gl_state.pow2_mapovrbr;
	VectorScale ( ambient, dot, ambient );

	dot = bound(0.0f, r_directedscale->value, 1.0f) * gl_state.pow2_mapovrbr;
	VectorScale ( diffuse, dot, diffuse );

	if ( e->flags & RF_MINLIGHT )
	{
		for (i=0 ; i<3 ; i++)
			if (ambient[i] > 0.1)
				break;

		if (i == 3)
		{
			ambient[0] = 0.1f;
			ambient[1] = 0.1f;
			ambient[2] = 0.1f;
		}
	}

	dot = direction_uv[0] * (1.0 / 255.0);
	t[0] = R_FastSin ( dot + 0.25f );
	t[1] = R_FastSin ( dot );

	dot = direction_uv[1] * (1.0 / 255.0);
	t[2] = R_FastSin ( dot + 0.25f );
	t[3] = R_FastSin ( dot );

	VectorSet ( dir, t[2] * t[1], t[3] * t[1], t[0] );

	// rotate direction
	Matrix3_Multiply_Vec3 ( e->axis, dir, direction );

	cArray = tempColorsArray[0];
	for ( i = 0; i < numColors; i++, cArray += 4 )
	{
		dot = DotProduct ( normalsArray[i], direction );
		
		if ( dot <= 0 )
			VectorCopy ( ambient, cArray );
		else
			VectorMA ( ambient, dot, diffuse, cArray );
	}

dynamic:
	//
	// add dynamic lights
	//
	if ( r_dynamiclight->value && r_newrefdef.num_dlights ) 
	{
		int lnum;
		dlight_t *dl;
		float dist, add, intensity8;

		dl = r_newrefdef.dlights;
		for (lnum=0 ; lnum<r_newrefdef.num_dlights ; lnum++, dl++)
		{
			// translate
			VectorSubtract ( dl->origin, e->origin, dir );
			dist = VectorLength ( dir );

			if ( dist > dl->intensity + e->model->radius * e->scale ) {
				continue;
			}

			// rotate
			Matrix3_Multiply_Vec3 ( e->axis, dir, dlorigin );

			intensity8 = dl->intensity * 8;

			cArray = tempColorsArray[0];
			for ( i = 0; i < numColors; i++, cArray += 4 )
			{
				VectorSubtract ( dlorigin, vertexArray[i], dir );
				add = DotProduct ( normalsArray[i], dir );

				if ( add > 0 ) {
					dot = DotProduct ( dir, dir );
					add *= (intensity8 / dot) * Q_RSqrt ( dot );
					VectorMA ( cArray, add, dl->color, cArray );
				}
			}
		}
	}

	cArray = tempColorsArray[0];
	for ( i = 0; i < numColors; i++, bArray += 4, cArray += 4 ) {
		bArray[0] = FloatToByte ( bound (0.0f, cArray[0], 1.0f) );
		bArray[1] = FloatToByte ( bound (0.0f, cArray[1], 1.0f) );
		bArray[2] = FloatToByte ( bound (0.0f, cArray[2], 1.0f) );
	}
}

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/
void R_BuildLightMap (byte *data, byte *dest)
{
	int smax, tmax;
	int size, i;
	float scale;
	float rgb[3], scaled[3];

	smax = LIGHTMAP_WIDTH;
	tmax = LIGHTMAP_HEIGHT;
	size = smax * tmax;

	if ( !data || r_fullbright->value ) {
		memset ( dest, 255, size * 4 );
		return;
	}
	
	scale = gl_state.pow2_mapovrbr;
	for ( i = 0; i < size; i++, dest += 4 )
	{
		scaled[0] = data[i * 3 + 0] * scale;
		scaled[1] = data[i * 3 + 1] * scale;
		scaled[2] = data[i * 3 + 2] * scale;

		ColorNormalize ( scaled, rgb );

		dest[0] = FloatToByte ( rgb[0] );
		dest[1] = FloatToByte ( rgb[1] );
		dest[2] = FloatToByte ( rgb[2] );
		dest[3] = 255;
	}
}
