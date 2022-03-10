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
// client.h -- primary header for client

//define	PARANOID			// speed sapping error checking

#include "../qcommon/qcommon.h"

#include "ref.h"
#include "cin.h"

#include "../ref_gl/render.h"

#include "vid.h"
#include "screen.h"
#include "sound.h"
#include "input.h"
#include "keys.h"
#include "console.h"

//=============================================================================

typedef struct
{
	qboolean		valid;			// cleared if delta parsing was invalid
	int				serverframe;
	int				servertime;		// server time the message is valid for (in msec)
	int				deltaframe;
	byte			areabits[MAX_MAP_AREAS/8];		// portalarea visibility bits
	player_state_t	playerstate;
	int				num_entities;
	int				parse_entities;	// non-masked index into cl_parse_entities array
} frame_t;

#define ITEM_RESPAWN_TIME	1000

typedef struct
{
	entity_state_t	baseline;		// delta from this if not from a previous frame
	entity_state_t	current;
	entity_state_t	prev;			// will always be valid, but might just be a copy of current

	int			serverframe;		// if not current, this ent isn't in the frame

	int			trailcount;			// for diminishing grenade trails
	vec3_t		lerp_origin;		// for trails (variable hz)

	int			fly_stoptime;

	int			respawnTime;
} centity_t;

#define MAX_CLIENTWEAPONMODELS		20

typedef struct
{
	char	name[MAX_QPATH];
	char	cinfo[MAX_QPATH];
	int		gender;
	struct shader_s	*skin;
	struct shader_s	*icon;
	char	iconname[MAX_QPATH];
	struct model_s	*model;
	struct model_s	*weaponmodel[MAX_CLIENTWEAPONMODELS];
} clientinfo_t;

extern char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
extern int num_cl_weaponmodels;

#define	CMD_BACKUP		64	// allow a lot of command backups for very fast systems

typedef enum {
	FOOTSTEP_NORMAL,
	FOOTSTEP_BOOT,
	FOOTSTEP_FLESH,
	FOOTSTEP_MECH,
	FOOTSTEP_ENERGY,
	FOOTSTEP_METAL,
	FOOTSTEP_SPLASH,

	FOOTSTEP_TOTAL
} footstep_t;

typedef struct
{
	// sounds
	struct	sfx_s		*sfxRic1;
	struct	sfx_s		*sfxRic2;
	struct	sfx_s		*sfxRic3;

	struct	sfx_s		*sfxLashit;

	struct	sfx_s		*sfxSpark5;
	struct	sfx_s		*sfxSpark6;
	struct	sfx_s		*sfxSpark7;

	struct	sfx_s		*sfxRailg;

	struct	sfx_s		*sfxRockexp;
	struct	sfx_s		*sfxGrenexp;
	struct	sfx_s		*sfxWatrexp;

	struct	sfx_s		*sfxLightning;
	struct	sfx_s		*sfxDisrexp;

	struct	sfx_s		*sfxLand;
	struct	sfx_s		*sfxItemRespawn;
	struct	sfx_s		*sfxTeleportIn;
	struct	sfx_s		*sfxTeleportOut;
	struct	sfx_s		*sfxJumpPad;

	struct	sfx_s		*sfxGrenBounce1;
	struct	sfx_s		*sfxGrenBounce2;

	struct	sfx_s		*sfxMachinegunSplashes[4];
	struct	sfx_s		*sfxFootsteps[FOOTSTEP_TOTAL][4];

	struct	sfx_s		*sfxHyperblasterSplash;

	struct	sfx_s		*sfxGibSound;

	// models
	struct	model_s		*modExplode;
	struct	model_s		*modSmoke;
	struct	model_s		*modFlash;
	struct	model_s		*modParasiteSegment;
	struct	model_s		*modGrappleCable;
	struct	model_s		*modParasiteTip;
	struct	model_s		*modExplo4;
	struct	model_s		*modBfgExplo;
	struct	model_s		*modPowerScreen;
	struct	model_s		*modLightning;
	struct	model_s		*modMeatyGib;
	struct	model_s		*modTeleportEffect;
	
	// shaders
	struct	shader_s	*shaderGrenadeExplosion;
	struct	shader_s	*shaderRocketExplosion;
	struct	shader_s	*shaderPowerupQuad;
	struct	shader_s	*shaderQuadWeapon;
	struct	shader_s	*shaderPowerupPenta;
	struct	shader_s	*shaderShellEffect;
	struct	shader_s	*shaderWaterBubble;
	struct	shader_s	*shaderTeleportEffect;
	struct	shader_s	*shaderSmokePuff;
	struct	shader_s	*shaderBulletMark;
	struct	shader_s	*shaderExplosionMark;
	struct	shader_s	*shaderEnergyMark;
} client_media_t;

