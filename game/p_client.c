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

void SP_misc_teleporter_dest (edict_t *ent);


/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
The normal starting point for a level.
*/
void SP_info_player_start(edict_t *self)
{
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for deathmatch games
*/
void SP_info_player_deathmatch(edict_t *self)
{
	if (!deathmatch->integer)
	{
		G_FreeEdict (self);
		return;
	}
	SP_misc_teleporter_dest (self);
}

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for coop games
*/

void SP_info_player_coop(edict_t *self)
{
	if (!coop->integer)
		G_FreeEdict (self);
}


/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The deathmatch intermission point will be at one of these
Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.  'pitch yaw roll'
*/
void SP_info_player_intermission(edict_t *ent)
{
}


//=======================================================================


void player_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	// player pain is handled at the end of the frame in P_DamageFeedback
}

void ClientObituary (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	int			mod;
	char		message[64];
	char		message2[64];
	qboolean	ff;

	if (coop->integer && attacker->r.client)
		meansOfDeath |= MOD_FRIENDLY_FIRE;

	if ( deathmatch->integer || coop->integer )
	{
		ff = meansOfDeath & MOD_FRIENDLY_FIRE;
		mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;

		GS_Obituary ( self, G_PlayerGender ( self ), attacker, mod, message, message2 );

		// duplicate message at server console for logging
		if ( attacker && attacker->r.client ) {
			if ( attacker != self ) {		// regular death message
				if ( deathmatch->integer ) {
					if( ff )
						attacker->r.client->resp.score--;
					else
						attacker->r.client->resp.score++;
				}

				self->enemy = attacker;

				if( dedicated->integer )
					G_Printf ( "%s %s %s%s\n", self->r.client->pers.netname, message, attacker->r.client->pers.netname, message2 );
			} else {			// suicide
				if( deathmatch->integer )
					self->r.client->resp.score--;

				self->enemy = NULL;

				if( dedicated->integer )
					G_Printf ( "%s %s\n", self->r.client->pers.netname, message );
			}

			G_Obituary ( self, attacker, mod );
		} else {		// wrong place, suicide, etc.
			if( deathmatch->integer )
				self->r.client->resp.score--;

			self->enemy = NULL;

			if( dedicated->integer )
				G_Printf( "%s %s\n", self->r.client->pers.netname, message );

			G_Obituary ( self, (attacker == self) ? self : world, mod );
		}
	}
}

void TossClientWeapon (edict_t *self)
{
	gitem_t		*item;
	edict_t		*drop;
	qboolean	quad;
	float		spread;

	if (!deathmatch->integer)
		return;

	item = self->r.client->pers.weapon;
	if (! self->r.client->pers.inventory[self->r.client->ammo_index] )
		item = NULL;
	if (item && (strcmp (item->pickup_name, "Blaster") == 0))
		item = NULL;

	if (!(dmflags->integer & DF_QUAD_DROP))
		quad = qfalse;
	else
		quad = (self->r.client->quad_timeout > (level.time + 1));

	if (item && quad)
		spread = 22.5;
	else
		spread = 0.0;

	if (item)
	{
		self->r.client->v_angle[YAW] -= spread;
		drop = Drop_Item (self, item);
		self->r.client->v_angle[YAW] += spread;
		drop->spawnflags = DROPPED_PLAYER_ITEM;
	}

	if (quad)
	{
		self->r.client->v_angle[YAW] += spread;
		drop = Drop_Item (self, FindItemByClassname ("item_quad"));
		self->r.client->v_angle[YAW] -= spread;
		drop->spawnflags |= DROPPED_PLAYER_ITEM;

		drop->touch = Touch_Item;
		drop->nextthink = self->r.client->quad_timeout;
		drop->think = G_FreeEdict;
	}
}


/*
==================
LookAtKiller
==================
*/
void LookAtKiller (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	vec3_t		dir;

	if (attacker && attacker != world && attacker != self)
	{
		VectorSubtract (attacker->s.origin, self->s.origin, dir);
	}
	else if (inflictor && inflictor != world && inflictor != self)
	{
		VectorSubtract (inflictor->s.origin, self->s.origin, dir);
	}
	else
	{
		self->r.client->killer_yaw = self->s.angles[YAW];
		return;
	}

	if (dir[0])
		self->r.client->killer_yaw = RAD2DEG ( atan2(dir[1], dir[0]) );
	else {
		self->r.client->killer_yaw = 0;
		if (dir[1] > 0)
			self->r.client->killer_yaw = 90;
		else if (dir[1] < 0)
			self->r.client->killer_yaw = -90;
	}
	if (self->r.client->killer_yaw < 0)
		self->r.client->killer_yaw += 360;
}

/*
==================
player_die
==================
*/
void player_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		contents;
	gclient_t	*client = self->r.client;

	VectorClear (self->avelocity);

	self->takedamage = DAMAGE_YES;
	self->movetype = MOVETYPE_TOSS;

	self->s.modelindex2 = 0;	// remove linked weapon model
//ZOID
	self->s.modelindex3 = 0;	// remove linked ctf flag
//ZOID

	self->s.angles[0] = 0;
	self->s.angles[2] = 0;

	self->s.sound = 0;
	self->r.client->weapon_sound = 0;

	self->r.maxs[2] = -8;

