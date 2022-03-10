/*
Copyright (C) 2002-2003 Victor Luchits

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

#include "client.h"
#include "../ui/ui_public.h"

// Structure containing functions exported from user interface DLL
ui_export_t	*uie;

mempool_t	*ui_mempool;

/*
===============
CL_UIModule_Print
===============
*/
static void CL_UIModule_Print ( char *msg ) {
	Com_Printf ( "%s", msg );
}

/*
===============
CL_UIModule_Error
===============
*/
static void CL_UIModule_Error ( char *msg ) {
	Com_Error ( ERR_FATAL, "%s", msg );
}

/*
===============
CL_UIModule_GetConfigString
===============
*/
static void CL_UIModule_GetConfigString ( int i, char *str, int size )
{
	if ( i < 0 || i >= MAX_CONFIGSTRINGS ) {
		Com_Error ( ERR_DROP, "CL_UIModule_GetConfigString: i > MAX_CONFIGSTRINGS" );
	}
	if ( !str || size <= 0 ) {
		Com_Error ( ERR_DROP, "CL_UIModule_GetConfigString: NULL string" );
	}

	Q_strncpyz ( str, cl.configstrings[i], size );
}

/*
===============
CL_UIModule_MemAlloc
===============
*/
static void *CL_UIModule_MemAlloc ( mempool_t *pool, int size, const char *filename, int fileline ) {
	return _Mem_Alloc ( pool, size, MEMPOOL_USERINTERFACE, 0, filename, fileline );
}

/*
===============
CL_UIModule_MemFree
===============
*/
static void CL_UIModule_MemFree ( void *data, const char *filename, int fileline ) {
	_Mem_Free ( data, MEMPOOL_USERINTERFACE, 0, filename, fileline );
}

/*
===============
CL_UIModule_MemAllocPool
===============
*/
static mempool_t *CL_UIModule_MemAllocPool ( const char *name, const char *filename, int fileline ) {
	return _Mem_AllocPool ( ui_mempool, name, MEMPOOL_USERINTERFACE, filename, fileline );
}

/*
===============
CL_UIModule_MemFreePool
===============
*/
static void CL_UIModule_MemFreePool ( mempool_t **pool, const char *filename, int fileline ) {
	_Mem_FreePool ( pool, MEMPOOL_USERINTERFACE, 0, filename, fileline );
}

/*
===============
CL_UIModule_MemEmptyPool
===============
*/
static void CL_UIModule_MemEmptyPool ( mempool_t *pool, const char *filename, int fileline ) {
	_Mem_EmptyPool ( pool, MEMPOOL_GAMEPROGS, 0, filename, fileline );
}

