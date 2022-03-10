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

#ifdef GAME_HARD_LINKED
# define GAME_IMPORT gi_imp_local
#endif

extern	game_import_t	GAME_IMPORT;

static inline void trap_Print( char *msg ) {
	GAME_IMPORT.Print( msg );
}

static inline void trap_Error( char *msg ) {
	GAME_IMPORT.Error( msg );
}

static inline void trap_Sound( vec3_t origin, struct edict_s *ent, int channel, int soundIndex, float volume, float attenuation ) {
	GAME_IMPORT.Sound( origin, ent, channel, soundIndex, volume, attenuation );
}

static inline void trap_ServerCmd( struct edict_s *ent, char *cmd ) {
	GAME_IMPORT.ServerCmd( ent, cmd );
}

static inline void trap_ConfigString( int num, char *string ) {
	GAME_IMPORT.ConfigString( num, string );
}

static inline void trap_Layout( struct edict_s *ent, char *string ) {
	GAME_IMPORT.Layout( ent, string );
}

static inline void trap_StuffCmd( struct edict_s *ent, char *string ) {
	GAME_IMPORT.StuffCmd( ent, string );
}

static inline int trap_ModelIndex( char *name ) {
	return GAME_IMPORT.ModelIndex( name );
}

static inline int trap_SoundIndex( char *name ) {
	return GAME_IMPORT.SoundIndex( name );
}

static inline int trap_ImageIndex( char *name ) {
	return GAME_IMPORT.ImageIndex( name );
}

static inline void trap_SetBrushModel( struct edict_s *ent, char *name ) {
	GAME_IMPORT.SetBrushModel( ent, name );
}

static inline int trap_Milliseconds( void ) {
	return GAME_IMPORT.Milliseconds ();
}

static inline void trap_Trace( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, struct edict_s *passent, int contentmask ) {
	GAME_IMPORT.Trace( tr, start, mins, maxs, end, passent, contentmask );
}

static inline int trap_PointContents( vec3_t point ) {
	return GAME_IMPORT.PointContents( point );
}

static inline qboolean trap_inPVS( vec3_t p1, vec3_t p2 ) {
	return GAME_IMPORT.inPVS( p1, p2 );
}

static inline qboolean trap_inPHS( vec3_t p1, vec3_t p2 ) {
	return GAME_IMPORT.inPHS( p1, p2 );
}

static inline void trap_SetAreaPortalState( struct edict_s *ent, qboolean open ) {
	GAME_IMPORT.SetAreaPortalState( ent, open );
}

static inline qboolean trap_AreasConnected( int area1, int area2 ) {
	return GAME_IMPORT.AreasConnected( area1, area2 );
}

static inline void trap_LinkEntity( struct edict_s *ent ) {
	GAME_IMPORT.LinkEntity( ent );
}

static inline void trap_UnlinkEntity( struct edict_s *ent ) {
	GAME_IMPORT.UnlinkEntity( ent );
}

static inline int trap_BoxEdicts( vec3_t mins, vec3_t maxs, struct edict_s **list, int maxcount, int areatype ) {
	return GAME_IMPORT.BoxEdicts( mins, maxs, list, maxcount, areatype );
}

static inline qboolean trap_EntityContact( vec3_t mins, vec3_t maxs, edict_t *ent ) {
	return GAME_IMPORT.EntityContact( mins, maxs, ent );
}

static inline struct mempool_s *trap_MemAllocPool( const char *name, const char *filename, int fileline ) {
	return GAME_IMPORT.Mem_AllocPool( name, filename, fileline );
}

static inline void *trap_MemAlloc( struct mempool_s *pool, int size, const char *filename, int fileline ) {
	return GAME_IMPORT.Mem_Alloc( pool, size, filename, fileline );
}

static inline void trap_MemFree( void *data, const char *filename, int fileline ) {
	GAME_IMPORT.Mem_Free( data, filename, fileline );
}

static inline void trap_MemFreePool( struct mempool_s **pool, const char *filename, int fileline ) {
	GAME_IMPORT.Mem_FreePool( pool, filename, fileline );
}

static inline void trap_MemEmptyPool( struct mempool_s *pool, const char *filename, int fileline ) {
	GAME_IMPORT.Mem_EmptyPool( pool, filename, fileline );
}

static inline cvar_t *trap_Cvar_Get( char *name, char *value, int flags ) {
	return GAME_IMPORT.Cvar_Get( name, value, flags );
}

static inline cvar_t *trap_Cvar_Set( char *name, char *value ) {
	return GAME_IMPORT.Cvar_Set( name, value );
}

static inline cvar_t *trap_Cvar_ForceSet( char *name, char *value ) {
	return GAME_IMPORT.Cvar_ForceSet( name, value );
}

static inline int	trap_Cmd_Argc( void ) {
	return GAME_IMPORT.Cmd_Argc ();
}

static inline char *trap_Cmd_Argv( int arg ) {
	return GAME_IMPORT.Cmd_Argv( arg );
}

static inline char *trap_Cmd_Args( void ) {
	return GAME_IMPORT.Cmd_Args ();
}

static inline int trap_FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return GAME_IMPORT.FS_FOpenFile( filename, filenum, mode );
}

static inline int trap_FS_Read( void *buffer, size_t len, int file ) {
	return GAME_IMPORT.FS_Read( buffer, len, file );
}

static inline int trap_FS_Write( const void *buffer, size_t len, int file ) {
	return GAME_IMPORT.FS_Write( buffer, len, file );
}

static inline int trap_FS_Tell( int file ) {
	return GAME_IMPORT.FS_Tell( file );
}

static inline int trap_FS_Seek( int file, int offset, int whence ) {
	return GAME_IMPORT.FS_Seek( file, offset, whence );
}

static inline int trap_FS_Eof( int file ) {
	return GAME_IMPORT.FS_Eof( file );
}

static inline int trap_FS_Flush( int file ) {
	return GAME_IMPORT.FS_Flush( file );
}

static inline void trap_FS_FCloseFile( int file ) { 
	GAME_IMPORT.FS_FCloseFile( file );
}

static inline int trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end ) {
	return GAME_IMPORT.FS_GetFileList( dir, extension, buf, bufsize, start, end );
}

static inline char *trap_FS_Gamedir( void ) {
	return GAME_IMPORT.FS_Gamedir ();
}

static inline void trap_AddCommandString( char *text ) {
	GAME_IMPORT.AddCommandString( text );
}

static inline void trap_FakeClientConnect( char *fakeUserinfo, char *fakeIP ) {
	GAME_IMPORT.FakeClientConnect( fakeUserinfo, fakeIP );
}

static inline void trap_DropClient( edict_t *ent ) {
	GAME_IMPORT.DropClient( ent );
}

static inline void trap_LocateEntities( struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts ) {
	GAME_IMPORT.LocateEntities( edicts, edict_size, num_edicts, max_edicts );
}
