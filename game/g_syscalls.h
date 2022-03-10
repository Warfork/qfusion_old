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

void trap_Sound( vec3_t origin, struct edict_s *ent, int channel, int soundindex, float volume, float attenuation );

void trap_ServerCmd( struct edict_s *ent, char *cmd );
void trap_ConfigString( int num, char *string );

void trap_Layout( struct edict_s *ent, char *string );
void trap_StuffCmd( struct edict_s *ent, char *string );

int trap_ModelIndex( char *name );
int trap_SoundIndex( char *name );
int trap_ImageIndex( char *name );

void trap_SetBrushModel( struct edict_s *ent, char *name );

int trap_Milliseconds( void );

void trap_Trace( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, struct edict_s *passent, int contentmask );
int trap_PointContents( vec3_t point );
qboolean trap_inPVS( vec3_t p1, vec3_t p2 );
qboolean trap_inPHS( vec3_t p1, vec3_t p2 );
void trap_SetAreaPortalState( struct edict_s *ent, qboolean open );
qboolean trap_AreasConnected( int area1, int area2 );

void trap_LinkEntity( struct edict_s *ent );
void trap_UnlinkEntity( struct edict_s *ent );
int trap_BoxEdicts( vec3_t mins, vec3_t maxs, struct edict_s **list, int maxcount, int areatype );
qboolean trap_EntityContact( vec3_t mins, vec3_t maxs, edict_t *ent );

struct mempool_s *trap_MemAllocPool( const char *name, const char *filename, int fileline );
void *trap_MemAlloc( struct mempool_s *pool, int size, const char *filename, int fileline );
void trap_MemFree( void *data, const char *filename, int fileline );
void trap_MemFreePool( struct mempool_s **pool, const char *filename, int fileline );
void trap_MemEmptyPool( struct mempool_s *pool, const char *filename, int fileline );

cvar_t *trap_Cvar_Get( char *name, char *value, int flags );
cvar_t *trap_Cvar_Set( char *name, char *value );
cvar_t *trap_Cvar_ForceSet( char *name, char *value );

int	 trap_Cmd_Argc( void );
char *trap_Cmd_Argv( int arg );
char *trap_Cmd_Args( void );

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

void trap_AddCommandString( char *text );

void trap_FakeClientConnect( char *fakeUserinfo, char *fakeIP );
void trap_DropClient( edict_t *ent );

void trap_LocateEntities( struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts );
