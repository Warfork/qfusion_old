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
=============================================================================

END GAME MENU

=============================================================================
*/
static int credits_start_time;
static const char **credits;
static char *creditsIndex[256];
static char *creditsBuffer;

static const char *idcredits[] =
{
	"+" S_COLOR_RED "PROGRAMMING",
	"Victor 'Vic' Luchits",
	"",
	"+" S_COLOR_RED "ADDITIONAL PROGRAMMING",
	"Anton 'Tonik' Gavrilov",
	"",
	"+" S_COLOR_RED "LINUX PORT",
	"Bobakitoo",
	"",
	"+" S_COLOR_RED "SPECIAL THANKS",
	"(in alphabetical order)",
	"",
	"Andrew Tridgell",
	"Bram 'Brambo' Stein",
	"EvilTypeGuy",
	"Forest 'LordHavoc' Hale",
	"Martin Kraus",
	"Mathias Heyer",
	"MrG",
	"Nate Miller",
	"Robert 'Heffo' Heffernan",
	"Stephen Taylor",
	"Tim Ferguson",
	0
};

void M_Credits_MenuDraw( void )
{
	int i, x, y;
	int w, h, length;
	int y_offset = UI_StringHeightOffset ( 0 );
	char *s;

	/*
	** draw the credits
	*/
	w = uis.vidWidth;
	h = uis.vidHeight;

	y = h - ( ( uis.time - credits_start_time ) * 0.025f );

	for ( i = 0; credits[i] && y < h; y += y_offset, i++ )
	{
		int stringoffset;
		int bold;

		if ( credits[i][0] == '+' )
		{
			bold = qtrue;
			stringoffset = 1;
		}
		else
		{
			bold = qfalse;
			stringoffset = 0;
		}

		s = ( char * )&credits[i][stringoffset];
		if ( Q_IsColorString (s) ) {
			length = strlen (s) - 2;
		} else {
			length = strlen (s);
		}

		if ( bold ) {
			x = ( w - length * BIG_CHAR_WIDTH ) / 2;
			UI_DrawNonPropString( x, y, s, FONT_BIG|FONT_SHADOWED, colorWhite );
		} else {
			x = ( w - length * SMALL_CHAR_WIDTH ) / 2;
			UI_DrawNonPropString( x, y, s, FONT_SMALL, colorWhite );
		}
	}

	if ( y < 0 )
		credits_start_time = uis.time;
}

const char *M_Credits_Key( int key )
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		if (creditsBuffer)
			trap_FS_FreeFile (creditsBuffer);
		M_PopMenu ();
		break;
	}

	return menu_out_sound;

}

void M_Menu_Credits_f( void )
{
	int		n;
	int		count;
	char	*p;
	int		isdeveloper = 0;

	creditsBuffer = NULL;
	count = trap_FS_LoadFile ( "credits", (void **)&creditsBuffer );
	if (count != -1)
	{
		p = creditsBuffer;
		for (n = 0; n < 255; n++)
		{
			creditsIndex[n] = p;
			while (*p != '\r' && *p != '\n')
			{
				p++;
				if (--count == 0)
					break;
			}
			if (*p == '\r')
			{
				*p++ = 0;
				if (--count == 0)
					break;
			}
			*p++ = 0;
			if (--count == 0)
				break;
		}
		creditsIndex[++n] = 0;
		credits = creditsIndex;
	} else {
		credits = idcredits;	
	}

	credits_start_time = uis.time;
	M_PushMenu( NULL, M_Credits_MenuDraw, M_Credits_Key);
}