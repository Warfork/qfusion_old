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


void InitTrigger (edict_t *self)
{
	// Vic
	if ( !VectorCompare (self->pos2, vec3_origin) ) {
		VectorCopy ( self->pos2, self->movedir );
		self->speed = 0.1;
	} else if ( !VectorCompare (self->s.angles, vec3_origin) ) {
		G_SetMovedir ( self->s.angles, self->movedir );
	}

	self->r.solid = SOLID_TRIGGER;
	self->movetype = MOVETYPE_NONE;
	trap_SetBrushModel (self, self->model);
	self->r.svflags = SVF_NOCLIENT;
}


// the wait time has passed, so set back up for another activation
void multi_wait (edict_t *ent)
{
	ent->nextthink = 0;
}


// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
void multi_trigger (edict_t *ent)
{
	if (ent->nextthink)
		return;		// already been triggered

	G_UseTargets (ent, ent->activator);

	if (ent->wait > 0)	
	{
		ent->think = multi_wait;
		ent->nextthink = level.time + ent->wait;
	}
	else
	{	// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->touch = NULL;
		ent->nextthink = level.time + FRAMETIME;
		ent->think = G_FreeEdict;
	}
}

void Use_Multi (edict_t *ent, edict_t *other, edict_t *activator)
{
	ent->activator = activator;
	multi_trigger (ent);
}

void Touch_Multi (edict_t *self, edict_t *other, cplane_t *plane, int surfFlags)
{
	if(other->r.client)
	{
		if (self->spawnflags & 2)
			return;
	}
	else if (other->r.svflags & SVF_MONSTER)
	{
		if (!(self->spawnflags & 1))
			return;
	}
	else
		return;

	if (!VectorCompare(self->movedir, vec3_origin))
	{
		vec3_t	forward;

		AngleVectors(other->s.angles, forward, NULL, NULL);
		if (DotProduct(forward, self->movedir) < 0)
			return;
	}

	self->activator = other;
	multi_trigger (self);
}

/*QUAKED trigger_multiple (.5 .5 .5) ? MONSTER NOT_PLAYER TRIGGERED
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
sounds
1)	secret
2)	beep beep
3)	large switch
4)
set "message" to text string
*/
void trigger_enable (edict_t *self, edict_t *other, edict_t *activator)
{
	self->r.solid = SOLID_TRIGGER;
	self->use = Use_Multi;
	trap_LinkEntity( self );
}

void SP_trigger_multiple (edict_t *ent)
{
	if (ent->sounds && ent->sounds[0])
	{
		if (ent->sounds[0] == '1')
			ent->noise_index = trap_SoundIndex ("sound/misc/secret.wav");
		else if (ent->sounds[0] == '2')
			ent->noise_index = trap_SoundIndex ("sound/misc/talk.wav");
		else if (ent->sounds[0] == '3')
			ent->noise_index = trap_SoundIndex ("sound/misc/trigger1.wav");
	}

	if (!ent->wait)
		ent->wait = 0.2;
	ent->touch = Touch_Multi;
	ent->movetype = MOVETYPE_NONE;
	ent->r.svflags |= SVF_NOCLIENT;

	if (ent->spawnflags & 4)
	{
		ent->r.solid = SOLID_NOT;
		ent->use = trigger_enable;
	}
	else
	{
		ent->r.solid = SOLID_TRIGGER;
		ent->use = Use_Multi;
	}

	if (!VectorCompare(ent->s.angles, vec3_origin))
		G_SetMovedir (ent->s.angles, ent->movedir);

	trap_SetBrushModel( ent, ent->model );
	trap_LinkEntity( ent );
}


/*QUAKED trigger_once (.5 .5 .5) ? x x TRIGGERED
Triggers once, then removes itself.
You must set the key "target" to the name of another object in the level that has a matching "targetname".

If TRIGGERED, this trigger must be triggered before it is live.

sounds
 1)	secret
 2)	beep beep
 3)	large switch
 4)

"message"	string to be displayed when triggered
*/

