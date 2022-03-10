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
// splitmodels - by Jalisko

// -Torso DEATH frames and Legs DEATH frames must be the same.


// #define	_ANIM_PRESTEP	// enable for pre-stepping animation

// ANIMATIONS

#define ANIM_NONE					0	// not listed in the config

/*
============================
	 animation config
============================
*/

// Vic: FIXME: make this enum?

// DEATHS

#define BOTH_DEATH1					1	// Death1
#define BOTH_DEAD1					2	// DEAD versions are only used for the dead corpses, and are frozen animations on the last frame
#define BOTH_DEATH2					3	// Death2
#define BOTH_DEAD2					4	// corpse on the ground
#define BOTH_DEATH3					5	// Death3
#define BOTH_DEAD3					6	// corpse on the ground

// TORSOS

#define TORSO_GESTURE				7	// (wave)
#define TORSO_ATTACK1				8	// Attack
#define TORSO_ATTACK2				9	// Attack2 (gaunlet) (DROP replacement)
#define TORSO_FLIPOUT				10	// (weapon drop)
#define TORSO_FLIPIN				11	// (weapon raise)
#define TORSO_STAND					12	// Stand
#define TORSO_STAND2				13	// Stand2 (gaunlet) (PAIN replacement)

//-----------			offset

// LEGS

#define LEGS_CRWALK					14	// Crouched Walk
#define LEGS_WALKFWD				15	// WalkFordward (few frames of this will show when moving by inertia)
#define LEGS_RUNFWD					16	// RunFordward
#define LEGS_RUNBACK				17	// RunBackward
#define LEGS_SWIMFWD				18	// Swim Fordward
#define LEGS_JUMP1					19	// Left leg Jump
#define LEGS_JUMP1ST				20	// Left leg land
#define LEGS_JUMP3					21	// Stand & Backwards jump
#define LEGS_JUMP3ST				22	// Stand & Backwards land
#define LEGS_STAND					23	// Stand
#define LEGS_CRSTAND				24	// Crouched Stand
#define LEGS_TURN					25	// good for nothing. Not used. It's here just for Q3 models support

//-----------		jal extra stuff

#define LEGS_JUMP2					26	// Right leg Jump
#define LEGS_JUMP2ST				27	// Right leg land
#define LEGS_SWIM					28	// Stand & Backwards Swim
#define LEGS_WALKBACK				29	// WalkBackward
#define LEGS_WALKLEFT				30	// WalkLeft
#define LEGS_WALKRIGHT				31	// WalkRight
#define LEGS_RUNLEFT				32	// RunLeft
#define LEGS_RUNRIGHT				33	// RunRight
#define TORSO_RUN					34	// Run (also used for jump)
#define TORSO_SWIM					35	// Swim
#define TORSO_DROPHOLD				36	// Drop hold is only used at handgrenade activation
#define TORSO_DROP					37	// Drop: true DROP animation
#define TORSO_PAIN1					38	// Pain1
#define TORSO_PAIN2					39	// Pain2
#define TORSO_PAIN3					40	// Pain3
#define PMODEL_MAX_ANIMATIONS		41





