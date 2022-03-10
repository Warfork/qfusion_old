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
// r_math.c
#include "../game/q_shared.h"
#include "r_math.h"

const mat4x4_t mat4x4_identity = 
{
	1, 0, 0, 0, 
	0, 1, 0, 0, 
	0, 0, 1, 0, 
	0, 0, 0, 1 
};

void Matrix4_Identity( mat4x4_t m )
{
	int i;

	for( i = 0; i < 16; i++ ) {
		if( i == 0 || i == 5 || i == 10 || i == 15 )
			m[i] = 1.0;
		else
			m[i] = 0.0;
	}
}

void Matrix4_Copy( const mat4x4_t m1, mat4x4_t m2 )
{
	int i;

	for( i = 0; i < 16; i++ )
		m2[i] = m1[i];
}

qboolean Matrix4_Compare( const mat4x4_t m1, const mat4x4_t m2 )
{
	int i;

	for( i = 0; i < 16; i++ )
		if( m1[i] != m2[i] )
			return qfalse;

	return qtrue;
}

void Matrix4_Multiply( const mat4x4_t m1, const mat4x4_t m2, mat4x4_t out )
{
	out[0]  = m1[0] * m2[0] + m1[4] * m2[1] + m1[8] * m2[2] + m1[12] * m2[3];
	out[1]  = m1[1] * m2[0] + m1[5] * m2[1] + m1[9] * m2[2] + m1[13] * m2[3];
	out[2]  = m1[2] * m2[0] + m1[6] * m2[1] + m1[10] * m2[2] + m1[14] * m2[3];
	out[3]  = m1[3] * m2[0] + m1[7] * m2[1] + m1[11] * m2[2] + m1[15] * m2[3];
	out[4]  = m1[0] * m2[4] + m1[4] * m2[5] + m1[8] * m2[6] + m1[12] * m2[7];
	out[5]  = m1[1] * m2[4] + m1[5] * m2[5] + m1[9] * m2[6] + m1[13] * m2[7];
	out[6]  = m1[2] * m2[4] + m1[6] * m2[5] + m1[10] * m2[6] + m1[14] * m2[7];
	out[7]  = m1[3] * m2[4] + m1[7] * m2[5] + m1[11] * m2[6] + m1[15] * m2[7];
	out[8]  = m1[0] * m2[8] + m1[4] * m2[9] + m1[8] * m2[10] + m1[12] * m2[11];
	out[9]  = m1[1] * m2[8] + m1[5] * m2[9] + m1[9] * m2[10] + m1[13] * m2[11];
	out[10] = m1[2] * m2[8] + m1[6] * m2[9] + m1[10] * m2[10] + m1[14] * m2[11];
	out[11] = m1[3] * m2[8] + m1[7] * m2[9] + m1[11] * m2[10] + m1[15] * m2[11];
	out[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[8] * m2[14] + m1[12] * m2[15];
	out[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[9] * m2[14] + m1[13] * m2[15];
	out[14] = m1[2] * m2[12] + m1[6] * m2[13] + m1[10] * m2[14] + m1[14] * m2[15];
	out[15] = m1[3] * m2[12] + m1[7] * m2[13] + m1[11] * m2[14] + m1[15] * m2[15];
}

void Matrix4_MultiplyFast( const mat4x4_t m1, const mat4x4_t m2, mat4x4_t out )
{
	out[0]  = m1[0] * m2[0] + m1[4] * m2[1] + m1[8] * m2[2];
	out[1]  = m1[1] * m2[0] + m1[5] * m2[1] + m1[9] * m2[2];
	out[2]  = m1[2] * m2[0] + m1[6] * m2[1] + m1[10] * m2[2];
	out[3]  = 0.0f;
	out[4]  = m1[0] * m2[4] + m1[4] * m2[5] + m1[8] * m2[6];
	out[5]  = m1[1] * m2[4] + m1[5] * m2[5] + m1[9] * m2[6];
	out[6]  = m1[2] * m2[4] + m1[6] * m2[5] + m1[10] * m2[6];
	out[7]  = 0.0f;
	out[8]  = m1[0] * m2[8] + m1[4] * m2[9] + m1[8] * m2[10];
	out[9]  = m1[1] * m2[8] + m1[5] * m2[9] + m1[9] * m2[10];
	out[10] = m1[2] * m2[8] + m1[6] * m2[9] + m1[10] * m2[10];
	out[11] = 0.0f;
	out[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[8] * m2[14] + m1[12];
	out[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[9] * m2[14] + m1[13];
	out[14] = m1[2] * m2[12] + m1[6] * m2[13] + m1[10] * m2[14] + m1[14];
	out[15] = 1.0f;
}

void Matrix4_MultiplyFast2( const mat4x4_t m1, const mat4x4_t m2, mat4x4_t out )
{
	out[0]  = m1[0] * m2[0] + m1[4] * m2[1] + m1[12] * m2[3];
	out[1]  = m1[1] * m2[0] + m1[5] * m2[1] + m1[13] * m2[3];
	out[2]  = m2[2];
	out[3]  = m2[3];
	out[4]  = m1[0] * m2[4] + m1[4] * m2[5] + m1[12] * m2[7];
	out[5]  = m1[1] * m2[4] + m1[5] * m2[5] + m1[13] * m2[7];
	out[6]  = m2[6];
	out[7]  = m2[7];
	out[8]  = m1[0] * m2[8] + m1[4] * m2[9] + m1[12] * m2[11];
	out[9]  = m1[1] * m2[8] + m1[5] * m2[9] + m1[13] * m2[11];
	out[10] = m2[10];
	out[11] = m2[11];
	out[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[12] * m2[15];
	out[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[13] * m2[15];
	out[14] = m2[14];
	out[15] = m2[15];
}

void Matrix4_Rotate( mat4x4_t m, vec_t angle, vec_t x, vec_t y, vec_t z )
{
	mat4x4_t t, b;
	vec_t c = cos( DEG2RAD(angle) );
	vec_t s = sin( DEG2RAD(angle) );
	vec_t mc = 1 - c, t1, t2;
	
	t[0]  = (x * x * mc) + c;
	t[5]  = (y * y * mc) + c;
	t[10] = (z * z * mc) + c;

	t1 = y * x * mc;
	t2 = z * s;
	t[1] = t1 + t2;
	t[4] = t1 - t2;

	t1 = x * z * mc;
	t2 = y * s;
	t[2] = t1 - t2;
	t[8] = t1 + t2;

	t1 = y * z * mc;
	t2 = x * s;
	t[6] = t1 + t2;
	t[9] = t1 - t2;

	t[3] = t[7] = t[11] = t[12] = t[13] = t[14] = 0;
	t[15] = 1;

	Matrix4_Copy( m, b );
	Matrix4_MultiplyFast( b, t, m );
}

void Matrix4_Translate( mat4x4_t m, vec_t x, vec_t y, vec_t z )
{
	m[12] = m[0] * x + m[4] * y + m[8]  * z + m[12];
	m[13] = m[1] * x + m[5] * y + m[9]  * z + m[13];
	m[14] = m[2] * x + m[6] * y + m[10] * z + m[14];
	m[15] = m[3] * x + m[7] * y + m[11] * z + m[15];
}

void Matrix4_Scale( mat4x4_t m, vec_t x, vec_t y, vec_t z )
{
	m[0] *= x; m[4] *= y; m[8]  *= z;
	m[1] *= x; m[5] *= y; m[9]  *= z;
	m[2] *= x; m[6] *= y; m[10] *= z;
	m[3] *= x; m[7] *= y; m[11] *= z;
}

void Matrix4_Transpose( const mat4x4_t m, mat4x4_t out )
{
	out[0] = m[0]; out[1] = m[4]; out[2] = m[8]; out[3] = m[12];
	out[4] = m[1]; out[5] = m[5]; out[6] = m[9]; out[7] = m[13];
	out[8] = m[2]; out[9] = m[6]; out[10] = m[10]; out[11] = m[14];
	out[12] = m[3]; out[13] = m[7]; out[14] = m[11]; out[15] = m[15];
}

void Matrix4_Matrix( const mat4x4_t in, vec3_t out[3] )
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

void Matrix4_Multiply_Vector( const mat4x4_t m, const vec4_t v, vec4_t out )
{
	out[0] = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12] * v[3];
	out[1] = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13] * v[3];
	out[2] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14] * v[3];
	out[3] = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15] * v[3];
}

//============================================================================

void Quat_Identity( quat_t q )
{
	q[0] = 0;
	q[1] = 0;
	q[2] = 0;
	q[3] = 1;
}

void Quat_Copy( const quat_t q1, quat_t q2 )
{
	q2[0] = q1[0];
	q2[1] = q1[1];
	q2[2] = q1[2];
	q2[3] = q1[3];
}

void Quat_Conjugate( const quat_t q1, quat_t q2 )
{
	q2[0] = -q1[0];
	q2[1] = -q1[1];
	q2[2] = -q1[2];
	q2[3] = q1[3];
}

vec_t Quat_Normalize( quat_t q )
{
	vec_t length;

	length = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
	if( length != 0 ) {
		vec_t ilength = 1.0 / sqrt( length );
		q[0] *= ilength;
		q[1] *= ilength;
		q[2] *= ilength;
		q[3] *= ilength;
	}

	return length;
}

vec_t Quat_Inverse( const quat_t q1, quat_t q2 )
{
	Quat_Conjugate( q1, q2 );

	return Quat_Normalize( q2 );
}

void Matrix_Quat( vec3_t m[3], quat_t q )
{
	vec_t tr, s;

	tr = m[0][0] + m[1][1] + m[2][2];
	if( tr > 0.00001 ) {
		s = sqrt( tr + 1.0 );
		q[3] = s * 0.5; s = 0.5 / s;
		q[0] = (m[2][1] - m[1][2]) * s;
		q[1] = (m[0][2] - m[2][0]) * s;
		q[2] = (m[1][0] - m[0][1]) * s;
	} else {
		int i, j, k;

		i = 0;
		if (m[1][1] > m[0][0]) i = 1;
		if (m[2][2] > m[i][i]) i = 2;
		j = (i + 1) % 3;
		k = (i + 2) % 3;

		s = sqrt( m[i][i] - (m[j][j] + m[k][k]) + 1.0 );

		q[i] = s * 0.5; if( s != 0.0 ) s = 0.5 / s;
		q[j] = (m[j][i] + m[i][j]) * s;
		q[k] = (m[k][i] + m[i][k]) * s;
		q[3] = (m[k][j] - m[j][k]) * s;
	}

	Quat_Normalize( q );
}

void Quat_Multiply( const quat_t q1, const quat_t q2, quat_t out )
{
	out[0] = q1[3] * q2[0] + q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1];
	out[1] = q1[3] * q2[1] + q1[1] * q2[3] + q1[2] * q2[0] - q1[0] * q2[2];
	out[2] = q1[3] * q2[2] + q1[2] * q2[3] + q1[0] * q2[1] - q1[1] * q2[0];
	out[3] = q1[3] * q2[3] - q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2];
}

