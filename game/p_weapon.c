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
// g_weapon.c

#include "g_local.h"
#include "m_player.h"


static qboolean	is_quad;
static qbyte	is_silenced;


void weapon_grenade_fire (edict_t *ent, qboolean held);


void P_ProjectSource (gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
	vec3_t	_distance;

	VectorCopy (distance, _distance);
	if (client->pers.hand == LEFT_HANDED)
		_distance[1] *= -1;
	else if (client->pers.hand == CENTER_HANDED)
		_distance[1] = 0;
	G_ProjectSource (point, _distance, forward, right, result);
}


/*
===============
PlayerNoise

Each player can have two noise objects associated with it:
a personal noise (jumping, pain, weapon firing), and a weapon
target noise (bullet wall impacts)

Monsters that don't directly see the player can move
to a noise in hopes of seeing the player from there.
===============
*/
void PlayerNoise(edict_t *who, vec3_t where, int type)
{
	edict_t		*noise;

	if (type == PNOISE_WEAPON)
	{
		if (who->r.client->silencer_shots)
		{
			who->r.client->silencer_shots--;
			return;
		}
	}

	if (deathmatch->integer)
		return;

	if (who->flags & FL_NOTARGET)
		return;


	if (!who->mynoise)
	{
		noise = G_Spawn();
		noise->classname = "player_noise";
		VectorSet (noise->r.mins, -8, -8, -8);
		VectorSet (noise->r.maxs, 8, 8, 8);
		noise->r.owner = who;
		noise->r.svflags = SVF_NOCLIENT;
		who->mynoise = noise;

		noise = G_Spawn();
		noise->classname = "player_noise";
		VectorSet (noise->r.mins, -8, -8, -8);
		VectorSet (noise->r.maxs, 8, 8, 8);
		noise->r.owner = who;
		noise->r.svflags = SVF_NOCLIENT;
		who->mynoise2 = noise;
	}

	if (type == PNOISE_SELF || type == PNOISE_WEAPON)
	{
		noise = who->mynoise;
		level.sound_entity = noise;
		level.sound_entity_framenum = level.framenum;
	}
	else // type == PNOISE_IMPACT
	{
		noise = who->mynoise2;
		level.sound2_entity = noise;
		level.sound2_entity_framenum = level.framenum;
	}

	VectorCopy (where, noise->s.origin);
	VectorSubtract (where, noise->r.maxs, noise->r.absmin);
	VectorAdd (where, noise->r.maxs, noise->r.absmax);
	noise->teleport_time = level.time;
	trap_LinkEntity (noise);
}


qboolean Pickup_Weapon (edict_t *ent, edict_t *other)
{
	int			index;
	gitem_t		*ammo;

	index = ITEM_INDEX(ent->item);

	if ( ( (dmflags->integer & DF_WEAPONS_STAY) || coop->integer) 
		&& other->r.client->pers.inventory[index])
	{
		if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM) ) )
			return qfalse;	// leave the weapon for others to pickup
	}

	other->r.client->pers.inventory[index]++;

	if (!(ent->spawnflags & DROPPED_ITEM) )
	{
		// give them some ammo with it
		ammo = FindItem (ent->item->ammo);
		if ( dmflags->integer & DF_INFINITE_AMMO )
			Add_Ammo (other, ammo, 1000);
		else
			Add_Ammo (other, ammo, ammo->quantity);

		if (! (ent->spawnflags & DROPPED_PLAYER_ITEM) )
		{
			if (deathmatch->integer)
			{
				if (dmflags->integer & DF_WEAPONS_STAY)
					ent->flags |= FL_RESPAWN;
				else
					SetRespawn (ent, 30);
			}
			if (coop->integer)
				ent->flags |= FL_RESPAWN;
		}
	}

	if (other->r.client->pers.weapon != ent->item && 
		(other->r.client->pers.inventory[index] == 1) &&
		( !deathmatch->integer || other->r.client->pers.weapon == FindItem("blaster") ) )
		other->r.client->newweapon = ent->item;

	return qtrue;
}


