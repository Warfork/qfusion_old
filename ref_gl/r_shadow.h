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

#if SHADOW_VOLUMES

#define MAX_SHADOWVOLUME_INDEXES	MAX_ARRAY_INDEXES*4

struct mesh_s;

void R_ShadowBlend( void );
void R_BuildTriangleNeighbors (int *neighbors, index_t *indexes, int numtris);
int R_BuildShadowVolumeTriangles (void);
void R_DrawShadowVolumes( struct mesh_s *mesh, vec3_t lightingOrigin, vec3_t mins, vec3_t maxs, float radius );

#endif

void R_Draw_SimpleShadow (entity_t *ent);
