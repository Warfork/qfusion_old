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
// cl_tent.c -- client side temporary entities

#include "client.h"

typedef enum
{
	ex_free, ex_explosion, ex_misc, ex_flash, ex_mflash, ex_poly, ex_poly2, ex_gib
} exptype_t;

typedef struct
{
	exptype_t	type;
	entity_t	ent;

	int			trailcount;
	int			frames;
	float		light;
	vec3_t		lightcolor;
	vec3_t		velocity;
	vec3_t		accel;
	float		start;
	int			baseframe;
} explosion_t;



#define	MAX_EXPLOSIONS	32
explosion_t	cl_explosions[MAX_EXPLOSIONS];


#define	MAX_BEAMS	32
typedef struct
{
	int		entity;
	int		dest_entity;
	struct model_s	*model;
	int		endtime;
	vec3_t	offset;
	vec3_t	start, end;
} beam_t;
beam_t		cl_beams[MAX_BEAMS];
//PMM - added this for player-linked beams.  Currently only used by the plasma beam
beam_t		cl_playerbeams[MAX_BEAMS];


#define	MAX_LASERS	32
typedef struct
{
	entity_t	ent;
	int			endtime;
} laser_t;
laser_t		cl_lasers[MAX_LASERS];

void CL_BlasterParticles (vec3_t org, vec3_t dir);
void CL_ExplosionParticles (vec3_t org);
void CL_BFGExplosionParticles (vec3_t org);
int	 CL_PMpointcontents (vec3_t point);

/*
=================
CL_RegisterMediaSounds
=================
*/
void CL_RegisterMediaSounds (void)
{
	int		i;
	char	name[MAX_QPATH];

	cl.media.sfxRic1 = S_RegisterSound ( "sound/weapons/machinegun/ric1.wav" );
	cl.media.sfxRic2 = S_RegisterSound ( "sound/weapons/machinegun/ric2.wav" );
	cl.media.sfxRic3 = S_RegisterSound ( "sound/weapons/machinegun/ric3.wav" );

	cl.media.sfxLashit = S_RegisterSound ( "sound/weapons/lashit.wav" );

	cl.media.sfxSpark5 = S_RegisterSound ( "sound/world/spark5.wav" );
	cl.media.sfxSpark6 = S_RegisterSound ( "sound/world/spark6.wav" );
	cl.media.sfxSpark7 = S_RegisterSound ( "sound/world/spark7.wav" );

	cl.media.sfxRailg = S_RegisterSound ( "sound/weapons/railgun/railgf1a.wav" );

	cl.media.sfxRockexp = S_RegisterSound ( "sound/weapons/rocket/rocklx1a.wav" );
	cl.media.sfxGrenexp = S_RegisterSound ( "sound/weapons/grenlx1a.wav" );
	cl.media.sfxWatrexp = S_RegisterSound ( "sound/weapons/xpld_wat.wav" );

	cl.media.sfxItemRespawn = S_RegisterSound ( "sound/items/respawn1.wav" );
	cl.media.sfxTeleportIn = S_RegisterSound ( "sound/world/telein.wav" );
	cl.media.sfxTeleportOut = S_RegisterSound ( "sound/world/teleout.wav" );
	cl.media.sfxJumpPad = S_RegisterSound ( "sound/world/jumppad.wav" );
	cl.media.sfxLand = S_RegisterSound ( "sound/player/land1.wav" );
	cl.media.sfxGibSound = S_RegisterSound ( "sound/misc/udeath.wav" );

	for ( i = 0; i < 4; i++ ) {
		Com_sprintf ( name, sizeof(name), "sound/player/footsteps/step%i.wav", i+1 );
		cl.media.sfxFootsteps[FOOTSTEP_NORMAL][i] = S_RegisterSound ( name );

		Com_sprintf ( name, sizeof(name), "sound/player/footsteps/boot%i.wav", i+1 );
		cl.media.sfxFootsteps[FOOTSTEP_BOOT][i] = S_RegisterSound ( name );

		Com_sprintf ( name, sizeof(name), "sound/player/footsteps/flesh%i.wav", i+1 );
		cl.media.sfxFootsteps[FOOTSTEP_FLESH][i] = S_RegisterSound ( name );

		Com_sprintf ( name, sizeof(name), "sound/player/footsteps/mech%i.wav", i+1 );
		cl.media.sfxFootsteps[FOOTSTEP_MECH][i] = S_RegisterSound ( name );

		Com_sprintf ( name, sizeof(name), "sound/player/footsteps/energy%i.wav", i+1 );
		cl.media.sfxFootsteps[FOOTSTEP_ENERGY][i] = S_RegisterSound ( name );

		Com_sprintf ( name, sizeof(name), "sound/player/footsteps/splash%i.wav", i+1 );
		cl.media.sfxFootsteps[FOOTSTEP_SPLASH][i] = S_RegisterSound ( name );

		Com_sprintf ( name, sizeof(name), "sound/player/footsteps/clank%i.wav", i+1 );
		cl.media.sfxFootsteps[FOOTSTEP_METAL][i] = S_RegisterSound ( name );
	}

	for ( i = 0; i < 4; i++ ) {
		Com_sprintf ( name, sizeof(name), "sound/weapons/machinegun/machgf%ib.wav", i+1 );
		cl.media.sfxMachinegunSplashes[i] = S_RegisterSound ( name );
	}

	cl.media.sfxHyperblasterSplash = S_RegisterSound("sound/weapons/hyprbf1a.wav");

	cl.media.sfxLightning = S_RegisterSound ( "sound/weapons/tesla.wav");
	cl.media.sfxDisrexp = S_RegisterSound ( "sound/weapons/disrupthit.wav");

	cl.media.sfxGrenBounce1 = S_RegisterSound ( "sound/weapons/grenade/hgrenb1a.wav" );
	cl.media.sfxGrenBounce2 = S_RegisterSound ( "sound/weapons/grenade/hgrenb2a.wav" );
}	

