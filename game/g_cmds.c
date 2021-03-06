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
#include "m_player.h"
#include "gs_pmodels.h"


char *ClientTeam (edict_t *ent)
{
	char		*p;
	static char	value[512];

	value[0] = 0;

	if (!ent->r.client)
		return value;

	strcpy(value, Info_ValueForKey (ent->r.client->pers.userinfo, "skin"));
	p = strchr(value, '/');
	if (!p)
		return value;

	if (dmflags->integer & DF_MODELTEAMS)
	{
		*p = 0;
		return value;
	}

	// if (dmflags->integer & DF_SKINTEAMS)
	return ++p;
}

qboolean OnSameTeam (edict_t *ent1, edict_t *ent2)
{
	char	ent1Team [512];
	char	ent2Team [512];

	if (!(dmflags->integer & (DF_MODELTEAMS | DF_SKINTEAMS)))
		return qfalse;

	strcpy (ent1Team, ClientTeam (ent1));
	strcpy (ent2Team, ClientTeam (ent2));

	if (strcmp(ent1Team, ent2Team) == 0)
		return qtrue;
	return qfalse;
}


void SelectNextItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->r.client;

//ZOID
	if (cl->menu) {
		PMenu_Next(ent);
		return;
	} else if (cl->chase_target) {
		ChaseNext(ent);
		return;
	}
//ZOID

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void SelectPrevItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->r.client;

//ZOID
	if (cl->menu) {
		PMenu_Prev(ent);
		return;
	} else if (cl->chase_target) {
		ChasePrev(ent);
		return;
	}
//ZOID

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void ValidateSelectedItem (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->r.client;

	if (cl->pers.inventory[cl->pers.selected_item])
		return;		// valid

	SelectNextItem (ent, -1);
}