//	self->solid = SOLID_NOT;
	self->r.svflags |= SVF_CORPSE;

	contents = trap_PointContents( self->s.origin );

	if (!self->deadflag)
	{
		self->r.client->respawn_time = level.time + 1.0;
		LookAtKiller (self, inflictor, attacker);
		self->r.client->ps.pmove.pm_type = PM_DEAD;
		ClientObituary (self, inflictor, attacker);
//ZOID
		// if at start and same team, clear
		if (ctf->integer && meansOfDeath == MOD_TELEFRAG &&
			self->r.client->resp.ctf_state < 2 &&
			self->r.client->resp.ctf_team == attacker->r.client->resp.ctf_team) {
			attacker->r.client->resp.score--;
			self->r.client->resp.ctf_state = 0;
		}

		CTFFragBonuses(self, inflictor, attacker);

//ZOID
		CTFPlayerResetGrapple(self);
//ZOID

		// check if player is in a nodrop area and
		// reset flags and techs, otherwise drop items
		if ( !(contents & CONTENTS_NODROP) )
		{
//ZOID
			TossClientWeapon ( self );
//ZOID
			CTFDeadDropFlag ( self );
			CTFDeadDropTech ( self );
//ZOID
		} else {		// reset flags and tech
			edict_t *tech = CTFDeadDropTech ( self );

			if ( tech ) {
				CTFRespawnTech ( tech );
			}

			if ( self->r.client->pers.inventory[ITEM_INDEX(FindItemByClassname("team_CTF_redflag"))] ) {
				CTFResetFlag ( CTF_TEAM1 );
			} else if ( self->r.client->pers.inventory[ITEM_INDEX(FindItemByClassname("team_CTF_blueflag"))] ) {
				CTFResetFlag ( CTF_TEAM2 );
			}
		}

		if (deathmatch->integer && !self->r.client->showscores)
			Cmd_Help_f (self);		// show scores
	}

	// remove powerups
	self->r.client->quad_timeout = 0;
	self->r.client->invincible_timeout = 0;
	self->r.client->breather_timeout = 0;
	self->r.client->enviro_timeout = 0;
	self->flags &= ~FL_POWER_ARMOR;

	// clear inventory
	memset(self->r.client->pers.inventory, 0, sizeof(self->r.client->pers.inventory));
	if( self->health < GIB_HEALTH ) {

		// do not throw gibs in nodrop area but drop head 
		ThrowSmallPileOfGibs ( self, 8, damage );
		G_AddEvent( self, EV_GIB, 0, qtrue );
		ThrowClientHead( self, damage );

		//jal(gibbed): Don't send animations anymore, just send s.frame = 0
		self->pmAnim.anim_priority[LOWER] = ANIM_DEATH;
		self->pmAnim.anim_priority[UPPER] = ANIM_DEATH;
		self->pmAnim.anim_priority[HEAD] = ANIM_DEATH;
		self->pmAnim.anim[LOWER] = 0;
		self->pmAnim.anim[UPPER] = 0;
		self->pmAnim.anim[HEAD] = 0;
		self->s.frame = 0;

	} else {	// normal death

		if( !self->deadflag ) {
			static int i;
			
			i = (i+1)%3;
			// start a death animation:
			// jal: Set the DEAD (corpse) animation in the game,
			// and send the DEATH (player dying) animation inside 
			// the event, so it is set at cgame in EVENT_CHANNEL
			switch (i)
			{
			case 0:
				self->pmAnim.anim[LOWER] = BOTH_DEAD1;
				self->pmAnim.anim[UPPER] = BOTH_DEAD1;
				self->pmAnim.anim[HEAD] = 0;
				break;
				
			case 1:
				self->pmAnim.anim[LOWER] = BOTH_DEAD2;
				self->pmAnim.anim[UPPER] = BOTH_DEAD2;
				self->pmAnim.anim[HEAD] = 0;
				break;
				
			case 2:
				self->pmAnim.anim[LOWER] = BOTH_DEAD3;
				self->pmAnim.anim[UPPER] = BOTH_DEAD3;
				self->pmAnim.anim[HEAD] = 0;
				break;
			}
			
			//pause time for finishing the animation before respawning
			self->r.client->respawn_time = level.time + ANIM_DEATH_TIME/1000;

			//send the death style (1, 2 or 3) inside parameters
			G_AddEvent( self, EV_DIE, i, qtrue );
			self->pmAnim.anim_priority[LOWER] = ANIM_DEATH;
			self->pmAnim.anim_priority[UPPER] = ANIM_DEATH;
			self->pmAnim.anim_priority[HEAD] = ANIM_DEATH;
			if( self->health <= GIB_HEALTH )
				self->health = GIB_HEALTH + 1;
		}
	}

	self->deadflag = DEAD_DEAD;

	trap_LinkEntity (self);
}

//=======================================================================

/*
==============
InitClientPersistant

This is only called when the game first initializes in single player,
but is called after each death and level change in deathmatch
==============
*/
void InitClientPersistant (gclient_t *client)
{
	gitem_t		*item;

	memset (&client->pers, 0, sizeof(client->pers));

	item = FindItem("Blaster");
	client->pers.inventory[ITEM_INDEX(item)] = 1;

	item = FindItem("Bullets");
	client->pers.inventory[ITEM_INDEX(item)] = 50;

	item = FindItem("Machinegun");
	client->pers.selected_item = ITEM_INDEX(item);
	client->pers.inventory[client->pers.selected_item] = 1;

	client->pers.weapon = item;

//ZOID
	client->pers.lastweapon = item;
//ZOID

//ZOID
	if ( ctf->integer ) {
		item = FindItem("Grapple");
		client->pers.inventory[ITEM_INDEX(item)] = 1;
	}
//ZOID

	client->pers.health			= 100;
	client->pers.max_health		= 100;

	client->pers.max_bullets	= 200;
	client->pers.max_shells		= 100;
	client->pers.max_rockets	= 50;
	client->pers.max_grenades	= 50;
	client->pers.max_cells		= 200;
	client->pers.max_slugs		= 50;

	client->pers.connected = qtrue;
}


