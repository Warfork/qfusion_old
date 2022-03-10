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
// cl_fx.c -- entity effects parsing and management

#include "client.h"

static vec3_t avelocities [NUMVERTEXNORMALS];

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/

cdlight_t		cl_dlights[MAX_DLIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights (void)
{
	memset (cl_dlights, 0, sizeof(cl_dlights));
}

/*
===============
CL_AllocDlight

===============
*/
cdlight_t *CL_AllocDlight (int key)
{
	int		i;
	cdlight_t	*dl;

// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (dl->key == key)
			{
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

// then look for anything else
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}

/*
==============
CL_ParseMuzzleFlash
==============
*/
void CL_ParseMuzzleFlash (void)
{
	vec3_t		fv, rv;
	cdlight_t	*dl;
	int			i, weapon;
	centity_t	*pl;
	int			silenced;
	float		volume;

	i = MSG_ReadShort (&net_message);
	if (i < 1 || i >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash: bad entity");

	weapon = MSG_ReadByte (&net_message);
	silenced = weapon & MZ_SILENCED;
	weapon &= ~MZ_SILENCED;

	pl = &cl_entities[i];

	dl = CL_AllocDlight (i);
	VectorCopy (pl->current.origin, dl->origin);
	AngleVectors (pl->current.angles, fv, rv, NULL);
	VectorMA (dl->origin, 18, fv, dl->origin);
	VectorMA (dl->origin, 16, rv, dl->origin);
	if (silenced)
		dl->radius = 100 + (rand()&31);
	else
		dl->radius = 200 + (rand()&31);
	dl->die = cl.time; // + 100;

	if (silenced)
		volume = 0.2;
	else
		volume = 1;

	switch (weapon)
	{
		case MZ_BLASTER:
			dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound("sound/weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
			break;
		case MZ_HYPERBLASTER:
			dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, cl.media.sfxHyperblasterSplash, volume, ATTN_NORM, 0);
			break;
		case MZ_MACHINEGUN:
			dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, cl.media.sfxMachinegunSplashes[rand()%4], volume, ATTN_NORM, 0);
			break;
		case MZ_SHOTGUN:
			dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound("sound/weapons/shotgf1b.wav"), volume, ATTN_NORM, 0);
			S_StartSound (NULL, i, CHAN_AUTO,   S_RegisterSound("sound/weapons/shotgr1b.wav"), volume, ATTN_NORM, 0.1);
			break;
		case MZ_SSHOTGUN:
			dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound("sound/weapons/sshotf1b.wav"), volume, ATTN_NORM, 0);
			break;
		case MZ_CHAINGUN1:
			dl->radius = 200 + (rand()&31);
			dl->color[0] = 1;dl->color[1] = 0.25;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, cl.media.sfxMachinegunSplashes[rand()%4], volume, ATTN_NORM, 0);
			break;
		case MZ_CHAINGUN2:
			dl->radius = 225 + (rand()&31);
			dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, cl.media.sfxMachinegunSplashes[rand()%4], volume, ATTN_NORM, 0);
			S_StartSound (NULL, i, CHAN_WEAPON, cl.media.sfxMachinegunSplashes[rand()%4], volume, ATTN_NORM, 0.05);
			break;
		case MZ_CHAINGUN3:
			dl->radius = 250 + (rand()&31);
			dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, cl.media.sfxMachinegunSplashes[rand()%4], volume, ATTN_NORM, 0);
			S_StartSound (NULL, i, CHAN_WEAPON, cl.media.sfxMachinegunSplashes[rand()%4], volume, ATTN_NORM, 0.033);
			S_StartSound (NULL, i, CHAN_WEAPON, cl.media.sfxMachinegunSplashes[rand()%4], volume, ATTN_NORM, 0.066);
			break;
		case MZ_RAILGUN:
			dl->color[0] = 0.5;dl->color[1] = 0.5;dl->color[2] = 1.0;
			S_StartSound (NULL, i, CHAN_WEAPON, cl.media.sfxRailg, volume, ATTN_NORM, 0);
			break;
		case MZ_ROCKET:
			dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0.2;
			S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound("sound/weapons/rocket/rocklf1a.wav"), volume, ATTN_NORM, 0);
			break;
		case MZ_GRENADE:
			dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound("sound/weapons/grenade/grenlf1a.wav"), volume, ATTN_NORM, 0);
			break;
		case MZ_BFG:
			dl->color[0] = 0;dl->color[1] = 1;dl->color[2] = 0;
			S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound("sound/weapons/bfg__f1y.wav"), volume, ATTN_NORM, 0);
			break;
	}
}


