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
// 1st person view chasecam
// by - Jalisko

#include "g_local.h"

/*
====================
UpdateChaseCam
====================
*/
void UpdateChaseCam( edict_t *ent )
{
	edict_t *targ;
	int i;
	gclient_t *client;

	client = ent->r.client;
	targ = ent->r.client->chase_target;

	if( !targ->r.inuse )	// lost target
		return;

	// is chasecam
	client->ps.stats[STAT_CHASE] = targ->s.number;
	client->ps.pmove.pm_type = PM_FREEZE;
	client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

	VectorCopy( targ->s.origin, ent->s.origin );
	VectorCopy( targ->r.client->v_angle, client->ps.viewangles );
	VectorCopy( targ->r.client->v_angle, client->v_angle );

	for( i = 0; i < 3 ; i++ )
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT( targ->r.client->v_angle[i] - client->resp.cmd_angles[i] );
	
	trap_LinkEntity(ent);

	if( ( !client->showscores && !client->menu &&
		!client->showinventory && !client->showhelp &&
		!( level.framenum & 31 ) ) || client->update_chase ) {
		char s[MAX_STRING_CHARS];

		client->update_chase = qfalse;
		Q_snprintfz( s, sizeof(s), "xv 0 yb -68 string \"%sChasing %s\"",
			S_COLOR_YELLOW, targ->r.client->pers.netname );
		trap_Layout( ent, s );
	}
}

/*
=================
G_EndFrame_UpdateChaseCam

  update chasecam follower stats & 1st person effects
=================
*/
void G_EndFrame_UpdateChaseCam( edict_t *ent )
{
	edict_t *targ;
	int		j;

	// not in chasecam
	if( !ent->r.client->chase_target )
		return;

	// is our chase target gone?
	if( !ent->r.client->chase_target->r.inuse ) {
		if( level.time < ent->r.client->chase.timeout )
			return;
		
		ent->r.client->chase_target = NULL;
		Cmd_ChaseCam_f( ent );	// chase someone, or go to spectator
		return;
	}
	ent->r.client->chase.timeout = level.time + 15*FRAMETIME; // update timeout

	// cam controls
	if( ent->r.client->chase.keyNext == qtrue ) {	// change player
		ent->r.client->chase.keyNext = qfalse;
		ChaseNext( ent );
	}

	// copy target playerState to me
	targ = ent->r.client->chase_target;
	ent->r.client->ps.stats[STAT_CHASE] = targ->s.number;

	// stats
	memcpy( ent->r.client->ps.stats, targ->r.client->ps.stats, sizeof( targ->r.client->ps.stats ) );
	ent->r.client->ps.stats[STAT_LAYOUTS] = 1;
	
	// view angles
	VectorCopy( targ->r.client->ps.viewoffset, ent->r.client->ps.viewoffset );
	VectorCopy( targ->r.client->ps.viewangles, ent->r.client->ps.viewangles );
	ent->viewheight = targ->viewheight;
	VectorCopy( targ->r.client->ps.kick_angles, ent->r.client->ps.kick_angles );

	// velocity for client side gun bobbing
	VectorCopy( targ->r.client->ps.pmove.velocity, ent->r.client->ps.pmove.velocity );

	// blends
	for( j = 0; j<4 ; j++ )
		ent->r.client->ps.blend[j] = targ->r.client->ps.blend[j];
}

/*
====================
G_EndServerFrames_UpdateChaseCam
====================
*/
void G_EndServerFrames_UpdateChaseCam( void )
{
	int i;
	edict_t	*ent;
	for( i = 0 ; i < game.maxclients ; i++ ) {
		ent = game.edicts + 1 + i;
		if( !ent->r.inuse || !ent->r.client )
			continue;

		G_EndFrame_UpdateChaseCam( ent );
	}
}

/*
====================
ChaseNext
====================
*/
void ChaseNext( edict_t *ent )
{
	int i;
	edict_t *e;

	if( !ent->r.client->chase_target )
		return;

	i = ent->r.client->chase_target - game.edicts;
	do {
		i++;
		if( i > game.maxclients )
			i = 1;
		e = game.edicts + i;
		if( !e->r.inuse )
			continue;
		if( e->r.solid != SOLID_NOT )
			break;
	} while( e != ent->r.client->chase_target );

	ent->r.client->chase_target = e;
	ent->r.client->update_chase = qtrue;
}

/*
====================
ChasePrev
====================
*/
void ChasePrev( edict_t *ent )
{
	int i;
	edict_t *e;

	if( !ent->r.client->chase_target )
		return;

	i = ent->r.client->chase_target - game.edicts;
	do {
		i--;
		if( i < 1 )
			i = game.maxclients;
		e = game.edicts + i;
		if( !e->r.inuse )
			continue;
		if( e->r.solid != SOLID_NOT )
			break;
	} while( e != ent->r.client->chase_target );

	ent->r.client->chase_target = e;
	ent->r.client->update_chase = qtrue;
}


/*
====================
Cmd_ChaseCam_f
====================
*/
void Cmd_ChaseCam_f( edict_t *ent )
{
	int i;
	edict_t *e;
	gclient_t	*client;
	char		userinfo[MAX_INFO_STRING];

	client = ent->r.client;

	// is already in chasecam
	if( client->chase_target )
		return;

	if( client->menu )
		PMenu_Close( ent );

	// go observer
	CTFPlayerResetGrapple( ent );
	CTFDeadDropFlag( ent );
	CTFDeadDropTech( ent );

	ent->deadflag = DEAD_NO;
	ent->movetype = MOVETYPE_NOCLIP;
	ent->r.solid = SOLID_NOT;
	ent->r.svflags |= SVF_NOCLIENT;
	ent->r.client->resp.ctf_team = CTF_NOTEAM;
	ent->r.client->ps.stats[STAT_CHASE] = 0;
	ent->r.client->resp.score = 0;
	memcpy( userinfo, ent->r.client->pers.userinfo, sizeof( userinfo ) );
	InitClientPersistant( ent->r.client );
	ClientUserinfoChanged( ent, userinfo );
	trap_LinkEntity( ent );

	// locate a chase target
	for( i = 1; i <= game.maxclients; i++ ) {
		e = game.edicts + i;
		if( e->r.inuse && e->r.solid != SOLID_NOT ) {
			client->chase_target = e;
			client->update_chase = qtrue;
			return;
		}
	}

	// failed: stay as observer
	client->update_chase = qfalse;
	client->ps.pmove.pm_type = PM_SPECTATOR;
	G_CenterPrintMsg( ent, "No one to chase" );
}
