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
#define	MAX_DLIGHTS				32
#define	MAX_ENTITIES			2048
#define MAX_POLY_VERTS			3000
#define MAX_POLYS				2048

// refdef flags
#define	RDF_UNDERWATER			1		// warp the screen as apropriate
#define RDF_NOWORLDMODEL		2		// used for player configuration screen
#define RDF_BLOOM				4		// FIXME?
#define RDF_OLDAREABITS			8		// forces R_MarkLeaves if not set
#define RDF_PORTALINVIEW		16		// cull entities using vis too because pvs\areabits are merged serverside
#define RDF_SKYPORTALINVIEW		32		// draw skyportal instead of regular sky
#define RDF_NOFOVADJUSTMENT		64		// do not adjust fov for widescreen
#define RDF_WORLDOUTLINES		128     // draw cell outlines for world surfaces

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
	int					fognum;			// -1 - do not bother adding fog later at rendering stage
										//  0 - determine fog later
										// >0 - valid fog volume number returned by R_GetClippedFragments
	vec3_t				normal;
} fragment_t;

typedef struct
{
	int					numverts;
	vec3_t				*verts;
	vec2_t				*stcoords;
	byte_vec4_t			*colors;
	struct shader_s		*shader;
	int					fognum;
	vec3_t				normal;
} poly_t;

typedef struct
{
	float				rgb[3];			// 0.0 - 2.0
} lightstyle_t;

typedef struct
{
	float				fov;
	float				scale;
	vec3_t				vieworg;
	vec3_t				viewanglesOffset;
} skyportal_t;

typedef enum
{
	RT_MODEL,
	RT_SPRITE,
	RT_PORTALSURFACE,

	NUM_RTYPES
} refEntityType_t;

typedef struct entity_s
{
	refEntityType_t		rtype;
	union
	{
		int				flags;
		int				renderfx;
	};

	struct model_s		*model;			// opaque type outside refresh

	/*
	** most recent data
	*/
	vec3_t				axis[3];
	vec3_t				origin, origin2;
	vec3_t				lightingOrigin;
	int					frame;
	bonepose_t			*boneposes;		// pretransformed boneposes for current frame

	/*
	** previous data for lerping
	*/
	int					oldframe;
	bonepose_t			*oldboneposes;	// pretransformed boneposes for old frame
	float				backlerp;		// 0.0 = current, 1.0 = old

	/*
	** texturing
	*/
	int					skinNum;
	struct skinfile_s	*customSkin;	// registered .skin file
	struct shader_s		*customShader;	// NULL for inline skin

	/*
	** misc
	*/
	unsigned int		shaderTime;
	union
	{
		byte_vec4_t		color;
		qbyte			shaderRGBA[4];
	};

	float				scale;
	float				radius;			// used as RT_SPRITE's radius
	float				rotation;

#ifdef HARDWARE_OUTLINES
	float				outlineHeight;
	union
	{
		byte_vec4_t		outlineColor;
		qbyte			outlineRGBA[4];
	};
#endif
} entity_t;

typedef struct
{
	int					x, y, width, height;	// in virtual screen coordinates
	float				fov_x, fov_y;
	float				vieworg[3];
	float				viewangles[3];
	float				blend[4];				// rgba 0-1 full screen blend
	unsigned int		time;					// time is used for timing offsets
	int					rdflags;				// RDF_UNDERWATER, etc
	skyportal_t			skyportal;
	qbyte				*areabits;				// if not NULL, only areas with set bits will be drawn
} refdef_t;

#endif // __REF_H