void InitClientResp (gclient_t *client)
{
//ZOID
	int ctf_team = client->resp.ctf_team;
	qboolean id_state = client->resp.id_state;
//ZOID

	memset (&client->resp, 0, sizeof(client->resp));
	
//ZOID
	client->resp.ctf_team = ctf_team;
	client->resp.id_state = id_state;
//ZOID

	client->resp.enterframe = level.framenum;
	client->resp.coop_respawn = client->pers;
 
//ZOID
	if (ctf->integer && client->resp.ctf_team < CTF_TEAM1)
		CTFAssignTeam(client);
//ZOID
}

/*
==================
SaveClientData

Some information that should be persistant, like health, 
is still stored in the edict structure, so it needs to
be mirrored out to the client structure before all the
edicts are wiped.
==================
*/
void SaveClientData (void)
{
	int		i;
	edict_t	*ent;

	for (i=0 ; i<game.maxclients ; i++)
	{
		ent = &game.edicts[1+i];
		if (!ent->r.inuse)
			continue;
		game.clients[i].pers.health = ent->health;
		game.clients[i].pers.max_health = ent->max_health;
		game.clients[i].pers.savedFlags = (ent->flags & (FL_GODMODE|FL_NOTARGET|FL_POWER_ARMOR));
		if (coop->integer)
			game.clients[i].pers.score = ent->r.client->resp.score;
	}
}

void FetchClientEntData (edict_t *ent)
{
	ent->health = ent->r.client->pers.health;
	ent->max_health = ent->r.client->pers.max_health;
	ent->flags |= ent->r.client->pers.savedFlags;
	if (coop->integer)
		ent->r.client->resp.score = ent->r.client->pers.score;
}



/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
PlayersRangeFromSpot

Returns the distance to the nearest player from the given spot
================
*/
float	PlayersRangeFromSpot (edict_t *spot)
{
	edict_t	*player;
	float	bestplayerdistance;
	vec3_t	v;
	int		n;
	float	playerdistance;


	bestplayerdistance = 9999999;

	for (n = 1; n <= game.maxclients; n++)
	{
		player = &game.edicts[n];

		if (!player->r.inuse)
			continue;
		if (player->health <= 0)
			continue;

		VectorSubtract (spot->s.origin, player->s.origin, v);
		playerdistance = VectorLength (v);

		if (playerdistance < bestplayerdistance)
			bestplayerdistance = playerdistance;
	}

	return bestplayerdistance;
}

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point, but NOT the two points closest
to other players
================
*/
edict_t *SelectRandomDeathmatchSpawnPoint (void)
{
	edict_t	*spot, *spot1, *spot2;
	int		count = 0;
	int		selection;
	float	range, range1, range2;

	spot = NULL;
	range1 = range2 = 99999;
	spot1 = spot2 = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		count++;
		range = PlayersRangeFromSpot(spot);
		if (range < range1)
		{
			range1 = range;
			spot1 = spot;
		}
		else if (range < range2)
		{
			range2 = range;
			spot2 = spot;
		}
	}

	if (!count)
		return NULL;

	if (count <= 2)
	{
		spot1 = spot2 = NULL;
	}
	else
	{
		if (spot1)
			count--;
		if (spot2 && spot2 != spot1)
			count--;
	}

	selection = rand() % count;

	spot = NULL;
	do
	{
		spot = G_Find (spot, FOFS(classname), "info_player_deathmatch");
		if (spot == spot1 || spot == spot2)
			selection++;
	} while(selection--);

	return spot;
}

/*
================
SelectFarthestDeathmatchSpawnPoint

================
*/
edict_t *SelectFarthestDeathmatchSpawnPoint (void)
{
	edict_t	*bestspot;
	float	bestdistance, bestplayerdistance;
	edict_t	*spot;


	spot = NULL;
	bestspot = NULL;
	bestdistance = 0;
	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		bestplayerdistance = PlayersRangeFromSpot (spot);

		if (bestplayerdistance > bestdistance)
		{
			bestspot = spot;
			bestdistance = bestplayerdistance;
		}
	}

	if (bestspot)
	{
		return bestspot;
	}

	// if there is a player just spawned on each and every start spot
	// we have no choice to turn one into a telefrag meltdown
	spot = G_Find (NULL, FOFS(classname), "info_player_deathmatch");

	return spot;
}

edict_t *SelectDeathmatchSpawnPoint (void)
{
	if (dmflags->integer & DF_SPAWN_FARTHEST)
		return SelectFarthestDeathmatchSpawnPoint ();
	else
		return SelectRandomDeathmatchSpawnPoint ();
}


edict_t *SelectCoopSpawnPoint (edict_t *ent)
{
	int		index;
	edict_t	*spot = NULL;
	const char *target;

	index = ent->r.client - game.clients;

	// player 0 starts in normal player spawn point
	if (!index)
		return NULL;

	spot = NULL;

	// assume there are four coop spots at each spawnpoint
	while (1)
	{
		spot = G_Find (spot, FOFS(classname), "info_player_coop");
		if (!spot)
			return NULL;	// we didn't have enough...

		target = spot->targetname;
		if (!target)
			target = "";
		if ( Q_stricmp(game.spawnpoint, target) == 0 )
		{	// this is a coop spawn point for one of the clients here
			index--;
			if (!index)
				return spot;		// this is it
		}
	}


	return spot;
}


