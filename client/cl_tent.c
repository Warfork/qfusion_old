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

#define	MAX_BEAMS			32
#define	MAX_LOCAL_ENTITIES	512

typedef enum
{
	LE_FREE, 
	LE_NO_FADE,
	LE_RGB_FADE,
	LE_ALPHA_FADE,
	LE_SCALE_ALPHA_FADE,
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
	cl.media.modTeleportEffect = R_RegisterModel ( "models/misc/telep.md3" );

	R_RegisterModel ("models/objects/laser/tris.md2");
	R_RegisterModel ("models/objects/grenade2/tris.md2");
	R_RegisterModel ("models/objects/gibs/bone/tris.md2");
	R_RegisterModel ("models/objects/gibs/bone2/tris.md2");
}	

/*
=================
CL_RegisterMediaShaders
=================
*/
void CL_RegisterMediaShaders (void)
{
	cl.media.shaderGrenadeExplosion = R_RegisterShader ( "grenadeExplosion" );
	cl.media.shaderRocketExplosion = R_RegisterShader ( "rocketExplosion" );
	cl.media.shaderWaterBubble = R_RegisterShader ( "waterBubble" );
	cl.media.shaderTeleportEffect = R_RegisterShader ( "teleportEffect" );
	cl.media.shaderSmokePuff = R_RegisterShader ( "smokePuff" );
	cl.media.shaderBulletMark = R_RegisterShader( "gfx/damage/bullet_mrk" );
	cl.media.shaderExplosionMark = R_RegisterShader ( "gfx/damage/burn_med_mrk" );
	cl.media.shaderEnergyMark = R_RegisterShader ( "gfx/damage/plasma_mrk" );

	cl.media.shaderPowerupQuad = R_RegisterSkin ( "powerups/quad" );
	cl.media.shaderQuadWeapon = R_RegisterSkin ( "powerups/quadWeapon" );
	cl.media.shaderPowerupPenta = R_RegisterSkin ( "powerups/blueflag" );
	cl.media.shaderShellEffect = R_RegisterSkin ( "shellEffect" );
}

/*
=================
CL_ClearLocalEntities
=================
*/
void CL_ClearLocalEntities (void)
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
	lentity_t	*free, *le;

// find free or the oldest entity
	time = cl.time;

	for ( i = 0, le = cl_localents, free = le; i < MAX_LOCAL_ENTITIES; i++, le++ ) {
		if ( le->type != LE_FREE ) {
			if ( le->start >= time ) {
				continue;
			}

			free = le;
			time = le->start;
			continue;
		}

		free = le;
		goto done;
	}

done:
	memset ( free, 0, sizeof (*free) );

	free->type = type;
	free->start = cl.frame.servertime - 100;

	return free;
}

