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
// sv_user.c -- server code for moving users

#include "server.h"

edict_t	*sv_player;

/*
============================================================

USER STRINGCMD EXECUTION

sv_client and sv_player will be valid.
============================================================
*/

/*
==================
SV_BeginDemoServer
==================
*/
void SV_BeginDemoserver (void)
{
	char		name[MAX_OSPATH];

	Q_snprintfz (name, sizeof(name), "demos/%s", sv.name);
	sv.demolen = FS_FOpenFile (name, &sv.demofile, FS_READ);
	if (!sv.demofile)
		Com_Error (ERR_DROP, "Couldn't open %s", name);
}

/*
================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_New_f (void)
{
	char		*gamedir;
	int			playernum;
	edict_t		*ent;

	Com_DPrintf ("New() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf ("New not valid -- already spawned\n");
		return;
	}

	// demo servers just dump the file message
	if (sv.state == ss_demo)
	{
		SV_BeginDemoserver ();
		return;
	}

	//
	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir
	//
	gamedir = Cvar_VariableString ("fs_gamedir");

	// send the serverdata
	MSG_WriteByte (&sv_client->netchan.message, svc_serverdata);
	MSG_WriteLong (&sv_client->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong (&sv_client->netchan.message, svs.spawncount);
	MSG_WriteByte (&sv_client->netchan.message, sv.attractloop);
	MSG_WriteByte (&sv_client->netchan.message, sv.frametime);
	MSG_WriteString (&sv_client->netchan.message, gamedir);

	if (sv.state == ss_cinematic)
		playernum = -1;
	else
		playernum = sv_client - svs.clients;
	MSG_WriteShort (&sv_client->netchan.message, playernum);

	// send full levelname
	MSG_WriteString (&sv_client->netchan.message, sv.name);

	//
	// game server
	// 
	if (sv.state == ss_game)
	{
		// set up the entity for the client
		ent = EDICT_NUM(playernum+1);
		ent->s.number = playernum+1;
		sv_client->edict = ent;
		memset (&sv_client->lastcmd, 0, sizeof(sv_client->lastcmd));

		// begin fetching configstrings
		MSG_WriteByte (&sv_client->netchan.message, svc_stringcmd);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i 0",svs.spawncount) );
	}

}

/*
==================
SV_Configstrings_f
==================
*/
void SV_Configstrings_f (void)
{
	int			start;

	Com_DPrintf ("Configstrings() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf ("configstrings not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Com_Printf ("SV_Configstrings_f from different level\n");
		SV_New_f ();
		return;
	}

	start = atoi(Cmd_Argv(2));
	if( start < 0 )
		start = 0;

	// write a packet full of data

	while ( sv_client->netchan.message.cursize < MAX_MSGLEN/2 
		&& start < MAX_CONFIGSTRINGS)
	{
		if (sv.configstrings[start][0])
		{
			MSG_WriteByte (&sv_client->netchan.message, svc_servercmd);
			MSG_WriteString (&sv_client->netchan.message, va("cs %i \"%s\"", start, sv.configstrings[start]));
		}
		start++;
	}

	// send next command

	if (start == MAX_CONFIGSTRINGS)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stringcmd);
		MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i 0",svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stringcmd);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i %i",svs.spawncount, start) );
	}
}

/*
==================
SV_Baselines_f
==================
*/
void SV_Baselines_f (void)
{
	int		start;
	entity_state_t	nullstate;
	entity_state_t	*base;

	Com_DPrintf ("Baselines() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf ("baselines not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Com_Printf ("SV_Baselines_f from different level\n");
		SV_New_f ();
		return;
	}

	start = atoi(Cmd_Argv(2));
	if( start < 0 )
		start = 0;

	memset (&nullstate, 0, sizeof(nullstate));

	// write a packet full of data

	while ( sv_client->netchan.message.cursize < MAX_MSGLEN/2
		&& start < MAX_EDICTS )
	{
		base = &sv.baselines[start];
		if (base->modelindex || base->sound || base->effects)
		{
			MSG_WriteByte (&sv_client->netchan.message, svc_spawnbaseline);
			MSG_WriteDeltaEntity (&nullstate, base, &sv_client->netchan.message, qtrue, qtrue);
		}
		start++;
	}

	// send next command

	if (start == MAX_EDICTS)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stringcmd);
		MSG_WriteString (&sv_client->netchan.message, va("precache %i", svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stringcmd);
		MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i %i",svs.spawncount, start) );
	}
}

/*
==================
SV_Begin_f
==================
*/
void SV_Begin_f (void)
{
	Com_DPrintf ("Begin() from %s\n", sv_client->name);

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Com_Printf ("SV_Begin_f from different level\n");
		SV_New_f ();
		return;
	}

	sv_client->state = cs_spawned;

	// call the game begin function
	ge->ClientBegin (sv_player);

	Cbuf_InsertFromDefer ();
}

//=============================================================================

