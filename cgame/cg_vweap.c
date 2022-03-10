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


// - Adding the View Weapon in split pieces
// by Jalisko

#include "cg_local.h"
#include "../game/gs_pmodels.h"

viewweaponinfo_t	vweap;

/*
======================================================================
			In Eyes Chasecam
======================================================================
*/

/*
===============
CG_ChaseHack
===============
*/
void CG_ChaseHack( void )
{	
	player_state_t *ps = &cg.frame.playerState;
	int			current;
	usercmd_t	cmd;

	// chasecam uses PM_FREEZE, as demo-play does
	if( ps->pmove.pm_type == PM_FREEZE && !cgs.attractLoop ) { 
		cg.chasedNum = ps->stats[STAT_CHASE] - 1; // minus one for parallel with cgs.playerNum
		
		current = trap_NET_GetCurrentUserCmdNum ();
		trap_NET_GetUserCmd( current, &cmd );
		if( cmd.buttons & BUTTON_ATTACK && cg.time > chaseCam.cmd_mode_delay ) {
			chaseCam.mode = ( chaseCam.mode != CAM_THIRDPERSON );
			chaseCam.cmd_mode_delay = cg.time + 200;
		}
	} else if( cgs.attractLoop ) {	// playing demo
		if( ps->stats[STAT_CHASE] > 0 ) // was recorded in chasecam
			cg.chasedNum = ps->stats[STAT_CHASE] - 1;
		else
			cg.chasedNum = cgs.playerNum;

		current = trap_NET_GetCurrentUserCmdNum ();
		trap_NET_GetUserCmd( current, &cmd );
		if(cmd.buttons & BUTTON_ATTACK && cg.time > chaseCam.cmd_mode_delay) {
			chaseCam.mode = ( chaseCam.mode != CAM_THIRDPERSON );
			chaseCam.cmd_mode_delay = cg.time + 200;
		}
	} else { 
		cg.chasedNum = cgs.playerNum;
		chaseCam.mode = qfalse;
	}

	// determine if we have to draw the view weapon
	if( ps->pmove.pm_type == PM_SPECTATOR ) {	// when in spectator, the entity is 
		cg.thirdPerson = qfalse;				// not updated, so cg.thirdPerson is not removed
		vweap.active = qfalse;
	} else if( !cg_gun->integer )
		vweap.active = qfalse;
	else
		vweap.active = ( cg.thirdPerson == qfalse );
}

/*
======================================================================
			ViewWeapon
======================================================================
*/

/*
================
CG_CalcViewOnGround

Store if the player is on ground or swimming. These checks are for 
animation or view effects purposes. Are not trustee for other matters.
================
*/
#define STEPSIZE 18 // fixme: another STEPSIZE define
void CG_CalcViewOnGround( void )
{
	vec3_t			lerpedorigin;
	vec3_t			point;
	trace_t			trace;
	int				i;
	centity_t		*cent;
	static vec3_t	fmins = { -15, -15, -24 };	// build a fake bbox (doh)
	static vec3_t	fmaxs = { 15, 15, 15 };		// crouched: I'm not interested in the upper part
	int				contents;
	player_state_t	*ps, *ops;

	// find the previous frame to interpolate from
	ps = &cg.frame.playerState;
	ops = vweap.ops;
	vweap.isSwim = qfalse;

	// using the entity
	cent = &cg_entities[cg.chasedNum+1];
	if( !cent )
		return;

	if( !cg.thirdPerson && cg.chasedNum == cgs.playerNum ) {
		float		backlerp;
		float		lerp;

		// predicted
		lerp = cg.lerpfrac;
		if( cg_predict->integer && !( cg.frame.playerState.pmove.pm_flags & PMF_NO_PREDICTION ) && !cg.thirdPerson ) {
			backlerp = 1.0f - lerp;
			for( i = 0; i < 3; i++ )
				lerpedorigin[i] = cg.predictedOrigin[i] - backlerp * cg.predictionError[i];
		} else {
			// interpolated
			for( i = 0; i < 3; i++ )
				lerpedorigin[i] = ops->pmove.origin[i]*(1.0/16.0) 
				+ lerp * (ps->pmove.origin[i]*(1.0/16.0) 
				- (ops->pmove.origin[i]*(1.0/16.0) ) );
		}

		// check water
		contents = CG_PointContents( cg.refdef.vieworg );
		if( contents & MASK_WATER )
			vweap.isSwim = qtrue;
	} else {
		// using packet entity (for chased players)
		for( i = 0; i < 3; i++ )
			lerpedorigin[i] = cent->prev.origin[i] + cg.lerpfrac * 
			( cent->current.origin[i] - cent->prev.origin[i] );

		// in water check
		contents = CG_PointContents( lerpedorigin );
		if( contents & MASK_WATER )
			vweap.isSwim = qtrue;
	}

	lerpedorigin[2] += fmins[2];	// send origin to lowest of bbox
	point[0] = lerpedorigin[0];
	point[1] = lerpedorigin[1];
	point[2] = lerpedorigin[2] - STEPSIZE*0.5;
	CG_Trace( &trace, lerpedorigin, fmins, fmaxs, point, cent->current.number, MASK_PLAYERSOLID );
	if( trace.plane.normal[2] < 0.7 && !trace.startsolid ) {
		vweap.isOnGround = qfalse;
		return;
	}

	// found solid
	vweap.isOnGround = qtrue;
}

