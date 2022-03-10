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
// cg_screen.c -- master status bar, crosshairs, hud, etc

/*

  full screen console
  put up loading plaque
  blanked background with loading plaque
  blanked background with menu
  cinematics
  full screen image for quit and victory

  end of unit intermissions

  */

#include "cg_local.h"

int			scr_draw_loading;

vrect_t		scr_vrect;

cvar_t		*cg_viewSize;
cvar_t		*cg_centerTime;
cvar_t		*cg_showPause;
cvar_t		*cg_showFPS;

cvar_t		*cg_debugLoading;

cvar_t		*crosshair;
cvar_t		*crosshair_size;
cvar_t		*crosshair_x;
cvar_t		*crosshair_y;


/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_center;

/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint ( char *str )
{
	char	*s;

	Q_strncpyz ( scr_centerstring, str, sizeof(scr_centerstring) );
	scr_centertime_off = cg_centerTime->value;
	scr_centertime_start = cg.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = str;
	while ( *s )
	{
		if (*s++ == '\n')
			scr_center_lines++;
	}
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l, length;
	int		x, y;
	int		remaining;
	int		fontstyle = FONT_SMALL|FONT_SHADOWED;

// the finale prints the characters one at a time
	remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if ( scr_center_lines <= 4 ) {
		y = cgs.vidHeight*0.35;
	} else {
		y = 48;
	}

	do	
	{
		length = 0;
	// scan the width of the line
		for (l=0 ; l<320/SMALL_CHAR_WIDTH ; l++) {
			if ( Q_IsColorString (&start[l]) ) {
				l++;
			} else if ( start[l] == '\n' || !start[l] ) {
				break;
			} else {
				length++;
			}
		}

		x = (cgs.vidWidth - length*SMALL_CHAR_WIDTH)/2;
		CG_DrawStringLen ( x, y, start, l, fontstyle, colorWhite );

		y += SMALL_CHAR_HEIGHT;

		while ( *start && *start != '\n' ) {
			start++;
		}
		if ( !*start ) {
			break;
		}
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_centertime_off -= cg.frameTime;
	if ( scr_centertime_off <= 0 ) {
		return;
	}

	SCR_DrawCenterString ();
}

//=============================================================================

/*
=================
SCR_CalcVrect

Sets scr_vrect, the coordinates of the rendered window
=================
*/
void SCR_CalcVrect (void)
{
	int		size;

	// bound viewsize
	if ( cg_viewSize->value < 40 ) {
		trap_Cvar_Set ( "cg_viewsize", "40" );
	} else if ( cg_viewSize->value > 100 ) {
		trap_Cvar_Set ( "cg_viewsize", "100" );
	}

	size = cg_viewSize->value;

	scr_vrect.width = cgs.vidWidth*size/100;
	scr_vrect.width &= ~7;

	scr_vrect.height = cgs.vidHeight*size/100;
	scr_vrect.height &= ~1;

	scr_vrect.x = (cgs.vidWidth - scr_vrect.width)/2;
	scr_vrect.y = (cgs.vidHeight - scr_vrect.height)/2;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void) {
	trap_Cvar_SetValue ( "cg_viewSize", cg_viewSize->value + 10 );
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void) {
	trap_Cvar_SetValue ( "cg_viewSize", cg_viewSize->value - 10 );
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	cg_viewSize = trap_Cvar_Get ( "cg_viewSize", "100", CVAR_ARCHIVE );
	cg_showPause = trap_Cvar_Get ( "cg_showPause", "1", 0 );
	cg_showFPS = trap_Cvar_Get ( "cg_showFPS", "0", CVAR_ARCHIVE );
	cg_centerTime = trap_Cvar_Get ( "cg_centerTime", "2.5", 0 );
	cg_debugLoading = trap_Cvar_Get ( "cg_debugLoading", "0", CVAR_ARCHIVE );

	crosshair = trap_Cvar_Get ( "cg_crosshair", "0", CVAR_ARCHIVE );
	crosshair_size = trap_Cvar_Get ( "cg_crosshairSize", "24", CVAR_ARCHIVE );
	crosshair_x = trap_Cvar_Get ( "cg_crosshairX", "0", CVAR_ARCHIVE );
	crosshair_y = trap_Cvar_Get ( "cg_crosshairY", "0", CVAR_ARCHIVE );

//
// register our commands
//
	trap_Cmd_AddCommand ( "sizeup", SCR_SizeUp_f );
	trap_Cmd_AddCommand ( "sizedown", SCR_SizeDown_f );
}

/*
==================
SCR_Init
==================
*/
void SCR_Shutdown (void)
{
	trap_Cmd_RemoveCommand ( "sizeup" );
	trap_Cmd_RemoveCommand ( "sizedown" );
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	int incomingAcknowledged, outgoingSequence;

	trap_NET_GetCurrentState ( &incomingAcknowledged, &outgoingSequence );
	if ( outgoingSequence - incomingAcknowledged < CMD_BACKUP-1 ) {
		return;
	}

	trap_Draw_StretchPic ( scr_vrect.x+64, scr_vrect.y, 32, 32, 0, 0, 1, 1, colorWhite, CG_MediaShader (cgs.media.shaderNet) );
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause (void)
{
	if ( !cg_paused->value ) {
		return;
	}
	if ( !cg_showPause->value ) {
		// turn off for screenshots
		return;
	}

	CG_DrawCenteredPropString ( cgs.vidHeight / 2, "PAUSED", FONT_BIG, colorRed );
}

/*
================
SCR_DrawFPS

================
*/
void SCR_DrawFPS (void)
{
	static int fps;
	static double oldtime;
	static int oldframecount;
	double t;
	char s[32];
	int x, width;

	if ( !cg.frameTime || !cg_showFPS->value ) {
		return;
	}

	if ( cg_showFPS->value != 2 ) {
		fps = (int)(1.0f / cg.frameTime);
	} else {
		t = cg.time * 0.001f;

		if ( (t - oldtime) >= 0.25 ) {	// updates 4 times a second
			fps = (cg.frameCount - oldframecount) / (t - oldtime) + 0.5;
			oldframecount = cg.frameCount;
			oldtime = t;
		}
	}

	Com_sprintf ( s, sizeof( s ), "%3dfps", fps );
	width = strlen(s)*BIG_CHAR_WIDTH;
	x = cgs.vidWidth - 5 - width;
	CG_DrawString ( x, 2, s, FONT_BIG|FONT_SHADOWED, colorWhite );
}

/*
=================
SCR_DrawCrosshair
=================
*/
void SCR_DrawCrosshair (void)
{
	int x, y;
	int w, h;

	if ( !crosshair->value || crosshair_size->value <= 0 ) {
		return;
	}
	if ( crosshair->modified ) {
		if ( crosshair->value > NUM_CROSSHAIRS || crosshair->value < 1 ) {
			crosshair->value = NUM_CROSSHAIRS;
		}
		crosshair->modified = qfalse;
	}

	x = max ( 0, (int)crosshair_x->value );
	y = max ( 0, (int)crosshair_y->value );

	w = (int)crosshair_size->value;
	h = (int)crosshair_size->value;

	trap_Draw_StretchPic ( scr_vrect.x + x + ((scr_vrect.width - w)>>1), scr_vrect.y + y + ((scr_vrect.height - h)>>1), w, h, 
		0, 0, 1, 1, colorWhite, CG_MediaShader (cgs.media.shaderCrosshair[(int)crosshair->value-1]) );
}

//=============================================================================

/*
================
CG_LoadLayout
================
*/
void CG_LoadLayout ( char *s )
{
	Q_strncpyz ( cg.layout, s, sizeof(cg.layout) );
}

/*
================
CG_LoadStatusBar
================
*/
void CG_LoadStatusBar ( char *s )
{
	int length;
	char *buffer;
	char path[MAX_QPATH], shortname[MAX_QPATH];

	if ( !s || !s[0] ) {
		return;
	}

	// strip extension and add local path
	COM_StripExtension ( s, shortname );
	Com_sprintf ( path, sizeof(path), "huds/%s.hud", shortname );

	// load the file
	length = trap_FS_LoadFile ( path, (void **)&buffer );
	if ( !buffer ) {
		CG_Printf ( "Bad hud file %s\n", path );
		return;
	}
	if ( !length ) {
		CG_Printf ( "Empty hud file %s\n", path );
		trap_FS_FreeFile ( buffer );
		return;
	}

	// free old status bar if present
	if ( cg.statusBar ) { 
		CG_Free ( cg.statusBar );
	}

	// copy file contents to statusbar string
	cg.statusBar = CG_Malloc ( length );
	memcpy ( cg.statusBar, buffer, length );
	trap_FS_FreeFile ( buffer );
}

//=============================================================================

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	static char str[MAX_QPATH];

	if ( cgs.configStrings[CS_MAPNAME][0] ) {
		// draw a level shot
		trap_Draw_StretchPic ( 0, 0, cgs.vidWidth, cgs.vidHeight, 
			0, 0, 1, 1, colorWhite, cgs.shaderLevelshot );
		trap_Draw_StretchPic ( 0, 0, cgs.vidWidth, cgs.vidHeight, 
			0, 0, 2.5, 2, colorWhite, cgs.shaderLevelshotDetail );

		// draw map name
		Com_sprintf ( str, sizeof(str), "Loading %s", cgs.configStrings[CS_MAPNAME] );
		CG_DrawCenteredPropString ( 16, str, FONT_BIG|FONT_SHADOWED, colorWhite );

		// what we're loading at the moment
		if ( cg.loadingstring[0] ) {
			if ( cg.loadingstring[0] == '-' && !cg.loadingstring[1])
				Q_strncpyz ( str, "awaiting snapshot...", sizeof(str) );
			else
				Com_sprintf ( str, sizeof(str), "loading... %s", cg.loadingstring );
			CG_DrawCenteredPropString ( 96, str, FONT_SMALL|FONT_SHADOWED, colorWhite );
		}

		if ( cg.checkname[0] ) {
			int maxlen;
			char prefix[] = "filename: ";

			maxlen = (cgs.vidWidth - 32)/BIG_CHAR_WIDTH - (sizeof(prefix)-1);
			clamp ( maxlen, 3, sizeof(str)-1 - (sizeof(prefix)-1) );

			if ( strlen(cg.checkname) > maxlen ) {
				Com_sprintf ( str, sizeof(str), "%s...%s", prefix, cg.checkname + strlen(cg.checkname) - maxlen + 3 );
			} else {
				Com_sprintf ( str, sizeof(str), "%s%s", prefix, cg.checkname );
			}

			CG_DrawString ( 16, cgs.vidHeight - 20, str, FONT_BIG|FONT_SHADOWED, colorWhite );
		}
	}
}

//=============================================================================

/*
================
CG_LoadingString
================
*/
void CG_LoadingString ( char *str )
{
	Q_strncpyz ( cg.loadingstring, str, sizeof(cg.loadingstring) );
	trap_R_UpdateScreen ();
}

/*
================
CG_LoadingFilename
================
*/
void CG_LoadingFilename ( char *str )
{
	if ( !cg_debugLoading->value ) {
		cg.checkname[0] = 0;
		return;
	}

	Q_strncpyz ( cg.checkname, str, sizeof(cg.checkname) );
	trap_R_UpdateScreen ();
}

/*
==============
SCR_TileClearRect

This repeats tile graphic to fill the screen around a sized down
refresh window.
==============
*/
void SCR_TileClearRect ( int x, int y, int w, int h, struct shader_s *shader )
{
	float iw, ih;

	iw = 1.0f / 64.0;
	ih = 1.0f / 64.0;

	trap_Draw_StretchPic ( x, y, w, h, x*iw, y*ih, (x+w)*iw, (y+h)*ih, colorWhite, shader );
}

/*
==============
SCR_TileClear

Clear any parts of the tiled background that were drawn on last frame
==============
*/
void SCR_TileClear (void)
{
	int		w, h;
	int		top, bottom, left, right;
	struct shader_s *backTile;

	if ( cg_viewSize->value == 100 ) {
		return;		// full screen rendering
	}

	w = cgs.vidWidth;
	h = cgs.vidHeight;

	top = scr_vrect.y;
	bottom = top + scr_vrect.height-1;
	left = scr_vrect.x;
	right = left + scr_vrect.width-1;

	backTile = CG_MediaShader ( cgs.media.shaderBackTile );

	// clear above view screen
	SCR_TileClearRect ( 0, 0, w, top, backTile );

	// clear below view screen
	SCR_TileClearRect ( 0, bottom, w, h - bottom, backTile );

	// clear left of view screen
	SCR_TileClearRect ( 0, top, left, bottom - top + 1, backTile );

	// clear left of view screen
	SCR_TileClearRect ( right, top, w - right, bottom - top + 1, backTile );
}


//===============================================================

/*
================
SCR_ExecuteLayoutString 

================
*/
void SCR_ExecuteLayoutString ( char *s )
{
	int		x, y, w, h;
	int		value;
	char	*token;
	int		width;
	int		index;
	cg_clientInfo_t	*ci;

	if ( !s || !s[0] ) {
		return;
	}

	x = 0;
	y = 0;
	width = 3;

	while ( s )
	{
		token = COM_Parse ( &s );

		if ( !strcmp (token, "xl") ) {
			token = COM_Parse ( &s );
			x = atoi ( token );
			continue;
		} else if ( !strcmp (token, "xr") ) {
			token = COM_Parse ( &s );
			x = cgs.vidWidth + atoi ( token );
			continue;
		} else if ( !strcmp(token, "xv") ) {
			token = COM_Parse ( &s );
			x = cgs.vidWidth/2 - 160 + atoi ( token );
			continue;
		} else if ( !strcmp(token, "yt") ) {
			token = COM_Parse ( &s );
			y = atoi ( token );
			continue;
		} else if ( !strcmp(token, "yb") ) {
			token = COM_Parse ( &s );
			y = cgs.vidHeight + atoi ( token );
			continue;
		} else if ( !strcmp(token, "yv") ) {
			token = COM_Parse ( &s );
			y = cgs.vidHeight/2 - 120 + atoi ( token );
			continue;
		} else if ( !strcmp(token, "size") ){
			token = COM_Parse ( &s );
			w = atoi ( token );
			token = COM_Parse ( &s );
			h = atoi ( token );
			continue;
		} else if ( !strcmp(token, "pic") ) {
			// draw a pic from a stat number
			token = COM_Parse ( &s );
			value = cg.frame.playerState.stats[atoi(token)];
			if ( value >= MAX_IMAGES ) {
				CG_Error ("Pic >= MAX_IMAGES");
			}
			if ( cgs.configStrings[CS_IMAGES+value] ) {
				trap_Draw_StretchPic (x, y, w, h, 0, 0, 1, 1, colorWhite, trap_R_RegisterPic(cgs.configStrings[CS_IMAGES+value]) );
			}
			continue;
		} else if ( !strcmp(token, "client") ) {
			// draw a deathmatch client block
			int		tag, score, ping, time;
			float	*color;

			token = COM_Parse ( &s );
			x = cgs.vidWidth/2 - 160 + atoi( token );
			token = COM_Parse (&s);
			y = cgs.vidHeight/2 - 120 + atoi( token );

			token = COM_Parse ( &s );
			tag = atoi( token );

			token = COM_Parse ( &s );
			value = atoi( token );
			if ( value >= MAX_CLIENTS || value < 0 ) {
				CG_Error ( "client >= MAX_CLIENTS" );
			}
			ci = &cgs.clientInfo[value];

			token = COM_Parse ( &s );
			score = atoi( token );

			token = COM_Parse ( &s );
			ping = atoi( token );

			token = COM_Parse (&s);
			time = atoi( token );

			if ( tag == 0 ) {			// player
				color = colorYellow;
			} else if ( tag == 1 ) {	// killer
				color = colorRed;
			} else {
				color = colorWhite;
			}

			CG_DrawString ( x+32, y, ci->name, FONT_SMALL, color );
			CG_DrawString ( x+32, y+SMALL_CHAR_HEIGHT, "Score: ", FONT_SMALL, color );
			CG_DrawString ( x+32+7*SMALL_CHAR_WIDTH, y+SMALL_CHAR_HEIGHT, va("%i", score), FONT_SMALL, color );
			CG_DrawString ( x+32, y+SMALL_CHAR_HEIGHT*2, va("Ping:  %i", ping), FONT_SMALL, color );
			CG_DrawString ( x+32, y+SMALL_CHAR_HEIGHT*3, va("Time:  %i", time), FONT_SMALL, color );

			if ( !ci->icon ) {
				ci = &cgs.baseClientInfo;
			}
			trap_Draw_StretchPic ( x, y, 32, 32, 0, 0, 1, 1, colorWhite, trap_R_RegisterPic(ci->iconname) );
			continue;
		} else if (!strcmp(token, "ctf")) {
			// draw a ctf client block
			int		score, ping;
			char	block[80];

			token = COM_Parse ( &s );
			x = cgs.vidWidth/2 - 160 + atoi( token );
			token = COM_Parse (&s);
			y = cgs.vidHeight/2 - 120 + atoi( token );

			token = COM_Parse ( &s );
			value = atoi( token );
			if ( value >= MAX_CLIENTS || value < 0 ) {
				CG_Error ( "client >= MAX_CLIENTS" );
			}

			ci = &cgs.clientInfo[value];

			token = COM_Parse ( &s );
			score = atoi( token );

			token = COM_Parse ( &s );
			ping = atoi( token );
			if ( ping > 999 ) {
				ping = 999;
			}

			sprintf ( block, "%3d %3d %-12.12s", score, ping, ci->name );

			if ( value == cgs.playerNum ) {
				CG_DrawString ( x, y, block, FONT_SMALL, colorRed );
			} else {
				CG_DrawString ( x, y, block, FONT_SMALL, colorWhite );
			}

			continue;
		} else if ( !strcmp(token, "picn") ) {
			// draw a pic from a name
			token = COM_Parse ( &s );
			trap_Draw_StretchPic ( x, y, w, h, 0, 0, 1, 1, colorWhite, trap_R_RegisterPic(token) );
			continue;
		} else if ( !strcmp(token, "model") ) {
			// draw a model from a stat number
			struct model_s *model;

			token = COM_Parse ( &s );
			value = cg.frame.playerState.stats[atoi(token)];
			if ( value >= MAX_MODELS ) {
				CG_Error ( "Model >= MAX_MODELS" );
			}

			model = value > 1 ? trap_R_RegisterModel ( cgs.configStrings[CS_MODELS+value] ) : NULL;
			token = COM_Parse ( &s );
			CG_DrawHUDModel ( x, y, w, h, model, NULL, atof(token) );
			continue;
		} else if (!strcmp(token, "modeln")) {
			// draw a model from a name
			struct model_s *model;
			struct shader_s *shader;

			token = COM_Parse ( &s );
			model = trap_R_RegisterModel ( token );
			token = COM_Parse ( &s );
			shader = Q_stricmp( token, "NULL" ) ? trap_R_RegisterPic ( token ) : NULL;
			token = COM_Parse ( &s );
			CG_DrawHUDModel ( x, y, w, h, model, shader, atof(token) );
			continue;
		} else if ( !strcmp(token, "num") ) {
			// draw a number
			token = COM_Parse ( &s );
			width = atoi( token );
			token = COM_Parse ( &s );
			value = cg.frame.playerState.stats[atoi(token)];
			CG_DrawHUDField ( x, y, colorWhite, width, value );
			continue;
		} else if ( !strcmp(token, "hnum") ) {
			// health number
			float	*color;

			width = 3;
			value = cg.frame.playerState.stats[STAT_HEALTH];

			if ( value > 25 ) {			// green
				color = colorWhite;				
			} else if ( value > 0 ) {	// flash
				color = (cg.frame.serverFrame>>2) & 1 ? colorRed : colorWhite;
			} else {
				color = colorRed;
			}

			CG_DrawHUDField ( x, y, color, width, value );
			continue;
		} else if ( !strcmp(token, "anum") ) {
			// ammo number
			float *color;

			width = 3;
			value = cg.frame.playerState.stats[STAT_AMMO];
			if ( value > 5 ) {				// green
				color = colorWhite;			
			} else if ( value >= 0 ) {		// flash
				color = (cg.frame.serverFrame>>2) & 1 ? colorRed : colorWhite;
			} else {
				continue;	// negative number = don't show
			}

			CG_DrawHUDField ( x, y, color, width, value );
			continue;
		} else if ( !strcmp(token, "rnum") ) {
			// armor number
			width = 3;
			value = cg.frame.playerState.stats[STAT_ARMOR];
			if ( value < 1 ) {
				continue;
			}

			CG_DrawHUDField ( x, y, colorWhite, width, value );
			continue;
		} else if ( !strcmp(token, "frags") ) {
			// frags number in a box
			vec4_t color;
			char str[32];

			Com_sprintf ( str, 32, "%2i", cg.frame.playerState.stats[STAT_FRAGS] );
			width = strlen(str)*BIG_CHAR_WIDTH + 8;

			color[0] = 0;
			color[1] = 0;
			color[2] = 1;
			color[3] = 0.33;
			CG_FillRect ( x - width, y, width, 24, color );
			trap_Draw_StretchPic ( x - width, y, width, 24, 0, 0, 1, 1, colorWhite, CG_MediaShader (cgs.media.shaderSelect) );
			CG_DrawString ( x - width + 4, y + 4, str, FONT_SHADOWED|FONT_BIG, colorWhite );
			continue;
		} else if ( !strcmp(token, "stat_string") ) {
			token = COM_Parse ( &s );

			index = atoi( token );
			if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
				CG_Error ( "Bad stat_string index" );
			}
			index = cg.frame.playerState.stats[index];
			if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
				CG_Error ("Bad stat_string index");
			}

			CG_DrawString ( x, y, cgs.configStrings[index], FONT_SMALL, colorWhite );
			continue;
		} else if ( !strcmp(token, "cstring") )	{
			token = COM_Parse ( &s );
			CG_DrawHUDString ( token, x, y, 320, FONT_SMALL, colorWhite );
			continue;
		} else if ( !strcmp(token, "string") ) {
			token = COM_Parse ( &s );
			CG_DrawHUDString ( token, x, y, 0, FONT_SMALL, colorWhite );
			continue;
		} else if ( !strcmp(token, "if") ) {
			// draw a number
			token = COM_Parse (&s);

			value = cg.frame.playerState.stats[atoi(token)];
			if ( !value ) {	// skip to endif
				while ( s && strcmp(token, "endif") )
					token = COM_Parse (&s);
			}
			continue;
		} else if ( !strcmp(token, "ifeq") ) {
			// draw a number
			token = COM_Parse ( &s );
			value = cg.frame.playerState.stats[atoi(token)];
			token = COM_Parse ( &s );

			if ( value != atoi(token) ) {	// skip to endif
				while ( s && strcmp(token, "endif") )
					token = COM_Parse (&s);
			}
			continue;
		} else if (!strcmp(token, "ifbit")) {
			// draw a number
			token = COM_Parse ( &s );
			value = cg.frame.playerState.stats[atoi(token)];
			token = COM_Parse ( &s );

			if ( !(value & atoi(token)) ) {	// skip to endif
				while ( s && strcmp(token, "endif") )
					token = COM_Parse (&s);
			}
			continue;
		} else if ( !strcmp(token, "ifle") ) {
			// draw a number
			token = COM_Parse ( &s );
			value = cg.frame.playerState.stats[atoi(token)];
			token = COM_Parse ( &s );

			if ( value > atoi(token) ) {	// skip to endif
				while ( s && strcmp(token, "endif") )
					token = COM_Parse (&s);
			}
			continue;
		} else if ( !strcmp(token, "ifl") ) {
			// draw a number
			token = COM_Parse ( &s );
			value = cg.frame.playerState.stats[atoi(token)];
			token = COM_Parse ( &s );

			if ( value >= atoi(token) ) {	// skip to endif
				while ( s && strcmp(token, "endif") )
					token = COM_Parse (&s);
			}
			continue;
		} else if ( !strcmp(token, "ifge") ) {
			// draw a number
			token = COM_Parse ( &s );
			value = cg.frame.playerState.stats[atoi(token)];
			token = COM_Parse ( &s );

			if ( value < atoi(token) ) {	// skip to endif
				while ( s && strcmp(token, "endif") )
					token = COM_Parse (&s);
			}
			continue;
		} else if ( !strcmp(token, "ifg") ) {
			// draw a number
			token = COM_Parse ( &s );
			value = cg.frame.playerState.stats[atoi(token)];
			token = COM_Parse ( &s );

			if ( value <= atoi(token) ) {	// skip to endif
				while ( s && strcmp(token, "endif") )
					token = COM_Parse (&s);
			}
			continue;
		}
	}
}


