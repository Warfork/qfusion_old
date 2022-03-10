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
// cg_effects.c -- entity effects parsing and management

#include "cg_local.h"

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/

typedef struct cdlight_s
{
	struct cdlight_s *prev, *next;
	vec3_t	color;
	vec3_t	origin;
	float	radius;
} cdlight_t;

cdlight_t		cg_dlights[MAX_DLIGHTS];
cdlight_t		cg_dlights_headnode, *cg_free_dlights;

/*
================
CG_ClearDlights
================
*/
void CG_ClearDlights (void)
{
	int i;

	memset ( cg_dlights, 0, sizeof(cg_dlights) );

	// link dynamic lights
	cg_free_dlights = cg_dlights;
	cg_dlights_headnode.prev = &cg_dlights_headnode;
	cg_dlights_headnode.next = &cg_dlights_headnode;
	for ( i = 0; i < MAX_DLIGHTS - 1; i++ ) {
		cg_dlights[i].next = &cg_dlights[i+1];
	}
}

/*
===============
CG_AllocDlight
===============
*/
void CG_AllocDlight ( float radius, const vec3_t origin, const vec3_t color )
{
	cdlight_t	*dl;

	if ( radius <= 0 ) {
		return;
	}

	if ( cg_free_dlights ) {	// take a free light if possible
		dl = cg_free_dlights;
		cg_free_dlights = dl->next;
	} else {					// grab the oldest one otherwise
		dl = cg_dlights_headnode.prev;
		dl->prev->next = dl->next;
		dl->next->prev = dl->prev;
	}

	dl->radius = radius;
	VectorCopy ( origin, dl->origin );
	VectorCopy ( color, dl->color );

	// put the light at the start of the list
	dl->prev = &cg_dlights_headnode;
	dl->next = cg_dlights_headnode.next;
	dl->next->prev = dl;
	dl->prev->next = dl;
}

/*
=================
CG_FreeDlight
=================
*/
static void CG_FreeDlight ( cdlight_t *dl )
{
	// remove from linked active list
	dl->prev->next = dl->next;
	dl->next->prev = dl->prev;

	// insert into linked free list
	dl->next = cg_free_dlights;
	cg_free_dlights = dl;
}

/*
===============
CG_AddDlights
===============
*/
void CG_AddDlights (void)
{
	cdlight_t	*dl, *hnode, *next;

	hnode = &cg_dlights_headnode;
	for ( dl = hnode->next; dl != hnode; dl = next )
	{
		next = dl->next;

		CG_AddLight ( dl->origin, dl->radius, dl->color[0], dl->color[1], dl->color[2] );
		CG_FreeDlight ( dl );
	}
}

/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/

typedef struct particle_s
{
	float		time;

	vec3_t		org;
	vec3_t		vel;
	vec3_t		accel;
	vec3_t		color;
	float		alpha;
	float		alphavel;
	float		scale;

	poly_t		poly;
	vec3_t		pVerts[4];
	vec2_t		pStcoords[4];
	byte_vec4_t	pColor[4];
} cparticle_t;

#define	PARTICLE_GRAVITY	40

#define MAX_PARTICLES		2048

static vec3_t avelocities [NUMVERTEXNORMALS];

cparticle_t	particles[MAX_PARTICLES], *free_particles[MAX_PARTICLES];
int			cg_numparticles;

/*
===============
CG_ClearParticles
===============
*/
void CG_ClearParticles (void)
{
	cg_numparticles = 0;
	memset ( particles, 0, sizeof(cparticle_t) * MAX_PARTICLES );
	memset ( free_particles, 0, sizeof(cparticle_t *) * MAX_PARTICLES );
}

inline cparticle_t *new_particle (void)
{
	cparticle_t	*p;

	if ( cg_numparticles >= MAX_PARTICLES ) {
		return NULL;
	}

	p = &particles[cg_numparticles++];
	p->time = cg.time;
	p->scale = 1.0f;
	p->alpha = 1.0f;
	VectorSet ( p->color, 0, 0, 0 );
	p->pStcoords[0][0] = 0; p->pStcoords[0][1] = 1;
	p->pStcoords[1][0] = 0; p->pStcoords[1][1] = 0;
	p->pStcoords[2][0] = 1; p->pStcoords[2][1] = 0;
	p->pStcoords[3][0] = 1; p->pStcoords[3][1] = 1;

	return p;
}

