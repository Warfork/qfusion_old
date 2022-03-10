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
#include <dirent.h>

#if defined(__FreeBSD__)
#include <machine/param.h>
#endif

#include "../qcommon/qcommon.h"
#include "glob.h"

cvar_t *nostdout;

unsigned	sys_frame_time;

uid_t saved_euid;
qboolean stdin_active = qtrue;

// =======================================================================
// General routines
// =======================================================================

void Sys_ConsoleOutput (char *string)
{
	if (nostdout && nostdout->integer)
		return;

	fputs (string, stdout);
}

/*
=================
Sys_Printf
=================
*/
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	unsigned char	*p;

	va_start (argptr, fmt);
	vsnprintf (text, 1024, fmt, argptr);
	va_end (argptr);

	if (nostdout && nostdout->integer)
		return;

	for (p = (unsigned char *)text; *p; p++) {
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf ("[%02x]", *p);
		else
			putc (*p, stdout);
	}
}

/*
=================
Sys_Quit
=================
*/
void Sys_Quit (void)
{
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	Qcommon_Shutdown ();

	_exit(0);
}

/*
=================
Sys_Init
=================
*/
void Sys_Init(void)
{
}

/*
=================
Sys_Error
=================
*/
void Sys_Error (char *error, ...)
{ 
	va_list     argptr;
	char        string[1024];

// change stdin to non blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	CL_Shutdown ();
	
	va_start (argptr, error);
	vsnprintf (string, 1024, error, argptr);
	va_end (argptr);
	fprintf (stderr, "Error: %s\n", string);

	Qcommon_Shutdown ();

	_exit (1);
} 

/*
================
Sys_Milliseconds
================
*/
int curtime;
int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int		secbase;

	gettimeofday(&tp, &tzp);
	
	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;
	
	return curtime;
}

void Sys_Mkdir (const char *path)
{
    mkdir (path, 0777);
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

	if (!dedicated || !dedicated->integer)
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
		stdin_active = qfalse;
		return NULL;
	}

	if (len < 1)
		return NULL;

	text[len-1] = 0;    // rip off the /n and terminate

	return text;
}


/*
========================================================================

DLL

========================================================================
*/

/*
=================
Sys_UnloadLibrary
=================
*/
void Sys_UnloadLibrary( void **lib )
{
	if( lib && *lib ) {
		if( dlclose( *lib ) )
			Com_Error( ERR_FATAL, "dlclose failed" );
		*lib = NULL;
	}
}

/*
=================
Sys_LoadLibrary
=================
*/
void *Sys_LoadLibrary( char *name, dllfunc_t *funcs )
{
	void *lib;
	dllfunc_t *func;

	if( !name || !name[0] || !funcs )
		return NULL;

	Com_DPrintf( "LoadLibrary (%s)\n", name );

	lib = dlopen( name, RTLD_NOW );
	if( !lib )
		return NULL;

	for( func = funcs; func->name; func++ ) {
		*(func->funcPointer) = ( void * )dlsym( lib, func->name );

		if( !(*(func->funcPointer)) ) {
			Sys_UnloadLibrary( &lib );
			Com_Error( ERR_FATAL, "%s: dlsym failed for %s", name, func->name );
		}
	}

	return lib;
}

/*****************************************************************************/

static void *game_library = NULL;
static void *cgame_library = NULL;
static void *ui_library = NULL;

#ifdef __cplusplus
# define EXTERN_API_FUNC	extern "C"
#else
# define EXTERN_API_FUNC	extern
#endif

/*
=================
Sys_UnloadGameLibrary
=================
*/
void Sys_UnloadGameLibrary (gamelib_t gamelib)
{
	void **lib;

	switch( gamelib ) {
		case LIB_GAME:
			lib = &game_library;
#ifdef GAME_HARD_LINKED
			*lib = NULL;
#endif
			break;
		case LIB_CGAME:
			lib = &cgame_library;
#ifdef CGAME_HARD_LINKED
			*lib = NULL;
#endif
			break;
		case LIB_UI:
			lib = &ui_library;
#ifdef UI_HARD_LINKED
			*lib = NULL;
#endif
			break;
		default: assert( 0 );
	}

	if( *lib ) {
		if( dlclose (*lib) )
			Com_Error (ERR_FATAL, "dlclose failed");
		*lib = NULL;
	}
}

