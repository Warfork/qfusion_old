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


qboolean	Pickup_Weapon (edict_t *ent, edict_t *other);
void		Use_Weapon (edict_t *ent, gitem_t *inv);
void		Drop_Weapon (edict_t *ent, gitem_t *inv);

void Weapon_Blaster (edict_t *ent);
void Weapon_Shotgun (edict_t *ent);
void Weapon_SuperShotgun (edict_t *ent);
void Weapon_Machinegun (edict_t *ent);
void Weapon_Chaingun (edict_t *ent);
void Weapon_HyperBlaster (edict_t *ent);
void Weapon_RocketLauncher (edict_t *ent);
void Weapon_Grenade (edict_t *ent);
void Weapon_GrenadeLauncher (edict_t *ent);
void Weapon_Railgun (edict_t *ent);
void Weapon_BFG (edict_t *ent);

gitem_armor_t jacketarmor_info	= { 25,  50, .30, .00, ARMOR_JACKET};
gitem_armor_t combatarmor_info	= { 50, 100, .60, .30, ARMOR_COMBAT};
gitem_armor_t bodyarmor_info	= {100, 200, .80, .60, ARMOR_BODY};

static int	jacket_armor_index;
static int	combat_armor_index;
static int	body_armor_index;
static int	power_screen_index;
static int	power_shield_index;

#define HEALTH_IGNORE_MAX	1
#define HEALTH_TIMED		2

void Use_Quad (edict_t *ent, gitem_t *item);
static int	quad_drop_timeout_hack;

//======================================================================

/*
===============
GetItemByIndex
===============
*/
gitem_t	*GetItemByIndex (int index)
{
	if (index == 0 || index >= game.num_items)
		return NULL;

	return &itemlist[index];
}


/*
===============
FindItemByClassname

===============
*/
gitem_t	*FindItemByClassname (char *classname)
{
	int		i;
	gitem_t	*it;

	it = itemlist;
	for (i=0 ; i<game.num_items ; i++, it++)
	{
		if (!it->classname)
			continue;
		if (!Q_stricmp(it->classname, classname))
			return it;
	}

	return NULL;
}

/*
===============
FindItem

===============
*/
gitem_t	*FindItem (char *pickup_name)
{
	int		i;
	gitem_t	*it;

	it = itemlist;
	for (i=0 ; i<game.num_items ; i++, it++)
	{
		if (!it->pickup_name)
			continue;
		if (!Q_stricmp(it->pickup_name, pickup_name))
			return it;
	}

	return NULL;
}

//======================================================================

void DoRespawn (edict_t *ent)
{
	if (ent->team)
	{
		edict_t	*master;
		int	count;
		int choice;

		master = ent->teammaster;

//ZOID
//in ctf, when we are weapons stay, only the master of a team of weapons
//is spawned
		if (ctf->integer &&
			(dmflags->integer & DF_WEAPONS_STAY) &&
			master->item && (master->item->flags & IT_WEAPON))
			ent = master;
		else {
//ZOID

			for (count = 0, ent = master; ent; ent = ent->chain, count++)
				;

			choice = rand() % count;

			for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
				;
		}
	}

	ent->r.svflags &= ~SVF_NOCLIENT;
	ent->r.solid = SOLID_TRIGGER;
	trap_LinkEntity (ent);

	// send an effect
	G_AddEvent (ent, EV_ITEM_RESPAWN, 0, qtrue);
}

void SetRespawn (edict_t *ent, float delay)
{
	ent->flags |= FL_RESPAWN;
	ent->r.svflags |= SVF_NOCLIENT;
	ent->r.solid = SOLID_NOT;
	ent->nextthink = level.time + delay;
	ent->think = DoRespawn;
	trap_LinkEntity (ent);
}


//======================================================================

qboolean Pickup_Powerup (edict_t *ent, edict_t *other)
{
	int		quantity;

	quantity = other->r.client->pers.inventory[ITEM_INDEX(ent->item)];
	if ((skill->integer == 1 && quantity >= 2) || (skill->integer >= 2 && quantity >= 1))
		return qfalse;

	if ((coop->integer) && (ent->item->flags & IT_STAY_COOP) && (quantity > 0))
		return qfalse;

	other->r.client->pers.inventory[ITEM_INDEX(ent->item)]++;

	if (deathmatch->integer)
	{
		if (!(ent->spawnflags & DROPPED_ITEM) )
			SetRespawn (ent, ent->item->quantity);
		if ((dmflags->integer & DF_INSTANT_ITEMS) || ((ent->item->use == Use_Quad) && (ent->spawnflags & DROPPED_PLAYER_ITEM)))
		{
			if ((ent->item->use == Use_Quad) && (ent->spawnflags & DROPPED_PLAYER_ITEM))
				quad_drop_timeout_hack = (ent->nextthink - level.time) / FRAMETIME;
			ent->item->use (other, ent->item);
		}
	}

	return qtrue;
}

void Drop_General (edict_t *ent, gitem_t *item)
{
	Drop_Item (ent, item);
	ent->r.client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);
}


//======================================================================

qboolean Pickup_Adrenaline (edict_t *ent, edict_t *other)
{
	if (!deathmatch->integer)
		other->max_health += 1;

	if (other->health < other->max_health)
		other->health = other->max_health;

	if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->integer))
		SetRespawn (ent, ent->item->quantity);

	return qtrue;
}

qboolean Pickup_AncientHead (edict_t *ent, edict_t *other)
{
	other->max_health += 2;

	if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->integer))
		SetRespawn (ent, ent->item->quantity);

	return qtrue;
}

qboolean Pickup_Bandolier (edict_t *ent, edict_t *other)
{
	gitem_t	*item;
	int		index;
	gclient_t *client;

	client = other->r.client;
	if (client->pers.max_bullets < 250)
		client->pers.max_bullets = 250;
	if (client->pers.max_shells < 150)
		client->pers.max_shells = 150;
	if (client->pers.max_cells < 250)
		client->pers.max_cells = 250;
	if (client->pers.max_slugs < 75)
		client->pers.max_slugs = 75;

	item = FindItem("Bullets");
	if (item)
	{
		index = ITEM_INDEX(item);
		client->pers.inventory[index] += item->quantity;
		if (client->pers.inventory[index] > client->pers.max_bullets)
			client->pers.inventory[index] = client->pers.max_bullets;
	}

	item = FindItem("Shells");
	if (item)
	{
		index = ITEM_INDEX(item);
		client->pers.inventory[index] += item->quantity;
		if (client->pers.inventory[index] > client->pers.max_shells)
			client->pers.inventory[index] = client->pers.max_shells;
	}

	if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->integer))
		SetRespawn (ent, ent->item->quantity);

	return qtrue;
}

