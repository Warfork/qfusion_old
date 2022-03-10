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

typedef enum {
	MATCH_NONE,
	MATCH_SETUP,
	MATCH_PREGAME,
	MATCH_GAME,
	MATCH_POST
} match_t;

typedef enum {
	ELECT_NONE,
	ELECT_MATCH,
	ELECT_ADMIN,
	ELECT_MAP
} elect_t;

typedef struct
{
	int team1, team2;
	int total1, total2; // these are only set when going into intermission!
	float last_flag_capture;
	int last_capture_team;

	match_t match;		// match state
	float matchtime;	// time for match start/end (depends on state)
	int lasttime;		// last time update
	qboolean countdown;	// has audio countdown started?

	elect_t election;	// election type
	edict_t *etarget;	// for admin election, who's being elected
	char elevel[32];	// for map election, target level
	int evotes;			// votes so far
	int needvotes;		// votes needed
	float electtime;	// remaining time until election times out
	char emsg[256];		// election name
	int warnactive; // true if stat string 30 is active

	ghost_t ghosts[MAX_CLIENTS]; // ghost codes
} ctfgame_t;

ctfgame_t ctfgame;

cvar_t *ctf;
cvar_t *ctf_forcejoin;

cvar_t *competition;
cvar_t *matchlock;
cvar_t *electpercentage;
cvar_t *matchtime;
cvar_t *matchsetuptime;
cvar_t *matchstarttime;
cvar_t *admin_password;
cvar_t *allow_admin;
cvar_t *warp_list;
cvar_t *warn_unbalanced;

// Index for various CTF pics, this saves us from calling trap_ImageIndex
// all the time and saves a few CPU cycles since we don't have to do
// a bunch of string compares all the time.
// These are set in CTFPrecache() called from worldspawn
int imageindex_i_ctf1;
int imageindex_i_ctf2;
int imageindex_i_ctf1d;
int imageindex_i_ctf2d;
int imageindex_i_ctf1t;
int imageindex_i_ctf2t;
int imageindex_i_ctfj;
int imageindex_sbfctf1;
int imageindex_sbfctf2;
int imageindex_ctfsb1;
int imageindex_ctfsb2;

char *ctf_statusbar = "ctf";

static char *tnames[] = {
	"item_tech1", "item_tech2", "item_tech3", "item_tech4",
	NULL
};

/*--------------------------------------------------------------------------*/

static void loc_buildboxpoints(vec3_t p[8], vec3_t org, vec3_t mins, vec3_t maxs)
{
	VectorAdd(org, mins, p[0]);
	VectorCopy(p[0], p[1]);
	p[1][0] -= mins[0];
	VectorCopy(p[0], p[2]);
	p[2][1] -= mins[1];
	VectorCopy(p[0], p[3]);
	p[3][0] -= mins[0];
	p[3][1] -= mins[1];
	VectorAdd(org, maxs, p[4]);
	VectorCopy(p[4], p[5]);
	p[5][0] -= maxs[0];
	VectorCopy(p[0], p[6]);
	p[6][1] -= maxs[1];
	VectorCopy(p[0], p[7]);
	p[7][0] -= maxs[0];
	p[7][1] -= maxs[1];
}

static qboolean loc_CanSee (edict_t *targ, edict_t *inflictor)
{
	trace_t	trace;
	vec3_t	targpoints[8];
	int i;
	vec3_t viewpoint;

// bmodels need special checking because their origin is 0,0,0
	if (targ->movetype == MOVETYPE_PUSH)
		return qfalse; // bmodels not supported

	loc_buildboxpoints(targpoints, targ->s.origin, targ->r.mins, targ->r.maxs);
	
	VectorCopy(inflictor->s.origin, viewpoint);
	viewpoint[2] += inflictor->viewheight;

	for (i = 0; i < 8; i++) {
		trap_Trace (&trace, viewpoint, vec3_origin, vec3_origin, targpoints[i], inflictor, MASK_SOLID);
		if (trace.fraction == 1.0)
			return qtrue;
	}

	return qfalse;
}

/*--------------------------------------------------------------------------*/

static gitem_t *flag1_item;
static gitem_t *flag2_item;

void CTFSpawn(void)
{
	if (!flag1_item)
		flag1_item = FindItemByClassname("team_CTF_redflag");
	if (!flag2_item)
		flag2_item = FindItemByClassname("team_CTF_blueflag");
	memset(&ctfgame, 0, sizeof(ctfgame));
	CTFSetupTechSpawn();

	if (competition->integer > 1) {
		ctfgame.match = MATCH_SETUP;
		ctfgame.matchtime = level.time + matchsetuptime->value * 60;
	}
}

void CTFInit(void)
{
	ctf = trap_Cvar_Get("ctf", "0", CVAR_SERVERINFO);
	ctf_forcejoin = trap_Cvar_Get("ctf_forcejoin", "", 0);
	competition = trap_Cvar_Get("competition", "0", CVAR_SERVERINFO);
	matchlock = trap_Cvar_Get("matchlock", "1", CVAR_SERVERINFO);
	electpercentage = trap_Cvar_Get("electpercentage", "66", 0);
	matchtime = trap_Cvar_Get("matchtime", "20", CVAR_SERVERINFO);
	matchsetuptime = trap_Cvar_Get("matchsetuptime", "10", 0);
	matchstarttime = trap_Cvar_Get("matchstarttime", "20", 0);
	admin_password = trap_Cvar_Get("admin_password", "", 0);
	allow_admin = trap_Cvar_Get("allow_admin", "1", 0);
	warp_list = trap_Cvar_Get("warp_list", "q2ctf1 q2ctf2 q2ctf3 q2ctf4 q2ctf5", 0);
	warn_unbalanced = trap_Cvar_Get("warn_unbalanced", "1", 0);
}

/*
 * Precache CTF items
 */

void CTFPrecache(void)
{
	imageindex_i_ctf1 =   trap_ImageIndex("pics/i_ctf1"); 
	imageindex_i_ctf2 =   trap_ImageIndex("pics/i_ctf2"); 
	imageindex_i_ctf1d =  trap_ImageIndex("pics/i_ctf1d");
	imageindex_i_ctf2d =  trap_ImageIndex("pics/i_ctf2d");
	imageindex_i_ctf1t =  trap_ImageIndex("pics/i_ctf1t");
	imageindex_i_ctf2t =  trap_ImageIndex("pics/i_ctf2t");
	imageindex_i_ctfj =   trap_ImageIndex("pics/i_ctfj"); 
	imageindex_sbfctf1 =  trap_ImageIndex("pics/sbfctf1");
	imageindex_sbfctf2 =  trap_ImageIndex("pics/sbfctf2");
	imageindex_ctfsb1 =   trap_ImageIndex("pics/ctfsb1");
	imageindex_ctfsb2 =   trap_ImageIndex("pics/ctfsb2");
}

/*--------------------------------------------------------------------------*/

char *CTFTeamName(int team)
{
	switch (team) {
	case CTF_TEAM1:
		return "RED";
	case CTF_TEAM2:
		return "BLUE";
	}
	return "UNKNOWN"; // Hanzo pointed out this was spelled wrong as "UKNOWN"
}

char *CTFOtherTeamName(int team)
{
	switch (team) {
	case CTF_TEAM1:
		return "BLUE";
	case CTF_TEAM2:
		return "RED";
	}
	return "UNKNOWN"; // Hanzo pointed out this was spelled wrong as "UKNOWN"
}

int CTFOtherTeam(int team)
{
	switch (team) {
	case CTF_TEAM1:
		return CTF_TEAM2;
	case CTF_TEAM2:
		return CTF_TEAM1;
	}
	return -1; // invalid value
}

/*--------------------------------------------------------------------------*/

edict_t *SelectRandomDeathmatchSpawnPoint (void);
edict_t *SelectFarthestDeathmatchSpawnPoint (void);
float	PlayersRangeFromSpot (edict_t *spot);

void CTFAssignSkin(edict_t *ent, char *s)
{
	int playernum = ent-game.edicts-1;
	char *p;
	char t[64];

	Q_snprintfz(t, sizeof(t), "%s", s);

	if ((p = strchr(t, '/')) != NULL)
		p[1] = 0;
	else
		strcpy(t, "male/");

	switch (ent->r.client->resp.ctf_team) {
	case CTF_TEAM1:
		trap_ConfigString (CS_PLAYERSKINS+playernum, va("%s\\%i\\%s%s", 
			ent->r.client->pers.netname, ent->r.client->pers.hand, t, CTF_TEAM1_SKIN) );
		break;
	case CTF_TEAM2:
		trap_ConfigString (CS_PLAYERSKINS+playernum,
			va("%s\\%i\\%s%s", ent->r.client->pers.netname, ent->r.client->pers.hand, t, CTF_TEAM2_SKIN) );
		break;
	default:
		trap_ConfigString (CS_PLAYERSKINS+playernum, 
			va("%s\\%i\\%s", ent->r.client->pers.netname, ent->r.client->pers.hand, s) );
		break;
	}

}

void CTFAssignTeam(gclient_t *who)
{
	edict_t		*player;
	int i;
	int team1count = 0, team2count = 0;

	who->resp.ctf_state = 0;

	if (!(dmflags->integer & DF_CTF_FORCEJOIN)) {
		who->resp.ctf_team = CTF_NOTEAM;
		return;
	}

	for (i = 1; i <= game.maxclients; i++) {
		player = &game.edicts[i];

		if (!player->r.inuse || player->r.client == who)
			continue;

		switch (player->r.client->resp.ctf_team) {
		case CTF_TEAM1:
			team1count++;
			break;
		case CTF_TEAM2:
			team2count++;
		}
	}
	if (team1count < team2count)
		who->resp.ctf_team = CTF_TEAM1;
	else if (team2count < team1count)
		who->resp.ctf_team = CTF_TEAM2;
	else if (rand() & 1)
		who->resp.ctf_team = CTF_TEAM1;
	else
		who->resp.ctf_team = CTF_TEAM2;
}

/*
================
SelectCTFSpawnPoint

go to a ctf point, but NOT the two points closest
to other players
================
*/
edict_t *SelectCTFSpawnPoint (edict_t *ent)
{
	edict_t	*spot, *spot1, *spot2;
	int		count = 0;
	int		selection;
	float	range, range1, range2;
	char	*cname;

	if (ent->r.client->resp.ctf_state) {
		switch (ent->r.client->resp.ctf_team) {
		case CTF_TEAM1:
			cname = "team_CTF_redplayer";
			break;
		case CTF_TEAM2:
			cname = "team_CTF_blueplayer";
			break;
		default:
			return SelectRandomDeathmatchSpawnPoint();
		}
	} else {
		ent->r.client->resp.ctf_state++;

		switch (ent->r.client->resp.ctf_team) {
		case CTF_TEAM1:
			cname = "team_CTF_redspawn";
			break;
		case CTF_TEAM2:
			cname = "team_CTF_bluespawn";
			break;
		default:
			return SelectRandomDeathmatchSpawnPoint();
		}
	}

	spot = NULL;
	range1 = range2 = 99999;
	spot1 = spot2 = NULL;

	while ((spot = G_Find (spot, FOFS(classname), cname)) != NULL)
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
		return SelectRandomDeathmatchSpawnPoint();

	if (count <= 2)
	{
		spot1 = spot2 = NULL;
	}
	else
		count -= 2;

	selection = rand() % count;

	spot = NULL;
	do
	{
		spot = G_Find (spot, FOFS(classname), cname);
		if (spot == spot1 || spot == spot2)
			selection++;
	} while(selection--);

	return spot;
}

/*------------------------------------------------------------------------*/
/*
CTFFragBonuses

Calculate the bonuses for flag defense, flag carrier defense, etc.
Note that bonuses are not cumaltive.  You get one, they are in importance
order.
*/
void CTFFragBonuses(edict_t *targ, edict_t *inflictor, edict_t *attacker)
{
	int i;
	edict_t *ent;
	gitem_t *flag_item, *enemy_flag_item;
	int otherteam;
	edict_t *flag, *carrier;
	char *c;
	vec3_t v1, v2;

	if (targ->r.client && attacker->r.client) {
		if (attacker->r.client->resp.ghost)
			if (attacker != targ)
				attacker->r.client->resp.ghost->kills++;
		if (targ->r.client->resp.ghost)
			targ->r.client->resp.ghost->deaths++;
	}

	// no bonus for fragging yourself
	if (!targ->r.client || !attacker->r.client || targ == attacker)
		return;

	otherteam = CTFOtherTeam(targ->r.client->resp.ctf_team);
	if (otherteam < 0)
		return; // whoever died isn't on a team

	// same team, if the flag at base, check to he has the enemy flag
	if (targ->r.client->resp.ctf_team == CTF_TEAM1) {
		flag_item = flag1_item;
		enemy_flag_item = flag2_item;
	} else {
		flag_item = flag2_item;
		enemy_flag_item = flag1_item;
	}

	// did the attacker frag the flag carrier?
	if (targ->r.client->pers.inventory[ITEM_INDEX(enemy_flag_item)]) {
		attacker->r.client->resp.ctf_lastfraggedcarrier = level.time;
		attacker->r.client->resp.score += CTF_FRAG_CARRIER_BONUS;
		G_PrintMsg (attacker, PRINT_MEDIUM, "BONUS: %d points for fragging enemy flag carrier.\n",
			CTF_FRAG_CARRIER_BONUS);

		// the target had the flag, clear the hurt carrier
		// field on the other team
		for (i = 1; i <= game.maxclients; i++) {
			ent = game.edicts + i;
			if (ent->r.inuse && ent->r.client->resp.ctf_team == otherteam)
				ent->r.client->resp.ctf_lasthurtcarrier = 0;
		}
		return;
	}

	if (targ->r.client->resp.ctf_lasthurtcarrier &&
		level.time - targ->r.client->resp.ctf_lasthurtcarrier < CTF_CARRIER_DANGER_PROTECT_TIMEOUT &&
		!attacker->r.client->pers.inventory[ITEM_INDEX(flag_item)]) {
		// attacker is on the same team as the flag carrier and
		// fragged a guy who hurt our flag carrier
		attacker->r.client->resp.score += CTF_CARRIER_DANGER_PROTECT_BONUS;
		G_PrintMsg (NULL, PRINT_MEDIUM, "%s%S defends %s's flag carrier against an agressive enemy\n",
			attacker->r.client->pers.netname, S_COLOR_WHITE, 
			CTFTeamName(attacker->r.client->resp.ctf_team));
		if (attacker->r.client->resp.ghost)
			attacker->r.client->resp.ghost->carrierdef++;
		return;
	}

	// flag and flag carrier area defense bonuses

	// we have to find the flag and carrier entities

	// find the flag
	switch (attacker->r.client->resp.ctf_team) {
	case CTF_TEAM1:
		c = "team_CTF_redflag";
		break;
	case CTF_TEAM2:
		c = "team_CTF_blueflag";
		break;
	default:
		return;
	}

	flag = NULL;
	while ((flag = G_Find (flag, FOFS(classname), c)) != NULL) {
		if (!(flag->spawnflags & DROPPED_ITEM))
			break;
	}

	if (!flag)
		return; // can't find attacker's flag

	// find attacker's team's flag carrier
	for (i = 1; i <= game.maxclients; i++) {
		carrier = game.edicts + i;
		if (carrier->r.inuse && 
			carrier->r.client->pers.inventory[ITEM_INDEX(flag_item)])
			break;
		carrier = NULL;
	}

	// ok we have the attackers flag and a pointer to the carrier

	// check to see if we are defending the base's flag
	VectorSubtract(targ->s.origin, flag->s.origin, v1);
	VectorSubtract(attacker->s.origin, flag->s.origin, v2);

	if ((VectorLength(v1) < CTF_TARGET_PROTECT_RADIUS ||
		VectorLength(v2) < CTF_TARGET_PROTECT_RADIUS ||
		loc_CanSee(flag, targ) || loc_CanSee(flag, attacker)) &&
		attacker->r.client->resp.ctf_team != targ->r.client->resp.ctf_team) {
		// we defended the base flag
		attacker->r.client->resp.score += CTF_FLAG_DEFENSE_BONUS;
		if (flag->r.solid == SOLID_NOT)
			G_PrintMsg (NULL, PRINT_MEDIUM, "%s%s defends the %s base.\n",
				attacker->r.client->pers.netname, S_COLOR_WHITE, 
				CTFTeamName(attacker->r.client->resp.ctf_team));
		else
			G_PrintMsg (NULL, PRINT_MEDIUM, "%s%s defends the %s flag.\n",
				attacker->r.client->pers.netname, S_COLOR_WHITE, 
				CTFTeamName(attacker->r.client->resp.ctf_team));
		if (attacker->r.client->resp.ghost)
			attacker->r.client->resp.ghost->basedef++;
		return;
	}

	if (carrier && carrier != attacker) {
		VectorSubtract(targ->s.origin, carrier->s.origin, v1);
		VectorSubtract(attacker->s.origin, carrier->s.origin, v1);

		if (VectorLength(v1) < CTF_ATTACKER_PROTECT_RADIUS ||
			VectorLength(v2) < CTF_ATTACKER_PROTECT_RADIUS ||
			loc_CanSee(carrier, targ) || loc_CanSee(carrier, attacker)) {
			attacker->r.client->resp.score += CTF_CARRIER_PROTECT_BONUS;
			G_PrintMsg (NULL, PRINT_MEDIUM, "%s%s defends the %s's flag carrier.\n",
				attacker->r.client->pers.netname, S_COLOR_WHITE, 
				CTFTeamName(attacker->r.client->resp.ctf_team));
			if (attacker->r.client->resp.ghost)
				attacker->r.client->resp.ghost->carrierdef++;
			return;
		}
	}
}