//
// the client_state_t structure is wiped completely at every
// server map change
//
typedef struct
{
	int			timeoutcount;

	int			timedemo_frames;
	int			timedemo_start;

	qboolean	refresh_prepped;	// false if on new level or new ref dll
	qboolean	sound_prepped;		// ambient sounds can start
	qboolean	force_refdef;		// vid has changed, so we can't use a paused refdef

	int			parse_entities;		// index (not anded off) into cl_parse_entities[]

	usercmd_t	cmd;
	usercmd_t	cmds[CMD_BACKUP];	// each mesage will send several old cmds
	int			cmd_time[CMD_BACKUP];	// time sent, for calculating pings
	int			predicted_origins[CMD_BACKUP][3];	// for debug comparing against server

	float		predicted_step;				// for stair up smoothing
	unsigned	predicted_step_time;

	vec3_t		predicted_origin;	// generated by CL_PredictMovement
	vec3_t		predicted_angles;
	vec3_t		prediction_error;

	frame_t		frame;				// received from server
	int			suppressCount;		// number of messages rate suppressed
	frame_t		frames[UPDATE_BACKUP];

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t		viewangles;

	int			time;			// this is the time value that the client
								// is rendering at.  always <= cls.realtime
	float		lerpfrac;		// between oldframe and frame

	refdef_t	refdef;

	vec3_t		v_forward, v_right, v_up;	// set when refdef.angles is set

	//
	// transient data from server
	//
	char		layout[1024];		// general 2D overlay
	int			inventory[MAX_ITEMS];

	//
	// non-gameserver infornamtion
	cinematics_t cin;

	//
	// server state information
	//
	qboolean	attractloop;		// running the attract loop, any key will menu
	int			servercount;	// server identification for prespawns
	char		gamedir[MAX_QPATH];
	int			playernum;

	char		configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];
	char		*statusbar;

	char		levelshot[MAX_QPATH];
	char		checkname[MAX_QPATH];
	char		loadingstring[32];
	int			effects;
	qboolean	thirdperson;

	//
	// locally derived information from server state
	//
	struct model_s	*model_draw[MAX_MODELS];
	struct cmodel_s	*model_clip[MAX_MODELS];

	struct sfx_s	*music_precache;

	struct sfx_s	*sound_precache[MAX_SOUNDS];
	struct shader_s	*image_precache[MAX_IMAGES];

	clientinfo_t	clientinfo[MAX_CLIENTS];
	clientinfo_t	baseclientinfo;

	client_media_t	media;
} client_state_t;

extern	client_state_t	cl;

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/

typedef enum {
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_single
} dltype_t;		// download type

typedef struct
{
	connstate_t	state;				// only set through CL_SetClientState
	keydest_t	key_dest;

	int			framecount;
	int			realtime;			// always increasing, no clamping, etc
	float		frametime;			// seconds since last frame

// screen rendering information
	float		disable_screen;		// showing loading plaque between levels
									// or changing rendering dlls
									// if time gets > 30 seconds ahead, break it
	int			disable_servercount;	// when we receive a frame and cl.servercount
									// > cls.disable_servercount, clear disable_screen

// connection information
	char		servername[MAX_OSPATH];	// name of server from original connect
	float		connect_time;		// for connection retransmits

	int			quakePort;			// a 16 bit value that allows quake servers
									// to work around address translating routers
	netchan_t	netchan;

	int			challenge;			// from the server to use for connecting

	FILE		*download;			// file transfer from server
	char		downloadtempname[MAX_OSPATH];
	char		downloadname[MAX_OSPATH];
	int			downloadnumber;
	dltype_t	downloadtype;
	int			downloadpercent;

// demo recording info must be here, so it isn't cleared on level change
	qboolean	demorecording;
	qboolean	demowaiting;	// don't record until a non-delta message is received
	FILE		*demofile;
} client_static_t;

