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
#include "q_shared.h"

vec3_t		vec3_origin = { 0, 0, 0 };

mat3_t		mat3_identity = 
{ 
	1, 0, 0, 
	0, 1, 0, 
	0, 0, 1 
};

mat4_t		mat4_identity = 
{ 
	1, 0, 0, 0, 
	0, 1, 0, 0, 
	0, 0, 1, 0, 
	0, 0, 0, 1 
};

mat3_t		axis_identity = 
{ 
	1, 0, 0, 
	0, 1, 0, 
	0, 0, 1 
};

//============================================================================

vec3_t	bytedirs[NUMVERTEXNORMALS] =
{
#include "anorms.h"
};

int DirToByte (vec3_t dir)
{
	int		i, best;
	float	d, bestd;
	
	if (!dir)
		return 0;

	bestd = 0;
	best = 0;
	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		d = DotProduct (dir, bytedirs[i]);
		if (d > bestd)
		{
			bestd = d;
			best = i;
		}
	}

	return best;
}

void ByteToDir (int b, vec3_t dir)
{
	if (b < 0 || b >= NUMVERTEXNORMALS)
		VectorSet (dir, 0, 0, 1);
	else
		VectorCopy (bytedirs[b], dir);
}

//============================================================================

vec4_t		colorBlack	= {0, 0, 0, 1};
vec4_t		colorRed	= {1, 0, 0, 1};
vec4_t		colorGreen	= {0, 1, 0, 1};
vec4_t		colorBlue	= {0, 0, 1, 1};
vec4_t		colorYellow	= {1, 1, 0, 1};
vec4_t		colorMagenta= {1, 0, 1, 1};
vec4_t		colorCyan	= {0, 1, 1, 1};
vec4_t		colorWhite	= {1, 1, 1, 1};
vec4_t		colorLtGrey	= {0.75, 0.75, 0.75, 1};
vec4_t		colorMdGrey	= {0.5, 0.5, 0.5, 1};
vec4_t		colorDkGrey	= {0.25, 0.25, 0.25, 1};

vec4_t	color_table[8] =
{
	{0.0, 0.0, 0.0, 1.0},
	{1.0, 0.0, 0.0, 1.0},
	{0.0, 1.0, 0.0, 1.0},
	{1.0, 1.0, 0.0, 1.0},
	{0.0, 0.0, 1.0, 1.0},
	{0.0, 1.0, 1.0, 1.0},
	{1.0, 0.0, 1.0, 1.0},
	{1.0, 1.0, 1.0, 1.0},
};

/*
===============
ColorNormalize

===============
*/
float ColorNormalize ( const vec3_t in, vec3_t out )
{
	float f = max ( max (in[0], in[1]), in[2] );

	if ( f > 1.0f ) {
		f = 1.0f / f;
		out[0] = in[0] * f;
		out[1] = in[1] * f;
		out[2] = in[2] * f;
	} else {
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	}

	return f;
}

//============================================================================

void NormToLatLong ( const vec3_t normal, qbyte latlong[2] )
{
	// can't do atan2 (normal[1], normal[0])
	if ( normal[0] == 0 && normal[1] == 0 ) {
		if ( normal[2] > 0 ) {
			latlong[0] = 0;		// acos ( 1 )
			latlong[1] = 0;
		} else {
			latlong[0] = 128;	// acos ( -1 )
			latlong[1] = 0;
		}
	} else {
		int angle;

		angle = (int)( acos (normal[2]) * 255.0 / M_TWOPI ) & 255;
		latlong[0] = angle;
		angle = (int)( atan2 (normal[1], normal[0]) * 255.0 / M_TWOPI ) & 255;
		latlong[1] = angle;
	}
}

