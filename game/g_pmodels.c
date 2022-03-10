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
// - Adding the Player models in split pieces
// by Jalisko


#include "g_local.h"
#include "m_player.h"
#include "gs_pmodels.h"


/*
=================
G_SetNewAnimUpper
=================
*/
void G_SetNewAnimUpper( edict_t *ent )
{
	pmanim_t	*pmanim = &ent->pmAnim;

	
	if( pmanim->anim_moveflags & ANIMMOVE_DUCK ) {			// CROUCH
		if( ( pmanim->anim_moveflags & ANIMMOVE_WALK ) ||
			( pmanim->anim_moveflags & ANIMMOVE_RUN ) )
			pmanim->anim[UPPER] = TORSO_RUN;
		else
			pmanim->anim[UPPER] = TORSO_STAND;
	} else if( pmanim->anim_moveflags & ANIMMOVE_SWIM ) {	// SWIM
		pmanim->anim[UPPER] = TORSO_SWIM;
	} else if( pmanim->anim_jump ) {						// JUMP
		if( pmanim->anim_moveflags & ANIMMOVE_GRAPPLE )	
			pmanim->anim[UPPER] = TORSO_STAND;
		else
			pmanim->anim[UPPER] = TORSO_RUN;
	} else if( pmanim->anim_moveflags & ANIMMOVE_RUN ) {	// RUN
		pmanim->anim[UPPER] = TORSO_RUN;
	} else if( pmanim->anim_moveflags & ANIMMOVE_WALK ) {	// WALK
		pmanim->anim[UPPER] = TORSO_STAND;
	} else													// STAND
		pmanim->anim[UPPER] = TORSO_STAND;
}

/*
=================
G_SetNewAnimLower
=================
*/
void G_SetNewAnimLower( edict_t *ent )
{
	pmanim_t	*pmanim = &ent->pmAnim;

	if( pmanim->anim_moveflags & ANIMMOVE_DUCK ) {
		if( ( pmanim->anim_moveflags & ANIMMOVE_WALK ) ||
			( pmanim->anim_moveflags & ANIMMOVE_RUN ) ) {	// CROUCH
			pmanim->anim[LOWER] = LEGS_CRWALK;
		} else
			pmanim->anim[LOWER] = LEGS_CRSTAND;
	} else if( pmanim->anim_moveflags & ANIMMOVE_SWIM ) {	// SWIM
		if( pmanim->anim_moveflags & ANIMMOVE_FRONT ) {
			pmanim->anim[LOWER] = LEGS_SWIMFWD;
		} else 
			pmanim->anim[LOWER] = LEGS_SWIM;
	} else if( pmanim->anim_jump ) {						// JUMP
		if( pmanim->anim_moveflags & ANIMMOVE_GRAPPLE ) {	
			pmanim->anim[LOWER] = LEGS_JUMP3;
		} else {
			if( pmanim->anim_jump_style == 1 )
				pmanim->anim[LOWER] = LEGS_JUMP1;
			else if( pmanim->anim_jump_style == 2 )
				pmanim->anim[LOWER] = LEGS_JUMP2;
			else 
				pmanim->anim[LOWER] = LEGS_JUMP3;
		}
	} else if( pmanim->anim_moveflags & ANIMMOVE_RUN ) {	// RUN
		// front/backward has priority over side movements
		if( pmanim->anim_moveflags & ANIMMOVE_FRONT )
			pmanim->anim[LOWER] = LEGS_RUNFWD;
		else if( pmanim->anim_moveflags & ANIMMOVE_BACK )
			pmanim->anim[LOWER] = LEGS_RUNBACK;
		else if( pmanim->anim_moveflags & ANIMMOVE_RIGHT )
			pmanim->anim[LOWER] = LEGS_RUNRIGHT;
		else if( pmanim->anim_moveflags & ANIMMOVE_LEFT )
			pmanim->anim[LOWER] = LEGS_RUNLEFT;
		else 	// is moving by inertia
			pmanim->anim[LOWER] = LEGS_WALKFWD;	
	} else if (pmanim->anim_moveflags & ANIMMOVE_WALK ) {	// WALK
		// front/backward has priority over side movements
		if( pmanim->anim_moveflags & ANIMMOVE_FRONT )
			pmanim->anim[LOWER] = LEGS_WALKFWD;
		else if( pmanim->anim_moveflags & ANIMMOVE_BACK )
			pmanim->anim[LOWER] = LEGS_WALKBACK;
		else if( pmanim->anim_moveflags & ANIMMOVE_RIGHT )
			pmanim->anim[LOWER] = LEGS_WALKRIGHT;
		else if( pmanim->anim_moveflags & ANIMMOVE_LEFT )
			pmanim->anim[LOWER] = LEGS_WALKLEFT;
		else 	// is moving by inertia
			pmanim->anim[LOWER] = LEGS_WALKFWD;
	} else													// STAND
		pmanim->anim[LOWER] = LEGS_STAND;
}

