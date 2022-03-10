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

#include "cg_local.h"

int cg_numSolids;
entity_state_t *cg_solidList[MAX_PARSE_ENTITIES];

/*
===================
CG_CheckPredictionError
===================
*/
void CG_CheckPredictionError (void)
{
	int		frame;
	int		delta[3];
	int		incomingAcknowledged, outgoingSequence;

	if ( !cg_predict->value || (cg.frame.playerState.pmove.pm_flags & PMF_NO_PREDICTION) ) {
		return;
	}

	trap_NET_GetCurrentState ( &incomingAcknowledged, &outgoingSequence );

	// calculate the last usercmd_t we sent that the server has processed
	frame = incomingAcknowledged & CMD_MASK;

	// compare what the server returned with what we had predicted it to be
	VectorSubtract ( cg.frame.playerState.pmove.origin, cg.predictedOrigins[frame], delta );

	// save the prediction error for interpolation
	if ( abs(delta[0]) > 128*16 || abs(delta[1]) > 128*16 || abs(delta[2]) > 128*16 ) {
		VectorClear ( cg.predictionError );					// a teleport or something
	} else {
		if ( cg_showMiss->value && (delta[0] || delta[1] || delta[2]) ) {
			Com_Printf ("prediction miss on %i: %i\n", cg.frame.serverFrame, delta[0] + delta[1] + delta[2]);
		}

		VectorCopy ( cg.frame.playerState.pmove.origin, cg.predictedOrigins[frame] );
		VectorScale ( delta, (1.0/16.0), cg.predictionError );	// save for error interpolation
	}
}

/*
====================
CG_BuildSolidList
====================
*/
void CG_BuildSolidList (void)
{
	int	i, num;
	entity_state_t *ent;

	cg_numSolids = 0;
	for ( i = 0; i < cg.frame.numEntities; i++ )
	{
		num = (cg.frame.parseEntities + i) & (MAX_PARSE_ENTITIES-1);
		ent = &cg_parseEntities[num];

		if ( !ent->solid || (ent->effects & EF_PORTALENTITY) ) {
			continue;
		}

		cg_solidList[cg_numSolids++] = ent;
	}
}

/*
====================
CG_ClipMoveToEntities
====================
*/
void CG_ClipMoveToEntities ( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignore, int contentmask, trace_t *tr )
{
	int			i, x, zd, zu;
	trace_t		trace;
	vec3_t		origin, angles;
	entity_state_t	*ent;
	struct cmodel_s	*cmodel;
	vec3_t		bmins, bmaxs;

	for ( i = 0; i < cg_numSolids; i++ )
	{
		ent = cg_solidList[i];

		if ( ent->number == ignore ) {
			continue;
		}
		if ( !(contentmask & CONTENTS_CORPSE) && (ent->effects & EF_CORPSE) )
			continue;

		if ( ent->solid == SOLID_BMODEL ) {	// special value for bmodel
			cmodel = trap_CM_InlineModel ( ent->modelindex );
			if ( !cmodel ) {
				continue;
			}

			VectorCopy ( ent->origin, origin );
			VectorCopy ( ent->angles, angles );
		} else {							// encoded bbox
			x = 8 * (ent->solid & 31);
			zd = 8 * ((ent->solid>>5) & 31);
			zu = 8 * ((ent->solid>>10) & 63) - 32;

			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;

			cmodel = trap_CM_ModelForBBox ( bmins, bmaxs );
			VectorCopy ( ent->origin, origin );
			VectorClear ( angles );	// boxes don't rotate
		}

		trap_CM_TransformedBoxTrace ( &trace, start, end, mins, maxs, cmodel, contentmask, origin, angles );
		if ( trace.allsolid || trace.fraction < tr->fraction ) {
			trace.ent = ent->number;
			*tr = trace;
		} else if ( trace.startsolid ) {
			tr->startsolid = qtrue;
		}
		if ( tr->allsolid ) {
			return;
		}
	}
}