/*
=================
CL_RegisterMediaModels
=================
*/
void CL_RegisterMediaModels (void)
{
	cl.media.modExplode = R_RegisterModel ("models/objects/explode/tris.md2");
	cl.media.modSmoke = R_RegisterModel ("models/objects/smoke/tris.md2");
	cl.media.modFlash = R_RegisterModel ("models/objects/flash/tris.md2");
	cl.media.modParasiteSegment = R_RegisterModel ("models/monsters/parasite/segment/tris.md2");
	cl.media.modGrappleCable = R_RegisterModel ("models/ctf/segment/tris.md2");
	cl.media.modParasiteTip = R_RegisterModel ("models/monsters/parasite/tip/tris.md2");
	cl.media.modExplo4 = R_RegisterModel ("models/objects/r_explode/tris.md2");
	cl.media.modBfgExplo = R_RegisterModel ("sprites/s_bfg2.sp2");
	cl.media.modPowerScreen = R_RegisterModel ("models/items/armor/effect/tris.md2");
	cl.media.modLightning = R_RegisterModel ("models/proj/lightning/tris.md2");
	cl.media.modMeatyGib = R_RegisterModel ("models/objects/gibs/sm_meat/tris.md2");

	R_RegisterModel ("models/objects/laser/tris.md2");
	R_RegisterModel ("models/objects/grenade2/tris.md2");
	R_RegisterModel ("models/objects/gibs/bone/tris.md2");
	R_RegisterModel ("models/objects/gibs/bone2/tris.md2");
}	

/*
=================
CL_RegisterMediaModels
=================
*/
void CL_RegisterMediaPics (void)
{
}

/*
=================
CL_ClearTEnts
=================
*/
void CL_ClearTEnts (void)
{
	memset (cl_beams, 0, sizeof(cl_beams));
	memset (cl_explosions, 0, sizeof(cl_explosions));
	memset (cl_lasers, 0, sizeof(cl_lasers));

//ROGUE
	memset (cl_playerbeams, 0, sizeof(cl_playerbeams));
//ROGUE
}

