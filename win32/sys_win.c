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
// sys_win.h

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include "../win32/conproc.h"

#define MINIMUM_WIN_MEMORY	0x0a00000
#define MAXIMUM_WIN_MEMORY	0x1000000

qboolean s_win95;

int			starttime;
qboolean	ActiveApp;
qboolean	Minimized;

static HANDLE		hinput, houtput;

unsigned	sys_msg_time;
unsigned	sys_frame_time;


static HANDLE		qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int			argc;
char		*argv[MAX_NUM_ARGVS];


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	CL_Shutdown ();

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	MessageBox(NULL, text, "Error", 0 /* MB_OK */ );

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);

// shut down QHOST hooks if necessary
	DeinitConProc ();

	Qcommon_Shutdown ();

	exit (1);
}

void Sys_Quit (void)
{
	timeEndPeriod( 1 );

	CL_Shutdown();

	CloseHandle (qwclsemaphore);
	if (dedicated && dedicated->integer)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();

	Qcommon_Shutdown ();

	exit (0);
}

//================================================================

/*
================
Sys_Milliseconds
================
*/
int	curtime;

int Sys_Milliseconds (void)
{
	static int		base;
	static qboolean	initialized = qfalse;

	if (!initialized)
	{	// let base retain 16 bits of effectively random data
		base = timeGetTime() & 0xffff0000;
		initialized = qtrue;
	}
	curtime = timeGetTime() - base;

	return curtime;
}

void Sys_Mkdir (const char *path)
{
	_mkdir (path);
}

//===============================================================================

char	findbase[MAX_OSPATH];
char	findpath[MAX_OSPATH];
int		findhandle = -1;

static qboolean CompareAttributes( unsigned found, unsigned musthave, unsigned canthave )
{
	if ( ( found & _A_RDONLY ) && ( canthave & SFF_RDONLY ) )
		return qfalse;
	if ( ( found & _A_HIDDEN ) && ( canthave & SFF_HIDDEN ) )
		return qfalse;
	if ( ( found & _A_SYSTEM ) && ( canthave & SFF_SYSTEM ) )
		return qfalse;
	if ( ( found & _A_SUBDIR ) && ( canthave & SFF_SUBDIR ) )
		return qfalse;
	if ( ( found & _A_ARCH ) && ( canthave & SFF_ARCH ) )
		return qfalse;

	if ( ( musthave & SFF_RDONLY ) && !( found & _A_RDONLY ) )
		return qfalse;
	if ( ( musthave & SFF_HIDDEN ) && !( found & _A_HIDDEN ) )
		return qfalse;
	if ( ( musthave & SFF_SYSTEM ) && !( found & _A_SYSTEM ) )
		return qfalse;
	if ( ( musthave & SFF_SUBDIR ) && !( found & _A_SUBDIR ) )
		return qfalse;
	if ( ( musthave & SFF_ARCH ) && !( found & _A_ARCH ) )
		return qfalse;

	return qtrue;
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canthave )
{
	struct _finddata_t findinfo;

	if (findhandle != -1)
		Sys_Error ("Sys_BeginFind without close");

	COM_FilePath (path, findbase);
	findhandle = _findfirst (path, &findinfo);

	while (findhandle != -1) {
		if ( !CompareAttributes( findinfo.attrib, musthave, canthave ) ) {
			_findclose ( findhandle );
			findhandle = _findnext ( findhandle, &findinfo );
		} else {
			Q_snprintfz (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
			return findpath;
		}
	}

	return NULL;
}

char *Sys_FindNext ( unsigned musthave, unsigned canthave )
{
	struct _finddata_t findinfo;

	if (findhandle == -1)
		return NULL;
	while (_findnext (findhandle, &findinfo) != -1) {
		if ( CompareAttributes( findinfo.attrib, musthave, canthave ) ) {
			Q_snprintfz (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
			return findpath;
		}
	}

	return NULL;
}

void Sys_FindClose (void)
{
	if (findhandle != -1)
		_findclose (findhandle);
	findhandle = -1;
}

/*
=================
Sys_GetHomeDirectory

=================
*/
char *Sys_GetHomeDirectory (void)
{
	return NULL;
}

//===============================================================================

/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

	timeBeginPeriod( 1 );

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4)
		Sys_Error ("%s requires windows version 4 or greater", APPLICATION);
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s)
		Sys_Error ("%s doesn't run on Win32s", APPLICATION);
	else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
		s_win95 = qtrue;

	if (dedicated->integer)
	{
		SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS);

		if (!AllocConsole ())
			Sys_Error ("Couldn't create dedicated server console");
		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);
	
		// let QHOST hook in
		InitConProc (argc, argv);
	}
}