/*
==============
CL_ParseMuzzleFlash2
==============
*/
void CL_ParseMuzzleFlash2 (void) 
{
	int			ent;
	vec3_t		origin;
	int			flash_number;
	cdlight_t	*dl;
	vec3_t		forward, right;
	char		soundname[64];

	ent = MSG_ReadShort (&net_message);
	if (ent < 1 || ent >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash2: bad entity");

	flash_number = MSG_ReadByte (&net_message);

	// locate the origin
	AngleVectors (cl_entities[ent].current.angles, forward, right, NULL);
	origin[0] = cl_entities[ent].current.origin[0] + forward[0] * monster_flash_offset[flash_number][0] + right[0] * monster_flash_offset[flash_number][1];
	origin[1] = cl_entities[ent].current.origin[1] + forward[1] * monster_flash_offset[flash_number][0] + right[1] * monster_flash_offset[flash_number][1];
	origin[2] = cl_entities[ent].current.origin[2] + forward[2] * monster_flash_offset[flash_number][0] + right[2] * monster_flash_offset[flash_number][1] + monster_flash_offset[flash_number][2];

	dl = CL_AllocDlight (ent);
	VectorCopy (origin,  dl->origin);
	dl->radius = 200 + (rand()&31);
	dl->die = cl.time;	// + 100;

	switch (flash_number)
	{
	case MZ2_INFANTRY_MACHINEGUN_1:
	case MZ2_INFANTRY_MACHINEGUN_2:
	case MZ2_INFANTRY_MACHINEGUN_3:
	case MZ2_INFANTRY_MACHINEGUN_4:
	case MZ2_INFANTRY_MACHINEGUN_5:
	case MZ2_INFANTRY_MACHINEGUN_6:
	case MZ2_INFANTRY_MACHINEGUN_7:
	case MZ2_INFANTRY_MACHINEGUN_8:
	case MZ2_INFANTRY_MACHINEGUN_9:
	case MZ2_INFANTRY_MACHINEGUN_10:
	case MZ2_INFANTRY_MACHINEGUN_11:
	case MZ2_INFANTRY_MACHINEGUN_12:
	case MZ2_INFANTRY_MACHINEGUN_13:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 0, 0, 40);
		CL_SmokeAndFlash(origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SOLDIER_MACHINEGUN_1:
	case MZ2_SOLDIER_MACHINEGUN_2:
	case MZ2_SOLDIER_MACHINEGUN_3:
	case MZ2_SOLDIER_MACHINEGUN_4:
	case MZ2_SOLDIER_MACHINEGUN_5:
	case MZ2_SOLDIER_MACHINEGUN_6:
	case MZ2_SOLDIER_MACHINEGUN_7:
	case MZ2_SOLDIER_MACHINEGUN_8:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 0, 0, 40);
		CL_SmokeAndFlash(origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/soldier/solatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GUNNER_MACHINEGUN_1:
	case MZ2_GUNNER_MACHINEGUN_2:
	case MZ2_GUNNER_MACHINEGUN_3:
	case MZ2_GUNNER_MACHINEGUN_4:
	case MZ2_GUNNER_MACHINEGUN_5:
	case MZ2_GUNNER_MACHINEGUN_6:
	case MZ2_GUNNER_MACHINEGUN_7:
	case MZ2_GUNNER_MACHINEGUN_8:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 0, 0, 40);
		CL_SmokeAndFlash(origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/gunner/gunatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_ACTOR_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_2:
	case MZ2_SUPERTANK_MACHINEGUN_3:
	case MZ2_SUPERTANK_MACHINEGUN_4:
	case MZ2_SUPERTANK_MACHINEGUN_5:
	case MZ2_SUPERTANK_MACHINEGUN_6:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 0, 0, 40);
		CL_SmokeAndFlash(origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_BOSS2_MACHINEGUN_L1:
	case MZ2_BOSS2_MACHINEGUN_L2:
	case MZ2_BOSS2_MACHINEGUN_L3:
	case MZ2_BOSS2_MACHINEGUN_L4:
	case MZ2_BOSS2_MACHINEGUN_L5:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 0, 0, 40);
		CL_SmokeAndFlash(origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/infantry/infatck1.wav"), 1, ATTN_NONE, 0);
		break;

	case MZ2_SOLDIER_BLASTER_1:
	case MZ2_SOLDIER_BLASTER_2:
	case MZ2_SOLDIER_BLASTER_3:
	case MZ2_SOLDIER_BLASTER_4:
	case MZ2_SOLDIER_BLASTER_5:
	case MZ2_SOLDIER_BLASTER_6:
	case MZ2_SOLDIER_BLASTER_7:
	case MZ2_SOLDIER_BLASTER_8:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/soldier/solatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_FLYER_BLASTER_1:
	case MZ2_FLYER_BLASTER_2:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/flyer/flyatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_MEDIC_BLASTER_1:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/medic/medatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_HOVER_BLASTER_1:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/hover/hovatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_FLOAT_BLASTER_1:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/floater/fltatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SOLDIER_SHOTGUN_1:
	case MZ2_SOLDIER_SHOTGUN_2:
	case MZ2_SOLDIER_SHOTGUN_3:
	case MZ2_SOLDIER_SHOTGUN_4:
	case MZ2_SOLDIER_SHOTGUN_5:
	case MZ2_SOLDIER_SHOTGUN_6:
	case MZ2_SOLDIER_SHOTGUN_7:
	case MZ2_SOLDIER_SHOTGUN_8:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_SmokeAndFlash(origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/soldier/solatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_BLASTER_1:
	case MZ2_TANK_BLASTER_2:
	case MZ2_TANK_BLASTER_3:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_MACHINEGUN_1:
	case MZ2_TANK_MACHINEGUN_2:
	case MZ2_TANK_MACHINEGUN_3:
	case MZ2_TANK_MACHINEGUN_4:
	case MZ2_TANK_MACHINEGUN_5:
	case MZ2_TANK_MACHINEGUN_6:
	case MZ2_TANK_MACHINEGUN_7:
	case MZ2_TANK_MACHINEGUN_8:
	case MZ2_TANK_MACHINEGUN_9:
	case MZ2_TANK_MACHINEGUN_10:
	case MZ2_TANK_MACHINEGUN_11:
	case MZ2_TANK_MACHINEGUN_12:
	case MZ2_TANK_MACHINEGUN_13:
	case MZ2_TANK_MACHINEGUN_14:
	case MZ2_TANK_MACHINEGUN_15:
	case MZ2_TANK_MACHINEGUN_16:
	case MZ2_TANK_MACHINEGUN_17:
	case MZ2_TANK_MACHINEGUN_18:
	case MZ2_TANK_MACHINEGUN_19:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 0, 0, 40);
		CL_SmokeAndFlash(origin);
		Com_sprintf(soundname, sizeof(soundname), "sound/tank/tnkatk2%c.wav", 'a' + rand() % 5);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound(soundname), 1, ATTN_NORM, 0);
		break;

	case MZ2_CHICK_ROCKET_1:
		dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0.2;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/chick/chkatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_ROCKET_1:
	case MZ2_TANK_ROCKET_2:
	case MZ2_TANK_ROCKET_3:
		dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0.2;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/tank/tnkatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SUPERTANK_ROCKET_1:
	case MZ2_SUPERTANK_ROCKET_2:
	case MZ2_SUPERTANK_ROCKET_3:
	case MZ2_BOSS2_ROCKET_1:
	case MZ2_BOSS2_ROCKET_2:
	case MZ2_BOSS2_ROCKET_3:
	case MZ2_BOSS2_ROCKET_4:
		dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0.2;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/tank/rocket.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GUNNER_GRENADE_1:
	case MZ2_GUNNER_GRENADE_2:
	case MZ2_GUNNER_GRENADE_3:
	case MZ2_GUNNER_GRENADE_4:
		dl->color[0] = 1;dl->color[1] = 0.5;dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/gunner/gunatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GLADIATOR_RAILGUN_1:
		dl->color[0] = 0.5;dl->color[1] = 0.5;dl->color[2] = 1.0;
		break;

// --- Xian's shit starts ---
	case MZ2_MAKRON_BFG:
		dl->color[0] = 0.5;dl->color[1] = 1 ;dl->color[2] = 0.5;
		break;

	case MZ2_MAKRON_BLASTER_1:
	case MZ2_MAKRON_BLASTER_2:
	case MZ2_MAKRON_BLASTER_3:
	case MZ2_MAKRON_BLASTER_4:
	case MZ2_MAKRON_BLASTER_5:
	case MZ2_MAKRON_BLASTER_6:
	case MZ2_MAKRON_BLASTER_7:
	case MZ2_MAKRON_BLASTER_8:
	case MZ2_MAKRON_BLASTER_9:
	case MZ2_MAKRON_BLASTER_10:
	case MZ2_MAKRON_BLASTER_11:
	case MZ2_MAKRON_BLASTER_12:
	case MZ2_MAKRON_BLASTER_13:
	case MZ2_MAKRON_BLASTER_14:
	case MZ2_MAKRON_BLASTER_15:
	case MZ2_MAKRON_BLASTER_16:
	case MZ2_MAKRON_BLASTER_17:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/makron/blaster.wav"), 1, ATTN_NORM, 0);
		break;
	
	case MZ2_JORG_MACHINEGUN_L1:
	case MZ2_JORG_MACHINEGUN_L2:
	case MZ2_JORG_MACHINEGUN_L3:
	case MZ2_JORG_MACHINEGUN_L4:
	case MZ2_JORG_MACHINEGUN_L5:
	case MZ2_JORG_MACHINEGUN_L6:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 0, 0, 40);
		CL_SmokeAndFlash(origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("sound/boss3/xfire.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_JORG_MACHINEGUN_R1:
	case MZ2_JORG_MACHINEGUN_R2:
	case MZ2_JORG_MACHINEGUN_R3:
	case MZ2_JORG_MACHINEGUN_R4:
	case MZ2_JORG_MACHINEGUN_R5:
	case MZ2_JORG_MACHINEGUN_R6:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 0, 0, 40);
		CL_SmokeAndFlash(origin);
		break;

	case MZ2_JORG_BFG_1:
		dl->color[0] = 0.5;dl->color[1] = 1 ;dl->color[2] = 0.5;
		break;

	case MZ2_BOSS2_MACHINEGUN_R1:
	case MZ2_BOSS2_MACHINEGUN_R2:
	case MZ2_BOSS2_MACHINEGUN_R3:
	case MZ2_BOSS2_MACHINEGUN_R4:
	case MZ2_BOSS2_MACHINEGUN_R5:
		dl->color[0] = 1;dl->color[1] = 1;dl->color[2] = 0;
		CL_ParticleEffect (origin, vec3_origin, 0, 0, 0, 40);
		CL_SmokeAndFlash(origin);
		break;

// --- Xian's shit ends ---

	}
}


