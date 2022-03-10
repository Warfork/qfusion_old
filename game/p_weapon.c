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
#include "gs_pmodels.h"


static qboolean	is_quad;
static qbyte	is_silenced;


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

	if (!client->pers.weapon)	// dead
		return;

	// remove the hold priority from weaponOUT and
	// set a standard animation for BASIC
	ent->pmAnim.anim_priority[UPPER] = ANIM_BASIC;
	ent->pmAnim.anim[UPPER] = TORSO_STAND;

	//Send the weaponIN animation and sound style through EVENTs
	if( level.time >= ent->pain_debounce_time ) {
		if( !ent->r.client->noammo )
			G_AddEvent (ent, EV_WEAPONUP, 1, qtrue);
		else
			G_AddEvent (ent, EV_WEAPONUP, 2, qtrue);

		ent->pain_debounce_time = level.time + 1;
	} else 
		G_AddEvent (ent, EV_WEAPONUP, 0, qtrue);

	client->noammo = qfalse;
	client->weaponstate = WEAPON_ACTIVATING;
	client->weapontime = 0;
	client->machinegun_shots = 0;
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
		is_quad = (ent->r.client->quad_timeout > level.time);
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
static void Weapon_Generic2 (edict_t *ent, int ACTIVATION_TIME, int POWERUP_TIME, int FIRE_DELAY, int COOLDOWN_TIME, int DEACTIVATION_TIME, void (*fire)(edict_t *ent))
{
	gclient_t *client = ent->r.client;

	client->weapon_missed = qtrue;
	if (ent->deadflag || ent->s.modelindex != 255) // VWep animations screw up corpses
		return;

	if (client->weaponstate == WEAPON_DROPPING)
	{
		if (client->weapontime >= DEACTIVATION_TIME)
		{
			ChangeWeapon (ent);
			return;
		} 
		else if ((DEACTIVATION_TIME - client->weapontime) <= (TORSO_FLIPOUT_TIME - 100))
		{	// weaponOUT
			ent->pmAnim.anim_priority[UPPER] = ANIM_HOLD; // hold this animation until weaponIN removes it
			ent->pmAnim.anim[UPPER] = TORSO_FLIPOUT;
		}

		client->weapontime += game.frametimeMsec;
		return;
	}

	if (client->weaponstate == WEAPON_ACTIVATING)
	{
		if (client->weapontime < ACTIVATION_TIME && !instantweap->integer)
		{
			client->weapontime += game.frametimeMsec;
			return;
		}
		else
		{
			client->weaponstate = WEAPON_READY;
			client->weapontime = 0;
		}
	}

	if (client->weaponstate == WEAPON_COOLINGDOWN)
	{
		if (client->weapontime < COOLDOWN_TIME)
		{
			client->weapontime += game.frametimeMsec;
		}
		else
		{
			client->weapontime = 0;
			client->weaponstate = WEAPON_READY;
		}
	}

	if (client->newweapon && (client->weaponstate != WEAPON_FIRING))
	{
		client->weaponstate = WEAPON_DROPPING;
		if (instantweap->integer)
		{
			ChangeWeapon(ent);
			return;
		} 
		else
		{
			client->weapontime = game.frametimeMsec;
		}

		if( DEACTIVATION_TIME < ( TORSO_FLIPOUT_TIME - 100 ) )
		{	// weaponOUT
			ent->pmAnim.anim_priority[UPPER] = ANIM_HOLD;
			ent->pmAnim.anim[UPPER] = TORSO_FLIPOUT;
		}
		return;
	}

	if (client->weaponstate == WEAPON_COOLINGDOWN)
		return;

	if (client->weaponstate == WEAPON_READY || client->weaponstate == WEAPON_POWERINGUP || client->weaponstate == WEAPON_FIRING)
	{
		if ( ((client->latched_buttons|client->buttons) & BUTTON_ATTACK) )
		{
			client->latched_buttons &= ~BUTTON_ATTACK;
			if( !client->ammo_index || 
				( client->pers.inventory[client->ammo_index] >= client->pers.weapon->quantity))
			{
				if (client->weaponstate == WEAPON_READY)
				{
					client->weapontime = 0;
					client->weaponstate = WEAPON_POWERINGUP;

					// HACK HACK HACK
					if (ent->s.weapon == WEAP_BFG)
					{	// send muzzle flash
						G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);
						PlayerNoise (ent, ent->s.origin, PNOISE_WEAPON);
					}
				}

				if (client->weaponstate == WEAPON_POWERINGUP)
				{
					if (client->weapontime < POWERUP_TIME)
					{
						client->weapontime += game.frametimeMsec;
						return;
					}
					else
					{
						client->weapontime = -1;
						client->weaponstate = WEAPON_FIRING;
					}
				}
			}
			else
			{
				client->weapontime = 0;
				client->weaponstate = WEAPON_COOLINGDOWN;
				NoAmmoWeaponChange( client );
				return;
			}
		}
		else
		{
			if (client->weaponstate == WEAPON_POWERINGUP)
			{
				client->weaponstate = WEAPON_COOLINGDOWN;
				client->weapontime = -(FIRE_DELAY - client->weapontime);
				return;
			}
		}
	}

	if (client->weaponstate == WEAPON_FIRING)
	{
		if (client->weapontime > 0 && !(client->buttons & BUTTON_ATTACK))
		{	// stopped firing
			client->weapon_sound = 0;		// FIXME?
			client->machinegun_shots = 0;

			client->weapontime = -(FIRE_DELAY - client->weapontime);
			client->weaponstate = WEAPON_COOLINGDOWN;
			return;
		}

		if (client->weapontime < 0 || client->weapontime >= FIRE_DELAY)
		{
			client->weapontime = 0;
//ZOID
			if (!CTFApplyStrengthSound(ent))
//ZOID
			if (client->quad_timeout > level.time)
				G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/damage3.wav"), 1, ATTN_NORM);
//ZOID
			CTFApplyHasteSound(ent);
//ZOID

			fire (ent);

			client->weapon_missed = qfalse;
		}

		client->weapontime += game.frametimeMsec;
	}
}

