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
// cg_pmodels.h -- local definitions for pmodels and view weapon
// by Jalisko

extern	cvar_t	*cg_weaponFlashes;
extern	cvar_t	*cg_ejectBrass;
extern	cvar_t	*cg_gunx;
extern	cvar_t	*cg_guny;
extern	cvar_t	*cg_gunz;
extern	cvar_t	*cg_debugPlayerModels;
extern	cvar_t	*cg_debugWeaponModels;
extern	cvar_t	*cg_bobSpeed;
extern	cvar_t	*cg_bobPitch;
extern	cvar_t	*cg_bobYaw;
extern	cvar_t	*cg_bobRoll;
extern	cvar_t	*cg_showLegs;

enum
{
	VWEAP_STANDBY = 1,
	VWEAP_ATTACK,
	VWEAP_ATTACK2_HOLD,
	VWEAP_ATTACK2_RELEASE,
	VWEAP_RELOAD,
	VWEAP_WEAPDOWN,
	VWEAP_WEAPONUP,
		
	VWEAP_MAXANIMS
};

// equivalent to pmodelinfo_t. Shared by different players, etc.
typedef struct weaponinfo_s
{
	char		name[MAX_QPATH];
	qboolean	inuse;

	struct	model_s	*model[WEAPMODEL_PARTS];	// one weapon consists of several models

	int			firstframe[VWEAP_MAXANIMS];		// animation script 
	int			lastframe[VWEAP_MAXANIMS];
	int			loopingframes[VWEAP_MAXANIMS];
	float		frametime[VWEAP_MAXANIMS];

	float		rotationscale;

	orientation_t	tag_projectionsource;
} weaponinfo_t;

extern weaponinfo_t	cg_pWeaponModelInfos[WEAP_TOTAL];

typedef struct
{
	int				flashtime;
	vec3_t			flashcolor;
	float			flashradius;
	vec3_t			angles;				// for barrel rotation
	float			rotationSpeed;

	weaponinfo_t	*weaponInfo;		// static information (models, animation script)
} pweapon_t;

enum
{
	BASIC_CHANNEL,	
	EVENT_CHANNEL,

	ANIMBUFFER_CHANNELS
};

typedef struct
{
	int		newanim[PMODEL_PARTS];
} animationbuffer_t;

typedef struct
{
	int		current[PMODEL_PARTS];			// running animation
	int		currentChannel[PMODEL_PARTS];	// what channel it came from

	int		frame[PMODEL_PARTS];
	int		oldframe[PMODEL_PARTS];
#ifdef SKELMOD
	float	prevframetime;	//interpolation timing stuff
	float	nextframetime;
	float	backlerp;
#else
	float	prevframetime[PMODEL_PARTS];	// interpolation timing stuff
	float	nextframetime[PMODEL_PARTS];
	float	backlerp[PMODEL_PARTS];
#endif
	animationbuffer_t	buffer[ANIMBUFFER_CHANNELS];
} animationinfo_t;

#ifdef SKELMOD
typedef struct cg_tagmask_s
{
	char	tagname[64];
	char	bonename[64];
	int		bonenum;
	struct cg_tagmask_s *next;
}cg_tagmask_t;
#endif

#define SKM_MAX_BONES 256
// pmodelinfo_t is the playermodel structure as originally readed
// consider it static 'read-only', cause it is shared by different players
typedef struct pmodelinfo_s
{
	char				model_name[MAX_QPATH];
	qboolean			inuse;
	int					sex;
#ifdef SKELMOD
	struct	model_s		*model;
	int					numRotators[PMODEL_PARTS];
	int					rotator[PMODEL_PARTS][16];
	int					boneAnims[SKM_MAX_BONES];
	cg_tagmask_t		*tagmasks;
#else
	struct	model_s		*model[PMODEL_PARTS];
#endif
	int		firstframe[PMODEL_MAX_ANIMS];
	int		lastframe[PMODEL_MAX_ANIMS];
	int		loopingframes[PMODEL_MAX_ANIMS];
#ifdef SKELMOD
	float	frametime;
#else
	float	frametime[PMODEL_MAX_ANIMS];
#endif
} pmodelinfo_t;

#define CG_MAX_PMODELS	256
extern pmodelinfo_t	cg_PModelInfos[CG_MAX_PMODELS];


