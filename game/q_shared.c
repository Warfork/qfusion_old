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

vec3_t		vec3_origin = {0,0,0};

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

//============================================================================

void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up)
{
	float		d;

	// this rotate and negat guarantees a vector
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

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float		angle;
	static float		sr, sp, sy, cr, cp, cy;
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
		right[0] = (-1*sr*sp*cy+-1*cr*-sy);
		right[1] = (-1*sr*sp*sy+-1*cr*cy);
		right[2] = -1*sr*cp;
	}
	if (up)
	{
		up[0] = (cr*sp*cy+-sr*-sy);
		up[1] = (cr*sp*sy+-sr*cy);
		up[2] = cr*cp;
	}
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

//============================================================================

float Q_fabs (float f)
{
	int tmp = * ( int * ) &f;
	tmp &= 0x7FFFFFFF;
	return * ( float * ) &tmp;
}

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

#if defined _M_IX86 && !defined C_ONLY
#pragma warning (disable:4035)
__declspec( naked ) long Q_ftol( float f )
{
	static int tmp;
	__asm fld dword ptr [esp+4]
	__asm fistp tmp
	__asm mov eax, tmp
	__asm ret
}
#pragma warning (default:4035)
#endif

/*
===============
LerpAngle

===============
*/
float LerpAngle (float a2, float a1, float frac)
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

int		i;
vec3_t	corners[2];


// this is the slow, general version
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	int		i;
	float	dist1, dist2;
	int		sides;
	vec3_t	corners[2];

	for (i=0 ; i<3 ; i++)
	{
		if (p->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

	return sides;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
#if !id386 || defined __linux__ 
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
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
#else
#pragma warning( disable: 4035 )

__declspec( naked ) int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	static int bops_initialized;
	static int Ljmptab[8];

	__asm {

		push ebx
			
		cmp bops_initialized, 1
		je  initialized
		mov bops_initialized, 1
		
		mov Ljmptab[0*4], offset Lcase0
		mov Ljmptab[1*4], offset Lcase1
		mov Ljmptab[2*4], offset Lcase2
		mov Ljmptab[3*4], offset Lcase3
		mov Ljmptab[4*4], offset Lcase4
		mov Ljmptab[5*4], offset Lcase5
		mov Ljmptab[6*4], offset Lcase6
		mov Ljmptab[7*4], offset Lcase7
			
initialized:

		mov edx,ds:dword ptr[4+12+esp]
		mov ecx,ds:dword ptr[4+4+esp]
		xor eax,eax
		mov ebx,ds:dword ptr[4+8+esp]
		mov al,ds:byte ptr[17+edx]
		cmp al,8
		jge Lerror
		fld ds:dword ptr[0+edx]
		fld st(0)
		jmp dword ptr[Ljmptab+eax*4]
Lcase0:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase1:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase2:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase3:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase4:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase5:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase6:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase7:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
LSetSides:
		faddp st(2),st(0)
		fcomp ds:dword ptr[12+edx]
		xor ecx,ecx
		fnstsw ax
		fcomp ds:dword ptr[12+edx]
		and ah,1
		xor ah,1
		add cl,ah
		fnstsw ax
		and ah,1
		add ah,ah
		add cl,ah
		pop ebx
		mov eax,ecx
		ret
Lerror:
		int 3
	}
}
#pragma warning( default: 4035 )
#endif

void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs)
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
		
	return length;

}

