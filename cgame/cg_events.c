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
#include "cg_local.h"

/*
==============
CG_TouchJumpPad
==============
*/
void CG_TouchJumpPad ( entity_state_t *ent, int otherNum )
{
	vec3_t org, mins, maxs;
	struct cmodel_s *cmodel;

	if ( !cg_entities[otherNum].current.modelindex ) {
		return;
	}

	cmodel = trap_CM_InlineModel ( cg_entities[otherNum].current.modelindex );
	if ( !cmodel ) {
		return;
	}

	trap_CM_InlineModelBounds ( cmodel, mins, maxs );
				
	org[0] = cg_entities[otherNum].current.origin[0] + 0.5 * (mins[0] + maxs[0]);
	org[1] = cg_entities[otherNum].current.origin[1] + 0.5 * (mins[1] + maxs[1]);
	org[2] = cg_entities[otherNum].current.origin[2] + 0.5 * (mins[2] + maxs[2]);
				
	CG_SexedSound ( ent->number, CHAN_VOICE, "*jump1.wav", 1 );
	trap_S_StartSound ( org, 0, CHAN_VOICE, CG_MediaSfx (cgs.media.sfxJumpPad), 1, ATTN_NORM, 0 );
}

/*
==============
CG_PlayerMuzzleFlash
==============
*/
void CG_PlayerMuzzleFlash ( entity_state_t *ent, int parms )
{
	int			i, j, weapon;
	centity_t	*pl;
	float		volume, radius;
	vec3_t		lightcolor;
	vec3_t		muzzle;
	int			silenced, shots;

	i = ent->number;
	weapon = ent->weapon;
	silenced = parms & MZ_SILENCED;
	shots = parms & ~MZ_SILENCED;

	if ( silenced ) {
		volume = 0.2f;
		radius = 100 + (rand()&31);
	} else {
		volume = 1.0f;
		radius = 200 + (rand()&31);
	}

	pl = &cg_entities[i];

	if ( ent->renderfx & RF_FRAMELERP ) {
		VectorCopy ( ent->origin, muzzle );
	} else {
		for ( j = 0; j < 3; j++ ) {
			muzzle[j] = pl->prev.origin[j] + cg.lerpfrac * (pl->current.origin[j] - pl->prev.origin[j]);
		}
	}

	switch ( weapon )
	{
		case WEAP_BLASTER:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxBlasterSplash), volume, ATTN_NORM, 0 );
			break;

		case WEAP_HYPERBLASTER:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxHyperblasterSplash), volume, ATTN_NORM, 0 );
			break;

		case WEAP_MACHINEGUN:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxMachinegunSplashes[rand()%4]), volume, ATTN_NORM, 0 );
			break;

		case WEAP_SHOTGUN:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxShotgunSplashes[0]), volume, ATTN_NORM, 0 );
			trap_S_StartSound ( NULL, i, CHAN_AUTO,   CG_MediaSfx(cgs.media.sfxShotgunSplashes[1]), volume, ATTN_NORM, 0.1 );
			break;

		case WEAP_SUPERSHOTGUN:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxSuperShotgunSplash), volume, ATTN_NORM, 0 );
			break;

		case WEAP_CHAINGUN:
			shots = 3 - shots;

			switch ( shots )
			{
				case 1:
					radius = 200 + (rand()&31);
					VectorSet ( lightcolor, 1.0f, 0.25f, 0.0f );
					trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxMachinegunSplashes[rand()%4]), volume, ATTN_NORM, 0 );
					break;

				case 2:
					radius = 225 + (rand()&31);
					VectorSet ( lightcolor, 1.0f, 0.5f, 0.0f );
					trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxMachinegunSplashes[rand()%4]), volume, ATTN_NORM, 0 );
					trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxMachinegunSplashes[rand()%4]), volume, ATTN_NORM, 0.05 );
					break;

				default:
					radius = 250 + (rand()&31);
					VectorSet ( lightcolor, 1.0f, 1.0f, 0.0f );
					trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxMachinegunSplashes[rand()%4]), volume, ATTN_NORM, 0 );
					trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxMachinegunSplashes[rand()%4]), volume, ATTN_NORM, 0.033 );
					trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxMachinegunSplashes[rand()%4]), volume, ATTN_NORM, 0.066 );
					break;
			}
			break;

		case WEAP_RAILGUN:
			VectorSet ( lightcolor, 0.5, 0.5, 1.0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxRailg), volume, ATTN_NORM, 0 );
			break;

		case WEAP_ROCKETLAUNCHER:
			VectorSet ( lightcolor, 1, 0, 0.2 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxRocketLauncherSplash), volume, ATTN_NORM, 0 );
			break;

		case WEAP_GRENADELAUNCHER:
			VectorSet ( lightcolor, 1, 0.5, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxGrenadeLauncherSplash), volume, ATTN_NORM, 0 );
			break;

		case WEAP_BFG:
			VectorSet ( lightcolor, 0, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, CG_MediaSfx(cgs.media.sfxBFGSplash), volume, ATTN_NORM, 0 );
			break;

		// default to no light
		default:
			radius = 0;
			VectorClear ( lightcolor );
			break;
	}

	// spawn light if not cleared
	if ( radius ) {
		CG_AllocDlight ( radius, muzzle, lightcolor );
	}
}