void CTFCheckHurtCarrier(edict_t *targ, edict_t *attacker)
{
	gitem_t *flag_item;

	if (!targ->r.client || !attacker->r.client)
		return;

	if (targ->r.client->resp.ctf_team == CTF_TEAM1)
		flag_item = flag2_item;
	else
		flag_item = flag1_item;

	if (targ->r.client->pers.inventory[ITEM_INDEX(flag_item)] &&
		targ->r.client->resp.ctf_team != attacker->r.client->resp.ctf_team)
		attacker->r.client->resp.ctf_lasthurtcarrier = level.time;
}


/*------------------------------------------------------------------------*/

void CTFResetFlag(int ctf_team)
{
	char *c;
	edict_t *ent;

	switch (ctf_team) {
	case CTF_TEAM1:
		c = "team_CTF_redflag";
		break;
	case CTF_TEAM2:
		c = "team_CTF_blueflag";
		break;
	default:
		return;
	}

	ent = NULL;
	while ((ent = G_Find (ent, FOFS(classname), c)) != NULL) {
		if (ent->spawnflags & DROPPED_ITEM)
			G_FreeEdict(ent);
		else {
			ent->r.svflags &= ~SVF_NOCLIENT;
			ent->r.solid = SOLID_TRIGGER;
			trap_LinkEntity (ent);
			G_AddEvent (ent, EV_ITEM_RESPAWN, 0, qtrue);
		}
	}
}

void CTFResetFlags(void)
{
	CTFResetFlag(CTF_TEAM1);
	CTFResetFlag(CTF_TEAM2);
}

qboolean CTFPickup_Flag(edict_t *ent, edict_t *other)
{
	int ctf_team;
	int i;
	edict_t *player;
	gitem_t *flag_item, *enemy_flag_item;

	// figure out what team this flag is
	if (strcmp(ent->classname, "team_CTF_redflag") == 0)
		ctf_team = CTF_TEAM1;
	else if (strcmp(ent->classname, "team_CTF_blueflag") == 0)
		ctf_team = CTF_TEAM2;
	else {
		G_PrintMsg (ent, PRINT_HIGH, "Don't know what team the flag is on.\n");
		return qfalse;
	}

	// same team, if the flag at base, check to he has the enemy flag
	if (ctf_team == CTF_TEAM1) {
		flag_item = flag1_item;
		enemy_flag_item = flag2_item;
	} else {
		flag_item = flag2_item;
		enemy_flag_item = flag1_item;
	}

	if (ctf_team == other->r.client->resp.ctf_team) {

		if (!(ent->spawnflags & DROPPED_ITEM)) {
			// the flag is at home base.  if the player has the enemy
			// flag, he's just won!
		
			if (other->r.client->pers.inventory[ITEM_INDEX(enemy_flag_item)]) {
				G_PrintMsg (NULL, PRINT_HIGH, "%s%s captured the %s flag!\n",
						other->r.client->pers.netname, S_COLOR_WHITE, CTFOtherTeamName(ctf_team));
				other->r.client->pers.inventory[ITEM_INDEX(enemy_flag_item)] = 0;

				ctfgame.last_flag_capture = level.time;
				ctfgame.last_capture_team = ctf_team;
				if (ctf_team == CTF_TEAM1)
					ctfgame.team1++;
				else
					ctfgame.team2++;

				if (ctf_team == CTF_TEAM1) {
					G_GlobalSound (CHAN_VOICE, trap_SoundIndex("sound/teamplay/flagcap_red.wav"));
				} else {
					G_GlobalSound (CHAN_VOICE, trap_SoundIndex("sound/teamplay/flagcap_blu.wav"));
				}

				// other gets another 10 frag bonus
				other->r.client->resp.score += CTF_CAPTURE_BONUS;
				if (other->r.client->resp.ghost)
					other->r.client->resp.ghost->caps++;

				// Ok, let's do the player loop, hand out the bonuses
				for (i = 1; i <= game.maxclients; i++) {
					player = &game.edicts[i];
					if (!player->r.inuse)
						continue;

					if (player->r.client->resp.ctf_team != other->r.client->resp.ctf_team)
						player->r.client->resp.ctf_lasthurtcarrier = -5;
					else if (player->r.client->resp.ctf_team == other->r.client->resp.ctf_team) {
						if (player != other)
							player->r.client->resp.score += CTF_TEAM_BONUS;
						// award extra points for capture assists
						if (player->r.client->resp.ctf_lastreturnedflag + CTF_RETURN_FLAG_ASSIST_TIMEOUT > level.time) {
							G_PrintMsg (NULL, PRINT_HIGH, "%s%s gets an assist for returning the flag!\n", player->r.client->pers.netname, S_COLOR_WHITE);
							player->r.client->resp.score += CTF_RETURN_FLAG_ASSIST_BONUS;
						}
						if (player->r.client->resp.ctf_lastfraggedcarrier + CTF_FRAG_CARRIER_ASSIST_TIMEOUT > level.time) {
							G_PrintMsg (NULL, PRINT_HIGH, "%s%s gets an assist for fragging the flag carrier!\n", player->r.client->pers.netname, S_COLOR_WHITE);
							player->r.client->resp.score += CTF_FRAG_CARRIER_ASSIST_BONUS;
						}
					}
				}

				CTFResetFlags();
				return qfalse;
			}
			return qfalse; // its at home base already
		}

		// hey, its not home.  return it by teleporting it back
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s returned the %s flag!\n", 
			other->r.client->pers.netname, S_COLOR_WHITE, CTFTeamName(ctf_team));
		other->r.client->resp.score += CTF_RECOVERY_BONUS;
		other->r.client->resp.ctf_lastreturnedflag = level.time;
		if (ctf_team == CTF_TEAM1) {
			G_GlobalSound (CHAN_VOICE, trap_SoundIndex("sound/teamplay/flagret_red.wav"));
		} else {
			G_GlobalSound (CHAN_VOICE, trap_SoundIndex("sound/teamplay/flagret_blu.wav"));
		}

		//CTFResetFlag will remove this entity!  We must return false
		CTFResetFlag (ctf_team);
		return qfalse;
	}

	// hey, its not our flag, pick it up
	G_PrintMsg (NULL, PRINT_HIGH, "%s%s got the %s flag!\n",
		other->r.client->pers.netname, S_COLOR_WHITE, CTFTeamName(ctf_team));
	other->r.client->resp.score += CTF_FLAG_BONUS;

	other->r.client->pers.inventory[ITEM_INDEX(flag_item)] = 1;
	other->r.client->resp.ctf_flagsince = level.time;

	if (ctf_team == CTF_TEAM1) {
		G_GlobalSound (CHAN_VOICE, trap_SoundIndex("sound/teamplay/flagtk_red.wav"));
	} else {
		G_GlobalSound (CHAN_VOICE, trap_SoundIndex("sound/teamplay/flagtk_blu.wav"));
	}

	// pick up the flag
	// if it's not a dropped flag, we just make it disappear
	// if it's dropped, it will be removed by the pickup caller
	if (!(ent->spawnflags & DROPPED_ITEM)) {
		ent->flags |= FL_RESPAWN;
		ent->r.svflags |= SVF_NOCLIENT;
		ent->r.solid = SOLID_NOT;
	}
	return qtrue;
}

static void CTFDropFlagTouch(edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags)
{
	//owner (who dropped us) can't touch for two secs
	if (other == ent->r.owner && 
		ent->nextthink - level.time > CTF_AUTO_FLAG_RETURN_TIMEOUT-2)
		return;

	Touch_Item (ent, other, plane, surfFlags);
}

static void CTFDropFlagThink(edict_t *ent)
{
	// auto return the flag
	// reset flag will remove ourselves
	if (strcmp(ent->classname, "team_CTF_redflag") == 0) {
		CTFResetFlag(CTF_TEAM1);
		G_PrintMsg (NULL, PRINT_HIGH, "The %s flag has returned!\n",
			CTFTeamName(CTF_TEAM1));
	} else if (strcmp(ent->classname, "team_CTF_blueflag") == 0) {
		CTFResetFlag(CTF_TEAM2);
		G_PrintMsg (NULL, PRINT_HIGH, "The %s flag has returned!\n",
			CTFTeamName(CTF_TEAM2));
	}
}

// Called from PlayerDie, to drop the flag from a dying player
void CTFDeadDropFlag(edict_t *self)
{
	edict_t *dropped = NULL;

	if (self->r.client->pers.inventory[ITEM_INDEX(flag1_item)]) {
		dropped = Drop_Item(self, flag1_item);
		self->r.client->pers.inventory[ITEM_INDEX(flag1_item)] = 0;
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s lost the %s flag!\n",
			self->r.client->pers.netname, S_COLOR_WHITE, CTFTeamName(CTF_TEAM1));
	} else if (self->r.client->pers.inventory[ITEM_INDEX(flag2_item)]) {
		dropped = Drop_Item(self, flag2_item);
		self->r.client->pers.inventory[ITEM_INDEX(flag2_item)] = 0;
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s lost the %s flag!\n",
			self->r.client->pers.netname, S_COLOR_WHITE, CTFTeamName(CTF_TEAM2));
	}

	if (dropped) {
		dropped->think = CTFDropFlagThink;
		dropped->nextthink = level.time + CTF_AUTO_FLAG_RETURN_TIMEOUT;
		dropped->touch = CTFDropFlagTouch;
	}
}

void CTFDrop_Flag(edict_t *ent, gitem_t *item)
{
	if (rand() & 1) 
		G_PrintMsg (ent, PRINT_HIGH, "Only lusers drop flags.\n");
	else
		G_PrintMsg (ent, PRINT_HIGH, "Winners don't drop flags.\n");
}