qboolean Pickup_Pack (edict_t *ent, edict_t *other)
{
	gitem_t	*item;
	int		index;
	gclient_t *client;

	client = other->r.client;
	if (client->pers.max_bullets < 300)
		client->pers.max_bullets = 300;
	if (client->pers.max_shells < 200)
		client->pers.max_shells = 200;
	if (client->pers.max_rockets < 100)
		client->pers.max_rockets = 100;
	if (client->pers.max_grenades < 100)
		client->pers.max_grenades = 100;
	if (client->pers.max_cells < 300)
		client->pers.max_cells = 300;
	if (client->pers.max_slugs < 100)
		client->pers.max_slugs = 100;

	item = FindItem("Bullets");
	if (item)
	{
		index = ITEM_INDEX(item);
		client->pers.inventory[index] += item->quantity;
		if (client->pers.inventory[index] > client->pers.max_bullets)
			client->pers.inventory[index] = client->pers.max_bullets;
	}

	item = FindItem("Shells");
	if (item)
	{
		index = ITEM_INDEX(item);
		client->pers.inventory[index] += item->quantity;
		if (client->pers.inventory[index] > client->pers.max_shells)
			client->pers.inventory[index] = client->pers.max_shells;
	}

	item = FindItem("Cells");
	if (item)
	{
		index = ITEM_INDEX(item);
		client->pers.inventory[index] += item->quantity;
		if (client->pers.inventory[index] > client->pers.max_cells)
			client->pers.inventory[index] = client->pers.max_cells;
	}

	item = FindItem("Grenades");
	if (item)
	{
		index = ITEM_INDEX(item);
		client->pers.inventory[index] += item->quantity;
		if (client->pers.inventory[index] > client->pers.max_grenades)
			client->pers.inventory[index] = client->pers.max_grenades;
	}

	item = FindItem("Rockets");
	if (item)
	{
		index = ITEM_INDEX(item);
		client->pers.inventory[index] += item->quantity;
		if (client->pers.inventory[index] > client->pers.max_rockets)
			client->pers.inventory[index] = client->pers.max_rockets;
	}

	item = FindItem("Slugs");
	if (item)
	{
		index = ITEM_INDEX(item);
		client->pers.inventory[index] += item->quantity;
		if (client->pers.inventory[index] > client->pers.max_slugs)
			client->pers.inventory[index] = client->pers.max_slugs;
	}

	if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->integer))
		SetRespawn (ent, ent->item->quantity);

	return qtrue;
}

//======================================================================

void Use_Quad (edict_t *ent, gitem_t *item)
{
	int		timeout;

	ent->r.client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	if (quad_drop_timeout_hack)
	{
		timeout = quad_drop_timeout_hack;
		quad_drop_timeout_hack = 0;
	}
	else
	{
		timeout = 300;
	}

	if (ent->r.client->quad_framenum > level.framenum)
		ent->r.client->quad_framenum += timeout;
	else
		ent->r.client->quad_framenum = level.framenum + timeout;

	G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/damage.wav"), 1, ATTN_NORM);
}

//======================================================================

void Use_Breather (edict_t *ent, gitem_t *item)
{
	ent->r.client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	if (ent->r.client->breather_framenum > level.framenum)
		ent->r.client->breather_framenum += 300;
	else
		ent->r.client->breather_framenum = level.framenum + 300;

//	G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void Use_Envirosuit (edict_t *ent, gitem_t *item)
{
	ent->r.client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	if (ent->r.client->enviro_framenum > level.framenum)
		ent->r.client->enviro_framenum += 300;
	else
		ent->r.client->enviro_framenum = level.framenum + 300;

//	G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void	Use_Invulnerability (edict_t *ent, gitem_t *item)
{
	ent->r.client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);

	if (ent->r.client->invincible_framenum > level.framenum)
		ent->r.client->invincible_framenum += 300;
	else
		ent->r.client->invincible_framenum = level.framenum + 300;

	G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/protect.wav"), 1, ATTN_NORM);
}

//======================================================================

void Use_Silencer (edict_t *ent, gitem_t *item)
{
	ent->r.client->pers.inventory[ITEM_INDEX(item)]--;
	ValidateSelectedItem (ent);
	ent->r.client->silencer_shots += 30;
}

//======================================================================

qboolean Add_Ammo (edict_t *ent, gitem_t *item, int count)
{
	int			index;
	int			max;

	if (!ent->r.client)
		return qfalse;

	if (item->tag == AMMO_BULLETS)
		max = ent->r.client->pers.max_bullets;
	else if (item->tag == AMMO_SHELLS)
		max = ent->r.client->pers.max_shells;
	else if (item->tag == AMMO_ROCKETS)
		max = ent->r.client->pers.max_rockets;
	else if (item->tag == AMMO_GRENADES)
		max = ent->r.client->pers.max_grenades;
	else if (item->tag == AMMO_CELLS)
		max = ent->r.client->pers.max_cells;
	else if (item->tag == AMMO_SLUGS)
		max = ent->r.client->pers.max_slugs;
	else
		return qfalse;

	index = ITEM_INDEX(item);

	if (ent->r.client->pers.inventory[index] == max)
		return qfalse;

	ent->r.client->pers.inventory[index] += count;

	if (ent->r.client->pers.inventory[index] > max)
		ent->r.client->pers.inventory[index] = max;

	return qtrue;
}

qboolean Pickup_Ammo (edict_t *ent, edict_t *other)
{
	int			oldcount;
	int			count;
	qboolean	weapon;

	weapon = (ent->item->flags & IT_WEAPON);
	if ( (weapon) && ( dmflags->integer & DF_INFINITE_AMMO ) )
		count = 1000;
	else if (ent->count)
		count = ent->count;
	else
		count = ent->item->quantity;

	oldcount = other->r.client->pers.inventory[ITEM_INDEX(ent->item)];

	if (!Add_Ammo (other, ent->item, count))
		return qfalse;

	if (weapon && !oldcount)
	{
		if (other->r.client->pers.weapon != ent->item && ( !deathmatch->integer || other->r.client->pers.weapon == FindItem("blaster") ) )
			other->r.client->newweapon = ent->item;
	}

	if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)) && (deathmatch->integer))
		SetRespawn (ent, 30);
	return qtrue;
}

void Drop_Ammo (edict_t *ent, gitem_t *item)
{
	edict_t	*dropped;
	int		index;

	index = ITEM_INDEX(item);
	dropped = Drop_Item (ent, item);
	if (ent->r.client->pers.inventory[index] >= item->quantity)
		dropped->count = item->quantity;
	else
		dropped->count = ent->r.client->pers.inventory[index];
	ent->r.client->pers.inventory[index] -= dropped->count;
	ValidateSelectedItem (ent);
}


//======================================================================

void MegaHealth_think (edict_t *self)
{
	if (self->r.owner->health > self->r.owner->max_health
//ZOID
		&& !CTFHasRegeneration(self->r.owner)
//ZOID
		)
	{
		self->nextthink = level.time + 1;
		self->r.owner->health -= 1;
		return;
	}

	if (!(self->spawnflags & DROPPED_ITEM) && (deathmatch->integer))
		SetRespawn (self, 20);
	else
		G_FreeEdict (self);
}

