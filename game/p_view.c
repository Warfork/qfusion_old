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



static	edict_t		*current_player;
static	gclient_t	*current_client;

trace_t pmtrace;

static	vec3_t	forward, right, up;
float	xyspeed;

float	bobmove;
int		bobcycle;		// odd cycles are right foot going forward
float	bobfracsin;		// sin(bobfrac*M_PI)

/*
===============
SV_CalcRoll

===============
*/
float SV_CalcRoll (vec3_t velocity)
{
	float	sign;
	float	side;
	float	value;
	
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);
	
	value = sv_rollangle->value;

	if (side < sv_rollspeed->value)
		side = side * value / sv_rollspeed->value;
	else
		side = value;
	
	return side*sign;
	
}

/*
===============
P_AddViewBlend
===============
*/
void P_AddViewBlend (gclient_t *client, float r, float g, float b, float a, float time)
{
	int i, blendnum;
	float besttime;

	blendnum = 0;
	besttime = client->blends[0].endtime;

	// pick effect which is about to end first or has already ended
	for (i = 1; i < MAX_CLIENT_VIEW_BLENDS && (besttime >= level.time); i++)
	{
		if (client->blends[i].endtime < besttime)
		{
			blendnum = i;
			besttime = client->blends[i].endtime;
		}
	}

	client->blends[blendnum].endtime = level.time + time;
	client->blends[blendnum].time = time;
	client->blends[blendnum].color[0] = r;
	client->blends[blendnum].color[1] = g;
	client->blends[blendnum].color[2] = b;
	client->blends[blendnum].color[3] = bound (0.0, a, 0.6);
}

/*
===============
P_AddWeaponKick
===============
*/
void P_AddWeaponKick (gclient_t *client, vec3_t offset, vec3_t angles, float time)
{
	int i, kicknum;
	float besttime;

	kicknum = 0;
	besttime = client->kicks[0].endtime;

	// pick effect which is about to end first or has already ended
	for (i = 1; i < MAX_CLIENT_WEAPON_KICKS && (besttime >= level.time); i++)
	{
		if (client->kicks[i].endtime < besttime)
		{
			kicknum = i;
			besttime = client->kicks[i].endtime;
		}
	}

	client->kicks[kicknum].endtime = level.time + time;
	client->kicks[kicknum].time = time;
	VectorCopy (offset, client->kicks[kicknum].origin);
	VectorCopy (angles, client->kicks[kicknum].angles);
}

/*
===============
P_DamageFeedback

Handles color blends and view kicks
===============
*/
void P_DamageFeedback (edict_t *player)
{
	gclient_t	*client;
	float	side;
	float	realcount, count, kick;
	float	color_alpha;
	vec3_t	v;
	static	vec3_t	pcolor = {0.0, 1.0, 0.0};
	static	vec3_t	acolor = {1.0, 1.0, 1.0};
	static	vec3_t	bcolor = {1.0, 0.0, 0.0};

	client = player->r.client;

	// flash the backgrounds behind the status numbers
	client->ps.stats[STAT_FLASHES] = 0;
	if (client->damage_blood)
		client->ps.stats[STAT_FLASHES] |= 1;
	if (client->damage_armor && !(player->flags & FL_GODMODE) && (client->invincible_timeout <= level.time))
		client->ps.stats[STAT_FLASHES] |= 2;

	// total points of damage shot at the player this frame
	count = (client->damage_blood + client->damage_armor + client->damage_parmor);
	if (count == 0)
		return;		// didn't take any damage

	realcount = count;
	if (count < 10)
		count = 10;	// always make a visible effect

	// play an apropriate pain sound
	if ((level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) && (client->invincible_timeout <= level.time))
	{
		player->pain_debounce_time = level.time + 0.7;

		if (player->health > 0)
		{
			if (player->health < 25)
				G_AddEvent (player, EV_PAIN, PAIN_25, qtrue);
			else if (player->health < 50)
				G_AddEvent (player, EV_PAIN, PAIN_50, qtrue);
			else if (player->health < 75)
				G_AddEvent (player, EV_PAIN, PAIN_75, qtrue);
			else
				G_AddEvent (player, EV_PAIN, PAIN_100, qtrue);
		}
	}

	//
	// create view blend effects from damage taken
	//
	if (client->damage_parmor)
	{
		color_alpha = client->damage_parmor * 0.005f;
		P_AddViewBlend (client, pcolor[0], pcolor[1], pcolor[2], bound (0.05f, color_alpha, 0.25f), 0.2);
	}
	if (client->damage_armor)
	{
		color_alpha = client->damage_armor * 0.005f;
		P_AddViewBlend (client, acolor[0], acolor[1], acolor[2], bound (0.05f, color_alpha, 0.25f), 0.2);
	}
	if (client->damage_blood)
	{
		color_alpha = client->damage_blood * 0.005f;
		P_AddViewBlend (client, bcolor[0], bcolor[1], bcolor[2], bound (0.05f, color_alpha, 0.25f), 0.2);
	}

	//
	// calculate view angle kicks
	//
	kick = abs(client->damage_knockback);
	if (kick && player->health > 0)	// kick of 0 means no view adjust at all
	{
		kick = kick * 100 / player->health;

		if (kick < count*0.5)
			kick = count*0.5;
		if (kick > 50)
			kick = 50;

		VectorSubtract (client->damage_from, player->s.origin, v);
		VectorNormalize (v);
		
		side = DotProduct (v, right);
		client->v_dmg_roll = kick*side*0.3;
		
		side = -DotProduct (v, forward);
		client->v_dmg_pitch = kick*side*0.3;

		client->v_dmg_time = level.time + DAMAGE_TIME;
	}

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_parmor = 0;
	client->damage_knockback = 0;
}

