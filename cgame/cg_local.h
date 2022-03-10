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
// cg_local.h -- local definitions for client game module

#include "../game/q_shared.h"
#include "../game/gs_public.h"
#include "ref.h"

#include "cg_public.h"
#include "cg_syscalls.h"
#include "cg_pmodels.h"

#define ITEM_RESPAWN_TIME	1000

typedef struct
{
	entity_state_t	current;
	entity_state_t	prev;			// will always be valid, but might just be a copy of current

	int			serverFrame;		// if not current, this ent isn't in the frame

	vec3_t		lerpOrigin;			// for trails (variable hz)

	int			fly_stoptime;

	int			respawnTime;
} centity_t;

typedef struct cgs_media_handle_s
{
	char *name;
	void *data;
	struct cgs_media_handle_s *next;
} cgs_media_handle_t;

#define NUM_CROSSHAIRS	10

#define STAT_MINUS		10	// num frame for '-' stats digit

typedef struct
{
	// sounds
	cgs_media_handle_t		*sfxRic[3];

	cgs_media_handle_t		*sfxLashit;

	cgs_media_handle_t		*sfxSpark5;
	cgs_media_handle_t		*sfxSpark6;
	cgs_media_handle_t		*sfxSpark7;

	cgs_media_handle_t		*sfxRailg;

	cgs_media_handle_t		*sfxRockexp;
	cgs_media_handle_t		*sfxGrenexp;
	cgs_media_handle_t		*sfxWatrexp;

	cgs_media_handle_t		*sfxLightning;
	cgs_media_handle_t		*sfxDisrexp;

	cgs_media_handle_t		*sfxLand;
	cgs_media_handle_t		*sfxItemRespawn;
	cgs_media_handle_t		*sfxTeleportIn;
	cgs_media_handle_t		*sfxTeleportOut;
	cgs_media_handle_t		*sfxJumpPad;

	cgs_media_handle_t		*sfxGrenBounce1;
	cgs_media_handle_t		*sfxGrenBounce2;

	cgs_media_handle_t		*sfxFootsteps[FOOTSTEP_TOTAL][4];

	cgs_media_handle_t		*sfxMachinegunSplashes[4];

	cgs_media_handle_t		*sfxBlasterSplash;
	cgs_media_handle_t		*sfxHyperblasterSplash;

	cgs_media_handle_t		*sfxShotgunSplash;
	cgs_media_handle_t		*sfxSuperShotgunSplash;

	cgs_media_handle_t		*sfxRocketLauncherSplash;
	cgs_media_handle_t		*sfxGrenadeLauncherSplash;

	cgs_media_handle_t		*sfxBFGSplash;

	cgs_media_handle_t		*sfxGibSound;

	cgs_media_handle_t		*sfxWeaponUp;
	cgs_media_handle_t		*sfxWeaponUpNoAmmo;

	// models
	cgs_media_handle_t		*modBulletExplode;
	cgs_media_handle_t		*modFlash;
	cgs_media_handle_t		*modParasiteSegment;
	cgs_media_handle_t		*modGrappleCable;
	cgs_media_handle_t		*modParasiteTip;
	cgs_media_handle_t		*modBfgExplo;
	cgs_media_handle_t		*modBfgBigExplo;
	cgs_media_handle_t		*modPowerScreen;
	cgs_media_handle_t		*modLightning;
	cgs_media_handle_t		*modMeatyGib;
	cgs_media_handle_t		*modTeleportEffect;
	cgs_media_handle_t		*modEjectBrassMachinegun;
	cgs_media_handle_t		*modEjectBrassShotgun;

	cgs_media_handle_t		*shaderParticle;
	cgs_media_handle_t		*shaderGrenadeExplosion;
	cgs_media_handle_t		*shaderRocketExplosion;
	cgs_media_handle_t		*shaderBulletExplosion;
	cgs_media_handle_t		*shaderPowerupQuad;
	cgs_media_handle_t		*shaderQuadWeapon;
	cgs_media_handle_t		*shaderPowerupPenta;
	cgs_media_handle_t		*shaderShellEffect;
	cgs_media_handle_t		*shaderWaterBubble;
	cgs_media_handle_t		*shaderTeleportEffect;
	cgs_media_handle_t		*shaderSmokePuff;
	cgs_media_handle_t		*shaderBulletMark;
	cgs_media_handle_t		*shaderExplosionMark;
	cgs_media_handle_t		*shaderEnergyMark;
	cgs_media_handle_t		*shaderLaser;
	cgs_media_handle_t		*shaderNet;
	cgs_media_handle_t		*shaderBackTile;
	cgs_media_handle_t		*shaderSelect;
	cgs_media_handle_t		*shaderPlasmaBall;

	cgs_media_handle_t		*sbNums[11];

	cgs_media_handle_t		*shaderCrosshair[11];
} cgs_media_t;