/*
===============
ChangeWeapon

The old weapon has been dropped all the way, so make the new one
current
===============
*/
void ChangeWeapon (edict_t *ent)
{
	gclient_t *client = ent->r.client;

	if (client->grenade_time)
	{
		client->grenade_time = level.time;
		client->weapon_sound = 0;
		weapon_grenade_fire (ent, qfalse);
		client->grenade_time = 0;
	}

	client->pers.lastweapon = client->pers.weapon;
	client->pers.weapon = client->newweapon;
	client->newweapon = NULL;
	client->machinegun_shots = 0;

	if (client->pers.weapon)
		ent->s.weapon = client->pers.weapon->weapmodel & 0xff;

	if (client->pers.weapon && client->pers.weapon->ammo)
		client->ammo_index = ITEM_INDEX(FindItem(client->pers.weapon->ammo));
	else
		client->ammo_index = 0;

	if (!client->pers.weapon)
	{	// dead
		client->ps.gunindex = 0;
		return;
	}

	if ( level.time >= ent->pain_debounce_time ) {
		if ( !client->noammo ) {
			G_Sound ( ent, CHAN_VOICE, trap_SoundIndex( "sound/weapons/change.wav" ), 1, ATTN_NORM );
		} else {
			G_Sound ( ent, CHAN_VOICE, trap_SoundIndex( "sound/weapons/noammo.wav" ), 1, ATTN_NORM );
		}

		ent->pain_debounce_time = level.time + 1;
	}

	client->noammo = qfalse;
	client->weaponstate = WEAPON_ACTIVATING;
	client->ps.gunframe = 0;
	client->ps.gunindex = trap_ModelIndex (client->pers.weapon->view_model);

	client->anim_priority = ANIM_PAIN;

	if (client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		ent->s.frame = FRAME_crpain1;
		client->anim_end = FRAME_crpain4;
	}
	else
	{
		ent->s.frame = FRAME_pain301;
		client->anim_end = FRAME_pain304;
	}
}

/*
=================
NoAmmoWeaponChange
=================
*/
void NoAmmoWeaponChange (gclient_t *client)
{
	client->noammo = qtrue;

	if ( client->pers.inventory[ITEM_INDEX(FindItem("slugs"))]
		&& client->pers.inventory[ITEM_INDEX(FindItem("railgun"))] )
	{
		client->newweapon = FindItem ("railgun");
		return;
	}
	if ( client->pers.inventory[ITEM_INDEX(FindItem("cells"))]
		&& client->pers.inventory[ITEM_INDEX(FindItem("plasma gun"))] )
	{
		client->newweapon = FindItem ("plasma gun");
		return;
	}
	if ( client->pers.inventory[ITEM_INDEX(FindItem("bullets"))]
		&& client->pers.inventory[ITEM_INDEX(FindItem("chaingun"))] )
	{
		client->newweapon = FindItem ("chaingun");
		return;
	}
	if ( client->pers.inventory[ITEM_INDEX(FindItem("bullets"))]
		&& client->pers.inventory[ITEM_INDEX(FindItem("machinegun"))] )
	{
		client->newweapon = FindItem ("machinegun");
		return;
	}
	if ( client->pers.inventory[ITEM_INDEX(FindItem("shells"))] > 1
		&& client->pers.inventory[ITEM_INDEX(FindItem("super shotgun"))] )
	{
		client->newweapon = FindItem ("super shotgun");
		return;
	}
	if ( client->pers.inventory[ITEM_INDEX(FindItem("shells"))]
		&& client->pers.inventory[ITEM_INDEX(FindItem("shotgun"))] )
	{
		client->newweapon = FindItem ("shotgun");
		return;
	}

	client->newweapon = FindItem ("blaster");
}

/*
=================
Think_Weapon

Called by ClientBeginServerFrame and ClientThink
=================
*/
void Think_Weapon (edict_t *ent)
{
	// if just died, put the weapon away
	if (ent->health < 1)
	{
		ent->r.client->newweapon = NULL;
		ChangeWeapon (ent);
	}

	// call active weapon think routine
	if (ent->r.client->pers.weapon && ent->r.client->pers.weapon->weaponthink)
	{
		is_quad = (ent->r.client->quad_framenum > level.framenum);
		if (ent->r.client->silencer_shots)
			is_silenced = MZ_SILENCED;
		else
			is_silenced = 0;
		ent->r.client->pers.weapon->weaponthink (ent);
	}
}


