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

// cg_public.h -- client game dll information visible to engine

#define	CGAME_API_VERSION	1

//
// structs and variables shared with the main engine
//

#define	CMD_BACKUP		64	// allow a lot of command backups for very fast systems
#define CMD_MASK		(CMD_BACKUP-1)

#define	MAX_PARSE_ENTITIES	1024

typedef struct
{
	qboolean		valid;			// cleared if delta parsing was invalid
	int				serverFrame;
	int				serverTime;		// server time the message is valid for (in msec)
	int				deltaFrame;
	qbyte			areabits[MAX_CM_AREAS/8];		// portalarea visibility bits
	player_state_t	playerState;
	int				numEntities;
	int				parseEntities;	// non-masked index into cg_parse_entities array
} frame_t;

//===============================================================

//
// functions provided by the main engine
//
typedef struct
{
	// drops to console a client game error
	void			(*Error) ( char *msg );

	// console messages
	void			(*Print) ( char *msg );

	// console variable interaction
	cvar_t			*(*Cvar_Get) ( char *name, char *value, int flags );
	cvar_t			*(*Cvar_Set) ( char *name, char *value );
	void			(*Cvar_SetValue)( char *name, float value );
	cvar_t			*(*Cvar_ForceSet) ( char *name, char *value );	// will return 0 0 if not found
	float			(*Cvar_VariableValue) ( char *name );
	char			*(*Cvar_VariableString) ( char *name );

	int				(*Cmd_Argc) (void);
	char			*(*Cmd_Argv) ( int arg );
	char			*(*Cmd_Args) (void);	// concatenation of all argv >= 1

	void			(*Cmd_AddCommand) ( char *name, void(*cmd)(void) );
	void			(*Cmd_RemoveCommand) ( char *cmd_name );
	void			(*Cmd_ExecuteText) ( int exec_when, char *text );
	void			(*Cmd_Execute) (void);

	// files will be memory mapped read only
	// the returned buffer may be part of a larger pak file,
	// or a discrete file from anywhere in the quake search path
	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int				(*FS_LoadFile) ( const char *name, void **buf );
	void			(*FS_FreeFile) ( void *buf );
	int				(*FS_FileExists) ( const char *path );
	int				(*FS_ListFiles) ( const char *path, const char *ext, char *buf, int bufsize );

	// gamedir will be the current directory that generated
	// files should be stored to, ie: "f:\quake\id1"
	char			*(*FS_Gamedir) (void);

	// key bindings
	char			*(*Key_GetBindingBuf)( int binding );
	char			*(*Key_KeynumToString) ( int keynum );

	void			(*GetConfigString) ( int i, char *str, int size );

	void			(*NET_GetUserCommand) ( int frame, usercmd_t *cmd );
	void			(*NET_GetCurrentUserCommand) ( usercmd_t *cmd );
	void			(*NET_GetCurrentState) ( int *incomingAcknowledged, int *outgoingSequence );

	// refresh system
	void			(*R_UpdateScreen) (void);
	int				(*R_GetClippedFragments) ( vec3_t origin, float radius, mat3_t axis, int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments );
	void			(*R_RenderFrame) ( refdef_t *fd );
	void			(*R_RegisterWorldModel) ( char *name );
	void			(*R_ModelBounds) ( struct model_s *mod, vec3_t mins, vec3_t maxs );
	struct model_s		*(*R_RegisterModel) ( char *name );
	struct shader_s 	*(*R_RegisterPic) ( char *name );
	struct shader_s 	*(*R_RegisterSkin) ( char *name );
	struct skinfile_s 	*(*R_RegisterSkinFile) ( char *name );
	qboolean		(*R_LerpAttachment) ( orientation_t *orient, struct model_s *mod, int frame, int oldframe, float backlerp, char *name );

	// 2D drawing routines
	void			(*Draw_StretchPic) ( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader );

	// collision detection
	int				(*CM_NumInlineModels) (void);
	struct cmodel_s	*(*CM_InlineModel) ( int num );
	struct cmodel_s	*(*CM_ModelForBBox) ( vec3_t mins, vec3_t maxs );
	void			(*CM_BoxTrace) ( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask );
	void			(*CM_TransformedBoxTrace) ( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles );
	int				(*CM_PointContents) ( vec3_t p, struct cmodel_s *cmodel );
	int				(*CM_TransformedPointContents) ( vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles );
	void			(*CM_InlineModelBounds) ( struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs );

	// sound system
	struct sfx_s	*(*S_RegisterSound) ( char *name );	
	void			(*S_StartSound) ( vec3_t origin, int entnum, int entchannel, struct sfx_s *sfx, float fvol, float attenuation, float timeofs );
	void			(*S_Update) ( vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up );
	void			(*S_AddLoopSound) ( struct sfx_s *sfx, vec3_t origin );

	// managed memory allocation
	struct mempool_s 	*(*Mem_AllocPool) ( const char *name, const char *filename, int fileline );
	
	void			*(*Mem_Alloc) ( struct mempool_s *pool, int size, const char *filename, int fileline );
	void			(*Mem_Free) ( void *data, const char *filename, int fileline );
	void			(*Mem_FreePool) ( struct mempool_s **pool, const char *filename, int fileline );
	void			(*Mem_EmptyPool) ( struct mempool_s *pool, const char *filename, int fileline );
} cgame_import_t;

//
// functions exported by the client game subsystem
//
typedef struct
{
	// if API is different, the dll cannot be used
	int				(*API) (void);

	// the init function will be called at each restart
	void			(*Init) ( int playerNum, qboolean attractLoop, int vidWidth, int vidHeight );
	void			(*Shutdown) (void);

	void			(*ServerCommand) (void);

	void			(*LoadLayout) ( char *s );

	void			(*BeginFrameSequence) ( frame_t fr );
	void			(*EndFrameSequence) ( int numEntities );

	void			(*NewPacketEntityState) ( int entnum, entity_state_t state );
	void			(*GetEntitySoundOrigin) ( int entnum, vec3_t org );

	void			(*Trace) ( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask );

	void			(*RenderView) ( float frameTime, int realTime, float stereo_separation, qboolean forceRefresh );
} cgame_export_t;
