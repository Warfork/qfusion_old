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

#define REF_OPENGL	0
#define REF_3DFX	1
#define REF_POWERVR	2
#define REF_VERITE	3

static void Video_MenuInit( void );
void M_Menu_GLExt_f (void);

/*
=======================================================================

VIDEO MENU

=======================================================================
*/

static menuframework_s	s_video_menu;

static menuaction_s		s_glext_action;

static menulist_s		s_mode_list;
static menulist_s		s_ref_list;
static menulist_s		s_detailtextures_list;
static menuslider_s		s_tq_slider;
static menuslider_s		s_sq_slider;
static menulist_s		s_tf_box;
static menulist_s		s_colordepth_box;
static menuslider_s		s_screensize_slider;
static menuslider_s		s_brightness_slider;
static menulist_s		s_lighting_box;
static menulist_s  		s_fs_box;

static menuaction_s		s_apply_action;
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

static void ExtensionsCallback( void *s )
{
	M_Menu_GLExt_f ();
}

static void ApplyChanges( void *unused )
{
	trap_Cvar_SetValue( "r_picmip", 3 - s_tq_slider.curvalue );
	trap_Cvar_SetValue( "r_skymip", 3 - s_sq_slider.curvalue );
	trap_Cvar_SetValue( "r_vertexlight", s_lighting_box.curvalue );
	trap_Cvar_SetValue( "vid_fullscreen", s_fs_box.curvalue );
	trap_Cvar_SetValue( "r_mode", s_mode_list.curvalue );
	trap_Cvar_SetValue( "r_colorbits", 16 * (int)s_colordepth_box.curvalue );
	trap_Cvar_SetValue( "r_detailtextures", s_detailtextures_list.curvalue );

	if ( s_tf_box.curvalue ) {
		trap_Cvar_Set( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
	} else {
	 	trap_Cvar_Set( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST" );
	}

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
		"no", "yes", 0
	};

	static const char *detailtextures_items[] =
	{
		"off", "on", 0
	};

	static const char *lighting_names[] =
	{
		"lightmap", "vertex", 0
	};

	static const char *filter_names[] =
	{
		"bilinear", "trilinear", 0
	};

	static const char *colordepth_names[] =
	{
		"desktop", "16 bits", "32 bits", 0
	};

	char *gl_driver = trap_Cvar_VariableString( "gl_driver" );
	int y = 0;
	int y_offset = PROP_SMALL_HEIGHT - 2;

	s_mode_list.curvalue = trap_Cvar_VariableValue( "r_mode" );
	s_screensize_slider.curvalue = trap_Cvar_VariableValue( "viewsize" )/10;

	if ( strcmp( gl_driver, "3dfxgl" ) == 0 )
		s_ref_list.curvalue = REF_3DFX;
	else if ( strcmp( gl_driver, "pvrgl" ) == 0 )
		s_ref_list.curvalue = REF_POWERVR;
	else if ( strcmp( gl_driver, "opengl32" ) == 0 )
		s_ref_list.curvalue = REF_OPENGL;
	else
		s_ref_list.curvalue = REF_OPENGL;

	s_video_menu.x = uis.vidWidth / 2;
	s_video_menu.y = 0;
	s_video_menu.nitems = 0;

	s_glext_action.generic.type		= MTYPE_ACTION;
	s_glext_action.generic.name		= "OpenGL Extensions";
	s_glext_action.generic.flags	= QMF_CENTERED;
	s_glext_action.generic.x		= 0;
	s_glext_action.generic.y		= y;
	s_glext_action.generic.callback = ExtensionsCallback;
	y+=y_offset;

	s_ref_list.generic.type			= MTYPE_SPINCONTROL;
	s_ref_list.generic.name			= "driver";
	s_ref_list.generic.x			= 0;
	s_ref_list.generic.y			= y+=y_offset;
	s_ref_list.itemnames			= refs;
	
	s_mode_list.generic.type		= MTYPE_SPINCONTROL;
	s_mode_list.generic.name		= "video mode";
	s_mode_list.generic.x			= 0;
	s_mode_list.generic.y			= y+=y_offset;
	s_mode_list.itemnames			= resolutions;
	
	s_screensize_slider.generic.type	= MTYPE_SLIDER;
	s_screensize_slider.generic.x		= 0;
	s_screensize_slider.generic.y		= y+=y_offset;
	s_screensize_slider.generic.name	= "screen size";
	s_screensize_slider.minvalue		= 3;
	s_screensize_slider.maxvalue		= 12;
	s_screensize_slider.generic.callback = ScreenSizeCallback;
	
	s_brightness_slider.generic.type	= MTYPE_SLIDER;
	s_brightness_slider.generic.x		= 0;
	s_brightness_slider.generic.y		= y+=y_offset;
	s_brightness_slider.generic.name	= "brightness";
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue		= 5;
	s_brightness_slider.maxvalue		= 13;
	s_brightness_slider.curvalue		= ( 1.3 - trap_Cvar_VariableValue( "vid_gamma" ) + 0.5 ) * 10;

	s_fs_box.generic.type	= MTYPE_SPINCONTROL;
	s_fs_box.generic.x		= 0;
	s_fs_box.generic.y		= y+=y_offset;
	s_fs_box.generic.name	= "fullscreen";
	s_fs_box.itemnames		= yesno_names;
	s_fs_box.curvalue		= trap_Cvar_VariableValue( "vid_fullscreen" );

	s_colordepth_box.generic.type	= MTYPE_SPINCONTROL;
	s_colordepth_box.generic.x		= 0;
	s_colordepth_box.generic.y		= y+=y_offset;
	s_colordepth_box.generic.name	= "color depth";
	s_colordepth_box.itemnames		= colordepth_names;

	if ( !Q_stricmp( trap_Cvar_VariableString( "r_colorbits" ), "16" ) )
		s_colordepth_box.curvalue		= 1;
	else if ( !Q_stricmp( trap_Cvar_VariableString( "r_colorbits" ), "32" ) )
		s_colordepth_box.curvalue		= 2;
	else
		s_colordepth_box.curvalue		= 0;

	y+=y_offset;

	s_detailtextures_list.generic.type		= MTYPE_SPINCONTROL;
	s_detailtextures_list.generic.x			= 0;
	s_detailtextures_list.generic.y			= y+=y_offset;
	s_detailtextures_list.generic.name		= "detail textures";
	s_detailtextures_list.itemnames			= detailtextures_items;
	s_detailtextures_list.curvalue			= trap_Cvar_VariableValue( "r_detailtextures" );

	s_tq_slider.generic.type	= MTYPE_SLIDER;
	s_tq_slider.generic.x		= 0;
	s_tq_slider.generic.y		= y+=y_offset;
	s_tq_slider.generic.name	= "texture quality";
	s_tq_slider.minvalue		= 0;
	s_tq_slider.maxvalue		= 3;
	s_tq_slider.curvalue		= 3-trap_Cvar_VariableValue( "r_picmip" );

	s_sq_slider.generic.type	= MTYPE_SLIDER;
	s_sq_slider.generic.x		= 0;
	s_sq_slider.generic.y		= y+=y_offset;
	s_sq_slider.generic.name	= "sky quality";
	s_sq_slider.minvalue		= 0;
	s_sq_slider.maxvalue		= 3;
	s_sq_slider.curvalue		= 3-trap_Cvar_VariableValue( "r_skymip" );

	s_tf_box.generic.type		= MTYPE_SPINCONTROL;
	s_tf_box.generic.x			= 0;
	s_tf_box.generic.y			= y+=y_offset;
	s_tf_box.generic.name		= "texture filter";
	s_tf_box.itemnames			= filter_names;

	if ( !Q_stricmp( trap_Cvar_VariableString( "gl_texturemode" ), "GL_LINEAR_MIPMAP_NEAREST" ) )
		s_tf_box.curvalue		= 0;
	else
		s_tf_box.curvalue		= 1;

	s_lighting_box.generic.type = MTYPE_SPINCONTROL;
	s_lighting_box.generic.x	= 0;
	s_lighting_box.generic.y	= y+=y_offset;
	s_lighting_box.generic.name	= "lighting";
	s_lighting_box.curvalue		= trap_Cvar_VariableValue( "r_vertexlight" );
	s_lighting_box.itemnames	= lighting_names;
	y+=y_offset;

	s_defaults_action.generic.type = MTYPE_ACTION;
	s_defaults_action.generic.name = "reset to defaults";
	s_defaults_action.generic.flags	= QMF_CENTERED;
	s_defaults_action.generic.x    = 0;
	s_defaults_action.generic.y    = y+=y_offset;
	s_defaults_action.generic.callback = ResetDefaults;
	
	s_apply_action.generic.type		= MTYPE_ACTION;
	s_apply_action.generic.flags	= QMF_CENTERED;
	s_apply_action.generic.name		= "apply changes";
	s_apply_action.generic.x		= 0;
	s_apply_action.generic.y		= y+=y_offset;
	s_apply_action.generic.callback = ApplyChanges;

	Menu_AddItem( &s_video_menu, ( void * ) &s_glext_action );
	Menu_AddItem( &s_video_menu, ( void * ) &s_ref_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_mode_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_screensize_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_brightness_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_fs_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_colordepth_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_detailtextures_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_tq_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_sq_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_tf_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_lighting_box );

	Menu_AddItem( &s_video_menu, ( void * ) &s_defaults_action );
	Menu_AddItem( &s_video_menu, ( void * ) &s_apply_action );

	Menu_Center( &s_video_menu );

	Menu_Init ( &s_video_menu );
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
	M_PushMenu( &s_video_menu, Video_MenuDraw, Video_MenuKey );
}
