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

typedef struct
{
	char	*name;
	void	(*spawn)(edict_t *ent);
} spawn_t;

void SP_info_player_start (edict_t *ent);
void SP_info_player_deathmatch (edict_t *ent);
void SP_info_player_coop (edict_t *ent);
void SP_info_player_intermission (edict_t *ent);

void SP_func_plat (edict_t *ent);
void SP_func_rotating (edict_t *ent);
void SP_func_button (edict_t *ent);
void SP_func_door (edict_t *ent);
void SP_func_door_secret (edict_t *ent);
void SP_func_door_rotating (edict_t *ent);
void SP_func_water (edict_t *ent);
void SP_func_train (edict_t *ent);
void SP_func_conveyor (edict_t *self);
void SP_func_wall (edict_t *self);
void SP_func_object (edict_t *self);
void SP_func_explosive (edict_t *self);
void SP_func_timer (edict_t *self);
void SP_func_clock (edict_t *ent);
void SP_func_killbox (edict_t *ent);
void SP_func_static (edict_t *ent);
void SP_func_bobbing ( edict_t *ent );
void SP_func_pendulum ( edict_t *ent );

void SP_trigger_always (edict_t *ent);
void SP_trigger_once (edict_t *ent);
void SP_trigger_multiple (edict_t *ent);
void SP_trigger_relay (edict_t *ent);
void SP_trigger_push (edict_t *ent);
void SP_trigger_hurt (edict_t *ent);
void SP_trigger_key (edict_t *ent);
void SP_trigger_counter (edict_t *ent);
void SP_trigger_elevator (edict_t *ent);
void SP_trigger_gravity (edict_t *ent);
void SP_trigger_monsterjump (edict_t *ent);

void SP_target_temp_entity (edict_t *ent);
void SP_target_speaker (edict_t *ent);
void SP_target_explosion (edict_t *ent);
void SP_target_changelevel (edict_t *ent);
void SP_target_secret (edict_t *ent);
void SP_target_goal (edict_t *ent);
void SP_target_splash (edict_t *ent);
void SP_target_spawner (edict_t *ent);
void SP_target_blaster (edict_t *ent);
void SP_target_crosslevel_trigger (edict_t *ent);
void SP_target_crosslevel_target (edict_t *ent);
void SP_target_laser (edict_t *self);
void SP_target_help (edict_t *ent);
void SP_target_actor (edict_t *ent);
void SP_target_lightramp (edict_t *self);
void SP_target_earthquake (edict_t *ent);
void SP_target_character (edict_t *ent);
void SP_target_string (edict_t *ent);
void SP_target_location (edict_t *self);
void SP_target_position (edict_t *self);
void SP_target_print (edict_t *self);

void SP_worldspawn (edict_t *ent);
void SP_viewthing (edict_t *ent);

void SP_light (edict_t *self);
void SP_light_mine1 (edict_t *ent);
void SP_light_mine2 (edict_t *ent);
void SP_info_null (edict_t *self);
void SP_info_notnull (edict_t *self);
void SP_info_camp (edict_t *self);
void SP_path_corner (edict_t *self);
void SP_point_combat (edict_t *self);

void SP_misc_explobox (edict_t *self);
void SP_misc_banner (edict_t *self);
void SP_misc_satellite_dish (edict_t *self);
void SP_misc_actor (edict_t *self);
void SP_misc_gib_arm (edict_t *self);
void SP_misc_gib_leg (edict_t *self);
void SP_misc_gib_head (edict_t *self);
void SP_misc_insane (edict_t *self);
void SP_misc_deadsoldier (edict_t *self);
void SP_misc_viper (edict_t *self);
void SP_misc_viper_bomb (edict_t *self);
void SP_misc_bigviper (edict_t *self);
void SP_misc_strogg_ship (edict_t *self);
void SP_misc_teleporter_dest (edict_t *self);
void SP_misc_blackhole (edict_t *self);
void SP_misc_eastertank (edict_t *self);
void SP_misc_easterchick (edict_t *self);
void SP_misc_easterchick2 (edict_t *self);
void SP_misc_model (edict_t *ent);
void SP_misc_portal_surface (edict_t *ent);
void SP_misc_portal_camera (edict_t *ent);