void CTFFlagSetup (edict_t *ent)
{
	trace_t		tr;
	vec3_t		dest;

	VectorSet ( ent->r.mins, -15, -15, -15 );
	VectorSet ( ent->r.maxs, 15, 15, 15 );

	if ( ent->model ) {
		ent->s.modelindex = trap_ModelIndex (ent->model);
	} else {
		ent->s.modelindex = trap_ModelIndex (ent->item->world_model[0]);
		ent->s.modelindex2 = trap_ModelIndex (ent->item->world_model[1]);
		ent->s.modelindex3 = trap_ModelIndex (ent->item->world_model[2]);
	}

	ent->r.solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;  
	ent->touch = Touch_Item;

	VectorSet ( dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 128 );

	trap_Trace (&tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent, MASK_SOLID);
	if (tr.startsolid)
	{
		G_Printf ("CTFFlagSetup: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
		G_FreeEdict (ent);
		return;
	}

	VectorCopy (tr.endpos, ent->s.origin);

	trap_LinkEntity (ent);
}

void CTFEffects(edict_t *player)
{
	player->s.effects &= ~(EF_FLAG1 | EF_FLAG2);
	if (player->health > 0) {
		if (player->r.client->pers.inventory[ITEM_INDEX(flag1_item)]) {
			player->s.effects |= EF_FLAG1;
		}
		if (player->r.client->pers.inventory[ITEM_INDEX(flag2_item)]) {
			player->s.effects |= EF_FLAG2;
		}
	}

	if (player->r.client->pers.inventory[ITEM_INDEX(flag1_item)])
		player->s.modelindex3 = trap_ModelIndex (flag1_item->world_model[0]);
	else if (player->r.client->pers.inventory[ITEM_INDEX(flag2_item)])
		player->s.modelindex3 = trap_ModelIndex (flag2_item->world_model[0]);
	else
		player->s.modelindex3 = 0;
}

// called when we enter the intermission
void CTFCalcScores(void)
{
	int i;

	ctfgame.total1 = ctfgame.total2 = 0;
	for (i = 0; i < game.maxclients; i++) {
		if (!game.edicts[i+1].r.inuse)
			continue;
		if (game.clients[i].resp.ctf_team == CTF_TEAM1)
			ctfgame.total1 += game.clients[i].resp.score;
		else if (game.clients[i].resp.ctf_team == CTF_TEAM2)
			ctfgame.total2 += game.clients[i].resp.score;
	}
}

void CTFID_f (edict_t *ent)
{
	if (ent->r.client->resp.id_state) {
		G_PrintMsg (ent, PRINT_HIGH, "Disabling player identication display.\n");
		ent->r.client->resp.id_state = qfalse;
	} else {
		G_PrintMsg (ent, PRINT_HIGH, "Activating player identication display.\n");
		ent->r.client->resp.id_state = qtrue;
	}
}

static void CTFSetIDView(edict_t *ent)
{
	vec3_t	forward, dir;
	trace_t	tr;
	edict_t	*who, *best;
	float	bd = 0, d;
	int i;

	// only check every few frames
	if (level.time - ent->r.client->resp.lastidtime < 0.25)
		return;
	ent->r.client->resp.lastidtime = level.time;

	ent->r.client->ps.stats[STAT_CTF_ID_VIEW] = 0;
	ent->r.client->ps.stats[STAT_CTF_ID_VIEW_COLOR] = 0;

	AngleVectors(ent->r.client->v_angle, forward, NULL, NULL);
	VectorScale(forward, 1024, forward);
	VectorAdd(ent->s.origin, forward, forward);
	trap_Trace(&tr, ent->s.origin, NULL, NULL, forward, ent, MASK_SOLID);
	if (tr.fraction < 1 && game.edicts[tr.ent].r.client) {
		ent->r.client->ps.stats[STAT_CTF_ID_VIEW] = 
			CS_GENERAL + tr.ent - 1;
		if (game.edicts[tr.ent].r.client->resp.ctf_team == CTF_TEAM1)
			ent->r.client->ps.stats[STAT_CTF_ID_VIEW_COLOR] = imageindex_sbfctf1;
		else if (game.edicts[tr.ent].r.client->resp.ctf_team == CTF_TEAM2)
			ent->r.client->ps.stats[STAT_CTF_ID_VIEW_COLOR] = imageindex_sbfctf2;
		return;
	}

	AngleVectors(ent->r.client->v_angle, forward, NULL, NULL);
	best = NULL;
	for (i = 1; i <= game.maxclients; i++) {
		who = game.edicts + i;
		if (!who->r.inuse || who->r.solid == SOLID_NOT)
			continue;
		VectorSubtract(who->s.origin, ent->s.origin, dir);
		VectorNormalize(dir);
		d = DotProduct(forward, dir);
		if (d > bd && loc_CanSee(ent, who)) {
			bd = d;
			best = who;
		}
	}
	if (bd > 0.90) {
		ent->r.client->ps.stats[STAT_CTF_ID_VIEW] = 
			CS_GENERAL + (best - game.edicts - 1);
		if (best->r.client->resp.ctf_team == CTF_TEAM1)
			ent->r.client->ps.stats[STAT_CTF_ID_VIEW_COLOR] = imageindex_sbfctf1;
		else if (best->r.client->resp.ctf_team == CTF_TEAM2)
			ent->r.client->ps.stats[STAT_CTF_ID_VIEW_COLOR] = imageindex_sbfctf2;
	}
}

void SetCTFStats(edict_t *ent)
{
	gitem_t *tech;
	int i;
	int p1, p2;
	edict_t *e;

	if (ctfgame.match > MATCH_NONE)
		ent->r.client->ps.stats[STAT_CTF_MATCH] = CONFIG_CTF_MATCH;
	else
		ent->r.client->ps.stats[STAT_CTF_MATCH] = 0;

	if (ctfgame.warnactive)
		ent->r.client->ps.stats[STAT_CTF_TEAMINFO] = CONFIG_CTF_TEAMINFO;
	else
		ent->r.client->ps.stats[STAT_CTF_TEAMINFO] = 0;

	//ghosting
	if (ent->r.client->resp.ghost) {
		ent->r.client->resp.ghost->score = ent->r.client->resp.score;
		strcpy(ent->r.client->resp.ghost->netname, ent->r.client->pers.netname);
		ent->r.client->resp.ghost->number = ent->s.number;
	}

	// logo headers for the frag display
	ent->r.client->ps.stats[STAT_CTF_TEAM1_HEADER] = imageindex_ctfsb1;
	ent->r.client->ps.stats[STAT_CTF_TEAM2_HEADER] = imageindex_ctfsb2;

	// if during intermission, we must blink the team header of the winning team
	if (level.intermissiontime && (level.framenum & 8)) { // blink 1/8th second
		// note that ctfgame.total[12] is set when we go to intermission
		if (ctfgame.team1 > ctfgame.team2)
			ent->r.client->ps.stats[STAT_CTF_TEAM1_HEADER] = 0;
		else if (ctfgame.team2 > ctfgame.team1)
			ent->r.client->ps.stats[STAT_CTF_TEAM2_HEADER] = 0;
		else if (ctfgame.total1 > ctfgame.total2) // frag tie breaker
			ent->r.client->ps.stats[STAT_CTF_TEAM1_HEADER] = 0;
		else if (ctfgame.total2 > ctfgame.total1) 
			ent->r.client->ps.stats[STAT_CTF_TEAM2_HEADER] = 0;
		else { // tie game!
			ent->r.client->ps.stats[STAT_CTF_TEAM1_HEADER] = 0;
			ent->r.client->ps.stats[STAT_CTF_TEAM2_HEADER] = 0;
		}
	}

	// tech icon
	i = 0;
	ent->r.client->ps.stats[STAT_CTF_TECH] = 0;
	while (tnames[i]) {
		if ((tech = FindItemByClassname(tnames[i])) != NULL &&
			ent->r.client->pers.inventory[ITEM_INDEX(tech)]) {
			ent->r.client->ps.stats[STAT_CTF_TECH] = trap_ImageIndex(tech->icon);
			break;
		}
		i++;
	}

	// figure out what icon to display for team logos
	// three states:
	//   flag at base
	//   flag taken
	//   flag dropped
	p1 = imageindex_i_ctf1;
	e = G_Find(NULL, FOFS(classname), "team_CTF_redflag");
	if (e != NULL) {
		if (e->r.solid == SOLID_NOT) {
			int i;

			// not at base
			// check if on player
			p1 = imageindex_i_ctf1d; // default to dropped
			for (i = 1; i <= game.maxclients; i++)
				if (game.edicts[i].r.inuse &&
					game.edicts[i].r.client->pers.inventory[ITEM_INDEX(flag1_item)]) {
					// enemy has it
					p1 = imageindex_i_ctf1t;
					break;
				}
		} else if (e->spawnflags & DROPPED_ITEM)
			p1 = imageindex_i_ctf1d; // must be dropped
	}
	p2 = imageindex_i_ctf2;
	e = G_Find(NULL, FOFS(classname), "team_CTF_blueflag");
	if (e != NULL) {
		if (e->r.solid == SOLID_NOT) {
			int i;

			// not at base
			// check if on player
			p2 = imageindex_i_ctf2d; // default to dropped
			for (i = 1; i <= game.maxclients; i++)
				if (game.edicts[i].r.inuse &&
					game.edicts[i].r.client->pers.inventory[ITEM_INDEX(flag2_item)]) {
					// enemy has it
					p2 = imageindex_i_ctf2t;
					break;
				}
		} else if (e->spawnflags & DROPPED_ITEM)
			p2 = imageindex_i_ctf2d; // must be dropped
	}


	ent->r.client->ps.stats[STAT_CTF_TEAM1_PIC] = p1;
	ent->r.client->ps.stats[STAT_CTF_TEAM2_PIC] = p2;

	if (ctfgame.last_flag_capture && level.time - ctfgame.last_flag_capture < 5) {
		if (ctfgame.last_capture_team == CTF_TEAM1)
			if (level.framenum & 8)
				ent->r.client->ps.stats[STAT_CTF_TEAM1_PIC] = p1;
			else
				ent->r.client->ps.stats[STAT_CTF_TEAM1_PIC] = 0;
		else
			if (level.framenum & 8)
				ent->r.client->ps.stats[STAT_CTF_TEAM2_PIC] = p2;
			else
				ent->r.client->ps.stats[STAT_CTF_TEAM2_PIC] = 0;
	}

	ent->r.client->ps.stats[STAT_CTF_TEAM1_CAPS] = ctfgame.team1;
	ent->r.client->ps.stats[STAT_CTF_TEAM2_CAPS] = ctfgame.team2;

	ent->r.client->ps.stats[STAT_CTF_FLAG_PIC] = 0;
	if (ent->r.client->resp.ctf_team == CTF_TEAM1 &&
		ent->r.client->pers.inventory[ITEM_INDEX(flag2_item)] &&
		(level.framenum & 8))
		ent->r.client->ps.stats[STAT_CTF_FLAG_PIC] = imageindex_i_ctf2;

	else if (ent->r.client->resp.ctf_team == CTF_TEAM2 &&
		ent->r.client->pers.inventory[ITEM_INDEX(flag1_item)] &&
		(level.framenum & 8))
		ent->r.client->ps.stats[STAT_CTF_FLAG_PIC] = imageindex_i_ctf1;

	ent->r.client->ps.stats[STAT_CTF_JOINED_TEAM1_PIC] = 0;
	ent->r.client->ps.stats[STAT_CTF_JOINED_TEAM2_PIC] = 0;
	if (ent->r.client->resp.ctf_team == CTF_TEAM1)
		ent->r.client->ps.stats[STAT_CTF_JOINED_TEAM1_PIC] = imageindex_i_ctfj;
	else if (ent->r.client->resp.ctf_team == CTF_TEAM2)
		ent->r.client->ps.stats[STAT_CTF_JOINED_TEAM2_PIC] = imageindex_i_ctfj;

	if (ent->r.client->resp.id_state)
		CTFSetIDView(ent);
	else {
		ent->r.client->ps.stats[STAT_CTF_ID_VIEW] = 0;
		ent->r.client->ps.stats[STAT_CTF_ID_VIEW_COLOR] = 0;
	}
}

/*------------------------------------------------------------------------*/

/*QUAKED SP_team_CTF_redspawn (1 0 0) (-16 -16 -24) (16 16 32)
potential team1 spawning position for ctf games
*/
void SP_team_CTF_redspawn(edict_t *self)
{
}

/*QUAKED SP_team_CTF_bluespawn (0 0 1) (-16 -16 -24) (16 16 32)
potential team2 spawning position for ctf games
*/
void SP_team_CTF_bluespawn(edict_t *self)
{
}

/*QUAKED SP_team_CTF_redplayer (1 0 0) (-16 -16 -24) (16 16 32)
potential team1 spawning position for ctf games
*/
void SP_team_CTF_redplayer(edict_t *self)
{
}

/*QUAKED SP_team_CTF_blueplayer (0 0 1) (-16 -16 -24) (16 16 32)
potential team2 spawning position for ctf games
*/
void SP_team_CTF_blueplayer(edict_t *self)
{
}

/*------------------------------------------------------------------------*/
/* GRAPPLE																  */
/*------------------------------------------------------------------------*/

// ent is player
void CTFPlayerResetGrapple(edict_t *ent)
{
	if (ent->r.client && ent->r.client->ctf_grapple)
		CTFResetGrapple(ent->r.client->ctf_grapple);
}

// self is grapple, not player
void CTFResetGrapple(edict_t *self)
{
	if (self->r.owner->r.client->ctf_grapple) {
		float volume = 1.0;
		gclient_t *cl;

		if (self->r.owner->r.client->silencer_shots)
			volume = 0.2;

		G_Sound (self->r.owner, CHAN_WEAPON, trap_SoundIndex("sound/weapons/grapple/grreset.wav"), volume, ATTN_NORM);
		cl = self->r.owner->r.client;
		cl->ctf_grapple = NULL;
		cl->ctf_grapplereleasetime = level.time;
		cl->ctf_grapplestate = CTF_GRAPPLE_STATE_FLY; // we're firing, not on hook
		cl->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
		G_FreeEdict(self);
	}
}

void CTFGrappleTouch (edict_t *self, edict_t *other, cplane_t *plane, int surfFlags)
{
	float volume = 1.0;
	edict_t *event;

	if (other == self->r.owner)
		return;
	if (self->r.owner->r.client->ctf_grapplestate != CTF_GRAPPLE_STATE_FLY)
		return;

	if (surfFlags & SURF_NOIMPACT)
	{
		CTFResetGrapple(self);
		return;
	}

	VectorClear (self->velocity);

	PlayerNoise(self->r.owner, self->s.origin, PNOISE_IMPACT);

	if (other->takedamage) {
		T_Damage (other, self, self->r.owner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, 0, MOD_GRAPPLE);
		CTFResetGrapple(self);
		return;
	}

	self->r.owner->r.client->ctf_grapplestate = CTF_GRAPPLE_STATE_PULL; // we're on hook
	self->enemy = other;
	self->r.solid = SOLID_NOT;

	if (self->r.owner->r.client->silencer_shots)
		volume = 0.2;

	G_Sound (self->r.owner, CHAN_WEAPON, trap_SoundIndex("sound/weapons/grapple/grpull.wav"), volume, ATTN_NORM);
	G_Sound (self, CHAN_WEAPON, trap_SoundIndex("sound/weapons/grapple/grhit.wav"), volume, ATTN_NORM);

	event = G_SpawnEvent ( EV_SPARKS, DirToByte (plane ? plane->normal : NULL), self->s.origin );
	event->r.svflags = SVF_NOOLDORIGIN;
}

// draw beam between grapple and self
void CTFGrappleDrawCable(edict_t *self)
{
	edict_t	*event;
	vec3_t	start, f, r;
	vec3_t	dir, offset;
	float	distance;

	AngleVectors (self->r.owner->r.client->v_angle, f, r, NULL);
	VectorSet (offset, 24, 8, self->r.owner->viewheight-8+2);
	P_ProjectSource (self->r.owner->r.client, self->r.owner->s.origin, offset, f, r, start);
	VectorSubtract (start, self->r.owner->s.origin, offset);

	VectorSubtract (start, self->s.origin, dir);
	distance = VectorLength (dir);

	// don't draw cable if close
	if (distance < 64)
		return;

	event = G_SpawnEvent ( EV_GRAPPLE_CABLE, 0, self->s.origin );
	VectorCopy ( offset, event->s.origin2 );
	event->s.ownerNum = self->r.owner - game.edicts;
	event->r.svflags = SVF_FORCEOLDORIGIN;
}

void SV_AddGravity (edict_t *ent);

// pull the player toward the grapple
void CTFGrapplePull(edict_t *self)
{
	vec3_t hookdir, v;
	float vlen;

	if (strcmp(self->r.owner->r.client->pers.weapon->classname, "weapon_grapple") == 0 &&
		!self->r.owner->r.client->newweapon &&
		self->r.owner->r.client->weaponstate != WEAPON_FIRING &&
		self->r.owner->r.client->weaponstate != WEAPON_ACTIVATING) {
		CTFResetGrapple(self);
		return;
	}

	if (self->enemy) {
		if (self->enemy->r.solid == SOLID_NOT) {
			CTFResetGrapple(self);
			return;
		}
		if (self->enemy->r.solid == SOLID_BBOX) {
			VectorScale (self->enemy->r.size, 0.5, v);
			VectorAdd (v, self->enemy->s.origin, v);
			VectorAdd (v, self->enemy->r.mins, self->s.origin);
			trap_LinkEntity (self);
		} else
			VectorCopy(self->enemy->velocity, self->velocity);
		if (self->enemy->takedamage &&
			!CheckTeamDamage (self->enemy, self->r.owner)) {
			float volume = 1.0;

			if (self->r.owner->r.client->silencer_shots)
				volume = 0.2;

			T_Damage (self->enemy, self, self->r.owner, self->velocity, self->s.origin, vec3_origin, 1, 1, 0, MOD_GRAPPLE);
			G_Sound (self, CHAN_WEAPON, trap_SoundIndex("sound/weapons/grapple/grhurt.wav"), volume, ATTN_NORM);
		}
		if (self->enemy->deadflag) { // he died
			CTFResetGrapple(self);
			return;
		}
	}

	CTFGrappleDrawCable(self);

	if (self->r.owner->r.client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY) {
		// pull player toward grapple
		// this causes icky stuff with prediction, we need to extend
		// the prediction layer to include two new fields in the player
		// move stuff: a point and a velocity.  The client should add
		// that velociy in the direction of the point
		vec3_t forward, up;

		AngleVectors (self->r.owner->r.client->v_angle, forward, NULL, up);
		VectorCopy(self->r.owner->s.origin, v);
		v[2] += self->r.owner->viewheight;
		VectorSubtract (self->s.origin, v, hookdir);

		vlen = VectorLength(hookdir);

		if (self->r.owner->r.client->ctf_grapplestate == CTF_GRAPPLE_STATE_PULL &&
			vlen < 64) {
			float volume = 1.0;

			if (self->r.owner->r.client->silencer_shots)
				volume = 0.2;

			self->r.owner->r.client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
			G_Sound (self->r.owner, CHAN_WEAPON, trap_SoundIndex("sound/weapons/grapple/grhang.wav"), volume, ATTN_NORM);
			self->r.owner->r.client->ctf_grapplestate = CTF_GRAPPLE_STATE_HANG;
		}

		VectorNormalize (hookdir);
		VectorScale(hookdir, CTF_GRAPPLE_PULL_SPEED, hookdir);
		VectorCopy(hookdir, self->r.owner->velocity);
		SV_AddGravity(self->r.owner);
	}
}

void CTFFireGrapple (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed)
{
	edict_t	*grapple;
	trace_t	tr;

	VectorNormalize (dir);

	grapple = G_Spawn();
	VectorCopy (start, grapple->s.origin);
	VectorCopy (start, grapple->s.old_origin);
	VecToAngles (dir, grapple->s.angles);
	VectorScale (dir, speed, grapple->velocity);
	grapple->movetype = MOVETYPE_FLYMISSILE;
	grapple->r.clipmask = MASK_SHOT;
	grapple->r.solid = SOLID_BBOX;
	grapple->s.renderfx = RF_NOSHADOW;
	grapple->s.effects = 0;
	VectorClear (grapple->r.mins);
	VectorClear (grapple->r.maxs);
	grapple->s.modelindex = trap_ModelIndex ("models/weapons/grapple/hook/tris.md2");
//	grapple->s.sound = trap_SoundIndex ("sound/misc/lasfly.wav");
	grapple->r.owner = self;
	grapple->touch = CTFGrappleTouch;
//	grapple->nextthink = level.time + FRAMETIME;
//	grapple->think = CTFGrappleThink;
	grapple->dmg = damage;
	self->r.client->ctf_grapple = grapple;
	self->r.client->ctf_grapplestate = CTF_GRAPPLE_STATE_FLY; // we're firing, not on hook
	trap_LinkEntity (grapple);

	trap_Trace (&tr, self->s.origin, NULL, NULL, grapple->s.origin, grapple, MASK_SHOT);
	if (tr.fraction < 1.0)
	{
		VectorMA (grapple->s.origin, -10, dir, grapple->s.origin);
		grapple->touch (grapple, &game.edicts[tr.ent], NULL, 0);
	}
}	

void CTFWeapon_Grapple_Fire (edict_t *ent)
{
	int		damage = 10;
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	offset;
	float	volume = 1.0;

	if (ent->r.client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY)
		return; // it's already out

	AngleVectors (ent->r.client->v_angle, forward, right, NULL);
	VectorSet (offset, 24, 8, ent->viewheight-8+2);
	P_ProjectSource (ent->r.client, ent->s.origin, offset, forward, right, start);

	VectorScale (forward, -2, ent->r.client->kick_origin);
	ent->r.client->kick_angles[0] = -1;

	if (ent->r.client->silencer_shots)
		volume = 0.2;

	G_Sound (ent, CHAN_WEAPON, trap_SoundIndex("sound/weapons/grapple/grfire.wav"), volume, ATTN_NORM);
	CTFFireGrapple (ent, start, forward, damage, CTF_GRAPPLE_SPEED);

	PlayerNoise (ent, start, PNOISE_WEAPON);

	ent->r.client->ps.gunframe++;
}

void CTFWeapon_Grapple (edict_t *ent)
{
	static int	pause_frames[]	= {10, 18, 27, 0};
	static int	fire_frames[]	= {6, 0};
	int prevstate;

	// if the the attack button is still down, stay in the firing frame
	if ((ent->r.client->buttons & BUTTON_ATTACK) && 
		ent->r.client->weaponstate == WEAPON_FIRING &&
		ent->r.client->ctf_grapple)
		ent->r.client->ps.gunframe = 9;

	if (!(ent->r.client->buttons & BUTTON_ATTACK) && 
		ent->r.client->ctf_grapple) {
		CTFResetGrapple(ent->r.client->ctf_grapple);
		if (ent->r.client->weaponstate == WEAPON_FIRING)
			ent->r.client->weaponstate = WEAPON_READY;
	}

	if (ent->r.client->newweapon && 
		ent->r.client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY &&
		ent->r.client->weaponstate == WEAPON_FIRING) {
		// he wants to change weapons while grappled
		ent->r.client->weaponstate = WEAPON_DROPPING;
		ent->r.client->ps.gunframe = 32;
	}

	prevstate = ent->r.client->weaponstate;
	Weapon_Generic (ent, 5, 9, 31, 36, pause_frames, fire_frames, 
		CTFWeapon_Grapple_Fire);

	// if we just switched back to grapple, immediately go to fire frame
	if (prevstate == WEAPON_ACTIVATING &&
		ent->r.client->weaponstate == WEAPON_READY &&
		ent->r.client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY) {
		if (!(ent->r.client->buttons & BUTTON_ATTACK))
			ent->r.client->ps.gunframe = 9;
		else
			ent->r.client->ps.gunframe = 5;
		ent->r.client->weaponstate = WEAPON_FIRING;
	}
}

void CTFTeam_f (edict_t *ent)
{
	char *t, *s;
	int desired_team;

	t = trap_Cmd_Args ();
	if (!*t) {
		G_PrintMsg (ent, PRINT_HIGH, "You are on the %s team.\n",
			CTFTeamName(ent->r.client->resp.ctf_team));
		return;
	}

	if (ctfgame.match > MATCH_SETUP) {
		G_PrintMsg (ent, PRINT_HIGH, "Can't change teams in a match.\n");
		return;
	}

	if (Q_stricmp(t, "red") == 0)
		desired_team = CTF_TEAM1;
	else if (Q_stricmp(t, "blue") == 0)
		desired_team = CTF_TEAM2;
	else {
		G_PrintMsg (ent, PRINT_HIGH, "Unknown team %s.\n", t);
		return;
	}

	if (ent->r.client->resp.ctf_team == desired_team) {
		G_PrintMsg (ent, PRINT_HIGH, "You are already on the %s team.\n",
			CTFTeamName(ent->r.client->resp.ctf_team));
		return;
	}

////
	ent->r.svflags = 0;
	ent->flags &= ~FL_GODMODE;
	ent->r.client->resp.ctf_team = desired_team;
	ent->r.client->resp.ctf_state = 0;
	s = Info_ValueForKey (ent->r.client->pers.userinfo, "skin");
	CTFAssignSkin(ent, s);

	if (ent->r.solid == SOLID_NOT) { // spectator
		PutClientInServer (ent);
		G_AddEvent (ent, EV_TELEPORT, 0, qtrue);

		// add a teleportation effect

		// hold in place briefly
		ent->r.client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
		ent->r.client->ps.pmove.pm_time = 14;
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s joined the %s team.\n",
			ent->r.client->pers.netname, S_COLOR_WHITE, CTFTeamName(desired_team));
		return;
	}

	ent->health = 0;
	player_die (ent, ent, ent, 100000, vec3_origin);
	// don't even bother waiting for death frames
	ent->deadflag = DEAD_DEAD;
	respawn (ent);

	ent->r.client->resp.score = 0;

	G_PrintMsg (NULL, PRINT_HIGH, "%s%s changed to the %s team.\n",
		ent->r.client->pers.netname, S_COLOR_WHITE, CTFTeamName(desired_team));
}

