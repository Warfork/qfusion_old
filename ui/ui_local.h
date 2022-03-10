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

#include "../game/q_shared.h"
#include "../game/gs_public.h"
#include "ui_public.h"
#include "../cgame/ref.h"
#include "ui_syscalls.h"
#include "ui_atoms.h"
#include "ui_keycodes.h"
#ifdef SKELMOD
	#include "ui_boneposes.h"
#else
	#include "ui_pmodels.h"
#endif

#define SMALL_CHAR_WIDTH	8
#define SMALL_CHAR_HEIGHT	16

#define BIG_CHAR_WIDTH		16
#define BIG_CHAR_HEIGHT		16

#define GIANT_CHAR_WIDTH	32
#define GIANT_CHAR_HEIGHT	48

#define PROP_CHAR_HEIGHT	27
#define PROP_SMALL_SCALE	0.75
#define PROP_BIG_SCALE		1
#define PROP_SMALL_SPACING	1.5
#define PROP_BIG_SPACING	1

#define PROP_SMALL_HEIGHT	PROP_CHAR_HEIGHT*PROP_SMALL_SCALE
#define PROP_BIG_HEIGHT		PROP_CHAR_HEIGHT*PROP_BIG_SCALE

#define MENU_DEFAULT_WIDTH		640
#define MENU_DEFAULT_HEIGHT		480

typedef struct
{
	int		vidWidth;
	int		vidHeight;

	int		time;

	float	scaleX;
	float	scaleY;

	int		cursorX;
	int		cursorY;

	int		clientState;
	int		serverState;

	struct shader_s *whiteShader;
	struct shader_s *charsetShader;
	struct shader_s *propfontShader;
} ui_local_t;

extern ui_local_t uis;

void UI_Error ( char *fmt, ... );
void UI_Printf ( char *fmt, ... );
void UI_FillRect ( int x, int y, int w, int h, vec4_t color );

#define UI_Malloc(size) trap_MemAlloc(size, __FILE__, __LINE__)
#define UI_Free(data) trap_MemFree(data, __FILE__, __LINE__)

char *_UI_CopyString (const char *in, const char *filename, int fileline);
#define UI_CopyString(in) _UI_CopyString(in,__FILE__,__LINE__)

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

void M_Menu_Main_f (void);
void M_AddToServerList (char *adr, char *info);
void M_ForceMenuOff (void);

int UI_API (void);
void UI_Init ( int vidWidth, int vidHeight );
void UI_Shutdown (void);
void UI_Refresh ( int time, int clientState, int serverState, qboolean backGround );
void UI_DrawConnectScreen ( char *serverName, int connectCount, qboolean backGround );
void UI_Keydown ( int key );
void UI_MouseMove (int dx, int dy);