typedef struct
{
	char					name[MAX_QPATH];
	int						flags;
	int						parent;
} cgs_bone_t;

typedef struct cgs_skeleton_s
{
	struct model_s			*model;

	int						numBones;
	cgs_bone_t				*bones;

	int						numFrames;
	bonepose_t				**bonePoses;

	struct cgs_skeleton_s	*next;
} cgs_skeleton_t;

#include "cg_boneposes.h"

typedef struct cg_sexedSfx_s
{
	char *name;
	struct sfx_s *sfx;
	struct cg_sexedSfx_s *next;
} cg_sexedSfx_t;

typedef struct
{
	char	name[MAX_QPATH];
	char	cinfo[MAX_QPATH];
	int		gender;
	int		hand;

	struct	shader_s	*icon;

	cg_sexedSfx_t		*sexedSfx;
} cg_clientInfo_t;

// this is not exactly "static" but still...
typedef struct
{
	int					playerNum;
	qboolean			attractLoop;		// running cinematics and demos for the local system only
	unsigned int		serverFrameTime;	// msecs between server frames

	// shaders
	struct shader_s		*shaderWhite;
	struct shader_s		*shaderCharset;
	struct shader_s		*shaderPropfont;
	struct shader_s		*shaderLevelshot;
	struct shader_s		*shaderLevelshotDetail;

	cgs_media_t			media;

	int					vidWidth, vidHeight;

	int					gametype;

	//
	// locally derived information from server state
	//
	char				configStrings[MAX_CONFIGSTRINGS][MAX_QPATH];

	char				weaponModels[WEAP_TOTAL][MAX_QPATH];
	int					numWeaponModels;

	cg_clientInfo_t		clientInfo[MAX_CLIENTS];

	struct model_s		*modelDraw[MAX_MODELS];
	struct model_s		*inlineModelDraw[MAX_MODELS];

	struct pmodelinfo_s	*pModelsIndex[MAX_MODELS];
	struct pskin_s		*pSkinsIndex[MAX_IMAGES];

	struct pmodelinfo_s	*basePModelInfo;
	struct pskin_s		*basePSkin;

	struct sfx_s		*soundPrecache[MAX_SOUNDS];
	struct shader_s		*imagePrecache[MAX_IMAGES];
} cg_static_t;

typedef struct
{
	int					time;
	int					realTime;
	float				frameTime;
	int					frameCount;

	frame_t				frame, oldFrame;
	qboolean			frameSequenceRunning;
	qboolean			oldAreabits;
	qboolean			portalInView;

 	int					predictedOrigins[CMD_BACKUP][3];	// for debug comparing against server

	int					groundEntity;
	float				predictedStep;		// for stair up smoothing
	unsigned			predictedStepTime;

	vec3_t				predictedVelocity;
	vec3_t				predictedOrigin;	// generated by CG_PredictMovement
	vec3_t				predictedAngles;
	vec3_t				predictionError;

	vec3_t				autorotateAxis[3];

	float				lerpfrac;			// between oldframe and frame

	refdef_t			refdef;

	vec3_t				v_forward, v_right, v_up;	// set when refdef.angles is set
	vec3_t				lightingOrigin;		// store resulting lightingOrigin for player's entity here
											// (we reuse it for weapon model later)

	int					effects;
	qboolean			thirdPerson;

	int					chasedNum;

	//
	// all cyclic walking effects
	//
	float				xyspeed;

	float				oldBobTime;
	int					bobCycle;		// odd cycles are right foot going forward
	float				bobFracSin;		// sin(bobfrac*M_PI)
	
	//
	// transient data from server
	//
	char				layout[MAX_STRING_CHARS];	// general 2D overlay
	char				*statusBar;
	int					inventory[MAX_ITEMS];

	char				checkname[MAX_QPATH];
	char				loadingstring[MAX_QPATH];
} cg_state_t;

extern	cg_static_t	cgs;
extern	cg_state_t	cg;

extern	centity_t	cg_entities[MAX_EDICTS];

// the cg_parse_entities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
extern	entity_state_t	cg_parseEntities[MAX_PARSE_ENTITIES];