/*
================
Use_Weapon

Make the weapon ready if there is ammo
================
*/
void Use_Weapon (edict_t *ent, gitem_t *item)
{
	int			ammo_index;
	gitem_t		*ammo_item;

	// see if we're already using it
	if (item == ent->r.client->pers.weapon)
		return;

	if (item->ammo && !g_select_empty->integer && !(item->flags & IT_AMMO))
	{
		ammo_item = FindItem(item->ammo);
		ammo_index = ITEM_INDEX(ammo_item);

		if (!ent->r.client->pers.inventory[ammo_index])
		{
			G_PrintMsg (ent, PRINT_HIGH, "No %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
			return;
		}

		if (ent->r.client->pers.inventory[ammo_index] < item->quantity)
		{
			G_PrintMsg (ent, PRINT_HIGH, "Not enough %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
			return;
		}
	}

	// change to this weapon when down
	ent->r.client->newweapon = item;
}



/*
================
Drop_Weapon
================
*/
void Drop_Weapon (edict_t *ent, gitem_t *item)
{
	int		index;

	if (dmflags->integer & DF_WEAPONS_STAY)
		return;

	index = ITEM_INDEX(item);
	// see if we're already using it
	if ( ((item == ent->r.client->pers.weapon) || (item == ent->r.client->newweapon))&& (ent->r.client->pers.inventory[index] == 1) )
	{
		G_PrintMsg (ent, PRINT_HIGH, "Can't drop current weapon\n");
		return;
	}

	Drop_Item (ent, item);
	ent->r.client->pers.inventory[index]--;
}


/*
================
Weapon_Generic

A generic function to handle the basics of weapon thinking
================
*/
#define FRAME_FIRE_FIRST		(FRAME_ACTIVATE_LAST + 1)
#define FRAME_IDLE_FIRST		(FRAME_FIRE_LAST + 1)
#define FRAME_DEACTIVATE_FIRST	(FRAME_IDLE_LAST + 1)

static void Weapon_Generic2 (edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, int *pause_frames, int *fire_frames, void (*fire)(edict_t *ent))
{
	int		n;
	gclient_t *client = ent->r.client;

	client->weapon_missed = qtrue;
	if (ent->deadflag || ent->s.modelindex != 255) // VWep animations screw up corpses
		return;

	if (client->weaponstate == WEAPON_DROPPING)
	{
		if (client->ps.gunframe == FRAME_DEACTIVATE_LAST)
		{
			ChangeWeapon (ent);
			return;
		}
		else if ((FRAME_DEACTIVATE_LAST - client->ps.gunframe) == 4)
		{
			client->anim_priority = ANIM_REVERSE;
			if (client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				ent->s.frame = FRAME_crpain4+1;
				client->anim_end = FRAME_crpain1;
			}
			else
			{
				ent->s.frame = FRAME_pain304+1;
				client->anim_end = FRAME_pain301;
				
			}
		}

		client->ps.gunframe++;
		return;
	}

	if (client->weaponstate == WEAPON_ACTIVATING)
	{
		if (client->ps.gunframe == FRAME_ACTIVATE_LAST || instantweap->integer)
		{
			client->weaponstate = WEAPON_READY;
			client->ps.gunframe = FRAME_IDLE_FIRST;
			return;
		}

		client->ps.gunframe++;
		return;
	}

	if ((client->newweapon) && (client->weaponstate != WEAPON_FIRING))
	{
		client->weaponstate = WEAPON_DROPPING;
		if (instantweap->integer) {
			ChangeWeapon(ent);
			return;
		} else
			client->ps.gunframe = FRAME_DEACTIVATE_FIRST;

		if ((FRAME_DEACTIVATE_LAST - FRAME_DEACTIVATE_FIRST) < 4)
		{
			client->anim_priority = ANIM_REVERSE;
			if (client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				ent->s.frame = FRAME_crpain4+1;
				client->anim_end = FRAME_crpain1;
			}
			else
			{
				ent->s.frame = FRAME_pain304+1;
				client->anim_end = FRAME_pain301;
				
			}
		}
		return;
	}

	if (client->weaponstate == WEAPON_READY)
	{
		if ( ((client->latched_buttons|client->buttons) & BUTTON_ATTACK) )
		{
			client->latched_buttons &= ~BUTTON_ATTACK;
			if ((!client->ammo_index) || 
				( client->pers.inventory[client->ammo_index] >= client->pers.weapon->quantity))
			{
				client->ps.gunframe = FRAME_FIRE_FIRST;
				client->weaponstate = WEAPON_FIRING;

				// start the animation
				client->anim_priority = ANIM_ATTACK;
				if (client->ps.pmove.pm_flags & PMF_DUCKED)
				{
					ent->s.frame = FRAME_crattak1-1;
					client->anim_end = FRAME_crattak9;
				}
				else
				{
					ent->s.frame = FRAME_attack1-1;
					client->anim_end = FRAME_attack8;
				}
			} else {
				NoAmmoWeaponChange ( client );
			}
		}
		else
		{
			if (client->ps.gunframe == FRAME_IDLE_LAST)
			{
				client->ps.gunframe = FRAME_IDLE_FIRST;
				return;
			}

			if (pause_frames)
			{
				for (n = 0; pause_frames[n]; n++)
				{
					if (client->ps.gunframe == pause_frames[n])
					{
						if (rand()&15)
							return;
					}
				}
			}

			client->ps.gunframe++;
			return;
		}
	}

	if (client->weaponstate == WEAPON_FIRING)
	{
		for (n = 0; fire_frames[n]; n++)
		{
			if (client->ps.gunframe == fire_frames[n])
			{
//ZOID
				if (!CTFApplyStrengthSound(ent))
//ZOID
				if (client->quad_framenum > level.framenum)
					G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/damage3.wav"), 1, ATTN_NORM);
//ZOID
				CTFApplyHasteSound(ent);
//ZOID

				fire (ent);
				client->weapon_missed = qfalse;
				break;
			}
		}

		if (!fire_frames[n])
			client->ps.gunframe++;

		if (client->ps.gunframe == FRAME_IDLE_FIRST+1)
			client->weaponstate = WEAPON_READY;
	}
}

//ZOID
void Weapon_Generic (edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, int *pause_frames, int *fire_frames, void (*fire)(edict_t *ent))
{
	int oldstate = ent->r.client->weaponstate;

	Weapon_Generic2 (ent, FRAME_ACTIVATE_LAST, FRAME_FIRE_LAST, 
		FRAME_IDLE_LAST, FRAME_DEACTIVATE_LAST, pause_frames, 
		fire_frames, fire);

	// run the weapon frame again if hasted
	if (Q_stricmp(ent->r.client->pers.weapon->pickup_name, "Grapple") == 0 &&
		ent->r.client->weaponstate == WEAPON_FIRING)
		return;

	if (((CTFApplyHaste(ent) && ent->r.client->weapon_missed) ||
		(Q_stricmp(ent->r.client->pers.weapon->pickup_name, "Grapple") == 0 &&
		ent->r.client->weaponstate != WEAPON_FIRING))
		&& oldstate == ent->r.client->weaponstate) {
		Weapon_Generic2 (ent, FRAME_ACTIVATE_LAST, FRAME_FIRE_LAST, 
			FRAME_IDLE_LAST, FRAME_DEACTIVATE_LAST, pause_frames, 
			fire_frames, fire);
	}
}
//ZOID

/*
======================================================================

GRENADE

======================================================================
*/

#define GRENADE_TIMER		3.0
#define GRENADE_MINSPEED	400
#define GRENADE_MAXSPEED	800

void weapon_grenade_fire (edict_t *ent, qboolean held)
{
	vec3_t	offset;
	vec3_t	forward, right;
	vec3_t	start;
	int		damage = 125;
	float	timer;
	int		speed;
	float	radius;
	gclient_t *client = ent->r.client;

	radius = damage+40;
	if (is_quad)
		damage *= 4;

	VectorSet(offset, 8, 8, ent->viewheight-8);
	AngleVectors (client->v_angle, forward, right, NULL);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);

	timer = client->grenade_time - level.time;
	speed = GRENADE_MINSPEED + (GRENADE_TIMER - timer) * ((GRENADE_MAXSPEED - GRENADE_MINSPEED) / GRENADE_TIMER);
	fire_grenade2 (ent, start, forward, damage, speed, timer, radius, held);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;

	client->grenade_time = level.time + 1.0;

	if (ent->deadflag || ent->s.modelindex != 255) // VWep animations screw up corpses
		return;

	if (client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		client->anim_priority = ANIM_ATTACK;
		ent->s.frame = FRAME_crattak1-1;
		client->anim_end = FRAME_crattak3;
	}
	else
	{
		client->anim_priority = ANIM_REVERSE;
		ent->s.frame = FRAME_wave08;
		client->anim_end = FRAME_wave01;
	}
}

void Weapon_Grenade (edict_t *ent)
{
	gclient_t *client = ent->r.client;

	if (client->newweapon && (client->weaponstate == WEAPON_READY))
	{
		ChangeWeapon (ent);
		return;
	}

	if (client->weaponstate == WEAPON_ACTIVATING)
	{
		client->weaponstate = WEAPON_READY;
		client->ps.gunframe = 16;
		return;
	}

	if (client->weaponstate == WEAPON_READY)
	{
		if ( ((client->latched_buttons|client->buttons) & BUTTON_ATTACK) )
		{
			client->latched_buttons &= ~BUTTON_ATTACK;
			if (client->pers.inventory[client->ammo_index])
			{
				client->ps.gunframe = 1;
				client->weaponstate = WEAPON_FIRING;
				client->grenade_time = 0;
			} else {
				NoAmmoWeaponChange ( client );
			}
			return;
		}

		if ((client->ps.gunframe == 29) || (client->ps.gunframe == 34) || (client->ps.gunframe == 39) || (client->ps.gunframe == 48))
		{
			if (rand()&15)
				return;
		}

		if (++client->ps.gunframe > 48)
			client->ps.gunframe = 16;
		return;
	}

	if (client->weaponstate == WEAPON_FIRING)
	{
		if (client->ps.gunframe == 5)
			G_Sound (ent, CHAN_WEAPON, trap_SoundIndex("sound/weapons/hgrena1b.wav"), 1, ATTN_NORM);

		if (client->ps.gunframe == 11)
		{
			if (!client->grenade_time)
			{
				client->grenade_time = level.time + GRENADE_TIMER + 0.2;
				client->weapon_sound = trap_SoundIndex("sound/weapons/hgrenc1b.wav");
			}

			// they waited too long, detonate it in their hand
			if (!client->grenade_blew_up && level.time >= client->grenade_time)
			{
				client->weapon_sound = 0;
				weapon_grenade_fire (ent, qtrue);
				client->grenade_blew_up = qtrue;
			}

			if (client->buttons & BUTTON_ATTACK)
				return;

			if (client->grenade_blew_up)
			{
				if (level.time >= client->grenade_time)
				{
					client->ps.gunframe = 15;
					client->grenade_blew_up = qfalse;
				}
				else
				{
					return;
				}
			}
		}

		if (client->ps.gunframe == 12)
		{
			client->weapon_sound = 0;
			weapon_grenade_fire (ent, qfalse);
		}

		if ((client->ps.gunframe == 15) && (level.time < client->grenade_time))
			return;

		client->ps.gunframe++;

		if (client->ps.gunframe == 16)
		{
			client->grenade_time = 0;
			client->weaponstate = WEAPON_READY;
		}
	}
}

/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

void weapon_grenadelauncher_fire (edict_t *ent)
{
	vec3_t	offset;
	vec3_t	forward, right;
	vec3_t	start;
	int		damage = 120;
	float	radius;
	gclient_t *client = ent->r.client;

	radius = damage+40;
	if (is_quad)
		damage *= 4;

	VectorSet(offset, 8, 8, ent->viewheight-8);
	AngleVectors (client->v_angle, forward, right, NULL);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);

	VectorScale (forward, -2, client->kick_origin);
	client->kick_angles[0] = -1;

	fire_grenade (ent, start, forward, damage, 650, 2.5, radius);
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	client->ps.gunframe++;

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}

