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

#include "gl_local.h"

#define R_Interpolate_Trilinear(t,v,dst) ((dst)=(t)[0]*(v)[0]+(t)[1]*(v)[1]+ \
	(t)[2]*(v)[2]+(t)[3]*(v)[3]+(t)[4]*(v)[4]+(t)[5]*(v)[5]+(t)[6]*(v)[6]+(t)[7]*(v)[7])

/*
=================
R_AddDynamicLights
=================
*/
void R_AddDynamicLights ( msurface_t *s, vec4_t *vertexArray )
{
	dlight_t *light;
	vec3_t point, v, lmins, lmaxs;
	mvertex_t *vert;
	float dist, scale, tc[2], vcolor[4], color[4];
	int i, j, div;

	if ( !r_dynamiclight->value || 
		!r_dlighttexture || 
		!r_newrefdef.num_dlights ||
		r_vertexlight->value ) {
		return;
	}
	if ( (s->flags & (SURF_SKY|SURF_NOLIGHTMAP|SURF_NODLIGHT)) || 
		!s->mesh.shader->numpasses ) {
		return;
	}

	div = floor ( r_mapoverbrightbits->value - r_overbrightbits->value );
	if ( div > 0 ) {
		div = pow( 2, div );
	} else {
		div = 1.0;
	}

	color[3] = 1.0f;
	
	GL_Bind ( r_dlighttexture->texnum );
	qglBlendFunc ( GL_DST_COLOR, GL_ONE );
	qglDepthFunc ( GL_LEQUAL );

	light = r_newrefdef.dlights;
	for ( i = 0; i < r_newrefdef.num_dlights; i++, light++ ) {
		VectorCopy ( light->origin, lmins );
		VectorCopy ( light->origin, lmaxs );
		lmins[0] -= light->intensity;
		lmins[1] -= light->intensity;
		lmins[2] -= light->intensity;
		lmaxs[0] += light->intensity;
		lmaxs[1] += light->intensity;
		lmaxs[2] += light->intensity;

		if ( s->facetype == FACETYPE_PLANAR ) {
			if ( lmaxs[0] <= s->origin[0] || lmaxs[1] <= s->origin[1] || lmaxs[2] <= s->origin[2] ) {
				continue;
			}

			dist = PlaneDiff ( light->origin, s->plane );
			
			if ( dist < 0 )
				dist = -dist;
			
			if ( dist > light->intensity )
				continue;
			
			if ( s->plane->type < 3 ) {
				VectorCopy ( light->origin, point );
				point[s->plane->type] -= dist;
			} else {
				VectorMA ( light->origin, -dist, s->plane->normal, point );
			}
			scale = 0.08f / ( 2*light->intensity - dist );
		} else {
			if ( lmins[0] >= s->maxs[0] || lmins[1] >= s->maxs[1]	|| lmins[2] >= s->maxs[2]
				|| lmaxs[0] <= s->mins[0] || lmaxs[1] <= s->mins[1] || lmaxs[2] <= s->mins[2] ) {
				continue;
			}
		}

		VectorScale ( light->color, div, color );
		color[0] = min ( color[0], 1 );
		color[1] = min ( color[1], 1 );
		color[2] = min ( color[2], 1 );

		vert = s->mesh.firstvert;
		for (j = 0; j < s->mesh.numverts; j++, vert++)
		{
			if ( s->facetype != FACETYPE_PLANAR ) {	
				// Project the light image onto the face
				VectorAdd ( vertexArray[j], currententity->origin, v );
				VectorSubtract ( light->origin, v, v );
				dist = DotProduct ( v, vert->normal );
				
				if ( dist < 0 )
					dist = -dist;
				
				VectorCopy ( color, vcolor );
				VectorMA ( light->origin, -dist, vert->normal, point );
				scale = 1.0 / ( 2*light->intensity - dist );
			} else {
				VectorCopy ( color, vcolor );
			}
				
			// Get our texture coordinates
			// Project the light image onto the face
			VectorAdd ( vertexArray[j], currententity->origin, v );
			VectorSubtract( v, point, v );
			
			if ( s->facetype == FACETYPE_MESH ) {	
				tc[0] = DotProduct( v, s->mesh.lm_mins[j] ) * scale + 0.5f;
				tc[1] = DotProduct( v, s->mesh.lm_maxs[j] ) * scale + 0.5f;
			} else {
				tc[0] = DotProduct( v, s->mins ) * scale + 0.5f;
				tc[1] = DotProduct( v, s->maxs ) * scale + 0.5f;
			}

			R_PushCoord ( tc );
			R_PushColor ( vcolor );
		}
		
		R_FlushArrays ();
	}
}

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