/*
===============
CL_AddDLights

===============
*/
void CL_AddDLights (void)
{
	int			i;
	cdlight_t	*dl;

	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (!dl->radius || dl->die < cl.time)
			continue;
		V_AddLight (dl->origin, dl->radius,
			dl->color[0], dl->color[1], dl->color[2]);
	}
}



/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/

cparticle_t	particles[MAX_PARTICLES], *free_particles[MAX_PARTICLES];
int			cl_numparticles;


/*
===============
CL_ClearParticles
===============
*/
void CL_ClearParticles (void)
{
	cl_numparticles = 0;

	memset ( particles, 0, sizeof(cparticle_t) * MAX_PARTICLES );
	memset ( free_particles, 0, sizeof(cparticle_t *) * MAX_PARTICLES );
}

inline cparticle_t *new_particle (void)
{
	cparticle_t	*p;

	if ( cl_numparticles >= MAX_PARTICLES ) {
		return NULL;
	}

	p = &particles[cl_numparticles++];
	p->time = cl.time;
	p->scale = 1.0f;
	p->alpha = 1.0f;
	VectorSet ( p->color, 0, 0, 0 );

	return p;
}

/*
===============
CL_ParticleEffect

Wall impact puffs
===============
*/
void CL_ParticleEffect (vec3_t org, vec3_t dir, float r, float g, float b, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i=0 ; i<count ; i++)
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		p->color[0] = r + frand()*0.1;
		p->color[1] = g + frand()*0.1;
		p->color[2] = b + frand()*0.1;

		d = rand()&31;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()&7) - 4) + d * dir[j];
			p->vel[j] = crand() * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / (0.5 + frand() * 0.3);
	}
}


