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

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <dlfcn.h>

#include "../qcommon/qcommon.h"


cvar_t *nostdout;

unsigned	sys_frame_time;

uid_t saved_euid;
qboolean stdin_active = true;

// =======================================================================
// General routines
// =======================================================================

void Sys_ConsoleOutput (char *string)
{
	if (nostdout && nostdout->value)
		return;

	fputs (string, stdout);
}

void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	unsigned char	*p;

	va_start (argptr, fmt);
	vsnprintf (text, 1024, fmt, argptr);
	va_end (argptr);

	if (nostdout && nostdout->value)
		return;

	for (p = (unsigned char *)text; *p; p++) {
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf ("[%02x]", *p);
		else
			putc (*p, stdout);
	}
}

void Sys_Quit (void)
{
	CL_Shutdown ();
	Qcommon_Shutdown ();

	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	_exit(0);
}

void Sys_Init(void)
{
}

void Sys_Error (char *error, ...)
{ 
	va_list     argptr;
	char        string[1024];

// change stdin to non blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	CL_Shutdown ();
	Qcommon_Shutdown ();
	
	va_start (argptr, error);
	vsnprintf (string, 1024, error, argptr);
	va_end (argptr);
	fprintf (stderr, "Error: %s\n", string);

	_exit (1);
} 

void Sys_Warn (char *warning, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr, warning);
	vsnprintf (string, 1024, warning, argptr);
	va_end (argptr);
	fprintf (stderr, "Warning: %s", string);
} 

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int Sys_FileTime (char *path)
{
	struct	stat	buf;
	
	if (stat (path, &buf) == -1)
		return -1;
	
	return buf.st_mtime;
}

void floating_point_exception_handler (int whatever)
{
	signal (SIGFPE, floating_point_exception_handler);
}

char *Sys_ConsoleInput(void)
{
	static char text[256];
	int     len;
	fd_set	fdset;
	struct timeval timeout;

	if (!dedicated || !dedicated->value)
		return NULL;

	if (!stdin_active)
		return NULL;

	FD_ZERO (&fdset);
	FD_SET (0, &fdset); // stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select (1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
		return NULL;

	len = read (0, text, sizeof(text));
	if (len == 0) { // eof!
		stdin_active = false;
		return NULL;
	}

	if (len < 1)
		return NULL;

	text[len-1] = 0;    // rip off the /n and terminate

	return text;
}

/*****************************************************************************/

static void *game_library = NULL;
static void *cgame_library = NULL;
static void *ui_library = NULL;

void Sys_UnloadLibrary (gamelib_t gamelib)
{
	void *lib;

	switch (gamelib) 
	{
		case LIB_GAME: lib = &game_library;	break;
		case LIB_CGAME:	lib = &cgame_library; break;
		case LIB_UI: lib = &ui_library;	break;
		default:
			return;
	}

	if (lib)
	{
		if (dlclose (lib))
			Com_Error (ERR_FATAL, "dlclose failed");
	}

	lib = NULL;
}

void *Sys_LoadLibrary (gamelib_t gamelib, void *parms)
{
	char name[MAX_OSPATH];
	char cwd[MAX_OSPATH];
	char *path;
	void *(*APIfunc) (void *);
	void *lib;
	char *libname;
	char *apifuncname;

#if defined __i386__
#define ARCH "i386"

#ifdef NDEBUG
	const char *debugdir = "releasei386";
#else
	const char *debugdir = "debugi386";
#endif
#elif defined __alpha__
#define ARCH "axp"
#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif

#elif defined __powerpc__
#define ARCH "axp"
#ifdef NDEBUG
	const char *debugdir = "releaseppc";
#else
	const char *debugdir = "debugppc";
#endif
#elif defined __sparc__
#define ARCH "sparc"
#ifdef NDEBUG
	const char *debugdir = "releasepsparc";
#else
	const char *debugdir = "debugpsparc";
#endif
#else
#define ARCH	"UNKNOW"
#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif
#endif

	switch ( gamelib ) {
		case LIB_GAME:
			lib = &game_library;
			libname = "game_" ARCH ".so";
			apifuncname = "GetGameAPI";
			break;

		case LIB_CGAME:
			lib = &cgame_library;
			libname = "cgame_" ARCH ".so";
			apifuncname = "GetCGameAPI";
			break;

		case LIB_UI:
			lib = &ui_library;
			libname = "ui_" ARCH ".so";
			apifuncname = "GetUIAPI";
			break;

		default: assert( false );
	}

	// check the current debug directory first for development purposes
	getcwd (cwd, sizeof(cwd));
	Com_sprintf (name, sizeof(name), "%s/%s/%s", cwd, debugdir, libname);

	lib = dlopen (name, RTLD_NOW);

	if (lib)
	{
		Com_Printf ("LoadLibrary (%s)\n", name);
	}
	else
	{
		// now run through the search paths
		path = NULL;

		while (1)
		{
			path = FS_NextPath (path);

			if (!path) 
				return NULL; // couldn't find one anywhere

			Com_sprintf (name, sizeof(name), "%s/%s", path, libname);
			lib = dlopen (name, RTLD_NOW);

			if (lib)
			{
				Com_Printf ("LoadLibrary (%s)\n", name);
				break;
			}
		}
		
	}

	APIfunc = (void *)dlsym (lib, apifuncname);

	if (!APIfunc)
	{
		Sys_UnloadLibrary (gamelib);
		return NULL;
	}

	return APIfunc(parms);
}

void Sys_AppActivate (void)
{
}

void Sys_SendKeyEvents (void)
{
#ifndef DEDICATED_ONLY
	KBD_Update();
#endif

	// grab frame time 
	sys_frame_time = Sys_Milliseconds();
}

/*****************************************************************************/

int main (int argc, char **argv)
{
	int time, oldtime, newtime;

	printf ("%s -- Version %s\n", APPLICATION, LINUX_VERSION);

	Qcommon_Init(argc, argv);

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	nostdout = Cvar_Get ("nostdout", "0", 0);
	if (!nostdout->value) {
		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);	
	}

	oldtime = Sys_Milliseconds ();
	while (1)
	{
		// find time spent rendering last frame
		do 
		{
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);

		Qcommon_Frame (time);
		oldtime = newtime;
	}
}