/*
==================
CTFScoreboardMessage
==================
*/
char *CTFScoreboardMessage (edict_t *ent, edict_t *killer)
{
	char	entry[MAX_TOKEN_CHARS];
	static char	string[MAX_STRING_CHARS];
	int		len;
	int		i, j, k, n;
	int		sorted[2][MAX_CLIENTS];
	int		sortedscores[2][MAX_CLIENTS];
	int		score, total[2], totalscore[2];
	int		last[2];
	gclient_t	*cl;
	edict_t		*cl_ent;
	int team;
	int maxsize = 1000;

	// sort the clients by team and score
	total[0] = total[1] = 0;
	last[0] = last[1] = 0;
	totalscore[0] = totalscore[1] = 0;
	for (i=0 ; i<game.maxclients ; i++)
	{
		cl_ent = game.edicts + 1 + i;
		if (!cl_ent->r.inuse)
			continue;
		if (game.clients[i].resp.ctf_team == CTF_TEAM1)
			team = 0;
		else if (game.clients[i].resp.ctf_team == CTF_TEAM2)
			team = 1;
		else
			continue; // unknown team?

		score = game.clients[i].resp.score;
		for (j=0 ; j<total[team] ; j++)
		{
			if (score > sortedscores[team][j])
				break;
		}
		for (k=total[team] ; k>j ; k--)
		{
			sorted[team][k] = sorted[team][k-1];
			sortedscores[team][k] = sortedscores[team][k-1];
		}
		sorted[team][j] = i;
		sortedscores[team][j] = score;
		totalscore[team] += score;
		total[team]++;
	}

	// print level name and exit rules
	// add the clients in sorted order
	*string = 0;
	len = 0;

	// team one
	Q_snprintfz(string, sizeof(string), "size 32 32 "
		"if 23 xv 8 yv 8 23 endif "
		"xv 40 yv 28 string \"%4d/%-3d\" "
		"xv 98 yv 12 num 2 %17 "
		"if 24 xv 168 yv 8 pic %24 endif "
		"xv 200 yv 28 string \"%4d/%-3d\" "
		"xv 256 yv 12 num 2 %19 ",
		totalscore[0], total[0],
		totalscore[1], total[1]);
	len = strlen(string);

	for (i=0 ; i<16 ; i++)
	{
		if (i >= total[0] && i >= total[1])
			break; // we're done

		*entry = 0;

		// left side
		if (i < total[0]) {
			cl = &game.clients[sorted[0][i]];
			cl_ent = game.edicts + 1 + sorted[0][i];

			sprintf(entry+strlen(entry),
				"ctf 0 %d %d %d %d ",
				42 + i * 16,
				sorted[0][i],
				cl->resp.score,
				cl->r.ping > 999 ? 999 : cl->r.ping);

			if (cl_ent->r.client->pers.inventory[ITEM_INDEX(flag2_item)])
				sprintf(entry + strlen(entry), "xv 56 yv %d picn pics/sbfctf2 ",
					42 + i * 16);

			if (maxsize - len > strlen(entry)) {
				strcat(string, entry);
				len = strlen(string);
				last[0] = i;
			}
		}

		// right side
		if (i < total[1]) {
			cl = &game.clients[sorted[1][i]];
			cl_ent = game.edicts + 1 + sorted[1][i];

			sprintf(entry+strlen(entry),
				"ctf 160 %d %d %d %d ",
				42 + i * 16,
				sorted[1][i],
				cl->resp.score,
				cl->r.ping > 999 ? 999 : cl->r.ping);

			if (cl_ent->r.client->pers.inventory[ITEM_INDEX(flag1_item)])
				sprintf(entry + strlen(entry), "xv 216 yv %d picn pics/sbfctf1 ",
					42 + i * 16);

			if (maxsize - len > strlen(entry)) {
				strcat(string, entry);
				len = strlen(string);
				last[1] = i;
			}
		}
	}

	// put in spectators if we have enough room
	if (last[0] > last[1])
		j = last[0];
	else
		j = last[1];
	j = (j + 2) * 16 + 42;

	k = n = 0;
	if (maxsize - len > 50) {
		for (i = 0; i < game.maxclients; i++) {
			cl_ent = game.edicts + 1 + i;
			cl = &game.clients[i];
			if (!cl_ent->r.inuse ||
				cl_ent->r.solid != SOLID_NOT ||
				cl_ent->r.client->resp.ctf_team != CTF_NOTEAM)
				continue;

			if (!k) {
				k = 1;
				sprintf(entry, "xv 0 yv %d string \"%sSpectators\"%s ", j, S_COLOR_YELLOW, S_COLOR_WHITE);
				strcat(string, entry);
				len = strlen(string);
				j += 16;
			}

			sprintf(entry+strlen(entry),
				"ctf %d %d %d %d %d ",
				(n & 1) ? 160 : 0, // x
				j, // y
				i, // playernum
				cl->resp.score,
				cl->r.ping > 999 ? 999 : cl->r.ping);
			if (maxsize - len > strlen(entry)) {
				strcat(string, entry);
				len = strlen(string);
			}
			
			if (n & 1)
				j += 16;
			n++;
		}
	}

	if (total[0] - last[0] > 1) // couldn't fit everyone
		sprintf(string + strlen(string), "xv 8 yv %d string \"..and %d more\" ",
			42 + (last[0]+1)*8, total[0] - last[0] - 1);
	if (total[1] - last[1] > 1) // couldn't fit everyone
		sprintf(string + strlen(string), "xv 168 yv %d string \"..and %d more\" ",
			42 + (last[1]+1)*8, total[1] - last[1] - 1);

	return string;
}

/*------------------------------------------------------------------------*/
/* TECH																	  */
/*------------------------------------------------------------------------*/

void CTFHasTech(edict_t *who)
{
	if (level.time - who->r.client->ctf_lasttechmsg > 2) {
		G_CenterPrintMsg (who, "You already have a TECH powerup.");
		who->r.client->ctf_lasttechmsg = level.time;
	}
}

gitem_t *CTFWhat_Tech(edict_t *ent)
{
	gitem_t *tech;
	int i;

	i = 0;
	while (tnames[i]) {
		if ((tech = FindItemByClassname(tnames[i])) != NULL &&
			ent->r.client->pers.inventory[ITEM_INDEX(tech)]) {
			return tech;
		}
		i++;
	}
	return NULL;
}

qboolean CTFPickup_Tech (edict_t *ent, edict_t *other)
{
	gitem_t *tech;
	int i;

	i = 0;
	while (tnames[i]) {
		if ((tech = FindItemByClassname(tnames[i])) != NULL &&
			other->r.client->pers.inventory[ITEM_INDEX(tech)]) {
			CTFHasTech(other);
			return qfalse; // has this one
		}
		i++;
	}
	
	// client only gets one tech
	other->r.client->pers.inventory[ITEM_INDEX(ent->item)]++;
	other->r.client->ctf_regentime = level.time;
	return qtrue;
}

static void SpawnTech(gitem_t *item, edict_t *spot);

static edict_t *FindTechSpawn(void)
{
	edict_t *spot = NULL;
	int i = rand() % 16;

	while (i--)
		spot = G_Find (spot, FOFS(classname), "info_player_deathmatch");
	if (!spot)
		spot = G_Find (spot, FOFS(classname), "info_player_deathmatch");
	return spot;
}

static void TechThink(edict_t *tech)
{
	edict_t *spot;

	if ((spot = FindTechSpawn()) != NULL) {
		SpawnTech(tech->item, spot);
		G_FreeEdict(tech);
	} else {
		tech->nextthink = level.time + CTF_TECH_TIMEOUT;
		tech->think = TechThink;
	}
}

void CTFDrop_Tech(edict_t *ent, gitem_t *item)
{
	edict_t *tech;

	tech = Drop_Item(ent, item);
	tech->nextthink = level.time + CTF_TECH_TIMEOUT;
	tech->think = TechThink;
	ent->r.client->pers.inventory[ITEM_INDEX(item)] = 0;
}

edict_t *CTFDeadDropTech(edict_t *ent)
{
	gitem_t *tech;
	edict_t *dropped;
	int i;

	i = 0;
	while (tnames[i]) {
		if ((tech = FindItemByClassname(tnames[i])) != NULL &&
			ent->r.client->pers.inventory[ITEM_INDEX(tech)]) {
			dropped = Drop_Item(ent, tech);
			// hack the velocity to make it bounce random
			dropped->velocity[0] = (rand() % 600) - 300;
			dropped->velocity[1] = (rand() % 600) - 300;
			dropped->nextthink = level.time + CTF_TECH_TIMEOUT;
			dropped->think = TechThink;
			dropped->r.owner = NULL;
			ent->r.client->pers.inventory[ITEM_INDEX(tech)] = 0;
			return dropped;
		}
		i++;
	}

	return NULL;
}

static void SpawnTech(gitem_t *item, edict_t *spot)
{
	edict_t	*ent;
	vec3_t	forward, right;
	vec3_t  angles;

	ent = G_Spawn();
	ent->classname = item->classname;
	ent->item = item;
	ent->spawnflags = DROPPED_ITEM;
	ent->s.effects = item->world_model_flags;
	VectorSet (ent->r.mins, -15, -15, -15);
	VectorSet (ent->r.maxs, 15, 15, 15);

	if ( ent->item->world_model[0] )
		ent->s.modelindex = trap_ModelIndex (ent->item->world_model[0]);
	if ( ent->item->world_model[1] )
		ent->s.modelindex2 = trap_ModelIndex (ent->item->world_model[1]);
	if ( ent->item->world_model[2] )
		ent->s.modelindex3 = trap_ModelIndex (ent->item->world_model[2]);

	ent->r.solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;  
	ent->touch = Touch_Item;
	ent->r.owner = ent;

	angles[0] = 0;
	angles[1] = rand() % 360;
	angles[2] = 0;

	AngleVectors (angles, forward, right, NULL);
	VectorCopy (spot->s.origin, ent->s.origin);
	ent->s.origin[2] += 16;
	VectorScale (forward, 100, ent->velocity);
	ent->velocity[2] = 300;

	ent->nextthink = level.time + CTF_TECH_TIMEOUT;
	ent->think = TechThink;

	trap_LinkEntity (ent);
}

void CTFSpawnTechs(edict_t *ent)
{
	gitem_t *tech;
	edict_t *spot;
	int i;

	if( !ctf->integer )
		return;

	i = 0;
	while (tnames[i]) {
		if ((tech = FindItemByClassname(tnames[i])) != NULL &&
			(spot = FindTechSpawn()) != NULL)
			SpawnTech(tech, spot);
		i++;
	}
	if (ent)
		G_FreeEdict(ent);
}

// frees the passed edict!
void CTFRespawnTech(edict_t *ent)
{
	edict_t *spot;

	if ((spot = FindTechSpawn()) != NULL)
		SpawnTech(ent->item, spot);
	G_FreeEdict(ent);
}

void CTFSetupTechSpawn(void)
{
	edict_t *ent;

	if (dmflags->integer & DF_CTF_NO_TECH)
		return;

	ent = G_Spawn();
	ent->nextthink = level.time + 2;
	ent->think = CTFSpawnTechs;
}

void CTFResetTech(void)
{
	edict_t *ent;
	int i;

	for (ent = game.edicts + 1, i = 1; i < game.numentities; i++, ent++) {
		if (ent->r.inuse)
			if (ent->item && (ent->item->flags & IT_TECH))
				G_FreeEdict(ent);
	}
	CTFSpawnTechs(NULL);
}

int CTFApplyResistance(edict_t *ent, int dmg)
{
	static gitem_t *tech = NULL;
	float volume = 1.0;

	if (ent->r.client && ent->r.client->silencer_shots)
		volume = 0.2;

	if (!tech)
		tech = FindItemByClassname("item_tech1");
	if (dmg && tech && ent->r.client && ent->r.client->pers.inventory[ITEM_INDEX(tech)]) {
		// make noise
	   	G_Sound (ent, CHAN_VOICE, trap_SoundIndex("sound/ctf/tech1.wav"), volume, ATTN_NORM);
		return dmg / 2;
	}
	return dmg;
}

int CTFApplyStrength(edict_t *ent, int dmg)
{
	static gitem_t *tech = NULL;

	if (!tech)
		tech = FindItemByClassname("item_tech2");
	if (dmg && tech && ent->r.client && ent->r.client->pers.inventory[ITEM_INDEX(tech)]) {
		return dmg * 2;
	}
	return dmg;
}

