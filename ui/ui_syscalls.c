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

void trap_Error( char *str ) {
	uii.Error( str );
}

void trap_Print( char *str ) {
	uii.Print( str );
}

void trap_Cmd_AddCommand( char *name, void(*cmd)(void) ) {
	uii.Cmd_AddCommand( name, cmd );
}

void trap_Cmd_RemoveCommand( char *cmd_name ) {
	uii.Cmd_RemoveCommand( cmd_name );
}

void trap_Cmd_ExecuteText( int exec_when, char *text ) {
	uii.Cmd_ExecuteText( exec_when, text );
}

void trap_Cmd_Execute( void ) {
	uii.Cmd_Execute ();
}

void trap_R_ClearScene( void ) {
	uii.R_ClearScene ();
}

void trap_R_AddEntityToScene( entity_t *ent ) {
	uii.R_AddEntityToScene( ent );
}

void trap_R_AddLightToScene( vec3_t org, float intensity, float r, float g, float b ) {
	uii.R_AddLightToScene( org, intensity, r, g, b );
}

void trap_R_AddPolyToScene( poly_t *poly ) {
	uii.R_AddPolyToScene( poly );
}

void trap_R_RenderScene( refdef_t *fd ) {
	uii.R_RenderScene( fd );
}

void trap_R_EndFrame( void ) {
	uii.R_EndFrame ();
}

void trap_R_ModelBounds( struct model_s *mod, vec3_t mins, vec3_t maxs ) {
	uii.R_ModelBounds( mod, mins, maxs );
}

struct model_s *trap_R_RegisterModel( char *name ) {
	return uii.R_RegisterModel( name );
}

struct shader_s *trap_R_RegisterSkin( char *name ) {
	return uii.R_RegisterSkin( name );
}

struct shader_s *trap_R_RegisterPic( char *name ) {
	return uii.R_RegisterPic( name );
}

struct skinfile_s *trap_R_RegisterSkinFile( char *name ) {
	return uii.R_RegisterSkinFile( name );
}

qboolean trap_R_LerpAttachment( orientation_t *orient, struct model_s *mod, int oldframe, int frame, float lerpfrac, char *name ) {
	return uii.R_LerpAttachment( orient, mod, oldframe, frame, lerpfrac, name );
}

void trap_R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader ) {
	uii.R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, color, shader );
}

void trap_S_StartLocalSound( char *s ) {
	uii.S_StartLocalSound( s );
}

void trap_S_StartBackgroundTrack( char *intro, char *loop ) {
	uii.S_StartBackgroundTrack( intro, loop );
}

void trap_S_StopBackgroundTrack( void ) {
	uii.S_StopBackgroundTrack ();
}

void trap_CL_Quit( void ) {
	uii.CL_Quit ();
}

void trap_CL_SetKeyDest( int key_dest ) {
	uii.CL_SetKeyDest( key_dest );
}

void trap_CL_ResetServerCount( void ) {
	uii.CL_ResetServerCount ();
}

void trap_CL_GetClipboardData( char *string, int size ) {
	uii.CL_GetClipboardData( string, size );
}

char *trap_Key_GetBindingBuf( int binding ) {
	return uii.Key_GetBindingBuf( binding );
}

void trap_Key_ClearStates( void ) {
	uii.Key_ClearStates ();
}

char *trap_Key_KeynumToString( int keynum ) {
	return uii.Key_KeynumToString( keynum );
}

void trap_Key_SetBinding( int keynum, char *binding ) {
	uii.Key_SetBinding( keynum, binding );
}

qboolean trap_Key_IsDown( int keynum ) {
	return uii.Key_IsDown( keynum );
}

void trap_GetConfigString( int i, char *str, int size ) {
	uii.GetConfigString( i, str, size );
}

int trap_Milliseconds( void ) {
	return uii.Milliseconds ();
}

int	trap_FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return uii.FS_FOpenFile( filename, filenum, mode );
}

int	trap_FS_Read( void *buffer, size_t len, int file ) {
	return uii.FS_Read( buffer, len, file );
}

int	trap_FS_Write( const void *buffer, size_t len, int file ) {
	return uii.FS_Write( buffer, len, file );
}

int	trap_FS_Tell( int file ) {
	return uii.FS_Tell( file );
}

int	trap_FS_Seek( int file, int offset, int whence ) {
	return uii.FS_Seek( file, offset, whence );
}

int	trap_FS_Eof( int file ) {
	return uii.FS_Eof( file );
}

int	trap_FS_Flush( int file ) {
	return uii.FS_Flush( file );
}

void trap_FS_FCloseFile( int file ) { 
	uii.FS_FCloseFile( file );
}

int	trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize ) {
	return uii.FS_GetFileList( dir, extension, buf, bufsize );
}

char *trap_FS_Gamedir( void ) {
	return uii.FS_Gamedir ();
}

cvar_t *trap_Cvar_Get( char *name, char *value, int flags ) {
	return uii.Cvar_Get( name, value, flags );
}

cvar_t *trap_Cvar_Set( char *name, char *value ) {
	return uii.Cvar_Set( name, value );
}

void trap_Cvar_SetValue( char *name, float value ) {
	uii.Cvar_SetValue( name, value );
}

cvar_t *trap_Cvar_ForceSet( char *name, char *value ) {
	return uii.Cvar_ForceSet( name, value );
}

float trap_Cvar_VariableValue( char *name ) {
	return uii.Cvar_VariableValue( name );
}

char *trap_Cvar_VariableString( char *name ) {
	return uii.Cvar_VariableString( name );
}

struct mempool_s *trap_Mem_AllocPool( const char *name, const char *filename, int fileline ) {
	return uii.Mem_AllocPool( name, filename, fileline );
}

void *trap_Mem_Alloc( struct mempool_s *pool, int size, const char *filename, int fileline ) {
	return uii.Mem_Alloc( pool, size, filename, fileline );
}

void trap_Mem_Free( void *data, const char *filename, int fileline ) {
	uii.Mem_Free( data, filename, fileline );
}

void trap_Mem_FreePool( struct mempool_s **pool, const char *filename, int fileline ) {
	uii.Mem_FreePool( pool, filename, fileline );
}

void trap_Mem_EmptyPool( struct mempool_s *pool, const char *filename, int fileline ) {
	uii.Mem_EmptyPool( pool, filename, fileline );
}

//======================================================================

/*
=================
GetUIAPI

Returns a pointer to the structure with all entry points
=================
*/
ui_export_t *GetUIAPI( ui_import_t *import )
{
	static ui_export_t	globals;

	uii = *import;

	globals.API = UI_API;

	globals.Init = UI_Init;
	globals.Shutdown = UI_Shutdown;

	globals.Refresh = UI_Refresh;
	globals.DrawConnectScreen = UI_DrawConnectScreen;

	globals.Keydown = UI_Keydown;
	globals.MouseMove = UI_MouseMove;

	globals.MainMenu = M_Menu_Main_f;
	globals.ForceMenuOff = M_ForceMenuOff;
	globals.AddToServerList = M_AddToServerList;

	return &globals;
}

#if defined(HAVE_DLLMAIN) && !defined(UI_HARD_LINKED)
int _stdcall DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif
