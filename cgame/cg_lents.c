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
// cg_lents.c -- client side temporary entities

#include "cg_local.h"

#define	MAX_BEAMS			32
#define	MAX_LOCAL_ENTITIES	512

#define MAX_BEAMENTS		32
#define NUM_BEAM_SEGS		6

static vec3_t debris_maxs = { 4, 4, 8 };
static vec3_t debris_mins = { -4, -4, 0 };

typedef enum
{
	LE_FREE, 
	LE_NO_FADE,
	LE_RGB_FADE,
	LE_ALPHA_FADE,
	LE_SCALE_ALPHA_FADE,
	LE_LASER
} letype_t;

typedef struct lentity_s
{
	struct lentity_s *prev, *next;

	letype_t	type;

	entity_t	ent;
	vec4_t		color;

	float		start;

	float		light;
	vec3_t		lightcolor;

	vec3_t		velocity;
	vec3_t		accel;

	int			bounce;
	int			oldtime;

	int			frames;
} lentity_t;

typedef struct
{
	int			entity;
	int			dest_entity;
	struct model_s	*model;
	int			endtime;
	vec3_t		offset;
	vec3_t		start, end;
} beam_t;

typedef struct
{
	vec3_t			verts[NUM_BEAM_SEGS][4];
	vec2_t			stcoords[NUM_BEAM_SEGS][4];
	byte_vec4_t		colors[NUM_BEAM_SEGS][4];
} cg_beament_t;

lentity_t		cg_localents[MAX_LOCAL_ENTITIES];
lentity_t		cg_localents_headnode, *cg_free_lents;

beam_t			cg_beams[MAX_BEAMS];

cg_beament_t	cg_beamEnts[MAX_BEAMENTS];
int				cg_numBeamEnts;

/*
=================
CG_ClearLocalEntities
=================
*/
void CG_ClearLocalEntities( void )
{
	int i;

	memset( cg_beams, 0, sizeof( cg_beams ) );
	memset( cg_localents, 0, sizeof( cg_localents ) );

	// link local entities
	cg_free_lents = cg_localents;
	cg_localents_headnode.prev = &cg_localents_headnode;
	cg_localents_headnode.next = &cg_localents_headnode;
	for( i = 0; i < MAX_LOCAL_ENTITIES - 1; i++ )
		cg_localents[i].next = &cg_localents[i+1];
}

/*
=================
CG_AllocLocalEntity
=================
*/
lentity_t *CG_AllocLocalEntity( int type, float r, float g, float b, float a )
{
	lentity_t *le;

	if ( cg_free_lents ) {	// take a free decal if possible
		le = cg_free_lents;
		cg_free_lents = le->next;
	} else {				// grab the oldest one otherwise
		le = cg_localents_headnode.prev;
		le->prev->next = le->next;
		le->next->prev = le->prev;
	}

	memset( le, 0, sizeof (*le) );
	le->type = type;
	le->start = cg.frame.serverTime - cgs.serverFrameTime;
	le->color[0] = r;
	le->color[1] = g;
	le->color[2] = b;
	le->color[3] = a;

	switch( le->type ) {
		case LE_NO_FADE:
			break;
		case LE_RGB_FADE:
			le->ent.color[3] = ( qbyte )( 255 * a );
			break;
		case LE_SCALE_ALPHA_FADE:
		case LE_ALPHA_FADE:
			le->ent.color[0] = ( qbyte )( 255 * r );
			le->ent.color[1] = ( qbyte )( 255 * g );
			le->ent.color[2] = ( qbyte )( 255 * b );
			break;
		default:
			break;
	}

	// put the decal at the start of the list
	le->prev = &cg_localents_headnode;
	le->next = cg_localents_headnode.next;
	le->next->prev = le;
	le->prev->next = le;

	return le;
}

/*
=================
CG_FreeLocalEntity
=================
*/
static void CG_FreeLocalEntity( lentity_t *le )
{
	// remove from linked active list
	le->prev->next = le->next;
	le->next->prev = le->prev;

	// insert into linked free list
	le->next = cg_free_lents;
	cg_free_lents = le;
}

