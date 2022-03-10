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

void trap_Error( char *str );
void trap_Print( char *str );

void trap_Cmd_AddCommand( char *name, void(*cmd)(void) );
void trap_Cmd_RemoveCommand( char *cmd_name );
void trap_Cmd_ExecuteText( int exec_when, char *text );
void trap_Cmd_Execute( void );

void trap_R_ClearScene( void );
void trap_R_AddEntityToScene( entity_t *ent );
void trap_R_AddLightToScene( vec3_t org, float intensity, float r, float g, float b );
void trap_R_AddPolyToScene( poly_t *poly );
void trap_R_RenderScene( refdef_t *fd );
void trap_R_EndFrame( void );
void trap_R_ModelBounds( struct model_s *mod, vec3_t mins, vec3_t maxs );
struct model_s *trap_R_RegisterModel( char *name );
struct shader_s *trap_R_RegisterSkin( char *name );
struct shader_s *trap_R_RegisterPic( char *name );
struct skinfile_s *trap_R_RegisterSkinFile( char *name );
qboolean trap_R_LerpAttachment( orientation_t *orient, struct model_s *mod, int oldframe, int frame, float lerpfrac, char *name );
void trap_R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader );

void trap_S_StartLocalSound( char *s );
void trap_S_StartBackgroundTrack( char *intro, char *loop );
void trap_S_StopBackgroundTrack( void );

void trap_CL_Quit( void );
void trap_CL_SetKeyDest( int key_dest );
void trap_CL_ResetServerCount( void );
void trap_CL_GetClipboardData( char *string, int size );

char *trap_Key_GetBindingBuf( int binding );
void trap_Key_ClearStates( void );
char *trap_Key_KeynumToString( int keynum );
void trap_Key_SetBinding( int keynum, char *binding );
qboolean trap_Key_IsDown( int keynum );

void trap_GetConfigString( int i, char *str, int size );

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

cvar_t *trap_Cvar_Get( char *name, char *value, int flags );
cvar_t *trap_Cvar_Set( char *name, char *value );
void trap_Cvar_SetValue( char *name, float value );
cvar_t *trap_Cvar_ForceSet( char *name, char *value );
float trap_Cvar_VariableValue( char *name );
char *trap_Cvar_VariableString( char *name );

struct mempool_s *trap_Mem_AllocPool( const char *name, const char *filename, int fileline );
void *trap_Mem_Alloc( struct mempool_s *pool, int size, const char *filename, int fileline );
void trap_Mem_Free( void *data, const char *filename, int fileline );
void trap_Mem_FreePool( struct mempool_s **pool, const char *filename, int fileline );
void trap_Mem_EmptyPool( struct mempool_s *pool, const char *filename, int fileline );
