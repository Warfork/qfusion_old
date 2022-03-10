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
==============
UI_DrawLoading
==============
*/
void UI_DrawLoading (void)
{
	char *mapname;
	static char str[64];

	mapname = trap_GetConfigString ( CS_MAPNAME );

	if ( uis.clientState == ca_connecting && mapname[0] ) {
		char levelshot[MAX_QPATH];

		Com_sprintf ( levelshot, sizeof(levelshot), "levelshots/%s.jpg", mapname );

		if ( !trap_FS_FileExists ( levelshot ) )
			Com_sprintf ( levelshot, sizeof(levelshot), "levelshots/%s.tga", mapname );
		if ( !trap_FS_FileExists ( levelshot ) )
			Com_sprintf ( levelshot, sizeof(levelshot), "menu/art/unknownmap" );

		// draw a level shot
		trap_DrawStretchPic ( 0, 0, uis.vidWidth, uis.vidHeight,
			0, 0, 1, 1, colorWhite, trap_RegisterPic ( levelshot ) );
		trap_DrawStretchPic ( 0, 0, uis.vidWidth, uis.vidHeight,
			0, 0, 2.5, 2, colorWhite, trap_RegisterPic ( "levelShotDetail" ) );
	} else {
		trap_DrawStretchPic ( 0, 0, uis.vidWidth, uis.vidHeight, 
			0, 0, 1, 1, colorWhite, trap_RegisterPic ( "menuback" ) );
	}

	Com_sprintf ( str, sizeof(str), "Connecting to %s", trap_GetServerName() );

	trap_DrawPropString ( (uis.vidWidth - trap_PropStringLength (str, FONT_SMALL)) / 2,
		64, str, FONT_SMALL|FONT_SHADOWED, colorWhite );

	if ( uis.clientState != ca_connecting && mapname[0] ) {
		char *checkName;
		char *loadingString;

		checkName = trap_Cvar_VariableString ( "scr_debugloadingname " );
		loadingString = trap_Cvar_VariableString ( "scr_loadingstring" );
		
		// draw map name
		Com_sprintf ( str, sizeof(str), "Loading %s", mapname );
		trap_DrawPropString ( (uis.vidWidth - trap_PropStringLength (str, FONT_BIG)) / 2,
			16, str, FONT_BIG|FONT_SHADOWED, colorWhite );

		// what we're loading at the moment
		if (loadingString && loadingString[0]) {
			if (loadingString[0] == '-' && !loadingString[1])
				strcpy (str, "awaiting snapshot...");
			else
				Com_sprintf ( str, sizeof(str), "loading... %s", loadingString);
			trap_DrawPropString ( (uis.vidWidth - trap_PropStringLength (str, FONT_SMALL)) / 2,
				96, str, FONT_SMALL|FONT_SHADOWED, colorWhite );
		}

		if ( checkName && checkName[0] ) {
			int maxlen;
			char prefix[] = "filename: ";

			maxlen = (uis.vidWidth - 32)/BIG_CHAR_WIDTH - (sizeof(prefix)-1);
			clamp (maxlen, 3, sizeof(str)-1 - (sizeof(prefix)-1));

			strcpy (str, prefix);
			if ( strlen(checkName) > maxlen ) {
				strcat (str, "...");
				strcat (str, checkName + strlen(checkName) - maxlen + 3);
			} else {
				strcat (str, checkName);
			}

			trap_DrawString ( 16, uis.vidWidth - 20, str, FONT_BIG|FONT_SHADOWED, colorWhite );
		}

		// level name ("message")
		trap_DrawPropString ( (uis.vidWidth - trap_PropStringLength (mapname, FONT_SMALL)) / 2,
			150, mapname, FONT_SMALL|FONT_SHADOWED, colorWhite );
	}
}