vec3_t			gridSize;
vec3_t			gridMins;
int				gridBounds[3];

float R_FastSin ( float t );

/*
===============
R_LightForPoint
===============
*/
void R_LightForPoint ( vec3_t p, vec3_t ambient, vec3_t diffuse, vec3_t direction )
{
	vec3_t vf, vf2;
	float v[8], t[8], direction_uv[2];
	int vi[3], i, j, index[4];

	VectorSet ( ambient, 1, 1, 1 );
	VectorSet ( diffuse, 0, 0, 0 );
	VectorSet ( direction, 0, 0, 1 );

	if ( !r_worldmodel || !r_worldmodel->lightgrid ) {
		return;
	}

	for ( i = 0; i < 3; i++ ) {
		vf[i] = (p[i] - gridMins[i]) / gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor(vf[i]);
		vf2[i] = 1.0f - vf[i];
	}

	index[0] = vi[2]*gridBounds[1]*gridBounds[0]+vi[1]*gridBounds[0]+vi[0];
	index[1] = index[0] + gridBounds[0];
	index[2] = index[1] + gridBounds[1]*gridBounds[0];
	index[3] = index[2] - gridBounds[0];

	for ( i = 0; i < 4; i++ ) {
		if ( index[i] < 0 || index[i] >= r_worldmodel->numlightgridelems )
			return;
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
		for ( i = 0; i < 4; i++ ) {
			v[i*2] = r_worldmodel->lightgrid[ index[i] ].ambient[j];
			v[i*2+1] = r_worldmodel->lightgrid[ index[i] + 1 ].ambient[j];
		}

		R_Interpolate_Trilinear ( t, v, ambient[j] );

		for ( i = 0; i < 4; i++ ) {
			v[i*2] = r_worldmodel->lightgrid[ index[i] ].diffuse[j];
			v[i*2+1] = r_worldmodel->lightgrid[ index[i] + 1 ].diffuse[j];
		}

		R_Interpolate_Trilinear ( t, v, diffuse[j] );

		if ( j < 2 ) {
			for ( i = 0; i < 4; i++ ) {
				v[i*2] = r_worldmodel->lightgrid[ index[i] ].direction[j];
				v[i*2+1] = r_worldmodel->lightgrid[ index[i] + 1 ].direction[j];
			}

			R_Interpolate_Trilinear ( t, v, direction_uv[j] );
		}
	}

	t[0] = R_FastSin ( direction_uv[0] + 0.25 );
	t[1] = R_FastSin ( direction_uv[0] );
	t[2] = R_FastSin ( direction_uv[1] + 0.25 );
	t[3] = R_FastSin ( direction_uv[1] );
	VectorSet ( direction, t[2] * t[1], t[3] * t[1], t[0] );
}

void R_SetGridsize (int x, int y, int z)
{
	int i;
	vec3_t maxs;

	gridSize[0] = x ? x : 64;
	gridSize[1] = y ? y : 64;
	gridSize[2] = z ? z : 128;

	for ( i = 0 ; i < 3 ; i++ ) 
	{
		gridMins[i] = gridSize[i] * ceil( r_worldmodel->mins[i] / gridSize[i] );
		maxs[i] = gridSize[i] * floor( r_worldmodel->maxs[i] / gridSize[i] );
		gridBounds[i] = (maxs[i] - gridMins[i])/gridSize[i] + 1;
	}
}

