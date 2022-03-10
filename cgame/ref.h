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

#define	MAX_DLIGHTS		32
#define	MAX_ENTITIES	2048
#define MAX_POLY_VERTS	3000
#define MAX_POLYS		2048

typedef struct 
{
	vec3_t			origin;
	float			axis[3][3];
} orientation_t;

typedef struct
{
	int				firstvert;
	int				numverts;		// can't exceed MAX_POLY_VERTS
} fragment_t;

typedef struct
{
	int				numverts;
	vec3_t			*verts;
	vec2_t			*stcoords;
	byte_vec4_t		*colors;
	struct shader_s	*shader;
} poly_t;

typedef enum
{
	RT_MODEL,
	RT_SPRITE,
	RT_PORTALSURFACE,
	RT_POLY,

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

	/*
	** most recent data
	*/
	vec3_t				origin;
	vec3_t				lightingOrigin;
	vec3_t				axis[3];

	/*
	** previous data for lerping
	*/
	float				oldorigin[3];
	int					oldframe;

	/*
	** misc
	*/
	float				backlerp;		// 0.0 = current, 1.0 = old
	int					skinnum;
	int					flags;
	float				scale;
	float				radius;			// used as RT_SPRITE's radius
	float				rotation;
	int					frame;
} entity_t;

typedef struct
{
	vec3_t	origin;
	vec3_t	color;
	float	intensity;
} dlight_t;

typedef struct
{
	vec3_t	origin;
	qbyte	color[4];
	float	scale;
} particle_t;

typedef struct
{
	int			x, y, width, height;// in virtual screen coordinates
	float		fov_x, fov_y;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4];			// rgba 0-1 full screen blend
	float		time;				// time is used for timing offsets
	int			rdflags;			// RDF_UNDERWATER, etc

	qbyte		*areabits;			// if not NULL, only areas with set bits will be drawn

	int			num_entities;
	entity_t	*entities;

	int			num_dlights;
	dlight_t	*dlights;

	int			num_polys;
	poly_t		*polys;
} refdef_t;

#endif // __REF_H
