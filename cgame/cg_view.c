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

// cg_view.c -- player rendering positioning

/*
================
CG_TestEntities

If cg_testEntities is set, create 32 player models
================
*/
void CG_TestEntities( void )
{
	int			i, j;
	float		f, r;
	entity_t	ent;

	memset( &ent, 0, sizeof( ent ) );

	trap_R_ClearScene ();

	for( i = 0; i < 100; i++ ) {
		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for ( j = 0; j < 3; j++ )
			ent.origin[j] = ent.lightingOrigin[j] = cg.refdef.vieworg[j] + cg.v_forward[j]*f + cg.v_right[j]*r;

		Matrix_Copy( cg.autorotateAxis, ent.axis );

		ent.scale = 1.0f;
		ent.rtype = RT_MODEL;
#ifdef SKELMOD
		ent.model = cgs.basePModelInfo->model;
		if (cgs.basePSkin->skin)
			ent.customSkin = cgs.basePSkin->skin;
		else
			ent.customSkin = NULL;

		CG_AddEntityToScene( &ent );
#else
		// it will only show torso
		ent.model = cgs.basePModelInfo->model[UPPER];
		if( cgs.basePSkin->skin[UPPER] )
			ent.customSkin = cgs.basePSkin->skin[UPPER];
		else
			ent.customSkin = NULL;

		CG_AddEntityToScene( &ent );
#endif
	}
}

/*
================
CG_TestLights

If cg_testLights is set, create 32 lights models
================
*/
void CG_TestLights( void )
{
	int			i, j;
	float		f, r;
	vec3_t		origin;

	for( i = 0; i < min( cg_testLights->integer, 32 ); i++ ) {
		r = 64 * ((i%4) - 1.5);
		f = 64 * (i/4) + 128;

		for ( j = 0; j < 3; j++ )
			origin[j] = cg.refdef.vieworg[j]/* + cg.v_forward[j]*f + cg.v_right[j]*r*/;
		trap_R_AddLightToScene( origin, 200, ((i%6)+1) & 1, (((i%6)+1) & 2)>>1, (((i%6)+1) & 4)>>2, NULL );
	}
}

/*
================
CG_TestBlend

If cg_testBlend is set, create a debug blend
================
*/
void CG_TestBlend( void )
{
	cg.refdef.blend[0] = 1;
	cg.refdef.blend[1] = 0.5;
	cg.refdef.blend[2] = 0.25;
	cg.refdef.blend[3] = 0.5;
}

//===================================================================

/*
================
CG_ThirdPerson_CameraUpdate
================
*/
void CG_ThirdPerson_CameraUpdate (void)
{
	float	dist, f, r;
	vec3_t	dest, stop;
	vec3_t	chase_dest;
	trace_t	trace;
	vec3_t	mins = { -4, -4, -4 };
	vec3_t	maxs = { 4, 4, 4 };

	// calc exact destination
	VectorCopy( cg.refdef.vieworg, chase_dest );
	r = DEG2RAD( cg_thirdPersonAngle->value );
	f = -cos( r );
	r = -sin( r );
	VectorMA( chase_dest, cg_thirdPersonRange->value * f, cg.v_forward, chase_dest );
	VectorMA( chase_dest, cg_thirdPersonRange->value * r, cg.v_right, chase_dest );
	chase_dest[2] += 8;

	// find the spot the player is looking at
	VectorMA( cg.refdef.vieworg, 512, cg.v_forward, dest );
	CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, dest, cgs.playerNum + 1, MASK_SOLID );

	// calculate pitch to look at the same spot from camera
	VectorSubtract( trace.endpos, cg.refdef.vieworg, stop );
	dist = sqrt( stop[0] * stop[0] + stop[1] * stop[1] );
	if( dist < 1 )
		dist = 1;
	cg.refdef.viewangles[PITCH] = RAD2DEG( -atan2(stop[2], dist) );
	cg.refdef.viewangles[YAW] -= cg_thirdPersonAngle->value;
	AngleVectors( cg.refdef.viewangles, cg.v_forward, cg.v_right, cg.v_up );

	// move towards destination
	CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, chase_dest, cgs.playerNum + 1, MASK_SOLID );

	if( trace.fraction != 1.0 ) {
		VectorCopy( trace.endpos, stop );
		stop[2] += ( 1.0 - trace.fraction ) * 32;
		CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, stop, cgs.playerNum + 1, MASK_SOLID );
		VectorCopy( trace.endpos, chase_dest );
	}

	VectorCopy( chase_dest, cg.refdef.vieworg );
}

/*
================
CG_CalcViewBob

Uses the ground/swimming values set in CG_CalcViewOnGround for 
making more accurate decissions on bobbing state
================
*/
void CG_CalcViewBob( void )
{
	float bobMove, bobTime;

	if( cg.thirdPerson )
		return;
	if( cg_paused->integer )
		return;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	cg.xyspeed = sqrt( cg.predictedVelocity[0]*cg.predictedVelocity[0] + cg.predictedVelocity[1]*cg.predictedVelocity[1] );

	bobMove = 0;
	if( cg.xyspeed < 5 )
		cg.oldBobTime = 0;			// start at beginning of cycle again
	else if( vweap.isSwim )
		bobMove = cg.frameTime * cg_bobSpeed->value * 0.3;
	else if( cg.frame.playerState.pmove.pm_flags & PMF_DUCKED )
		bobMove = cg.frameTime * cg_bobSpeed->value * 0.6;
	else if( vweap.isOnGround )
		bobMove = cg.frameTime * cg_bobSpeed->value;

	bobTime = cg.oldBobTime += bobMove;

	cg.bobCycle = (int)bobTime;
	cg.bobFracSin = fabs( sin( bobTime*M_PI ) );
}