/*
===============
CG_ParticleEffect

Wall impact puffs
===============
*/
void CG_ParticleEffect ( vec3_t org, vec3_t dir, float r, float g, float b, int count )
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for ( i = 0; i < count ; i++ )
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		p->color[0] = r + random()*0.1;
		p->color[1] = g + random()*0.1;
		p->color[2] = b + random()*0.1;

		d = rand()&31;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()&7) - 4) + d * dir[j];
			p->vel[j] = crandom() * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / (0.5 + random() * 0.3);
	}
}


/*
===============
CG_ParticleEffect2
===============
*/
void CG_ParticleEffect2 ( vec3_t org, vec3_t dir, float r, float g, float b, int count )
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for ( i = 0; i < count; i++ )
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
			p->vel[j] = crandom() * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / (0.5 + random() * 0.3);
	}
}

/*
===============
CG_BigTeleportParticles
===============
*/
void CG_BigTeleportParticles ( vec3_t org )
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

		p->org[0] = org[0] + cos( angle ) * dist;
		p->vel[0] = cos( angle ) * (70 + (rand() & 63));
		p->accel[0] = -cos( angle ) * 100;

		p->org[1] = org[1] + sin(angle) * dist;
		p->vel[1] = sin( angle ) * (70 + (rand() & 63));
		p->accel[1] = -sin( angle ) * 100;

		p->org[2] = org[2] + 8 + (rand()%90);
		p->vel[2] = -100 + (rand() & 31);
		p->accel[2] = PARTICLE_GRAVITY * 4;

		p->alphavel = -0.3 / (0.5 + random() * 0.3);
	}
}


/*
===============
CG_BlasterParticles

Wall impact puffs
===============
*/
void CG_BlasterParticles ( vec3_t org, vec3_t dir )
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

		p->color[0] = 1.0f;
		p->color[1] = 0.8f;

		d = rand() & 15;
		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + ((rand()&7) - 4) + d * dir[j];
			p->vel[j] = dir[j] * 30 + crandom() * 40;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / (0.5 + random() * 0.3);
	}
}

/*
===============
CG_BlasterTrail
===============
*/
void CG_BlasterTrail ( vec3_t start, vec3_t end )
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy ( start, move );
	VectorSubtract ( end, start, vec );
	len = VectorNormalize ( vec );

	dec = 5;
	VectorScale (vec, dec, vec);

	while ( len > 0 )
	{
		len -= dec;

		if ( !(p = new_particle ()) ) {
			return;
		}
		VectorClear ( p->accel );
		
		p->alphavel = -1.0 / (0.3 + random() * 0.2);
		p->color[0] = 1.0f;
		p->color[1] = 0.7f;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = move[j] + crandom();
			p->vel[j] = crandom() * 5;
			p->accel[j] = 0;
		}

		VectorAdd ( move, vec, move );
	}
}

/*
===============
CG_FlagTrail
===============
*/
void CG_FlagTrail ( vec3_t start, vec3_t end, int effect )
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;
	vec3_t		color;

	VectorCopy ( start, move );
	VectorSubtract ( end, start, vec );
	len = VectorNormalize ( vec );

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

		VectorCopy ( color, p->color );
		
		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = move[j] + crandom() * 16;
			p->vel[j] = crandom() * 5;
			p->accel[j] = 0;
		}

		p->alphavel = -1.0 / (0.8 + random() * 0.2);

		VectorAdd ( move, vec, move );
	}
}

/*
===============
CG_BloodTrail
===============
*/
void CG_BloodTrail ( vec3_t start, vec3_t end )
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;

	dec = 4;
	VectorCopy ( start, move );
	VectorSubtract ( end, start, vec );
	len = VectorNormalize ( vec );

	while (len > 0)
	{
		len -= dec;

		if ( !(p = new_particle ()) ) {
			return;
		}

		p->scale = 1.5f;
		p->color[0] = 0.9 + crandom() * 0.5;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crandom();
			p->vel[j] = crandom() * 5;
			p->accel[j] = 0;
		}

		p->alphavel = -1.0 / (1 + random() * 0.4);
		p->vel[2] -= PARTICLE_GRAVITY;

		VectorAdd (move, vec, move);
	}
}

