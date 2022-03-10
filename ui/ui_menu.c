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

char *menu_in_sound		= "sound/misc/menu1.wav";
char *menu_move_sound	= "sound/misc/menu2.wav";
char *menu_out_sound	= "sound/misc/menu3.wav";

void M_Menu_Main_f (void);
	void M_Menu_Game_f (void);
		void M_Menu_LoadGame_f (void);
		void M_Menu_SaveGame_f (void);
		void M_Menu_PlayerConfig_f (void);
			void M_Menu_DownloadOptions_f (void);
		void M_Menu_Credits_f( void );
	void M_Menu_Multiplayer_f( void );
		void M_Menu_JoinServer_f (void);
		void M_Menu_StartServer_f (void);
			void M_Menu_DMOptions_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
	void M_Menu_Sound_f (void);
	void M_Menu_Gfx_f (void);
	void M_Menu_Video_f (void);
	void M_Menu_Quit_f (void);

	void M_Menu_Credits( void );

ui_local_t	uis;

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound

menuframework_s *m_active;
void	*m_cursoritem;
void	(*m_drawfunc) (void);
const char *(*m_keyfunc) (int key);

//======================================================================

/*
============
UI_API
============
*/
int UI_API (void) {
	return UI_API_VERSION;
}

/*
============
UI_Error
============
*/
void UI_Error ( char *fmt, ... )
{
	char		msg[1024];
	va_list		argptr;

	va_start ( argptr, fmt );
	if ( vsprintf (msg, fmt, argptr) > sizeof(msg) ) {
		trap_Error ( "CG_Error: Buffer overflow" );
	}
	va_end ( argptr );

	trap_Error ( msg );
}

/*
============
UI_Printf
============
*/
void UI_Printf ( char *fmt, ... )
{
	char		msg[1024];
	va_list		argptr;

	va_start ( argptr, fmt );
	if ( vsprintf (msg, fmt, argptr) > sizeof(msg) ) {
		trap_Error ( "CG_Print: Buffer overflow" );
	}
	va_end ( argptr );

	trap_Print ( msg );
}

//=============================================================================
/* Support Routines */

#define	MAX_MENU_DEPTH	8


typedef struct
{
	menuframework_s *m;
	void	(*draw) (void);
	const char *(*key) (int k);
} menulayer_t;

menulayer_t	m_layers[MAX_MENU_DEPTH];
int		m_menudepth;

void UI_UpdateMousePosition (void);

void M_Cache( void )
{
	uis.whiteShader = trap_R_RegisterPic ( "white" );
	uis.charsetShader = trap_R_RegisterPic ( "gfx/2d/bigchars" );
	uis.propfontShader = trap_R_RegisterPic ( "menu/art/font1_prop" );
}

void M_PushMenu( menuframework_s *m, void (*draw) (void), const char *(*key) (int k) )
{
	int		i;

	if (trap_Cvar_VariableValue ("sv_maxclients") == 1 
		&& uis.serverState)
		trap_Cvar_Set ("paused", "1");

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for (i=0 ; i<m_menudepth ; i++)
		if (m_layers[i].m == m && m_layers[i].draw == draw &&
			m_layers[i].key == key)
		{
			m_menudepth = i;
		}

	if (i == m_menudepth)
	{
		if (m_menudepth >= MAX_MENU_DEPTH) {
			UI_Error ("M_PushMenu: MAX_MENU_DEPTH");
			return;
		}

		m_layers[m_menudepth].m = m_active;
		m_layers[m_menudepth].draw = m_drawfunc;
		m_layers[m_menudepth].key = m_keyfunc;
		m_menudepth++;

		M_Cache ();
	}

	m_drawfunc = draw;
	m_keyfunc = key;
	m_active = m;

	m_entersound = qtrue;

	UI_UpdateMousePosition ();

	trap_CL_SetKeyDest ( key_menu );
}

void M_ForceMenuOff (void)
{
	m_active = 0;
	m_drawfunc = 0;
	m_keyfunc = 0;
	trap_CL_SetKeyDest ( key_game );
	m_menudepth = 0;
	trap_Key_ClearStates ();
	trap_Cvar_Set ("paused", "0");
}

