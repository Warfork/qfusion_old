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

// ui_pmodels.h - drawing the player models in split parts
// by jalisko

#ifndef SKELMOD

#include "../game/gs_pmodels.h"

enum {
	BASIC_CHANNEL,	
	EVENT_CHANNEL,
	MIXER_CHANNEL,

	ANIMBUFFER_CHANNELS
};


typedef struct
{
	int		newanim[PMODEL_PARTS];
	int		channel[ANIMBUFFER_CHANNELS];
	
} animationbuffer_t;


typedef struct
{
	char	filename[MAX_QPATH];	// animation.cfg path for availability checks.
									
	int		firstframe[PMODEL_MAX_ANIMS];
	int		lastframe[PMODEL_MAX_ANIMS];
	int		loopingframes[PMODEL_MAX_ANIMS];
	float	frametime[PMODEL_MAX_ANIMS];		// 1 frame time in miliseconds

	int		current[PMODEL_PARTS];				// running animation
	int		currentChannel[PMODEL_PARTS];		// what channel it came from

	int		frame[PMODEL_PARTS];
	int		oldframe[PMODEL_PARTS];

	float	prevframetime[PMODEL_PARTS];
	float	nextframetime[PMODEL_PARTS];
	float	backlerp[PMODEL_PARTS];

	animationbuffer_t	buffer[ANIMBUFFER_CHANNELS];
	
	int		frame_delay;

} animationinfo_t;

typedef struct
{
	char				model_name[MAX_QPATH];
	char				skin_name[MAX_QPATH];

	int					sex;

	struct	model_s		*model[PMODEL_PARTS];
	struct	skinfile_s	*customSkin[PMODEL_PARTS];	// skin files

	animationinfo_t		anim;
	
} ui_pmodel_t;


typedef struct
{
	int		width;		// window size
	int		height;
	int		x;			// window origin (relative to center of screen)
	int		y;

	int		fov;

	struct shader_s *shader;	// background image

} pmwindow_t;


typedef struct
{
	int		loweranim;
	int		upperanim;
	int		headanim;
	int		channel;
	int		delayTime;
	char	*playsound;
} animgroup_t;

typedef struct pmodelitem_s
{
	char			name[MAX_QPATH];

	ui_pmodel_t		pmodel;

	qboolean		inuse;
	int				registerTime;	// cl.time at what the model was registered

	vec3_t			origin;
	vec3_t			angles;
	qboolean		effectRotate;

	animgroup_t		*animgroup;			// set of animations to reproduce

	int				animgroupTime;		// when the last animation was launched
	int				animgroupInitTime;
	int				numanims;

	pmwindow_t	window;

} pmodelitem_t;


#define	MAX_UI_PMODELS	32
extern pmodelitem_t	uiPlayerModelItems[MAX_UI_PMODELS];


//
// ui_pmodels.c
//
qboolean ui_PModel_ValidModel( char *model_name );
void ui_RegisterPModelItem( char *name, char *model_name, char *skin_name, animgroup_t *animgroup, int numanims);
struct pmodelitem_s *ui_PModelItem_UpdateRegistration( char *name, char *model_name, char *skin_name );
void ui_AddPModelAnimation ( ui_pmodel_t *pmodel, int loweranim, int upperanim, int headanim, int channel);
void ui_DrawPModel( char *name, int originz, int originx, int originy, int angle );
void ui_DrawPModel_InWindow( char *name );
struct pmodelitem_s *ui_PModelItemFindByName( char *name );

#endif // SKELMOD