//
// cg_ents.c
//
extern	cvar_t	*cg_gun;
extern	cvar_t	*cg_timeDemo;

void CG_BeginFrameSequence( frame_t frame );
void CG_EndFrameSequence( int numEntities );
void CG_NewPacketEntityState( int entnum, entity_state_t state );
void CG_AddEntities( void );
void CG_GlobalSound( vec3_t origin, int entNum, int entChannel, int soundNum, float fvol, float attenuation );
void CG_GetEntitySoundOrigin( int entNum, vec3_t org );
void CG_CalcGunOffset( player_state_t *ps, player_state_t *ops, vec3_t angles );
void CG_AddViewWeapon( void );

//
// cg_draw.c
//
#define	FONT_BIG		1
#define FONT_SMALL		2
#define FONT_GIANT		4
#define FONT_SHADOWED	8

#define SMALL_CHAR_WIDTH	8
#define SMALL_CHAR_HEIGHT	16

#define BIG_CHAR_WIDTH		16
#define BIG_CHAR_HEIGHT		16

#define GIANT_CHAR_WIDTH	32
#define GIANT_CHAR_HEIGHT	48

#define PROP_CHAR_HEIGHT	27
#define PROP_SMALL_SCALE	0.75
#define PROP_BIG_SCALE		1
#define PROP_SMALL_SPACING	1.5
#define PROP_BIG_SPACING	1

#define PROP_SMALL_HEIGHT	PROP_CHAR_HEIGHT*PROP_SMALL_SCALE
#define PROP_BIG_HEIGHT		PROP_CHAR_HEIGHT*PROP_BIG_SCALE

void CG_DrawChar( int x, int y, int num, int fontstyle, vec4_t color );
void CG_DrawString( int x, int y, char *str, int fontstyle, vec4_t color );
void CG_DrawStringLen( int x, int y, char *str, int len, int fontstyle, vec4_t color );
void CG_DrawPropString( int x, int y, char *str, int fontstyle, vec4_t color );
int CG_PropStringLength( char *str, int fontstyle );
void CG_DrawCenteredPropString( int y, char *str, int fontstyle, vec4_t color );
void CG_FillRect( int x, int y, int w, int h, vec4_t color );
void CG_DrawHUDString( char *string, int x, int y, int centerwidth, int fontstyle, vec4_t color );
void CG_DrawHUDField( int x, int y, float *color, int width, int value );
void CG_DrawHUDField2( int x, int y, float *color, int width, int value );
void CG_DrawModel( int x, int y, int w, int h, struct model_s *model, struct shader_s *shader, vec3_t origin, vec3_t angles );
void CG_DrawHUDModel( int x, int y, int w, int h, struct model_s *model, struct shader_s *shader, float yawspeed );
void CG_DrawHUDRect( int x, int y, int w, int h, int val, int maxval, vec4_t color );

//
// cg_media.c
//
void CG_RegisterMediaSounds( void );
void CG_RegisterMediaModels( void );
void CG_RegisterMediaShaders( void );
void CG_RegisterLevelShot( void );

struct sfx_s *CG_MediaSfx( cgs_media_handle_t *mediasfx );
struct model_s *CG_MediaModel( cgs_media_handle_t *mediamodel );
struct shader_s *CG_MediaShader( cgs_media_handle_t *mediashader );

//
// cg_players.c
//
extern	cvar_t	*cg_noSkins;
extern	cvar_t	*cg_vwep;

extern	cvar_t	*skin;
extern	cvar_t	*hand;
extern	cvar_t	*gender;
extern	cvar_t	*gender_auto;

void CG_LoadClientInfo( cg_clientInfo_t *ci, char *s, int client );
void CG_SexedSound( int entnum, int entchannel, char *name, float fvol );

//
// cg_predict.c
//
extern	cvar_t	*cg_predict;
extern	cvar_t	*cg_showMiss;

void CG_PredictMovement( void );
void CG_CheckPredictionError( void );
void CG_BuildSolidList( void );
void CG_Trace( trace_t *t, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignore, int contentmask );
int	CG_PointContents( vec3_t point );

//
// cg_screen.c
//
typedef struct
{
	int		x, y, width, height;
} vrect_t;

extern vrect_t scr_vrect;

void SCR_Init( void );
void SCR_Shutdown( void );
void SCR_Draw2D( void );
void SCR_CalcVrect( void );
void SCR_TileClear( void );
void SCR_DrawLoading( void );
void SCR_CenterPrint( const char *str );

void CG_LoadLayout( char *s );
void CG_LoadStatusBar( char *s );

