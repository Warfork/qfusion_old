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
#include <string.h>
#include <ctype.h>

#include "ui_local.h"

static void	 Field_Init( menufield_s *f );
static void	 Action_DoEnter( menuaction_s *a );
static void	 Action_Draw( menuaction_s *a );
static void  Menu_DrawStatusBar( const char *string );
void Menu_AdjustRectangle ( int *mins, int *maxs );
static void	 Menulist_DoEnter( menulist_s *l );
static void	 MenuList_Draw( menulist_s *l );
static void	 Separator_Draw( menuseparator_s *s );
static void	 Slider_DoSlide( menuslider_s *s, int dir );
static void	 Slider_Draw( menuslider_s *s );
static void	 SpinControl_DoEnter( menulist_s *s );
static void	 SpinControl_Draw( menulist_s *s );
static void	 SpinControl_DoSlide( menulist_s *s, int dir );

#define RCOLUMN_OFFSET  BIG_CHAR_WIDTH
#define LCOLUMN_OFFSET -BIG_CHAR_WIDTH

/*
===============================================================================

STRINGS DRAWING

===============================================================================
*/

/*
================
UI_DrawNonPropChar

Draws one graphics character with 0 being transparent.
================
*/
void UI_DrawNonPropChar ( int x, int y, int num, int fontstyle, vec4_t color )
{
	float	frow, fcol;
	int		width, height;

	num &= 255;
	
	if ( (num&127) == 32 )
		return;		// space

	if ( fontstyle & FONT_BIG ) {
		width = BIG_CHAR_WIDTH;
		height = BIG_CHAR_HEIGHT;
	} else if ( fontstyle & FONT_GIANT ) {
		width = GIANT_CHAR_WIDTH;
		height = GIANT_CHAR_HEIGHT;
	} else {	// FONT_SMALL is default
		width = SMALL_CHAR_WIDTH;
		height = SMALL_CHAR_HEIGHT;
	}

	if (y <= -height)
		return;			// totally off screen

	frow = (num>>4)*0.0625f;
	fcol = (num&15)*0.0625f;

	if ( fontstyle & FONT_SHADOWED ) {
		trap_R_DrawStretchPic ( x+2, y+2, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, colorBlack, uis.charsetShader );
	}
	trap_R_DrawStretchPic ( x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, color, uis.charsetShader );
}

/*
=============
UI_DrawNonPropString
=============
*/
void UI_DrawNonPropString ( int x, int y, char *str, int fontstyle, vec4_t color )
{
	int		num;
	float	frow, fcol;
	int		width, height;
	vec4_t	scolor;

	if ( fontstyle & FONT_BIG ) {
		width = BIG_CHAR_WIDTH;
		height = BIG_CHAR_HEIGHT;
	} else if ( fontstyle & FONT_GIANT ) {
		width = GIANT_CHAR_WIDTH;
		height = GIANT_CHAR_HEIGHT;
	} else {	// FONT_SMALL is default
		width = SMALL_CHAR_WIDTH;
		height = SMALL_CHAR_HEIGHT;
	}

	if ( y <= -height ) {
		return;			// totally off screen
	}

	Vector4Copy ( color, scolor );

	while (*str) {
		if ( Q_IsColorString( str ) ) {
			VectorCopy ( color_table[ColorIndex(str[1])], scolor );
			str += 2;
			continue;
		}

		num = *str++;
		num &= 255;
	
		if ( (num&127) == 32 ) {
			x += width;
			continue;		// space
		}

		frow = (num>>4)*0.0625f;
		fcol = (num&15)*0.0625f;

		if ( fontstyle & FONT_SHADOWED ) {
			trap_R_DrawStretchPic ( x+2, y+2, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, colorBlack, uis.charsetShader );
		}
		trap_R_DrawStretchPic ( x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, scolor, uis.charsetShader );

		x += width;
	}
}

//=============================================================================
//
//	Variable width (proportional) fonts
//

typedef struct {
	qbyte x, y, width;
} propchardesc_t;

