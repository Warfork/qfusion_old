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
#include <ctype.h>

#include "ui_local.h"
#include "ui_keycodes.h"

char *menu_in_sound		= "misc/menu1.wav";
char *menu_move_sound	= "misc/menu2.wav";
char *menu_out_sound	= "misc/menu3.wav";

void M_Menu_Main_f (void);
	void M_Menu_Game_f (void);
		void M_Menu_LoadGame_f (void);
		void M_Menu_SaveGame_f (void);
		void M_Menu_PlayerConfig_f (void);
			void M_Menu_DownloadOptions_f (void);
		void M_Menu_Credits_f( void );
	void M_Menu_Multiplayer_f( void );
		void M_Menu_JoinServer_f (void);
			void M_Menu_AddressBook_f( void );
		void M_Menu_StartServer_f (void);
			void M_Menu_DMOptions_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
	void M_Menu_Video_f (void);
	void M_Menu_Quit_f (void);

	void M_Menu_Credits( void );

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound

void	(*m_drawfunc) (void);
const char *(*m_keyfunc) (int key);

//=============================================================================
/* Support Routines */

#define	MAX_MENU_DEPTH	8


typedef struct
{
	void	(*draw) (void);
	const char *(*key) (int k);
} menulayer_t;

menulayer_t	m_layers[MAX_MENU_DEPTH];
int		m_menudepth;

void M_PushMenu ( void (*draw) (void), const char *(*key) (int k) )
{
	int		i;

	if (trap_Cvar_VariableValue ("maxclients") == 1 
		&& trap_Com_ServerState ())
		trap_Cvar_Set ("paused", "1");

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for (i=0 ; i<m_menudepth ; i++)
		if (m_layers[i].draw == draw &&
			m_layers[i].key == key)
		{
			m_menudepth = i;
		}

	if (i == m_menudepth)
	{
		if (m_menudepth >= MAX_MENU_DEPTH) {
			trap_Sys_Error (ERR_FATAL, "M_PushMenu: MAX_MENU_DEPTH");
			return;
		}

		m_layers[m_menudepth].draw = m_drawfunc;
		m_layers[m_menudepth].key = m_keyfunc;
		m_menudepth++;
	}

	m_drawfunc = draw;
	m_keyfunc = key;

	m_entersound = true;

	trap_CL_SetKeyDest_f ( key_menu );
}

void M_ForceMenuOff (void)
{
	m_drawfunc = 0;
	m_keyfunc = 0;
	trap_CL_SetKeyDest_f ( key_game );
	m_menudepth = 0;
	trap_Key_ClearStates ();
	trap_Cvar_Set ("paused", "0");
}

void M_PopMenu (void)
{
	trap_S_StartLocalSound( menu_out_sound );

	if (m_menudepth < 1) {
		trap_Sys_Error (ERR_FATAL, "M_PopMenu: depth < 1");
		return;
	}

	m_menudepth--;

	m_drawfunc = m_layers[m_menudepth].draw;
	m_keyfunc = m_layers[m_menudepth].key;

	if (!m_menudepth)
		M_ForceMenuOff ();
}


