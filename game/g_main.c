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

#include "g_local.h"

game_locals_t	game;
level_locals_t	level;
spawn_temp_t	st;

struct mempool_s *gamepool;
struct mempool_s *levelpool;

int	sm_meat_index;
int	snd_fry;
int meansOfDeath;

cvar_t	*deathmatch;
cvar_t	*coop;
cvar_t	*dmflags;
cvar_t	*skill;
cvar_t	*fraglimit;
cvar_t	*timelimit;
//ZOID
cvar_t	*capturelimit;
cvar_t	*instantweap;
//ZOID
cvar_t	*password;
cvar_t	*g_select_empty;
cvar_t	*dedicated;
cvar_t	*developer;

cvar_t	*filterban;

cvar_t	*sv_maxclients;
cvar_t	*sv_maxentities;

cvar_t	*sv_maxvelocity;
cvar_t	*sv_gravity;
cvar_t	*sv_airaccelerate;

cvar_t	*sv_rollspeed;
cvar_t	*sv_rollangle;

cvar_t	*sv_cheats;

cvar_t	*sv_maplist;

cvar_t	*run_pitch;
cvar_t	*run_roll;
cvar_t	*bob_up;
cvar_t	*bob_pitch;
cvar_t	*bob_roll;

cvar_t	*flood_msgs;
cvar_t	*flood_persecond;
cvar_t	*flood_waitdelay;

//===================================================================

/*
=================
G_API
=================
*/
int G_API (void) {
	return GAME_API_VERSION;
}

/*
============
G_Error

Abort the server with a game error
============
*/
void G_Error ( char *fmt, ... )
{
	char		msg[1024];
	va_list		argptr;

	va_start( argptr, fmt );
	vsnprintf( msg, sizeof(msg), fmt, argptr );
	va_end( argptr );
	msg[sizeof(msg)-1] = 0;

	trap_Error ( msg );
}

/*
============
G_Printf

Debug print to server console
============
*/
void G_Printf ( char *fmt, ... )
{
	char		msg[1024];
	va_list		argptr;

	va_start( argptr, fmt );
	vsnprintf( msg, sizeof(msg), fmt, argptr );
	va_end( argptr );
	msg[sizeof(msg)-1] = 0;

	trap_Print ( msg );
}