void MakeNormalVectors (const vec3_t forward, vec3_t right, vec3_t up)
{
	float		d;

	// this rotate and negate guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
	float		t0, t1;
	float		c, s;
	vec3_t		vr, vu, vf;

	s = DEG2RAD (degrees);
	c = cos (s);
	s = sin (s);

	VectorCopy (dir, vf);
	MakeNormalVectors (vf, vr, vu);

	t0 = vr[0] *  c + vu[0] * -s;
	t1 = vr[0] *  s + vu[0] *  c;
	dst[0] = (t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0]) * point[0]
		+ (t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1]) * point[1]
		+ (t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2]) * point[2];

	t0 = vr[1] *  c + vu[1] * -s;
	t1 = vr[1] *  s + vu[1] *  c;
	dst[1] = (t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0]) * point[0]
		+ (t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1]) * point[1]
		+ (t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2]) * point[2];

	t0 = vr[2] *  c + vu[2] * -s;
	t1 = vr[2] *  s + vu[2] *  c;
	dst[2] = (t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0]) * point[0]
		+ (t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1]) * point[1]
		+ (t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2]) * point[2];
}

void AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float				angle;
	static float		sr, sp, sy, cr, cp, cy, t;
	// static to help MS compiler fp bugs

	angle = DEG2RAD ( angles[YAW] );
	sy = sin(angle);
	cy = cos(angle);
	angle = DEG2RAD ( angles[PITCH] );
	sp = sin(angle);
	cp = cos(angle);
	angle = DEG2RAD ( angles[ROLL] );
	sr = sin(angle);
	cr = cos(angle);

	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (right)
	{
		t = sr*sp;
		right[0] = (-1*t*cy+-1*cr*-sy);
		right[1] = (-1*t*sy+-1*cr*cy);
		right[2] = -1*sr*cp;
	}
	if (up)
	{
		t = cr*sp;
		up[0] = (t*cy+-sr*-sy);
		up[1] = (t*sy+-sr*cy);
		up[2] = cr*cp;
	}
}

