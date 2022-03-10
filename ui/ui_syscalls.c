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

#include "ui_local.h"

ui_import_t uii;

void trap_Error ( char *str ) {
	uii.Error ( str );
}

void trap_Print ( char *str ) {
	uii.Print ( str );
}

void trap_Cmd_AddCommand ( char *name, void(*cmd)(void) ) {
	uii.Cmd_AddCommand ( name, cmd );
}

void trap_Cmd_RemoveCommand ( char *cmd_name ) {
	uii.Cmd_RemoveCommand ( cmd_name );
}

void trap_Cmd_ExecuteText ( int exec_when, char *text ) {
	uii.Cmd_ExecuteText ( exec_when, text );
}

void trap_Cmd_Execute (void) {
	uii.Cmd_Execute ();
}

void trap_R_RenderFrame ( refdef_t *fd ) {
	uii.R_RenderFrame ( fd );
}

void trap_R_EndFrame (void) {
	uii.R_EndFrame ();
}

void trap_R_ModelBounds ( struct model_s *mod, vec3_t mins, vec3_t maxs ) {
	uii.R_ModelBounds ( mod, mins, maxs );
}

struct model_s *trap_R_RegisterModel ( char *name ) {
	return uii.R_RegisterModel ( name );
}

struct shader_s *trap_R_RegisterSkin ( char *name ) {
	return uii.R_RegisterSkin ( name );
}

struct shader_s *trap_R_RegisterPic ( char *name ) {
	return uii.R_RegisterPic ( name );
}

struct skinfile_s *trap_R_RegisterSkinFile ( char *name ) {
	return uii.R_RegisterSkinFile ( name );
}

qboolean trap_R_LerpAttachment ( orientation_t *orient, struct model_s *mod, int frame, int oldframe, float backlerp, char *name ) {
	return uii.R_LerpAttachment ( orient, mod, frame, oldframe, backlerp, name );
}

void trap_S_StartLocalSound ( char *s ) {
	uii.S_StartLocalSound ( s );
}

void trap_CL_Quit (void) {
	uii.CL_Quit ();
}

void trap_CL_SetKeyDest ( int key_dest ) {
	uii.CL_SetKeyDest ( key_dest );
}

void trap_CL_ResetServerCount (void) {
	uii.CL_ResetServerCount ();
}

void trap_CL_GetClipboardData ( char *string, int size ) {
	uii.CL_GetClipboardData ( string, size );
}

char *trap_Key_GetBindingBuf ( int binding ) {
	return uii.Key_GetBindingBuf ( binding );
}

void trap_Key_ClearStates (void) {
	uii.Key_ClearStates ();
}

char *trap_Key_KeynumToString ( int keynum ) {
	return uii.Key_KeynumToString ( keynum );
}

void trap_Key_SetBinding ( int keynum, char *binding ) {
	uii.Key_SetBinding ( keynum, binding );
}

qboolean trap_Key_IsDown ( int keynum ) {
	return uii.Key_IsDown ( keynum );
}

void trap_GetConfigString ( int i, char *str, int size ) {
	uii.GetConfigString ( i, str, size );
}

int	trap_FS_LoadFile ( const char *name, void **buf ) {
	return uii.FS_LoadFile ( name, buf );
}

void trap_FS_FreeFile ( void *buf ) {
	uii.FS_FreeFile ( buf );
}

int trap_FS_FileExists ( const char *path ) {
	return uii.FS_FileExists ( path );
}

int	trap_FS_ListFiles ( const char *path, const char *ext, char *buf, int bufsize ) {
	return uii.FS_ListFiles ( path, ext, buf, bufsize );
}

char *trap_FS_Gamedir (void) {
	return uii.FS_Gamedir ();
}

cvar_t *trap_Cvar_Get ( char *name, char *value, int flags ) {
	return uii.Cvar_Get ( name, value, flags );
}

cvar_t *trap_Cvar_Set( char *name, char *value ) {
	return uii.Cvar_Set ( name, value );
}

void trap_Cvar_SetValue ( char *name, float value ) {
	uii.Cvar_SetValue ( name, value );
}

cvar_t *trap_Cvar_ForceSet ( char *name, char *value ) {
	return uii.Cvar_ForceSet ( name, value );
}

float trap_Cvar_VariableValue ( char *name ) {
	return uii.Cvar_VariableValue ( name );
}

char *trap_Cvar_VariableString ( char *name ) {
	return uii.Cvar_VariableString ( name );
}

struct mempool_s *trap_Mem_AllocPool ( const char *name, const char *filename, int fileline ) {
	return uii.Mem_AllocPool ( name, filename, fileline );
}

void *trap_Mem_Alloc ( struct mempool_s *pool, int size, const char *filename, int fileline ) {
	return uii.Mem_Alloc ( pool, size, filename, fileline );
}

void trap_Mem_Free ( void *data, const char *filename, int fileline ) {
	uii.Mem_Free ( data, filename, fileline );
}


void trap_Mem_FreePool ( struct mempool_s **pool, const char *filename, int fileline ) {
	uii.Mem_FreePool ( pool, filename, fileline );
}

void trap_Mem_EmptyPool ( struct mempool_s *pool, const char *filename, int fileline ) {
	uii.Mem_EmptyPool ( pool, filename, fileline );
}

void trap_Draw_StretchPic ( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader ) {
	uii.Draw_StretchPic ( x, y, w, h, s1, t1, s2, t2, color, shader );
}


/*
=================
GetUIAPI

Returns a pointer to the structure with all entry points
=================
*/
ui_export_t *GetUIAPI (ui_import_t *uiimp)
{
	static ui_export_t	uie;

	uii = *uiimp;

	uie.API = UI_API;

	uie.Init = UI_Init;
	uie.Shutdown = UI_Shutdown;

	uie.Refresh = UI_Refresh;
	uie.DrawConnectScreen = UI_DrawConnectScreen;

	uie.Keydown = UI_Keydown;
	uie.MouseMove = UI_MouseMove;

	uie.MainMenu = M_Menu_Main_f;
	uie.ForceMenuOff = M_ForceMenuOff;
	uie.AddToServerList = M_AddToServerList;

	return &uie;
}

#if defined(HAS_DLLMAIN) && !defined(UI_HARD_LINKED)
int _stdcall DLLMain (void *hinstDll, unsigned long dwReason, void *reserved)
{
	return 1;
}
#endif
