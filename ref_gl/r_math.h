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
// r_math.h
typedef vec_t mat4x4_t[16];
typedef vec_t quat_t[4];

extern const mat4x4_t mat4x4_identity;

void Matrix4_Identity( mat4x4_t m );
void Matrix4_Copy( const mat4x4_t m1, mat4x4_t m2 );
qboolean Matrix4_Compare( const mat4x4_t m1, const mat4x4_t m2 );
void Matrix4_Multiply( const mat4x4_t m1, const mat4x4_t m2, mat4x4_t out );
void Matrix4_MultiplyFast( const mat4x4_t m1, const mat4x4_t m2, mat4x4_t out );
void Matrix4_MultiplyFast2( const mat4x4_t m1, const mat4x4_t m2, mat4x4_t out );
void Matrix4_Rotate( mat4x4_t m, vec_t angle, vec_t x, vec_t y, vec_t z );
void Matrix4_Translate( mat4x4_t m, vec_t x, vec_t y, vec_t z );
void Matrix4_Scale( mat4x4_t m, vec_t x, vec_t y, vec_t z );
void Matrix4_Transpose( const mat4x4_t m, mat4x4_t out );
void Matrix4_Matrix( const mat4x4_t in, vec3_t out[3] );
void Matrix4_Multiply_Vector( const mat4x4_t m, const vec4_t v, vec4_t out );

void Quat_Identity( quat_t q );
void Quat_Copy( const quat_t q1, quat_t q2 );
void Quat_Conjugate( const quat_t q1, quat_t q2 );
vec_t Quat_Normalize( quat_t q );
vec_t Quat_Inverse( const quat_t q1, quat_t q2 );
void Quat_Multiply( const quat_t q1, const quat_t q2, quat_t out );
void Quat_Lerp( const quat_t q1, const quat_t q2, vec_t t, quat_t out );
void Quat_Matrix( const quat_t q, vec3_t m[3] );
void Matrix_Quat( vec3_t m[3], quat_t q );
void Quat_TransformVector( const quat_t q, const vec3_t v, vec3_t out );
void Quat_ConcatTransforms( const quat_t q1, const vec3_t v1, const quat_t q2, const vec3_t v2, quat_t q, vec3_t v );
