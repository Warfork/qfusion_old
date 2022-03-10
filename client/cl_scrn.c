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
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

/*

  full screen console
  put up loading plaque
  blanked background with loading plaque
  blanked background with menu
  cinematics
  full screen image for quit and victory

  end of unit intermissions

  */

#include "client.h"

float		scr_con_current;	// aproaches scr_conlines at scr_conspeed
float		scr_conlines;		// 0.0 to 1.0 lines of console to display

qboolean	scr_initialized;	// ready to draw

int			scr_draw_loading;

vrect_t		scr_vrect;		// position of render window on screen


cvar_t		*scr_viewsize;
cvar_t		*scr_conspeed;
cvar_t		*scr_centertime;
cvar_t		*scr_showturtle;
cvar_t		*scr_showpause;
cvar_t		*scr_showfps;
cvar_t		*scr_printspeed;

cvar_t		*scr_netgraph;
cvar_t		*scr_timegraph;
cvar_t		*scr_debuggraph;
cvar_t		*scr_graphheight;
cvar_t		*scr_graphscale;
cvar_t		*scr_graphshift;
cvar_t		*scr_drawall;
cvar_t		*scr_debugloading;

typedef struct
{
	int		x1, y1, x2, y2;
} dirty_t;

dirty_t		scr_dirty, scr_old_dirty[2];

struct shader_s *crosshair_shader;
int			crosshairX, crosshairY;

void SCR_TimeRefresh_f (void);
void SCR_Loading_f (void);


/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
==============
CL_AddNetgraph

A new packet was just parsed
==============
*/
void CL_AddNetgraph (void)
{
	int		i;
	int		in;
	int		ping;

	// if using the debuggraph for something else, don't
	// add the net lines
	if (scr_debuggraph->value || scr_timegraph->value)
		return;

	for (i=0 ; i<cls.netchan.dropped ; i++)
		SCR_DebugGraph (30, 0.655, 0.231, 0.169);

	for (i=0 ; i<cl.suppressCount ; i++)
		SCR_DebugGraph (30, 0.0f, 1.0f, 0.0);

	// see what the latency was on this packet
	in = cls.netchan.incoming_acknowledged & (CMD_BACKUP-1);
	ping = cls.realtime - cl.cmd_time[in];
	ping /= 30;
	if (ping > 30)
		ping = 30;
	SCR_DebugGraph (ping, 1.0f, 0.75, 0.06);
}


typedef struct
{
	float	value;
	vec4_t	color;
} graphsamp_t;