/*
===============
SV_CalcViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 = 

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
void SV_CalcViewOffset (edict_t *ent)
{
	int			i;
	float		bob;
	float		ratio;
	float		delta;
	vec3_t		viewangles, viewoffset;
	gclient_t	*client = ent->r.client;

//===================================

	// if dead, fix the angle and don't add any kick
	if (ent->deadflag)
	{
		client->ps.viewangles[ROLL] = 40;
		client->ps.viewangles[PITCH] = -15;
		client->ps.viewangles[YAW] = client->killer_yaw;
	}
	else
	{
		VectorClear (viewangles);
		VectorClear (viewoffset);

		//
		// add view height
		//
		viewoffset[2] = ent->viewheight;

		//
		// add angles based on weapon kicks
		//
		for (i = 0; i < MAX_CLIENT_WEAPON_KICKS; i++)
		{
			if (client->kicks[i].endtime > level.time)
			{
				ratio = (client->kicks[i].endtime - level.time) / client->kicks[i].time;
				ratio = sin (DEG2RAD (ratio*180));
				clamp (ratio, 0, 1);

				VectorMA (viewangles, ratio, client->kicks[i].angles, viewangles);
				VectorMA (viewoffset, client->kicks[i].origin[0] * ratio, forward, viewoffset);
				VectorMA (viewoffset, client->kicks[i].origin[1] * ratio, right, viewoffset);
				VectorMA (viewoffset, client->kicks[i].origin[2] * ratio, up, viewoffset);
			}
		}

		//
		// add angles based on damage kick
		//
		if (client->v_dmg_time > level.time)
		{
			ratio = (client->v_dmg_time - level.time) / DAMAGE_TIME;
			ratio = sin (DEG2RAD (ratio*180));
			clamp (ratio, 0, 1);

			viewangles[PITCH] += ratio * client->v_dmg_pitch;
			viewangles[ROLL] += ratio * client->v_dmg_roll;
		}
		else
		{
			client->v_dmg_pitch = 0;
			client->v_dmg_roll = 0;
		}

		//
		// add pitch based on fall kick
		//
		if (client->fall_time > level.time)
		{
			float fval = bound (0, client->fall_value, 30);

			ratio = (client->fall_time - level.time) / FALL_TIME;
			ratio = sin (DEG2RAD (ratio*180));
			clamp (ratio, 0, 1);

			viewangles[PITCH] += ratio * fval;

			fval = 12 * (fval / 30); // scale for height offset
			viewoffset[2] -= ratio * fval;
		}

		// add angles based on velocity
		delta = DotProduct (ent->velocity, forward);
		viewangles[PITCH] += delta*run_pitch->value;
		
		delta = DotProduct (ent->velocity, right);
		viewangles[ROLL] += delta*run_roll->value;

		// add angles based on bob
		delta = bobfracsin * bob_pitch->value * xyspeed;
		if (client->ps.pmove.pm_flags & PMF_DUCKED)
			delta *= 6;		// crouching
		viewangles[PITCH] += delta;
		delta = bobfracsin * bob_roll->value * xyspeed;
		if (client->ps.pmove.pm_flags & PMF_DUCKED)
			delta *= 6;		// crouching
		if (bobcycle & 1)
			delta = -delta;
		viewangles[ROLL] += delta;

		// add bob height
		bob = bobfracsin * xyspeed * bob_up->value;
		if (bob > 6)
			bob = 6;
		viewoffset[2] += bob;

		// absolutely bound offsets
		// so the view can never be outside the player box
		VectorSet (client->ps.kick_angles, bound (-45, viewangles[0], 45), bound (-45, viewangles[1], 45), bound (-45, viewangles[2], 45));
		VectorSet (client->ps.viewoffset, bound (-14, viewoffset[0], 14), bound (-14, viewoffset[1], 14), bound (-22, viewoffset[2], 30));
	}
}

/*
=============
SV_AddBlend
=============
*/
void SV_AddBlend (float r, float g, float b, float a, float *v_blend)
{
	float	a2, a3;

	if (a <= 0)
		return;
	a2 = v_blend[3] + (1-v_blend[3])*a;	// new total alpha
	a3 = v_blend[3]/a2;		// fraction of color from old

	v_blend[0] = v_blend[0]*a3 + r*(1-a3);
	v_blend[1] = v_blend[1]*a3 + g*(1-a3);
	v_blend[2] = v_blend[2]*a3 + b*(1-a3);
	v_blend[3] = bound (0, a2, 0.6);
}


