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

#define MAX_DECALS				256
#define MAX_DECAL_VERTS			128
#define MAX_DECAL_FRAGMENTS		64

typedef struct cdecal_s
{
	struct cdecal_s *prev, *next;
	int			die;				// stop lighting after this time
	int			fadetime;
	float		fadefreq;
	qboolean	fadealpha;
	float		color[4];
	struct shader_s *shader;

	poly_t			poly;
	vec3_t			verts[MAX_POLY_VERTS];
	vec2_t			stcoords[MAX_POLY_VERTS];
	byte_vec4_t		colors[MAX_POLY_VERTS];
} cdecal_t;

cdecal_t	cg_decals[MAX_DECALS];
cdecal_t	cg_decals_headnode, *cg_free_decals;

/*
=================
CG_ClearDecals
=================
*/
void CG_ClearDecals( void )
{
	int i;

	memset( cg_decals, 0, sizeof( cg_decals ) );

	// link decals
	cg_free_decals = cg_decals;
	cg_decals_headnode.prev = &cg_decals_headnode;
	cg_decals_headnode.next = &cg_decals_headnode;
	for( i = 0; i < MAX_DECALS - 1; i++ )
		cg_decals[i].next = &cg_decals[i+1];
}

/*
=================
CG_AllocDecal

Returns either a free decal or the oldest one
=================
*/
cdecal_t *CG_AllocDecal( void )
{
	cdecal_t *dl;

	if ( cg_free_decals ) {	// take a free decal if possible
		dl = cg_free_decals;
		cg_free_decals = dl->next;
	} else {				// grab the oldest one otherwise
		dl = cg_decals_headnode.prev;
		dl->prev->next = dl->next;
		dl->next->prev = dl->prev;
	}

	// put the decal at the start of the list
	dl->prev = &cg_decals_headnode;
	dl->next = cg_decals_headnode.next;
	dl->next->prev = dl;
	dl->prev->next = dl;

	return dl;
}

/*
=================
CG_FreeDecal
=================
*/
void CG_FreeDecal( cdecal_t *dl )
{
	// remove from linked active list
	dl->prev->next = dl->next;
	dl->next->prev = dl->prev;

	// insert into linked free list
	dl->next = cg_free_decals;
	cg_free_decals = dl;
}

/*
=================
CG_SpawnDecal
=================
*/
void CG_SpawnDecal( vec3_t origin, vec3_t dir, float orient, float radius,
				 float r, float g, float b, float a, float die, float fadetime, qboolean fadealpha, struct shader_s *shader )
{
	int i, j;
	vec3_t axis[3];
	vec3_t verts[MAX_DECAL_VERTS];
	byte_vec4_t color;
	fragment_t *fr, fragments[MAX_DECAL_FRAGMENTS];
	int numfragments;
	cdecal_t *dl;

	if( !cg_addDecals->integer )
		return;

	// invalid decal
	if( radius <= 0 || VectorCompare( dir, vec3_origin ) )
		return;

	// calculate orientation matrix
	VectorNormalize2( dir, axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], orient );
	CrossProduct( axis[0], axis[2], axis[1] );

	numfragments = trap_R_GetClippedFragments( origin, radius, axis, // clip it
		MAX_DECAL_VERTS, verts, MAX_DECAL_FRAGMENTS, fragments );

	// no valid fragments
	if( !numfragments )
		return;

	color[0] = (qbyte)( r*255 );
	color[1] = (qbyte)( g*255 );
	color[2] = (qbyte)( b*255 );
	color[3] = (qbyte)( a*255 );

	VectorScale( axis[1], 0.5f / radius, axis[1] );
	VectorScale( axis[2], 0.5f / radius, axis[2] );

	for( i = 0, fr = fragments; i < numfragments; i++, fr++ ) {
		if( fr->numverts > MAX_POLY_VERTS )
			fr->numverts = MAX_POLY_VERTS;
		else if( fr->numverts <= 0 )
			continue;

		// allocate decal
		dl = CG_AllocDecal ();
		dl->die = cg.time + die * 1000;
		dl->fadetime = cg.time + (die - min( fadetime, die )) * 1000;
		dl->fadefreq = 0.001f / min( fadetime, die );
		dl->fadealpha = fadealpha;
		dl->shader = shader;
		dl->color[0] = r; 
		dl->color[1] = g;
		dl->color[2] = b;
		dl->color[3] = a;
		dl->poly.numverts = fr->numverts;
		dl->poly.colors = dl->colors;
		dl->poly.verts = dl->verts;
		dl->poly.stcoords = dl->stcoords;
		dl->poly.shader = dl->shader;

		for( j = 0; j < fr->numverts; j++ ) {
			vec3_t v;

			VectorCopy( verts[fr->firstvert+j], dl->verts[j] );
			VectorSubtract( dl->verts[j], origin, v );
			dl->stcoords[j][0] = DotProduct( v, axis[1] ) + 0.5f;
			dl->stcoords[j][1] = DotProduct( v, axis[2] ) + 0.5f;
			*(int *)dl->colors[j] = *(int *)color;
		}
	}
}

/*
=================
CG_AddDecals
=================
*/
void CG_AddDecals( void )
{
	int			i;
	float		fade;
	cdecal_t	*dl, *next, *hnode;
	byte_vec4_t color;

	// add decals in first-spawed - first-drawn order
	hnode = &cg_decals_headnode;
	for( dl = hnode->prev; dl != hnode; dl = next ) {
		next = dl->prev;

		// it's time to DIE
		if( dl->die <= cg.time ) {
			CG_FreeDecal( dl );
			continue;
		}

		// fade out
		if( dl->fadetime < cg.time ) {
			fade = (dl->die - cg.time) * dl->fadefreq;

			if( dl->fadealpha ) {
				color[0] = (qbyte)( dl->color[0]*255 );
				color[1] = (qbyte)( dl->color[1]*255 );
				color[2] = (qbyte)( dl->color[2]*255 );
				color[3] = (qbyte)( dl->color[3]*255*fade );
			} else {
				color[0] = (qbyte)( dl->color[0]*255*fade );
				color[1] = (qbyte)( dl->color[1]*255*fade );
				color[2] = (qbyte)( dl->color[2]*255*fade );
				color[3] = (qbyte)( dl->color[3]*255 );
			}

			for( i = 0; i < dl->poly.numverts; i++ )
				*(int *)dl->colors[i] = *(int *)color;
		}

		trap_R_AddPolyToScene( &dl->poly );
	}
}

