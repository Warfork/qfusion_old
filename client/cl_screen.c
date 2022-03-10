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

vrect_t		scr_vrect;			// position of render window on screen

cvar_t		*scr_conspeed;
cvar_t		*scr_netgraph;
cvar_t		*scr_timegraph;
cvar_t		*scr_debuggraph;
cvar_t		*scr_graphheight;
cvar_t		*scr_graphscale;
cvar_t		*scr_graphshift;
cvar_t		*scr_debugloading;
cvar_t		*scr_viewsize;

void SCR_TimeRefresh_f (void);
void SCR_Loading_f (void);

/*
===============================================================================

STRINGS DRAWING

===============================================================================
*/

/*
================
Draw_Char

Draws one graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Char ( int x, int y, int num, vec4_t color )
{
	float	frow, fcol;
	int		width, height;

	num &= 255;
	
	if ( (num&127) == 32 )
		return;		// space

	width = SMALL_CHAR_WIDTH;
	height = SMALL_CHAR_HEIGHT;

	if (y <= -height)
		return;			// totally off screen

	frow = (num>>4)*0.0625f;
	fcol = (num&15)*0.0625f;

	Draw_StretchPic ( x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, color, cls.charsetShader );
}

/*
=============
Draw_String
=============
*/
void Draw_String ( int x, int y, char *str, vec4_t color )
{
	int		num;
	float	frow, fcol;
	int		width, height;
	vec4_t	scolor;

	width = SMALL_CHAR_WIDTH;
	height = SMALL_CHAR_HEIGHT;

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

		if ( (num&127) != 32 ) {		// not a space
			frow = (num>>4)*0.0625f;
			fcol = (num&15)*0.0625f;
			Draw_StretchPic ( x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, scolor, cls.charsetShader );

		}

		x += width;
	}
}

/*
=============
Draw_StringLen

Same as Draw_String, but draws "len" bytes at max
=============
*/
void Draw_StringLen ( int x, int y, char *str, int len, vec4_t color )
{
	char saved_byte;

	if (len < 0)
	{
		Draw_String ( x, y, str, color );
		return;
	}

	saved_byte = str[len];
	str[len] = 0;
	Draw_String ( x, y, str, color );
	str[len] = saved_byte;
}

/*
=============
Draw_FillRect

Fills a box of pixels with a single color
=============
*/
void Draw_FillRect ( int x, int y, int w, int h, vec4_t color ) {
	Draw_StretchPic ( x, y, w, h, 0, 0, 1, 1, color, cls.whiteShader );
}

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
	in = cls.netchan.incoming_acknowledged & CMD_MASK;
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

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	scr_viewsize = Cvar_Get ("scr_viewSize", "100", CVAR_ARCHIVE);
	scr_conspeed = Cvar_Get ("scr_conspeed", "3", 0);
	scr_netgraph = Cvar_Get ("netgraph", "0", 0);
	scr_timegraph = Cvar_Get ("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get ("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get ("graphheight", "32", 0);
	scr_graphscale = Cvar_Get ("graphscale", "1", 0);
	scr_graphshift = Cvar_Get ("graphshift", "0", 0);

//
// register our commands
//
	Cmd_AddCommand ("timerefresh", SCR_TimeRefresh_f);
	Cmd_AddCommand ("loading", SCR_Loading_f);

	scr_initialized = qtrue;

	SCR_RegisterConsoleMedia ();
}

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
	if (scr_viewsize->value < 40)
		Cvar_Set ("viewsize", "40");
	if (scr_viewsize->value > 100)
		Cvar_Set ("viewsize", "100");

	size = scr_viewsize->value;

	scr_vrect.width = viddef.width*size/100;
	scr_vrect.width &= ~7;

	scr_vrect.height = viddef.height*size/100;
	scr_vrect.height &= ~1;

	scr_vrect.x = (viddef.width - scr_vrect.width)/2;
	scr_vrect.y = (viddef.height - scr_vrect.height)/2;
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
	if ( scr_con_current ) {
		Con_DrawConsole (scr_con_current);
		return;
	}

	if ( cls.state == ca_active && (cls.key_dest == key_game
		|| cls.key_dest == key_message) ) {
		Con_DrawNotify ();	// only draw notify in game
	}
}

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds ();
	cl.sound_prepped = qfalse;		// don't play ambients