qboolean CTFApplyStrengthSound(edict_t *ent)
{
	static gitem_t *tech = NULL;
	float volume = 1.0;

	if (ent->r.client && ent->r.client->silencer_shots)
		volume = 0.2;

	if (!tech)
		tech = FindItemByClassname("item_tech2");
	if (tech && ent->r.client &&
		ent->r.client->pers.inventory[ITEM_INDEX(tech)]) {
		if (ent->r.client->ctf_techsndtime < level.time) {
			ent->r.client->ctf_techsndtime = level.time + 1;
			if (ent->r.client->quad_framenum > level.framenum)
				G_Sound (ent, CHAN_VOICE, trap_SoundIndex("sound/ctf/tech2x.wav"), volume, ATTN_NORM);
			else
				G_Sound (ent, CHAN_VOICE, trap_SoundIndex("sound/ctf/tech2.wav"), volume, ATTN_NORM);
		}
		return qtrue;
	}
	return qfalse;
}


qboolean CTFApplyHaste(edict_t *ent)
{
	static gitem_t *tech = NULL;

	if (!tech)
		tech = FindItemByClassname("item_tech3");
	if (tech && ent->r.client &&
		ent->r.client->pers.inventory[ITEM_INDEX(tech)])
		return qtrue;
	return qfalse;
}

void CTFApplyHasteSound(edict_t *ent)
{
	static gitem_t *tech = NULL;
	float volume = 1.0;

	if (ent->r.client && ent->r.client->silencer_shots)
		volume = 0.2;

	if (!tech)
		tech = FindItemByClassname("item_tech3");
	if (tech && ent->r.client &&
		ent->r.client->pers.inventory[ITEM_INDEX(tech)] &&
		ent->r.client->ctf_techsndtime < level.time) {
		ent->r.client->ctf_techsndtime = level.time + 1;
		G_Sound (ent, CHAN_VOICE, trap_SoundIndex("sound/ctf/tech3.wav"), volume, ATTN_NORM);
	}
}

void CTFApplyRegeneration(edict_t *ent)
{
	static gitem_t *tech = NULL;
	qboolean noise = qfalse;
	gclient_t *client;
	int index;
	float volume = 1.0;

	client = ent->r.client;
	if (!client)
		return;

	if (client->silencer_shots)
		volume = 0.2;

	if (!tech)
		tech = FindItemByClassname("item_tech4");
	if (tech && client->pers.inventory[ITEM_INDEX(tech)]) {
		if (client->ctf_regentime < level.time) {
			client->ctf_regentime = level.time;
			if (ent->health < 150) {
				ent->health += 5;
				if (ent->health > 150)
					ent->health = 150;
				client->ctf_regentime += 0.5;
				noise = qtrue;
			}
			index = ArmorIndex (ent);
			if (index && client->pers.inventory[index] < 150) {
				client->pers.inventory[index] += 5;
				if (client->pers.inventory[index] > 150)
					client->pers.inventory[index] = 150;
				client->ctf_regentime += 0.5;
				noise = qtrue;
			}
		}
		if (noise && client->ctf_techsndtime < level.time) {
			client->ctf_techsndtime = level.time + 1;
			G_Sound (ent, CHAN_VOICE, trap_SoundIndex("sound/ctf/tech4.wav"), volume, ATTN_NORM);
		}
	}
}

qboolean CTFHasRegeneration(edict_t *ent)
{
	static gitem_t *tech = NULL;

	if (!tech)
		tech = FindItemByClassname("item_tech4");
	if (tech && ent->r.client &&
		ent->r.client->pers.inventory[ITEM_INDEX(tech)])
		return qtrue;
	return qfalse;
}

/*
======================================================================

SAY_TEAM

======================================================================
*/
static void CTFSay_Team_Location(edict_t *who, char *buf, int buflen)
{
	edict_t *what = NULL;
	edict_t *hot = NULL;
	float hotdist = 3.0f*8192.0f*8192.0f, newdist;
	vec3_t v;

	while ((what = G_Find(what, FOFS(classname), "target_location")) != NULL) {
		VectorSubtract( what->s.origin, who->s.origin, v );
		newdist = VectorLength( v );

		if ( newdist > hotdist ) {
			continue;
		}

		if ( !trap_inPVS ( what->s.origin, who->s.origin ) )
			continue;

		hot = what;
		hotdist = newdist;
	}

	if (!hot) {
		buf[0] = 0;
		return;
	}

	// we now have the closest target_location
	if ( hot->count ) {
		Q_snprintfz ( buf, buflen, "%c%c%s" S_COLOR_WHITE, Q_COLOR_ESCAPE, hot->count + '0', hot->message );
	} else {
		Q_snprintfz ( buf, buflen, "%s", hot->message );
	}
}

static void CTFSay_Team_Armor(edict_t *who, char *buf, int buflen)
{
	gitem_t		*item;
	int			index, cells;
	int			power_armor_type;

	*buf = 0;

	power_armor_type = PowerArmorType (who);
	if (power_armor_type)
	{
		cells = who->r.client->pers.inventory[ITEM_INDEX(FindItem ("cells"))];
		if (cells)
			sprintf(buf+strlen(buf), "%s with %i cells ",
				(power_armor_type == POWER_ARMOR_SCREEN) ?
				"Power Screen" : "Power Shield", cells);
	}

	index = ArmorIndex (who);
	if (index)
	{
		item = GetItemByIndex (index);
		if (item) {
			if (*buf)
				strcat(buf, "and ");
			sprintf(buf+strlen(buf), "%i units of %s",
				who->r.client->pers.inventory[index], item->pickup_name);
		}
	}

	if (!*buf)
		Q_snprintfz(buf, buflen, "no armor");
}

static void CTFSay_Team_Health(edict_t *who, char *buf, int buflen)
{
	if (who->health <= 0)
		Q_snprintfz (buf, buflen, "dead");
	else
		Q_snprintfz (buf, buflen, "%i health", who->health);
}

static void CTFSay_Team_Tech(edict_t *who, char *buf, int buflen)
{
	gitem_t *tech;
	int i;

	// see if the player has a tech powerup
	i = 0;
	while (tnames[i]) {
		if ((tech = FindItemByClassname(tnames[i])) != NULL &&
			who->r.client->pers.inventory[ITEM_INDEX(tech)]) {
			sprintf(buf, "the %s", tech->pickup_name);
			return;
		}
		i++;
	}
	Q_snprintfz(buf, buflen, "no powerup");
}

static void CTFSay_Team_Weapon(edict_t *who, char *buf, int buflen)
{
	if (who->r.client->pers.weapon)
		Q_snprintfz(buf, buflen, who->r.client->pers.weapon->pickup_name);
	else
		Q_snprintfz(buf, buflen, "none");
}

static void CTFSay_Team_Sight(edict_t *who, char *buf, int buflen)
{
	int i;
	edict_t *targ;
	int n = 0;
	char s[1024];
	char s2[1024];

	*s = *s2 = 0;
	for (i = 1; i <= game.maxclients; i++) {
		targ = game.edicts + i;
		if (!targ->r.inuse || 
			targ == who ||
			!loc_CanSee(targ, who))
			continue;
		if (*s2) {
			if (strlen(s) + strlen(s2) + 3 < sizeof(s)) {
				if (n)
					strcat(s, ", ");
				strcat(s, s2);
				*s2 = 0;
			}
			n++;
		}
		strcpy(s2, targ->r.client->pers.netname);
	}
	if (*s2) {
		if (strlen(s) + strlen(s2) + 6 < sizeof(s)) {
			if (n)
				strcat(s, " and ");
			strcat(s, s2);
		}
		Q_snprintfz(buf, buflen, s);
	} else
		Q_snprintfz(buf, buflen, "no one");
}

void CTFSay_Team(edict_t *who, char *msg)
{
	char outmsg[256];
	char buf[256];
	int i;
	char *p;
	edict_t *cl_ent;

	if (CheckFlood(who))
		return;

	outmsg[0] = 0;

	if (*msg == '\"') {
		msg[strlen(msg) - 1] = 0;
		msg++;
	}

	for (p = outmsg; *msg && (p - outmsg) < sizeof(outmsg) - 2; msg++) {
		if (*msg == '%') {
			switch (*++msg) {
				case 'l' :
				case 'L' :
					CTFSay_Team_Location(who, buf, sizeof(buf));
					if (strlen(buf) + (p - outmsg) < sizeof(outmsg) - 2) {
						if ( buf[0] ) {
							strcpy(p, buf);
							p += strlen(buf);
						}
					}
					break;
				case 'a' :
				case 'A' :
					CTFSay_Team_Armor(who, buf, sizeof(buf));
					if (strlen(buf) + (p - outmsg) < sizeof(outmsg) - 2) {
						strcpy(p, buf);
						p += strlen(buf);
					}
					break;
				case 'h' :
				case 'H' :
					CTFSay_Team_Health(who, buf, sizeof(buf));
					if (strlen(buf) + (p - outmsg) < sizeof(outmsg) - 2) {
						strcpy(p, buf);
						p += strlen(buf);
					}
					break;
				case 't' :
				case 'T' :
					CTFSay_Team_Tech(who, buf, sizeof(buf));
					if (strlen(buf) + (p - outmsg) < sizeof(outmsg) - 2) {
						strcpy(p, buf);
						p += strlen(buf);
					}
					break;
				case 'w' :
				case 'W' :
					CTFSay_Team_Weapon(who, buf, sizeof(buf));
					if (strlen(buf) + (p - outmsg) < sizeof(outmsg) - 2) {
						strcpy(p, buf);
						p += strlen(buf);
					}
					break;

				case 'n' :
				case 'N' :
					CTFSay_Team_Sight(who, buf, sizeof(buf));
					if (strlen(buf) + (p - outmsg) < sizeof(outmsg) - 2) {
						strcpy(p, buf);
						p += strlen(buf);
					}
					break;

				default :
					*p++ = *msg;
			}
		} else
			*p++ = *msg;
	}
	*p = 0;

	for (i = 0; i < game.maxclients; i++) {
		cl_ent = game.edicts + 1 + i;
		if (!cl_ent->r.inuse)
			continue;
		if (cl_ent->r.client->resp.ctf_team == who->r.client->resp.ctf_team)
			G_PrintMsg (cl_ent, PRINT_CHAT, "%s%s: %s\n", 
				who->r.client->pers.netname, S_COLOR_MAGENTA, outmsg);
	}
}

/*----------------------------------------------------------------------*/

static void SetLevelName(pmenu_t *p)
{
	static char levelname[33];

	levelname[0] = '*';

	if (game.edicts[0].message)
		Q_strncpyz (levelname+1, game.edicts[0].message, sizeof(levelname) - 1);
	else
		Q_strncpyz (levelname+1, level.mapname, sizeof(levelname) - 1);

	levelname[sizeof(levelname) - 1] = 0;

	p->text = levelname;
}


/*-----------------------------------------------------------------------*/


/* ELECTIONS */

qboolean CTFBeginElection(edict_t *ent, elect_t type, char *msg)
{
	int i;
	int count;
	edict_t *e;

	if (electpercentage->integer == 0) {
		G_PrintMsg (ent, PRINT_HIGH, "Elections are disabled, only an admin can process this action.\n");
		return qfalse;
	}


	if (ctfgame.election != ELECT_NONE) {
		G_PrintMsg (ent, PRINT_HIGH, "Election already in progress.\n");
		return qfalse;
	}

	// clear votes
	count = 0;
	for (i = 1; i <= game.maxclients; i++) {
		e = game.edicts + i;
		e->r.client->resp.voted = qfalse;
		if (e->r.inuse)
			count++;
	}

	if (count < 2) {
		G_PrintMsg (ent, PRINT_HIGH, "Not enough players for election.\n");
		return qfalse;
	}

	ctfgame.etarget = ent;
	ctfgame.election = type;
	ctfgame.evotes = 0;
	ctfgame.needvotes = (count * electpercentage->value) / 100;
	ctfgame.electtime = level.time + 20; // twenty seconds for election
	Q_strncpyz (ctfgame.emsg, msg, sizeof(ctfgame.emsg));

	// tell everyone
	G_PrintMsg (NULL, PRINT_CHAT, "%s\n", ctfgame.emsg);
	G_PrintMsg (NULL, PRINT_HIGH, "Type YES or NO to vote on this request.\n");
	G_PrintMsg (NULL, PRINT_HIGH, "Votes: %d  Needed: %d  Time left: %ds\n", ctfgame.evotes, ctfgame.needvotes,
		(int)(ctfgame.electtime - level.time));

	return qtrue;
}

void DoRespawn (edict_t *ent);

void CTFResetAllPlayers(void)
{
	int i;
	edict_t *ent;

	for (i = 1; i <= game.maxclients; i++) {
		ent = game.edicts + i;
		if (!ent->r.inuse)
			continue;

		if (ent->r.client->menu)
			PMenu_Close(ent);

		CTFPlayerResetGrapple(ent);
		CTFDeadDropFlag(ent);
		CTFDeadDropTech(ent);

		ent->r.client->resp.ctf_team = CTF_NOTEAM;
		ent->r.client->resp.ready = qfalse;

		ent->r.svflags = 0;
		ent->flags &= ~FL_GODMODE;
		PutClientInServer(ent);
	}

	// reset the level
	CTFResetTech();
	CTFResetFlags();

	for (ent = game.edicts + 1, i = 1; i < game.numentities; i++, ent++) {
		if (ent->r.inuse && !ent->r.client) {
			if (ent->r.solid == SOLID_NOT && ent->think == DoRespawn &&
				ent->nextthink >= level.time) {
				ent->nextthink = 0;
				DoRespawn(ent);
			}
		}
	}
	if (ctfgame.match == MATCH_SETUP)
		ctfgame.matchtime = level.time + matchsetuptime->value * 60;
}

void CTFAssignGhost(edict_t *ent)
{
	int ghost, i;

	for (ghost = 0; ghost < MAX_CLIENTS; ghost++)
		if (!ctfgame.ghosts[ghost].code)
			break;
	if (ghost == MAX_CLIENTS)
		return;
	ctfgame.ghosts[ghost].team = ent->r.client->resp.ctf_team;
	ctfgame.ghosts[ghost].score = 0;
	for (;;) {
		ctfgame.ghosts[ghost].code = 10000 + (rand() % 90000);
		for (i = 0; i < MAX_CLIENTS; i++)
			if (i != ghost && ctfgame.ghosts[i].code == ctfgame.ghosts[ghost].code)
				break;
		if (i == MAX_CLIENTS)
			break;
	}
	ctfgame.ghosts[ghost].ent = ent;
	strcpy(ctfgame.ghosts[ghost].netname, ent->r.client->pers.netname);
	ent->r.client->resp.ghost = ctfgame.ghosts + ghost;
	G_PrintMsg (ent, PRINT_CHAT, "Your ghost code is **** %d ****\n", ctfgame.ghosts[ghost].code);
	G_PrintMsg (ent, PRINT_HIGH, "If you lose connection, you can rejoin with your score "
		"intact by typing \"ghost %d\".\n", ctfgame.ghosts[ghost].code);
}

// start a match
void CTFStartMatch(void)
{
	int i;
	edict_t *ent;

	ctfgame.match = MATCH_GAME;
	ctfgame.matchtime = level.time + matchtime->value * 60;
	ctfgame.countdown = qfalse;

	ctfgame.team1 = ctfgame.team2 = 0;

	memset(ctfgame.ghosts, 0, sizeof(ctfgame.ghosts));

	for (i = 1; i <= game.maxclients; i++) {
		ent = game.edicts + i;
		if (!ent->r.inuse)
			continue;

		ent->r.client->resp.score = 0;
		ent->r.client->resp.ctf_state = 0;
		ent->r.client->resp.ghost = NULL;

		G_CenterPrintMsg (ent, "******************\n\nMATCH HAS STARTED!\n\n******************");

		if (ent->r.client->resp.ctf_team != CTF_NOTEAM) {
			// make up a ghost code
			CTFAssignGhost(ent);
			CTFPlayerResetGrapple(ent);
			ent->r.svflags = SVF_NOCLIENT;
			ent->flags &= ~FL_GODMODE;

			ent->r.client->respawn_time = level.time + 1.0 + ((rand()%30)/10.0);
			ent->r.client->ps.pmove.pm_type = PM_DEAD;
			ent->r.client->anim_priority = ANIM_DEATH;
			ent->s.frame = FRAME_death308-1;
			ent->r.client->anim_end = FRAME_death308;
			ent->deadflag = DEAD_DEAD;
			ent->movetype = MOVETYPE_NOCLIP;
			ent->r.client->ps.gunindex = 0;
			trap_LinkEntity (ent);
		}
	}
}

