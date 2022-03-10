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

#include "cg_local.h"

/*
================
CG_RegisterSexedSound
================
*/
struct sfx_s *CG_RegisterSexedSound ( int entnum, char *name )
{
	cg_clientInfo_t *ci;
	char			*p, *s, model[MAX_QPATH];
	cg_sexedSfx_t	*sexedSfx;
	char			sexedFilename[MAX_QPATH];

	if( entnum < 1 || entnum > MAX_CLIENTS )
		CG_Error ( "CG_RegisterSexedSound: bad entnum" );

	ci = &cgs.clientInfo[entnum-1];

	for( sexedSfx = ci->sexedSfx; sexedSfx; sexedSfx = sexedSfx->next ) {
		if( !Q_stricmp (sexedSfx->name, name) )
			return sexedSfx->sfx;
	}

	// determine what model the client is using
	model[0] = 0;
	s = cgs.configStrings[CS_PLAYERSKINS + entnum - 1];
	if( s[0] ) {
		p = strchr ( s, '\\' );
		if( p ) {
			s = p + 1;
			p = strchr ( s, '\\' );
			if( p ) {
				strcpy ( model, p + 1 );
				p = strchr ( model, '/' );
				if ( p )
					*p = 0;
			}
		}
	}

	// if we can't figure it out, they're male
	if( !model[0] )
		strcpy ( model, "male" );

	sexedSfx = CG_Malloc( sizeof(cg_sexedSfx_t) );
	sexedSfx->name = CG_CopyString( name );
	sexedSfx->next = ci->sexedSfx;
	ci->sexedSfx = sexedSfx;

	// see if we already know of the model specific sound
	Q_snprintfz( sexedFilename, sizeof(sexedFilename), "players/%s/%s", model, name+1 );

	// so see if it exists
	if( trap_FS_FOpenFile (sexedFilename, NULL, FS_READ) != -1 ) {	// yes, register it
		sexedSfx->sfx = trap_S_RegisterSound( sexedFilename );
	} else {		// no, revert to the male sound
		Q_snprintfz( sexedFilename, sizeof(sexedFilename), "sound/player/%s/%s", "male", name+1 );
		sexedSfx->sfx = trap_S_RegisterSound( sexedFilename );
	}

	return sexedSfx->sfx;
}

/*
================
CG_SexedSound
================
*/
void CG_SexedSound ( int entnum, int entchannel, char *name, float fvol )
{
	if( name[0] == '*' )
		trap_S_StartSound ( NULL, entnum, entchannel, CG_RegisterSexedSound (entnum, name), fvol, ATTN_NORM, 0.0f );
}