void Weapon_GrenadeLauncher (edict_t *ent)
{
	static int	pause_frames[]	= {34, 51, 59, 0};
	static int	fire_frames[]	= {6, 0};

	Weapon_Generic (ent, 5, 16, 59, 64, pause_frames, fire_frames, weapon_grenadelauncher_fire);
}

/*
======================================================================

ROCKET

======================================================================
*/

void Weapon_RocketLauncher_Fire (edict_t *ent)
{
	vec3_t	offset, start, end;
	vec3_t	forward, right;
	int		damage;
	float	damage_radius;
	int		radius_damage;
	trace_t	tr;
	gclient_t *client = ent->r.client;

	damage = 100 + (int)(random() * 20.0);
	radius_damage = 120;
	damage_radius = 120;
	if (is_quad)
	{
		damage *= 4;
		radius_damage *= 4;
	}

	AngleVectors (client->v_angle, forward, right, NULL);

	VectorScale (forward, -2, client->kick_origin);
	client->kick_angles[0] = -1;

	VectorSet(offset, 8, 8, ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);
	VectorMA (start, 14, forward, end);
	trap_Trace (&tr, start, vec3_origin, vec3_origin, end, ent, MASK_SOLID);

	fire_rocket (ent, tr.endpos, forward, damage, 700, damage_radius, radius_damage);

	// send muzzle flash
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	client->ps.gunframe++;

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}

