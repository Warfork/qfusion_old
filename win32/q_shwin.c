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

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

//===============================================================================

int MHz, CPUfamily, Mem, InstCache, DataCache, L2Cache;
char VendorID[16];
unsigned char SupportCMOVs, Support3DNow, Support3DNowExt, SupportMMX, SupportMMXext, SupportSSE;

//===============================================================================

/*
================
Sys_Milliseconds
================
*/
int			curtime;

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

void Sys_Mkdir (char *path)
{
	_mkdir (path);
}

//============================================

char	findbase[MAX_OSPATH];
char	findpath[MAX_OSPATH];
int		findhandle;

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

	if (findhandle)
		Sys_Error ("Sys_BeginFind without close");
	findhandle = 0;

	COM_FilePath (path, findbase);
	findhandle = _findfirst (path, &findinfo);

	while (findhandle != -1) {
		if ( !CompareAttributes( findinfo.attrib, musthave, canthave ) ) {
			_findclose ( findhandle );
			findhandle = _findnext ( findhandle, &findinfo );
		} else {
			Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
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
			Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
			return findpath;
		}
	}

	return NULL;
}

void Sys_FindClose (void)
{
	if (findhandle != -1)
		_findclose (findhandle);
	findhandle = 0;
}


//============================================

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