void SP_trigger_once(edict_t *ent)
{
	// make old maps work because I messed up on flag assignments here
	// triggered was on bit 1 when it should have been on bit 4
	if (ent->spawnflags & 1)
	{
		vec3_t	v;

		VectorMA (ent->r.mins, 0.5, ent->r.size, v);
		ent->spawnflags &= ~1;
		ent->spawnflags |= 4;

		if (developer->integer)
			G_Printf ("fixed TRIGGERED flag on %s at %s\n", ent->classname, vtos(v));
	}

	ent->wait = -1;
	SP_trigger_multiple (ent);
}

/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
This fixed size trigger cannot be touched, it can only be fired by other events.
*/
void trigger_relay_use (edict_t *self, edict_t *other, edict_t *activator)
{
	G_UseTargets (self, activator);
}

void SP_trigger_relay (edict_t *self)
{
	self->use = trigger_relay_use;
}

/*
==============================================================================

trigger_counter

==============================================================================
*/

/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.

If nomessage is not set, t will print "1 more.. " etc when triggered and "sequence complete" when finished.

After the counter has been triggered "count" times (default 2), it will fire all of its targets and remove itself.
*/

void trigger_counter_use(edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->count == 0)
		return;
	
	self->count--;

	if (self->count)
	{
		if (! (self->spawnflags & 1))
		{
			G_CenterPrintMsg (activator, "%i more to go...", self->count);
			G_Sound (activator, CHAN_AUTO, trap_SoundIndex ("sound/misc/talk1.wav"), 1, ATTN_NORM);
		}
		return;
	}
	
	if (! (self->spawnflags & 1))
	{
		G_CenterPrintMsg (activator, "Sequence completed!");
		G_Sound (activator, CHAN_AUTO, trap_SoundIndex ("sound/misc/talk1.wav"), 1, ATTN_NORM);
	}
	self->activator = activator;
	multi_trigger (self);
}

void SP_trigger_counter (edict_t *self)
{
	self->wait = -1;
	if (!self->count)
		self->count = 2;

	self->use = trigger_counter_use;
}


/*
==============================================================================

trigger_always

==============================================================================
*/

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always (edict_t *ent)
{
	// we must have some delay to make sure our use targets are present
	if (ent->delay < 0.2)
		ent->delay = 0.2;
	G_UseTargets(ent, ent);
}


/*
==============================================================================

trigger_push

==============================================================================
*/

#define PUSH_ONCE		1

void trigger_push_touch (edict_t *self, edict_t *other, cplane_t *plane, int surfFlags)
{
	edict_t *event;
	float time, dist, f;
	vec3_t origin, velocity;

	if( self->wait >= level.time )
		return;
	if( !other->r.client )
		return;
	if( other->r.client->ps.pmove.pm_type != PM_NORMAL )
		return;

	VectorAdd( self->r.absmin, self->r.absmax, origin );
	VectorScale ( origin, 0.5, origin );
	time = sqrt ((self->movetarget->s.origin[2] - origin[2]) / (0.5 * sv_gravity->value));
	if (!time)
		goto remove;

	VectorSubtract ( self->movetarget->s.origin, origin, velocity );
	velocity[2] = 0;
	dist = VectorNormalize ( velocity );

	f = dist / time;
	VectorScale (velocity, f, velocity);
	velocity[2] = time * sv_gravity->value;

	CTFPlayerResetGrapple ( other );

	other->r.client->jumppad_time = level.time;
	VectorCopy ( velocity, other->velocity );

	// don't take falling damage immediately from this
	VectorCopy ( other->velocity, other->r.client->oldvelocity );

	// add an event
	event = G_SpawnEvent( EV_JUMP_PAD, 0, other->s.origin );
	event->r.svflags = SVF_NOOLDORIGIN;
	event->s.ownerNum = other - game.edicts;
	event->s.targetNum = self - game.edicts;

	if ( !(self->spawnflags & PUSH_ONCE) )
	{
		self->wait = level.time + 2*FRAMETIME;
		return;
	}

remove:
	// we can't just remove (self) here, because this is a touch function
	// called while looping through area links...
	self->touch = NULL;
	self->nextthink = level.time + FRAMETIME;
	self->think = G_FreeEdict;
}

