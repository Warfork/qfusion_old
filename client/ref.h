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

#include "qcommon.h"

#define	MAX_DLIGHTS		32
#define	MAX_ENTITIES	128
#define	MAX_PARTICLES	4096

#define POWERSUIT_SCALE		4.0F

#define SHELL_RED_COLOR		0xF2
#define SHELL_GREEN_COLOR	0xD0
#define SHELL_BLUE_COLOR	0xF3

#define SHELL_RG_COLOR		0xDC
//#define SHELL_RB_COLOR		0x86
#define SHELL_RB_COLOR		0x68
#define SHELL_BG_COLOR		0x78

#define SHELL_WHITE_COLOR	0xD7

typedef enum 
{
	FONT_BIG,
	FONT_SMALL,
	FONT_GIANT
} fontstyle_t;

typedef struct entity_s
{
	struct model_s		*model;			// opaque type outside refresh
	float				angles[3];

	/*
	** most recent data
	*/
	float				origin[3];		// also used as RF_BEAM's "from"
	int					frame;			// also used as RF_BEAM's diameter

	/*
	** previous data for lerping
	*/
	float				oldorigin[3];	// also used as RF_BEAM's "to"
	int					oldframe;

	/*
	** misc
	*/
	float	backlerp;				// 0.0 = current, 1.0 = old
	int		skinnum;				// also used as RF_BEAM's palette index

	int		lightstyle;				// for flashing entities
	float	alpha;					// ignore if RF_TRANSLUCENT isn't set

	struct shader_s	*skin;			// NULL for inline skin
	int		flags;

	vec3_t	angleVectors[3];
	float	scale;

	qboolean	transignore;

	int		visframe;
} entity_t;

#define ENTITY_FLAGS  68

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
	float		fov_x, fov_y, cos_half_fox_x;
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

void	R_SetPalette ( const unsigned char *palette);
void	Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);

void	R_BeginRegistration (char *map);

struct model_s	*R_RegisterModel (char *name);

struct shader_s *R_RegisterShaderNoMip (char *name);
struct shader_s *R_RegisterShader (char *name);
struct shader_s *R_RegisterShaderMD3 (char *name);

int		R_ModelNumFrames ( struct model_s *model );

void	R_SetGridsize (int x, int y, int z);
void	R_EndRegistration (void);

void	R_RenderFrame (refdef_t *fd);

void	Draw_Pic (int x, int y, char *name);
void	Draw_StretchPic (int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *colour, struct shader_s *shader);
void	Draw_Char (int x, int y, int num, fontstyle_t fntstl, vec4_t colour);
void	Draw_StringLen (int x, int y, char *str, int len, fontstyle_t fntstl, vec4_t colour);
void	Draw_TileClear (int x, int y, int w, int h, char *name);
void	Draw_Fill (int x, int y, int w, int h, int c);
void	Draw_FadeScreen (void);
void	Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);

void	R_BeginFrame( float camera_separation );
void	GLimp_EndFrame (void);

void	GLimp_AppActivate( qboolean active );

qboolean R_Init( void *hinstance, void *hWnd );
void	R_Shutdown (void);

#endif // __REF_H