/*
================
CG_Trace
================
*/
void CG_Trace ( trace_t *t, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignore, int contentmask )
{
	// check against world
	trap_CM_BoxTrace ( t, start, end, mins, maxs, NULL, contentmask );
	t->ent = 0;
	if ( t->fraction == 0 ) {
		return;			// blocked by the world
	}

	// check all other solid models
	CG_ClipMoveToEntities ( start, mins, maxs, end, ignore, contentmask, t );
}


/*
================
CG_PMTrace
================
*/
void CG_PMTrace ( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end ) {
	CG_Trace ( tr, start, mins, maxs, end, cgs.playerNum+1, MASK_PLAYERSOLID );
}

/*
================
CG_PointContents
================
*/
int	CG_PointContents ( vec3_t point )
{
	int			i;
	entity_state_t	*ent;
	struct cmodel_s	*cmodel;
	int			contents;

	contents = trap_CM_PointContents ( point, NULL );

	for ( i = 0; i < cg_numSolids; i++ )
	{
		ent = cg_solidList[i];

		if ( ent->solid != SOLID_BMODEL ) { // special value for bmodel
			continue;
		}

		cmodel = trap_CM_InlineModel ( ent->modelindex );
		if ( !cmodel ) {
			continue;
		}

		contents |= trap_CM_TransformedPointContents ( point, cmodel, ent->origin, ent->angles );
	}

	return contents;
}


/*
=================
CG_PredictMovement

Sets cg.predictedOrigin and cg.predictedAngles
=================
*/
void CG_PredictMovement (void)
{
	int			ack, current;
	int			frame;
	int			oldframe;
	usercmd_t	cmd;
	pmove_t		pm;
	int			i;
	int			step;
	float		oldstep;
	int			oldz;

	if ( cg_paused->value ) {
		return;
	}

	if ( !cg_predict->value || (cg.frame.playerState.pmove.pm_flags & PMF_NO_PREDICTION) ) {
		// just set angles
		trap_NET_GetCurrentUserCommand ( &cmd );

		for ( i = 0; i < 3; i++ ) {
			cg.predictedAngles[i] = SHORT2ANGLE (cmd.angles[i]) + SHORT2ANGLE(cg.frame.playerState.pmove.delta_angles[i]);
		}
		return;
	}

	trap_NET_GetCurrentState ( &ack, &current );

	// if we are too far out of date, just freeze
	if ( current - ack >= CMD_BACKUP ) {
		if ( cg_showMiss->value ) {
			CG_Printf ( "exceeded CMD_BACKUP\n" );
		}
		return;	
	}

	// copy current state to pmove
	memset ( &pm, 0, sizeof(pm) );
	pm.trace = CG_PMTrace;
	pm.pointcontents = CG_PointContents;
	pm.airaccelerate = atof( cgs.configStrings[CS_AIRACCEL] );
	pm.s = cg.frame.playerState.pmove;
	if (cgs.attractLoop)
		pm.s.pm_type = PM_FREEZE;		// demo playback

	// run frames
	while ( ++ack < current )
	{
		frame = ack & CMD_MASK;
		trap_NET_GetUserCommand ( frame, &pm.cmd );

		Pmove ( &pm );

		// save for debug checking
		VectorCopy ( pm.s.origin, cg.predictedOrigins[frame] );
	}

	oldframe = (ack-2) & CMD_MASK;
	oldz = cg.predictedOrigins[oldframe][2];
	step = pm.s.origin[2] - oldz;

	if ( pm.step && step > 0 && step < 320 ) {
		if ( cg.realTime - cg.predictedStepTime < 150 ) {
			oldstep = cg.predictedStep * (150 - (cg.realTime - cg.predictedStepTime)) * (1.0 / 150.0);
		} else {
			oldstep = 0;
		}

		cg.predictedStep = oldstep + step * (1.0/16.0);
		cg.predictedStepTime = cg.realTime - cg.frameTime * 500;
	}

	// copy results out for rendering
	VectorScale ( pm.s.origin, (1.0/16.0), cg.predictedOrigin );
	VectorCopy ( pm.viewangles, cg.predictedAngles );
}

