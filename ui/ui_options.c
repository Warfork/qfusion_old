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

void M_Menu_Keys_f (void);

/*
=======================================================================

CONTROLS MENU

=======================================================================
*/
static menuframework_s	s_options_menu;

static menuseparator_s	s_options_title;

static menuaction_s		s_options_defaults_action;
static menuaction_s		s_options_customize_options_action;
static menuslider_s		s_options_sensitivity_slider;
static menulist_s		s_options_freelook_box;
static menulist_s		s_options_noalttab_box;
static menulist_s		s_options_alwaysrun_box;
static menulist_s		s_options_invertmouse_box;
static menulist_s		s_options_lookspring_box;
static menulist_s		s_options_lookstrafe_box;
static menulist_s		s_options_crosshair_box;
static menuslider_s		s_options_sfxvolume_slider;
static menuslider_s		s_options_musicvolume_slider;
static menulist_s		s_options_joystick_box;
static menulist_s		s_options_compatibility_list;
static menulist_s		s_options_console_action;

static void CrosshairFunc( void *unused )
{
	trap_Cvar_SetValue( "crosshair", s_options_crosshair_box.curvalue );
}

static void JoystickFunc( void *unused )
{
	trap_Cvar_SetValue( "in_joystick", s_options_joystick_box.curvalue );
}

static void CustomizeControlsFunc( void *unused )
{
	M_Menu_Keys_f();
}

static void AlwaysRunFunc( void *unused )
{
	trap_Cvar_SetValue( "cl_run", s_options_alwaysrun_box.curvalue );
}

static void FreeLookFunc( void *unused )
{
	trap_Cvar_SetValue( "freelook", s_options_freelook_box.curvalue );
}

static void MouseSpeedFunc( void *unused )
{
	trap_Cvar_SetValue( "sensitivity", s_options_sensitivity_slider.curvalue / 2.0F );
}

static void NoAltTabFunc( void *unused )
{
	trap_Cvar_SetValue( "win_noalttab", s_options_noalttab_box.curvalue );
}

static void ControlsSetMenuItemValues( void )
{
	s_options_sfxvolume_slider.curvalue		= trap_Cvar_VariableValue( "s_volume" ) * 10;
	s_options_musicvolume_slider.curvalue 	= trap_Cvar_VariableValue( "s_musicvolume" ) * 10;
	s_options_sensitivity_slider.curvalue	= ( trap_Cvar_VariableValue("sensitivity") ) * 2;

	trap_Cvar_SetValue( "cl_run", M_ClampCvar( 0, 1, trap_Cvar_VariableValue("cl_run") ) );
	s_options_alwaysrun_box.curvalue		= trap_Cvar_VariableValue("cl_run");

	s_options_invertmouse_box.curvalue		= trap_Cvar_VariableValue("m_pitch") < 0;

	trap_Cvar_SetValue( "lookspring", M_ClampCvar( 0, 1, trap_Cvar_VariableValue("lookspring") ) );
	s_options_lookspring_box.curvalue		= trap_Cvar_VariableValue("lookspring");

	trap_Cvar_SetValue( "lookstrafe", M_ClampCvar( 0, 1, trap_Cvar_VariableValue("lookstrafe") ) );
	s_options_lookstrafe_box.curvalue		= trap_Cvar_VariableValue("lookstrafe");

	trap_Cvar_SetValue( "freelook", M_ClampCvar( 0, 1, trap_Cvar_VariableValue("freelook") ) );
	s_options_freelook_box.curvalue			= trap_Cvar_VariableValue("freelook");

	trap_Cvar_SetValue( "crosshair", M_ClampCvar( 0, NUM_CROSSHAIRS, trap_Cvar_VariableValue("crosshair") ) );
	s_options_crosshair_box.curvalue		= trap_Cvar_VariableValue("crosshair");

	trap_Cvar_SetValue( "in_joystick", M_ClampCvar( 0, 1, trap_Cvar_VariableValue("in_joystick") ) );
	s_options_joystick_box.curvalue		= trap_Cvar_VariableValue("in_joystick");

	s_options_noalttab_box.curvalue			= trap_Cvar_VariableValue ("win_noalttab");
}

static void ControlsResetDefaultsFunc( void *unused )
{
	trap_Cmd_ExecuteText (EXEC_APPEND, "exec default.cfg\n");
	trap_Cmd_Execute();

	ControlsSetMenuItemValues();
}

static void InvertMouseFunc( void *unused )
{
	trap_Cvar_SetValue( "m_pitch", -trap_Cvar_VariableValue ("m_pitch") );
}

static void LookspringFunc( void *unused )
{
	trap_Cvar_SetValue( "lookspring", trap_Cvar_VariableValue ("lookspring") );
}