/*
==============
CL_UIModule_Init
==============
*/
void CL_UIModule_Init (void)
{
	int apiversion;
	ui_import_t	ui;

	CL_UIModule_Shutdown ();

	ui_mempool = Mem_AllocPool ( NULL, "User Iterface" );

	ui.Error = CL_UIModule_Error;
	ui.Print = CL_UIModule_Print;

	ui.R_RenderFrame = R_RenderFrame;
	ui.R_EndFrame = GLimp_EndFrame;
	ui.R_ModelBounds = R_ModelBounds;
	ui.R_RegisterModel = R_RegisterModel;
	ui.R_RegisterPic = R_RegisterPic;
	ui.R_RegisterSkin = R_RegisterSkin;
	ui.R_RegisterSkinFile = R_RegisterSkinFile;
	ui.R_LerpAttachment = R_LerpAttachment;

	ui.S_StartLocalSound = S_StartLocalSound;

	ui.CL_Quit = CL_Quit;
	ui.CL_SetKeyDest = CL_SetKeyDest;
	ui.CL_ResetServerCount = CL_ResetServerCount;
	ui.CL_GetClipboardData = CL_GetClipboardData;

	ui.Cmd_AddCommand = Cmd_AddCommand;
	ui.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ui.Cmd_ExecuteText = Cbuf_ExecuteText;
	ui.Cmd_Execute = Cbuf_Execute;

	ui.FS_LoadFile = FS_LoadFile;
	ui.FS_FreeFile = FS_FreeFile;
	ui.FS_FileExists = FS_FileExists;
	ui.FS_Gamedir = FS_Gamedir;
	ui.FS_ListFiles = FS_GetFileList;

	ui.Mem_Alloc = CL_UIModule_MemAlloc;
	ui.Mem_Free = CL_UIModule_MemFree;
	ui.Mem_AllocPool = CL_UIModule_MemAllocPool;
	ui.Mem_FreePool = CL_UIModule_MemFreePool;
	ui.Mem_EmptyPool = CL_UIModule_MemEmptyPool;

	ui.Cvar_Get = Cvar_Get;
	ui.Cvar_Set = Cvar_Set;
	ui.Cvar_SetValue = Cvar_SetValue;
	ui.Cvar_ForceSet = Cvar_ForceSet;
	ui.Cvar_VariableString = Cvar_VariableString;
	ui.Cvar_VariableValue = Cvar_VariableValue;

	ui.Key_ClearStates = Key_ClearStates;
	ui.Key_GetBindingBuf = Key_GetBindingBuf;
	ui.Key_KeynumToString = Key_KeynumToString;
	ui.Key_SetBinding = Key_SetBinding;
	ui.Key_IsDown = Key_IsDown;

	ui.GetConfigString = CL_UIModule_GetConfigString;

	ui.Draw_StretchPic = Draw_StretchPic;

	uie = ( ui_export_t * )Sys_LoadLibrary ( LIB_UI, &ui );
	if ( !uie ) {
		Com_Error ( ERR_DROP, "Failed to load UI dll" );
	}

	apiversion = uie->API ();
	if ( apiversion != UI_API_VERSION ) {
		Sys_UnloadLibrary ( LIB_UI );
		Mem_FreePool ( &ui_mempool );
		uie = NULL;

		Com_Error ( ERR_FATAL, "ui version is %i, not %i", apiversion, UI_API_VERSION );
	}

	uie->Init ( viddef.width, viddef.height );
}

/*
===============
CL_UIModule_Shutdown
===============
*/
void CL_UIModule_Shutdown ( void )
{
	if ( !uie )
		return;

	uie->Shutdown ();
	Sys_UnloadLibrary ( LIB_UI );
	Mem_FreePool ( &ui_mempool );
	uie = NULL;
}

/*
===============
CL_UIModule_Refresh
===============
*/
void CL_UIModule_Refresh ( qboolean backGround )
{
	if ( uie ) {
		uie->Refresh ( cls.realtime, Com_ClientState (), Com_ServerState (), backGround );
	}
}

/*
===============
CL_UIModule_DrawConnectScreen
===============
*/
void CL_UIModule_DrawConnectScreen ( qboolean backGround )
{
	if ( uie ) {
		uie->DrawConnectScreen ( cls.servername, cls.connect_count, backGround );
	}
}

/*
===============
CL_UIModule_Keydown
===============
*/
void CL_UIModule_Keydown ( int key )
{
	if ( uie ) {
		uie->Keydown ( key );
	}
}

/*
===============
CL_UIModule_MenuMain
===============
*/
void CL_UIModule_MenuMain (void)
{
	if ( uie ) {
		uie->MainMenu ();
	}
}

/*
===============
CL_UIModule_ForceMenuOff
===============
*/
void CL_UIModule_ForceMenuOff (void)
{
	if ( uie ) {
		uie->ForceMenuOff ();
	}
}

/*
===============
CL_UIModule_AddToServerList
===============
*/
void CL_UIModule_AddToServerList ( char *adr, char *info )
{
	if ( uie ) {
		uie->AddToServerList ( adr, info );
	}
}

/*
===============
CL_UIModule_MouseMove
===============
*/
void CL_UIModule_MouseMove ( int dx, int dy )
{
	if ( uie ) {
		uie->MouseMove ( dx, dy );
	}
}