/*
==============
CG_MonsterMuzzleFlash
==============
*/
void CG_MonsterMuzzleFlash ( entity_state_t *ent, int parms )
{
	int			i, flash_number;
	vec3_t		origin;
	vec3_t		forward, right;
	float		radius;
	vec3_t		lightcolor;

	// locate the origin
	i = ent->number;
	flash_number = ent->weapon;
	AngleVectors ( cg_entities[i].current.angles, forward, right, NULL );
	origin[0] = ent->origin[0] + forward[0] * monster_flash_offset[flash_number][0] + right[0] * monster_flash_offset[flash_number][1];
	origin[1] = ent->origin[1] + forward[1] * monster_flash_offset[flash_number][0] + right[1] * monster_flash_offset[flash_number][1];
	origin[2] = ent->origin[2] + forward[2] * monster_flash_offset[flash_number][0] + right[2] * monster_flash_offset[flash_number][1] + monster_flash_offset[flash_number][2];

	radius = 200 + (rand()&31);

	switch ( flash_number )
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
			VectorSet ( lightcolor, 1, 1, 0 );
			CG_ParticleEffect ( origin, vec3_origin, 0, 0, 0, 40 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/infantry/infatck1.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_SOLDIER_MACHINEGUN_1:
		case MZ2_SOLDIER_MACHINEGUN_2:
		case MZ2_SOLDIER_MACHINEGUN_3:
		case MZ2_SOLDIER_MACHINEGUN_4:
		case MZ2_SOLDIER_MACHINEGUN_5:
		case MZ2_SOLDIER_MACHINEGUN_6:
		case MZ2_SOLDIER_MACHINEGUN_7:
		case MZ2_SOLDIER_MACHINEGUN_8:
			VectorSet ( lightcolor, 1, 1, 0 );
			CG_ParticleEffect ( origin, vec3_origin, 0, 0, 0, 40 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/soldier/solatck3.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_GUNNER_MACHINEGUN_1:
		case MZ2_GUNNER_MACHINEGUN_2:
		case MZ2_GUNNER_MACHINEGUN_3:
		case MZ2_GUNNER_MACHINEGUN_4:
		case MZ2_GUNNER_MACHINEGUN_5:
		case MZ2_GUNNER_MACHINEGUN_6:
		case MZ2_GUNNER_MACHINEGUN_7:
		case MZ2_GUNNER_MACHINEGUN_8:
			VectorSet ( lightcolor, 1, 1, 0 );
			CG_ParticleEffect ( origin, vec3_origin, 0, 0, 0, 40 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/gunner/gunatck2.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_ACTOR_MACHINEGUN_1:
		case MZ2_SUPERTANK_MACHINEGUN_1:
		case MZ2_SUPERTANK_MACHINEGUN_2:
		case MZ2_SUPERTANK_MACHINEGUN_3:
		case MZ2_SUPERTANK_MACHINEGUN_4:
		case MZ2_SUPERTANK_MACHINEGUN_5:
		case MZ2_SUPERTANK_MACHINEGUN_6:
			VectorSet ( lightcolor, 1, 1, 0 );
			CG_ParticleEffect ( origin, vec3_origin, 0, 0, 0, 40 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/infantry/infatck1.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_BOSS2_MACHINEGUN_L1:
		case MZ2_BOSS2_MACHINEGUN_L2:
		case MZ2_BOSS2_MACHINEGUN_L3:
		case MZ2_BOSS2_MACHINEGUN_L4:
		case MZ2_BOSS2_MACHINEGUN_L5:
			VectorSet ( lightcolor, 1, 1, 0 );
			CG_ParticleEffect ( origin, vec3_origin, 0, 0, 0, 40 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/infantry/infatck1.wav"), 1, ATTN_NONE, 0 );
			break;

		case MZ2_SOLDIER_BLASTER_1:
		case MZ2_SOLDIER_BLASTER_2:
		case MZ2_SOLDIER_BLASTER_3:
		case MZ2_SOLDIER_BLASTER_4:
		case MZ2_SOLDIER_BLASTER_5:
		case MZ2_SOLDIER_BLASTER_6:
		case MZ2_SOLDIER_BLASTER_7:
		case MZ2_SOLDIER_BLASTER_8:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/soldier/solatck2.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_FLYER_BLASTER_1:
		case MZ2_FLYER_BLASTER_2:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/flyer/flyatck3.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_MEDIC_BLASTER_1:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/medic/medatck1.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_HOVER_BLASTER_1:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/hover/hovatck1.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_FLOAT_BLASTER_1:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/floater/fltatck1.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_SOLDIER_SHOTGUN_1:
		case MZ2_SOLDIER_SHOTGUN_2:
		case MZ2_SOLDIER_SHOTGUN_3:
		case MZ2_SOLDIER_SHOTGUN_4:
		case MZ2_SOLDIER_SHOTGUN_5:
		case MZ2_SOLDIER_SHOTGUN_6:
		case MZ2_SOLDIER_SHOTGUN_7:
		case MZ2_SOLDIER_SHOTGUN_8:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/soldier/solatck1.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_TANK_BLASTER_1:
		case MZ2_TANK_BLASTER_2:
		case MZ2_TANK_BLASTER_3:
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/tank/tnkatck3.wav"), 1, ATTN_NORM, 0 );
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
			VectorSet ( lightcolor, 1, 1, 0 );
			CG_ParticleEffect ( origin, vec3_origin, 0, 0, 0, 40 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound (va("sound/tank/tnkatk2%c.wav", 'a' + rand() % 5)), 1, ATTN_NORM, 0 );
			break;

		case MZ2_CHICK_ROCKET_1:
			VectorSet ( lightcolor, 1, 0.5, 0.2 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/chick/chkatck2.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_TANK_ROCKET_1:
		case MZ2_TANK_ROCKET_2:
		case MZ2_TANK_ROCKET_3:
			VectorSet ( lightcolor, 1, 0.5, 0.2 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/tank/tnkatck1.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_SUPERTANK_ROCKET_1:
		case MZ2_SUPERTANK_ROCKET_2:
		case MZ2_SUPERTANK_ROCKET_3:
		case MZ2_BOSS2_ROCKET_1:
		case MZ2_BOSS2_ROCKET_2:
		case MZ2_BOSS2_ROCKET_3:
		case MZ2_BOSS2_ROCKET_4:
			VectorSet ( lightcolor, 1, 0.5, 0.2 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/tank/rocket.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_GUNNER_GRENADE_1:
		case MZ2_GUNNER_GRENADE_2:
		case MZ2_GUNNER_GRENADE_3:
		case MZ2_GUNNER_GRENADE_4:
			VectorSet ( lightcolor, 1, 0.5, 0.2 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/gunner/gunatck3.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_GLADIATOR_RAILGUN_1:
			VectorSet ( lightcolor, 0.5, 0.5, 1.0 );
			break;

		case MZ2_MAKRON_BFG:
			VectorSet ( lightcolor, 0.5, 1, 0.5 );
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
			VectorSet ( lightcolor, 1, 1, 0 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/makron/blaster.wav"), 1, ATTN_NORM, 0 );
			break;
		
		case MZ2_JORG_MACHINEGUN_L1:
		case MZ2_JORG_MACHINEGUN_L2:
		case MZ2_JORG_MACHINEGUN_L3:
		case MZ2_JORG_MACHINEGUN_L4:
		case MZ2_JORG_MACHINEGUN_L5:
		case MZ2_JORG_MACHINEGUN_L6:
			VectorSet ( lightcolor, 1, 1, 0 );
			CG_ParticleEffect ( origin, vec3_origin, 0, 0, 0, 40 );
			trap_S_StartSound ( NULL, i, CHAN_WEAPON, trap_S_RegisterSound("sound/boss3/xfire.wav"), 1, ATTN_NORM, 0 );
			break;

		case MZ2_JORG_MACHINEGUN_R1:
		case MZ2_JORG_MACHINEGUN_R2:
		case MZ2_JORG_MACHINEGUN_R3:
		case MZ2_JORG_MACHINEGUN_R4:
		case MZ2_JORG_MACHINEGUN_R5:
		case MZ2_JORG_MACHINEGUN_R6:
			VectorSet ( lightcolor, 1, 1, 0 );
			CG_ParticleEffect ( origin, vec3_origin, 0, 0, 0, 40 );
			break;

		case MZ2_JORG_BFG_1:
			VectorSet ( lightcolor, 0.5, 1, 0.5 );
			break;

		case MZ2_BOSS2_MACHINEGUN_R1:
		case MZ2_BOSS2_MACHINEGUN_R2:
		case MZ2_BOSS2_MACHINEGUN_R3:
		case MZ2_BOSS2_MACHINEGUN_R4:
		case MZ2_BOSS2_MACHINEGUN_R5:
			VectorSet ( lightcolor, 1, 1, 0 );
			CG_ParticleEffect ( origin, vec3_origin, 0, 0, 0, 40 );
			break;
	}

	// spawn light if not cleared
	if ( radius ) {
		CG_AllocDlight ( radius, origin, lightcolor );
	}
}

/*
==============
CG_FireLead

Clientside prediction of gunshots.
Must match fire_lead in g_weapon.c
==============
*/
static void CG_FireLead (int self, vec3_t start, vec3_t axis[3], int hspread, int vspread, int *seed, trace_t *trace)
{
	trace_t		tr;
	vec3_t		dir;
	vec3_t		end;
	float		r;
	float		u;
	vec3_t		water_start;
	qboolean	water = qfalse, impact = qfalse;
	int			content_mask = MASK_SHOT | MASK_WATER;

	r = Q_crandom ( seed ) * hspread;
	u = Q_crandom ( seed ) * vspread;
	VectorMA ( start, 8192, axis[0], end );
	VectorMA ( end, r, axis[1], end );
	VectorMA ( end, u, axis[2], end );

	if ( CG_PointContents (start) & MASK_WATER ) {
		water = qtrue;
		VectorCopy ( start, water_start );
		content_mask &= ~MASK_WATER;
	}
	
	CG_Trace ( &tr, start, vec3_origin, vec3_origin, end, self, content_mask );

	// see if we hit water
	if ( tr.contents & MASK_WATER ) {
		water = qtrue;
		VectorCopy ( tr.endpos, water_start );

		if ( !VectorCompare (start, tr.endpos) ) {
			vec3_t forward, right, up;

			if ( tr.contents & CONTENTS_WATER ) {
				CG_ParticleEffect (tr.endpos, tr.plane.normal, 0.47, 0.48, 0.8, 8);
			} else if ( tr.contents & CONTENTS_SLIME ) {
				CG_ParticleEffect (tr.endpos, tr.plane.normal, 0.0, 1.0, 0.0, 8);
			} else if ( tr.contents & CONTENTS_LAVA ) {
				CG_ParticleEffect (tr.endpos, tr.plane.normal, 1.0, 0.67, 0.0, 8);
			}

			// change bullet's course when it enters water
			VectorSubtract ( end, start, dir );
			VecToAngles ( dir, dir );
			AngleVectors ( dir, forward, right, up );
			r = Q_crandom ( seed ) * hspread * 2;
			u = Q_crandom ( seed ) * vspread * 2;
			VectorMA ( water_start, 8192, forward, end );
			VectorMA ( end, r, right, end );
			VectorMA ( end, u, up, end );
		}

		// re-trace ignoring water this time
		CG_Trace ( &tr, water_start, vec3_origin, vec3_origin, end, self, MASK_SHOT );
	}

	// save the final trace
	*trace = tr;

	// if went through water, determine where the end and make a bubble trail
	if ( water ) {
		vec3_t	pos;

		VectorSubtract ( tr.endpos, water_start, dir );
		VectorNormalize ( dir );
		VectorMA ( tr.endpos, -2, dir, pos );

		if ( CG_PointContents (pos) & MASK_WATER ) {
			VectorCopy ( pos, tr.endpos );
		} else {
			CG_Trace ( &tr, pos, vec3_origin, vec3_origin, water_start, tr.ent ? cg_entities[tr.ent].current.number : 0, MASK_WATER );
		}

		VectorAdd ( water_start, tr.endpos, pos );
		VectorScale ( pos, 0.5, pos );

		CG_BubbleTrail ( water_start, tr.endpos, 32 );
	}
}

/*
==============
CG_FireBullet
==============
*/
void CG_FireBullet ( int self, vec3_t start, vec3_t forward, int count, int vspread, int hspread, int seed, void (*impact) (trace_t *tr) )
{
	int i;
	trace_t tr;
	vec3_t axis[3];
	qboolean takedamage;

	// calculate normal vectors
	VectorNormalize2 ( forward, axis[0] );
	PerpendicularVector ( axis[1], axis[0] );
	CrossProduct ( axis[0], axis[1], axis[2] );

	for ( i = 0; i < count; i++ ) {
		CG_FireLead ( self, start, axis, vspread, hspread, &seed, &tr );

		takedamage = tr.ent && ( cg_entities[tr.ent].current.type & ET_INVERSE );

		if ( tr.fraction < 1.0f && !takedamage && !(tr.surfFlags & SURF_NOIMPACT) ) {
			impact ( &tr );
		}
	}
}

/*
==============
CG_BulletImpact
==============
*/
void CG_BulletImpact ( trace_t *tr )
{
	// bullet impact
	CG_BulletExplosion ( tr->endpos, tr->plane.normal );

	// spawn decal
	CG_SpawnDecal ( tr->endpos, tr->plane.normal, random()*360, 8, 1, 1, 1, 1, 8, 1, qfalse, CG_MediaShader (cgs.media.shaderBulletMark) );

	// throw particles on dust
	if ( tr->surfFlags & SURF_DUST ) {
		CG_ParticleEffect ( tr->endpos, tr->plane.normal, 0.30, 0.30, 0.25, 20 );
	}

	// impact sound
	trap_S_StartSound ( tr->endpos, 0, 0, CG_MediaSfx (cgs.media.sfxRic[rand()&2]), 1, ATTN_NORM, 0 );
}

/*
==============
CG_ShotgunImpact
==============
*/
void CG_ShotgunImpact ( trace_t *tr )
{
	// bullet impact
	CG_BulletExplosion ( tr->endpos, tr->plane.normal );

	// throw particles on dust
	if ( tr->surfFlags & SURF_DUST ) {
		CG_ParticleEffect ( tr->endpos, tr->plane.normal, 0.30, 0.30, 0.25, 20 );
	}

	// spawn decal
	CG_SpawnDecal ( tr->endpos, tr->plane.normal, random()*360, 8, 1, 1, 1, 1, 8, 1, qfalse, CG_MediaShader (cgs.media.shaderBulletMark) );
}

/*
==============
CG_EntityEvent

An entity has just been parsed that has an event value
==============
*/
void CG_EntityEvent ( entity_state_t *ent )
{
	int i;
	int parm;
	vec3_t dir;

	for ( i = 0; i < 2; i++ ) {
		parm = ent->eventParms[i];

		switch ( ent->events[i] )
		{
			case EV_FOOTSTEP:
				if ( cg_footSteps->value ) {
					trap_S_StartSound ( NULL, ent->number, CHAN_BODY, CG_MediaSfx (cgs.media.sfxFootsteps[parm][rand()&3]), 1, ATTN_NORM, 0 );
				}
				break;

			case EV_FALL:
				if ( parm == FALL_MEDIUM ) {
					CG_SexedSound ( ent->number, CHAN_AUTO, "*fall2.wav", 1 );
				} else if ( parm == FALL_FAR ) {
					CG_SexedSound ( ent->number, CHAN_AUTO, "*fall1.wav", 1 );
				} else {
					trap_S_StartSound ( NULL, ent->number, CHAN_AUTO, CG_MediaSfx (cgs.media.sfxLand), 1, ATTN_NORM, 0 );
				}
				break;

			case EV_PAIN:
				CG_SexedSound ( ent->number, CHAN_VOICE, va("*pain%i_%i.wav", 25 * (parm+1), 1 + (rand()&1)), 1 );
				break;

			case EV_DIE:
				CG_SexedSound ( ent->number, CHAN_BODY, va("*death%i.wav", ( rand()%4 ) + 1), 1 );
				break;

			case EV_GIB:
				trap_S_StartSound ( NULL, ent->number, CHAN_VOICE, CG_MediaSfx (cgs.media.sfxGibSound), 1, ATTN_NORM, 0 );
				break;

			case EV_JUMP:
				CG_SexedSound ( ent->number, CHAN_VOICE, "*jump1.wav", 1 );
				break;

			case EV_JUMP_PAD:
				CG_TouchJumpPad ( ent, parm );
				break;

			case EV_MUZZLEFLASH:
				CG_PlayerMuzzleFlash ( ent, parm );
				break;
				
			case EV_PLAYER_TELEPORT_IN:
				trap_S_StartSound ( ent->origin, ent->ownerNum, 0, CG_MediaSfx (cgs.media.sfxTeleportIn), 1, ATTN_NORM, 0 );
				CG_TeleportEffect ( ent->origin );
				break;
				
			case EV_PLAYER_TELEPORT_OUT:
				trap_S_StartSound ( ent->origin, ent->ownerNum, 0, CG_MediaSfx (cgs.media.sfxTeleportOut), 1, ATTN_NORM, 0 );
				CG_TeleportEffect ( ent->origin );
				break;

			case EV_ITEM_RESPAWN:
				cg_entities[ent->number].respawnTime = cg.time;
				trap_S_StartSound ( NULL, ent->number, CHAN_WEAPON, CG_MediaSfx (cgs.media.sfxItemRespawn), 1, ATTN_IDLE, 0 );
				break;

			case EV_EXPLOSION1:
				CG_Explosion1 ( ent->origin );
				break;

			case EV_EXPLOSION2:
				CG_Explosion2 ( ent->origin );
				break;

			case EV_ROCKET_EXPLOSION:
				ByteToDir ( parm, dir );
				CG_RocketExplosion ( ent->origin, dir );
				break;

			case EV_GRENADE_EXPLOSION:
				ByteToDir ( parm, dir );
				CG_GrenadeExplosion ( ent->origin, dir );
				break;

			case EV_GRENADE_BOUNCE:
				if ( rand() & 1 ) {
					trap_S_StartSound ( NULL, ent->number, CHAN_VOICE, CG_MediaSfx (cgs.media.sfxGrenBounce1), 1, ATTN_NORM, 0 );
				} else {
					trap_S_StartSound ( NULL, ent->number, CHAN_VOICE, CG_MediaSfx (cgs.media.sfxGrenBounce2), 1, ATTN_NORM, 0 );
				}
				break;

			case EV_BOSSTPORT:
				CG_BigTeleportParticles ( ent->origin );
				trap_S_StartSound ( ent->origin, 0, 0, trap_S_RegisterSound ("sound/misc/bigtele.wav"), 1, ATTN_NONE, 0 );
				break;

			case EV_MUZZLEFLASH2:
				CG_MonsterMuzzleFlash ( ent, parm );
				break;

			case EV_FIRE_BULLET:
				CG_FireBullet ( ent->ownerNum, ent->origin, ent->origin2, 1, DEFAULT_BULLET_VSPREAD, DEFAULT_BULLET_HSPREAD, parm, CG_BulletImpact );
				break;

			case EV_FIRE_SHOTGUN:
				CG_FireBullet ( ent->ownerNum, ent->origin, ent->origin2, ent->eventCount, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SHOTGUN_HSPREAD, parm, CG_ShotgunImpact );
				break;

			case EV_GRAPPLE_CABLE:
				CG_AddBeam ( ent->ownerNum, cg_entities[ent->ownerNum].current.origin, ent->origin, ent->origin2, CG_MediaModel (cgs.media.modGrappleCable) );
				break;

			case EV_PARASITE_ATTACK:
				CG_AddBeam ( ent->ownerNum, cg_entities[ent->ownerNum].current.origin, ent->origin, ent->origin2, CG_MediaModel (cgs.media.modParasiteSegment) );
				break;

			case EV_MEDIC_CABLE_ATTACK:
				CG_AddBeam ( ent->ownerNum, cg_entities[ent->ownerNum].current.origin, ent->origin, ent->origin2, CG_MediaModel (cgs.media.modParasiteSegment) );
				break;

			case EV_RAILTRAIL:
				CG_RailTrail ( ent->origin, ent->origin2 );
				trap_S_StartSound ( ent->origin2, 0, 0, CG_MediaSfx (cgs.media.sfxRailg), 1, ATTN_NORM, 0 );
				break;

			case EV_BFG_LASER:
				CG_BFGLaser ( ent->origin, ent->origin2 );
				break;

			case EV_BFG_EXPLOSION:
				CG_BFGExplosion ( ent->origin );
				break;

			case EV_BFG_BIGEXPLOSION:
				CG_BFGBigExplosion ( ent->origin );
				break;

			case EV_LIGHTNING:
				CG_AddLightning ( ent->ownerNum, ent->targetNum, ent->origin, ent->origin2, CG_MediaModel (cgs.media.modLightning) );
				trap_S_StartSound ( NULL, ent->ownerNum, CHAN_WEAPON, CG_MediaSfx (cgs.media.sfxLightning), 1, ATTN_NORM, 0 );
				break;

			case EV_BLASTER:
				ByteToDir ( parm, dir );
				CG_BlasterExplosion ( ent->origin, dir );
				break;

			case EV_BLOOD:
				ByteToDir ( parm, dir );
				CG_ParticleEffect ( ent->origin, dir, 0.61, 0.1, 0, 60 );
				break;

			case EV_SPARKS:
				ByteToDir ( parm, dir );
				CG_ParticleEffect ( ent->origin, dir, 1, 0.67, 0, 6 );
				break;

			case EV_BULLET_SPARKS:
				ByteToDir ( parm, dir );
				CG_BulletExplosion ( ent->origin, dir );
				CG_ParticleEffect ( ent->origin, dir, 1, 0.67, 0, 6 );
				trap_S_StartSound ( ent->origin, 0, 0, CG_MediaSfx (cgs.media.sfxRic[rand()&2]), 1, ATTN_NORM, 0 );
				break;

			case EV_SCREEN_SPARKS:
				ByteToDir ( parm, dir );
				CG_ParticleEffect ( ent->origin, dir, 0, 1, 0, 40 );
				//FIXME : replace or remove this sound
				trap_S_StartSound ( ent->origin, 0, 0, CG_MediaSfx (cgs.media.sfxLashit), 1, ATTN_NORM, 0 );
				break;

			case EV_SHIELD_SPARKS:
				ByteToDir ( parm, dir );
				CG_ParticleEffect ( ent->origin, dir, 0.47, 0.48, 0.8, 40 );
				//FIXME : replace or remove this sound
				trap_S_StartSound ( ent->origin, 0, 0, CG_MediaSfx (cgs.media.sfxLashit), 1, ATTN_NORM, 0 );
				break;

			case EV_LASER_SPARKS:
				ByteToDir ( parm, dir );
				CG_ParticleEffect2 ( ent->origin, dir, 
					COLOR_R (ent->colorRGBA) * (1.0 / 255.0), 
					COLOR_G (ent->colorRGBA) * (1.0 / 255.0), 
					COLOR_B (ent->colorRGBA) * (1.0 / 255.0), 
					ent->eventCount );
				break;
		}
	}
}
