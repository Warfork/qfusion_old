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

#include "ui_local.h"

void M_Menu_DMOptions_f ( void );
void M_SetMultiplayerStatusBar ( const char *string );

/*
=============================================================================

START SERVER MENU

=============================================================================
*/
static menuframework_s s_startserver_menu;
static char **mapnames;
static int	  nummaps;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_capturelimit_field;
static menulist_s	s_cheats_list;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

static void			*s_levelshot;

void DMOptionsFunc( void *self )
{
	if (s_rules_box.curvalue == 1)
		return;
	M_Menu_DMOptions_f();
}

void MapChangeFunc ( void *self )
{
	char path[MAX_QPATH];

	Com_sprintf ( path, sizeof(path), "levelshots/%s.jpg", mapnames[s_startmap_list.curvalue] );
	
	if ( !trap_FS_FileExists ( path ) ) 
		Com_sprintf ( path, sizeof(path), "levelshots/%s.tga", mapnames[s_startmap_list.curvalue] );
	
	if ( !trap_FS_FileExists ( path ) ) 
		Com_sprintf ( path, sizeof(path), "menu/art/unknownmap", mapnames[s_startmap_list.curvalue] );
	
	s_levelshot = trap_R_RegisterPic ( path );
}

void RulesChangeFunc ( void *self )
{
	if ( s_rules_box.curvalue == 0 )			// dm
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;

		strcpy( s_capturelimit_field.buffer, "0" );
		s_capturelimit_field.generic.statusbar = "N/A for deathmatch";
	}
	else if ( s_rules_box.curvalue == 1 )		// coop
	{
		s_maxclients_field.generic.statusbar = "4 maximum for cooperative";
		if (atoi(s_maxclients_field.buffer) > 4)
			strcpy( s_maxclients_field.buffer, "4" );
		s_startserver_dmoptions_action.generic.statusbar = "N/A for cooperative";

		strcpy( s_capturelimit_field.buffer, "0" );
		s_capturelimit_field.generic.statusbar = "N/A for cooperative";
	}
	else if ( s_rules_box.curvalue == 2 )		// ctf
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
		s_capturelimit_field.generic.statusbar = NULL;
	}
}

