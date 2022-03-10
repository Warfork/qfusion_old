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



/*
======================================================================

INTERMISSION

======================================================================
*/

void MoveClientToIntermission (edict_t *ent)
{
	gclient_t *client;

	client = ent->r.client;
	if (deathmatch->value || coop->value)
		ent->r.client->showscores = qtrue;
	VectorCopy (level.intermission_origin, ent->s.origin);
	client->ps.pmove.origin[0] = level.intermission_origin[0]*16;
	client->ps.pmove.origin[1] = level.intermission_origin[1]*16;
	client->ps.pmove.origin[2] = level.intermission_origin[2]*16;
	VectorCopy (level.intermission_angle, client->ps.viewangles);
	client->ps.pmove.pm_type = PM_FREEZE;
	client->ps.gunindex = 0;
	client->ps.blend[3] = 0;

	// clean up powerup info
	client->quad_framenum = 0;
	client->invincible_framenum = 0;
	client->breather_framenum = 0;
	client->enviro_framenum = 0;
	client->grenade_blew_up = qfalse;
	client->grenade_time = 0;

	ent->viewheight = 0;
	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.modelindex = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->s.light = 0;
	ent->r.solid = SOLID_NOT;

	// add the layout
	if (deathmatch->value || coop->value)
		DeathmatchScoreboardMessage (ent, NULL);
}

void BeginIntermission (edict_t *targ)
{
	int		i;
	edict_t	*ent, *client;

	if (level.intermissiontime)
		return;		// already activated

//ZOID
	if (deathmatch->value && ctf->value)
		CTFCalcScores();
//ZOID

	game.autosaved = qfalse;

	// respawn any dead clients
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = game.edicts + 1 + i;
		if (!client->r.inuse)
			continue;
		if (client->health <= 0)
			respawn(client);
	}

	level.intermissiontime = level.time;
	level.changemap = targ->map;

	if (!strstr(level.changemap, "*"))
	{
		if (!deathmatch->value)
		{
			level.exitintermission = 1;		// go immediately to the next level
			return;
		}
	}

	level.exitintermission = 0;

	// find an intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	if (!ent)
	{	// the map creator forgot to put in an intermission point...
		ent = G_Find (NULL, FOFS(classname), "info_player_start");
		if (!ent)
			ent = G_Find (NULL, FOFS(classname), "info_player_deathmatch");
	}
	else
	{	// chose one of four spots
		i = rand() & 3;
		while (i--)
		{
			ent = G_Find (ent, FOFS(classname), "info_player_intermission");
			if (!ent)	// wrap around the list
				ent = G_Find (ent, FOFS(classname), "info_player_intermission");
		}
	}

	VectorCopy (ent->s.origin, level.intermission_origin);
	VectorCopy (ent->s.angles, level.intermission_angle);

	// move all clients to the intermission point
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = game.edicts + 1 + i;
		if (!client->r.inuse)
			continue;
		MoveClientToIntermission (client);
	}
}


