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

#include "q_shared.h"
#include "gs_public.h"

// this table is needed by both the game dll and client game dll,
// the game needs it to source shot locations, the client
// needs it to position muzzle flashes
const vec3_t monster_flash_offset [] =
{
// flash 0 is not used
	0.0, 0.0, 0.0,

// MZ2_TANK_BLASTER_1				1
	20.7, -18.5, 28.7,
// MZ2_TANK_BLASTER_2				2
	16.6, -21.5, 30.1,
// MZ2_TANK_BLASTER_3				3
	11.8, -23.9, 32.1,
// MZ2_TANK_MACHINEGUN_1			4
	22.9, -0.7, 25.3,
// MZ2_TANK_MACHINEGUN_2			5
	22.2, 6.2, 22.3,
// MZ2_TANK_MACHINEGUN_3			6
	19.4, 13.1, 18.6,
// MZ2_TANK_MACHINEGUN_4			7
	19.4, 18.8, 18.6,
// MZ2_TANK_MACHINEGUN_5			8
	17.9, 25.0, 18.6,
// MZ2_TANK_MACHINEGUN_6			9
	14.1, 30.5, 20.6,
// MZ2_TANK_MACHINEGUN_7			10
	9.3, 35.3, 22.1,
// MZ2_TANK_MACHINEGUN_8			11
	4.7, 38.4, 22.1,
// MZ2_TANK_MACHINEGUN_9			12
	-1.1, 40.4, 24.1,
// MZ2_TANK_MACHINEGUN_10			13
	-6.5, 41.2, 24.1,
// MZ2_TANK_MACHINEGUN_11			14
	3.2, 40.1, 24.7,
// MZ2_TANK_MACHINEGUN_12			15
	11.7, 36.7, 26.0,
// MZ2_TANK_MACHINEGUN_13			16
	18.9, 31.3, 26.0,
// MZ2_TANK_MACHINEGUN_14			17
	24.4, 24.4, 26.4,
// MZ2_TANK_MACHINEGUN_15			18
	27.1, 17.1, 27.2,
// MZ2_TANK_MACHINEGUN_16			19
	28.5, 9.1, 28.0,
// MZ2_TANK_MACHINEGUN_17			20
	27.1, 2.2, 28.0,
// MZ2_TANK_MACHINEGUN_18			21
	24.9, -2.8, 28.0,
// MZ2_TANK_MACHINEGUN_19			22
	21.6, -7.0, 26.4,
// MZ2_TANK_ROCKET_1				23
	6.2, 29.1, 49.1,
// MZ2_TANK_ROCKET_2				24
	6.9, 23.8, 49.1,
// MZ2_TANK_ROCKET_3				25
	8.3, 17.8, 49.5,

// MZ2_INFANTRY_MACHINEGUN_1		26
	26.6, 7.1, 13.1,
// MZ2_INFANTRY_MACHINEGUN_2		27
	18.2, 7.5, 15.4,
// MZ2_INFANTRY_MACHINEGUN_3		28
	17.2, 10.3, 17.9,
// MZ2_INFANTRY_MACHINEGUN_4		29
	17.0, 12.8, 20.1,
// MZ2_INFANTRY_MACHINEGUN_5		30
	15.1, 14.1, 21.8,
// MZ2_INFANTRY_MACHINEGUN_6		31
	11.8, 17.2, 23.1,
// MZ2_INFANTRY_MACHINEGUN_7		32
	11.4, 20.2, 21.0,
// MZ2_INFANTRY_MACHINEGUN_8		33
	9.0, 23.0, 18.9,
// MZ2_INFANTRY_MACHINEGUN_9		34
	13.9, 18.6, 17.7,
// MZ2_INFANTRY_MACHINEGUN_10		35
	15.4, 15.6, 15.8,
// MZ2_INFANTRY_MACHINEGUN_11		36
	10.2, 15.2, 25.1,
// MZ2_INFANTRY_MACHINEGUN_12		37
	-1.9, 15.1, 28.2,
// MZ2_INFANTRY_MACHINEGUN_13		38
	-12.4, 13.0, 20.2,

// MZ2_SOLDIER_BLASTER_1			39
	10.6 * 1.2, 7.7 * 1.2, 7.8 * 1.2,
// MZ2_SOLDIER_BLASTER_2			40
	21.1 * 1.2, 3.6 * 1.2, 19.0 * 1.2,
// MZ2_SOLDIER_SHOTGUN_1			41
	10.6 * 1.2, 7.7 * 1.2, 7.8 * 1.2,
// MZ2_SOLDIER_SHOTGUN_2			42
	21.1 * 1.2, 3.6 * 1.2, 19.0 * 1.2,
// MZ2_SOLDIER_MACHINEGUN_1			43
	10.6 * 1.2, 7.7 * 1.2, 7.8 * 1.2,
// MZ2_SOLDIER_MACHINEGUN_2			44
	21.1 * 1.2, 3.6 * 1.2, 19.0 * 1.2,

// MZ2_GUNNER_MACHINEGUN_1			45
	30.1 * 1.15, 3.9 * 1.15, 19.6 * 1.15,
// MZ2_GUNNER_MACHINEGUN_2			46
	29.1 * 1.15, 2.5 * 1.15, 20.7 * 1.15,
// MZ2_GUNNER_MACHINEGUN_3			47
	28.2 * 1.15, 2.5 * 1.15, 22.2 * 1.15,
// MZ2_GUNNER_MACHINEGUN_4			48
	28.2 * 1.15, 3.6 * 1.15, 22.0 * 1.15,
// MZ2_GUNNER_MACHINEGUN_5			49
	26.9 * 1.15, 2.0 * 1.15, 23.4 * 1.15,
// MZ2_GUNNER_MACHINEGUN_6			50
	26.5 * 1.15, 0.6 * 1.15, 20.8 * 1.15,
// MZ2_GUNNER_MACHINEGUN_7			51
	26.9 * 1.15, 0.5 * 1.15, 21.5 * 1.15,
// MZ2_GUNNER_MACHINEGUN_8			52
	29.0 * 1.15, 2.4 * 1.15, 19.5 * 1.15,
// MZ2_GUNNER_GRENADE_1				53
	4.6 * 1.15, -16.8 * 1.15, 7.3 * 1.15,
// MZ2_GUNNER_GRENADE_2				54
	4.6 * 1.15, -16.8 * 1.15, 7.3 * 1.15,
// MZ2_GUNNER_GRENADE_3				55
	4.6 * 1.15, -16.8 * 1.15, 7.3 * 1.15,
// MZ2_GUNNER_GRENADE_4				56
	4.6 * 1.15, -16.8 * 1.15, 7.3 * 1.15,

// MZ2_CHICK_ROCKET_1				57
	24.8, -9.0, 39.0,
// MZ2_FLYER_BLASTER_1				58
	12.1, 13.4, -14.5,
// MZ2_FLYER_BLASTER_2				59
	12.1, -7.4, -14.5,

// MZ2_MEDIC_BLASTER_1				60
	12.1, 5.4, 16.5,

// MZ2_GLADIATOR_RAILGUN_1			61
	30.0, 18.0, 28.0,

// MZ2_HOVER_BLASTER_1				62
	32.5, -0.8, 10.0,

// MZ2_ACTOR_MACHINEGUN_1			63
	18.4, 7.4, 9.6,

// MZ2_SUPERTANK_MACHINEGUN_1		64
	30.0, 30.0, 88.5,
// MZ2_SUPERTANK_MACHINEGUN_2		65
	30.0, 30.0, 88.5,
// MZ2_SUPERTANK_MACHINEGUN_3		66
	30.0, 30.0, 88.5,
// MZ2_SUPERTANK_MACHINEGUN_4		67
	30.0, 30.0, 88.5,
// MZ2_SUPERTANK_MACHINEGUN_5		68
	30.0, 30.0, 88.5,
// MZ2_SUPERTANK_MACHINEGUN_6		69
	30.0, 30.0, 88.5,
// MZ2_SUPERTANK_ROCKET_1			70
	16.0, -22.5, 91.2,
// MZ2_SUPERTANK_ROCKET_2			71
	16.0, -33.4, 86.7,
// MZ2_SUPERTANK_ROCKET_3			72
	16.0, -42.8, 83.3,

// --- Start Xian Stuff ---
// MZ2_BOSS2_MACHINEGUN_L1			73
	32,	-40,	70,
// MZ2_BOSS2_MACHINEGUN_L2			74
	32,	-40,	70,
// MZ2_BOSS2_MACHINEGUN_L3			75
	32,	-40,	70,
// MZ2_BOSS2_MACHINEGUN_L4			76
	32,	-40,	70,
// MZ2_BOSS2_MACHINEGUN_L5			77
	32,	-40,	70,
// --- End Xian Stuff

// MZ2_BOSS2_ROCKET_1				78
	22.0, 16.0, 10.0,
// MZ2_BOSS2_ROCKET_2				79
	22.0, 8.0, 10.0,
// MZ2_BOSS2_ROCKET_3				80
	22.0, -8.0, 10.0,
// MZ2_BOSS2_ROCKET_4				81
	22.0, -16.0, 10.0,

// MZ2_FLOAT_BLASTER_1				82
	32.5, -0.8, 10,

// MZ2_SOLDIER_BLASTER_3			83
	20.8 * 1.2, 10.1 * 1.2, -2.7 * 1.2,
// MZ2_SOLDIER_SHOTGUN_3			84
	20.8 * 1.2, 10.1 * 1.2, -2.7 * 1.2,
// MZ2_SOLDIER_MACHINEGUN_3			85
	20.8 * 1.2, 10.1 * 1.2, -2.7 * 1.2,
// MZ2_SOLDIER_BLASTER_4			86
	7.6 * 1.2, 9.3 * 1.2, 0.8 * 1.2,
// MZ2_SOLDIER_SHOTGUN_4			87
	7.6 * 1.2, 9.3 * 1.2, 0.8 * 1.2,
// MZ2_SOLDIER_MACHINEGUN_4			88
	7.6 * 1.2, 9.3 * 1.2, 0.8 * 1.2,
// MZ2_SOLDIER_BLASTER_5			89
	30.5 * 1.2, 9.9 * 1.2, -18.7 * 1.2,
// MZ2_SOLDIER_SHOTGUN_5			90
	30.5 * 1.2, 9.9 * 1.2, -18.7 * 1.2,
// MZ2_SOLDIER_MACHINEGUN_5			91
	30.5 * 1.2, 9.9 * 1.2, -18.7 * 1.2,
// MZ2_SOLDIER_BLASTER_6			92
	27.6 * 1.2, 3.4 * 1.2, -10.4 * 1.2,
// MZ2_SOLDIER_SHOTGUN_6			93
	27.6 * 1.2, 3.4 * 1.2, -10.4 * 1.2,
// MZ2_SOLDIER_MACHINEGUN_6			94
	27.6 * 1.2, 3.4 * 1.2, -10.4 * 1.2,
// MZ2_SOLDIER_BLASTER_7			95
	28.9 * 1.2, 4.6 * 1.2, -8.1 * 1.2,
// MZ2_SOLDIER_SHOTGUN_7			96
	28.9 * 1.2, 4.6 * 1.2, -8.1 * 1.2,
// MZ2_SOLDIER_MACHINEGUN_7			97
	28.9 * 1.2, 4.6 * 1.2, -8.1 * 1.2,
// MZ2_SOLDIER_BLASTER_8			98
//	34.5 * 1.2, 9.6 * 1.2, 6.1 * 1.2,
	31.5 * 1.2, 9.6 * 1.2, 10.1 * 1.2,
// MZ2_SOLDIER_SHOTGUN_8			99
	34.5 * 1.2, 9.6 * 1.2, 6.1 * 1.2,
// MZ2_SOLDIER_MACHINEGUN_8			100
	34.5 * 1.2, 9.6 * 1.2, 6.1 * 1.2,

// --- Xian shit below ---
// MZ2_MAKRON_BFG					101
	17,		-19.5,	62.9,
// MZ2_MAKRON_BLASTER_1				102
	-3.6,	-24.1,	59.5,
// MZ2_MAKRON_BLASTER_2				103
	-1.6,	-19.3,	59.5,
// MZ2_MAKRON_BLASTER_3				104
	-0.1,	-14.4,	59.5,		
// MZ2_MAKRON_BLASTER_4				105
	2.0,	-7.6,	59.5,	
// MZ2_MAKRON_BLASTER_5				106
	3.4,	1.3,	59.5,
// MZ2_MAKRON_BLASTER_6				107
	3.7,	11.1,	59.5,	
// MZ2_MAKRON_BLASTER_7				108
	-0.3,	22.3,	59.5,
// MZ2_MAKRON_BLASTER_8				109
	-6,		33,		59.5,
// MZ2_MAKRON_BLASTER_9				110
	-9.3,	36.4,	59.5,
// MZ2_MAKRON_BLASTER_10			111
	-7,		35,		59.5,
// MZ2_MAKRON_BLASTER_11			112
	-2.1,	29,		59.5,
// MZ2_MAKRON_BLASTER_12			113
	3.9,	17.3,	59.5,
// MZ2_MAKRON_BLASTER_13			114
	6.1,	5.8,	59.5,
// MZ2_MAKRON_BLASTER_14			115
	5.9,	-4.4,	59.5,
// MZ2_MAKRON_BLASTER_15			116
	4.2,	-14.1,	59.5,		
// MZ2_MAKRON_BLASTER_16			117
	2.4,	-18.8,	59.5,
// MZ2_MAKRON_BLASTER_17			118
	-1.8,	-25.5,	59.5,
// MZ2_MAKRON_RAILGUN_1				119
	-17.3,	7.8,	72.4,

// MZ2_JORG_MACHINEGUN_L1			120
	78.5,	-47.1,	96,			
// MZ2_JORG_MACHINEGUN_L2			121
	78.5,	-47.1,	96,			
// MZ2_JORG_MACHINEGUN_L3			122
	78.5,	-47.1,	96,			
// MZ2_JORG_MACHINEGUN_L4			123
	78.5,	-47.1,	96,			
// MZ2_JORG_MACHINEGUN_L5			124
	78.5,	-47.1,	96,			
// MZ2_JORG_MACHINEGUN_L6			125
	78.5,	-47.1,	96,			
// MZ2_JORG_MACHINEGUN_R1			126
	78.5,	46.7,  96,			
// MZ2_JORG_MACHINEGUN_R2			127
	78.5,	46.7,	96,			
// MZ2_JORG_MACHINEGUN_R3			128
	78.5,	46.7,	96,			
// MZ2_JORG_MACHINEGUN_R4			129
	78.5,	46.7,	96,			
// MZ2_JORG_MACHINEGUN_R5			130
	78.5,	46.7,	96,			
// MZ2_JORG_MACHINEGUN_R6			131
	78.5,	46.7,	96,			
// MZ2_JORG_BFG_1					132
	6.3,	-9,		111.2,

// MZ2_BOSS2_MACHINEGUN_R1			73
	32,	40,	70,
// MZ2_BOSS2_MACHINEGUN_R2			74
	32,	40,	70,
// MZ2_BOSS2_MACHINEGUN_R3			75
	32,	40,	70,
// MZ2_BOSS2_MACHINEGUN_R4			76
	32,	40,	70,
// MZ2_BOSS2_MACHINEGUN_R5			77
	32,	40,	70,

// --- End Xian Shit ---

// end of table
	0.0, 0.0, 0.0
};