static	int			current;
static	graphsamp_t	values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value, float r, float g, float b)
{
	values[current].value = value;
	values[current].color[0] = r;
	values[current].color[1] = g;
	values[current].color[2] = b;
	values[current].color[3] = 1.0f;

	current++;
	current &= 1023;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;

	//
	// draw the graph
	//
	w = scr_vrect.width;

	x = scr_vrect.x;
	y = scr_vrect.y+scr_vrect.height;
	Draw_FillRect (x, y-scr_graphheight->value,
		w, scr_graphheight->value, colorMdGrey);

	for (a=0 ; a<w ; a++)
	{
		i = (current-1-a+1024) & 1023;
		v = values[i].value;
		v = v*scr_graphscale->value + scr_graphshift->value;
		
		if (v < 0)
			v += scr_graphheight->value * (1+(int)(-v/scr_graphheight->value));
		h = (int)v % (int)scr_graphheight->value;
		Draw_FillRect (x+w-1-a, y - h, 1, h, values[i].color);
	}
}

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
void SCR_CenterPrint (char *str)
{
	char	*s;
	char	line[64];
	int		i, j, l;

	Q_strncpyz (scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = str;
	while (*s)
	{
		if (*s == '\n')
			scr_center_lines++;
		s++;
	}

	// echo it to the console
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

	s = str;
	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (s[l] == '\n' || !s[l])
				break;
		for (i=0 ; i<(40-l)/2 ; i++)
			line[i] = ' ';

		for (j=0 ; j<l ; j++)
		{
			line[i++] = s[j];
		}

		line[i] = '\n';
		line[i+1] = 0;

		Com_Printf ("%s", line);

		while (*s && *s != '\n')
			s++;

		if (!*s)
			break;
		s++;		// skip the \n
	} while (1);
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_ClearNotify ();
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

	if (scr_center_lines <= 4)
		y = viddef.height*0.35;
	else
		y = 48;

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

		x = (viddef.width - length*SMALL_CHAR_WIDTH)/2;
		SCR_AddDirtyPoint (x, y);

		Draw_StringLen ( x, y, start, l, fontstyle, colorWhite );

		SCR_AddDirtyPoint (x, y+SMALL_CHAR_HEIGHT);

		y += SMALL_CHAR_HEIGHT;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_centertime_off -= cls.frametime;
	
	if (scr_centertime_off <= 0)
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
static void SCR_CalcVrect (void)
{
	int		size;

	// bound viewsize
	if (scr_viewsize->value < 40)
		Cvar_Set ("viewsize","40");
	if (scr_viewsize->value > 100)
		Cvar_Set ("viewsize","100");

	size = scr_viewsize->value;

	scr_vrect.width = viddef.width*size/100;
	scr_vrect.width &= ~7;

	scr_vrect.height = viddef.height*size/100;
	scr_vrect.height &= ~1;

	scr_vrect.x = (viddef.width - scr_vrect.width)/2;
	scr_vrect.y = (viddef.height - scr_vrect.height)/2;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize", scr_viewsize->value+10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize", scr_viewsize->value-10);
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE);
	scr_conspeed = Cvar_Get ("scr_conspeed", "3", 0);
	scr_showturtle = Cvar_Get ("scr_showturtle", "0", 0);
	scr_showpause = Cvar_Get ("scr_showpause", "1", 0);
	scr_showfps = Cvar_Get ("scr_showfps", "0", CVAR_ARCHIVE);
	scr_centertime = Cvar_Get ("scr_centertime", "2.5", 0);
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", 0);
	scr_netgraph = Cvar_Get ("netgraph", "0", 0);
	scr_timegraph = Cvar_Get ("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get ("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get ("graphheight", "32", 0);
	scr_graphscale = Cvar_Get ("graphscale", "1", 0);
	scr_graphshift = Cvar_Get ("graphshift", "0", 0);
	scr_drawall = Cvar_Get ("scr_drawall", "0", 0);
	scr_debugloading = Cvar_Get ("scr_debugloading", "0", 0);

//
// register our commands
//
	Cmd_AddCommand ("timerefresh",SCR_TimeRefresh_f);
	Cmd_AddCommand ("loading",SCR_Loading_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	scr_initialized = true;
}


/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged 
		< CMD_BACKUP-1)
		return;

	Draw_StretchPic (scr_vrect.x+64, scr_vrect.y, 32, 32, 0, 0, 1, 1, colorWhite, R_RegisterPic("gfx/2d/net") );
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause (void)
{
	if (!scr_showpause->value)		// turn off for screenshots
		return;

	if (!cl_paused->value)
		return;

	Draw_CenteredPropString ( viddef.height / 2, "PAUSED", FONT_BIG, colorRed );
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

	if ( !cls.frametime || cls.state < ca_connected )
		return;
	if ( !scr_showfps->value )
		return;

	if ( scr_showfps->value == 2 )
	{
		t = curtime * 0.001;
		if ((t - oldtime) >= 0.25) {	// updates 4 times a second
			fps = (cls.framecount - oldframecount) / (t - oldtime) + 0.5;
			oldframecount = cls.framecount;
			oldtime = t;
		}
	}
	else
		fps = (int)(1.0f / cls.frametime);

	Com_sprintf ( s, sizeof( s ), "%3dfps", fps );
	width = strlen(s)*BIG_CHAR_WIDTH;
	x = viddef.width - 5 - width;
	Draw_String ( x, 2, s, FONT_BIG|FONT_SHADOWED, colorWhite );
	SCR_AddDirtyPoint (x, 2);
	SCR_AddDirtyPoint (x + width - 1, 2 + BIG_CHAR_HEIGHT - 1);
}


//=============================================================================

