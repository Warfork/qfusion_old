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
// g_utils.c -- misc utility functions for game module

#include "g_local.h"

void G_ProjectSource ( vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result )
{
	result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1];
	result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1];
	result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + distance[2];
}

/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
edict_t *G_Find (edict_t *from, size_t fieldofs, char *match)
{
	char	*s;

	if (!from)
		from = world;
	else
		from++;

	for ( ; from < &game.edicts[game.numentities] ; from++)
	{
		if (!from->r.inuse)
			continue;
		s = *(char **) ((qbyte *)from + fieldofs);
		if (!s)
			continue;
		if (!Q_stricmp (s, match))
			return from;
	}

	return NULL;
}


/*
=================
findradius

Returns entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
edict_t *findradius (edict_t *from, vec3_t org, float rad)
{
	vec3_t	eorg;
	int		j;

	if (!from)
		from = world;
	else
		from++;

	for ( ; from < &game.edicts[game.numentities]; from++)
	{
		if (!from->r.inuse)
			continue;
		if (from->r.solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (from->s.origin[j] + (from->r.mins[j] + from->r.maxs[j])*0.5);
		if (VectorLength(eorg) > rad)
			continue;
		return from;
	}

	return NULL;
}


/*
=============
G_PickTarget

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
#define MAXCHOICES	8

edict_t *G_PickTarget (char *targetname)
{
	edict_t	*ent = NULL;
	int		num_choices = 0;
	edict_t	*choice[MAXCHOICES];

	if (!targetname)
	{
		G_Printf ("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	while(1)
	{
		ent = G_Find (ent, FOFS(targetname), targetname);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices)
	{
		G_Printf ("G_PickTarget: target %s not found\n", targetname);
		return NULL;
	}

	return choice[rand() % num_choices];
}



void Think_Delay (edict_t *ent)
{
	G_UseTargets (ent, ent->activator);
	G_FreeEdict (ent);
}

/*
==============================
G_UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets (edict_t *ent, edict_t *activator)
{
	edict_t		*t;

//
// check for a delay
//
	if (ent->delay)
	{
	// create a temp object to fire at a later time
		t = G_Spawn();
		t->classname = "DelayedUse";
		t->nextthink = level.time + ent->delay;
		t->think = Think_Delay;
		t->activator = activator;
		if (!activator)
			G_Printf ("Think_Delay with no activator\n");
		t->message = ent->message;
		t->target = ent->target;
		t->killtarget = ent->killtarget;
		return;
	}
	
	
//
// print the message
//
	if ((ent->message) && !(activator->r.svflags & SVF_MONSTER))
	{
		G_CenterPrintMsg (activator, "%s", ent->message);
		if (ent->noise_index)
			G_Sound (activator, CHAN_AUTO, ent->noise_index, 1, ATTN_NORM);
		else
			G_Sound (activator, CHAN_AUTO, trap_SoundIndex ("sound/misc/talk1.wav"), 1, ATTN_NORM);
	}

//
// kill killtargets
//
	if (ent->killtarget)
	{
		t = NULL;
		while ((t = G_Find (t, FOFS(targetname), ent->killtarget)))
		{
			G_FreeEdict (t);
			if (!ent->r.inuse)
			{
				G_Printf ("entity was removed while using killtargets\n");
				return;
			}
		}
	}

//	G_Printf ("TARGET: activating %s\n", ent->target);

//
// fire targets
//
	if (ent->target)
	{
		t = NULL;
		while ((t = G_Find (t, FOFS(targetname), ent->target)))
		{
			if (t == ent)
			{
				G_Printf ("WARNING: Entity used itself.\n");
			}
			else
			{
				if (t->use)
					t->use (t, ent, activator);
			}
			if (!ent->r.inuse)
			{
				G_Printf ("entity was removed while using targets\n");
				return;
			}
		}
	}
}


vec3_t VEC_UP		= {0, -1, 0};
vec3_t MOVEDIR_UP	= {0, 0, 1};
vec3_t VEC_DOWN		= {0, -2, 0};
vec3_t MOVEDIR_DOWN	= {0, 0, -1};

void G_SetMovedir (vec3_t angles, vec3_t movedir)
{
	if (VectorCompare (angles, VEC_UP))
	{
		VectorCopy (MOVEDIR_UP, movedir);
	}
	else if (VectorCompare (angles, VEC_DOWN))
	{
		VectorCopy (MOVEDIR_DOWN, movedir);
	}
	else
	{
		AngleVectors (angles, movedir, NULL, NULL);
	}

	VectorClear (angles);
}


float vectoyaw (vec3_t vec)
{
	float	yaw;
	
	if (/* vec[YAW] == 0 && */ vec[PITCH] == 0) 
	{
		yaw = 0;
		if (vec[YAW] > 0)
			yaw = 90;
		else if (vec[YAW] < 0)
			yaw = -90;
	}
	else
	{
		yaw = RAD2DEG ( atan2(vec[YAW], vec[PITCH]) );
		if (yaw < 0)
			yaw += 360;
	}

	return yaw;
}