/*
=================
CL_AllocExplosion
=================
*/
explosion_t *CL_AllocExplosion (void)
{
	int		i;
	int		time;
	int		index;
	
	for (i=0 ; i<MAX_EXPLOSIONS ; i++)
	{
		if (cl_explosions[i].type == ex_free)
		{
			memset (&cl_explosions[i], 0, sizeof (cl_explosions[i]));
			return &cl_explosions[i];
		}
	}
// find the oldest explosion
	time = cl.time;
	index = 0;

	for (i=0 ; i<MAX_EXPLOSIONS ; i++)
		if (cl_explosions[i].start < time)
		{
			time = cl_explosions[i].start;
			index = i;
		}
	memset (&cl_explosions[index], 0, sizeof (cl_explosions[index]));
	return &cl_explosions[index];
}

/*
=================
CL_SmokeAndFlash
=================
*/
void CL_SmokeAndFlash(vec3_t origin)
{
	explosion_t	*ex;

	ex = CL_AllocExplosion ();
	VectorCopy (origin, ex->ent.origin);
	ex->type = ex_misc;
	ex->frames = 4;
	ex->ent.flags = RF_TRANSLUCENT;
	ex->start = cl.frame.servertime - 100;
	ex->ent.model = cl.media.modSmoke;

	ex = CL_AllocExplosion ();
	VectorCopy (origin, ex->ent.origin);
	ex->type = ex_flash;
	ex->ent.flags = RF_FULLBRIGHT;
	ex->frames = 2;
	ex->start = cl.frame.servertime - 100;
	ex->ent.model = cl.media.modFlash;
}

/*
===================
CL_GibPlayer

===================
*/
void CL_GibPlayer ( entity_state_t *ent )
{
	vec3_t	mins, maxs, size, origin;
	int		x, zd, zu, i;
	explosion_t	*ex;

	x = 8*(ent->solid & 31);
	zd = 8*((ent->solid>>5) & 31);
	zu = 8*((ent->solid>>10) & 63) - 32;

	mins[0] = mins[1] = -x;
	maxs[0] = maxs[1] = x;
	mins[2] = -zd;
	maxs[2] = zu;

	VectorSubtract ( maxs, mins, size );
	VectorAdd ( ent->origin, mins, origin );
	VectorMA ( origin, 0.5f, size, origin );

	for ( i = 0; i < 4; i++ ) {
		ex = CL_AllocExplosion ();
		ex->type = ex_gib;
		ex->trailcount = 1024;
		ex->start = cl.frame.servertime - 100;
		ex->velocity[0] = 10.0 * crand();
		ex->velocity[1] = 10.0 * crand();
		ex->velocity[2] = 20.0 + 10.0 * frand();
		ex->baseframe = 0;
		ex->accel[0] = 0.0f;
		ex->accel[1] = 0.0f;
		ex->accel[2] = -100.0f;
		ex->frames = 50;
		ex->ent.model = cl.media.modMeatyGib;
		ex->ent.angles[0] = rand()%360;
		ex->ent.angles[1] = rand()%360;
		ex->ent.angles[2] = rand()%360;
		ex->ent.origin[0] = origin[0] + crand() * size[0];
		ex->ent.origin[1] = origin[1] + crand() * size[1];
		ex->ent.origin[2] = origin[2] + crand() * size[2];
	}
}

/*
=================
CL_ParseBeam
=================
*/
int CL_ParseBeam (struct model_s *model)
{
	int		ent;
	vec3_t	start, end;
	beam_t	*b;
	int		i;
	
	ent = MSG_ReadShort (&net_message);
	
	MSG_ReadLongPos (&net_message, start);
	MSG_ReadLongPos (&net_message, end);

// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return ent;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return ent;
		}
	}
	Com_Printf ("beam list overflow!\n");	
	return ent;
}

