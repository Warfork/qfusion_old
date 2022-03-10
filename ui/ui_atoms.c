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

void Action_DoEnter( menuaction_s *a )
{
	if ( a->generic.callback )
		a->generic.callback( a );
}

void Action_Draw( menuaction_s *a )
{
	int x, y;
	int fontstyle;
	float *color;

	if ( a->generic.flags & QMF_GIANT ) {
		fontstyle = FONT_GIANT;
	} else {
		fontstyle = FONT_SMALL;
	}

	if ( Menu_ItemAtCursor( a->generic.parent ) == a ) {
		fontstyle |= FONT_SHADOWED;

		color = colorRed;
		if ( a->generic.flags & QMF_GRAYED ) {
			color = colorBlue;
		}
	} else {
		color = colorWhite;
		if ( a->generic.flags & QMF_GRAYED ) {
			color = colorYellow;
		}
	}

	x = a->generic.x + a->generic.parent->x;
	if ( a->generic.flags & QMF_CENTERED ) {
		x -= trap_PropStringLength ( ( char * )a->generic.name, fontstyle ) / 2;
	} else if ( !(a->generic.flags & QMF_LEFT_JUSTIFY) ) {
		x -= trap_PropStringLength ( ( char * )a->generic.name, fontstyle );
	}

	y = a->generic.y + a->generic.parent->y;

	trap_DrawPropString ( x, y, ( char * )a->generic.name, fontstyle, color );

	if ( a->generic.ownerdraw )
		a->generic.ownerdraw( a );
}

qboolean Field_DoEnter( menufield_s *f )
{
	if ( f->generic.callback )
	{
		f->generic.callback( f );
		return true;
	}
	return false;
}

void Field_Draw( menufield_s *f )
{
	char tempbuffer[128] = "";
	int x, y, delta;
	int fontstyle;

	fontstyle = FONT_SMALL;

	x = f->generic.x + f->generic.parent->x + LCOLUMN_OFFSET;
	y = f->generic.y + f->generic.parent->y;

	if ( f->generic.name ) {
		delta = trap_PropStringLength ( ( char * )f->generic.name, fontstyle );
		trap_DrawPropString ( x - delta, y, ( char * )f->generic.name, fontstyle, colorWhite );
	}

	if ( Menu_ItemAtCursor( f->generic.parent ) == f )
	{
		int offset, len;

		fontstyle |= FONT_SHADOWED;

		if ( f->visible_offset )
			offset = f->visible_length;
		else
			offset = f->cursor;

		len = 0;
		x = f->generic.x + f->generic.parent->x + 16;
		y = f->generic.y + f->generic.parent->y;

		if ( offset > 0 ) {
			strncpy( tempbuffer, f->buffer, offset - 1 );
			trap_DrawPropString ( x + len, y, tempbuffer, fontstyle, colorWhite );
			len += trap_PropStringLength ( tempbuffer, fontstyle );

			strncpy( tempbuffer, f->buffer + offset - 1, 1 );
			tempbuffer[1] = 0;
			trap_DrawPropString ( x + len, y, tempbuffer, fontstyle, colorRed );
			len += trap_PropStringLength ( tempbuffer, fontstyle );
	
			if ( offset < f->visible_length ) {
				strncpy( tempbuffer, f->buffer + offset, f->visible_length - offset );
				trap_DrawPropString ( x + len, y, tempbuffer, fontstyle, colorWhite );
			}
		} else {
			strncpy( tempbuffer, f->buffer + f->visible_offset, f->visible_length );
			trap_DrawPropString ( x, y, tempbuffer, fontstyle, colorRed );
		}
	}
	else
	{
		x = f->generic.x + f->generic.parent->x + 16;
		y = f->generic.y + f->generic.parent->y;

		strncpy( tempbuffer, f->buffer + f->visible_offset, f->visible_length );
		trap_DrawPropString ( x, y, tempbuffer, fontstyle, colorWhite );
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
			return false;
		}
	}

	/*
	** support pasting from the clipboard
	*/
