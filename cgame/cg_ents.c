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
void CG_BeginFrameSequence ( frame_t frame ) 
{
	if ( cg.frameSequenceRunning ) {
		CG_Error ( "CG_BeginFrameSequence: already running sequence" );
	}

	cg.oldFrame = cg.frame;
	cg.frame = frame;
	cg.frameSequenceRunning = qtrue;
}

/*
==================
CG_NewPacketEntityState
==================
*/
void CG_NewPacketEntityState ( int entnum, entity_state_t state )
{
	centity_t *ent;

	if ( !cg.frameSequenceRunning ) {
		CG_Error ( "CG_NewPacketEntityState: no sequence" );
	}

	ent = &cg_entities[entnum];
	cg_parseEntities[(cg.frame.parseEntities+cg.frame.numEntities) & (MAX_PARSE_ENTITIES-1)] = state;
	cg.frame.numEntities++;

	// some data changes will force no lerping
	if ( state.modelindex != ent->current.modelindex
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

	if ( ent->serverFrame != cg.frame.serverFrame - 1 )
	{	// wasn't in last update, so initialize some things
		// duplicate the current state so lerping doesn't hurt anything
		ent->prev = state;

		if ( state.events[0] == EV_TELEPORT || state.events[1] == EV_TELEPORT ) {
			VectorCopy ( state.origin, ent->prev.origin );
			VectorCopy ( state.origin, ent->lerpOrigin );
		} else {
			VectorCopy ( state.old_origin, ent->prev.origin );
			VectorCopy ( state.old_origin, ent->lerpOrigin );
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
void CG_FireEvents (void)
{
	int					pnum;
	entity_state_t		*state;

	for ( pnum = 0; pnum < cg.frame.numEntities; pnum++ )
	{
		state = &cg_parseEntities[(cg.frame.parseEntities+pnum)&(MAX_PARSE_ENTITIES-1)];
		if ( state->events[0] ) {
			CG_EntityEvent ( state );
		}
	}
}

/*
==================
CG_EndFrameSequence
==================
*/
void CG_EndFrameSequence ( int numEntities )
{
	if ( !cg.frameSequenceRunning ) {
		CG_Error ( "CG_EndFrameSequence: no sequence" );
	}

	cg.frameSequenceRunning = qfalse;

	// clamp time
	clamp ( cg.time, cg.frame.serverTime - 100, cg.frame.serverTime );

	if ( !cg.frame.valid ) {
		return;
	}

	// verify our data is valid
	if ( cg.frame.numEntities != numEntities ) {
		CG_Error ( "CG_EndFrameSequence: bad sequence" );
	}

	CG_BuildSolidList ();
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
void CG_AddBeamEnt ( centity_t *cent )
{
	entity_t		ent;
	entity_state_t	*state;

	memset ( &ent, 0, sizeof(entity_t) );

	state = &cent->current;
	VectorCopy ( state->origin, ent.origin );
	VectorCopy ( state->origin2, ent.oldorigin );
	CG_AddLaser ( state->origin, state->origin2, state->frame * 0.5f, state->colorRGBA, CG_MediaShader (cgs.media.shaderLaser) );
}

/*
===============
CG_AddPortalSurfaceEnt

===============
*/
void CG_AddPortalSurfaceEnt ( centity_t *cent )
{
	entity_t		ent;
	entity_state_t	*state;

	memset ( &ent, 0, sizeof(entity_t) );

	state = &cent->current;
	VectorCopy ( state->origin, ent.origin );
	VectorCopy ( state->origin2, ent.oldorigin );

	ent.rtype = RT_PORTALSURFACE;
	ent.scale = state->frame / 256.0f;

	if ( state->modelindex3 ) {
		ent.frame = state->modelindex2 ? state->modelindex2 : 50;
	}

	ent.skinnum = state->skinnum;
	CG_AddEntity ( &ent );
}

/*
===============
CG_AddEntityEffects

===============
*/
void CG_AddEntityEffects ( centity_t *cent, vec3_t origin, int effects )
{
	if ( effects & EF_BLASTER ) {
		CG_BlasterTrail ( cent->lerpOrigin, origin );
		CG_AddLight ( origin, 300, 1, 1, 0 );
	} else if ( effects & EF_HYPERBLASTER ) {
		CG_AddLight ( origin, 300, 1, 1, 0 );
	} else if ( effects & EF_GIB ) {
		CG_BloodTrail ( cent->lerpOrigin, origin );
	} 
	
	if ( effects & EF_ROCKET ) {
		CG_RocketTrail ( cent->lerpOrigin, origin );
		CG_AddLight ( origin, 300, 1, 1, 0 );
	} else if ( effects & EF_GRENADE ) {
		CG_GrenadeTrail ( cent->lerpOrigin, origin );
	} else if ( effects & EF_FLIES ) {
		CG_FlyEffect ( cent, origin ); 
	} else if ( effects & EF_BFG ) {
		CG_BfgParticles ( origin );
		CG_AddLight ( origin, 300, 0, 1, 0 );
	} else if ( (effects & (EF_FLAG1|EF_FLAG2)) == EF_FLAG1 ) {
		CG_FlagTrail ( cent->lerpOrigin, origin, EF_FLAG1 );
		CG_AddLight ( origin, 225, 1, 0.1, 0.1 );
	} else if ( (effects & (EF_FLAG1|EF_FLAG2)) == EF_FLAG2 ) {
		CG_FlagTrail ( cent->lerpOrigin, origin, EF_FLAG2 );
		CG_AddLight ( origin, 225, 0.1, 0.1, 1 );
	} else if ( (effects & (EF_FLAG1|EF_FLAG2)) == (EF_FLAG1|EF_FLAG2) ) {
		CG_FlagTrail ( cent->lerpOrigin, origin, EF_FLAG1 );
		CG_FlagTrail ( cent->lerpOrigin, origin, EF_FLAG2 );
		CG_AddLight ( origin, 225, 1, 0.2, 1 );
	}
}

/*
===============
CG_AddPlayerEnt

===============
*/
void CG_AddPlayerEnt ( centity_t *cent )
{
	entity_t		ent;
	entity_state_t	*state;
	vec3_t			ent_angles;
	int				i;
	cg_clientInfo_t	*ci;
	unsigned int	effects, renderfx;

	state = &cent->current;

	memset ( &ent, 0, sizeof(entity_t) );

	effects = state->effects & ~(EF_BOB|EF_ROTATE);
	renderfx = state->renderfx & ~RF_LEFTHAND;

	ent.frame = state->frame;
	ent.oldframe = cent->prev.frame;
	ent.backlerp = 1.0 - cg.lerpfrac;
	ent.scale = 1.0f;
	ent.rtype = RT_MODEL;

	if ( renderfx & RF_FRAMELERP )
	{	// step origin discretely, because the frames
		// do the animation properly
		VectorCopy ( cent->current.origin, ent.origin );
		VectorCopy ( cent->current.old_origin, ent.oldorigin );
	}
	else
	{	// interpolate origin
		for ( i = 0; i < 3; i++ )
			ent.origin[i] = ent.oldorigin[i] = cent->prev.origin[i] + cg.lerpfrac * 
				(cent->current.origin[i] - cent->prev.origin[i]);
	}

	// create a new entity	
	if (state->modelindex == 255)
	{	// use custom player skin
		ci = &cgs.clientInfo[state->skinnum & 0xff];
		if (ci->skin)
			ent.customSkin = ci->skin;
		else
			ent.customShader = ci->shader;
		ent.model = ci->model;
		if ((!ent.customSkin && !ent.customShader) || !ent.model)
		{
			if (cgs.baseClientInfo.skin)
				ent.customSkin = cgs.baseClientInfo.skin;
			else
				ent.customShader = cgs.baseClientInfo.shader;
			ent.model = cgs.baseClientInfo.model;
		}
	}
	else
	{
		ent.skinnum = state->skinnum;
		ent.model = cgs.modelDraw[state->modelindex];
	}

	// render effects
	if ( renderfx & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE) ) {
		ent.flags = RF_MINLIGHT;	// renderfx go on color shell entity
	} else {
		ent.flags = renderfx | RF_MINLIGHT;
	}

	VectorCopy ( ent.origin, ent.lightingOrigin );

	// interpolate angles
	for ( i = 0; i < 3; i++ )
		ent_angles[i] = LerpAngle ( cent->prev.angles[i], cent->current.angles[i], cg.lerpfrac );

	if ( ent_angles[0] || ent_angles[1] || ent_angles[2] ) {
		AnglesToAxis ( ent_angles, ent.axis );
	} else {
		Matrix3_Copy ( axis_identity, ent.axis );
	}

	if ( state->number == cgs.playerNum+1 ) {
		cg.effects = effects;
		
		if ( !cg_thirdPerson->value ) {
			ent.flags |= RF_VIEWERMODEL;	// only draw from mirrors
			cg.thirdPerson = qfalse;
		} else { 
			cg.thirdPerson = ( state->modelindex != 0 );
		}
	}

	// if set to invisible, skip
	if ( !state->modelindex ) {
		goto done;
	}

	// add to refresh list
	CG_AddEntity ( &ent );

	ent.customSkin = NULL;
	ent.customShader = NULL;		// never use a custom skin on others

	// quad and pent can do different things on client
	if ( effects & EF_QUAD ) {
		ent.customShader = CG_MediaShader ( cgs.media.shaderPowerupQuad );
		CG_AddEntity (&ent);
	}
	if ( effects & EF_PENT ) {
		ent.customShader = CG_MediaShader ( cgs.media.shaderPowerupPenta );
		CG_AddEntity (&ent);
	}

	// color shells generate a separate entity for the main model
	if ( renderfx & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE) )
	{
		vec4_t shadelight = { 0, 0, 0, 0.3 };

		if ( renderfx & RF_SHELL_RED )
			shadelight[0] = 1.0;
		if ( renderfx & RF_SHELL_GREEN )
			shadelight[1] = 1.0;
		if ( renderfx & RF_SHELL_BLUE )
			shadelight[2] = 1.0;

		for (i=0 ; i<4 ; i++)
			ent.color[i] = shadelight[i] * 255;

		ent.customShader = CG_MediaShader ( cgs.media.shaderShellEffect );
		CG_AddEntity ( &ent );
	}

	ent.skinnum = 0;
	Vector4Set ( ent.color, 255, 255, 255, 255 );

	if ( ent.flags & RF_VIEWERMODEL ) {
		// only draw from mirrors
		ent.flags = RF_VIEWERMODEL;
	} else {
		ent.flags = 0;
	}

	// duplicate for linked models
	if (state->modelindex2)
	{
		if (state->modelindex2 == 255)
		{	// custom weapon
			ci = &cgs.clientInfo[state->skinnum & 0xff];
			i = state->weapon; // 0 is default weapon model
			if (!cg_vwep->value || i > WEAP_TOTAL - 1)
				i = 0;
			ent.model = ci->weaponmodel[i];
			if (!ent.model) {
				if (i != 0)
					ent.model = ci->weaponmodel[0];
				if (!ent.model)
					ent.model = cgs.baseClientInfo.weaponmodel[0];
			}

			CG_AddEntity ( &ent );
		}
		else
		{
			ent.model = cgs.modelDraw[state->modelindex2];
			CG_AddEntity ( &ent );
		}

		// quad and pent can do different things on client
		if ( effects & EF_QUAD ) {
			ent.customShader = CG_MediaShader ( cgs.media.shaderPowerupQuad );
			CG_AddEntity ( &ent );
		}

		if ( effects & EF_PENT ) {
			ent.customShader = CG_MediaShader ( cgs.media.shaderPowerupPenta );
			CG_AddEntity ( &ent );
		}
	}

	if ( state->modelindex3 ) {
		int frame, oldframe;

		frame = ent.frame;
		oldframe = ent.oldframe;

		if ( effects & (EF_FLAG1|EF_FLAG2) ) {
			ent.frame = 0;
			ent.oldframe = 0;
		}

		ent.model = cgs.modelDraw[state->modelindex3];
		CG_AddEntity (&ent);
		
		ent.frame = frame;
		ent.oldframe = oldframe;
	}

	// add automatic particle trails
	if ( effects ) {
		if ( effects & EF_POWERSCREEN ) {
			ent.model = CG_MediaModel ( cgs.media.modPowerScreen );
			ent.oldframe = 0;
			ent.frame = 0;
			CG_AddEntity (&ent);
		}

		CG_AddEntityEffects ( cent, ent.origin, effects );
	}

done:
	VectorCopy ( ent.origin, cent->lerpOrigin );
}

/*
===============
CG_AddGenericEnt

===============
*/
void CG_AddGenericEnt (centity_t *cent)
{
	entity_t			ent;
	entity_state_t		*state;
	vec3_t				ent_angles = { 0, 0, 0 };
	int					i;
	unsigned int		effects, renderfx;
	int					msec;

	state = &cent->current;

	memset ( &ent, 0, sizeof(ent) );

	effects = state->effects;
	renderfx = state->renderfx & ~RF_LEFTHAND;

	// set frame
	ent.frame = state->frame;
	ent.oldframe = cent->prev.frame;
	ent.backlerp = 1.0 - cg.lerpfrac;

	if ( renderfx & RF_FRAMELERP )
	{	// step origin discretely, because the frames
		// do the animation properly
		VectorCopy ( cent->current.origin, ent.origin );
		VectorCopy ( cent->current.old_origin, ent.oldorigin );
	}
	else
	{	// interpolate origin
		for ( i = 0; i < 3; i++ )
			ent.origin[i] = ent.oldorigin[i] = cent->prev.origin[i] + cg.lerpfrac * 
				(cent->current.origin[i] - cent->prev.origin[i]);
	}

	// create a new entity	
	ent.rtype = RT_MODEL;
	if ( state->solid == SOLID_BMODEL )	{
		ent.model = cgs.inlineModelDraw[state->modelindex];
	} else {
		ent.skinnum = state->skinnum;
		ent.model = cgs.modelDraw[state->modelindex];
	}

	// render effects
	if ( renderfx & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE) ) {
		ent.flags = renderfx & RF_MINLIGHT;	// renderfx go on color shell entity
	} else {
		ent.flags = renderfx;
	}

	// respawning items
	if ( cent->respawnTime ) {
		msec = cg.time - cent->respawnTime;
	} else {
		msec = ITEM_RESPAWN_TIME;
	}

	if ( msec >= 0 && msec < ITEM_RESPAWN_TIME  ) {
		ent.scale = (float)msec / ITEM_RESPAWN_TIME;
	} else {
		ent.scale = 1.0f;
	}

	if ( renderfx & RF_SCALEHACK ) {
		ent.scale *= 1.5;
	}

	// bobbing items
	if ( effects & EF_BOB ) {
		float scale = 0.005f + state->number * 0.00001f;
		float bob = 4 + cos( (cg.time + 1000) * scale ) * 4;
		ent.oldorigin[2] += bob;
		ent.origin[2] += bob;
	}

	VectorCopy ( ent.origin, ent.lightingOrigin );

	if ( !ent.scale ) {
		goto done;
	}

	// calculate angles
	if ( effects & EF_ROTATE ) {	// some bonus items auto-rotate
		Matrix3_Copy ( cg.autorotateAxis, ent.axis );
	} else {						// interpolate angles
		for ( i = 0; i < 3; i++ )
			ent_angles[i] = LerpAngle ( cent->prev.angles[i], cent->current.angles[i], cg.lerpfrac );
		
		if ( ent_angles[0] || ent_angles[1] || ent_angles[2] ) {
			AnglesToAxis ( ent_angles, ent.axis );
		} else {
			Matrix3_Copy ( axis_identity, ent.axis );
		}
	}

	// if set to invisible, skip
	if ( !state->modelindex ) {
		goto done;
	}

	// add to refresh list
	CG_AddEntity ( &ent );

	ent.customSkin = NULL;
	ent.customShader = NULL;		// never use a custom skin on others

	// color shells generate a separate entity for the main model
	if ( renderfx & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE) )
	{
		vec4_t shadelight = { 0, 0, 0, 0.3 };

		if ( renderfx & RF_SHELL_RED )
			shadelight[0] = 1.0;
		if ( renderfx & RF_SHELL_GREEN )
			shadelight[1] = 1.0;
		if ( renderfx & RF_SHELL_BLUE )
			shadelight[2] = 1.0;

		for (i=0 ; i<4 ; i++)
			ent.color[i] = shadelight[i] * 255;

		ent.customShader = CG_MediaShader ( cgs.media.shaderShellEffect );
		CG_AddEntity (&ent);
	}

	Vector4Set ( ent.color, 255, 255, 255, 255 );

	ent.skinnum = 0;

	// duplicate for linked models
	if ( state->modelindex2 ) {
		ent.model = cgs.modelDraw[state->modelindex2];
		CG_AddEntity ( &ent );
	}
	if ( state->modelindex3 ) {
		ent.model = cgs.modelDraw[state->modelindex3];
		CG_AddEntity ( &ent );
	}

	// add automatic particle trails
	if ( (effects & ~(EF_ROTATE|EF_BOB)) ) {
		if ( effects & EF_POWERSCREEN )
		{
			ent.model = CG_MediaModel ( cgs.media.modPowerScreen );
			ent.oldframe = 0;
			ent.frame = 0;
			CG_AddEntity (&ent);
		}

		CG_AddEntityEffects ( cent, ent.origin, effects );
	}

done:
	VectorCopy ( ent.origin, cent->lerpOrigin );
}

/*
===============
CG_AddPacketEntities

===============
*/
void CG_AddPacketEntities (void)
{
	entity_t			ent;
	entity_state_t		*state;
	vec3_t				autorotate;
	int					pnum, type;
	centity_t			*cent;

	// bonus items rotate at a fixed rate
	VectorSet ( autorotate, 0, anglemod(cg.time*0.1), 0 );
	AnglesToAxis ( autorotate, cg.autorotateAxis );

	memset (&ent, 0, sizeof(ent));

	for ( pnum = 0; pnum < cg.frame.numEntities; pnum++ )
	{
		state = &cg_parseEntities[(cg.frame.parseEntities+pnum)&(MAX_PARSE_ENTITIES-1)];
		cent = &cg_entities[state->number];
		type = state->type & ~ET_INVERSE;

		switch ( type )
		{
			case ET_GENERIC:
				CG_AddGenericEnt ( cent );
				break;

			case ET_PLAYER:
				CG_AddPlayerEnt ( cent );
				break;

			case ET_BEAM:
				CG_AddBeamEnt ( cent );
				break;

			case ET_PORTALSURFACE:
				CG_AddPortalSurfaceEnt ( cent );
				break;

			case ET_EVENT:
				break;

			case ET_PUSH_TRIGGER:
				break;

			default:
				CG_Error ( "CG_AddPacketEntities: unknown entity type" );
				break;
		}

		// add loop sound
		if ( cent->current.sound ) {
			trap_S_AddLoopSound ( cgs.soundPrecache[cent->current.sound], cent->lerpOrigin );
		}

		// glow if light is set
		if ( state->light ) {
			CG_AddLight ( cent->lerpOrigin, 
				COLOR_A ( state->light ) * 4.0, 
				COLOR_R ( state->light ) * (1.0/255.0), 
				COLOR_G ( state->light ) * (1.0/255.0),	
				COLOR_B ( state->light ) * (1.0/255.0) );
		}
	}
}

/*
==============
CG_AddViewWeapon
==============
*/
void CG_AddViewWeapon ( player_state_t *ps, player_state_t *ops )
{
	entity_t	gun;		// view model
	vec3_t		gun_angles;
	int			i;

	// allow the gun to be completely removed
	if ( !cg_gun->value || cg.thirdPerson || !ps->gunindex ) {
		return;
	}

	// don't draw gun if in wide angle view
	if ( (ps->fov > 90) && (cg_gun->value == 2) ) {
		return;
	}

	// don't draw if hand is centered
	if ( hand->value == 2 ) {
		return;
	}

	memset ( &gun, 0, sizeof(gun) );

	gun.model = cgs.modelDraw[ps->gunindex];

	if ( !gun.model ) {
		return;
	}

	// set up gun position
	for ( i = 0; i < 3; i++ ) {
		gun.origin[i] = cg.refdef.vieworg[i];
		gun_angles[i] = cg.refdef.viewangles[i] + LerpAngle ( ops->gunangles[i],
			ps->gunangles[i], cg.lerpfrac );
	}

	VectorCopy ( gun.origin, gun.lightingOrigin );
	AnglesToAxis ( gun_angles, gun.axis );

	gun.frame = ps->gunframe;
	if (gun.frame == 0)
		gun.oldframe = 0;	// just changed weapons, don't lerp from old
	else
		gun.oldframe = ops->gunframe;
	gun.scale = 1.0f;
	gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL | ((hand->value == 1.0f) ? RF_LEFTHAND : 0);
	gun.backlerp = 1.0 - cg.lerpfrac;
	VectorCopy ( gun.origin, gun.oldorigin );	// don't lerp at all
	CG_AddEntity ( &gun );

	if ( cg.effects & EF_QUAD ) {
		gun.customShader = CG_MediaShader ( cgs.media.shaderQuadWeapon );
		CG_AddEntity ( &gun );
	}
	if ( cg.effects & EF_PENT ) {
		gun.customShader = CG_MediaShader ( cgs.media.shaderPowerupPenta );
		CG_AddEntity ( &gun );
	}
}


/*
===============
CG_CalcViewValues

Sets cl.refdef view values
===============
*/
void CG_CalcViewValues (void)
{
	int			i;
	float		lerp, backlerp;
	centity_t	*ent;
	frame_t		*oldframe;
	player_state_t	*ps, *ops;

	// find the previous frame to interpolate from
	ps = &cg.frame.playerState;
	oldframe = &cg.oldFrame;
	if ( oldframe->serverFrame != cg.frame.serverFrame-1 || !oldframe->valid ) {
		oldframe = &cg.frame;		// previous frame was dropped or invalid
	}
	ops = &oldframe->playerState;

	// see if the player entity was teleported this frame
	if ( abs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 256*16
		|| abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 256*16
		|| abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 256*16 )
		ops = ps;		// don't interpolate

	ent = &cg_entities[cgs.playerNum+1];
	lerp = cg.lerpfrac;

	// calculate the origin
	if ( cg_predict->value && !(cg.frame.playerState.pmove.pm_flags & PMF_NO_PREDICTION)
		&& !cg.thirdPerson )
	{	// use predicted values
		unsigned	delta;

		backlerp = 1.0f - lerp;
		for ( i = 0; i < 3; i++ )
			cg.refdef.vieworg[i] = cg.predictedOrigin[i] + ops->viewoffset[i] 
				+ lerp * (ps->viewoffset[i] - ops->viewoffset[i])
				- backlerp * cg.predictionError[i];

		// smooth out stair climbing
		delta = cg.realTime - cg.predictedStepTime;
		if ( delta < 150 ) {
			cg.refdef.vieworg[2] -= cg.predictedStep * (150 - delta) / 150;
		}
	}
	else
	{	// just use interpolated values
		for ( i = 0; i < 3; i++ )
			cg.refdef.vieworg[i] = ops->pmove.origin[i]*(1.0/16.0) + ops->viewoffset[i] 
				+ lerp * (ps->pmove.origin[i]*(1.0/16.0) + ps->viewoffset[i] 
				- (ops->pmove.origin[i]*(1.0/16.0) + ops->viewoffset[i]) );
	}

	// if not running a demo or on a locked frame, add the local angle movement
	if ( (cg.frame.playerState.pmove.pm_type < PM_DEAD) && !cgs.attractLoop )
	{	// use predicted values
		for ( i = 0; i < 3; i++ )
			cg.refdef.viewangles[i] = cg.predictedAngles[i];
	}
	else
	{	// just use interpolated values
		for ( i = 0; i < 3; i++ )
			cg.refdef.viewangles[i] = LerpAngle ( ops->viewangles[i], ps->viewangles[i], lerp );
	}

	for ( i = 0; i < 3; i++ )
		cg.refdef.viewangles[i] += LerpAngle ( ops->kick_angles[i], ps->kick_angles[i], lerp );

	AngleVectors ( cg.refdef.viewangles, cg.v_forward, cg.v_right, cg.v_up );

	// interpolate field of view
	cg.refdef.fov_x = ops->fov + lerp * (ps->fov - ops->fov);

	// don't interpolate blend color
	for ( i = 0; i < 4; i++ )
		cg.refdef.blend[i] = ps->blend[i];

	// add the weapon
	CG_AddViewWeapon ( ps, ops );
}

/*
===============
CG_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CG_AddEntities (void)
{
	extern int cg_numBeamEnts;

	if ( cg_timeDemo->value ) {
		cg.lerpfrac = 1.0;
	} else {
		cg.lerpfrac = 1.0 - (cg.frame.serverTime - cg.time) * 0.01;
	}

	CG_CalcViewValues ();

	cg_numBeamEnts = 0;

	CG_AddPacketEntities ();
	CG_AddBeams ();
	CG_AddLocalEntities ();
	CG_AddDecals ();
	CG_AddParticles ();
	CG_AddDlights ();
}

/*
===============
CG_GetEntitySoundOrigin

Called to get the sound spatialization origin
===============
*/
void CG_GetEntitySoundOrigin ( int entnum, vec3_t org )
{
	centity_t	*ent;
	struct cmodel_s *cmodel;
	vec3_t		mins, maxs;

	if ( entnum < 0 || entnum >= MAX_EDICTS ) {
		CG_Error ( "CG_GetEntitySoundOrigin: bad entnum" );
	}

	ent = &cg_entities[entnum];
	if ( ent->current.solid != SOLID_BMODEL ) {
		VectorCopy ( ent->lerpOrigin, org );
		return;
	}

	cmodel = trap_CM_InlineModel ( ent->current.modelindex );
	trap_CM_InlineModelBounds ( cmodel, mins, maxs );
	VectorAdd ( maxs, mins, org );
	VectorMA ( ent->lerpOrigin, 0.5f, org, org );
}