void S_trigger_push_think (edict_t *ent)
{
	ent->movetarget = G_PickTarget (ent->target);
	if (!ent->movetarget)
		G_FreeEdict (ent);

}

/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE
Pushes the player
*/
void SP_trigger_push (edict_t *self)
{
	InitTrigger( self );

	trap_SoundIndex( "sound/world/jumppad.wav" );

	self->touch = trigger_push_touch;
	self->think = S_trigger_push_think;
	self->nextthink = level.time + FRAMETIME;
	self->r.svflags &= ~SVF_NOCLIENT;
	self->s.type = ET_PUSH_TRIGGER;

	trap_LinkEntity( self );
}


/*
==============================================================================

trigger_hurt

==============================================================================
*/

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF TOGGLE SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.

It does dmg points of damage each server frame

SILENT			suppresses playing the sound
SLOW			changes the damage rate to once per second
NO_PROTECTION	*nothing* stops the damage

"dmg"			default 5 (whole numbers only)

*/
void hurt_use (edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->r.solid == SOLID_NOT)
		self->r.solid = SOLID_TRIGGER;
	else
		self->r.solid = SOLID_NOT;
	trap_LinkEntity (self);

	if (!(self->spawnflags & 2))
		self->use = NULL;
}


void hurt_touch (edict_t *self, edict_t *other, cplane_t *plane, int surfFlags)
{
	int		dflags;

	if (!other->takedamage)
		return;
	if (self->timestamp > level.time)
		return;

	if (self->spawnflags & 16)
		self->timestamp = level.time + 1;
	else
		self->timestamp = level.time + FRAMETIME;

	if (!(self->spawnflags & 4))
	{
		if ((level.framenum % 10) == 0)
			G_Sound (other, CHAN_AUTO, self->noise_index, 1, ATTN_NORM);
	}

	if (self->spawnflags & 8)
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = 0;
	T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, self->dmg, dflags, MOD_TRIGGER_HURT);
}

void SP_trigger_hurt (edict_t *self)
{
	InitTrigger (self);

	self->noise_index = trap_SoundIndex ("sound/world/electro.wav");
	self->touch = hurt_touch;

	if (!self->dmg)
		self->dmg = 5;

	if (self->spawnflags & 1)
		self->r.solid = SOLID_NOT;
	else
		self->r.solid = SOLID_TRIGGER;

	if (self->spawnflags & 2)
		self->use = hurt_use;

	trap_LinkEntity (self);
}


/*
==============================================================================

trigger_gravity

==============================================================================
*/

/*QUAKED trigger_gravity (.5 .5 .5) ?
Changes the touching entites gravity to
the value of "gravity".  1.0 is standard
gravity for the level.
*/

void trigger_gravity_touch (edict_t *self, edict_t *other, cplane_t *plane, int surfFlags)
{
	other->gravity = self->gravity;
}

void SP_trigger_gravity (edict_t *self)
{
	if (st.gravity == 0)
	{
		if (developer->integer)
			G_Printf ("trigger_gravity without gravity set at %s\n", vtos(self->s.origin));
		G_FreeEdict  (self);
		return;
	}

	InitTrigger (self);
	self->gravity = atoi(st.gravity);
	self->touch = trigger_gravity_touch;
}


/*
==============================================================================

trigger_monsterjump

==============================================================================
*/

/*QUAKED trigger_monsterjump (.5 .5 .5) ?
Walking monsters that touch this will jump in the direction of the trigger's angle
"speed" default to 200, the speed thrown forward
"height" default to 200, the speed thrown upwards
*/

void trigger_monsterjump_touch (edict_t *self, edict_t *other, cplane_t *plane, int surfFlags)
{
	if (other->flags & (FL_FLY | FL_SWIM) )
		return;
	if (other->r.svflags & SVF_CORPSE)
		return;
	if ( !(other->r.svflags & SVF_MONSTER))
		return;

// set XY even if not on ground, so the jump will clear lips
	other->velocity[0] = self->movedir[0] * self->speed;
	other->velocity[1] = self->movedir[1] * self->speed;
	
	if (!other->groundentity)
		return;
	
	other->groundentity = NULL;
	other->velocity[2] = self->movedir[2];
}