void Weapon_RocketLauncher (edict_t *ent)
{
	static int	pause_frames[]	= {25, 33, 42, 50, 0};
	static int	fire_frames[]	= {5, 0};

	Weapon_Generic (ent, 4, 12, 50, 54, pause_frames, fire_frames, Weapon_RocketLauncher_Fire);
}


/*
======================================================================

BLASTER / HYPERBLASTER

======================================================================
*/

void Blaster_Fire (edict_t *ent, vec3_t g_offset, int damage, qboolean hyper, int type)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	offset;
	gclient_t *client = ent->r.client;

	if (is_quad)
		damage *= 4;
	AngleVectors (client->v_angle, forward, right, NULL);
	VectorSet(offset, 24, 8, ent->viewheight-8);
	VectorAdd (offset, g_offset, offset);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);

	VectorScale (forward, -2, client->kick_origin);
	client->kick_angles[0] = -1;

	fire_blaster (ent, start, forward, damage, 1000, type, hyper ? MOD_HYPERBLASTER : MOD_BLASTER);

	// send muzzle flash
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	PlayerNoise(ent, start, PNOISE_WEAPON);
}


void Weapon_Blaster_Fire (edict_t *ent)
{
	int		damage;

	if (deathmatch->integer)
		damage = 15;
	else
		damage = 10;
	Blaster_Fire (ent, vec3_origin, damage, qfalse, ET_BLASTER);
	ent->r.client->ps.gunframe++;
}

