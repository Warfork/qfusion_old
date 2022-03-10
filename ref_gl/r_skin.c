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
#include "r_local.h"

skinfile_t r_skinfiles[MAX_SKINFILES];

void R_SkinFile_Init (void)
{
	memset ( r_skinfiles, 0, sizeof (r_skinfiles) );
}

void R_SkinFile_FreeSkinFile ( skinfile_t *skinfile )
{
	Mem_ZoneFree ( skinfile->pairs );

	memset ( skinfile, 0, sizeof (skinfile_t) );
}

shader_t *R_SkinFile_FindShader ( skinfile_t *skinfile, char *meshname )
{
	int i;
	mesh_shader_pair_t *pair;

	if ( !skinfile || !skinfile->numpairs ) {
		return NULL;
	}

	for ( i = 0, pair = skinfile->pairs; i < skinfile->numpairs; i++, pair++ ) {
		if ( !Q_stricmp (pair->meshname, meshname) ) {
			return pair->shader;
		}
	}

	return NULL;
}

int R_SkinFile_Parse ( char *buffer, mesh_shader_pair_t *pairs )
{
	int numpairs;
	char *ptr, *t, *token;
	char meshname[MD3_MAX_PATH], shadername[MAX_QPATH];

	ptr = buffer;
	numpairs = 0;

	while ( ptr ) {
		token = COM_ParseExt ( &ptr, qfalse );
		if ( !token ) {
			break;
		}

		Q_strncpyz ( meshname, token, sizeof(meshname) );
		
		t = strstr ( meshname, "," );
		if ( !t || !(t+1) ) {
			continue;
		}
		if ( *(t+1) == '\0' || *(t+1) == '\n' ) {
			continue;
		}

		*t = 0;
		Q_strncpyz ( shadername, token + strlen (meshname) + 1, sizeof(shadername) );

		if ( pairs ) {
			Q_strncpyz ( pairs[numpairs].meshname, meshname, sizeof(pairs[numpairs].meshname) );
			pairs[numpairs].shader = R_RegisterSkin ( shadername );
		}

		numpairs++;
	}

	return numpairs;
}

skinfile_t *R_SkinFile_Load ( char *name )
{
	int i, f;
	char *buffer;
	skinfile_t *skinfile;

	for ( i = 0, f = -1, skinfile = r_skinfiles; i < MAX_SKINFILES; i++, skinfile++ ) {
		if ( !Q_stricmp (skinfile->name, name) ) {
			return skinfile;
		}
		if ( (f == -1) && !skinfile->registration_sequence ) {
			f = i;
		}
	}

	if ( f == -1 ) {
		Com_Printf ( S_COLOR_YELLOW "R_SkinFile_Load: Skin files limit exceeded\n");
		return NULL;
	}
	if ( FS_LoadFile (name, (void **)&buffer) == -1 ) {
		Com_DPrintf ( S_COLOR_YELLOW "R_SkinFile_Load: Failed to load %s\n", name );
		return NULL;
	}

	skinfile = &r_skinfiles[f];
	Q_strncpyz ( skinfile->name, name, sizeof(skinfile->name) );

	skinfile->numpairs = R_SkinFile_Parse ( buffer, NULL );
	if ( skinfile->numpairs ) {
		skinfile->pairs = Mem_ZoneMalloc ( skinfile->numpairs * sizeof (mesh_shader_pair_t) );
		R_SkinFile_Parse ( buffer, skinfile->pairs );
	} else {
		Com_DPrintf ( S_COLOR_YELLOW "R_SkinFile_Load: no mesh/shader pairs in %s\n", name );
	}

	FS_FreeFile ( (void *)buffer );
	
	return skinfile;
}

skinfile_t *R_RegisterSkinFile ( char *name )
{
	skinfile_t *skinfile;

	skinfile = R_SkinFile_Load ( name );
	if ( skinfile ) {
		skinfile->registration_sequence = registration_sequence;
	}

	return skinfile;
}

void R_SkinFile_UpdateRegistration (void)
{
	int i, j;
	skinfile_t *skinfile;
	mesh_shader_pair_t *pair;

	for ( i = 0, skinfile = r_skinfiles; i < MAX_SKINFILES; i++, skinfile++ ) {
		if ( !skinfile->registration_sequence ) {
			continue;
		}
		if ( skinfile->registration_sequence != registration_sequence ) {
			R_SkinFile_FreeSkinFile ( skinfile );
			continue;
		}

		for ( j = 0, pair = skinfile->pairs; j < skinfile->numpairs; j++, pair++ ) {
			pair->shader->registration_sequence = registration_sequence;
		}
	}
}

void R_SkinFile_Shutdown (void)
{
	int i;
	skinfile_t *skinfile;

	for ( i = 0, skinfile = r_skinfiles; i < MAX_SKINFILES; i++, skinfile++ ) {
		if ( skinfile->numpairs ) {
			R_SkinFile_FreeSkinFile ( skinfile );
		}
	}
}
