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

#include "server.h"

/*
=============================================================================

Encode a client frame onto the network channel

=============================================================================
*/

/*
=============
SV_EmitPacketEntities

Writes a delta update of an entity_state_t list to the message.
=============
*/
void SV_EmitPacketEntities (client_frame_t *from, client_frame_t *to, sizebuf_t *msg)
{
	entity_state_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;
	int		bits;

	MSG_WriteByte (msg, svc_packetentities);

	if (!from)
		from_num_entities = 0;
	else
		from_num_entities = from->num_entities;

	newindex = 0;
	oldindex = 0;
	while (newindex < to->num_entities || oldindex < from_num_entities)
	{
		if (newindex >= to->num_entities)
			newnum = 9999;
		else
		{
			newent = &svs.client_entities[(to->first_entity+newindex)%svs.num_client_entities];
			newnum = newent->number;
		}

		if (oldindex >= from_num_entities)
			oldnum = 9999;
		else
		{
			oldent = &svs.client_entities[(from->first_entity+oldindex)%svs.num_client_entities];
			oldnum = oldent->number;
		}

		if (newnum == oldnum)
		{	// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping
			MSG_WriteDeltaEntity (oldent, newent, msg, qfalse, newent->number <= sv_maxclients->value 
				|| ((EDICT_NUM(newent->number))->r.svflags & SVF_FORCEOLDORIGIN));
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity (&sv.baselines[newnum], newent, msg, qtrue, !((EDICT_NUM(newent->number))->r.svflags & SVF_NOOLDORIGIN));
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
			bits = U_REMOVE;
			if (oldnum >= 256)
				bits |= U_NUMBER16 | U_MOREBITS1;

			MSG_WriteByte (msg,	bits&255 );
			if (bits & 0x0000ff00)
				MSG_WriteByte (msg,	(bits>>8)&255 );

			if (bits & U_NUMBER16)
				MSG_WriteShort (msg, oldnum);
			else
				MSG_WriteByte (msg, oldnum);

			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (msg, 0);	// end of packetentities
}

/*
=============
SV_WritePlayerstateToClient
=============
*/
void SV_WritePlayerstateToClient (client_frame_t *from, client_frame_t *to, sizebuf_t *msg)
{
	int				i;
	int				pflags;
	player_state_t	*ps, *ops;
	player_state_t	dummy;
	int				statbits;

	ps = &to->ps;
	if (!from)
	{
		memset (&dummy, 0, sizeof(dummy));
		ops = &dummy;
	}
	else
		ops = &from->ps;

	//
	// determine what needs to be sent
	//
	pflags = 0;

	if (ps->pmove.pm_type != ops->pmove.pm_type)
		pflags |= PS_M_TYPE;

	if (ps->pmove.origin[0] != ops->pmove.origin[0])
		pflags |= PS_M_ORIGIN0;
	if (ps->pmove.origin[1] != ops->pmove.origin[1])
		pflags |= PS_M_ORIGIN1;
	if (ps->pmove.origin[2] != ops->pmove.origin[2])
		pflags |= PS_M_ORIGIN2;

	if (ps->pmove.velocity[0] != ops->pmove.velocity[0])
		pflags |= PS_M_VELOCITY0;
	if (ps->pmove.velocity[1] != ops->pmove.velocity[1])
		pflags |= PS_M_VELOCITY1;
	if (ps->pmove.velocity[2] != ops->pmove.velocity[2])
		pflags |= PS_M_VELOCITY2;

	if (ps->pmove.pm_time != ops->pmove.pm_time)
		pflags |= PS_M_TIME;

	if (ps->pmove.pm_flags != ops->pmove.pm_flags)
		pflags |= PS_M_FLAGS;

	if (ps->pmove.gravity != ops->pmove.gravity)
		pflags |= PS_M_GRAVITY;

	if (ps->pmove.delta_angles[0] != ops->pmove.delta_angles[0])
		pflags |= PS_M_DELTA_ANGLES0;
	if (ps->pmove.delta_angles[1] != ops->pmove.delta_angles[1])
		pflags |= PS_M_DELTA_ANGLES1;
	if (ps->pmove.delta_angles[2] != ops->pmove.delta_angles[2])
		pflags |= PS_M_DELTA_ANGLES2;

	if (ps->viewoffset[0] != ops->viewoffset[0]
		|| ps->viewoffset[1] != ops->viewoffset[1]
		|| ps->viewoffset[2] != ops->viewoffset[2] )
		pflags |= PS_VIEWOFFSET;

	if (ps->viewangles[0] != ops->viewangles[0]
		|| ps->viewangles[1] != ops->viewangles[1]
		|| ps->viewangles[2] != ops->viewangles[2] )
		pflags |= PS_VIEWANGLES;

	if (ps->kick_angles[0] != ops->kick_angles[0]
		|| ps->kick_angles[1] != ops->kick_angles[1]
		|| ps->kick_angles[2] != ops->kick_angles[2] )
		pflags |= PS_KICKANGLES;

	if (ps->blend[0] != ops->blend[0]
		|| ps->blend[1] != ops->blend[1]
		|| ps->blend[2] != ops->blend[2]
		|| ps->blend[3] != ops->blend[3] )
		pflags |= PS_BLEND;

	if (ps->fov != ops->fov)
		pflags |= PS_FOV;

	if (ps->gunframe != ops->gunframe)
		pflags |= PS_WEAPONFRAME;

	if (ps->gunindex != ops->gunindex)
		pflags |= PS_WEAPONINDEX;

	//
	// write it
	//
	MSG_WriteByte (msg, svc_playerinfo);

	if (pflags & 0xff000000)
		pflags |= PS_MOREBITS3 | PS_MOREBITS2 | PS_MOREBITS1;
	else if (pflags & 0x00ff0000)
		pflags |= PS_MOREBITS2 | PS_MOREBITS1;
	else if (pflags & 0x0000ff00)
		pflags |= PS_MOREBITS1;

	MSG_WriteByte (msg,	pflags&255 );

	if (pflags & 0xff000000)
	{
		MSG_WriteByte (msg,	(pflags>>8)&255 );
		MSG_WriteByte (msg,	(pflags>>16)&255 );
		MSG_WriteByte (msg,	(pflags>>24)&255 );
	}
	else if (pflags & 0x00ff0000)
	{
		MSG_WriteByte (msg,	(pflags>>8)&255 );
		MSG_WriteByte (msg,	(pflags>>16)&255 );
	}
	else if (pflags & 0x0000ff00)
	{
		MSG_WriteByte (msg,	(pflags>>8)&255 );
	}

	//
	// write the pmove_state_t
	//
	if (pflags & PS_M_TYPE)
		MSG_WriteByte (msg, ps->pmove.pm_type);

	if (pflags & PS_M_ORIGIN0)
		MSG_WriteLong (msg, ps->pmove.origin[0]);
	if (pflags & PS_M_ORIGIN1)
		MSG_WriteLong (msg, ps->pmove.origin[1]);
	if (pflags & PS_M_ORIGIN2)
		MSG_WriteLong (msg, ps->pmove.origin[2]);

	if (pflags & PS_M_VELOCITY0)
		MSG_WriteLong (msg, ps->pmove.velocity[0]);
	if (pflags & PS_M_VELOCITY1)
		MSG_WriteLong (msg, ps->pmove.velocity[1]);
	if (pflags & PS_M_VELOCITY2)
		MSG_WriteLong (msg, ps->pmove.velocity[2]);

	if (pflags & PS_M_TIME)
		MSG_WriteByte (msg, ps->pmove.pm_time);

	if (pflags & PS_M_FLAGS)
		MSG_WriteByte (msg, ps->pmove.pm_flags);

	if (pflags & PS_M_GRAVITY)
		MSG_WriteShort (msg, ps->pmove.gravity);

	if (pflags & PS_M_DELTA_ANGLES0)
		MSG_WriteShort (msg, ps->pmove.delta_angles[0]);
	if (pflags & PS_M_DELTA_ANGLES1)
		MSG_WriteShort (msg, ps->pmove.delta_angles[1]);
	if (pflags & PS_M_DELTA_ANGLES2)
		MSG_WriteShort (msg, ps->pmove.delta_angles[2]);

	//
	// write the rest of the player_state_t
	//
	if (pflags & PS_VIEWOFFSET)
	{
		MSG_WriteChar (msg, ps->viewoffset[0]*4);
		MSG_WriteChar (msg, ps->viewoffset[1]*4);
		MSG_WriteChar (msg, ps->viewoffset[2]*4);
	}

	if (pflags & PS_VIEWANGLES)
	{
		MSG_WriteAngle16 (msg, ps->viewangles[0]);
		MSG_WriteAngle16 (msg, ps->viewangles[1]);
		MSG_WriteAngle16 (msg, ps->viewangles[2]);
	}

	if (pflags & PS_KICKANGLES)
	{
		MSG_WriteChar (msg, ps->kick_angles[0]*4);
		MSG_WriteChar (msg, ps->kick_angles[1]*4);
		MSG_WriteChar (msg, ps->kick_angles[2]*4);
	}

	if (pflags & PS_WEAPONINDEX)
		MSG_WriteByte (msg, ps->gunindex);

	if (pflags & PS_WEAPONFRAME)
		MSG_WriteShort (msg, ps->gunframe);

	if (pflags & PS_BLEND)
	{
		MSG_WriteByte (msg, ps->blend[0]*255);
		MSG_WriteByte (msg, ps->blend[1]*255);
		MSG_WriteByte (msg, ps->blend[2]*255);
		MSG_WriteByte (msg, ps->blend[3]*255);
	}
	if (pflags & PS_FOV)
		MSG_WriteByte (msg, ps->fov);

	// send stats
	statbits = 0;
	for (i=0 ; i<MAX_STATS ; i++)
		if (ps->stats[i] != ops->stats[i])
			statbits |= 1<<i;

	MSG_WriteLong (msg, statbits);
	for (i=0 ; i<MAX_STATS ; i++)
		if (statbits & (1<<i) )
			MSG_WriteShort (msg, ps->stats[i]);
}

/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient (client_t *client, sizebuf_t *msg)
{
	client_frame_t		*frame, *oldframe;
	int					lastframe;

	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];

	if (client->lastframe <= 0)
	{	// client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	}
	else if (sv.framenum - client->lastframe >= (UPDATE_BACKUP - 3) )
	{	// client hasn't gotten a good message through in a long time
		oldframe = NULL;
		lastframe = -1;
	}
	else
	{	// we have a valid message to delta from
		oldframe = &client->frames[client->lastframe & UPDATE_MASK];
		lastframe = client->lastframe;
	}

	MSG_WriteByte (msg, svc_frame);
	MSG_WriteLong (msg, sv.framenum);
	MSG_WriteLong (msg, lastframe);	// what we are delta'ing from
	MSG_WriteByte (msg, client->suppressCount);	// rate dropped packets
	client->suppressCount = 0;

	// send over the areabits
	MSG_WriteByte (msg, frame->areabytes);
	SZ_Write (msg, frame->areabits, frame->areabytes);

	// delta encode the playerstate
	SV_WritePlayerstateToClient (oldframe, frame, msg);

	// delta encode the entities
	SV_EmitPacketEntities (oldframe, frame, msg);
}


/*
=============================================================================

Build a client frame structure

=============================================================================
*/

qbyte		fatpvs[MAX_MAP_LEAFS/8];
qbyte		fatphs[MAX_MAP_LEAFS/8];

/*
============
SV_FatPVS

The client will interpolate the view position,
so we can't use a single PVS point
===========
*/
void SV_FatPVS (vec3_t org)
{
	int		leafs[128];
	int		i, j, count;
	int		longs;
	qbyte	*src;
	vec3_t	mins, maxs;

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = CM_BoxLeafnums (mins, maxs, leafs, 128, NULL);
	if (count < 1)
		Com_Error (ERR_FATAL, "SV_FatPVS: count < 1");
	longs = CM_ClusterSize()>>2;

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = CM_LeafCluster(leafs[i]);

	memcpy (fatpvs, CM_ClusterPVS(leafs[0]), longs<<2);

	// or in all the other leaf bits
	for (i=1 ; i<count ; i++)
	{
		for (j=0 ; j<i ; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue;		// already have the cluster we want
		src = CM_ClusterPVS(leafs[i]);
		for (j=0 ; j<longs ; j++)
			((long *)fatpvs)[j] |= ((long *)src)[j];
	}
}

/*
============
SV_FatPHS
===========
*/
void SV_FatPHS (int cluster)
{
	memcpy (fatphs, CM_ClusterPHS (cluster), CM_ClusterSize());
}

/*
============
SV_MergePVS

Portal entities add a second PVS at origin2 to fatpvs 
===========
*/
void SV_MergePVS (vec3_t org)
{
	int		leafs[128];
	int		i, j, count;
	int		longs;
	qbyte	*src;
	vec3_t	mins, maxs;

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = org[i] - 1;
		maxs[i] = org[i] + 1;
	}

	count = CM_BoxLeafnums (mins, maxs, leafs, 128, NULL);
	if (count < 1)
		Com_Error (ERR_FATAL, "SV_FatPVS: count < 1");
	longs = CM_ClusterSize()>>2;

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = CM_LeafCluster(leafs[i]);

	// or in all the other leaf bits
	for (i=0 ; i<count ; i++)
	{
		for (j=0 ; j<i ; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue;		// already have the cluster we want
		src = CM_ClusterPVS(leafs[i]);
		for (j=0 ; j<longs ; j++)
			((long *)fatpvs)[j] |= ((long *)src)[j];
	}
}

/*
============
SV_MergePHS
===========
*/
void SV_MergePHS (int cluster)
{
	int		i, longs;
	qbyte	*src;

	longs = CM_ClusterSize()>>2;

	// or in all the other leaf bits
	src = CM_ClusterPHS (cluster);
	for (i=0 ; i<longs ; i++)
		((long *)fatphs)[i] |= ((long *)src)[i];
}

/*
=============
SV_CullEntity
=============
*/
qboolean SV_CullEntity (edict_t *ent)
{
	int i, l;

	if (ent->r.num_clusters == -1)
	{	// too many leafs for individual check, go by headnode
		if (!CM_HeadnodeVisible (ent->r.headnode, fatpvs))
			return qtrue;
		return qfalse;
	}

	// check individual leafs
	for (i=0 ; i < ent->r.num_clusters ; i++)
	{
		l = ent->r.clusternums[i];
		if (fatpvs[l >> 3] & (1 << (l&7) )) {
			return qfalse;
		}
	}
		
	return qtrue;		// not visible
}

/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client, and
copies off the playerstat and areabits.
=============
*/
void SV_BuildClientFrame (client_t *client)
{
	int				e, l;
	vec3_t			org;
	edict_t			*ent, *clent;
	edict_t			*pedicts[MAX_EDICTS];
	client_frame_t	*frame;
	entity_state_t	*state;
	int				clientarea, portalarea;
	int				leafnum, clusternum;
	int				numedicts;
	qboolean		portalview;

	clent = client->edict;
	if( !clent->r.client )
		return;		// not in game yet

	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];
	frame->senttime = svs.realtime; // save it for ping calc later

	// find the client's PVS
	VectorAdd( clent->s.origin, clent->r.client->ps.viewoffset, org );

	leafnum = CM_PointLeafnum( org );
	clusternum = CM_LeafCluster( leafnum );
	clientarea = CM_LeafArea( leafnum );

	// calculate the visible areas
	frame->areabytes = CM_WriteAreaBits( frame->areabits, clientarea );

	// grab the current player_state_t
	frame->ps = clent->r.client->ps;

	// build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	if( clusternum == -1 ) {
		for( e = 1; e < sv.num_edicts; e++ ) {
			ent = EDICT_NUM( e );

			// ignore ents without visible models
			if( ent->r.svflags & SVF_NOCLIENT )
				continue;
			if( ent->r.visclent && (ent->r.visclent != clent) )
				continue;

			// ignore ents without visible models unless they have an effect
			if( !ent->s.modelindex && !ent->s.effects && !ent->s.sound && !ent->s.events[0] )
				continue;
			if( !(ent->r.svflags & SVF_BROADCAST) || ent != clent )
				continue;

			// fix number if broken
			if( ent->s.number != e ) {
				Com_DPrintf( "FIXING ENT->S.NUMBER!!!\n" );
				ent->s.number = e;
			}

			// add it to the circular client_entities array
			state = &svs.client_entities[svs.next_client_entities%svs.num_client_entities];
			*state = ent->s;

			svs.next_client_entities++;
			frame->num_entities++;
		}
		return;
	}

	// save client's PVS so it can be later compared with fatpvs
	SV_FatPVS( org );
	SV_FatPHS( clusternum );

	portalview = qfalse;

	// portal entities are the first to be checked so we can merge PV sets
	for( e = 1, numedicts = 0; e < sv.num_edicts; e++ ) {
		ent = EDICT_NUM( e );

		// ignore ents without visible models
		if( ent->r.svflags & SVF_NOCLIENT )
			continue;
		if( ent->r.visclent && (ent->r.visclent != clent) )
			continue;

		// ignore ents without visible models unless they have an effect
		if( !ent->s.modelindex && !ent->s.effects && !ent->s.sound && !ent->s.events[0] )
			continue;

		// fix number if broken
		if( ent->s.number != e ) {
			Com_DPrintf( "FIXING ENT->S.NUMBER!!!\n" );
			ent->s.number = e;
		}

		// ignore if not touching a PV leaf
		if( ent != clent || !(ent->r.svflags & SVF_BROADCAST) ) {
			if( !(ent->r.svflags & SVF_PORTAL) ) {
				if( !ent->s.modelindex && !ent->s.events[0] ) {
					// don't send sounds if they will be attenuated away
					vec3_t	delta;
					float	len;

					VectorSubtract( org, ent->s.origin, delta );
					len = VectorLength( delta );
					if( len > 400 )
						continue;
				}

				pedicts[numedicts++] = ent;
				continue;
			}

			// check area
			if( !CM_AreasConnected( clientarea, ent->r.areanum ) )
				continue;
			if( SV_CullEntity( ent ) )
				continue;

			// merge PV sets if portal 
			if( !VectorCompare( ent->s.origin, ent->s.origin2 ) ) {
				SV_MergePVS( ent->s.old_origin );

				portalarea = CM_PointLeafnum( ent->s.origin2 );
				SV_MergePHS( CM_LeafCluster( portalarea ) );

				portalarea = CM_LeafArea( portalarea );
				CM_MergeAreaBits( frame->areabits, portalarea );

				portalview = qtrue;
			}
		}

		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities%svs.num_client_entities];
		*state = ent->s;

		// don't mark players missiles as solid
		if( ent->r.owner == client->edict )
			state->solid = 0;

		svs.next_client_entities++;
		frame->num_entities++;
	}

	for( e = 0; e < numedicts; e++ ) {
		ent = pedicts[e];

		// check area
		if( ! (frame->areabits[ent->r.areanum>>3] & (1<<(ent->r.areanum&7)) ) ) {
			// doors can legally straddle two areas, so
			// we may need to check another one
			if( !ent->r.areanum2
				|| !(frame->areabits[ent->r.areanum2>>3] & (1<<(ent->r.areanum2&7)) ) )
				continue;		// blocked by a door
		}

		// just check one point for PHS
		if( ent->r.svflags & SVF_FORCEOLDORIGIN ) {
			if( ent->r.num_clusters == -1 ) {
				if( !CM_HeadnodeVisible( ent->r.headnode, fatphs ) )
					continue;
			} else {
				l = ent->r.clusternums[0];
				if( !(fatphs[l >> 3] & (1 << (l&7) )) )
					continue;
			}
		} else if( SV_CullEntity( ent ) ) {
			continue;
		}

		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities%svs.num_client_entities];
		*state = ent->s;

		// don't mark players missiles as solid
		if( ent->r.owner == client->edict )
			state->solid = 0;

		svs.next_client_entities++;
		frame->num_entities++;
	}
}


