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
static void CL_UIModule_Print( char *msg ) {
	Com_Printf( "%s", msg );
}

/*
===============
CL_UIModule_Error
===============
*/
static void CL_UIModule_Error( char *msg ) {
	Com_Error( ERR_FATAL, "%s", msg );
}

/*
===============
CL_UIModule_GetConfigString
===============
*/
static void CL_UIModule_GetConfigString ( int i, char *str, int size )
{
	if( i < 0 || i >= MAX_CONFIGSTRINGS )
		Com_Error ( ERR_DROP, "CL_UIModule_GetConfigString: i > MAX_CONFIGSTRINGS" );
	if( !str || size <= 0 )
		Com_Error ( ERR_DROP, "CL_UIModule_GetConfigString: NULL string" );
	Q_strncpyz( str, cl.configstrings[i], size );
}

/*
===============
CL_UIModule_MemAlloc
===============
*/
static void *CL_UIModule_MemAlloc( mempool_t *pool, int size, const char *filename, int fileline ) {
	return _Mem_Alloc( pool, size, MEMPOOL_USERINTERFACE, 0, filename, fileline );
}

/*
===============
CL_UIModule_MemFree
===============
*/
static void CL_UIModule_MemFree( void *data, const char *filename, int fileline ) {
	_Mem_Free( data, MEMPOOL_USERINTERFACE, 0, filename, fileline );
}

/*
===============
CL_UIModule_MemAllocPool
===============
*/
static mempool_t *CL_UIModule_MemAllocPool( const char *name, const char *filename, int fileline ) {
	return _Mem_AllocPool( ui_mempool, name, MEMPOOL_USERINTERFACE, filename, fileline );
}

/*
===============
CL_UIModule_MemFreePool
===============
*/
static void CL_UIModule_MemFreePool( mempool_t **pool, const char *filename, int fileline ) {
	_Mem_FreePool( pool, MEMPOOL_USERINTERFACE, 0, filename, fileline );
}

/*
===============
CL_UIModule_MemEmptyPool
===============
*/
static void CL_UIModule_MemEmptyPool( mempool_t *pool, const char *filename, int fileline ) {
	_Mem_EmptyPool( pool, MEMPOOL_GAMEPROGS, 0, filename, fileline );
}