#if 0
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
#endif
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
	refdef_t refdef;

	if ( cls.state != ca_active )
		return;

	memset ( &refdef, 0, sizeof(refdef_t) );

	refdef.width = viddef.width;
	refdef.height = viddef.height;
	refdef.fov_x = 90;
	refdef.fov_y = CalcFov ( refdef.fov_x, refdef.width, refdef.height );
	VectorMA ( cl.frame.playerState.viewoffset, (1.0/16.0), cl.frame.playerState.pmove.origin, refdef.vieworg );

	start = Sys_Milliseconds ();

	if (Cmd_Argc() == 2)
	{	// run without page flipping
		R_BeginFrame( 0 );
		for (i=0 ; i<128 ; i++)
		{
			refdef.viewangles[1] = i/128.0*360.0;
			R_RenderFrame (&refdef);
		}
		GLimp_EndFrame();
	}
	else
	{
		for (i=0 ; i<128 ; i++)
		{
			refdef.viewangles[1] = i/128.0*360.0;

			R_BeginFrame( 0 );
			R_RenderFrame (&refdef);
			GLimp_EndFrame();
		}
	}

	stop = Sys_Milliseconds ();
	time = (stop-start)/1000.0;
	Com_Printf ("%f seconds (%f fps)\n", time, 128/time);
}

//=======================================================

/*
=================
SCR_RegisterConsoleMedia
=================
*/
void SCR_RegisterConsoleMedia (void)
{
	cls.whiteShader = R_RegisterPic ( "white" );
	cls.consoleShader = R_RegisterPic ( "console" );
	cls.charsetShader = R_RegisterPic ( "gfx/2d/bigchars" );
}

/*
=================
SCR_PrepRefresh

Call before entering a new level, or after changing dlls
=================
*/
void SCR_PrepRefresh (void)
{
	if ( !cl.configstrings[CS_MODELS+1][0] ) {
		if (!cl.cin.time)
		{
			R_BeginRegistration ();
			S_BeginRegistration ();

			SCR_RegisterConsoleMedia ();

			CL_UIModule_Init ();
			CL_GameModule_Shutdown ();

			S_EndRegistration ();
			R_EndRegistration ();

			if ( cls.state <= ca_disconnected ) {
				CL_UIModule_MenuMain ();
			}

			SCR_UpdateScreen ();
		}

		return;		// no map loaded
	}

	cl.cgame_active = qfalse;
	cl.cgame_loading = qtrue;

	// check memory integrity
	Mem_CheckSentinelsGlobal ();

	// register models, pics, and skins
	R_BeginRegistration ();
	S_BeginRegistration ();

	SCR_RegisterConsoleMedia ();

	CL_GameModule_Init ();
	CL_UIModule_Init ();

	S_EndRegistration ();

	// the renderer can now free unneeded stuff
	R_EndRegistration ();

	// check memory integrity
	Mem_CheckSentinelsGlobal ();

	// clear any lines of console text
	Con_ClearNotify ();

	SCR_UpdateScreen ();

	cl.sound_prepped = qtrue;
	cl.cgame_active = qtrue;
	cl.cgame_loading = qfalse;
}

//============================================================================

/*
==================
SCR_RenderView

==================
*/
void SCR_RenderView( float stereo_separation )
{
	if (cls.state == ca_active)
	{
		if (cl_timedemo->value)
		{
			if (!cl.timedemo_start)
				cl.timedemo_start = Sys_Milliseconds ();
			cl.timedemo_frames++;
		}
	}

	// frame is not valid until we load the CM data
	if ( CM_ClientLoad () ) {
		CL_GameModule_RenderView ( stereo_separation );
	}
}

//============================================================================

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
			scr_draw_loading = 0;
		}
		// if a cinematic is supposed to be running, handle menus
		// and console specially
		else if (cl.cin.time > 0)
		{
			SCR_DrawCinematic ();
		}
		else if ( cls.state == ca_active )
		{
			SCR_RenderView ( separation[i] );

			CL_UIModule_Refresh ( qfalse );

			if ( cl.cgame_active ) {
				SCR_CalcVrect ();

				if (scr_timegraph->value)
					SCR_DebugGraph (cls.frametime*300, 0, 0, 0);

				if (scr_debuggraph->value || scr_timegraph->value || scr_netgraph->value)
					SCR_DrawDebugGraph ();

				SCR_DrawConsole ();
			}
		}
		else
		{
			if ( cls.state == ca_disconnected ) {
				CL_UIModule_Refresh ( qtrue );
				SCR_DrawConsole ();
			} else {
				if ( cl.cgame_loading ) {
					SCR_RenderView ( separation[i] );
					CL_UIModule_DrawConnectScreen ( qfalse );
				} else {
					CL_UIModule_DrawConnectScreen ( qtrue );
				}
			}
		}
	}

	R_ApplySoftwareGamma ();

	R_Flush ();
}
