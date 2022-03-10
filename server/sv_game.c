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
// sv_game.c -- interface to the game dll

#include "server.h"

game_export_t	*ge;

mempool_t		*sv_gameprogspool;

/*
===============
PF_Layout

Sends the layout to a single client
===============
*/
static void PF_Layout (edict_t *ent, const char *string)
{
	int			p;
	client_t	*client;

	if (!ent || !string || !string[0])
		return;
	if (ent->r.svflags & SVF_FAKECLIENT)
		return;

	p = NUM_FOR_EDICT(ent);
	if (p < 1 || p > sv_maxclients->integer)
		return;

	client = svs.clients + (p-1);
	MSG_WriteByte (&client->datagram, svc_layout);
	MSG_WriteString (&client->datagram, string);
}

/*
===============
PF_DropClient
===============
*/
void PF_DropClient( edict_t *ent )
{
	int			p;
	client_t	*drop;

	if (!ent)
		return;

	p = NUM_FOR_EDICT(ent);
	if( p < 1 || p > sv_maxclients->integer )
		return;

	drop = svs.clients + (p-1);
	SV_DropClient( drop );
}

/*
===============
PF_ServerCmd

Sends the server command to clients. 
if ent is NULL the command will be sent to all connected clients
===============
*/
static void PF_ServerCmd (edict_t *ent, const char *cmd)
{
	if (!cmd || !cmd[0])
		return;

	if (!ent)
	{
		SZ_Clear (&sv.multicast);
		MSG_WriteByte (&sv.multicast, svc_servercmd);
		MSG_WriteString (&sv.multicast, cmd);
		SV_Multicast (NULL, MULTICAST_ALL_R);
	}
	else
	{
		int		p;
		client_t	*client;

		if (ent->r.svflags & SVF_FAKECLIENT)
			return;

		p = NUM_FOR_EDICT(ent);
		if (p < 1 || p > sv_maxclients->integer)
			return;

		client = svs.clients + (p-1);

		MSG_WriteByte (&client->netchan.message, svc_servercmd);
		MSG_WriteString (&client->netchan.message, cmd);
	}
}

/*
===============
PF_dprint

Debug print to server console
===============
*/
static void PF_dprint (const char *msg)
{
	int		i;
	char	copy[MAX_PRINTMSG];

	if (!msg)
		return;

	// mask off high bits and colored strings
	for (i=0 ; i<sizeof(copy)-1 && msg[i] ; i++ )
		copy[i] = msg[i]&127;
	copy[i] = 0;
	
	Com_Printf ("%s", copy);
}

/*
===============
PF_error

Abort the server with a game error
===============
*/
static void PF_error (const char *msg)
{
	int		i;
	char	copy[MAX_PRINTMSG];

	if (!msg)
	{
		Com_Error (ERR_DROP, "Game Error: unknown error");
		return;
	}

	// mask off high bits and colored strings
	for (i=0 ; i<sizeof(copy)-1 && msg[i] ; i++ )
		copy[i] = msg[i]&127;
	copy[i] = 0;

	Com_Error (ERR_DROP, "Game Error: %s", copy);
}

/*
===============
PF_StuffCmd
===============
*/
static void PF_StuffCmd (edict_t *ent, const char *string)
{
	int			p;
	client_t	*client;

	if (!ent || !string || !string[0])
		return;
	if (ent->r.svflags & SVF_FAKECLIENT)
		return;

	p = NUM_FOR_EDICT(ent);
	if (p < 1 || p > sv_maxclients->integer)
		return;

	client = svs.clients + (p-1);
	MSG_WriteByte (&client->netchan.message, svc_stufftext);
	MSG_WriteString (&client->netchan.message, string);
}

/*
=================
PF_SetBrushModel

Also sets mins and maxs for inline bmodels
=================
*/
static void PF_SetBrushModel (edict_t *ent, const char *name)
{
	struct cmodel_s *cmodel;

	if (!name)
		Com_Error (ERR_DROP, "PF_setmodel: NULL");
	if (!name[0])
	{
		 ent->s.modelindex = 0;
		 return;
	}

// if it is an inline model, get the size information for it
	if (name[0] != '*')
	{
		ent->s.modelindex = SV_ModelIndex (name);
		return;
	}

	if (!strcmp (name, "*0"))
	{
		ent->s.modelindex = 1;
		cmodel = CM_InlineModel (0);
		CM_InlineModelBounds (cmodel, ent->r.mins, ent->r.maxs);
		return;
	}

	ent->s.modelindex = atoi (name+1);
	cmodel = CM_InlineModel (ent->s.modelindex);
	CM_InlineModelBounds (cmodel, ent->r.mins, ent->r.maxs);
	SV_LinkEdict (ent);
}

/*
===============
PF_EntityContact
===============
*/
static qboolean PF_EntityContact( vec3_t mins, vec3_t maxs, edict_t *ent )
{
	trace_t tr;
	struct cmodel_s *model;

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	if( !ent->s.modelindex ) {
		if( ent->r.solid == SOLID_TRIGGER )
			return( BoundsIntersect( mins, maxs, ent->r.absmin, ent->r.absmax ) );
		return qfalse;
	}

	if( ent->r.solid == SOLID_TRIGGER || ent->r.solid == SOLID_BSP ) {
		model = CM_InlineModel( ent->s.modelindex );
		if( !model )
			Com_Error( ERR_FATAL, "MOVETYPE_PUSH with a non bsp model" );

		CM_TransformedBoxTrace( &tr, vec3_origin, vec3_origin, mins, maxs, model, MASK_ALL, ent->s.origin, ent->s.angles );

		return tr.startsolid || tr.allsolid;
	}

	return( BoundsIntersect( mins, maxs, ent->r.absmin, ent->r.absmax ) );
}