/*
=================
CG_AllocPoly
=================
*/
lentity_t *CG_AllocPoly( letype_t type, const vec3_t origin, const vec3_t angles, int frames,
				float r, float g, float b, float a, float light, float lr, float lg, float lb, struct model_s *model, struct shader_s *shader  )
{
	lentity_t	*le;

	le = CG_AllocLocalEntity( type, r, g, b, a );
	le->frames = frames;
	le->light = light;
	le->lightcolor[0] = lr;
	le->lightcolor[1] = lg;
	le->lightcolor[2] = lb;

	le->ent.rtype = RT_MODEL;
	le->ent.flags = RF_NOSHADOW;
	le->ent.model = model;
	le->ent.customShader = shader;
	le->ent.shaderTime = cg.time * 0.001f;
	le->ent.scale = 1.0f;

	AnglesToAxis( angles, le->ent.axis );
	VectorCopy( origin, le->ent.origin );

	return le;
}

/*
=================
CG_AllocSprite
=================
*/
lentity_t *CG_AllocSprite( letype_t type, vec3_t origin, float radius, int frames,
				float r, float g, float b, float a, float light, float lr, float lg, float lb, struct shader_s *shader  )
{
	lentity_t	*le;

	le = CG_AllocLocalEntity( type, r, g, b, a );
	le->frames = frames;
	le->light = light;
	le->lightcolor[0] = lr;
	le->lightcolor[1] = lg;
	le->lightcolor[2] = lb;

	le->ent.rtype = RT_SPRITE;
	le->ent.flags = RF_NOSHADOW;
	le->ent.radius = radius;
	le->ent.customShader = shader;
	le->ent.shaderTime = cg.time * 0.001f;
	le->ent.scale = 1.0f;

	Matrix_Identity( le->ent.axis );
	VectorCopy( origin, le->ent.origin );

	return le;
}

/*
=================
CG_AllocLaser
=================
*/
lentity_t *CG_AllocLaser( vec3_t start, vec3_t end, float radius, int frames, 
						 float r, float g, float b, float a, struct shader_s *shader )
{
	lentity_t	*le;

	le = CG_AllocLocalEntity( LE_LASER, 1, 1, 1, 1 );
	le->frames = frames;

	le->ent.radius = radius;
	le->ent.customShader = shader;
	le->ent.skinNum = COLOR_RGBA( (int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255) );

	VectorCopy( start, le->ent.origin );
	VectorCopy( end, le->ent.origin2 );

	return le;
}