void VecToAngles (const vec3_t vec, vec3_t angles)
{
	float	forward;
	float	yaw, pitch;
	
	if (vec[1] == 0 && vec[0] == 0)
	{
		yaw = 0;
		if (vec[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		if (vec[0])
			yaw = RAD2DEG ( atan2(vec[1], vec[0]) );
		else if (vec[1] > 0)
			yaw = 90;
		else
			yaw = -90;
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (vec[0]*vec[0] + vec[1]*vec[1]);
		pitch = RAD2DEG ( atan2(vec[2], forward) );
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

void AnglesToAxis ( const vec3_t angles, mat3_t axis )
{
	AngleVectors ( angles, axis[0], axis[1], axis[2] );
	VectorInverse ( axis[1] );
}

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	int	pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalize( dst );
}

/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms (const float in1[3*4], const float in2[3*4], float out[3*4])
{
	out[0*4+0] = in1[0*4+0] * in2[0*4+0] + in1[0*4+1] * in2[1*4+0] + in1[0*4+2] * in2[2*4+0];
	out[0*4+1] = in1[0*4+0] * in2[0*4+1] + in1[0*4+1] * in2[1*4+1] + in1[0*4+2] * in2[2*4+1];
	out[0*4+2] = in1[0*4+0] * in2[0*4+2] + in1[0*4+1] * in2[1*4+2] + in1[0*4+2] * in2[2*4+2];
	out[0*4+3] = in1[0*4+0] * in2[0*4+3] + in1[0*4+1] * in2[1*4+3] + in1[0*4+2] * in2[2*4+3] + in1[0*4+3];
	out[1*4+0] = in1[1*4+0] * in2[0*4+0] + in1[1*4+1] * in2[1*4+0] + in1[1*4+2] * in2[2*4+0];
	out[1*4+1] = in1[1*4+0] * in2[0*4+1] + in1[1*4+1] * in2[1*4+1] + in1[1*4+2] * in2[2*4+1];
	out[1*4+2] = in1[1*4+0] * in2[0*4+2] + in1[1*4+1] * in2[1*4+2] + in1[1*4+2] * in2[2*4+2];
	out[1*4+3] = in1[1*4+0] * in2[0*4+3] + in1[1*4+1] * in2[1*4+3] + in1[1*4+2] * in2[2*4+3] + in1[1*4+3];
	out[2*4+0] = in1[2*4+0] * in2[0*4+0] + in1[2*4+1] * in2[1*4+0] + in1[2*4+2] * in2[2*4+0];
	out[2*4+1] = in1[2*4+0] * in2[0*4+1] + in1[2*4+1] * in2[1*4+1] + in1[2*4+2] * in2[2*4+1];
	out[2*4+2] = in1[2*4+0] * in2[0*4+2] + in1[2*4+1] * in2[1*4+2] + in1[2*4+2] * in2[2*4+2];
	out[2*4+3] = in1[2*4+0] * in2[0*4+3] + in1[2*4+1] * in2[1*4+3] + in1[2*4+2] * in2[2*4+3] + in1[2*4+3];
}

//============================================================================

float Q_RSqrt (float number)
{
	int i;
	float x2, y;

	if (number == 0.0)
		return 0.0;

	x2 = number * 0.5f;
	y = number;
	i = * (int *) &y;		// evil floating point bit level hacking
	i = 0x5f3759df - (i >> 1);		// what the fuck?
	y = * (float *) &i;
	y = y * (1.5f - (x2 * y * y));	// this can be done a second time

	return y;
}

int Q_rand ( int *seed )
{
	*seed = *seed * 1103515245 + 12345;
	return ((unsigned int)(*seed / 65536) % 32768);
}

/*
===============
LerpAngle

===============
*/
float LerpAngle (float a2, float a1, const float frac)
{
	if (a1 - a2 > 180)
		a1 -= 360;
	if (a1 - a2 < -180)
		a1 += 360;
	return a2 + frac * (a1 - a2);
}

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float	x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);

	x = width/tan(fov_x/360*M_PI);

	return atan (height/x)*360/M_PI;
}

float	anglemod(float a)
{
	a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide (const vec3_t emins, const vec3_t emaxs, const cplane_t *p)
{
	float	dist1, dist2;
	int		sides;

// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}
	
// general case
	switch (p->signbits)
	{
	case 0:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 1:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 2:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 3:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 4:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 5:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 6:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	case 7:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	default:
		dist1 = dist2 = 0;		// shut up compiler
		assert( 0 );
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

	assert( sides != 0 );

	return sides;
}

/*
=================
SignbitsForPlane
=================
*/
int SignbitsForPlane ( const cplane_t *out )
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

/*
=================
PlaneTypeForNormal
=================
*/
int	PlaneTypeForNormal ( const vec3_t normal )
{
// NOTE: should these have an epsilon around 1.0?		
	if ( normal[0] >= 1.0)
		return PLANE_X;
	if ( normal[1] >= 1.0 )
		return PLANE_Y;
	if ( normal[2] >= 1.0 )
		return PLANE_Z;
		
	return PLANE_NONAXIAL;
}

/*
=================
CategorizePlane

A slightly simplier version of SignbitsForPlane and PlaneTypeForNormal
=================
*/
void CategorizePlane ( cplane_t *plane )
{
	int i;

	plane->signbits = 0;
	plane->type = PLANE_NONAXIAL;
	for (i = 0; i < 3; i++)
	{
		if (plane->normal[i] < 0)
			plane->signbits |= 1<<i;
		if (plane->normal[i] == 1.0f)
			plane->type = i;
	}
}

/*
=================
PlaneFromPoints
=================
*/
void PlaneFromPoints (  vec3_t verts[3], cplane_t *plane )
{
	vec3_t	v1, v2;

	VectorSubtract( verts[1], verts[0], v1 );
	VectorSubtract( verts[2], verts[0], v2 );
	CrossProduct( v2, v1, plane->normal );
	VectorNormalize( plane->normal );
	plane->dist = DotProduct( verts[0], plane->normal );
}

#define	PLANE_NORMAL_EPSILON	0.00001
#define	PLANE_DIST_EPSILON		0.01

/*
=================
ComparePlanes
=================
*/
qboolean ComparePlanes ( const vec3_t p1normal, vec_t p1dist, const vec3_t p2normal, vec_t p2dist )
{
	if (fabs (p1normal[0] - p2normal[0]) < PLANE_NORMAL_EPSILON
		&& fabs (p1normal[1] - p2normal[1]) < PLANE_NORMAL_EPSILON
		&& fabs (p1normal[2] - p2normal[2]) < PLANE_NORMAL_EPSILON
		&& fabs (p1dist - p2dist) < PLANE_DIST_EPSILON )
		return qtrue;

	return qfalse;
}

/*
==============
SnapVector
==============
*/
void SnapVector ( vec3_t normal )
{
	int		i;

	for ( i = 0; i < 3; i++ ) {
		if ( fabs (normal[i] - 1) < PLANE_NORMAL_EPSILON ) {
			VectorClear ( normal );
			normal[i] = 1;
			break;
		}
		if ( fabs (normal[i] - -1) < PLANE_NORMAL_EPSILON ) {
			VectorClear ( normal );
			normal[i] = -1;
			break;
		}
	}
}

/*
==============
SnapPlane
==============
*/
void SnapPlane ( vec3_t normal, vec_t *dist )
{
	SnapVector ( normal );

	if ( fabs (*dist - Q_rint (*dist)) < PLANE_DIST_EPSILON ) {
		*dist = Q_rint ( *dist );
	}
}

void ClearBounds (vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

qboolean BoundsIntersect (const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2)
{
	return (mins1[0] <= maxs2[0] && mins1[1] <= maxs2[1] && mins1[2] <= maxs2[2] &&
		 maxs1[0] >= mins2[0] && maxs1[1] >= mins2[1] && maxs1[2] >= mins2[2]);
}

qboolean BoundsAndSphereIntersect (const vec3_t mins, const vec3_t maxs, const vec3_t centre, float radius)
{
	return (mins[0] <= centre[0]+radius && mins[1] <= centre[1]+radius && mins[2] <= centre[2]+radius &&
		maxs[0] >= centre[0]-radius && maxs[1] >= centre[1]-radius && maxs[2] >= centre[2]-radius);
}

void AddPointToBounds (const vec3_t v, vec3_t mins, vec3_t maxs)
{
	int		i;
	vec_t	val;

	for (i=0 ; i<3 ; i++)
	{
		val = v[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (const vec3_t mins, const vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength (corner);
}

vec_t VectorNormalize (vec3_t v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

	if (length)
	{
		length = sqrt (length);		// FIXME
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;
}

vec_t VectorNormalize2 (vec3_t v, vec3_t out)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

	if (length)
	{
		length = sqrt (length);		// FIXME
		ilength = 1/length;
		out[0] = v[0]*ilength;
		out[1] = v[1]*ilength;
		out[2] = v[2]*ilength;
	}
	else
	{
		VectorClear (out);
	}
		
	return length;
}

void VectorNormalizeFast (vec3_t v)
{
	float ilength = Q_RSqrt (DotProduct(v,v));

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

void _VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}


vec_t _DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]-vecb[0];
	out[1] = veca[1]-vecb[1];
	out[2] = veca[2]-vecb[2];
}

void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]+vecb[0];
	out[1] = veca[1]+vecb[1];
	out[2] = veca[2]+vecb[2];
}

void _VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

int Q_log2(int val)
{
	int answer=0;
	while (val>>=1)
		answer++;
	return answer;
}

void Matrix4_Identity (mat4_t mat)
{
	mat[1] = mat[2] = mat[3] = mat[4] = mat[6] = mat[7] = mat[8] = mat[9] =	mat[11] = mat[12] = mat[13] = mat[14] = 0;
	mat[0] = mat[5] = mat[10] = mat[15] = 1.0f;
}

void Matrix4_Copy (mat4_t a, mat4_t b)
{
	b[0] = a[0]; b[1] = a[1]; b[2] = a[2]; b[3] = a[3];
	b[4] = a[4]; b[5] = a[5]; b[6] = a[6]; b[7] = a[7];
	b[8] = a[8]; b[9] = a[9]; b[10] = a[10]; b[11] = a[11];
	b[12] = a[12]; b[13] = a[13]; b[14] = a[14]; b[15] = a[15];
}

qboolean Matrix4_Compare (mat4_t a, mat4_t b)
{
	int i;

	for (i = 0; i < 16; i++)
	{
		if ( a[i] != b[i] )
			return qfalse;
	}

	return qtrue;
}

void Matrix4_Multiply (mat4_t a, mat4_t b, mat4_t product)
{
	product[0]  = a[0] * b[0] + a[4] * b[1] + a[8] * b[2] + a[12] * b[3];
	product[1]  = a[1] * b[0] + a[5] * b[1] + a[9] * b[2] + a[13] * b[3];
	product[2]  = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
	product[3]  = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];
	product[4]  = a[0] * b[4] + a[4] * b[5] + a[8] * b[6] + a[12] * b[7];
	product[5]  = a[1] * b[4] + a[5] * b[5] + a[9] * b[6] + a[13] * b[7];
	product[6]  = a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7];
	product[7]  = a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7];
	product[8]  = a[0] * b[8] + a[4] * b[9] + a[8] * b[10] + a[12] * b[11];
	product[9]  = a[1] * b[8] + a[5] * b[9] + a[9] * b[10] + a[13] * b[11];
	product[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11];
	product[11] = a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11];
	product[12] = a[0] * b[12] + a[4] * b[13] + a[8] * b[14] + a[12] * b[15];
	product[13] = a[1] * b[12] + a[5] * b[13] + a[9] * b[14] + a[13] * b[15];
	product[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15];
	product[15] = a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15];
}