void Quat_Lerp( const quat_t q1, const quat_t q2, vec_t t, quat_t out )
{
	quat_t p1;
	vec_t omega, cosom, sinom, scale0, scale1;

	cosom = q1[0] * q2[0] + q1[1] * q2[1] + q1[2] * q2[2] + q1[3] * q2[3];
	if( cosom < 0.0 ) { 
		cosom = -cosom;
		p1[0] = -q1[0]; p1[1] = -q1[1];
		p1[2] = -q1[2]; p1[3] = -q1[3];
	} else {
		p1[0] = q1[0]; p1[1] = q1[1];
		p1[2] = q1[2]; p1[3] = q1[3];
	}

	if( cosom < 1.0 - 0.001 ) {
		omega = acos( cosom );
		sinom = 1.0 / sin( omega );
		scale0 = sin( (1.0 - t) * omega ) * sinom;
		scale1 = sin( t * omega ) * sinom;
	} else { 
		scale0 = 1.0 - t;
		scale1 = t;
	}

	out[0] = scale0 * p1[0] + scale1 * q2[0];
	out[1] = scale0 * p1[1] + scale1 * q2[1];
	out[2] = scale0 * p1[2] + scale1 * q2[2];
	out[3] = scale0 * p1[3] + scale1 * q2[3];
}