void Weapon_Blaster (edict_t *ent)
{
	static int	pause_frames[]	= {19, 32, 0};
	static int	fire_frames[]	= {5, 0};

	Weapon_Generic (ent, 4, 8, 52, 55, pause_frames, fire_frames, Weapon_Blaster_Fire);
}


void Weapon_HyperBlaster_Fire (edict_t *ent)
{
	float	rotation;
	vec3_t	offset;
	int		type;
	int		damage;
	gclient_t *client = ent->r.client;

	client->weapon_sound = trap_SoundIndex("sound/weapons/hyprbl1a.wav");

	if (!(client->buttons & BUTTON_ATTACK))
	{
		client->ps.gunframe++;
	}
	else
	{
		if (! client->pers.inventory[client->ammo_index] ) {
			NoAmmoWeaponChange ( client );
		} else {
			rotation = (client->ps.gunframe - 5) * M_TWOPI/6;
			offset[0] = -4 * sin(rotation);
			offset[1] = 0;
			offset[2] = 4 * cos(rotation);

			if ((client->ps.gunframe == 6) || (client->ps.gunframe == 9))
				type = ET_HYPERBLASTER;
			else
				type = ET_GENERIC;
			if (deathmatch->integer)
				damage = 15;
			else
				damage = 20;
			Blaster_Fire (ent, offset, damage, qtrue, type);
			if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
				client->pers.inventory[client->ammo_index]--;

			client->anim_priority = ANIM_ATTACK;
			if (client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				ent->s.frame = FRAME_crattak1 - 1;
				client->anim_end = FRAME_crattak9;
			}
			else
			{
				ent->s.frame = FRAME_attack1 - 1;
				client->anim_end = FRAME_attack8;
			}
		}

		client->ps.gunframe++;
		if (client->ps.gunframe == 12 && client->pers.inventory[client->ammo_index])
			client->ps.gunframe = 6;
	}

	if (client->ps.gunframe == 12)
	{
		G_Sound (ent, CHAN_AUTO, trap_SoundIndex("sound/weapons/hyprbd1a.wav"), 1, ATTN_NORM);
		client->weapon_sound = 0;
	}
}

void Weapon_HyperBlaster (edict_t *ent)
{
	static int	pause_frames[]	= {0};
	static int	fire_frames[]	= {6, 7, 8, 9, 10, 11, 0};

	Weapon_Generic (ent, 5, 20, 49, 53, pause_frames, fire_frames, Weapon_HyperBlaster_Fire);
}

/*
======================================================================

MACHINEGUN / CHAINGUN

======================================================================
*/

void Machinegun_Fire (edict_t *ent)
{
	int	i;
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		angles;
	int			damage = 8;
	int			kick = 2;
	vec3_t		offset;
	gclient_t	*client = ent->r.client;

	if (!(client->buttons & BUTTON_ATTACK))
	{
		client->machinegun_shots = 0;
		client->ps.gunframe++;
		return;
	}

	if (client->ps.gunframe == 5)
		client->ps.gunframe = 4;
	else
		client->ps.gunframe = 5;

	if (client->pers.inventory[client->ammo_index] < 1)
	{
		client->ps.gunframe = 6;
		NoAmmoWeaponChange ( client );
		return;
	}

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	for (i=1 ; i<3 ; i++)
	{
		client->kick_origin[i] = crandom() * 0.35;
		client->kick_angles[i] = crandom() * 0.7;
	}
	client->kick_origin[0] = crandom() * 0.35;
	client->kick_angles[0] = client->machinegun_shots * -1.5;

	// raise the gun as it is firing
	if (!deathmatch->integer)
	{
		client->machinegun_shots++;
		if (client->machinegun_shots > 9)
			client->machinegun_shots = 9;
	}

	// get start / end positions
	VectorAdd (client->v_angle, client->kick_angles, angles);
	AngleVectors (angles, forward, right, NULL);
	VectorSet(offset, 0, 8, ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);
	fire_bullet (ent, start, forward, damage, kick, MOD_MACHINEGUN);
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;

	client->anim_priority = ANIM_ATTACK;
	if (client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		ent->s.frame = FRAME_crattak1 - (int) (random()+0.25);
		client->anim_end = FRAME_crattak9;
	}
	else
	{
		ent->s.frame = FRAME_attack1 - (int) (random()+0.25);
		client->anim_end = FRAME_attack8;
	}
}

