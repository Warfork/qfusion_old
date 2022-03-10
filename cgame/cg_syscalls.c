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

#include "cg_local.h"

static cgame_import_t	cgi;
static cgame_export_t	cge;

void trap_Print ( char *msg ) {
	cgi.Print ( msg );
}

void trap_Error ( char *msg ) {
	cgi.Error ( msg );
}

cvar_t *trap_Cvar_Get ( char *name, char *value, int flags ) {
	return cgi.Cvar_Get ( name, value, flags );
}

cvar_t *trap_Cvar_Set ( char *name, char *value ) {
	return cgi.Cvar_Set ( name, value );
}

void trap_Cvar_SetValue ( char *name, float value ) {
	cgi.Cvar_SetValue ( name, value );
}

cvar_t *trap_Cvar_ForceSet ( char *name, char *value ) {
	return cgi.Cvar_ForceSet ( name, value );
}

float trap_Cvar_VariableValue ( char *name ) {
	return cgi.Cvar_VariableValue ( name );
}

char *trap_Cvar_VariableString ( char *name ) {
	return cgi.Cvar_VariableString ( name );
}

int trap_Cmd_Argc (void) {
	return cgi.Cmd_Argc ();
}

char *trap_Cmd_Argv ( int arg ) {
	return cgi.Cmd_Argv ( arg );
}

char *trap_Cmd_Args (void) {
	return cgi.Cmd_Args ();
}

void trap_Cmd_AddCommand ( char *name, void(*cmd)(void) ) {
	cgi.Cmd_AddCommand ( name, cmd );
}

void trap_Cmd_RemoveCommand ( char *cmd_name ) {
	cgi.Cmd_RemoveCommand ( cmd_name );
}

void trap_Cmd_ExecuteText ( int exec_when, char *text ) {
	cgi.Cmd_ExecuteText ( exec_when, text );
}

void trap_Cmd_Execute (void) {
	cgi.Cmd_Execute ();
}

int trap_FS_LoadFile ( const char *name, void **buf ) {
	return cgi.FS_LoadFile ( name, buf );
}

void trap_FS_FreeFile ( void *buf ) {
	cgi.FS_FreeFile ( buf );
}

int trap_FS_FileExists ( const char *path ) {
	return cgi.FS_FileExists ( path );
}

int trap_FS_ListFiles ( const char *path, const char *ext, char *buf, int bufsize ) {
	return cgi.FS_ListFiles ( path, ext, buf, bufsize );
}

char *trap_FS_Gamedir (void) {
	return cgi.FS_Gamedir ();
}

char *trap_Key_GetBindingBuf ( int binding ) {
	return cgi.Key_GetBindingBuf ( binding );
}

char *trap_Key_KeynumToString ( int keynum ) {
	return cgi.Key_KeynumToString ( keynum );
}

void trap_GetConfigString ( int i, char *str, int size ) {
	cgi.GetConfigString ( i, str, size );
}

void trap_NET_GetUserCommand ( int frame, usercmd_t *cmd ) {
	cgi.NET_GetUserCommand ( frame, cmd );
}

void trap_NET_GetCurrentUserCommand ( usercmd_t *cmd ) {
	cgi.NET_GetCurrentUserCommand ( cmd );
}

void trap_NET_GetCurrentState ( int *incomingAcknowledged, int *outgoingSequence ) {
	cgi.NET_GetCurrentState ( incomingAcknowledged, outgoingSequence );
}

void trap_R_UpdateScreen (void) {
	cgi.R_UpdateScreen ();
}

int trap_R_GetClippedFragments ( vec3_t origin, float radius, mat3_t axis, int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments ) {
	return cgi.R_GetClippedFragments ( origin, radius, axis, maxfverts, fverts, maxfragments, fragments );
}

void trap_R_RenderFrame ( refdef_t *fd ) {
	cgi.R_RenderFrame ( fd );
}

void trap_R_RegisterWorldModel ( char *name ) {
	cgi.R_RegisterWorldModel ( name );
}

struct model_s *trap_R_RegisterModel ( char *name ) {
	return cgi.R_RegisterModel ( name );
}

void trap_R_ModelBounds ( struct model_s *mod, vec3_t mins, vec3_t maxs ) {
	cgi.R_ModelBounds ( mod, mins, maxs );
}

struct shader_s *trap_R_RegisterPic ( char *name ) {
	return cgi.R_RegisterPic ( name );
}