void SP_trigger_monsterjump (edict_t *self)
{
	if (!self->speed)
		self->speed = 200;
	if (!st.height)
		st.height = 200;
	if (self->s.angles[YAW] == 0)
		self->s.angles[YAW] = 360;
	InitTrigger (self);
	self->touch = trigger_monsterjump_touch;
	self->movedir[2] = st.height;
}

/*--------------------------------------------------------------------------
 * just here to help old map conversions
 *--------------------------------------------------------------------------*/

static void old_teleporter_touch (edict_t *self, edict_t *other, cplane_t *plane, int surfFlags)
{
	edict_t		*dest;
	edict_t		*event;
	int			i;
	vec3_t		forward;

	if ( !other->r.client ) {
		return;
	} 
	if ( self->spawnflags & 1 ) {
		if ( other->r.client->ps.pmove.pm_type != PM_SPECTATOR )
			return;
	} else {
		if ( other->r.client->ps.pmove.pm_type == PM_DEAD )
			return;
	}

	dest = G_Find (NULL, FOFS(targetname), self->target);

	if (!dest)
	{
		if (developer->integer)
			G_Printf ("Couldn't find destination.\n");
		return;
	}

//ZOID
	CTFPlayerResetGrapple(other);
//ZOID

	// unlink to make sure it can't possibly interfere with KillBox
	trap_UnlinkEntity (other);

	VectorCopy (dest->s.origin, other->s.origin);
	VectorCopy (dest->s.origin, other->s.old_origin);
	G_AddEvent (other, EV_TELEPORT, 0, qtrue);

	// clear the velocity and hold them in place briefly
	VectorClear (other->velocity);
	other->r.client->ps.pmove.pm_time = 160>>3;		// hold time
	other->r.client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;

	// draw the teleport splash at source and on the player
	event = G_SpawnEvent ( EV_PLAYER_TELEPORT_OUT, 0, self->s.origin );
	event->r.svflags = SVF_NOOLDORIGIN;
	event->s.ownerNum = other - game.edicts;

	event = G_SpawnEvent ( EV_PLAYER_TELEPORT_IN, 0, other->s.origin );
	event->r.svflags = SVF_NOOLDORIGIN;
	event->s.ownerNum = other - game.edicts;

	// set angles
	other->s.angles[PITCH] = 0;
	other->s.angles[YAW] = dest->s.angles[YAW];
	other->s.angles[ROLL] = 0;
	VectorCopy (dest->s.angles, other->r.client->ps.viewangles);
	VectorCopy (dest->s.angles, other->r.client->v_angle);

	// set the delta angle
	for (i=0 ; i<3 ; i++)
		other->r.client->ps.pmove.delta_angles[i] = ANGLE2SHORT(other->s.angles[i] - other->r.client->resp.cmd_angles[i]);

	// give a little forward velocity
	AngleVectors (other->r.client->v_angle, forward, NULL, NULL);
	VectorScale(forward, 200, other->velocity);

	// kill anything at the destination
	if (!KillBox (other))
	{
	}

	trap_LinkEntity (other);
}

/*QUAKED trigger_teleport (0.5 0.5 0.5) ?
Players touching this will be teleported
*/
void SP_trigger_teleport (edict_t *ent)
{
	if (!ent->target)
	{
		if (developer->integer)
			G_Printf ( "teleporter without a target.\n" );
		G_FreeEdict ( ent );
		return;
	}

	InitTrigger ( ent );
	ent->touch = old_teleporter_touch;
}

/*QUAKED info_teleport_destination (0.5 0.5 0.5) (-16 -16 -24) (16 16 32)
Point trigger_teleports at these.
*/
void SP_info_teleport_destination (edict_t *ent)
{
	ent->s.origin[2] += 16;
}

