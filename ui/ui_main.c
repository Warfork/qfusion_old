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

void M_Menu_Game_f (void);
void M_Menu_Multiplayer_f( void );
void M_Menu_Options_f (void);
void M_Menu_Sound_f (void);
void M_Menu_Gfx_f (void);
void M_Menu_Video_f (void);
void M_Menu_Credits_f (void);
void M_Menu_Quit_f (void);

/*
=======================================================================

MAIN MENU

=======================================================================
*/
static menuframework_s	s_main_menu;

static menuseparator_s	s_main_title;

static menuaction_s		s_game_menu_action;
static menuaction_s		s_multiplayer_menu_action;
static menuaction_s		s_options_setup_action;
static menuaction_s		s_sound_menu_action;
static menuaction_s		s_gfx_menu_action;
static menuaction_s		s_video_menu_action;
static menuaction_s		s_console_action;
static menuaction_s		s_credits_action;
static menuaction_s		s_quit_menu_action;

static void GameMenuFunc( void *unused )
{
	M_Menu_Game_f();
}

static void MultiplayerMenuFunc( void *unused )
{
	M_Menu_Multiplayer_f();
}

static void OptionsSetupFunc( void *unused )
{
	M_Menu_Options_f ();
}

static void SoundMenuFunc( void *unused )
{
	M_Menu_Sound_f();
}

static void GfxMenuFunc( void *unused )
{
	M_Menu_Gfx_f();
}

static void VideoMenuFunc( void *unused )
{
	M_Menu_Video_f();
}

static void ConsoleFunc( void *unused )
{
	if ( uis.clientState > ca_disconnected ) {
		M_ForceMenuOff ();
	}

	trap_Cmd_ExecuteText (EXEC_APPEND, "toggleconsole");
	trap_Cmd_Execute();
}

static void CreditsFunc( void *unused )
{
	M_Menu_Credits_f();
}

static void QuitMenuFunc( void *unused )
{
	M_Menu_Quit_f ();
}

void M_MainInit( void )
{
	int w, h;
	int y = 0;
	int y_offset = PROP_BIG_HEIGHT - 3;

	w = uis.vidWidth;
	h = uis.vidHeight;

	s_main_menu.x = w / 2;
	s_main_menu.y = h / 2 - 138;
	s_main_menu.nitems = 0;

	s_main_title.generic.type			= MTYPE_SEPARATOR;
	s_main_title.generic.flags			= QMF_CENTERED|QMF_GIANT;
	s_main_title.generic.name			= "Main Menu";
	s_main_title.generic.x				= 0;
	s_main_title.generic.y				= y;
	y += y_offset;

	s_game_menu_action.generic.type		= MTYPE_ACTION;
	s_game_menu_action.generic.flags	= QMF_CENTERED|QMF_GIANT;
	s_game_menu_action.generic.x		= 0;
	s_game_menu_action.generic.y		= y+=y_offset;
	s_game_menu_action.generic.name		= "Single Player";
	s_game_menu_action.generic.callback = GameMenuFunc;

	s_multiplayer_menu_action.generic.type		= MTYPE_ACTION;
	s_multiplayer_menu_action.generic.flags		= QMF_CENTERED|QMF_GIANT;
	s_multiplayer_menu_action.generic.x			= 0;
	s_multiplayer_menu_action.generic.y			= y+=y_offset;
	s_multiplayer_menu_action.generic.name		= "Multiplayer";
	s_multiplayer_menu_action.generic.callback	= MultiplayerMenuFunc;

	s_options_setup_action.generic.type		= MTYPE_ACTION;
	s_options_setup_action.generic.flags	= QMF_CENTERED|QMF_GIANT;
	s_options_setup_action.generic.x		= 0;
	s_options_setup_action.generic.y		= y+=y_offset;
	s_options_setup_action.generic.name		= "Options";
	s_options_setup_action.generic.callback	= OptionsSetupFunc;

	s_sound_menu_action.generic.type		= MTYPE_ACTION;
	s_sound_menu_action.generic.flags		= QMF_CENTERED|QMF_GIANT;
	s_sound_menu_action.generic.x			= 0;
	s_sound_menu_action.generic.y			= y+=y_offset;
	s_sound_menu_action.generic.name		= "Sound Options";
	s_sound_menu_action.generic.callback	= SoundMenuFunc;

	s_gfx_menu_action.generic.type			= MTYPE_ACTION;
	s_gfx_menu_action.generic.flags			= QMF_CENTERED|QMF_GIANT;
	s_gfx_menu_action.generic.x				= 0;
	s_gfx_menu_action.generic.y				= y+=y_offset;
	s_gfx_menu_action.generic.name			= "Graphics Options";
	s_gfx_menu_action.generic.callback		= GfxMenuFunc;

	s_video_menu_action.generic.type		= MTYPE_ACTION;
	s_video_menu_action.generic.flags		= QMF_CENTERED|QMF_GIANT;
	s_video_menu_action.generic.x			= 0;
	s_video_menu_action.generic.y			= y+=y_offset;
	s_video_menu_action.generic.name		= "Video Options";
	s_video_menu_action.generic.callback	= VideoMenuFunc;

	s_console_action.generic.type			= MTYPE_ACTION;
	s_console_action.generic.flags			= QMF_CENTERED|QMF_GIANT;
	s_console_action.generic.x				= 0;
	s_console_action.generic.y				= y+=y_offset;
	s_console_action.generic.name			= "Console";
	s_console_action.generic.callback		= ConsoleFunc;

	s_credits_action.generic.type			= MTYPE_ACTION;
	s_credits_action.generic.flags			= QMF_CENTERED|QMF_GIANT;
	s_credits_action.generic.x				= 0;
	s_credits_action.generic.y				= y+=y_offset;
	s_credits_action.generic.name			= "Credits";
	s_credits_action.generic.callback		= CreditsFunc;

	s_quit_menu_action.generic.type			= MTYPE_ACTION;
	s_quit_menu_action.generic.flags		= QMF_CENTERED|QMF_GIANT;
	s_quit_menu_action.generic.x			= 0;
	s_quit_menu_action.generic.y			= y+=y_offset;
	s_quit_menu_action.generic.name			= "Quit";
	s_quit_menu_action.generic.callback		= QuitMenuFunc;

	Menu_AddItem( &s_main_menu, ( void *) &s_main_title );
	Menu_AddItem( &s_main_menu, ( void * ) &s_game_menu_action );
	Menu_AddItem( &s_main_menu, ( void * ) &s_multiplayer_menu_action );
	Menu_AddItem( &s_main_menu, ( void * ) &s_options_setup_action );
	Menu_AddItem( &s_main_menu, ( void * ) &s_sound_menu_action );
	Menu_AddItem( &s_main_menu, ( void * ) &s_gfx_menu_action );
	Menu_AddItem( &s_main_menu, ( void * ) &s_video_menu_action );
	Menu_AddItem( &s_main_menu, ( void * ) &s_console_action );
	Menu_AddItem( &s_main_menu, ( void * ) &s_credits_action );
	Menu_AddItem( &s_main_menu, ( void * ) &s_quit_menu_action );

	Menu_Init ( &s_main_menu );

	Menu_SetStatusBar( &s_main_menu, NULL );
}


void M_Main_Draw (void)
{
	Menu_AdjustCursor( &s_main_menu, 1 );
	Menu_Draw( &s_main_menu );
}

const char *M_Main_Key (int key)
{
	return Default_MenuKey( &s_main_menu, key );
}

void M_Menu_Main_f (void)
{
	M_MainInit ();
	M_PushMenu ( &s_main_menu, M_Main_Draw, M_Main_Key );
}