/*
==============
CG_CalcGunOffset
==============
*/
void CG_CalcGunOffset( player_state_t *ps, player_state_t *ops, vec3_t angles )
{
	int		i;
	float	delta;

	// gun angles from bobbing
	if( cg.bobCycle & 1 ) {
		angles[ROLL] -= cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobRoll->value;
		angles[YAW] -= cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobYaw->value;
	} else {
		angles[ROLL] += cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobRoll->value;
		angles[YAW] += cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobYaw->value;
	}
	angles[PITCH] += cg.xyspeed * cg.bobFracSin * 0.002 * cg_bobPitch->value;

	// gun angles from delta movement
	for( i = 0; i < 3; i++ ) {
		delta = ( ops->viewangles[i] - ps->viewangles[i] ) * cg.lerpfrac;
		if( delta > 180 )
			delta -= 360;
		if( delta < -180 )
			delta += 360;
		clamp( delta, -45, 45 );

		if( i == YAW )
			angles[ROLL] += 0.001 * delta;
		angles[i] += 0.002 * delta;
	}
}

/*
==============
CG_vWeapStartFallKickEff
==============
*/
void CG_vWeapStartFallKickEff( int parms )
{
	int	bouncetime;
	bouncetime = ((parms + 1)*50)+150;
	vweap.fallEff_Time = cg.time + 2*bouncetime;
	vweap.fallEff_rebTime = cg.time + bouncetime;
}

/*
===============
CG_vWeapSetPosition
===============
*/
void CG_vWeapSetPosition( entity_t *gun )
{
	vec3_t			gun_angles;
	vec3_t			forward, right, up;
	float			gunx, guny, gunz;

	// set up gun position
	VectorCopy( cg.refdef.vieworg, gun->origin );
	VectorCopy( cg.refdef.viewangles, gun_angles );

	// offset by client cvars
	gunx = cg_gunx->value;
	guny = cg_guny->value;
	gunz = cg_gunz->value;

	// move hand to the left/center
	if( cgs.clientInfo[cg.chasedNum].hand == 2 )
		gunx -= 3;
	else if( cgs.clientInfo[cg.chasedNum].hand == 1 )
		gunx -= 8;

	// add fallkick effect
	if( vweap.fallEff_Time > cg.time )
		vweap.fallKick += ( vweap.fallEff_rebTime - cg.time ) * 0.001f;
	else
		vweap.fallKick = 0;

	guny -= vweap.fallKick;

	// move the gun
	AngleVectors( gun_angles, forward, right, up );
	VectorMA( gun->origin, gunx, right, gun->origin );
	VectorMA( gun->origin, gunz, forward, gun->origin );
	VectorMA( gun->origin, guny, up, gun->origin );

	// add bobbing
	CG_CalcGunOffset( &cg.frame.playerState, vweap.ops, gun_angles );

	VectorCopy( gun->origin, gun->origin2 );	// don't lerp
	VectorCopy( gun->origin, gun->lightingOrigin );
	AnglesToAxis( gun_angles, gun->axis );
}

/*
===============
CG_vWeapSetFrame
===============
*/
void CG_vWeapSetFrame( void )
{
	vweap.oldframe = vweap.frame;
	vweap.frame++;

	// looping
	if( vweap.frame > vweap.pweapon.weaponInfo->lastframe[vweap.currentAnim] ) {
		if( vweap.pweapon.weaponInfo->loopingframes[vweap.currentAnim] )
			vweap.frame = (vweap.pweapon.weaponInfo->lastframe[vweap.currentAnim] - (vweap.pweapon.weaponInfo->loopingframes[vweap.currentAnim] - 1));
		else if( !vweap.newAnim )
			vweap.newAnim = VWEAP_STANDBY;
	}

	// new animation
	if( vweap.newAnim ) {
		if( vweap.newAnim == VWEAP_WEAPONUP ) { // weapon change
			vweap.pweapon.weaponInfo = vweap.newWeaponInfo;
			vweap.oldframe = vweap.pweapon.weaponInfo->firstframe[vweap.newAnim];	// don't lerp
			vweap.pweapon.rotationSpeed = 0;
		}
		vweap.currentAnim = vweap.newAnim;
		vweap.frame = vweap.pweapon.weaponInfo->firstframe[vweap.newAnim];
		vweap.newAnim = 0;
	}
}