/*
==================
SCR_RunConsole

Scroll it up or down
==================
*/
void SCR_RunConsole (void)
{
// decide on the height of the console
	if (cls.key_dest == key_console)
		scr_conlines = 0.5;		// half screen
	else
		scr_conlines = 0;				// none visible
	
	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed->value*cls.frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed->value*cls.frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	Con_CheckResize ();

	if ( cls.state == ca_disconnected ) {
		if ( scr_con_current )
			Con_DrawConsole (scr_con_current);
		return;
	}

	if ( cls.state == ca_active && !cl.refresh_prepped )
		return;

	if ( scr_con_current )
	{
		Con_DrawConsole (scr_con_current);
		return;
	}

	if ( cls.state == ca_active && (cls.key_dest == key_game
		|| cls.key_dest == key_message) ) {
		Con_DrawNotify ();	// only draw notify in game
	}
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	static char str[64];

	if ( cls.state != ca_connecting && cl.levelshot[0]
		&& cl.configstrings[CS_MAPNAME][0])
	{	// draw a level shot
		Draw_StretchPic ( 0, 0, viddef.width, viddef.height, 
			0, 0, 1, 1, colorWhite, R_RegisterPic ( cl.levelshot ) );
		Draw_StretchPic ( 0, 0, viddef.width, viddef.height, 
			0, 0, 2.5, 2, colorWhite, R_RegisterPic ( "levelShotDetail" ) );
	} else {	// draw menu background
		Draw_StretchPic ( 0, 0, viddef.width, viddef.height, 
			0, 0, 1, 1, colorWhite, R_RegisterPic ( "menuback" ) );
	}

	Com_sprintf ( str, sizeof(str), "Connecting to %s", cls.servername );
	Draw_CenteredPropString ( 64, str, FONT_SMALL|FONT_SHADOWED, colorWhite );

	if ( cls.state != ca_connecting && cl.configstrings[CS_MAPNAME][0] ) {
		// draw map name
		Com_sprintf ( str, sizeof(str), "Loading %s", cl.configstrings[CS_MAPNAME] );
		Draw_CenteredPropString ( 16, str, FONT_BIG|FONT_SHADOWED, colorWhite );

		// what we're loading at the moment
		if (cl.loadingstring[0]) {
			if (cl.loadingstring[0] == '-' && !cl.loadingstring[1])
				strcpy (str, "awaiting snapshot...");
			else
				Com_sprintf ( str, sizeof(str), "loading... %s", cl.loadingstring);
			Draw_CenteredPropString ( 96, str, FONT_SMALL|FONT_SHADOWED, colorWhite );
		}

		if ( cl.checkname[0] && !cls.download ) {
			int maxlen;
			char prefix[] = "filename: ";

			maxlen = (viddef.width - 32)/BIG_CHAR_WIDTH - (sizeof(prefix)-1);
			clamp (maxlen, 3, sizeof(str)-1 - (sizeof(prefix)-1));

			strcpy (str, prefix);
			if ( strlen(cl.checkname) > maxlen ) {
				strcat (str, "...");
				strcat (str, cl.checkname + strlen(cl.checkname) - maxlen + 3);
			} else {
				strcat (str, cl.checkname);
			}

			Draw_String ( 16, viddef.height - 20, str, FONT_BIG|FONT_SHADOWED, colorWhite );
		}

		// level name ("message")
		Draw_CenteredPropString ( 150, cl.configstrings[CS_MESSAGE], FONT_SMALL|FONT_SHADOWED, colorWhite );
	}

//ZOID
	// draw the download bar
	// figure out width
	if (cls.download) {
		char	*text;
		int		i, x, y, j, n;
		char	dlbar[1024];

		if ((text = strrchr(cls.downloadname, '/')) != NULL)
			text++;
		else
			text = cls.downloadname;

		x = con.linewidth - ((con.linewidth * 7) / 40);
		y = x - strlen(text) - SMALL_CHAR_WIDTH;
		i = con.linewidth / 3;

		if (strlen(text) > i) {
			y = x - i - 11;
			strncpy (dlbar, text, i);
			dlbar[i] = 0;
			strcat (dlbar, "...");
		} else {
			strcpy (dlbar, text);
		}

		strcat (dlbar, ": ");
		i = strlen (dlbar);
		dlbar[i++] = '\x80';

		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;
			
		for (j = 0; j < y; j++)
		{
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		}

		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		sprintf (dlbar + strlen(dlbar), " %02d%%", cls.downloadpercent);

		// draw it
		Draw_String (16, viddef.height - 20, dlbar, FONT_SMALL, colorWhite);
	}
//ZOID
}

//=============================================================================

/*
================
CL_LoadingString
================
*/
void CL_LoadingString (char *str)
{
	Q_strncpyz (cl.loadingstring, str, sizeof(cl.loadingstring));
	SCR_UpdateScreen ();
}

/*
================
CL_LoadingFilename
================
*/
void CL_LoadingFilename (char *str)
{
	if (!scr_debugloading->value) {
		cl.checkname[0] = 0;
		return;
	}

	Q_strncpyz (cl.checkname, str, sizeof(cl.checkname));
	SCR_UpdateScreen ();
}

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds ();
	cl.sound_prepped = false;		// don't play ambients

	if (cls.disable_screen)
		return;
	if (developer->value)
		return;
	if (cls.state == ca_disconnected)
		return;	// if at console, don't bring up the plaque
	if (cls.key_dest == key_console)
		return;
	if (cl.cin.time > 0)
		scr_draw_loading = 2;	// clear to black first
	else
		scr_draw_loading = 1;
	SCR_UpdateScreen ();
	cls.disable_screen = Sys_Milliseconds ();
	cls.disable_servercount = cl.servercount;
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque (void)
{
	cls.disable_screen = 0;
	Con_ClearNotify ();
}