void Matrix4_MultiplyFast (mat4_t a, mat4_t b, mat4_t product)
{
	product[0]  = a[0] * b[0] + a[4] * b[1] + a[8] * b[2];
	product[1]  = a[1] * b[0] + a[5] * b[1] + a[9] * b[2];
	product[2]  = a[2] * b[0] + a[6] * b[1] + a[10] * b[2];
	product[3]  = 0.0f;
	product[4]  = a[0] * b[4] + a[4] * b[5] + a[8] * b[6];
	product[5]  = a[1] * b[4] + a[5] * b[5] + a[9] * b[6];
	product[6]  = a[2] * b[4] + a[6] * b[5] + a[10] * b[6];
	product[7]  = 0.0f;
	product[8]  = a[0] * b[8] + a[4] * b[9] + a[8] * b[10];
	product[9]  = a[1] * b[8] + a[5] * b[9] + a[9] * b[10];
	product[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10];
	product[11] = 0.0f;
	product[12] = a[0] * b[12] + a[4] * b[13] + a[8] * b[14] + a[12];
	product[13] = a[1] * b[12] + a[5] * b[13] + a[9] * b[14] + a[13];
	product[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14];
	product[15] = 1.0f;
}

void Matrix4_Rotate (mat4_t a, float angle, float x, float y, float z)
{
	mat4_t m, b;
	float c = cos( DEG2RAD(angle) );
	float s = sin( DEG2RAD(angle) );
	float mc = 1 - c, t1, t2;
	
	m[0]  = (x * x * mc) + c;
	m[5]  = (y * y * mc) + c;
	m[10] = (z * z * mc) + c;

	t1 = y * x * mc;
	t2 = z * s;
	m[1] = t1 + t2;
	m[4] = t1 - t2;

	t1 = x * z * mc;
	t2 = y * s;
	m[2] = t1 - t2;
	m[8] = t1 + t2;

	t1 = y * z * mc;
	t2 = x * s;
	m[6] = t1 + t2;
	m[9] = t1 - t2;

	m[3] = m[7] = m[11] = m[12] = m[13] = m[14] = 0;
	m[15] = 1;

	Matrix4_Copy ( a, b );
	Matrix4_MultiplyFast ( b, m, a );
}