qboolean Pickup_Health (edict_t *ent, edict_t *other)
{
	if (!(ent->style & HEALTH_IGNORE_MAX))
		if (other->health >= other->max_health)
			return qfalse;

//ZOID
	if (other->health >= 250 && ent->count > 25)
		return qfalse;
//ZOID

	other->health += ent->count;

//ZOID
	if (other->health > 250 && ent->count > 25)
		other->health = 250;
//ZOID

	if (!(ent->style & HEALTH_IGNORE_MAX))
	{
		if (other->health > other->max_health)
			other->health = other->max_health;
	}

//ZOID
	if ((ent->style & HEALTH_TIMED)
		&& !CTFHasRegeneration(other)
//ZOID
	)
	{
		ent->think = MegaHealth_think;
		ent->nextthink = level.time + 5;
		ent->r.owner = other;
		ent->flags |= FL_RESPAWN;
		ent->r.svflags |= SVF_NOCLIENT;
		ent->r.solid = SOLID_NOT;
	}
	else
	{
		if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->integer))
			SetRespawn (ent, 30);
	}

	return qtrue;
}

//======================================================================

int ArmorIndex (edict_t *ent)
{
	if (!ent->r.client)
		return 0;

	if (ent->r.client->pers.inventory[jacket_armor_index] > 0)
		return jacket_armor_index;
	if (ent->r.client->pers.inventory[combat_armor_index] > 0)
		return combat_armor_index;
	if (ent->r.client->pers.inventory[body_armor_index] > 0)
		return body_armor_index;

	return 0;
}

qboolean Pickup_Armor (edict_t *ent, edict_t *other)
{
	int				old_armor_index;
	gitem_armor_t	*oldinfo;
	gitem_armor_t	*newinfo;
	int				newcount;
	float			salvage;
	int				salvagecount;

	// get info on new armor
	newinfo = (gitem_armor_t *)ent->item->info;

	old_armor_index = ArmorIndex (other);

	// handle armor shards specially
	if (ent->item->tag == ARMOR_SHARD)
	{
		if (!old_armor_index)
			other->r.client->pers.inventory[jacket_armor_index] = 2;
		else
			other->r.client->pers.inventory[old_armor_index] += 2;
	}

	// if player has no armor, just use it
	else if (!old_armor_index)
	{
		other->r.client->pers.inventory[ITEM_INDEX(ent->item)] = newinfo->base_count;
	}

	// use the better armor
	else
	{
		// get info on old armor
		if (old_armor_index == jacket_armor_index)
			oldinfo = &jacketarmor_info;
		else if (old_armor_index == combat_armor_index)
			oldinfo = &combatarmor_info;
		else // (old_armor_index == body_armor_index)
			oldinfo = &bodyarmor_info;

		if (newinfo->normal_protection > oldinfo->normal_protection)
		{
			// calc new armor values
			salvage = oldinfo->normal_protection / newinfo->normal_protection;
			salvagecount = salvage * other->r.client->pers.inventory[old_armor_index];
			newcount = newinfo->base_count + salvagecount;
			if (newcount > newinfo->max_count)
				newcount = newinfo->max_count;

			// zero count of old armor so it goes away
			other->r.client->pers.inventory[old_armor_index] = 0;

			// change armor to new item with computed value
			other->r.client->pers.inventory[ITEM_INDEX(ent->item)] = newcount;
		}
		else
		{
			// calc new armor values
			salvage = newinfo->normal_protection / oldinfo->normal_protection;
			salvagecount = salvage * newinfo->base_count;
			newcount = other->r.client->pers.inventory[old_armor_index] + salvagecount;
			if (newcount > oldinfo->max_count)
				newcount = oldinfo->max_count;

			// if we're already maxed out then we don't need the new armor
			if (other->r.client->pers.inventory[old_armor_index] >= newcount)
				return qfalse;

			// update current armor value
			other->r.client->pers.inventory[old_armor_index] = newcount;
		}
	}

	if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->integer))
		SetRespawn (ent, 20);

	return qtrue;
}

//======================================================================

int PowerArmorType (edict_t *ent)
{
	if (!ent->r.client)
		return POWER_ARMOR_NONE;

	if (!(ent->flags & FL_POWER_ARMOR))
		return POWER_ARMOR_NONE;

	if (ent->r.client->pers.inventory[power_shield_index] > 0)
		return POWER_ARMOR_SHIELD;
	if (ent->r.client->pers.inventory[power_screen_index] > 0)
		return POWER_ARMOR_SCREEN;

	return POWER_ARMOR_NONE;
}

void Use_PowerArmor (edict_t *ent, gitem_t *item)
{
	int		index;

	if (ent->flags & FL_POWER_ARMOR)
	{
		ent->flags &= ~FL_POWER_ARMOR;
		G_Sound (ent, CHAN_AUTO, trap_SoundIndex("sound/misc/power2.wav"), 1, ATTN_NORM);
	}
	else
	{
		index = ITEM_INDEX(FindItem("cells"));
		if (!ent->r.client->pers.inventory[index])
		{
			G_PrintMsg (ent, PRINT_HIGH, "No cells for power armor.\n");
			return;
		}
		ent->flags |= FL_POWER_ARMOR;
		G_Sound (ent, CHAN_AUTO, trap_SoundIndex("sound/misc/power1.wav"), 1, ATTN_NORM);
	}
}

qboolean Pickup_PowerArmor (edict_t *ent, edict_t *other)
{
	int		quantity;

	quantity = other->r.client->pers.inventory[ITEM_INDEX(ent->item)];

	other->r.client->pers.inventory[ITEM_INDEX(ent->item)]++;

	if (deathmatch->integer)
	{
		if (!(ent->spawnflags & DROPPED_ITEM) )
			SetRespawn (ent, ent->item->quantity);
		// auto-use for DM only if we didn't already have one
		if (!quantity)
			ent->item->use (other, ent->item);
	}

	return qtrue;
}

void Drop_PowerArmor (edict_t *ent, gitem_t *item)
{
	if ((ent->flags & FL_POWER_ARMOR) && (ent->r.client->pers.inventory[ITEM_INDEX(item)] == 1))
		Use_PowerArmor (ent, item);
	Drop_General (ent, item);
}

//======================================================================