//============================================================================

/*
==================
CG_SkyPortal
==================
*/
int CG_SkyPortal( void )
{
	float fov;
	vec3_t origin;

	if( cgs.configStrings[CS_SKYBOXORG][0] == '\0' )
		return 0;

	if( sscanf( cgs.configStrings[CS_SKYBOXORG], "%f %f %f %f", &origin[0], &origin[1], &origin[2], &fov ) == 4 ) {
		cg.refdef.skyportal.fov = fov;
		VectorCopy( origin, cg.refdef.skyportal.origin );
		return RDF_SKYPORTALINVIEW;
	}

	return 0;
}

//============================================================================

/*
==================
CG_RenderFlags
==================
*/
int CG_RenderFlags( void )
{
	int rdflags, contents;

	rdflags = 0;

	contents = CG_PointContents( cg.refdef.vieworg );
	if( contents & MASK_WATER )
		rdflags |= RDF_UNDERWATER;
	else
		rdflags &= ~RDF_UNDERWATER;

	if( cg.oldAreabits )
		rdflags |= RDF_OLDAREABITS;

	if( cg.portalInView )
		rdflags |= RDF_PORTALINVIEW;

	rdflags |= RDF_BLOOM;

	rdflags |= CG_SkyPortal ();

	return rdflags;
}

//============================================================================

/*
==================
CG_RenderView
==================
*/
#define	WAVE_AMPLITUDE	0.015	// [0..1]
#define	WAVE_FREQUENCY	0.6		// [0..1]

void CG_RenderView ( float frameTime, int realTime, float stereo_separation, qboolean forceRefresh )
{
	if( !cg.frame.valid ) {
		SCR_DrawLoading ();
		return;
	}

	// do 3D refresh drawing, and then update the screen
	SCR_CalcVrect ();

	// clear any dirty part of the background
	SCR_TileClear ();

	if( !cg_paused->value ) {
		// update time
		cg.realTime = realTime;
		cg.frameTime = frameTime;
		cg.frameCount++;
		cg.time += frameTime * 1000;

		// clamp time
		clamp( cg.time, cg.frame.serverTime - cgs.serverFrameTime, cg.frame.serverTime );
	}

	if( cg.frameTime > (1.0 / 5.0) )
		cg.frameTime = (1.0 / 5.0);

	CG_ChaseHack ();

	// predict all unacknowledged movements
	CG_PredictMovement ();

	// run lightstyles
	CG_RunLightStyles ();

	CG_FixUpGender ();

	trap_R_ClearScene ();

	// build a refresh entity list
	// this also calls CG_CalcViewValues which loads v_forward, etc.
	CG_AddEntities ();

	// update bob for next frame
	CG_CalcViewBob ();

	CG_AddLightStyles ();

	if( cg_testEntities->integer )
		CG_TestEntities ();
	if( cg_testLights->integer )
		CG_TestLights ();
	if( cg_testBlend->integer )
		CG_TestBlend ();

	// offset vieworg appropriately if we're doing stereo separation
	if( stereo_separation != 0 )
		VectorMA( cg.refdef.vieworg, stereo_separation, cg.v_right, cg.refdef.vieworg );

	if( cg.thirdPerson )
		CG_ThirdPerson_CameraUpdate ();

	// never let it sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
	cg.refdef.vieworg[0] += 1.0/16;
	cg.refdef.vieworg[1] += 1.0/16;
	cg.refdef.vieworg[2] += 1.0/16;

	cg.refdef.x = scr_vrect.x;
	cg.refdef.y = scr_vrect.y;
	cg.refdef.width = scr_vrect.width;
	cg.refdef.height = scr_vrect.height;
	cg.refdef.fov_y = CalcFov( cg.refdef.fov_x, cg.refdef.width, cg.refdef.height );

	cg.refdef.time = cg.time * 0.001;
	cg.refdef.areabits = cg.frame.areabits;

	cg.refdef.rdflags = CG_RenderFlags ();

	// warp if underwater
	if( cg.refdef.rdflags & RDF_UNDERWATER ) {
		float phase = cg.refdef.time * WAVE_FREQUENCY * M_TWOPI;
		float v = WAVE_AMPLITUDE * (sin( phase ) - 1.0) + 1;
		cg.refdef.fov_x *= v;
		cg.refdef.fov_y *= v;
	}

	trap_R_RenderScene( &cg.refdef );

	cg.oldAreabits = qtrue;

	// update audio
	trap_S_Update( cg.refdef.vieworg, cg.predictedVelocity, cg.v_forward, cg.v_right, cg.v_up );

	SCR_Draw2D ();

	CG_ResetTemporaryBoneposesCache();
}