/*
==================
DeathmatchScoreboardMessage

==================
*/
char *DeathmatchScoreboardMessage (edict_t *ent, edict_t *killer)
{
	char	entry[MAX_TOKEN_CHARS];
	static char	string[MAX_STRING_CHARS];
	int		stringlength;
	int		i, j, k;
	int		sorted[MAX_CLIENTS];
	int		sortedscores[MAX_CLIENTS];
	int		score, total;
	int		picnum;
	int		x, y;
	gclient_t	*cl;
	edict_t		*cl_ent;
	int		tag;

//ZOID
	if (ctf->value) {
		return CTFScoreboardMessage (ent, killer);
	}
//ZOID

	// sort the clients by score
	total = 0;
	for (i=0 ; i<game.maxclients ; i++)
	{
		cl_ent = game.edicts + 1 + i;
		if (!cl_ent->r.inuse)
			continue;
		score = game.clients[i].resp.score;
		for (j=0 ; j<total ; j++)
		{
			if (score > sortedscores[j])
				break;
		}
		for (k=total ; k>j ; k--)
		{
			sorted[k] = sorted[k-1];
			sortedscores[k] = sortedscores[k-1];
		}
		sorted[j] = i;
		sortedscores[j] = score;
		total++;
	}

	// print level name and exit rules
	string[0] = 0;

	stringlength = strlen(string);

	// add the clients in sorted order
	if (total > 12)
		total = 12;

	for (i=0 ; i<total ; i++)
	{
		cl = &game.clients[sorted[i]];
		cl_ent = game.edicts + 1 + sorted[i];

		picnum = trap_ImageIndex ("pics/i_fixme");
		x = (i>=6) ? 160 : 0;
		y = 32 + 64 * (i%6);

		// add a dogtag
		if (cl_ent == ent)
			tag = 0;
		else if (cl_ent == killer)
			tag = 1;
		else
			tag = 2;

		// send the layout
		Com_sprintf (entry, sizeof(entry),
			"client %i %i %i %i %i %i %i ",
			x, y, tag, sorted[i], cl->resp.score, cl->r.ping, (level.framenum - cl->resp.enterframe)/600);
		j = strlen(entry);
		if (stringlength + j > 1024)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	return string;
}


/*
==================
DeathmatchScoreboard

Draw instead of help message.
Note that it isn't that hard to overflow the 1400 byte message limit!
==================
*/
void DeathmatchScoreboard (edict_t *ent) {
	trap_Layout (ent, DeathmatchScoreboardMessage (ent, ent->enemy));
}


/*
==================
Cmd_Score_f

Display the scoreboard
==================
*/
void Cmd_Score_f (edict_t *ent)
{
	ent->r.client->showinventory = qfalse;
	ent->r.client->showhelp = qfalse;
//ZOID
	if (ent->r.client->menu)
		PMenu_Close(ent);
//ZOID

	if (!deathmatch->value && !coop->value)
		return;

	if (ent->r.client->showscores)
	{
		ent->r.client->showscores = qfalse;
		ent->r.client->update_chase = qtrue;
		return;
	}

	ent->r.client->showscores = qtrue;

	DeathmatchScoreboard (ent);
}


/*
==================
HelpComputer

Draw help computer.
==================
*/
void HelpComputer (edict_t *ent)
{
	char	string[MAX_STRING_CHARS];
	char	*sk;

	if (skill->value == 0)
		sk = "easy";
	else if (skill->value == 1)
		sk = "medium";
	else if (skill->value == 2)
		sk = "hard";
	else
		sk = "hard+";

	// send the layout
	Com_sprintf (string, sizeof(string),
		"size 32 32 "
		"xv 32 yv 8 picn pics/help "	// background
		"xv 202 yv 12 string \"%s%s\" "		// skill
		"xv 0 yv 24 cstring \"%s%s\" "		// level name
		"xv 0 yv 54 cstring \"%s%s\" "		// help 1
		"xv 0 yv 110 cstring \"%s%s\" "		// help 2
		"xv 50 yv 164 string \"%s kills     goals    secrets\" "
		"xv 50 yv 172 string \"%s%3i/%3i     %i/%i       %i/%i\" ", 
		S_COLOR_GREEN, sk,
		S_COLOR_GREEN, level.level_name,
		S_COLOR_GREEN, game.helpmessage1,
		S_COLOR_GREEN, game.helpmessage2, S_COLOR_GREEN,
		S_COLOR_GREEN, level.killed_monsters, level.total_monsters, 
		level.found_goals, level.total_goals,
		level.found_secrets, level.total_secrets);

	trap_Layout (ent, string);
}


/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f (edict_t *ent)
{
	// this is for backwards compatibility
	if (deathmatch->value)
	{
		Cmd_Score_f (ent);
		return;
	}

	ent->r.client->showinventory = qfalse;
	ent->r.client->showscores = qfalse;

	if (ent->r.client->showhelp && (ent->r.client->resp.game_helpchanged == game.helpchanged))
	{
		ent->r.client->showhelp = qfalse;
		return;
	}

	ent->r.client->showhelp = qtrue;
	ent->r.client->resp.helpchanged = 0;
	HelpComputer (ent);
}


//=======================================================================

/*
===============
G_SetStats
===============
*/
void G_SetStats (edict_t *ent)
{
	gitem_t		*item;
	int			index, cells;
	int			power_armor_type;
	gclient_t	*client = ent->r.client;

	//
	// health
	//
	client->ps.stats[STAT_HEALTH] = ent->health;
	client->r.health = client->ps.stats[STAT_HEALTH];
	client->r.frags = client->ps.stats[STAT_FRAGS];

	//
	// ammo
	//
	if (!client->ammo_index /* || !client->pers.inventory[client->ammo_index] */)
	{
		client->ps.stats[STAT_AMMO_ICON] = 0;
		client->ps.stats[STAT_AMMO] = 0;
	}
	else
	{
		item = &itemlist[client->ammo_index];
		client->ps.stats[STAT_AMMO_ICON] = trap_ImageIndex (item->icon);
		client->ps.stats[STAT_AMMO] = client->pers.inventory[client->ammo_index];
	}
	
	//
	// armor
	//
	power_armor_type = PowerArmorType (ent);
	if (power_armor_type)
	{
		cells = client->pers.inventory[ITEM_INDEX(FindItem ("cells"))];
		if (cells == 0)
		{	// ran out of cells for power armor
			ent->flags &= ~FL_POWER_ARMOR;
			G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/misc/power2.wav"), 1, ATTN_NORM);
			power_armor_type = 0;
		}
	}

	index = ArmorIndex (ent);
	if (power_armor_type && (!index || (level.framenum & 8) ) )
	{	// flash between power armor and other armor icon
		item = FindItem ("Power Shield");
		client->ps.stats[STAT_ARMOR_ICON] = trap_ModelIndex (item->world_model[0]);
		client->ps.stats[STAT_ARMOR] = cells;
	}
	else if (index)
	{
		item = GetItemByIndex (index);
		client->ps.stats[STAT_ARMOR_ICON] = trap_ModelIndex (item->world_model[0]);
		client->ps.stats[STAT_ARMOR] = client->pers.inventory[index];
	}
	else
	{
		client->ps.stats[STAT_ARMOR_ICON] = 0;
		client->ps.stats[STAT_ARMOR] = 0;
	}

	//
	// pickup message
	//
	if (level.time > client->pickup_msg_time)
	{
		client->ps.stats[STAT_PICKUP_ICON] = 0;
		client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	//
	// timers
	//
	if (client->quad_framenum > level.framenum)
	{
		client->ps.stats[STAT_TIMER_ICON] = trap_ImageIndex ("icons/quad");
		client->ps.stats[STAT_TIMER] = (client->quad_framenum - level.framenum)/10;
	}
	else if (client->invincible_framenum > level.framenum)
	{
		client->ps.stats[STAT_TIMER_ICON] = trap_ImageIndex ("pics/p_invulnerability");
		client->ps.stats[STAT_TIMER] = (client->invincible_framenum - level.framenum)/10;
	}
	else if (client->enviro_framenum > level.framenum)
	{
		client->ps.stats[STAT_TIMER_ICON] = trap_ImageIndex ("icons/envirosuit");
		client->ps.stats[STAT_TIMER] = (client->enviro_framenum - level.framenum)/10;
	}
	else if (client->breather_framenum > level.framenum)
	{
		client->ps.stats[STAT_TIMER_ICON] = trap_ImageIndex ("pics/p_rebreather");
		client->ps.stats[STAT_TIMER] = (client->breather_framenum - level.framenum)/10;
	}
	else
	{
		client->ps.stats[STAT_TIMER_ICON] = 0;
		client->ps.stats[STAT_TIMER] = 0;
	}

	//
	// selected item
	//
	if (client->pers.selected_item == -1)
		client->ps.stats[STAT_SELECTED_ICON] = 0;
	else
		client->ps.stats[STAT_SELECTED_ICON] = trap_ImageIndex (itemlist[client->pers.selected_item].icon);

	client->ps.stats[STAT_SELECTED_ITEM] = client->pers.selected_item;

	//
	// layouts
	//
	client->ps.stats[STAT_LAYOUTS] = 0;

	if (deathmatch->value)
	{
		if (client->pers.health <= 0 || level.intermissiontime
			|| client->showscores)
			client->ps.stats[STAT_LAYOUTS] |= 1;
		if (client->showinventory && client->pers.health > 0)
			client->ps.stats[STAT_LAYOUTS] |= 2;
	}
	else
	{
		if (client->showscores || client->showhelp)
			client->ps.stats[STAT_LAYOUTS] |= 1;
		if (client->showinventory && client->pers.health > 0)
			client->ps.stats[STAT_LAYOUTS] |= 2;
	}

	//
	// frags
	//
	client->ps.stats[STAT_FRAGS] = client->resp.score;

	//
	// help icon / current weapon if not shown
	//
	if (client->resp.helpchanged && (level.framenum&8) )
		client->ps.stats[STAT_HELPICON] = trap_ImageIndex ("pics/i_help");
	else if ( (client->pers.hand == CENTER_HANDED || client->ps.fov > 91)
		&& client->pers.weapon)
		client->ps.stats[STAT_HELPICON] = trap_ImageIndex (client->pers.weapon->icon);
	else
		client->ps.stats[STAT_HELPICON] = 0;

//ZOID
	SetCTFStats(ent);
//ZOID
}

