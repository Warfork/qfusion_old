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
#include "qcommon.h"

/*
===============
Mesh_FlatnessTest
===============
*/
static int Mesh_FlatnessTest ( float maxflat, vec4_t point0, vec4_t point1, vec4_t point2 )
{
	vec3_t v1, v2, v3;
	vec3_t t, n;
	float dist, d, l;
	int ft0, ft1;

	VectorSubtract ( point2, point0, n );
	l = VectorNormalize ( n );

	if ( !l ) {
		return 0;
	}

	VectorSubtract ( point1, point0, t );
	d = -DotProduct ( t, n );
	VectorMA ( t, d, n, t );
	dist = VectorLength ( t );

	if ( fabs(dist) <= maxflat ) {
		return 0;
	}

	VectorAvg ( point1, point0, v1 );
	VectorAvg ( point2, point1, v2 );
	VectorAvg ( v1, v2, v3 );

	ft0 = Mesh_FlatnessTest ( maxflat, point0, v1, v3 );
	ft1 = Mesh_FlatnessTest ( maxflat, v3, v2, point2 );

	return 1 + (int)floor( max ( ft0, ft1 ) + 0.5f );
}

/*
===============
Mesh_GetFlatness
===============
*/
void Mesh_GetFlatness ( float maxflat, vec4_t *points, int *mesh_cp, int *flat )
{
	int i, p, u, v;

	flat[0] = flat[1] = 0;
	for (v = 0; v < mesh_cp[1] - 1; v += 2)
	{
		for (u = 0; u < mesh_cp[0] - 1; u += 2)
		{
			p = v * mesh_cp[0] + u;

			i = Mesh_FlatnessTest ( maxflat, points[p], points[p+1], points[p+2] );
			flat[0] = max ( flat[0], i );
			i = Mesh_FlatnessTest ( maxflat, points[p+mesh_cp[0]], points[p+mesh_cp[0]+1], points[p+mesh_cp[0]+2] );
			flat[0] = max ( flat[0], i );
			i = Mesh_FlatnessTest ( maxflat, points[p+2*mesh_cp[0]], points[p+2*mesh_cp[0]+1], points[p+2*mesh_cp[0]+2] );
			flat[0] = max ( flat[0], i );

			i = Mesh_FlatnessTest ( maxflat, points[p], points[p+mesh_cp[0]], points[p+2*mesh_cp[0]] );
			flat[1] = max ( flat[1], i );
			i = Mesh_FlatnessTest ( maxflat, points[p+1], points[p+mesh_cp[0]+1], points[p+2*mesh_cp[0]+1] );
			flat[1] = max ( flat[1], i );
			i = Mesh_FlatnessTest ( maxflat, points[p+2], points[p+mesh_cp[0]+2], points[p+2*mesh_cp[0]+2] );
			flat[1] = max ( flat[1], i );
		}
	}
}

/*
===============
Mesh_EvalQuadricBezier
===============
*/
static void Mesh_EvalQuadricBezier ( float t, vec4_t point0, vec4_t point1, vec3_t point2, vec4_t out )
{
	float qt = t * t;
	float dt = 2.0f * t, tt;
	vec4_t tvec4;

	tt = 1.0f - dt + qt;
	Vector4Scale ( point0, tt, out );

	tt = dt - 2.0f * qt;
	Vector4Scale ( point1, tt, tvec4 );
	Vector4Add ( out, tvec4, out );

	Vector4Scale ( point2, qt, tvec4 );
	Vector4Add ( out, tvec4, out );
}

/*
===============
Mesh_EvalQuadricBezierPatch
===============
*/
void Mesh_EvalQuadricBezierPatch ( vec4_t *p, int *numcp, int *tess, vec4_t *dest )
{
	int num_patches[2], num_tess[2];
	int index[3], dstpitch, i, u, v, x, y;
	float s, t, step[2];
	vec4_t *tvec, pv[3][3], v1, v2, v3;

	num_patches[0] = numcp[0] / 2;
	num_patches[1] = numcp[1] / 2;
	dstpitch = num_patches[0] * tess[0] + 1;

	step[0] = 1.0f / (float)tess[0];
	step[1] = 1.0f / (float)tess[1];

	for ( v = 0; v < num_patches[1]; v++ )
	{
		// last patch has one more row 
		if ( v < num_patches[1] - 1 ) {
			num_tess[1] = tess[1];
		} else {
			num_tess[1] = tess[1] + 1;
		}

		for ( u = 0; u < num_patches[0]; u++ )
		{
			// last patch has one more column
			if ( u < num_patches[0] - 1 ) {
				num_tess[0] = tess[0];
			} else {
				num_tess[0] = tess[0] + 1;
			}

			index[0] = (v * numcp[0] + u) * 2;
			index[1] = index[0] + numcp[0];
			index[2] = index[1] + numcp[0];

			// current 3x3 patch control points
			for ( i = 0; i < 3; i++ ) 
			{
				Vector4Copy ( p[index[0]+i], pv[i][0] );
				Vector4Copy ( p[index[1]+i], pv[i][1] );
				Vector4Copy ( p[index[2]+i], pv[i][2] );
			}
			
			t = 0.0f;
			tvec = dest + v * tess[1] * dstpitch + u * tess[0];

			for ( y = 0; y < num_tess[1]; y++, t += step[1] )
			{
				Mesh_EvalQuadricBezier ( t, pv[0][0], pv[0][1], pv[0][2], v1 );
				Mesh_EvalQuadricBezier ( t, pv[1][0], pv[1][1], pv[1][2], v2 );
				Mesh_EvalQuadricBezier ( t, pv[2][0], pv[2][1], pv[2][2], v3 );

				s = 0.0f;
				for ( x = 0; x < num_tess[0]; x++, s += step[0] )
				{
					Mesh_EvalQuadricBezier ( s, v1, v2, v3, tvec[x] );
				}

				tvec += dstpitch;
			}
		}
	}
}