static void LookstrafeFunc( void *unused )
{
	trap_Cvar_SetValue( "lookstrafe", !-trap_Cvar_VariableValue ("lookstrafe") );
}

static void UpdateSfxVolumeFunc( void *unused )
{
	trap_Cvar_SetValue( "s_volume", s_options_sfxvolume_slider.curvalue * 0.1f );
}

static void UpdateMusicVolumeFunc( void *unused )
{
	trap_Cvar_SetValue( "s_musicvolume", s_options_musicvolume_slider.curvalue * 0.1f );
}

static void ConsoleFunc( void *unused )
{
	if (!trap_Con_Func ())
		return;

	M_ForceMenuOff ();

	trap_CL_SetKeyDest_f ( key_console );
}

static void UpdateSoundQualityFunc( void *unused )
{
	trap_Cvar_SetValue( "s_primary", s_options_compatibility_list.curvalue );

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Restarting the sound system. This" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );

	// the text box won't show up unless we do a buffer swap
	trap_EndFrame();

	trap_CL_Snd_Restart_f();
}

void Options_MenuInit( void )
{
	static const char *cd_music_items[] =
	{
		"disabled",
		"enabled",
		0
	};
	static const char *quality_items[] =
	{
		"low", "high", 0
	};

	static const char *compatibility_items[] =
	{
		"max compatibility", "max performance", 0
	};

	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *crosshair_names[] =
	{
		"a",
		"b",
		"c",
		"d",
		"e",
		"f",
		"g",
		"h",
		"i",
		"j",
		0
	};

	int w, h;
	int y = 0;
	int y_offset = BIG_CHAR_HEIGHT - 2;

	/*
	** configure controls menu and menu items
	*/
	trap_Vid_GetCurrentInfo ( &w, &h );

	s_options_menu.x = w / 2;
	s_options_menu.y = h / 2 - 88;
	s_options_menu.nitems = 0;

	s_options_title.generic.type = MTYPE_SEPARATOR;
	s_options_title.generic.name = "Options";
	s_options_title.generic.x    = 48;
	s_options_title.generic.y	 = y;
	y+=y_offset;

	s_options_sfxvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_sfxvolume_slider.generic.x	= 0;
	s_options_sfxvolume_slider.generic.y	= y+=y_offset;
	s_options_sfxvolume_slider.generic.name	= "effects volume";
	s_options_sfxvolume_slider.generic.callback	= UpdateSfxVolumeFunc;
	s_options_sfxvolume_slider.minvalue		= 0;
	s_options_sfxvolume_slider.maxvalue		= 10;
	s_options_sfxvolume_slider.curvalue		= trap_Cvar_VariableValue( "s_volume" ) * 10;

	s_options_musicvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_musicvolume_slider.generic.x		= 0;
	s_options_musicvolume_slider.generic.y		= y += y_offset;
	s_options_musicvolume_slider.generic.name	= "music volume";
	s_options_musicvolume_slider.generic.callback	= UpdateMusicVolumeFunc;
	s_options_musicvolume_slider.minvalue		= 0;
	s_options_musicvolume_slider.maxvalue		= 10;
	s_options_musicvolume_slider.curvalue 		= trap_Cvar_VariableValue( "s_musicvolume" ) * 10;

	s_options_compatibility_list.generic.type	= MTYPE_SPINCONTROL;
	s_options_compatibility_list.generic.x		= 0;
	s_options_compatibility_list.generic.y		= y += y_offset;
	s_options_compatibility_list.generic.name	= "sound compatibility";
	s_options_compatibility_list.generic.callback = UpdateSoundQualityFunc;
	s_options_compatibility_list.itemnames		= compatibility_items;
	s_options_compatibility_list.curvalue		= trap_Cvar_VariableValue( "s_primary" );
	y += y_offset;

	s_options_sensitivity_slider.generic.type	= MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x		= 0;
	s_options_sensitivity_slider.generic.y		= y += y_offset;
	s_options_sensitivity_slider.generic.name	= "mouse speed";
	s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
	s_options_sensitivity_slider.minvalue		= 2;
	s_options_sensitivity_slider.maxvalue		= 22;

	s_options_alwaysrun_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_alwaysrun_box.generic.x			= 0;
	s_options_alwaysrun_box.generic.y			= y += y_offset;
	s_options_alwaysrun_box.generic.name		= "always run";
	s_options_alwaysrun_box.generic.callback	= AlwaysRunFunc;
	s_options_alwaysrun_box.itemnames			= yesno_names;

	s_options_invertmouse_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_invertmouse_box.generic.x			= 0;
	s_options_invertmouse_box.generic.y			= y += y_offset;
	s_options_invertmouse_box.generic.name		= "invert mouse";
	s_options_invertmouse_box.generic.callback	= InvertMouseFunc;
	s_options_invertmouse_box.itemnames			= yesno_names;

	s_options_lookspring_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_lookspring_box.generic.x			= 0;
	s_options_lookspring_box.generic.y			= y += y_offset;
	s_options_lookspring_box.generic.name		= "lookspring";
	s_options_lookspring_box.generic.callback	= LookspringFunc;
	s_options_lookspring_box.itemnames			= yesno_names;

	s_options_lookstrafe_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_lookstrafe_box.generic.x			= 0;
	s_options_lookstrafe_box.generic.y			= y += y_offset;
	s_options_lookstrafe_box.generic.name		= "lookstrafe";
	s_options_lookstrafe_box.generic.callback	= LookstrafeFunc;
	s_options_lookstrafe_box.itemnames			= yesno_names;

	s_options_freelook_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_freelook_box.generic.x			= 0;
	s_options_freelook_box.generic.y			= y += y_offset;
	s_options_freelook_box.generic.name			= "free look";
	s_options_freelook_box.generic.callback		= FreeLookFunc;
	s_options_freelook_box.itemnames			= yesno_names;

#ifdef _WIN32
	s_options_noalttab_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_noalttab_box.generic.x			= 0;
	s_options_noalttab_box.generic.y			= y += y_offset;
	s_options_noalttab_box.generic.name			= "disable alt-tab";
	s_options_noalttab_box.generic.callback		= NoAltTabFunc;
	s_options_noalttab_box.itemnames			= yesno_names;
#endif

	s_options_joystick_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_joystick_box.generic.x			= 0;
	s_options_joystick_box.generic.y			= y += y_offset;
	s_options_joystick_box.generic.name			= "use joystick";
	s_options_joystick_box.generic.callback		= JoystickFunc;
	s_options_joystick_box.itemnames			= yesno_names;
	y += y_offset;

	s_options_crosshair_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.flags		= QMF_NOITEMNAMES;
	s_options_crosshair_box.generic.x			= 0;
	s_options_crosshair_box.generic.y			= y += y_offset;
	s_options_crosshair_box.generic.name		= "crosshair";
	s_options_crosshair_box.generic.callback	= CrosshairFunc;
	s_options_crosshair_box.itemnames			= crosshair_names;
	y += y_offset;

	s_options_customize_options_action.generic.type	= MTYPE_ACTION;
	s_options_customize_options_action.generic.x		= 0;
	s_options_customize_options_action.generic.y		= y += y_offset;
	s_options_customize_options_action.generic.name		= "customize controls";
	s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

	s_options_defaults_action.generic.type		= MTYPE_ACTION;
	s_options_defaults_action.generic.x			= 0;
	s_options_defaults_action.generic.y			= y += y_offset;
	s_options_defaults_action.generic.name		= "reset defaults";
	s_options_defaults_action.generic.callback	= ControlsResetDefaultsFunc;

	s_options_console_action.generic.type		= MTYPE_ACTION;
	s_options_console_action.generic.x			= 0;
	s_options_console_action.generic.y			= y += y_offset;
	s_options_console_action.generic.name		= "go to console";
	s_options_console_action.generic.callback	= ConsoleFunc;

	ControlsSetMenuItemValues();

	Menu_AddItem( &s_options_menu, ( void * ) &s_options_title );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sfxvolume_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_musicvolume_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_compatibility_list );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sensitivity_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_alwaysrun_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_invertmouse_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookspring_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookstrafe_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_freelook_box );

#ifdef _WIN32
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_noalttab_box );
#endif

	Menu_AddItem( &s_options_menu, ( void * ) &s_options_joystick_box );

	Menu_AddItem( &s_options_menu, ( void * ) &s_options_crosshair_box );

	Menu_AddItem( &s_options_menu, ( void * ) &s_options_customize_options_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_defaults_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_console_action );
}

void Options_MenuDraw (void)
{
	Menu_AdjustCursor( &s_options_menu, 1 );

	trap_DrawPic ( BIG_CHAR_WIDTH*1.2 + s_options_menu.x + s_options_crosshair_box.generic.x,
		s_options_menu.y + s_options_crosshair_box.generic.y - BIG_CHAR_WIDTH / 2,
		va ("gfx/2d/crosshair%s", s_options_crosshair_box.itemnames[s_options_crosshair_box.curvalue]) );

	Menu_Draw( &s_options_menu );
}

const char *Options_MenuKey( int key )
{
	return Default_MenuKey( &s_options_menu, key );
}

void M_Menu_Options_f (void)
{
	Options_MenuInit();
	M_PushMenu ( Options_MenuDraw, Options_MenuKey );
}