//ZOID
void Weapon_Generic (edict_t *ent, int ACTIVATION_TIME, int POWERUP_TIME, int FIRE_DELAY, int COOLDOWN_TIME, int DEACTIVATION_TIME, void (*fire)(edict_t *ent))
{
	int oldstate = ent->r.client->weaponstate;

	Weapon_Generic2 (ent, ACTIVATION_TIME, POWERUP_TIME, 
		FIRE_DELAY, COOLDOWN_TIME,
		DEACTIVATION_TIME, fire);

	// run the weapon frame again if hasted
	if (Q_stricmp(ent->r.client->pers.weapon->pickup_name, "Grapple") == 0 &&
		ent->r.client->weaponstate == WEAPON_FIRING)
		return;

	if (((CTFApplyHaste(ent) && ent->r.client->weapon_missed) ||
		(Q_stricmp(ent->r.client->pers.weapon->pickup_name, "Grapple") == 0 &&
		ent->r.client->weaponstate != WEAPON_FIRING))
		&& oldstate == ent->r.client->weaponstate) {
		Weapon_Generic2 (ent, ACTIVATION_TIME, POWERUP_TIME, 
			FIRE_DELAY, COOLDOWN_TIME,
			DEACTIVATION_TIME, fire);
	}
}
//ZOID

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

	P_AddWeaponKick (client, tv(-2, 0, 0), tv(-1.5, 0, 0), 0.2);

	fire_grenade (ent, start, forward, damage, 650, 2.5, radius);
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}

void Weapon_GrenadeLauncher (edict_t *ent)
{
	// FRAME_ACTIVATE_LAST = 5, FRAME_FIRE_LAST = 16, FRAME_IDLE_LAST = 59, FRAME_DEACTIVATE_LAST = 64
	Weapon_Generic (ent, 500, 0, 800, 0, 100, weapon_grenadelauncher_fire);
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

	P_AddWeaponKick (client, tv(-2, 0, 0), tv(-2.5, 0, 0), 0.25);

	VectorSet(offset, 8, 8, ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);
	VectorMA (start, 14, forward, end);
	trap_Trace (&tr, start, vec3_origin, vec3_origin, end, ent, MASK_SOLID);

	fire_rocket (ent, tr.endpos, forward, damage, 700, damage_radius, radius_damage);

	// send muzzle flash
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}

