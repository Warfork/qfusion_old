/*
Copyright (C) 2002-2003 Victor Luchits

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

cg_static_t		cgs;
cg_state_t		cg;

centity_t		cg_entities[MAX_EDICTS];
entity_state_t	cg_parseEntities[MAX_PARSE_ENTITIES];

cvar_t			*cg_paused;
cvar_t			*cg_noSkins;
cvar_t			*cg_vwep;
cvar_t			*cg_predict;
cvar_t			*cg_showMiss;

cvar_t			*skin;
cvar_t			*hand;
cvar_t			*gender;
cvar_t			*gender_auto;

cvar_t			*cg_outlineWorld;
cvar_t			*cg_outlineModels;

cvar_t			*cg_testEntities;
cvar_t			*cg_testLights;
cvar_t			*cg_testBlend;

cvar_t			*cg_addDecals;

cvar_t			*cg_footSteps;

cvar_t			*cg_gun;
cvar_t			*cg_timeDemo;

cvar_t			*cg_thirdPerson;
cvar_t			*cg_thirdPersonAngle;
cvar_t			*cg_thirdPersonRange;
	
cvar_t			*cg_weaponFlashes;
cvar_t			*cg_ejectBrass;
cvar_t			*cg_gunx;
cvar_t			*cg_guny;
cvar_t			*cg_gunz;
cvar_t			*cg_debugPlayerModels;
cvar_t			*cg_debugWeaponModels;
cvar_t			*cg_bobSpeed;
cvar_t			*cg_bobPitch;
cvar_t			*cg_bobYaw;
cvar_t			*cg_bobRoll;
cvar_t			*cg_showLegs;

/*
============
CG_Error
============
*/
int CG_API( void ) {
	return CGAME_API_VERSION;
}

/*
============
CG_Error
============
*/
void CG_Error( char *fmt, ... )
{
	char		msg[1024];
	va_list		argptr;

	va_start ( argptr, fmt );
	if( vsprintf (msg, fmt, argptr) > sizeof(msg) )
		trap_Error ( "CG_Error: Buffer overflow" );
	va_end ( argptr );

	trap_Error ( msg );
}

/*
============
CG_Printf
============
*/
void CG_Printf( char *fmt, ... )
{
	char		msg[1024];
	va_list		argptr;

	va_start ( argptr, fmt );
	if( vsprintf (msg, fmt, argptr) > sizeof(msg) )
		trap_Error ( "CG_Print: Buffer overflow" );
	va_end ( argptr );

	trap_Print ( msg );
}

/*
=================
CG_CopyString
=================
*/
char *_CG_CopyString( const char *in, const char *filename, int fileline )
{
	char	*out;
	
	out = trap_MemAlloc( strlen( in ) + 1, filename, fileline );
	strcpy( out, in );

	return out;
}

/*
=================
CG_RegisterModels
=================
*/
void CG_RegisterModels( void )
{
	int i;
	char *name;

	name = cgs.configStrings[CS_MODELS+1];
	CG_LoadingString( name );

	trap_R_RegisterWorldModel( name );

	CG_LoadingString( "models" );

	CG_RegisterBasePModel ();

	cgs.numWeaponModels = 1;
	strcpy ( cgs.weaponModels[0], "generic/generic.md3" );

	for ( i = 1; i < MAX_MODELS; i++ )
	{
		name = cgs.configStrings[CS_MODELS+i];
		if( !name[0] )
			break;
		if( name[0] == '#' ) {	// special player weapon model
			if ( cgs.numWeaponModels < WEAP_TOTAL ) {
				Q_strncpyz( cgs.weaponModels[cgs.numWeaponModels], name+1, sizeof(cgs.weaponModels[cgs.numWeaponModels]) );
				cgs.numWeaponModels++;
			}

		} else if( name[0] == '$' ) {	// indexed pmodel
			cgs.pModelsIndex[i] = CG_RegisterPModel( name+1 );

		} else {
			CG_LoadingFilename( name );
			cgs.modelDraw[i] = CG_RegisterModel( name );
		}
	}

	for( i = 1; i < trap_CM_NumInlineModels (); i++ )
		cgs.inlineModelDraw[i] = CG_RegisterModel( va("*%i", i) );

	CG_RegisterMediaModels ();
}

/*
=================
CG_RegisterSounds
=================
*/
void CG_RegisterSounds( void )
{
	int i;
	char *name;

	CG_LoadingString( "sounds" );

	for( i = 1; i < MAX_SOUNDS; i++ ) {
		name = cgs.configStrings[CS_SOUNDS+i];

		if( !name[0] )
			break;
		if( name[0] != '*' ) {
			CG_LoadingFilename( name );
			cgs.soundPrecache[i] = trap_S_RegisterSound( name );
		}
	}

	CG_RegisterMediaSounds ();
}