/*
=================
CL_ParseBeam2
=================
*/
int CL_ParseBeam2 (struct model_s *model)
{
	int		ent;
	vec3_t	start, end, offset;
	beam_t	*b;
	int		i;
	
	ent = MSG_ReadShort (&net_message);
	
	MSG_ReadLongPos (&net_message, start);
	MSG_ReadLongPos (&net_message, end);
	MSG_ReadLongPos (&net_message, offset);

// override any beam with the same entity

	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;	
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}
	}
	Com_Printf ("beam list overflow!\n");	
	return ent;
}

// ROGUE
/*
=================
CL_ParsePlayerBeam
  - adds to the cl_playerbeam array instead of the cl_beams array
=================
*/
int CL_ParsePlayerBeam (struct model_s *model)
{
	int		ent;
	vec3_t	start, end, offset;
	beam_t	*b;
	int		i;
	
	ent = MSG_ReadShort (&net_message);
	
	MSG_ReadLongPos (&net_message, start);
	MSG_ReadLongPos (&net_message, end);
	MSG_ReadLongPos (&net_message, offset);

// override any beam with the same entity
// PMM - For player beams, we only want one per player (entity) so..
	for (i=0, b=cl_playerbeams ; i< MAX_BEAMS ; i++, b++)
	{
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}
	}

// find a free beam
	for (i=0, b=cl_playerbeams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 100;		// PMM - this needs to be 100 to prevent multiple heatbeams
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}
	}
	Com_Printf ("beam list overflow!\n");	
	return ent;
}
//rogue

/*
=================
CL_ParseLightning
=================
*/
int CL_ParseLightning (struct model_s *model)
{
	int		srcEnt, destEnt;
	vec3_t	start, end;
	beam_t	*b;
	int		i;
	
	srcEnt = MSG_ReadShort (&net_message);
	destEnt = MSG_ReadShort (&net_message);

	MSG_ReadLongPos (&net_message, start);
	MSG_ReadLongPos (&net_message, end);

// override any beam with the same source AND destination entities
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == srcEnt && b->dest_entity == destEnt)
		{
			b->entity = srcEnt;
			b->dest_entity = destEnt;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return srcEnt;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = srcEnt;
			b->dest_entity = destEnt;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return srcEnt;
		}
	}
	Com_Printf ("beam list overflow!\n");	
	return srcEnt;
}

/*
=================
CL_ParseLaser
=================
*/
void CL_ParseLaser (int colors)
{
	vec3_t	start;
	vec3_t	end;
	laser_t	*l;
	int		i;

	MSG_ReadLongPos (&net_message, start);
	MSG_ReadLongPos (&net_message, end);

	for (i=0, l=cl_lasers ; i< MAX_LASERS ; i++, l++)
	{
		if (l->endtime < cl.time)
		{
			l->ent.flags = RF_TRANSLUCENT | RF_BEAM;
			VectorCopy (start, l->ent.origin);
			VectorCopy (end, l->ent.oldorigin);
			l->ent.alpha = 0.30;
			l->ent.skinnum = (colors >> ((rand() % 4)*8)) & 0xff;
			l->ent.model = NULL;
			l->ent.frame = 4;
			l->endtime = cl.time + 100;
			return;
		}
	}
}

/*
=================
CL_ParseTEnt
=================
*/
static byte splash_color[] = {0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8};