static char	console_text[256];
static int	console_textlen;

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	INPUT_RECORD	recs[1024];
	int		dummy;
	int		ch, numread, numevents;

	if (!dedicated || !dedicated->integer)
		return NULL;

	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);	

						if (console_textlen)
						{
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return console_text;
						}
						break;

					case '\b':
						if (console_textlen)
						{
							console_textlen--;
							WriteFile(houtput, "\b \b", 3, &dummy, NULL);	
						}
						break;

					default:
						if (ch >= ' ')
						{
							if (console_textlen < sizeof(console_text)-2)
							{
								WriteFile(houtput, &ch, 1, &dummy, NULL);	
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}

						break;

				}
			}
		}
	}

	return NULL;
}


/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (char *string)
{
	int		dummy;
	char	text[256];

	if (!dedicated || !dedicated->integer)
		return;

	if (console_textlen)
	{
		text[0] = '\r';
		memset(&text[1], ' ', console_textlen);
		text[console_textlen+1] = '\r';
		text[console_textlen+2] = 0;
		WriteFile(houtput, text, console_textlen+2, &dummy, NULL);
	}

	WriteFile(houtput, string, strlen(string), &dummy, NULL);

	if (console_textlen)
		WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
    MSG        msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage (&msg, NULL, 0, 0))
			Sys_Quit ();
		sys_msg_time = msg.time;
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	// grab frame time 
	sys_frame_time = timeGetTime();	// FIXME: should this be at start?
}