/*
=================
Sys_LoadGameLibrary
=================
*/
void *Sys_LoadGameLibrary (gamelib_t gamelib, void *parms)
{
	char name[MAX_OSPATH];
	char cwd[MAX_OSPATH];
	char *path;
	void *(*APIfunc) (void *);
	void **lib;
	char *libname;
	char *apifuncname;

#if defined __i386__
#ifdef NDEBUG
	const char *debugdir = "releasei386";
#else
	const char *debugdir = "debugi386";
#endif
#elif defined __alpha__
#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif
#elif defined __powerpc__
#ifdef NDEBUG
	const char *debugdir = "releaseppc";
#else
	const char *debugdir = "debugppc";
#endif
#elif defined __x86_64__
#ifdef NDEBUG
	const char *debugdir = "releasex86_64";
#else
	const char *debugdir = "debugx86_64";
#endif
#elif defined __sparc__
#ifdef NDEBUG
	const char *debugdir = "releasepsparc";
#else
	const char *debugdir = "debugpsparc";
#endif
#else
#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif
#endif

	APIfunc = NULL;
	switch( gamelib ) {
		case LIB_GAME:
		{
#ifdef GAME_HARD_LINKED
			EXTERN_API_FUNC void *GetGameAPI( void * );
			APIfunc = GetGameAPI;
#endif
			lib = &game_library;
			libname = "game_" ARCH ".so";
			apifuncname = "GetGameAPI";
			break;
		}
		case LIB_CGAME:
		{
#ifdef CGAME_HARD_LINKED
			EXTERN_API_FUNC void *GetCGameAPI( void * );
			APIfunc = GetCGameAPI;
#endif
			lib = &cgame_library;
			libname = "cgame_" ARCH ".so";
			apifuncname = "GetCGameAPI";
			break;
		}
		case LIB_UI:
		{
#ifdef UI_HARD_LINKED
			EXTERN_API_FUNC void *GetUIAPI( void * );
			APIfunc = GetUIAPI;
#endif
			lib = &ui_library;
			libname = "ui_" ARCH ".so";
			apifuncname = "GetUIAPI";
			break;
		}
		default: assert( 0 );
	}

	if (*lib)
		Com_Error (ERR_FATAL, "Sys_LoadGameLibrary without Sys_UnloadGameLibrary");

	if (APIfunc)
	{
		*lib = ( void * )1;
		return APIfunc (parms);
	}

	// check the current debug directory first for development purposes
	getcwd (cwd, sizeof(cwd));
	Q_snprintfz (name, sizeof(name), "%s/%s/%s", cwd, debugdir, libname);

	*lib = dlopen (name, RTLD_NOW);

	if (*lib)
	{
		Com_DPrintf ("LoadLibrary (%s)\n", name);
	}
	else
	{
		char *prev;

		// now run through the search paths
		prev = path = NULL;

		while (1)
		{
			path = FS_NextPath (prev);

			if (!path) 
				return NULL; // couldn't find one anywhere
			if (prev)
				if (!strcmp (path, prev))
				{
					prev = path;
					continue;	// happens on UNIX systems (homedir as gamedir)
				}

			Q_snprintfz (name, sizeof(name), "%s/%s", path, libname);
			*lib = dlopen (name, RTLD_NOW);

			if (*lib)
			{
				Com_DPrintf ("LoadLibrary (%s)\n", name);
				break;
			}
			prev = path;
		}
		
	}

	APIfunc = (void *)dlsym (*lib, apifuncname);

	if (!APIfunc)
	{
		Sys_UnloadGameLibrary (gamelib);
		return NULL;
	}

	return APIfunc(parms);
}

//===============================================================================

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
static	DIR	*fdir;

static qboolean CompareAttributes(char *path, char *name,
	unsigned musthave, unsigned canthave )
{
	struct stat st;
	char fn[MAX_OSPATH];

// . and .. never match
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return qfalse;

	return qtrue;

	if (stat(fn, &st) == -1)
		return qfalse; // shouldn't happen

	if ( ( st.st_mode & S_IFDIR ) && ( canthave & SFF_SUBDIR ) )
		return qfalse;

	if ( ( musthave & SFF_SUBDIR ) && !( st.st_mode & S_IFDIR ) )
		return qfalse;

	return qtrue;
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canhave)
{
	struct dirent *d;
	char *p;

	if (fdir)
		Sys_Error ("Sys_BeginFind without close");

//	COM_FilePath (path, findbase);
	strcpy(findbase, path);

	if ((p = strrchr(findbase, '/')) != NULL) {
		*p = 0;
		strcpy(findpattern, p + 1);
	} else
		strcpy(findpattern, "*");

	if (strcmp(findpattern, "*.*") == 0)
		strcpy(findpattern, "*");
	
	if ((fdir = opendir(findbase)) == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
//			if (*findpattern)
//				printf("%s matched %s\n", findpattern, d->d_name);
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

char *Sys_FindNext (unsigned musthave, unsigned canhave)
{
	struct dirent *d;

	if (fdir == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
//			if (*findpattern)
//				printf("%s matched %s\n", findpattern, d->d_name);
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

void Sys_FindClose (void)
{
	if (fdir != NULL)
		closedir(fdir);
	fdir = NULL;
}

/*
=================
Sys_GetHomeDirectory
=================
*/
char *Sys_GetHomeDirectory (void)
{
	return getenv ( "HOME" );
}

/*
=================
Sys_LockFile
=================
*/
void *Sys_LockFile (const char *path)
{
	return (void *)1;	// return a non-NULL pointer
}

/*
=================
Sys_UnlockFile
=================
*/
void Sys_UnlockFile (void *handle)
{
}

//===============================================================================

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
}

/*
=================
Sys_SendKeyEvents
=================
*/
void Sys_SendKeyEvents (void)
{
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
	if (!nostdout->integer) {
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


