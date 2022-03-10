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
// sv_main.c -- server main program

#include "server.h"

/*
=============================================================================

Com_Printf redirection

=============================================================================
*/

char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void SV_FlushRedirect (int sv_redirected, char *outputbuf)
{
	if (sv_client->edict && (sv_client->edict->r.svflags & SVF_FAKECLIENT))
		return;

	if (sv_redirected == RD_PACKET)
	{
		Netchan_OutOfBandPrint (NS_SERVER, net_from, "print\n%s", outputbuf);
	}
	else if (sv_redirected == RD_CLIENT)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_servercmd);
		MSG_WriteString (&sv_client->netchan.message, va ("pr %i \"%s\"", PRINT_HIGH, outputbuf));
	}
}


/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/


/*
=================
SV_ClientPrintf

Sends text across to be displayed if the level passes
=================
*/
void SV_ClientPrintf (client_t *cl, int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	if (level < cl->messagelevel)
		return;
	if (cl->edict && (cl->edict->r.svflags & SVF_FAKECLIENT))
		return;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);
	
	MSG_WriteByte (&cl->netchan.message, svc_servercmd);
	MSG_WriteString (&cl->netchan.message, va ("pr %i \"%s\"", level, string));
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[2048];
	client_t	*cl;
	int			i;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);
	
	// echo to console
	if (dedicated->value)
	{
		char	copy[1024];
		int		i, j;
		
		// mask off high bits and colored strings
		for (i=0, j=0 ; j<1023 && string[j] ; ) {
			if ( Q_IsColorString( &string[j] ) ) {
				j += 2;
				continue;
			}
			
			copy[i++] = string[j++]&127;
		}

		copy[i] = 0;
		Com_Printf ("%s", copy);
	}

	for (i=0, cl = svs.clients ; i<sv_maxclients->value; i++, cl++)
	{
		if (level < cl->messagelevel)
			continue;
		if (cl->state != cs_spawned)
			continue;
		if (cl->edict && (cl->edict->r.svflags & SVF_FAKECLIENT))
			continue;

		MSG_WriteByte (&cl->netchan.message, svc_servercmd);
		MSG_WriteString (&cl->netchan.message, va ("pr %i \"%s\"", level, string));
	}
}

/*
=================
SV_BroadcastCommand

Sends a command to all active clients
=================
*/
void SV_BroadcastCommand (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	if (!sv.state)
		return;
	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&sv.multicast, svc_stringcmd);
	MSG_WriteString (&sv.multicast, string);
	SV_Multicast (NULL, MULTICAST_ALL_R);
}

/*
=================
SV_Multicast

Sends the contents of sv.multicast to a subset of the clients,
then clears sv.multicast.

MULTICAST_ALL	same as broadcast (origin can be NULL)
MULTICAST_PVS	send to clients potentially visible from org
MULTICAST_PHS	send to clients potentially hearable from org
=================
*/
void SV_Multicast (vec3_t origin, multicast_t to)
{
	client_t	*client;
	qbyte		*mask;
	int			leafnum, cluster;
	int			j;
	qboolean	reliable;
	int			area1, area2;

	reliable = qfalse;

	if (to != MULTICAST_ALL_R && to != MULTICAST_ALL)
	{
		assert (origin != NULL);

		leafnum = CM_PointLeafnum (origin);
		area1 = CM_LeafArea (leafnum);
	}
	else
	{
		leafnum = 0;	// just to avoid compiler warnings
		area1 = 0;
	}

	// if doing a serverrecord, store everything
	if (svs.demofile)
		SZ_Write (&svs.demo_multicast, sv.multicast.data, sv.multicast.cursize);
	
	switch (to)
	{
	case MULTICAST_ALL_R:
		reliable = qtrue;	// intentional fallthrough
	case MULTICAST_ALL:
		leafnum = 0;
		mask = NULL;
		break;

	case MULTICAST_PHS_R:
		reliable = qtrue;	// intentional fallthrough
	case MULTICAST_PHS:
		leafnum = CM_PointLeafnum (origin);
		cluster = CM_LeafCluster (leafnum);
		mask = CM_ClusterPHS (cluster);
		break;

	case MULTICAST_PVS_R:
		reliable = qtrue;	// intentional fallthrough
	case MULTICAST_PVS:
		leafnum = CM_PointLeafnum (origin);
		cluster = CM_LeafCluster (leafnum);
		mask = CM_ClusterPVS (cluster);
		break;

	default:
		mask = NULL;
		Com_Error (ERR_FATAL, "SV_Multicast: bad to:%i", to);
	}

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < sv_maxclients->value; j++, client++)
	{
		if (client->state == cs_free || client->state == cs_zombie)
			continue;
		if (client->state != cs_spawned && !reliable)
			continue;
		if (client->edict && (client->edict->r.svflags & SVF_FAKECLIENT))
			continue;

		if (mask)
		{
			leafnum = CM_PointLeafnum (client->edict->s.origin);
			cluster = CM_LeafCluster (leafnum);
			area2 = CM_LeafArea (leafnum);
			if (!CM_AreasConnected (area1, area2))
				continue;
			if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
				continue;
		}

		if (reliable)
			SZ_Write (&client->netchan.message, sv.multicast.data, sv.multicast.cursize);
		else
			SZ_Write (&client->datagram, sv.multicast.data, sv.multicast.cursize);
	}

	SZ_Clear (&sv.multicast);
}