void Matrix4_Translate (mat4_t m, float x, float y, float z)
{
	m[12] = m[0] * x + m[4] * y + m[8]  * z + m[12];
	m[13] = m[1] * x + m[5] * y + m[9]  * z + m[13];
	m[14] = m[2] * x + m[6] * y + m[10] * z + m[14];
	m[15] = m[3] * x + m[7] * y + m[11] * z + m[15];
}

void Matrix4_Scale (mat4_t m, float x, float y, float z)
{
	m[0] *= x;   m[4] *= y;   m[8]  *= z;
	m[1] *= x;   m[5] *= y;   m[9]  *= z;
	m[2] *= x;   m[6] *= y;   m[10] *= z;
	m[3] *= x;   m[7] *= y;   m[11] *= z;
}

void Matrix4_Transpose (mat4_t m, mat4_t ret)
{
	ret[0] = m[0]; ret[1] = m[4]; ret[2] = m[8]; ret[3] = m[12];
	ret[4] = m[1]; ret[5] = m[5]; ret[6] = m[9]; ret[7] = m[13];
	ret[8] = m[2]; ret[9] = m[6]; ret[10] = m[10]; ret[11] = m[14];
	ret[12] = m[3]; ret[13] = m[7]; ret[14] = m[11]; ret[15] = m[15];
}

