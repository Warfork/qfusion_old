/*
Copyright (C) 2002-2003 Victor Luchits

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
==================
CG_BeginFrameSequence
==================
*/
void CG_BeginFrameSequence( frame_t frame ) 
{
	if( cg.frameSequenceRunning )
		CG_Error( "CG_BeginFrameSequence: already running sequence" );

	cg.oldFrame = cg.frame;
	cg.frame = frame;
	cg.frameSequenceRunning = qtrue;
}

/*
==================
CG_NewPacketEntityState
==================
*/
void CG_NewPacketEntityState( int entNum, entity_state_t state )
{
	centity_t *ent;

	if( !cg.frameSequenceRunning )
		CG_Error( "CG_NewPacketEntityState: no sequence" );

	ent = &cg_entities[entNum];
	cg_parseEntities[(cg.frame.parseEntities+cg.frame.numEntities) & (MAX_PARSE_ENTITIES-1)] = state;
	cg.frame.numEntities++;

	// some data changes will force no lerping
	if( state.modelindex != ent->current.modelindex
		|| state.modelindex2 != ent->current.modelindex2
		|| state.modelindex3 != ent->current.modelindex3
		|| abs(state.origin[0] - ent->current.origin[0]) > 512
		|| abs(state.origin[1] - ent->current.origin[1]) > 512
		|| abs(state.origin[2] - ent->current.origin[2]) > 512
		|| state.events[0] == EV_TELEPORT 
		|| state.events[1] == EV_TELEPORT
		)
	{
		ent->serverFrame = -99;
	}

	if( ent->serverFrame != cg.frame.serverFrame - 1 )
	{	// wasn't in last update, so initialize some things
		// duplicate the current state so lerping doesn't hurt anything
		ent->prev = state;

		if ( state.events[0] == EV_TELEPORT || state.events[1] == EV_TELEPORT ) {
			VectorCopy( state.origin, ent->prev.origin );
			VectorCopy( state.origin, ent->lerpOrigin );
		} else {
			VectorCopy( state.old_origin, ent->prev.origin );
			VectorCopy( state.old_origin, ent->lerpOrigin );
		}

		// initialize the animation when new into PVS
		if ( cg.frame.valid && (state.type & ~ET_INVERSE) == ET_PLAYER ) {
			CG_ClearEventAnimations( &state );
			CG_AddAnimationFromState ( &state, (state.frame)&0x3F, (state.frame>>6)&0x3F, (state.frame>>12)&0xF, BASIC_CHANNEL );
		}
	}
	else
	{	// shuffle the last state to previous
		ent->prev = ent->current;
	}

	ent->serverFrame = cg.frame.serverFrame;
	ent->current = state;
}

/*
==================
CG_FireEvents
==================
*/
void CG_FireEvents( void )
{
	int					pnum;
	entity_state_t		*state;

	for( pnum = 0; pnum < cg.frame.numEntities; pnum++ ) {
		state = &cg_parseEntities[(cg.frame.parseEntities+pnum)&(MAX_PARSE_ENTITIES-1)];
		if( state->events[0] )
			CG_EntityEvent( state );
	}
}

/*
==================
CG_EndFrameSequence
==================
*/
void CG_EndFrameSequence( int numEntities )
{
	if( !cg.frameSequenceRunning )
		CG_Error( "CG_EndFrameSequence: no sequence" );

	cg.frameSequenceRunning = qfalse;

	// clamp time
	if( !cg_paused->integer )
		clamp( cg.time, cg.frame.serverTime - cgs.serverFrameTime, cg.frame.serverTime );

	if( !cg.frame.valid )
		return;

	if( memcmp( cg.oldFrame.areabits, cg.frame.areabits, sizeof( cg.frame.areabits ) ) == 0 )
		cg.oldAreabits = qtrue;
	else
		cg.oldAreabits = qfalse;

	// verify our data is valid
	if( cg.frame.numEntities != numEntities )
		CG_Error( "CG_EndFrameSequence: bad sequence" );

	CG_BuildSolidList ();
	CG_PModelsUpdateStates ();
	CG_vWeapUpdateState ();
	CG_FireEvents ();
	CG_CheckPredictionError ();
}

/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/

