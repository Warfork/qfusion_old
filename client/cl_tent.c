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

#define	MAX_LOCAL_ENTITIES	64
#define	MAX_BEAMS			32

typedef enum
{
	LE_FREE, 
	LE_MODEL, 
	LE_SPRITE,
	LE_GIB,
	LE_LASER
} letype_t;

typedef struct
{
	letype_t	type;

	entity_t	ent;

	float		start;

	float		light;
	vec3_t		lightcolor;

	vec3_t		velocity;
	vec3_t		accel;

	int			trailcount;
	int			frames;
	int			baseframe;
} lentity_t;

typedef struct
{
	int		entity;
	int		dest_entity;
	struct model_s	*model;
	int		endtime;
	vec3_t	offset;
	vec3_t	start, end;
} beam_t;

lentity_t	cl_localents[MAX_LOCAL_ENTITIES];
beam_t		cl_beams[MAX_BEAMS];

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
	cl.media.sfxGibSound = S_RegisterSound ( "sound/player/gibsplt1.wav" );

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
	cl.media.shaderGrenadeExplosion = R_RegisterShader ( "grenadeExplosion" );
	cl.media.shaderRocketExplosion = R_RegisterShader ( "rocketExplosion" );
	
	cl.media.shaderPowerupQuad = R_RegisterSkin ( "powerups/quad" );
	cl.media.shaderQuadWeapon = R_RegisterSkin ( "powerups/quadWeapon" );
	cl.media.shaderPowerupPenta = R_RegisterSkin ( "powerups/blueflag" );
	cl.media.shaderShellEffect = R_RegisterSkin ( "shellEffect" );
}

/*
=================
CL_ClearTEnts
=================
*/
void CL_ClearTEnts (void)
{
	memset ( cl_beams, 0, sizeof(cl_beams) );
	memset ( cl_localents, 0, sizeof(cl_localents) );
}

/*
=================
CL_AllocLocalEntity
=================
*/
lentity_t *CL_AllocLocalEntity ( int type )
{
	int			i;
	int			time;
	lentity_t	*le;
	
	for ( i = 0, le = cl_localents; i < MAX_LOCAL_ENTITIES; i++, le++ ) {
		if ( le->type != LE_FREE ) {
			continue;
		}

		goto done;
	}

// find the oldest entity
	time = cl.time;
	le = cl_localents;

	for (i = 0; i < MAX_LOCAL_ENTITIES; i++ ) {
		if ( cl_localents[i].start >= time ) {
			continue;
		}

		le = &cl_localents[i];
		time = le->start;
	}

done:
	memset ( le, 0, sizeof (*le) );

	le->type = type;
	le->start = cl.frame.servertime - 100;

	return le;
}

/*
=================
CL_AllocPoly
=================
*/
lentity_t *CL_AllocPoly ( vec3_t origin, vec3_t angles, int frames,
				float light, float lr, float lg, float lb, struct model_s *model, struct shader_s *shader  )
{
	lentity_t	*le;

	le = CL_AllocLocalEntity ( LE_MODEL );
	le->frames = frames;
	le->light = light;
	le->lightcolor[0] = lr;
	le->lightcolor[1] = lg;
	le->lightcolor[2] = lb;

	le->ent.type = ET_MODEL;
	le->ent.flags = RF_NOSHADOW;
	le->ent.model = model;
	le->ent.customShader = shader;
	le->ent.shaderTime = cl.time * 0.001f;
	le->ent.scale = 1.0f;
	le->ent.color[0] = 255;
	le->ent.color[1] = 255;
	le->ent.color[2] = 255;
	le->ent.color[3] = 255;

	AnglesToAxis ( angles, le->ent.axis );
	VectorCopy ( origin, le->ent.origin );

	return le;
}

/*
=================
CL_AllocSprite
=================
*/
lentity_t *CL_AllocSprite ( vec3_t origin, float radius, int frames,
				float light, float lr, float lg, float lb, struct shader_s *shader  )
{
	lentity_t	*le;

	le = CL_AllocLocalEntity ( LE_SPRITE );
	le->frames = frames;
	le->light = light;
	le->lightcolor[0] = lr;
	le->lightcolor[1] = lg;
	le->lightcolor[2] = lb;

	le->ent.type = ET_SPRITE;
	le->ent.flags = RF_NOSHADOW;
	le->ent.radius = radius;
	le->ent.customShader = shader;
	le->ent.shaderTime = cl.time * 0.001f;
	le->ent.scale = 1.0f;
	le->ent.color[0] = 255;
	le->ent.color[1] = 255;
	le->ent.color[2] = 255;
	le->ent.color[3] = 255;

	Matrix3_Copy ( mat3_identity, le->ent.axis );
	VectorCopy ( origin, le->ent.origin );

	return le;
}