static const propchardesc_t propfont1[] =
{
	{0, 0, 0},		// space
	{10, 122, 9},	// !
	{153, 181, 16},	// "
	{55, 122, 17},	// #
	{78, 122, 20},	// $
	{100, 122, 24},	// %
	{152, 122, 18},	// &
	{133, 181, 7},	// '
	{206, 122, 10},	// (
	{230, 122, 10},	// )
	{177, 122, 18},	// *
	{30, 152, 18},	// +
	{85, 181, 7},	// ,
	{34, 94, 11},	// -
	{110, 181, 7},	// .
	{130, 152, 14},	// /

	{21, 64, 19},	// 0
	{41, 64, 13},	// 1
	{57, 64, 19},	// 2
	{77, 64, 20},	// 3
	{97, 64, 21},	// 4
	{119, 64, 20},	// 5
	{140, 64, 20},	// 6
	{204, 64, 16},	// 7
	{161, 64, 19},	// 8
	{182, 64, 19},	// 9
	{59, 181, 7},	// :
	{35, 181, 7},	// ;
	{203, 152, 15},	// <
	{57, 94, 14},	// =
	{228, 152, 15},	// >
	{177, 181, 19},	// ?

	{28, 122, 23},	// @
	{4, 4, 20},		// A
	{26, 4, 20},	// B
	{47, 4, 20},	// C
	{68, 4, 20},	// D
	{89, 4, 14},	// E
	{105, 4, 14},	// F
	{120, 4, 20},	// G
	{142, 4, 19},	// H
	{163, 4, 10},	// I
	{174, 4, 18},	// J
	{194, 4, 20},	// K
	{215, 4, 13},	// L
	{229, 4, 25},	// M
	{5, 34, 20},	// N
	{26, 34, 20},	// O

	{47, 34, 19},	// P
	{67, 34, 20},	// Q
	{89, 34, 19},	// R
	{110, 34, 19},	// S
	{130, 34, 14},	// T
	{145, 34, 20},	// U
	{166, 34, 19},	// V
	{186, 34, 28},	// W
	{214, 34, 20},	// X
	{234, 34, 19},	// Y
	{4, 64, 15},	// Z
	{59, 152, 9},	// [
	{105, 152, 14},	// backslash
	{82, 152, 9},	// ]
	{128, 122, 17},	// ^
	{4, 152, 21},	// _

	{9, 94, 8},		// `
	{153, 152, 14},	// {
	{9, 181, 10},	// |
	{179, 152, 14},	// }
	{79, 94, 17},	// ~
};


/*
=============
UI_DrawPropString
=============
*/
void UI_DrawPropString ( int x, int y, char *str, int fontstyle, vec4_t color )
{
	int		num;
	vec4_t	scolor;
	float	frow, fcol;
	int		width, height;
	float	scale, sheight, swidth, spacing;

	height = PROP_CHAR_HEIGHT;

	if ( y <= -height )
		return;			// totally off screen

	if ( fontstyle & FONT_SMALL ) {
		scale = PROP_SMALL_SCALE;
		spacing = PROP_SMALL_SPACING;
	} else {
		scale = PROP_BIG_SCALE;
		spacing = PROP_BIG_SPACING;
	}

	sheight = height * scale;

	Vector4Copy ( color, scolor );

	while (*str) {
		if ( Q_IsColorString( str ) ) {
			VectorCopy ( color_table[ColorIndex(str[1])], scolor );
			str += 2;
			continue;
		}

		num = toupper(*str++);

		if ( num == 32 ) {
			x += 11 * scale;
			continue;		// space
		}

		if (num >= '{' && num <= '~')
			num -= '{' - 'a';

		num -= 32;

		if ((unsigned)num >= sizeof(propfont1)/sizeof(propfont1[0]))
			continue;

		fcol = propfont1[num].x * (1/256.0f);
		frow = propfont1[num].y * (1/256.0f);
		width = propfont1[num].width;
		swidth = width * scale;

		if ( fontstyle & FONT_SHADOWED )
			trap_R_DrawStretchPic ( x+2, y+2, swidth, sheight, fcol, frow, fcol + width/256.0f, frow + height*(1/256.0f), colorBlack, uis.propfontShader );
		trap_R_DrawStretchPic ( x, y, swidth, sheight, fcol, frow, fcol + width/256.0f, frow + height*(1/256.0f), scolor, uis.propfontShader );

		x += swidth + spacing;
	}
}


/*
=============
UI_PropStringLength

Calculate total width of a string, for centering text on the screen
=============
*/
int UI_PropStringLength ( char *str, int fontstyle )
{
	int		num;
	float	scale, swidth, spacing;
	int		x;

	if ( fontstyle & FONT_SMALL ) {
		scale = PROP_SMALL_SCALE;
		spacing = PROP_SMALL_SPACING;
	} else {
		scale = PROP_BIG_SCALE;
		spacing = PROP_BIG_SPACING;
	}

	x = 0;

	while (*str) {
		if ( Q_IsColorString( str ) ) {
			str += 2;
			continue;
		}

		num = toupper(*str++);

		if ( num == 32 ) {
			x += 11 * scale;
			continue;		// space
		}

		if (num >= '{' && num <= '~')
			num -= '{' - 'a';

		num -= 32;

		if ((unsigned)num >= sizeof(propfont1)/sizeof(propfont1[0]))
			continue;

		swidth = propfont1[num].width * scale;

		x += swidth + spacing;
	}

	return x;
}


