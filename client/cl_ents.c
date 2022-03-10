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
// cl_ents.c -- entity parsing and management

#include "client.h"

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

/*
=================
CL_ParseEntityBits

Returns the entity number and the header bits
=================
*/

int CL_ParseEntityBits (unsigned *bits)
{
	unsigned	b, total;
	int			number;

	total = MSG_ReadByte (&net_message);
	if (total & U_MOREBITS1)
	{
		b = MSG_ReadByte (&net_message);
		total |= b<<8;
	}
	if (total & U_MOREBITS2)
	{
		b = MSG_ReadByte (&net_message);
		total |= b<<16;
	}
	if (total & U_MOREBITS3)
	{
		b = MSG_ReadByte (&net_message);
		total |= b<<24;
	}

	if (total & U_NUMBER16)
		number = MSG_ReadShort (&net_message);
	else
		number = MSG_ReadByte (&net_message);

	*bits = total;

	return number;
}

/*
==================
CL_ParseDelta

Can go from either a baseline or a previous packet_entity
==================
*/
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, unsigned bits)
{
	// set everything to the state we are delta'ing from
	*to = *from;

	VectorCopy (from->origin, to->old_origin);
	to->number = number;

	if (bits & U_SOLID)
		to->solid = MSG_ReadShort (&net_message);

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte (&net_message);
	if (bits & U_MODEL2)
		to->modelindex2 = MSG_ReadByte (&net_message);
	if (bits & U_MODEL3)
		to->modelindex3 = MSG_ReadByte (&net_message);

	if (bits & U_FRAME8)
		to->frame = MSG_ReadByte (&net_message);
	if (bits & U_FRAME16)
		to->frame = MSG_ReadShort (&net_message);

	if ((bits & U_SKIN8) && (bits & U_SKIN16))		//used for laser colors
		to->skinnum = MSG_ReadLong(&net_message);
	else if (bits & U_SKIN8)
		to->skinnum = MSG_ReadByte(&net_message);
	else if (bits & U_SKIN16)
		to->skinnum = MSG_ReadShort(&net_message);

	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		to->effects = MSG_ReadLong(&net_message);
	else if (bits & U_EFFECTS8)
		to->effects = MSG_ReadByte(&net_message);
	else if (bits & U_EFFECTS16)
		to->effects = MSG_ReadShort(&net_message);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
		to->renderfx = MSG_ReadLong(&net_message);
	else if (bits & U_RENDERFX8)
		to->renderfx = MSG_ReadByte(&net_message);
	else if (bits & U_RENDERFX16)
		to->renderfx = MSG_ReadShort(&net_message);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord (&net_message);
		
	if ( (bits & U_ANGLE1) && (to->solid == SOLID_BMODEL) )
		to->angles[0] = MSG_ReadAngle16(&net_message);
	else if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle(&net_message);

	if ( (bits & U_ANGLE2) && (to->solid == SOLID_BMODEL) )
		to->angles[1] = MSG_ReadAngle16(&net_message);
	else if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle(&net_message);

	if ( (bits & U_ANGLE3) && (to->solid == SOLID_BMODEL) )
		to->angles[2] = MSG_ReadAngle16(&net_message);
	else if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle(&net_message);

	if (bits & U_OLDORIGIN)
		MSG_ReadPos (&net_message, to->old_origin);

	if (bits & U_TYPE)
		to->type = MSG_ReadByte (&net_message);

	if (bits & U_SOUND)
		to->sound = MSG_ReadByte (&net_message);

	if ( bits & U_EVENT ) {
		int event;

		event = MSG_ReadByte (&net_message);
		if ( event & EV_INVERSE ) {
			to->events[0] = event & ~EV_INVERSE;
			to->eventParms[0] = MSG_ReadByte (&net_message);
		} else {
			to->events[0] = event;
			to->eventParms[0] = 0;
		}
	} else {
		to->events[0] = 0;
		to->eventParms[0] = 0;
	}

	if ( bits & U_EVENT2 ) {
		int event;

		event = MSG_ReadByte (&net_message);
		if ( event & EV_INVERSE ) {
			to->events[1] = event & ~EV_INVERSE;
			to->eventParms[1] = MSG_ReadByte (&net_message);
		} else {
			to->events[1] = event;
			to->eventParms[1] = 0;
		}
	} else {
		to->events[1] = 0;
		to->eventParms[1] = 0;
	}

	if (bits & U_WEAPON)
		to->weapon = MSG_ReadByte (&net_message);

	if (bits & U_LIGHT)
		to->light = MSG_ReadLong (&net_message);
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
void CL_DeltaEntity (frame_t *frame, int newnum, entity_state_t *old, unsigned bits)
{
	entity_state_t	*state;

	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];
	cl.parse_entities++;
	frame->numEntities++;

	CL_ParseDelta (old, state, newnum, bits);
	CL_GameModule_NewPacketEntityState (newnum, state);
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities (frame_t *oldframe, frame_t *newframe)
{
	int			newnum;
	unsigned	bits;
	entity_state_t	*oldstate;
	int			oldindex, oldnum;

	newframe->parseEntities = cl.parse_entities;
	newframe->numEntities = 0;

	CL_GameModule_BeginFrameSequence ();

	// delta from the entities present in oldframe
	oldindex = 0;
	if (!oldframe)
		oldnum = 99999;
	else
	{
		if (oldindex >= oldframe->numEntities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parseEntities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while (1)
	{
		newnum = CL_ParseEntityBits (&bits);
		if (newnum >= MAX_EDICTS)
			Com_Error (ERR_DROP, "CL_ParsePacketEntities: bad number:%i", newnum);

		if (net_message.readcount > net_message.cursize)
			Com_Error (ERR_DROP, "CL_ParsePacketEntities: end of message");

		if (!newnum)
			break;

		while (oldnum < newnum)
		{	// one or more entities from the old packet are unchanged
			if (cl_shownet->integer == 3)
				Com_Printf ("   unchanged: %i\n", oldnum);
			CL_DeltaEntity (newframe, oldnum, oldstate, 0);
			
			oldindex++;

			if (oldindex >= oldframe->numEntities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parseEntities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}

		if (bits & U_REMOVE)
		{	// the entity present in oldframe is not in the current frame
			if (cl_shownet->integer == 3)
				Com_Printf ("   remove: %i\n", newnum);
			if (oldnum != newnum)
				Com_Printf ("U_REMOVE: oldnum != newnum\n");

			oldindex++;

			if (oldindex >= oldframe->numEntities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parseEntities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum == newnum)
		{	// delta from previous state
			if (cl_shownet->integer == 3)
				Com_Printf ("   delta: %i\n", newnum);
			CL_DeltaEntity (newframe, newnum, oldstate, bits);

			oldindex++;

			if (oldindex >= oldframe->numEntities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parseEntities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum > newnum)
		{	// delta from baseline
			if (cl_shownet->integer == 3)
				Com_Printf ("   baseline: %i\n", newnum);
			CL_DeltaEntity (newframe, newnum, &cl_baselines[newnum], bits);
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while (oldnum != 99999)
	{	// one or more entities from the old packet are unchanged
		if (cl_shownet->integer == 3)
			Com_Printf ("   unchanged: %i\n", oldnum);
		CL_DeltaEntity (newframe, oldnum, oldstate, 0);

		oldindex++;

		if (oldindex >= oldframe->numEntities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parseEntities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	CL_GameModule_EndFrameSequence ();
}



/*
===================
CL_ParsePlayerstate
===================
*/
void CL_ParsePlayerstate (frame_t *oldframe, frame_t *newframe)
{
	int			flags;
	player_state_t	*state;
	int			i, b;
	int			statbits;

	state = &newframe->playerState;

	// clear to old value before delta parsing
	if (oldframe)
		*state = oldframe->playerState;
	else
		memset (state, 0, sizeof(*state));

	flags = MSG_ReadByte (&net_message);
	if (flags & PS_MOREBITS1)
	{
		b = MSG_ReadByte (&net_message);
		flags |= b<<8;
	}
	if (flags & PS_MOREBITS2)
	{
		b = MSG_ReadByte (&net_message);
		flags |= b<<16;
	}
	if (flags & PS_MOREBITS3)
	{
		b = MSG_ReadByte (&net_message);
		flags |= b<<24;
	}

	//
	// parse the pmove_state_t
	//
	if (flags & PS_M_TYPE)
		state->pmove.pm_type = MSG_ReadByte (&net_message);

	if (flags & PS_M_ORIGIN0)
		state->pmove.origin[0] = MSG_ReadInt3 (&net_message);
	if (flags & PS_M_ORIGIN1)
		state->pmove.origin[1] = MSG_ReadInt3 (&net_message);
	if (flags & PS_M_ORIGIN2)
		state->pmove.origin[2] = MSG_ReadInt3 (&net_message);

	if (flags & PS_M_VELOCITY0)
		state->pmove.velocity[0] = MSG_ReadInt3 (&net_message);
	if (flags & PS_M_VELOCITY1)
		state->pmove.velocity[1] = MSG_ReadInt3 (&net_message);
	if (flags & PS_M_VELOCITY2)
		state->pmove.velocity[2] = MSG_ReadInt3 (&net_message);

	if (flags & PS_M_TIME)
		state->pmove.pm_time = MSG_ReadByte (&net_message);

	if (flags & PS_M_FLAGS)
		state->pmove.pm_flags = MSG_ReadByte (&net_message);

	if (flags & PS_M_GRAVITY)
		state->pmove.gravity = MSG_ReadShort (&net_message);

	if (flags & PS_M_DELTA_ANGLES0)
		state->pmove.delta_angles[0] = MSG_ReadShort (&net_message);
	if (flags & PS_M_DELTA_ANGLES1)
		state->pmove.delta_angles[1] = MSG_ReadShort (&net_message);
	if (flags & PS_M_DELTA_ANGLES2)
		state->pmove.delta_angles[2] = MSG_ReadShort (&net_message);

	//
	// parse the rest of the player_state_t
	//
	if (flags & PS_VIEWOFFSET)
	{
		state->viewoffset[0] = MSG_ReadChar (&net_message) * 0.25;
		state->viewoffset[1] = MSG_ReadChar (&net_message) * 0.25;
		state->viewoffset[2] = MSG_ReadChar (&net_message) * 0.25;
	}

	if (flags & PS_VIEWANGLES)
	{
		state->viewangles[0] = MSG_ReadAngle16 (&net_message);
		state->viewangles[1] = MSG_ReadAngle16 (&net_message);
		state->viewangles[2] = MSG_ReadAngle16 (&net_message);
	}

	if (flags & PS_KICKANGLES)
	{
		state->kick_angles[0] = MSG_ReadChar (&net_message) * 0.25;
		state->kick_angles[1] = MSG_ReadChar (&net_message) * 0.25;
		state->kick_angles[2] = MSG_ReadChar (&net_message) * 0.25;
	}

	if (flags & PS_WEAPONINDEX)
	{
		state->gunindex = MSG_ReadByte (&net_message);
	}

	if (flags & PS_WEAPONFRAME)
	{
		state->gunframe = MSG_ReadShort (&net_message);
	}

	if (flags & PS_BLEND)
	{
		state->blend[0] = MSG_ReadByte (&net_message)/255.0;
		state->blend[1] = MSG_ReadByte (&net_message)/255.0;
		state->blend[2] = MSG_ReadByte (&net_message)/255.0;
		state->blend[3] = MSG_ReadByte (&net_message)/255.0;
	}

	if (flags & PS_FOV)
		state->fov = MSG_ReadByte (&net_message);

	// parse stats
	statbits = MSG_ReadLong (&net_message);
	for (i=0 ; i<MAX_STATS ; i++)
		if (statbits & (1<<i) )
			state->stats[i] = MSG_ReadShort(&net_message);
}

/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame (void)
{
	int			cmd;
	int			len;
	frame_t		*old;

	memset (&cl.frame, 0, sizeof(cl.frame));

	cl.frame.serverFrame = MSG_ReadLong (&net_message);
	cl.frame.deltaFrame = MSG_ReadLong (&net_message);
	cl.frame.serverTime = cl.frame.serverFrame*100;
	cl.suppressCount = MSG_ReadByte (&net_message);

	if (cl_shownet->integer == 3)
		Com_Printf ("   frame:%i  delta:%i\n", cl.frame.serverFrame,
		cl.frame.deltaFrame);

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if (cl.frame.deltaFrame <= 0)
	{
		cl.frame.valid = qtrue;		// uncompressed frame
		old = NULL;
		cls.demowaiting = qfalse;	// we can start recording now
	}
	else
	{
		old = &cl.frames[cl.frame.deltaFrame & UPDATE_MASK];
		if (!old->valid)
		{	// should never happen
			Com_Printf ("Delta from invalid frame (not supposed to happen!).\n");
		}
		if (old->serverFrame != cl.frame.deltaFrame)
		{	// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_Printf ("Delta frame too old.\n");
		}
		else if (cl.parse_entities - old->parseEntities > MAX_PARSE_ENTITIES-128)
		{
			Com_Printf ("Delta parse_entities too old.\n");
		}
		else
			cl.frame.valid = qtrue;	// valid delta parse
	}

	// read areabits
	len = MSG_ReadByte (&net_message);
	MSG_ReadData (&net_message, &cl.frame.areabits, len);

	// read playerinfo
	cmd = MSG_ReadByte (&net_message);
	SHOWNET(svc_strings[cmd]);
	if (cmd != svc_playerinfo)
		Com_Error (ERR_DROP, "CL_ParseFrame: not playerinfo");
	CL_ParsePlayerstate (old, &cl.frame);

	// read packet entities
	cmd = MSG_ReadByte (&net_message);
	SHOWNET(svc_strings[cmd]);
	if (cmd != svc_packetentities)
		Com_Error (ERR_DROP, "CL_ParseFrame: not packetentities");
	CL_ParsePacketEntities (old, &cl.frame);

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverFrame & UPDATE_MASK] = cl.frame;

	if (cl.frame.valid)
	{
		// getting a valid frame message ends the connection process
		if (cls.state != ca_active)
			CL_SetClientState (ca_active);
		cl.soundPrepped = qtrue;	// can start mixing ambient sounds
	}
}