/*
==================
SV_NextDownload_f
==================
*/
void SV_NextDownload_f (void)
{
	int		r;
	int		percent;
	int		size;

	if (!sv_client->download)
		return;

	r = sv_client->downloadsize - sv_client->downloadcount;
	if (r > 1024)
		r = 1024;

	MSG_WriteByte (&sv_client->netchan.message, svc_download);
	MSG_WriteShort (&sv_client->netchan.message, r);

	sv_client->downloadcount += r;
	size = sv_client->downloadsize;
	if (!size)
		size = 1;
	percent = sv_client->downloadcount*100/size;
	MSG_WriteByte (&sv_client->netchan.message, percent);
	SZ_Write (&sv_client->netchan.message,
		sv_client->download + sv_client->downloadcount - r, r);

	if (sv_client->downloadcount != sv_client->downloadsize)
		return;

	FS_FreeFile (sv_client->download);
	sv_client->download = NULL;
}

/*
==================
SV_BeginDownload_f
==================
*/
void SV_BeginDownload_f(void)
{
	char	*name;
	extern	cvar_t *allow_download;
	extern	cvar_t *allow_download_players;
	extern	cvar_t *allow_download_models;
	extern	cvar_t *allow_download_sounds;
	extern	cvar_t *allow_download_maps;
	extern	int		file_from_pak; // ZOID did file come from pak?
	int offset = 0;

	name = Cmd_Argv(1);

	if (Cmd_Argc() > 2)
		offset = atoi(Cmd_Argv(2)); // downloaded offset

	// hacked by zoid to allow more conrol over download
	// first off, no .. or global allow check
	if (strstr (name, "..") || !allow_download->integer
		// leading dot is no good
		|| *name == '.' 
		// leading slash bad as well, must be in subdir
		|| *name == '/'
		// next up, skin check
		|| (strncmp(name, "players/", 8) == 0 && !allow_download_players->integer)
		// now models
		|| (strncmp(name, "models/", 7) == 0 && !allow_download_models->integer)
		// now sounds
		|| (strncmp(name, "sound/", 6) == 0 && !allow_download_sounds->integer)
		// now maps (note special case for maps, must not be in pak)
		|| (strncmp(name, "maps/", 5) == 0 && !allow_download_maps->integer)
		// MUST be in a subdirectory	
		|| !strstr (name, "/") )	
	{	// don't allow anything with .. path
		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}


	if (sv_client->download)
		FS_FreeFile (sv_client->download);

	sv_client->downloadsize = FS_LoadFile (name, (void **)&sv_client->download, NULL, 0);
	sv_client->downloadcount = offset;

	if (offset > sv_client->downloadsize)
		sv_client->downloadcount = sv_client->downloadsize;

	if (!sv_client->download
		// special check for maps, if it came from a pak file, don't allow
		// download  ZOID
		|| (strncmp(name, "maps/", 5) == 0 && file_from_pak))
	{
		Com_DPrintf ("Couldn't download %s to %s\n", name, sv_client->name);
		if (sv_client->download) {
			FS_FreeFile (sv_client->download);
			sv_client->download = NULL;
		}

		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}

	SV_NextDownload_f ();
	Com_DPrintf ("Downloading %s to %s\n", name, sv_client->name);
}



//============================================================================


/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately
=================
*/
void SV_Disconnect_f (void)
{
//	SV_EndRedirect ();
	SV_DropClient (sv_client);	
}


/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
void SV_ShowServerinfo_f (void)
{
	Info_Print (Cvar_Serverinfo());
}


void SV_Nextserver (void)
{
	char	*v;

	//ZOID, ss_pic can be nextserver'd in coop mode
	if (sv.state == ss_game)
		return;		// can't nextserver while playing a normal game

	svs.spawncount++;	// make sure another doesn't sneak in
	v = Cvar_VariableString ("nextserver");
	if (!v[0])
		Cbuf_AddText ("killserver\n");
	else
	{
		Cbuf_AddText (v);
		Cbuf_AddText ("\n");
	}
	Cvar_Set ("nextserver","");
}

/*
==================
SV_Nextserver_f

A cinematic has completed or been aborted by a client, so move
to the next server,
==================
*/
void SV_Nextserver_f (void)
{
	if ( atoi(Cmd_Argv(1)) != svs.spawncount ) {
		Com_DPrintf ("Nextserver() from wrong level, from %s\n", sv_client->name);
		return;		// leftover from last server
	}

	Com_DPrintf ("Nextserver() from %s\n", sv_client->name);

	SV_Nextserver ();
}

typedef struct
{
	char	*name;
	void	(*func) (void);
} ucmd_t;

ucmd_t ucmds[] =
{
	// auto issued
	{"new", SV_New_f},
	{"configstrings", SV_Configstrings_f},
	{"baselines", SV_Baselines_f},
	{"begin", SV_Begin_f},

	{"nextserver", SV_Nextserver_f},

	{"disconnect", SV_Disconnect_f},

	// issued by hand at client consoles	
	{"info", SV_ShowServerinfo_f},

	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},

	{NULL, NULL}
};