/*
===============
CL_ParticleEffect2
===============
*/
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, float r, float g, float b, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i=0 ; i<count ; i++)
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		p->color[0] = r;
		p->color[1] = g;
		p->color[2] = b;

		d = rand()&7;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()&7) - 4) + d * dir[j];
			p->vel[j] = crand() * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / (0.5 + frand() * 0.3);
	}
}

/*
===============
CL_BigTeleportParticles
===============
*/
void CL_BigTeleportParticles (vec3_t org)
{
	int			i;
	cparticle_t	*p;
	float		angle, dist;
	static float colortable[4] = {0.0625, 0.40625, 0.65625, 0.5625};

	for ( i = 0; i < 4096; i++ )
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		p->color[0] = colortable[rand()&3];

		angle = M_TWOPI * (rand()&1023)/1023.0;
		dist = rand() & 31;

		p->org[0] = org[0] + cos(angle) * dist;
		p->vel[0] = cos(angle) * (70 + (rand() & 63));
		p->accel[0] = -cos(angle) * 100;

		p->org[1] = org[1] + sin(angle) * dist;
		p->vel[1] = sin(angle) * (70 + (rand() & 63));
		p->accel[1] = -sin(angle) * 100;

		p->org[2] = org[2] + 8 + (rand()%90);
		p->vel[2] = -100 + (rand() & 31);
		p->accel[2] = PARTICLE_GRAVITY * 4;

		p->alphavel = -0.3 / (0.5 + frand() * 0.3);
	}
}