/*
	if ( ( toupper( key ) == 'V' && keydown[K_CTRL] ) ||
		 ( ( ( key == K_INS ) || ( key == K_KP_INS ) ) && keydown[K_SHIFT] ) )
	{
		char *cbd;
		
		if ( ( cbd = Sys_GetClipboardData() ) != 0 )
		{
			strtok( cbd, "\n\r\b" );

			strncpy( f->buffer, cbd, f->length - 1 );
			f->cursor = strlen( f->buffer );
			f->visible_offset = f->cursor - f->visible_length;
			if ( f->visible_offset < 0 )
				f->visible_offset = 0;

			Q_free( cbd );
		}
		return true;
	}
*/
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
		return false;

	case K_SPACE:
	default:
		if ( !isdigit( key ) && ( f->generic.flags & QMF_NUMBERSONLY ) )
			return false;

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

	return true;
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
	f->generic.mins[1] = f->generic.y + f->generic.parent->y;

	f->generic.maxs[0] = f->generic.mins[0] + trap_PropStringLength ( f->buffer, FONT_SMALL );
	f->generic.maxs[1] = f->generic.mins[1] + PROP_SMALL_HEIGHT;
}

static void	Action_Init( menuaction_s *a )
{
	int fontstyle;

	if ( a->generic.flags & QMF_GIANT ) {
		fontstyle = FONT_GIANT;
	} else {
		fontstyle = FONT_SMALL;
	}

	a->generic.mins[0] = a->generic.x + a->generic.parent->x;
	if ( a->generic.flags & QMF_CENTERED ) {
		a->generic.mins[0] -= trap_PropStringLength ( ( char * )a->generic.name, fontstyle ) / 2;
	} else if ( !(a->generic.flags & QMF_LEFT_JUSTIFY) ) {
		a->generic.mins[0] -= trap_PropStringLength ( ( char * )a->generic.name, fontstyle );
	}
	a->generic.mins[1] = a->generic.y + a->generic.parent->y;

	a->generic.maxs[0] = a->generic.mins[0] + trap_PropStringLength ( ( char * )a->generic.name, fontstyle );
	a->generic.maxs[1] = a->generic.mins[1] + ((fontstyle == FONT_SMALL) ? PROP_SMALL_HEIGHT : PROP_BIG_HEIGHT);
}

