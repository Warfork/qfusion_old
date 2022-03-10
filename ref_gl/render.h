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
#ifndef __RENDER_H__
#define __RENDER_H__

#include "../cgame/ref.h"

int		R_Init( void *hinstance, void *hWnd );
void	R_Restart( void );
void	R_Shutdown( void );

void	R_RegisterWorldModel( char *model );
void	R_ModelBounds( struct model_s *model, vec3_t mins, vec3_t maxs );

struct model_s	*R_RegisterModel( char *name );
struct shader_s *R_RegisterPic( char *name );
struct shader_s *R_RegisterShader( char *name );
struct shader_s *R_RegisterSkin( char *name );
struct skinfile_s *R_RegisterSkinFile( char *name );

void	R_ClearScene( void );
void	R_AddEntityToScene( entity_t *ent );
void	R_AddLightToScene( vec3_t org, float intensity, float r, float g, float b, struct shader_s *shader );
void	R_AddPolyToScene( poly_t *poly );
void	R_AddLightStyleToScene( int style, float r, float g, float b );
void	R_RenderScene( refdef_t *fd );

void	R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *color, struct shader_s *shader );
void	R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, int frame, qbyte *data );

qboolean R_LerpTag( orientation_t *orient, struct model_s *mod, int oldframe, int frame, float lerpfrac, char *name );

int		R_SkeletalGetNumBones( struct model_s *mod, int *numFrames );
int		R_SkeletalGetBoneInfo( struct model_s *mod, int bone, char *name, int size, int *flags );
void	R_SkeletalGetBonePose( struct model_s *mod, int bone, int frame, bonepose_t *bonepose );

int		R_GetClippedFragments( vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments );

void	R_TransformVectorToScreen( refdef_t *rd, vec3_t in, vec2_t out );

void	R_BeginFrame( float cameraSeparation );
void	R_EndFrame( void );
void	R_ApplySoftwareGamma( void );

void	R_BeginAviDemo( void );
void	R_WriteAviFrame( int frame, qboolean scissor );
void	R_StopAviDemo( void );

void	GLimp_AppActivate( qboolean active );

extern void CL_GameModule_Trace( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignore, int contentmask );

#endif //__RENDER_H__