/*
===============
CG_AddBeamEnt
===============
*/
void CG_AddBeamEnt( centity_t *cent )
{
	entity_t		ent;
	entity_state_t	*state;

	memset( &ent, 0, sizeof(ent) );

	state = &cent->current;
	VectorCopy( state->origin, ent.origin );
	VectorCopy( state->origin2, ent.origin2 );

	CG_AddLaser( state->origin, state->origin2, state->frame * 0.5f, state->colorRGBA, CG_MediaShader( cgs.media.shaderLaser ) );
}

/*
===============
CG_AddPortalSurfaceEnt
===============
*/
void CG_AddPortalSurfaceEnt( centity_t *cent )
{
	entity_t		ent;
	entity_state_t	*state;

	memset( &ent, 0, sizeof(ent) );

	state = &cent->current;
	VectorCopy( state->origin, ent.origin );
	VectorCopy( state->origin2, ent.origin2 );

	ent.rtype = RT_PORTALSURFACE;
	Matrix_Identity( ent.axis );

	// when origin and origin2 are the same, it's drawn as a mirror, otherwise as portal
	if( !VectorCompare( ent.origin, ent.origin2 ) )
	{
		cg.portalInView = qtrue;

		ent.frame = state->skinnum;
		if( state->modelindex3 )
		{
			float phase = cent->current.frame / 256.0f;
			float speed = cent->current.modelindex2 ? cent->current.modelindex2 : 50;
			Matrix_Rotate( ent.axis, 5 * sin( ( phase + cg.time * 0.001 * speed * 0.01 ) * M_TWOPI ), 1, 0, 0 );

		}
	}

	CG_AddEntityToScene( &ent );
}

/*
===============
CG_AddEntityEffects
===============
*/
void CG_AddEntityEffects( centity_t *cent, vec3_t origin, int effects )
{
	if( effects & EF_CORPSE ) {
		CG_FlyEffect( cent, origin ); 
	} else if( (effects & (EF_FLAG1|EF_FLAG2)) == EF_FLAG1 ) {
		CG_FlagTrail( cent->lerpOrigin, origin, EF_FLAG1 );
		trap_R_AddLightToScene( origin, 225, 1, 0.1, 0.1, NULL );
	} else if( (effects & (EF_FLAG1|EF_FLAG2)) == EF_FLAG2 ) {
		CG_FlagTrail( cent->lerpOrigin, origin, EF_FLAG2 );
		trap_R_AddLightToScene( origin, 225, 0.1, 0.1, 1, NULL );
	} else if( (effects & (EF_FLAG1|EF_FLAG2)) == (EF_FLAG1|EF_FLAG2) ) {
		CG_FlagTrail( cent->lerpOrigin, origin, EF_FLAG1 );
		CG_FlagTrail( cent->lerpOrigin, origin, EF_FLAG2 );
		trap_R_AddLightToScene( origin, 225, 1, 0.2, 1, NULL );
	}
}