/*
================
SCR_DrawStats

The status bar is a small layout program that
is based on the stats array
================
*/
void SCR_DrawStats (void)
{
	if ( !cg.statusBar ) {
		return;
	}
	SCR_ExecuteLayoutString ( cg.statusBar );
}


/*
================
SCR_DrawLayout
================
*/
void SCR_DrawLayout (void)
{
	if ( !cg.frame.playerState.stats[STAT_LAYOUTS] ) {
		return;
	}
	SCR_ExecuteLayoutString ( cg.layout );
}

/*
================
SCR_DrawInventory
================
*/
#define	DISPLAY_ITEMS	17

void SCR_DrawInventory (void)
{
	int		i, j;
	int		num, selected_num, item;
	int		index[MAX_ITEMS];
	char	string[1024];
	int		x, y;
	char	binding[1024];
	char	*bind;
	int		selected;
	int		top;

	selected = cg.frame.playerState.stats[STAT_SELECTED_ITEM];

	num = 0;
	selected_num = 0;
	for ( i = 0; i < MAX_ITEMS; i++ )
	{
		if ( i == selected ) {
			selected_num = num;
		}
		if ( cg.inventory[i] ) {
			index[num] = i;
			num++;
		}
	}

	// determine scroll point
	top = selected_num - DISPLAY_ITEMS / 2;
	if ( num - top < DISPLAY_ITEMS ) {
		top = num - DISPLAY_ITEMS;
	}
	if ( top < 0 ) {
		top = 0;
	}

	x = (cgs.vidWidth-256) / 2;
	y = (cgs.vidHeight-240) / 2;

	y += 24;
	x += 24;

	CG_DrawString ( x, y,   "hotkey ### item", FONT_SMALL, colorWhite );
	CG_DrawString ( x, y+SMALL_CHAR_HEIGHT, "------ --- ----", FONT_SMALL, colorWhite );

	y += SMALL_CHAR_HEIGHT * 2;
	for ( i = top; i < num && i < top+DISPLAY_ITEMS; i++ )
	{
		item = index[i];

		// search for a binding
		Com_sprintf ( binding, sizeof(binding), "use %s", cgs.configStrings[CS_ITEMS+item] );
		bind = "";

		for ( j = 0; j < 256; j++ )
		{
			if ( trap_Key_GetBindingBuf(j) && !Q_stricmp (trap_Key_GetBindingBuf(j), binding) )
			{
				bind = trap_Key_KeynumToString ( j );
				break;
			}
		}

		Com_sprintf (string, sizeof(string), "%6s %3i %s", bind, cg.inventory[item], cgs.configStrings[CS_ITEMS+item] );

		if ( item != selected ) {
			CG_DrawString ( x, y, string, FONT_SMALL, colorWhite );
		} else {	// draw a blinky cursor by the selected item
			if ( (int)(cg.realTime*10) & 1 ) {
				CG_DrawChar ( x-SMALL_CHAR_WIDTH, y, FONT_SMALL, 15, colorWhite );
			}
			CG_DrawString ( x, y, string, FONT_SMALL, colorYellow );
		}

		y += SMALL_CHAR_HEIGHT;
	}
}


//=======================================================

/*
==================
SCR_Draw2D

==================
*/
void SCR_Draw2D (void)
{
	SCR_DrawCrosshair ();

	SCR_DrawStats ();

	if ( cg.frame.playerState.stats[STAT_LAYOUTS] & 1 ) {
		SCR_DrawLayout ();
	}
	if ( cg.frame.playerState.stats[STAT_LAYOUTS] & 2 ) {
		SCR_DrawInventory ();
	}

	SCR_DrawNet ();
	SCR_CheckDrawCenterString ();

	SCR_DrawFPS ();

	SCR_DrawPause ();
}