extern client_static_t	cls;

//=============================================================================

//
// cvars
//
extern	cvar_t	*cl_stereo_separation;
extern	cvar_t	*cl_stereo;

extern	cvar_t	*cl_gun;
extern	cvar_t	*cl_add_blend;
extern	cvar_t	*cl_add_lights;
extern	cvar_t	*cl_add_particles;
extern	cvar_t	*cl_add_entities;
extern	cvar_t	*cl_add_polys;
extern	cvar_t	*cl_add_decals;
extern	cvar_t	*cl_predict;
extern	cvar_t	*cl_footsteps;
extern	cvar_t	*cl_noskins;
extern	cvar_t	*cl_autoskins;

extern	cvar_t	*cl_upspeed;
extern	cvar_t	*cl_forwardspeed;
extern	cvar_t	*cl_sidespeed;

extern	cvar_t	*cl_yawspeed;
extern	cvar_t	*cl_pitchspeed;

extern	cvar_t	*cl_run;

extern	cvar_t	*cl_anglespeedkey;

extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_showmiss;
extern	cvar_t	*cl_showclamp;

extern	cvar_t	*lookspring;
extern	cvar_t	*lookstrafe;
extern	cvar_t	*sensitivity;

extern	cvar_t	*m_pitch;
extern	cvar_t	*m_yaw;
extern	cvar_t	*m_forward;
extern	cvar_t	*m_side;

extern	cvar_t	*cl_freelook;

extern	cvar_t	*cl_paused;
extern	cvar_t	*cl_timedemo;
extern	cvar_t	*cl_avidemo;

extern	cvar_t	*cl_vwep;

extern	cvar_t	*cl_thirdPerson;
extern	cvar_t	*cl_thirdPersonAngle;
extern	cvar_t	*cl_thirdPersonRange;

typedef struct
{
	int		key;				// so entities can reuse same entry
	vec3_t	color;
	vec3_t	origin;
	float	radius;
	int		die;				// stop lighting after this time
} cdlight_t;

extern	centity_t	cl_entities[MAX_EDICTS];
extern	cdlight_t	cl_dlights[MAX_DLIGHTS];

// the cl_parse_entities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
#define	MAX_PARSE_ENTITIES	1024
extern	entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];


//=============================================================================

extern	netadr_t	net_from;
extern	sizebuf_t	net_message;

qboolean	CL_CheckOrDownloadFile (char *filename);

void CL_AddNetgraph (void);

void CL_ParticleEffect (vec3_t org, vec3_t dir, float r, float g, float b, int count);
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, float r, float g, float b, int count);

//=================================================

typedef struct particle_s
{
	float		time;

	vec3_t		org;
	vec3_t		vel;
	vec3_t		accel;
	vec3_t		color;
	float		alpha;
	float		alphavel;
	float		scale;
} cparticle_t;


#define	PARTICLE_GRAVITY	40

void CL_ClearLocalEntities (void);
void CL_ClearEffects (void);
void CL_ClearDecals (void);

void CL_TeleportEffect ( vec3_t org );

void CL_AddLaser ( vec3_t start, vec3_t end, int colors );
void CL_AddBeam (int ent, vec3_t start, vec3_t end, vec3_t offset, struct model_s *model );
void CL_AddLightning (int srcEnt, int destEnt, vec3_t start, vec3_t end, struct model_s *model);

int CL_ParseEntityBits (unsigned *bits);
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, unsigned bits);
void CL_ParseFrame (void);

void CL_ParseTEnt (void);
void CL_ParseConfigString (void);
void CL_ParseMuzzleFlash (void);
void CL_ParseMuzzleFlash2 (void);

void CL_AddBeams (void);
void CL_AddLocalEntities (void);
void CL_AddDLights (void);

void CL_AddEntities (void);

//=================================================


//
// cl_main
//
void CL_Init (void);
void CL_Quit_f (void);

void CL_Spawn ( void );
void CL_FixUpGender(void);
void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_GetChallengePacket (void);
void CL_PingServers_f (void);
void CL_Snd_Restart_f (void);
void CL_PlayBackgroundMusic (void); 
void CL_RequestNextDownload (void);
void CL_SetClientState (int state);

