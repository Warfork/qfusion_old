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

#include "../client/ref.h"
#include "ui.h"

static uiimport_t uii;

void M_Init (void);
void M_Shutdown (void);

void M_Draw (void);
void M_Keydown ( int key );

void M_Menu_Main_f (void);
void M_AddToServerList ( netadr_t *adr, char *info );
void M_ForceMenuOff (void);

#define	MAXPRINTMSG	4096
void trap_Sys_Error ( int err_level, char *str, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start ( argptr, str );
	vsprintf ( msg, str, argptr );
	va_end ( argptr );

	uii.Sys_Error ( err_level, msg );
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

qboolean trap_Con_Func (void) {
	return uii.Con_Func ();
}

void trap_Con_Printf ( int print_level, char *str, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start ( argptr, str );
	vsprintf ( msg, str, argptr );
	va_end ( argptr );

	uii.Con_Printf ( print_level, str );
}

struct model_s *trap_RegisterModel ( char *name ) {
	return uii.RegisterModel ( name );
}

struct shader_s *trap_RegisterSkin ( char *name ) {
	return uii.RegisterSkin ( name );
}

struct shader_s *trap_RegisterPic ( char *name ) {
	return uii.RegisterPic ( name );
}

int	trap_ModelNumFrames ( struct model_s *model ) {
	return uii.ModelNumFrames ( model );
}

void trap_RenderFrame ( refdef_t *fd ) {
	uii.RenderFrame ( fd );
}

void trap_S_StartLocalSound ( char *s ) {
	uii.S_StartLocalSound ( s );
}

void trap_CL_PingServers_f (void) {
	uii.CL_PingServers_f ();
}

float trap_CL_GetTime_f (void) {
	return uii.CL_GetTime_f ();
}

void trap_CL_SetKeyDest_f ( enum keydest_t keydest ) {
	uii.CL_SetKeyDest_f ( keydest );
}

void trap_CL_ResetServerCount_f (void) {
	uii.CL_ResetServerCount_f ();
}

void trap_CL_Quit_f (void) {
	uii.CL_Quit_f ();
}

int trap_Com_ServerState (void) {
	return uii.Com_ServerState ();
}

char *trap_NET_AdrToString ( netadr_t *a ) {
	return uii.NET_AdrToString ( a );
}

char *trap_Key_GetBindingBuf ( int binding ) {
	return uii.Key_GetBindingBuf ( binding );
}

void trap_Key_ClearStates(void) {
	uii.Key_ClearStates ();
}

char *trap_Key_KeynumToString ( int keynum ) {
	return uii.Key_KeynumToString ( keynum );
}

void trap_Key_SetBinding ( int keynum, char *binding ) {
	uii.Key_SetBinding ( keynum, binding );
}

int	trap_FS_LoadFile ( char *name, void **buf )
{
	return uii.FS_LoadFile ( name, buf );
}

void trap_FS_FreeFile ( void *buf ) {
	uii.FS_FreeFile ( buf );
}

int	trap_FS_ListFiles ( char *path, char *ext, char *buf, int bufsize ) {
	return uii.FS_ListFiles ( path, ext, buf, bufsize );
}

char *trap_FS_NextPath ( char *prevpath ) {
	return uii.FS_NextPath ( prevpath );
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

void trap_DrawPic ( int x, int y, char *name ) {
	uii.DrawPic ( x, y, name );
}

void trap_DrawChar ( int x, int y, int c, fontstyle_t fntstl, vec4_t colour ) {
	uii.DrawChar ( x, y, c, fntstl, colour );
}

void trap_DrawStringLen ( int x, int y, char *str, int len, fontstyle_t fntstl, vec4_t colour ) {
	uii.DrawStringLen ( x, y, str, len, fntstl, colour );
}

void trap_DrawFill ( int x, int y, int w, int h, int c ) {
	uii.DrawFill ( x, y, w, h, c );
}

void trap_EndFrame (void) {
	uii.EndFrame ();
}

void trap_Vid_GetCurrentInfo ( int *width, int *height ) {
	uii.Vid_GetCurrentInfo ( width, height );
}

int trap_GetWidth (void) {
	static int vid_width;
	uii.Vid_GetCurrentInfo ( &vid_width, NULL );
	return vid_width;
}

int trap_GetHeight (void) {
	static int vid_height;
	uii.Vid_GetCurrentInfo ( NULL, &vid_height );
	return vid_height;
}

int Q_PlayerGender ( void *player ) {
	return GENDER_MALE;
}

__declspec(dllexport) uiexport_t GetUiAPI (uiimport_t uiimp)
{
	uiexport_t	uie;

	uii = uiimp;

	uie.api_version = UI_API_VERSION;

	uie.Init = M_Init;
	uie.Shutdown = M_Shutdown;

	uie.Draw = M_Draw;
	uie.Keydown = M_Keydown;

	uie.MainMenu = M_Menu_Main_f;
	uie.ForceMenuOff = M_ForceMenuOff;
	uie.AddToServerList = M_AddToServerList;

	return uie;
}

#ifndef UI_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	uii.Sys_Error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	uii.Con_Printf (PRINT_ALL, "%s", text);
}

#endif