void VectorNormalizeFast (vec3_t v)
{
	float ilength = Q_RSqrt (DotProduct(v,v));

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
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


void Matrix4_Identity(mat4_t mat)
{
	memset(mat, 0, 16 * sizeof(float));
	mat[0] = mat[5] = mat[10] = mat[15] = 1.0f;
}

void Matrix3_Identity (vec3_t mat[3])
{
	memset(mat, 0, 9 * sizeof(float));
	mat[0][0] = mat[1][1] = mat[2][2] = 1.0f;
}

void Matrix4_Multiply(mat4_t a, mat4_t b, mat4_t product)
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

void Matrix4_MultiplyFast(mat4_t a, mat4_t b, mat4_t product)
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

void Matrix3_Multiply (vec3_t in1[3], vec3_t in2[3], vec3_t out[3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}

void Matrix3_Multiply_Vec3 (vec3_t a[3], vec3_t b, vec3_t product)
{
	product[0] = a[0][0]*b[0] + a[1][0]*b[1] + a[2][0]*b[2];
	product[1] = a[0][1]*b[0] + a[1][1]*b[1] + a[2][1]*b[2];
	product[2] = a[0][2]*b[0] + a[1][2]*b[1] + a[2][2]*b[2];
}

// This can be used to calc the inverse of a rotation matrix 
void Matrix3_Transponse (vec3_t in[3], vec3_t out[3])
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

void Matrix_Multiply_Vec4 (mat4_t a, vec4_t b, vec4_t product)
{
	product[0] = a[0]*b[0] + a[4]*b[1] + a[8]*b[2] + a[12]*b[3];
	product[1] = a[1]*b[0] + a[5]*b[1] + a[9]*b[2] + a[13]*b[3];
	product[2] = a[2]*b[0] + a[6]*b[1] + a[10]*b[2] + a[14]*b[3];
	product[3] = a[3]*b[0] + a[7]*b[1] + a[11]*b[2] + a[15]*b[3];
}

void Matrix_Multiply_Vec3 (mat4_t a, vec3_t b, vec3_t product)
{
	product[0] = a[0]*b[0] + a[4]*b[1] + a[8]*b[2] + a[12];
	product[1] = a[1]*b[0] + a[5]*b[1] + a[9]*b[2] + a[13];
	product[2] = a[2]*b[0] + a[6]*b[1] + a[10]*b[2] + a[14];
}

void Matrix4_Transponse (mat4_t m, mat4_t ret)
{
	ret[0] = m[0]; ret[1] = m[4]; ret[2] = m[8]; ret[3] = m[12];
	ret[4] = m[1]; ret[5] = m[5]; ret[6] = m[9]; ret[7] = m[13];
	ret[8] = m[2]; ret[9] = m[6]; ret[10] = m[10]; ret[11] = m[14];
	ret[12] = m[3]; ret[13] = m[7]; ret[14] = m[11]; ret[15] = m[15];
}

void Matrix4_Matrix3 (mat4_t in, mat3x3_t out)
{
	out[0][0] = in[0];
	out[0][1] = in[1];
	out[0][2] = in[2];

	out[1][0] = in[4];
	out[1][1] = in[5];
	out[1][2] = in[6];

	out[2][0] = in[8];
	out[2][1] = in[9];
	out[2][2] = in[10];
}

void Matrix_Multiply_Vec2 (mat4_t a, vec2_t b, vec2_t product)
{
	product[0] = a[0]*b[0] + a[1]*b[1] + a[2] + a[3];
	product[1] = a[4]*b[0] + a[5]*b[1] + a[6] + a[7];
}

static float Matrix3_Det (float *mat)
{
    return (mat[0] * (mat[4]*mat[8] - mat[7]*mat[5])
         - mat[1] * (mat[3]*mat[8] - mat[6]*mat[5])
         + mat[2] * (mat[3]*mat[7] - mat[6]*mat[4]));
}

static void Matrix3_Inverse(float *mr, float *ma)
{
	float det = Matrix3_Det ( ma );
	
	if ( fabs( det ) < 0.0005 )
	{
		Matrix3_Identity( (float (*)[3])ma );
		return;
	}

	det = 1 / det;
	
	mr[0] =   ma[4]*ma[8] - ma[5]*ma[7]  * det;
	mr[1] = -(ma[1]*ma[8] - ma[7]*ma[2]) * det;
	mr[2] =   ma[1]*ma[5] - ma[4]*ma[2]  * det;
	
	mr[3] = -(ma[3]*ma[8] - ma[5]*ma[6]) * det;
	mr[4] =   ma[0]*ma[8] - ma[6]*ma[2]  * det;
	mr[5] = -(ma[0]*ma[5] - ma[3]*ma[2]) * det;
	
	mr[6] =   ma[3]*ma[7] - ma[6]*ma[4]  * det;
	mr[7] = -(ma[0]*ma[7] - ma[6]*ma[1]) * det;
	mr[8] =   ma[0]*ma[4] - ma[1]*ma[3]  * det;
}

static void Matrix4_Submat( mat4_t mr, float * mb, int i, int j )
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
						mb[idst*3 + jdst] = mr[ti*4 + tj ];
			}
	}
}

static float Matrix4_Det( mat4_t mr )
{
     float  det, result = 0, i = 1;
     float	msub3[9];
     int    n;

     for ( n = 0; n < 4; n++, i *= -1 )
	 {
		 Matrix4_Submat(mr, msub3, 0, n);
		 
		 det     = Matrix3_Det( msub3 );
		 result += mr[n] * det * i;
	 }
	 
	 return result;
}

qboolean Matrix4_Inverse( mat4_t mr, mat4_t ma )
{
    float  mdet = Matrix4_Det( ma );
    float  mtemp[9];
	int    i, j, sign;
	
	if ( fabs( mdet ) < 0.0005 )
        return 0;
	
	for ( i = 0; i < 4; i++ )
		for ( j = 0; j < 4; j++ )
		{
			sign = 1 - ( (i + j) % 2 ) * 2;
			
			Matrix4_Submat( ma, mtemp, i, j );
			
			mr[i+j*4] = ( Matrix3_Det( mtemp ) * sign ) / mdet;
		}
		
	return 1;
}

//====================================================================================

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static char exten[8];
	int		i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i=0 ; i<7 && *in ; i++,in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char *p, *start, *end;
	int length; 
	
	end = in + strlen(in);
	start = in;
	
	for (p = end-1 ; p > in ; p--)
	{
		if (*p == '.')
			end = p;
		else if (*p == '/')
		{
			start = p + 1;
			break;
		}
	}
	
	length = end - start;
	if (length <= 0)
		strcpy (out, "?empty filename?");
	else
	{
		if (length > 31)
			length = 31;
		memcpy (out, start, length);
		out[length] = 0;
	}
}

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath (char *in, char *out)
{
	char *s;
	
	s = in + strlen(in) - 1;
	
	while (s != in && *s != '/')
		s--;

	strncpy (out,in, s-in);
	out[s-in] = 0;
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, char *extension)
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean	bigendien;

// can't just use function pointers, or dll linkage can
// mess up when qcommon is included in multiple places
short	(*_BigShort) (short l);
short	(*_LittleShort) (short l);
int		(*_BigLong) (int l);
int		(*_LittleLong) (int l);
float	(*_BigFloat) (float l);
float	(*_LittleFloat) (float l);

short	BigShort(short l){return _BigShort(l);}
short	LittleShort(short l) {return _LittleShort(l);}
int		BigLong (int l) {return _BigLong(l);}
int		LittleLong (int l) {return _LittleLong(l);}
float	BigFloat (float l) {return _BigFloat(l);}
float	LittleFloat (float l) {return _LittleFloat(l);}

