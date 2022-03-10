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

static const char *cg_defaultSexedSounds[] = 
{
	"*death1.wav", "*death2.wav", "*death3.wav", "*death4.wav",
	"*fall1.wav", "*fall2.wav",
	"*gurp1.wav", "*gurp2.wav",			// drowning damage
	"*jump1.wav",						// player jump
	"*pain25_1.wav", "*pain25_2.wav",
	"*pain50_1.wav", "*pain50_2.wav",
	"*pain75_1.wav", "*pain75_2.wav",
	"*pain100_1.wav", "*pain100_2.wav",
	"*taunt.wav",
	NULL
};

/*
================
CG_RegisterSexedSound
================
*/
struct sfx_s *CG_RegisterSexedSound( int entnum, const char *name )
{
	cg_clientInfo_t *ci;
	char			*p, *s, model[MAX_QPATH];
	cg_sexedSfx_t	*sexedSfx;
	char			sexedFilename[MAX_QPATH];

	if( entnum < 1 || entnum > MAX_CLIENTS )
		return NULL;

	ci = &cgs.clientInfo[entnum-1];

	for( sexedSfx = ci->sexedSfx; sexedSfx; sexedSfx = sexedSfx->next ) {
		if( !Q_stricmp( sexedSfx->name, name ) )
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

	// if we can't figure it out, they're DEFAULT_PLAYERMODEL
	if( !model[0] )
		strcpy( model, DEFAULT_PLAYERMODEL );

	sexedSfx = CG_Malloc( sizeof(cg_sexedSfx_t) );
	sexedSfx->name = CG_CopyString( name );
	sexedSfx->next = ci->sexedSfx;
	ci->sexedSfx = sexedSfx;

	// see if we already know of the model specific sound
	Q_snprintfz( sexedFilename, sizeof( sexedFilename ), "sound/player/%s/%s", model, name+1 );

	// so see if it exists
	if( trap_FS_FOpenFile( sexedFilename, NULL, FS_READ ) != -1 ) {
		sexedSfx->sfx = trap_S_RegisterSound( sexedFilename );

	} else {

		// set up like at Q2, but sexed
		if( ci->gender == GENDER_FEMALE ) {
			Q_snprintfz( sexedFilename, sizeof(sexedFilename), "players/%s/%s", "female", name+1 );
			sexedSfx->sfx = trap_S_RegisterSound( sexedFilename );
			
		} else {
			Q_snprintfz( sexedFilename, sizeof(sexedFilename), "players/%s/%s", "male", name+1 );
			sexedSfx->sfx = trap_S_RegisterSound( sexedFilename );
		}
	}

	return sexedSfx->sfx;
}

/*
================
CG_SexedSound
================
*/
void CG_SexedSound( int entnum, int entchannel, char *name, float fvol ) {
	trap_S_StartSound( NULL, entnum, entchannel, CG_RegisterSexedSound( entnum, name ), fvol, ATTN_NORM, 0.0f );
}

/*
================
CG_LoadClientInfo
================
*/
void CG_LoadClientInfo( cg_clientInfo_t *ci, char *s, int client )
{
	int				i;
	char			*t;
	const char		*name;
	char			model_name[MAX_QPATH];
	char			skin_name[MAX_QPATH];
	cg_sexedSfx_t	*sexedSfx, *next;

	// free loaded sounds
	for( sexedSfx = ci->sexedSfx; sexedSfx; sexedSfx = next ) {
		next = sexedSfx->next;
		CG_Free( sexedSfx );
	}
	ci->sexedSfx = NULL;

	Q_strncpyz( ci->cinfo, s, sizeof(ci->cinfo) );
	
	// isolate the player's name
	Q_strncpyz( ci->name, s, sizeof(ci->name) );
	t = strstr( s, "\\" );
	if( t ) {
		ci->name[t-s] = 0;
		s = t+1;
	}

	strcpy( model_name, s );
	t = strstr( model_name, "\\" );
	if( t ) {
		*t = 0;
		s = s + strlen( model_name ) + 1;
		ci->hand = atoi( model_name );
	}

	if( cg_noSkins->integer || *s == 0 ) {

		// use defaults
		strcpy( model_name, DEFAULT_PLAYERMODEL );
		strcpy( skin_name, DEFAULT_PLAYERSKIN );
		CG_LoadClientPmodel ( client + 1, model_name, skin_name );

	} else {

		// isolate the model name
		strcpy( model_name, s );
		t = strstr( model_name, "/" );
		if( !t )
			t = strstr( model_name, "\\" );
		if( !t )
			t = model_name;
		*t = 0;
		
		// isolate the skin name
		strcpy( skin_name, s + strlen(model_name) + 1 );
		CG_LoadClientPmodel( client + 1, model_name, skin_name );
	}

	// updating gender must happen before loading sounds
	ci->gender = cg_entPModels[client+1].pmodelinfo->sex;

	// icon
	ci->icon = cg_entPModels[client+1].pSkin->icon;
	if( !ci->icon )
		ci->icon = cgs.basePSkin->icon;

	// load sexed sounds
	if( client != -1 ) {
		// load default sounds
		for( i = 0; ; i++ ) {
			name = cg_defaultSexedSounds[i];
			if( !name )
				break;
			CG_RegisterSexedSound( client+1, name );
		}

		// load sounds server told us
		for( i = 1; i < MAX_SOUNDS; i++ ) {
			name = cgs.configStrings[CS_SOUNDS+i];

			if( !name[0] )
				break;
			if ( name[0] == '*' )
				CG_RegisterSexedSound( client+1, name );
		}
	}
}

/*
==============
CG_FixUpGender
==============
*/
void CG_FixUpGender( void )
{
	cg_clientInfo_t	*ci;

	if( !skin->modified )
		return;

	ci = &cgs.clientInfo[cgs.playerNum];

	// always use pmodelinfo gender ignoring the user
	if ( !gender_auto->integer ) {
		trap_Cvar_Set ( "gender_auto", "1" );

	} else {
		ci->gender = cg_entPModels[cgs.playerNum+1].pmodelinfo->sex;
		if( gender->modified ) {
			if( ci->gender == GENDER_FEMALE )
				trap_Cvar_Set( "gender", "female" );
			else
				trap_Cvar_Set( "gender", "male" );

			gender->modified = qfalse;
		}
	}
}