static void Matrix4_Submat (mat4_t mr, mat3_t mb, int i, int j)
{
    int ti, tj, idst, jdst;
	
    for ( ti = 0; ti < 4; ti++ )
	{
		if ( ti < i )
			idst = ti;
		else
			if ( ti > i )
				idst = ti-1;
			
			for ( tj = 0; tj < 4; tj++ )
			{
				if ( tj < j )
					jdst = tj;
				else
					if ( tj > j )
						jdst = tj-1;
					
					if ( ti != i && tj != j )
						mb[idst][jdst] = mr[ti*4 + tj];
			}
	}
}

float Matrix4_Det (mat4_t mr)
{
     float  det, result = 0, i = 1;
     mat3_t	msub3;
     int    n;

     for ( n = 0; n < 4; n++, i *= -1 )
	 {
		 Matrix4_Submat (mr, msub3, 0, n);
		 
		 det     = Matrix3_Det ( msub3 );
		 result += mr[n] * det * i;
	 }
	 
	 return result;
}

void Matrix4_Inverse (mat4_t mr, mat4_t ma)
{
    float  mdet = Matrix4_Det( mr );
    mat3_t mtemp;
	int    i, j, sign;
	
	if ( fabs( mdet ) < 0.0005 )
	{
		Matrix4_Identity ( ma );
        return;
	}

	mdet = 1.0f / mdet;
	for ( i = 0; i < 4; i++ )
		for ( j = 0; j < 4; j++ )
		{
			sign = 1 - ( (i + j) % 2 ) * 2;
			
			Matrix4_Submat( mr, mtemp, i, j );
			ma[i+j*4] = ( Matrix3_Det( mtemp ) * sign ) * mdet;
		}
}

void Matrix4_Matrix3 (mat4_t in, mat3_t out)
{
	out[0][0] = in[0];
	out[0][1] = in[4];
	out[0][2] = in[8];

	out[1][0] = in[1];
	out[1][1] = in[5];
	out[1][2] = in[9];

	out[2][0] = in[2];
	out[2][1] = in[6];
	out[2][2] = in[10];
}

