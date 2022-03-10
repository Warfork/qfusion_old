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
// Quake refresh engine.

#include <dlfcn.h> // ELF dl loader
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "../client/client.h"


// Console variables that we need to access from this module
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*vid_fullscreen;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules

qboolean	vid_ref_modified;
qboolean	vid_ref_active;

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

extern void	VID_MenuShutdown (void);

/*
==========================================================================

DLL GLUE

==========================================================================
*/

void VID_Restart (void)
{
	vid_ref_modified = qtrue;
}

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f (void)
{
	VID_Restart ();
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
	const char *description;
	int         width, height;
	int         mode;
} vidmode_t;

vidmode_t vid_modes[] =
{
	{ "Mode 0: 320x240",   320, 240,   0 },
	{ "Mode 1: 400x300",   400, 300,   1 },
	{ "Mode 2: 512x384",   512, 384,   2 },
	{ "Mode 3: 640x480",   640, 480,   3 },
	{ "Mode 4: 800x600",   800, 600,   4 },
	{ "Mode 5: 960x720",   960, 720,   5 },
	{ "Mode 6: 1024x768",  1024, 768,  6 },
	{ "Mode 7: 1152x864",  1152, 864,  7 },
	{ "Mode 8: 1280x1024",  1280, 1024, 8 },
	{ "Mode 9: 1600x1200", 1600, 1200, 9 },
	{ "Mode 10: 2048x1536", 2048, 1536, 10 }
};

qboolean VID_GetModeInfo(int *width, int *height, int mode )
{
	if ( mode < 0 || mode >= VID_NUM_MODES )
		return qfalse;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return qtrue;
}

/*
** VID_NewWindow
*/
void VID_NewWindow(int width, int height)
{
	viddef.width  = width;
	viddef.height = height;
}

/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the rendering DLL and/or video mode to match.
============
*/
void VID_CheckChanges (void)
{
	extern cvar_t *gl_driver;

	if ( vid_ref_modified )
	{
		qboolean cgameActive = cls.cgameActive;

		cls.disable_screen = qtrue;

		CL_ShutdownMedia ();

		if( vid_ref_active ) {
			R_Shutdown ();
			vid_ref_active = qfalse;
		}

		if( R_Init( NULL, NULL ) == -1 )
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
	}
}

/*
============
VID_Init
============
*/
void VID_Init (void)
{
	vid_xpos = Cvar_Get ("vid_xpos", "3", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get ("vid_ypos", "22", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);

	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);

	/* Start the graphics mode and load refresh DLL */
	vid_ref_modified = qtrue;
	vid_ref_active = qfalse;
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
		R_Shutdown ();
		vid_ref_active = qfalse;
	}

	Cmd_RemoveCommand ("vid_restart");
}