void Weapon_Machinegun (edict_t *ent)
{
	static int	pause_frames[]	= {23, 45, 0};
	static int	fire_frames[]	= {4, 5, 0};

	Weapon_Generic (ent, 3, 5, 45, 49, pause_frames, fire_frames, Machinegun_Fire);
}

void Chaingun_Fire (edict_t *ent)
{
	int			i;
	int			shots;
	vec3_t		start;
	vec3_t		forward, right, up;
	float		r, u;
	vec3_t		offset;
	int			damage;
	int			kick = 2;
	gclient_t	*client = ent->r.client;

	if (deathmatch->integer)
		damage = 6;
	else
		damage = 8;

	if (client->ps.gunframe == 5)
		G_Sound (ent, CHAN_AUTO, trap_SoundIndex("sound/weapons/chngnu1a.wav"), 1, ATTN_IDLE);

	if ((client->ps.gunframe == 14) && !(client->buttons & BUTTON_ATTACK))
	{
		client->ps.gunframe = 32;
		client->weapon_sound = 0;
		return;
	}
	else if ((client->ps.gunframe == 21) && (client->buttons & BUTTON_ATTACK)
		&& client->pers.inventory[client->ammo_index])
	{
		client->ps.gunframe = 15;
	}
	else
	{
		client->ps.gunframe++;
	}

	if (client->ps.gunframe == 22)
	{
		client->weapon_sound = 0;
		G_Sound (ent, CHAN_AUTO, trap_SoundIndex("sound/weapons/chngnd1a.wav"), 1, ATTN_IDLE);
	}
	else
	{
		client->weapon_sound = trap_SoundIndex("sound/weapons/chngnl1a.wav");
	}

	client->anim_priority = ANIM_ATTACK;
	if (client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		ent->s.frame = FRAME_crattak1 - (client->ps.gunframe & 1);
		client->anim_end = FRAME_crattak9;
	}
	else
	{
		ent->s.frame = FRAME_attack1 - (client->ps.gunframe & 1);
		client->anim_end = FRAME_attack8;
	}

	if (client->ps.gunframe <= 9)
		shots = 1;
	else if (client->ps.gunframe <= 14)
	{
		if (client->buttons & BUTTON_ATTACK)
			shots = 2;
		else
			shots = 1;
	}
	else
		shots = 3;

	if (client->pers.inventory[client->ammo_index] < shots)
		shots = client->pers.inventory[client->ammo_index];

	if (!shots)
	{
		NoAmmoWeaponChange ( client );
		return;
	}

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	for (i=0 ; i<3 ; i++)
	{
		client->kick_origin[i] = crandom() * 0.35;
		client->kick_angles[i] = crandom() * 0.7;
	}

	AngleVectors (client->v_angle, forward, right, up);

	for (i=0 ; i<shots ; i++)
	{
		// get start / end positions
		r = 7 + crandom()*4;
		u = crandom()*4;
		VectorSet(offset, 0, r, u + ent->viewheight-8);
		P_ProjectSource (client, ent->s.origin, offset, forward, right, start);

		fire_bullet (ent, start, forward, damage, kick, MOD_CHAINGUN);
	}

	// send muzzle flash
	G_AddEvent (ent, EV_MUZZLEFLASH, (3-shots) | is_silenced, qtrue);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index] -= shots;
}


void Weapon_Chaingun (edict_t *ent)
{
	static int	pause_frames[]	= {38, 43, 51, 61, 0};
	static int	fire_frames[]	= {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 0};

	Weapon_Generic (ent, 4, 31, 61, 64, pause_frames, fire_frames, Chaingun_Fire);
}


/*
======================================================================

SHOTGUN / SUPERSHOTGUN

======================================================================
*/

void weapon_shotgun_fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		offset;
	int			damage = 4;
	int			kick = 8;
	gclient_t	*client = ent->r.client;

	if (client->ps.gunframe == 9)
	{
		client->ps.gunframe++;
		return;
	}

	AngleVectors (client->v_angle, forward, right, NULL);

	VectorScale (forward, -2, client->kick_origin);
	client->kick_angles[0] = -2;

	VectorSet(offset, 0, 8,  ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	fire_shotgun (ent, start, forward, damage, kick, DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN);

	// send muzzle flash
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	client->ps.gunframe++;
	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}