/*
============
G_Init

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game is loaded.
============
*/
void G_Init (unsigned int seed, unsigned int frametime)
{
	G_Printf ("==== G_Init ====\n");

	srand (seed);

	gamepool = G_MemAllocPool ( "Game" );
	levelpool = G_MemAllocPool ( "Level" );

	//FIXME: sv_ prefix is wrong for these
	sv_rollspeed = trap_Cvar_Get ("sv_rollspeed", "200", 0);
	sv_rollangle = trap_Cvar_Get ("sv_rollangle", "2", 0);
	sv_maxvelocity = trap_Cvar_Get ("sv_maxvelocity", "2000", 0);
	sv_gravity = trap_Cvar_Get ("sv_gravity", "800", 0);
	sv_airaccelerate = trap_Cvar_Get ("sv_airaccelerate", "0", CVAR_SERVERINFO|CVAR_LATCH);

	developer = trap_Cvar_Get ( "developer", "0", 0 );

	// noset vars
	dedicated = trap_Cvar_Get ("dedicated", "0", CVAR_NOSET);

	// latched vars
	sv_cheats = trap_Cvar_Get ("sv_cheats", "0", CVAR_SERVERINFO|CVAR_LATCH);
	trap_Cvar_Get ("gamename", GAMENAME , CVAR_SERVERINFO | CVAR_LATCH);
	trap_Cvar_Get ("gamedate", __DATE__ , CVAR_SERVERINFO | CVAR_LATCH);

	sv_maxclients = trap_Cvar_Get ("sv_maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	sv_maxentities = trap_Cvar_Get ("sv_maxentities", "1024", CVAR_LATCH);

	deathmatch = trap_Cvar_Get ("deathmatch", "1", CVAR_LATCH);
	coop = trap_Cvar_Get ("coop", "0", CVAR_LATCH);
	skill = trap_Cvar_Get ("skill", "1", CVAR_LATCH);

	if (!deathmatch->integer) {
		// force ctf off
		trap_Cvar_Set ("ctf", "0");
	}

	// change anytime vars
	dmflags = trap_Cvar_Get ("dmflags", va("%i", DF_INSTANT_ITEMS), CVAR_SERVERINFO);
	fraglimit = trap_Cvar_Get ("fraglimit", "0", CVAR_SERVERINFO);
	timelimit = trap_Cvar_Get ("timelimit", "0", CVAR_SERVERINFO);
//ZOID
	capturelimit = trap_Cvar_Get ("capturelimit", "0", CVAR_SERVERINFO);
	instantweap = trap_Cvar_Get ("instantweap", "0", CVAR_SERVERINFO);
//ZOID
 	password = trap_Cvar_Get ("password", "", CVAR_USERINFO);
	filterban = trap_Cvar_Get ("filterban", "1", 0);

	g_select_empty = trap_Cvar_Get ("g_select_empty", "0", CVAR_ARCHIVE);

#if 0
	run_pitch = trap_Cvar_Get ("run_pitch", "0.002", 0);
	run_roll = trap_Cvar_Get ("run_roll", "0.005", 0);
	bob_up  = trap_Cvar_Get ("bob_up", "0.005", 0);
	bob_pitch = trap_Cvar_Get ("bob_pitch", "0.002", 0);
	bob_roll = trap_Cvar_Get ("bob_roll", "0.002", 0);
#else
	run_pitch = trap_Cvar_Get ("run_pitch", "0", 0);
	run_roll = trap_Cvar_Get ("run_roll", "0", 0);
	bob_up  = trap_Cvar_Get ("bob_up", "0", 0);
	bob_pitch = trap_Cvar_Get ("bob_pitch", "0", 0);
	bob_roll = trap_Cvar_Get ("bob_roll", "0", 0);
#endif

	// flood control
	flood_msgs = trap_Cvar_Get ("flood_msgs", "4", 0);
	flood_persecond = trap_Cvar_Get ("flood_persecond", "4", 0);
	flood_waitdelay = trap_Cvar_Get ("flood_waitdelay", "10", 0);

	// dm map list
	sv_maplist = trap_Cvar_Get ("sv_maplist", "", 0);

	// items
	InitItems ();

	Q_snprintfz (game.helpmessage1, sizeof(game.helpmessage1), "");

	Q_snprintfz (game.helpmessage2, sizeof(game.helpmessage2), "");

	// initialize all entities for this game
	game.maxentities = sv_maxentities->integer;
	game.edicts = G_GameMalloc (game.maxentities * sizeof(game.edicts[0]));

	// initialize all clients for this game
	game.maxclients = sv_maxclients->integer;
	game.clients = G_GameMalloc (game.maxclients * sizeof(game.clients[0]));

	game.numentities = game.maxclients+1;

	game.frametimeMsec = frametime;
	game.frametime = frametime * 0.001;

	trap_LocateEntities (game.edicts, sizeof(game.edicts[0]), game.numentities, game.maxentities);

//ZOID
	CTFInit();
//ZOID
}

/*
=================
G_Shutdown
=================
*/
void G_Shutdown (void)
{
	G_Printf ("==== G_Shutdown ====\n");

	G_MemFreePool (&gamepool);
	G_MemFreePool (&levelpool);
}

//======================================================================

/*
================
G_SetEntityBits

Set misc bits like ET_INVERSE, EF_CORPSE, etc
================
*/
void G_SetEntityBits (edict_t *ent)
{
	// inverse entity type if it can take damage
	if (ent->takedamage)
		ent->s.type |= ET_INVERSE;

	// set EF_CORPSE for client side prediction
	if (ent->r.svflags & SVF_CORPSE)
		ent->s.effects |= EF_CORPSE;
}

/*
=================
ClientEndServerFrames
=================
*/
void ClientEndServerFrames (void)
{
	int		i;
	edict_t	*ent;

	// calc the player views now that all pushing
	// and damage has been added
	for (i=0 ; i<game.maxclients ; i++)
	{
		ent = game.edicts + 1 + i;
		if (!ent->r.inuse || !ent->r.client)
			continue;

		ClientEndServerFrame (ent);

		G_SetEntityBits (ent);
	}

	G_EndServerFrames_UpdateChaseCam ();
}

/*
=================
CreateTargetChangeLevel

Returns the created target changelevel
=================
*/
edict_t *CreateTargetChangeLevel(char *map)
{
	edict_t *ent;

	ent = G_Spawn ();
	ent->classname = "target_changelevel";
	Q_strncpyz (level.nextmap, map, sizeof(level.nextmap));
	ent->map = level.nextmap;
	return ent;
}

/*
=================
G_EndDMLevel

The timelimit or fraglimit has been exceeded
=================
*/
void G_EndDMLevel (void)
{
	edict_t		*ent;
	char *s, *t, *f;
	static const char *seps = " ,\n\r";

	// stay on same level flag
	if (dmflags->integer & DF_SAME_LEVEL)
	{
		BeginIntermission (CreateTargetChangeLevel (level.mapname) );
		return;
	}

	if (*level.forcemap) {
		BeginIntermission (CreateTargetChangeLevel (level.forcemap) );
		return;
	}

	// see if it's in the map list
	if (*sv_maplist->string) {
		s = G_CopyString (sv_maplist->string);
		f = NULL;
		t = strtok(s, seps);
		while (t != NULL) {
			if (Q_stricmp(t, level.mapname) == 0) {
				// it's in the list, go to the next one
				t = strtok(NULL, seps);
				if (t == NULL) { // end of list, go to first one
					if (f == NULL) // there isn't a first one, same level
						BeginIntermission (CreateTargetChangeLevel (level.mapname) );
					else
						BeginIntermission (CreateTargetChangeLevel (f) );
				} else
					BeginIntermission (CreateTargetChangeLevel (t) );
				G_Free (s);
				return;
			}
			if (!f)
				f = t;
			t = strtok(NULL, seps);
		}
		G_Free (s);
	}

	if (level.nextmap[0]) // go to a specific map
		BeginIntermission (CreateTargetChangeLevel (level.nextmap) );
	else {	// search for a changelevel
		ent = G_Find (NULL, FOFS(classname), "target_changelevel");
		if (!ent)
		{	// the map designer didn't include a changelevel,
			// so create a fake ent that goes back to the same level
			BeginIntermission (CreateTargetChangeLevel (level.mapname) );
			return;
		}
		BeginIntermission (ent);
	}
}

/*
=================
G_CheckDMRules
=================
*/
void G_CheckDMRules (void)
{
	int			i;
	gclient_t	*cl;

	if (level.intermissiontime)
		return;

	if (!deathmatch->integer)
		return;

//ZOID
	if (ctf->integer && CTFCheckRules()) {
		G_EndDMLevel ();
		return;
	}
	if (CTFInMatch())
		return; // no checking in match mode
//ZOID

	if (timelimit->value)
	{
		if (level.time >= timelimit->value*60)
		{
			G_PrintMsg (NULL, PRINT_HIGH, "Timelimit hit.\n");
			G_EndDMLevel ();
			return;
		}
	}

	if (fraglimit->integer)
		for (i=0 ; i<game.maxclients ; i++)
		{
			cl = game.clients + i;
			if (!game.edicts[i+1].r.inuse)
				continue;

			if (cl->resp.score >= fraglimit->integer)
			{
				G_PrintMsg (NULL, PRINT_HIGH, "Fraglimit hit.\n");
				G_EndDMLevel ();
				return;
			}
		}
}


/*
=============
G_ExitLevel
=============
*/
void G_ExitLevel (void)
{
	int		i;
	edict_t	*ent;
	char	command [256];

	level.exitintermission = 0;
	level.intermissiontime = 0;

	if (CTFNextMap())
		return;

	Q_snprintfz (command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
	trap_AddCommandString (command);
	ClientEndServerFrames ();

	level.changemap = NULL;

	// clear some things before going to next level
	for (i=0 ; i<game.maxclients ; i++)
	{
		ent = game.edicts + 1 + i;
		if (!ent->r.inuse)
			continue;
		if (ent->health > ent->r.client->pers.max_health)
			ent->health = ent->r.client->pers.max_health;
	}
}

/*
================
G_RunFrame

Advances the world by frametime
================
*/
void G_RunFrame (void)
{
	int		i;
	edict_t	*ent;

	level.framenum++;
	level.time = level.framenum * game.frametime;

	//
	// clear all events to free entity spots
	//
	ent = &game.edicts[0];
	for (i=0 ; i<game.numentities ; i++, ent++)
	{
		if( !ent->r.inuse )
			continue;

		// free if no events
		if( !ent->s.events[0] ) {
			ent->numEvents = 0;
			ent->eventPriority[0] = ent->eventPriority[1] = qfalse;
			if( ent->freeAfterEvent )
				G_FreeEdict( ent );
		}
	}

	// choose a client for monsters to target this frame
	AI_SetSightClient ();

	// exit intermissions

	if (level.exitintermission)
	{
		G_ExitLevel ();
		return;
	}

	//
	// treat each object in turn
	// even the world gets a chance to think
	//
	ent = &game.edicts[0];
	for (i=0 ; i<game.numentities ; i++, ent++)
	{
		if (!ent->r.inuse)
			continue;
		if (ent->freeAfterEvent)
			continue;		// events do not think

		ent->s.type &= ~ET_INVERSE;		// remove ET_INVERSE bit
		ent->s.effects &= ~EF_CORPSE;	// remove EF_CORPSE bit

		level.current_entity = ent;

		if (!(ent->r.svflags & SVF_FORCEOLDORIGIN))
			VectorCopy (ent->s.origin, ent->s.old_origin);

		// if the ground entity moved, make sure we are still on it
		if ((ent->groundentity) && (ent->groundentity->r.linkcount != ent->groundentity_linkcount))
		{
			ent->groundentity = NULL;
			if ( !(ent->flags & (FL_SWIM|FL_FLY)) && (ent->r.svflags & SVF_MONSTER) )
				M_CheckGround (ent);
		}

		if (i > 0 && i <= game.maxclients)
		{
			ClientBeginServerFrame (ent);
			continue;
		}

		G_RunEntity (ent);
		G_SetEntityBits (ent);
	}

	// see if it is time to end a deathmatch
	G_CheckDMRules ();

	// build the playerstate_t structures for all players
	ClientEndServerFrames ();
}

//======================================================================

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);
	text[sizeof(text)-1] = 0;

	G_Error ("%s", text);
}

void Com_Printf (char *msg, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsnprintf (text, sizeof(text), msg, argptr);
	va_end (argptr);
	text[sizeof(text)-1] = 0;

	G_Printf ("%s", text);
}

#endif

