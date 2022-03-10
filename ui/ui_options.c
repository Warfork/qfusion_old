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
static menulist_s		s_options_joystick_box;
static menulist_s		s_options_console_action;

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

static void ConsoleFunc( void *unused )
{
	if (!trap_Con_Func ())
		return;

	M_ForceMenuOff ();

	trap_CL_SetKeyDest_f ( key_console );
}

void Options_MenuInit( void )
{
	static const char *yesno_names[] =
	{
		"no",
		"yes",
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

	Menu_AddItem( &s_options_menu, ( void * ) &s_options_customize_options_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_defaults_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_console_action );
}

void Options_MenuDraw (void)
{
	Menu_AdjustCursor( &s_options_menu, 1 );
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