void Weapon_RocketLauncher (edict_t *ent)
{
	// FRAME_ACTIVATE_LAST = 4, FRAME_FIRE_LAST = 12, FRAME_IDLE_LAST = 50, FRAME_DEACTIVATE_LAST = 54
	Weapon_Generic (ent, 400, 0, 800, 0, 100, Weapon_RocketLauncher_Fire);
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

	P_AddWeaponKick (client, tv(-2, 0, 0), tv(-1, 0, 0), 0.15);

	fire_blaster (ent, start, forward, damage, 1000, type, hyper ? MOD_HYPERBLASTER : MOD_BLASTER);

	// send muzzle flash
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	PlayerNoise(ent, start, PNOISE_WEAPON);
}


void Weapon_Blaster_Fire (edict_t *ent)
{
	int		damage = deathmatch->integer ? 15 : 10;
	Blaster_Fire (ent, vec3_origin, damage, qfalse, ET_BLASTER);
}

void Weapon_Blaster (edict_t *ent)
{
	// FRAME_ACTIVATE_LAST = 4, FRAME_FIRE_LAST = 8, FRAME_IDLE_LAST = 52, FRAME_DEACTIVATE_LAST = 55
	Weapon_Generic (ent, 400, 0, 400, 0, 100, Weapon_Blaster_Fire);
}


void Weapon_HyperBlaster_Fire (edict_t *ent)
{
	float	rotation;
	vec3_t	offset;
	int		type;
	int		damage = deathmatch->integer ? 15 : 20;
	gclient_t *client = ent->r.client;

	if (!client->machinegun_shots)
		client->machinegun_shots = 6;

	rotation = (client->machinegun_shots - 5) * M_TWOPI/6;
	offset[0] = -4 * sin(rotation);
	offset[1] = 0;
	offset[2] = 4 * cos(rotation);

	if (client->machinegun_shots == 6 || client->machinegun_shots == 9)
		type = ET_HYPERBLASTER;
	else
		type = ET_BLASTER2;

	Blaster_Fire (ent, offset, damage, qtrue, type);

	client->machinegun_shots++;
	if (client->machinegun_shots == 12)
		client->machinegun_shots = 6;

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}

void Weapon_HyperBlaster (edict_t *ent)
{
	// FRAME_ACTIVATE_LAST = 5, FRAME_FIRE_LAST = 20, FRAME_IDLE_LAST = 49, FRAME_DEACTIVATE_LAST = 53
	Weapon_Generic (ent, 500, 0, 100, 100, 100, Weapon_HyperBlaster_Fire);
}

/*
======================================================================

MACHINEGUN / CHAINGUN

======================================================================
*/

void Machinegun_Fire (edict_t *ent)
{
	vec3_t		start;
	vec3_t		forward, right;
	vec3_t		angles;
	int			damage = 8;
	int			kick = 2;
	vec3_t		offset;
	gclient_t	*client = ent->r.client;

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	P_AddWeaponKick (client, 
		tv (-1.0f + crandom() * 0.35f, crandom() * 0.35f, crandom() * 0.15f), 
		tv (-1.5f + crandom() * 0.35f, crandom() * 0.35f, crandom() * 0.2f), 
		0.2);

	// raise the gun as it is firing
	if (!deathmatch->integer)
	{
		client->machinegun_shots++;
		if (client->machinegun_shots > 9)
			client->machinegun_shots = 9;
	}

	// get start / end positions
	VectorAdd (client->v_angle, client->ps.kick_angles, angles);
	AngleVectors (angles, forward, right, NULL);
	VectorSet(offset, 0, 8, ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);
	fire_bullet (ent, start, forward, damage, kick, MOD_MACHINEGUN);
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}