/*
================
CG_LoadClientInfo
================
*/
void CG_LoadClientInfo( cg_clientInfo_t *ci, char *s, int client )
{
	int			i;
	char		*t, *name;
	char		model_name[MAX_QPATH];
	char		skin_name[MAX_QPATH];
	char		model_filename[MAX_QPATH];
	char		skin_filename[MAX_QPATH];
	char		weapon_filename[MAX_QPATH];
	cg_sexedSfx_t *sexedSfx, *next;

	// free loaded sounds
	for( sexedSfx = ci->sexedSfx; sexedSfx; sexedSfx = next ) {
		next = sexedSfx->next;
		CG_Free ( sexedSfx );
	}
	ci->sexedSfx = NULL;

	// load sounds server told us
	if( client != -1 ) {
		for( i = 1; i < MAX_SOUNDS; i++ ) {
			name = cgs.configStrings[CS_SOUNDS+i];

			if( !name[0] )
				break;
			if ( name[0] == '*' )
				CG_RegisterSexedSound( client+1, name );
		}
	}

	Q_strncpyz( ci->cinfo, s, sizeof(ci->cinfo) );

	// isolate the player's name
	Q_strncpyz( ci->name, s, sizeof(ci->name) );
	t = strstr( s, "\\" );
	if ( t ) {
		ci->name[t-s] = 0;
		s = t+1;
	}

	strcpy ( model_name, s );
	t = strstr( model_name, "\\" );
	if ( t ) {
		*t = 0;
		s = s + strlen ( model_name ) + 1;
		ci->hand = atoi ( model_name );
	}

	if( cg_noSkins->integer || *s == 0 ) {
		Q_snprintfz( model_filename, sizeof(model_filename), "players/male/tris.md2" );
		Q_snprintfz( weapon_filename, sizeof(weapon_filename), "players/male/weapon.md2" );
		Q_snprintfz( ci->iconname, sizeof(ci->iconname), "players/male/grunt_i.pcx" );
		ci->model = trap_R_RegisterModel( model_filename );
		memset( ci->weaponmodel, 0, sizeof(ci->weaponmodel) );
		ci->weaponmodel[0] = trap_R_RegisterModel( weapon_filename );

		Q_snprintfz( skin_filename, sizeof(skin_filename), "players/male/grunt_default.skin" );
		ci->skin = trap_R_RegisterSkinFile( skin_filename );
		if ( !ci->skin ) {
			Q_snprintfz( skin_filename, sizeof(skin_filename), "players/male/grunt" );
			ci->shader = trap_R_RegisterSkin( skin_filename );
		}

		ci->icon = trap_R_RegisterPic( ci->iconname );
		ci->gender = GENDER_MALE;
	} else {
		// isolate the model name
		strcpy( model_name, s );
		t = strstr( model_name, "/" );
		if( !t )
			t = strstr ( model_name, "\\" );
		if( !t )
			t = model_name;
		*t = 0;

		// isolate the skin name
		strcpy( skin_name, s + strlen(model_name) + 1 );

		Q_snprintfz( model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name );
		ci->model = trap_R_RegisterModel( model_filename );
		if( !ci->model ) {	// model file
			strcpy( model_name, "male" );
			Q_snprintfz( model_filename, sizeof(model_filename), "players/male/tris.md2" );
			ci->model = trap_R_RegisterModel( model_filename );

			// skin file
			strcpy( skin_name, "grunt" );
			Q_snprintfz( skin_filename, sizeof(skin_filename), "players/%s/%s_default.skin", model_name, skin_name );
			ci->skin = trap_R_RegisterSkinFile( skin_filename );
			if ( !ci->skin ) {
				Q_snprintfz( skin_filename, sizeof(skin_filename), "players/%s/%s", model_name, skin_name );
				ci->shader = trap_R_RegisterSkin( skin_filename );
			}
		} else {			// skin file
			Q_snprintfz( skin_filename, sizeof(skin_filename), "players/%s/%s_default.skin", model_name, skin_name );
			ci->skin = trap_R_RegisterSkinFile( skin_filename );
			if ( !ci->skin ) {
				Q_snprintfz( skin_filename, sizeof(skin_filename), "players/%s/%s", model_name, skin_name );
				ci->shader = trap_R_RegisterSkin( skin_filename );
			}

			// if we don't have the skin and the model wasn't male,
			// see if the male has it (this is for CTF's skins)
 			if( (!ci->skin && !ci->shader) && Q_stricmp(model_name, "male") ) {
				// change model to male
				strcpy ( model_name, "male" );
				Q_snprintfz( model_filename, sizeof(model_filename), "players/male/tris.md2" );
				ci->model = trap_R_RegisterModel( model_filename );

				// see if the skin exists for the male model
				Q_snprintfz( skin_filename, sizeof(skin_filename), "players/%s/%s_default.skin", model_name, skin_name );
				ci->skin = trap_R_RegisterSkinFile( skin_filename );
				if( !ci->skin ) {
					Q_snprintfz ( skin_filename, sizeof(skin_filename), "players/%s/%s", model_name, skin_name );
					ci->shader = trap_R_RegisterSkin( skin_filename );
				}
			}

			// if we still don't have a skin, it means that the male model didn't have
			// it, so default to grunt
			if( !ci->skin && !ci->shader ) {
				// see if the skin exists for the male model
				Q_snprintfz( skin_filename, sizeof(skin_filename), "players/%s/grunt_default.skin", model_name, skin_name );
				ci->skin = trap_R_RegisterSkinFile( skin_filename );
				if ( !ci->skin ) {
					Q_snprintfz( skin_filename, sizeof(skin_filename), "players/%s/grunt", model_name, skin_name );
					ci->shader = trap_R_RegisterSkin( skin_filename );
				}
			}
		}

		// gender
		if( model_name[0] == 'm' || model_name[0] == 'M' )
			ci->gender = GENDER_MALE;
		else if ( model_name[0] == 'f' || model_name[0] == 'F' )
			ci->gender = GENDER_FEMALE;
		else
			ci->gender = GENDER_NEUTRAL;

		// weapon file
		for( i = 0; i < cgs.numWeaponModels; i++ ) {
			Q_snprintfz ( weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cgs.weaponModels[i] );
			ci->weaponmodel[i] = trap_R_RegisterModel( weapon_filename );

			if( !ci->weaponmodel[i] && strcmp(model_name, "cyborg") == 0 ) {
				// try male
				Q_snprintfz( weapon_filename, sizeof(weapon_filename), "players/male/%s", cgs.weaponModels[i] );
				ci->weaponmodel[i] = trap_R_RegisterModel( weapon_filename );
			}
			if( !cg_vwep->integer )
				break; // only one when vwep is off
		}

		// icon file
		Q_snprintfz( ci->iconname, sizeof(ci->iconname), "players/%s/%s_i", model_name, skin_name );
		ci->icon = trap_R_RegisterPic( ci->iconname );
	}

	// must have loaded all data types to be valud
	if( (!ci->skin && !ci->shader) || !ci->icon || !ci->model || !ci->weaponmodel[0] ) {
		ci->skin = NULL;
		ci->shader = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
	}
}

/*
==============
CG_FixUpGender
==============
*/
void CG_FixUpGender (void)
{
	char *p;
	char sk[MAX_QPATH];

	if( !skin->modified )
		return;

	if( gender_auto->integer ) {
		if( gender->modified ) {
			// was set directly, don't override the user
			gender->modified = qfalse;
			return;
		}

		Q_strncpyz ( sk, skin->string, sizeof(sk) );
		if ((p = strchr(sk, '/')) != NULL)
			*p = 0;
		if( Q_stricmp(sk, "male") == 0 || Q_stricmp(sk, "cyborg") == 0 )
			trap_Cvar_Set( "gender", "male" );
		else if ( Q_stricmp(sk, "female") == 0 || Q_stricmp(sk, "crackhor") == 0 )
			trap_Cvar_Set( "gender", "female" );
		else
			trap_Cvar_Set( "gender", "none" );
		gender->modified = qfalse;
	}
}
