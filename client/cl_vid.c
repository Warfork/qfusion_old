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
// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// qfusion refresh engine.
#include "client.h"

cvar_t		*vid_width;			// custom screen width
cvar_t		*vid_height;		// custom screen height
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*vid_fullscreen;
cvar_t		*vid_displayfrequency;
cvar_t		*vid_multiscreen_head;
cvar_t		*vid_xinerama;
cvar_t		*win_noalttab;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules

#define VID_NUM_MODES (int)( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

static qboolean vid_ref_modified;
static qboolean vid_ref_verbose;
static qboolean vid_ref_active;

// These are system specific functions
int	VID_Sys_Init( qboolean verbose );			// wrapper around R_Init
void VID_Front_f( void );
void VID_UpdateWindowPosAndSize( int x, int y );
void VID_EnableAltTab( qboolean enable );

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the vid_ref_modified variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart( qboolean verbose )
{
	vid_ref_modified = qtrue;
	vid_ref_verbose = verbose;
}

void VID_Restart_f (void)
{
	VID_Restart( (Cmd_Argc() >= 2 ? qtrue : qfalse) );
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
	int         width, height;
	qboolean	wideScreen;
} vidmode_t;

vidmode_t vid_modes[] =
{
//	{ 320,	240,	qfalse },
//	{ 400,	300,	qfalse },
//	{ 512,	384,	qfalse },
	{ 640,	480,	qfalse },
	{ 800,	600,	qfalse },
	{ 960,	720,	qfalse },
	{ 1024, 768,	qfalse },
	{ 1152, 864,	qfalse },
	{ 1280, 800,	qfalse },
	{ 1280, 960,	qfalse },
	{ 1280, 1024,	qfalse },
	{ 1600, 1200,	qfalse },
	{ 2048, 1536,	qfalse },

	{ 856,	480,	qtrue },
	{ 1024,	576,	qtrue },
	{ 1440,	900,	qtrue },
	{ 1680,	1050,	qtrue },
	{ 1920,	1200,	qtrue },
	{ 2560,	1600,	qtrue },

	{ 2400, 600,	qfalse },
	{ 3072, 768,	qfalse },
	{ 3840, 720,	qfalse },
	{ 3840, 1024,	qfalse },
	{ 4800, 1200,	qfalse },
	{ 6144, 1536,	qfalse }
};

qboolean VID_GetModeInfo( int *width, int *height, qboolean *wideScreen, int mode )
{
	if ( mode < -1 || mode >= VID_NUM_MODES )
		return qfalse;

	if( mode == -1 ) {
		*width = vid_width->integer;
		*height = vid_height->integer;
		*wideScreen = qfalse;
	} else {
		*width  = vid_modes[mode].width;
		*height = vid_modes[mode].height;
		*wideScreen = vid_modes[mode].wideScreen;
	}

	return qtrue;
}

/*
============
VID_ModeList_f
============
*/
static void VID_ModeList_f( void )
{
	int i;

	Com_Printf( "Mode -1: for custom width/height (use vid_width and vid_height)\n" );
	for( i = 0; i < VID_NUM_MODES; i++ )
		Com_Printf( "Mode %i: %ix%i%s\n", i, vid_modes[i].width, vid_modes[i].height, (vid_modes[i].wideScreen ? " (wide)" : "") );
}

/*
** VID_NewWindow
*/
void VID_NewWindow ( int width, int height )
{
	viddef.width  = width;
	viddef.height = height;
}


/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and its sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the rendering DLL and/or video mode to match.
============
*/
void VID_CheckChanges (void)
{
	extern cvar_t *gl_driver;

	if ( win_noalttab->modified )
	{
		VID_EnableAltTab( win_noalttab->integer ? qfalse : qtrue );
		win_noalttab->modified = qfalse;
	}

	if ( vid_ref_modified )
	{
		qboolean cgameActive = cls.cgameActive;

		cls.disable_screen = qtrue;

		CL_ShutdownMedia ();

		if( vid_ref_active ) {
			R_Shutdown( qfalse );
			vid_ref_active = qfalse;
		}

		if( VID_Sys_Init( vid_ref_verbose ) == -1 )
			Com_Error ( ERR_FATAL, "Failed to load %s", (gl_driver && gl_driver->name) ? gl_driver->string : "" );

		CL_InitMedia ();

		cls.disable_screen = qfalse;

		Con_Close ();

		if( cgameActive ) {
			CL_GameModule_Init ();
			CL_SetKeyDest( key_game );
		} else {
			CL_UIModule_MenuMain ();
			CL_SetKeyDest( key_menu );
		}

		vid_ref_active = qtrue;
		vid_ref_modified = qfalse;
		vid_ref_verbose = qtrue;
	}

	/*
	** update our window position
	*/
	if ( vid_xpos->modified || vid_ypos->modified )
	{
		if (!vid_fullscreen->integer)
			VID_UpdateWindowPosAndSize( vid_xpos->integer, vid_ypos->integer );
		vid_xpos->modified = qfalse;
		vid_ypos->modified = qfalse;
	}
}

/*
============
VID_Init
============
*/
void VID_Init (void)
{
	/* Create the video variables so we know how to start the graphics drivers */
	vid_width = Cvar_Get ( "vid_width", "1024", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_height = Cvar_Get ( "vid_height", "768", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_xpos = Cvar_Get ( "vid_xpos", "3", CVAR_ARCHIVE );
	vid_ypos = Cvar_Get ( "vid_ypos", "22", CVAR_ARCHIVE );
	vid_fullscreen = Cvar_Get ( "vid_fullscreen", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_displayfrequency = Cvar_Get ( "vid_displayfrequency", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_multiscreen_head = Cvar_Get ( "vid_multiscreen_head", "-1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_xinerama = Cvar_Get( "vid_xinerama", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	win_noalttab = Cvar_Get( "win_noalttab", "0", CVAR_ARCHIVE );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);
	Cmd_AddCommand ("vid_front", VID_Front_f);
	Cmd_AddCommand ("vid_modelist", VID_ModeList_f);

	/* Start the graphics mode and load refresh DLL */
	vid_ref_modified = qtrue;
	vid_ref_active = qfalse;
	vid_ref_verbose = qtrue;
	vid_fullscreen->modified = qtrue;
	VID_CheckChanges ();
}

/*
============
VID_Shutdown
============
*/
void VID_Shutdown (void)
{
	if( vid_ref_active ) {
		R_Shutdown( qtrue );
		vid_ref_active = qfalse;
	}

	Cmd_RemoveCommand ("vid_restart");
	Cmd_RemoveCommand ("vid_front");
	Cmd_RemoveCommand ("vid_modelist");
}
