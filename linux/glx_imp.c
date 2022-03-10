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
/*
** GLW_IMP.C
**
** This file contains ALL Linux specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
**
*/

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>

#include "linux/x11.h"

#include "ref_gl/r_local.h"
#include "client/keys.h"

#include "linux/glw_linux.h"


#define DISPLAY_MASK (VisibilityChangeMask | StructureNotifyMask | ExposureMask )
#define INIT_MASK (KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | DISPLAY_MASK )

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask |  PointerMotionMask | ButtonMotionMask )
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask )

x11display_t x11display;

glwstate_t glw_state;

static qboolean vidmode_ext = qfalse;

static int _xf86_vidmodes_suported = 0;
static XF86VidModeModeInfo **_xf86_vidmodes;
static int _xf86_vidmodes_num;
static int _xf86_vidmodes_active = 0;
static qboolean _xf86_xinerama_supported = qfalse;

static void _xf86_VidmodesInit(void){
	int MajorVersion, MinorVersion;
	int i;

	// Get video mode list
	MajorVersion = MinorVersion = 0;

	if (XF86VidModeQueryVersion (x11display.dpy, &MajorVersion, &MinorVersion))
	{
		Com_Printf ("..XFree86-VidMode Extension Version %d.%d\n", MajorVersion, MinorVersion);
		XF86VidModeGetAllModeLines (x11display.dpy, x11display.scr, &_xf86_vidmodes_num, &_xf86_vidmodes);
		_xf86_vidmodes_suported = 1;
	}
	else
	{
		Com_Printf ("..XFree86-VidMode Extension not available\n");
		_xf86_vidmodes_suported = 0;
	}
}

static void _xf86_VidmodesFree (void)
{
	if (_xf86_vidmodes_suported) {
		XFree(_xf86_vidmodes);
	}

	_xf86_vidmodes_suported = 0;
}

static void _xf86_XineramaInit(void)
{
	extern cvar_t *vid_xinerama;
	
	_xf86_xinerama_supported = qfalse;
	if (vid_xinerama->integer)
	{
		int MajorVersion = 0, MinorVersion = 0;
		if ((XineramaQueryVersion (x11display.dpy, &MajorVersion, &MinorVersion)) && (XineramaIsActive (x11display.dpy)))
		{
			Com_Printf ("..XFree86-Xinerama Extension Version %d.%d\n", MajorVersion, MinorVersion);
			_xf86_xinerama_supported = qtrue;
		}
	}

	if (_xf86_xinerama_supported)
	{
		Com_Printf ("..XFree86-Xinerama Extension not available\n");
	}
}

static void _xf86_XineramaFree(void)
{
	_xf86_xinerama_supported = qfalse;
}

static void _xf86_XineramaFindBest (int *x, int *y, int *width, int *height)
{
	int i, screens;
	int best_fit, best_dist, dist, w, h;
	XineramaScreenInfo *xinerama;
	extern cvar_t *vid_multiscreen_head;

	if (_xf86_xinerama_supported == qfalse)
		return;

	best_fit = -1;
	best_dist = 999999999;

	xinerama = XineramaQueryScreens (x11display.dpy, &screens);
	for( i = 0; i < screens; i++ )
	{
		if (((*width <= xinerama[i].width) && (*height <= xinerama[i].height)) || i == vid_multiscreen_head->integer)
		{
			w = xinerama[i].width - *width;
			h = xinerama[i].height - *height;

			if (w > h) dist = h;
			else dist = w;

			if (dist < 0) dist = -dist; // Only positive number please
		
			if( dist < best_dist || i == vid_multiscreen_head->integer) {
				best_dist = dist;
				best_fit = i;
			}
		}
		if (i == vid_multiscreen_head->integer)
			break;
	}

	if (best_fit == -1)
	{
		_xf86_xinerama_supported = qfalse;
		return;
	}
	
	*x = xinerama[best_fit].x_org;
	*y = xinerama[best_fit].y_org;
	*width = xinerama[best_fit].width;
	*height = xinerama[best_fit].height;

	Com_Printf ("Xinerama: using screen %d: %dx%d+%d+%d\n", xinerama[best_fit].screen_number, xinerama[best_fit].width, xinerama[best_fit].height, xinerama[best_fit].x_org, xinerama[best_fit].y_org);
}