void M_PopMenu (void)
{
	if ( m_menudepth == 1 ) {
		// start the demo loop again
		if( uis.clientState < ca_connecting ) {
//			trap_Cmd_ExecuteText (EXEC_APPEND, "d1\n");
			return;
		}
		M_ForceMenuOff ();
		return;
	}

	trap_S_StartLocalSound( menu_out_sound );

	if (m_menudepth < 1) {
		UI_Error ("M_PopMenu: depth < 1");
		return;
	}

	m_menudepth--;

	m_drawfunc = m_layers[m_menudepth].draw;
	m_keyfunc = m_layers[m_menudepth].key;
	m_active = m_layers[m_menudepth].m;

	M_Cache ();

	UI_UpdateMousePosition ();
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

	case K_MOUSE1:
		if ( m && (m_cursoritem == item) && Menu_SlideItem( m, 1 ) )
		{
			sound = menu_move_sound;
		}
		else
		{
			if ( m )
				Menu_SelectItem( m );
			sound = menu_move_sound;
		}
		break;

	case K_MOUSE2:
		if ( m && (m_cursoritem == item) && Menu_SlideItem( m, -1 ) )
		{
			sound = menu_move_sound;
		}
		else
		{
			M_PopMenu ();
			sound = menu_out_sound;
		}
		break;

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
=================
UI_CopyString
=================
*/
char *_UI_CopyString (const char *in, const char *filename, int fileline)
{
	char	*out;
	
	out = trap_MemAlloc (strlen(in)+1, filename, fileline);
	strcpy (out, in);
	return out;
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

	w = uis.vidWidth;
	h = uis.vidHeight;

	UI_DrawNonPropString (cx + ((w - 320)>>1), cy + ((h - 240)>>1), str, FONT_SMALL, colorWhite);
}

void M_PrintWhite (int cx, int cy, char *str)
{
	int w, h;

	w = uis.vidWidth;
	h = uis.vidHeight;

	UI_DrawNonPropString (cx + ((w - 320)>>1), cy + ((h - 240)>>1), str, FONT_SMALL, colorYellow);
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

	// draw left side
	cx = x + ((uis.vidWidth - 320)>>1);
	cy = y + ((uis.vidHeight - 240)>>1);

	UI_DrawNonPropChar ( cx, cy, 1, FONT_SMALL, colorGreen );
	for (n = 0; n < lines; n++)
	{
		cy += SMALL_CHAR_HEIGHT;
		UI_DrawNonPropChar ( cx, cy, 4, FONT_SMALL, colorGreen );
	}
	UI_DrawNonPropChar ( cx, cy+SMALL_CHAR_HEIGHT/2-1, 7, FONT_SMALL, colorGreen );

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y + ((uis.vidHeight - 240)>>1);
		UI_DrawNonPropChar ( cx, cy, 2, FONT_SMALL, colorGreen );
		for (n = 0; n < lines; n++)
		{
			cy += SMALL_CHAR_HEIGHT;
			UI_DrawNonPropChar ( cx, cy, 5, FONT_SMALL, colorWhite );
		}
		UI_DrawNonPropChar ( cx, cy+SMALL_CHAR_HEIGHT/2, 7, FONT_SMALL, colorGreen );
		width -= 1;
		cx += SMALL_CHAR_WIDTH;
	}

	// draw right side
	cy = y + ((uis.vidHeight - 240)>>1);
	UI_DrawNonPropChar ( cx, cy, 3, FONT_SMALL, colorGreen );

	for (n = 0; n < lines; n++)
	{
		cy += SMALL_CHAR_HEIGHT;
		UI_DrawNonPropChar ( cx, cy, 6, FONT_SMALL, colorGreen );
	}
	UI_DrawNonPropChar ( cx, cy+SMALL_CHAR_HEIGHT/2, 9, FONT_SMALL, colorGreen );
}


//=============================================================================
/* User Interface Subsystem */