/*
==================
SV_RecordDemoMessage

Save everything in the world out without deltas.
Used for recording footage for merged or assembled demos
==================
*/
void SV_RecordDemoMessage (void)
{
	int			e;
	edict_t		*ent;
	entity_state_t	nostate;
	sizebuf_t	buf;
	qbyte		buf_data[MAX_DEMO_MSGLEN];
	int			len;

	if (!svs.demofile)
		return;

	memset (&nostate, 0, sizeof(nostate));
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	// write a frame message that doesn't contain a player_state_t
	MSG_WriteByte (&buf, svc_frame);
	MSG_WriteLong (&buf, sv.framenum);

	MSG_WriteByte (&buf, svc_packetentities);

	e = 1;
	ent = EDICT_NUM(e);
	while (e < sv.num_edicts) 
	{
		// ignore ents without visible models unless they have an effect
		if (ent->r.inuse &&
			ent->s.number && 
			(ent->s.modelindex || ent->s.effects || ent->s.sound || ent->s.events[0]) && 
			!(ent->r.svflags & SVF_NOCLIENT))
			MSG_WriteDeltaEntity (&nostate, &ent->s, &buf, qfalse, qtrue);

		e++;
		ent = EDICT_NUM(e);
	}

	MSG_WriteShort (&buf, 0);		// end of packetentities

	// now add the accumulated multicast information
	SZ_Write (&buf, svs.demo_multicast.data, svs.demo_multicast.cursize);
	SZ_Clear (&svs.demo_multicast);

	// now write the entire message to the file, prefixed by the length
	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, svs.demofile);
	fwrite (buf.data, buf.cursize, 1, svs.demofile);
}
