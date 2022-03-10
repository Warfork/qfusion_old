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
#include "ui_keycodes.h"

#define REF_OPENGL	0
#define REF_3DFX	1
#define REF_POWERVR	2
#define REF_VERITE	3

static void Video_MenuInit( void );

/*
=======================================================================

VIDEO MENU

=======================================================================
*/

static menuframework_s	s_video_menu;

static menulist_s		s_mode_list;
static menulist_s		s_ref_list;
static menuslider_s		s_tq_slider;
static menuslider_s		s_screensize_slider;
static menuslider_s		s_brightness_slider;
static menulist_s		s_lighting_box;
static menulist_s  		s_fs_box;
static menuaction_s		s_cancel_action;
static menuaction_s		s_defaults_action;

static void ScreenSizeCallback( void *s )
{
	menuslider_s *slider = ( menuslider_s * ) s;

	trap_Cvar_SetValue( "viewsize", slider->curvalue * 10 );
}

static void BrightnessCallback( void *s )
{
	float gamma;
	gamma = ( 0.8 - ( s_brightness_slider.curvalue/10.0 - 0.5 ) ) + 0.5;

	trap_Cvar_SetValue( "vid_gamma", gamma );
}

static void ResetDefaults( void *unused )
{
	Video_MenuInit ();
}

static void ApplyChanges( void *unused )
{
	trap_Cvar_SetValue( "gl_picmip", 3 - s_tq_slider.curvalue );
	trap_Cvar_SetValue( "r_vertexlight", s_lighting_box.curvalue );
	trap_Cvar_SetValue( "vid_fullscreen", s_fs_box.curvalue );
	trap_Cvar_SetValue( "gl_mode", s_mode_list.curvalue );

	trap_Cmd_ExecuteText (EXEC_APPEND, "vid_restart\n");
	trap_Cmd_Execute();

	M_ForceMenuOff();
}

static void CancelChanges( void *unused )
{
	M_PopMenu();
}

/*
** Video_MenuInit
*/
static void Video_MenuInit( void )
{
	static const char *resolutions[] = 
	{
		"[320 240  ]",
		"[400 300  ]",
		"[512 384  ]",
		"[640 480  ]",
		"[800 600  ]",
		"[960 720  ]",
		"[1024 768 ]",
		"[1152 864 ]",
		"[1280 960 ]",
		"[1600 1200]",
		"[2048 1536]",
		0
	};
	static const char *refs[] =
	{
		"[default OpenGL]",
		"[3Dfx OpenGL   ]",
		"[PowerVR OpenGL]",
//		"[Rendition OpenGL]",
		0
	};
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *lighting_names[] =
	{
		"lightmap",
		"vertex",
		0
	};

	char *gl_driver = trap_Cvar_VariableString( "gl_driver" );
	int y = 0;
	int y_offset = BIG_CHAR_HEIGHT + 2;

	s_mode_list.curvalue = trap_Cvar_VariableValue( "gl_mode" );
	s_screensize_slider.curvalue = trap_Cvar_VariableValue( "viewsize" )/10;

	if ( strcmp( gl_driver, "3dfxgl" ) == 0 )
		s_ref_list.curvalue = REF_3DFX;
	else if ( strcmp( gl_driver, "pvrgl" ) == 0 )
		s_ref_list.curvalue = REF_POWERVR;
	else if ( strcmp( gl_driver, "opengl32" ) == 0 )
		s_ref_list.curvalue = REF_OPENGL;
	else
		s_ref_list.curvalue = REF_OPENGL;

	s_video_menu.x = trap_GetWidth() * 0.5;
	s_video_menu.nitems = 0;

	s_ref_list.generic.type = MTYPE_SPINCONTROL;
	s_ref_list.generic.name = "driver";
	s_ref_list.generic.x = 0;
	s_ref_list.generic.y = y;
	s_ref_list.itemnames = refs;
	
	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = y+=y_offset;
	s_mode_list.itemnames = resolutions;
	
	s_screensize_slider.generic.type	= MTYPE_SLIDER;
	s_screensize_slider.generic.x		= 0;
	s_screensize_slider.generic.y		= y+=y_offset;
	s_screensize_slider.generic.name	= "screen size";
	s_screensize_slider.minvalue = 3;
	s_screensize_slider.maxvalue = 12;
	s_screensize_slider.generic.callback = ScreenSizeCallback;
	
	s_brightness_slider.generic.type	= MTYPE_SLIDER;
	s_brightness_slider.generic.x	= 0;
	s_brightness_slider.generic.y	= y+=y_offset;
	s_brightness_slider.generic.name	= "brightness";
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 5;
	s_brightness_slider.maxvalue = 13;
	s_brightness_slider.curvalue = ( 1.3 - trap_Cvar_VariableValue( "vid_gamma" ) + 0.5 ) * 10;

	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.x	= 0;
	s_fs_box.generic.y	= y+=y_offset;
	s_fs_box.generic.name	= "fullscreen";
	s_fs_box.itemnames = yesno_names;
	s_fs_box.curvalue = trap_Cvar_VariableValue( "vid_fullscreen" );
	y+=y_offset;

	s_tq_slider.generic.type	= MTYPE_SLIDER;
	s_tq_slider.generic.x		= 0;
	s_tq_slider.generic.y		= y+=y_offset;
	s_tq_slider.generic.name	= "texture quality";
	s_tq_slider.minvalue		= 0;
	s_tq_slider.maxvalue		= 3;
	s_tq_slider.curvalue		= 3-trap_Cvar_VariableValue( "gl_picmip" );

	s_lighting_box.generic.type = MTYPE_SPINCONTROL;
	s_lighting_box.generic.x	= 0;
	s_lighting_box.generic.y	= y+=y_offset;
	s_lighting_box.generic.name	= "lighting";
	s_lighting_box.curvalue		= trap_Cvar_VariableValue( "r_vertexlight" );
	s_lighting_box.itemnames	= lighting_names;
	y+=y_offset;

	s_defaults_action.generic.type = MTYPE_ACTION;
	s_defaults_action.generic.name = "reset to defaults";
	s_defaults_action.generic.x    = 0;
	s_defaults_action.generic.y    = y+=y_offset;
	s_defaults_action.generic.callback = ResetDefaults;
	
	s_cancel_action.generic.type = MTYPE_ACTION;
	s_cancel_action.generic.name = "apply changes";
	s_cancel_action.generic.x    = 0;
	s_cancel_action.generic.y    = y+=y_offset;
	s_cancel_action.generic.callback = ApplyChanges;

	Menu_AddItem( &s_video_menu, ( void * ) &s_ref_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_mode_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_screensize_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_brightness_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_fs_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_tq_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_lighting_box );

	Menu_AddItem( &s_video_menu, ( void * ) &s_defaults_action );
	Menu_AddItem( &s_video_menu, ( void * ) &s_cancel_action );

	Menu_Center( &s_video_menu );
	s_video_menu.x -= 8;
}

/*
================
Video_MenuDraw
================
*/
void Video_MenuDraw (void)
{
	/*
	** move cursor to a reasonable starting position
	*/
	Menu_AdjustCursor( &s_video_menu, 1 );

	/*
	** draw the menu
	*/
	Menu_Draw( &s_video_menu );
}

/*
================
Video_MenuKey
================
*/
const char *Video_MenuKey( int key )
{
	return Default_MenuKey( &s_video_menu, key );
}

void M_Menu_Video_f (void)
{
	Video_MenuInit ();
	M_PushMenu( Video_MenuDraw, Video_MenuKey );
}