/*
=================
UI_Init
=================
*/
void UI_Init ( int vidWidth, int vidHeight )
{
	m_active = NULL;
	m_cursoritem = NULL;
	m_drawfunc = NULL;
	m_keyfunc = NULL;
	m_entersound = qfalse;

	memset( &uis, 0, sizeof( uis ) );

	uis.vidWidth = vidWidth;
	uis.vidHeight = vidHeight;

#if 0
	uis.scaleX = uis.vidWidth / MENU_DEFAULT_WIDTH;
	uis.scaleY = uis.vidHeight / MENU_DEFAULT_HEIGHT;
#else
	uis.scaleX = 1;
	uis.scaleY = 1;
#endif

	uis.cursorX = uis.vidWidth / 2;
	uis.cursorY = uis.vidHeight / 2;

	trap_Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	trap_Cmd_AddCommand ("menu_game", M_Menu_Game_f);
		trap_Cmd_AddCommand ("menu_loadgame", M_Menu_LoadGame_f);
		trap_Cmd_AddCommand ("menu_savegame", M_Menu_SaveGame_f);
		trap_Cmd_AddCommand ("menu_joinserver", M_Menu_JoinServer_f);
		trap_Cmd_AddCommand ("menu_startserver", M_Menu_StartServer_f);
			trap_Cmd_AddCommand ("menu_dmoptions", M_Menu_DMOptions_f);
		trap_Cmd_AddCommand ("menu_playerconfig", M_Menu_PlayerConfig_f);
			trap_Cmd_AddCommand ("menu_downloadoptions", M_Menu_DownloadOptions_f);
		trap_Cmd_AddCommand ("menu_credits", M_Menu_Credits_f );
	trap_Cmd_AddCommand ("menu_multiplayer", M_Menu_Multiplayer_f );
	trap_Cmd_AddCommand ("menu_sound", M_Menu_Sound_f);
	trap_Cmd_AddCommand ("menu_gfx", M_Menu_Gfx_f);
	trap_Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	trap_Cmd_AddCommand ("menu_options", M_Menu_Options_f);
		trap_Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	trap_Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);

	M_Cache ();
#ifdef SKELMOD
	UI_InitTemporaryBoneposesCache();
#endif
}

/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown (void)
{
	trap_Cmd_RemoveCommand ("menu_main");
	trap_Cmd_RemoveCommand ("menu_game");
	trap_Cmd_RemoveCommand ("menu_loadgame");
	trap_Cmd_RemoveCommand ("menu_savegame");
	trap_Cmd_RemoveCommand ("menu_joinserver");
	trap_Cmd_RemoveCommand ("menu_startserver");
	trap_Cmd_RemoveCommand ("menu_dmoptions");
	trap_Cmd_RemoveCommand ("menu_playerconfig");
	trap_Cmd_RemoveCommand ("menu_downloadoptions");
	trap_Cmd_RemoveCommand ("menu_credits");
	trap_Cmd_RemoveCommand ("menu_multiplayer");
	trap_Cmd_RemoveCommand ("menu_gfx");
	trap_Cmd_RemoveCommand ("menu_sound");
	trap_Cmd_RemoveCommand ("menu_video");
	trap_Cmd_RemoveCommand ("menu_options");
	trap_Cmd_RemoveCommand ("menu_keys");
	trap_Cmd_RemoveCommand ("menu_quit");
}

/*
=================
UI_UpdateMousePosition
=================
*/
void UI_UpdateMousePosition (void)
{
	int i;

	if ( !m_active || !m_active->nitems ) {
		return;
	}

	/*
	** check items
	*/
	m_cursoritem = NULL;

	for ( i = 0; i < m_active->nitems; i++ )
	{
		if ( uis.cursorX > ( ( menucommon_s * ) m_active->items[i] )->maxs[0] || 
			uis.cursorY > ( ( menucommon_s * ) m_active->items[i] )->maxs[1] ||
			uis.cursorX < ( ( menucommon_s * ) m_active->items[i] )->mins[0] || 
			uis.cursorY < ( ( menucommon_s * ) m_active->items[i] )->mins[1] )
			continue;

		m_cursoritem = m_active->items[i];

		if ( m_active->cursor == i ) {
			break;
		}

		Menu_AdjustCursor( m_active, i - m_active->cursor );
		m_active->cursor = i;

		trap_S_StartLocalSound( ( char * )menu_move_sound );

		break;
	}
}

/*
=================
UI_MouseMove
=================
*/
void UI_MouseMove (int dx, int dy)
{
	uis.cursorX += dx;
	uis.cursorY += dy;

	clamp ( uis.cursorX, 0, uis.vidWidth );
	clamp ( uis.cursorY, 0, uis.vidHeight );

	if ( dx || dy ) {
		UI_UpdateMousePosition ();
	}
}