/*
==================
SV_ExecuteUserCommand
==================
*/
static void SV_ExecuteUserCommand (client_t *client, char *s)
{
	ucmd_t	*u;

	// wsw : r1q2[start] : catch end-of-message exploit
	if( strchr (s, '\xFF') )
	{
		const char *remote = NET_AdrToString( &client->netchan.remote_address );

		if( dedicated->integer )
			Com_Printf( "%s [%s] Dropped. Found end-of-message inside command string\n", client->name, remote );
		SV_BroadcastPrintf( PRINT_HIGH,"%s [%s] Dropped. Found end-of-message inside command string\n", client->name, remote );
		SV_DropClient( client );
		return;
	}
	// wsw : r1q2[end]

	Cmd_TokenizeString ( s, qfalse );

	sv_player = sv_client->edict;

	for (u=ucmds ; u->name ; u++)
		if (!strcmp (Cmd_Argv(0), u->name) )
		{
			u->func ();
			break;
		}

	if (!u->name && sv.state == ss_game)
		ge->ClientCommand (sv_player);
}

/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/



void SV_ClientThink (client_t *cl, usercmd_t *cmd)
{
	cl->commandMsec -= cmd->msec;

	if (cl->commandMsec < 0 && sv_enforcetime->integer)
	{
		Com_DPrintf ("commandMsec underflow from %s\n", cl->name);
		return;
	}

	ge->ClientThink (cl->edict, cmd);
}



#define	MAX_STRINGCMDS	8
/*
===================
SV_ExecuteClientMessage

The current net_message is parsed for the given client
===================
*/
void SV_ExecuteClientMessage (client_t *cl)
{
	int		c;
	char	*s;
	usercmd_t	nullcmd;
	usercmd_t	oldest, oldcmd, newcmd;
	int		net_drop;
	int		stringCmdCount;
	int		checksum, calculatedChecksum;
	int		checksumIndex;
	qboolean	move_issued;
	int		lastframe;

	sv_client = cl;
	sv_player = sv_client->edict;

	// only allow one move command
	move_issued = qfalse;
	stringCmdCount = 0;

	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Printf ("SV_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}	

		c = MSG_ReadByte (&net_message);
		if (c == -1)
			break;
				
		switch (c)
		{
		default:
			Com_Printf ("SV_ReadClientMessage: unknown command char\n");
			SV_DropClient (cl);
			return;
						
		case clc_nop:
			break;

		case clc_userinfo:
			Q_strncpyz (cl->userinfo, MSG_ReadString (&net_message), sizeof(cl->userinfo));
			SV_UserinfoChanged (cl);
			break;

		case clc_move:
			if (move_issued)
				return;		// someone is trying to cheat...

			move_issued = qtrue;
			checksumIndex = net_message.readcount;
			checksum = MSG_ReadByte (&net_message);
			lastframe = MSG_ReadLong (&net_message);
			if (lastframe != cl->lastframe) {
				cl->lastframe = lastframe;
				if (cl->lastframe > 0) {
					cl->frame_latency[cl->lastframe&(LATENCY_COUNTS-1)] = 
						svs.realtime - cl->frames[cl->lastframe & UPDATE_MASK].senttime;
				}
			}

			memset (&nullcmd, 0, sizeof(nullcmd));
			MSG_ReadDeltaUsercmd (&net_message, &nullcmd, &oldest);
			MSG_ReadDeltaUsercmd (&net_message, &oldest, &oldcmd);
			MSG_ReadDeltaUsercmd (&net_message, &oldcmd, &newcmd);

			if ( cl->state != cs_spawned )
			{
				cl->lastframe = -1;
				break;
			}

			// if the checksum fails, ignore the rest of the packet
			calculatedChecksum = COM_BlockSequenceCRCByte (
				net_message.data + checksumIndex + 1,
				net_message.readcount - checksumIndex - 1,
				cl->netchan.incoming_sequence);

			if (calculatedChecksum != checksum)
			{
				Com_DPrintf ("Failed command checksum for %s (%d != %d)/%d\n", 
					cl->name, calculatedChecksum, checksum, 
					cl->netchan.incoming_sequence);
				return;
			}

			if (!sv_paused->integer)
			{
				net_drop = cl->netchan.dropped;
				if (net_drop < 20)
				{
					while (net_drop > 2)
					{
						SV_ClientThink (cl, &cl->lastcmd);

						net_drop--;
					}
					if (net_drop > 1)
						SV_ClientThink (cl, &oldest);

					if (net_drop > 0)
						SV_ClientThink (cl, &oldcmd);

				}
				SV_ClientThink (cl, &newcmd);
			}

			cl->lastcmd = newcmd;
			break;

		case clc_stringcmd:	
			s = MSG_ReadString (&net_message);

			// malicious users may try using too many string commands
			if (++stringCmdCount < MAX_STRINGCMDS)
				SV_ExecuteUserCommand (cl, s);

			if (cl->state == cs_zombie)
				return;	// disconnect command
			break;
		}
	}
}