/*
===============
CG_AddPlayerEnt
===============
*/
void CG_AddPlayerEnt( centity_t *cent )
{
	int				i;
	entity_t		ent;
	entity_state_t	*state;
	vec3_t			ent_angles;
	unsigned int	effects, renderfx;

	state = &cent->current;

	memset( &ent, 0, sizeof( ent ) );

	effects = state->effects & ~EF_ROTATE_AND_BOB;
	renderfx = state->renderfx & ~RF_CULLHACK;

	ent.frame = state->frame;
	ent.oldframe = cent->prev.frame;
	ent.backlerp = 1.0 - cg.lerpfrac;
	ent.scale = 1.0f;
	ent.rtype = RT_MODEL;

	// render effects
	if( renderfx & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE) )
		ent.flags = RF_MINLIGHT;	// renderfx go on color shell entity
	else
		ent.flags = renderfx | RF_MINLIGHT;
	ent.flags |= RF_OCCLUSIONTEST;

	// interpolate angles
	for( i = 0; i < 3; i++ )
		ent_angles[i] = LerpAngle( cent->prev.angles[i], cent->current.angles[i], cg.lerpfrac );

	if( ent_angles[0] || ent_angles[1] || ent_angles[2] )
		AnglesToAxis( ent_angles, ent.axis );
	else
		Matrix_Copy( axis_identity, ent.axis );

	if( renderfx & RF_FRAMELERP ) {
		// step origin discretely, because the frames
		// do the animation properly
		vec3_t delta, move;

		VectorSubtract( cent->current.old_origin, cent->current.origin, move );
		Matrix_TransformVector( ent.axis, move, delta );
		VectorMA( cent->current.origin, ent.backlerp, delta, ent.origin );
	} else {
		// interpolate origin
		for( i = 0; i < 3; i++ )
			ent.origin[i] = cent->prev.origin[i] + cg.lerpfrac * 
				(cent->current.origin[i] - cent->prev.origin[i]);
	}

	VectorCopy( ent.origin, ent.lightingOrigin );

	if( state->number == cg.chasedNum + 1 ) { 
		cg.effects = effects;
		
		if( chaseCam.mode == CAM_THIRDPERSON )
			cg.thirdPerson = qtrue;
		else if ( !cg_thirdPerson->integer ) {
			ent.flags |= RF_VIEWERMODEL;	// only draw from mirrors
			cg.thirdPerson = qfalse;
		} else 
			cg.thirdPerson = ( state->modelindex != 0 );

		VectorCopy( ent.lightingOrigin, cg.lightingOrigin );
	}

	// if set to invisible, skip
	if( !state->modelindex )
		goto done;

	CG_AddPModel( &ent, state );	// add the player model

	ent.customSkin = NULL;
	ent.customShader = NULL;		// never use a custom skin on others

	ent.skinNum = 0;
	ent.flags = ent.flags & RF_VIEWERMODEL;		// only draw from mirrors
	Vector4Set( ent.color, 255, 255, 255, 255 );

	// duplicate for linked models
	
	if( state->modelindex2 && state->modelindex2 != 255 ) {
		ent.model = cgs.modelDraw[state->modelindex2];
		CG_SetBoneposesForTemporaryEntity( &ent );
		CG_AddEntityToScene( &ent );

		CG_AddShellEffects( &ent, effects );
		CG_AddColorShell( &ent, renderfx );
	}

	if( state->modelindex3 ) {
		int frame, oldframe;

		frame = ent.frame;
		oldframe = ent.oldframe;

		if( effects & (EF_FLAG1|EF_FLAG2) ) {
			ent.frame = 0;
			ent.oldframe = 0;
		}

		ent.model = cgs.modelDraw[state->modelindex3];
		CG_SetBoneposesForTemporaryEntity( &ent );
		CG_AddEntityToScene( &ent );

		ent.frame = frame;
		ent.oldframe = oldframe;
	}

	// add automatic particle trails
	if( effects ) {
		if( effects & EF_POWERSCREEN ) {
			ent.model = CG_MediaModel( cgs.media.modPowerScreen );
			ent.oldframe = 0;
			ent.frame = 0;
			CG_SetBoneposesForTemporaryEntity( &ent );
			CG_AddEntityToScene( &ent );
		}

		CG_AddEntityEffects( cent, ent.origin, effects );
	}

done:
	VectorCopy ( ent.origin, cent->lerpOrigin );
}