/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
void	SelectSpawnPoint (edict_t *ent, vec3_t origin, vec3_t angles)
{
	edict_t	*spot = NULL;

	if (deathmatch->integer)
//ZOID
		if (ctf->integer)
			spot = SelectCTFSpawnPoint(ent);
		else
//ZOID
			spot = SelectDeathmatchSpawnPoint ();
	else if (coop->integer)
		spot = SelectCoopSpawnPoint (ent);

	// find a single player start spot
	if (!spot)
	{
		while ((spot = G_Find (spot, FOFS(classname), "info_player_start")) != NULL)
		{
			if (!game.spawnpoint[0] && !spot->targetname)
				break;

			if (!game.spawnpoint[0] || !spot->targetname)
				continue;

			if (Q_stricmp(game.spawnpoint, spot->targetname) == 0)
				break;
		}

		if (!spot)
		{
			if (!game.spawnpoint[0])
			{	// there wasn't a spawnpoint without a target, so use any
				spot = G_Find (spot, FOFS(classname), "info_player_start");
			}
			if (!spot)
				G_Error ("Couldn't find spawn point %s\n", game.spawnpoint);
		}
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);
}

//======================================================================


void InitBodyQue (void)
{
	int		i;
	edict_t	*ent;

	level.body_que = 0;
	for (i=0; i<BODY_QUEUE_SIZE ; i++)
	{
		ent = G_Spawn();
		ent->classname = "bodyque";
	}
}

void body_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	if( self->health >= GIB_HEALTH )
		return;

	ThrowSmallPileOfGibs( self, 8, damage );
	G_AddEvent( self, EV_GIB, 0, qtrue );
	self->s.origin[2] -= 48;
	ThrowClientHead( self, damage );
}

/*
=============
body_think
  autodestruct body. Body self removal after some time.
  fixme: make a better effect for the body when dissapearing
=============
*/
void body_think( edict_t *self )
{
	int	damage;

	self->health = GIB_HEALTH - 1;
	damage = 25;
	// effect: small gibs, and only when it is still a body, not a gibbed head.
	if( self->s.type == ET_PLAYER ) {
		G_AddEvent( self, EV_GIB, 0, qtrue );
		ThrowSmallPileOfGibs( self, 6, damage );
	}
	self->takedamage = DAMAGE_NO;
	self->r.solid = SOLID_NOT;
	self->s.sound = 0;
	self->flags |= FL_NO_KNOCKBACK;

	//remove the model
	self->s.type = ET_GENERIC;
	self->r.svflags &= ~SVF_CORPSE;
	self->s.modelindex = 0;
	trap_LinkEntity(self);
}

/*
=============
CopyToBodyQue
=============
*/
void CopyToBodyQue (edict_t *ent)
{
	edict_t		*body;
	int			contents;

	trap_UnlinkEntity (ent);

	contents = trap_PointContents( ent->s.origin );
	if( contents & CONTENTS_NODROP )
		return;

	// grab a body que and cycle to the next one
	body = &game.edicts[game.maxclients + level.body_que + 1];
	level.body_que = (level.body_que + 1) % BODY_QUEUE_SIZE;

	// FIXME: send an effect on the removed body

	trap_UnlinkEntity( body );

	//clean up garbage
	memset( body,0,sizeof( edict_t ) );

	//init body edict
	G_InitEdict( body );
	body->classname = "body";

	body->health = ent->health;
	body->r.owner = ent->r.owner;
	body->s.effects = ent->s.effects;
	body->r.svflags = ent->r.svflags & ~SVF_FAKECLIENT;
	body->r.svflags |= SVF_CORPSE;

	//copy player position
	VectorCopy( ent->s.old_origin, body->s.old_origin );
	VectorCopy( ent->s.origin, body->s.origin );
	VectorCopy( ent->s.origin2, body->s.origin2 );
	//use flat yaw
	body->s.angles[PITCH] = 0;
	body->s.angles[ROLL] = 0;
	body->s.angles[YAW] = ent->s.angles[YAW];

	VectorCopy( ent->r.mins, body->r.mins );
	VectorCopy( ent->r.maxs, body->r.maxs );
	VectorCopy( ent->r.absmin, body->r.absmin );
	VectorCopy( ent->r.absmax, body->r.absmax );
	VectorCopy( ent->r.size, body->r.size );

	//make a indexed pmodel from client skin
	if( ent->s.type == ET_PLAYER ) {
		char		*s, *t;
		char		model_name[MAX_QPATH], skin_name[MAX_QPATH], scratch[MAX_QPATH];

		body->s.type = ET_PLAYER;
		s = Info_ValueForKey( ent->r.client->pers.userinfo, "skin" );
		strcpy( model_name, s );
		t = strstr( s, "/" );
		if( t ) {
			t++;
			strcpy( skin_name, t );
			model_name[strlen(model_name)-(strlen(skin_name)+1)] = 0;
		}
		
		if( !model_name || !skin_name ) {
			strcpy( model_name, DEFAULT_PLAYERMODEL );
			strcpy( skin_name, DEFAULT_PLAYERSKIN );
		}
		
		//in ctf, send client number to mirror his ctf skin
		if( ctf->integer ) {
			body->s.skinnum = ent - game.edicts - 1;
			body->s.modelindex = qtrue;	//value doesn't matter as long as it has one
			
		} else {
			Q_snprintfz( scratch, sizeof( scratch ), "$models/players/%s",model_name );
			body->s.modelindex = trap_ModelIndex( scratch );
			Q_snprintfz( scratch, sizeof( scratch ), "$models/players/%s/%s",model_name, skin_name );
			body->s.skinnum = trap_ImageIndex( scratch );
		}

		//copy the last animation in the client
		body->s.frame = ( (ent->pmAnim.anim[UPPER]&0x3F) | ((ent->pmAnim.anim[LOWER]&0x3F)<<6) | ((ent->pmAnim.anim[HEAD]&0xF)<<12) );
	
	} else {

		body->s.type = ent->s.type;
		body->s.modelindex = ent->s.modelindex;
		body->s.frame = ent->s.frame;
	}

	G_AddEvent( body, EV_TELEPORT, 0, qtrue );

	body->think = body_think;//body self destruction
	body->nextthink = level.time + (FRAMETIME*400);
	body->takedamage = DAMAGE_YES;
	body->die = body_die;

	body->r.solid = ent->r.solid;
	body->r.clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
	body->movetype = MOVETYPE_TOSS;

	trap_LinkEntity( body );
	M_CheckGround( body );
}