/*
=================
G_SetNewAnim
=================
*/
void G_SetNewAnim( edict_t *ent )
{
	int			part;
	pmanim_t	*pmanim = &ent->pmAnim;

	pmanim->anim_jump_thunk = qtrue;
	for( part = 0; part < PMODEL_PARTS; part++ ) {
		if( pmanim->anim_priority[part] > ANIM_ATTACK )
			continue;
		
		switch( part )	{
			case LOWER:
				G_SetNewAnimLower( ent );
				break;
			case UPPER:
				G_SetNewAnimUpper( ent );
				break;
			case HEAD:
			default:
				pmanim->anim[part] = 0;
				break;
		}
	}
}

/*
===================
Anim_CheckJump
===================
*/
void Anim_CheckJump( edict_t *ent )
{
	vec3_t		point;
	trace_t		trace;
	pmanim_t	*pmanim = &ent->pmAnim;

	// not jumping anymore?
	if( ent->groundentity ) {	
		pmanim->anim_jump_thunk = qfalse;
		pmanim->anim_jump = qfalse;
		return;
	}

	if( ( pmanim->anim_priority[LOWER] > ANIM_ATTACK ) )
		return;
	if( ent->velocity[2] > -80 )
		return;
	if( pmanim->anim_jump_prestep )	// I already did.
		return;

	point[0] = ent->s.origin[0];
	point[1] = ent->s.origin[1];
	point[2] = ent->s.origin[2] + ( 0.025 * ent->velocity[2] );
	trap_Trace( &trace, ent->s.origin, ent->r.mins, ent->r.maxs, point, ent, MASK_PLAYERSOLID );

	if( trace.plane.normal[2] < 0.7 && !trace.startsolid )
		return;

	// found solid. Get ready to land
	if( pmanim->anim_jump_style == 1 )
		pmanim->anim[LOWER] = LEGS_JUMP1ST;
	else if( pmanim->anim_jump_style == 2 )
		pmanim->anim[LOWER] = LEGS_JUMP2ST;
	else 
		pmanim->anim[LOWER] = LEGS_JUMP3ST;
	pmanim->anim_jump_prestep = qtrue;
}

/*
===================
AnimIsStep
===================
*/
#define STEPSIZE 18 // fixme: another STEPSIZE define
qboolean Anim_IsStep( edict_t *ent )
{
	vec3_t		point;
	trace_t		trace;
	
	point[0] = ent->s.origin[0];
	point[1] = ent->s.origin[1];
	point[2] = ent->s.origin[2] - ( 1.6 * STEPSIZE );
	trap_Trace( &trace, ent->s.origin, ent->r.mins, ent->r.maxs, point, ent, MASK_PLAYERSOLID );
	
	if( trace.plane.normal[2] < 0.7 && !trace.startsolid )
		return qfalse;
	
	// found solid
	return qtrue;
}

/*
===================
AnimIsSwim
===================
*/
qboolean AnimIsSwim( edict_t *ent )
{
	if( ent->waterlevel > 2 )
		return qtrue;

	if( ent->waterlevel && !ent->groundentity ) {
		if( !Anim_IsStep( ent ) )
			return qtrue;
	}
	return qfalse;
}

/*
=================
G_SetPModelFrame
=================
*/
void G_SetPModelFrame (edict_t *ent)
{
	pmanim_t	*pmanim = &ent->pmAnim;
	qboolean	updateanims = qfalse;

	// finish updating the moveflags
	if( AnimIsSwim( ent ) )
		pmanim->anim_moveflags |= ANIMMOVE_SWIM;

	// jump is special
	if( pmanim->anim_jump && ( ent->groundentity ||
		pmanim->anim_moveflags & ANIMMOVE_SWIM ) ) { // JUMP stop
		pmanim->anim_jump = qfalse;
		pmanim->anim_jump_thunk = qfalse;
	} else if( !ent->groundentity && !pmanim->anim_jump &&
		!( pmanim->anim_moveflags & ANIMMOVE_SWIM ) ) { // falling
		if( !Anim_IsStep( ent ) ){
			pmanim->anim_jump_style = 0;
			pmanim->anim_jump = qtrue;
			updateanims = qtrue; // activate update
		}
	}

#ifdef _ANIM_PRESTEP
	if( pmanim->anim_jump )
		Anim_CheckJump( ent );
#endif

	// see if we need to update based on the new moveflags
	if( pmanim->anim_jump_thunk == qfalse && !( pmanim->anim_moveflags & ANIMMOVE_SWIM ) ) {
		updateanims = qtrue;
		pmanim->anim_jump_prestep = qfalse;
	
	} else if( pmanim->anim_moveflags != pmanim->anim_oldmoveflags )
		updateanims = qtrue;

	if( updateanims )
		G_SetNewAnim( ent );

	// set animations
	ent->s.frame = ( (pmanim->anim[LOWER] &0x3F) | (pmanim->anim[UPPER] &0x3F)<<6 | (pmanim->anim[HEAD] &0xF)<<12 );

	pmanim->anim_oldmoveflags = pmanim->anim_moveflags;
}

/*
=================
G_SetClientFrame
=================
*/
void G_SetClientFrame( edict_t *ent )
{
	if( (!ent->r.client) || (ent->s.modelindex != 255) || (ent->r.svflags & SVF_NOCLIENT) )
		return;
	G_SetPModelFrame( ent );
}
