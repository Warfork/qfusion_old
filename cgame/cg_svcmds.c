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

/*
==================
CG_SC_Print
==================
*/
void CG_SC_Print (void)
{
	int level = atoi (trap_Cmd_Argv(1));

	if ( level == PRINT_CHAT ) {
		trap_S_StartSound ( NULL, cgs.playerNum+1, 0, trap_S_RegisterSound ("sound/player/talk.wav"), 1.0f, ATTN_NONE, 0.0f );
	}

	CG_Printf ( "%s", trap_Cmd_Argv(2));
}

/*
==================
CG_SC_CenterPrint
==================
*/
void CG_SC_CenterPrint (void) {
	SCR_CenterPrint ( trap_Cmd_Argv(1) );
}

/*
==================
CG_SC_Obituary
==================
*/
void CG_SC_Obituary (void)
{
	char message[128];
	char message2[128];
	cg_clientInfo_t *victim, *attacker;
	int victimNum = atoi ( trap_Cmd_Argv(1) );
	int attackerNum = atoi ( trap_Cmd_Argv(2) );
	int mod = atoi ( trap_Cmd_Argv(3) );

	victim = &cgs.clientInfo[victimNum - 1];

	if ( attackerNum ) {
		attacker = &cgs.clientInfo[attackerNum - 1];
	} else {
		attacker = NULL;
	}

	GS_Obituary ( victim, victim->gender, attacker, mod, message, message2 );

	if ( attackerNum ) {
		if ( victimNum != attackerNum ) {
			CG_Printf ( "%s %s%s %s%s%s\n", victim->name, S_COLOR_WHITE, message, attacker->name, S_COLOR_WHITE, message2 );
		} else {
			CG_Printf ( "%s %s%s\n", victim->name, S_COLOR_WHITE, message );
		}
	} else {
		CG_Printf ( "%s %s%s\n", victim->name, S_COLOR_WHITE, message );
	}
}


/*
================
CG_CS_ConfigString
================
*/
void CG_CS_ConfigString (void)
{
	int		i = atoi ( trap_Cmd_Argv(1) );
	char	*s = trap_Cmd_Argv ( 2 );
	char	olds[MAX_QPATH];

	if ( i < 0 || i >= MAX_CONFIGSTRINGS ) {
		CG_Error ( "configstring > MAX_CONFIGSTRINGS" );
	}

	Q_strncpyz ( olds, cgs.configStrings[i], sizeof(olds) );
	Q_strncpyz ( cgs.configStrings[i], s, sizeof(cgs.configStrings[i]) );

	// do something apropriate 
	if ( i == CS_MAPNAME ) {
		CG_RegisterLevelShot ();
	} else if ( i == CS_STATUSBAR ) { 
		CG_LoadStatusBar ( cgs.configStrings[i] );
	} else if ( i >= CS_MODELS && i < CS_MODELS+MAX_MODELS ) {
		cgs.modelDraw[i-CS_MODELS] = trap_R_RegisterModel ( cgs.configStrings[i] );
	} else if ( i >= CS_SOUNDS && i < CS_SOUNDS+MAX_SOUNDS ) {
		if ( cgs.configStrings[i][0] != '*' ) {
			cgs.soundPrecache[i-CS_SOUNDS] = trap_S_RegisterSound ( cgs.configStrings[i] );
		}
	} else if( i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES ) {
		CG_SetLightStyle( i - CS_LIGHTS );
	} else if ( i >= CS_IMAGES && i < CS_IMAGES+MAX_IMAGES ) {
		cgs.imagePrecache[i-CS_IMAGES] = trap_R_RegisterPic ( cgs.configStrings[i] );
	} else if ( i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS ) {
		if ( strcmp (olds, s) ) {
			CG_LoadClientInfo ( &cgs.clientInfo[i-CS_PLAYERSKINS], cgs.configStrings[i], i-CS_PLAYERSKINS );
		}
	}
}

/*
================
CG_SC_Inventory
================
*/
void CG_SC_Inventory (void)
{
	int		i, rep;
	char	inv[MAX_TOKEN_CHARS], *s;

	Q_strncpyz( inv, trap_Cmd_Argv( 1 ), sizeof( inv ) );

	cg.inventory[0] = 0;	// item 0 is never used
	for( i = 1, s = inv; (i < MAX_ITEMS) && s && *s; i++ ) {
		cg.inventory[i] = atoi( COM_Parse(&s) );
		if( cg.inventory[i] )
			continue;

		for( rep = atoi( COM_Parse(&s) ); (rep > 0) && (i < MAX_ITEMS); rep-- )
			cg.inventory[i++] = 0;
		i--;
	}
}

//=======================================================

typedef struct
{
	char	*name;
	void	(*func) (void);
} svcmd_t;

svcmd_t cg_svcmds[] =
{
	{ "pr", CG_SC_Print },
	{ "cp", CG_SC_CenterPrint },
	{ "obry", CG_SC_Obituary },
	{ "cs", CG_CS_ConfigString },
	{ "inv", CG_SC_Inventory },
	{ NULL }
};

/*
==================
CG_ServerCommand
==================
*/
void CG_ServerCommand (void)
{
	char *s;
	svcmd_t *cmd;

	s = trap_Cmd_Argv ( 0 );
	for (cmd = cg_svcmds; cmd->name; cmd++)
		if (!strcmp (s, cmd->name) ) {
			cmd->func ();
			return;
		}
}
