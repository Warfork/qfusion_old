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

/*
=======================================================================

SOUND MENU

=======================================================================
*/

static menuframework_s	s_sound_menu;

static menuseparator_s	s_sound_title;

static menuslider_s		s_sound_sfxvolume_slider;
static menuslider_s		s_sound_musicvolume_slider;
static menulist_s		s_sound_quality_list;
static menulist_s		s_sound_compatibility_list;

static void SoundSetMenuItemValues( void )
{
	s_sound_sfxvolume_slider.curvalue	= trap_Cvar_VariableValue( "s_volume" ) * 10;
	s_sound_musicvolume_slider.curvalue = trap_Cvar_VariableValue( "s_musicvolume" ) * 10;
}

static void UpdateSfxVolumeFunc( void *unused )
{
	trap_Cvar_SetValue( "s_volume", s_sound_sfxvolume_slider.curvalue * 0.1f );
}

static void UpdateMusicVolumeFunc( void *unused )
{
	trap_Cvar_SetValue( "s_musicvolume", s_sound_musicvolume_slider.curvalue * 0.1f );
}

static void UpdateSoundCompatibilityFunc( void *unused )
{
	trap_Cvar_SetValue( "s_primary", s_sound_compatibility_list.curvalue );

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Restarting the sound system. This" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );

	// the text box won't show up unless we do a buffer swap
	trap_EndFrame();

	trap_Cmd_ExecuteText (EXEC_APPEND, "snd_restart\n");
	trap_Cmd_Execute();
}

static void UpdateSoundQualityFunc( void *unused )
{
	trap_Cvar_SetValue( "s_khz", (1<<(int)(s_sound_quality_list.curvalue))*11 );

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Restarting the sound system. This" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );

	// the text box won't show up unless we do a buffer swap
	trap_EndFrame();

	trap_Cmd_ExecuteText (EXEC_APPEND, "snd_restart\n");
	trap_Cmd_Execute();
}

void Sound_MenuInit( void )
{
	static const char *quality_items[] =
	{
		"low", "medium", "high", 0
	};

	static const char *compatibility_items[] =
	{
		"max compatibility", "max performance", 0
	};

	int w, h;
	int y = 0;
	int y_offset = BIG_CHAR_HEIGHT - 2;

	/*
	** configure controls menu and menu items
	*/
	trap_Vid_GetCurrentInfo ( &w, &h );

	s_sound_menu.x = w / 2;
	s_sound_menu.y = h / 2 - 88;
	s_sound_menu.nitems = 0;

	s_sound_title.generic.type = MTYPE_SEPARATOR;
	s_sound_title.generic.name = "Sound Options";
	s_sound_title.generic.x    = 48;
	s_sound_title.generic.y	 = y;
	y+=y_offset;

	s_sound_sfxvolume_slider.generic.type	= MTYPE_SLIDER;
	s_sound_sfxvolume_slider.generic.x		= 0;
	s_sound_sfxvolume_slider.generic.y		= y+=y_offset;
	s_sound_sfxvolume_slider.generic.name	= "effects volume";
	s_sound_sfxvolume_slider.generic.callback	= UpdateSfxVolumeFunc;
	s_sound_sfxvolume_slider.minvalue		= 0;
	s_sound_sfxvolume_slider.maxvalue		= 10;
	s_sound_sfxvolume_slider.curvalue		= trap_Cvar_VariableValue( "s_volume" ) * 10;

	s_sound_musicvolume_slider.generic.type	= MTYPE_SLIDER;
	s_sound_musicvolume_slider.generic.x	= 0;
	s_sound_musicvolume_slider.generic.y	= y += y_offset;
	s_sound_musicvolume_slider.generic.name	= "music volume";
	s_sound_musicvolume_slider.generic.callback	= UpdateMusicVolumeFunc;
	s_sound_musicvolume_slider.minvalue		= 0;
	s_sound_musicvolume_slider.maxvalue		= 10;
	s_sound_musicvolume_slider.curvalue 	= trap_Cvar_VariableValue( "s_musicvolume" ) * 10;

	s_sound_quality_list.generic.type		= MTYPE_SPINCONTROL;
	s_sound_quality_list.generic.x			= 0;
	s_sound_quality_list.generic.y			= y += y_offset;
	s_sound_quality_list.generic.name		= "sound quality";
	s_sound_quality_list.generic.callback	= UpdateSoundQualityFunc;
	s_sound_quality_list.itemnames			= quality_items;
	s_sound_quality_list.curvalue			= (int)trap_Cvar_VariableValue( "s_khz" ) / 11;

	if ( s_sound_quality_list.curvalue == 4 ) {
		s_sound_quality_list.curvalue = 2;
	} else if ( s_sound_quality_list.curvalue == 2 ) {
		s_sound_quality_list.curvalue = 1;
	} else {
		s_sound_quality_list.curvalue = 0;
	}

	s_sound_compatibility_list.generic.type	= MTYPE_SPINCONTROL;
	s_sound_compatibility_list.generic.x	= 0;
	s_sound_compatibility_list.generic.y	= y += y_offset;
	s_sound_compatibility_list.generic.name	= "sound compatibility";
	s_sound_compatibility_list.generic.callback = UpdateSoundCompatibilityFunc;
	s_sound_compatibility_list.itemnames	= compatibility_items;
	s_sound_compatibility_list.curvalue		= trap_Cvar_VariableValue( "s_primary" );
	y += y_offset;

	SoundSetMenuItemValues();

	Menu_AddItem( &s_sound_menu, ( void * ) &s_sound_title );
	Menu_AddItem( &s_sound_menu, ( void * ) &s_sound_sfxvolume_slider );
	Menu_AddItem( &s_sound_menu, ( void * ) &s_sound_musicvolume_slider );
	Menu_AddItem( &s_sound_menu, ( void * ) &s_sound_quality_list );
	Menu_AddItem( &s_sound_menu, ( void * ) &s_sound_compatibility_list );
}

void Sound_MenuDraw (void)
{
	Menu_AdjustCursor( &s_sound_menu, 1 );
	Menu_Draw( &s_sound_menu );
}

const char *Sound_MenuKey( int key )
{
	return Default_MenuKey( &s_sound_menu, key );
}

void M_Menu_Sound_f (void)
{
	Sound_MenuInit();
	M_PushMenu ( Sound_MenuDraw, Sound_MenuKey );
}
