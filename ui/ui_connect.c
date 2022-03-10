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

/*
=============================================================================

JOIN SERVER MENU

=============================================================================
*/

#define MAX_MENU_SERVERS 12

static menuframework_s	s_joinserver_menu;

static menuaction_s		s_joinserver_search_local_action;
static menuaction_s		s_joinserver_search_global_action;
static menulist_s		s_joinserver_ignore_full_box;
static menulist_s		s_joinserver_ignore_empty_box;
static menuaction_s		s_joinserver_server_actions[MAX_MENU_SERVERS];

int		m_num_servers;
#define	NO_SERVER_STRING	"<no server>"

// user readable information
static char local_server_names[MAX_MENU_SERVERS][80];

// network address
static char local_server_netadr[MAX_MENU_SERVERS][64];

void M_AddToServerList (char *adr, char *info)
{
	int		i;

	if (m_num_servers == MAX_MENU_SERVERS)
		return;
	while ( *info == ' ' )
		info++;

	// ignore if duplicated
	for (i=0 ; i<m_num_servers ; i++)
		if (!strcmp(info, local_server_names[i]))
			return;

	Q_strncpyz ( local_server_netadr[m_num_servers], adr, sizeof(local_server_netadr[0]) );
	Q_strncpyz ( local_server_names[m_num_servers], info, sizeof(local_server_names[0]) );
	m_num_servers++;
}


void JoinServerFunc( void *self )
{
	char	buffer[128];
	int		index;

	index = ( menuaction_s * ) self - s_joinserver_server_actions;

	if ( Q_stricmp( local_server_names[index], NO_SERVER_STRING ) == 0 )
		return;

	if (index >= m_num_servers)
		return;

	Com_sprintf (buffer, sizeof(buffer), "connect %s\n", local_server_netadr[index]);
	trap_Cmd_ExecuteText (EXEC_APPEND, buffer);
	M_ForceMenuOff ();
}

void SearchGames( char *s )
{
	int		i;

	m_num_servers = 0;
	for (i=0 ; i<MAX_MENU_SERVERS ; i++)
		strcpy (local_server_names[i], NO_SERVER_STRING);

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  va ("Searching for %s servers, this", s) );
	M_Print( 16 + 16, 120 - 48 + 24, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 40, "please be patient." );

	// send out info packets
	trap_Cmd_ExecuteText (EXEC_APPEND, va("pingservers %s %s %s\n", s, 
		s_joinserver_ignore_full_box.curvalue ? "" : "full",
		s_joinserver_ignore_empty_box.curvalue ? "" : "empty"));
}

void SearchLocalGamesFunc( void *self )
{
	SearchGames ( "local" );
}

void SearchGlobalGamesFunc( void *self )
{
	SearchGames ( "global" );
}


void JoinServer_MenuInit( void )
{
	int i;
	int y = 0;
	int y_offset = UI_StringHeightOffset ( 0 );
	static char sbar[64];

	static char *noyes_names[] =
	{
		"no", "yes", 0
	};

	s_joinserver_menu.x = uis.vidWidth / 2;
	s_joinserver_menu.nitems = 0;

	y_offset = UI_StringHeightOffset ( QMF_NONPROPOTIONAL );
	s_joinserver_ignore_full_box.generic.type	= MTYPE_SPINCONTROL;
	s_joinserver_ignore_full_box.generic.x		= 65;
	s_joinserver_ignore_full_box.generic.y		= y += y_offset;
	s_joinserver_ignore_full_box.generic.flags	= QMF_NONPROPOTIONAL;
	s_joinserver_ignore_full_box.generic.name	= "Ignore full servers";
	s_joinserver_ignore_full_box.itemnames		= noyes_names;

	y_offset = UI_StringHeightOffset ( QMF_NONPROPOTIONAL );
	s_joinserver_ignore_empty_box.generic.type	= MTYPE_SPINCONTROL;
	s_joinserver_ignore_empty_box.generic.x		= 65;
	s_joinserver_ignore_empty_box.generic.y		= y+=y_offset;
	s_joinserver_ignore_empty_box.generic.flags	= QMF_NONPROPOTIONAL;
	s_joinserver_ignore_empty_box.generic.name	= "Ignore empty servers";
	s_joinserver_ignore_empty_box.itemnames		= noyes_names;

	y_offset = UI_StringHeightOffset ( 0 );
	y += y_offset;
	s_joinserver_search_local_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_local_action.generic.name	= "search for local servers";
	s_joinserver_search_local_action.generic.flags	= QMF_CENTERED;
	s_joinserver_search_local_action.generic.x	= 0;
	s_joinserver_search_local_action.generic.y	= y+=y_offset;
	s_joinserver_search_local_action.generic.callback = SearchLocalGamesFunc;

	s_joinserver_search_global_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_global_action.generic.name	= "search for global servers";
	s_joinserver_search_global_action.generic.flags	= QMF_CENTERED;
	s_joinserver_search_global_action.generic.x	= 0;
	s_joinserver_search_global_action.generic.y	= y+=y_offset;
	s_joinserver_search_global_action.generic.callback = SearchGlobalGamesFunc;

	Com_sprintf ( sbar, sizeof(sbar), "Master server at %s", trap_Cvar_VariableString("cl_masterServer") );
	s_joinserver_search_global_action.generic.statusbar = sbar;

	y_offset = UI_StringHeightOffset ( QMF_NONPROPOTIONAL );
	y += y_offset;
	for ( i = 0; i < MAX_MENU_SERVERS; i++ )
	{
		s_joinserver_server_actions[i].generic.type	= MTYPE_ACTION;
		strcpy (local_server_names[i], NO_SERVER_STRING);
		s_joinserver_server_actions[i].generic.name	= local_server_names[i];
		s_joinserver_server_actions[i].generic.flags	= QMF_CENTERED|QMF_NONPROPOTIONAL;
		s_joinserver_server_actions[i].generic.x		= 0;
		s_joinserver_server_actions[i].generic.y		= y+=y_offset;
		s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
		s_joinserver_server_actions[i].generic.statusbar = "press ENTER to connect";
	}

	Menu_AddItem( &s_joinserver_menu, &s_joinserver_ignore_full_box );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_ignore_empty_box );

	Menu_AddItem( &s_joinserver_menu, &s_joinserver_search_local_action );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_search_global_action );

	for ( i = 0; i < MAX_MENU_SERVERS; i++ )
		Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_actions[i] );

	Menu_Center( &s_joinserver_menu );

	Menu_Init ( &s_joinserver_menu );
}

void JoinServer_MenuDraw(void)
{
	Menu_Draw( &s_joinserver_menu );
}

const char *JoinServer_MenuKey( int key )
{
	return Default_MenuKey( &s_joinserver_menu, key );
}

void M_Menu_JoinServer_f (void)
{
	JoinServer_MenuInit();
	M_PushMenu( &s_joinserver_menu, JoinServer_MenuDraw, JoinServer_MenuKey );
}