static void _xf86_VidmodesSwitch (int mode)
{
	if (_xf86_vidmodes_suported) {
		XF86VidModeSwitchToMode (x11display.dpy, x11display.scr, _xf86_vidmodes[mode]);
		XF86VidModeSetViewPort (x11display.dpy, x11display.scr, 0, 0);
	}

	_xf86_vidmodes_active = 1;
}

static void _xf86_VidmodesSwitchBack (void)
{
	if (_xf86_vidmodes_suported) {
		if (_xf86_vidmodes_active) 
			_xf86_VidmodesSwitch (0);
	}

	_xf86_vidmodes_active = 0;
}

static void _xf86_VidmodesFindBest(int *mode, int *pwidth, int *pheight){
	int i, best_fit, best_dist, dist, x, y;

	best_fit = -1;
	best_dist = 999999999;

	if (_xf86_vidmodes_suported)
	{
		for (i =0; i <_xf86_vidmodes_num; i++)
		{
			if (_xf86_vidmodes[i]->hdisplay <*pwidth || _xf86_vidmodes[i]->vdisplay <*pheight) 
				continue;

			x = _xf86_vidmodes[i]->hdisplay -*pwidth;
			y = _xf86_vidmodes[i]->vdisplay -*pheight;

			if (x > y)
				dist = y;
			else
				dist = x;

			if (dist < 0) 
				dist = -dist; // Only positive number please
		
			if( dist < best_dist) {
				best_dist = dist;
				best_fit = i;
			}

			Com_Printf ("%ix%i -> %ix%i: %i\n", *pwidth, *pheight, _xf86_vidmodes[i]->hdisplay, _xf86_vidmodes[i]->vdisplay, dist);
		}

		if (best_fit >= 0)
		{
			Com_Printf ("%ix%i selected\n", _xf86_vidmodes[best_fit]->hdisplay, _xf86_vidmodes[best_fit]->vdisplay);

			*pwidth =_xf86_vidmodes[best_fit]->hdisplay;
			*pheight =_xf86_vidmodes[best_fit]->vdisplay;
		}
	}

	*mode = best_fit;

}

static void _x11_WaitForMaped (Window w)
{
	XEvent event;

	if (x11display.dpy)
	{
		do 
		{
			XMaskEvent(x11display.dpy, StructureNotifyMask, &event);
		} while((event.type != MapNotify) || (event.xmap.event != w));
	}
}

static void _x11_WaitForUnmaped (Window w)
{
	XEvent event;

	if (x11display.dpy)
	{
		do
		{
			XMaskEvent(x11display.dpy, StructureNotifyMask, &event);
		} while((event.type != UnmapNotify) || (event.xunmap.event != w));
	}
}

static void _x11_SetNoResize (Window w, int width, int height)
{
	XSizeHints *hints;

	if (x11display.dpy)
	{
		hints = XAllocSizeHints();

		if (hints)
		{
			hints->min_width = hints->max_width = width;
			hints->min_height = hints->max_height = height;
				
			hints->flags = PMaxSize | PMinSize;

			XSetWMNormalHints (x11display.dpy, w, hints);
			XFree (hints);
		}
	}
}

/*****************************************************************************/

int GLimp_InitGL (void);

static void signal_handler (int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	GLimp_Shutdown();
	_exit(0);
}