void Quat_Matrix( const quat_t q, vec3_t m[3] )
{
	vec_t wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	x2 = q[0] + q[0]; y2 = q[1] + q[1]; z2 = q[2] + q[2];
	xx = q[0] * x2; xy = q[0] * y2; xz = q[0] * z2;
	yy = q[1] * y2; yz = q[1] * z2; zz = q[2] * z2;
	wx = q[3] * x2; wy = q[3] * y2; wz = q[3] * z2;

	m[0][0] = 1.0f - yy - zz; m[0][1] = xy - wz; m[0][2] = xz + wy;
	m[1][0] = xy + wz; m[1][1] = 1.0f - xx - zz; m[1][2] = yz - wx;
	m[2][0] = xz - wy; m[2][1] = yz + wx; m[2][2] = 1.0f - xx - yy;
}

void Quat_TransformVector( const quat_t q, const vec3_t v, vec3_t out )
{
	vec_t wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	x2 = q[0] + q[0]; y2 = q[1] + q[1]; z2 = q[2] + q[2];
	xx = q[0] * x2; xy = q[0] * y2; xz = q[0] * z2;
	yy = q[1] * y2; yz = q[1] * z2; zz = q[2] * z2;
	wx = q[3] * x2; wy = q[3] * y2; wz = q[3] * z2;

	out[0] = (1.0f - yy - zz) * v[0] + (xy - wz) * v[1] + (xz + wy) * v[2];
	out[1] = (xy + wz) * v[0] + (1.0f - xx - zz) * v[1] + (yz - wx) * v[2];
	out[2] = (xz - wy) * v[0] + (yz + wx) * v[1] + (1.0f - xx - yy) * v[2];
}

void Quat_ConcatTransforms( const quat_t q1, const vec3_t v1, const quat_t q2, const vec3_t v2, quat_t q, vec3_t v )
{
	Quat_Multiply( q1, q2, q );
	Quat_TransformVector( q1, v2, v );
	v[0] += v1[0]; v[1] += v1[1]; v[2] += v1[2];
}