char *G_CopyString (char *in)
{
	char	*out;
	
	out = G_Malloc (strlen(in)+1);
	strcpy (out, in);
	return out;
}


void G_InitEdict (edict_t *e)
{
	e->r.inuse = qtrue;
	e->classname = "noclass";
	e->gravity = 1.0;
	e->s.number = e - game.edicts;
}

/*
=================
G_Spawn

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *G_Spawn (void)
{
	int			i;
	edict_t		*e;

	e = &game.edicts[game.maxclients+1];
	for ( i=game.maxclients+1 ; i<game.numentities ; i++, e++)
	{
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (!e->r.inuse && ( e->freetime < 2 || level.time - e->freetime > 0.5 ) )
		{
			G_InitEdict (e);
			return e;
		}
	}
	
	if (i == game.maxentities)
		G_Error ("ED_Alloc: no free edicts");
		
	game.numentities++;

	trap_LocateEntities (game.edicts, sizeof(game.edicts[0]), game.numentities, game.maxentities);

	G_InitEdict (e);

	return e;
}

/*
=================
G_FreeEdict

Marks the edict as free
=================
*/
void G_FreeEdict (edict_t *ed)
{
	trap_UnlinkEntity (ed);		// unlink from world

	if ((ed - game.edicts) <= (game.maxclients + BODY_QUEUE_SIZE))
	{
//		G_Printf ("tried to free special edict\n");
		return;
	}

	memset (ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->r.inuse = qfalse;
}

/*
============
G_AddEvent

============
*/
void G_AddEvent ( edict_t *ent, int event, int parm, qboolean highPriority )
{
	if ( !ent || ent == world || !ent->r.inuse ) {
		return;
	}
	if ( !event ) {
		return;
	}
	// replace the most outdated low-priority event
	if ( !highPriority ) {
		int oldEventNum = -1;

		if ( !ent->eventPriority[0] && !ent->eventPriority[1] ) {
			oldEventNum = (ent->numEvents + 1) & 2;
		} else if ( !ent->eventPriority[0] ) {
			oldEventNum = 0;
		} else if ( !ent->eventPriority[1] ) {
			oldEventNum = 1;
		}

		// no luck
		if ( oldEventNum == -1 ) {
			return;
		}

		ent->s.events[oldEventNum] = event;
		ent->s.eventParms[oldEventNum] = parm;
		ent->eventPriority[oldEventNum] = qfalse;
		return;
	}

	ent->s.events[ent->numEvents & 1] = event;
	ent->s.eventParms[ent->numEvents & 1] = parm;
	ent->eventPriority[ent->numEvents & 1] = highPriority;
	ent->numEvents++;
}

/*
============
G_SpawnEvent

============
*/
edict_t *G_SpawnEvent ( int event, int parm, vec3_t origin )
{
	edict_t *ent;

	ent = G_Spawn ();
	ent->s.type = ET_EVENT;
	ent->freeAfterEvent = qtrue;
	VectorCopy ( origin, ent->s.origin );
	G_AddEvent ( ent, event, parm, qtrue );

	trap_LinkEntity (ent);

	return ent;
}

/*
============
G_TurnEntityIntoEvent

============
*/
void G_TurnEntityIntoEvent ( edict_t *ent, int event, int parm )
{
	ent->s.type = ET_EVENT;
	ent->r.solid = SOLID_NOT;
	ent->freeAfterEvent = qtrue;
	G_AddEvent ( ent, event, parm, qtrue );

	trap_LinkEntity (ent);
}

/*
============
G_TouchTriggers

============
*/
void G_TouchTriggers (edict_t *ent)
{
	int			i, num;
	edict_t		*touch[MAX_EDICTS], *hit;
	vec3_t		mins, maxs;

	// dead things don't activate triggers!
	if( (ent->r.client || (ent->r.svflags & SVF_MONSTER)) && (ent->health <= 0) )
		return;

	VectorAdd( ent->s.origin, ent->r.mins, mins );
	VectorAdd( ent->s.origin, ent->r.maxs, maxs );

	// FIXME: should be s.origin + mins and s.origin + maxs because of absmin and absmax padding?
	num = trap_BoxEdicts( ent->r.absmin, ent->r.absmax, touch, MAX_EDICTS, AREA_TRIGGERS );

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for( i = 0; i < num; i++ ) {
		hit = touch[i];
		if( !hit->r.inuse )
			continue;
		if( !hit->touch )
			continue;
		if( !hit->item && !trap_EntityContact( mins, maxs, hit ) )
			continue;
		hit->touch (hit, ent, NULL, 0);
	}
}

/*
============
G_TouchSolids

Call after linking a new trigger in during gameplay
to force all entities it covers to immediately touch it
============
*/
void	G_TouchSolids (edict_t *ent)
{
	int			i, num;
	edict_t		*touch[MAX_EDICTS], *hit;
	vec3_t		mins, maxs;

	// FIXME: should be s.origin + mins and s.origin + maxs because of absmin and absmax padding?
	num = trap_BoxEdicts( ent->r.absmin, ent->r.absmax, touch, MAX_EDICTS, AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for( i = 0; i < num; i++ ) {
		hit = touch[i];
		if( !hit->r.inuse )
			continue;

		VectorAdd( hit->s.origin, hit->r.mins, mins );
		VectorAdd( hit->s.origin, hit->r.maxs, maxs );
		if( !ent->item && !trap_EntityContact( mins, maxs, ent ) )
			continue;

		if( ent->touch )
			ent->touch( hit, ent, NULL, 0 );
		if( !ent->r.inuse )
			break;
	}
}

/*
============
G_InitMover

============
*/
void G_InitMover ( edict_t *ent )
{
	ent->r.solid = SOLID_BSP;
	ent->movetype = MOVETYPE_PUSH;

	trap_SetBrushModel ( ent, ent->model );

	if ( ent->model2 ) {
		ent->s.modelindex2 = trap_ModelIndex( ent->model2 );
	}

	if (ent->light || !VectorCompare(ent->color, vec3_origin))
	{
		int r, g, b, i;

		if ( !ent->light )
			i = 100;
		else 
			i = ent->light;

		i /= 4;
		i = min (i, 255);

		r = ent->color[0];
		if ( r <= 1.0 ) {
			r *= 255;
		}
		clamp (r, 0, 255);

		g = ent->color[1];
		if ( g <= 1.0 ) {
			g *= 255;
		}
		clamp (g, 0, 255);

		b = ent->color[2];
		if ( b <= 1.0 ) {
			b *= 255;
		}
		clamp (b, 0, 255);

		ent->s.light = COLOR_RGBA ( r, g, b, i );
	}
}

/*
============
G_PlayerGender

============
*/
int G_PlayerGender ( edict_t *player )
{
	char		*info;

	if ( !player->r.client ) {
		return GENDER_NEUTRAL;
	}

	info = Info_ValueForKey ( player->r.client->pers.userinfo, "skin" );

	if ( info[0] == 'm' || info[0] == 'M' ) {
		return GENDER_MALE;
	} else if ( info[0] == 'f' || info[0] == 'F' ) {
		return GENDER_FEMALE;
	}

	return GENDER_NEUTRAL;
}

/*
============
G_PrintMsg

NULL sends to all the message to all clients 
============
*/
void G_PrintMsg ( edict_t *ent, int level, char *fmt, ... )
{
	char		msg[1024];
	va_list		argptr;
	char		*s, *p;

	va_start( argptr, fmt );
	vsnprintf( msg, sizeof(msg) - 1, fmt, argptr );
	va_end( argptr );
	msg[sizeof(msg)-1] = 0;

	// double quotes are bad
	while ((p = strchr(msg, '\"')) != NULL)
		*p = '\'';

	s = va ( "pr %i \"%s\"", level, msg );

	if ( !ent ) {
		int i;	

		for ( i=0; i<game.maxclients; i++)
		{
			ent = game.edicts + 1 + i;
			if (!ent->r.inuse)
				continue;
			if (!ent->r.client || level < ent->r.client->pers.messagelevel)
				continue;
			trap_ServerCmd( ent, s );
		}

		// mirror at server console
		if( dedicated->integer )
			G_Printf( "%s", msg );
		return;
	}

	if( ent->r.inuse && ent->r.client && (level >= ent->r.client->pers.messagelevel) )
		trap_ServerCmd( ent, s );
}

/*
============
G_CenterPrintMsg

NULL sends to all the message to all clients 
============
*/
void G_CenterPrintMsg ( edict_t *ent, char *fmt, ... )
{
	char		msg[1024];
	va_list		argptr;
	char		*p;

	va_start( argptr, fmt );
	vsnprintf( msg, sizeof(msg), fmt, argptr );
	va_end( argptr );
	msg[sizeof(msg)-1] = 0;

	// double quotes are bad
	while ((p = strchr(msg, '\"')) != NULL)
		*p = '\'';

	trap_ServerCmd ( ent, va ( "cp \"%s\"", msg ) );
}

/*
============
G_Obituary

Prints death message to all clients
============
*/
void G_Obituary ( edict_t *victim, edict_t *attacker, int mod )
{
	if( victim && attacker )
		trap_ServerCmd ( NULL, va ( "obry %i %i %i", victim - game.edicts, attacker - game.edicts, mod ) );
}

/*
=================
G_Sound
=================
*/
void G_Sound ( edict_t *ent, int channel, int soundindex, float volume, float attenuation )
{
	if( ent )
		trap_Sound ( NULL, ent, channel, soundindex, volume, attenuation );
}

/*
=================
G_PositionedSound
=================
*/
void G_PositionedSound ( vec3_t origin, edict_t *ent, int channel, int soundindex, float volume, float attenuation )
{
	if( origin || ent )
		trap_Sound ( origin, ent, channel, soundindex, volume, attenuation );
}

/*
=================
G_GlobalSound
=================
*/
void G_GlobalSound ( int channel, int soundindex )
{
	trap_Sound ( vec3_origin, game.edicts, channel | CHAN_NO_PHS_ADD | CHAN_RELIABLE, soundindex, 1.0, ATTN_NONE );
}

/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
qboolean KillBox (edict_t *ent)
{
	trace_t		tr;

	while (1)
	{
		trap_Trace (&tr, ent->s.origin, ent->r.mins, ent->r.maxs, ent->s.origin, NULL, MASK_PLAYERSOLID);
		if (tr.fraction == 1)
			break;

		// nail it
		T_Damage (&game.edicts[tr.ent], ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);

		// if we didn't kill it, fail
		if (game.edicts[tr.ent].r.solid)
			return qfalse;
	}

	return qtrue;		// all clear
}