typedef struct pskin_s
{
	qboolean			inuse;
	char				name[MAX_QPATH];
	struct	shader_s	*icon;
#ifdef SKELMOD
	struct	skinfile_s	*skin;
#else
	struct	skinfile_s	*skin[PMODEL_PARTS];
#endif
} pskin_t;

#define CG_MAX_PSKINS	256
extern pskin_t	cg_pSkins[CG_MAX_PSKINS];


typedef struct
{
	pskin_t				*pSkin;
	pmodelinfo_t		*pmodelinfo;				
#ifdef SKELMOD
	int					number;
	struct cgs_skeleton_s	*skel;
	bonepose_t			*curboneposes;
	bonepose_t			*oldboneposes;
#else
	entity_t			ents[PMODEL_PARTS];			// entities to be added
#endif
	animationinfo_t		anim;						// animation state
	pweapon_t			pweapon;					// active weapon state

	vec3_t				angles[PMODEL_PARTS];		// for rotations
	vec3_t				oldangles[PMODEL_PARTS];

	orientation_t		projectionSource;
	weaponinfo_t		*weaponIndex[WEAP_TOTAL];	// weaponmodels pointer indexes
} pmodel_t;

extern pmodel_t		cg_entPModels[MAX_EDICTS];		// a pmodel handle for each cg_entity

//
// cg_pmodels.c
//

// utils
void CG_AddShellEffects( entity_t *ent, int effects );
void CG_AddColorShell( entity_t *ent, int renderfx );
qboolean CG_GrabTag( orientation_t *tag, entity_t *ent, const char *tagname );
void CG_PlaceModelOnTag( entity_t *ent, entity_t *dest, orientation_t *tag );
void CG_PlaceRotatedModelOnTag( entity_t *ent, entity_t *dest, orientation_t *tag );
void CG_MoveToTag( vec3_t move_origin, vec3_t move_axis[3], vec3_t dest_origin, vec3_t dest_axis[3], vec3_t tag_origin, vec3_t tag_axis[3] );

// pmodels
void CG_PModelsInit( void );
void CG_RegisterBasePModel( void );
struct pskin_s *CG_RegisterPSkin ( char *filename );
struct pmodelinfo_s *CG_RegisterPModel( char *filename );
void CG_LoadClientPmodel( int cenum, char *model_name, char *skin_name );
void CG_AddPModel( entity_t *ent, entity_state_t *state );
void CG_AddAnimationFromState( entity_state_t *state, int loweranim, int upperanim, int headanim, int channel );
void CG_PModels_AddFireEffects( entity_state_t *state );
void CG_ClearEventAnimations( entity_state_t *state );
void CG_PModelsUpdateStates( void );
qboolean CG_PModel_CalcProjectionSource( int entnum, orientation_t *tag_result );
void CG_PModels_ResetBarrelRotation( entity_state_t *state );

//
// cg_wmodels.c
//
struct weaponinfo_s *CG_CreateWeaponZeroModel( char *cgs_name );
struct weaponinfo_s *CG_RegisterWeaponModel( char *cgs_name );
void CG_AddWeaponOnTag( entity_t *ent, orientation_t *tag, pweapon_t *pweapon, int effects, orientation_t *projectionSource );
struct weaponinfo_s *CG_GetWeaponFromPModelIndex( pmodel_t *pmodel, int currentweapon );



//	VIEW WEAPON

enum
{
	CAM_INEYES,
	CAM_THIRDPERSON,

	CAM_MODES
};
	
typedef struct
{
	int mode;
	int	cmd_mode_delay;
} cg_chasecam_t;

cg_chasecam_t	chaseCam;

typedef struct
{
	qboolean		active;

	pweapon_t		pweapon;			// weapon state
	weaponinfo_t	*newWeaponInfo;		// player active weapon latched for weapon change

	entity_state_t	*state;				// player's cent->current

	int				currentAnim;
	int				newAnim;
	int				frame;
	int				oldframe;

	float			nextframetime;
	float			prevframetime;
	float			backlerp;

	player_state_t	*ops;				// old player state, just for not having to figure it out again

	int				fallEff_Time;		// fallKickEffect
	int				fallEff_rebTime;
	float			fallKick;

	orientation_t	projectionSource;

	int				isOnGround;			// only needed qboolean, but still...
	int				isSwim;

} viewweaponinfo_t;

extern viewweaponinfo_t	vweap;

//
// cg_vweap.c
//

void CG_ChaseHack( void );
void CG_vWeapUpdateState( void );
void CG_CalcViewOnGround( void );