/*
=============
SV_CalcBlend
=============
*/
void SV_CalcBlend (edict_t *ent)
{
	int			i;
	int			contents;
	vec3_t		vieworg;
	float		ratio, remaining;
	gclient_t	*client = ent->r.client;

	client->ps.blend[0] = client->ps.blend[1] = 
		client->ps.blend[2] = client->ps.blend[3] = 0;

	//
	// constant blend effects
	//

	// add for contents
	VectorAdd (ent->s.origin, client->ps.viewoffset, vieworg);
	contents = trap_PointContents (vieworg);

	if (contents & CONTENTS_LAVA)
		SV_AddBlend (1.0, 0.3, 0.0, 0.6, client->ps.blend);
	else if (contents & CONTENTS_SLIME)
		SV_AddBlend (0.0, 0.1, 0.05, 0.6, client->ps.blend);

	// add for powerups
	if (client->quad_timeout > level.time)
	{
		remaining = client->quad_timeout - level.time;
		if (remaining == 3)		// beginning to fade
			G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/damage2.wav"), 1, ATTN_NORM);
		if (remaining > 3 || ((int)(remaining*10) & 4) )
			SV_AddBlend (0, 0, 1, 0.08, client->ps.blend);
	}
	else if (client->invincible_timeout > level.time)
	{
		remaining = client->invincible_timeout - level.time;
		if (remaining == 3)		// beginning to fade
			G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/protect2.wav"), 1, ATTN_NORM);
		if (remaining > 3 || ((int)(remaining*10) & 4) )
			SV_AddBlend (1, 1, 0, 0.08, client->ps.blend);
	}
	else if (client->enviro_timeout > level.time)
	{
		remaining = client->enviro_timeout - level.time;
		if (remaining == 3)		// beginning to fade
			G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/airout.wav"), 1, ATTN_NORM);
		if (remaining > 30 || ((int)(remaining*10) & 4) )
			SV_AddBlend (0, 1, 0, 0.08, client->ps.blend);
	}
	else if (client->breather_timeout > level.time)
	{
		remaining = client->breather_timeout - level.time;
		if (remaining == 3)		// beginning to fade
			G_Sound (ent, CHAN_ITEM, trap_SoundIndex("sound/items/airout.wav"), 1, ATTN_NORM);
		if (remaining > 3 || ((int)(remaining*10) & 4) )
			SV_AddBlend (0.4, 1, 0.4, 0.04, client->ps.blend);
	}

	//
	// timed blend effects
	//
	for (i = 0; i < MAX_CLIENT_VIEW_BLENDS; i++)
	{
		if (client->blends[i].endtime > level.time)
		{
			ratio = (client->blends[i].endtime - level.time) / client->blends[i].time;
			ratio = cos (DEG2RAD ((1 - ratio)*90));
			clamp (ratio, 0, 1);

			SV_AddBlend (client->blends[i].color[0], client->blends[i].color[1], client->blends[i].color[2], client->blends[i].color[3] * ratio, client->ps.blend);
		}
	}
}

