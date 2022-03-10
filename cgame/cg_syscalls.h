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

void trap_Print( char *msg );
void trap_Error( char *msg );

cvar_t *trap_Cvar_Get( char *name, char *value, int flags );
cvar_t *trap_Cvar_Set( char *name, char *value );
cvar_t *trap_Cvar_ForceSet( char *name, char *value );
void trap_Cvar_SetValue( char *name, float value );
float trap_Cvar_VariableValue( char *name );
char *trap_Cvar_VariableString( char *name );

int trap_Cmd_Argc( void );
char *trap_Cmd_Argv( int arg );
char *trap_Cmd_Args( void );
void trap_Cmd_AddCommand( char *name, void(*cmd)( void ) );
void trap_Cmd_RemoveCommand( char *cmd_name );

int	trap_FS_FOpenFile( const char *filename, int *filenum, int mode );
int	trap_FS_Read( void *buffer, size_t len, int file );
int	trap_FS_Write( const void *buffer, size_t len, int file );
int	trap_FS_Tell( int file );
int	trap_FS_Seek( int file, int offset, int whence );
int	trap_FS_Eof( int file );
int	trap_FS_Flush( int file );
void trap_FS_FCloseFile( int file );
int	trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize );
char *trap_FS_Gamedir( void );

char *trap_Key_GetBindingBuf( int binding );
char *trap_Key_KeynumToString( int keynum );

void trap_GetConfigString( int i, char *str, int size );

int trap_Milliseconds( void );

void trap_NET_GetUserCmd( int frame, usercmd_t *cmd );
int trap_NET_GetCurrentUserCmdNum( void );
void trap_NET_GetCurrentState( int *incomingAcknowledged, int *outgoingSequence );

void trap_R_UpdateScreen( void );
int trap_R_GetClippedFragments( vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments );
void trap_R_ClearScene( void );
void trap_R_AddEntityToScene( entity_t *ent );
void trap_R_AddLightToScene( vec3_t org, float intensity, float r, float g, float b );
void trap_R_AddPolyToScene( poly_t *poly );
void trap_R_RenderScene( refdef_t *fd );
void trap_R_RegisterWorldModel( char *name );
void trap_R_ModelBounds( struct model_s *mod, vec3_t mins, vec3_t maxs );
struct model_s *trap_R_RegisterModel( char *name );
struct shader_s *trap_R_RegisterPic( char *name );
struct shader_s *trap_R_RegisterSkin( char *name );
struct skinfile_s *trap_R_RegisterSkinFile( char *name );
qboolean trap_R_LerpAttachment( orientation_t *orient, struct model_s *mod, int frame, int oldframe, float lerpfrac, char *name );
void trap_R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader );

struct cmodel_s *trap_CM_InlineModel( int num );
struct cmodel_s *trap_CM_ModelForBBox( vec3_t mins, vec3_t maxs );
int trap_CM_NumInlineModels( void );
void trap_CM_BoxTrace( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask );
void trap_CM_TransformedBoxTrace( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles );
int trap_CM_PointContents( vec3_t p, struct cmodel_s *cmodel );
int trap_CM_TransformedPointContents( vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles );
void trap_CM_InlineModelBounds( struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs );

void trap_S_Update( vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up );
struct sfx_s *trap_S_RegisterSound( char *name );
void trap_S_StartSound( vec3_t origin, int entnum, int entchannel, struct sfx_s *sfx, float fvol, float attenuation, float timeofs );
void trap_S_AddLoopSound( struct sfx_s *sfx, vec3_t origin );
void trap_S_StartBackgroundTrack( char *intro, char *loop );
void trap_S_StopBackgroundTrack( void );

struct mempool_s *trap_MemAllocPool( const char *name, const char *filename, int fileline );
void *trap_MemAlloc( struct mempool_s *pool, int size, const char *filename, int fileline );
void trap_MemFree( void *data, const char *filename, int fileline );
void trap_MemFreePool( struct mempool_s **pool, const char *filename, int fileline );
void trap_MemEmptyPool( struct mempool_s *pool, const char *filename, int fileline );