/*
=================
CG_RegisterShaders
=================
*/
void CG_RegisterShaders (void)
{
	int i;
	char *name;

	CG_LoadingString( "images" );

	for( i = 1; i < MAX_IMAGES; i++ ) {
		name = cgs.configStrings[CS_IMAGES+i];
		if( !name[0] )
			break;

		if( name[0] == '$' ) {	// indexed pSkin
			cgs.pSkinsIndex[i] = CG_RegisterPSkin(name+1);
			continue;
		}

		CG_LoadingFilename( name );
		cgs.imagePrecache[i] = trap_R_RegisterPic( name );
	}

	CG_RegisterMediaShaders ();
}

/*
=================
CG_RegisterClients
=================
*/
void CG_RegisterClients (void)
{
	int i;
	char *name;

	CG_LoadingFilename( "" );

	for( i = 0; i < MAX_CLIENTS; i++ ) {
		name = cgs.configStrings[CS_PLAYERSKINS+i];
		if( !name[0] )
			break;

		CG_LoadingString( va ("client %i", i) );
		CG_LoadClientInfo( &cgs.clientInfo[i], name, i );
	}
}

/*
=================
CG_RegisterLightStyles
=================
*/
void CG_RegisterLightStyles (void)
{
	int i;
	char *name;

	CG_LoadingFilename( "" );

	for( i = 0; i < MAX_LIGHTSTYLES; i++ ) {
		name = cgs.configStrings[CS_LIGHTS+i];
		if( !name[0] )
			continue;
		CG_SetLightStyle( i );
	}
}