void CTFEndMatch(void)
{
	ctfgame.match = MATCH_POST;
	G_PrintMsg (NULL, PRINT_CHAT, "MATCH COMPLETED!\n");

	CTFCalcScores();

	G_PrintMsg (NULL, PRINT_HIGH, "RED TEAM:  %d captures, %d points\n",
		ctfgame.team1, ctfgame.total1);
	G_PrintMsg (NULL, PRINT_HIGH, "BLUE TEAM:  %d captures, %d points\n",
		ctfgame.team2, ctfgame.total2);

	if (ctfgame.team1 > ctfgame.team2)
		G_PrintMsg (NULL, PRINT_CHAT, "RED team won over the BLUE team by %d CAPTURES!\n",
			ctfgame.team1 - ctfgame.team2);
	else if (ctfgame.team2 > ctfgame.team1)
		G_PrintMsg (NULL, PRINT_CHAT, "BLUE team won over the RED team by %d CAPTURES!\n",
			ctfgame.team2 - ctfgame.team1);
	else if (ctfgame.total1 > ctfgame.total2) // frag tie breaker
		G_PrintMsg (NULL, PRINT_CHAT, "RED team won over the BLUE team by %d POINTS!\n",
			ctfgame.total1 - ctfgame.total2);
	else if (ctfgame.total2 > ctfgame.total1) 
		G_PrintMsg (NULL, PRINT_CHAT, "BLUE team won over the RED team by %d POINTS!\n",
			ctfgame.total2 - ctfgame.total1);
	else
		G_PrintMsg (NULL, PRINT_CHAT, "TIE GAME!\n");

	G_EndDMLevel();
}

qboolean CTFNextMap(void)
{
	if (ctfgame.match == MATCH_POST) {
		ctfgame.match = MATCH_SETUP;
		CTFResetAllPlayers();
		return qtrue;
	}
	return qfalse;
}

void CTFWinElection(void)
{
	switch (ctfgame.election) {
	case ELECT_MATCH :
		// reset into match mode
		if (competition->integer < 3)
			trap_Cvar_Set("competition", "2");
		ctfgame.match = MATCH_SETUP;
		CTFResetAllPlayers();
		break;

	case ELECT_ADMIN :
		ctfgame.etarget->r.client->resp.admin = qtrue;
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s has become an admin.\n", ctfgame.etarget->r.client->pers.netname, S_COLOR_WHITE);
		G_PrintMsg (ctfgame.etarget, PRINT_HIGH, "Type 'admin' to access the adminstration menu.\n");
		break;

	case ELECT_MAP :
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s is warping to level %s.\n", 
			ctfgame.etarget->r.client->pers.netname, S_COLOR_WHITE, ctfgame.elevel);
		Q_strncpyz (level.forcemap, ctfgame.elevel, sizeof(level.forcemap));
		G_EndDMLevel();
		break;
	}
	ctfgame.election = ELECT_NONE;
}

void CTFVoteYes(edict_t *ent)
{
	if (ctfgame.election == ELECT_NONE) {
		G_PrintMsg (ent, PRINT_HIGH, "No election is in progress.\n");
		return;
	}
	if (ent->r.client->resp.voted) {
		G_PrintMsg (ent, PRINT_HIGH, "You already voted.\n");
		return;
	}
	if (ctfgame.etarget == ent) {
		G_PrintMsg (ent, PRINT_HIGH, "You can't vote for yourself.\n");
		return;
	}

	ent->r.client->resp.voted = qtrue;

	ctfgame.evotes++;
	if (ctfgame.evotes == ctfgame.needvotes) {
		// the election has been won
		CTFWinElection();
		return;
	}

	G_PrintMsg (NULL, PRINT_HIGH, "%s\n", ctfgame.emsg);
	G_PrintMsg (NULL, PRINT_CHAT, "Votes: %d  Needed: %d  Time left: %ds\n", ctfgame.evotes, ctfgame.needvotes,
		(int)(ctfgame.electtime - level.time));
}

void CTFVoteNo(edict_t *ent)
{
	if (ctfgame.election == ELECT_NONE) {
		G_PrintMsg (ent, PRINT_HIGH, "No election is in progress.\n");
		return;
	}
	if (ent->r.client->resp.voted) {
		G_PrintMsg (ent, PRINT_HIGH, "You already voted.\n");
		return;
	}
	if (ctfgame.etarget == ent) {
		G_PrintMsg (ent, PRINT_HIGH, "You can't vote for yourself.\n");
		return;
	}

	ent->r.client->resp.voted = qtrue;

	G_PrintMsg (NULL, PRINT_HIGH, "%s\n", ctfgame.emsg);
	G_PrintMsg (NULL, PRINT_CHAT, "Votes: %d  Needed: %d  Time left: %ds\n", ctfgame.evotes, ctfgame.needvotes,
		(int)(ctfgame.electtime - level.time));
}

void CTFReady(edict_t *ent)
{
	int i, j;
	edict_t *e;
	int t1, t2;

	if (ent->r.client->resp.ctf_team == CTF_NOTEAM) {
		G_PrintMsg (ent, PRINT_HIGH, "Pick a team first (hit <TAB> for menu)\n");
		return;
	}

	if (ctfgame.match != MATCH_SETUP) {
		G_PrintMsg (ent, PRINT_HIGH, "A match is not being setup.\n");
		return;
	}

	if (ent->r.client->resp.ready) {
		G_PrintMsg (ent, PRINT_HIGH, "You have already commited.\n");
		return;
	}

	ent->r.client->resp.ready = qtrue;
	G_PrintMsg (NULL, PRINT_HIGH, "%s%s is ready.\n", ent->r.client->pers.netname, S_COLOR_WHITE);

	t1 = t2 = 0;
	for (j = 0, i = 1; i <= game.maxclients; i++) {
		e = game.edicts + i;
		if (!e->r.inuse)
			continue;
		if (e->r.client->resp.ctf_team != CTF_NOTEAM && !e->r.client->resp.ready)
			j++;
		if (e->r.client->resp.ctf_team == CTF_TEAM1)
			t1++;
		else if (e->r.client->resp.ctf_team == CTF_TEAM2)
			t2++;
	}
	if (!j && t1 && t2) {
		// everyone has commited
		G_PrintMsg (NULL, PRINT_CHAT, "All players have commited.  Match starting\n");
		ctfgame.match = MATCH_PREGAME;
		ctfgame.matchtime = level.time + matchstarttime->value;
		ctfgame.countdown = qfalse;
		G_GlobalSound (CHAN_AUTO, trap_SoundIndex("sound/misc/talk1.wav"));
	}
}

void CTFNotReady(edict_t *ent)
{
	if (ent->r.client->resp.ctf_team == CTF_NOTEAM) {
		G_PrintMsg (ent, PRINT_HIGH, "Pick a team first (hit <TAB> for menu)\n");
		return;
	}

	if (ctfgame.match != MATCH_SETUP && ctfgame.match != MATCH_PREGAME) {
		G_PrintMsg (ent, PRINT_HIGH, "A match is not being setup.\n");
		return;
	}

	if (!ent->r.client->resp.ready) {
		G_PrintMsg (ent, PRINT_HIGH, "You haven't commited.\n");
		return;
	}

	ent->r.client->resp.ready = qfalse;
	G_PrintMsg (NULL, PRINT_HIGH, "%s%s is no longer ready.\n", ent->r.client->pers.netname, S_COLOR_WHITE);

	if (ctfgame.match == MATCH_PREGAME) {
		G_PrintMsg (NULL, PRINT_CHAT, "Match halted.\n");
		ctfgame.match = MATCH_SETUP;
		ctfgame.matchtime = level.time + matchsetuptime->value * 60;
	}
}

void CTFGhost(edict_t *ent)
{
	int i;
	int n;

	if (trap_Cmd_Argc () < 2) {
		G_PrintMsg (ent, PRINT_HIGH, "Usage:  ghost <code>\n");
		return;
	}

	if (ent->r.client->resp.ctf_team != CTF_NOTEAM) {
		G_PrintMsg (ent, PRINT_HIGH, "You are already in the game.\n");
		return;
	}
	if (ctfgame.match != MATCH_GAME) {
		G_PrintMsg (ent, PRINT_HIGH, "No match is in progress.\n");
		return;
	}

	n = atoi(trap_Cmd_Argv(1));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (ctfgame.ghosts[i].code && ctfgame.ghosts[i].code == n) {
			G_PrintMsg (ent, PRINT_HIGH, "Ghost code accepted, your position has been reinstated.\n");
			ctfgame.ghosts[i].ent->r.client->resp.ghost = NULL;
			ent->r.client->resp.ctf_team = ctfgame.ghosts[i].team;
			ent->r.client->resp.ghost = ctfgame.ghosts + i;
			ent->r.client->resp.score = ctfgame.ghosts[i].score;
			ent->r.client->resp.ctf_state = 0;
			ctfgame.ghosts[i].ent = ent;
			ent->r.svflags = 0;
			ent->flags &= ~FL_GODMODE;
			PutClientInServer(ent);
			G_PrintMsg (NULL, PRINT_HIGH, "%s%s has been reinstated to %s team.\n",
				ent->r.client->pers.netname, S_COLOR_WHITE, CTFTeamName(ent->r.client->resp.ctf_team));
			return;
		}
	}
	G_PrintMsg (ent, PRINT_HIGH, "Invalid ghost code.\n");
}

qboolean CTFMatchSetup(void)
{
	if (ctfgame.match == MATCH_SETUP || ctfgame.match == MATCH_PREGAME)
		return qtrue;
	return qfalse;
}

qboolean CTFMatchOn(void)
{
	if (ctfgame.match == MATCH_GAME)
		return qtrue;
	return qfalse;
}


/*-----------------------------------------------------------------------*/

void CTFJoinTeam1(edict_t *ent, pmenuhnd_t *p);
void CTFJoinTeam2(edict_t *ent, pmenuhnd_t *p);
void CTFCredits(edict_t *ent, pmenuhnd_t *p);
void CTFReturnToMain(edict_t *ent, pmenuhnd_t *p);
void CTFChaseCam(edict_t *ent, pmenuhnd_t *p);

pmenu_t creditsmenu[] = {
	{ "*" GAMENAME,						PMENU_ALIGN_CENTER, NULL },
	{ "*ThreeWave Capture the Flag",	PMENU_ALIGN_CENTER, NULL },
	{ NULL,								PMENU_ALIGN_CENTER, NULL },
	{ "*Programming",					PMENU_ALIGN_CENTER, NULL }, 
	{ "Dave 'Zoid' Kirsch",				PMENU_ALIGN_CENTER, NULL },
	{ "*Level Design", 					PMENU_ALIGN_CENTER, NULL },
	{ "Christian Antkow",				PMENU_ALIGN_CENTER, NULL },
	{ "Tim Willits",					PMENU_ALIGN_CENTER, NULL },
	{ "Dave 'Zoid' Kirsch",				PMENU_ALIGN_CENTER, NULL },
	{ "*Art",							PMENU_ALIGN_CENTER, NULL },
	{ "Adrian Carmack Paul Steed",		PMENU_ALIGN_CENTER, NULL },
	{ "Kevin Cloud",					PMENU_ALIGN_CENTER, NULL },
	{ "*Sound",							PMENU_ALIGN_CENTER, NULL },
	{ "Tom 'Bjorn' Klok",				PMENU_ALIGN_CENTER, NULL },
	{ "*Original CTF Art Design",		PMENU_ALIGN_CENTER, NULL },
	{ "Brian 'Whaleboy' Cozzens",		PMENU_ALIGN_CENTER, NULL },
	{ NULL,								PMENU_ALIGN_CENTER, NULL },
	{ "Return to Main Menu",			PMENU_ALIGN_LEFT, CTFReturnToMain }
};

static const int jmenu_level = 2;
static const int jmenu_match = 3;
static const int jmenu_red = 5;
static const int jmenu_blue = 7;
static const int jmenu_chase = 9;
static const int jmenu_reqmatch = 11;

pmenu_t joinmenu[] = {
	{ "*" GAMENAME,			PMENU_ALIGN_CENTER, NULL },
	{ "*ThreeWave Capture the Flag",	PMENU_ALIGN_CENTER, NULL },
	{ NULL,					PMENU_ALIGN_CENTER, NULL },
	{ NULL,					PMENU_ALIGN_CENTER, NULL },
	{ NULL,					PMENU_ALIGN_CENTER, NULL },
	{ "Join Red Team",		PMENU_ALIGN_LEFT, CTFJoinTeam1 },
	{ NULL,					PMENU_ALIGN_LEFT, NULL },
	{ "Join Blue Team",		PMENU_ALIGN_LEFT, CTFJoinTeam2 },
	{ NULL,					PMENU_ALIGN_LEFT, NULL },
	{ "Chase Camera",		PMENU_ALIGN_LEFT, CTFChaseCam },
	{ "Credits",			PMENU_ALIGN_LEFT, CTFCredits },
	{ NULL,					PMENU_ALIGN_LEFT, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL },
	{ "Use [ and ] to move cursor",	PMENU_ALIGN_LEFT, NULL },
	{ "ENTER to select",	PMENU_ALIGN_LEFT, NULL },
	{ "ESC to Exit Menu",	PMENU_ALIGN_LEFT, NULL },
	{ "(TAB to Return)",	PMENU_ALIGN_LEFT, NULL },
	{ "v" CTF_STRING_VERSION,	PMENU_ALIGN_RIGHT, NULL },
};

pmenu_t nochasemenu[] = {
	{ "*" GAMENAME,			PMENU_ALIGN_CENTER, NULL },
	{ "*ThreeWave Capture the Flag",	PMENU_ALIGN_CENTER, NULL },
	{ NULL,					PMENU_ALIGN_CENTER, NULL },
	{ NULL,					PMENU_ALIGN_CENTER, NULL },
	{ "No one to chase",	PMENU_ALIGN_LEFT, NULL },
	{ NULL,					PMENU_ALIGN_CENTER, NULL },
	{ "Return to Main Menu", PMENU_ALIGN_LEFT, CTFReturnToMain }
};

void CTFJoinTeam(edict_t *ent, int desired_team)
{
	char *s;
	edict_t *event;

	PMenu_Close(ent);

	ent->r.svflags &= ~SVF_NOCLIENT;
	ent->r.client->resp.ctf_team = desired_team;
	ent->r.client->resp.ctf_state = 0;
	s = Info_ValueForKey (ent->r.client->pers.userinfo, "skin");
	CTFAssignSkin(ent, s);

	// assign a ghost if we are in match mode
	if (ctfgame.match == MATCH_GAME) {
		if (ent->r.client->resp.ghost)
			ent->r.client->resp.ghost->code = 0;
		ent->r.client->resp.ghost = NULL;
		CTFAssignGhost(ent);
	}

	PutClientInServer (ent);
	G_AddEvent (ent, EV_TELEPORT, 0, qtrue);

	// add a teleportation effect
	event = G_SpawnEvent ( EV_PLAYER_TELEPORT_IN, 0, ent->s.origin );
	event->r.svflags = SVF_NOOLDORIGIN;
	event->s.ownerNum = ent - game.edicts;

	// hold in place briefly
	ent->r.client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
	ent->r.client->ps.pmove.pm_time = 14;
	G_PrintMsg (NULL, PRINT_HIGH, "%s%s joined the %s team.\n",
		ent->r.client->pers.netname, S_COLOR_WHITE, CTFTeamName(desired_team));

	if (ctfgame.match == MATCH_SETUP) {
		G_CenterPrintMsg (ent,	"***********************\n"
								"Type \"ready\" in console\n"
								"to ready up.\n"
								"***********************");
	}
}

void CTFJoinTeam1(edict_t *ent, pmenuhnd_t *p)
{
	CTFJoinTeam(ent, CTF_TEAM1);
}

void CTFJoinTeam2(edict_t *ent, pmenuhnd_t *p)
{
	CTFJoinTeam(ent, CTF_TEAM2);
}

void CTFChaseCam(edict_t *ent, pmenuhnd_t *p)
{
	int i;
	edict_t *e;

	if (ent->r.client->chase_target) {
		ent->r.client->chase_target = NULL;
		ent->r.client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
		PMenu_Close(ent);
		return;
	}

	for (i = 1; i <= game.maxclients; i++) {
		e = game.edicts + i;
		if (e->r.inuse && e->r.solid != SOLID_NOT) {
			ent->r.client->chase_target = e;
			PMenu_Close(ent);
			ent->r.client->update_chase = qtrue;
			return;
		}
	}

	SetLevelName(nochasemenu + jmenu_level);

	PMenu_Close(ent);
	PMenu_Open(ent, nochasemenu, -1, sizeof(nochasemenu) / sizeof(pmenu_t), NULL);
}

void CTFReturnToMain(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Close(ent);
	CTFOpenJoinMenu(ent);
}

void CTFRequestMatch(edict_t *ent, pmenuhnd_t *p)
{
	char text[1024];

	PMenu_Close(ent);

	sprintf(text, "%s%s has requested to switch to competition mode.",
		ent->r.client->pers.netname, S_COLOR_WHITE);
	CTFBeginElection(ent, ELECT_MATCH, text);
}