/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void )
{
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 )
		{
			if ( ( cliptext = GlobalLock( hClipboardData ) ) != 0 ) 
			{
				data = Q_malloc( GlobalSize( hClipboardData ) + 1 );
				strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
#ifndef DEDICATED_ONLY
	ShowWindow ( cl_hwnd, SW_RESTORE);
	SetForegroundWindow ( cl_hwnd );
#endif
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
		if( !FreeLibrary( *lib ) )
			Com_Error( ERR_FATAL, "FreeLibrary failed" );
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
	HINSTANCE lib;
	dllfunc_t *func;

	if( !name || !name[0] || !funcs )
		return NULL;

	Com_DPrintf( "LoadLibrary (%s)\n", name );

	lib = LoadLibrary( name );
	if( !lib )
		return NULL;

	for( func = funcs; func->name; func++ ) {
		*(func->funcPointer) = ( void * )GetProcAddress( lib, func->name );

		if( !(*(func->funcPointer)) ) {
			Sys_UnloadLibrary( &lib );
			Com_Error( ERR_FATAL, "%s: GetProcAddress failed for %s", name, func->name );
		}
	}

	return lib;
}

/*
========================================================================

GAME DLL

========================================================================
*/

static HINSTANCE	game_library, cgame_library, ui_library;

/*
=================
Sys_UnloadGameLibrary
=================
*/
void Sys_UnloadGameLibrary( gamelib_t gamelib )
{
	HINSTANCE *lib;

	switch ( gamelib ) {
		case LIB_GAME: lib = &game_library; break;
		case LIB_CGAME: lib = &cgame_library; break;
		case LIB_UI: lib = &ui_library; break;
		default: assert( qfalse );
	}

	if (!FreeLibrary (*lib))
		Com_Error (ERR_FATAL, "FreeLibrary failed");
	*lib = NULL;
}

/*
=================
Sys_LoadGameLibrary

Loads the game dll
=================
*/
void *Sys_LoadGameLibrary( gamelib_t gamelib, void *parms )
{
	char	name[MAX_OSPATH];
	char	cwd[MAX_OSPATH];
	char	*path;
	void	*(*APIfunc) (void *);
	HINSTANCE *lib;
	char	*libname;
	char	*apifuncname;

#if defined _M_IX86
#define ARCH	"x86"
#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif
#elif defined _M_ALPHA
#define ARCH	"axp"
#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif
#endif

	switch ( gamelib ) {
		case LIB_GAME:
			lib = &game_library;
			libname = "game_" ARCH ".dll";
			apifuncname = "GetGameAPI";
			break;
		case LIB_CGAME:
			lib = &cgame_library;
			libname = "cgame_" ARCH ".dll";
			apifuncname = "GetCGameAPI";
			break;
		case LIB_UI:
			lib = &ui_library;
			libname = "ui_" ARCH ".dll";
			apifuncname = "GetUIAPI";
			break;
		default: assert( qfalse );
	}

	if (*lib)
		Com_Error (ERR_FATAL, "Sys_LoadGameLibrary without Sys_UnloadGameLibrary");

	// check the current debug directory first for development purposes
	_getcwd (cwd, sizeof(cwd));
	Q_snprintfz (name, sizeof(name), "%s/%s/%s", cwd, debugdir, libname);

	*lib = LoadLibrary ( name );
	if (*lib)
	{
		Com_DPrintf ("LoadGameLibrary (%s)\n", name);
	}
	else
	{
#ifdef DEBUG
		// check the current directory for other development purposes
		Q_snprintfz (name, sizeof(name), "%s/%s", cwd, gamename);
		*lib = LoadLibrary ( name );
		if (*lib)
		{
			Com_DPrintf ("LoadGameLibrary (%s)\n", name);
		}
		else
#endif
		{
			// now run through the search paths
			path = NULL;
			while (1)
			{
				path = FS_NextPath (path);
				if (!path)
					return NULL;		// couldn't find one anywhere
				Q_snprintfz (name, sizeof(name), "%s/%s", path, libname);
				*lib = LoadLibrary (name);
				if (*lib)
				{
					Com_DPrintf ("LoadGameLibrary (%s)\n",name);
					break;
				}
			}
		}
	}

	APIfunc = (void *)GetProcAddress (*lib, apifuncname);
	if (!APIfunc)
	{
		Sys_UnloadGameLibrary (gamelib);
		return NULL;
	}

	return APIfunc (parms);
}

//===============================================================================

int MHz, CPUfamily, Mem, InstCache, DataCache, L2Cache;
char VendorID[16];
unsigned char SupportCMOVs, Support3DNow, Support3DNowExt, SupportMMX, SupportMMXext, SupportSSE;

#ifdef id386
#pragma warning(disable: 4035)
#if _MSC_VER || __BORLANDC__
inline __int64 GetCycleNumber()
#else
inline long long GetCycleNumber()
#endif
{
	__asm 
	{
		RDTSC
	}
}

static void GetMHz (void)
{
	LARGE_INTEGER t1, t2, tf;
#if _MSC_VER || __BORLANDC__
	__int64     c1, c2;
#else
	long long   c1, c2;
#endif

	QueryPerformanceFrequency (&tf);
	QueryPerformanceCounter (&t1);
	c1 = GetCycleNumber ();

	_asm {
		MOV  EBX, 5000000
		WaitAlittle:
			DEC		EBX
		JNZ	WaitAlittle
	}

	QueryPerformanceCounter (&t2);
	c2 = GetCycleNumber ();
	
	Com_Printf ("Detecting CPU Speed: %d MHz\n", (int) ((c2 - c1) * tf.QuadPart / (t2.QuadPart - t1.QuadPart) / 1000000));
}

static void GetSysInfo (void)
{
	MEMORYSTATUS ms;

	ms.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus(&ms);
	Mem = ms.dwTotalPhys >> 10;

	__asm {
		PUSHFD
		POP		EAX
		MOV		EBX, EAX
		XOR		EAX, 00200000h
		PUSH	EAX
		POPFD
		PUSHFD
		POP		EAX
		CMP		EAX, EBX
		JZ		ExitCpuTest

			XOR		EAX, EAX
			CPUID

			MOV		DWORD PTR [VendorID],		EBX
			MOV		DWORD PTR [VendorID + 4],	EDX
			MOV		DWORD PTR [VendorID + 8],	ECX
			MOV		DWORD PTR [VendorID + 12],	0

			MOV		EAX, 1
			CPUID
			TEST	EDX, 0x00008000
			SETNZ	AL
			MOV		SupportCMOVs, AL
			TEST	EDX, 0x00800000
			SETNZ	AL
			MOV		SupportMMX, AL
	
			TEST	EDX, 0x02000000
			SETNZ	AL
			MOV		SupportSSE, AL

			SHR		EAX, 8
			AND		EAX, 0x0000000F
			MOV		CPUfamily, EAX
	
			MOV		MHz, 0
			TEST	EDX, 0x00000008
			JZ		NoTimeStampCounter
				CALL	GetMHz
				MOV		MHz, EAX
			NoTimeStampCounter:

			MOV		Support3DNow, 0
			MOV		EAX, 80000000h
			CPUID
			CMP		EAX, 80000000h
			JBE		NoExtendedFunction
				MOV		EAX, 80000001h
				CPUID
				TEST	EDX, 80000000h
				SETNZ	AL
				MOV		Support3DNow, AL

				TEST	EDX, 40000000h
				SETNZ	AL
				MOV		Support3DNowExt, AL

				TEST	EDX, 0x00400000
				SETNZ	AL
				MOV		SupportMMXext, AL

				MOV		EAX, 80000005h
				CPUID
				SHR		ECX, 24
				MOV		DataCache, ECX
				SHR		EDX, 24
				MOV		InstCache, EDX
				
				MOV		EAX, 80000006h
				CPUID
				SHR		ECX, 16
				MOV		L2Cache, ECX

				
			JMP		ExitCpuTest

			NoExtendedFunction:
			MOV		EAX, 2
			CPUID

			MOV		ESI, 4
			TestCache:
				CMP		DL, 0x40
				JNA		NotL2
					MOV		CL, DL
					SUB		CL, 0x40
					SETZ	CH
					DEC		CH
					AND		CL, CH
					MOV		EBX, 64
					SHL		EBX, CL
					MOV		L2Cache, EBX
				NotL2:
				CMP		DL, 0x06
				JNE		Next1
					MOV		InstCache, 8
				Next1:
				CMP		DL, 0x08
				JNE		Next2
					MOV		InstCache, 16
				Next2:
				CMP		DL, 0x0A
				JNE		Next3
					MOV		DataCache, 8
				Next3:
				CMP		DL, 0x0C
				JNE		Next4
					MOV		DataCache, 16
				Next4:
				SHR		EDX, 8
				DEC		ESI
			JNZ	TestCache

		ExitCpuTest:
	}
}

void Sys_PrintCPUInfo (void)
{
	GetSysInfo ();

	Com_Printf ( "CPU Vendor: %s\n", VendorID );
	Com_Printf ( "CPU Family: %i\n", CPUfamily );
	Com_Printf ( "Detecting CPU extensions:\n" );

	if ( Support3DNow ) {
		Com_Printf ( "3DNow!" );
		if ( Support3DNowExt ) {
			Com_Printf ( "(extended)" );
		}
		Com_Printf ( " " );
	}
	if ( SupportMMX ) {
		Com_Printf ( "MMX" );
		if ( SupportMMXext ) {
			Com_Printf ( "(extended)" );
		}
		Com_Printf ( " " );
	}
	if ( SupportSSE ) {
		Com_Printf ( "SSE " );
	}

	Com_Printf ( "\n" );
}
#else
void Sys_PrintCPUInfo (void)
{
}
#endif

//=======================================================================


/*
==================
ParseCommandLine

==================
*/
void ParseCommandLine (LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
			
		}
	}

}

/*
==================
WinMain

==================
*/
HINSTANCE	global_hInstance;
qboolean	hwtimer = qfalse;
double		pfreq;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG				msg;
	int				time, oldtime, newtime;

    /* previous instances do not exist in Win32 */
    if (hPrevInstance)
        return 0;

	global_hInstance = hInstance;

	ParseCommandLine (lpCmdLine);

	Qcommon_Init (argc, argv);

	oldtime = Sys_Milliseconds ();

    /* main window message loop */
	while (1)
	{
		// if at a full screen console, don't update unless needed
		if (Minimized || (dedicated && dedicated->integer) )
		{
			Sleep (1);
		}

		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0))
				Com_Quit ();
			sys_msg_time = msg.time;
			TranslateMessage (&msg);
   			DispatchMessage (&msg);
		}

		do
		{
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);
//			Con_Printf ("time:%5.2f - %5.2f = %5.2f\n", newtime, oldtime, time);

		//	_controlfp( ~( _EM_ZERODIVIDE /*| _EM_INVALID*/ ), _MCW_EM );
		_controlfp( _PC_24, _MCW_PC );
		Qcommon_Frame (time);

		oldtime = newtime;
	}

	// never gets here
    return TRUE;
}