void Matrix4_Multiply_Vec3 (mat4_t a, vec3_t b, vec3_t product)
{
	product[0] = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
	product[1] = a[4]*b[0] + a[5]*b[1] + a[6]*b[2];
	product[2] = a[8]*b[0] + a[9]*b[1] + a[10]*b[2];
}

void Matrix3_Identity (mat3_t mat)
{
	mat[0][1] = mat[0][2] = mat[1][0] = mat[1][2] = mat[2][0] = mat[2][1] = 0;
	mat[0][0] = mat[1][1] = mat[2][2] = 1.0f;
}

void Matrix3_Copy (mat3_t a, mat3_t b)
{
	VectorCopy (a[0], b[0]);
	VectorCopy (a[1], b[1]);
	VectorCopy (a[2], b[2]);
}

qboolean Matrix3_Compare (mat3_t a, mat3_t b)
{
	return ( VectorCompare (a[0], b[0]) && 
		VectorCompare (a[1], b[1]) && VectorCompare (a[2], b[2]) );
}

void Matrix3_Multiply (mat3_t in1, mat3_t in2, mat3_t out)
{
	out[0][0] = in1[0][0]*in2[0][0] + in1[0][1]*in2[1][0] + in1[0][2]*in2[2][0];
	out[0][1] = in1[0][0]*in2[0][1] + in1[0][1]*in2[1][1] + in1[0][2]*in2[2][1];
	out[0][2] = in1[0][0]*in2[0][2] + in1[0][1]*in2[1][2] + in1[0][2]*in2[2][2];
	out[1][0] = in1[1][0]*in2[0][0] + in1[1][1]*in2[1][0] +	in1[1][2]*in2[2][0];
	out[1][1] = in1[1][0]*in2[0][1] + in1[1][1]*in2[1][1] + in1[1][2]*in2[2][1];
	out[1][2] = in1[1][0]*in2[0][2] + in1[1][1]*in2[1][2] +	in1[1][2]*in2[2][2];
	out[2][0] = in1[2][0]*in2[0][0] + in1[2][1]*in2[1][0] +	in1[2][2]*in2[2][0];
	out[2][1] = in1[2][0]*in2[0][1] + in1[2][1]*in2[1][1] +	in1[2][2]*in2[2][1];
	out[2][2] = in1[2][0]*in2[0][2] + in1[2][1]*in2[1][2] +	in1[2][2]*in2[2][2];
}

void Matrix3_Multiply_Vec3 (mat3_t a, vec3_t b, vec3_t product)
{
	product[0] = a[0][0]*b[0] + a[0][1]*b[1] + a[0][2]*b[2];
	product[1] = a[1][0]*b[0] + a[1][1]*b[1] + a[1][2]*b[2];
	product[2] = a[2][0]*b[0] + a[2][1]*b[1] + a[2][2]*b[2];
}

void Matrix3_Transpose (mat3_t in, mat3_t out)
{
	out[0][0] = in[0][0];
	out[1][1] = in[1][1];
	out[2][2] = in[2][2];

	out[0][1] = in[1][0];
	out[0][2] = in[2][0];
	out[1][0] = in[0][1];
	out[1][2] = in[2][1];
	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
}

float Matrix3_Det (mat3_t mat)
{
    return (mat[0][0] * (mat[1][1]*mat[2][2] - mat[2][1]*mat[1][2])
         - mat[0][1] * (mat[1][0]*mat[2][2] - mat[2][0]*mat[1][2])
         + mat[0][2] * (mat[1][0]*mat[2][1] - mat[2][0]*mat[1][1]));
}