void DeathmatchScoreboard (edict_t *ent);

void CTFShowScores(edict_t *ent, pmenu_t *p)
{
	PMenu_Close(ent);

	ent->r.client->showscores = qtrue;
	ent->r.client->showinventory = qfalse;
	DeathmatchScoreboard (ent);
}

int CTFUpdateJoinMenu(edict_t *ent)
{
	static char team1players[32];
	static char team2players[32];
	int num1, num2, i;

	if (ctfgame.match >= MATCH_PREGAME && matchlock->value) {
		joinmenu[jmenu_red].text = "MATCH IS LOCKED";
		joinmenu[jmenu_red].SelectFunc = NULL;
		joinmenu[jmenu_blue].text = "  (entry is not permitted)";
		joinmenu[jmenu_blue].SelectFunc = NULL;
	} else {
		if (ctfgame.match >= MATCH_PREGAME) {
			joinmenu[jmenu_red].text = "Join Red MATCH Team";
			joinmenu[jmenu_blue].text = "Join Blue MATCH Team";
		} else {
			joinmenu[jmenu_red].text = "Join Red Team";
			joinmenu[jmenu_blue].text = "Join Blue Team";
		}
		joinmenu[jmenu_red].SelectFunc = CTFJoinTeam1;
		joinmenu[jmenu_blue].SelectFunc = CTFJoinTeam2;
	}

	if (ctf_forcejoin->string && *ctf_forcejoin->string) {
		if (Q_stricmp(ctf_forcejoin->string, "red") == 0) {
			joinmenu[jmenu_blue].text = NULL;
			joinmenu[jmenu_blue].SelectFunc = NULL;
		} else if (Q_stricmp(ctf_forcejoin->string, "blue") == 0) {
			joinmenu[jmenu_red].text = NULL;
			joinmenu[jmenu_red].SelectFunc = NULL;
		}
	}

	if (ent->r.client->chase_target)
		joinmenu[jmenu_chase].text = "Leave Chase Camera";
	else
		joinmenu[jmenu_chase].text = "Chase Camera";

	SetLevelName(joinmenu + jmenu_level);

	num1 = num2 = 0;
	for (i = 0; i < game.maxclients; i++) {
		if (!game.edicts[i+1].r.inuse)
			continue;
		if (game.clients[i].resp.ctf_team == CTF_TEAM1)
			num1++;
		else if (game.clients[i].resp.ctf_team == CTF_TEAM2)
			num2++;
	}

	sprintf(team1players, "  (%d players)", num1);
	sprintf(team2players, "  (%d players)", num2);

	switch (ctfgame.match) {
	case MATCH_NONE :
		joinmenu[jmenu_match].text = NULL;
		break;

	case MATCH_SETUP :
		joinmenu[jmenu_match].text = "*MATCH SETUP IN PROGRESS";
		break;

	case MATCH_PREGAME :
		joinmenu[jmenu_match].text = "*MATCH STARTING";
		break;

	case MATCH_GAME :
		joinmenu[jmenu_match].text = "*MATCH IN PROGRESS";
		break;
	}

	if (joinmenu[jmenu_red].text)
		joinmenu[jmenu_red+1].text = team1players;
	else
		joinmenu[jmenu_red+1].text = NULL;
	if (joinmenu[jmenu_blue].text)
		joinmenu[jmenu_blue+1].text = team2players;
	else
		joinmenu[jmenu_blue+1].text = NULL;

	joinmenu[jmenu_reqmatch].text = NULL;
	joinmenu[jmenu_reqmatch].SelectFunc = NULL;
	if (competition->integer && ctfgame.match < MATCH_SETUP) {
		joinmenu[jmenu_reqmatch].text = "Request Match";
		joinmenu[jmenu_reqmatch].SelectFunc = CTFRequestMatch;
	}
	
	if (num1 > num2)
		return CTF_TEAM1;
	else if (num2 > num1)
		return CTF_TEAM2;
	return (rand() & 1) ? CTF_TEAM1 : CTF_TEAM2;
}

void CTFOpenJoinMenu(edict_t *ent)
{
	int team;

	team = CTFUpdateJoinMenu(ent);
	if (ent->r.client->chase_target)
		team = 8;
	else if (team == CTF_TEAM1)
		team = 4;
	else
		team = 6;
	PMenu_Open(ent, joinmenu, team, sizeof(joinmenu) / sizeof(pmenu_t), NULL);
}

void CTFCredits(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Close(ent);
	PMenu_Open(ent, creditsmenu, -1, sizeof(creditsmenu) / sizeof(pmenu_t), NULL);
}

qboolean CTFStartClient(edict_t *ent)
{
	if (ent->r.client->resp.ctf_team != CTF_NOTEAM)
		return qfalse;

	if (!(dmflags->integer & DF_CTF_FORCEJOIN) || ctfgame.match >= MATCH_SETUP) {
		// start as 'observer'
		ent->movetype = MOVETYPE_NOCLIP;
		ent->r.solid = SOLID_NOT;
		ent->r.svflags |= SVF_NOCLIENT;
		ent->r.client->resp.ctf_team = CTF_NOTEAM;
		ent->r.client->ps.gunindex = 0;
		trap_LinkEntity (ent);

		CTFOpenJoinMenu(ent);
		return qtrue;
	}
	return qfalse;
}

void CTFObserver(edict_t *ent)
{
	char		userinfo[MAX_INFO_STRING];

	// start as 'observer'
	if (ent->movetype == MOVETYPE_NOCLIP)

	CTFPlayerResetGrapple(ent);
	CTFDeadDropFlag(ent);
	CTFDeadDropTech(ent);

	ent->deadflag = DEAD_NO;
	ent->movetype = MOVETYPE_NOCLIP;
	ent->r.solid = SOLID_NOT;
	ent->r.svflags |= SVF_NOCLIENT;
	ent->r.client->resp.ctf_team = CTF_NOTEAM;
	ent->r.client->ps.gunindex = 0;
	ent->r.client->resp.score = 0;
	memcpy (userinfo, ent->r.client->pers.userinfo, sizeof(userinfo));
	InitClientPersistant(ent->r.client);
	ClientUserinfoChanged (ent, userinfo);
	trap_LinkEntity (ent);
	CTFOpenJoinMenu(ent);
}

qboolean CTFInMatch(void)
{
	if (ctfgame.match > MATCH_NONE)
		return qtrue;
	return qfalse;
}

qboolean CTFCheckRules(void)
{
	int t;
	int i, j;
	char text[64];
	edict_t *ent;

	if (ctfgame.election != ELECT_NONE && ctfgame.electtime <= level.time) {
		G_PrintMsg (NULL, PRINT_CHAT, "Election timed out and has been cancelled.\n");
		ctfgame.election = ELECT_NONE;
	}

	if (ctfgame.match != MATCH_NONE) {
		t = ctfgame.matchtime - level.time;

		// no team warnings in match mode
		ctfgame.warnactive = 0;

		if (t <= 0) { // time ended on something
			switch (ctfgame.match) {
			case MATCH_SETUP :
				// go back to normal mode
				if (competition->integer < 3) {
					ctfgame.match = MATCH_NONE;
					trap_Cvar_Set("competition", "1");
					CTFResetAllPlayers();
				} else {
					// reset the time
					ctfgame.matchtime = level.time + matchsetuptime->value * 60;
				}
				return qfalse;

			case MATCH_PREGAME :
				// match started!
				CTFStartMatch();
				G_GlobalSound (CHAN_AUTO, trap_SoundIndex("sound/misc/tele_up.wav"));
				return qfalse;

			case MATCH_GAME :
				// match ended!
				CTFEndMatch();
				G_GlobalSound (CHAN_AUTO, trap_SoundIndex("sound/misc/bigtele.wav"));
				return qfalse;
			}
		}

		if (t == ctfgame.lasttime)
			return qfalse;

		ctfgame.lasttime = t;

		switch (ctfgame.match) {
		case MATCH_SETUP :
			for (j = 0, i = 1; i <= game.maxclients; i++) {
				ent = game.edicts + i;
				if (!ent->r.inuse)
					continue;
				if (ent->r.client->resp.ctf_team != CTF_NOTEAM &&
					!ent->r.client->resp.ready)
					j++;
			}

			if (competition->integer < 3)
				sprintf(text, "%02d:%02d SETUP: %d not ready",
					t / 60, t % 60, j);
			else
				sprintf(text, "SETUP: %d not ready", j);

			trap_ConfigString (CONFIG_CTF_MATCH, text);
			break;

		case MATCH_PREGAME :
			sprintf(text, "%02d:%02d UNTIL START",
				t / 60, t % 60);
			trap_ConfigString (CONFIG_CTF_MATCH, text);

			if (t <= 10 && !ctfgame.countdown) {
				ctfgame.countdown = qtrue;
				G_GlobalSound (CHAN_AUTO, trap_SoundIndex("sound/world/10_0.wav"));
			}
			break;

		case MATCH_GAME:
			sprintf(text, "%02d:%02d MATCH",
				t / 60, t % 60);
			trap_ConfigString (CONFIG_CTF_MATCH, text);
			if (t <= 10 && !ctfgame.countdown) {
				ctfgame.countdown = qtrue;
				G_GlobalSound (CHAN_AUTO, trap_SoundIndex("sound/world/10_0.wav"));
			}
			break;
		}
		return qfalse;

	} else {
		int team1 = 0, team2 = 0;

		if (level.time == ctfgame.lasttime)
			return qfalse;
		ctfgame.lasttime = level.time;
		// this is only done in non-match (public) mode

		if (warn_unbalanced->integer) {
			// count up the team totals
			for (i = 1; i <= game.maxclients; i++) {
				ent = game.edicts + i;
				if (!ent->r.inuse)
					continue;
				if (ent->r.client->resp.ctf_team == CTF_TEAM1)
					team1++;
				else if (ent->r.client->resp.ctf_team == CTF_TEAM2)
					team2++;
			}

			if (team1 - team2 >= 2 && team2 >= 2) {
				if (ctfgame.warnactive != CTF_TEAM1) {
					ctfgame.warnactive = CTF_TEAM1;
					trap_ConfigString (CONFIG_CTF_TEAMINFO, "WARNING: Red has too many players");
				}
			} else if (team2 - team1 >= 2 && team1 >= 2) {
				if (ctfgame.warnactive != CTF_TEAM2) {
					ctfgame.warnactive = CTF_TEAM2;
					trap_ConfigString (CONFIG_CTF_TEAMINFO, "WARNING: Blue has too many players");
				}
			} else
				ctfgame.warnactive = 0;
		} else
			ctfgame.warnactive = 0;

	}

	if (capturelimit->integer && 
		(ctfgame.team1 >= capturelimit->integer ||
		ctfgame.team2 >= capturelimit->integer)) {
		G_PrintMsg (NULL, PRINT_HIGH, "Capturelimit hit.\n");
		return qtrue;
	}
	return qfalse;
}

/*----------------------------------------------------------------------------------*/
/* ADMIN */

typedef struct admin_settings_s {
	int matchlen;
	int matchsetuplen;
	int matchstartlen;
	qboolean weaponsstay;
	qboolean instantitems;
	qboolean quaddrop;
	qboolean instantweap;
	qboolean matchlock;
} admin_settings_t;

#define SETMENU_SIZE (7 + 5)

void CTFAdmin_UpdateSettings(edict_t *ent, pmenuhnd_t *setmenu);
void CTFOpenAdminMenu(edict_t *ent);

void CTFAdmin_SettingsApply(edict_t *ent, pmenuhnd_t *p)
{
	admin_settings_t *settings = p->arg;
	char st[80];
	int i;

	if (settings->matchlen != matchtime->value) {
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s changed the match length to %d minutes.\n",
			ent->r.client->pers.netname, S_COLOR_WHITE, settings->matchlen);
		if (ctfgame.match == MATCH_GAME) {
			// in the middle of a match, change it on the fly
			ctfgame.matchtime = (ctfgame.matchtime - matchtime->value*60) + settings->matchlen*60;
		} 
		sprintf(st, "%d", settings->matchlen);
		trap_Cvar_Set("matchtime", st);
	}

	if (settings->matchsetuplen != matchsetuptime->value) {
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s changed the match setup time to %d minutes.\n",
			ent->r.client->pers.netname, S_COLOR_WHITE, settings->matchsetuplen);
		if (ctfgame.match == MATCH_SETUP) {
			// in the middle of a match, change it on the fly
			ctfgame.matchtime = (ctfgame.matchtime - matchsetuptime->value*60) + settings->matchsetuplen*60;
		} 
		sprintf(st, "%d", settings->matchsetuplen);
		trap_Cvar_Set("matchsetuptime", st);
	}

	if (settings->matchstartlen != matchstarttime->value) {
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s changed the match start time to %d seconds.\n",
			ent->r.client->pers.netname, S_COLOR_WHITE, settings->matchstartlen);
		if (ctfgame.match == MATCH_PREGAME) {
			// in the middle of a match, change it on the fly
			ctfgame.matchtime = (ctfgame.matchtime - matchstarttime->value) + settings->matchstartlen;
		} 
		sprintf(st, "%d", settings->matchstartlen);
		trap_Cvar_Set("matchstarttime", st);
	}

	if (settings->weaponsstay != !!(dmflags->integer & DF_WEAPONS_STAY)) {
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s turned %s weapons stay.\n",
			ent->r.client->pers.netname, S_COLOR_WHITE, settings->weaponsstay ? "on" : "off");
		i = dmflags->integer;
		if (settings->weaponsstay)
			i |= DF_WEAPONS_STAY;
		else
			i &= ~DF_WEAPONS_STAY;
		sprintf(st, "%d", i);
		trap_Cvar_Set("dmflags", st);
	}

	if (settings->instantitems != !!(dmflags->integer & DF_INSTANT_ITEMS)) {
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s turned %s instant items.\n",
			ent->r.client->pers.netname, S_COLOR_WHITE, settings->instantitems ? "on" : "off");
		i = dmflags->integer;
		if (settings->instantitems)
			i |= DF_INSTANT_ITEMS;
		else
			i &= ~DF_INSTANT_ITEMS;
		sprintf(st, "%d", i);
		trap_Cvar_Set("dmflags", st);
	}

	if (settings->quaddrop != !!(dmflags->integer & DF_QUAD_DROP)) {
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s turned %s quad drop.\n",
			ent->r.client->pers.netname, S_COLOR_WHITE, settings->quaddrop ? "on" : "off");
		i = dmflags->integer;
		if (settings->quaddrop)
			i |= DF_QUAD_DROP;
		else
			i &= ~DF_QUAD_DROP;
		sprintf(st, "%d", i);
		trap_Cvar_Set("dmflags", st);
	}

	if (settings->instantweap != !!((int)instantweap->value)) {
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s turned %s instant weapons.\n",
			ent->r.client->pers.netname, S_COLOR_WHITE, settings->instantweap ? "on" : "off");
		sprintf(st, "%d", (int)settings->instantweap);
		trap_Cvar_Set("instantweap", st);
	}

	if (settings->matchlock != !!((int)matchlock->value)) {
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s turned %s match lock.\n",
			ent->r.client->pers.netname, S_COLOR_WHITE, settings->matchlock ? "on" : "off");
		sprintf(st, "%d", (int)settings->matchlock);
		trap_Cvar_Set("matchlock", st);
	}

	PMenu_Close(ent);
	CTFOpenAdminMenu(ent);
}

void CTFAdmin_SettingsCancel(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Close(ent);
	CTFOpenAdminMenu(ent);
}

void CTFAdmin_ChangeMatchLen(edict_t *ent, pmenuhnd_t *p)
{
	admin_settings_t *settings = p->arg;

	settings->matchlen = (settings->matchlen % 60) + 5;
	if (settings->matchlen < 5)
		settings->matchlen = 5;

	CTFAdmin_UpdateSettings(ent, p);
}

void CTFAdmin_ChangeMatchSetupLen(edict_t *ent, pmenuhnd_t *p)
{
	admin_settings_t *settings = p->arg;

	settings->matchsetuplen = (settings->matchsetuplen % 60) + 5;
	if (settings->matchsetuplen < 5)
		settings->matchsetuplen = 5;

	CTFAdmin_UpdateSettings(ent, p);
}

void CTFAdmin_ChangeMatchStartLen(edict_t *ent, pmenuhnd_t *p)
{
	admin_settings_t *settings = p->arg;

	settings->matchstartlen = (settings->matchstartlen % 600) + 10;
	if (settings->matchstartlen < 20)
		settings->matchstartlen = 20;

	CTFAdmin_UpdateSettings(ent, p);
}

