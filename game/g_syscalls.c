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

#include "g_local.h"

static game_import_t gi;
static game_export_t globals;

void trap_Print ( char *msg ) {
	gi.Print ( msg );
}

void trap_Error ( char *msg ) {
	gi.Error ( msg );
}

void trap_Sound ( vec3_t origin, struct edict_s *ent, int channel, int soundIndex, float volume, float attenuation ) {
	gi.Sound ( origin, ent, channel, soundIndex, volume, attenuation );
}

void trap_ServerCmd ( struct edict_s *ent, char *cmd ) {
	gi.ServerCmd ( ent, cmd );
}

void trap_ConfigString ( int num, char *string ) {
	gi.ConfigString ( num, string );
}

void trap_Layout ( struct edict_s *ent, char *string ) {
	gi.Layout ( ent, string );
}

void trap_StuffCmd ( struct edict_s *ent, char *string ) {
	gi.StuffCmd ( ent, string );
}

int trap_ModelIndex ( char *name ) {
	return gi.ModelIndex ( name );
}

int trap_SoundIndex ( char *name ) {
	return gi.SoundIndex ( name );
}

int trap_ImageIndex ( char *name ) {
	return gi.ImageIndex ( name );
}

void trap_SetBrushModel ( struct edict_s *ent, char *name ) {
	gi.SetBrushModel ( ent, name );
}

void trap_Trace ( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, struct edict_s *passent, int contentmask ) {
	gi.Trace ( tr, start, mins, maxs, end, passent, contentmask );
}

int trap_PointContents ( vec3_t point ) {
	return gi.PointContents ( point );
}

qboolean trap_inPVS ( vec3_t p1, vec3_t p2 ) {
	return gi.inPVS ( p1, p2 );
}

qboolean trap_inPHS ( vec3_t p1, vec3_t p2 ) {
	return gi.inPHS ( p1, p2 );
}

void trap_SetAreaPortalState ( struct edict_s *ent, qboolean open ) {
	gi.SetAreaPortalState ( ent, open );
}

qboolean trap_AreasConnected ( int area1, int area2 ) {
	return gi.AreasConnected ( area1, area2 );
}

void trap_LinkEntity ( struct edict_s *ent ) {
	gi.LinkEntity ( ent );
}

void trap_UnlinkEntity ( struct edict_s *ent ) {
	gi.UnlinkEntity ( ent );
}

int trap_BoxEdicts ( vec3_t mins, vec3_t maxs, struct edict_s **list, int maxcount, int areatype ) {
	return gi.BoxEdicts ( mins, maxs, list, maxcount, areatype );
}

struct mempool_s *trap_MemAllocPool ( const char *name, const char *filename, int fileline ) {
	return gi.Mem_AllocPool ( name, filename, fileline );
}

void *trap_MemAlloc ( struct mempool_s *pool, int size, const char *filename, int fileline ) {
	return gi.Mem_Alloc ( pool, size, filename, fileline );
}

void trap_MemFree ( void *data, const char *filename, int fileline ) {
	gi.Mem_Free ( data, filename, fileline );
}


void trap_MemFreePool ( struct mempool_s **pool, const char *filename, int fileline ) {
	gi.Mem_FreePool ( pool, filename, fileline );
}

void trap_MemEmptyPool ( struct mempool_s *pool, const char *filename, int fileline ) {
	gi.Mem_EmptyPool ( pool, filename, fileline );
}

cvar_t *trap_Cvar_Get ( char *name, char *value, int flags ) {
	return gi.Cvar_Get ( name, value, flags );
}

cvar_t *trap_Cvar_Set ( char *name, char *value ) {
	return gi.Cvar_Set ( name, value );
}

cvar_t *trap_Cvar_ForceSet ( char *name, char *value ) {
	return gi.Cvar_ForceSet ( name, value );
}

int	trap_Cmd_Argc (void) {
	return gi.Cmd_Argc ();
}

char *trap_Cmd_Argv ( int arg ) {
	return gi.Cmd_Argv ( arg );
}

char *trap_Cmd_Args (void) {
	return gi.Cmd_Args ();
}

void trap_AddCommandString ( char *text ) {
	gi.AddCommandString ( text );
}

void trap_LocateEntities ( struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts ) {
	gi.LocateEntities ( edicts, edict_size, num_edicts, max_edicts );
}

//======================================================================

/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
=================
*/
game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;

	globals.API = G_API;

	globals.Init = G_Init;
	globals.Shutdown = G_Shutdown;

	globals.SpawnEntities = SpawnEntities;
	globals.WriteGame = WriteGame;
	globals.ReadGame = ReadGame;
	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;

	globals.ClientThink = ClientThink;
	globals.ClientConnect = ClientConnect;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientCommand = ClientCommand;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = ServerCommand;

	return &globals;
}

#if defined(HAS_DLLMAIN) && !defined(GAME_HARD_LINKED)
int _stdcall DLLMain (void *hinstDll, unsigned long dwReason, void *reserved)
{
	return 1;
}
#endif