void Matrix3_Inverse (mat3_t mr, mat3_t ma)
{
	float det = Matrix3_Det ( mr );
	
	if ( fabs( det ) < 0.0005 )
	{
		Matrix3_Identity( ma );
		return;
	}

	det = 1.0 / det;
	
	ma[0][0] =  (mr[1][1]*mr[2][2] - mr[1][2]*mr[2][1]) * det;
	ma[0][1] = -(mr[0][1]*mr[2][2] - mr[2][1]*mr[0][2]) * det;
	ma[0][2] =  (mr[0][1]*mr[1][2] - mr[1][1]*mr[0][2]) * det;
	
	ma[1][0] = -(mr[1][0]*mr[2][2] - mr[1][2]*mr[2][0]) * det;
	ma[1][1] =  (mr[0][0]*mr[2][2] - mr[2][0]*mr[0][2]) * det;
	ma[1][2] = -(mr[0][0]*mr[1][2] - mr[1][0]*mr[0][2]) * det;
	
	ma[2][0] =  (mr[1][0]*mr[2][1] - mr[2][0]*mr[1][1]) * det;
	ma[2][1] = -(mr[0][0]*mr[2][1] - mr[2][0]*mr[0][1]) * det;
	ma[2][2] =  (mr[0][0]*mr[1][1] - mr[0][1]*mr[1][0]) * det;
}

void Matrix3_EulerAngles (mat3_t mat, vec3_t angles)
{
	float	c;
	float	pitch, yaw, roll;

	pitch = -asin( mat[0][2] );
	c = cos ( pitch );
	pitch = RAD2DEG( pitch );

	if ( fabs( c ) > 0.005 )             // Gimball lock?
	{
		c = 1.0f / c;
		yaw = RAD2DEG( atan2 ( (-1)*-mat[0][1] * c, mat[0][0] * c ) );
		roll = RAD2DEG( atan2 ( -mat[1][2] * c, mat[2][2] * c ) );
	}
	else
	{
		if (mat[0][2] > 0)
			pitch = -90;
		else
			pitch = 90;
		yaw = RAD2DEG( atan2 ( mat[1][0], (-1)*mat[1][1] ) );
		roll = 0;
	}

	angles[PITCH] = anglemod( pitch );
	angles[YAW] = anglemod( yaw );
	angles[ROLL] = anglemod( roll );
}

void Matrix_Multiply_Vec2 (mat4_t a, vec2_t b, vec2_t product)
{
	product[0] = a[0]*b[0] + a[1]*b[1] + a[2] + a[3];
	product[1] = a[4]*b[0] + a[5]*b[1] + a[6] + a[7];
}

void Matrix_Multiply_Vec3 (mat4_t a, vec3_t b, vec3_t product)
{
	product[0] = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[12];
	product[1] = a[4]*b[0] + a[5]*b[1] + a[6]*b[2] + a[13];
	product[2] = a[8]*b[0] + a[8]*b[1] + a[10]*b[2] + a[14];
}

void Matrix_Multiply_Vec4 (mat4_t a, vec4_t b, vec4_t product)
{
	product[0] = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[12]*b[3];
	product[1] = a[4]*b[0] + a[5]*b[1] + a[6]*b[2] + a[13]*b[3];
	product[2] = a[8]*b[0] + a[9]*b[1] + a[10]*b[2] + a[14]*b[3];
	product[3] = a[3]*b[0] + a[7]*b[1] + a[11]*b[2] + a[15]*b[3];
}

void Matrix3_Rotate (mat3_t a, float angle, float x, float y, float z)
{
	mat3_t m, b;
	float c = cos( DEG2RAD(angle) );
	float s = sin( DEG2RAD(angle) );
	float mc = 1 - c, t1, t2;
	
	m[0][0] = (x * x * mc) + c;
	m[1][1] = (y * y * mc) + c;
	m[2][2] = (z * z * mc) + c;

	t1 = y * x * mc;
	t2 = z * s;
	m[0][1] = t1 + t2;
	m[1][0] = t1 - t2;

	t1 = x * z * mc;
	t2 = y * s;
	m[0][2] = t1 - t2;
	m[2][0] = t1 + t2;

	t1 = y * z * mc;
	t2 = x * s;
	m[1][2] = t1 + t2;
	m[2][1] = t1 - t2;

	Matrix3_Copy ( a, b );
	Matrix3_Multiply ( b, m, a );
}
