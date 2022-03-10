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

vrect_t		scr_vrect;

cvar_t		*cg_viewSize;
cvar_t		*cg_centerTime;
cvar_t		*cg_showPause;
cvar_t		*cg_showFPS;
cvar_t		*cg_showHUD;

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
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint( const char *str )
{
	const char	*s;

	Q_strncpyz( scr_centerstring, str, sizeof(scr_centerstring) );
	scr_centertime_off = cg_centerTime->value;
	scr_centertime_start = cg.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = str;
	while( *s )
		if( *s++ == '\n' )
			scr_center_lines++;
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

	if( scr_center_lines <= 4 )
		y = cgs.vidHeight*0.35;
	else
		y = 48;

	do {
		length = 0;

		// scan the width of the line
		for( l = 0; l < 320 / SMALL_CHAR_WIDTH; l++ ) {
			if( Q_IsColorString( &start[l] ) )
				l++;
			else if( start[l] == '\n' || !start[l] )
				break;
			else
				length++;
		}

		x = (cgs.vidWidth - length*SMALL_CHAR_WIDTH)/2;
		CG_DrawStringLen( x, y, start, l, fontstyle, colorWhite );

		y += SMALL_CHAR_HEIGHT;

		while( *start && *start != '\n' )
			start++;
		if( !*start )
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_centertime_off -= cg.frameTime;
	if( scr_centertime_off <= 0 )
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
=================
SCR_CalcVrect

Sets scr_vrect, the coordinates of the rendered window
=================
*/
void SCR_CalcVrect( void )
{
	int		size;

	// bound viewsize
	if( cg_viewSize->integer < 40 )
		trap_Cvar_Set( "cg_viewsize", "40" );
	else if( cg_viewSize->integer > 100 )
		trap_Cvar_Set( "cg_viewsize", "100" );

	size = cg_viewSize->integer;

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
void SCR_SizeUp_f( void ) {
	trap_Cvar_SetValue( "cg_viewSize", cg_viewSize->integer + 10 );
}

/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f( void ) {
	trap_Cvar_SetValue( "cg_viewSize", cg_viewSize->integer - 10 );
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	cg_viewSize = trap_Cvar_Get( "cg_viewSize", "100", CVAR_ARCHIVE );
	cg_showPause = trap_Cvar_Get( "cg_showPause", "1", 0 );
	cg_showFPS = trap_Cvar_Get( "cg_showFPS", "0", CVAR_ARCHIVE );
	cg_showHUD = trap_Cvar_Get( "cg_showHUD", "1", CVAR_ARCHIVE );
	cg_centerTime = trap_Cvar_Get( "cg_centerTime", "2.5", 0 );
	cg_debugLoading = trap_Cvar_Get( "cg_debugLoading", "0", CVAR_ARCHIVE );

	crosshair = trap_Cvar_Get( "cg_crosshair", "0", CVAR_ARCHIVE );
	crosshair_size = trap_Cvar_Get( "cg_crosshairSize", "24", CVAR_ARCHIVE );
	crosshair_x = trap_Cvar_Get( "cg_crosshairX", "0", CVAR_ARCHIVE );
	crosshair_y = trap_Cvar_Get( "cg_crosshairY", "0", CVAR_ARCHIVE );

//
// register our commands
//
	trap_Cmd_AddCommand( "sizeup", SCR_SizeUp_f );
	trap_Cmd_AddCommand( "sizedown", SCR_SizeDown_f );
}

/*
==================
SCR_Shutdown
==================
*/
void SCR_Shutdown (void)
{
	trap_Cmd_RemoveCommand( "sizeup" );
	trap_Cmd_RemoveCommand( "sizedown" );
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet( void )
{
	int incomingAcknowledged, outgoingSequence;

	trap_NET_GetCurrentState( &incomingAcknowledged, &outgoingSequence );
	if( outgoingSequence - incomingAcknowledged < CMD_BACKUP-1 )
		return;
	trap_R_DrawStretchPic( scr_vrect.x+64, scr_vrect.y, 32, 32, 0, 0, 1, 1, colorWhite, CG_MediaShader (cgs.media.shaderNet) );
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause( void )
{
	if( !cg_paused->integer )
		return;
	if( !cg_showPause->integer )
		return;		// turn off for screenshots
	CG_DrawCenteredPropString( cgs.vidHeight / 2, "PAUSED", FONT_BIG, colorRed );
}

/*
================
SCR_DrawFPS
================
*/
void SCR_DrawFPS( void )
{
	static int fps;
	static double oldtime;
	static int oldframecount;
	double t;
	char s[32];
	int x, width;

	if( !cg.frameTime || !cg_showFPS->integer )
		return;

	if ( cg_showFPS->integer != 2 ) {
		fps = (int)(1.0f / cg.frameTime);
	} else {
		t = cg.realTime * 0.001f;

		if ( (t - oldtime) >= 0.25 ) {	// updates 4 times a second
			fps = (cg.frameCount - oldframecount) / (t - oldtime) + 0.5;
			oldframecount = cg.frameCount;
			oldtime = t;
		}
	}

	Q_snprintfz ( s, sizeof( s ), "%3dfps", fps );
	width = strlen(s) * BIG_CHAR_WIDTH;
	x = cgs.vidWidth - 5 - width;
	CG_DrawString( x, 2, s, FONT_BIG|FONT_SHADOWED, colorWhite );
}

/*
================
SCR_DrawRSpeeds
================
*/
void SCR_DrawRSpeeds( void )
{
	char msg[1024];
	int x, y;
	vec4_t color;

	x = 5;
	y = cgs.vidHeight / 2;
	Vector4Copy( colorWhite, color );

	trap_R_SpeedsMessage( msg, sizeof( msg ) );

	if( msg[0] )
	{
		int height;
		char *p, *start, *end;

		height = SMALL_CHAR_HEIGHT;

		p = start = msg;
		do
		{
			end = strchr( p, '\n' );
			if( end )
				msg[end-start] = '\0';

			CG_DrawString( x, y, p, FONT_SMALL, color );
			y += height;

			if( end )
				p = end + 1;
			else
				break;
		} while( 1 );
	}
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

	if( !crosshair->integer || crosshair_size->integer <= 0 )
		return;
	if( crosshair->modified ) {
		if( crosshair->integer > NUM_CROSSHAIRS || crosshair->integer < 1 )
			trap_Cvar_Set( "crosshair", va("%i", NUM_CROSSHAIRS ) ); 
		crosshair->modified = qfalse;
	}

	x = max( 0, crosshair_x->integer );
	y = max( 0, crosshair_y->integer );

	w = max( 1, crosshair_size->integer );
	h = max( 1, crosshair_size->integer );

	trap_R_DrawStretchPic( scr_vrect.x + x + ((scr_vrect.width - w)>>1), scr_vrect.y + y + ((scr_vrect.height - h)>>1), w, h, 
		0, 0, 1, 1, colorWhite, CG_MediaShader (cgs.media.shaderCrosshair[(int)crosshair->value-1]) );
}

//=============================================================================

/*
================
CG_LoadLayout
================
*/
void CG_LoadLayout( char *s ) {
	Q_strncpyz ( cg.layout, s, sizeof(cg.layout) );
}

/*
================
CG_LoadStatusBar
================
*/
void CG_LoadStatusBar( char *s )
{
	int length, f;
	char path[MAX_QPATH], shortname[MAX_QPATH];

	if( !s || !s[0] )
		return;

	// strip extension and add local path
	COM_StripExtension( s, shortname );
	Q_snprintfz( path, sizeof(path), "huds/%s.hud", shortname );

	// load the file
	length = trap_FS_FOpenFile( path, &f, FS_READ );
	if( length == -1 )
		return;
	if( !length ) {
		trap_FS_FCloseFile( f );
		return;
	}

	// free old status bar if present
	if( cg.statusBar )
		CG_Free( cg.statusBar );

	// copy file contents to statusbar string
	cg.statusBar = CG_Malloc( length + 1 );
	trap_FS_Read( cg.statusBar, length, f );
	trap_FS_FCloseFile( f );
}

//=============================================================================

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading( void )
{
	char str[MAX_QPATH];

	if( cgs.configStrings[CS_MAPNAME][0] ) {
		// draw a level shot
		trap_R_DrawStretchPic( 0, 0, cgs.vidWidth, cgs.vidHeight, 
			0, 0, 1, 1, colorWhite, cgs.shaderLevelshot );
		trap_R_DrawStretchPic( 0, 0, cgs.vidWidth, cgs.vidHeight, 
			0, 0, 2.5, 2, colorWhite, cgs.shaderLevelshotDetail );

		// draw map name
		Q_snprintfz( str, sizeof(str), "Loading %s", cgs.configStrings[CS_MAPNAME] );
		CG_DrawCenteredPropString( 16, str, FONT_BIG|FONT_SHADOWED, colorWhite );

		// what we're loading at the moment
		if( cg.loadingstring[0] ) {
			if ( cg.loadingstring[0] == '-' && !cg.loadingstring[1])
				Q_strncpyz( str, "awaiting snapshot...", sizeof(str) );
			else
				Q_snprintfz( str, sizeof(str), "loading... %s", cg.loadingstring );
			CG_DrawCenteredPropString( 96, str, FONT_SMALL|FONT_SHADOWED, colorWhite );
		}

		if( cg.checkname[0] ) {
			int maxlen;
			char prefix[] = "filename: ";

			maxlen = (cgs.vidWidth - 32)/BIG_CHAR_WIDTH - (sizeof(prefix)-1);
			clamp( maxlen, 3, sizeof(str)-1 - (sizeof(prefix)-1) );

			if( strlen(cg.checkname) > maxlen )
				Q_snprintfz( str, sizeof(str), "%s...%s", prefix, cg.checkname + strlen(cg.checkname) - maxlen + 3 );
			else
				Q_snprintfz( str, sizeof(str), "%s%s", prefix, cg.checkname );
			CG_DrawString( 16, cgs.vidHeight - 20, str, FONT_BIG|FONT_SHADOWED, colorWhite );
		}
	}
}

//=============================================================================

/*
================
CG_LoadingString
================
*/
void CG_LoadingString( char *str )
{
	Q_strncpyz( cg.loadingstring, str, sizeof(cg.loadingstring) );
	trap_R_UpdateScreen ();
}

/*
================
CG_LoadingFilename
================
*/
void CG_LoadingFilename( char *str )
{
	if ( !cg_debugLoading->integer ) {
		cg.checkname[0] = 0;
		return;
	}

	Q_strncpyz( cg.checkname, str, sizeof(cg.checkname) );
	trap_R_UpdateScreen ();
}

/*
==============
SCR_TileClearRect

This repeats tile graphic to fill the screen around a sized down
refresh window.
==============
*/
void SCR_TileClearRect( int x, int y, int w, int h, struct shader_s *shader )
{
	float iw, ih;

	iw = 1.0f / 64.0;
	ih = 1.0f / 64.0;

	trap_R_DrawStretchPic( x, y, w, h, x*iw, y*ih, (x+w)*iw, (y+h)*ih, colorWhite, shader );
}

/*
==============
SCR_TileClear

Clear any parts of the tiled background that were drawn on last frame
==============
*/
void SCR_TileClear( void )
{
	int		w, h;
	int		top, bottom, left, right;
	struct shader_s *backTile;

	if ( cg_viewSize->integer == 100 )
		return;		// full screen rendering

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
SCR_ParseValue
================
*/
int SCR_ParseValue( char **s )
{
	int		index;
	char	*token;

	token = COM_Parse( s );
	if( !token[0] )
		return 0;
	else if( token[0] != '%' )
		return atoi( token );

	index = atoi( token + 1 );
	if( index < 0 || index >= MAX_STATS )
		CG_Error( "Bad stat index: %i", index );

	return cg.frame.playerState.stats[index];
}

/*
================
SCR_ExecuteLayoutString
================
*/
void SCR_ExecuteLayoutString( char *s )
{
	int		x, y, w, h;
	int		value;
	char	*token;
	int		width;
	int		index, stack;
	qboolean skip, flash;
	vec4_t	color, flashColor;
	cg_clientInfo_t	*ci;

	if( !s || !s[0] )
		return;

	x = y = 0;
	w = h = 1;
	width = 3;
	Vector4Copy( colorWhite, color );
	Vector4Copy( colorWhite, flashColor );
	stack = 0;
	skip = qfalse;
	flash = (cg.frame.serverFrame >> 2) & 1 ? qtrue : qfalse;

	while( s ) {
		token = COM_Parse( &s );

		if( stack > 0 ) {
			if( !Q_stricmp( token, "endif") ) {
				stack--;
				if( !stack )
					skip = qfalse;
				continue;
			}
		}

		if( !Q_stricmp( token, "if" ) ) {
			if( skip || !SCR_ParseValue( &s ) ) {
				stack++;
				skip = qtrue; // skip to endif
			}
		} else if( !Q_stricmp( token, "ifeq" ) ) {
			if( skip || SCR_ParseValue( &s ) != SCR_ParseValue( &s ) ) {
				stack++;
				skip = qtrue; // skip to endif
			}
		} else if( !Q_stricmp( token, "ifbit" ) ) {
			if( skip || !(SCR_ParseValue( &s ) & SCR_ParseValue( &s )) ) {
				stack++;
				skip = qtrue; // skip to endif
			}
		} else if( !Q_stricmp( token, "ifle" ) ) {
			if( skip || SCR_ParseValue( &s ) > SCR_ParseValue( &s ) ) {
				stack++;
				skip = qtrue; // skip to endif
			}
		} else if( !Q_stricmp( token, "ifl" ) ) {
			if( skip || SCR_ParseValue( &s ) >= SCR_ParseValue( &s ) ) {
				stack++;
				skip = qtrue; // skip to endif
			}
		} else if( !Q_stricmp( token, "ifge" ) ) {
			if( skip || SCR_ParseValue( &s ) < SCR_ParseValue( &s ) ) {
				stack++;
				skip = qtrue; // skip to endif
			}
		} else if( !Q_stricmp( token, "ifg" ) ) {
			if( skip || SCR_ParseValue( &s ) <= SCR_ParseValue( &s ) ) {
				stack++;
				skip = qtrue; // skip to endif
			}
		} else if( !skip ) {
			if( !Q_stricmp( token, "xl" ) ) {
				x = SCR_ParseValue( &s );
			} else if( !Q_stricmp( token, "xr" ) ) {
				x = cgs.vidWidth + SCR_ParseValue( &s );
			} else if( !Q_stricmp( token, "xv" ) ) {
				x = cgs.vidWidth / 2 - 160 + SCR_ParseValue( &s );
			} else if( !Q_stricmp( token, "yt" ) ) {
				y = SCR_ParseValue( &s );
			} else if( !Q_stricmp( token, "yb" ) ) {
				y = cgs.vidHeight + SCR_ParseValue( &s );
			} else if( !Q_stricmp( token, "yv" ) ) {
				y = cgs.vidHeight / 2 - 120 + SCR_ParseValue( &s );
			} else if( !Q_stricmp( token, "size" ) ){
				w = SCR_ParseValue( &s );
				h = SCR_ParseValue( &s );
			} else if( !Q_stricmp( token, "color" ) ) {
				color[0] = atof( COM_Parse( &s ) ); clamp( color[0], 0, 1 );
				color[1] = atof( COM_Parse( &s ) ); clamp( color[1], 0, 1 );
				color[2] = atof( COM_Parse( &s ) ); clamp( color[2], 0, 1 );
				color[3] = atof( COM_Parse( &s ) ); clamp( color[3], 0, 1 );
			} else if( !Q_stricmp( token, "flashColor" ) ) {
				flashColor[0] = atof( COM_Parse( &s ) ); clamp( flashColor[0], 0, 1 );
				flashColor[1] = atof( COM_Parse( &s ) ); clamp( flashColor[1], 0, 1 );
				flashColor[2] = atof( COM_Parse( &s ) ); clamp( flashColor[2], 0, 1 );
				flashColor[3] = atof( COM_Parse( &s ) ); clamp( flashColor[3], 0, 1 );
			} else if( !Q_stricmp( token, "pic" ) ) {	// draw a pic from a stat number
				value = SCR_ParseValue( &s );
				if( value < 0 || value >= MAX_IMAGES )
					CG_Error( "Pic >= MAX_IMAGES" );
				if( cgs.configStrings[CS_IMAGES + value][0] )
					trap_R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, color, trap_R_RegisterPic(cgs.configStrings[CS_IMAGES+value]) );
			} else if( !Q_stricmp( token, "client" ) ) {// draw a deathmatch client block
				int		tag, score, ping, time;
				float	*color;

				x = cgs.vidWidth / 2 - 160 + SCR_ParseValue( &s );
				y = cgs.vidHeight / 2 - 120 + SCR_ParseValue( &s );
				tag = SCR_ParseValue( &s );
				value = SCR_ParseValue( &s );
				if( value >= MAX_CLIENTS || value < 0 )
					CG_Error( "client >= MAX_CLIENTS" );

				ci = &cgs.clientInfo[value];
				score = SCR_ParseValue( &s );
				ping = SCR_ParseValue( &s );
				time = SCR_ParseValue( &s );

				if( tag == 0 )			// player
					color = colorYellow;
				else if ( tag == 1 )	// killer
					color = colorRed;
				else
					color = colorWhite;

				CG_DrawString( x+32, y, ci->name, FONT_SMALL, color );
				CG_DrawString( x+32, y+SMALL_CHAR_HEIGHT, "Score: ", FONT_SMALL, color );
				CG_DrawString( x+32+7*SMALL_CHAR_WIDTH, y+SMALL_CHAR_HEIGHT, va( "%i", score ), FONT_SMALL, color );
				CG_DrawString( x+32, y+SMALL_CHAR_HEIGHT*2, va( "Ping:  %i", ping ), FONT_SMALL, color );
				CG_DrawString( x+32, y+SMALL_CHAR_HEIGHT*3, va( "Time:  %i", time ), FONT_SMALL, color );

				if( !ci->icon )
					ci->icon = cgs.basePSkin->icon;
				trap_R_DrawStretchPic( x, y, 32, 32, 0, 0, 1, 1, colorWhite, ci->icon );

			} else if( !Q_stricmp( token, "ctf" ) ) {// draw a ctf client block
				int		score, ping;
				char	block[80];

				x = cgs.vidWidth / 2 - 160 + SCR_ParseValue( &s );
				y = cgs.vidHeight / 2 - 120 + SCR_ParseValue( &s );

				value = SCR_ParseValue( &s );
				if( value >= MAX_CLIENTS || value < 0 )
					CG_Error ( "client >= MAX_CLIENTS" );

				ci = &cgs.clientInfo[value];
				score = SCR_ParseValue( &s );
				ping = SCR_ParseValue( &s );
				if( ping > 999 )
					ping = 999;

				Q_snprintfz( block, sizeof(block), "%3d %3d %-12.12s", score, ping, ci->name );

				if( value == cgs.playerNum )
					CG_DrawString( x, y, block, FONT_SMALL, colorRed );
				else
					CG_DrawString( x, y, block, FONT_SMALL, colorWhite );
			} else if( !Q_stricmp( token, "picn" ) ) {	// draw a pic from a name
				trap_R_DrawStretchPic ( x, y, w, h, 0, 0, 1, 1, colorWhite, trap_R_RegisterPic( COM_Parse ( &s ) ) );
			} else if( !Q_stricmp( token, "model" ) ) {	// draw a model from a stat number
				struct model_s *model;

				value = SCR_ParseValue( &s );
				if( value < 0 || value >= MAX_MODELS )
					CG_Error( "Model >= MAX_MODELS" );
				model = value > 1 ? CG_RegisterModel( cgs.configStrings[CS_MODELS+value] ) : NULL;
				CG_DrawHUDModel ( x, y, w, h, model, NULL, atof( COM_Parse ( &s ) ) );
			} else if( !Q_stricmp( token, "modeln" ) ) {// draw a model from a name
				struct model_s *model;
				struct shader_s *shader;

				model = CG_RegisterModel( COM_Parse( &s ) );
				token = COM_Parse( &s );
				shader = Q_stricmp( token, "NULL" ) ? trap_R_RegisterPic ( token ) : NULL;
				CG_DrawHUDModel ( x, y, w, h, model, shader, atof( COM_Parse ( &s ) ) );
			} else if( !Q_stricmp( token, "num" ) ) {	// draw a number
				width = SCR_ParseValue( &s );
				value = SCR_ParseValue( &s );
				CG_DrawHUDField ( x, y, flash ? flashColor : color, width, value );
			} else if( !Q_stricmp( token, "num2" ) ) {	// draw a number
				width = SCR_ParseValue( &s );
				value = SCR_ParseValue( &s );
				CG_DrawHUDField2 ( x, y, flash ? flashColor : color, width, value );
			} else if( !Q_stricmp( token, "frags" ) ) {	// frags number in a box
				vec4_t color;
				char str[32];

				Q_snprintfz( str, 32, "%2i", cg.frame.playerState.stats[STAT_FRAGS] );
				width = strlen( str ) * BIG_CHAR_WIDTH + 8;

				Vector4Set( color, 0, 0, 1, 0.33 );
				CG_FillRect( x - width, y, width, 24, color );
				trap_R_DrawStretchPic( x - width, y, width, 24, 0, 0, 1, 1, colorWhite, CG_MediaShader( cgs.media.shaderSelect ) );
				CG_DrawString( x - width + 4, y + 4, str, FONT_SHADOWED|FONT_BIG, colorWhite );
			} else if( !Q_stricmp( token, "stat_string" ) ) {
				index = SCR_ParseValue( &s );
				if( index < 0 || index >= MAX_CONFIGSTRINGS )
					CG_Error( "Bad stat_string index" );
				CG_DrawString( x, y, cgs.configStrings[index], FONT_SMALL, flash ? flashColor : color );
			} else if( !Q_stricmp( token, "cstring" ) )	{
				CG_DrawHUDString( COM_Parse( &s ), x, y, 320, FONT_SMALL, flash ? flashColor : color );
			} else if( !Q_stricmp( token, "string" ) ) {
				CG_DrawHUDString( COM_Parse( &s ), x, y, 0, FONT_SMALL, flash ? flashColor : color );
			} else if( !Q_stricmp( token, "rect" ) ) {
				value = SCR_ParseValue( &s );
				CG_DrawHUDRect( x, y, w, h, value, SCR_ParseValue( &s ), flash ? flashColor : color );
			}
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
void SCR_DrawStats( void )
{
	if( !cg.statusBar || !cg_showHUD->integer )
		return;
	SCR_ExecuteLayoutString ( cg.statusBar );
}

/*
================
SCR_DrawLayout
================
*/
void SCR_DrawLayout( void ) {
	SCR_ExecuteLayoutString ( cg.layout );
}

/*
================
SCR_DrawInventory
================
*/
#define	DISPLAY_ITEMS	17

void SCR_DrawInventory( void )
{
	int		i, j;
	int		num, selected_num, item;
	int		index[MAX_ITEMS];
	char	string[1024];
	int		x, y;
	char	binding[1024];
	char	*bind, *buf;
	int		selected;
	int		top;

	selected = cg.frame.playerState.stats[STAT_SELECTED_ITEM];

	num = 0;
	selected_num = 0;
	for( i = 0; i < MAX_ITEMS; i++ ) {
		if( i == selected )
			selected_num = num;
		if( cg.inventory[i] ) {
			index[num] = i;
			num++;
		}
	}

	// determine scroll point
	top = selected_num - DISPLAY_ITEMS / 2;
	if( num - top < DISPLAY_ITEMS )
		top = num - DISPLAY_ITEMS;
	if( top < 0 )
		top = 0;

	x = (cgs.vidWidth-256) / 2;
	y = (cgs.vidHeight-240) / 2;

	y += 24;
	x += 24;

	CG_DrawString( x, y,   "hotkey ### item", FONT_SMALL, colorWhite );
	CG_DrawString( x, y+SMALL_CHAR_HEIGHT, "------ --- ----", FONT_SMALL, colorWhite );

	y += SMALL_CHAR_HEIGHT * 2;
	for( i = top; i < num && i < top+DISPLAY_ITEMS; i++ ) {
		item = index[i];

		// search for a binding
		Q_snprintfz( binding, sizeof(binding), "use %s", cgs.configStrings[CS_ITEMS+item] );
		bind = "";

		for( j = 0; j < 256; j++ ) {
			buf = trap_Key_GetBindingBuf( j );
			if( buf && !Q_stricmp( buf, binding ) ) {
				bind = trap_Key_KeynumToString( j );
				break;
			}
		}

		Q_snprintfz( string, sizeof(string), "%6s %3i %s", bind, cg.inventory[item], cgs.configStrings[CS_ITEMS+item] );

		if ( item != selected ) {
			CG_DrawString ( x, y, string, FONT_SMALL, colorWhite );
		} else {	// draw a blinky cursor by the selected item
			if ( (int)(cg.realTime*10) & 1 )
				CG_DrawChar( x-SMALL_CHAR_WIDTH, y, FONT_SMALL, 15, colorWhite );
			CG_DrawString( x, y, string, FONT_SMALL, colorYellow );
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
void SCR_Draw2D( void )
{
	SCR_DrawRSpeeds ();

	SCR_DrawCrosshair ();

	SCR_DrawStats ();

	if( cg.frame.playerState.stats[STAT_LAYOUTS] & 1 )
		SCR_DrawLayout ();
	if( cg.frame.playerState.stats[STAT_LAYOUTS] & 2 )
		SCR_DrawInventory ();

	SCR_DrawNet ();
	SCR_CheckDrawCenterString ();

	SCR_DrawFPS ();

	SCR_DrawPause ();
}