/*
================
SCR_Loading_f
================
*/
void SCR_Loading_f (void)
{
	SCR_BeginLoadingPlaque ();
}

/*
================
SCR_TimeRefresh_f
================
*/
void SCR_TimeRefresh_f (void)
{
	int		i;
	int		start, stop;
	float	time;

	if ( cls.state != ca_active )
		return;

	start = Sys_Milliseconds ();

	if (Cmd_Argc() == 2)
	{	// run without page flipping
		R_BeginFrame( 0 );
		for (i=0 ; i<128 ; i++)
		{
			cl.refdef.viewangles[1] = i/128.0*360.0;
			R_RenderFrame (&cl.refdef);
		}
		GLimp_EndFrame();
	}
	else
	{
		for (i=0 ; i<128 ; i++)
		{
			cl.refdef.viewangles[1] = i/128.0*360.0;

			R_BeginFrame( 0 );
			R_RenderFrame (&cl.refdef);
			GLimp_EndFrame();
		}
	}

	stop = Sys_Milliseconds ();
	time = (stop-start)/1000.0;
	Com_Printf ("%f seconds (%f fps)\n", time, 128/time);
}

/*
=================
SCR_AddDirtyPoint
=================
*/
void SCR_AddDirtyPoint (int x, int y)
{
	if (x < scr_dirty.x1)
		scr_dirty.x1 = x;
	if (x > scr_dirty.x2)
		scr_dirty.x2 = x;
	if (y < scr_dirty.y1)
		scr_dirty.y1 = y;
	if (y > scr_dirty.y2)
		scr_dirty.y2 = y;
}

void SCR_DirtyScreen (void)
{
	SCR_AddDirtyPoint (0, 0);
	SCR_AddDirtyPoint (viddef.width-1, viddef.height-1);
}

/*
==============
SCR_TileClearRect

This repeats tile graphic to fill the screen around a sized down
refresh window.
==============
*/
void SCR_TileClearRect (int x, int y, int w, int h, struct shader_s *shader)
{
	float iw, ih;

	iw = 1.0f / 64.0;
	ih = 1.0f / 64.0;

	Draw_StretchPic ( x, y, w, h, x*iw, y*ih, (x+w)*iw, (y+h)*ih, colorWhite, shader );
}

/*
==============
SCR_TileClear

Clear any parts of the tiled background that were drawn on last frame
==============
*/
void SCR_TileClear (void)
{
	int		i;
	int		top, bottom, left, right;
	dirty_t	clear;
	struct shader_s *gfxBackTileShader;

	if (scr_drawall->value)
		SCR_DirtyScreen ();	// for power vr or broken page flippers...

	if (scr_con_current == 1.0)
		return;		// full screen console
	if (scr_viewsize->value == 100)
		return;		// full screen rendering
	if (cl.cin.time > 0)
		return;		// full screen cinematic

	// erase rect will be the union of the past three frames
	// so tripple buffering works properly
	clear = scr_dirty;
	for (i=0 ; i<2 ; i++)
	{
		if (scr_old_dirty[i].x1 < clear.x1)
			clear.x1 = scr_old_dirty[i].x1;
		if (scr_old_dirty[i].x2 > clear.x2)
			clear.x2 = scr_old_dirty[i].x2;
		if (scr_old_dirty[i].y1 < clear.y1)
			clear.y1 = scr_old_dirty[i].y1;
		if (scr_old_dirty[i].y2 > clear.y2)
			clear.y2 = scr_old_dirty[i].y2;
	}

	scr_old_dirty[1] = scr_old_dirty[0];
	scr_old_dirty[0] = scr_dirty;

	scr_dirty.x1 = 9999;
	scr_dirty.x2 = -9999;
	scr_dirty.y1 = 9999;
	scr_dirty.y2 = -9999;

	// don't bother with anything convered by the console)
	top = scr_con_current*viddef.height;
	if (top >= clear.y1)
		clear.y1 = top;

	if (clear.y2 <= clear.y1)
		return;		// nothing disturbed

	top = scr_vrect.y;
	bottom = top + scr_vrect.height-1;
	left = scr_vrect.x;
	right = left + scr_vrect.width-1;

	if ( (clear.y1 >= top) && (clear.y2 <= bottom) &&
		(clear.x1 >= left) && (clear.x2 <= right) ) {
		return;
	}

	gfxBackTileShader = R_RegisterPic ( "gfx/2d/backtile" );

	if (clear.y1 < top)
	{	// clear above view screen
		i = clear.y2 < top - 1 ? clear.y2 : top - 1;
		SCR_TileClearRect ( clear.x1, clear.y1,
			clear.x2 - clear.x1 + 1, i - clear.y1 + 1, gfxBackTileShader );
		clear.y1 = top;
	}
	if (clear.y2 > bottom)
	{	// clear below view screen
		i = clear.y1 > bottom + 1 ? clear.y1 : bottom + 1;
		SCR_TileClearRect ( clear.x1, i,
			clear.x2 - clear.x1 + 1, clear.y2 - i + 1, gfxBackTileShader );
		clear.y2 = bottom;
	}
	if (clear.x1 < left)
	{	// clear left of view screen
		i = clear.x2 < left - 1 ? clear.x2 : left - 1;
		SCR_TileClearRect ( clear.x1, clear.y1,
			i-clear.x1 + 1, clear.y2 - clear.y1 + 1, gfxBackTileShader );
		clear.x1 = left;
	}
	if (clear.x2 > right)
	{	// clear left of view screen
		i = clear.x1 > right+1 ? clear.x1 : right+1;
		SCR_TileClearRect ( i, clear.y1,
			clear.x2 - i + 1, clear.y2 - clear.y1 + 1, gfxBackTileShader );
		clear.x2 = right;
	}
}