//============================================================================

/*
================
GS_Obituary

Can be called by either the server or the client
================
*/
void GS_Obituary ( void *victim, int gender, void *attacker, int mod, char *message, char *message2 )
{
	message [0] = 0;
	message2[0] = 0;

	switch (mod)
	{
		case MOD_SUICIDE:
			strcpy ( message, "suicides" );
			break;
		case MOD_FALLING:
			strcpy ( message, "cratered" );
			break;
		case MOD_CRUSH:
			strcpy ( message, "was squished" );
			break;
		case MOD_WATER:
			strcpy ( message, "sank like a rock" );
			break;
		case MOD_SLIME:
			strcpy ( message, "melted" );
			break;
		case MOD_LAVA:
			strcpy ( message, "does a back flip into the lava" );
			break;
		case MOD_EXPLOSIVE:
		case MOD_BARREL:
			strcpy ( message, "blew up" );
			break;
		case MOD_EXIT:
			strcpy ( message, "found a way out" );
			break;
		case MOD_TARGET_LASER:
			strcpy ( message, "saw the light" );
			break;
		case MOD_TARGET_BLASTER:
			strcpy ( message, "got blasted" );
			break;
		case MOD_BOMB:
		case MOD_SPLASH:
		case MOD_TRIGGER_HURT:
			strcpy ( message, "was in the wrong place" );
			break;
	}
	
	if ( attacker == victim ) 
	{
		switch (mod)
		{
		case MOD_HG_SPLASH:
		case MOD_G_SPLASH:
			if ( gender == GENDER_FEMALE )
				strcpy ( message, "tripped on her own grenade" );
			else if ( gender == GENDER_MALE )
				strcpy ( message, "tripped on his own grenade" );
			else
				strcpy ( message, "tripped on its own grenade" );
			break;
		case MOD_R_SPLASH:
			if ( gender == GENDER_FEMALE )
				strcpy ( message, "blew herself up" );
			else if ( gender == GENDER_MALE )
				strcpy ( message, "blew himself up" );
			else
				strcpy ( message, "blew itself up" );
			break;
		case MOD_BFG_BLAST:
			strcpy ( message, "should have used a smaller gun" );
			break;
		default:
			if ( gender == GENDER_FEMALE )
				strcpy ( message, "killed herself" );
			else if ( gender == GENDER_MALE )
				strcpy ( message, "killed himself" );
			else
				strcpy ( message, "killed itself" );
			break;
		}
	}

	// we already have one
	if ( message[0] ) {
		return;
	}

	if ( attacker ) {
		switch (mod)
		{
		case MOD_BLASTER:
			strcpy ( message, "was blasted by" );
			break;
		case MOD_SHOTGUN:
			strcpy ( message, "was gunned down by" );
			break;
		case MOD_SSHOTGUN:
			strcpy ( message, "was blown away by" );
			strcpy ( message2, "'s super shotgun" );
			break;
		case MOD_MACHINEGUN:
			strcpy ( message, "was machinegunned by" );
			break;
		case MOD_CHAINGUN:
			strcpy ( message, "was cut in half by" );
			strcpy ( message2, "'s chaingun" );
			break;
		case MOD_GRENADE:
			strcpy ( message, "was popped by" );
			strcpy ( message2, "'s grenade" );
			break;
		case MOD_G_SPLASH:
			strcpy ( message, "was shredded by" );
			strcpy ( message, "'s shrapnel" );
			break;
		case MOD_ROCKET:
			strcpy ( message, "ate" );
			strcpy ( message2, "'s rocket" );
			break;
		case MOD_R_SPLASH:
			strcpy ( message, "almost dodged" );
			strcpy ( message2, "'s rocket" );
			break;
		case MOD_HYPERBLASTER:
			strcpy ( message, "was melted by" );
			strcpy ( message2,"'s hyperblaster" );
			break;
		case MOD_RAILGUN:
			strcpy ( message, "was railed by" );
			break;
		case MOD_BFG_LASER:
			strcpy ( message, "saw the pretty lights from" );
			strcpy ( message2, "'s BFG" );
			break;
		case MOD_BFG_BLAST:
			strcpy ( message, "was disintegrated by" );
			strcpy ( message2, "'s BFG blast" );
			break;
		case MOD_BFG_EFFECT:
			strcpy ( message, "couldn't hide from" );
			strcpy ( message2, "'s BFG" );
			break;
		case MOD_HANDGRENADE:
			strcpy ( message, "caught" );
			strcpy ( message2, "'s handgrenade" );
			break;
		case MOD_HG_SPLASH:
			strcpy ( message, "didn't see" );
			strcpy ( message2, "'s handgrenade" );
			break;
		case MOD_TELEFRAG:
			strcpy ( message, "tried to invade" );
			strcpy ( message2, "'s personal space" );
			break;
			//ZOID
		case MOD_GRAPPLE:
			strcpy ( message, "was caught by" );
			strcpy ( message2, "'s grapple" );
			break;
			//ZOID

		default:
			strcpy ( message, "was killed by" );
			break;
		}
		
		if ( message[0] ) {
			return;
		}
	}

	strcpy ( message, " died." );
}
