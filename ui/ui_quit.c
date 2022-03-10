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

QUIT MENU

=======================================================================
*/
static menuframework_s	s_quit_menu;

static menuseparator_s	s_quit_title;
static menuaction_s		s_yes_action;
static menuaction_s		s_no_action;

void YesFunc ( void *unused )
{
	trap_CL_SetKeyDest_f ( key_console );
	trap_CL_Quit_f ();
}

void NoFunc ( void *unused )
{
	M_PopMenu ();
}

void M_QuitInit( void )
{
	int y = 0;
	int y_offset = PROP_SMALL_HEIGHT - 2;

	s_quit_menu.x = trap_GetWidth() * 0.50;
	s_quit_menu.nitems = 0;

	s_quit_title.generic.type		= MTYPE_SEPARATOR;
	s_quit_title.generic.name		= "Are you sure?";
	s_quit_title.generic.flags		= QMF_CENTERED;
	s_quit_title.generic.x			= 0;
	s_quit_title.generic.y			= y;
	y += y_offset;

	s_yes_action.generic.type		= MTYPE_ACTION;
	s_yes_action.generic.flags		= QMF_CENTERED;
	s_yes_action.generic.x			= 0;
	s_yes_action.generic.y			= y+=y_offset;
	s_yes_action.generic.name		= "YES";
	s_yes_action.generic.callback	= YesFunc;

	s_no_action.generic.type		= MTYPE_ACTION;
	s_no_action.generic.flags		= QMF_CENTERED;
	s_no_action.generic.x			= 0;
	s_no_action.generic.y			= y+=y_offset;
	s_no_action.generic.name		= "NO";
	s_no_action.generic.callback	= NoFunc;

	Menu_AddItem( &s_quit_menu, ( void *) &s_quit_title );
	Menu_AddItem( &s_quit_menu, ( void *) &s_yes_action );
	Menu_AddItem( &s_quit_menu, ( void *) &s_no_action );

	Menu_Center( &s_quit_menu );

	Menu_Init ( &s_quit_menu );
}

const char *M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		M_PopMenu ();
		return NULL;

	case 'Y':
	case 'y':
		trap_CL_SetKeyDest_f ( key_console );
		trap_CL_Quit_f ();
		return NULL;

	default:
		break;
	}

	return Default_MenuKey( &s_quit_menu, key );
}


void M_Quit_Draw (void)
{
	Menu_AdjustCursor( &s_quit_menu, 1 );
	Menu_Draw( &s_quit_menu );
}

void M_Menu_Quit_f (void)
{
	M_QuitInit ();
	M_PushMenu ( &s_quit_menu, M_Quit_Draw, M_Quit_Key );
}