void Weapon_Machinegun (edict_t *ent)
{
	// FRAME_ACTIVATE_LAST = 3, FRAME_FIRE_LAST = 5, FRAME_IDLE_LAST = 45, FRAME_DEACTIVATE_LAST = 49
	Weapon_Generic (ent, 300, 0, 100, 0, 100, Machinegun_Fire);
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

	if (!client->weapontime)
		G_Sound (ent, CHAN_AUTO, trap_SoundIndex("sound/weapons/chngnu1a.wav"), 1, ATTN_IDLE);
	client->weapon_sound = trap_SoundIndex("sound/weapons/chngnl1a.wav");

	if (client->machinegun_shots <= 9)
		shots = 1;
	else if (client->machinegun_shots <= 14)
	{
		if (client->buttons & BUTTON_ATTACK)
			shots = 2;
		else
			shots = 1;
	}
	else
		shots = 3;

	client->machinegun_shots++;
	if (client->machinegun_shots == 22)
		client->machinegun_shots = 15;

	if (client->pers.inventory[client->ammo_index] < shots)
		shots = client->pers.inventory[client->ammo_index];

	if (is_quad)
	{
		damage *= 4;
		kick *= 4;
	}

	P_AddWeaponKick (client, 
		tv (-1.0f + crandom() * 0.7f, crandom() * 0.7f, crandom() * 0.7f), 
		tv (-1.5f + crandom() * 0.45f, crandom() * 0.45f, crandom() * 0.45f), 
		0.25);

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
	// FRAME_ACTIVATE_LAST = 4, FRAME_FIRE_LAST = 31, FRAME_IDLE_LAST = 61, FRAME_DEACTIVATE_LAST = 64
	Weapon_Generic (ent, 400, 400, 100, 200, 100, Chaingun_Fire);
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

	AngleVectors (client->v_angle, forward, right, NULL);

	P_AddWeaponKick (client, tv(-2, 0, 0), tv(-2, 0, 0), 0.2);

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

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}

void Weapon_Shotgun (edict_t *ent)
{
	// FRAME_ACTIVATE_LAST = 7, FRAME_FIRE_LAST = 18, FRAME_IDLE_LAST = 36, FRAME_DEACTIVATE_LAST = 39
	Weapon_Generic (ent, 700, 0, 1000, 0, 100, weapon_shotgun_fire);
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

	P_AddWeaponKick (client, tv(-2, 0, 0), tv(-2, 0, 0), 0.25);

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

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index] -= 2;
}

void Weapon_SuperShotgun (edict_t *ent)
{
	// FRAME_ACTIVATE_LAST = 6, FRAME_FIRE_LAST = 17, FRAME_IDLE_LAST = 57, FRAME_DEACTIVATE_LAST = 61
	Weapon_Generic (ent, 600, 0, 1100, 0, 100, weapon_supershotgun_fire);
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

	P_AddWeaponKick (client, tv(-3, 0, 0), tv(-3, 0, 0), 0.3);

	VectorSet(offset, 0, 7,  ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);
	fire_rail (ent, start, forward, damage, kick);

	// send muzzle flash
	G_AddEvent (ent, EV_MUZZLEFLASH, is_silenced, qtrue);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index]--;
}


void Weapon_Railgun (edict_t *ent)
{
	// FRAME_ACTIVATE_LAST = 3, FRAME_FIRE_LAST = 18, FRAME_IDLE_LAST = 56, FRAME_DEACTIVATE_LAST = 61
	Weapon_Generic (ent, 300, 0, 1400, 0, 100, weapon_railgun_fire);
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

	// cells can go down during windup (from power armor hits), so
	// check again and abort firing if we don't have enough now
	if (client->pers.inventory[client->ammo_index] < 50)
	{
		NoAmmoWeaponChange (ent->r.client);
		return;
	}

	if (is_quad)
		damage *= 4;

	AngleVectors (client->v_angle, forward, right, NULL);

	P_AddWeaponKick (client, tv (-2, 0, 0), tv(-40, 0, crandom()*8), 0.35);

	VectorSet(offset, 8, 8, ent->viewheight-8);
	P_ProjectSource (client, ent->s.origin, offset, forward, right, start);
	fire_bfg (ent, start, forward, damage, 400, damage_radius);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	if (! ( dmflags->integer & DF_INFINITE_AMMO ) )
		client->pers.inventory[client->ammo_index] -= 50;
}

void Weapon_BFG (edict_t *ent)
{
	// FRAME_ACTIVATE_LAST = 8, FRAME_FIRE_LAST = 32, FRAME_IDLE_LAST = 55, FRAME_DEACTIVATE_LAST = 58
	Weapon_Generic (ent, 800, 800, 2400, 0, 100, weapon_bfg_fire);
}


//======================================================================