void respawn (edict_t *self)
{
	if (deathmatch->integer || coop->integer)
	{
		edict_t *event;

		if (self->movetype != MOVETYPE_NOCLIP)
			CopyToBodyQue (self);
		self->r.svflags &= ~SVF_NOCLIENT;
		PutClientInServer (self);
		G_AddEvent (self, EV_TELEPORT, 0, qtrue);

		// add a teleportation effect
		event = G_SpawnEvent ( EV_PLAYER_TELEPORT_IN, 0, self->s.origin );
		event->r.svflags = SVF_NOOLDORIGIN;
		event->s.ownerNum = self - game.edicts;

		// hold in place briefly
		self->r.client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
		self->r.client->ps.pmove.pm_time = 14;

		self->r.client->respawn_time = level.time;

		return;
	}

	// restart the entire server
	trap_AddCommandString ("menu_loadgame\n");
}

//==============================================================


/*
===========
PutClientInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void PutClientInServer (edict_t *ent)
{
	vec3_t	mins = {-15, -15, -24};
	vec3_t	maxs = {15, 15, 32};
	int		index;
	vec3_t	spawn_origin, spawn_angles;
	gclient_t	*client;
	int		i;
	client_persistant_t	saved;
	client_respawn_t	resp;

	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	SelectSpawnPoint (ent, spawn_origin, spawn_angles);

	index = ent-game.edicts-1;
	client = ent->r.client;

	// deathmatch wipes most client data every spawn
	if (deathmatch->integer)
	{
		char		userinfo[MAX_INFO_STRING];

		resp = client->resp;
		memcpy (userinfo, client->pers.userinfo, sizeof(userinfo));
		InitClientPersistant (client);
		ClientUserinfoChanged (ent, userinfo);
	}
	else if (coop->integer)
	{
		char		userinfo[MAX_INFO_STRING];

		resp = client->resp;
		memcpy (userinfo, client->pers.userinfo, sizeof(userinfo));
		client->pers = resp.coop_respawn;
		ClientUserinfoChanged (ent, userinfo);
		if (resp.score > client->pers.score)
			client->pers.score = resp.score;
	}
	else
	{
		memset (&resp, 0, sizeof(resp));
	}

	// clear everything but the persistant data
	saved = client->pers;
	memset (client, 0, sizeof(*client));
	client->pers = saved;
	if (client->pers.health <= 0)
		InitClientPersistant(client);
	client->resp = resp;

	// copy some data from the client to the entity
	FetchClientEntData (ent);

	// clear entity values
	ent->groundentity = NULL;
	ent->r.client = &game.clients[index];
	ent->takedamage = DAMAGE_AIM;
	ent->movetype = MOVETYPE_WALK;
	ent->viewheight = 22;
	ent->r.inuse = qtrue;
	ent->classname = "player";
	ent->mass = 200;
	ent->r.solid = SOLID_BBOX;
	ent->deadflag = DEAD_NO;
	ent->air_finished = level.time + 12;
	ent->r.clipmask = MASK_PLAYERSOLID;
	ent->pain = player_pain;
	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->flags &= ~FL_NO_KNOCKBACK;
	ent->r.svflags &= ~SVF_CORPSE;

	VectorCopy (mins, ent->r.mins);
	VectorCopy (maxs, ent->r.maxs);
	VectorClear (ent->velocity);

	// clear playerstate values
	memset (&ent->r.client->ps, 0, sizeof(client->ps));

	client->ps.pmove.origin[0] = spawn_origin[0]*16;
	client->ps.pmove.origin[1] = spawn_origin[1]*16;
	client->ps.pmove.origin[2] = spawn_origin[2]*16;
//ZOID
	client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
//ZOID

	if (deathmatch->integer && (dmflags->integer & DF_FIXED_FOV))
	{
		client->ps.fov = 90;
	}
	else
	{
		client->ps.fov = atoi(Info_ValueForKey(client->pers.userinfo, "fov"));
		if (client->ps.fov < 1)
			client->ps.fov = 90;
		else if (client->ps.fov > 160)
			client->ps.fov = 160;
	}

	// clear entity state values
	ent->s.type = ET_PLAYER;
	ent->s.effects = 0;
	ent->s.light = 0;
	ent->s.modelindex = 255;		// will use the skin specified model
	ent->s.modelindex2 = 255;		// custom gun model
	ent->s.skinnum = ent - game.edicts - 1;

	ent->pmAnim.anim_priority[LOWER] = ANIM_BASIC;
	ent->pmAnim.anim_priority[UPPER] = ANIM_BASIC;
	ent->pmAnim.anim_priority[HEAD] = ANIM_BASIC;

	ent->pmAnim.anim[LOWER] = LEGS_STAND;
	ent->pmAnim.anim[UPPER] = TORSO_STAND;
	ent->pmAnim.anim[HEAD] = ANIM_NONE;

	ent->s.frame = 0;
	VectorCopy (spawn_origin, ent->s.origin);
	ent->s.origin[2] += 1;	// make sure off ground
	VectorCopy (ent->s.origin, ent->s.old_origin);

	// set angles
	ent->s.angles[PITCH] = 0;
	ent->s.angles[YAW] = spawn_angles[YAW];
	ent->s.angles[ROLL] = 0;
	VectorCopy (ent->s.angles, client->ps.viewangles);
	VectorCopy (ent->s.angles, client->v_angle);

	// set the delta angle 
	for (i=0 ; i<3 ; i++) 
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->s.angles[i] - client->resp.cmd_angles[i]); 

//ZOID
	if (ctf->integer && CTFStartClient(ent))
		return;
//ZOID

	if (!KillBox (ent))
	{	// could't spawn in?
	}

	trap_LinkEntity (ent);

	// force the current weapon up
	client->newweapon = client->pers.weapon;
	ChangeWeapon (ent);
}

/*
=====================
ClientBeginDeathmatch

A client has just connected to the server in 
deathmatch mode, so clear everything out before starting them.
=====================
*/
void ClientBeginDeathmatch (edict_t *ent)
{
	G_InitEdict (ent);

	InitClientResp (ent->r.client);

	// locate ent at a spawn point
	PutClientInServer (ent);

	if (level.intermissiontime)
	{
		MoveClientToIntermission (ent);
	}
	else
	{
		edict_t *event;

		// send effect
		event = G_SpawnEvent ( EV_PLAYER_TELEPORT_IN, 0, ent->s.origin );
		event->r.svflags = SVF_NOOLDORIGIN;
		event->s.ownerNum = ent - game.edicts;
	}

	G_PrintMsg (NULL, PRINT_HIGH, "%s %sentered the game\n", ent->r.client->pers.netname, S_COLOR_WHITE);

	// make sure all view stuff is valid
	ClientEndServerFrame (ent);
}