static void	MenuList_Init( menulist_s *l )
{
	const char **n;
	int fontstyle;
	int xsize, ysize, len, spacing;

	n = l->itemnames;

	if ( !n )
		return;

	fontstyle = FONT_SMALL;

	if ( fontstyle & FONT_SMALL ) {
		spacing = PROP_SMALL_HEIGHT + 2;
	} else {
		spacing = PROP_BIG_HEIGHT + 2;
	}

	l->generic.mins[0] = l->generic.x + l->generic.parent->x + LCOLUMN_OFFSET - 
		trap_PropStringLength ( ( char * )l->generic.name, fontstyle );
	l->generic.mins[1] = l->generic.y + l->generic.parent->y;

	ysize = 0;
	xsize = 0;

	while ( *n )
	{
		len = trap_PropStringLength ( ( char * )*n, fontstyle );
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
	int delta;
	int fontstyle;

	fontstyle = FONT_SMALL;
	delta = trap_PropStringLength ( ( char * )s->generic.name, fontstyle );

	s->generic.mins[0] = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET;
	if ( s->generic.flags & QMF_CENTERED ) {
		s->generic.mins[0] += delta / 2;
	}

	s->generic.mins[1] = s->generic.y + s->generic.parent->y;

	s->generic.maxs[0] = s->generic.mins[0] + SLIDER_RANGE * SMALL_CHAR_WIDTH + SMALL_CHAR_WIDTH;
	s->generic.maxs[1] = s->generic.mins[1] + SMALL_CHAR_HEIGHT;
}

static void SpinControl_Init( menulist_s *s )
{
	char buffer[100] = { 0 };
	int fontstyle;
	int ysize, xsize, spacing, len;
	const char **n;

	n = s->itemnames;

	if ( !n )
		return;

	fontstyle = FONT_SMALL;

	s->generic.mins[0] = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET;
	s->generic.mins[1] = s->generic.y + s->generic.parent->y;

	if ( fontstyle == FONT_SMALL ) {
		ysize = PROP_SMALL_HEIGHT;
	} else {
		ysize = PROP_BIG_HEIGHT;
	}
	spacing = ysize + 2;

	xsize = 0;

	while ( *n )
	{
		if ( !strchr( *n, '\n' ) )
		{
			len = trap_PropStringLength ( ( char * )*n, fontstyle );
			xsize = max ( xsize, len );
		}
		else
		{

			strcpy( buffer, *n );
			*strchr( buffer, '\n' ) = 0;
			len = trap_PropStringLength ( buffer, fontstyle );
			xsize = max ( xsize, len );

			ysize = ysize + spacing;
			strcpy( buffer, strchr( *n, '\n' ) + 1 );
			len = trap_PropStringLength ( buffer, fontstyle );
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

	trap_FillRect ( 0, uis.vidHeight-SMALL_CHAR_HEIGHT, uis.vidWidth, SMALL_CHAR_HEIGHT, colorDkGrey );
	trap_DrawString ( col*SMALL_CHAR_WIDTH, uis.vidHeight - SMALL_CHAR_HEIGHT, ( char * )string, FONT_SMALL, colorWhite );
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
			return true;
		case MTYPE_LIST:
//			Menulist_DoEnter( ( menulist_s * ) item );
			return false;
		case MTYPE_SPINCONTROL:
//			SpinControl_DoEnter( ( menulist_s * ) item );
			return false;
		}
	}
	return false;
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
			return true;
		case MTYPE_SPINCONTROL:
			SpinControl_DoSlide( ( menulist_s * ) item, dir );
			return true;
		}
	}

	return false;
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
			const char **n = ( ( menulist_s * ) menu->items[i] )->itemnames;

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
	const char **n;
	int fontstyle; 
	int x, y, spacing;

	fontstyle = FONT_SMALL;

	if ( fontstyle & FONT_SMALL ) {
		spacing = PROP_SMALL_HEIGHT + 2;
	} else {
		spacing = PROP_BIG_HEIGHT + 2;
	}

	x = l->generic.x + l->generic.parent->x + LCOLUMN_OFFSET - 
		trap_PropStringLength ( ( char * )l->generic.name, fontstyle );
	y = l->generic.y + l->generic.parent->y;

	trap_DrawPropString ( x, y, ( char * )l->generic.name, fontstyle, colorYellow );

	n = l->itemnames;

  	trap_FillRect ( l->generic.x - 112 + l->generic.parent->x, l->generic.parent->y + l->generic.y + l->curvalue*10 + 10, 128, 10, colorDkGrey );
	while ( *n )
	{
		trap_DrawPropString ( x, y + spacing, ( char * )*n++, fontstyle, colorYellow );
		y += spacing;
	}
}