short   ShortSwap (short l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

short	ShortNoSwap (short l)
{
	return l;
}

int    LongSwap (int l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

int	LongNoSwap (int l)
{
	return l;
}

float FloatSwap (float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;
	
	
	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
================
Swap_Init
================
*/
void Swap_Init (void)
{
	byte	swaptest[2] = {1,0};

// set the byte swapping variables in a portable manner	
	if ( *(short *)swaptest == 1)
	{
		bigendien = false;
		_BigShort = ShortSwap;
		_LittleShort = ShortNoSwap;
		_BigLong = LongSwap;
		_LittleLong = LongNoSwap;
		_BigFloat = FloatSwap;
		_LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		_BigShort = ShortNoSwap;
		_LittleShort = ShortSwap;
		_BigLong = LongNoSwap;
		_LittleLong = LongSwap;
		_BigFloat = FloatNoSwap;
		_LittleFloat = FloatSwap;
	}

}



/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char	*va(char *format, ...)
{
	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);

	return string;	
}


char	com_token[MAX_TOKEN_CHARS];

/*
==============
COM_ParseExt

Parse a token out of a string
==============
*/
char *COM_ParseExt (char **data_p, qboolean nl)
{
	int		c;
	int		len;
	char	*data;
	qboolean newlines = false;

	data = *data_p;
	len = 0;
	com_token[0] = 0;
	
	if (!data)
	{
		*data_p = NULL;
		return "";
	}
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		if (c == '\n')
			newlines = true;
		data++;
	}

	if ( newlines && !nl ) {
		*data_p = data;
		return com_token;
	}

// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
	{
//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}

/*
===============
Com_PageInMemory

===============
*/
int	paged_total;

void Com_PageInMemory (byte *buffer, int size)
{
	int		i;

	for (i=size-1 ; i>0 ; i-=4096)
		paged_total += buffer[i];
}



/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

// FIXME: replace all Q_stricmp with Q_strcasecmp
int Q_stricmp (char *s1, char *s2)
{
#if defined(WIN32)
	return _stricmp (s1, s2);
#else
	return strcasecmp (s1, s2);
#endif
}


int Q_strncasecmp (char *s1, char *s2, int n)
{
	int		c1, c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point
		
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		// strings not equal
		}
	} while (c1);
	
	return 0;		// strings are equal
}

int Q_strcasecmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}



void Com_sprintf (char *dest, int size, char *fmt, ...)
{
	int		len;
	va_list		argptr;
	char	bigbuffer[0x10000];

	va_start (argptr,fmt);
	len = vsprintf (bigbuffer,fmt,argptr);
	va_end (argptr);
	if (len >= size)
		Com_Printf ("Com_sprintf: overflow of %i(%s) in %i\n", len, bigbuffer, size);
	strncpy (dest, bigbuffer, size-1);
}