/*
===============
CG_vWeapUpdateAnimation
===============
*/
void CG_vWeapUpdateAnimation( void )
{
	if( cg.time > vweap.nextframetime ) {
		vweap.backlerp = 1.0f;
		CG_vWeapSetFrame();	// the model can change at this point
		vweap.prevframetime = cg.time;
		vweap.nextframetime = cg.time + vweap.pweapon.weaponInfo->frametime[vweap.currentAnim];
	} else {
		vweap.backlerp = 1.0f - ( ( cg.time - vweap.prevframetime ) / ( vweap.nextframetime - vweap.prevframetime ) );
		if( vweap.backlerp > 1 )
			vweap.backlerp = 1.0f;
		else if( vweap.backlerp < 0 )
			vweap.backlerp = 0;
	}
}

/*
==================
CG_vWeapUpdateState
==================
*/
void CG_vWeapUpdateState( void )
{
	int i;
	centity_t		*cent;
	int				torsoNewAnim;

	cent = &cg_entities[cg.chasedNum+1];
	vweap.state = &cent->current;	
	// no weapon to draw
	if( !vweap.state->weapon || vweap.state->modelindex2 != 255 ) {
		vweap.pweapon.weaponInfo = NULL;
		return;
	}

	// update newweapon info
	vweap.newWeaponInfo = CG_GetWeaponFromPModelIndex( &cg_entPModels[cg.chasedNum+1], vweap.state->weapon );

	// Update animations based on Torso

	// filter repeated animations coming from game
	torsoNewAnim = ( cent->current.frame>>6 &0x3F ) * ( ( cent->current.frame>>6 &0x3F ) != ( cent->prev.frame>>6 &0x3F ) );

	if( torsoNewAnim == TORSO_FLIPOUT && vweap.newAnim < VWEAP_WEAPDOWN )
		vweap.newAnim = VWEAP_WEAPDOWN;
	if( torsoNewAnim == TORSO_DROPHOLD && vweap.newAnim < VWEAP_ATTACK2_HOLD )
		vweap.newAnim = VWEAP_ATTACK2_HOLD;

	// Update based on Events
	for( i = 0; i < 2; i++ ) {
		switch( vweap.state->events[i] ) {
			case EV_FOOTSTEP:
				// add bobing angles?
				break;
			case EV_FALL:
				CG_vWeapStartFallKickEff( vweap.state->eventParms[i] );
				break;
			case EV_PAIN:
				// add damage kickangles?
				break;
			case EV_JUMP:
				// add some kind of jumping angles?
				break;
			case EV_JUMP_PAD:
				// add jumpad kickangles?
				break;
			case EV_MUZZLEFLASH:
				if( vweap.newAnim < VWEAP_ATTACK )
					vweap.newAnim = VWEAP_ATTACK;
				// activate flash
				if( cg_weaponFlashes->integer == 2 && vweap.pweapon.weaponInfo )
					vweap.pweapon.flashtime = cg.time + (int)( ( vweap.pweapon.weaponInfo->frametime[VWEAP_ATTACK]/4 )*3 );

				// eject brass-debris
				if( cg_ejectBrass->integer && cg_ejectBrass->integer < 3 && vweap.pweapon.weaponInfo && vweap.active ) {
					vec3_t origin;
					vec3_t	forward, right, up;

					VectorCopy( cg.refdef.vieworg, origin );
					AngleVectors( cg.refdef.viewangles, forward, right, up );
					// move it a bit fordward and up
					VectorMA( origin, 16, forward, origin );
					VectorMA( origin, 4, up, origin );
					if( cgs.clientInfo[cg.chasedNum].hand == 0 )
						VectorMA( origin, 8, right, origin );
					else if( cgs.clientInfo[cg.chasedNum].hand == 1 )
						VectorMA( origin, -4, right, origin );

					if( vweap.state->weapon == WEAP_MACHINEGUN )
						CG_EjectBrass( origin, 1, CG_MediaModel( cgs.media.modEjectBrassMachinegun ) );
					else if( vweap.state->weapon == WEAP_CHAINGUN )
						CG_EjectBrass( origin, 3, CG_MediaModel( cgs.media.modEjectBrassMachinegun ) );
					else if( vweap.state->weapon == WEAP_SHOTGUN )
						CG_EjectBrass( origin, 1, CG_MediaModel( cgs.media.modEjectBrassShotgun ) );
					else if( vweap.state->weapon == WEAP_SUPERSHOTGUN )
						CG_EjectBrass( origin, 2, CG_MediaModel( cgs.media.modEjectBrassShotgun ) );
				}
				break;
			case EV_DROP:
				break;
			case EV_WEAPONUP:
				vweap.newAnim = VWEAP_WEAPONUP;	// is top priority
				break;
		}
	}

	// remove when there is no hand model, so it tries to reboot
	if( vweap.pweapon.weaponInfo && !vweap.pweapon.weaponInfo->model[HAND] )
		vweap.pweapon.weaponInfo = NULL;

	// init
	if( !vweap.pweapon.weaponInfo && vweap.newWeaponInfo ) {
		vweap.newAnim = 0;
		if( vweap.newWeaponInfo->model[HAND] ) {
			vweap.nextframetime = cg.time;	// don't wait for next frame 
			vweap.pweapon.weaponInfo = vweap.newWeaponInfo;
			vweap.currentAnim = VWEAP_STANDBY;
			vweap.pweapon.flashtime = 0;
			vweap.pweapon.rotationSpeed = 0;
			vweap.frame = vweap.pweapon.weaponInfo->firstframe[vweap.currentAnim];
			vweap.oldframe = vweap.frame;
		}
		return;
	}

	// if showing the wrong weapon, fix. (It happens in chasecam when changing POV)
	if( ( vweap.pweapon.weaponInfo && vweap.newWeaponInfo ) 
		&& ( vweap.pweapon.weaponInfo != vweap.newWeaponInfo ) ) {
		if( ( vweap.newAnim != VWEAP_WEAPONUP ) && ( vweap.currentAnim != VWEAP_WEAPDOWN )
			&& ( vweap.newWeaponInfo->model[HAND] ) ) {
			if( cg_debugWeaponModels->integer )
				CG_Printf( "fixing wrong viewWeapon\n" );

			if( !vweap.currentAnim ) {
				if( !vweap.newAnim )
					vweap.currentAnim = VWEAP_WEAPONUP;
				else
					vweap.currentAnim = vweap.newAnim;
			}

			vweap.nextframetime = cg.time; // don't wait for next frame 
			vweap.pweapon.weaponInfo = vweap.newWeaponInfo;
			vweap.frame = vweap.pweapon.weaponInfo->firstframe[vweap.currentAnim];
			vweap.oldframe = vweap.frame;
		}
	}
}

