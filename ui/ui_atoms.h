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
#ifndef __QMENU_H__
#define __QMENU_H__

#define	FONT_BIG		1
#define FONT_SMALL		2
#define FONT_GIANT		4
#define FONT_SHADOWED	8

#define MAXMENUITEMS		64

#define MTYPE_SLIDER		0
#define MTYPE_LIST			1
#define MTYPE_ACTION		2
#define MTYPE_SPINCONTROL	3
#define MTYPE_SEPARATOR  	4
#define MTYPE_FIELD			5

#define QMF_LEFT_JUSTIFY	0x00000001
#define QMF_GRAYED			0x00000002
#define QMF_NUMBERSONLY		0x00000004
#define QMF_NOITEMNAMES		0x00000008
#define QMF_CENTERED		0x00000010
#define QMF_GIANT			0x00000020
#define QMF_NONPROPORTIONAL	0x00000040

#define SLIDER_RANGE		10

typedef struct _tag_menuframework
{
	int x, y;
	int	cursor;

	int	nitems;
	int nslots;
	void *items[MAXMENUITEMS];

	const char *statusbar;

	void (*cursordraw)( struct _tag_menuframework *m );
	
} menuframework_s;

typedef struct
{
	int type;
	const char *name;
	int x, y;
	int mins[2], maxs[2];
	menuframework_s *parent;
	int cursor_offset;
	int	localdata[4];
	unsigned flags;

	const char *statusbar;

	void (*callback)( void *self );
	void (*statusbarfunc)( void *self );
	void (*ownerdraw)( void *self );
	void (*cursordraw)( void *self );
} menucommon_s;

typedef struct
{
	menucommon_s generic;

	char		buffer[80];
	int			cursor;
	int			length;
	int			visible_length;
	int			visible_offset;
} menufield_s;

typedef struct 
{
	menucommon_s generic;

	float minvalue;
	float maxvalue;
	float curvalue;

	float range;
} menuslider_s;

typedef struct
{
	menucommon_s generic;

	int curvalue;

	char **itemnames;
} menulist_s;

typedef struct
{
	menucommon_s generic;
} menuaction_s;

typedef struct
{
	menucommon_s generic;
} menuseparator_s;

qboolean Field_Key( menufield_s *field, int key );

void	UI_DrawNonPropChar ( int x, int y, int num, int fontstyle, vec4_t color );
void	UI_DrawNonPropString ( int x, int y, char *str, int fontstyle, vec4_t color );
void	UI_DrawPropString ( int x, int y, char *str, int fontstyle, vec4_t color );
int		UI_PropStringLength ( char *str, int fontstyle );

int		UI_FontstyleForFlags ( int flags );
int		UI_NonPropStringLength ( char *s );
int		UI_StringWidth ( int flags, char *s );
int		UI_StringHeight ( int flags );
int		UI_StringHeightOffset ( int flags );
int		UI_AdjustStringPosition ( int flags, char *s );
void	UI_DrawString ( int x, int y, char *str, int flags, int fsm, vec4_t color );

void	Menu_AddItem( menuframework_s *menu, void *item );
void	Menu_AdjustCursor( menuframework_s *menu, int dir );
void	Menu_Center( menuframework_s *menu );
void	Menu_Init( menuframework_s *menu );
void	Menu_Draw( menuframework_s *menu );
void	*Menu_ItemAtCursor( menuframework_s *m );
qboolean Menu_SelectItem( menuframework_s *s );
void	Menu_SetStatusBar( menuframework_s *s, const char *string );
qboolean Menu_SlideItem( menuframework_s *s, int dir );
int		Menu_TallySlots( menuframework_s *menu );

#endif