void SP_skyportal(edict_t *ent);

void SP_monster_berserk (edict_t *self);
void SP_monster_gladiator (edict_t *self);
void SP_monster_gunner (edict_t *self);
void SP_monster_infantry (edict_t *self);
void SP_monster_soldier_light (edict_t *self);
void SP_monster_soldier (edict_t *self);
void SP_monster_soldier_ss (edict_t *self);
void SP_monster_tank (edict_t *self);
void SP_monster_medic (edict_t *self);
void SP_monster_flipper (edict_t *self);
void SP_monster_chick (edict_t *self);
void SP_monster_parasite (edict_t *self);
void SP_monster_flyer (edict_t *self);
void SP_monster_brain (edict_t *self);
void SP_monster_floater (edict_t *self);
void SP_monster_hover (edict_t *self);
void SP_monster_mutant (edict_t *self);
void SP_monster_supertank (edict_t *self);
void SP_monster_boss2 (edict_t *self);
void SP_monster_jorg (edict_t *self);
void SP_monster_boss3_stand (edict_t *self);

void SP_monster_commander_body (edict_t *self);

void SP_turret_breach (edict_t *self);
void SP_turret_base (edict_t *self);
void SP_turret_driver (edict_t *self);


spawn_t	spawns[] = {
	{"info_player_start", SP_info_player_start},
	{"info_player_deathmatch", SP_info_player_deathmatch},
	{"info_player_coop", SP_info_player_coop},
	{"info_player_intermission", SP_info_player_intermission},
//ZOID
	{"team_CTF_redspawn", SP_team_CTF_redspawn},
	{"team_CTF_bluespawn", SP_team_CTF_bluespawn},
	{"team_CTF_redplayer", SP_team_CTF_redplayer},
	{"team_CTF_blueplayer", SP_team_CTF_blueplayer},
//ZOID

	{"func_plat", SP_func_plat},
	{"func_button", SP_func_button},
	{"func_door", SP_func_door},
	{"func_door_secret", SP_func_door_secret},
	{"func_door_rotating", SP_func_door_rotating},
	{"func_rotating", SP_func_rotating},
	{"func_train", SP_func_train},
	{"func_water", SP_func_water},
	{"func_conveyor", SP_func_conveyor},
	{"func_clock", SP_func_clock},
	{"func_wall", SP_func_wall},
	{"func_object", SP_func_object},
	{"func_timer", SP_func_timer},
	{"func_explosive", SP_func_explosive},
	{"func_killbox", SP_func_killbox},
	{"func_static", SP_func_static},
	{"func_bobbing", SP_func_bobbing},
	{"func_pendulum", SP_func_pendulum},

	{"trigger_always", SP_trigger_always},
	{"trigger_once", SP_trigger_once},
	{"trigger_multiple", SP_trigger_multiple},
	{"trigger_relay", SP_trigger_relay},
	{"trigger_push", SP_trigger_push},
	{"trigger_hurt", SP_trigger_hurt},
	{"trigger_counter", SP_trigger_counter},
	{"trigger_elevator", SP_trigger_elevator},
	{"trigger_gravity", SP_trigger_gravity},
	{"trigger_monsterjump", SP_trigger_monsterjump},

	{"target_temp_entity", SP_target_temp_entity},
	{"target_speaker", SP_target_speaker},
	{"target_explosion", SP_target_explosion},
	{"target_changelevel", SP_target_changelevel},
	{"target_secret", SP_target_secret},
	{"target_goal", SP_target_goal},
	{"target_splash", SP_target_splash},
	{"target_spawner", SP_target_spawner},
	{"target_blaster", SP_target_blaster},
	{"target_crosslevel_trigger", SP_target_crosslevel_trigger},
	{"target_crosslevel_target", SP_target_crosslevel_target},
	{"target_laser", SP_target_laser},
	{"target_help", SP_target_help},
#if 0 // remove monster code
	{"target_actor", SP_target_actor},
#endif
	{"target_lightramp", SP_target_lightramp},
	{"target_earthquake", SP_target_earthquake},
	{"target_character", SP_target_character},
	{"target_string", SP_target_string},
	{"target_location", SP_target_location},
	{"target_position", SP_target_position},
	{"target_print", SP_target_print},

	{"worldspawn", SP_worldspawn},
	{"viewthing", SP_viewthing},

	{"light", SP_light},
	{"light_mine1", SP_light_mine1},
	{"light_mine2", SP_light_mine2},
	{"info_null", SP_info_null},
	{"func_group", SP_info_null},
	{"info_notnull", SP_info_notnull},
	{"info_camp", SP_info_camp},
	{"path_corner", SP_path_corner},
	{"point_combat", SP_point_combat},

	{"misc_explobox", SP_misc_explobox},
	{"misc_banner", SP_misc_banner},

	{"misc_satellite_dish", SP_misc_satellite_dish},
#if 0 // remove monster code
	{"misc_actor", SP_misc_actor},
#endif
	{"misc_gib_arm", SP_misc_gib_arm},
	{"misc_gib_leg", SP_misc_gib_leg},
	{"misc_gib_head", SP_misc_gib_head},
#if 0 // remove monster code
	{"misc_insane", SP_misc_insane},
#endif
	{"misc_deadsoldier", SP_misc_deadsoldier},
	{"misc_viper", SP_misc_viper},
	{"misc_viper_bomb", SP_misc_viper_bomb},
	{"misc_bigviper", SP_misc_bigviper},
	{"misc_strogg_ship", SP_misc_strogg_ship},
	{"misc_teleporter_dest", SP_misc_teleporter_dest},
//ZOID
	{"trigger_teleport", SP_trigger_teleport},
	{"info_teleport_destination", SP_info_teleport_destination},
//ZOID
	{"misc_blackhole", SP_misc_blackhole},
	{"misc_eastertank", SP_misc_eastertank},
	{"misc_easterchick", SP_misc_easterchick},
	{"misc_easterchick2", SP_misc_easterchick2},
	{"misc_model", SP_misc_model},
	{"misc_portal_surface", SP_misc_portal_surface},
	{"misc_portal_camera", SP_misc_portal_camera},
	{"misc_skyportal", SP_skyportal},
	{"props_skyportal", SP_skyportal},
	
#if 0 // remove monster code
	{"monster_berserk", SP_monster_berserk},
	{"monster_gladiator", SP_monster_gladiator},
	{"monster_gunner", SP_monster_gunner},
	{"monster_infantry", SP_monster_infantry},
	{"monster_soldier_light", SP_monster_soldier_light},
	{"monster_soldier", SP_monster_soldier},
	{"monster_soldier_ss", SP_monster_soldier_ss},
	{"monster_tank", SP_monster_tank},
	{"monster_tank_commander", SP_monster_tank},
	{"monster_medic", SP_monster_medic},
	{"monster_flipper", SP_monster_flipper},
	{"monster_chick", SP_monster_chick},
	{"monster_parasite", SP_monster_parasite},
	{"monster_flyer", SP_monster_flyer},
	{"monster_brain", SP_monster_brain},
	{"monster_floater", SP_monster_floater},
	{"monster_hover", SP_monster_hover},
	{"monster_mutant", SP_monster_mutant},
	{"monster_supertank", SP_monster_supertank},
	{"monster_boss2", SP_monster_boss2},
	{"monster_boss3_stand", SP_monster_boss3_stand},
	{"monster_jorg", SP_monster_jorg},

	{"monster_commander_body", SP_monster_commander_body},

	{"turret_breach", SP_turret_breach},
	{"turret_base", SP_turret_base},
	{"turret_driver", SP_turret_driver},
#endif

	{NULL, NULL}
};

