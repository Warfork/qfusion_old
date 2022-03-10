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

#include "../qcommon/qcommon.h"
#include "cin.h"

#define	MAX_DLIGHTS		32
#define	MAX_ENTITIES	128
#define	MAX_PARTICLES	4096

#define POWERSUIT_SCALE		4.0F

#define	FONT_BIG		1
#define FONT_SMALL		2
#define FONT_GIANT		4
#define FONT_SHADOWED	8

typedef unsigned int index_t;

typedef struct 
{
	vec3_t			origin;
	float			axis[3][3];
} orientation_t;

typedef enum
{
	ET_MODEL,
	ET_SPRITE,
	ET_BEAM,
	ET_PORTALSURFACE,

	NUM_ETYPES
} etype_t;

typedef struct entity_s
{
	unsigned int		number;

	etype_t				type;
	struct model_s		*model;			// opaque type outside refresh
	struct shader_s		*customShader;	// NULL for inline skin
	float				shaderTime;
	byte_vec4_t			color;

	/*
	** most recent data
	*/
	float				origin[3];		// also used as ET_BEAM's "from"
	vec3_t				axis[3];
	float				scale;
	float				radius;			// used as ET_SPRITE's and ET_BEAM's radius
	int					frame;

	/*
	** previous data for lerping
	*/
	float				oldorigin[3];	// also used as ET_BEAM's "to"
	int					oldframe;

	/*
	** misc
	*/
	float				backlerp;		// 0.0 = current, 1.0 = old
	int					skinnum;		// used as ET_BEAM's palette index
	int					flags;
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
	int		color;
	float	alpha;
} particle_t;

typedef struct refdef_s
{
	int			x, y, width, height;// in virtual screen coordinates
	float		fov_x, fov_y;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4];			// rgba 0-1 full screen blend
	float		time;				// time is uesed to auto animate
	int			rdflags;			// RDF_UNDERWATER, etc

	byte		*areabits;			// if not NULL, only areas with set bits will be drawn

	int			num_entities;
	entity_t	*entities;

	int			num_dlights;
	dlight_t	*dlights;

	int			num_particles;
	particle_t	*particles;
} refdef_t;

void	R_BeginRegistration (char *map);
void	R_EndRegistration (void);

struct model_s	*R_RegisterModel (char *name);

struct shader_s *R_RegisterPic (char *name);
struct shader_s *R_RegisterShader (char *name);
struct shader_s *R_RegisterSkin (char *name);

void	R_RenderFrame (refdef_t *fd);

void	Draw_StretchPic (int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *color, struct shader_s *shader);
void	Draw_Char (int x, int y, int num, int fontstyle, vec4_t color);
void	Draw_String (int x, int y, char *str, int fontstyle, vec4_t color);
void	Draw_StringLen (int x, int y, char *str, int len, int fontstyle, vec4_t color);
void	Draw_PropString (int x, int y, char *str, int fontstyle, vec4_t color);
void	Draw_CenteredPropString (int y, char *str, int fontstyle, vec4_t color);
int		Q_PropStringLength (char *str, int fontstyle);
void	Draw_TileClear (int x, int y, int w, int h, char *name);
void	Draw_FillRect (int x, int y, int w, int h, vec4_t color);
void	Draw_FadeScreen (void);
void	Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, int frame, byte *data);

void	R_BeginFrame( float camera_separation );
void	R_ApplySoftwareGamma( void );
void	R_Flush (void);

void	GLimp_EndFrame (void);

void	GLimp_AppActivate( qboolean active );

int		R_Init( void *hinstance, void *hWnd );
void	R_Shutdown (void);

#endif // __REF_H