/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
void ClientBegin (edict_t *ent)
{
	int		i;

	ent->r.client = game.clients + (ent - game.edicts - 1);

	if (deathmatch->integer)
	{
		ClientBeginDeathmatch (ent);
		return;
	}

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->r.inuse == qtrue)
	{
		// the client has cleared the client side viewangles upon
		// connecting to the server, which is different than the
		// state when the game is saved, so we need to compensate
		// with deltaangles
		for (i=0 ; i<3 ; i++)
			ent->r.client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->r.client->ps.viewangles[i]);
	}
	else
	{
		// a spawn point will completely reinitialize the entity
		// except for the persistant data that was initialized at
		// ClientConnect() time
		G_InitEdict (ent);
		ent->classname = "player";
		InitClientResp (ent->r.client);
		PutClientInServer (ent);
	}

	if (level.intermissiontime)
	{
		MoveClientToIntermission (ent);
	}
	else
	{
		// send effect if in a multiplayer game
		if (game.maxclients > 1)
		{
			edict_t *event;

			event = G_SpawnEvent ( EV_PLAYER_TELEPORT_IN, 0, ent->s.origin );
			event->r.svflags = SVF_NOOLDORIGIN;
			event->s.ownerNum = ent - game.edicts;

			G_PrintMsg ( NULL, PRINT_HIGH, "%s %sentered the game\n", ent->r.client->pers.netname, S_COLOR_WHITE );
		}
	}

	// make sure all view stuff is valid
	ClientEndServerFrame (ent);
}

/*
===========
ClientUserInfoChanged

called whenever the player updates a userinfo variable.

The game can override any of the settings in place
(forcing skins or names, etc) before copying it off.
============
*/
void ClientUserinfoChanged (edict_t *ent, char *userinfo)
{
	char	*s;
	int		playernum;
	gclient_t *cl;

	cl = ent->r.client;

	// check for malformed or illegal info strings
	if (!Info_Validate(userinfo))
		Q_snprintfz( userinfo, sizeof( userinfo ), "\\name\\badinfo\\hand\\0\\skin\\%s/%s", DEFAULT_PLAYERMODEL, DEFAULT_PLAYERSKIN );

	// set name
	s = Info_ValueForKey (userinfo, "name");
	Q_strncpyz (cl->pers.netname, s, sizeof(cl->pers.netname));

	// handedness
	s = Info_ValueForKey (userinfo, "hand");
	if( strlen(s) )
		cl->pers.hand = atoi(s);

	// set skin
	s = Info_ValueForKey (userinfo, "skin");

	playernum = ent-game.edicts-1;

	// combine name and skin into a configstring
//ZOID
	if (ctf->integer)
		CTFAssignSkin(ent, s);
	else
//ZOID
		trap_ConfigString (CS_PLAYERSKINS+playernum, va("%s\\%i\\%s", cl->pers.netname, cl->pers.hand, s) );

//ZOID
	// set player name field (used in id_state view)
	trap_ConfigString (CS_GENERAL+playernum, cl->pers.netname);
//ZOID

	// fov
	if (deathmatch->integer && (dmflags->integer & DF_FIXED_FOV))
	{
		cl->ps.fov = 90;
	}
	else
	{
		cl->ps.fov = atoi(Info_ValueForKey(userinfo, "fov"));
		if (cl->ps.fov < 1)
			cl->ps.fov = 90;
		else if (cl->ps.fov > 160)
			cl->ps.fov = 160;
	}

	// msg command
	s = Info_ValueForKey (userinfo, "msg");
	if (strlen(s))
	{
		cl->pers.messagelevel = atoi(s);
	}

	// save off the userinfo in case we want to check something later
	Q_strncpyz (cl->pers.userinfo, userinfo, sizeof(cl->pers.userinfo));
}