void CG_LoadingString( char *str );
void CG_LoadingFilename( char *str );

//
// cg_main.c
//
extern	cvar_t	*cg_paused;

#define CG_Malloc(size) trap_MemAlloc(size, __FILE__, __LINE__)
#define CG_Free(data) trap_MemFree(data, __FILE__, __LINE__)

int CG_API( void );
void CG_Init( int playerNum, qboolean attractLoop, unsigned int serverFrameTime, int vidWidth, int vidHeight );
void CG_Shutdown( void );
void CG_Printf( char *fmt, ... );
void CG_Error( char *fmt, ... );
char *_CG_CopyString( const char *in, const char *filename, int fileline );
#define CG_CopyString(in) _CG_CopyString(in,__FILE__,__LINE__)
void CG_FixUpGender( void );

//
// cg_svcmds.c
//
void CG_ServerCommand( void );

//
// cg_view.c
//
extern	cvar_t	*cg_testEntities;
extern	cvar_t	*cg_testLights;
extern	cvar_t	*cg_testBlend;

extern	cvar_t	*cg_outlineWorld;
extern	cvar_t	*cg_outlineModels;

extern	cvar_t	*cg_thirdPerson;
extern	cvar_t	*cg_thirdPersonAngle;
extern	cvar_t	*cg_thirdPersonRange;

void CG_RenderView( float frameTime, int realTime, float stereo_separation, qboolean forceRefresh );

//
// cg_lents.c
//
void CG_ClearLocalEntities( void );
void CG_AddBeams( void );
void CG_AddLocalEntities( void );

void CG_AddLaser( vec3_t start, vec3_t end, float radius, int colors, struct shader_s *shader );
void CG_BulletExplosion( vec3_t origin, vec3_t dir );
void CG_AddBeam( int ent, vec3_t start, vec3_t end, vec3_t offset, struct model_s *model );
void CG_AddLightning( int srcEnt, int destEnt, vec3_t start, vec3_t end, struct model_s *model );
void CG_BubbleTrail( vec3_t start, vec3_t end, int dist );
void CG_BlasterExplosion( vec3_t pos, vec3_t dir );
void CG_Explosion1( vec3_t pos );
void CG_Explosion2( vec3_t pos );
void CG_RocketExplosion( vec3_t pos, vec3_t dir );
void CG_RocketTrail( vec3_t start, vec3_t end );
void CG_GrenadeExplosion( vec3_t pos, vec3_t dir );
void CG_GrenadeTrail( vec3_t start, vec3_t end );
void CG_TeleportEffect( vec3_t org );
void CG_BFGLaser( vec3_t start, vec3_t end );
void CG_BFGExplosion( vec3_t pos );
void CG_BFGBigExplosion( vec3_t pos );
void CG_SmallPileOfGibs( vec3_t origin, int	count, vec3_t velocity );
void CG_EjectBrass( vec3_t origin, int	count, struct model_s *model );

//
// cg_decals.c
//
extern	cvar_t	*cg_addDecals;

void CG_ClearDecals( void );
void CG_SpawnDecal( vec3_t origin, vec3_t dir, float orient, float radius,
				 float r, float g, float b, float a, float die, float fadetime, qboolean fadealpha, struct shader_s *shader );
void CG_AddDecals( void );

//
// cg_effects.c
//
void CG_ClearEffects( void );

void CG_AllocDlight( float radius, const vec3_t origin, const vec3_t color );
void CG_AddDlights( void );

void CG_RunLightStyles( void );
void CG_SetLightStyle( int i );
void CG_AddLightStyles( void );

void CG_AddParticles( void );
void CG_ParticleEffect( vec3_t org, vec3_t dir, float r, float g, float b, int count );
void CG_ParticleEffect2( vec3_t org, vec3_t dir, float r, float g, float b, int count );
void CG_BigTeleportParticles( vec3_t org );
void CG_BlasterParticles( vec3_t org, vec3_t dir );
void CG_BlasterTrail( vec3_t start, vec3_t end );
void CG_FlagTrail( vec3_t start, vec3_t end, int effect );
void CG_BloodTrail( vec3_t start, vec3_t end );
void CG_RailTrail( vec3_t start, vec3_t end );
void CG_FlyEffect( centity_t *ent, vec3_t origin );
void CG_BfgParticles( vec3_t origin );
void CG_BFGExplosionParticles( vec3_t org );

//
// cg_events.c
//
extern	cvar_t	*cg_footSteps;

void CG_EntityEvent( entity_state_t *ent );

//=================================================