void CL_ParseTEnt (void)
{
	int		type;
	vec3_t	pos, pos2, dir;
	explosion_t	*ex;
	int		cnt;
	int		color;
	int		r;
	int		ent;

	type = MSG_ReadByte (&net_message);

	switch (type)
	{
	case TE_BLOOD:			// bullet hitting flesh
		MSG_ReadLongPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		CL_ParticleEffect (pos, dir, 0xe8, 60);
		break;

	case TE_GUNSHOT:			// bullet hitting wall
	case TE_SPARKS:
	case TE_BULLET_SPARKS:
		MSG_ReadLongPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		if (type == TE_GUNSHOT)
			CL_ParticleEffect (pos, dir, 0, 40);
		else
			CL_ParticleEffect (pos, dir, 0xe0, 6);

		if (type != TE_SPARKS)
		{
			CL_SmokeAndFlash(pos);
			
			// impact sound
			cnt = rand() & 3;
			if (cnt < 2)
				S_StartSound (pos, 0, 0, cl.media.sfxRic1, 1, ATTN_NORM, 0);
			else if (cnt == 2)
				S_StartSound (pos, 0, 0, cl.media.sfxRic2, 1, ATTN_NORM, 0);
			else
				S_StartSound (pos, 0, 0, cl.media.sfxRic3, 1, ATTN_NORM, 0);
		}

		break;
		
	case TE_SCREEN_SPARKS:
	case TE_SHIELD_SPARKS:
		MSG_ReadLongPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		if (type == TE_SCREEN_SPARKS)
			CL_ParticleEffect (pos, dir, 0xd0, 40);
		else
			CL_ParticleEffect (pos, dir, 0xb0, 40);
		//FIXME : replace or remove this sound
		S_StartSound (pos, 0, 0, cl.media.sfxLashit, 1, ATTN_NORM, 0);
		break;
		
	case TE_SHOTGUN:			// bullet hitting wall
		MSG_ReadLongPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		CL_ParticleEffect (pos, dir, 0, 20);
		CL_SmokeAndFlash(pos);
		break;

	case TE_SPLASH:			// bullet hitting water
		cnt = MSG_ReadByte (&net_message);
		MSG_ReadLongPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		r = MSG_ReadByte (&net_message);
		if (r > 6)
			color = 0x00;
		else
			color = splash_color[r];
		CL_ParticleEffect (pos, dir, color, cnt);

		if (r == SPLASH_SPARKS)
		{
			r = rand() & 3;
			if (r == 0)
				S_StartSound (pos, 0, 0, cl.media.sfxSpark5, 1, ATTN_STATIC, 0);
			else if (r == 1)
				S_StartSound (pos, 0, 0, cl.media.sfxSpark6, 1, ATTN_STATIC, 0);
			else
				S_StartSound (pos, 0, 0, cl.media.sfxSpark7, 1, ATTN_STATIC, 0);
		}
		break;

	case TE_LASER_SPARKS:
		cnt = MSG_ReadByte (&net_message);
		MSG_ReadLongPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		color = MSG_ReadByte (&net_message);
		CL_ParticleEffect2 (pos, dir, color, cnt);
		break;

	case TE_BLASTER:			// blaster hitting wall
		MSG_ReadLongPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		CL_BlasterParticles (pos, dir);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->ent.angles[0] = RAD2DEG( acos(dir[2]) );
	// PMM - fixed to correct for pitch of 0
		if (dir[0])
			ex->ent.angles[1] = RAD2DEG( atan2(dir[1], dir[0]) );
		else if (dir[1] > 0)
			ex->ent.angles[1] = 90;
		else if (dir[1] < 0)
			ex->ent.angles[1] = 270;
		else
			ex->ent.angles[1] = 0;

		ex->type = ex_misc;
		ex->ent.flags = RF_FULLBRIGHT|RF_TRANSLUCENT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 150;
		ex->lightcolor[0] = 1;
		ex->lightcolor[1] = 1;
		ex->lightcolor[2] = 0;
		ex->ent.model = cl.media.modExplode;
		ex->frames = 4;
		S_StartSound (pos,  0, 0, cl.media.sfxLashit, 1, ATTN_NORM, 0);
		break;
		
	case TE_RAILTRAIL:			// railgun effect
		MSG_ReadLongPos (&net_message, pos);
		MSG_ReadLongPos (&net_message, pos2);
		CL_RailTrail (pos, pos2);
		S_StartSound (pos2, 0, 0, cl.media.sfxRailg, 1, ATTN_NORM, 0);
		break;

	case TE_EXPLOSION2:
	case TE_GRENADE_EXPLOSION:
	case TE_GRENADE_EXPLOSION_WATER:
		MSG_ReadLongPos (&net_message, pos);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 1.0;
		ex->lightcolor[2] = 0.0;
		ex->ent.model = cl.media.modExplo4;
		ex->frames = 19;
		ex->baseframe = 30;
		ex->ent.angles[1] = rand() % 360;
		CL_ExplosionParticles (pos);
		if (type == TE_GRENADE_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl.media.sfxWatrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl.media.sfxGrenexp, 1, ATTN_NORM, 0);
		break;

	case TE_EXPLOSION1:
	case TE_ROCKET_EXPLOSION:
	case TE_ROCKET_EXPLOSION_WATER:
		MSG_ReadLongPos (&net_message, pos);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 0.75;
		ex->lightcolor[2] = 0.0;
		ex->ent.angles[1] = rand() % 360;
		ex->ent.model = cl.media.modExplo4;
		if (frand() < 0.5)
			ex->baseframe = 15;
		ex->frames = 15;
		CL_ExplosionParticles (pos);									// PMM
		if (type == TE_ROCKET_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl.media.sfxWatrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl.media.sfxRockexp, 1, ATTN_NORM, 0);
		break;

	case TE_BFG_EXPLOSION:
		MSG_ReadLongPos (&net_message, pos);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 0.0;
		ex->lightcolor[1] = 1.0;
		ex->lightcolor[2] = 0.0;
		ex->ent.model = cl.media.modBfgExplo;
		ex->ent.flags |= RF_TRANSLUCENT;
		ex->ent.alpha = 0.30;
		ex->frames = 4;
		break;

	case TE_BFG_BIGEXPLOSION:
		MSG_ReadLongPos (&net_message, pos);

		CL_BFGExplosionParticles (pos);
		break;

	case TE_BFG_LASER:
		CL_ParseLaser (0xd0d1d2d3);
		break;

	case TE_BUBBLETRAIL:
		MSG_ReadLongPos (&net_message, pos);
		MSG_ReadLongPos (&net_message, pos2);
		CL_BubbleTrail (pos, pos2, 32);
		break;

	case TE_PARASITE_ATTACK:
	case TE_MEDIC_CABLE_ATTACK:
		ent = CL_ParseBeam (cl.media.modParasiteSegment);
		break;

	case TE_BOSSTPORT:			// boss teleporting to station
		MSG_ReadLongPos (&net_message, pos);
		CL_BigTeleportParticles (pos);
		S_StartSound (pos, 0, 0, S_RegisterSound ("sound/misc/bigtele.wav"), 1, ATTN_NONE, 0);
		break;

	case TE_GRAPPLE_CABLE:
		ent = CL_ParseBeam2 (cl.media.modGrappleCable);
		break;

	case TE_PLAYER_TELEPORT_IN:
		MSG_ReadLongPos (&net_message, pos);
		S_StartSound (pos, 0, 0, cl.media.sfxTeleportIn, 1, ATTN_NORM, 0);
		CL_TeleportParticles (pos);
		break;

	case TE_PLAYER_TELEPORT_OUT:
		MSG_ReadLongPos (&net_message, pos);
		S_StartSound (pos, 0, 0, cl.media.sfxTeleportOut, 1, ATTN_NORM, 0);
		CL_TeleportParticles (pos);
		break;

	case TE_LIGHTNING:
		ent = CL_ParseLightning (cl.media.modLightning);
		S_StartSound (NULL, ent, CHAN_WEAPON, cl.media.sfxLightning, 1, ATTN_NORM, 0);
		break;

	default:
		Com_Error (ERR_DROP, "CL_ParseTEnt: bad type");
	}
}