//===============================================================


#define STAT_MINUS		10	// num frame for '-' stats digit
char		*sb_nums[11] = 
{
	"gfx/2d/numbers/zero_32b", "gfx/2d/numbers/one_32b", 
	"gfx/2d/numbers/two_32b", "gfx/2d/numbers/three_32b", 
	"gfx/2d/numbers/four_32b", "gfx/2d/numbers/five_32b",
	"gfx/2d/numbers/six_32b", "gfx/2d/numbers/seven_32b",
	"gfx/2d/numbers/eight_32b", "gfx/2d/numbers/nine_32b", 
	"gfx/2d/numbers/minus_32b"
};


void DrawHUDString (char *string, int x, int y, int centerwidth, int fontstyle, vec4_t color)
{
	int		margin;
	int		length, l;
	int		width, height;

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

	margin = x;

	while (*string)
	{
		// scan out one line of text from the string
		length = l = 0;
		while (*string && *string != '\n') {
			if ( Q_IsColorString (string) ) {
				l += 2;
			} else {
				l++;
				length++;
			}
		}

		if (centerwidth)
			x = margin + (centerwidth - length*width)/2;
		else
			x = margin;

		Draw_StringLen ( x, y, string, l, fontstyle, color );

		if (*string)
		{
			string++;	// skip the \n
			x = margin;
			y += height;
		}
	}
}


/*
==============
SCR_DrawField
==============
*/
void SCR_DrawField (int x, int y, float *color, int width, int value)
{
	char	num[16], *ptr;
	int		l;
	int		frame;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	SCR_AddDirtyPoint (x, y);
	SCR_AddDirtyPoint (x+width*32+2, y+31);

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;
	x += 2 + 32*(width - l);

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		Draw_StretchPic ( x, y, 32, 32, 0, 0, 1, 1, color, R_RegisterPic(sb_nums[frame]) );
		x += 32;
		ptr++;
		l--;
	}
}


/*
===============
SCR_TouchPics

Allows rendering code to cache all needed sbar graphics
===============
*/
void SCR_TouchPics (void)
{
	int		i;

	for (i = 0; i < 11; i++)
		R_RegisterPic (sb_nums[i]);

	if (crosshair->value)
	{
		if (crosshair->value > NUM_CROSSHAIRS || crosshair->value < 0)
			crosshair->value = NUM_CROSSHAIRS;

		crosshairX = max (0, (int)crosshair_x->value);
		crosshairY = max (0, (int)crosshair_y->value);

		if (crosshair_size->value <= 0)
			crosshair_shader = NULL;
		else
			crosshair_shader = R_RegisterShader( va( "gfx/2d/crosshair%c", 'a'+(int)(crosshair->value) ) );
	}
}