/*
=================
CG_RegisterVariables
=================
*/
void CG_RegisterVariables (void)
{
	char default_skin[MAX_QPATH];
	Q_snprintfz (default_skin, sizeof(default_skin), "%s/%s", DEFAULT_PLAYERMODEL, DEFAULT_PLAYERSKIN);

	cg_paused = trap_Cvar_Get ( "paused", "0", 0 );
	cg_noSkins = trap_Cvar_Get ( "cg_noSkins", "0", 0 );
	cg_vwep = trap_Cvar_Get ( "cg_vwep", "1", 0 );
	cg_predict = trap_Cvar_Get ( "cg_predict", "1", 0 );
	cg_showMiss = trap_Cvar_Get ( "cg_showMiss", "0", 0 );

	cg_outlineWorld = trap_Cvar_Get ( "cg_outlineWorld", "0", CVAR_ARCHIVE );
	cg_outlineModels = trap_Cvar_Get ( "cg_outlineModels", "0", CVAR_ARCHIVE );

	cg_testBlend = trap_Cvar_Get ( "cg_testBlend", "0", CVAR_CHEAT );
	cg_testEntities = trap_Cvar_Get ( "cg_testEntities", "0", CVAR_CHEAT );
	cg_testLights = trap_Cvar_Get ( "cg_testLights", "0", CVAR_CHEAT );

	skin = trap_Cvar_Get ( "skin", default_skin, CVAR_USERINFO | CVAR_ARCHIVE );
	hand = trap_Cvar_Get ( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	gender = trap_Cvar_Get ( "gender", "male", CVAR_USERINFO | CVAR_ARCHIVE );
	gender_auto = trap_Cvar_Get ( "gender_auto", "1", CVAR_ARCHIVE );
	gender->modified = qfalse; // clear this so we know when user sets it manually

	cg_addDecals = trap_Cvar_Get ( "cg_decals", "1", 0 );

	cg_footSteps = trap_Cvar_Get ( "cg_footSteps", "1", 0 );

	cg_gun = trap_Cvar_Get ( "cg_gun", "1", 0 );
	cg_timeDemo = trap_Cvar_Get ( "timedemo", "0", 0 );

	cg_thirdPerson = trap_Cvar_Get ( "cg_thirdPerson", "0", CVAR_CHEAT );
	cg_thirdPersonAngle = trap_Cvar_Get ( "cg_thirdPersonAngle", "0", 0 );
	cg_thirdPersonRange = trap_Cvar_Get ( "cg_thirdPersonRange", "70", 0 );

	cg_weaponFlashes = trap_Cvar_Get ( "cg_weaponFlashes", "2", CVAR_ARCHIVE );
	cg_ejectBrass = trap_Cvar_Get ( "cg_ejectBrass", "2", CVAR_ARCHIVE );
	cg_gunx = trap_Cvar_Get ("cg_gunx", "0", CVAR_ARCHIVE);
	cg_guny = trap_Cvar_Get ("cg_guny", "0", CVAR_ARCHIVE);
	cg_gunz = trap_Cvar_Get ("cg_gunz", "0", CVAR_ARCHIVE);
	cg_debugPlayerModels = trap_Cvar_Get ("cg_debugPlayerModels", "0", CVAR_ARCHIVE);
	cg_debugWeaponModels = trap_Cvar_Get ("cg_debugWeaponModels", "0", CVAR_ARCHIVE);
	cg_bobSpeed = trap_Cvar_Get ("cg_bobSpeed", "2.5", CVAR_ARCHIVE);
	cg_bobPitch = trap_Cvar_Get ("cg_bobPitch", "6", CVAR_ARCHIVE);
	cg_bobYaw = trap_Cvar_Get ("cg_bobYaw", "3", CVAR_ARCHIVE);
	cg_bobRoll = trap_Cvar_Get ("cg_bobRoll", "6", CVAR_ARCHIVE);
	cg_showLegs = trap_Cvar_Get ("cg_showLegs", "1", CVAR_ARCHIVE);
}

/*
=================
CG_RegisterGameType
=================
*/
void CG_RegisterGameType (void)
{
	if( !Q_stricmp( cgs.configStrings[CS_GAMETYPE], "ctf" ) )
		cgs.gametype = GAMETYPE_CTF;
	else if( !Q_stricmp( cgs.configStrings[CS_GAMETYPE], "deathmatch" ) )
		cgs.gametype = GAMETYPE_DM;
	else if( !Q_stricmp( cgs.configStrings[CS_GAMETYPE], "cooperative" ) )
		cgs.gametype = GAMETYPE_COOP;
	else
		cgs.gametype = GAMETYPE_SP;
}

/*
=================
CG_RegisterConfigStrings
=================
*/
void CG_RegisterConfigStrings (void)
{
	int i;

	for( i = 0; i < MAX_CONFIGSTRINGS; i++ )
		trap_GetConfigString( i, cgs.configStrings[i], MAX_QPATH );
}

/*
=================
CG_StartBackgroundTrack
=================
*/
void CG_StartBackgroundTrack (void)
{
	char *string;
	char intro[MAX_QPATH], loop[MAX_QPATH];

	string = cgs.configStrings[CS_AUDIOTRACK];
	Q_strncpyz( intro, COM_Parse( &string ), sizeof( intro ) );
	Q_strncpyz( loop, COM_Parse( &string ), sizeof( loop ) );

	trap_S_StartBackgroundTrack( intro, loop );
}

/*
============
CG_Init
============
*/
void CG_Init( int playerNum, qboolean attractLoop, unsigned int serverFrameTime, int vidWidth, int vidHeight )
{
	memset( &cg, 0, sizeof(cg_state_t) );
	memset( &cgs, 0, sizeof(cg_static_t) );

	memset( cg_entities, 0, sizeof(cg_entities) );
	memset( cg_parseEntities, 0, sizeof(cg_parseEntities) );

	CG_InitTemporaryBoneposesCache();
	CG_PModelsInit();

	// save local player number
	cgs.playerNum = playerNum;
	cgs.attractLoop = attractLoop;		// true if demo playback
	cgs.serverFrameTime = serverFrameTime;

	// save current width and height
	cgs.vidWidth = vidWidth;
	cgs.vidHeight = vidHeight;

	SCR_Init ();

	// get configstrings
	CG_RegisterConfigStrings ();

	CG_RegisterVariables ();

	CG_RegisterGameType ();

	// register fonts here so loading screen works
	cgs.shaderWhite = trap_R_RegisterPic ( "white" );
	cgs.shaderCharset = trap_R_RegisterPic ( "gfx/2d/bigchars" );
	cgs.shaderPropfont = trap_R_RegisterPic ( "menu/art/font1_prop" );

	CG_RegisterLevelShot ();

	CG_RegisterModels ();
	CG_RegisterSounds ();
	CG_RegisterShaders ();
	CG_RegisterClients ();

	CG_LoadStatusBar ( cgs.configStrings[CS_STATUSBAR] );

	CG_LoadingString ( "-" );	// awaiting snapshot...
	CG_LoadingFilename ( "" );

	CG_ClearDecals ();
	CG_ClearEffects ();
	CG_ClearLocalEntities ();

	CG_RegisterLightStyles ();

	// start background track
	CG_StartBackgroundTrack ();
}

/*
============
CG_Shutdown
============
*/
void CG_Shutdown (void)
{
	SCR_Shutdown ();
}

//======================================================================

#ifndef CGAME_HARD_LINKED
// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);
	text[sizeof(text)-1] = 0;

	trap_Error (text);
}

void Com_Printf (char *msg, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsnprintf (text, sizeof(text), msg, argptr);
	va_end (argptr);
	text[sizeof(text)-1] = 0;

	trap_Print (text);
}

#endif