/*
===============
CG_RailTrail
===============
*/
void CG_RailTrail ( vec3_t start, vec3_t end )
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

	VectorSubtract ( end, start, vec );
	len = VectorNormalize ( vec );
	MakeNormalVectors ( vec, right, up );

	i = 0;
	dec = 3.2f;
	VectorScale ( vec, dec, vec );
	VectorCopy ( start, move );

	while ( len > 0 )
	{
		len -= dec;
		if ( !(p = new_particle ()) ) {
			return;
		}
		
		VectorClear ( p->accel );

		d = i++ * 0.1;
		c = cos (d);
		s = sin (d);

		VectorScale (right, c, dir);
		VectorMA (dir, s, up, dir);

		if ( random() > 0.9 ) {
			p->color[0] = 0.7 + crandom()*0.1;
			p->color[1] = 0.3;
			p->color[2] = 0.4 + crandom()*0.1;
		} else {
			p->color[0] = 0.3 + crandom()*0.1;
			p->color[1] = 0.3;
			p->color[2] = 0.8 + crandom()*0.1;
		}

		p->scale = 2.0f;

		p->alphavel = -1.0 / (1 + random()*0.2);
		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = move[j] + dir[j]*3;
			p->vel[j] = dir[j]*6;
		}

		VectorAdd ( move, vec, move );
	}

	VectorSubtract ( end, start, vec );
	len = VectorNormalize ( vec );

	dec = 2.6f;
	VectorScale ( vec, dec, vec );
	VectorCopy ( start, move );

	while ( len > 0 )
	{
		len -= dec;

		if ( !(p = new_particle ()) ) {
			return;
		}

		VectorClear ( p->accel );

		p->scale = 1.2f;

		p->alphavel = -1.0 / (0.6 + random()*0.2);
		p->color[0] = 0.7f + crandom()*0.1;
		p->color[1] = 0.7f + crandom()*0.1;
		p->color[2] = 0.7f + crandom()*0.1;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = move[j];
			p->vel[j] = crandom()*3;
			p->accel[j] = 0;
		}

		VectorAdd ( move, vec, move );
	}
}


#define	BEAMLENGTH			16

/*
===============
CG_FlyParticles
===============
*/
void CG_FlyParticles ( vec3_t origin, int count )
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward, dir;
	float		dist;
	float		ltime;

	if (count > NUMVERTEXNORMALS)
		count = NUMVERTEXNORMALS;

	if ( !avelocities[0][0] ) {
		for ( i = 0; i < NUMVERTEXNORMALS*3; i++ ) {
			avelocities[0][i] = (rand()&255) * 0.01;
		}
	}

	ltime = (float)cg.time / 1000.0;
	for ( i = 0; i < count; i += 2 )
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		angle = ltime * avelocities[i][0];
		sy = sin( angle );
		cy = cos( angle );
		angle = ltime * avelocities[i][1];
		sp = sin( angle );
		cp = cos( angle );
		angle = ltime * avelocities[i][2];
		sr = sin( angle );
		cr = cos( angle );
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		dist = sin ( ltime + i ) * 64;
		ByteToDir ( i, dir );
		p->org[0] = origin[0] + dir[0]*dist + forward[0]*BEAMLENGTH;
		p->org[1] = origin[1] + dir[1]*dist + forward[1]*BEAMLENGTH;
		p->org[2] = origin[2] + dir[2]*dist + forward[2]*BEAMLENGTH;

		VectorClear ( p->vel );
		VectorClear ( p->accel );
		p->alphavel = -100;
	}
}

/*
===============
CG_FlyEffect
===============
*/
void CG_FlyEffect ( centity_t *ent, vec3_t origin )
{
	int		n;
	int		count;
	int		starttime;

	if ( ent->fly_stoptime < cg.time ) {
		starttime = cg.time;
		ent->fly_stoptime = cg.time + 60000;
	} else {
		starttime = ent->fly_stoptime - 60000;
	}

	n = cg.time - starttime;
	if ( n < 20000 ) {
		count = n * 162 / 20000.0;
	} else {
		n = ent->fly_stoptime - cg.time;
		if ( n < 20000 ) {
			count = n * 162 / 20000.0;
		} else {
			count = 162;
		}
	}

	CG_FlyParticles ( origin, count );
}