struct shader_s *trap_R_RegisterSkin ( char *name ) {
	return cgi.R_RegisterSkin ( name );
}

struct skinfile_s *trap_R_RegisterSkinFile ( char *name ) {
	return cgi.R_RegisterSkinFile ( name );
}

qboolean trap_R_LerpAttachment ( orientation_t *orient, struct model_s *mod, int frame, int oldframe, float backlerp, char *name ) {
	return cgi.R_LerpAttachment ( orient, mod, frame, oldframe, backlerp, name );
}

void trap_Draw_StretchPic ( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader ) {
	cgi.Draw_StretchPic ( x, y, w, h, s1, t1, s2, t2, color, shader );
}

struct cmodel_s *trap_CM_InlineModel ( int num ) {
	return cgi.CM_InlineModel ( num );
}

struct cmodel_s *trap_CM_ModelForBBox ( vec3_t mins, vec3_t maxs ) {
	return cgi.CM_ModelForBBox ( mins, maxs );
}

int trap_CM_NumInlineModels (void) {
	return cgi.CM_NumInlineModels ();
}

void trap_CM_BoxTrace ( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask ) {
	cgi.CM_BoxTrace ( tr, start, end, mins, maxs, cmodel, brushmask );
}

void trap_CM_TransformedBoxTrace ( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles ) {
	cgi.CM_TransformedBoxTrace ( tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}

int trap_CM_PointContents ( vec3_t p, struct cmodel_s *cmodel ) {
	return cgi.CM_PointContents ( p, cmodel );
}

int trap_CM_TransformedPointContents ( vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles ) {
	return cgi.CM_TransformedPointContents ( p, cmodel, origin, angles );
}

void trap_CM_InlineModelBounds ( struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs ) {
	cgi.CM_InlineModelBounds ( cmodel, mins, maxs );
}

struct sfx_s *trap_S_RegisterSound ( char *name ) {
	return cgi.S_RegisterSound ( name );
}

void trap_S_Update ( vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up ) {
	cgi.S_Update ( origin, v_forward, v_right, v_up );
}

void trap_S_StartSound ( vec3_t origin, int entnum, int entchannel, struct sfx_s *sfx, float fvol, float attenuation, float timeofs ) {
	cgi.S_StartSound ( origin, entnum, entchannel, sfx, fvol, attenuation, timeofs );
}

void trap_S_AddLoopSound ( struct sfx_s *sfx, vec3_t origin ) {
	cgi.S_AddLoopSound ( sfx, origin );
}

struct mempool_s *trap_MemAllocPool ( const char *name, const char *filename, int fileline ) {
	return cgi.Mem_AllocPool ( name, filename, fileline );
}

void *trap_MemAlloc ( struct mempool_s *pool, int size, const char *filename, int fileline ) {
	return cgi.Mem_Alloc ( pool, size, filename, fileline );
}

void trap_MemFree ( void *data, const char *filename, int fileline ) {
	cgi.Mem_Free ( data, filename, fileline );
}


void trap_MemFreePool ( struct mempool_s **pool, const char *filename, int fileline ) {
	cgi.Mem_FreePool ( pool, filename, fileline );
}

void trap_MemEmptyPool ( struct mempool_s *pool, const char *filename, int fileline ) {
	cgi.Mem_EmptyPool ( pool, filename, fileline );
}


//======================================================================

/*
=================
GetCGameAPI

Returns a pointer to the structure with all entry points
=================
*/
cgame_export_t *GetCGameAPI ( cgame_import_t *import )
{
	cgi = *import;

	cge.API = CG_API;

	cge.Init = CG_Init;
	cge.Shutdown = CG_Shutdown;

	cge.ServerCommand = CG_ServerCommand;

	cge.LoadLayout = CG_LoadLayout;

	cge.BeginFrameSequence = CG_BeginFrameSequence;
	cge.NewPacketEntityState = CG_NewPacketEntityState;
	cge.EndFrameSequence = CG_EndFrameSequence;

	cge.GetEntitySoundOrigin = CG_GetEntitySoundOrigin;

	cge.Trace = CG_Trace;
	cge.RenderView = CG_RenderView;

	return &cge;
}

#if defined(HAS_DLLMAIN) && !defined(CGAME_HARD_LINKED)
int _stdcall DLLMain (void *hinstDll, unsigned long dwReason, void *reserved)
{
	return 1;
}
#endif