/*
=============
UI_FillRect
=============
*/
void UI_FillRect ( int x, int y, int w, int h, vec4_t color ) {
	trap_R_DrawStretchPic ( x, y, w, h, 0, 0, 1, 1, color, uis.whiteShader );
}

/*
=============
UI_FontstyleForFlags
=============
*/
int UI_FontstyleForFlags ( int flags )
{
	int fontStyle = 0;

	if ( flags & QMF_GIANT ) {
		fontStyle |= FONT_GIANT;
	} else {
		fontStyle |= FONT_SMALL;
	}

	return fontStyle;
}

/*
=============
UI_NonPropStringLength
=============
*/
int UI_NonPropStringLength ( char *s )
{
	int len = 0;

	while ( *s && *s != '\n' ) {
		if ( Q_IsColorString (s) ) {
			s += 2;
		} else {
			len++;
			s++;
		}
	}

	return len;
}

/*
=============
UI_StringWidth
=============
*/
int UI_StringWidth ( int flags, char *s )
{
	int fontStyle = UI_FontstyleForFlags ( flags );

	if ( flags & QMF_NONPROPORTIONAL ) {
		if ( fontStyle & FONT_BIG ) {
			return UI_NonPropStringLength (s) * BIG_CHAR_WIDTH;
		} else if ( fontStyle & FONT_GIANT ) {
			return UI_NonPropStringLength (s) * GIANT_CHAR_WIDTH;
		}

		// FONT_SMALL is default
		return UI_NonPropStringLength (s) * SMALL_CHAR_WIDTH;
	}

	return UI_PropStringLength ( s, fontStyle );
}

/*
=============
UI_StringHeight
=============
*/
int UI_StringHeight ( int flags )
{
	int fontStyle = UI_FontstyleForFlags ( flags );

	if ( flags & QMF_NONPROPORTIONAL ) {
		if ( fontStyle & FONT_BIG ) {
			return BIG_CHAR_HEIGHT;
		} else if ( fontStyle & FONT_GIANT ) {
			return GIANT_CHAR_HEIGHT;
		}

		// FONT_SMALL is default
		return SMALL_CHAR_HEIGHT;
	}

	if ( fontStyle & FONT_GIANT ) {
		return PROP_CHAR_HEIGHT * PROP_BIG_SCALE;
	}

	return PROP_CHAR_HEIGHT * PROP_SMALL_SCALE;
}

/*
=============
UI_StringHeightOffset
=============
*/
int UI_StringHeightOffset ( int flags )
{
	int fontStyle = UI_FontstyleForFlags ( flags );

	if ( flags & QMF_NONPROPORTIONAL ) {
		if ( fontStyle & FONT_BIG ) {
			return BIG_CHAR_HEIGHT - 3;
		} else if ( fontStyle & FONT_GIANT ) {
			return GIANT_CHAR_HEIGHT - 4;
		}

		// FONT_SMALL is default
		return SMALL_CHAR_HEIGHT;
	}

	if ( fontStyle & FONT_GIANT ) {
		return PROP_CHAR_HEIGHT * PROP_BIG_SCALE - 3;
	}

	return PROP_CHAR_HEIGHT * PROP_SMALL_SCALE - 2;
}

/*
=============
UI_AdjustStringPosition
=============
*/
int UI_AdjustStringPosition ( int flags, char *s )
{
	int dx = 0;

	if ( flags & QMF_CENTERED ) {
		dx -= UI_StringWidth ( flags, s ) / 2;
	} else if ( !(flags & QMF_LEFT_JUSTIFY) ) {
		dx -= UI_StringWidth ( flags, s );
	}

	return dx;
}

/*
=============
UI_DrawString
=============
*/
void UI_DrawString ( int x, int y, char *str, int flags, int fsm, vec4_t color )
{
	x += UI_AdjustStringPosition ( flags, str );

	if ( flags & QMF_NONPROPORTIONAL ) {
		UI_DrawNonPropString ( x, y, str, UI_FontstyleForFlags (flags) | fsm, color );
	} else {
		UI_DrawPropString ( x, y, str, UI_FontstyleForFlags (flags) | fsm, color );
	}
}

//=================================================================

void Action_DoEnter( menuaction_s *a )
{
	if ( a->generic.callback )
		a->generic.callback( a );
}