/*
=================
CL_AddBeams
=================
*/
void CL_AddBeams (void)
{
	int			i,j;
	beam_t		*b;
	vec3_t		dist, org;
	float		d;
	entity_t	ent;
	float		yaw, pitch;
	float		forward;
	float		len, steps;
	float		model_length;
	
// update beams
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.playernum+1)	// entity 0 is the world
		{
			VectorCopy (cl.refdef.vieworg, b->start);
			b->start[2] -= 22;	// adjust for view height
		}
		VectorAdd (b->start, b->offset, org);

	// calculate pitch and yaw
		VectorSubtract (b->end, org, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
	// PMM - fixed to correct for pitch of 0
			if (dist[0])
				yaw = RAD2DEG ( atan2(dist[1], dist[0]) );
			else if (dist[1] > 0)
				yaw = 90;
			else
				yaw = 270;
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = -RAD2DEG ( atan2(dist[2], forward) );
			if (pitch < 0)
				pitch += 360.0;
		}

	// add new entities for the beams
		d = VectorNormalize(dist);

		memset (&ent, 0, sizeof(ent));
		if (b->model == cl.media.modLightning)
		{
			model_length = 35.0;
			d-= 20.0;  // correction so it doesn't end in middle of tesla
		}
		else
		{
			model_length = 30.0;
		}
		steps = ceil(d/model_length);
		len = (d-model_length)/(steps-1);

		// PMM - special case for lightning model .. if the real length is shorter than the model,
		// flip it around & draw it from the end to the start.  This prevents the model from going
		// through the tesla mine (instead it goes through the target)
		if ((b->model == cl.media.modLightning) && (d <= model_length))
		{
			VectorCopy (b->end, ent.origin);
			// offset to push beam outside of tesla model (negative because dist is from end to start
			// for this beam)
			ent.model = b->model;
			ent.flags = RF_FULLBRIGHT;
			ent.angles[0] = pitch;
			ent.angles[1] = yaw;
			ent.angles[2] = rand()%360;
			V_AddEntity (&ent);			
			return;
		}
		while (d > 0)
		{
			VectorCopy (org, ent.origin);
			ent.model = b->model;
			if (b->model == cl.media.modLightning)
			{
				ent.flags = RF_FULLBRIGHT;
				ent.angles[0] = -pitch;
				ent.angles[1] = yaw + 180.0;
				ent.angles[2] = rand()%360;
			}
			else
			{
				ent.angles[0] = pitch;
				ent.angles[1] = yaw;
				ent.angles[2] = rand()%360;
			}
			
			ent.scale = 1.0f;
			V_AddEntity (&ent);

			for (j=0 ; j<3 ; j++)
				org[j] += dist[j]*len;
			d -= model_length;
		}
	}
}