/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;
	
	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\"))
	{
//		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate (char *s)
{
	if (strstr (s, "\""))
		return false;
	if (strstr (s, ";"))
		return false;
	return true;
}

void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int		c;
	int		maxsize = MAX_INFO_STRING;

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, ";") )
	{
		Com_Printf ("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_INFO_KEY-1 || strlen(value) > MAX_INFO_KEY-1)
	{
		Com_Printf ("Keys and values must be < 64 characters.\n");
		return;
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	Com_sprintf (newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > maxsize)
	{
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;		// strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	*s = 0;
}

//====================================================================


// Vic added this
void *Q_malloc (int cnt)
{
	void *buf = (void *)malloc (cnt);

	if (!buf) {
		Com_Printf ("Q_malloc: failed on allocation of %i bytes.\n", cnt);
		return NULL;
	}

	memset (buf, 0, cnt);
	return buf;
}

void Q_free (void *buf)
{
	if (!buf) {
		return;
	}

	free (buf);
}

void Q_Obituary ( void *victim, void *attacker, int mod, char *message, char *message2 )
{
	int	gender = Q_PlayerGender ( victim );

	message [0] = 0;
	message2[0] = 0;

	switch (mod)
	{
		case MOD_SUICIDE:
			strcpy ( message, "suicides" );
			break;
		case MOD_FALLING:
			strcpy ( message, "cratered" );
			break;
		case MOD_CRUSH:
			strcpy ( message, "was squished" );
			break;
		case MOD_WATER:
			strcpy ( message, "sank like a rock" );
			break;
		case MOD_SLIME:
			strcpy ( message, "melted" );
			break;
		case MOD_LAVA:
			strcpy ( message, "does a back flip into the lava" );
			break;
		case MOD_EXPLOSIVE:
		case MOD_BARREL:
			strcpy ( message, "blew up" );
			break;
		case MOD_EXIT:
			strcpy ( message, "found a way out" );
			break;
		case MOD_TARGET_LASER:
			strcpy ( message, "saw the light" );
			break;
		case MOD_TARGET_BLASTER:
			strcpy ( message, "got blasted" );
			break;
		case MOD_BOMB:
		case MOD_SPLASH:
		case MOD_TRIGGER_HURT:
			strcpy ( message, "was in the wrong place" );
			break;
	}
	
	if ( attacker == victim ) 
	{
		switch (mod)
		{
		case MOD_HELD_GRENADE:
			strcpy ( message, "tried to put the pin back in" );
			break;
		case MOD_HG_SPLASH:
		case MOD_G_SPLASH:
			if ( gender == GENDER_FEMALE )
				strcpy ( message, "tripped on her own grenade" );
			else if ( gender == GENDER_MALE )
				strcpy ( message, "tripped on his own grenade" );
			else
				strcpy ( message, "tripped on its own grenade" );
			break;
		case MOD_R_SPLASH:
			if ( gender == GENDER_FEMALE )
				strcpy ( message, "blew herself up" );
			else if ( gender == GENDER_MALE )
				strcpy ( message, "blew himself up" );
			else
				strcpy ( message, "blew itself up" );
			break;
		case MOD_BFG_BLAST:
			strcpy ( message, "should have used a smaller gun" );
			break;
		default:
			if ( gender == GENDER_FEMALE )
				strcpy ( message, "killed herself" );
			else if ( gender == GENDER_MALE )
				strcpy ( message, "killed himself" );
			else
				strcpy ( message, "killed itself" );
			break;
		}
	}

	// we already have one
	if ( message[0] ) {
		return;
	}

	if ( attacker ) {
		switch (mod)
		{
		case MOD_BLASTER:
			strcpy ( message, "was blasted by" );
			break;
		case MOD_SHOTGUN:
			strcpy ( message, "was gunned down by" );
			break;
		case MOD_SSHOTGUN:
			strcpy ( message, "was blown away by" );
			strcpy ( message2, "'s super shotgun" );
			break;
		case MOD_MACHINEGUN:
			strcpy ( message, "was machinegunned by" );
			break;
		case MOD_CHAINGUN:
			strcpy ( message, "was cut in half by" );
			strcpy ( message2, "'s chaingun" );
			break;
		case MOD_GRENADE:
			strcpy ( message, "was popped by" );
			strcpy ( message2, "'s grenade" );
			break;
		case MOD_G_SPLASH:
			strcpy ( message, "was shredded by" );
			strcpy ( message, "'s shrapnel" );
			break;
		case MOD_ROCKET:
			strcpy ( message, "ate" );
			strcpy ( message2, "'s rocket" );
			break;
		case MOD_R_SPLASH:
			strcpy ( message, "almost dodged" );
			strcpy ( message2, "'s rocket" );
			break;
		case MOD_HYPERBLASTER:
			strcpy ( message, "was melted by" );
			strcpy ( message2,"'s hyperblaster" );
			break;
		case MOD_RAILGUN:
			strcpy ( message, "was railed by" );
			break;
		case MOD_BFG_LASER:
			strcpy ( message, "saw the pretty lights from" );
			strcpy ( message2, "'s BFG" );
			break;
		case MOD_BFG_BLAST:
			strcpy ( message, "was disintegrated by" );
			strcpy ( message2, "'s BFG blast" );
			break;
		case MOD_BFG_EFFECT:
			strcpy ( message, "couldn't hide from" );
			strcpy ( message2, "'s BFG" );
			break;
		case MOD_HANDGRENADE:
			strcpy ( message, "caught" );
			strcpy ( message2, "'s handgrenade" );
			break;
		case MOD_HG_SPLASH:
			strcpy ( message, "didn't see" );
			strcpy ( message2, "'s handgrenade" );
			break;
		case MOD_HELD_GRENADE:
			strcpy ( message, "feels" );
			strcpy ( message2, "'s pain" );
			break;
		case MOD_TELEFRAG:
			strcpy ( message, "tried to invade" );
			strcpy ( message2, "'s personal space" );
			break;
			//ZOID
		case MOD_GRAPPLE:
			strcpy ( message, "was caught by" );
			strcpy ( message2, "'s grapple" );
			break;
			//ZOID

		default:
			strcpy ( message, "was killed by" );
			break;
		}
		
		if ( message[0] ) {
			return;
		}
	}

	strcpy ( message, " died." );
}