/*
===============
ED_CallSpawn

Finds the spawn function for the entity and calls it
===============
*/
void ED_CallSpawn (edict_t *ent)
{
	spawn_t	*s;
	gitem_t	*item;
	int		i;

	if (!ent->classname)
	{
		if (developer->integer)
			G_Printf ("ED_CallSpawn: NULL classname\n");
		return;
	}

	// check item spawn functions
	for (i=0,item=itemlist ; i<game.num_items ; i++,item++)
	{
		if (!item->classname)
			continue;
		if (!Q_stricmp(item->classname, ent->classname))
		{	// found it
			SpawnItem (ent, item);
			return;
		}
	}

	// check normal spawn functions
	for (s=spawns ; s->name ; s++)
	{
		if (!Q_stricmp(s->name, ent->classname))
		{	// found it
			s->spawn (ent);
			return;
		}
	}

	if (developer->integer)
		G_Printf ("%s doesn't have a spawn function\n", ent->classname);
}

/*
=============
ED_NewString
=============
*/
char *ED_NewString (char *string)
{
	char	*newb, *new_p;
	int		i,l;
	
	l = strlen(string) + 1;

	newb = G_LevelMalloc (l);

	new_p = newb;

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n') {
				*new_p++ = '\n';
			} else {
				*new_p++ = '/';
				*new_p++ = string[i];
			}
		}
		else
			*new_p++ = string[i];
	}
	
	return newb;
}