extern cvar_t *hand;

/*
=================
ROGUE - draw player locked beams
CL_AddPlayerBeams
=================
*/
void CL_AddPlayerBeams (void)
{
	int			i,j;
	beam_t		*b;
	vec3_t		dist, org;
	float		d;
	entity_t	ent;
	float		yaw, pitch;
	float		forward;
	float		len, steps;
	float		model_length;

// update beams
	for (i=0, b=cl_playerbeams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.playernum+1)	// entity 0 is the world
		{
			VectorCopy (cl.refdef.vieworg, b->start);
			b->start[2] -= 22;	// adjust for view height
		}
		
		VectorAdd (b->start, b->offset, org);

	// calculate pitch and yaw
		VectorSubtract (b->end, org, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
	// PMM - fixed to correct for pitch of 0
			if (dist[0])
				yaw = RAD2DEG ( atan2(dist[1], dist[0]) );
			else if (dist[1] > 0)
				yaw = 90;
			else
				yaw = 270;
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = -RAD2DEG ( atan2(dist[2], forward) );
			if (pitch < 0)
				pitch += 360.0;
		}
		
	// add new entities for the beams
		d = VectorNormalize(dist);

		memset (&ent, 0, sizeof(ent));

		if (b->model == cl.media.modLightning)
		{
			model_length = 35.0;
			d-= 20.0;  // correction so it doesn't end in middle of tesla
		}
		else
		{
			model_length = 30.0;
		}

		steps = ceil(d/model_length);
		len = (d-model_length)/(steps-1);

		while (d > 0)
		{
			VectorCopy (org, ent.origin);
			ent.model = b->model;

			if (b->model == cl.media.modLightning)
			{
				ent.flags = RF_FULLBRIGHT;
				ent.angles[0] = -pitch;
				ent.angles[1] = yaw + 180.0;
				ent.angles[2] = rand()%360;
			}
			else
			{
				ent.angles[0] = pitch;
				ent.angles[1] = yaw;
				ent.angles[2] = rand()%360;
			}
			
			ent.scale = 1.0f;
			V_AddEntity (&ent);

			for (j=0 ; j<3 ; j++)
				org[j] += dist[j]*len;
			d -= model_length;
		}
	}
}