/*
=================
CL_AllocPoly
=================
*/
lentity_t *CL_AllocPoly ( letype_t type, vec3_t origin, vec3_t angles, int frames,
				float light, float lr, float lg, float lb, struct model_s *model, struct shader_s *shader  )
{
	lentity_t	*le;

	le = CL_AllocLocalEntity ( type );
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
lentity_t *CL_AllocSprite ( letype_t type, vec3_t origin, float radius, int frames,
				float light, float lr, float lg, float lb, struct shader_s *shader  )
{
	lentity_t	*le;

	le = CL_AllocLocalEntity ( type );
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

	le = CL_AllocPoly ( LE_NO_FADE, origin, vec3_origin, 2, 0, 0, 0, 0, cl.media.modFlash, NULL );
	le = CL_AllocPoly ( LE_ALPHA_FADE, origin, vec3_origin, 4, 0, 0, 0, 0, cl.media.modSmoke, NULL );
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

	if ( !model ) {
		return;
	}

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

	if ( !model ) {
		return;
	}

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
===============
CL_BubbleTrail
===============
*/
void CL_BubbleTrail ( vec3_t start, vec3_t end, int dist )
{
	int			i;
	float		len;
	vec3_t		move, vec;
	lentity_t	*le;

	VectorCopy ( start, move );
	VectorSubtract ( end, start, vec );
	len = VectorNormalize ( vec );
	VectorScale ( vec, dist, vec );

	for ( i = 0; i < len; i += dist )
	{
		le = CL_AllocSprite ( LE_ALPHA_FADE, move, 3, 10, 0, 0, 0, 0, cl.media.shaderWaterBubble );
		VectorSet ( le->velocity, crand()*5, crand()*5, crand()*5 + 6 );
		VectorAdd ( move, vec, move );
	}
}

/*
===============
CL_RocketTrail
===============
*/
void CL_RocketTrail (vec3_t start, vec3_t end)
{
	lentity_t	*le;
	float		len;
	vec3_t		vec;
	int			contents, oldcontents;

	if ( frand() < 0.5f ) {
		return;
	}

	VectorSubtract ( end, start, vec );
	len = VectorNormalize ( vec );
	if ( !len ) {
		return;
	}

	contents = CM_PointContents (end, 0);
	oldcontents = CM_PointContents (start, 0);

	if ( contents & MASK_WATER ) {
		if ( oldcontents & MASK_WATER ) {
			CL_BubbleTrail ( start, end, 8 );
		}
		return;
	}

	le = CL_AllocSprite ( LE_SCALE_ALPHA_FADE, end, 5, 9, 0, 0, 0, 0, cl.media.shaderSmokePuff );
	VectorSet ( le->velocity, -vec[0] * 5 + crand()*5, -vec[1] * 5 + crand()*5, -vec[2] * 5 + crand()*5 + 3 );
}

/*
===============
CL_GrenadeTrail
===============
*/
void CL_GrenadeTrail (vec3_t start, vec3_t end)
{
	lentity_t	*le;
	float		len;
	vec3_t		vec;
	int			contents, oldcontents;

	if ( frand() < 0.4f ) {
		return;
	}

	VectorSubtract ( end, start, vec );
	len = VectorNormalize ( vec );
	if ( !len ) {
		return;
	}

	contents = CM_PointContents (end, 0);
	oldcontents = CM_PointContents (start, 0);

	if ( contents & MASK_WATER ) {
		if ( oldcontents & MASK_WATER ) {
			CL_BubbleTrail ( start, end, 8 );
		}
		return;
	}

	le = CL_AllocSprite ( LE_SCALE_ALPHA_FADE, end, 8, 8, 0, 0, 0, 0, cl.media.shaderSmokePuff );
	VectorSet ( le->velocity, -vec[0] * 5 + crand()*5, -vec[1] * 5 + crand()*5, -vec[2] * 5 + crand()*5 + 3 );
	le->ent.color[0] = 64;
	le->ent.color[1] = 64;	
	le->ent.color[2] = 64;
}

/*
===============
CL_TeleportEffect
===============
*/
void CL_TeleportEffect ( vec3_t org )
{
	lentity_t *le;

	le = CL_AllocPoly ( LE_RGB_FADE, org, vec3_origin, 5, 0, 0, 0, 0, cl.media.modTeleportEffect, cl.media.shaderTeleportEffect );
	le->ent.origin[2] -= 24;
}

/*
=================
CL_ParseTEnt
=================
*/
static vec3_t splash_color[] = 
{
	{ 0, 0, 0 }, 
	{ 1, 0.67, 0 },
	{ 0.47, 0.48, 0.8 }, 
	{ 0.48, 0.37, 0.3 },
	{ 0, 1, 0 },
	{ 1, 0.67, 0 },
	{ 0.61, 0.1, 0 }
};

void CL_ParseTEnt (void)
{
	int			type;
	vec3_t		pos, pos2, dir, angles;
	vec3_t		color;
	int			cnt, r;
	int			ent, ent2;
	lentity_t	*le;

	type = MSG_ReadByte (&net_message);

	switch (type)
	{
	case TE_BLOOD:			// bullet hitting flesh
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		CL_ParticleEffect (pos, dir, 0.61, 0.1, 0, 60);
		break;

	case TE_GUNSHOT:			// bullet hitting wall
	case TE_SPARKS:
	case TE_BULLET_SPARKS:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		if (type == TE_GUNSHOT)
			CL_ParticleEffect (pos, dir, 0, 0, 0, 40);
		else
			CL_ParticleEffect (pos, dir, 1, 0.67, 0, 6);

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

			CL_SpawnDecal ( pos, dir, frand()*360, 8, 1, 1, 1, 1, 8, 1, false, cl.media.shaderBulletMark );
		}

		break;
		
	case TE_SCREEN_SPARKS:
	case TE_SHIELD_SPARKS:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		if (type == TE_SCREEN_SPARKS)
			CL_ParticleEffect (pos, dir, 0, 1, 0, 40);
		else
			CL_ParticleEffect (pos, dir, 0.47, 0.48, 0.8, 40);
		//FIXME : replace or remove this sound
		S_StartSound (pos, 0, 0, cl.media.sfxLashit, 1, ATTN_NORM, 0);
		break;
		
	case TE_SHOTGUN:			// bullet hitting wall
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		CL_ParticleEffect (pos, dir, 0, 0, 0, 20);
		CL_SmokeAndFlash (pos);

		CL_SpawnDecal ( pos, dir, frand()*360, 8, 1, 1, 1, 1, 8, 1, false, cl.media.shaderBulletMark );
		break;

	case TE_SPLASH:			// bullet hitting water
		cnt = MSG_ReadByte (&net_message);
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		r = MSG_ReadByte (&net_message);

		if (r > 6)
			VectorClear (color);
		else
			VectorCopy (splash_color[r], color);

		CL_ParticleEffect (pos, dir, color[0], color[1], color[2], cnt);

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
		r = MSG_ReadLong (&net_message);

		CL_ParticleEffect2 (pos, dir, 
			(( r	   ) & 0xFF) * (1.0 / 255.0), 
			(( r >> 8  ) & 0xFF) * (1.0 / 255.0), 
			(( r >> 16 ) & 0xFF) * (1.0 / 255.0), 
			cnt);
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

		le = CL_AllocPoly ( LE_ALPHA_FADE, pos, angles, 4, 150, 1, 1, 0, cl.media.modExplode, NULL  );
		S_StartSound (pos,  0, 0, cl.media.sfxLashit, 1, ATTN_NORM, 0);

		CL_SpawnDecal ( pos, dir, frand()*360, 16, 1, 0.8, 0, 1, 8, 2, true, cl.media.shaderEnergyMark );
		break;
		
	case TE_RAILTRAIL:			// railgun effect
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);

		CL_RailTrail (pos, pos2);
		S_StartSound (pos2, 0, 0, cl.media.sfxRailg, 1, ATTN_NORM, 0);
		break;

	case TE_GRENADE_EXPLOSION:
	case TE_GRENADE_EXPLOSION_WATER:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		le = CL_AllocSprite ( LE_NO_FADE, pos, 64, 5, 350, 1, 1, 0, cl.media.shaderGrenadeExplosion );

		if (type == TE_GRENADE_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl.media.sfxWatrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl.media.sfxGrenexp, 1, ATTN_NORM, 0);

		CL_SpawnDecal ( pos, dir, frand()*360, 64, 1, 1, 1, 1, 10, 1, false, cl.media.shaderExplosionMark );
		break;

	case TE_ROCKET_EXPLOSION:
	case TE_ROCKET_EXPLOSION_WATER:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		le = CL_AllocSprite ( LE_NO_FADE, pos, 64, 8, 350, 1, 0.75, 0, cl.media.shaderRocketExplosion  );

		if (type == TE_ROCKET_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl.media.sfxWatrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl.media.sfxRockexp, 1, ATTN_NORM, 0);

		CL_SpawnDecal ( pos, dir, frand()*360, 64, 1, 1, 1, 1, 10, 1, false, cl.media.shaderExplosionMark );
		break;

	case TE_EXPLOSION1:
		MSG_ReadPos (&net_message, pos);
		le = CL_AllocSprite ( LE_NO_FADE, pos, 64, 8, 350, 1, 0.75, 0, cl.media.shaderRocketExplosion  );
		S_StartSound (pos, 0, 0, cl.media.sfxRockexp, 1, ATTN_NORM, 0);
		break;

	case TE_EXPLOSION2:
		MSG_ReadPos (&net_message, pos);
		le = CL_AllocSprite ( LE_NO_FADE, pos, 62, 5, 350, 1, 1, 0, cl.media.shaderGrenadeExplosion );
		S_StartSound (pos, 0, 0, cl.media.sfxGrenexp, 1, ATTN_NORM, 0);
		break;

	case TE_BFG_EXPLOSION:
		MSG_ReadPos (&net_message, pos);

		le = CL_AllocPoly ( LE_NO_FADE, pos, vec3_origin, 4, 350, 0, 1.0, 0, cl.media.modBfgExplo, NULL  );
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

		CL_AddBeam (ent, pos, pos2, dir, cl.media.modGrappleCable);
		break;

	case TE_PLAYER_TELEPORT_IN:
		MSG_ReadPos (&net_message, pos);

		S_StartSound (pos, 0, 0, cl.media.sfxTeleportIn, 1, ATTN_NORM, 0);
		CL_TeleportEffect (pos);
		break;

	case TE_PLAYER_TELEPORT_OUT:
		MSG_ReadPos (&net_message, pos);

		S_StartSound (pos, 0, 0, cl.media.sfxTeleportOut, 1, ATTN_NORM, 0);
		CL_TeleportEffect (pos);
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
	int			i, j;
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

		ent.scale = 1.0f;
		ent.color[0] = ent.color[1] = ent.color[2] = ent.color[3] = 255;

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
			VectorCopy (b->end, ent.oldorigin);

			// offset to push beam outside of tesla model
			// (negative because dist is from end to start for this beam)
			ent.type = ET_MODEL;
			ent.model = b->model;
			ent.flags = RF_FULLBRIGHT|RF_NOSHADOW;
			angles[0] = angles2[0];
			angles[1] = angles2[1];
			angles[2] = rand()%360;
			AnglesToAxis (angles, ent.axis);
			V_AddEntity (&ent);			
			return;
		}

		ent.type = ET_MODEL;
		ent.flags = RF_NOSHADOW;
		ent.model = b->model;

		while (d > 0)
		{
			VectorCopy (org, ent.origin);
			VectorCopy (org, ent.oldorigin);

			if (b->model == cl.media.modLightning)
			{
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
CL_AddLocalEntities
=================
*/
void CL_AddLocalEntities (void)
{
	int			i, f;
	lentity_t	*le;
	entity_t	*ent;
	float		frac, fade, time;
	float		backlerp;

	backlerp = 1.0f - cl.lerpfrac;

	for ( i = 0, le = cl_localents; i < MAX_LOCAL_ENTITIES; i++, le++ )
	{
		if ( le->type == LE_FREE ) {
			continue;
		}

		frac = (cl.time - le->start) * 0.01f;
		time = cls.frametime;

		if ( le->frames > 1 ) {
			fade = 1.0 - frac / (le->frames-1);
			fade = max ( fade, 0.0f );
		} else {
			fade = 1.0f;
		}

		f = floor (frac);
		f = max ( f, 0.0f );

		ent = &le->ent;

		switch ( le->type )
		{
			case LE_NO_FADE:
				if (f >= le->frames-1) {
					le->type = LE_FREE;
				}
				break;

			case LE_RGB_FADE:
				le->ent.color[0] = FloatToByte ( fade );
				le->ent.color[1] = FloatToByte ( fade );
				le->ent.color[2] = FloatToByte ( fade );

				if (f >= le->frames-1) {
					le->type = LE_FREE;
				}
				break;

			case LE_ALPHA_FADE:
				le->ent.color[3] = FloatToByte ( fade );

				if (f >= le->frames-1) {
					le->type = LE_FREE;
				}
				break;

			case LE_SCALE_ALPHA_FADE:
				le->ent.scale = 1.0 + 1/fade;
				le->ent.scale = min (le->ent.scale, 5.0f);

				le->ent.color[3] = FloatToByte ( fade );

				if (f >= le->frames-1) {
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

		if ( le->light && fade ) {
			V_AddLight (ent->origin, le->light*fade,
				le->lightcolor[0], le->lightcolor[1], le->lightcolor[2]);
		}

		ent->frame = f + 1;
		ent->oldframe = f;
		ent->backlerp = backlerp;

		VectorCopy ( ent->origin, ent->oldorigin );
		VectorMA ( ent->origin, time, le->velocity, ent->origin );
		VectorMA ( le->velocity, time, le->accel, le->velocity );

		V_AddEntity (ent);
	}
}