/*
===============
Touch_Item
===============
*/
void Touch_Item (edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags)
{
	qboolean	taken;

	if (!other->r.client)
		return;
	if (other->health < 1)
		return;		// dead people can't pickup
	if (!ent->item->pickup)
		return;		// not a grabbable item?

	if (CTFMatchSetup())
		return; // can't pick stuff up right now

	taken = ent->item->pickup(ent, other);

	if (taken)
	{
		// flash the screen
		other->r.client->bonus_alpha = 0.25;	

		// show icon and name on status bar
		other->r.client->ps.stats[STAT_PICKUP_ICON] = trap_ImageIndex (ent->item->icon);
		other->r.client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS+ITEM_INDEX(ent->item);
		other->r.client->pickup_msg_time = level.time + 3.0;

		// change selected item
		if (ent->item->use)
			other->r.client->pers.selected_item = other->r.client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(ent->item);

		if (ent->item->pickup_sound)
		{
			if (ent->item->flags & IT_POWERUP)
				G_Sound (other, CHAN_ITEM, trap_SoundIndex(ent->item->pickup_sound), 1, ATTN_NORM);
			else
				G_Sound (other, CHAN_AUTO, trap_SoundIndex(ent->item->pickup_sound), 1, ATTN_NORM);
		}
	}

	if (!(ent->spawnflags & ITEM_TARGETS_USED))
	{
		G_UseTargets (ent, other);
		ent->spawnflags |= ITEM_TARGETS_USED;
	}

	if (!taken)
		return;

	if (!((coop->integer) && (ent->item->flags & IT_STAY_COOP)) || (ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)))
	{
		if (ent->flags & FL_RESPAWN)
			ent->flags &= ~FL_RESPAWN;
		else
			G_FreeEdict (ent);
	}
}

//======================================================================

static void drop_temp_touch (edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags)
{
	if (other == ent->r.owner)
		return;
	Touch_Item (ent, other, plane, surfFlags);
}

static void drop_make_touchable (edict_t *ent)
{
	ent->touch = Touch_Item;
	if (deathmatch->integer)
	{
		ent->nextthink = level.time + 29;
		ent->think = G_FreeEdict;
	}
}

edict_t *Drop_Item (edict_t *ent, gitem_t *item)
{
	edict_t	*dropped;
	vec3_t	forward, right;
	vec3_t	offset;

	dropped = G_Spawn();
	dropped->classname = item->classname;
	dropped->item = item;
	dropped->spawnflags = DROPPED_ITEM;
	dropped->s.effects = item->world_model_flags;
	VectorSet (dropped->r.mins, -15, -15, -15);
	VectorSet (dropped->r.maxs, 15, 15, 15);
	dropped->r.solid = SOLID_TRIGGER;
	dropped->movetype = MOVETYPE_TOSS;  
	dropped->touch = drop_temp_touch;
	dropped->r.owner = ent;

	if ( (dropped->item->flags & ~IT_STAY_COOP) == IT_WEAPON ) {
		dropped->s.renderfx = RF_SCALEHACK;
	} else {
		dropped->s.renderfx = 0;
	}

	dropped->s.modelindex = trap_ModelIndex (dropped->item->world_model[0]);
	dropped->s.modelindex2 = trap_ModelIndex (dropped->item->world_model[1]);
	dropped->s.modelindex3 = trap_ModelIndex (dropped->item->world_model[2]);

	if (ent->r.client)
	{
		trace_t	trace;

		AngleVectors (ent->r.client->v_angle, forward, right, NULL);
		VectorSet(offset, 24, 0, -16);
		G_ProjectSource (ent->s.origin, offset, forward, right, dropped->s.origin);
		trap_Trace (&trace, ent->s.origin, dropped->r.mins, dropped->r.maxs,
			dropped->s.origin, ent, CONTENTS_SOLID);
		VectorCopy (trace.endpos, dropped->s.origin);
	}
	else
	{
		AngleVectors (ent->s.angles, forward, right, NULL);
		VectorCopy (ent->s.origin, dropped->s.origin);
	}

	VectorScale (forward, 100, dropped->velocity);
	dropped->velocity[2] = 300;

	dropped->think = drop_make_touchable;
	dropped->nextthink = level.time + 1;

	trap_LinkEntity (dropped);

	return dropped;
}

void Use_Item (edict_t *ent, edict_t *other, edict_t *activator)
{
	ent->r.svflags &= ~SVF_NOCLIENT;
	ent->use = NULL;

	if (ent->spawnflags & ITEM_NO_TOUCH)
	{
		ent->r.solid = SOLID_BBOX;
		ent->touch = NULL;
	}
	else
	{
		ent->r.solid = SOLID_TRIGGER;
		ent->touch = Touch_Item;
	}

	trap_LinkEntity (ent);
}

//======================================================================