/*
=================
CL_AddExplosions
=================
*/
void CL_AddExplosions (void)
{
	entity_t	*ent;
	int			i;
	explosion_t	*ex;
	float		frac, time, time2;
	int			f;

	memset (&ent, 0, sizeof(ent));

	for (i=0, ex=cl_explosions ; i< MAX_EXPLOSIONS ; i++, ex++)
	{
		if (ex->type == ex_free)
			continue;
		frac = (cl.time - ex->start)*0.01f;
		time = frac * 0.1f;
		time2 = time*time*0.5f;
		f = floor(frac);

		ent = &ex->ent;

		switch (ex->type)
		{
		case ex_mflash:
			if (f >= ex->frames-1)
				ex->type = ex_free;
			break;
		case ex_misc:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}
			ent->alpha = 1.0 - frac/(ex->frames-1);
			break;
		case ex_flash:
			if (f >= 1)
			{
				ex->type = ex_free;
				break;
			}
			ent->alpha = 1.0;
			break;
		case ex_poly:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}

			ent->alpha = (16.0 - (float)f)/16.0;

			if (f < 10)
			{
				ent->skinnum = (f>>1);
				if (ent->skinnum < 0)
					ent->skinnum = 0;
			}
			else
			{
				ent->flags |= RF_TRANSLUCENT;
				if (f < 13)
					ent->skinnum = 5;
				else
					ent->skinnum = 6;
			}
			break;
		case ex_poly2:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}

			ent->alpha = (5.0 - (float)f)/5.0;
			ent->skinnum = 0;
			ent->flags |= RF_TRANSLUCENT;
			break;
		case ex_gib:
			if (f >= ex->frames-1 || (CL_PMpointcontents (ex->ent.origin) & (CONTENTS_SOLID|CONTENTS_NODROP)))
				ex->type = ex_free;
			break;
		}

		if (ex->type == ex_free)
			continue;
		if (ex->light)
		{
			V_AddLight (ent->origin, ex->light*ent->alpha,
				ex->lightcolor[0], ex->lightcolor[1], ex->lightcolor[2]);
		}

		VectorCopy (ent->origin, ent->oldorigin);

		if (f < 0)
			f = 0;

		ent->origin[0] += ex->velocity[0]*time + ex->accel[0]*time2;
		ent->origin[1] += ex->velocity[1]*time + ex->accel[1]*time2;
		ent->origin[2] += ex->velocity[2]*time + ex->accel[2]*time2;

		if ( ex->type != ex_gib ) {
			ent->frame = ex->baseframe + f + 1;
			ent->oldframe = ex->baseframe + f;
		} else {
			ent->frame = ex->baseframe;
			ent->oldframe = ex->baseframe;

			CL_DiminishingTrail ( ent->oldorigin, ent->origin, &ex->trailcount, EF_GIB );
		}

		ent->scale = 1.0f;
		ent->backlerp = 1.0 - cl.lerpfrac;

		V_AddEntity (ent);
	}
}


/*
=================
CL_AddLasers
=================
*/
void CL_AddLasers (void)
{
	laser_t		*l;
	int			i;

	for (i=0, l=cl_lasers ; i< MAX_LASERS ; i++, l++)
	{
		if (l->endtime >= cl.time) {
			l->ent.scale = 1.0f;
			V_AddEntity (&l->ent);
		}
	}
}

/*
=================
CL_AddTEnts
=================
*/
void CL_AddTEnts (void)
{
	CL_AddBeams ();
	// PMM - draw plasma beams
	CL_AddPlayerBeams ();
	CL_AddExplosions ();
	CL_AddLasers ();
}