/*  
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

If cahnnel & 8, the sound will be sent to everyone, not just
things in the PHS.

FIXME: if entity isn't in PHS, they must be forced to be sent or
have the origin explicitly sent.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

Timeofs can range from 0.0 to 0.1 to cause sounds to be started
later in the frame than they normally would.

If origin is NULL, the origin is determined from the entity origin
or the midpoint of the entity box for bmodels.
==================
*/  
void SV_StartSound (vec3_t origin, edict_t *entity, int channel,
					int soundindex, float volume,
					float attenuation)
{       
	int			sendchan;
    int			flags;
    int			i, v;
	int			ent;
	vec3_t		origin_v;
	qboolean	use_phs;

	if (soundindex < 0 || soundindex >= MAX_SOUNDS)
		Com_Error (ERR_FATAL, "SV_StartSound: soundindex = %i", soundindex);
	if (volume < 0 || volume > 1.0)
		Com_Error (ERR_FATAL, "SV_StartSound: volume = %f", volume);
	if (attenuation < 0 || attenuation > 4)
		Com_Error (ERR_FATAL, "SV_StartSound: attenuation = %f", attenuation);
//	if (channel < 0 || channel > 15)
//		Com_Error (ERR_FATAL, "SV_StartSound: channel = %i", channel);

	ent = NUM_FOR_EDICT(entity);

	if (channel & CHAN_NO_PHS_ADD)	// no PHS flag
	{
		use_phs = qfalse;
		channel &= 7;
	}
	else
		use_phs = qtrue;

	sendchan = (ent<<3) | (channel&7);

	flags = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		flags |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		flags |= SND_ATTENUATION;

	// use the entity origin unless it is a bmodel or explicitly specified
	if (!origin)
	{
		origin = origin_v;

		// the client doesn't know that bmodels have weird origins
		// the origin can also be explicitly set
		if (entity->r.solid == SOLID_BSP)
		{
			for (i=0 ; i<3 ; i++)
				origin_v[i] = entity->s.origin[i]+0.5*(entity->r.mins[i]+entity->r.maxs[i]);
		}
		else
		{
			VectorCopy (entity->s.origin, origin_v);
		}
	}

	v = Q_rint(origin[0]); flags |= (SND_POS0_8|SND_POS0_16);
	if (v+256/2 >= 0 && v+256/2 < 256) flags &= ~SND_POS0_16; else if (v+65536/2 >= 0 && v+65536/2 < 65536) flags &= ~SND_POS0_8;

	v = Q_rint(origin[1]); flags |= (SND_POS1_8|SND_POS1_16);
	if (v+256/2 >= 0 && v+256/2 < 256) flags &= ~SND_POS1_16; else if (v+65536/2 >= 0 && v+65536/2 < 65536) flags &= ~SND_POS1_8;

	v = Q_rint(origin[2]); flags |= (SND_POS2_8|SND_POS2_16);
	if (v+256/2 >= 0 && v+256/2 < 256) flags &= ~SND_POS2_16; else if (v+65536/2 >= 0 && v+65536/2 < 65536) flags &= ~SND_POS2_8;

	MSG_WriteByte (&sv.multicast, svc_sound);
	MSG_WriteByte (&sv.multicast, flags);
	MSG_WriteByte (&sv.multicast, soundindex);

	// always send the entity number for channel overrides
	MSG_WriteShort (&sv.multicast, sendchan);

	if ((flags & (SND_POS0_8|SND_POS0_16)) == SND_POS0_8)
		MSG_WriteChar (&sv.multicast, Q_rint(origin[0]));
	else if ((flags & (SND_POS0_8|SND_POS0_16)) == SND_POS0_16)
		MSG_WriteShort (&sv.multicast, Q_rint(origin[0]));
	else
		MSG_WriteInt3 (&sv.multicast, Q_rint(origin[0]));

	if ((flags & (SND_POS1_8|SND_POS1_16)) == SND_POS1_8)
		MSG_WriteChar (&sv.multicast, Q_rint(origin[1]));
	else if ((flags & (SND_POS1_8|SND_POS1_16)) == SND_POS1_16)
		MSG_WriteShort (&sv.multicast, Q_rint(origin[1]));
	else
		MSG_WriteInt3 (&sv.multicast, Q_rint(origin[1]));

	if ((flags & (SND_POS2_8|SND_POS2_16)) == SND_POS2_8)
		MSG_WriteChar (&sv.multicast, Q_rint(origin[2]));
	else if ((flags & (SND_POS2_8|SND_POS2_16)) == SND_POS2_16)
		MSG_WriteShort (&sv.multicast, Q_rint(origin[2]));
	else
		MSG_WriteInt3 (&sv.multicast, Q_rint(origin[2]));

	if (flags & SND_VOLUME)
		MSG_WriteByte (&sv.multicast, volume*255);
	if (flags & SND_ATTENUATION)
		MSG_WriteByte (&sv.multicast, attenuation*64);

	// if the sound doesn't attenuate,send it to everyone
	// (global radio chatter, voiceovers, etc)
	if (attenuation == ATTN_NONE)
		use_phs = qfalse;

	if (channel & CHAN_RELIABLE)
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS_R);
		else
			SV_Multicast (origin, MULTICAST_ALL_R);
	}
	else
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS);
		else
			SV_Multicast (origin, MULTICAST_ALL);
	}
}           