void Action_Draw( menuaction_s *a )
{
	int x, y;
	float *color;

	x = a->generic.x + a->generic.parent->x;
	y = a->generic.y + a->generic.parent->y;

	if ( Menu_ItemAtCursor( a->generic.parent ) == a ) {
		color = colorRed;
		if ( a->generic.flags & QMF_GRAYED ) {
			color = colorBlue;
		}

		UI_DrawString ( x, y, ( char * )a->generic.name, a->generic.flags, FONT_SHADOWED, color );
	} else {
		color = colorWhite;
		if ( a->generic.flags & QMF_GRAYED ) {
			color = colorYellow;
		}

		UI_DrawString ( x, y, ( char * )a->generic.name, a->generic.flags, 0, color );
	}

	if ( a->generic.ownerdraw )
		a->generic.ownerdraw( a );
}

qboolean Field_DoEnter( menufield_s *f )
{
	if ( f->generic.callback )
	{
		f->generic.callback( f );
		return qtrue;
	}
	return qfalse;
}

void Field_Draw( menufield_s *f )
{
	int x, y;
	char tempbuffer[128] = "";

	x = f->generic.x + f->generic.parent->x + LCOLUMN_OFFSET;
	y = f->generic.y + f->generic.parent->y;

	if ( f->generic.name ) {
		UI_DrawString ( x, y, ( char * )f->generic.name, f->generic.flags, 0, colorWhite );
	}

	x = f->generic.x + f->generic.parent->x + 16;
	y = f->generic.y + f->generic.parent->y;

	{
		float color[4] = { 0.5, 0.5, 0.5, 0.5 };
		UI_FillRect ( x, y, SMALL_CHAR_WIDTH * f->visible_length, SMALL_CHAR_HEIGHT, color );
	}

	if ( Menu_ItemAtCursor( f->generic.parent ) == f )
	{
		int offset, len;

		if ( f->visible_offset )
			offset = f->visible_length;
		else
			offset = f->cursor;

		len = 0;

		if ( offset > 0 ) {
			strncpy( tempbuffer, f->buffer, offset - 1 );
			len += UI_NonPropStringLength ( tempbuffer ) * SMALL_CHAR_WIDTH;
			UI_DrawString ( x + len, y, tempbuffer, QMF_NONPROPORTIONAL, 0, colorWhite );

			strncpy( tempbuffer, f->buffer + offset - 1, 1 );
			tempbuffer[1] = 0;
			len += UI_NonPropStringLength ( tempbuffer ) * SMALL_CHAR_WIDTH;
			UI_DrawString ( x + len, y, tempbuffer, QMF_NONPROPORTIONAL, FONT_SHADOWED, colorWhite );
	
			if ( offset < f->visible_length ) {
				strncpy( tempbuffer, f->buffer + offset, f->visible_length - offset );
				len += UI_NonPropStringLength ( tempbuffer ) * SMALL_CHAR_WIDTH;
				UI_DrawString ( x, y, tempbuffer, QMF_NONPROPORTIONAL, 0, colorWhite );
			}
		} else {
			strncpy( tempbuffer, f->buffer + f->visible_offset, f->visible_length );
			len += UI_NonPropStringLength ( tempbuffer ) * SMALL_CHAR_WIDTH;
			UI_DrawString ( x + len, y, tempbuffer, QMF_NONPROPORTIONAL, 0, colorWhite );
		}
	}
	else
	{
		x = f->generic.x + f->generic.parent->x + 16;
		y = f->generic.y + f->generic.parent->y;

		strncpy( tempbuffer, f->buffer + f->visible_offset, f->visible_length );
		UI_DrawString ( x + UI_NonPropStringLength ( tempbuffer ) * SMALL_CHAR_WIDTH, y, tempbuffer, QMF_NONPROPORTIONAL, 0, colorWhite );
	}
}