void Weapon_Shotgun (edict_t *ent)
{
	static int	pause_frames[]	= {22, 28, 34, 0};
	static int	fire_frames[]	= {8, 9, 0};

	Weapon_Generic (ent, 7, 18, 36, 39, pause_frames, fire_frames, weapon_shotgun_fire);
}


void weapon_supershotgun_fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		offset;
	vec3_t		v;
	int			damage = 6;
	int			kick = 12;
	gclient_t	*client = ent->r.client;

	AngleVectors (client->v_angle, forward, right, NULL);

	VectorScale (forward, -2, client->kick_origin);
	client->kick_angles[0] = -2;

	VectorSet(offset, 0, 8,  ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	v[PITCH] = client->v_angle[PITCH];
	v[YAW]   = client->v_angle[YAW] - 5;
	v[ROLL]  = client->v_angle[ROLL];
	AngleVectors (v, forward, NULL, NULL);
	fire_shotgun (ent, start, forward, damage, kick, DEFAULT_SSHOTGUN_COUNT/2, MOD_SSHOTGUN);
	v[YAW]   = client->v_angle[YAW] + 5;
	AngleVectors (v, forward, NULL, NULL);
	fire_shotgun (ent, start, forward, damage, kick, DEFAULT_SSHOTGUN_COUNT/2, MOD_SSHOTGUN);

	// send muzzle flash
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	client->ps.gunframe++;
	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index] -= 2;
}

void Weapon_SuperShotgun (edict_t *ent)
{
	static int	pause_frames[]	= {29, 42, 57, 0};
	static int	fire_frames[]	= {7, 0};

	Weapon_Generic (ent, 6, 17, 57, 61, pause_frames, fire_frames, weapon_supershotgun_fire);
}



/*
======================================================================

RAILGUN

======================================================================
*/

void weapon_railgun_fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		offset;
	int			damage;
	int			kick;
	gclient_t	*client = ent->r.client;

	if (deathmatch->integer)
	{	// normal damage is too extreme in dm
		damage = 100;
		kick = 200;
	}
	else
	{
		damage = 150;
		kick = 250;
	}

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	AngleVectors (client->v_angle, forward, right, NULL);

	VectorScale (forward, -3, client->kick_origin);
	client->kick_angles[0] = -3;

	VectorSet(offset, 0, 7,  ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);
	fire_rail (ent, start, forward, damage, kick);

	// send muzzle flash
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	client->ps.gunframe++;
	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}


void Weapon_Railgun (edict_t *ent)
{
	static int	pause_frames[]	= {56, 0};
	static int	fire_frames[]	= {4, 0};

	Weapon_Generic (ent, 3, 18, 56, 61, pause_frames, fire_frames, weapon_railgun_fire);
}


/*
======================================================================

BFG10K

======================================================================
*/

void weapon_bfg_fire (edict_t *ent)
{
	vec3_t		offset, start;
	vec3_t		forward, right;
	int			damage;
	float		damage_radius = 1000;
	gclient_t	*client = ent->r.client;

	if (deathmatch->integer)
		damage = 200;
	else
		damage = 500;

	if (client->ps.gunframe == 9)
	{
		// send muzzle flash
		G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

		client->ps.gunframe++;

		PlayerNoise(ent, start, PNOISE_WEAPON);
		return;
	}

	// cells can go down during windup (from power armor hits), so
	// check again and abort firing if we don't have enough now
	if (client->pers.inventory[client->ammo_index] < 50)
	{
		client->ps.gunframe++;
		return;
	}

	if (is_quad)
		damage *= 4;

	AngleVectors (client->v_angle, forward, right, NULL);

	VectorScale (forward, -2, client->kick_origin);

	// make a big pitch kick with an inverse fall
	client->v_dmg_pitch = -40;
	client->v_dmg_roll = crandom()*8;
	client->v_dmg_time = level.time + DAMAGE_TIME;

	VectorSet(offset, 8, 8, ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);
	fire_bfg (ent, start, forward, damage, 400, damage_radius);

	client->ps.gunframe++;

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index] -= 50;
}

void Weapon_BFG (edict_t *ent)
{
	static int	pause_frames[]	= {39, 45, 50, 55, 0};
	static int	fire_frames[]	= {9, 17, 0};

	Weapon_Generic (ent, 8, 32, 55, 58, pause_frames, fire_frames, weapon_bfg_fire);
}


//======================================================================