/*
=================
UI_DrawConnectScreen
=================
*/
void UI_DrawConnectScreen ( char *serverName, int connectCount, qboolean backGround )
{
	qboolean localhost;
	int fontstyle = FONT_SMALL|FONT_SHADOWED;
	char str[MAX_QPATH], levelshot[MAX_QPATH];
	char mapname[MAX_QPATH], message[MAX_QPATH];

	localhost = !serverName || !serverName[0] || !Q_stricmp ( serverName, "localhost" );

	M_Cache ();

	trap_GetConfigString ( CS_MAPNAME, mapname, sizeof(mapname) );
	if ( backGround ) {
		if ( mapname[0] ) {
			Q_snprintfz ( levelshot, sizeof(levelshot), "levelshots/%s.jpg", mapname );

			if ( trap_FS_FOpenFile( levelshot, NULL, FS_READ ) == -1 ) 
				Q_snprintfz ( levelshot, sizeof(levelshot), "levelshots/%s.tga", mapname );

			if ( trap_FS_FOpenFile( levelshot, NULL, FS_READ ) == -1 ) 
				Q_snprintfz ( levelshot, sizeof(levelshot), "menu/art/unknownmap" );

			trap_R_DrawStretchPic ( 0, 0, uis.vidWidth, uis.vidHeight, 
				0, 0, 1, 1, colorWhite, trap_R_RegisterPic ( levelshot ) );
			trap_R_DrawStretchPic ( 0, 0, uis.vidWidth, uis.vidHeight, 
				0, 0, 2.5, 2, colorWhite, trap_R_RegisterPic ( "levelShotDetail" ) );
		} else {
			UI_FillRect ( 0, 0, uis.vidWidth, uis.vidHeight, colorBlack );
		}
	}

	// draw server name if not local host
	if ( !localhost ) {
		Q_snprintfz ( str, sizeof(str), "Connecting to %s", serverName );
		UI_DrawPropString ( (uis.vidWidth - UI_PropStringLength (str, fontstyle)) / 2, 64, str, fontstyle, colorWhite );
	}

	if( mapname[0] ) {
		trap_GetConfigString( CS_MESSAGE, message, sizeof(message) );

		if( message[0] )	// level name ("message")
			UI_DrawPropString( (uis.vidWidth - UI_PropStringLength (message, fontstyle)) / 2, 150, message, fontstyle, colorWhite );
	} else {
		if( !localhost ) {
			Q_snprintfz ( message, sizeof(message), "Awaiting connection... %i", connectCount );
			UI_DrawPropString( (uis.vidWidth - UI_PropStringLength (message, fontstyle)) / 2, 150, message, fontstyle, colorWhite );
		} else {
			Q_strncpyz ( message, "Loading...", sizeof(message) );
			UI_DrawPropString( (uis.vidWidth - UI_PropStringLength (message, fontstyle)) / 2, 150, message, fontstyle, colorWhite );
		}
	}
}

/*
=================
UI_Refresh
=================
*/
void UI_Refresh ( int time, int clientState, int serverState, qboolean backGround )
{
	uis.time = time;
	uis.clientState = clientState;
	uis.serverState = serverState;

	// draw background
	if ( backGround ) {
		trap_R_DrawStretchPic ( 0, 0, uis.vidWidth, uis.vidHeight, 
			0, 0, 1, 1, colorWhite, trap_R_RegisterPic ( "menuback" ) );
	}

	if ( !m_drawfunc )
		return;

	m_drawfunc ();

	// draw cursor
	trap_R_DrawStretchPic ( uis.cursorX - 16, uis.cursorY - 16, 32, 32, 
		0, 0, 1, 1, colorWhite, trap_R_RegisterPic ( "menu/art/3_cursor2" ) );

	// delay playing the enter sound until after the
	// menu has been drawn, to avoid delay while
	// caching images
	if (m_entersound)
	{
		trap_S_StartLocalSound( menu_in_sound );
		m_entersound = qfalse;
	}
}

/*
=================
UI_Keydown
=================
*/
void UI_Keydown (int key)
{
	const char *s;

	if (m_keyfunc)
		if ( ( s = m_keyfunc( key ) ) != 0 )
			trap_S_StartLocalSound( ( char * ) s );
}

//======================================================================

#ifndef UI_HARD_LINKED
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

void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);
	text[sizeof(text)-1] = 0;

	trap_Print (text);
}
#endif