/*
===============
CL_BlasterParticles

Wall impact puffs
===============
*/
void CL_BlasterParticles (vec3_t org, vec3_t dir)
{
	int			i, j;
	cparticle_t	*p;
	float		d;
	int			count;

	count = 40;
	for ( i = 0; i < count; i++ )
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		p->scale = 1.5f;
		p->color[0] = 1.0f;
		p->color[1] = 0.8f;

		d = rand() & 15;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()&7) - 4) + d * dir[j];
			p->vel[j] = dir[j] * 30 + crand() * 40;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / (0.5 + frand() * 0.3);
	}
}

/*
===============
CL_BlasterTrail
===============
*/
void CL_BlasterTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, dec, vec);

	while (len > 0)
	{
		len -= dec;

		if ( !(p = new_particle ()) ) {
			return;
		}
		VectorClear (p->accel);
		
		p->alphavel = -1.0 / (0.3 + frand() * 0.2);
		p->color[0] = 1.0f;
		p->color[1] = 0.7f;
		p->scale = 2.0f;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crand();
			p->vel[j] = crand() * 5;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_FlagTrail
===============
*/
void CL_FlagTrail (vec3_t start, vec3_t end, int effect)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;
	vec3_t		color;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, dec, vec);

	if ( effect == EF_FLAG1 ) {
		VectorSet ( color, 0.8f, 0.1f, 0.1f );
	} else if ( effect == EF_FLAG2 ) {
		VectorSet ( color, 0.1f, 0.1f, 0.8f );
	} else {
		VectorSet ( color, 1.0f, 1.0f, 1.0f );
	}

	while (len > 0)
	{
		len -= dec;

		if ( !(p = new_particle ()) ) {
			return;
		}

		VectorCopy (color, p->color);
		
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crand() * 16;
			p->vel[j] = crand() * 5;
			p->accel[j] = 0;
		}

		p->alphavel = -1.0 / (0.8 + frand() * 0.2);

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_DiminishingTrail
===============
*/
void CL_BloodTrail (vec3_t start, vec3_t end, int *trailcount)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;
	float		orgscale;
	float		velscale;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 0.5;
	VectorScale (vec, dec, vec);

	if (*trailcount > 900)
	{
		orgscale = 4;
		velscale = 15;
	}
	else if (*trailcount > 800)
	{
		orgscale = 2;
		velscale = 10;
	}
	else
	{
		orgscale = 1;
		velscale = 5;
	}

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		// drop less particles as it flies
		if ((rand()&1023) < *trailcount)
		{
			if ( !(p = new_particle ()) ) {
				return;
			}

			p->scale = 2.0f;

			p->color[0] = 0.9 + crand() * 0.5;

			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = move[j] + crand() * orgscale;
				p->vel[j] = crand() * velscale;
				p->accel[j] = 0;
			}

			p->alphavel = -1.0 / (1 + frand() * 0.4);
			p->vel[2] -= PARTICLE_GRAVITY;
		}

		*trailcount -= 5;
		if (*trailcount < 100)
			*trailcount = 100;

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_RailTrail
===============
*/
void CL_RailTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;
	vec3_t		right, up;
	int			i;
	float		d, c, s;
	vec3_t		dir;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	MakeNormalVectors (vec, right, up);

	for ( i = 0; i < len ; i++ )
	{
		if ( !(p = new_particle ()) ) {
			return;
		}
		
		VectorClear (p->accel);

		d = i * 0.1;
		c = cos (d);
		s = sin (d);

		VectorScale (right, c, dir);
		VectorMA (dir, s, up, dir);

		p->color[0] = 0.2;
		p->color[1] = 0.2;
		p->color[2] = 0.8 + crand()*0.1;

		p->scale = 1.8f;

		p->alphavel = -1.0 / (1+frand()*0.2);
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + dir[j]*3;
			p->vel[j] = dir[j]*6;
		}

		VectorAdd (move, vec, move);
	}

	dec = 1.5;
	VectorScale (vec, dec, vec);
	VectorCopy (start, move);

	while (len > 0)
	{
		len -= dec;

		if ( !(p = new_particle ()) ) {
			return;
		}

		VectorClear (p->accel);

		p->scale = 1.25f;

		p->alphavel = -1.0 / (0.6+frand()*0.2);
		p->color[0] = 0.8f + crand()*0.1;
		p->color[1] = 0.8f + crand()*0.1;
		p->color[2] = 0.8f + crand()*0.1;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j];
			p->vel[j] = crand()*3;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);
	}
}