/*
===============
CG_BfgParticles
===============
*/
void CG_BfgParticles ( vec3_t origin )
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward, dir;
	float		dist;
	vec3_t		v;
	float		ltime;
	
	if ( !avelocities[0][0] )
	{
		for ( i = 0; i < NUMVERTEXNORMALS*3; i++ ) {
			avelocities[0][i] = (rand()&255) * 0.01;
		}
	}

	ltime = (float)cg.time / 1000.0;
	for ( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		if ( !(p = new_particle ()) ) {
			return;
		}

		angle = ltime * avelocities[i][0];
		sy = sin ( angle );
		cy = cos ( angle );
		angle = ltime * avelocities[i][1];
		sp = sin ( angle );
		cp = cos ( angle );
		angle = ltime * avelocities[i][2];
		sr = sin ( angle );
		cr = cos ( angle );
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		dist = sin ( ltime + i ) * 64;
		ByteToDir ( i, dir );
		p->org[0] = origin[0] + dir[0]*dist + forward[0]*BEAMLENGTH;
		p->org[1] = origin[1] + dir[1]*dist + forward[1]*BEAMLENGTH;
		p->org[2] = origin[2] + dir[2]*dist + forward[2]*BEAMLENGTH;

		VectorClear ( p->vel );
		VectorClear ( p->accel );

		VectorSubtract ( p->org, origin, v );
		dist = VectorLength ( v ) / 90.0;

		p->color[1] = 1.5f * dist;
		clamp ( p->color[1], 0.0f, 1.0f );

		p->scale = 1.5f;
		p->alpha = 1.0f - dist;
		p->alphavel = -100;
	}
}

/*
===============
CG_BFGExplosionParticles
===============
*/
void CG_BFGExplosionParticles ( vec3_t org )
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

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + ((rand() & 31) - 16);
			p->vel[j] = (rand() & 383) - 192;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -0.8 / (0.5 + random() * 0.3);
	}
}

/*
===============
CG_AddParticles
===============
*/
void CG_AddParticles (void)
{
	int				i, j;
	cparticle_t		*p;
	float			alpha;
	float			time, time2;
	vec3_t			org;
	vec3_t			corner;
	byte_vec4_t		color;
	struct shader_s *shader;
	int				maxparticle, activeparticles;

	if ( !cg_numparticles ) {
		return;
	}

	maxparticle = -1;
	activeparticles = 0;
	shader = CG_MediaShader ( cgs.media.shaderParticle );

	for ( i = 0, j = 0, p = particles; i < cg_numparticles; i++, p++ )
	{
		time = (cg.time - p->time) * 0.001f;
		alpha = p->alpha + time * p->alphavel;

		if ( alpha <= 0 || p->scale <= 0 ) {	// faded out
			free_particles[j++] = p;
			continue;
		}

		maxparticle = i;
		activeparticles++;

		time2 = time * time * 0.5f;

		org[0] = p->org[0] + p->vel[0]*time + p->accel[0]*time2;
		org[1] = p->org[1] + p->vel[1]*time + p->accel[1]*time2;
		org[2] = p->org[2] + p->vel[2]*time + p->accel[2]*time2;

		color[0] = (qbyte) ( bound(0, p->color[0], 1.0f)*255 );
		color[1] = (qbyte) ( bound(0, p->color[1], 1.0f)*255 );
		color[2] = (qbyte) ( bound(0, p->color[2], 1.0f)*255 );
		color[3] = (qbyte) ( bound(0, alpha, 1.0f)*255 );

		*(int *)p->pColor[0] = *(int *)color;
		*(int *)p->pColor[1] = *(int *)color;
		*(int *)p->pColor[2] = *(int *)color;
		*(int *)p->pColor[3] = *(int *)color;

		corner[0] = org[0];
		corner[1] = org[1] - 0.5f * p->scale;
		corner[2] = org[2] - 0.5f * p->scale;

		VectorSet ( p->pVerts[0], corner[0], corner[1] + p->scale, corner[2] + p->scale );
		VectorSet ( p->pVerts[1], corner[0], corner[1], corner[2] + p->scale );
		VectorSet ( p->pVerts[2], corner[0], corner[1], corner[2] );
		VectorSet ( p->pVerts[3], corner[0], corner[1] + p->scale, corner[2] );

		p->poly.numverts = 4;
		p->poly.verts = p->pVerts;
		p->poly.stcoords = p->pStcoords;
		p->poly.colors = p->pColor;
		p->poly.shader = shader;

		CG_AddPoly ( &p->poly );
	}

	i = 0;
	while ( maxparticle >= activeparticles ) 
	{
		*free_particles[i++] = particles[maxparticle--];

		while ( maxparticle >= activeparticles )
		{
			p = &particles[maxparticle];
			time = (cg.time - p->time) * 0.001f;
			alpha = p->alpha + time * p->alphavel;

			if ( alpha <= 0 ) {
				maxparticle--;
			} else {
				break;
			}
		}
	}

	cg_numparticles = activeparticles;
}

/*
==============
CG_ClearEffects

==============
*/
void CG_ClearEffects (void)
{
	CG_ClearParticles ();
	CG_ClearDlights ();
}
