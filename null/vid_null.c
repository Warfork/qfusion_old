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
// vid_null.c -- null video driver to aid porting efforts
// this assumes that one of the refs is statically linked to the executable

#include "../client/client.h"

/*
============
VID_Sys_Init
============
*/
int	VID_Sys_Init( void ) {
	return R_Init( NULL, NULL );
}

/*
============
VID_Front_f
============
*/
void VID_Front_f( void ) {
}

/*
============
VID_UpdateWindowPosAndSize
============
*/
void VID_UpdateWindowPosAndSize( int x, int y ) {
}

/*
============
VID_EnableAltTab
============
*/
void VID_EnableAltTab( qboolean enable ) {
}