qboolean Field_Key( menufield_s *f, int key )
{
	switch ( key )
	{
	case K_KP_SLASH:
		key = '/';
		break;
	case K_KP_MINUS:
		key = '-';
		break;
	case K_KP_PLUS:
		key = '+';
		break;
	case K_KP_HOME:
		key = '7';
		break;
	case K_KP_UPARROW:
		key = '8';
		break;
	case K_KP_PGUP:
		key = '9';
		break;
	case K_KP_LEFTARROW:
		key = '4';
		break;
	case K_KP_5:
		key = '5';
		break;
	case K_KP_RIGHTARROW:
		key = '6';
		break;
	case K_KP_END:
		key = '1';
		break;
	case K_KP_DOWNARROW:
		key = '2';
		break;
	case K_KP_PGDN:
		key = '3';
		break;
	case K_KP_INS:
		key = '0';
		break;
	case K_KP_DEL:
		key = '.';
		break;
	}

	if ( key > 127 )
	{
		switch ( key )
		{
		case K_DEL:
		default:
			return qfalse;
		}
	}

	/*
	** support pasting from the clipboard
	*/
	if ( ( toupper( key ) == 'V' && trap_Key_IsDown (K_CTRL) ) ||
		 ( ( ( key == K_INS ) || ( key == K_KP_INS ) ) && trap_Key_IsDown (K_SHIFT) ) )
	{
		char cbd[64];

		trap_CL_GetClipboardData ( cbd, sizeof(cbd) );

		if ( cbd[0] )
		{
			strtok( cbd, "\n\r\b" );

			strncpy( f->buffer, cbd, f->length - 1 );
			f->cursor = strlen( f->buffer );
			f->visible_offset = f->cursor - f->visible_length;
			if ( f->visible_offset < 0 )
				f->visible_offset = 0;
		}
		return qtrue;
	}

	switch ( key )
	{
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
	case K_BACKSPACE:
		if ( f->cursor > 0 )
		{
			memmove( &f->buffer[f->cursor-1], &f->buffer[f->cursor], strlen( &f->buffer[f->cursor] ) + 1 );
			f->cursor--;

			if ( f->visible_offset )
			{
				f->visible_offset--;
			}
		}
		break;

	case K_KP_DEL:
	case K_DEL:
		memmove( &f->buffer[f->cursor], &f->buffer[f->cursor+1], strlen( &f->buffer[f->cursor+1] ) + 1 );
		break;

	case K_ESCAPE:
	case K_MOUSE1:
	case K_MOUSE2:
	case K_KP_ENTER:
	case K_ENTER:
	case K_TAB:
		return qfalse;

	case K_SPACE:
	default:
		if ( !isdigit( key ) && ( f->generic.flags & QMF_NUMBERSONLY ) )
			return qfalse;

		if ( f->cursor < f->length )
		{
			f->buffer[f->cursor++] = key;
			f->buffer[f->cursor] = 0;

			if ( f->cursor > f->visible_length )
			{
				f->visible_offset++;
			}

			Field_Init ( f );
			Menu_AdjustRectangle ( f->generic.mins, f->generic.maxs );
		}
	}

	return qtrue;
}

void Menu_AddItem( menuframework_s *menu, void *item )
{
	if ( menu->nitems == 0 )
		menu->nslots = 0;

	if ( menu->nitems < MAXMENUITEMS )
	{
		menu->items[menu->nitems] = item;
		( ( menucommon_s * ) menu->items[menu->nitems] )->parent = menu;
		menu->nitems++;
	}

	menu->nslots = Menu_TallySlots( menu );
}

/*
** Menu_AdjustCursor
**
** This function takes the given menu, the direction, and attempts
** to adjust the menu's cursor so that it's at the next available
** slot.
*/
void Menu_AdjustCursor( menuframework_s *m, int dir )
{
	menucommon_s *citem;

	/*
	** see if it's in a valid spot
	*/
	if ( m->cursor >= 0 && m->cursor < m->nitems )
	{
		if ( ( citem = Menu_ItemAtCursor( m ) ) != 0 )
		{
			if ( citem->type != MTYPE_SEPARATOR )
				return;
		}
	}

	/*
	** it's not in a valid spot, so crawl in the direction indicated until we
	** find a valid spot
	*/
	if ( dir == 1 )
	{
		while ( 1 )
		{
			citem = Menu_ItemAtCursor( m );
			if ( citem )
				if ( citem->type != MTYPE_SEPARATOR )
					break;
			m->cursor += dir;
			if ( m->cursor >= m->nitems )
				m->cursor = 0;
		}
	}
	else
	{
		while ( 1 )
		{
			citem = Menu_ItemAtCursor( m );
			if ( citem )
				if ( citem->type != MTYPE_SEPARATOR )
					break;
			m->cursor += dir;
			if ( m->cursor < 0 )
				m->cursor = m->nitems - 1;
		}
	}
}

void Menu_Center( menuframework_s *menu )
{
	int height;

	height = ( ( menucommon_s * ) menu->items[menu->nitems-1])->y;
	height += 10;

	menu->y = ( uis.vidHeight - height ) / 2;
}

static void Field_Init( menufield_s *f )
{
	f->generic.mins[0] = f->generic.x + f->generic.parent->x + 16;
	f->generic.maxs[0] = f->generic.mins[0] + UI_StringWidth ( f->generic.flags, f->buffer );

	f->generic.mins[1] = f->generic.y + f->generic.parent->y;
	f->generic.maxs[1] = f->generic.mins[1] + UI_StringHeight ( f->generic.flags );
}

static void	Action_Init( menuaction_s *a )
{
	a->generic.mins[0] = a->generic.x + a->generic.parent->x + UI_AdjustStringPosition ( a->generic.flags, ( char * )a->generic.name );
	a->generic.maxs[0] = a->generic.mins[0] + UI_StringWidth ( a->generic.flags, ( char * )a->generic.name );

	a->generic.mins[1] = a->generic.y + a->generic.parent->y;
	a->generic.maxs[1] = a->generic.mins[1] + UI_StringHeight ( a->generic.flags );
}