/*
===============
CG_AddGenericEnt
===============
*/
void CG_AddGenericEnt( centity_t *cent )
{
	int				i;
	entity_t		ent;
	entity_state_t	*state;
	vec3_t			ent_angles = { 0, 0, 0 };
	unsigned int	effects, renderfx;
	int				msec;

	state = &cent->current;

	memset( &ent, 0, sizeof( ent ) );

	effects = state->effects;
	renderfx = state->renderfx & ~RF_CULLHACK;

	// set frame
	ent.frame = state->frame;
	ent.oldframe = cent->prev.frame;
	ent.backlerp = 1.0 - cg.lerpfrac;

	// create a new entity	
	if( state->solid == SOLID_BMODEL ) {
		ent.rtype = RT_MODEL;
		ent.model = cgs.inlineModelDraw[state->modelindex];
	} else {
		switch( state->type )
		{
			case ET_BLASTER:
			case ET_BLASTER2:
			case ET_HYPERBLASTER:
				ent.rtype = RT_SPRITE;
				ent.radius = 16;
				ent.customShader = CG_MediaShader( cgs.media.shaderPlasmaBall );
				break;
			default:
				ent.rtype = RT_MODEL;
				ent.skinNum = state->skinnum;
				ent.model = cgs.modelDraw[state->modelindex];
				break;
		}
	}

	CG_SetBoneposesForCGEntity( &ent, cent );

	// render effects
	if( renderfx & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE) )
		ent.flags = renderfx & RF_MINLIGHT;	// renderfx go on color shell entity
	else
		ent.flags = renderfx;
	ent.flags |= RF_PLANARSHADOW;

	// respawning items
	if( cent->respawnTime )
		msec = cg.time - cent->respawnTime;
	else
		msec = ITEM_RESPAWN_TIME;

	if( msec >= 0 && msec < ITEM_RESPAWN_TIME )
		ent.scale = (float)msec / ITEM_RESPAWN_TIME;
	else
		ent.scale = 1.0f;

	if( renderfx & RF_SCALEHACK )
		ent.scale *= 1.5;

	// calculate angles
	if( effects & EF_ROTATE_AND_BOB ) {	// some bonus items auto-rotate
		Matrix_Copy ( cg.autorotateAxis, ent.axis );
	} else {						// interpolate angles
		for( i = 0; i < 3; i++ )
			ent_angles[i] = LerpAngle( cent->prev.angles[i], cent->current.angles[i], cg.lerpfrac );
		
		if( ent_angles[0] || ent_angles[1] || ent_angles[2] )
			AnglesToAxis( ent_angles, ent.axis );
		else
			Matrix_Copy( axis_identity, ent.axis );
	}

	if( renderfx & RF_FRAMELERP ) {
		// step origin discretely, because the frames
		// do the animation properly
		vec3_t delta, move;

		VectorSubtract( cent->current.old_origin, cent->current.origin, move );
		Matrix_TransformVector( ent.axis, move, delta );
		VectorMA( cent->current.origin, ent.backlerp, delta, ent.origin );
	} else {
		// interpolate origin
		for( i = 0; i < 3; i++ )
			ent.origin[i] = ent.origin2[i] = cent->prev.origin[i] + cg.lerpfrac * 
				(cent->current.origin[i] - cent->prev.origin[i]);
	}

	// bobbing items
	if( effects & EF_ROTATE_AND_BOB ) {
		float scale = 0.005f + state->number * 0.00001f;
		float bob = 4 + cos( (cg.time + 1000) * scale ) * 4;
		ent.origin[2] += bob;
		ent.origin2[2] += bob;
	}

	VectorCopy( ent.origin, ent.lightingOrigin );

	// if set to invisible, skip
	if( !ent.scale || !state->modelindex )
		goto done;

	// add to refresh list
	CG_AddEntityToScene( &ent );

	ent.customSkin = NULL;
	ent.customShader = NULL;		// never use a custom skin on others

	// color shells generate a separate entity for the main model
	if( renderfx & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE) ) {
		vec4_t shadelight = { 0, 0, 0, 0.3 };

		if( renderfx & RF_SHELL_RED )
			shadelight[0] = 1.0;
		if( renderfx & RF_SHELL_GREEN )
			shadelight[1] = 1.0;
		if( renderfx & RF_SHELL_BLUE )
			shadelight[2] = 1.0;

		for( i = 0; i < 4; i++ )
			ent.color[i] = shadelight[i] * 255;

		ent.customShader = CG_MediaShader( cgs.media.shaderShellEffect );
		CG_AddEntityToScene( &ent );
		ent.customShader = NULL;
	}

	Vector4Set( ent.color, 255, 255, 255, 255 );

	ent.skinNum = 0;

	// duplicate for linked models
	if( state->modelindex2 ) {
		orientation_t	tag;

		// tag to modelindex1, if it had a tag barrel
		if( CG_GrabTag( &tag, &ent, "tag_barrel" ) )
			CG_PlaceModelOnTag( &ent, &ent, &tag );
		
		ent.model = cgs.modelDraw[state->modelindex2];
		CG_SetBoneposesForTemporaryEntity( &ent );
		CG_AddEntityToScene( &ent );
		ent.customShader = NULL;
	}
	if( state->modelindex3 ) {
		ent.model = cgs.modelDraw[state->modelindex3];
		CG_SetBoneposesForTemporaryEntity( &ent );
		CG_AddEntityToScene( &ent );
		ent.customShader = NULL;
	}

	// add automatic particle trails
	if( cent->current.type != ET_GENERIC ) {
		int type;
		
		type = cent->current.type;
		switch( type ) {
			case ET_GIB:
				CG_BloodTrail( cent->lerpOrigin, ent.origin );
				break;
			case ET_BLASTER:
				CG_BlasterTrail( cent->lerpOrigin, ent.origin );
				trap_R_AddLightToScene( ent.origin, 300, 1, 1, 0, NULL );
				break;
			case ET_HYPERBLASTER:
				trap_R_AddLightToScene( ent.origin, 300, 1, 1, 0, NULL );
				break;
			case ET_ROCKET:
				CG_RocketTrail( cent->lerpOrigin, ent.origin );
				trap_R_AddLightToScene( ent.origin, 300, 1, 1, 0, NULL );
				break;
			case ET_GRENADE:
				CG_GrenadeTrail( cent->lerpOrigin, ent.origin );
				break;
			case ET_BFG:
				CG_BfgParticles( ent.origin );
				trap_R_AddLightToScene( ent.origin, 300, 0, 1, 0, NULL );
				break;
			default:
				break;
		}
	}

	if( (effects & ~EF_ROTATE_AND_BOB) ) {
		if ( effects & EF_POWERSCREEN ) {
			ent.model = CG_MediaModel( cgs.media.modPowerScreen );
			ent.oldframe = 0;
			ent.frame = 0;
			CG_SetBoneposesForTemporaryEntity( &ent );
			CG_AddEntityToScene( &ent );
		}

		CG_AddEntityEffects( cent, ent.origin, effects );
	}

