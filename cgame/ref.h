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
#ifndef __REF_H
#define __REF_H

// FIXME: move these to r_local.h?
#define	MAX_DLIGHTS			32
#define	MAX_ENTITIES		2048
#define MAX_POLY_VERTS		3000
#define MAX_POLYS			2048

// refdef flags
#define	RDF_UNDERWATER		1		// warp the screen as apropriate
#define RDF_NOWORLDMODEL	2		// used for player configuration screen

// skm flags
#define SKM_ATTACHMENT_BONE	1

typedef struct 
{
	vec3_t				axis[3];
	vec3_t				origin;
} orientation_t;

typedef struct
{
	quat_t				quat;
	vec3_t				origin;
} bonepose_t;

typedef struct
{
	int					firstvert;
	int					numverts;		// can't exceed MAX_POLY_VERTS
} fragment_t;

typedef struct
{
	int					numverts;
	vec3_t				*verts;
	vec2_t				*stcoords;
	byte_vec4_t			*colors;
	struct shader_s		*shader;
} poly_t;

typedef struct
{
	float				rgb[3];			// 0.0 - 2.0
} lightstyle_t;

typedef enum
{
	RT_MODEL,
	RT_SPRITE,
	RT_PORTALSURFACE,

	NUM_RTYPES
} entity_rtype_t;

typedef struct entity_s
{
	int					rtype;
	struct model_s		*model;			// opaque type outside refresh

	struct skinfile_s	*customSkin;	// registered .skin file
	struct shader_s		*customShader;	// NULL for inline skin

	float				shaderTime;
	byte_vec4_t			color;

	int					flags;
	float				backlerp;		// 0.0 = current, 1.0 = old

	/*
	** most recent data
	*/
	vec3_t				axis[3];
	vec3_t				origin;
	int					frame;
	bonepose_t			*boneposes;		// pretransformed boneposes for current frame

	/*
	** previous data for lerping
	*/
	vec3_t				oldorigin;
	int					oldframe;
	bonepose_t			*oldboneposes;	// pretransformed boneposes for old frame

	/*
	** misc
	*/
	int					skinnum;
	float				scale;
	float				radius;			// used as RT_SPRITE's radius
	float				rotation;
	vec3_t				lightingOrigin;
} entity_t;

typedef struct
{
	int					x, y, width, height;	// in virtual screen coordinates
	float				fov_x, fov_y;
	float				vieworg[3];
	float				viewangles[3];
	float				blend[4];				// rgba 0-1 full screen blend
	float				time;					// time is used for timing offsets
	int					rdflags;				// RDF_UNDERWATER, etc

	qbyte				*areabits;				// if not NULL, only areas with set bits will be drawn
} refdef_t;

#endif // __REF_H
