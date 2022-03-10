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

#include "ui_syscalls.h"
#include "ui_atoms.h"
#include "ui_keycodes.h"

#define MENU_DEFAULT_WIDTH		640
#define MENU_DEFAULT_HEIGHT		480

typedef struct
{
	int vidWidth;
	int vidHeight;

	float scaleX;
	float scaleY;

	int cursorX;
	int cursorY;

	int	clientState;
	int	serverState;
} ui_local_t;

extern ui_local_t uis;

void *UI_malloc (int cnt);
void UI_free (void *buf);

#define NUM_CURSOR_FRAMES 15

const char *Default_MenuKey( menuframework_s *m, int key );

extern char *menu_in_sound;
extern char *menu_move_sound;
extern char *menu_out_sound;

extern qboolean	m_entersound;

float M_ClampCvar( float min, float max, float value );

void M_PopMenu (void);
void M_PushMenu ( menuframework_s *m, void (*draw) (void), const char *(*key) (int k) );
void M_ForceMenuOff (void);
void M_DrawTextBox ( int x, int y, int width, int lines );
void M_DrawCursor ( int x, int y, int f );
void M_Print ( int cx, int cy, char *str );
