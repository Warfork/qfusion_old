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

#include "../qcommon/qcommon.h"
#include "../ref_gl/render.h"
#include "../cgame/cg_public.h"

#include "cin.h"
#include "vid.h"
#include "sound.h"
#include "input.h"
#include "keys.h"
#include "console.h"

//=============================================================================

//
// the client_state_t structure is wiped completely at every
// server map change
//
typedef struct
{
	int			timeoutcount;

	int			timedemo_frames;
	int			timedemo_start;

	qboolean	soundPrepped;		// ambient sounds can start

	int			parse_entities;		// index (not anded off) into cl_parse_entities[]

	int			cmdNum;				// current cmd
	usercmd_t	cmds[CMD_BACKUP];	// each mesage will send several old cmds
	int			cmd_time[CMD_BACKUP];	// time sent, for calculating pings

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

	char		servermessage[MAX_QPATH];
	char		configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];
} client_state_t;

extern	client_state_t	cl;

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/

typedef struct
{
	connstate_t	state;				// only set through CL_SetClientState
	keydest_t	key_dest;
	keydest_t	old_key_dest;

	int			framecount;
	int			realtime;			// always increasing, no clamping, etc
	float		trueframetime;
	float		frametime;			// seconds since last frame

// screen rendering information
	qboolean	cgameActive;
	qboolean	mediaInitialized;

	int			disable_screen;		// showing loading plaque between levels
									// or changing rendering dlls
									// if time gets > 30 seconds ahead, break it
	int			disable_servercount;	// when we receive a frame and cl.servercount
									// > cls.disable_servercount, clear disable_screen

// connection information
	char		servername[MAX_OSPATH];	// name of server from original connect
	float		connect_time;		// for connection retransmits
	int			connect_count;

	int			quakePort;			// a 16 bit value that allows quake servers
									// to work around address translating routers
	netchan_t	netchan;

	char		*statusbar;

	int			challenge;			// from the server to use for connecting

	FILE		*download;			// file transfer from server
	char		downloadtempname[MAX_OSPATH];
	char		downloadname[MAX_OSPATH];
	int			downloadnumber;
	int			downloadpercent;

// demo recording info must be here, so it isn't cleared on level change
	qboolean	demorecording;
	qboolean	demowaiting;	// don't record until a non-delta message is received
	FILE		*demofile;

	// these shaders have nothing to do with media
	struct shader_s *whiteShader;
	struct shader_s *charsetShader;
	struct shader_s *consoleShader;
} client_static_t;

extern client_static_t	cls;

//=============================================================================

//
// cvars
//
extern	cvar_t	*cl_stereo_separation;
extern	cvar_t	*cl_stereo;

extern	cvar_t	*cl_upspeed;
extern	cvar_t	*cl_forwardspeed;
extern	cvar_t	*cl_sidespeed;

extern	cvar_t	*cl_yawspeed;
extern	cvar_t	*cl_pitchspeed;

extern	cvar_t	*cl_run;

extern	cvar_t	*cl_anglespeedkey;

extern	cvar_t	*cl_shownet;

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

extern	cvar_t	*cl_masterServer;

// delta from this if not from a previous frame
extern	entity_state_t	cl_baselines[MAX_EDICTS];

// the cl_parse_entities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
extern	entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];


//=============================================================================

extern	netadr_t	net_from;
extern	sizebuf_t	net_message;

//=============================================================================


//
// cl_cin.c
//
void SCR_PlayCinematic (char *name);
qboolean SCR_DrawCinematic (void);
void SCR_RunCinematic (void);
void SCR_StopCinematic (void);
void SCR_FinishCinematic (void);

//
// cl_main.c
//
void CL_Init (void);
void CL_Quit (void);

void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_PingServers_f (void);
void CL_RequestNextDownload (void);
void CL_GetClipboardData (char *string, int size);
void CL_SetKeyDest (int key_dest);
void CL_SetOldKeyDest (int key_dest);
void CL_ResetServerCount (void);
void CL_SetClientState (int state);
void CL_ClearState (void);
void CL_ReadPackets (void);

//
// cl_ents.c
//
int CL_ParseEntityBits (unsigned *bits);
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, unsigned bits);
void CL_ParseFrame (void);

//
// cl_game.c
//
void CL_GameModule_Init (void);
void CL_GameModule_Shutdown (void);
void CL_GameModule_ServerCommand (void);
void CL_GameModule_LoadLayout ( char *s );
void CL_GameModule_RenderView ( float stereo_separation );
void CL_GameModule_BeginFrameSequence (void);
void CL_GameModule_NewPacketEntityState ( int entnum, entity_state_t *state );
void CL_GameModule_EndFrameSequence (void);
void CL_GameModule_GetEntitySoundOrigin ( int entnum, vec3_t origin );
void CL_GameModule_Trace ( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask );

//
// cl_ui.c
//
void CL_UIModule_Init (void);
void CL_UIModule_Shutdown (void);
void CL_UIModule_Keydown ( int key );
void CL_UIModule_Refresh ( qboolean backGround );
void CL_UIModule_DrawConnectScreen ( qboolean backGround );
void CL_UIModule_MenuMain (void);
void CL_UIModule_ForceMenuOff (void);
void CL_UIModule_AddToServerList ( char *adr, char *info );
void CL_UIModule_MouseMove ( int dx, int dy );

//
// cl_input.c
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
void CL_BaseMove (usercmd_t *cmd);

void IN_CenterView (void);

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
void SHOWNET(char *s);
void CL_Download_f (void);
qboolean CL_CheckOrDownloadFile (char *filename);

//
// cl_screen.c
//
#define SMALL_CHAR_WIDTH	8
#define SMALL_CHAR_HEIGHT	16

#define BIG_CHAR_WIDTH		16
#define BIG_CHAR_HEIGHT		16

#define GIANT_CHAR_WIDTH	32
#define GIANT_CHAR_HEIGHT	48

void SCR_Init (void);
void SCR_UpdateScreen (void);
void SCR_BeginLoadingPlaque (void);
void SCR_EndLoadingPlaque (void);
void SCR_DebugGraph (float value, float r, float g, float b);
void SCR_RunConsole (void);
void SCR_RegisterConsoleMedia (void);

void CL_InitMedia( void );
void CL_ShutdownMedia( void );
void CL_RestartMedia( void );

void CL_AddNetgraph (void);

extern	float	scr_con_current;
extern	float	scr_conlines;		// lines of console to display

void Draw_Char ( int x, int y, int num, vec4_t color );
void Draw_String ( int x, int y, char *str, vec4_t color );
void Draw_StringLen ( int x, int y, char *str, int len, vec4_t color );
void Draw_FillRect ( int x, int y, int w, int h, vec4_t color );