/*
================
SCR_ExecuteLayoutString 

================
*/
void SCR_ExecuteLayoutString (char *s)
{
	int		x, y, w, h;
	int		value;
	char	*token;
	int		width;
	int		index;
	clientinfo_t	*ci;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	if (!s[0])
		return;

	x = 0;
	y = 0;
	width = 3;

	while (s)
	{
		token = COM_Parse (&s);
		if (!strcmp(token, "xl"))
		{
			token = COM_Parse (&s);
			x = atoi(token);
			continue;
		}
		if (!strcmp(token, "xr"))
		{
			token = COM_Parse (&s);
			x = viddef.width + atoi(token);
			continue;
		}
		if (!strcmp(token, "xv"))
		{
			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			continue;
		}

		if (!strcmp(token, "yt"))
		{
			token = COM_Parse (&s);
			y = atoi(token);
			continue;
		}
		if (!strcmp(token, "yb"))
		{
			token = COM_Parse (&s);
			y = viddef.height + atoi(token);
			continue;
		}
		if (!strcmp(token, "yv"))
		{
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			continue;
		}

		if (!strcmp(token, "pic"))
		{	// draw a pic from a stat number
			token = COM_Parse (&s);
			w = atoi (token);
			token = COM_Parse (&s);
			h = atoi (token);
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			if (value >= MAX_IMAGES)
				Com_Error (ERR_DROP, "Pic >= MAX_IMAGES");
			if (cl.configstrings[CS_IMAGES+value])
			{
				SCR_AddDirtyPoint (x, y);
				SCR_AddDirtyPoint (x+w-1, y+h-1);
				Draw_StretchPic (x, y, w, h, 0, 0, 1, 1, colorWhite, R_RegisterPic(cl.configstrings[CS_IMAGES+value]) );
			}
			continue;
		}

		if (!strcmp(token, "client"))
		{	// draw a deathmatch client block
			int		tag, score, ping, time;
			float	*color;

			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+159, y+31);

			token = COM_Parse (&s);
			tag = atoi(token);

			token = COM_Parse (&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");
			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi(token);

			token = COM_Parse (&s);
			ping = atoi(token);

			token = COM_Parse (&s);
			time = atoi(token);

			if ( tag == 0 ) {			// player
				color = colorYellow;
			} else if ( tag == 1 ) {	// killer
				color = colorRed;
			} else {
				color = colorWhite;
			}

			Draw_String ( x+32, y, ci->name, FONT_SMALL, color );
			Draw_String ( x+32, y+SMALL_CHAR_HEIGHT, "Score: ", FONT_SMALL, color );
			Draw_String ( x+32+7*SMALL_CHAR_WIDTH, y+SMALL_CHAR_HEIGHT, va("%i", score), FONT_SMALL, color );
			Draw_String ( x+32, y+SMALL_CHAR_HEIGHT*2, va("Ping:  %i", ping), FONT_SMALL, color );
			Draw_String ( x+32, y+SMALL_CHAR_HEIGHT*3, va("Time:  %i", time), FONT_SMALL, color );

			if (!ci->icon)
				ci = &cl.baseclientinfo;
			Draw_StretchPic (x, y, 32, 32, 0, 0, 1, 1, colorWhite, R_RegisterPic(ci->iconname) );
			continue;
		}

		if (!strcmp(token, "ctf"))
		{	// draw a ctf client block
			int		score, ping;
			char	block[80];

			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+159, y+31);

			token = COM_Parse (&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");
			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi(token);

			token = COM_Parse (&s);
			ping = atoi(token);
			if (ping > 999)
				ping = 999;

			sprintf(block, "%3d %3d %-12.12s", score, ping, ci->name);
		
			if (value == cl.playernum)
				Draw_String ( x, y, block, FONT_SMALL, colorRed );
			else
				Draw_String ( x, y, block, FONT_SMALL, colorWhite );

			continue;
		}

		if (!strcmp(token, "picn"))
		{	// draw a pic from a name
			token = COM_Parse (&s);
			w = atoi (token);
			token = COM_Parse (&s);
			h = atoi (token);
			token = COM_Parse (&s);
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+w-1, y+h-1);
			Draw_StretchPic (x, y, w, h, 0, 0, 1, 1, colorWhite, R_RegisterPic(token) );
			continue;
		}

		if (!strcmp(token, "num"))
		{	// draw a number
			token = COM_Parse (&s);
			width = atoi(token);
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			SCR_DrawField (x, y, colorWhite, width, value);
			continue;
		}

		if (!strcmp(token, "hnum"))
		{	// health number
			float	*color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_HEALTH];

			if (value > 25)
				color = colorWhite;							// green
			else if (value > 0)
				color = (cl.frame.serverframe>>2) & 1 ? colorRed : colorWhite;		// flash
			else
				color = colorRed;

			SCR_DrawField (x, y, color, width, value);
			continue;
		}

		if (!strcmp(token, "anum"))
		{	// ammo number
			float *color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_AMMO];
			if (value > 5)
				color = colorWhite;	// green
			else if (value >= 0)
				color = (cl.frame.serverframe>>2) & 1 ? colorRed : colorWhite;		// flash
			else
				continue;	// negative number = don't show

			SCR_DrawField (x, y, color, width, value);
			continue;
		}

		if (!strcmp(token, "rnum"))
		{	// armor number
			width = 3;
			value = cl.frame.playerstate.stats[STAT_ARMOR];
			if (value < 1)
				continue;

			SCR_DrawField (x, y, colorWhite, width, value);
			continue;
		}

		if (!strcmp(token, "frags"))
		{	// frags number in a box
			vec4_t color;
			char str[32];

			Com_sprintf (str, 32, "%2i", cl.frame.playerstate.stats[STAT_FRAGS]);
			width = strlen(str)*BIG_CHAR_WIDTH + 8;

			color[0] = 0;
			color[1] = 0;
			color[2] = 1;
			color[3] = 0.33;
			Draw_FillRect (x - width, y, width, 24, color);
			Draw_StretchPic (x - width, y, width, 24, 0, 0, 1, 1, colorWhite, R_RegisterPic("gfx/2d/select") );
			Draw_String (x - width + 4, y + 4, str, FONT_SHADOWED|FONT_BIG, colorWhite);
			SCR_AddDirtyPoint (x - width, y);
			SCR_AddDirtyPoint (x - 1, y + BIG_CHAR_HEIGHT - 1);
			continue;
		}

		if (!strcmp(token, "stat_string"))
		{
			token = COM_Parse (&s);
			index = atoi(token);
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");
			index = cl.frame.playerstate.stats[index];
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");
			Draw_String ( x, y, cl.configstrings[index], FONT_SMALL, colorWhite );
			continue;
		}

		if (!strcmp(token, "cstring"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 320, FONT_SMALL, colorWhite);
			continue;
		}

		if (!strcmp(token, "string"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 0, FONT_SMALL, colorWhite);
			continue;
		}

		if (!strcmp(token, "if"))
		{	// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			if (!value)
			{	// skip to endif
				while (s && strcmp(token, "endif") )
				{
					token = COM_Parse (&s);
				}
			}
			continue;
		}

		if (!strcmp(token, "ifeq"))
		{	// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			token = COM_Parse (&s);
			if (value != atoi(token))
			{	// skip to endif
				while (s && strcmp(token, "endif") )
				{
					token = COM_Parse (&s);
				}
			}
			continue;
		}

		if (!strcmp(token, "ifbit"))
		{	// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			token = COM_Parse (&s);
			if (!(value & atoi(token)))
			{	// skip to endif
				while (s && strcmp(token, "endif") )
				{
					token = COM_Parse (&s);
				}
			}
			continue;
		}

		if (!strcmp(token, "ifle"))
		{	// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			token = COM_Parse (&s);
			if (value > atoi(token))
			{	// skip to endif
				while (s && strcmp(token, "endif") )
				{
					token = COM_Parse (&s);
				}
			}
			continue;
		}

		if (!strcmp(token, "ifl"))
		{	// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			token = COM_Parse (&s);
			if (value >= atoi(token))
			{	// skip to endif
				while (s && strcmp(token, "endif") )
				{
					token = COM_Parse (&s);
				}
			}
			continue;
		}

		if (!strcmp(token, "ifge"))
		{	// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			token = COM_Parse (&s);
			if (value < atoi(token))
			{	// skip to endif
				while (s && strcmp(token, "endif") )
				{
					token = COM_Parse (&s);
				}
			}
			continue;
		}

		if (!strcmp(token, "ifg"))
		{	// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			token = COM_Parse (&s);
			if (value <= atoi(token))
			{	// skip to endif
				while (s && strcmp(token, "endif") )
				{
					token = COM_Parse (&s);
				}
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
	if ( !cl.statusbar ) {
		return;
	}

	SCR_ExecuteLayoutString ( cl.statusbar );
}


/*
================
SCR_DrawLayout
================
*/
void SCR_DrawLayout (void)
{
	if ( !cl.frame.playerstate.stats[STAT_LAYOUTS] ) {
		return;
	}

	SCR_ExecuteLayoutString ( cl.layout );
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

	selected = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];

	num = 0;
	selected_num = 0;
	for (i=0 ; i<MAX_ITEMS ; i++)
	{
		if (i==selected)
			selected_num = num;
		if (cl.inventory[i])
		{
			index[num] = i;
			num++;
		}
	}

	// determine scroll point
	top = selected_num - DISPLAY_ITEMS/2;
	if (num - top < DISPLAY_ITEMS)
		top = num - DISPLAY_ITEMS;
	if (top < 0)
		top = 0;

	x = (viddef.width-256)/2;
	y = (viddef.height-240)/2;

	// repaint everything next frame
	SCR_DirtyScreen ();

	y += 24;
	x += 24;

	Draw_String ( x, y,   "hotkey ### item", FONT_SMALL, colorWhite );
	Draw_String ( x, y+SMALL_CHAR_HEIGHT, "------ --- ----", FONT_SMALL, colorWhite );

	y += SMALL_CHAR_HEIGHT * 2;
	for (i=top ; i<num && i < top+DISPLAY_ITEMS ; i++)
	{
		item = index[i];
		// search for a binding
		Com_sprintf (binding, sizeof(binding), "use %s", cl.configstrings[CS_ITEMS+item]);
		bind = "";
		for (j=0 ; j<256 ; j++)
			if (keybindings[j] && !Q_stricmp (keybindings[j], binding))
			{
				bind = Key_KeynumToString(j);
				break;
			}

		Com_sprintf (string, sizeof(string), "%6s %3i %s", bind, cl.inventory[item],
			cl.configstrings[CS_ITEMS+item] );

		if (item != selected)
			Draw_String ( x, y, string, FONT_SMALL, colorWhite );
		else	// draw a blinky cursor by the selected item
		{
			if ( (int)(cls.realtime*10) & 1)
				Draw_Char ( x-SMALL_CHAR_WIDTH, y, FONT_SMALL, 15, colorWhite );

			Draw_String ( x, y, string, FONT_SMALL, colorYellow );
		}

		y += SMALL_CHAR_HEIGHT;
	}
}