/*
===========
ClientConnect

Called when a player begins connecting to the server.
The game can refuse entrance to a client by returning false.
If the client is allowed, the connection process will continue
and eventually get to ClientBegin()
Changing levels will NOT cause this to be called again, but
loadgames will.
============
*/
qboolean ClientConnect (edict_t *ent, char *userinfo, qboolean fakeClient)
{
	char	*value;
	int		numents = game.maxentities;

	// check to see if they are on the banned IP list
	value = Info_ValueForKey (userinfo, "ip");
	if (SV_FilterPacket(value)) {
		Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
		return qfalse;
	}


	// check for a password
	value = Info_ValueForKey (userinfo, "password");
	if (*password->string && strcmp(password->string, "none") && 
		strcmp(password->string, value)) {
		Info_SetValueForKey(userinfo, "rejmsg", "Password required or incorrect.");
		return qfalse;
	}

	// they can connect
	ent->r.client = game.clients + (ent - game.edicts - 1);

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->r.inuse == qfalse)
	{
		// clear the respawning variables
//ZOID -- force team join
		ent->r.client->resp.ctf_team = -1;
		ent->r.client->resp.id_state = qtrue; 
//ZOID
		InitClientResp (ent->r.client);
		if (!game.autosaved || !ent->r.client->pers.weapon)
			InitClientPersistant (ent->r.client);
	}

	ClientUserinfoChanged (ent, userinfo);

	if (game.maxclients > 1)
		G_Printf ("%s %sconnected\n", ent->r.client->pers.netname, S_COLOR_WHITE);

	// make sure we start with known default
	if (fakeClient)
		ent->r.svflags = SVF_FAKECLIENT;
	else
		ent->r.svflags = 0;
	ent->r.client->pers.connected = qtrue;
	return qtrue;
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void ClientDisconnect (edict_t *ent)
{
	edict_t	*event;
	int		playernum;

	if (!ent->r.client)
		return;

	G_PrintMsg (NULL, PRINT_HIGH, "%s %sdisconnected\n", ent->r.client->pers.netname, S_COLOR_WHITE);

//ZOID
	CTFDeadDropFlag(ent);
	CTFDeadDropTech(ent);
//ZOID

	// send effect
	event = G_SpawnEvent ( EV_PLAYER_TELEPORT_OUT, 0, ent->s.origin );
	event->r.svflags = SVF_NOOLDORIGIN;
	event->s.ownerNum = ent - game.edicts - 1;

	trap_UnlinkEntity (ent);
	ent->s.modelindex = 0;
	ent->r.solid = SOLID_NOT;
	ent->r.inuse = qfalse;
	ent->classname = "disconnected";
	ent->r.client->pers.connected = qfalse;

	playernum = ent-game.edicts-1;
	trap_ConfigString (CS_PLAYERSKINS+playernum, "");
}


//==============================================================


edict_t	*pm_passent;

// pmove doesn't need to know about passent and contentmask
void PM_trace (trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	if (pm_passent->health > 0)
		trap_Trace (tr, start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
	else
		trap_Trace (tr, start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink (edict_t *ent, usercmd_t *ucmd)
{
	gclient_t	*client;
	edict_t	*other;
	int		i, j, xyspeedcheck;
	pmove_t	pm;

	level.current_entity = ent;
	client = ent->r.client;

	// clear events here, because ClientThink can be called
	// several times during one server frame (G_RunFrame hasn't advanced yet)
	if (!ent->s.events[0]) {
		ent->numEvents = 0;
		ent->eventPriority[0] = ent->eventPriority[1] = qfalse;
	}

	if (level.intermissiontime)
	{
		client->ps.pmove.pm_type = PM_FREEZE;
		// can exit intermission after five seconds
		if (level.time > level.intermissiontime + 5.0 
			&& (ucmd->buttons & BUTTON_ANY) )
			level.exitintermission = qtrue;
		return;
	}

	pm_passent = ent;

//ZOID
	if( ent->r.client->chase_target ) {
		client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
		client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
		client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);
		if( ucmd->upmove && level.time > client->chase.keytime ) {
			client->chase.keytime = level.time + 1;
			client->chase.keyNext = qtrue;
		}
		return;
	}
//ZOID

	// set up for pmove
	memset (&pm, 0, sizeof(pm));

	if (ent->movetype == MOVETYPE_NOCLIP)
		client->ps.pmove.pm_type = PM_SPECTATOR;
	else if (ent->s.modelindex != 255)
		client->ps.pmove.pm_type = PM_GIB;
	else if (ent->deadflag)
		client->ps.pmove.pm_type = PM_DEAD;
	else
		client->ps.pmove.pm_type = PM_NORMAL;

	client->ps.pmove.gravity = sv_gravity->value;
	pm.s = client->ps.pmove;

	for (i=0 ; i<3 ; i++)
	{
		pm.s.origin[i] = ent->s.origin[i]*16;
		pm.s.velocity[i] = ent->velocity[i]*16;
	}

	if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s)))
	{
		pm.snapinitial = qtrue;
//		G_Printf ("pmove changed!\n");
	}

	pm.cmd = *ucmd;

	pm.trace = PM_trace;	// adds default parms
	pm.pointcontents = trap_PointContents;
	pm.airaccelerate = sv_airaccelerate->value;

	// perform a pmove
	Pmove (&pm);

	// save results of pmove
	client->ps.pmove = pm.s;
	client->old_pmove = pm.s;

	for (i=0 ; i<3 ; i++)
	{
		ent->s.origin[i] = pm.s.origin[i]*(1.0/16.0);
		ent->velocity[i] = pm.s.velocity[i]*(1.0/16.0);
	}

	VectorCopy (pm.mins, ent->r.mins);
	VectorCopy (pm.maxs, ent->r.maxs);

	client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
	client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
	client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);

	xyspeedcheck = sqrt(ent->velocity[0]*ent->velocity[0] + ent->velocity[1]*ent->velocity[1]);

	if (ent->groundentity && (pm.groundentity == -1) && (pm.cmd.upmove >= 10) && (pm.waterlevel == 0))
	{
		G_AddEvent( ent, EV_JUMP, 0, qtrue );
		PlayerNoise( ent, ent->s.origin, PNOISE_SELF );
		// launch jump animations
		if( pm.s.velocity[2] < 5500 || !ent->pmAnim.anim_jump ) { // is not a double jump
			if( ent->pmAnim.anim_jump == qtrue && ent->pmAnim.anim_jump_thunk == qtrue )
			{	// rebounce
				if( xyspeedcheck > 50 ) {
					if( ent->pmAnim.anim_jump_style < 2 )
						ent->pmAnim.anim_jump_style = 2;
					else
						ent->pmAnim.anim_jump_style = 1;
				} else
					ent->pmAnim.anim_jump_style = 0;
				
			} else {	// is a simple jump
				if( pm.cmd.forwardmove >= 20 && xyspeedcheck > 50 ) {
					if( ent->pmAnim.anim_jump_style < 2 )
						ent->pmAnim.anim_jump_style = 2;
					else
						ent->pmAnim.anim_jump_style = 1;
				} else
					ent->pmAnim.anim_jump_style = 0;
			}
			//set the jump as active and launch the animation change process.
			ent->pmAnim.anim_jump_thunk = qfalse;
			ent->pmAnim.anim_jump = qtrue;
		}
	}

	ent->viewheight = pm.viewheight;
	ent->waterlevel = pm.waterlevel;
	ent->watertype = pm.watertype;
	if (pm.groundentity != -1)
	{
		ent->groundentity = &game.edicts[pm.groundentity];
		ent->groundentity_linkcount = ent->groundentity->r.linkcount;
	}
	else
	{
		ent->groundentity = NULL;
	}

	if (ent->deadflag)
	{
		client->ps.viewangles[ROLL] = 40;
		client->ps.viewangles[PITCH] = -15;
		client->ps.viewangles[YAW] = client->killer_yaw;
	}
	else
	{
		VectorCopy (pm.viewangles, client->v_angle);
		VectorCopy (pm.viewangles, client->ps.viewangles);
	}

