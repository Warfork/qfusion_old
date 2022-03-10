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

// g_public.h -- game dll information visible to server

#define	GAME_API_VERSION	1

// edict->svflags
#define	SVF_NOCLIENT		0x00000001	// don't send entity to clients, even if it has effects
#define SVF_PORTAL			0x00000002	// merge PVS at old_origin
#define	SVF_NOOLDORIGIN		0x00000004	// don't send old_origin (non-continuous events)
#define	SVF_FORCEOLDORIGIN	0x00000008	// always send old_origin (beams, etc), just check one point for PHS if not SVF_PORTAL (must be non-solid)
#define	SVF_MONSTER			0x00000010	// treat as CONTENTS_MONSTER for collision
#define SVF_FAKECLIENT		0x00000020	// do not try to send anything to this client

// edict->solid values
typedef enum
{
	SOLID_NOT,			// no interaction with other objects
	SOLID_TRIGGER,		// only touch when inside, after moving
	SOLID_BBOX,			// touch on edge
	SOLID_BSP			// bsp clip, touch on edge
} solid_t;

//===============================================================

// link_t is only used for entity area links now
typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;

#define	MAX_ENT_CLUSTERS	16

typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;

typedef struct
{
	int		ping;
	int		health;
	int		frags;
} client_shared_t;

typedef struct
{
	gclient_t	*client;
	qboolean	inuse;
	int			linkcount;

	// FIXME: move these fields to a server private sv_entity_t
	link_t		area;				// linked to a division node or leaf
	
	int			num_clusters;		// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			headnode;			// unused if num_clusters != -1
	int			areanum, areanum2;

	//================================

	int			svflags;			// SVF_NOCLIENT, SVF_MONSTER, etc
	edict_t		*visclent;			// send only to this client
	vec3_t		mins, maxs;
	vec3_t		absmin, absmax, size;
	solid_t		solid;
	int			clipmask;
	edict_t		*owner;
} entity_shared_t;

//===============================================================

//
// functions provided by the main engine
//
typedef struct
{
	// special messages
	void		(*Print) (char *msg);

	// aborts server with a game error
	void		(*Error) (char *msg);

	// hardly encoded sound message
	void		(*Sound) (vec3_t origin, edict_t *ent, int channel, int soundindex, float volume, float attenuation);

	// server commands sent to clients
	void		(*ServerCmd) (edict_t *ent, char *cmd);

	// config strings hold all the index strings,
	// and misc data like audio track and gridsize.
	// All of the current configstrings are sent to clients when
	// they connect, and changes are sent to all connected clients.
	void		(*ConfigString) (int num, char *string);

	// general 2D layout
	void		(*Layout) (edict_t *ent, char *string);

	// stuffed into client's console buffer, should be \n terminated
	void		(*StuffCmd) (edict_t *ent, char *string);

	// the *index functions create configstrings and some internal server state
	int			(*ModelIndex) (char *name);
	int			(*SoundIndex) (char *name);
	int			(*ImageIndex) (char *name);

	void		(*SetBrushModel) (edict_t *ent, char *name);

	// collision detection
	void		(*Trace) (trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask);
	int			(*PointContents) (vec3_t point);
	qboolean	(*inPVS) (vec3_t p1, vec3_t p2);
	qboolean	(*inPHS) (vec3_t p1, vec3_t p2);
	void		(*SetAreaPortalState) (edict_t *ent, qboolean open);
	qboolean	(*AreasConnected) (int area1, int area2);

	// an entity will never be sent to a client or used for collision
	// if it is not passed to linkentity.  If the size, position, or
	// solidity changes, it must be relinked.
	void		(*LinkEntity) (edict_t *ent);
	void		(*UnlinkEntity) (edict_t *ent);		// call before removing an interactive edict
	int			(*BoxEdicts) (vec3_t mins, vec3_t maxs, edict_t **list,	int maxcount, int areatype);

	// managed memory allocation
	struct mempool_s *(*Mem_AllocPool) ( const char *name, const char *filename, int fileline );	
	void		*(*Mem_Alloc) ( struct mempool_s *pool, int size, const char *filename, int fileline );
	void		(*Mem_Free) ( void *data, const char *filename, int fileline );
	void		(*Mem_FreePool) ( struct mempool_s **pool, const char *filename, int fileline );
	void		(*Mem_EmptyPool) ( struct mempool_s *pool, const char *filename, int fileline );

	// console variable interaction
	cvar_t		*(*Cvar_Get) ( char *name, char *value, int flags );
	cvar_t		*(*Cvar_Set) ( char *name, char *value );
	cvar_t		*(*Cvar_ForceSet) ( char *name, char *value );	// will return 0 0 if not found

	// ClientCommand and ServerCommand parameter access
	int			(*Cmd_Argc) (void);
	char		*(*Cmd_Argv) ( int arg );
	char		*(*Cmd_Args) (void);		// concatenation of all argv >= 1

	// add commands to the server console as if they were typed in
	// for map changing, etc
	void		(*AddCommandString) (char *text);

	// The edict array is allocated in the game dll so it
	// can vary in size from one game to another.
	void		(*LocateEntities) (struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts);
} game_import_t;

//
// functions exported by the game subsystem
//
typedef struct
{
	// if API is different, the dll cannot be used
	int			(*API) (void);

	// the init function will only be called when a game starts,
	// not each time a level is loaded.  Persistant data for clients
	// and the server can be allocated in init
	void		(*Init) (unsigned int seed);
	void		(*Shutdown) (void);

	// each new level entered will cause a call to SpawnEntities
	void		(*SpawnEntities) (char *mapname, char *entstring, char *spawnpoint);

	// Read/Write Game is for storing persistant cross level information
	// about the world state and the clients.
	// WriteGame is called every time a level is exited.
	// ReadGame is called on a loadgame.
	void		(*WriteGame) (char *filename, qboolean autosave);
	void		(*ReadGame) (char *filename);

	// ReadLevel is called after the default map information has been
	// loaded with SpawnEntities
	void		(*WriteLevel) (char *filename);
	void		(*ReadLevel) (char *filename);

	qboolean	(*ClientConnect) (edict_t *ent, char *userinfo);
	void		(*ClientBegin) (edict_t *ent);
	void		(*ClientUserinfoChanged) (edict_t *ent, char *userinfo);
	void		(*ClientDisconnect) (edict_t *ent);
	void		(*ClientCommand) (edict_t *ent);
	void		(*ClientThink) (edict_t *ent, usercmd_t *cmd);

	void		(*RunFrame) (void);

	// ServerCommand will be called when an "sv <command>" command is issued on the
	// server console.
	// The game can issue trap_Cmd_Args() / trap_Cmd_Argv() commands to get the rest
	// of the parameters
	void		(*ServerCommand) (void);
} game_export_t;