/*
==============
CG_AddViewWeapon
==============
*/
void CG_AddViewWeapon( void )
{
	orientation_t	tag;
	entity_t		gun;

	// gun disabled
	if( !vweap.state )
		return;

	// remove if player in POV has changed
	if( vweap.state->number != cg.chasedNum+1 )
		vweap.pweapon.weaponInfo = NULL;

	if( !vweap.pweapon.weaponInfo )
		return;
	if( !vweap.pweapon.weaponInfo->model[HAND] )
		return;

	// setup

	// we need to update animation even if we don't draw
	// the weapon model this frame for some reason (we're 
	// in third person view or something)
	CG_vWeapUpdateAnimation();

	if( !vweap.active )
		return;
	
	// hand entity
	memset( &gun, 0, sizeof( gun ) );
	gun.model = vweap.pweapon.weaponInfo->model[HAND];

	CG_vWeapSetPosition( &gun );

	// setup common parameters
	gun.flags = RF_MINLIGHT | RF_WEAPONMODEL;
	gun.scale = 1.0;
	VectorCopy( gun.origin, gun.origin2 );
	VectorCopy( cg.lightingOrigin, gun.lightingOrigin );
	gun.frame = vweap.frame;
	gun.oldframe = vweap.oldframe;
	gun.backlerp = vweap.backlerp;
	CG_SetBoneposesForTemporaryEntity( &gun );
	CG_AddEntityToScene( &gun );

	CG_AddShellEffects( &gun, cg.effects );
	
	// always create a projectionSource fallback
	VectorMA( gun.origin, 12, gun.axis[2], vweap.projectionSource.origin );
	VectorMA( vweap.projectionSource.origin, 18, gun.axis[0], vweap.projectionSource.origin );
	Matrix_Copy( gun.axis, vweap.projectionSource.axis );

	// add attached weapon
	if( CG_GrabTag( &tag, &gun, "tag_weapon" ) )
		CG_AddWeaponOnTag( &gun, &tag, &vweap.pweapon, cg.effects, &vweap.projectionSource );
}
