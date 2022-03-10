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

static vec3_t	light_vertex[MAX_VERTS];

/*
=================
R_MarkLights

Optimize it someday...
=================
*/
void R_MarkLights ( msurface_t *slist )
{
	msurface_t *s;
	dlight_t *light;
	vec3_t point, v, lastnormal;
	mvertex_t *vert;
	float dist, scale, cs, tc[2], color[4];
	int i, j, *lindex;

	color[3] = 1.0f;

	VectorClear ( lastnormal );

	for ( s = slist; s; s = s->texturechain ) {
		if ( s->shaderref->flags & SURF_NODLIGHT ) {
			continue;
		}
		if ( s->shaderref->flags & SURF_SKY ) {
			continue;
		}
		if ( s->shaderref->contents & MASK_WATER ) {
			continue;
		}
		if ( s->facetype == FACETYPE_TRISURF ) {
			continue;
		}

		light = r_newrefdef.dlights;
		for ( i = 0; i < r_newrefdef.num_dlights; i++, light++ ) {
			if ( s->facetype == FACETYPE_PLANAR ) {
				VectorClear ( lastnormal );
				dist = PlaneDiff ( light->origin, s->plane );
				
				if ( dist < 0 )
					dist = -dist;
				
				if ( dist > light->intensity )
					continue;

				ProjectPointOnPlane ( point, light->origin, s->plane->normal );

				scale = 0.08f / ( 2*light->intensity - dist );
				cs = 1.0f - ( dist / light->intensity );
				VectorScale ( light->color, cs, color );
			}

			lindex = s->mesh.firstindex;
			for (j = 0; j < s->mesh.numindexes; j++, lindex++)
				R_PushElem (*lindex);

			R_DeformVertices ( &s->mesh, light_vertex, MAX_VERTS );

			vert = s->mesh.firstvert;
			for (j = 0; j < s->mesh.numverts; j++, vert++)
			{
				if ( s->facetype != FACETYPE_PLANAR ) {
					if ( !VectorCompare (vert->normal, lastnormal) ) {
						VectorCopy ( vert->normal, lastnormal );

						// Project the light image onto the face
						VectorSubtract ( light->origin, light_vertex[j], v );
						dist = DotProduct ( v, vert->normal );
						
						if ( dist < 0 )
							dist = -dist;
						
						if ( dist > light->intensity ) {
							cs = 0;
						} else {
							ProjectPointOnPlane ( point, light->origin, vert->normal );

							scale = 1.0f / ( 2*light->intensity - dist );
							cs = 1.0f - ( dist / light->intensity );
						}

						VectorScale ( light->color, cs, color );
					} else {
						VectorScale ( light->color, cs, color );
					}					
				}

				// Project the light image onto the face
				VectorAdd ( light_vertex[j], currententity->origin, v );
				VectorSubtract( v, point, v );

				// Get our texture coordinates
				if ( s->facetype == FACETYPE_MESH ) {	
					tc[0] = DotProduct( v, s->mesh.lm_mins[j] ) * scale + 0.5f;
					tc[1] = DotProduct( v, s->mesh.lm_maxs[j] ) * scale + 0.5f;
				} else {
					tc[0] = DotProduct( v, s->mins ) * scale + 0.5f;
					tc[1] = DotProduct( v, s->maxs ) * scale + 0.5f;
				}
				
				R_PushCoord ( tc );
				R_PushColor ( color );
			}

			R_LockArrays ();
			R_FlushArrays ();
			R_UnlockArrays ();
			R_ClearArrays ();
		}
	}	
}

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

vec3_t			pointcolor;
cplane_t		*lightplane;		// used as shadow plane
vec3_t			lightspot;

vec3_t			gridSize;
vec3_t			gridMins;
int				gridBounds[3];

int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	return 255;
}

/*
===============
R_LightOfPoint
===============
*/
mlightgrid_t *R_LightOfPoint ( vec3_t p )
{
	int v[3], index;

	if ( !r_worldmodel || !r_worldmodel->lightgrid )
		return NULL;

	v[0] = p[0] - gridMins[0];
	v[1] = p[1] - gridMins[1];
	v[2] = p[2] - gridMins[2];
	v[0] /= gridSize[0];
	v[1] /= gridSize[1];
	v[2] /= gridSize[2];

	index = v[2]*gridBounds[1]*gridBounds[0]+v[1]*gridBounds[0]+v[0];
	return &r_worldmodel->lightgrid[index % r_worldmodel->numlightgridelems];
}

void R_SetVertexColour ( vec3_t v, vec3_t norm, float *colour )
{
	int i;
	float dot;
	vec3_t org, normal;
	mlightgrid_t *light;

	if ( !currententity ) {
		colour[0] = colour[1] = colour[2] = 1.0f;
		return;
	}

	// rotate normal
	normal[0] = DotProduct ( norm, currententity->angleVectors[0] );
	normal[1] = -DotProduct ( norm, currententity->angleVectors[1] );
	normal[2] = DotProduct ( norm, currententity->angleVectors[2] );

	if ( !( currententity->flags & RF_WEAPONMODEL ) )
		VectorAdd ( v, currententity->origin, org );
	else
		VectorAdd ( v, r_origin, org );
		
	light = R_LightOfPoint ( org );

	if ( light ) {
		dot = DotProduct ( normal, light->lightPosition );

		for ( i = 0; i < 3; i++ ){
			colour[i] = light->lightAmbient[i] + dot * light->lightDiffuse[i];
			colour[i] = max ( 0, min( colour[i], 1 ) );
		}
	} else {
		VectorSet ( colour, 1, 1, 1 );
	}
}

void R_SetGridsize (float x, float y, float z)
{
	int i;
	vec3_t	maxs;

	gridSize[0] = x;
	gridSize[1] = y;
	gridSize[2] = z;

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
	
	bl = s_blocklights;
	for (i=0 ; i<size ; i++, bl+=3)
	{
		bl[0] = data[i*3+0]*gl_modulate->value;
		bl[1] = data[i*3+1]*gl_modulate->value;
		bl[2] = data[i*3+2]*gl_modulate->value;
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