static void	MenuList_Init( menulist_s *l )
{
	char **n;
	int xsize, ysize, len, spacing;

	n = l->itemnames;

	if ( !n )
		return;

	l->generic.mins[0] = l->generic.x + l->generic.parent->x + LCOLUMN_OFFSET - 
		UI_StringWidth ( l->generic.flags, ( char * )l->generic.name );
	l->generic.mins[1] = l->generic.y + l->generic.parent->y;

	ysize = 0;
	xsize = 0;
	spacing = UI_StringHeightOffset ( l->generic.flags );

	while ( *n )
	{
		len = UI_StringWidth ( l->generic.flags, *n  );
		xsize = max ( xsize, len );
		ysize += spacing;

		*n++;
	}

	l->generic.maxs[0] = l->generic.mins[0] + xsize;
	l->generic.maxs[1] = l->generic.mins[1] + ysize;
}

static void Separator_Init( menuseparator_s *s )
{

}

static void Slider_Init( menuslider_s *s )
{
	s->generic.flags |= QMF_LEFT_JUSTIFY;

	s->generic.mins[0] = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET - UI_AdjustStringPosition ( s->generic.flags, ( char * )s->generic.name );
	s->generic.maxs[0] = s->generic.mins[0] + SLIDER_RANGE * SMALL_CHAR_WIDTH + SMALL_CHAR_WIDTH;

	s->generic.mins[1] = s->generic.y + s->generic.parent->y;
	s->generic.maxs[1] = s->generic.mins[1] + SMALL_CHAR_HEIGHT;
}

static void SpinControl_Init( menulist_s *s )
{
	char buffer[100] = { 0 };
	int ysize, xsize, spacing, len;
	char **n;

	n = s->itemnames;

	if ( !n )
		return;

	s->generic.mins[0] = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET;
	s->generic.mins[1] = s->generic.y + s->generic.parent->y;

	ysize = UI_StringHeight ( s->generic.flags );
	spacing = UI_StringHeightOffset ( s->generic.flags );

	xsize = 0;

	while ( *n )
	{
		if ( !strchr( *n, '\n' ) )
		{
			len = UI_StringWidth ( s->generic.flags, *n );
			xsize = max ( xsize, len );
		}
		else
		{

			strcpy( buffer, *n );
			*strchr( buffer, '\n' ) = 0;
			len = UI_StringWidth ( s->generic.flags, buffer );
			xsize = max ( xsize, len );

			ysize = ysize + spacing;
			strcpy( buffer, strchr( *n, '\n' ) + 1 );
			len = UI_StringWidth ( s->generic.flags, buffer );
			xsize = max ( xsize, len );
		}

		*n++;
	}

	if ( s->generic.flags & QMF_CENTERED ) {
		s->generic.mins[0] -= xsize / 2;
	}

	s->generic.maxs[0] = s->generic.mins[0] + xsize;
	s->generic.maxs[1] = s->generic.mins[1] + ysize;
}

void Menu_AdjustRectangle ( int *mins, int *maxs )
{
	mins[0] *= uis.scaleX;
	maxs[0] *= uis.scaleX;

	mins[1] *= uis.scaleY;
	maxs[1] *= uis.scaleY;
}

void Menu_Init( menuframework_s *menu )
{
	int i;

	/*
	** init items
	*/
	for ( i = 0; i < menu->nitems; i++ )
	{
		switch ( ( ( menucommon_s * ) menu->items[i] )->type )
		{
		case MTYPE_FIELD:
			Field_Init( ( menufield_s * ) menu->items[i] );
			break;
		case MTYPE_SLIDER:
			Slider_Init( ( menuslider_s * ) menu->items[i] );
			break;
		case MTYPE_LIST:
			MenuList_Init( ( menulist_s * ) menu->items[i] );
			break;
		case MTYPE_SPINCONTROL:
			SpinControl_Init( ( menulist_s * ) menu->items[i] );
			break;
		case MTYPE_ACTION:
			Action_Init( ( menuaction_s * ) menu->items[i] );
			break;
		case MTYPE_SEPARATOR:
			Separator_Init( ( menuseparator_s * ) menu->items[i] );
			break;
		}

		Menu_AdjustRectangle ( ( ( menucommon_s * ) menu->items[i] )->mins,
			( ( menucommon_s * ) menu->items[i] )->maxs );
	}
}