//ZOID
	if (client->ctf_grapple)
		CTFGrapplePull(client->ctf_grapple);
//ZOID

	trap_LinkEntity (ent);

	if (ent->movetype != MOVETYPE_NOCLIP)
		G_TouchTriggers (ent);

	// touch other objects
	for (i=0 ; i<pm.numtouch ; i++)
	{
		other = &game.edicts[pm.touchents[i]];
		for (j=0 ; j<i ; j++)
			if (&game.edicts[pm.touchents[j]] == other)
				break;
		if (j != i)
			continue;	// duplicated
		if (!other->touch)
			continue;
		other->touch (other, ent, NULL, 0);
	}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	ent->pmAnim.anim_moveflags = 0;//start from 0
	if( ucmd->forwardmove < -1 )
		ent->pmAnim.anim_moveflags |= ANIMMOVE_BACK;
	else if( ucmd->forwardmove > 1 )
		ent->pmAnim.anim_moveflags |= ANIMMOVE_FRONT;
	if( ucmd->sidemove < -1 )
		ent->pmAnim.anim_moveflags |= ANIMMOVE_LEFT;
	else if( ucmd->sidemove > 1 )
		ent->pmAnim.anim_moveflags |= ANIMMOVE_RIGHT;
	if( xyspeedcheck > 240 )
		ent->pmAnim.anim_moveflags |= ANIMMOVE_RUN;
	else if( xyspeedcheck )
		ent->pmAnim.anim_moveflags |= ANIMMOVE_WALK;
	if( client->ps.pmove.pm_flags & PMF_DUCKED )
		ent->pmAnim.anim_moveflags |= ANIMMOVE_DUCK;
	if( client->ctf_grapple != NULL )
		ent->pmAnim.anim_moveflags |= ANIMMOVE_GRAPPLE;

	// fire weapon from final position if needed
	if (client->latched_buttons & BUTTON_ATTACK
//ZOID
		&& ent->movetype != MOVETYPE_NOCLIP
//ZOID
		)
	{
		if (!client->weapon_thunk)
		{
			client->weapon_thunk = qtrue;
			Think_Weapon (ent);
		}
	}

//ZOID
//regen tech
	CTFApplyRegeneration(ent);
//ZOID

//ZOID
	for (i = 1; i <= game.maxclients; i++) {
		other = game.edicts + i;
		if (other->r.inuse && other->r.client->chase_target == ent)
			UpdateChaseCam(other);
	}

	if (client->menudirty && client->menutime <= level.time) {
		trap_Layout (ent, PMenu_Do_Update (ent));
		client->menutime = level.time;
		client->menudirty = qfalse;
	}
//ZOID
}


/*
==============
ClientBeginServerFrame

This will be called once for each server frame, before running
any other entities in the world.
==============
*/
void ClientBeginServerFrame (edict_t *ent)
{
	gclient_t	*client;
	int			buttonMask;

	if (level.intermissiontime)
		return;

	client = ent->r.client;

	// run weapon animations if it hasn't been done by a ucmd_t
	if (!client->weapon_thunk
//ZOID
		&& ent->movetype != MOVETYPE_NOCLIP
//ZOID
		)
		Think_Weapon (ent);
	else
		client->weapon_thunk = qfalse;

	if (ent->deadflag)
	{
		// wait for any button just going down
		if (level.time > client->respawn_time)
		{
			// in deathmatch, only wait for attack button
			if (deathmatch->integer)
				buttonMask = BUTTON_ATTACK;
			else
				buttonMask = -1;

			if ( ( client->latched_buttons & buttonMask ) ||
				(deathmatch->integer && (dmflags->integer & DF_FORCE_RESPAWN) ) ||
				CTFMatchOn())
			{
				respawn(ent);
				client->latched_buttons = 0;
			}
		}
		return;
	}

	// add player trail so monsters can follow
	if (!deathmatch->integer)
		if (!visible (ent, PlayerTrail_LastSpot() ) )
			PlayerTrail_Add (ent->s.old_origin);

	client->latched_buttons = 0;
}