/*
==============
CL_UIModule_Init
==============
*/
void CL_UIModule_Init (void)
{
	int apiversion;
	ui_import_t	import;

	CL_UIModule_Shutdown ();

	ui_mempool = Mem_AllocPool( NULL, "User Iterface" );

	import.Error = CL_UIModule_Error;
	import.Print = CL_UIModule_Print;

	import.Cvar_Get = Cvar_Get;
	import.Cvar_Set = Cvar_Set;
	import.Cvar_SetValue = Cvar_SetValue;
	import.Cvar_ForceSet = Cvar_ForceSet;
	import.Cvar_VariableString = Cvar_VariableString;
	import.Cvar_VariableValue = Cvar_VariableValue;

	import.Cmd_AddCommand = Cmd_AddCommand;
	import.Cmd_RemoveCommand = Cmd_RemoveCommand;
	import.Cmd_ExecuteText = Cbuf_ExecuteText;
	import.Cmd_Execute = Cbuf_Execute;

	import.FS_FOpenFile = FS_FOpenFile;
	import.FS_Read = FS_Read;
	import.FS_Write = FS_Write;
	import.FS_Tell = FS_Tell;
	import.FS_Seek = FS_Seek;
	import.FS_Eof = FS_Eof;
	import.FS_Flush = FS_Flush;
	import.FS_FCloseFile = FS_FCloseFile;
	import.FS_GetFileList = FS_GetFileList;
	import.FS_Gamedir = FS_Gamedir;

	import.CL_Quit = CL_Quit;
	import.CL_SetKeyDest = CL_SetKeyDest;
	import.CL_ResetServerCount = CL_ResetServerCount;
	import.CL_GetClipboardData = CL_GetClipboardData;

	import.Key_ClearStates = Key_ClearStates;
	import.Key_GetBindingBuf = Key_GetBindingBuf;
	import.Key_KeynumToString = Key_KeynumToString;
	import.Key_SetBinding = Key_SetBinding;
	import.Key_IsDown = Key_IsDown;

	import.R_ClearScene = R_ClearScene;
	import.R_AddEntityToScene = R_AddEntityToScene;
	import.R_AddLightToScene = R_AddLightToScene;
	import.R_AddPolyToScene = R_AddPolyToScene;
	import.R_RenderScene = R_RenderScene;
	import.R_EndFrame = R_EndFrame;
	import.R_ModelBounds = R_ModelBounds;
	import.R_RegisterModel = R_RegisterModel;
	import.R_RegisterPic = R_RegisterPic;
	import.R_RegisterSkin = R_RegisterSkin;
	import.R_RegisterSkinFile = R_RegisterSkinFile;
	import.R_LerpAttachment = R_LerpAttachment;
	import.R_DrawStretchPic = R_DrawStretchPic;

	import.S_StartLocalSound = S_StartLocalSound;
	import.S_StartBackgroundTrack = S_StartBackgroundTrack;
	import.S_StopBackgroundTrack = S_StopBackgroundTrack;

	import.GetConfigString = CL_UIModule_GetConfigString;

	import.Milliseconds = Sys_Milliseconds;

	import.Mem_Alloc = CL_UIModule_MemAlloc;
	import.Mem_Free = CL_UIModule_MemFree;
	import.Mem_AllocPool = CL_UIModule_MemAllocPool;
	import.Mem_FreePool = CL_UIModule_MemFreePool;
	import.Mem_EmptyPool = CL_UIModule_MemEmptyPool;

	uie = ( ui_export_t * )Sys_LoadGameLibrary( LIB_UI, &import );
	if ( !uie )
		Com_Error( ERR_DROP, "Failed to load UI dll" );

	apiversion = uie->API ();
	if ( apiversion != UI_API_VERSION ) {
		Sys_UnloadGameLibrary( LIB_UI );
		Mem_FreePool( &ui_mempool );
		uie = NULL;

		Com_Error( ERR_FATAL, "ui version is %i, not %i", apiversion, UI_API_VERSION );
	}

	uie->Init( viddef.width, viddef.height );
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
	Sys_UnloadGameLibrary( LIB_UI );
	Mem_FreePool( &ui_mempool );
	uie = NULL;
}

/*
===============
CL_UIModule_Refresh
===============
*/
void CL_UIModule_Refresh( qboolean backGround )
{
	if( uie )
		uie->Refresh( cls.realtime, Com_ClientState (), Com_ServerState (), backGround );
}

/*
===============
CL_UIModule_DrawConnectScreen
===============
*/
void CL_UIModule_DrawConnectScreen( qboolean backGround )
{
	if( uie )
		uie->DrawConnectScreen( cls.servername, cls.connect_count, backGround );
}

/*
===============
CL_UIModule_Keydown
===============
*/
void CL_UIModule_Keydown( int key )
{
	if( uie )
		uie->Keydown( key );
}

/*
===============
CL_UIModule_MenuMain
===============
*/
void CL_UIModule_MenuMain( void )
{
	if( uie )
		uie->MainMenu ();
}

/*
===============
CL_UIModule_ForceMenuOff
===============
*/
void CL_UIModule_ForceMenuOff( void )
{
	if( uie )
		uie->ForceMenuOff ();
}

/*
===============
CL_UIModule_AddToServerList
===============
*/
void CL_UIModule_AddToServerList( char *adr, char *info )
{
	if( uie )
		uie->AddToServerList( adr, info );
}

/*
===============
CL_UIModule_MouseMove
===============
*/
void CL_UIModule_MouseMove( int dx, int dy )
{
	if( uie )
		uie->MouseMove( dx, dy );
}