/*
================
Finish_SpawningItem
================
*/
void Finish_SpawningItem (edict_t *ent)
{
	trace_t		tr;
	vec3_t		dest;

	VectorSet ( ent->r.mins, -15, -15, -15 );
	VectorSet ( ent->r.maxs, 15, 15, 15 );

	if ( ent->model ) {
		ent->s.modelindex = trap_ModelIndex ( ent->model );
	} else {
		if ( ent->item->world_model[0] )
			ent->s.modelindex = trap_ModelIndex (ent->item->world_model[0]);
		if ( ent->item->world_model[1] )
			ent->s.modelindex2 = trap_ModelIndex (ent->item->world_model[1]);
		if ( ent->item->world_model[2] )
			ent->s.modelindex3 = trap_ModelIndex (ent->item->world_model[2]);
	}

	if ( (ent->item->flags & ~IT_STAY_COOP) == IT_WEAPON ) {
		ent->s.renderfx = RF_SCALEHACK;
	} else {
		ent->s.renderfx = 0;
	}

	ent->r.solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;  
	ent->touch = Touch_Item;

	if ( !(ent->spawnflags & 1) ) {	// droptofloor
		VectorSet ( dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 128 );

		 trap_Trace ( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent, MASK_SOLID );

		if ( tr.startsolid ) {
			if (developer->integer)
				G_Printf ( "droptofloor: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin) );
			G_FreeEdict ( ent );
			return;
		}

		VectorCopy ( tr.endpos, ent->s.origin );
	} else {
		ent->gravity = 0;
	}

	if ( ent->team ) {
		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamchain;
		ent->teamchain = NULL;

		ent->r.svflags |= SVF_NOCLIENT;
		ent->r.solid = SOLID_NOT;

		if ( ent == ent->teammaster ) {
			ent->nextthink = level.time + FRAMETIME;
			ent->think = DoRespawn;
		}
	}

	trap_LinkEntity ( ent );
}


/*
===============
PrecacheItem

Precaches all data needed for a given item.
This will be called for each item spawned in a level,
and for each item in each client's inventory.
===============
*/
void PrecacheItem (gitem_t *it)
{
	int i;
	char	*s, *start;
	char	data[MAX_QPATH];
	int		len;
	gitem_t	*ammo;

	if (!it)
		return;

	if (it->pickup_sound)
		trap_SoundIndex (it->pickup_sound);
	for ( i = 0; i < MAX_ITEM_MODELS; i++ ) {
		if (it->world_model[i])
			trap_ModelIndex (it->world_model[i]);
	}

	if (it->view_model)
		trap_ModelIndex (it->view_model);
	if (it->icon)
		trap_ImageIndex (it->icon);

	// parse everything for its ammo
	if (it->ammo && it->ammo[0])
	{
		ammo = FindItem (it->ammo);
		if (ammo != it)
			PrecacheItem (ammo);
	}

	// parse the space separated precache string for other items
	s = it->precaches;
	if (!s || !s[0])
		return;

	while (*s)
	{
		start = s;
		while (*s && *s != ' ')
			s++;

		len = s-start;
		if (len >= MAX_QPATH || len < 5)
			G_Error ("PrecacheItem: %s has bad precache string", it->classname);
		memcpy (data, start, len);
		data[len] = 0;
		if (*s)
			s++;

		// determine type based on extension
		if (!strcmp(data+len-3, "md2"))
			trap_ModelIndex (data);
		else if (!strcmp(data+len-3, "md3"))
			trap_ModelIndex (data);
		else if (!strcmp(data+len-3, "dpm"))
			trap_ModelIndex (data);
		else if (!strcmp(data+len-3, "sp2"))
			trap_ModelIndex (data);
		else if (!strcmp(data+len-3, "wav"))
			trap_SoundIndex (data);
		else if (!strcmp(data+len-3, "pcx"))
			trap_ImageIndex (data);
		else if (!strcmp(data+len-3, "tga"))
			trap_ImageIndex (data);
		else if (!strcmp(data+len-3, "jpg"))
			trap_ImageIndex (data);
	}
}

/*
============
SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void SpawnItem (edict_t *ent, gitem_t *item)
{
	PrecacheItem (item);

	// some items will be prevented in deathmatch
	if (deathmatch->integer)
	{
		if ( dmflags->integer & DF_NO_ARMOR )
		{
			if (item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor)
			{
				G_FreeEdict (ent);
				return;
			}
		}
		if ( dmflags->integer & DF_NO_ITEMS )
		{
			if (item->pickup == Pickup_Powerup)
			{
				G_FreeEdict (ent);
				return;
			}
		}
		if ( dmflags->integer & DF_NO_HEALTH )
		{
			if (item->pickup == Pickup_Health || item->pickup == Pickup_Adrenaline || item->pickup == Pickup_AncientHead)
			{
				G_FreeEdict (ent);
				return;
			}
		}
		if ( dmflags->integer & DF_INFINITE_AMMO )
		{
			if ( (item->flags == IT_AMMO) || (strcmp(ent->classname, "weapon_bfg") == 0) )
			{
				G_FreeEdict (ent);
				return;
			}
		}
		if ( dmflags->integer & DF_NO_HEALTH )
		{
			if ( item->flags == IT_HEALTH )
			{
				G_FreeEdict (ent);
				return;
			}
		}
	}

	// don't let them drop items that stay in a coop game
	if ((coop->integer) && (item->flags & IT_STAY_COOP))
	{
		item->drop = NULL;
	}

//ZOID
//Don't spawn the flags unless enabled
	if ( !ctf->integer && (item->flags & IT_FLAG) ) {
		G_FreeEdict(ent);
		return;
	}
//ZOID

	ent->item = item;
	ent->nextthink = level.time + 2 * FRAMETIME;    // items start after other solids
	ent->think = Finish_SpawningItem;
	ent->s.effects = item->world_model_flags;

//ZOID
//flags are server animated and have special handling
	if ( item->flags & IT_FLAG ) {
		ent->think = CTFFlagSetup;
	}
//ZOID

	if (strcmp(ent->classname, "item_health") == 0) {
		ent->count = 10;
	} else if (strcmp(ent->classname, "item_health_small") == 0) {
		ent->count = 2;
		ent->style = HEALTH_IGNORE_MAX;
	} else if (strcmp(ent->classname, "item_health_large") == 0) {
		ent->count = 25;
	} else if (strcmp(ent->classname, "item_health_mega") == 0) {
		ent->count = 100;
		ent->style = HEALTH_IGNORE_MAX|HEALTH_TIMED;
	}
}

//======================================================================

gitem_t	itemlist[] = 
{
	{
		NULL
	},	// leave index 0 alone

	//
	// ARMOR
	//

/*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_armor_body", 
		Pickup_Armor,
		NULL,
		NULL,
		NULL,
		"sound/misc/ar3_pkup.wav",
		{ "models/powerups/armor/armor_red.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/iconr_red",
/* pickup */	"Body Armor",
		0,
		NULL,
		IT_ARMOR,
		0,
		&bodyarmor_info,
		ARMOR_BODY,
/* precache */ ""
	},

/*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_armor_combat", 
		Pickup_Armor,
		NULL,
		NULL,
		NULL,
		"sound/misc/ar2_pkup.wav",
		{ "models/powerups/armor/armor_yel.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/iconr_yellow",
/* pickup */	"Combat Armor",
		0,
		NULL,
		IT_ARMOR,
		0,
		&combatarmor_info,
		ARMOR_COMBAT,
/* precache */ ""
	},

/*QUAKED item_armor_jacket (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_armor_jacket", 
		Pickup_Armor,
		NULL,
		NULL,
		NULL,
		"sound/misc/ar1_pkup.wav",
		{ "models/items/armor/jacket/tris.md2", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/i_jacketarmor",
/* pickup */	"Jacket Armor",
		0,
		NULL,
		IT_ARMOR,
		0,
		&jacketarmor_info,
		ARMOR_JACKET,
/* precache */ ""
	},

/*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_armor_shard", 
		Pickup_Armor,
		NULL,
		NULL,
		NULL,
		"sound/misc/ar1_pkup.wav",
		{ "models/powerups/armor/shard.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/iconr_shard",
/* pickup */	"Armor Shard",
		0,
		NULL,
		IT_ARMOR,
		0,
		NULL,
		ARMOR_SHARD,
/* precache */ ""
	},


/*QUAKED item_power_screen (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_power_screen", 
		Pickup_PowerArmor,
		Use_PowerArmor,
		Drop_PowerArmor,
		NULL,
		"sound/misc/ar3_pkup.wav",
		{ "models/items/armor/screen/tris.md2", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/i_powerscreen",
/* pickup */	"Power Screen",
		60,
		NULL,
		IT_ARMOR,
		0,
		NULL,
		0,
/* precache */ ""
	},

/*QUAKED item_power_shield (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_power_shield",
		Pickup_PowerArmor,
		Use_PowerArmor,
		Drop_PowerArmor,
		NULL,
		"sound/misc/ar3_pkup.wav",
		{ "models/items/armor/shield/tris.md2", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"i_powershield",
/* pickup */	"Power Shield",
		60,
		NULL,
		IT_ARMOR,
		0,
		NULL,
		0,
/* precache */ "sound/misc/power2.wav sound/misc/power1.wav"
	},


	//
	// WEAPONS 
	//