/*
===============================================================================

FRAME UPDATES

===============================================================================
*/



/*
=======================
SV_SendClientDatagram
=======================
*/
qboolean SV_SendClientDatagram (client_t *client)
{
	qbyte		msg_buf[MAX_MSGLEN];
	sizebuf_t	msg;

	SV_BuildClientFrame (client);

	SZ_Init (&msg, msg_buf, sizeof(msg_buf));
	msg.allowoverflow = qtrue;

	// send over all the relevant entity_state_t
	// and the player_state_t
	SV_WriteFrameToClient (client, &msg);

	// copy the accumulated multicast datagram
	// for this client out to the message
	// it is necessary for this to be after the WriteEntities
	// so that entity references will be current
	if (client->datagram.overflowed)
		Com_Printf ("WARNING: datagram overflowed for %s\n", client->name);
	else
		SZ_Write (&msg, client->datagram.data, client->datagram.cursize);
	SZ_Clear (&client->datagram);

	if (msg.overflowed)
	{	// must have room left for the packet header
		Com_Printf ("WARNING: msg overflowed for %s\n", client->name);
		SZ_Clear (&msg);
	}

	// send the datagram
	Netchan_Transmit (&client->netchan, msg.cursize, msg.data);

	// record the size for rate estimation
	client->message_size[sv.framenum % RATE_MESSAGES] = msg.cursize;

	return qtrue;
}


/*
==================
SV_DemoCompleted
==================
*/
void SV_DemoCompleted (void)
{
	if (sv.demofile)
	{
		FS_FCloseFile (sv.demofile);
		sv.demofile = 0;
	}
	SV_Nextserver ();
}


/*
=======================
SV_RateDrop

Returns true if the client is over its current
bandwidth estimation and should not be sent another packet
=======================
*/
qboolean SV_RateDrop (client_t *c)
{
	int		total;
	int		i;

	// never drop over the loopback
	if (c->netchan.remote_address.type == NA_LOOPBACK)
		return qfalse;

	total = 0;

	for (i = 0 ; i < RATE_MESSAGES ; i++)
	{
		total += c->message_size[i];
	}

	if (total > c->rate)
	{
		c->suppressCount++;
		c->message_size[sv.framenum % RATE_MESSAGES] = 0;
		return qtrue;
	}

	return qfalse;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int			i;
	client_t	*c;
	int			msglen;
	qbyte		msgbuf[MAX_MSGLEN];

	msglen = 0;

	// read the next demo message if needed
	if (sv.state == ss_demo && sv.demofile)
	{
		if (sv_paused->value)
			msglen = 0;
		else
		{
			if (sv.demolen <= 0)
			{
				SV_DemoCompleted ();
				return;
			}

			// get the next message
			FS_Read (&msglen, 4, sv.demofile);
			sv.demolen -= 4;

			msglen = LittleLong (msglen);
			if (msglen == -1)
			{
				SV_DemoCompleted ();
				return;
			}

			if (msglen > MAX_MSGLEN)
				Com_Error (ERR_DROP, "SV_SendClientMessages: msglen > MAX_MSGLEN");

			if (sv.demolen <= 0)
			{
				SV_DemoCompleted ();
				return;
			}
			FS_Read (msgbuf, msglen, sv.demofile);
			sv.demolen -= msglen;
		}
	}

	// send a message to each connected client
	for (i=0, c = svs.clients ; i<sv_maxclients->value; i++, c++)
	{
		if (!c->state)
			continue;
		if (c->edict && (c->edict->r.svflags & SVF_FAKECLIENT))
			continue;

		// if the reliable message overflowed,
		// drop the client
		if (c->netchan.message.overflowed)
		{
			SZ_Clear (&c->netchan.message);
			SZ_Clear (&c->datagram);
			SV_BroadcastPrintf (PRINT_HIGH, "%s overflowed\n", c->name);
			SV_DropClient (c);
		}

		if (sv.state == ss_cinematic 
			|| sv.state == ss_demo 
			)
			Netchan_Transmit (&c->netchan, msglen, msgbuf);
		else if (c->state == cs_spawned)
		{
			// don't overrun bandwidth
			if (SV_RateDrop (c))
				continue;

			SV_SendClientDatagram (c);
		}
		else
		{
	// just update reliable	if needed
			if (c->netchan.message.cursize	|| curtime - c->netchan.last_sent > 1000)
				Netchan_Transmit (&c->netchan, 0, NULL);
		}
	}
}

