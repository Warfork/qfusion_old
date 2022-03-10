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

/*QUAKED target_temp_entity (1 0 0) (-8 -8 -8) (8 8 8)
Fire an origin based temp entity event to the clients.
"style"		type byte
*/
void Use_Target_Tent (edict_t *ent, edict_t *other, edict_t *activator)
{
	G_SpawnEvent ( ent->style, 0, ent->s.origin );
}

void SP_target_temp_entity (edict_t *ent)
{
	ent->use = Use_Target_Tent;
}


//==========================================================

//==========================================================

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off reliable
"noise"		wav file to play
"attenuation"
-1 = none, send to whole level
1 = normal fighting sounds
2 = idle sound level
3 = ambient sound level
"volume"	0.0 to 1.0

Normal sounds play each time the target is used.  The reliable flag can be set for crucial voiceovers.

Looped sounds are always atten 3 / vol 1, and the use function toggles it on/off.
Multiple identical looping sounds will just increase volume without any speed cost.
*/
void Use_Target_Speaker (edict_t *ent, edict_t *other, edict_t *activator)
{
	if (ent->spawnflags & 3)
	{	// looping sound toggles
		if (ent->s.sound)
			ent->s.sound = 0;	// turn it off
		else
			ent->s.sound = ent->noise_index;	// start it
	}
	else
	{	// normal sound
		if( ent->spawnflags & 8 )
			G_GlobalSound( CHAN_VOICE|CHAN_RELIABLE, ent->noise_index );
		// use a G_PositionedSound, because this entity won't normally be
		// sent to any clients because it is invisible
		else if( ent->spawnflags & 4 )
			G_PositionedSound( ent->s.origin, ent, CHAN_VOICE|CHAN_RELIABLE, ent->noise_index, ent->volume, ent->attenuation );
		else
			G_PositionedSound( ent->s.origin, ent, CHAN_VOICE, ent->noise_index, ent->volume, ent->attenuation );
	}
}

void SP_target_speaker (edict_t *ent)
{
	char	buffer[MAX_QPATH];

	if (!st.noise)
	{
		if (developer->integer)
			G_Printf ("target_speaker with no noise set at %s\n", vtos(ent->s.origin));
		return;
	}

	if (!strstr (st.noise, ".wav"))
		Q_snprintfz (buffer, sizeof(buffer), "%s.wav", st.noise);
	else
		Q_strncpyz (buffer, st.noise, sizeof(buffer));
	ent->noise_index = trap_SoundIndex (buffer);

	if (!ent->volume)
		ent->volume = 1.0;
	if (!ent->attenuation)
		ent->attenuation = ATTN_NORM;
	else if (ent->attenuation == -1)	// use -1 so 0 defaults to ATTN_NONE
		ent->attenuation = ATTN_NONE;

	// check for prestarted looping sound
	if (ent->spawnflags & 1)
		ent->s.sound = ent->noise_index;

	ent->use = Use_Target_Speaker;

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	trap_LinkEntity (ent);
}


//==========================================================

void Use_Target_Help (edict_t *ent, edict_t *other, edict_t *activator)
{
	if (ent->spawnflags & 1)
		Q_strncpyz (game.helpmessage1, ent->message, sizeof(game.helpmessage2));
	else
		Q_strncpyz (game.helpmessage2, ent->message, sizeof(game.helpmessage1));

	game.helpchanged++;
}

/*QUAKED target_help (1 0 1) (-16 -16 -24) (16 16 24) help1
When fired, the "message" key becomes the current personal computer string, and the message light will be set on all clients status bars.
*/
void SP_target_help(edict_t *ent)
{
	if (deathmatch->integer)
	{	// auto-remove for deathmatch
		G_FreeEdict (ent);
		return;
	}

	if (!ent->message)
	{
		if (developer->integer)
			G_Printf ("%s with no message at %s\n", ent->classname, vtos(ent->s.origin));
		G_FreeEdict (ent);
		return;
	}
	ent->use = Use_Target_Help;
}

//==========================================================

/*QUAKED target_secret (1 0 1) (-8 -8 -8) (8 8 8)
Counts a secret found.
These are single use targets.
*/
void use_target_secret (edict_t *ent, edict_t *other, edict_t *activator)
{
	G_Sound (ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM);

	level.found_secrets++;

	G_UseTargets (ent, activator);
	G_FreeEdict (ent);
}

