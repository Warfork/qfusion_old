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


void UpdateChaseCam(edict_t *ent)
{
	vec3_t o, ownerv, goal;
	edict_t *targ;
	vec3_t forward, right;
	trace_t trace;
	int i;
	vec3_t oldgoal;
	vec3_t angles;

	// is our chase target gone?
	if (!ent->r.client->chase_target->r.inuse) {
		ent->r.client->chase_target = NULL;
		return;
	}

	targ = ent->r.client->chase_target;

	VectorCopy(targ->s.origin, ownerv);
	VectorCopy(ent->s.origin, oldgoal);

	ownerv[2] += targ->viewheight;

	VectorCopy(targ->r.client->v_angle, angles);
	if (angles[PITCH] > 56)
		angles[PITCH] = 56;
	AngleVectors (angles, forward, right, NULL);
	VectorNormalize(forward);
	VectorMA(ownerv, -30, forward, o);

	if (o[2] < targ->s.origin[2] + 20)
		o[2] = targ->s.origin[2] + 20;

	// jump animation lifts
	if (!targ->groundentity)
		o[2] += 16;

	trap_Trace (&trace, ownerv, vec3_origin, vec3_origin, o, targ, MASK_SOLID);

	VectorCopy (trace.endpos, goal);

	VectorMA (goal, 2, forward, goal);

	// pad for floors and ceilings
	VectorCopy(goal, o);
	o[2] += 6;
	trap_Trace (&trace, goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
	if (trace.fraction < 1) {
		VectorCopy(trace.endpos, goal);
		goal[2] -= 6;
	}

	VectorCopy(goal, o);
	o[2] -= 6;
	trap_Trace (&trace, goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
	if (trace.fraction < 1) {
		VectorCopy(trace.endpos, goal);
		goal[2] += 6;
	}

	ent->r.client->ps.pmove.pm_type = PM_FREEZE;

	VectorCopy(goal, ent->s.origin);
	for (i=0 ; i<3 ; i++)
		ent->r.client->ps.pmove.delta_angles[i] = ANGLE2SHORT(targ->r.client->v_angle[i] - ent->r.client->resp.cmd_angles[i]);

	VectorCopy(targ->r.client->v_angle, ent->r.client->ps.viewangles);
	VectorCopy(targ->r.client->v_angle, ent->r.client->v_angle);

	ent->viewheight = 0;
	ent->r.client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
	trap_LinkEntity (ent);

	if ((!ent->r.client->showscores && !ent->r.client->menu &&
		!ent->r.client->showinventory && !ent->r.client->showhelp &&
		!(level.framenum & 31)) || ent->r.client->update_chase) {
		char s[MAX_STRING_CHARS];

		ent->r.client->update_chase = qfalse;
		Q_snprintfz(s, sizeof(s), "xv 0 yb -68 string \"%sChasing %s\"",
			S_COLOR_YELLOW, targ->r.client->pers.netname);
		trap_Layout (ent, s);
	}
}

void ChaseNext(edict_t *ent)
{
	int i;
	edict_t *e;

	if (!ent->r.client->chase_target)
		return;

	i = ent->r.client->chase_target - game.edicts;
	do {
		i++;
		if (i > game.maxclients)
			i = 1;
		e = game.edicts + i;
		if (!e->r.inuse)
			continue;
		if (e->r.solid != SOLID_NOT)
			break;
	} while (e != ent->r.client->chase_target);

	ent->r.client->chase_target = e;
	ent->r.client->update_chase = qtrue;
}

void ChasePrev(edict_t *ent)
{
	int i;
	edict_t *e;

	if (!ent->r.client->chase_target)
		return;

	i = ent->r.client->chase_target - game.edicts;
	do {
		i--;
		if (i < 1)
			i = game.maxclients;
		e = game.edicts + i;
		if (!e->r.inuse)
			continue;
		if (e->r.solid != SOLID_NOT)
			break;
	} while (e != ent->r.client->chase_target);

	ent->r.client->chase_target = e;
	ent->r.client->update_chase = qtrue;
}