/* weapon_grapplinghook (.3 .3 1) (-16 -16 -16) (16 16 16)
always owned, never in the world
*/
	{
		"weapon_grapplinghook", 
		NULL,
		Use_Weapon,
		NULL,
		CTFWeapon_Grapple,
		"sound/misc/w_pkup.wav",
		{ "models/weapons2/grapple/grapple.md3", 0, 0 },
		EF_ROTATE_AND_BOB,
		"models/weapons/grapple/tris.md2",
/* icon */		"icons/iconw_grapple",
/* pickup */	"Grapple",
		0,
		NULL,
		IT_WEAPON,
		WEAP_GRAPPLE,
		NULL,
		0,
/* precache */ "sound/weapons/grapple/grfire.wav sound/weapons/grapple/grpull.wav sound/weapons/grapple/grhang.wav sound/weapons/grapple/grreset.wav sound/weapons/grapple/grhit.wav"
	},

/* weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16)
always owned, never in the world
*/
	{
		"weapon_blaster", 
		NULL,
		Use_Weapon,
		NULL,
		Weapon_Blaster,
		"sound/misc/w_pkup.wav",
		{ 0, 0, 0 }, 0,
		"models/weapons/v_blast/tris.md2",
/* icon */		"pics/w_blaster",
/* pickup */	"Blaster",
		0,
		NULL,
		IT_WEAPON|IT_STAY_COOP,
		WEAP_BLASTER,
		NULL,
		0,
/* precache */ "sound/weapons/blastf1a.wav sound/misc/lasfly.wav"
	},

/*QUAKED weapon_shotgun_q2 (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_shotgun_q2", 
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_Shotgun,
		"sound/misc/w_pkup.wav",
		{ "models/weapons/g_shotg/tris.md2", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		"models/weapons/v_shotg/tris.md2",
/* icon */		"pics/w_shotgun",
/* pickup */	"Shotgun",
		1,
		"Shells",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_SHOTGUN,
		NULL,
		0,
/* precache */ "sound/weapons/shotgf1b.wav sound/weapons/shotgr1b.wav"
	},

/*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_shotgun", 
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_SuperShotgun,
		"sound/misc/w_pkup.wav",
		{ "models/weapons2/shotgun/shotgun.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		"models/weapons/v_shotg2/tris.md2",
/* icon */		"icons/iconw_shotgun",
/* pickup */	"Super Shotgun",
		2,
		"Shells",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_SUPERSHOTGUN,
		NULL,
		0,
/* precache */ "sound/weapons/sshotf1b.wav"
	},

/*QUAKED weapon_supershotgun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_supershotgun", 
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_SuperShotgun,
		"sound/misc/w_pkup.wav",
		{ "models/weapons/g_shotg2/tris.md2", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		"models/weapons/v_shotg2/tris.md2",
/* icon */		"pics/w_sshotgun",
/* pickup */	"Super Shotgun",
		2,
		"Shells",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_SUPERSHOTGUN,
		NULL,
		0,
/* precache */ "sound/weapons/sshotf1b.wav"
	},

/*QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_machinegun", 
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_Machinegun,
		"sound/misc/w_pkup.wav",
		{ "models/weapons/g_machn/tris.md2", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		"models/weapons/v_machn/tris.md2",
/* icon */		"icons/iconw_machinegun",
/* pickup */	"Machinegun",
		1,
		"Bullets",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_MACHINEGUN,
		NULL,
		0,
/* precache */ "sound/weapons/machinegun/machgf1b.wav sound/weapons/machinegun/machgf2b.wav sound/weapons/machinegun/machgf3b.wav sound/weapons/machinegun/machgf4b.wav"
	},

/*QUAKED weapon_chaingun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_chaingun", 
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_Chaingun,
		"sound/misc/w_pkup.wav",
		{ "models/weapons/vulcan/vulcan.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		"models/weapons/v_chain/tris.md2",
/* icon */		"icons/iconw_chaingun",
/* pickup */	"Chaingun",
		1,
		"Bullets",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_CHAINGUN,
		NULL,
		0,
/* precache */ "sound/weapons/chngnu1a.wav sound/weapons/chngnl1a.wav sound/weapons/chngnd1a.wav sound/weapons/machinegun/machgf1b.wav sound/weapons/machinegun/machgf2b.wav sound/weapons/machinegun/machgf3b.wav sound/weapons/machinegun/machgf4b.wav"
	},

/*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"ammo_grenades",
		Pickup_Ammo,
		Use_Weapon,
		Drop_Ammo,
		Weapon_Grenade,
		"sound/misc/am_pkup.wav",
		{ "models/powerups/ammo/grenadeam.md3", 0, 0 },
		EF_ROTATE_AND_BOB,
		"models/weapons/v_handgr/tris.md2",
/* icon */		"icons/icona_grenade",
/* pickup */	"Grenades",
		5,
		"grenades",
		IT_AMMO|IT_WEAPON,
		WEAP_GRENADES,
		NULL,
		AMMO_GRENADES,
/* precache */ "sound/weapons/hgrent1a.wav sound/weapons/hgrena1b.wav sound/weapons/hgrenc1b.wav sound/weapons/hgrenb1a.wav sound/weapons/hgrenb2a.wav "
	},

/*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_grenadelauncher",
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_GrenadeLauncher,
		"sound/misc/w_pkup.wav",
		{ "models/weapons2/grenadel/grenadel.md3", 0, 0 },
		EF_ROTATE_AND_BOB,
		"models/weapons/v_launch/tris.md2",
/* icon */		"icons/iconw_grenade",
/* pickup */	"Grenade Launcher",
		1,
		"Grenades",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_GRENADELAUNCHER,
		NULL,
		0,
/* precache */ "sound/weapons/grenade/grenlf1a.wav"
	},

/*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_rocketlauncher",
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_RocketLauncher,
		"sound/misc/w_pkup.wav",
		{ "models/weapons2/rocketl/rocketl.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		"models/weapons/v_rocket/tris.md2",
/* icon */		"icons/iconw_rocket",
/* pickup */	"Rocket Launcher",
		1,
		"Rockets",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_ROCKETLAUNCHER,
		NULL,
		0,
/* precache */ "sound/weapons/rocket/rockfly.wav sound/weapons/rocket/rocklx1a.wav sound/weapons/rocket/rocklf1a.wav"
	},

/*QUAKED weapon_plasmagun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_plasmagun", 
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_HyperBlaster,
		"sound/misc/w_pkup.wav",
		{ "models/weapons2/plasma/plasma.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		"models/weapons/v_hyperb/tris.md2",
/* icon */		"icons/iconw_plasma",
/* pickup */	"Plasma Gun",
		1,
		"Cells",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_HYPERBLASTER,
		NULL,
		0,
/* precache */ "sound/weapons/hyprbu1a.wav sound/weapons/hyprbl1a.wav sound/weapons/hyprbf1a.wav sound/weapons/hyprbd1a.wav sound/misc/lasfly.wav"
	},