done:
	VectorCopy( ent.origin, cent->lerpOrigin );
}

/*
===============
CG_AddPacketEntities
===============
*/
void CG_AddPacketEntities( void )
{
	entity_state_t	*state;
	vec3_t			autorotate;
	int				pnum, type;
	centity_t		*cent;

	// bonus items rotate at a fixed rate
	VectorSet( autorotate, 0, anglemod( cg.time * 0.1 ), 0 );
	AnglesToAxis( autorotate, cg.autorotateAxis );

	for( pnum = 0; pnum < cg.frame.numEntities; pnum++ )
	{
		state = &cg_parseEntities[(cg.frame.parseEntities + pnum) & (MAX_PARSE_ENTITIES - 1)];
		cent = &cg_entities[state->number];
		type = state->type & ~ET_INVERSE;

		switch( type ) {
			case ET_GENERIC:
			case ET_GIB:
			case ET_BLASTER:
			case ET_BLASTER2:
			case ET_HYPERBLASTER:
			case ET_ROCKET:
			case ET_GRENADE:
			case ET_BFG:
				CG_AddGenericEnt( cent );
				break;

			case ET_PLAYER:
				CG_AddPlayerEnt( cent );
				break;

			case ET_BEAM:
				CG_AddBeamEnt( cent );
				break;

			case ET_PORTALSURFACE:
				CG_AddPortalSurfaceEnt( cent );
				break;

			case ET_EVENT:
				break;

			case ET_PUSH_TRIGGER:
				break;

			default:
				CG_Error( "CG_AddPacketEntities: unknown entity type" );
				break;
		}

		// add loop sound
		if( cent->current.sound ) {
			vec3_t org;

			CG_GetEntitySoundOrigin( state->number, org );
			trap_S_AddLoopSound( cgs.soundPrecache[cent->current.sound], state->number, org, vec3_origin );
      }

		// glow if light is set
		if( state->light )
			trap_R_AddLightToScene( cent->lerpOrigin, 
				COLOR_A( state->light ) * 4.0, 
				COLOR_R( state->light ) * (1.0/255.0), 
				COLOR_G( state->light ) * (1.0/255.0),	
				COLOR_B( state->light ) * (1.0/255.0), NULL );
	}
}