//=================================================================================

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f (edict_t *ent)
{
	char		*name;
	gitem_t		*it;
	int			index;
	int			i;
	qboolean	give_all;
	edict_t		*it_ent;

	if (deathmatch->integer && !sv_cheats->integer)
	{
		G_PrintMsg (ent, PRINT_HIGH, "Cheats are not enabled on this server.\n");
		return;
	}

	name = trap_Cmd_Args ();

	if (Q_stricmp(name, "all") == 0)
		give_all = qtrue;
	else
		give_all = qfalse;

	if (give_all || Q_stricmp(trap_Cmd_Argv(1), "health") == 0)
	{
		if (trap_Cmd_Argc() == 3)
			ent->health = atoi(trap_Cmd_Argv(2));
		else
			ent->health = ent->max_health;
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_WEAPON))
				continue;
			ent->r.client->pers.inventory[i] += 1;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_AMMO))
				continue;
			Add_Ammo (ent, it, 1000);
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		gitem_armor_t	*info;

		it = FindItem("Jacket Armor");
		ent->r.client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Combat Armor");
		ent->r.client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Body Armor");
		info = (gitem_armor_t *)it->info;
		ent->r.client->pers.inventory[ITEM_INDEX(it)] = info->max_count;

		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "Power Shield") == 0)
	{
		it = FindItem("Power Shield");
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, 0);
		if (it_ent->r.inuse)
			G_FreeEdict(it_ent);

		if (!give_all)
			return;
	}

	if (give_all)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (it->flags & (IT_ARMOR|IT_WEAPON|IT_AMMO|IT_FLAG))
				continue;
			ent->r.client->pers.inventory[i] = 1;
		}
		return;
	}

	it = FindItem (name);
	if (!it)
	{
		name = trap_Cmd_Argv (1);
		it = FindItem (name);
		if (!it)
		{
			G_PrintMsg (ent, PRINT_HIGH, "unknown item\n");
			return;
		}
	}

	if (!it->pickup || (it->flags & IT_FLAG) )
	{
		G_PrintMsg (ent, PRINT_HIGH, "non-pickup (givable) item\n");
		return;
	}

	index = ITEM_INDEX(it);

	if (it->flags & IT_AMMO)
	{
		if (trap_Cmd_Argc() == 3)
			ent->r.client->pers.inventory[index] = atoi(trap_Cmd_Argv(2));
		else
			ent->r.client->pers.inventory[index] += it->quantity;
	}
	else
	{
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, 0);
		if (it_ent->r.inuse)
			G_FreeEdict(it_ent);
	}
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->integer && !sv_cheats->integer)
	{
		G_PrintMsg (ent, PRINT_HIGH, "Cheats are not enabled on this server.\n");
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	G_PrintMsg (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->integer && !sv_cheats->integer)
	{
		G_PrintMsg (ent, PRINT_HIGH, "Cheats are not enabled on this server.\n");
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	G_PrintMsg (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->integer && !sv_cheats->integer)
	{
		G_PrintMsg (ent, PRINT_HIGH, "Cheats are not enabled on this server.\n");
		return;
	}

	if (ent->movetype == MOVETYPE_NOCLIP)
	{
		ent->movetype = MOVETYPE_WALK;
		msg = "noclip OFF\n";
	}
	else
	{
		ent->movetype = MOVETYPE_NOCLIP;
		msg = "noclip ON\n";
	}

	G_PrintMsg (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Use_f

Use an inventory item
==================
*/
void Cmd_Use_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	s = trap_Cmd_Args ();
	it = FindItem (s);
	if (!it)
	{
		G_PrintMsg (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->use)
	{
		G_PrintMsg (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->r.client->pers.inventory[index])
	{
		G_PrintMsg (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->use (ent, it);
}


/*
==================
Cmd_Drop_f

Drop an inventory item
==================
*/
void Cmd_Drop_f( edict_t *ent )
{
	int			index;
	gitem_t		*it;
	char		*s;

//ZOID--special case for tech powerups
	if( Q_stricmp( trap_Cmd_Args(), "tech" ) == 0 && ( it = CTFWhat_Tech( ent ) ) != NULL ) {
		it->drop( ent, it );
		return;
	}
//ZOID

	s = trap_Cmd_Args ();
	it = FindItem( s );
	if( !it ) {
		G_PrintMsg (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if( !it->drop ) {
		G_PrintMsg( ent, PRINT_HIGH, "Item is not dropable.\n" );
		return;
	}
	index = ITEM_INDEX( it );
	if( !ent->r.client->pers.inventory[index] ) {
		G_PrintMsg( ent, PRINT_HIGH, "Out of item: %s\n", s );
		return;
	}

	if( ent->pmAnim.anim_priority[UPPER] <= ANIM_WAVE )
		G_AddEvent( ent, EV_DROP, 0, qtrue );

	it->drop( ent, it );
}


/*
=================
Cmd_Inven_f
=================
*/
void Cmd_Inven_f (edict_t *ent)
{
	int			i;
	char		s[1024];
	int			row[MAX_ITEMS * 2], rowsize, rep;
	gclient_t	*cl;

	cl = ent->r.client;

	cl->showscores = qfalse;
	cl->showhelp = qfalse;

//ZOID
	if (cl->menu) {
		PMenu_Close(ent);
		cl->update_chase = qtrue;
		return;
	}
//ZOID

	if (cl->showinventory)
	{
		cl->showinventory = qfalse;
		return;
	}

//ZOID
	if (ctf->integer && cl->resp.ctf_team == CTF_NOTEAM) {
		CTFOpenJoinMenu(ent);
		return;
	}
//ZOID

	cl->showinventory = qtrue;

	// RLE compression
	for (i=1,rowsize=0,row[0] = 0 ; i<game.num_items ; i++)
	{
		row[rowsize++] = cl->pers.inventory[i];
		if (cl->pers.inventory[i])
			continue;

		for (i++, rep = 1; !cl->pers.inventory[i] && (i < game.num_items); i++, rep++);
		row[rowsize++] = rep;
		i--;
	}

	// item 0 is never used
	Q_strncpyz (s, "inv \"", sizeof(s));
	for (i=0 ; i<rowsize-1 ; i++)
		Q_strncatz (s, va("%i ", row[i]), sizeof(s));
	Q_strncatz (s, va("%i\"", row[i]), sizeof(s));

	trap_ServerCmd (ent, s);
}

/*
=================
Cmd_InvUse_f
=================
*/
void Cmd_InvUse_f (edict_t *ent)
{
	gitem_t		*it;
	gclient_t	*cl;

	cl = ent->r.client;

//ZOID
	if (cl->menu) {
		PMenu_Select(ent);
		return;
	}
//ZOID

	ValidateSelectedItem (ent);

	if (cl->pers.selected_item == -1)
	{
		G_PrintMsg (ent, PRINT_HIGH, "No item to use.\n");
		return;
	}

	it = &itemlist[cl->pers.selected_item];
	if (!it->use)
	{
		G_PrintMsg (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	it->use (ent, it);
}

//ZOID
/*
=================
Cmd_LastWeap_f
=================
*/
void Cmd_LastWeap_f (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->r.client;

	if (!cl->pers.weapon || !cl->pers.lastweapon)
		return;

	cl->pers.lastweapon->use (ent, cl->pers.lastweapon);
}
//ZOID

/*
=================
Cmd_WeapPrev_f
=================
*/
void Cmd_WeapPrev_f( edict_t *ent )
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->r.client;

	if( cl->menu ) {
		PMenu_Prev( ent );
		return;
	} else if( cl->chase_target ) {
		ChasePrev( ent );
		return;
	}

	if( !cl->pers.weapon )
		return;

	selected_weapon = ITEM_INDEX( cl->pers.weapon );

	// scan  for the next valid one
	for( i = 1 ; i <= MAX_ITEMS ; i++ ) {
		index = (selected_weapon + i)%MAX_ITEMS;
		if( !cl->pers.inventory[index] )
			continue;
		it = &itemlist[index];
		if( !it->use )
			continue;
		if(! (it->flags & IT_WEAPON) )
			continue;
		it->use( ent, it );
		if( cl->pers.weapon == it )
			return;	// successful
	}
}

/*
=================
Cmd_WeapNext_f
=================
*/
void Cmd_WeapNext_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->r.client;

	if( cl->menu ) {
		PMenu_Next( ent );
		return;
	} else if( cl->chase_target ) {
		ChaseNext( ent );
		return;
	}

	if( !cl->pers.weapon )
		return;

	selected_weapon = ITEM_INDEX( cl->pers.weapon );

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapLast_f
=================
*/
void Cmd_WeapLast_f (edict_t *ent)
{
	gclient_t	*cl;
	int			index;
	gitem_t		*it;

	cl = ent->r.client;

	if (!cl->pers.weapon || !cl->pers.lastweapon)
		return;

	index = ITEM_INDEX(cl->pers.lastweapon);
	if (!cl->pers.inventory[index])
		return;
	it = &itemlist[index];
	if (!it->use)
		return;
	if (! (it->flags & IT_WEAPON) )
		return;
	it->use (ent, it);
}

/*
=================
Cmd_InvDrop_f
=================
*/
void Cmd_InvDrop_f (edict_t *ent)
{
	gclient_t	*cl;
	gitem_t		*it;

	cl = ent->r.client;

	ValidateSelectedItem (ent);

	if (cl->pers.selected_item == -1)
	{
		G_PrintMsg (ent, PRINT_HIGH, "No item to drop.\n");
		return;
	}

	it = &itemlist[cl->pers.selected_item];
	if (!it->drop)
	{
		G_PrintMsg (ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}

	if( ent->pmAnim.anim_priority[UPPER] <= ANIM_WAVE )
		G_AddEvent( ent, EV_DROP, 0, qtrue );

	it->drop( ent, it );
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f (edict_t *ent)
{
//ZOID
	if (ent->r.solid == SOLID_NOT)
		return;
//ZOID

	if((level.time - ent->r.client->respawn_time) < 5)
		return;
	ent->flags &= ~FL_GODMODE;
	ent->health = 0;
	meansOfDeath = MOD_SUICIDE;
	player_die (ent, ent, ent, 100000, vec3_origin);
}

/*
=================
Cmd_PutAway_f
=================
*/
void Cmd_PutAway_f (edict_t *ent)
{
	ent->r.client->showscores = qfalse;
	ent->r.client->showhelp = qfalse;
	ent->r.client->showinventory = qfalse;
//ZOID
	if (ent->r.client->menu)
		PMenu_Close(ent);
	ent->r.client->update_chase = qtrue;
//ZOID
}


int PlayerSort (void const *a, void const *b)
{
	int		anum, bnum;

	anum = *(int *)a;
	bnum = *(int *)b;

	anum = game.clients[anum].ps.stats[STAT_FRAGS];
	bnum = game.clients[bnum].ps.stats[STAT_FRAGS];

	if (anum < bnum)
		return -1;
	if (anum > bnum)
		return 1;
	return 0;
}

/*
=================
Cmd_Players_f
=================
*/
void Cmd_Players_f (edict_t *ent)
{
	int		i;
	int		count;
	char	small[64];
	char	large[1280];
	int		index[256];

	count = 0;
	for (i = 0 ; i < game.maxclients ; i++)
		if (game.clients[i].pers.connected)
		{
			index[count] = i;
			count++;
		}

	// sort by frags
	qsort (index, count, sizeof(index[0]), PlayerSort);

	// print information
	large[0] = 0;

	for (i = 0 ; i < count ; i++)
	{
		Q_snprintfz (small, sizeof(small), "%3i %s\n",
			game.clients[index[i]].ps.stats[STAT_FRAGS],
			game.clients[index[i]].pers.netname);
		if (strlen (small) + strlen(large) > sizeof(large) - 100 )
		{	// can't print all of them in one packet
			strcat (large, "...\n");
			break;
		}
		strcat (large, small);
	}

	G_PrintMsg (ent, PRINT_HIGH, "%s\n%i players\n", large, count);
}

/*
=================
Cmd_Wave_f
=================
*/
void Cmd_Wave_f( edict_t *ent )
{
	int		i;

	i = atoi( trap_Cmd_Argv(1) );

	if( ent->pmAnim.anim_priority[UPPER] > ANIM_WAVE )
		return;

	ent->pmAnim.anim_priority[UPPER] = ANIM_WAVE;

	switch( i )
	{
	case 0:
	default:
		G_AddEvent( ent, EV_GESTURE, 0, qtrue );
		break;
	}
}

qboolean CheckFlood(edict_t *ent)
{
	int		i;
	gclient_t *cl;

	if (flood_msgs->integer) {
		cl = ent->r.client;

        if (level.time < cl->flood_locktill) {
			G_PrintMsg (ent, PRINT_HIGH, "You can't talk for %d more seconds\n",
				(int)(cl->flood_locktill - level.time));
            return qtrue;
        }
        i = cl->flood_whenhead - flood_msgs->integer + 1;
        if (i < 0)
            i = (sizeof(cl->flood_when)/sizeof(cl->flood_when[0])) + i;
		if (cl->flood_when[i] && 
			level.time - cl->flood_when[i] < flood_persecond->integer) {
			cl->flood_locktill = level.time + flood_waitdelay->value;
			G_PrintMsg (ent, PRINT_CHAT, "Flood protection:  You can't talk for %d seconds.\n",
				flood_waitdelay->integer);
            return qtrue;
        }
		cl->flood_whenhead = (cl->flood_whenhead + 1) %
			(sizeof(cl->flood_when)/sizeof(cl->flood_when[0]));
		cl->flood_when[cl->flood_whenhead] = level.time;
	}
	return qfalse;
}

/*
==================
Cmd_Say_f
==================
*/
void Cmd_Say_f (edict_t *ent, qboolean team, qboolean arg0)
{
	int		j, color;
	edict_t	*other;
	char	*p;
	char	text[2048];

	if (trap_Cmd_Argc () < 2 && !arg0)
		return;

	if (!(dmflags->integer & (DF_MODELTEAMS | DF_SKINTEAMS)))
		team = qfalse;

	if ( team ) {
		color = COLOR_MAGENTA;
	} else {
		color = COLOR_GREEN;
	}

	Q_snprintfz (text, sizeof(text), "%s%c%c: ", ent->r.client->pers.netname, Q_COLOR_ESCAPE, color + '0');

	if (arg0)
	{
		strcat (text, trap_Cmd_Argv (0));
		strcat (text, " ");
		strcat (text, trap_Cmd_Args ());
	}
	else
	{
		p = trap_Cmd_Args();

		if (*p == '"')
		{
			p++;
			p[strlen(p)-1] = 0;
		}
		strcat(text, p);
	}

	// don't let text be too long for malicious reasons
	if (strlen(text) > 150)
		text[150] = 0;

	strcat(text, "\n");

	if (CheckFlood(ent))
		return;

	if (dedicated->integer)
		G_Printf ("%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = &game.edicts[j];
		if (!other->r.inuse)
			continue;
		if (!other->r.client)
			continue;
		if (team)
		{
			if (!OnSameTeam(ent, other))
				continue;
		}
		G_PrintMsg (other, PRINT_CHAT, "%s", text);
	}
}

/*
=================
ClientCommand
=================
*/
void ClientCommand (edict_t *ent)
{
	char	*cmd;

	if (!ent->r.client)
		return;		// not fully in game yet

	cmd = trap_Cmd_Argv(0);

	if (Q_stricmp (cmd, "players") == 0)
	{
		Cmd_Players_f (ent);
		return;
	}
	if (Q_stricmp (cmd, "say") == 0)
	{
		Cmd_Say_f (ent, qfalse, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "say_team") == 0 || Q_stricmp (cmd, "steam") == 0)
	{
		CTFSay_Team(ent, trap_Cmd_Args ());
		return;
	}
	if (Q_stricmp (cmd, "score") == 0)
	{
		Cmd_Score_f (ent);
		return;
	}
	if (Q_stricmp (cmd, "help") == 0)
	{
		Cmd_Help_f (ent);
		return;
	}

	if (level.intermissiontime)
		return;

	if (Q_stricmp (cmd, "use") == 0)
		Cmd_Use_f (ent);
	else if (Q_stricmp (cmd, "drop") == 0)
		Cmd_Drop_f (ent);
	else if (Q_stricmp (cmd, "give") == 0)
		Cmd_Give_f (ent);
	else if (Q_stricmp (cmd, "god") == 0)
		Cmd_God_f (ent);
	else if (Q_stricmp (cmd, "notarget") == 0)
		Cmd_Notarget_f (ent);
	else if (Q_stricmp (cmd, "noclip") == 0)
		Cmd_Noclip_f (ent);
	else if (Q_stricmp (cmd, "inven") == 0)
		Cmd_Inven_f (ent);
	else if (Q_stricmp (cmd, "invnext") == 0)
		SelectNextItem (ent, -1);
	else if (Q_stricmp (cmd, "invprev") == 0)
		SelectPrevItem (ent, -1);
	else if (Q_stricmp (cmd, "invnextw") == 0)
		SelectNextItem (ent, IT_WEAPON);
	else if (Q_stricmp (cmd, "invprevw") == 0)
		SelectPrevItem (ent, IT_WEAPON);
	else if (Q_stricmp (cmd, "invnextp") == 0)
		SelectNextItem (ent, IT_POWERUP);
	else if (Q_stricmp (cmd, "invprevp") == 0)
		SelectPrevItem (ent, IT_POWERUP);
	else if (Q_stricmp (cmd, "invuse") == 0)
		Cmd_InvUse_f (ent);
	else if (Q_stricmp (cmd, "invdrop") == 0)
		Cmd_InvDrop_f (ent);
	else if (Q_stricmp (cmd, "weapprev") == 0)
		Cmd_WeapPrev_f (ent);
	else if (Q_stricmp (cmd, "weapnext") == 0)
		Cmd_WeapNext_f (ent);
	else if (Q_stricmp (cmd, "weaplast") == 0)
		Cmd_WeapLast_f (ent);
	else if (Q_stricmp (cmd, "kill") == 0)
		Cmd_Kill_f (ent);
	else if (Q_stricmp (cmd, "putaway") == 0)
		Cmd_PutAway_f (ent);
	else if (Q_stricmp (cmd, "wave") == 0)
		Cmd_Wave_f (ent);
//ZOID
	else if (Q_stricmp (cmd, "team") == 0)
	{
		CTFTeam_f (ent);
	} else if (Q_stricmp(cmd, "id") == 0) {
		CTFID_f (ent);
	} else if (Q_stricmp(cmd, "yes") == 0) {
		CTFVoteYes(ent);
	} else if (Q_stricmp(cmd, "no") == 0) {
		CTFVoteNo(ent);
	} else if (Q_stricmp(cmd, "ready") == 0) {
		CTFReady(ent);
	} else if (Q_stricmp(cmd, "notready") == 0) {
		CTFNotReady(ent);
	} else if (Q_stricmp(cmd, "ghost") == 0) {
		CTFGhost(ent);
	} else if (Q_stricmp(cmd, "admin") == 0) {
		CTFAdmin(ent);
	} else if (Q_stricmp(cmd, "stats") == 0) {
		CTFStats(ent);
	} else if (Q_stricmp(cmd, "warp") == 0) {
		CTFWarp(ent);
	} else if (Q_stricmp(cmd, "boot") == 0) {
		CTFBoot(ent);
	} else if (Q_stricmp(cmd, "playerlist") == 0) {
		CTFPlayerList(ent);
	} else if (Q_stricmp(cmd, "observer") == 0) {
		CTFObserver(ent);
	} else if ( !Q_stricmp( cmd, "chasecam" ) || !Q_stricmp( cmd, "chase" ) ) {
		Cmd_ChaseCam_f( ent );
	}
//ZOID
	else	// anything that doesn't match a command will be a chat
		Cmd_Say_f (ent, qfalse, qtrue);
}