//
// cl_input
//
typedef struct
{
	int			down[2];		// key nums holding it down
	unsigned	downtime;		// msec timestamp
	unsigned	msec;			// msec down this frame
	int			state;
} kbutton_t;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;

void CL_InitInput (void);
void CL_SendCmd (void);
void CL_SendMove (usercmd_t *cmd);

void CL_ClearState (void);

void CL_ReadPackets (void);

int  CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd);

void IN_CenterView (void);

float CL_KeyState (kbutton_t *key);
char *Key_KeynumToString (int keynum);

//
// cl_demo.c
//
void CL_WriteDemoMessage (void);
void CL_Stop_f (void);
void CL_Record_f (void);

//
// cl_parse.c
//
extern	char *svc_strings[256];

void CL_ParseServerMessage (void);
void CL_LoadClientinfo (clientinfo_t *ci, char *s);
void SHOWNET(char *s);
void CL_ParseClientinfo (int player);
void CL_Download_f (void);
void CL_RegisterSounds (void);

//
// cl_view.c
//
void V_Init (void);
void V_RenderView( float stereo_separation );
void V_AddEntity (entity_t *ent);
void V_AddParticle (vec3_t org, float r, float g, float b, float alpha, float scale);
void V_AddLight (vec3_t org, float intensity, float r, float g, float b);
void V_AddPoly (poly_t *poly);

void CL_PrepRefresh (void);

//
// cl_tent.c
//

void CL_RegisterMediaSounds (void);
void CL_RegisterMediaModels (void);
void CL_RegisterMediaShaders (void);
void CL_SmokeAndFlash(vec3_t origin);

//
// cl_decals.c
//
#define MAX_DECALS				256

typedef struct
{
	int			start, die;				// stop lighting after this time
	int			fadetime;
	float		fadefreq;
	qboolean	fadealpha;
	float		color[4];
	struct shader_s *shader;

	poly_t			poly;
	vec3_t			verts[MAX_POLY_VERTS];
	vec2_t			stcoords[MAX_POLY_VERTS];
	byte_vec4_t		colors[MAX_POLY_VERTS];
} cdecal_t;

void CL_SpawnDecal ( vec3_t origin, vec3_t dir, float orient, float radius,
				 float r, float g, float b, float a, float die, float fadetime, qboolean fadealpha, struct shader_s *shader );
void CL_AddDecals (void);

//
// cl_pred.c
//
void CL_InitPrediction (void);
void CL_PredictMovement (void);
void CL_CheckPredictionError (void);

#define IGNORE_NOTHING	-1
#define IGNORE_WORLD	0
#define IGNORE_PLAYER	cl.playernum+1

trace_t CL_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignore, int contentmask);

//
// cl_fx.c
//
cdlight_t *CL_AllocDlight (int key);

void CL_RailTrail (vec3_t start, vec3_t end);
void CL_FlagTrail (vec3_t start, vec3_t end, int effect);
void CL_BubbleTrail (vec3_t start, vec3_t end, int dist);
void CL_BlasterTrail (vec3_t start, vec3_t end);
void CL_BloodTrail (vec3_t start, vec3_t end, int *trailcount);
void CL_RocketTrail (vec3_t start, vec3_t end);
void CL_GrenadeTrail (vec3_t start, vec3_t end);

void CL_DiminishingTrail (vec3_t start, vec3_t end, int *trailcount, int flags);

void CL_BigTeleportParticles (vec3_t org);
void CL_FlyEffect (centity_t *ent, vec3_t origin);
void CL_BfgParticles (entity_t *ent);
void CL_AddParticles (void);
void CL_EntityEvent (entity_state_t *ent);

//
// user interface
//
void UI_Init (void);
void UI_Shutdown (void);
void UI_Keydown (int key);
void UI_Refresh (int frametime);
void UI_Update (void);
void UI_Menu_Main_f (void);
void UI_ForceMenuOff (void);
void UI_AddToServerList (char *adr, char *info);
void UI_MouseMove (int dx, int dy);

//
// cl_inv.c
//
void CL_ParseInventory (void);
void CL_DrawInventory (void);

//
// cl_scrn.c
void CL_LoadingString (char *str);
void CL_LoadingFilename (char *str);