#define	BEAMLENGTH			16

/*
===============
CL_FlyParticles
===============
*/
void CL_FlyParticles (vec3_t origin, int count)
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist;
	float		ltime;

	if (count > NUMVERTEXNORMALS)
		count = NUMVERTEXNORMALS;

	if ( !avelocities[0][0] ) {
		for ( i = 0; i < NUMVERTEXNORMALS*3; i++ ) {
			avelocities[0][i] = (rand()&255) * 0.01;
		}
	}

	ltime = (float)cl.time / 1000.0;
	for ( i = 0; i < count; i += 2 )
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		angle = ltime * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = ltime * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = ltime * avelocities[i][2];
		sr = sin(angle);
		cr = cos(angle);
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		dist = sin (ltime + i) * 64;
		p->org[0] = origin[0] + bytedirs[i][0]*dist + forward[0]*BEAMLENGTH;
		p->org[1] = origin[1] + bytedirs[i][1]*dist + forward[1]*BEAMLENGTH;
		p->org[2] = origin[2] + bytedirs[i][2]*dist + forward[2]*BEAMLENGTH;

		VectorClear (p->vel);
		VectorClear (p->accel);
		p->alphavel = -100;
	}
}

void CL_FlyEffect (centity_t *ent, vec3_t origin)
{
	int		n;
	int		count;
	int		starttime;

	if (ent->fly_stoptime < cl.time)
	{
		starttime = cl.time;
		ent->fly_stoptime = cl.time + 60000;
	}
	else
	{
		starttime = ent->fly_stoptime - 60000;
	}

	n = cl.time - starttime;
	if (n < 20000)
		count = n * 162 / 20000.0;
	else
	{
		n = ent->fly_stoptime - cl.time;
		if (n < 20000)
			count = n * 162 / 20000.0;
		else
			count = 162;
	}

	CL_FlyParticles (origin, count);
}