/*
=================
CL_AllocLaser
=================
*/
lentity_t *CL_AllocLaser ( vec3_t start, vec3_t end, float radius, int frames, int color, struct shader_s *shader  )
{
	lentity_t	*le;

	le = CL_AllocLocalEntity ( LE_LASER );
	le->frames = frames;

	le->ent.type = ET_BEAM;
	le->ent.flags = RF_NOSHADOW;
	le->ent.radius = radius;
	le->ent.customShader = shader;
	le->ent.shaderTime = cl.time * 0.001f;
	le->ent.color[0] = ( color & 0xFF );
	le->ent.color[1] = ( color >> 8 ) & 0xFF;
	le->ent.color[2] = ( color >> 16 ) & 0xFF;
	le->ent.color[3] = ( color >> 24 ) & 0xFF;

	VectorCopy ( start, le->ent.origin );
	VectorCopy ( end, le->ent.oldorigin );

	return le;
}

/*
=================
CL_SmokeAndFlash
=================
*/
void CL_SmokeAndFlash ( vec3_t origin )
{
	lentity_t	*le;

	le = CL_AllocPoly ( origin, vec3_origin, 4, 0, 0, 0, 0, cl.media.modSmoke, NULL );
	le = CL_AllocPoly ( origin, vec3_origin, 2, 0, 0, 0, 0, cl.media.modFlash, NULL );
}

/*
===================
CL_GibPlayer
===================
*/
void CL_GibPlayer ( vec3_t origin, vec3_t mins, vec3_t maxs )
{
	int			i;
	vec3_t		org, size, angles;
	lentity_t	*le;

	VectorSubtract ( maxs, mins, size );
	VectorAdd ( origin, mins, org );
	VectorMA ( org, 0.5f, size, org );

	for ( i = 0; i < 4; i++ ) {
		le = CL_AllocLocalEntity ( LE_GIB );
		le->trailcount = 1024;
		le->velocity[0] = 10.0 * crand();
		le->velocity[1] = 10.0 * crand();
		le->velocity[2] = 20.0 + 10.0 * frand();
		le->baseframe = 0;
		le->accel[0] = 0.0f;
		le->accel[1] = 0.0f;
		le->accel[2] = -100.0f;
		le->frames = 50;

		le->ent.type = ET_MODEL;
		le->ent.flags = RF_NOSHADOW;
		le->ent.model = cl.media.modMeatyGib;
		le->ent.origin[0] = org[0] + crand() * size[0];
		le->ent.origin[1] = org[1] + crand() * size[1];
		le->ent.origin[2] = org[2] + crand() * size[2];

		angles[0] = rand()%360;
		angles[1] = rand()%360;
		angles[2] = rand()%360;
		AnglesToAxis ( angles, le->ent.axis );
	}
}

/*
=================
CL_AddLaser
=================
*/
void CL_AddLaser ( vec3_t start, vec3_t end, int colors )
{
	lentity_t	*le;

	le = CL_AllocLaser ( start, end, 2, 2, colors, NULL );
}

/*
=================
CL_AddBeam
=================
*/
void CL_AddBeam (int ent, vec3_t start, vec3_t end, vec3_t offset, struct model_s *model )
{
	int		i;
	beam_t	*b;

// override any beam with the same entity
	for ( i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++ ) {
		if ( b->entity != ent ) {
			continue;
		}

		b->entity = ent;
		b->model = model;
		b->endtime = cl.time + 200;
		VectorCopy (start, b->start);
		VectorCopy (end, b->end);
		VectorCopy (offset, b->offset);
		return;
	}

// find a free beam
	for ( i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++ ) {
		if ( b->model || b->endtime >= cl.time ) {
			continue;
		}

		b->entity = ent;
		b->model = model;
		b->endtime = cl.time + 200;
		VectorCopy (start, b->start);
		VectorCopy (end, b->end);
		VectorCopy (offset, b->offset);
		return;
	}

	Com_DPrintf ("Beam list overflow!\n");
}

/*
=================
CL_AddLightning
=================
*/
void CL_AddLightning (int srcEnt, int destEnt, vec3_t start, vec3_t end, struct model_s *model)
{
	int		i;
	beam_t	*b;

// override any beam with the same source AND destination entities
	for ( i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++ ) {
		if ( b->entity != srcEnt || b->dest_entity != destEnt ) {
			continue;
		}

		b->entity = srcEnt;
		b->dest_entity = destEnt;
		b->model = model;
		b->endtime = cl.time + 200;
		VectorCopy (start, b->start);
		VectorCopy (end, b->end);
		VectorClear (b->offset);
		return;
	}

// find a free beam
	for ( i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++ ) {
		if ( b->model || b->endtime >= cl.time ) {
			continue;
		}

		b->entity = srcEnt;
		b->dest_entity = destEnt;
		b->model = model;
		b->endtime = cl.time + 200;
		VectorCopy (start, b->start);
		VectorCopy (end, b->end);
		VectorClear (b->offset);
		return;
	}

	Com_DPrintf ("Beam list overflow!\n");	
}