/*
=================
P_FootstepEvent

Footsteps have low priority, so do not override other events
=================
*/
static void P_FootstepEvent ( edict_t *ent )
{
	if (pmtrace.surfFlags & SURF_NOSTEPS)
		return;

	if (pmtrace.surfFlags & SURF_METALSTEPS)
		G_AddEvent (ent, EV_FOOTSTEP, FOOTSTEP_METAL, qfalse);
	else
		G_AddEvent (ent, EV_FOOTSTEP, FOOTSTEP_NORMAL, qfalse);
}

/*
=================
P_FallingDamage
=================
*/
void P_FallingDamage (edict_t *ent)
{
	float	delta;
	int		damage;
	vec3_t	dir;
	gclient_t *client = ent->r.client;

	if (ent->s.modelindex != 255)
		return;		// not in the player model
 	if (ent->movetype == MOVETYPE_NOCLIP)
		return;

	if ((client->oldvelocity[2] < 0) && (ent->velocity[2] > client->oldvelocity[2]) && !ent->groundentity)
	{
		delta = client->oldvelocity[2];
	}
	else
	{
		if (!ent->groundentity)
			return;
		delta = ent->velocity[2] - client->oldvelocity[2];
	}
	delta = delta * delta * 0.0001;

//ZOID
	// never take damage if just release grapple or on grapple
	if (level.time - client->ctf_grapplereleasetime <= FRAMETIME * 2 ||
		(client->ctf_grapple && 
		client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY))
		return;
//ZOID

	// scale delta if was pushed by jump pad
	if (client->jumppad_time && client->jumppad_time < level.time) 
	{
		delta /= (1 + level.time - client->jumppad_time) * 0.5;
		client->jumppad_time = 0;
	}

	// never take falling damage if completely underwater
	if (ent->waterlevel == 3)
		return;
	if (ent->waterlevel == 2)
		delta *= 0.25;
	if (ent->waterlevel == 1)
		delta *= 0.5;

	if (delta < 1)
		return;

	if (delta < 15)
	{
		P_FootstepEvent (ent);
		return;
	}

	// never take damage
	if (pmtrace.surfFlags & SURF_NODAMAGE)
	{
		G_AddEvent (ent, EV_FALL, FALL_SHORT, qfalse);
		return;
	}

	if (delta >= 55)
		client->fall_value = 20;
	else if (delta > 30) 
		client->fall_value = 10;
	else if (delta > 10)
		client->fall_value = 5;
	else
		client->fall_value = 0;
	client->fall_time = level.time + FALL_TIME;

	if (delta > 30)
	{
		ent->pain_debounce_time = level.time;	// no normal pain sound
		damage = (delta-30)/2;
		if (damage < 1)
			damage = 1;
		VectorSet (dir, 0, 0, 1);

		if (!deathmatch->integer || !(dmflags->integer & DF_NO_FALLING) )
			T_Damage (ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0, 0, MOD_FALLING);

		if (ent->health > 0)
		{
			if (delta >= 55)
				G_AddEvent (ent, EV_FALL, FALL_FAR, qtrue);
			else
				G_AddEvent (ent, EV_FALL, FALL_MEDIUM, qtrue);
		}
		else
		{
			G_AddEvent (ent, EV_FALL, FALL_SHORT, qfalse);
		}
	}
	else
	{
		G_AddEvent (ent, EV_FALL, FALL_SHORT, qfalse);
		return;
	}
}

/*
=============
P_WorldEffects
=============
*/
void P_WorldEffects (void)
{
	qboolean	breather;
	qboolean	envirosuit;
	int			waterlevel, old_waterlevel;

	if (current_player->movetype == MOVETYPE_NOCLIP)
	{
		current_player->air_finished = level.time + 12;	// don't need air
		return;
	}

	waterlevel = current_player->waterlevel;
	old_waterlevel = current_client->old_waterlevel;
	current_client->old_waterlevel = waterlevel;

	breather = current_client->breather_timeout > level.time;
	envirosuit = current_client->enviro_timeout > level.time;

	//
	// if just entered a water volume, play a sound
	//
	if (!old_waterlevel && waterlevel)
	{
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		if (current_player->watertype & CONTENTS_LAVA)
			G_Sound (current_player, CHAN_BODY, trap_SoundIndex("sound/player/lava_in.wav"), 1, ATTN_NORM);
		else if (current_player->watertype & CONTENTS_SLIME)
			G_Sound (current_player, CHAN_BODY, trap_SoundIndex("sound/player/watr_in.wav"), 1, ATTN_NORM);
		else if (current_player->watertype & CONTENTS_WATER)
			G_Sound (current_player, CHAN_BODY, trap_SoundIndex("sound/player/watr_in.wav"), 1, ATTN_NORM);
		current_player->flags |= FL_INWATER;

		// clear damage_debounce, so the pain sound will play immediately
		current_player->damage_debounce_time = level.time - 1;
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (old_waterlevel && ! waterlevel)
	{
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		G_Sound (current_player, CHAN_BODY, trap_SoundIndex("sound/player/watr_out.wav"), 1, ATTN_NORM);
		current_player->flags &= ~FL_INWATER;
	}

	//
	// check for head just going under water
	//
	if (old_waterlevel != 3 && waterlevel == 3)
	{
		G_Sound (current_player, CHAN_BODY, trap_SoundIndex("sound/player/watr_un.wav"), 1, ATTN_NORM);
	}

	//
	// check for head just coming out of water
	//
	if (old_waterlevel == 3 && waterlevel != 3)
	{
		if (current_player->air_finished < level.time)
		{	// gasp for air
			G_Sound (current_player, CHAN_VOICE, trap_SoundIndex("sound/player/gasp1.wav"), 1, ATTN_NORM);
			PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		}
		else  if (current_player->air_finished < level.time + 11)
		{	// just break surface
			G_Sound (current_player, CHAN_VOICE, trap_SoundIndex("sound/player/gasp2.wav"), 1, ATTN_NORM);
		}
	}

	//
	// check for drowning
	//
	if (waterlevel == 3)
	{
		// breather or envirosuit give air
		if (breather || envirosuit)
		{
			current_player->air_finished = level.time + 10;

			if ((((int)(current_client->breather_timeout - level.time)*100) % 250) == 0)
			{
				if (!current_client->breather_sound)
					G_Sound (current_player, CHAN_AUTO, trap_SoundIndex("sound/player/u_breath1.wav"), 1, ATTN_NORM);
				else
					G_Sound (current_player, CHAN_AUTO, trap_SoundIndex("sound/player/u_breath2.wav"), 1, ATTN_NORM);

				current_client->breather_sound ^= 1;
				PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);

				// FIXME: release a bubble?
			}
		}

		// if out of air, start drowning
		if (current_player->air_finished < level.time)
		{	// drown!
			if (current_client->next_drown_time < level.time 
				&& current_player->health > 0)
			{
				current_client->next_drown_time = level.time + 1;

				// take more damage the longer underwater
				current_player->dmg += 2;
				if (current_player->dmg > 15)
					current_player->dmg = 15;

				// play a gurp sound instead of a normal pain sound
				if (current_player->health <= current_player->dmg)
					G_Sound (current_player, CHAN_VOICE, trap_SoundIndex("sound/player/drown1.wav"), 1, ATTN_NORM);
				else if (rand()&1)
					G_Sound (current_player, CHAN_VOICE, trap_SoundIndex("*gurp1.wav"), 1, ATTN_NORM);
				else
					G_Sound (current_player, CHAN_VOICE, trap_SoundIndex("*gurp2.wav"), 1, ATTN_NORM);

				current_player->pain_debounce_time = level.time;

				T_Damage (current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, current_player->dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	}
	else
	{
		current_player->air_finished = level.time + 12;
		current_player->dmg = 2;
	}

	//
	// check for sizzle damage
	//
	if (waterlevel && (current_player->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) )
	{
		if (current_player->watertype & CONTENTS_LAVA)
		{
			if (current_player->health > 0
				&& current_player->pain_debounce_time <= level.time
				&& current_client->invincible_timeout < level.time)
			{
				if (rand()&1)
					G_Sound (current_player, CHAN_VOICE, trap_SoundIndex("sound/player/burn1.wav"), 1, ATTN_NORM);
				else
					G_Sound (current_player, CHAN_VOICE, trap_SoundIndex("sound/player/burn2.wav"), 1, ATTN_NORM);
				current_player->pain_debounce_time = level.time + 1;
			}

			if (envirosuit)	// take 1/3 damage with envirosuit
				T_Damage (current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1*waterlevel, 0, 0, MOD_LAVA);
			else
				T_Damage (current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 3*waterlevel, 0, 0, MOD_LAVA);
		}

		if (current_player->watertype & CONTENTS_SLIME)
		{
			if (!envirosuit)
			{	// no damage from slime with envirosuit
				T_Damage (current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1*waterlevel, 0, 0, MOD_SLIME);
			}
		}
	}
}


/*
===============
G_SetClientEffects
===============
*/
void G_SetClientEffects (edict_t *ent)
{
	int			pa_type;
	float		remaining;
	gclient_t	*client = ent->r.client;

	ent->s.effects = 0;
	ent->s.renderfx = 0;

	if (ent->health <= 0 || level.intermissiontime)
		return;

	if (ent->powerarmor_time > level.time)
	{
		pa_type = PowerArmorType (ent);
		if (pa_type == POWER_ARMOR_SCREEN)
		{
			ent->s.effects |= EF_POWERSCREEN;
		}
		else if (pa_type == POWER_ARMOR_SHIELD)
		{
			ent->s.renderfx |= RF_SHELL_GREEN;
		}
	}

//ZOID
	CTFEffects(ent);
//ZOID

	if (client->quad_timeout > level.time)
	{
		remaining = client->quad_timeout - level.time;
		if (remaining > 3 || ((int)(remaining*10) & 4) )
			CTFSetPowerUpEffect(ent, EF_QUAD);
	}

	if (client->invincible_timeout > level.time)
	{
		remaining = client->invincible_timeout - level.time;
		if (remaining > 3 || ((int)(remaining*10) & 4) )
			CTFSetPowerUpEffect(ent, EF_PENT);
	}

	// show cheaters!!!
	if (ent->flags & FL_GODMODE)
	{
		ent->s.renderfx |= (RF_SHELL_RED|RF_SHELL_GREEN|RF_SHELL_BLUE);
	}
}


/*
===============
G_SetClientEvent
===============
*/
void G_SetClientEvent (edict_t *ent)
{
	if ( ent->health < 1 ) {
		return;
	}

	if ( (int)(current_client->bobtime+bobmove) == bobcycle ) {
		return;
	}

	if ( ent->groundentity && (xyspeed > 225) && (ent->waterlevel == 0) ) {
		P_FootstepEvent ( ent );
	} else if ( ent->waterlevel == 1 ) {
		G_AddEvent (ent, EV_FOOTSTEP, FOOTSTEP_SPLASH, qfalse);
	} else if ( ent->waterlevel == 2 ) {
		G_AddEvent (ent, EV_FOOTSTEP, FOOTSTEP_SPLASH, qfalse);
	}
}

/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound (edict_t *ent)
{
	const char	*weap;
	gclient_t	*client = ent->r.client;

	if (client->resp.game_helpchanged != game.helpchanged)
	{
		client->resp.game_helpchanged = game.helpchanged;
		client->resp.helpchanged = 1;
	}

	// help beep (no more than three times)
	if (client->resp.helpchanged && client->resp.helpchanged <= 3 && !(level.framenum&63) )
	{
		client->resp.helpchanged++;
		G_Sound (ent, CHAN_VOICE, trap_SoundIndex ("sound/misc/pc_up.wav"), 1, ATTN_STATIC);
	}

	if (client->pers.weapon)
		weap = client->pers.weapon->classname;
	else
		weap = "";

	if (ent->waterlevel && (ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) )
		ent->s.sound = snd_fry;
	else if (strcmp(weap, "weapon_railgun") == 0)
		ent->s.sound = trap_SoundIndex("sound/weapons/railgun/rg_hum.wav");
	else if (strcmp(weap, "weapon_bfg") == 0)
		ent->s.sound = trap_SoundIndex("sound/weapons/bfg_hum.wav");
	else if (client->weapon_sound)
		ent->s.sound = client->weapon_sound;
	else
		ent->s.sound = 0;
}

/*
=================
ClientEndServerFrame

Called for each player at the end of the server frame
and right after spawning
=================
*/
void ClientEndServerFrame (edict_t *ent)
{
	float	bobtime;
	int		i;
	vec3_t dir;

	current_player = ent;
	current_client = ent->r.client;

	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	// 
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	for (i=0 ; i<3 ; i++)
	{
		current_client->ps.pmove.origin[i] = ent->s.origin[i]*16;
		current_client->ps.pmove.velocity[i] = ent->velocity[i]*16;
	}

	VectorSet ( dir, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 0.25 );

	if ( ent->health > 0 ) {
		if ( ent->groundentity ) {
			trap_Trace ( &pmtrace, ent->s.origin, ent->r.mins, ent->r.maxs, dir, ent, MASK_PLAYERSOLID );
		} else {
			trap_Trace ( &pmtrace, ent->s.origin, ent->r.mins, ent->r.maxs, ent->s.origin, ent, MASK_PLAYERSOLID );
		}
	} else {
		trap_Trace ( &pmtrace, ent->s.origin, ent->r.mins, ent->r.maxs, ent->s.origin, ent, MASK_DEADSOLID );
	}

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if (level.intermissiontime)
	{
		// FIXME: add view drifting here?
		current_client->ps.blend[3] = 0;
		current_client->ps.fov = 90;
		G_SetStats (ent);
		return;
	}

	AngleVectors (current_client->v_angle, forward, right, up);

	// burn from lava, etc
	P_WorldEffects ();

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	ent->s.angles[YAW] = current_client->v_angle[YAW];
	if( ent->deadflag ) {
		ent->s.angles[PITCH] = 0;
		ent->s.angles[ROLL] = 0;
	} else {
		ent->s.angles[PITCH] = current_client->v_angle[PITCH];	
		ent->s.angles[ROLL] = SV_CalcRoll (ent->velocity)*4; // roll is ignored clientside
		//ent->s.angles[ROLL] = 0;
	}

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xyspeed = sqrt(ent->velocity[0]*ent->velocity[0] + ent->velocity[1]*ent->velocity[1]);

	if (xyspeed < 5)
	{
		bobmove = 0;
		current_client->bobtime = 0;	// start at beginning of cycle again
	}
	else if (ent->groundentity)
	{	// so bobbing only cycles when on ground
		if (xyspeed > 210)
			bobmove = 2.5;
		else if (xyspeed > 100)
			bobmove = 1.25;
		else
			bobmove = 0.625;
		bobmove *= FRAMETIME;
	}
	
	bobtime = (current_client->bobtime += bobmove);

	if (current_client->ps.pmove.pm_flags & PMF_DUCKED)
		bobtime *= 4;

	bobcycle = (int)bobtime;
	bobfracsin = fabs(sin(bobtime*M_PI));

	// detect hitting the floor
	P_FallingDamage (ent);

	// apply all the damage taken this frame
	P_DamageFeedback (ent);

	// determine the view offsets
	SV_CalcViewOffset (ent);

	// determine the full screen color blend
	// must be after viewoffset, so eye contents can be
	// accurately determined
	// FIXME: with client prediction, the contents
	// should be determined by the client
	SV_CalcBlend (ent);

//ZOID
	if (!current_client->chase_target)
//ZOID
		G_SetStats (ent);

	G_SetClientEvent (ent);

	G_SetClientEffects (ent);

	G_SetClientSound (ent);

	G_SetClientFrame (ent);

	VectorCopy (ent->velocity, ent->r.client->oldvelocity);
	VectorCopy (ent->r.client->ps.viewangles, ent->r.client->oldviewangles);

	// if the scoreboard is up, update it
	if (ent->r.client->showscores && !(level.framenum & 31) )
	{
		char *s;
//ZOID
		if (ent->r.client->menu) {
			s = PMenu_Do_Update (ent);
			ent->r.client->menudirty = qfalse;
			ent->r.client->menutime = level.time;
		} else
//ZOID
			s = DeathmatchScoreboardMessage (ent, ent->enemy);

		trap_Layout (ent, s);
	}
}