void SP_target_secret (edict_t *ent)
{
	if (deathmatch->integer)
	{	// auto-remove for deathmatch
		G_FreeEdict (ent);
		return;
	}

	ent->use = use_target_secret;
	if (!st.noise)
		st.noise = "sound/misc/secret.wav";
	ent->noise_index = trap_SoundIndex (st.noise);
	ent->r.svflags = SVF_NOCLIENT;
	level.total_secrets++;
}

//==========================================================

/*QUAKED target_goal (1 0 1) (-8 -8 -8) (8 8 8)
Counts a goal completed.
These are single use targets.
*/
void use_target_goal (edict_t *ent, edict_t *other, edict_t *activator)
{
	G_Sound (ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM);

	level.found_goals++;

	G_UseTargets (ent, activator);
	G_FreeEdict (ent);
}

void SP_target_goal (edict_t *ent)
{
	if (deathmatch->integer)
	{	// auto-remove for deathmatch
		G_FreeEdict (ent);
		return;
	}

	ent->use = use_target_goal;
	if (!st.noise)
		st.noise = "sound/misc/secret.wav";
	ent->noise_index = trap_SoundIndex (st.noise);
	ent->r.svflags = SVF_NOCLIENT;
	level.total_goals++;
}

//==========================================================


/*QUAKED target_explosion (1 0 0) (-8 -8 -8) (8 8 8)
Spawns an explosion temporary entity when used.

"delay"		wait this long before going off
"dmg"		how much radius damage should be done, defaults to 0
*/
void target_explosion_explode (edict_t *self)
{
	float		save;
	
	G_SpawnEvent ( EV_EXPLOSION1, 0, self->s.origin );
	T_RadiusDamage (self, self->activator, self->dmg, NULL, self->dmg+40, MOD_EXPLOSIVE);

	save = self->delay;
	self->delay = 0;
	G_UseTargets (self, self->activator);
	self->delay = save;
}

void use_target_explosion (edict_t *self, edict_t *other, edict_t *activator)
{
	self->activator = activator;

	if (!self->delay)
	{
		target_explosion_explode (self);
		return;
	}

	self->think = target_explosion_explode;
	self->nextthink = level.time + self->delay;
}