/*QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_railgun", 
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_Railgun,
		"sound/misc/w_pkup.wav",
		{ "models/weapons2/railgun/railgun.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		"models/weapons/v_rail/tris.md2",
/* icon */		"icons/iconw_railgun",
/* pickup */	"Railgun",
		1,
		"Slugs",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_RAILGUN,
		NULL,
		0,
/* precache */ "sound/weapons/railgun/rg_hum.wav"
	},

/*QUAKED weapon_bfg (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"weapon_bfg",
		Pickup_Weapon,
		Use_Weapon,
		Drop_Weapon,
		Weapon_BFG,
		"sound/misc/w_pkup.wav",
		{ "models/weapons2/bfg/bfg.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		"models/weapons/v_bfg/tris.md2",
/* icon */		"icons/iconw_bfg",
/* pickup */	"BFG10K",
		50,
		"Cells",
		IT_WEAPON|IT_STAY_COOP,
		WEAP_BFG,
		NULL,
		0,
/* precache */ "sound/weapons/bfg__f1y.wav sound/weapons/bfg__l1a.wav sound/weapons/bfg__x1b.wav sound/weapons/bfg_hum.wav"
	},

	//
	// AMMO ITEMS
	//

/*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"ammo_shells",
		Pickup_Ammo,
		NULL,
		Drop_Ammo,
		NULL,
		"sound/misc/am_pkup.wav",
		{ "models/powerups/ammo/shotgunam.md3", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/icona_shotgun",
/* pickup */	"Shells",
		10,
		NULL,
		IT_AMMO,
		0,
		NULL,
		AMMO_SHELLS,
/* precache */ ""
	},

/*QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"ammo_bullets",
		Pickup_Ammo,
		NULL,
		Drop_Ammo,
		NULL,
		"sound/misc/am_pkup.wav",
		{ "models/powerups/ammo/machinegunam.md3", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/icona_machinegun",
/* pickup */	"Bullets",
		50,
		NULL,
		IT_AMMO,
		0,
		NULL,
		AMMO_BULLETS,
/* precache */ ""
	},

/*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"ammo_cells",
		Pickup_Ammo,
		NULL,
		Drop_Ammo,
		NULL,
		"sound/misc/am_pkup.wav",
		{ "models/powerups/ammo/plasmaam.md3", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/icona_plasma",
/* pickup */	"Cells",
		50,
		NULL,
		IT_AMMO,
		0,
		NULL,
		AMMO_CELLS,
/* precache */ ""
	},

/*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"ammo_rockets",
		Pickup_Ammo,
		NULL,
		Drop_Ammo,
		NULL,
		"sound/misc/am_pkup.wav",
		{ "models/powerups/ammo/rocketam.md3", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/icona_rocket",
/* pickup */	"Rockets",
		5,
		NULL,
		IT_AMMO,
		0,
		NULL,
		AMMO_ROCKETS,
/* precache */ ""
	},

/*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"ammo_slugs",
		Pickup_Ammo,
		NULL,
		Drop_Ammo,
		NULL,
		"sound/misc/am_pkup.wav",
		{ "models/powerups/ammo/railgunam.md3", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/icona_railgun",
/* pickup */	"Slugs",
		10,
		NULL,
		IT_AMMO,
		0,
		NULL,
		AMMO_SLUGS,
/* precache */ ""
	},


	//
	// POWERUP ITEMS
	//
/*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_quad", 
		Pickup_Powerup,
		Use_Quad,
		Drop_General,
		NULL,
		"sound/items/pkup.wav",
		{ "models/powerups/instant/quad.md3", 
		"models/powerups/instant/quad_ring.md3", 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/quad",
/* pickup */	"Quad Damage",
		60,
		NULL,
		IT_POWERUP,
		0,
		NULL,
		0,
/* precache */ "sound/items/damage.wav sound/items/damage2.wav sound/items/damage3.wav"
	},

/*QUAKED item_invulnerability (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_invulnerability",
		Pickup_Powerup,
		Use_Invulnerability,
		Drop_General,
		NULL,
		"sound/items/pkup.wav",
		{ "models/items/invulner/tris.md2", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/p_invulnerability",
/* pickup */	"Invulnerability",
		300,
		NULL,
		IT_POWERUP,
		0,
		NULL,
		0,
/* precache */ "sound/items/protect.wav sound/items/protect2.wav sound/items/protect4.wav"
	},

/*QUAKED item_silencer (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_silencer",
		Pickup_Powerup,
		Use_Silencer,
		Drop_General,
		NULL,
		"sound/items/pkup.wav",
		{ "models/items/silencer/tris.md2", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/p_silencer",
/* pickup */	"Silencer",
		60,
		NULL,
		IT_POWERUP,
		0,
		NULL,
		0,
/* precache */ ""
	},

/*QUAKED item_breather (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_breather",
		Pickup_Powerup,
		Use_Breather,
		Drop_General,
		NULL,
		"sound/items/pkup.wav",
		{ "models/items/breather/tris.md2", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/p_rebreather",
/* pickup */	"Rebreather",
		60,
		NULL,
		IT_STAY_COOP|IT_POWERUP,
		0,
		NULL,
		0,
/* precache */ "sound/items/airout.wav"
	},

/*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_enviro",
		Pickup_Powerup,
		Use_Envirosuit,
		Drop_General,
		NULL,
		"sound/items/pkup.wav",
		{ "models/powerups/instant/enviro.md3", 
		"models/powerups/instant/enviro_ring.md3", 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/envirosuit",
/* pickup */	"Battle Suit",
		60,
		NULL,
		IT_STAY_COOP|IT_POWERUP,
		0,
		NULL,
		0,
/* precache */ "sound/items/airout.wav"
	},

/*QUAKED item_ancient_head (.3 .3 1) (-16 -16 -16) (16 16 16)
Special item that gives +2 to maximum health
*/
	{
		"item_ancient_head",
		Pickup_AncientHead,
		NULL,
		NULL,
		NULL,
		"sound/items/pkup.wav",
		{ "models/items/c_head/tris.md2", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/i_fixme",
/* pickup */	"Ancient Head",
		60,
		NULL,
		0,
		0,
		NULL,
		0,
/* precache */ ""
	},

/*QUAKED holdable_medkit (.3 .3 1) (-16 -16 -16) (16 16 16)
gives +1 to maximum health
*/
	{
		"holdable_medkit",
		Pickup_Adrenaline,
		NULL,
		NULL,
		NULL,
		"sound/items/use_medkit.wav",
		{ "models/powerups/holdable/medkit.md3", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/medkit",
/* pickup */	"Medkit",
		60,
		NULL,
		0,
		0,
		NULL,
		0,
/* precache */ ""
	},

/*QUAKED item_bandolier (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_bandolier",
		Pickup_Bandolier,
		NULL,
		NULL,
		NULL,
		"sound/items/pkup.wav",
		{ "models/items/band/tris.md2", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/p_bandolier",
/* pickup */	"Bandolier",
		60,
		NULL,
		0,
		0,
		NULL,
		0,
/* precache */ ""
	},

/*QUAKED item_pack (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_pack",
		Pickup_Pack,
		NULL,
		NULL,
		NULL,
		"sound/items/pkup.wav",
		{ "models/items/pack/tris.md2", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/i_pack",
/* pickup */	"Ammo Pack",
		180,
		NULL,
		0,
		0,
		NULL,
		0,
/* precache */ ""
	},