/*
===============
ED_ParseField

Takes a key/value pair and sets the binary values
in an edict
===============
*/
void ED_ParseField (char *key, char *value, edict_t *ent)
{
	field_t	*f;
	qbyte	*b;
	float	v;
	vec3_t	vec;

	for (f=fields ; f->name ; f++)
	{
		if (!Q_stricmp(f->name, key))
		{	// found it
			if (f->flags & FFL_SPAWNTEMP)
				b = (qbyte *)&st;
			else
				b = (qbyte *)ent;

			switch (f->type)
			{
			case F_LSTRING:
				*(char **)(b+f->ofs) = ED_NewString (value);
				break;
			case F_VECTOR:
				sscanf (value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
				((float *)(b+f->ofs))[0] = vec[0];
				((float *)(b+f->ofs))[1] = vec[1];
				((float *)(b+f->ofs))[2] = vec[2];
				break;
			case F_INT:
				*(int *)(b+f->ofs) = atoi(value);
				break;
			case F_FLOAT:
				*(float *)(b+f->ofs) = atof(value);
				break;
			case F_ANGLEHACK:
				v = atof(value);
				((float *)(b+f->ofs))[0] = 0;
				((float *)(b+f->ofs))[1] = v;
				((float *)(b+f->ofs))[2] = 0;
				break;
			case F_IGNORE:
				break;
			}
			return;
		}
	}

	if (developer->integer)
		G_Printf ("%s is not a field\n", key);
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
====================
*/
char *ED_ParseEdict (char *data, edict_t *ent)
{
	qboolean	init;
	char		keyname[256];
	char		*com_token;

	init = qfalse;
	memset (&st, 0, sizeof(st));

// go through all the dictionary pairs
	while (1)
	{	
	// parse key
		com_token = COM_Parse (&data);
		if (com_token[0] == '}')
			break;
		if (!data)
			G_Error ("ED_ParseEntity: EOF without closing brace");

		Q_strncpyz (keyname, com_token, sizeof(keyname));
		
	// parse value	
		com_token = COM_Parse (&data);
		if (!data)
			G_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			G_Error ("ED_ParseEntity: closing brace without data");

		init = qtrue;	

	// keynames with a leading underscore are used for utility comments,
	// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		ED_ParseField (keyname, com_token, ent);
	}

	if (!init)
		memset (ent, 0, sizeof(*ent));

	return data;
}


/*
================
G_FindTeams

Chain together all entities with a matching team field.

All but the first will have the FL_TEAMSLAVE flag set.
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams (void)
{
	edict_t	*e, *e2, *chain;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for (i=1, e=game.edicts+i ; i < game.numentities ; i++,e++)
	{
		if (!e->r.inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMSLAVE)
			continue;
		chain = e;
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < game.numentities ; j++,e2++)
		{
			if (!e2->r.inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				chain->teamchain = e2;
				e2->teammaster = e;
				chain = e2;
				e2->flags |= FL_TEAMSLAVE;
			}
		}
	}

	if (developer->integer)
		G_Printf ("%i teams with %i entities\n", c, c2);
}

/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SpawnEntities (char *mapname, char *entities, char *spawnpoint)
{
	edict_t		*ent;
	int			inhibit;
	char		*com_token;
	int			i;
	float		skill_level;

	skill_level = skill->integer;
	clamp (skill_level, 0, 3);
	if (skill->integer != skill_level)
		trap_Cvar_ForceSet ("skill", va("%f", skill_level));

	SaveClientData ();

	G_LevelInitPool( strlen(mapname) + 1 + strlen(entities) + 1 + G_LEVEL_DEFAULT_POOL_SIZE );

	memset (&level, 0, sizeof(level));
	memset (game.edicts, 0, game.maxentities * sizeof (game.edicts[0]));

	Q_strncpyz (level.mapname, mapname, sizeof(level.mapname));
	Q_strncpyz (game.spawnpoint, spawnpoint, sizeof(game.spawnpoint));

	// set client fields on player ents
	for (i=0 ; i<game.maxclients ; i++)
		game.edicts[i+1].r.client = game.clients + i;

	ent = NULL;
	inhibit = 0;

// parse ents
	while (1)
	{
		// parse the opening brace	
		com_token = COM_Parse (&entities);
		if (!entities)
			break;
		if (com_token[0] != '{')
			G_Error ("ED_LoadFromFile: found %s when expecting {",com_token);

		if (!ent)
			ent = world;
		else
			ent = G_Spawn ();
		entities = ED_ParseEdict (entities, ent);
		
		// remove things (except the world) from different skill levels or deathmatch
		if (ent != world)
		{
			if ( deathmatch->integer )
			{
				if ( ctf->integer || (dmflags->integer & (DF_MODELTEAMS | DF_SKINTEAMS)) ) {
					if ( st.notteam ) {
						G_FreeEdict (ent);	
						inhibit++;
						continue;
					}
				} else if ( (ent->spawnflags & SPAWNFLAG_NOT_DEATHMATCH) || st.notfree ) {
					G_FreeEdict (ent);	
					inhibit++;
					continue;
				}
			}
			else
			{
				if ( /* ((coop->integer) && (ent->spawnflags & SPAWNFLAG_NOT_COOP)) || */
					((skill->integer == 0) && (ent->spawnflags & SPAWNFLAG_NOT_EASY)) ||
					((skill->integer == 1) && (ent->spawnflags & SPAWNFLAG_NOT_MEDIUM)) ||
					(((skill->integer == 2) || (skill->integer == 3)) && (ent->spawnflags & SPAWNFLAG_NOT_HARD))
					)
				{
					G_FreeEdict (ent);	
					inhibit++;
					continue;
				} else if ( st.notsingle || st.notfree ) {
					G_FreeEdict (ent);	
					inhibit++;
					continue;
				}
			}

			ent->spawnflags &= ~(SPAWNFLAG_NOT_EASY|SPAWNFLAG_NOT_MEDIUM|SPAWNFLAG_NOT_HARD|SPAWNFLAG_NOT_COOP|SPAWNFLAG_NOT_DEATHMATCH);
		}

		ED_CallSpawn (ent);
	}	

	if (developer->integer)
		G_Printf ("%i entities inhibited\n", inhibit);

	G_FindTeams ();

	PlayerTrail_Init ();