/*
===============
CG_CalcViewValues

Sets cl.refdef view values
===============
*/
void CG_CalcViewValues( void )
{
	int			i;
	float		lerp, backlerp;
	centity_t	*ent;
	frame_t		*oldframe;
	player_state_t	*ps, *ops;

	// find the previous frame to interpolate from
	ps = &cg.frame.playerState;
	oldframe = &cg.oldFrame;
	if( oldframe->serverFrame != cg.frame.serverFrame-1 || !oldframe->valid )
		oldframe = &cg.frame;		// previous frame was dropped or invalid
	ops = &oldframe->playerState;

	// see if the player entity was teleported this frame
	if( abs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 256*16
		|| abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 256*16
		|| abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 256*16 )
		ops = ps;		// don't interpolate

	ent = &cg_entities[cgs.playerNum+1];
	lerp = cg.lerpfrac;

	// calculate the origin
	if( cg_predict->integer && !(cg.frame.playerState.pmove.pm_flags & PMF_NO_PREDICTION)	&& !cg.thirdPerson ) {
		// use predicted values
		unsigned	delta;

		backlerp = 1.0f - lerp;
		for( i = 0; i < 3; i++ )
			cg.refdef.vieworg[i] = cg.predictedOrigin[i] + ops->viewoffset[i] 
				+ lerp * (ps->viewoffset[i] - ops->viewoffset[i])
				- backlerp * cg.predictionError[i];

		// smooth out stair climbing
		delta = cg.realTime - cg.predictedStepTime;
		if( delta < 150 )
			cg.refdef.vieworg[2] -= cg.predictedStep * (150 - delta) / 150;
	} else {
		// just use interpolated values
		for( i = 0; i < 3; i++ )
			cg.refdef.vieworg[i] = ops->pmove.origin[i]*(1.0/16.0) + ops->viewoffset[i] 
				+ lerp * (ps->pmove.origin[i]*(1.0/16.0) + ps->viewoffset[i] 
				- (ops->pmove.origin[i]*(1.0/16.0) + ops->viewoffset[i]) );
	}

	// if not running a demo or on a locked frame, add the local angle movement
	if( (cg.frame.playerState.pmove.pm_type < PM_DEAD) && !cgs.attractLoop ) {
		// use predicted values
		for( i = 0; i < 3; i++ )
			cg.refdef.viewangles[i] = cg.predictedAngles[i];
	} else {
		// just use interpolated values
		for( i = 0; i < 3; i++ )
			cg.refdef.viewangles[i] = LerpAngle( ops->viewangles[i], ps->viewangles[i], lerp );
	}

	for( i = 0; i < 3; i++ )
		cg.refdef.viewangles[i] += LerpAngle( ops->kick_angles[i], ps->kick_angles[i], lerp );

	AngleVectors( cg.refdef.viewangles, cg.v_forward, cg.v_right, cg.v_up );

	// interpolate field of view
	cg.refdef.fov_x = ops->fov + lerp * (ps->fov - ops->fov);

	// don't interpolate blend color
	for( i = 0; i < 4; i++ )
		cg.refdef.blend[i] = ps->blend[i];

	vweap.ops = ops; //just for not having to find it out again later
}

/*
===============
CG_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CG_AddEntities( void )
{
	extern int cg_numBeamEnts;

	if( cg_timeDemo->integer || cg_paused->integer )
		cg.lerpfrac = 1.0;
	else
		cg.lerpfrac = 1.0 - (cg.frame.serverTime - cg.time) / (float)cgs.serverFrameTime;

	cg.portalInView = qfalse;

	CG_CalcViewValues ();

	cg_numBeamEnts = 0;

	CG_AddPacketEntities ();
	CG_CalcViewOnGround ();
	CG_AddViewWeapon ();
	CG_AddBeams ();
	CG_AddLocalEntities ();
	CG_AddDecals ();
	CG_AddParticles ();
	CG_AddDlights ();
}

/*
===============
CG_GlobalSound
===============
*/
void CG_GlobalSound( vec3_t origin, int entNum, int entChannel, int soundNum, float fvol, float attenuation )
{
	if( entNum < 0 || entNum >= MAX_EDICTS )
		CG_Error( "CG_GlobalSound: bad entnum" );

	if( cgs.soundPrecache[soundNum] )
		trap_S_StartSound( origin, entNum, entChannel, cgs.soundPrecache[soundNum], fvol, attenuation, 0.0 ); 
	else if( cgs.configStrings[CS_SOUNDS + soundNum][0] == '*' )
		CG_SexedSound( entNum, entChannel, cgs.configStrings[CS_SOUNDS + soundNum], fvol );
}

/*
===============
CG_GetEntitySoundOrigin

Called to get the sound spatialization origin
===============
*/
void CG_GetEntitySoundOrigin( int entNum, vec3_t org )
{
	centity_t	*ent;
	struct cmodel_s *cmodel;
	vec3_t		mins, maxs;

	if( entNum < 0 || entNum >= MAX_EDICTS )
		CG_Error( "CG_GetEntitySoundOrigin: bad entnum" );

	ent = &cg_entities[entNum];
	if( ent->current.solid != SOLID_BMODEL ) {
		VectorCopy( ent->lerpOrigin, org );
		return;
	}

	cmodel = trap_CM_InlineModel( ent->current.modelindex );
	trap_CM_InlineModelBounds( cmodel, mins, maxs );
	VectorAdd( maxs, mins, org );
	VectorMA( ent->lerpOrigin, 0.5f, org, org );
}