/*
===============
CL_BfgParticles
===============
*/
void CL_BfgParticles (entity_t *ent)
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist;
	vec3_t		v;
	float		ltime;
	
	if ( !avelocities[0][0] )
	{
		for ( i = 0; i < NUMVERTEXNORMALS*3; i++ ) {
			avelocities[0][i] = (rand()&255) * 0.01;
		}
	}

	ltime = (float)cl.time / 1000.0;
	for ( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		angle = ltime * avelocities[i][0];
		sy = sin (angle);
		cy = cos (angle);
		angle = ltime * avelocities[i][1];
		sp = sin (angle);
		cp = cos (angle);
		angle = ltime * avelocities[i][2];
		sr = sin (angle);
		cr = cos (angle);
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		dist = sin (ltime + i) * 64;
		p->org[0] = ent->origin[0] + bytedirs[i][0]*dist + forward[0]*BEAMLENGTH;
		p->org[1] = ent->origin[1] + bytedirs[i][1]*dist + forward[1]*BEAMLENGTH;
		p->org[2] = ent->origin[2] + bytedirs[i][2]*dist + forward[2]*BEAMLENGTH;

		VectorClear (p->vel);
		VectorClear (p->accel);

		VectorSubtract (p->org, ent->origin, v);
		dist = VectorLength(v) / 90.0;

		p->color[1] = 1.5f * dist;
		clamp (p->color[1], 0.0f, 1.0f);

		p->scale = 1.5f;
		p->alpha = 1.0f - dist;
		p->alphavel = -100;
	}
}

/*
===============
CL_BFGExplosionParticles
===============
*/
void CL_BFGExplosionParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for ( i = 0; i < 256; i++ )
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		p->scale = 1.5f;
		p->color[1] = 0.8f;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand() & 31) - 16);
			p->vel[j] = (rand() & 383) - 192;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -0.8 / (0.5 + frand() * 0.3);
	}
}

/*
===============
CL_AddParticles
===============
*/
void CL_AddParticles (void)
{
	int				i, j;
	int				maxparticle, activeparticles;
	cparticle_t		*p;
	float			alpha;
	float			time, time2;
	vec3_t			org;

	if ( !cl_numparticles ) {
		return;
	}

	maxparticle = -1;
	activeparticles = 0;

	for ( i = 0, j = 0, p = particles; i < cl_numparticles; i++, p++ )
	{
		time = (cl.time - p->time) * 0.001f;
		alpha = p->alpha + time * p->alphavel;

		if (alpha <= 0)
		{	// faded out
			free_particles[j++] = p;
			continue;
		}

		maxparticle = i;
		activeparticles++;

		time2 = time * time * 0.5f;

		org[0] = p->org[0] + p->vel[0]*time + p->accel[0]*time2;
		org[1] = p->org[1] + p->vel[1]*time + p->accel[1]*time2;
		org[2] = p->org[2] + p->vel[2]*time + p->accel[2]*time2;

		V_AddParticle (org, p->color[0], p->color[1], p->color[2], alpha, p->scale);
	}

	i = 0;
	while ( maxparticle >= activeparticles ) 
	{
		*free_particles[i++] = particles[maxparticle--];

		while ( maxparticle >= activeparticles )
		{
			time = (cl.time - p->time) * 0.001f;
			alpha = p->alpha + time * p->alphavel;

			if ( alpha <= 0 ) {
				maxparticle--;
			} else {
				break;
			}
		}
	}

	cl_numparticles = activeparticles;
}

/*
==============
CL_ClearEffects

==============
*/
void CL_ClearEffects (void)
{
	CL_ClearParticles ();
	CL_ClearDlights ();
}
