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

static menuframework_s	s_gfx_menu;

static menuseparator_s	s_gfx_title;

static menulist_s		s_crosshair_box;

static menulist_s		s_skyquality_list;
static menulist_s		s_dlights_list;
static menulist_s		s_lightflares_list;
static menulist_s		s_thirdperson_list;
static menulist_s  		s_finish_box;
static menulist_s  		s_marks_list;

static void				*s_crosshair_pic;

static void CrosshairFunc( void *unused )
{
	s_crosshair_pic = trap_RegisterPic( va ("gfx/2d/crosshair%s", s_crosshair_box.itemnames[s_crosshair_box.curvalue]));
	trap_Cvar_SetValue( "crosshair", s_crosshair_box.curvalue );
}

static void UpdateSkyQualityFunc( void *unused )
{
	trap_Cvar_SetValue( "r_fastsky", !s_skyquality_list.curvalue );
}

static void UpdateDynamicLightsFunc( void *unused )
{
	trap_Cvar_SetValue( "r_dynamiclight", s_dlights_list.curvalue );
}

static void UpdateLightFlaresFunc( void *unused )
{
	trap_Cvar_SetValue( "r_flares", s_lightflares_list.curvalue );
}

static void UpdateThirdPersonFunc( void *unused )
{
	trap_Cvar_SetValue( "cl_thirdperson", s_thirdperson_list.curvalue );
}

static void UpdateFinishFunc( void *unused )
{
	trap_Cvar_SetValue( "gl_finish", s_finish_box.curvalue );
}

static void UpdateMarksFunc( void *unused )
{
	trap_Cvar_SetValue( "cl_decals", s_marks_list.curvalue );
}

