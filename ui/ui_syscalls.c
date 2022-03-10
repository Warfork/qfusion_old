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

static ui_import_t uii;

void UI_Init (void);
void UI_Shutdown (void);

void UI_Refresh ( int frametime );
void UI_Update (void);

void UI_Keydown ( int key );
void UI_MouseMove (int dx, int dy);

void M_Menu_Main_f (void);
void M_AddToServerList ( netadr_t *adr, char *info );
void M_ForceMenuOff (void);

#define	MAXPRINTMSG	4096
void trap_Sys_Error ( int err_level, char *str, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start ( argptr, str );
	vsnprintf ( msg, sizeof(msg), str, argptr );
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

void trap_Con_Printf ( int print_level, char *str, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start ( argptr, str );
	vsnprintf ( msg, sizeof(msg), str, argptr );
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

void trap_RenderFrame ( refdef_t *fd ) {
	uii.RenderFrame ( fd );
}

void trap_S_StartLocalSound ( char *s ) {
	uii.S_StartLocalSound ( s );
}

float trap_CL_GetTime_f (void) {
	return uii.CL_GetTime_f ();
}

void trap_CL_SetKeyDest_f ( int key_dest ) {
	uii.CL_SetKeyDest_f ( key_dest );
}

void trap_CL_ResetServerCount_f (void) {
	uii.CL_ResetServerCount_f ();
}

void trap_CL_Quit_f (void) {
	uii.CL_Quit_f ();
}

int trap_GetClientState (void) {
	return uii.GetClientState ();
}

int trap_GetServerState (void) {
	return uii.GetServerState ();
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

int trap_FS_FileExists ( char *path ) {
	return uii.FS_FileExists ( path );
}

int	trap_FS_ListFiles ( char *path, char *ext, char *buf, int bufsize ) {
	return uii.FS_ListFiles ( ( const char * )path, ( const char * )ext, buf, bufsize );
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

void trap_DrawStretchPic (int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *color, struct shader_s *shader) {
	uii.DrawStretchPic ( x, y, w, h, s1, t1, s2, t2, color, shader );
}

void trap_DrawChar ( int x, int y, int c, int fontstyle, vec4_t color ) {
	uii.DrawChar ( x, y, c, fontstyle, color );
}

void trap_DrawString ( int x, int y, char *str, int fontstyle, vec4_t color ) {
	uii.DrawString ( x, y, str, fontstyle, color );
}

void trap_DrawPropString ( int x, int y, char *str, int fontstyle, vec4_t color ) {
	uii.DrawPropString ( x, y, str, fontstyle, color );
}

int trap_PropStringLength ( char *str, int fontstyle ) {
	return uii.PropStringLength ( str, fontstyle );
}

void trap_FillRect ( int x, int y, int w, int h, vec4_t color ) {
	uii.FillRect ( x, y, w, h, color );
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

ui_export_t *GetUIAPI (ui_import_t *uiimp)
{
	static ui_export_t	uie;

	uii = *uiimp;

	uie.api_version = UI_API_VERSION;

	uie.Init = UI_Init;
	uie.Shutdown = UI_Shutdown;

	uie.Refresh = UI_Refresh;
	uie.Update = UI_Update;

	uie.Keydown = UI_Keydown;
	uie.MouseMove = UI_MouseMove;

	uie.MainMenu = M_Menu_Main_f;
	uie.ForceMenuOff = M_ForceMenuOff;
	uie.AddToServerList = M_AddToServerList;

	return &uie;
}

#ifndef UI_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	uii.Sys_Error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	uii.Con_Printf (PRINT_ALL, "%s", text);
}

#endif