/*QUAKED item_pack (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_pack",
		Pickup_Pack,
		NULL,
		NULL,
		NULL,
		"sound/items/pkup.wav",
		{ "models/items/pack/tris.md2", 0, 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/i_pack",
/* pickup */	"Ammo Pack",
		180,
		NULL,
		0,
		0,
		NULL,
		0,
/* precache */ ""
	},
	
/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_health",
		Pickup_Health,
		NULL,
		NULL,
		NULL,
		"sound/items/n_health.wav",
		{ "models/powerups/health/medium_cross.md3", 
		"models/powerups/health/medium_sphere.md3", 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/iconh_green",
/* pickup */	"10 Health",
		0,
		NULL,
		IT_HEALTH,
		0,
		NULL,
		0,
/* precache */ "sound/items/n_health.wav"
	},

/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_health_small",
		Pickup_Health,
		NULL,
		NULL,
		NULL,
		"sound/items/s_health.wav",
		{ "models/powerups/health/small_cross.md3", 
		"models/powerups/health/small_sphere.md3", 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/iconh_yellow",
/* pickup */	"2 Health",
		0,
		NULL,
		IT_HEALTH,
		0,
		NULL,
		0,
/* precache */ "sound/items/s_health.wav"
	},

/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_health_large",
		Pickup_Health,
		NULL,
		NULL,
		NULL,
		"sound/items/l_health.wav",
		{ "models/powerups/health/large_cross.md3", 
		"models/powerups/health/large_sphere.md3", 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/iconh_red",
/* pickup */	"25 Health",
		0,
		NULL,
		IT_HEALTH,
		0,
		NULL,
		0,
/* precache */ "sound/items/l_health.wav"
},

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		"item_health_mega",
		Pickup_Health,
		NULL,
		NULL,
		NULL,
		"sound/items/m_health.wav",
		{ "models/powerups/health/mega_cross.md3", 
		"models/powerups/health/mega_sphere.md3", 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/iconh_mega",
/* pickup */	"Mega Health",
		0,
		NULL,
		IT_HEALTH,
		0,
		NULL,
		0,
/* precache */ "sound/items/m_health.wav"
},

//ZOID
/*QUAKED team_CTF_redflag (1 0.2 0) (-16 -16 -24) (16 16 32)
*/
	{
		"team_CTF_redflag",
		CTFPickup_Flag,
		NULL,
		CTFDrop_Flag, // Should this be null if we don't want players to drop it manually?
		NULL,
		NULL,
		{ "models/flags/r_flag.md3", 0, 0 }, 
		EF_FLAG1|EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/iconf_red1",
/* pickup */	"Red Flag",
		0,
		NULL,
		IT_FLAG,
		0,
		NULL,
		0,
/* precache */ "sound/ctf/flagcap.wav"
	},

/*QUAKED team_CTF_blueflag (1 0.2 0) (-16 -16 -24) (16 16 32)
*/
	{
		"team_CTF_blueflag",
		CTFPickup_Flag,
		NULL,
		CTFDrop_Flag, // Should this be null if we don't want players to drop it manually?
		NULL,
		NULL,
		{ "models/flags/b_flag.md3", 0, 0 }, 
		EF_FLAG2|EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/iconf_blu1",
/* pickup */	"Blue Flag",
		0,
		NULL,
		IT_FLAG,
		0,
		NULL,
		0,
/* precache */ "sound/ctf/flagcap.wav"
	},

/* Resistance Tech */
	{
		"item_tech1",
		CTFPickup_Tech,
		NULL,
		CTFDrop_Tech, // Should this be null if we don't want players to drop it manually?
		NULL,
		"sound/items/pkup.wav",
		{ "models/ctf/resistance/tris.md2", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/tech1",
/* pickup */	"Disruptor Shield",
		0,
		NULL,
		IT_TECH,
		0,
		NULL,
		0,
/* precache */ "sound/ctf/tech1.wav"
	},

/* Strength Tech */
	{
		"item_tech2",
		CTFPickup_Tech,
		NULL,
		CTFDrop_Tech, // Should this be null if we don't want players to drop it manually?
		NULL,
		"sound/items/pkup.wav",
		{ "models/ctf/strength/tris.md2", 0, 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"pics/tech2",
/* pickup */	"Power Amplifier",
		0,
		NULL,
		IT_TECH,
		0,
		NULL,
		0,
/* precache */ "sound/ctf/tech2.wav sound/ctf/tech2x.wav"
	},

/* Haste Tech */
	{
		"item_tech3",
		CTFPickup_Tech,
		NULL,
		CTFDrop_Tech, // Should this be null if we don't want players to drop it manually?
		NULL,
		"sound/items/pkup.wav",
		{ "models/powerups/instant/haste.md3", 
		"models/powerups/instant/haste_ring.md3", 0 }, 
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/haste",
/* pickup */	"Time Accel",
		0,
		NULL,
		IT_TECH,
		0,
		NULL,
		0,
/* precache */ "sound/ctf/tech3.wav"
	},

/* Regeneration Tech */
	{
		"item_tech4",
		CTFPickup_Tech,
		NULL,
		CTFDrop_Tech, // Should this be null if we don't want players to drop it manually?
		NULL,
		"sound/items/pkup.wav",
		{ "models/powerups/instant/regen.md3", 
		"models/powerups/instant/regen_ring.md3", 0 },
		EF_ROTATE_AND_BOB,
		NULL,
/* icon */		"icons/regen",
/* pickup */	"AutoDoc",
		0,
		NULL,
		IT_TECH,
		0,
		NULL,
		0,
/* precache */ "sound/ctf/tech4.wav"
	},

//ZOID

	// end of list marker
	{NULL}
};

void InitItems (void)
{
	game.num_items = sizeof(itemlist)/sizeof(itemlist[0]) - 1;
}


/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames (void)
{
	int		i;
	gitem_t	*it;

	for (i=0 ; i<game.num_items ; i++)
	{
		it = &itemlist[i];
		trap_ConfigString (CS_ITEMS+i, it->pickup_name);
	}

	jacket_armor_index = ITEM_INDEX(FindItem("Jacket Armor"));
	combat_armor_index = ITEM_INDEX(FindItem("Combat Armor"));
	body_armor_index   = ITEM_INDEX(FindItem("Body Armor"));
	power_screen_index = ITEM_INDEX(FindItem("Power Screen"));
	power_shield_index = ITEM_INDEX(FindItem("Power Shield"));
}