//ZOID
	CTFSpawn();
//ZOID

	// make sure server got the edicts data (cinematics)
	trap_LocateEntities (game.edicts, sizeof(game.edicts[0]), game.numentities, game.maxentities);
}


//===================================================================

char *single_statusbar = "single";
char *dm_statusbar = "dm";

/*QUAKED worldspawn (0 0 0) ?

Only used for the world.
"sky"	environment map name
"skyaxis"	vector axis for rotating sky
"skyrotate"	speed of rotation in degrees/second
"sounds"	music cd track number
"gravity"	800 is default gravity
"message"	text to print at user logon
*/
void SP_worldspawn (edict_t *ent)
{
	ent->movetype = MOVETYPE_PUSH;
	ent->r.solid = SOLID_BSP;
	ent->r.inuse = qtrue;			// since the world doesn't use G_Spawn()
	VectorClear (ent->s.origin);
	VectorClear (ent->s.angles);
	trap_SetBrushModel (ent, "*0");	// sets mins / maxs and modelindex 1

	//---------------

	// reserve some spots for dead player bodies for coop / deathmatch
	InitBodyQue ();

	// set configstrings for items
	SetItemNames ();

	if (st.nextmap)
		strcpy (level.nextmap, st.nextmap);

	// make some data visible to the server
	if (ent->message && ent->message[0])
	{
		trap_ConfigString ( CS_MESSAGE, ent->message );
		Q_strncpyz ( level.level_name, ent->message, sizeof(level.level_name) );
	}
	else
	{
		trap_ConfigString ( CS_MESSAGE, level.mapname );
		Q_strncpyz ( level.level_name, level.mapname, sizeof(level.level_name) );
	}

	// send music
	if ( st.music ) {
		trap_ConfigString ( CS_AUDIOTRACK, st.music );
	}

	trap_ConfigString (CS_MAPNAME, level.mapname);
	trap_ConfigString (CS_MAXCLIENTS, va("%i", sv_maxclients->integer ) );
	trap_ConfigString (CS_AIRACCEL, va("%g", sv_airaccelerate->integer) );
	trap_ConfigString (CS_SKYBOX, "");

	// status bar program
	if (deathmatch->integer)
//ZOID
		if (ctf->integer) {
			trap_ConfigString (CS_STATUSBAR, ctf_statusbar);
			CTFPrecache();
		} else
//ZOID
			trap_ConfigString (CS_STATUSBAR, dm_statusbar);
	else
		trap_ConfigString (CS_STATUSBAR, single_statusbar);

	if( ctf->integer )
		trap_ConfigString( CS_GAMETYPE, "ctf" );
	else if( deathmatch->integer )
		trap_ConfigString( CS_GAMETYPE, "deathmatch" );
	else if( coop->integer )
		trap_ConfigString( CS_GAMETYPE, "cooperative" );
	else
		trap_ConfigString( CS_GAMETYPE, "single player" );


	// help icon for statusbar
	trap_ImageIndex ("pics/i_help");
	trap_ImageIndex ("pics/help");

	if (!st.gravity)
		trap_Cvar_Set("sv_gravity", "800");
	else
		trap_Cvar_Set("sv_gravity", st.gravity);

	snd_fry = trap_SoundIndex ("sound/player/fry.wav");	// standing in lava / slime

	PrecacheItem (FindItem ("Blaster"));
	PrecacheItem (FindItem ("Machinegun"));

	trap_SoundIndex ("sound/player/lava1.wav");
	trap_SoundIndex ("sound/player/lava2.wav");

	trap_SoundIndex ("sound/misc/pc_up.wav");
	trap_SoundIndex ("sound/misc/talk1.wav");

	trap_SoundIndex ("sound/misc/udeath.wav");

	// viewable weapon models
	// THIS ORDER MUST MATCH THE DEFINES IN gs_public.h
	// you can add more, max 255
	trap_ModelIndex ("#gauntlet/gauntlet.md3");		// WEAP_BLASTER
	trap_ModelIndex ("#shotgunq2/shotgunq2.md3");	// WEAP_SHOTGUN
	trap_ModelIndex ("#shotgun/shotgun.md3");		// WEAP_SUPERSHOTGUN
	trap_ModelIndex ("#machinegun/machinegun.md3");	// WEAP_MACHINEGUN
	trap_ModelIndex ("#chaingun/chaingun.md3");		// WEAP_CHAINGUN
	trap_ModelIndex ("#grenadel/grenadel.md3");		// WEAP_GRENADELAUNCHER
	trap_ModelIndex ("#rocketl/rocketl.md3");		// WEAP_ROCKETLAUNCHER
	trap_ModelIndex ("#plasma/plasma.md3");			// WEAP_HYPERBLASTER
	trap_ModelIndex ("#railgun/railgun.md3");		// WEAP_RAILGUN
	trap_ModelIndex ("#bfg/bfg.md3");				// WEAP_BFG
	trap_ModelIndex ("#grapple/grapple.md3");		// WEAP_GRAPPLE


	trap_SoundIndex ("sound/player/gasp1.wav");		// gasping for air
	trap_SoundIndex ("sound/player/gasp2.wav");		// head breaking surface, not gasping

	trap_SoundIndex ("sound/player/watr_in.wav");	// feet hitting water
	trap_SoundIndex ("sound/player/watr_out.wav");	// feet leaving water

	trap_SoundIndex ("sound/player/watr_un.wav");	// head going underwater
	
	trap_SoundIndex ("sound/player/u_breath1.wav");
	trap_SoundIndex ("sound/player/u_breath2.wav");

	trap_SoundIndex ("sound/world/land.wav");		// landing thud
	trap_SoundIndex ("sound/misc/h2ohit1.wav");		// landing splash

	trap_SoundIndex ("sound/items/damage.wav");
	trap_SoundIndex ("sound/items/protect.wav");
	trap_SoundIndex ("sound/items/protect4.wav");
	trap_SoundIndex ("sound/weapons/noammo.wav");

	sm_meat_index = trap_ModelIndex ("models/objects/gibs/sm_meat/tris.md2");
	trap_ModelIndex ("models/objects/gibs/arm/tris.md2");
	trap_ModelIndex ("models/objects/gibs/bone/tris.md2");
	trap_ModelIndex ("models/objects/gibs/bone2/tris.md2");
	trap_ModelIndex ("models/objects/gibs/chest/tris.md2");
	trap_ModelIndex ("models/objects/gibs/skull/tris.md2");
	trap_ModelIndex ("models/objects/gibs/head2/tris.md2");

	//
	// Setup light animation tables. 'a' is total darkness, 'z' is doublebright.
	//

	// 0 normal
	trap_ConfigString( CS_LIGHTS+0, "m" );

	// 1 FLICKER (first variety)
	trap_ConfigString( CS_LIGHTS+1, "mmnmmommommnonmmonqnmmo" );

	// 2 SLOW STRONG PULSE
	trap_ConfigString( CS_LIGHTS+2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba" );

	// 3 CANDLE (first variety)
	trap_ConfigString( CS_LIGHTS+3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg" );

	// 4 FAST STROBE
	trap_ConfigString( CS_LIGHTS+4, "mamamamamama" );

	// 5 GENTLE PULSE 1
	trap_ConfigString( CS_LIGHTS+5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj" );

	// 6 FLICKER (second variety)
	trap_ConfigString( CS_LIGHTS+6, "nmonqnmomnmomomno" );

	// 7 CANDLE (second variety)
	trap_ConfigString( CS_LIGHTS+7, "mmmaaaabcdefgmmmmaaaammmaamm" );

	// 8 CANDLE (third variety)
	trap_ConfigString( CS_LIGHTS+8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa" );

	// 9 SLOW STROBE (fourth variety)
	trap_ConfigString( CS_LIGHTS+9, "aaaaaaaazzzzzzzz" );

	// 10 FLUORESCENT FLICKER
	trap_ConfigString( CS_LIGHTS+10, "mmamammmmammamamaaamammma" );

	// 11 SLOW PULSE NOT FADE TO BLACK
	trap_ConfigString( CS_LIGHTS+11, "abcdefghijklmnopqrrqponmlkjihgfedcba" );

	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	trap_ConfigString( CS_LIGHTS+63, "a" );
}