/*
=================
CG_AddLaser
=================
*/
void CG_AddLaser( vec3_t start, vec3_t end, float radius, int colors, struct shader_s *shader )
{
	int				i;
	vec3_t			perpvec;
	vec3_t			direction, normalized_direction;
	vec3_t			start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t			oldorigin, origin;
	poly_t			poly;
	cg_beament_t	*beamEnt;

	if( cg_numBeamEnts >= MAX_BEAMENTS )
		return;

	VectorCopy( start, origin );
	VectorCopy( end, oldorigin );
	VectorSubtract( oldorigin, origin, direction );

	if( VectorNormalize2( direction, normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, radius, perpvec );

	for( i = 0; i < NUM_BEAM_SEGS; i++ ) {
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	beamEnt = &cg_beamEnts[cg_numBeamEnts++];

	memset( &poly, 0, sizeof(poly) );
	poly.numverts = 4;
	poly.shader = shader;

	for( i = 0; i < NUM_BEAM_SEGS; i++ ) {
		poly.colors = beamEnt->colors[i];
		poly.stcoords = beamEnt->stcoords[i];
		poly.verts = beamEnt->verts[i];
		poly.fognum = 0;

		VectorCopy( start_points[i], poly.verts[0] );
		poly.stcoords[0][0] = 0;
		poly.stcoords[0][1] = 0;
		*(int *)poly.colors[0] = colors;

		VectorCopy( end_points[i], poly.verts[1] );
		poly.stcoords[1][0] = 0;
		poly.stcoords[1][1] = 1;
		*(int *)poly.colors[1] = colors;

		VectorCopy( end_points[(i+1)%NUM_BEAM_SEGS], poly.verts[2] );
		poly.stcoords[2][0] = 1;
		poly.stcoords[2][1] = 1;
		*(int *)poly.colors[2] = colors;

		VectorCopy( start_points[(i+1)%NUM_BEAM_SEGS], poly.verts[3] );
		poly.stcoords[3][0] = 1;
		poly.stcoords[3][1] = 0;
		*(int *)poly.colors[3] = colors;

		trap_R_AddPolyToScene( &poly );
	}
}

/*
=================
CG_BulletExplosion
=================
*/
void CG_BulletExplosion( vec3_t origin, vec3_t dir )
{
	vec3_t		v;
	lentity_t	*le;

	le = CG_AllocPoly( LE_NO_FADE, origin, vec3_origin, 6, 
		1, 1, 1, 1, 
		0, 0, 0, 0, 
		CG_MediaModel( cgs.media.modBulletExplode ), 
		CG_MediaShader( cgs.media.shaderBulletExplosion ) );

	if( !dir || VectorCompare( dir, vec3_origin ) ) {
		Matrix_Identity( le->ent.axis );
		return;
	}

	VectorMA( le->ent.origin, -8, dir, le->ent.origin );
	VectorCopy( dir, le->ent.axis[0] );
	PerpendicularVector( v, le->ent.axis[0] );
	RotatePointAroundVector( le->ent.axis[1], le->ent.axis[0], v, rand() % 360 );
	CrossProduct( le->ent.axis[0], le->ent.axis[1], le->ent.axis[2] );
}

/*
=================
CG_AddBeam
=================
*/
void CG_AddBeam( int ent, vec3_t start, vec3_t end, vec3_t offset, struct model_s *model )
{
	int		i;
	beam_t	*b;

	if( !model )
		return;

// override any beam with the same entity
	for( i = 0, b = cg_beams; i < MAX_BEAMS; i++, b++ ) {
		if( b->entity != ent )
			continue;

		b->entity = ent;
		b->model = model;
		b->endtime = cg.time + 100;
		VectorCopy( start, b->start );
		VectorCopy( end, b->end );
		VectorCopy( offset, b->offset );
		return;
	}

	// find a free beam
	for( i = 0, b = cg_beams; i < MAX_BEAMS; i++, b++ ) {
		if( b->model || b->endtime >= cg.time )
			continue;

		b->entity = ent;
		b->model = model;
		b->endtime = cg.time + 100;
		VectorCopy( start, b->start );
		VectorCopy( end, b->end );
		VectorCopy( offset, b->offset );
		return;
	}
}

/*
=================
CG_AddLightning
=================
*/
void CG_AddLightning( int srcEnt, int destEnt, vec3_t start, vec3_t end, struct model_s *model )
{
	int		i;
	beam_t	*b;

	if( !model )
		return;

	// override any beam with the same source AND destination entities
	for( i = 0, b = cg_beams; i < MAX_BEAMS; i++, b++ ) {
		if( b->entity != srcEnt || b->dest_entity != destEnt )
			continue;

		b->entity = srcEnt;
		b->dest_entity = destEnt;
		b->model = model;
		b->endtime = cg.time + 200;
		VectorCopy( start, b->start );
		VectorCopy( end, b->end );
		VectorClear( b->offset );
		return;
	}

	// find a free beam
	for( i = 0, b = cg_beams; i < MAX_BEAMS; i++, b++ ) {
		if( b->model || b->endtime >= cg.time )
			continue;

		b->entity = srcEnt;
		b->dest_entity = destEnt;
		b->model = model;
		b->endtime = cg.time + 200;
		VectorCopy( start, b->start );
		VectorCopy( end, b->end );
		VectorClear( b->offset );
		return;
	}
}

/*
===============
CG_BubbleTrail
===============
*/
void CG_BubbleTrail( vec3_t start, vec3_t end, int dist )
{
	int			i;
	float		len;
	vec3_t		move, vec;
	lentity_t	*le;
	struct shader_s *shader;

	VectorCopy( start, move );
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	if( !len )
		return;

	VectorScale( vec, dist, vec );
	shader = CG_MediaShader( cgs.media.shaderWaterBubble );

	for( i = 0; i < len; i += dist ) {
		le = CG_AllocSprite( LE_ALPHA_FADE, move, 3, 10, 
			1, 1, 1, 1,
			0, 0, 0, 0, 
			shader );
		VectorSet( le->velocity, crandom()*5, crandom()*5, crandom()*5 + 6 );
		VectorAdd( move, vec, move );
	}
}

/*
===============
CG_BlasterExplosion
===============
*/
void CG_BlasterExplosion( vec3_t pos, vec3_t dir )
{
	CG_BlasterParticles( pos, dir );

	trap_S_StartSound( pos,  0, 0, CG_MediaSfx( cgs.media.sfxLashit ), 1, ATTN_NORM, 0 );

	CG_SpawnDecal( pos, dir, random()*360, 16, 0.8, 0.8, 1, 1, 8, 2, qtrue, CG_MediaShader( cgs.media.shaderEnergyMark ) );
}

/*
===============
CG_Explosion1
===============
*/
void CG_Explosion1( vec3_t pos )
{
	lentity_t	*le;

	le = CG_AllocSprite( LE_NO_FADE, pos, 64, 8, 
		1, 1, 1, 1,
		350, 1, 0.75, 0, 
		CG_MediaShader( cgs.media.shaderRocketExplosion ) );
	le->ent.rotation = rand () % 360;

	trap_S_StartSound( pos, 0, 0, CG_MediaSfx( cgs.media.sfxRockexp ), 1, ATTN_NORM, 0 );
}

/*
===============
CG_Explosion2
===============
*/
void CG_Explosion2( vec3_t pos )
{
	lentity_t	*le;

	le = CG_AllocSprite ( LE_NO_FADE, pos, 62, 5, 
		1, 1, 1, 1,
		350, 1, 1, 0, 
		CG_MediaShader( cgs.media.shaderGrenadeExplosion ) );
	le->ent.rotation = rand () % 360;

	trap_S_StartSound ( pos, 0, 0, CG_MediaSfx( cgs.media.sfxGrenexp ), 1, ATTN_NORM, 0 );
}

/*
===============
CG_RocketExplosion
===============
*/
void CG_RocketExplosion( vec3_t pos, vec3_t dir )
{
	lentity_t	*le;

	le = CG_AllocSprite( LE_NO_FADE, pos, 64, 8, 
		1, 1, 1, 1,
		350, 1, 0.75, 0, 
		CG_MediaShader( cgs.media.shaderRocketExplosion ) );
	le->ent.rotation = rand () % 360;

	if( CG_PointContents( pos ) & MASK_WATER )
		trap_S_StartSound( pos, 0, 0, CG_MediaSfx( cgs.media.sfxWatrexp ), 1, ATTN_NORM, 0 );
	else
		trap_S_StartSound( pos, 0, 0, CG_MediaSfx( cgs.media.sfxRockexp ), 1, ATTN_NORM, 0 );

	CG_SpawnDecal( pos, dir, random()*360, 64, 1, 1, 1, 1, 10, 1, qfalse, CG_MediaShader( cgs.media.shaderExplosionMark ) );
}

/*
===============
CG_RocketTrail
===============
*/
void CG_RocketTrail( vec3_t start, vec3_t end )
{
	lentity_t	*le;
	float		len;
	vec3_t		vec;
	int			contents, oldcontents;

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	if( !len )
		return;

	contents = CG_PointContents( end );
	if( contents & MASK_WATER ) {
		oldcontents = CG_PointContents( start );
		if( oldcontents & MASK_WATER )
			CG_BubbleTrail( start, end, 8 );
		return;
	}

	le = CG_AllocSprite( LE_SCALE_ALPHA_FADE, end, 4, 10, 
		1, 1, 1, 0.33f,
		0, 0, 0, 0, 
		CG_MediaShader( cgs.media.shaderSmokePuff ) );
	VectorSet( le->velocity, -vec[0] * 5 + crandom()*5, -vec[1] * 5 + crandom()*5, -vec[2] * 5 + crandom()*5 + 3 );
	le->ent.rotation = rand () % 360;
}

/*
===============
CG_GrenadeExplosion
===============
*/
void CG_GrenadeExplosion( vec3_t pos, vec3_t dir )
{
	lentity_t	*le;

	le = CG_AllocSprite( LE_NO_FADE, pos, 64, 5, 
		1, 1, 1, 1,
		350, 1, 1, 0, 
		CG_MediaShader( cgs.media.shaderGrenadeExplosion ) );
	
	if( CG_PointContents( pos ) & MASK_WATER )
		trap_S_StartSound( pos, 0, 0, CG_MediaSfx( cgs.media.sfxWatrexp ), 1, ATTN_NORM, 0 );
	else
		trap_S_StartSound( pos, 0, 0, CG_MediaSfx( cgs.media.sfxGrenexp ), 1, ATTN_NORM, 0 );

	CG_SpawnDecal( pos, dir, random()*360, 64, 1, 1, 1, 1, 10, 1, qfalse, CG_MediaShader( cgs.media.shaderExplosionMark ) );
}

/*
===============
CG_GrenadeTrail
===============
*/
void CG_GrenadeTrail( vec3_t start, vec3_t end )
{
	lentity_t	*le;
	float		len;
	vec3_t		vec;
	int			contents, oldcontents;

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	if( !len )
		return;

	contents = CG_PointContents( end );

	if( contents & MASK_WATER ) {
		oldcontents = CG_PointContents( start );
		if( oldcontents & MASK_WATER )
			CG_BubbleTrail( start, end, 8 );
		return;
	}

	le = CG_AllocSprite( LE_SCALE_ALPHA_FADE, end, 3, 8, 
		0.6, 0.6, 0.6, 0.5,
		0, 0, 0, 0, 
		CG_MediaShader( cgs.media.shaderSmokePuff ) );
	VectorSet( le->velocity, -vec[0] * 5 + crandom()*5, -vec[1] * 5 + crandom()*5, -vec[2] * 5 + crandom()*5 + 3 );

	le->ent.rotation = rand () % 360;
}

/*
===============
CG_TeleportEffect
===============
*/
void CG_TeleportEffect( vec3_t org )
{
	lentity_t *le;

	le = CG_AllocPoly( LE_RGB_FADE, org, vec3_origin, 5, 
		1, 1, 1, 1,
		0, 0, 0, 0, 
		CG_MediaModel( cgs.media.modTeleportEffect ), 
		CG_MediaShader( cgs.media.shaderTeleportEffect ) );
	le->ent.origin[2] -= 24;
}

/*
===============
CG_BFGLaser
===============
*/
void CG_BFGLaser( vec3_t start, vec3_t end )
{
	lentity_t *le;

	le = CG_AllocLaser( start, end, 2, 2, 0, 0.85, 0, 0.3, CG_MediaShader( cgs.media.shaderLaser ) );
}

/*
===============
CG_BFGExplosion
===============
*/
void CG_BFGExplosion( vec3_t pos )
{
	lentity_t *le;

	le = CG_AllocPoly( LE_NO_FADE, pos, vec3_origin, 4, 
		1, 1, 1, 1, 
		350, 0, 1.0, 0, 
		CG_MediaModel( cgs.media.modBfgExplo ), 
		NULL );
}

/*
===============
CG_BFGBigExplosion
===============
*/
void CG_BFGBigExplosion( vec3_t pos )
{
	lentity_t *le;

	le = CG_AllocPoly( LE_NO_FADE, pos, vec3_origin, 6,  
		1, 1, 1, 1,
		700, 0, 1.0, 0, 
		CG_MediaModel( cgs.media.modBfgBigExplo ), 
		NULL );

	CG_BFGExplosionParticles( pos );
}

/*
=================
CG_SmallPileOfGibs
 accepts NULL as velocity
=================
*/
void CG_SmallPileOfGibs( vec3_t origin, int	count, vec3_t velocity )
{
	lentity_t	*le;
	int			i;
	float		mass = 60;
	vec3_t		angles;

	for( i = 0; i < count; i++ ) {

		le = CG_AllocPoly( LE_NO_FADE, origin, vec3_origin, 200 + 200*random(), 
			1, 1, 1, 1, 
			0, 0, 0, 0, 
			CG_MediaModel( cgs.media.modMeatyGib ), 
			NULL );

		// random rotation and scale variations
		VectorSet( angles, crandom()*360, crandom()*360, crandom()*360 );
		AnglesToAxis ( angles, le->ent.axis );
		le->ent.scale = 1.0 + (crandom()*0.25);
		
		if( velocity ) {
			// velocity brought by game + random variations
			le->velocity[0] = velocity[0] + crandom()*75;
			le->velocity[1] = velocity[1] + crandom()*75;
			le->velocity[2] = velocity[2] + crandom()*75;
		} else {
			vec3_t		dir;
			float		v;
			// pure random dir & velocity
			VectorSet( dir, crandom()*0.5, crandom()*0.5, random() );
			v = 100 + random() * 100;
			VectorSet( le->velocity, dir[0]*v, dir[1]*v, dir[2]*v );
		}

		// friction and gravity
		VectorSet( le->accel, -0.2, -0.2, -9.8 * mass );
		
		le->bounce = 35;
	}
}

/*
=================
CG_EjectBrass
=================
*/
void CG_EjectBrass( vec3_t origin, int	count, struct model_s *model )
{
	lentity_t	*le;
	int			i;
	float		mass = 40;
	vec3_t		angles;
	vec3_t		dir;
	float		v;

	if( !cg_ejectBrass->integer )
		return;

	for( i = 0; i < count; i++ ) {

		le = CG_AllocPoly( LE_NO_FADE, origin, vec3_origin, 50 + 50*random(), 
			1, 1, 1, 1, 
			0, 0, 0, 0, 
			model, 
			NULL );

		//random rotation
		VectorSet( angles, crandom()*360, crandom()*360, crandom()*360 );
		AnglesToAxis ( angles, le->ent.axis );
		
		//pure random dir & velocity
		VectorSet( dir, crandom()*0.25, crandom()*0.25, random() );
		v = 100 + random() * 25;
		VectorSet( le->velocity, dir[0]*v, dir[1]*v, dir[2]*v );
		

		//friction and gravity
		VectorSet( le->accel, -0.2, -0.2, -9.8 * mass );
		
		le->bounce = 60;
	}
}

/*
=================
CG_AddBeams
=================
*/
void CG_AddBeams( void )
{
	int			i;
	beam_t		*b;
	vec3_t		dist, org, angles, angles2;
	float		d;
	entity_t	ent;
	float		len, steps;
	float		model_length;
	
// update beams
	for( i = 0, b = cg_beams; i < MAX_BEAMS; i++, b++ ) {
		if( !b->model || b->endtime < cg.time )
			continue;

		// if coming from the player, update the start position
		if( b->entity == cgs.playerNum + 1 ) {	// entity 0 is the world
			VectorCopy( cg.refdef.vieworg, b->start );
			b->start[2] -= 22;				// adjust for view height
		}
		VectorAdd( b->start, b->offset, org );

		// calculate pitch and yaw
		VectorSubtract( b->end, org, dist );
		VecToAngles( dist, angles2 );

		// add new entities for the beams
		d = VectorNormalize( dist );

		memset( &ent, 0, sizeof(ent) );

		ent.scale = 1.0f;
		ent.color[0] = ent.color[1] = ent.color[2] = ent.color[3] = 255;

		if( b->model == CG_MediaModel( cgs.media.modLightning ) ) {
			model_length = 35.0;
			d -= 20.0;  // correction so it doesn't end in middle of tesla
		} else {
			model_length = 30.0;
		}

		steps = ceil( d / model_length);
		len = (d - model_length) / (steps - 1);

		// PMM - special case for lightning model .. if the real length is shorter than the model,
		// flip it around & draw it from the end to the start.  This prevents the model from going
		// through the tesla mine (instead it goes through the target)
		if( (b->model == CG_MediaModel( cgs.media.modLightning )) && (d <= model_length) ) {
			VectorCopy( b->end, ent.origin );
			VectorCopy( b->end, ent.lightingOrigin );
			VectorCopy( b->end, ent.origin2 );

			// offset to push beam outside of tesla model
			// (negative because dist is from end to start for this beam)
			ent.rtype = RT_MODEL;
			ent.model = b->model;
			ent.flags = RF_FULLBRIGHT|RF_NOSHADOW;
			angles[0] = angles2[0];
			angles[1] = angles2[1];
			angles[2] = rand()%360;
			AnglesToAxis( angles, ent.axis );
			CG_AddEntityToScene( &ent );
			return;
		}

		ent.rtype = RT_MODEL;
		ent.flags = RF_NOSHADOW;
		ent.model = b->model;

		while( d > 0 ) {
			VectorCopy( org, ent.origin );
			VectorCopy( org, ent.lightingOrigin );
			VectorCopy( org, ent.origin2 );

			if( b->model == CG_MediaModel( cgs.media.modLightning ) ) {
				angles[0] = -angles2[0];
				angles[1] = angles2[1] + 180.0;
				angles[2] = rand()%360;
			} else {
				angles[0] = angles2[0];
				angles[1] = angles2[1];
				angles[2] = rand()%360;
			}
			
			AnglesToAxis( angles, ent.axis );
			CG_AddEntityToScene( &ent );
			VectorMA( org, len, dist, org );
			d -= model_length;
		}
	}
}


/*
=================
CG_AddLocalEntities
=================
*/
void CG_AddLocalEntities( void )
{
	int			f;
	lentity_t	*le, *next, *hnode;
	entity_t	*ent;
	float		scale, frac, fade, time;
	float		backlerp, fps;

	fps = 1.0f / 100.0f;
	time = cg_paused->integer ? 0 : cg.frameTime;
	backlerp = 1.0f - cg.lerpfrac;

	hnode = &cg_localents_headnode;
	for( le = hnode->next; le != hnode; le = next ) {
		next = le->next;

		frac = (cg.time - le->start) * fps;
		f = ( int )floor( frac );
		f = max( f, 0 );

		// it's time to DIE
		if( f >= le->frames - 1 ) {
			le->type = LE_FREE;
			CG_FreeLocalEntity( le );
			continue;
		}

		if( le->frames > 1 ) {
			scale = 1.0f - frac / (le->frames - 1);
			scale = bound( 0.0f, scale, 1.0f );
			fade = scale * 255.0f;
		} else {
			scale = 1.0f;
			fade = 255.0f;
		}

		ent = &le->ent;

		if( le->light && scale )
			trap_R_AddLightToScene( ent->origin, le->light * scale, le->lightcolor[0], le->lightcolor[1], le->lightcolor[2], NULL );

		if( le->type == LE_LASER ) {
			CG_AddLaser( ent->origin, ent->origin2, ent->radius, ent->skinNum, ent->customShader );
			continue;
		}

		switch( le->type ) {
			case LE_NO_FADE:
				break;
			case LE_RGB_FADE:
				ent->color[0] = ( qbyte )( fade * le->color[0] );
				ent->color[1] = ( qbyte )( fade * le->color[1] );
				ent->color[2] = ( qbyte )( fade * le->color[2] );
				break;
			case LE_SCALE_ALPHA_FADE:
				ent->scale = 1.0f + 1.0f / scale;
				ent->scale = min( ent->scale, 5.0f );
			case LE_ALPHA_FADE:
				ent->color[3] = ( qbyte )( fade * le->color[3] );
				break;
			default:
				break;
		}

		ent->backlerp = backlerp;

		if( le->bounce ) {
			trace_t	trace;
			vec3_t	next_origin;

			VectorMA( ent->origin, time, le->velocity, next_origin );

			CG_Trace( &trace, ent->origin, debris_mins, debris_maxs, next_origin, 0, MASK_SOLID );
			if( trace.fraction != 1.0 ) {
				float	dot;
				vec3_t	vel;
				float	xzyspeed;
				float	timefraction = 10.0f / (float)( cg.time - le->oldtime );

				// reflect velocity
				VectorSubtract( next_origin, ent->origin, vel );
				dot = -2 * DotProduct( vel, trace.plane.normal );
				VectorMA( vel, dot, trace.plane.normal, le->velocity );

				// put new origin in the impact point and 1 unit out along the normal
				VectorMA( trace.endpos, 1, trace.plane.normal, ent->origin );

				// the entity has not speed enough. Stop checks
				xzyspeed = sqrt( le->velocity[0]*le->velocity[0] + le->velocity[1]*le->velocity[1] + le->velocity[2]*le->velocity[2] );
				if( xzyspeed * timefraction < 1.0f ) {
					trace_t traceground;
					vec3_t	ground_origin;

					//see if we have ground
					VectorCopy( ent->origin, ground_origin );
					ground_origin[2] += debris_mins[2] - 4;
					CG_Trace( &traceground, ent->origin, debris_mins, debris_maxs, ground_origin, 0, MASK_SOLID );
					if( traceground.fraction != 1.0 ) {
						le->bounce = qfalse;
						VectorClear(le->velocity);
						VectorClear(le->accel);
					}

				} else 
					VectorScale( le->velocity, le->bounce * timefraction, le->velocity );

			} else {
				VectorCopy( ent->origin, ent->origin2 );
				VectorCopy( next_origin, ent->origin );
			}

		} else {
			VectorCopy( ent->origin, ent->origin2 );
			VectorMA( ent->origin, time, le->velocity, ent->origin );
		}
		
		VectorCopy( ent->origin, ent->lightingOrigin );
		VectorMA( le->velocity, time, le->accel, le->velocity );
		CG_AddEntityToScene( ent );
		
		le->oldtime = cg.time;
	}
}