/*
=================
CL_ParseTEnt
=================
*/
static byte splash_color[] = {0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8};

void CL_ParseTEnt (void)
{
	int			type;
	vec3_t		pos, pos2, dir, angles;
	int			cnt, color, r;
	int			ent, ent2;
	lentity_t	*le;

	type = MSG_ReadByte (&net_message);

	switch (type)
	{
	case TE_BLOOD:			// bullet hitting flesh
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		CL_ParticleEffect (pos, dir, 0xe8, 60);
		break;

	case TE_GUNSHOT:			// bullet hitting wall
	case TE_SPARKS:
	case TE_BULLET_SPARKS:
		MSG_ReadPos (&net_message, pos);
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
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		if (type == TE_SCREEN_SPARKS)
			CL_ParticleEffect (pos, dir, 0xd0, 40);
		else
			CL_ParticleEffect (pos, dir, 0xb0, 40);
		//FIXME : replace or remove this sound
		S_StartSound (pos, 0, 0, cl.media.sfxLashit, 1, ATTN_NORM, 0);
		break;
		
	case TE_SHOTGUN:			// bullet hitting wall
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		CL_ParticleEffect (pos, dir, 0, 20);
		CL_SmokeAndFlash(pos);
		break;

	case TE_SPLASH:			// bullet hitting water
		cnt = MSG_ReadByte (&net_message);
		MSG_ReadPos (&net_message, pos);
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
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		color = MSG_ReadByte (&net_message);

		CL_ParticleEffect2 (pos, dir, color, cnt);
		break;

	case TE_BLASTER:			// blaster hitting wall
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		CL_BlasterParticles (pos, dir);

		angles[0] = RAD2DEG( acos(dir[2]) );
		if ( dir[0] ) {
			angles[1] = RAD2DEG( atan2(dir[1], dir[0]) );
		} else if ( dir[1] > 0 ) {
			angles[1] = 90;
		} else if ( dir[1] < 0 ) {
			angles[1] = 270;
		} else {
			angles[1] = 0;
		}
		angles[2] = 0;

		le = CL_AllocPoly ( pos, angles, 4, 150, 1, 1, 0, cl.media.modExplode, NULL  );
		S_StartSound (pos,  0, 0, cl.media.sfxLashit, 1, ATTN_NORM, 0);
		break;
		
	case TE_RAILTRAIL:			// railgun effect
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);

		CL_RailTrail (pos, pos2);
		S_StartSound (pos2, 0, 0, cl.media.sfxRailg, 1, ATTN_NORM, 0);
		break;

	case TE_EXPLOSION2:
	case TE_GRENADE_EXPLOSION:
	case TE_GRENADE_EXPLOSION_WATER:
		MSG_ReadPos (&net_message, pos);

		le = CL_AllocSprite ( pos, 62, 4, 350, 1, 1, 0, cl.media.shaderGrenadeExplosion );

		if (type == TE_GRENADE_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl.media.sfxWatrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl.media.sfxGrenexp, 1, ATTN_NORM, 0);
		break;

	case TE_EXPLOSION1:
	case TE_ROCKET_EXPLOSION:
	case TE_ROCKET_EXPLOSION_WATER:
		MSG_ReadPos (&net_message, pos);

		le = CL_AllocSprite ( pos, 62, 8, 350, 1, 0.75, 0, cl.media.shaderRocketExplosion  );

		if (type == TE_ROCKET_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl.media.sfxWatrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl.media.sfxRockexp, 1, ATTN_NORM, 0);
		break;

	case TE_BFG_EXPLOSION:
		MSG_ReadPos (&net_message, pos);

		le = CL_AllocPoly ( pos, vec3_origin, 4, 350, 0, 1.0, 0, cl.media.modBfgExplo, NULL  );
		break;

	case TE_BFG_BIGEXPLOSION:
		MSG_ReadPos (&net_message, pos);

		CL_BFGExplosionParticles (pos);
		break;

	case TE_BFG_LASER:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);

		CL_AddLaser (pos, pos2, 0xd0d1d2d3);
		break;

	case TE_BUBBLETRAIL:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);

		CL_BubbleTrail (pos, pos2, 32);
		break;

	case TE_PARASITE_ATTACK:
	case TE_MEDIC_CABLE_ATTACK:
		ent = MSG_ReadShort (&net_message);
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);

		CL_AddBeam (ent, pos, pos2, vec3_origin, cl.media.modParasiteSegment);
		break;

	case TE_BOSSTPORT:			// boss teleporting to station
		MSG_ReadPos (&net_message, pos);

		CL_BigTeleportParticles (pos);
		S_StartSound (pos, 0, 0, S_RegisterSound ("sound/misc/bigtele.wav"), 1, ATTN_NONE, 0);
		break;

	case TE_GRAPPLE_CABLE:
		ent = MSG_ReadShort (&net_message);
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);
		MSG_ReadPos (&net_message, dir);

		CL_AddBeam (ent, pos, pos2, dir, cl.media.modParasiteSegment);
		break;

	case TE_PLAYER_TELEPORT_IN:
		MSG_ReadPos (&net_message, pos);

		S_StartSound (pos, 0, 0, cl.media.sfxTeleportIn, 1, ATTN_NORM, 0);
		CL_TeleportParticles (pos);
		break;

	case TE_PLAYER_TELEPORT_OUT:
		MSG_ReadPos (&net_message, pos);

		S_StartSound (pos, 0, 0, cl.media.sfxTeleportOut, 1, ATTN_NORM, 0);
		CL_TeleportParticles (pos);
		break;

	case TE_LIGHTNING:
		ent = MSG_ReadShort (&net_message);
		ent2 = MSG_ReadShort (&net_message);

		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);

		CL_AddLightning ( ent, ent2, pos, pos2, cl.media.modLightning );
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
	vec3_t		dist, org, angles, angles2;
	float		d;
	entity_t	ent;
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
		VecToAngles (dist, angles2);

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
			ent.type = ET_MODEL;
			ent.scale = 1.0f;
			ent.model = b->model;
			ent.flags = RF_FULLBRIGHT;
			angles[0] = angles2[0];
			angles[1] = angles2[1];
			angles[2] = rand()%360;
			AnglesToAxis (angles, ent.axis);
			V_AddEntity (&ent);			
			return;
		}
		while (d > 0)
		{
			VectorCopy (org, ent.origin);
			ent.type = ET_MODEL;
			ent.scale = 1.0f;
			ent.model = b->model;
			if (b->model == cl.media.modLightning)
			{
				ent.flags = RF_FULLBRIGHT;
				angles[0] = -angles2[0];
				angles[1] = angles2[1] + 180.0;
				angles[2] = rand()%360;
			}
			else
			{
				angles[0] = angles2[0];
				angles[1] = angles2[1];
				angles[2] = rand()%360;
			}
			
			AnglesToAxis (angles, ent.axis);

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
	int			i;
	lentity_t	*le;
	entity_t	*ent;
	float		frac, time, time2;
	float		frontlerp, backlerp;
	int			f;

	memset ( &ent, 0, sizeof(ent) );

	backlerp = 1.0f - cl.lerpfrac;
	frontlerp = 1.0f - backlerp;

	for ( i = 0, le = cl_localents; i < MAX_LOCAL_ENTITIES; i++, le++ )
	{
		if ( le->type == LE_FREE ) {
			continue;
		}

		frac = (cl.time - le->start) * 0.01f;
		time = frac * 0.1f;
		time2 = time * time * 0.5f;

		f = floor (frac);
		if ( f < 0 ) {
			f = 0;
		}

		ent = &le->ent;

		switch ( le->type )
		{
			case LE_MODEL:
				if ( f >= le->frames-1 ) {
					le->type = LE_FREE;
				}
				break;
			case LE_SPRITE:
				if (f >= le->frames-1) {
					le->type = LE_FREE;
				}
				break;
			case LE_GIB:
				if ( f >= le->frames-1 ) { 
					le->type = LE_FREE;
				}
				
				if ( (CL_PMpointcontents(ent->origin) & (CONTENTS_SOLID|CONTENTS_NODROP)) ) {
					le->type = LE_FREE;
				}
				break;
			case LE_LASER:
				if ( f >= le->frames-1 ) {
					le->type = LE_FREE;
				}
				break;
		}

		if ( le->type == LE_FREE ) {
			continue;
		}

		if ( le->light ) {
			V_AddLight (ent->origin, le->light*(1.0 - frac/(le->frames-1)),
				le->lightcolor[0], le->lightcolor[1], le->lightcolor[2]);
		}

		VectorCopy ( ent->origin, ent->oldorigin );

		ent->origin[0] += le->velocity[0]*time + le->accel[0]*time2;
		ent->origin[1] += le->velocity[1]*time + le->accel[1]*time2;
		ent->origin[2] += le->velocity[2]*time + le->accel[2]*time2;

		if ( le->type != LE_GIB ) {
			ent->frame = f + 1;
			ent->oldframe = f;
		} else {
			ent->frame = 0;
			ent->oldframe = 0;

			CL_DiminishingTrail ( ent->oldorigin, ent->origin, &le->trailcount, EF_GIB );
		}

		ent->backlerp = 1.0 - cl.lerpfrac;
		V_AddEntity (ent);
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
	CL_AddExplosions ();
}