void Menu_Draw( menuframework_s *menu )
{
	int i;
	menucommon_s *item;

	/*
	** draw contents
	*/
	for ( i = 0; i < menu->nitems; i++ )
	{
		switch ( ( ( menucommon_s * ) menu->items[i] )->type )
		{
		case MTYPE_FIELD:
			Field_Draw( ( menufield_s * ) menu->items[i] );
			break;
		case MTYPE_SLIDER:
			Slider_Draw( ( menuslider_s * ) menu->items[i] );
			break;
		case MTYPE_LIST:
			MenuList_Draw( ( menulist_s * ) menu->items[i] );
			break;
		case MTYPE_SPINCONTROL:
			SpinControl_Draw( ( menulist_s * ) menu->items[i] );
			break;
		case MTYPE_ACTION:
			Action_Draw( ( menuaction_s * ) menu->items[i] );
			break;
		case MTYPE_SEPARATOR:
			Separator_Draw( ( menuseparator_s * ) menu->items[i] );
			break;
		}
	}

	item = Menu_ItemAtCursor( menu );

	if ( item && item->cursordraw )	{
		item->cursordraw( item );
	} else if ( menu->cursordraw ) {
		menu->cursordraw( menu );
	}

	if ( item ) {
		if ( item->statusbarfunc )
			item->statusbarfunc( ( void * ) item );
		else if ( item->statusbar )
			Menu_DrawStatusBar( item->statusbar );
		else if ( menu->statusbar )
			Menu_DrawStatusBar( menu->statusbar );
	} else if ( menu->statusbar )  {
		Menu_DrawStatusBar( menu->statusbar );
	}
}

void Menu_DrawStatusBar( const char *string )
{
	int maxrow, maxcol, col;
	int l = strlen( string );

	maxrow = uis.vidHeight / SMALL_CHAR_HEIGHT;
	maxcol = uis.vidWidth / SMALL_CHAR_WIDTH;
	col = (maxcol >> 1) - l / 2;

	UI_FillRect ( 0, uis.vidHeight-SMALL_CHAR_HEIGHT, uis.vidWidth, SMALL_CHAR_HEIGHT, colorDkGrey );
	UI_DrawNonPropString ( col*SMALL_CHAR_WIDTH, uis.vidHeight - SMALL_CHAR_HEIGHT, ( char * )string, FONT_SMALL, colorWhite );
}

void *Menu_ItemAtCursor( menuframework_s *m )
{
	if ( m->cursor < 0 || m->cursor >= m->nitems )
		return 0;

	return m->items[m->cursor];
}

qboolean Menu_SelectItem( menuframework_s *s )
{
	menucommon_s *item = ( menucommon_s * ) Menu_ItemAtCursor( s );

	if ( item )
	{
		switch ( item->type )
		{
		case MTYPE_FIELD:
			return Field_DoEnter( ( menufield_s * ) item ) ;
		case MTYPE_ACTION:
			Action_DoEnter( ( menuaction_s * ) item );
			return qtrue;
		case MTYPE_LIST:
//			Menulist_DoEnter( ( menulist_s * ) item );
			return qfalse;
		case MTYPE_SPINCONTROL:
//			SpinControl_DoEnter( ( menulist_s * ) item );
			return qfalse;
		}
	}
	return qfalse;
}

void Menu_SetStatusBar( menuframework_s *m, const char *string )
{
	m->statusbar = string;
}

qboolean Menu_SlideItem( menuframework_s *s, int dir )
{
	menucommon_s *item = ( menucommon_s * ) Menu_ItemAtCursor( s );

	if ( item )
	{
		switch ( item->type )
		{
		case MTYPE_SLIDER:
			Slider_DoSlide( ( menuslider_s * ) item, dir );
			return qtrue;
		case MTYPE_SPINCONTROL:
			SpinControl_DoSlide( ( menulist_s * ) item, dir );
			return qtrue;
		}
	}

	return qfalse;
}

int Menu_TallySlots( menuframework_s *menu )
{
	int i;
	int total = 0;

	for ( i = 0; i < menu->nitems; i++ )
	{
		if ( ( ( menucommon_s * ) menu->items[i] )->type == MTYPE_LIST )
		{
			int nitems = 0;
			char **n = ( ( menulist_s * ) menu->items[i] )->itemnames;

			while (*n)
				nitems++, n++;

			total += nitems;
		}
		else
		{
			total++;
		}
	}

	return total;
}

void Menulist_DoEnter( menulist_s *l )
{
	int start;

	start = l->generic.y / 10 + 1;

	l->curvalue = l->generic.parent->cursor - start;

	if ( l->generic.callback )
		l->generic.callback( l );
}