const char *Default_MenuKey( menuframework_s *m, int key )
{
	const char *sound = NULL;
	menucommon_s *item;

	if ( m )
	{
		if ( ( item = Menu_ItemAtCursor( m ) ) != 0 )
		{
			if ( item->type == MTYPE_FIELD )
			{
				if ( Field_Key( ( menufield_s * ) item, key ) )
					return NULL;
			}
		}
	}

	switch ( key )
	{
	case K_ESCAPE:
		M_PopMenu();
		return menu_out_sound;
	case K_KP_UPARROW:
	case K_UPARROW:
		if ( m )
		{
			m->cursor--;
			Menu_AdjustCursor( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_TAB:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		if ( m )
		{
			Menu_SlideItem( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		if ( m )
		{
			Menu_SlideItem( m, 1 );
			sound = menu_move_sound;
		}
		break;

	case K_MOUSE1:
	case K_MOUSE2:
	case K_MOUSE3:
	case K_JOY1:
	case K_JOY2:
	case K_JOY3:
	case K_JOY4:
	case K_AUX1:
	case K_AUX2:
	case K_AUX3:
	case K_AUX4:
	case K_AUX5:
	case K_AUX6:
	case K_AUX7:
	case K_AUX8:
	case K_AUX9:
	case K_AUX10:
	case K_AUX11:
	case K_AUX12:
	case K_AUX13:
	case K_AUX14:
	case K_AUX15:
	case K_AUX16:
	case K_AUX17:
	case K_AUX18:
	case K_AUX19:
	case K_AUX20:
	case K_AUX21:
	case K_AUX22:
	case K_AUX23:
	case K_AUX24:
	case K_AUX25:
	case K_AUX26:
	case K_AUX27:
	case K_AUX28:
	case K_AUX29:
	case K_AUX30:
	case K_AUX31:
	case K_AUX32:
		
	case K_KP_ENTER:
	case K_ENTER:
		if ( m )
			Menu_SelectItem( m );
		sound = menu_move_sound;
		break;
	}

	return sound;
}

float M_ClampCvar( float min, float max, float value )
{
	if ( value < min ) return min;
	if ( value > max ) return max;
	return value;
}

//=============================================================================

/*
================
M_DrawCharacter

Draws one solid graphics character
cx and cy are in 320*240 coordinates, and will be centered on
higher res screens.
================
*/

void M_Print (int cx, int cy, char *str)
{
	int w, h;

	trap_Vid_GetCurrentInfo ( &w, &h );
	trap_DrawStringLen (cx + ((w - 320)>>1), cy + ((h - 240)>>1), str, -1, FONT_SMALL, colorWhite);
}

void M_PrintWhite (int cx, int cy, char *str)
{
	int w, h;

	trap_Vid_GetCurrentInfo ( &w, &h );
	trap_DrawStringLen (cx + ((w - 320)>>1), cy + ((h - 240)>>1), str, -1, FONT_SMALL, colorYellow);
}

void M_DrawPic (int x, int y, char *pic)
{
	int w, h;

	trap_Vid_GetCurrentInfo ( &w, &h );
	trap_DrawPic (x + ((w - 320)>>1), y + ((h - 240)>>1), pic);
}


/*
=============
M_DrawCursor

=============
*/
void M_DrawCursor( int x, int y, int f )
{
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	int		cx, cy;
	int		n;
	int		w, h;

	trap_Vid_GetCurrentInfo ( &w, &h );

	// draw left side
	cx = x + ((w - 320)>>1);
	cy = y + ((h - 240)>>1);

	trap_DrawChar ( cx, cy, 1, FONT_SMALL, colorWhite );
	for (n = 0; n < lines; n++)
	{
		cy += SMALL_CHAR_HEIGHT/2;
		trap_DrawChar ( cx, cy, 4, FONT_SMALL, colorWhite );
	}
	trap_DrawChar ( cx, cy+SMALL_CHAR_HEIGHT/2-1, 7, FONT_SMALL, colorWhite );

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y + ((h - 240)>>1);
		trap_DrawChar ( cx, cy, 2, FONT_SMALL, colorWhite );
		for (n = 0; n < lines; n++)
		{
			cy += SMALL_CHAR_HEIGHT/2;
			trap_DrawChar ( cx, cy, 5, FONT_SMALL, colorWhite );
		}
		trap_DrawChar ( cx, cy+SMALL_CHAR_HEIGHT/2, 7, FONT_SMALL, colorWhite );
		width -= 1;
		cx += SMALL_CHAR_WIDTH;
	}

	// draw right side
	cy = y + ((h - 240)>>1);
	trap_DrawChar ( cx, cy, 3, FONT_SMALL, colorWhite );

	for (n = 0; n < lines; n++)
	{
		cy += SMALL_CHAR_HEIGHT/2;
		trap_DrawChar ( cx, cy, 6, FONT_SMALL, colorWhite );
	}
	trap_DrawChar ( cx, cy+SMALL_CHAR_HEIGHT/2, 9, FONT_SMALL, colorWhite );
}

//=============================================================================
/* Menu Subsystem */


/*
=================
M_Init
=================
*/
void M_Init (void)
{
	trap_Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	trap_Cmd_AddCommand ("menu_game", M_Menu_Game_f);
		trap_Cmd_AddCommand ("menu_loadgame", M_Menu_LoadGame_f);
		trap_Cmd_AddCommand ("menu_savegame", M_Menu_SaveGame_f);
		trap_Cmd_AddCommand ("menu_joinserver", M_Menu_JoinServer_f);
			trap_Cmd_AddCommand ("menu_addressbook", M_Menu_AddressBook_f);
		trap_Cmd_AddCommand ("menu_startserver", M_Menu_StartServer_f);
			trap_Cmd_AddCommand ("menu_dmoptions", M_Menu_DMOptions_f);
		trap_Cmd_AddCommand ("menu_playerconfig", M_Menu_PlayerConfig_f);
			trap_Cmd_AddCommand ("menu_downloadoptions", M_Menu_DownloadOptions_f);
		trap_Cmd_AddCommand ("menu_credits", M_Menu_Credits_f );
	trap_Cmd_AddCommand ("menu_multiplayer", M_Menu_Multiplayer_f );
	trap_Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	trap_Cmd_AddCommand ("menu_options", M_Menu_Options_f);
		trap_Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	trap_Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}

/*
=================
M_Init
=================
*/
void M_Shutdown (void)
{
	trap_Cmd_RemoveCommand ("menu_main");
	trap_Cmd_RemoveCommand ("menu_game");
	trap_Cmd_RemoveCommand ("menu_loadgame");
	trap_Cmd_RemoveCommand ("menu_savegame");
	trap_Cmd_RemoveCommand ("menu_joinserver");
	trap_Cmd_RemoveCommand ("menu_addressbook");
	trap_Cmd_RemoveCommand ("menu_startserver");
	trap_Cmd_RemoveCommand ("menu_dmoptions");
	trap_Cmd_RemoveCommand ("menu_playerconfig");
	trap_Cmd_RemoveCommand ("menu_downloadoptions");
	trap_Cmd_RemoveCommand ("menu_credits");
	trap_Cmd_RemoveCommand ("menu_multiplayer");
	trap_Cmd_RemoveCommand ("menu_video");
	trap_Cmd_RemoveCommand ("menu_options");
	trap_Cmd_RemoveCommand ("menu_keys");
	trap_Cmd_RemoveCommand ("menu_quit");
}

/*
=================
M_Draw
=================
*/
void M_Draw (void)
{
	m_drawfunc ();

	// delay playing the enter sound until after the
	// menu has been drawn, to avoid delay while
	// caching images
	if (m_entersound)
	{
		trap_S_StartLocalSound( menu_in_sound );
		m_entersound = false;
	}
}


/*
=================
M_Keydown
=================
*/
void M_Keydown (int key)
{
	const char *s;

	if (m_keyfunc)
		if ( ( s = m_keyfunc( key ) ) != 0 )
			trap_S_StartLocalSound( ( char * ) s );
}
