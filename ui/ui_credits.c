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

static const char *qfcredits[] =
{
	"+" "^1" "q" "^2" "f" "^3" "u" "^4" "s" "^5" "i" "^6" "o" "^7" "n",
	"http://hkitchen.quakesrc.org/",
	"",
	"+" S_COLOR_RED "PROGRAMMING",
	"Victor 'Vic' Luchits",
	"",
	"+" S_COLOR_RED "ADDITIONAL PROGRAMMING",
	"Anton 'Tonik' Gavrilov",
	"",
	"+" S_COLOR_RED "LINUX PORT ASSISTANCE",
	"Bobakitoo",
	"",
	"+" S_COLOR_RED "SPECIAL THANKS",
	"(in alphabetical order)",
	"",
	"Andrew Tridgell",
	"Andrey '[SkulleR]' Nazarov",
	"Bram 'Brambo' Stein",
	"DarkOne",
	"Dmitri 'Zindahsh' Kolesnikov",
	"EvilTypeGuy",
	"Forest 'LordHavoc' Hale",
	"Jalisko",
	"Martin Kraus",
	"Mathias Heyer",
	"MrG",
	"Nate Miller",
	"Robert 'Heffo' Heffernan",
	"Stephen Taylor",
	"Tim Ferguson",
	"{wf}shadowspawn",
	"",
	"+" S_COLOR_RED "ORIGINAL CODE BY ID SOFTWARE",
	"",
	"+" S_COLOR_RED "PROGRAMMING",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	0
};

void M_Credits_MenuDraw( void )
{
	int i, x, y, j;
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
		length = strlen( s );

		for( j = 0; s[j]; j++ ) {
			if( Q_IsColorString( &s[j] ) ) {
				length -= 2;
				j++;
			}
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
		if (creditsBuffer) {
			UI_MemFree( creditsBuffer );
			creditsBuffer = NULL;
		}
		M_PopMenu ();
		break;
	}

	return menu_out_sound;

}

void M_Menu_Credits_f( void )
{
	int		n, f;
	int		count;
	char	*p;
	int		isdeveloper = 0;

	creditsBuffer = NULL;
	count = trap_FS_FOpenFile( "credits", &f, FS_READ );
	if (count > 0)
	{
		creditsBuffer = UI_Malloc( count + 1 );
		trap_FS_Read( creditsBuffer, count, f );
		trap_FS_FCloseFile( f );

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
		credits = ( const char **)creditsIndex;
	} else if( count == 0 ) {
		trap_FS_FCloseFile( f );
	} else {
		credits = qfcredits;	
	}

	credits_start_time = uis.time;
	M_PushMenu( NULL, M_Credits_MenuDraw, M_Credits_Key);
}