void CTFAdmin_ChangeWeapStay(edict_t *ent, pmenuhnd_t *p)
{
	admin_settings_t *settings = p->arg;

	settings->weaponsstay = !settings->weaponsstay;
	CTFAdmin_UpdateSettings(ent, p);
}

void CTFAdmin_ChangeInstantItems(edict_t *ent, pmenuhnd_t *p)
{
	admin_settings_t *settings = p->arg;

	settings->instantitems = !settings->instantitems;
	CTFAdmin_UpdateSettings(ent, p);
}

void CTFAdmin_ChangeQuadDrop(edict_t *ent, pmenuhnd_t *p)
{
	admin_settings_t *settings = p->arg;

	settings->quaddrop = !settings->quaddrop;
	CTFAdmin_UpdateSettings(ent, p);
}

void CTFAdmin_ChangeInstantWeap(edict_t *ent, pmenuhnd_t *p)
{
	admin_settings_t *settings = p->arg;

	settings->instantweap = !settings->instantweap;
	CTFAdmin_UpdateSettings(ent, p);
}

void CTFAdmin_ChangeMatchLock(edict_t *ent, pmenuhnd_t *p)
{
	admin_settings_t *settings = p->arg;

	settings->matchlock = !settings->matchlock;
	CTFAdmin_UpdateSettings(ent, p);
}

void CTFAdmin_UpdateSettings(edict_t *ent, pmenuhnd_t *setmenu)
{
	int i = 2;
	char text[64];
	admin_settings_t *settings = setmenu->arg;

	sprintf(text, "Match Len:       %2d mins", settings->matchlen);
	PMenu_UpdateEntry(setmenu->entries + i, text, PMENU_ALIGN_LEFT, CTFAdmin_ChangeMatchLen);
	i++;

	sprintf(text, "Match Setup Len: %2d mins", settings->matchsetuplen);
	PMenu_UpdateEntry(setmenu->entries + i, text, PMENU_ALIGN_LEFT, CTFAdmin_ChangeMatchSetupLen);
	i++;

	sprintf(text, "Match Start Len: %2d secs", settings->matchstartlen);
	PMenu_UpdateEntry(setmenu->entries + i, text, PMENU_ALIGN_LEFT, CTFAdmin_ChangeMatchStartLen);
	i++;

	sprintf(text, "Weapons Stay:    %s", settings->weaponsstay ? "Yes" : "No");
	PMenu_UpdateEntry(setmenu->entries + i, text, PMENU_ALIGN_LEFT, CTFAdmin_ChangeWeapStay);
	i++;

	sprintf(text, "Instant Items:   %s", settings->instantitems ? "Yes" : "No");
	PMenu_UpdateEntry(setmenu->entries + i, text, PMENU_ALIGN_LEFT, CTFAdmin_ChangeInstantItems);
	i++;

	sprintf(text, "Quad Drop:       %s", settings->quaddrop ? "Yes" : "No");
	PMenu_UpdateEntry(setmenu->entries + i, text, PMENU_ALIGN_LEFT, CTFAdmin_ChangeQuadDrop);
	i++;

	sprintf(text, "Instant Weapons: %s", settings->instantweap ? "Yes" : "No");
	PMenu_UpdateEntry(setmenu->entries + i, text, PMENU_ALIGN_LEFT, CTFAdmin_ChangeInstantWeap);
	i++;

	sprintf(text, "Match Lock:      %s", settings->matchlock ? "Yes" : "No");
	PMenu_UpdateEntry(setmenu->entries + i, text, PMENU_ALIGN_LEFT, CTFAdmin_ChangeMatchLock);
	i++;

	PMenu_Update(ent);
}

pmenu_t def_setmenu[] = {
	{ "*Settings Menu", PMENU_ALIGN_CENTER, NULL },
	{ NULL,				PMENU_ALIGN_CENTER, NULL },
	{ NULL,				PMENU_ALIGN_LEFT, NULL },	//int matchlen;         
	{ NULL,				PMENU_ALIGN_LEFT, NULL },	//int matchsetuplen;    
	{ NULL,				PMENU_ALIGN_LEFT, NULL },	//int matchstartlen;    
	{ NULL,				PMENU_ALIGN_LEFT, NULL },	//qboolean weaponsstay; 
	{ NULL,				PMENU_ALIGN_LEFT, NULL },	//qboolean instantitems;
	{ NULL,				PMENU_ALIGN_LEFT, NULL },	//qboolean quaddrop;    
	{ NULL,				PMENU_ALIGN_LEFT, NULL },	//qboolean instantweap; 
	{ NULL,				PMENU_ALIGN_LEFT, NULL },	//qboolean matchlock; 
	{ NULL,				PMENU_ALIGN_LEFT, NULL },
	{ "Apply",			PMENU_ALIGN_LEFT, CTFAdmin_SettingsApply },
	{ "Cancel",			PMENU_ALIGN_LEFT, CTFAdmin_SettingsCancel }
};

admin_settings_t settings;

void CTFAdmin_Settings(edict_t *ent, pmenuhnd_t *p)
{
	pmenuhnd_t *menu;

	PMenu_Close(ent);

	settings.matchlen = matchtime->value;
	settings.matchsetuplen = matchsetuptime->value;
	settings.matchstartlen = matchstarttime->value;
	settings.weaponsstay = !!(dmflags->integer & DF_WEAPONS_STAY);
	settings.instantitems = !!(dmflags->integer & DF_INSTANT_ITEMS);
	settings.quaddrop = !!(dmflags->integer & DF_QUAD_DROP);
	settings.instantweap = instantweap->value != 0;
	settings.matchlock = matchlock->value != 0;

	menu = PMenu_Open(ent, def_setmenu, -1, sizeof(def_setmenu) / sizeof(pmenu_t), &settings);
	CTFAdmin_UpdateSettings(ent, menu);
}

void CTFAdmin_MatchSet(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Close(ent);

	if (ctfgame.match == MATCH_SETUP) {
		G_PrintMsg (NULL, PRINT_CHAT, "Match has been forced to start.\n");
		ctfgame.match = MATCH_PREGAME;
		ctfgame.matchtime = level.time + matchstarttime->value;
		G_GlobalSound (CHAN_AUTO, trap_SoundIndex("sound/misc/talk1.wav"));
		ctfgame.countdown = qfalse;
	} else if (ctfgame.match == MATCH_GAME) {
		G_PrintMsg (NULL, PRINT_CHAT, "Match has been forced to terminate.\n");
		ctfgame.match = MATCH_SETUP;
		ctfgame.matchtime = level.time + matchsetuptime->value * 60;
		CTFResetAllPlayers();
	}
}

void CTFAdmin_MatchMode(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Close(ent);

	if (ctfgame.match != MATCH_SETUP) {
		if (competition->value < 3)
			trap_Cvar_Set("competition", "2");
		ctfgame.match = MATCH_SETUP;
		CTFResetAllPlayers();
	}
}

void CTFAdmin_Reset(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Close(ent);

	// go back to normal mode
	G_PrintMsg (NULL, PRINT_CHAT, "Match mode has been terminated, reseting to normal game.\n");
	ctfgame.match = MATCH_NONE;
	trap_Cvar_Set("competition", "1");
	CTFResetAllPlayers();
}

void CTFAdmin_Cancel(edict_t *ent, pmenuhnd_t *p)
{
	PMenu_Close(ent);
}


pmenu_t adminmenu[] = {
	{ "*Administration Menu",	PMENU_ALIGN_CENTER, NULL },
	{ NULL,						PMENU_ALIGN_CENTER, NULL }, // blank
	{ "Settings",				PMENU_ALIGN_LEFT, CTFAdmin_Settings },
	{ NULL,						PMENU_ALIGN_LEFT, NULL },
	{ NULL,						PMENU_ALIGN_LEFT, NULL },
	{ "Cancel",					PMENU_ALIGN_LEFT, CTFAdmin_Cancel },
	{ NULL,						PMENU_ALIGN_CENTER, NULL },
};

void CTFOpenAdminMenu(edict_t *ent)
{
	adminmenu[3].text = NULL;
	adminmenu[3].SelectFunc = NULL;
	adminmenu[4].text = NULL;
	adminmenu[4].SelectFunc = NULL;
	if (ctfgame.match == MATCH_SETUP) {
		adminmenu[3].text = "Force start match";
		adminmenu[3].SelectFunc = CTFAdmin_MatchSet;
		adminmenu[4].text = "Reset to pickup mode";
		adminmenu[4].SelectFunc = CTFAdmin_Reset;
	} else if (ctfgame.match == MATCH_GAME || ctfgame.match == MATCH_PREGAME) {
		adminmenu[3].text = "Cancel match";
		adminmenu[3].SelectFunc = CTFAdmin_MatchSet;
	} else if (ctfgame.match == MATCH_NONE && competition->value) {
		adminmenu[3].text = "Switch to match mode";
		adminmenu[3].SelectFunc = CTFAdmin_MatchMode;
	}

//	if (ent->client->menu)
//		PMenu_Close(ent->client->menu);

	PMenu_Open(ent, adminmenu, -1, sizeof(adminmenu) / sizeof(pmenu_t), NULL);
}

void CTFAdmin(edict_t *ent)
{
	char text[1024];

	if (!allow_admin->value) {
		G_PrintMsg (ent, PRINT_HIGH, "Administration is disabled\n");
		return;
	}

	if (trap_Cmd_Argc() > 1 && admin_password->string && *admin_password->string &&
		!ent->r.client->resp.admin && strcmp(admin_password->string, trap_Cmd_Argv(1)) == 0) {
		ent->r.client->resp.admin = qtrue;
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s has become an admin.\n", ent->r.client->pers.netname, S_COLOR_WHITE);
		G_PrintMsg (ent, PRINT_HIGH, "Type 'admin' to access the adminstration menu.\n");
	}

	if (!ent->r.client->resp.admin) {
		sprintf (text, "%s%s has requested admin rights.",
			ent->r.client->pers.netname, S_COLOR_WHITE);
		CTFBeginElection(ent, ELECT_ADMIN, text);
		return;
	}

	if (ent->r.client->menu)
		PMenu_Close(ent);

	CTFOpenAdminMenu(ent);
}

/*----------------------------------------------------------------*/

void CTFStats(edict_t *ent)
{
	int i, e;
	ghost_t *g;
	char st[80];
	char text[1024];
	edict_t *e2;

	*text = 0;
	if (ctfgame.match == MATCH_SETUP) {
		for (i = 1; i <= game.maxclients; i++) {
			e2 = game.edicts + i;
			if (!e2->r.inuse)
				continue;
			if (!e2->r.client->resp.ready && e2->r.client->resp.ctf_team != CTF_NOTEAM) {
				sprintf(st, "%s%s is not ready.\n", e2->r.client->pers.netname, S_COLOR_WHITE);
				if (strlen(text) + strlen(st) < sizeof(text) - 50)
					strcat(text, st);
			}
		}
	}

	for (i = 0, g = ctfgame.ghosts; i < MAX_CLIENTS; i++, g++)
		if (g->ent)
			break;

	if (i == MAX_CLIENTS) {
		if (*text)
			G_PrintMsg (ent, PRINT_HIGH, "%s", text);
		G_PrintMsg (ent, PRINT_HIGH, "No statistics available.\n");
		return;
	}

	strcat(text, "  #|Name            |Score|Kills|Death|BasDf|CarDf|Effcy|\n");

	for (i = 0, g = ctfgame.ghosts; i < MAX_CLIENTS; i++, g++) {
		if (!*g->netname)
			continue;

		if (g->deaths + g->kills == 0)
			e = 50;
		else
			e = g->kills * 100 / (g->kills + g->deaths);
		sprintf(st, "%3d|%-16.16s|%5d|%5d|%5d|%5d|%5d|%4d%%|\n",
			g->number, 
			g->netname, 
			g->score, 
			g->kills, 
			g->deaths, 
			g->basedef,
			g->carrierdef, 
			e);
		if (strlen(text) + strlen(st) > sizeof(text) - 50) {
			sprintf(text+strlen(text), "And more...\n");
			G_PrintMsg (ent, PRINT_HIGH, "%s", text);
			return;
		}
		strcat(text, st);
	}
	G_PrintMsg (ent, PRINT_HIGH, "%s", text);
}

void CTFPlayerList(edict_t *ent)
{
	int i;
	char st[80];
	char text[1400];
	edict_t *e2;

	// number, name, connect time, ping, score, admin

	*text = 0;
	for (i = 1; i <= game.maxclients; i++) {
		e2 = game.edicts + i;
		if (!e2->r.inuse)
			continue;

		Q_snprintfz(st, sizeof(st), "%3d %-16.16s%s %02d:%02d %4d %3d%s%s\n",
			i,
			e2->r.client->pers.netname, S_COLOR_WHITE,
			(level.framenum - e2->r.client->resp.enterframe) / 600,
			((level.framenum - e2->r.client->resp.enterframe) % 600)/10,
			e2->r.client->r.ping,
			e2->r.client->resp.score,
			(ctfgame.match == MATCH_SETUP || ctfgame.match == MATCH_PREGAME) ?
			(e2->r.client->resp.ready ? " (ready)" : " (notready)") : "",
			e2->r.client->resp.admin ? " (admin)" : "");

		if (strlen(text) + strlen(st) > sizeof(text) - 50) {
			sprintf(text+strlen(text), "And more...\n");
			G_PrintMsg (ent, PRINT_HIGH, "%s", text);
			return;
		}
		strcat(text, st);
	}
	G_PrintMsg (ent, PRINT_HIGH, "%s", text);
}


void CTFWarp(edict_t *ent)
{
	char text[1024];
	char *mlist, *token;
	static const char *seps = " \t\n\r";

	if (trap_Cmd_Argc() < 2) {
		G_PrintMsg (ent, PRINT_HIGH, "Where do you want to warp to?\n");
		G_PrintMsg (ent, PRINT_HIGH, "Available levels are: %s\n", warp_list->string);
		return;
	}

	mlist = G_CopyString(warp_list->string);

	token = strtok(mlist, seps);
	while (token != NULL) {
		if (Q_stricmp(token, trap_Cmd_Argv(1)) == 0)
			break;
		token = strtok(NULL, seps);
	}

	if (token == NULL) {
		G_PrintMsg (ent, PRINT_HIGH, "Unknown CTF level.\n");
		G_PrintMsg (ent, PRINT_HIGH, "Available levels are: %s\n", warp_list->string);
		free(mlist);
		return;
	}

	G_LevelFree (mlist);

	if (ent->r.client->resp.admin) {
		G_PrintMsg (NULL, PRINT_HIGH, "%s%s is warping to level %s.\n", 
			ent->r.client->pers.netname, S_COLOR_WHITE, trap_Cmd_Argv(1));
		Q_strncpyz (level.forcemap, trap_Cmd_Argv(1), sizeof(level.forcemap));
		G_EndDMLevel();
		return;
	}

	sprintf(text, "%s%s has requested warping to level %s.", 
			ent->r.client->pers.netname, S_COLOR_WHITE, trap_Cmd_Argv(1));
	if (CTFBeginElection(ent, ELECT_MAP, text))
		Q_strncpyz (ctfgame.elevel, trap_Cmd_Argv(1), sizeof(ctfgame.elevel));
}

void CTFBoot(edict_t *ent)
{
	int i;
	edict_t *targ;
	char text[80];

	if (!ent->r.client->resp.admin) {
		G_PrintMsg (ent, PRINT_HIGH, "You are not an admin.\n");
		return;
	}

	if (trap_Cmd_Argc() < 2) {
		G_PrintMsg (ent, PRINT_HIGH, "Who do you want to kick?\n");
		return;
	}

	if (*trap_Cmd_Argv(1) < '0' && *trap_Cmd_Argv(1) > '9') {
		G_PrintMsg (ent, PRINT_HIGH, "Specify the player number to kick.\n");
		return;
	}

	i = atoi(trap_Cmd_Argv(1));
	if (i < 1 || i > game.maxclients) {
		G_PrintMsg (ent, PRINT_HIGH, "Invalid player number.\n");
		return;
	}

	targ = game.edicts + i;
	if (!targ->r.inuse) {
		G_PrintMsg (ent, PRINT_HIGH, "That player number is not connected.\n");
		return;
	}

	sprintf(text, "kick %d\n", i - 1);
	trap_AddCommandString(text);
}


void CTFSetPowerUpEffect(edict_t *ent, int def)
{
	if (ent->r.client->resp.ctf_team == CTF_TEAM1)
		ent->s.effects |= EF_PENT; // red
	else if (ent->r.client->resp.ctf_team == CTF_TEAM2)
		ent->s.effects |= EF_QUAD; // blue
	else
		ent->s.effects |= def;
}

