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
// g_pmodels.h -- game module definition of player model animations
// by Jalisko

// client_t->anim_priority
#define	ANIM_BASIC		0		// stand / run
#define	ANIM_WAVE		1
#define	ANIM_PAIN		2
#define	ANIM_ATTACK		3
#define ANIM_HOLD		4
#define ANIM_CINEMATIC	5
#define	ANIM_DEATH		6

// timers
#define ANIM_DEATH_TIME			3000	// time milliseconds to wait for the death animations (for respawning pause)
#define TORSO_FLIPOUT_TIME		500		// time in milliseconds of the weaponOut animation (for synchronization with weaponIn)

// movement flags for animation control
#define ANIMMOVE_FRONT			0x00000001	//	Player is pressing fordward
#define ANIMMOVE_BACK			0x00000002	//	Player is pressing backpedal
#define ANIMMOVE_LEFT			0x00000004	//	Player is pressing sideleft
#define ANIMMOVE_RIGHT			0x00000008	//	Player is pressing sideright
#define ANIMMOVE_WALK			0x00000010	//	Player is pressing the walk key
#define ANIMMOVE_RUN			0x00000020	//	Player is running
#define ANIMMOVE_DUCK			0x00000040	//	Player is crouching
#define ANIMMOVE_SWIM			0x00000080	//	Player is swimming
#define ANIMMOVE_GRAPPLE		0x00000100	//	Player is using grapple


typedef struct
{
	qboolean	anim_jump;
	qboolean	anim_jump_thunk;		// think jump animation
	qboolean	anim_jump_prestep;		// done/undone
	int			anim_jump_style;		// jump style (left step, right step, backwards)
	int			anim_moveflags;			// moving direction
	int			anim_oldmoveflags;

	int			anim[PMODEL_PARTS];
	int			anim_priority[PMODEL_PARTS];

} pmanim_t;


typedef struct
{
	int			mode;				// 3rd or 1st person
	int			range;
	int			keyNext;			// pressed jump key = next player
	int			keytime;			// jump key delay
	int			timeout;			// delay after loosing target
} chasecam_t;

