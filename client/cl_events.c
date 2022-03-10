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
#include "client.h"


/*
==============
CL_ItemRespawn
==============
*/
void CL_ItemRespawn ( entity_state_t *ent ) 
{
	cl_entities[ent->number].respawnTime = cl.time;
	S_StartSound ( NULL, ent->number, CHAN_WEAPON, cl.media.sfxItemRespawn, 1, ATTN_IDLE, 0 );
}

/*
==============
CL_Footstep
==============
*/
void CL_Footstep ( entity_state_t *ent, int footstep ) 
{
	if ( cl_footsteps->value ) {
		S_StartSound ( NULL, ent->number, CHAN_BODY, cl.media.sfxFootsteps[footstep][rand()&3], 1, ATTN_NORM, 0 );
	}
}

/*
==============
CL_GrenadeBounce
==============
*/
void CL_GrenadeBounce ( entity_state_t *ent ) 
{
	if ( rand() & 1) {
		S_StartSound ( NULL, ent->number, CHAN_VOICE, cl.media.sfxGrenBounce1, 1, ATTN_NORM, 0 );
	} else {
		S_StartSound ( NULL, ent->number, CHAN_VOICE, cl.media.sfxGrenBounce2, 1, ATTN_NORM, 0 );
	}
}

/*
==============
CL_JumpPad
==============
*/
void CL_JumpPad ( entity_state_t *ent ) 
{
	S_StartSound ( NULL, ent->number, CHAN_AUTO, cl.media.sfxJumpPad, 1, ATTN_NORM, 0 );
	S_StartSound ( NULL, ent->number, CHAN_VOICE, S_RegisterSound ("*jump1.wav"), 1, ATTN_NORM, 0 );
}

/*
==============
CL_Jump
==============
*/
void CL_Jump ( entity_state_t *ent ) 
{
	S_StartSound ( NULL, ent->number, CHAN_VOICE, S_RegisterSound ("*jump1.wav"), 1, ATTN_NORM, 0 );
}

/*
==============
CL_FallShort
==============
*/
void CL_FallShort ( entity_state_t *ent ) 
{
	S_StartSound ( NULL, ent->number, CHAN_AUTO, cl.media.sfxLand, 1, ATTN_NORM, 0 );
}

/*
==============
CL_FallMedium
==============
*/
void CL_FallMedium ( entity_state_t *ent ) 
{
	S_StartSound ( NULL, ent->number, CHAN_AUTO, S_RegisterSound ("*fall1.wav"), 1, ATTN_NORM, 0 );
}

/*
==============
CL_FallFar
==============
*/
void CL_FallFar ( entity_state_t *ent ) 
{
	S_StartSound ( NULL, ent->number, CHAN_AUTO, S_RegisterSound ("*fall1.wav"), 1, ATTN_NORM, 0 );
}

/*
==============
CL_Pain
==============
*/
void CL_Pain ( entity_state_t *ent, int percent ) 
{
	int r = 1 + (rand()&1);
	S_StartSound ( NULL, ent->number, CHAN_VOICE, S_RegisterSound(va("*pain%i_%i.wav", percent, r)), 1, ATTN_NORM, 0 );
}

/*
==============
CL_Die
==============
*/
void CL_Die ( entity_state_t *ent ) 
{
	int r = ( rand()%4 ) + 1;
	S_StartSound ( NULL, ent->number, CHAN_BODY, S_RegisterSound (va("*death%i.wav", r)), 1, ATTN_NORM, 0 );
}

/*
==============
CL_Gib
==============
*/
void CL_Gib ( entity_state_t *ent ) 
{
	S_StartSound ( NULL, ent->number, CHAN_VOICE, cl.media.sfxGibSound, 1, ATTN_NORM, 0 );
}

/*
==============
CL_EntityEvent

An entity has just been parsed that has an event value

the female events are there for backwards compatibility
==============
*/
void CL_EntityEvent (entity_state_t *ent)
{
	switch (ent->event)
	{
		case EV_ITEM_RESPAWN:
			CL_ItemRespawn ( ent );
			break;

		case EV_FOOTSTEP:
			CL_Footstep ( ent, FOOTSTEP_NORMAL );
			break;

		case EV_FOOTSTEP_METAL:
			CL_Footstep ( ent, FOOTSTEP_METAL );
			break;

		case EV_FOOTSPLASH:
			CL_Footstep ( ent, FOOTSTEP_SPLASH );
			break;

		case EV_SWIM:
			CL_Footstep ( ent, FOOTSTEP_SPLASH );
			break;

		case EV_GRENADE_BOUNCE:
			CL_GrenadeBounce ( ent );
			break;

		case EV_JUMP_PAD:
			CL_JumpPad ( ent );
			break;
			
		case EV_JUMP:
			CL_Jump ( ent );
			break;

		case EV_FALL_SHORT:
			CL_FallShort ( ent );
			break;

		case EV_FALL_MEDIUM:
			CL_FallMedium ( ent );
			break;

		case EV_FALL_FAR:
			CL_FallFar ( ent );
			break;

		case EV_PAIN100:
		case EV_PAIN75:
		case EV_PAIN50:
		case EV_PAIN25:
			CL_Pain ( ent, 100 - 25 * ( ent->event - EV_PAIN100 ) );
			break;

		case EV_DIE:
			CL_Die ( ent );
			break;

		case EV_GIB:
			CL_Gib ( ent );
			break;
	}
}