void MenuList_Draw( menulist_s *l )
{
	char **n;
	int x, y;

	x = l->generic.x + l->generic.parent->x + LCOLUMN_OFFSET - 
		UI_StringWidth ( l->generic.flags, ( char * )l->generic.name );
	y = l->generic.y + l->generic.parent->y;

	UI_DrawString ( x, y, ( char * )l->generic.name, l->generic.flags, 0, colorYellow );

	n = l->itemnames;

  	UI_FillRect ( l->generic.x - 112 + l->generic.parent->x, l->generic.parent->y + l->generic.y + l->curvalue*10 + 10, 128, 10, colorDkGrey );
	while ( *n )
	{
		UI_DrawPropString ( x, y +=  UI_StringHeightOffset (l->generic.flags), *n++, 0, colorYellow );
	}
}

void Separator_Draw( menuseparator_s *s )
{
	int x, y;

	if ( !s->generic.name || !s->generic.name[0] ) {
		return;
	}

	x = s->generic.x + s->generic.parent->x;
	y = s->generic.y + s->generic.parent->y;

	UI_DrawString ( x, y, ( char * )s->generic.name, s->generic.flags, 0, colorYellow );
}

void Slider_DoSlide( menuslider_s *s, int dir )
{
	s->curvalue += dir;

	if ( s->curvalue > s->maxvalue )
		s->curvalue = s->maxvalue;
	else if ( s->curvalue < s->minvalue )
		s->curvalue = s->minvalue;

	if ( s->generic.callback )
		s->generic.callback( s );
}

void Slider_Draw( menuslider_s *s )
{
	int	i;
	int x, y, w;
	int fontStyle;
	float *color;

	fontStyle = FONT_SMALL;
	x = s->generic.x + s->generic.parent->x;
	y = s->generic.y + s->generic.parent->y;
	w = SMALL_CHAR_WIDTH;

	UI_DrawString ( x + LCOLUMN_OFFSET, y, ( char *)s->generic.name, 0, 0, colorYellow );

	s->range = ( s->curvalue - s->minvalue );
	s->range /= ( float ) ( s->maxvalue - s->minvalue );
	clamp ( s->range, 0, 1 );

	color = colorWhite;
	if ( Menu_ItemAtCursor( s->generic.parent ) == s )
	{
		color = colorRed;
		fontStyle |= FONT_SHADOWED;
	}

	x = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET;
	UI_DrawNonPropChar( x, y, 128, fontStyle, colorWhite);
	for ( i = 0; i < SLIDER_RANGE; i++ )
		UI_DrawNonPropChar( x + i * w + w, y, 129, fontStyle, colorWhite);
	UI_DrawNonPropChar( x + i * w + w, y, 130, fontStyle, colorWhite);
	UI_DrawNonPropChar( x + ( int )(SLIDER_RANGE-1) * w * s->range + w, y, 131, fontStyle, color);
}

void SpinControl_DoEnter( menulist_s *s )
{
	s->curvalue++;
	if ( s->itemnames[s->curvalue] == 0 )
		s->curvalue = 0;

	if ( s->generic.callback )
		s->generic.callback( s );
}

void SpinControl_DoSlide( menulist_s *s, int dir )
{
	s->curvalue += dir;

	if ( s->curvalue < 0 )
		s->curvalue = 0;
	else if ( s->itemnames[s->curvalue] == 0 )
		s->curvalue--;

	if ( s->generic.callback )
		s->generic.callback( s );
}

void SpinControl_Draw( menulist_s *s )
{
	char buffer[100];
	int x, y;
	int fsm;
	float *color;

	x = s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET;
	y = s->generic.y + s->generic.parent->y;

	if ( s->generic.name )
	{
		UI_DrawString ( x, y, ( char *)s->generic.name, s->generic.flags, 0, colorYellow );
	}

	if ( s->generic.flags & QMF_NOITEMNAMES )
		return;

	fsm = 0;
	color = colorWhite;
	if ( Menu_ItemAtCursor( s->generic.parent ) == s )
	{
		color = colorRed;
		fsm = FONT_SHADOWED;
	}

	x = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET;
	y = s->generic.y + s->generic.parent->y;

	if ( !strchr( s->itemnames[s->curvalue], '\n' ) )
	{
		UI_DrawString ( x, y, ( char *)s->itemnames[s->curvalue], QMF_LEFT_JUSTIFY | (s->generic.flags & QMF_NONPROPORTIONAL), fsm, color );
	}
	else
	{

		strcpy( buffer, s->itemnames[s->curvalue] );
		*strchr( buffer, '\n' ) = 0;
		UI_DrawString ( x, y, buffer, s->generic.flags, fsm, color );

		strcpy( buffer, strchr( s->itemnames[s->curvalue], '\n' ) + 1 );
		UI_DrawString ( x, y + UI_StringHeightOffset (s->generic.flags), buffer, QMF_LEFT_JUSTIFY | (s->generic.flags & QMF_NONPROPORTIONAL), fsm, color );
	}
}