static void InitSig (void) 
{
	signal(SIGHUP, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

/*
** GLimp_SetMode
*/
int GLimp_SetMode( int mode, qboolean fullscreen )
{
	int width, height, screen_x, screen_y, screen_width, screen_height, screen_mode;
	float ratio;
	XSetWindowAttributes wa;
	unsigned long mask;
	qboolean wideScreen;

	if (!VID_GetModeInfo( &width, &height, &wideScreen, mode))
	{
		Com_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	screen_x = screen_y = 0;
	screen_width = width;
	screen_height = height;

	if (fullscreen)
	{
		_xf86_VidmodesFindBest (&screen_mode, &screen_width, &screen_height);

		if (screen_mode < 0) 
			return rserr_invalid_mode;

		if (_xf86_xinerama_supported)
		{
			screen_width = width;
			screen_height = height;
			_xf86_VidmodesSwitch (screen_mode);
			_xf86_XineramaFindBest (&screen_x, &screen_y, &screen_width, &screen_height);
			if (!_xf86_xinerama_supported)
				_xf86_VidmodesSwitchBack ();
		}

		if (screen_width < width || screen_height < height)
		{
			if (width > height)
			{
				ratio = width / height;
				height = height *ratio;
				width = screen_width;
			}
			else
			{
				ratio = height / width;
				width = width *ratio;
				height = screen_height;
			}
		}

		Com_Printf (PRINT_ALL, "...setting fullscreen mode %d:", mode);

		/* Create fulscreen window */
		x11display.old_win = x11display.win;

		wa.background_pixel = 0;
		wa.border_pixel = 0;
		wa.event_mask = INIT_MASK;
		wa.override_redirect = True;
		wa.backing_store = NotUseful;
		wa.save_under = False;
		mask = CWBackPixel | CWBorderPixel | CWEventMask | CWSaveUnder | CWBackingStore | CWOverrideRedirect;

		x11display.win = XCreateWindow(x11display.dpy, x11display.root, screen_x, screen_y, screen_width, screen_height,
			0, CopyFromParent, InputOutput, CopyFromParent, mask, &wa);

		XResizeWindow (x11display.dpy, x11display.gl_win, width, height);
		XReparentWindow (x11display.dpy, x11display.gl_win, x11display.win, (screen_width /2) -(width /2), (screen_height /2) - (height /2));

		XMapWindow (x11display.dpy, x11display.gl_win);
		XMapWindow (x11display.dpy, x11display.win);
	
		_x11_SetNoResize (x11display.win, screen_width, screen_height);

		if (!_xf86_xinerama_supported)
			_xf86_VidmodesSwitch (screen_mode);
	}
	else
	{
		Com_Printf (PRINT_ALL, "...setting mode %d:", mode);

		/* Create managed window */
		x11display.old_win = x11display.win;

		wa.background_pixel = 0;
		wa.border_pixel = 0;
		wa.event_mask = INIT_MASK;
		mask = CWBackPixel | CWBorderPixel | CWEventMask;

		x11display.win = XCreateWindow (x11display.dpy, x11display.root, 0, 0, screen_width, screen_height,
			0, CopyFromParent, InputOutput, CopyFromParent, mask, &wa);
		XSetStandardProperties (x11display.dpy, x11display.win, APPLICATION, None, None, NULL, 0, NULL);
		x11display.wmDeleteWindow = XInternAtom(x11display.dpy, "WM_DELETE_WINDOW", False);
		XSetWMProtocols (x11display.dpy, x11display.win, &x11display.wmDeleteWindow, 1);

		XResizeWindow (x11display.dpy, x11display.gl_win, width, height);
		XReparentWindow (x11display.dpy, x11display.gl_win, x11display.win, 0, 0);
		XMapWindow (x11display.dpy, x11display.gl_win);
		XMapWindow (x11display.dpy, x11display.win);
	
		_x11_SetNoResize(x11display.win, width, height);
	}

	// save the parent window size for mouse use.
	// this is not the gl context window
	x11display.win_width = width;
	x11display.win_height = height;

	if (x11display.old_win)
	{
		XDestroyWindow (x11display.dpy, x11display.old_win);
		x11display.old_win = 0;
	}
	
	XFlush (x11display.dpy);

	glState.width = width;
	glState.height = height;
	glState.fullScreen = fullscreen;

	// let the sound and input subsystems know about the new window
	VID_NewWindow (width, height);

	return rserr_ok;
}

/*
** GLimp_Shutdown
*/
void GLimp_Shutdown (void)
{
	if (x11display.dpy)
	{
//		IN_Activate (qfalse);
		_xf86_VidmodesSwitchBack();
		_xf86_VidmodesFree();
		_xf86_XineramaFree();
		
		if (x11display.ctx) 
			qglXDestroyContext (x11display.dpy, x11display.ctx);

		if (x11display.gl_win) 
			XDestroyWindow (x11display.dpy, x11display.gl_win);

		if (x11display.win) 
			XDestroyWindow(x11display.dpy, x11display.win);

		XCloseDisplay (x11display.dpy);
	}

	x11display.ctx = NULL;
	x11display.gl_win = 0;
	x11display.win = 0;
	x11display.dpy = NULL;
}

/*
** GLimp_Init
*/
int GLimp_Init( void *hinstance, void *wndproc)
{
	int i;

	int attributes_0[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 4,
		GLX_GREEN_SIZE, 4,
		GLX_BLUE_SIZE, 4,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8,
		None
	};

	int attributes_1[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		GLX_RED_SIZE, 4,
		GLX_GREEN_SIZE, 4,
		GLX_BLUE_SIZE, 4,
		GLX_DEPTH_SIZE, 24,
		None
	};

	struct{
		char *name;
		int *attributes;
	} attributes_list[] = {
		{"rbga, double-buffered, 24bits depth and 8bit stencil", attributes_0},
		{"rbga, double-buffered and 24bits depth", attributes_1},
		{0, 0}
	};

	XSetWindowAttributes attr;
	unsigned long mask;

	if (x11display.dpy) 
		GLimp_Shutdown();

	InitSig ();

	Sys_Printf ("Display initialization\n");

	x11display.dpy = XOpenDisplay (NULL);
	if (!x11display.dpy)
	{
		Sys_Printf ("..Error couldn't open the X display\n");
		return 0;
	}

	x11display.scr = DefaultScreen (x11display.dpy);
	x11display.root = RootWindow (x11display.dpy, x11display.scr);

	_xf86_VidmodesInit ();
	_xf86_XineramaInit ();

	for (i =0; attributes_list[i].name; i++){
		x11display.visinfo = qglXChooseVisual (x11display.dpy, x11display.scr, attributes_list[i].attributes);
		if (!x11display.visinfo){
			Sys_Printf ("..Failed to get %s\n", attributes_list[i].name);
		} else{
            Sys_Printf ("..Get %s\n", attributes_list[i].name);
			break;
		}
	}

	if (!x11display.visinfo) 
		return 0;

	x11display.ctx = qglXCreateContext(x11display.dpy, x11display.visinfo, NULL, True);
	x11display.cmap = XCreateColormap(x11display.dpy, x11display.root, x11display.visinfo->visual, AllocNone);

	attr.colormap = x11display.cmap;
	attr.border_pixel = 0;
	attr.event_mask = DISPLAY_MASK;
	attr.override_redirect = False;
	mask = CWBorderPixel | CWColormap;

	x11display.gl_win = XCreateWindow(x11display.dpy, x11display.root, 0, 0, 1, 1, 0,
		x11display.visinfo->depth, InputOutput, x11display.visinfo->visual, mask, &attr);
	qglXMakeCurrent(x11display.dpy, x11display.gl_win, x11display.ctx);

	XSync (x11display.dpy, False);

	return(1);
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( void )
{
}

/*
** GLimp_EndFrame
** 
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (void)
{
	qglXSwapBuffers (x11display.dpy, x11display.gl_win);
}

/*
** GLimp_BeginFrame
*/
qboolean GLimp_GetGammaRamp( size_t stride, unsigned short *ramp )
{
	if( XF86VidModeGetGammaRamp( x11display.dpy, x11display.scr, stride, ramp, ramp + stride, ramp + (stride << 1) ) != 0 )
	    return qtrue;
	return qfalse;
}

/*
** GLimp_BeginFrame
*/
void GLimp_SetGammaRamp( size_t stride, unsigned short *ramp )
{
    XF86VidModeSetGammaRamp( x11display.dpy, x11display.scr, stride, ramp, ramp + stride, ramp + (stride << 1) );
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( qboolean active )
{
}