void Gfx_MenuInit( void )
{
	static const char *crosshair_names[] =
	{
		"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", 0
	};

	static const char *sky_quality_items[] =
	{
		"fast", "high quality", 0
	};

	static const char *dlights_items[] =
	{
		"off", "nice", "fast", 0
	};

	static const char *lightflares_items[] =
	{
		"off", "on", 0
	};

	static const char *thirdperson_items[] =
	{
		"off", "on", 0
	};

	static const char *finish_names[] =
	{
		"no", "yes", 0
	};

	static const char *marks_names[] =
	{
		"off", "on", 0
	};

	int w, h;
	int y = 0;
	int y_offset = PROP_SMALL_HEIGHT - 2;

	/*
	** configure controls menu and menu items
	*/
	w = uis.vidWidth;
	h = uis.vidHeight;

	s_gfx_menu.x = w / 2;
	s_gfx_menu.nitems = 0;

	s_gfx_title.generic.type = MTYPE_SEPARATOR;
	s_gfx_title.generic.name = "Graphics Options";
	s_gfx_title.generic.flags = QMF_CENTERED|QMF_GIANT;
	s_gfx_title.generic.x    = 0;
	s_gfx_title.generic.y	 = y;
	y+=y_offset;

	s_crosshair_box.generic.type		= MTYPE_SPINCONTROL;
	s_crosshair_box.generic.flags		= QMF_NOITEMNAMES;
	s_crosshair_box.generic.x			= 0;
	s_crosshair_box.generic.y			= y += y_offset;
	s_crosshair_box.generic.name		= "crosshair";
	s_crosshair_box.generic.callback	= CrosshairFunc;
	s_crosshair_box.itemnames			= crosshair_names;
	y += y_offset;

	trap_Cvar_SetValue( "crosshair", M_ClampCvar( 0, NUM_CROSSHAIRS, trap_Cvar_VariableValue("crosshair") ) );
	s_crosshair_box.curvalue		= trap_Cvar_VariableValue("crosshair");
	clamp ( s_crosshair_box.curvalue, 0, 9 );
	s_crosshair_pic = trap_RegisterPic( va ("gfx/2d/crosshair%s", s_crosshair_box.itemnames[s_crosshair_box.curvalue]));

	s_skyquality_list.generic.type			= MTYPE_SPINCONTROL;
	s_skyquality_list.generic.x				= 0;
	s_skyquality_list.generic.y				= y += y_offset;
	s_skyquality_list.generic.name			= "sky";
	s_skyquality_list.generic.callback		= UpdateSkyQualityFunc;
	s_skyquality_list.itemnames				= sky_quality_items;
	s_skyquality_list.curvalue				= !(int)trap_Cvar_VariableValue( "r_fastsky" );
	clamp ( s_skyquality_list.curvalue, 0, 1 );

	s_dlights_list.generic.type				= MTYPE_SPINCONTROL;
	s_dlights_list.generic.x				= 0;
	s_dlights_list.generic.y				= y += y_offset;
	s_dlights_list.generic.name				= "dynamic lights";
	s_dlights_list.generic.callback			= UpdateDynamicLightsFunc;
	s_dlights_list.itemnames				= dlights_items;
	s_dlights_list.curvalue					= trap_Cvar_VariableValue( "r_dynamiclight" );
	clamp ( s_dlights_list.curvalue, 0, 2 );

	s_lightflares_list.generic.type			= MTYPE_SPINCONTROL;
	s_lightflares_list.generic.x			= 0;
	s_lightflares_list.generic.y			= y += y_offset;
	s_lightflares_list.generic.name			= "light flares";
	s_lightflares_list.generic.callback		= UpdateLightFlaresFunc;
	s_lightflares_list.itemnames			= lightflares_items;
	s_lightflares_list.curvalue				= trap_Cvar_VariableValue( "r_flares" );
	clamp ( s_lightflares_list.curvalue, 0, 1 );

	s_thirdperson_list.generic.type			= MTYPE_SPINCONTROL;
	s_thirdperson_list.generic.x			= 0;
	s_thirdperson_list.generic.y			= y += y_offset;
	s_thirdperson_list.generic.name			= "third person view";
	s_thirdperson_list.generic.callback		= UpdateThirdPersonFunc;
	s_thirdperson_list.itemnames			= thirdperson_items;
	s_thirdperson_list.curvalue				= trap_Cvar_VariableValue( "cl_thirdperson" );
	clamp ( s_thirdperson_list.curvalue, 0, 1 );

	s_finish_box.generic.type				= MTYPE_SPINCONTROL;
	s_finish_box.generic.x					= 0;
	s_finish_box.generic.y					= y+=y_offset;
	s_finish_box.generic.name				= "sync every frame";
	s_finish_box.curvalue					= trap_Cvar_VariableValue( "gl_finish" );
	s_finish_box.itemnames					= finish_names;
	clamp ( s_finish_box.curvalue, 0, 1 );

	s_marks_list.generic.type				= MTYPE_SPINCONTROL;
	s_marks_list.generic.x					= 0;
	s_marks_list.generic.y					= y+=y_offset;
	s_marks_list.generic.name				= "marks on walls";
	s_marks_list.curvalue					= trap_Cvar_VariableValue( "cl_decals" );
	s_marks_list.itemnames					= marks_names;
	clamp ( s_marks_list.curvalue, 0, 1 );

	Menu_AddItem( &s_gfx_menu, ( void * ) &s_gfx_title );
	Menu_AddItem( &s_gfx_menu, ( void * ) &s_crosshair_box );
	Menu_AddItem( &s_gfx_menu, ( void * ) &s_skyquality_list );
	Menu_AddItem( &s_gfx_menu, ( void * ) &s_dlights_list );
	Menu_AddItem( &s_gfx_menu, ( void * ) &s_lightflares_list );
	Menu_AddItem( &s_gfx_menu, ( void * ) &s_thirdperson_list );
	Menu_AddItem( &s_gfx_menu, ( void * ) &s_finish_box );
	Menu_AddItem( &s_gfx_menu, ( void * ) &s_marks_list );

	Menu_Center( &s_gfx_menu );

	Menu_Init ( &s_gfx_menu );
}

void Gfx_MenuDraw (void)
{
	Menu_AdjustCursor( &s_gfx_menu, 1 );

	trap_DrawStretchPic ( (int)(BIG_CHAR_WIDTH*1.2) + s_gfx_menu.x + s_crosshair_box.generic.x,
		s_gfx_menu.y + s_crosshair_box.generic.y - BIG_CHAR_WIDTH / 2, 32, 32, 
		0, 0, 1, 1, colorWhite, s_crosshair_pic );

	Menu_Draw( &s_gfx_menu );
}

const char *Gfx_MenuKey( int key )
{
	return Default_MenuKey( &s_gfx_menu, key );
}

void M_Menu_Gfx_f (void)
{
	Gfx_MenuInit();
	M_PushMenu ( &s_gfx_menu, Gfx_MenuDraw, Gfx_MenuKey );
}