void Separator_Draw( menuseparator_s *s )
{
	int x, y, delta;
	int fontstyle;

	if ( !s->generic.name ) {
		return;
	}

	if ( s->generic.flags & QMF_GIANT ) {
		fontstyle = FONT_GIANT;
	} else {
		fontstyle = FONT_SMALL;
	}

	delta = trap_PropStringLength ( ( char * )s->generic.name, fontstyle );

	x = s->generic.x + s->generic.parent->x - delta;
	if ( s->generic.flags & QMF_CENTERED ) {
		x += delta / 2;
	}

	y = s->generic.y + s->generic.parent->y;

	trap_DrawPropString ( x, y, ( char * )s->generic.name, fontstyle, colorYellow );
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
	int x, y, delta;
	int fontstyle;
	float *color;

	fontstyle = FONT_SMALL;
	delta = trap_PropStringLength ( ( char * )s->generic.name, fontstyle );

	x = s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET;
	if ( s->generic.flags & QMF_CENTERED ) {
		x += delta / 2;
	}

	y = s->generic.y + s->generic.parent->y;

	trap_DrawPropString ( x - delta, y, ( char *)s->generic.name, fontstyle, colorYellow );

	s->range = ( s->curvalue - s->minvalue ) / ( float ) ( s->maxvalue - s->minvalue );
	clamp ( s->range, 0, 1 );

	if ( Menu_ItemAtCursor( s->generic.parent ) == s )
	{
		fontstyle |= FONT_SHADOWED;
		color = colorRed;
	}
	else
	{
		color = colorWhite;
	}

	trap_DrawChar( s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET, s->generic.y + s->generic.parent->y, 128, FONT_SMALL, colorWhite);
	for ( i = 0; i < SLIDER_RANGE; i++ )
		trap_DrawChar( RCOLUMN_OFFSET + s->generic.x + i*SMALL_CHAR_WIDTH + s->generic.parent->x + SMALL_CHAR_WIDTH, 
		s->generic.y + s->generic.parent->y, 129, FONT_SMALL, colorWhite);
	trap_DrawChar( RCOLUMN_OFFSET + s->generic.x + i*SMALL_CHAR_WIDTH + s->generic.parent->x + SMALL_CHAR_WIDTH, 
		s->generic.y + s->generic.parent->y, 130, FONT_SMALL, colorWhite);
	trap_DrawChar( ( int ) ( SMALL_CHAR_WIDTH + RCOLUMN_OFFSET + s->generic.parent->x + s->generic.x + (SLIDER_RANGE-1)*SMALL_CHAR_WIDTH * s->range ), 
		s->generic.y + s->generic.parent->y, 131, FONT_SMALL, color);
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
	int x, y, delta;
	int fontstyle;
	float *color;

	fontstyle = FONT_SMALL;

	x = s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET;
	y = s->generic.y + s->generic.parent->y;

	if ( s->generic.name )
	{
		delta = trap_PropStringLength ( ( char * )s->generic.name, fontstyle );

		if ( s->generic.flags & QMF_CENTERED ) {
			x += delta / 2;
		}

		trap_DrawPropString ( x - delta, y, ( char *)s->generic.name, fontstyle, colorYellow );
	}

	if ( s->generic.flags & QMF_NOITEMNAMES )
		return;

	if ( Menu_ItemAtCursor( s->generic.parent ) == s )
	{
		fontstyle |= FONT_SHADOWED;
		color = colorRed;
	}
	else
	{
		color = colorWhite;
	}

	y = s->generic.y + s->generic.parent->y;

	if ( !strchr( s->itemnames[s->curvalue], '\n' ) )
	{
		x = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET;

		if ( s->generic.flags & QMF_CENTERED ) {
			x -= trap_PropStringLength ( ( char * )s->itemnames[s->curvalue], fontstyle ) / 2;
		}

		trap_DrawPropString ( x, y, ( char *)s->itemnames[s->curvalue], fontstyle, color );
	}
	else
	{

		strcpy( buffer, s->itemnames[s->curvalue] );
		*strchr( buffer, '\n' ) = 0;

		x = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET;
		if ( s->generic.flags & QMF_CENTERED ) {
			x -= trap_PropStringLength ( buffer, fontstyle ) / 2;
		}

		trap_DrawPropString ( x, y, buffer, fontstyle, color );

		strcpy( buffer, strchr( s->itemnames[s->curvalue], '\n' ) + 1 );

		x = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET;
		if ( s->generic.flags & QMF_CENTERED ) {
			x -= trap_PropStringLength ( buffer, fontstyle ) / 2;
		}

		trap_DrawPropString ( x, y + (fontstyle == FONT_SMALL) ? PROP_SMALL_HEIGHT + 2 : PROP_BIG_HEIGHT + 2, buffer, fontstyle, color );
	}
}