void StartServerActionFunc( void *self )
{
	char	startmap[1024];
	int		timelimit;
	int		fraglimit;
	int		capturelimit;
	int		cheats;
	int		maxclients;
	char	*spot;

	strcpy( startmap, mapnames[s_startmap_list.curvalue] );

	maxclients  = atoi( s_maxclients_field.buffer );
	timelimit	= atoi( s_timelimit_field.buffer );
	fraglimit	= atoi( s_fraglimit_field.buffer );
	capturelimit	= atoi( s_capturelimit_field.buffer );
	cheats	= bound( 0, s_cheats_list.curvalue, 1 );

	trap_Cvar_SetValue ("sv_cheats", M_ClampCvar( 0, cheats, cheats ) );
	trap_Cvar_SetValue ("sv_maxclients", M_ClampCvar( 0, maxclients, maxclients ) );
	trap_Cvar_SetValue ("timelimit", M_ClampCvar( 0, timelimit, timelimit ) );
	trap_Cvar_SetValue ("fraglimit", M_ClampCvar( 0, fraglimit, fraglimit ) );
	trap_Cvar_SetValue ("capturelimit", M_ClampCvar( 0, capturelimit, capturelimit ) );
	trap_Cvar_Set ("sv_hostname", s_hostname_field.buffer );
	trap_Cvar_SetValue ("deathmatch", !s_rules_box.curvalue || s_rules_box.curvalue == 2 );
	trap_Cvar_SetValue ("coop", s_rules_box.curvalue == 1 );
	trap_Cvar_SetValue ("ctf", s_rules_box.curvalue == 2 );

	spot = NULL;

	if (spot)
	{
		if (uis.serverState)
			trap_Cmd_ExecuteText (EXEC_APPEND, "disconnect\n");
		trap_Cmd_ExecuteText (EXEC_APPEND, va("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		trap_Cmd_ExecuteText (EXEC_APPEND, va("map %s\n", startmap));
	}

	M_ForceMenuOff ();
}

qboolean StartServer_MenuInit( void )
{
	static char *dm_coop_ctf_names[] =
	{
		"deathmatch",
		"cooperative",
		"capture the flag",
		0
	};

	static char *cheats_items[] =
	{
		"off", "on", 0
	};

	char *s;
	char buffer[2048];
	int nummaps;
	int length;
	int i;
	int y = 40;
	int y_offset = UI_StringHeightOffset ( 0 );
	char path[MAX_QPATH];

	/*
	** load the list of maps
	*/
	if ( (nummaps = trap_FS_ListFiles( "maps", ".bsp", buffer, sizeof(buffer) )) == 0 ) {
		M_SetMultiplayerStatusBar( "No maps found" );
		return qfalse;
	}

	mapnames = UI_Malloc( sizeof( char * ) * ( nummaps + 1 ) );

	s = buffer;
	length = 0;

	for ( i = 0; i < nummaps; i++, s += length+1 )
	{
		char  shortname[MAX_TOKEN_CHARS];
		char  scratch[200];
		int	  j;

		length = strlen( s );
		for (j=0 ; j<length-4 ; j++)			// skip .bsp
			shortname[j] = toupper( s[j] );
		shortname[j] = 0;

		Com_sprintf( scratch, sizeof( scratch ), shortname );

		mapnames[i] = UI_CopyString( scratch );

		Com_sprintf ( path, sizeof(path), "levelshots/%s", mapnames[i] );
		trap_R_RegisterPic ( path );
	}

	mapnames[nummaps] = 0;

	/*
	** initialize the menu stuff
	*/
	s_startserver_menu.x = uis.vidWidth / 2;
	s_startserver_menu.y = y;
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x	= 0;
	s_startmap_list.generic.y	= y;
	s_startmap_list.curvalue	%= nummaps;
	s_startmap_list.generic.callback = MapChangeFunc;
	s_startmap_list.generic.name	= "initial map";
	s_startmap_list.itemnames = mapnames;
	MapChangeFunc ( (void *)&s_startmap_list );

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x	= 0;
	s_rules_box.generic.y	= y += y_offset;
	s_rules_box.generic.name	= "rules";
	s_rules_box.itemnames = dm_coop_ctf_names;
	
	if (trap_Cvar_VariableValue ("ctf"))
		s_rules_box.curvalue = 2;
	else if (trap_Cvar_VariableValue ("coop"))
		s_rules_box.curvalue = 1;
	else
		s_rules_box.curvalue = 0;

	s_rules_box.generic.callback = RulesChangeFunc;

	s_cheats_list.generic.type			= MTYPE_SPINCONTROL;
	s_cheats_list.generic.x				= 0;
	s_cheats_list.generic.y				= y += y_offset;
	s_cheats_list.generic.name			= "cheats";
	s_cheats_list.generic.callback		= NULL;
	s_cheats_list.itemnames				= cheats_items;
	s_cheats_list.curvalue				= (int)trap_Cvar_VariableValue( "sv_cheats" );
	clamp ( s_cheats_list.curvalue, 0, 1 );

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x	= 0;
	s_timelimit_field.generic.y	= y += y_offset*2;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.length = 3;
	s_timelimit_field.visible_length = 3;
	strcpy( s_timelimit_field.buffer, trap_Cvar_VariableString("timelimit") );

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x	= 0;
	s_fraglimit_field.generic.y	= y += y_offset;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.length = 3;
	s_fraglimit_field.visible_length = 3;
	strcpy( s_fraglimit_field.buffer, trap_Cvar_VariableString("fraglimit") );

	s_capturelimit_field.generic.type = MTYPE_FIELD;
	s_capturelimit_field.generic.name = "capture limit";
	s_capturelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_capturelimit_field.generic.x	= 0;
	s_capturelimit_field.generic.y	= y += y_offset;
	s_capturelimit_field.generic.statusbar = "0 = no limit";
	s_capturelimit_field.length = 3;
	s_capturelimit_field.visible_length = 3;
	strcpy( s_capturelimit_field.buffer, trap_Cvar_VariableString("capturelimit") );

	/*
	** maxclients determines the maximum number of players that can join
	** the game.  If maxclients is only "1" then we should default the menu
	** option to 8 players, otherwise use whatever its current value is. 
	** Clamping will be done when the server is actually started.
	*/
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x	= 0;
	s_maxclients_field.generic.y	= y += y_offset;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;
	if ( trap_Cvar_VariableValue( "sv_maxclients" ) == 1 )
		strcpy( s_maxclients_field.buffer, "8" );
	else 
		strcpy( s_maxclients_field.buffer, trap_Cvar_VariableString("sv_maxclients") );

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x	= 0;
	s_hostname_field.generic.y	= y += y_offset;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	strcpy( s_hostname_field.buffer, trap_Cvar_VariableString("sv_hostname") );

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name	= "deathmatch flags";
	s_startserver_dmoptions_action.generic.flags= QMF_CENTERED;
	s_startserver_dmoptions_action.generic.x	= 0;
	s_startserver_dmoptions_action.generic.y	= y += y_offset;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name	= "begin";
	s_startserver_start_action.generic.flags= QMF_CENTERED;
	s_startserver_start_action.generic.x	= 0;
	s_startserver_start_action.generic.y	= y += y_offset*2;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	Menu_AddItem( &s_startserver_menu, &s_startmap_list );
	Menu_AddItem( &s_startserver_menu, &s_rules_box );
	Menu_AddItem( &s_startserver_menu, &s_cheats_list );
	Menu_AddItem( &s_startserver_menu, &s_timelimit_field );
	Menu_AddItem( &s_startserver_menu, &s_fraglimit_field );
	Menu_AddItem( &s_startserver_menu, &s_capturelimit_field );
	Menu_AddItem( &s_startserver_menu, &s_maxclients_field );
	Menu_AddItem( &s_startserver_menu, &s_hostname_field );
	Menu_AddItem( &s_startserver_menu, &s_startserver_dmoptions_action );
	Menu_AddItem( &s_startserver_menu, &s_startserver_start_action );

	Menu_Center( &s_startserver_menu );

	Menu_Init ( &s_startserver_menu );

	// call this now to set proper inital state
	RulesChangeFunc ( NULL );

	return qtrue;
}

void StartServer_MenuDraw(void)
{
	trap_Draw_StretchPic ( s_startserver_menu.x - 80, s_startserver_menu.y - 90, 160, 120, 0, 0, 1, 1, colorWhite, s_levelshot );

	Menu_Draw( &s_startserver_menu );
}

const char *StartServer_MenuKey( int key )
{
	menucommon_s *item;

	item = Menu_ItemAtCursor ( &s_startserver_menu );

	if ( key == K_ESCAPE || ( (key == K_MOUSE2) && (item->type != MTYPE_SPINCONTROL) &&
		(item->type != MTYPE_SLIDER)) )
	{
		if ( mapnames )
		{
			int i;

			for ( i = 0; i < nummaps; i++ )
				UI_Free( mapnames[i] );
			UI_Free( mapnames );
		}
		mapnames = 0;
		nummaps = 0;
	} 

	return Default_MenuKey( &s_startserver_menu, key );
}

void M_Menu_StartServer_f (void)
{
	if ( !StartServer_MenuInit() )
		return;

	M_SetMultiplayerStatusBar( NULL );
	M_PushMenu( &s_startserver_menu, StartServer_MenuDraw, StartServer_MenuKey );
}
