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

void M_Menu_PlayerConfig_f (void);
void M_Menu_JoinServer_f (void);
void M_Menu_StartServer_f (void);
void M_Menu_DownloadOptions_f (void);

/*
=======================================================================

MULTIPLAYER MENU

=======================================================================
*/
static menuframework_s	s_multiplayer_menu;

static menuseparator_s	s_multiplayer_title;

static menuaction_s		s_join_network_server_action;
static menuaction_s		s_start_network_server_action;
static menuaction_s		s_player_setup_action;
static menuaction_s		s_player_download_action;

static void Multiplayer_MenuDraw (void)
{
	Menu_AdjustCursor( &s_multiplayer_menu, 1 );
	Menu_Draw( &s_multiplayer_menu );
}

static void PlayerSetupFunc( void *unused )
{
	M_Menu_PlayerConfig_f();
}

void DownloadOptionsFunc( void *self )
{
	M_Menu_DownloadOptions_f();
}

static void JoinNetworkServerFunc( void *unused )
{
	M_Menu_JoinServer_f();
}

static void StartNetworkServerFunc( void *unused )
{
	M_Menu_StartServer_f ();
}

void Multiplayer_MenuInit( void )
{
	int w, h;
	int y = 0;
	int y_offset = PROP_SMALL_HEIGHT - 2;

	w = uis.vidWidth;
	h = uis.vidHeight;

	s_multiplayer_menu.x = w / 2;
	s_multiplayer_menu.y = h / 2 - 88;
	s_multiplayer_menu.nitems = 0;

	s_multiplayer_title.generic.type = MTYPE_SEPARATOR;
	s_multiplayer_title.generic.name = "Multiplayer";
	s_multiplayer_title.generic.flags  = QMF_CENTERED|QMF_GIANT;
	s_multiplayer_title.generic.x    = 0;
	s_multiplayer_title.generic.y	 = y;
	y += y_offset;

	s_join_network_server_action.generic.type	= MTYPE_ACTION;
	s_join_network_server_action.generic.flags  = QMF_CENTERED;
	s_join_network_server_action.generic.x		= 0;
	s_join_network_server_action.generic.y		= y+=y_offset;
	s_join_network_server_action.generic.name	= "join network server";
	s_join_network_server_action.generic.callback = JoinNetworkServerFunc;

	s_start_network_server_action.generic.type	= MTYPE_ACTION;
	s_start_network_server_action.generic.flags  = QMF_CENTERED;
	s_start_network_server_action.generic.x		= 0;
	s_start_network_server_action.generic.y		= y+=y_offset;
	s_start_network_server_action.generic.name	= "start network server";
	s_start_network_server_action.generic.callback = StartNetworkServerFunc;

	s_player_setup_action.generic.type			= MTYPE_ACTION;
	s_player_setup_action.generic.flags			= QMF_CENTERED;
	s_player_setup_action.generic.x				= 0;
	s_player_setup_action.generic.y				= y+=y_offset;
	s_player_setup_action.generic.name			= "player setup";
	s_player_setup_action.generic.callback		= PlayerSetupFunc;

	s_player_download_action.generic.type		= MTYPE_ACTION;
	s_player_download_action.generic.name		= "download options";
	s_player_download_action.generic.flags		= QMF_CENTERED;
	s_player_download_action.generic.x			= 0;
	s_player_download_action.generic.y			= y+=y_offset;
	s_player_download_action.generic.statusbar	= NULL;
	s_player_download_action.generic.callback	= DownloadOptionsFunc;

	Menu_AddItem( &s_multiplayer_menu, ( void * )&s_multiplayer_title );
	Menu_AddItem( &s_multiplayer_menu, ( void * )&s_join_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * )&s_start_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * )&s_player_setup_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * )&s_player_download_action );

	Menu_SetStatusBar( &s_multiplayer_menu, NULL );

	Menu_Center( &s_multiplayer_menu );

	Menu_Init ( &s_multiplayer_menu );
}

const char *Multiplayer_MenuKey( int key )
{
	return Default_MenuKey( &s_multiplayer_menu, key );
}

void M_Menu_Multiplayer_f( void )
{
	Multiplayer_MenuInit();
	M_PushMenu( &s_multiplayer_menu, Multiplayer_MenuDraw, Multiplayer_MenuKey );
}

void M_SetMultiplayerStatusBar ( const char *string )
{
	s_multiplayer_menu.statusbar = string;
}