//===================================================================

static float s_blocklights[128*128*3];

void R_BuildLightMap (byte *data, byte *dest)
{
	int w = 128, h = 128;
	int size = w * h, i;
	float *bl;
	int	r, g, b, a, max;
	int monolightmap;

	if (!data)
	{
		for (i=0 ; i<size*3 ; i++)
			s_blocklights[i] = 255;

		goto store;
	}
	
	r = (int)(r_mapoverbrightbits->value - r_overbrightbits->value);
	bl = s_blocklights;

	if ( r > 0 ) {
		r = 1 << r;
		for (i=0 ; i<size ; i++, bl+=3)
		{
			bl[0] = data[i*3+0]*r;
			bl[1] = data[i*3+1]*r;
			bl[2] = data[i*3+2]*r;
		}
	} else {
		for (i=0 ; i<size ; i++, bl+=3)
		{
			bl[0] = data[i*3+0];
			bl[1] = data[i*3+1];
			bl[2] = data[i*3+2];
		}
	}

store:
	bl = s_blocklights;
	monolightmap = gl_monolightmap->string[0];

	if ( monolightmap == '0' )
	{
		for (i = 0; i<size; i++, bl+=3, dest += 4)
		{
			r = Q_ftol( bl[0] );
			g = Q_ftol( bl[1] );
			b = Q_ftol( bl[2] );

			// catch negative lights
			if (r < 0)
				r = 0;
			if (g < 0)
				g = 0;
			if (b < 0)
				b = 0;

			/*
			** determine the brightest of the three color components
			*/
			if (r > g)
				max = r;
			else
				max = g;
			if (b > max)
				max = b;

			/*
			** alpha is ONLY used for the mono lightmap case.  For this reason
			** we set it to the brightest of the color components so that 
			** things don't get too dim.
			*/
			a = max;

			/*
			** rescale all the color components if the intensity of the greatest
			** channel exceeds 1.0
			*/
			if (max > 255)
			{
				float t = 255.0F / max;
				r = r*t;
				g = g*t;
				b = b*t;
				a = a*t;
			}

			dest[0] = r;
			dest[1] = g;
			dest[2] = b;
			dest[3] = a;
		}
	}
	else
	{
		for (i = 0; i < size; i++, bl+=3, dest += 4)
		{
			r = Q_ftol( bl[0] );
			g = Q_ftol( bl[1] );
			b = Q_ftol( bl[2] );

			// catch negative lights
			if (r < 0)
				r = 0;
			if (g < 0)
				g = 0;
			if (b < 0)
				b = 0;

			/*
			** determine the brightest of the three color components
			*/
			if (r > g)
				max = r;
			else
				max = g;
			if (b > max)
				max = b;

			/*
			** alpha is ONLY used for the mono lightmap case.  For this reason
			** we set it to the brightest of the color components so that 
			** things don't get too dim.
			*/
			a = max;

			/*
			** rescale all the color components if the intensity of the greatest
			** channel exceeds 1.0
			*/
			if (max > 255)
			{
				float t = 255.0F / max;
				r = r*t;
				g = g*t;
				b = b*t;
				a = a*t;
			}

			/*
			** So if we are doing alpha lightmaps we need to set the R, G, and B
			** components to 0 and we need to set alpha to 1-alpha.
			*/
			switch ( monolightmap )
			{
				case 'L':
				case 'I':
					r = a;
					g = b = 0;
					break;
				case 'C':
					// try faking colored lighting
					a = 255 - ((r+g+b)/3);
					r *= a/255.0;
					g *= a/255.0;
					b *= a/255.0;
					break;
				case 'A':
				default:
					r = g = b = 0;
					a = 255 - a;
					break;
			}

			dest[0] = r;
			dest[1] = g;
			dest[2] = b;
			dest[3] = a;
		}
	}
}