void SP_target_explosion (edict_t *ent)
{
	ent->use = use_target_explosion;
	ent->r.svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_changelevel (1 0 0) (-8 -8 -8) (8 8 8)
Changes level to "map" when fired
*/
void use_target_changelevel (edict_t *self, edict_t *other, edict_t *activator)
{
	if (level.intermissiontime)
		return;		// already activated

	if (!deathmatch->integer && !coop->integer)
	{
		if (game.edicts[1].health <= 0)
			return;
	}

	// if noexit, do a ton of damage to other
	if (deathmatch->integer && !( dmflags->integer & DF_ALLOW_EXIT) && other != world)
	{
		T_Damage (other, self, self, vec3_origin, other->s.origin, vec3_origin, 10 * other->max_health, 1000, 0, MOD_EXIT);
		return;
	}

	// if multiplayer, let everyone know who hit the exit
	if (deathmatch->integer)
	{
		if (activator && activator->r.client)
			G_PrintMsg (NULL, PRINT_HIGH, "%s exited the level.\n", activator->r.client->pers.netname);
	}

	// if going to a new unit, clear cross triggers
	if (strstr(self->map, "*"))	
		game.serverflags &= ~(SFL_CROSS_TRIGGER_MASK);

	BeginIntermission (self);
}

void SP_target_changelevel (edict_t *ent)
{
	if (!ent->map)
	{
		if (developer->integer)
			G_Printf ("target_changelevel with no map at %s\n", vtos(ent->s.origin));
		G_FreeEdict (ent);
		return;
	}

	ent->use = use_target_changelevel;
	ent->r.svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_splash (1 0 0) (-8 -8 -8) (8 8 8)
Creates a particle splash effect when used.

"count"	how many pixels in the splash
"dmg"	if set, does a radius damage at this location when it splashes
		useful for lava/sparks
"color" color in r g b floating point format
*/

void use_target_splash (edict_t *self, edict_t *other, edict_t *activator)
{
	edict_t *event;

	event = G_SpawnEvent ( EV_LASER_SPARKS, DirToByte (self->movedir), self->s.origin );
	event->s.eventCount = self->count;

	// default to none
	if (VectorCompare (self->color, vec3_origin))
		event->s.colorRGBA = 0;
	else 
		event->s.colorRGBA = COLOR_RGBA (
		(int)(self->color[0]*255)&255,
		(int)(self->color[1]*255)&255,
		(int)(self->color[2]*255)&255, 
		255);

	if (self->dmg)
		T_RadiusDamage (self, activator, self->dmg, NULL, self->dmg+40, MOD_SPLASH);
}

void SP_target_splash (edict_t *self)
{
	self->use = use_target_splash;
	G_SetMovedir (self->s.angles, self->movedir);

	if (!self->count)
		self->count = 32;

	self->r.svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_spawner (1 0 0) (-8 -8 -8) (8 8 8)
Set target to the type of entity you want spawned.
Useful for spawning monsters and gibs in the factory levels.

For monsters:
	Set direction to the facing you want it to have.

For gibs:
	Set direction if you want it moving and
	speed how fast it should be moving otherwise it
	will just be dropped
*/
void ED_CallSpawn (edict_t *ent);

void use_target_spawner (edict_t *self, edict_t *other, edict_t *activator)
{
	edict_t	*ent;

	ent = G_Spawn();
	ent->classname = self->target;
	VectorCopy (self->s.origin, ent->s.origin);
	VectorCopy (self->s.angles, ent->s.angles);
	ED_CallSpawn (ent);
	trap_UnlinkEntity (ent);
	KillBox (ent);
	trap_LinkEntity (ent);
	if (self->speed)
		VectorCopy (self->movedir, ent->velocity);
}

void SP_target_spawner (edict_t *self)
{
	self->use = use_target_spawner;
	self->r.svflags = SVF_NOCLIENT;
	if (self->speed)
	{
		G_SetMovedir (self->s.angles, self->movedir);
		VectorScale (self->movedir, self->speed, self->movedir);
	}
}

//==========================================================

/*QUAKED target_blaster (1 0 0) (-8 -8 -8) (8 8 8) NOTRAIL NOEFFECTS
Fires a blaster bolt in the set direction when triggered.

dmg		default is 15
speed	default is 1000
*/

void use_target_blaster (edict_t *self, edict_t *other, edict_t *activator)
{
	int type;

	if (self->spawnflags & 2)
		type = ET_GENERIC;
	else if (self->spawnflags & 1)
		type = ET_HYPERBLASTER;
	else
		type = ET_BLASTER;

	fire_blaster (self, self->s.origin, self->movedir, self->dmg, self->speed, type, MOD_TARGET_BLASTER);
	G_Sound (self, CHAN_VOICE, self->noise_index, 1, ATTN_NORM);
}

void SP_target_blaster (edict_t *self)
{
	self->use = use_target_blaster;
	G_SetMovedir (self->s.angles, self->movedir);
	self->noise_index = trap_SoundIndex ("sound/weapons/laser2.wav");

	if (!self->dmg)
		self->dmg = 15;
	if (!self->speed)
		self->speed = 1000;

	self->r.svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_crosslevel_trigger (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Once this trigger is touched/used, any trigger_crosslevel_target with the same trigger number is automatically used when a level is started within the same unit.  It is OK to check multiple triggers.  Message, delay, target, and killtarget also work.
*/
void trigger_crosslevel_trigger_use (edict_t *self, edict_t *other, edict_t *activator)
{
	game.serverflags |= self->spawnflags;
	G_FreeEdict (self);
}

void SP_target_crosslevel_trigger (edict_t *self)
{
	self->r.svflags = SVF_NOCLIENT;
	self->use = trigger_crosslevel_trigger_use;
}

/*QUAKED target_crosslevel_target (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Triggered by a trigger_crosslevel elsewhere within a unit.  If multiple triggers are checked, all must be true.  Delay, target and
killtarget also work.

"delay"		delay before using targets if the trigger has been activated (default 1)
*/
void target_crosslevel_target_think (edict_t *self)
{
	if (self->spawnflags == (game.serverflags & SFL_CROSS_TRIGGER_MASK & self->spawnflags))
	{
		G_UseTargets (self, self);
		G_FreeEdict (self);
	}
}

void SP_target_crosslevel_target (edict_t *self)
{
	if (! self->delay)
		self->delay = 1;
	self->r.svflags = SVF_NOCLIENT;

	self->think = target_crosslevel_target_think;
	self->nextthink = level.time + self->delay;
}

//==========================================================

/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON RED GREEN BLUE YELLOW ORANGE FAT
When triggered, fires a laser.  You can either set a target
or a direction.
*/

void target_laser_think (edict_t *self)
{
	edict_t	*ignore;
	vec3_t	start;
	vec3_t	end;
	trace_t	tr;
	vec3_t	point;
	vec3_t	last_movedir;
	int		count;

	// our lifetime has expired
	if (self->delay && (self->wait < level.time))
	{
		if ( self->r.owner && self->r.owner->use ) {
			self->r.owner->use ( self->r.owner, self, self->activator );
		}

		G_FreeEdict (self);
		return;
	}

	if (self->spawnflags & 0x80000000)
		count = 8;
	else
		count = 4;

	if (self->enemy)
	{
		VectorCopy (self->movedir, last_movedir);
		VectorMA (self->enemy->r.absmin, 0.5, self->enemy->r.size, point);
		VectorSubtract (point, self->s.origin, self->movedir);
		VectorNormalize (self->movedir);
		if (!VectorCompare(self->movedir, last_movedir))
			self->spawnflags |= 0x80000000;
	}

	ignore = self;
	VectorCopy (self->s.origin, start);
	VectorMA (start, 2048, self->movedir, end);
	while (1)
	{
		trap_Trace (&tr, start, NULL, NULL, end, ignore, MASK_SHOT);
		if (tr.fraction == 1)
			break;

		// hurt it if we can
		if ((game.edicts[tr.ent].takedamage) && !(game.edicts[tr.ent].flags & FL_IMMUNE_LASER))
		{
			if (game.edicts[tr.ent].r.client && self->activator->r.client)
			{
				if (!ctf->integer || game.edicts[tr.ent].r.client->resp.ctf_team != self->activator->r.client->resp.ctf_team)
					T_Damage (&game.edicts[tr.ent], self, self->activator, self->movedir, tr.endpos, vec3_origin, self->dmg, 1, DAMAGE_ENERGY, self->count);
			}
			else
			{
				T_Damage (&game.edicts[tr.ent], self, self->activator, self->movedir, tr.endpos, vec3_origin, self->dmg, 1, DAMAGE_ENERGY, self->count);
			}
		}

		// if we hit something that's not a monster or player or is immune to lasers, we're done
		if (!(game.edicts[tr.ent].r.svflags & SVF_MONSTER) && (!game.edicts[tr.ent].r.client))
		{
			if (self->spawnflags & 0x80000000)
			{
				edict_t *event;

				self->spawnflags &= ~0x80000000;

				event = G_SpawnEvent ( EV_LASER_SPARKS, DirToByte (tr.plane.normal), tr.endpos );
				event->s.eventCount = count;
				event->s.colorRGBA = self->s.colorRGBA;
			}
			break;
		}

		ignore = &game.edicts[tr.ent];
		VectorCopy (tr.endpos, start);
	}

	VectorCopy (tr.endpos, self->s.origin2);

	self->nextthink = level.time + FRAMETIME;
}

void target_laser_on (edict_t *self)
{
	if (!self->activator)
		self->activator = self;
	self->spawnflags |= 0x80000001;
	self->r.svflags &= ~SVF_NOCLIENT;
	self->wait = level.time + self->delay;
	target_laser_think (self);
}

void target_laser_off (edict_t *self)
{
	self->spawnflags &= ~1;
	self->r.svflags |= SVF_NOCLIENT;
	self->nextthink = 0;
}

void target_laser_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->activator = activator;
	if (self->spawnflags & 1)
		target_laser_off (self);
	else
		target_laser_on (self);
}

void target_laser_start (edict_t *self)
{
	edict_t *ent;

	self->movetype = MOVETYPE_NONE;
	self->r.solid = SOLID_NOT;
	self->s.type = ET_BEAM;
	self->s.modelindex = 1;			// must be non-zero
	self->r.svflags = SVF_FORCEOLDORIGIN;

	// set the beam diameter
	if (self->spawnflags & 64)
		self->s.frame = 16;
	else
		self->s.frame = 4;

	// set the color
	if (self->spawnflags & 2)
		self->s.colorRGBA = COLOR_RGBA (220, 0, 0, 76);
	else if (self->spawnflags & 4)
		self->s.colorRGBA = COLOR_RGBA (0, 220, 0, 76);
	else if (self->spawnflags & 8)
		self->s.colorRGBA = COLOR_RGBA (0, 0, 220, 76);
	else if (self->spawnflags & 16)
		self->s.colorRGBA = COLOR_RGBA (220, 220, 0, 76);
	else if (self->spawnflags & 32)
		self->s.colorRGBA = COLOR_RGBA (255, 255, 0, 76);

	if (!self->enemy)
	{
		if (self->target)
		{
			ent = G_Find (NULL, FOFS(targetname), self->target);
			if (!ent)
				if (developer->integer)
					G_Printf ("%s at %s: %s is a bad target\n", self->classname, vtos(self->s.origin), self->target);
			self->enemy = ent;
		}
		else
		{
			G_SetMovedir (self->s.angles, self->movedir);
		}
	}
	self->use = target_laser_use;
	self->think = target_laser_think;

	if (!self->dmg)
		self->dmg = 1;

	VectorSet (self->r.mins, -8, -8, -8);
	VectorSet (self->r.maxs, 8, 8, 8);
	trap_LinkEntity (self);

	if (self->spawnflags & 1)
		target_laser_on (self);
	else
		target_laser_off (self);
}

void SP_target_laser (edict_t *self)
{
	// let everything else get spawned before we start firing
	self->think = target_laser_start;
	self->nextthink = level.time + 1;
	self->count = MOD_TARGET_LASER;
}

//==========================================================

/*QUAKED target_earthquake (1 0 0) (-8 -8 -8) (8 8 8)
When triggered, this initiates a level-wide earthquake.
All players and monsters are affected.
"speed"		severity of the quake (default:200)
"count"		duration of the quake (default:5)
*/

void target_earthquake_think (edict_t *self)
{
	int		i;
	edict_t	*e;

	if (self->last_move_time < level.time)
	{
		G_Sound (self, CHAN_AUTO, self->noise_index, 1.0, ATTN_NONE);
		self->last_move_time = level.time + 0.5;
	}

	for (i=1, e=game.edicts+i; i < game.numentities; i++,e++)
	{
		if (!e->r.inuse)
			continue;
		if (!e->r.client)
			continue;
		if (!e->groundentity)
			continue;

		e->groundentity = NULL;
		e->velocity[0] += crandom()* 150;
		e->velocity[1] += crandom()* 150;
		e->velocity[2] = self->speed * (100.0 / e->mass);
	}

	if (level.time < self->timestamp)
		self->nextthink = level.time + FRAMETIME;
}

void target_earthquake_use (edict_t *self, edict_t *other, edict_t *activator)
{
	self->timestamp = level.time + self->count;
	self->nextthink = level.time + FRAMETIME;
	self->activator = activator;
	self->last_move_time = 0;
}

void SP_target_earthquake (edict_t *self)
{
	if (!self->targetname)
		if (developer->integer)
			G_Printf ("untargeted %s at %s\n", self->classname, vtos(self->s.origin));

	if (!self->count)
		self->count = 5;

	if (!self->speed)
		self->speed = 200;

	self->r.svflags |= SVF_NOCLIENT;
	self->think = target_earthquake_think;
	self->use = target_earthquake_use;

	self->noise_index = trap_SoundIndex ("sound/world/quake.wav");
}

//==========================================================

/*QUAKED target_position (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for calculations in the utilities (spotlights, etc), but removed during gameplay.
*/
void SP_target_position (edict_t *self) 
{
	self->r.svflags |= SVF_NOCLIENT;
}

/*QUAKED target_location (1 0 0) (-8 -8 -8) (8 8 8)
*/
void SP_target_location (edict_t *self)
{
	self->r.svflags |= SVF_NOCLIENT;

	if ( self->count ) {
		if ( self->count < 0 ) {
			self->count = 0;
		} else if ( self->count > 7 ) {
			self->count = 7;
		}
	}
}

void SP_target_print_use (edict_t *self, edict_t *other, edict_t *activator)
{
	int n;
	edict_t *player;

	if (activator->r.client && (self->spawnflags & 4)) 
	{
		G_CenterPrintMsg ( activator, self->message );
		return;
	}

	// will add team-specific later...
	if ( self->spawnflags & 3 ) {
		return;
	}
		
	for (n = 1; n <= game.maxclients; n++)
	{
		player = &game.edicts[n];
		if (!player->r.inuse)
			continue;
		
		G_CenterPrintMsg ( player, self->message );
	}
}

void SP_target_print (edict_t *self)
{
	if ( !self->message ) {
		G_FreeEdict ( self );
		return;
	}

	self->use = SP_target_print_use;
	// do nothing
}