//=======================================================

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen (void)
{
	int numframes;
	int i;
	float separation[2];

	// if the screen is disabled (loading plaque is up, or vid mode changing)
	// do nothing at all
	if (cls.disable_screen)
	{
		if (Sys_Milliseconds() - cls.disable_screen > 120000)
		{
			cls.disable_screen = 0;
			Com_Printf ("Loading plaque timed out.\n");
		}
		return;
	}

	if (!scr_initialized || !con.initialized)
		return;				// not initialized yet

	/*
	** range check cl_camera_separation so we don't inadvertently fry someone's
	** brain
	*/
	if ( cl_stereo_separation->value > 1.0 )
		Cvar_SetValue( "cl_stereo_separation", 1.0 );
	else if ( cl_stereo_separation->value < 0 )
		Cvar_SetValue( "cl_stereo_separation", 0.0 );

	if ( cl_stereo->value )
	{
		numframes = 2;
		separation[0] = -cl_stereo_separation->value / 2;
		separation[1] =  cl_stereo_separation->value / 2;
	}		
	else
	{
		separation[0] = 0;
		separation[1] = 0;
		numframes = 1;
	}

	GLimp_EndFrame();

	for ( i = 0; i < numframes; i++ )
	{
		R_BeginFrame( separation[i] );

		if (scr_draw_loading == 2)
		{	// loading plaque over black screen
			scr_draw_loading = false;
		}
		// if a cinematic is supposed to be running, handle menus
		// and console specially
		else if (cl.cin.time > 0)
		{
			if (cls.key_dest == key_menu)
			{
				UI_Refresh ( cls.frametime );
			}
			else if (cls.key_dest == key_console)
			{
				UI_Refresh ( cls.frametime );

				SCR_DrawConsole ();
			}
			else
			{
				SCR_DrawCinematic ();
			}
		}
		else if ( cls.state != ca_disconnected &&
			(cls.state != ca_active || !cl.refresh_prepped) )
		{
			UI_Refresh ( cls.frametime );
			SCR_DrawLoading ();
			SCR_DrawConsole ();
		}
		else
		{
			// do 3D refresh drawing, and then update the screen
			SCR_CalcVrect ();

			// clear any dirty part of the background
			SCR_TileClear ();

			V_RenderView ( separation[i] );

			SCR_DrawStats ();
			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
				SCR_DrawLayout ();
			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 2)
				SCR_DrawInventory ();

			SCR_DrawNet ();
			SCR_CheckDrawCenterString ();

			SCR_DrawFPS ();

			if (scr_timegraph->value)
				SCR_DebugGraph (cls.frametime*300, 0, 0, 0);

			if (scr_debuggraph->value || scr_timegraph->value || scr_netgraph->value)
				SCR_DrawDebugGraph ();

			SCR_DrawPause ();

			UI_Refresh ( cls.frametime );

			SCR_DrawConsole ();
		}
	}

	R_ApplySoftwareGamma ();

	R_Flush ();
}
