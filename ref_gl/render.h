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
#include "../client/ref.h"

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
void	Draw_FillRect (int x, int y, int w, int h, vec4_t color);
void	Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, int frame, byte *data);

void	R_LerpAttachment ( orientation_t *orient, struct model_s *mod, int frame, int oldframe, float backlerp, char *name );

int		R_GetClippedFragments ( vec3_t origin, float radius, mat3_t axis, int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments );

void	R_BeginFrame( float camera_separation );
void	R_ApplySoftwareGamma( void );
void	R_Flush (void);

void	R_ScreenShot (qboolean silent);

void	GLimp_EndFrame (void);

void	GLimp_AppActivate( qboolean active );

int		R_Init( void *hinstance, void *hWnd );
void	R_Shutdown (void);