/*
===============
PF_Configstring
===============
*/
static void PF_Configstring (int index, const char *val)
{
	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring: bad index %i", index);

	if (!val)
		val = "";

	// change the string in sv
	Q_strncpyz (sv.configstrings[index], val, sizeof(sv.configstrings[index]));

	if (sv.state != ss_loading)
	{	// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_WriteByte (&sv.multicast, svc_servercmd);
		MSG_WriteString (&sv.multicast, va("cs %i \"%s\"", index, val));
		SV_Multicast (NULL, MULTICAST_ALL_R);
	}
}


/*
=================
PF_inPVS

Also checks portalareas so that doors block sight
=================
*/
static qboolean PF_inPVS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	qbyte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;
	if (!CM_AreasConnected (area1, area2))
		return qfalse;		// a door blocks sight
	return qtrue;
}


/*
=================
PF_inPHS

Also checks portalareas so that doors block sound
=================
*/
static qboolean PF_inPHS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	qbyte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPHS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;		// more than one bounce away
	if (!CM_AreasConnected (area1, area2))
		return qfalse;		// a door blocks hearing

	return qtrue;
}

/*
===============
PF_MemAlloc
===============
*/
static void *PF_MemAlloc( size_t size, const char *filename, int fileline ) {
	return _Mem_Alloc( sv_gameprogspool, size, MEMPOOL_GAMEPROGS, 0, filename, fileline );
}

/*
===============
PF_MemFree
===============
*/
static void PF_MemFree( void *data, const char *filename, int fileline ) {
	_Mem_Free( data, MEMPOOL_GAMEPROGS, 0, filename, fileline );
}


//==============================================

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_ShutdownGameProgs (void)
{
	if (!ge)
		return;
	ge->Shutdown ();
	Mem_FreePool (&sv_gameprogspool);
	Sys_UnloadGameLibrary (LIB_GAME);
	ge = NULL;
}

/*
=================
SV_LocateEntities
=================
*/
void SV_LocateEntities (struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts)
{
	if (!edicts || edict_size < sizeof(entity_shared_t) ) {
		Com_Error (ERR_DROP, "SV_LocateEntities: bad edicts");
	}

	sv.edicts = edicts;
	sv.edict_size = edict_size;
	sv.num_edicts = num_edicts;
	sv.max_edicts = max_edicts;
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
void SV_InitGameProgs (void)
{
	int	apiversion;
	int frametime;
	game_import_t import;

	// unload anything we have now
	if (ge)
		SV_ShutdownGameProgs ();

	sv_gameprogspool = _Mem_AllocPool( NULL, "Game Progs", MEMPOOL_GAMEPROGS, __FILE__, __LINE__ );

	// load a new game dll
	import.Print = PF_dprint;
	import.Error = PF_error;
	import.ServerCmd = PF_ServerCmd;

	import.LinkEntity = SV_LinkEdict;
	import.UnlinkEntity = SV_UnlinkEdict;
	import.BoxEdicts = SV_AreaEdicts;
	import.Trace = SV_Trace;
	import.PointContents = SV_PointContents;
	import.SetBrushModel = PF_SetBrushModel;
	import.inPVS = PF_inPVS;
	import.inPHS = PF_inPHS;
	import.EntityContact = PF_EntityContact;

	import.Milliseconds = Sys_Milliseconds;

	import.ModelIndex = SV_ModelIndex;
	import.SoundIndex = SV_SoundIndex;
	import.ImageIndex = SV_ImageIndex;

	import.ConfigString = PF_Configstring;
	import.Layout = PF_Layout;
	import.StuffCmd = PF_StuffCmd;
	import.Sound = SV_StartSound;

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

	import.Mem_Alloc = PF_MemAlloc;
	import.Mem_Free = PF_MemFree;

	import.Cvar_Get = Cvar_Get;
	import.Cvar_Set = Cvar_Set;
	import.Cvar_ForceSet = Cvar_ForceSet;

	import.Cmd_Argc = Cmd_Argc;
	import.Cmd_Argv = Cmd_Argv;
	import.Cmd_Args = Cmd_Args;
	import.AddCommandString = Cbuf_AddText;

	import.SetAreaPortalState = SV_SetAreaPortalState;
	import.AreasConnected = CM_AreasConnected;

	import.FakeClientConnect = SVC_FakeConnect;
	import.DropClient = PF_DropClient;

	import.LocateEntities = SV_LocateEntities;

	ge = (game_export_t *)Sys_LoadGameLibrary (LIB_GAME, &import);

	if (!ge)
		Com_Error (ERR_DROP, "failed to load game DLL");

	apiversion = ge->API ();
	if (apiversion != GAME_API_VERSION)
		Com_Error (ERR_DROP, "game is version %i, not %i", apiversion, GAME_API_VERSION);

	// sv will get zeroed out later so calculate frametime here too
	if( sv_fps->value < 5 )
		frametime = 200;
	else if( sv_fps->value > 100 )
		frametime = 10;
	else
		frametime = (int)( 1000 / sv_fps->value );

	ge->Init ( time(NULL), frametime );
}

