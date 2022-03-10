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

#define MAX_SKINFILES		1024

typedef struct
{
	char				meshname[MD3_MAX_PATH];
	shader_t			*shader;
} mesh_shader_pair_t;

typedef struct skinfile_s
{
	char				name[MAX_QPATH];

	mesh_shader_pair_t	*pairs;
	int					numpairs;
} skinfile_t;

void R_InitSkinFiles (void);
void R_ShutdownSkinFiles (void);

skinfile_t *R_SkinFile_Load ( char *name );
skinfile_t *R_RegisterSkinFile ( char *name );
shader_t *R_FindShaderForSkinFile( skinfile_t *skinfile, char *meshname );


