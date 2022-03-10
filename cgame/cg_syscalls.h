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

#ifdef CGAME_HARD_LINKED
# define CGAME_IMPORT cgi_imp_local
#endif

extern	cgame_import_t	CGAME_IMPORT;

static inline void trap_Print( char *msg ) {
	CGAME_IMPORT.Print( msg );
}

static inline void trap_Error( char *msg ) {
	CGAME_IMPORT.Error( msg );
}

static inline cvar_t *trap_Cvar_Get( char *name, char *value, int flags ) {
	return CGAME_IMPORT.Cvar_Get( name, value, flags );
}

static inline cvar_t *trap_Cvar_Set( char *name, char *value ) {
	return CGAME_IMPORT.Cvar_Set( name, value );
}

static inline void trap_Cvar_SetValue( char *name, float value ) {
	CGAME_IMPORT.Cvar_SetValue( name, value );
}

static inline cvar_t *trap_Cvar_ForceSet( char *name, char *value ) {
	return CGAME_IMPORT.Cvar_ForceSet( name, value );
}

static inline float trap_Cvar_VariableValue( char *name ) {
	return CGAME_IMPORT.Cvar_VariableValue( name );
}

static inline char *trap_Cvar_VariableString( char *name ) {
	return CGAME_IMPORT.Cvar_VariableString( name );
}

static inline int trap_Cmd_Argc( void ) {
	return CGAME_IMPORT.Cmd_Argc ();
}

static inline char *trap_Cmd_Argv( int arg ) {
	return CGAME_IMPORT.Cmd_Argv( arg );
}

static inline char *trap_Cmd_Args( void ) {
	return CGAME_IMPORT.Cmd_Args ();
}

static inline void trap_Cmd_AddCommand( char *name, void(*cmd)(void) ) {
	CGAME_IMPORT.Cmd_AddCommand( name, cmd );
}

static inline void trap_Cmd_RemoveCommand( char *cmd_name ) {
	CGAME_IMPORT.Cmd_RemoveCommand( cmd_name );
}

static inline void trap_Cmd_ExecuteText( int exec_when, char *text ) {
	CGAME_IMPORT.Cmd_ExecuteText( exec_when, text );
}

static inline void trap_Cmd_Execute( void ) {
	CGAME_IMPORT.Cmd_Execute ();
}

static inline int trap_FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return CGAME_IMPORT.FS_FOpenFile( filename, filenum, mode );
}

static inline int trap_FS_Read( void *buffer, size_t len, int file ) {
	return CGAME_IMPORT.FS_Read( buffer, len, file );
}

static inline int trap_FS_Write( const void *buffer, size_t len, int file ) {
	return CGAME_IMPORT.FS_Write( buffer, len, file );
}

static inline int trap_FS_Tell( int file ) {
	return CGAME_IMPORT.FS_Tell( file );
}

static inline int trap_FS_Seek( int file, int offset, int whence ) {
	return CGAME_IMPORT.FS_Seek( file, offset, whence );
}

static inline int trap_FS_Eof( int file ) {
	return CGAME_IMPORT.FS_Eof( file );
}

static inline int trap_FS_Flush( int file ) {
	return CGAME_IMPORT.FS_Flush( file );
}

static inline void trap_FS_FCloseFile( int file ) { 
	CGAME_IMPORT.FS_FCloseFile( file );
}

static inline int trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize ) {
	return CGAME_IMPORT.FS_GetFileList( dir, extension, buf, bufsize );
}

static inline char *trap_FS_Gamedir( void ) {
	return CGAME_IMPORT.FS_Gamedir ();
}

static inline char *trap_Key_GetBindingBuf( int binding ) {
	return CGAME_IMPORT.Key_GetBindingBuf( binding );
}

static inline char *trap_Key_KeynumToString( int keynum ) {
	return CGAME_IMPORT.Key_KeynumToString( keynum );
}

static inline void trap_GetConfigString( int i, char *str, int size ) {
	CGAME_IMPORT.GetConfigString( i, str, size );
}

static inline int trap_Milliseconds( void ) {
	return CGAME_IMPORT.Milliseconds ();
}

static inline void trap_NET_GetUserCmd( int frame, usercmd_t *cmd ) {
	CGAME_IMPORT.NET_GetUserCmd( frame, cmd );
}

static inline int trap_NET_GetCurrentUserCmdNum( void ) {
	return CGAME_IMPORT.NET_GetCurrentUserCmdNum ();
}

static inline void trap_NET_GetCurrentState( int *incomingAcknowledged, int *outgoingSequence ) {
	CGAME_IMPORT.NET_GetCurrentState( incomingAcknowledged, outgoingSequence );
}

static inline void trap_R_UpdateScreen( void ) {
	CGAME_IMPORT.R_UpdateScreen ();
}

static inline int trap_R_GetClippedFragments( vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments ) {
	return CGAME_IMPORT.R_GetClippedFragments( origin, radius, axis, maxfverts, fverts, maxfragments, fragments );
}

static inline void trap_R_ClearScene( void ) {
	CGAME_IMPORT.R_ClearScene ();
}

static inline void trap_R_AddEntityToScene( entity_t *ent ) {
	CGAME_IMPORT.R_AddEntityToScene( ent );
}

static inline void trap_R_AddLightToScene( vec3_t org, float intensity, float r, float g, float b, struct shader_s *shader ) {
	CGAME_IMPORT.R_AddLightToScene( org, intensity, r, g, b, shader );
}

static inline void trap_R_AddPolyToScene( poly_t *poly ) {
	CGAME_IMPORT.R_AddPolyToScene( poly );
}

static inline void trap_R_AddLightStyleToScene( int style, float r, float g, float b ) {
	CGAME_IMPORT.R_AddLightStyleToScene( style, r, g, b );
}

static inline void trap_R_RenderScene( refdef_t *fd ) {
	CGAME_IMPORT.R_RenderScene( fd );
}

static inline void trap_R_RegisterWorldModel( char *name ) {
	CGAME_IMPORT.R_RegisterWorldModel( name );
}

static inline struct model_s *trap_R_RegisterModel( char *name ) {
	return CGAME_IMPORT.R_RegisterModel( name );
}

static inline void trap_R_ModelBounds( struct model_s *mod, vec3_t mins, vec3_t maxs ) {
	CGAME_IMPORT.R_ModelBounds( mod, mins, maxs );
}

static inline struct shader_s *trap_R_RegisterPic( char *name ) {
	return CGAME_IMPORT.R_RegisterPic( name );
}

static inline struct shader_s *trap_R_RegisterSkin( char *name ) {
	return CGAME_IMPORT.R_RegisterSkin( name );
}

static inline struct skinfile_s *trap_R_RegisterSkinFile( char *name ) {
	return CGAME_IMPORT.R_RegisterSkinFile( name );
}

static inline qboolean trap_R_LerpTag( orientation_t *orient, struct model_s *mod, int oldframe, int frame, float lerpfrac, char *name ) {
	return CGAME_IMPORT.R_LerpTag( orient, mod, oldframe, frame, lerpfrac, name );
}

static inline void trap_R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader ) {
	CGAME_IMPORT.R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, color, shader );
}

static inline void trap_R_TransformVectorToScreen( refdef_t *rd, vec3_t in, vec2_t out ) {
	CGAME_IMPORT.R_TransformVectorToScreen( rd, in, out );
}

static int trap_R_SkeletalGetNumBones( struct model_s *mod, int *numFrames ) {
	return CGAME_IMPORT.R_SkeletalGetNumBones( mod, numFrames );
}

static int trap_R_SkeletalGetBoneInfo( struct model_s *mod, int bone, char *name, int size, int *flags ) {
	return CGAME_IMPORT.R_SkeletalGetBoneInfo( mod, bone, name, size, flags );
}

static void trap_R_SkeletalGetBonePose( struct model_s *mod, int bone, int frame, bonepose_t *bonepose ) {
	CGAME_IMPORT.R_SkeletalGetBonePose( mod, bone, frame, bonepose );
}

static inline struct cmodel_s *trap_CM_InlineModel( int num ) {
	return CGAME_IMPORT.CM_InlineModel( num );
}

static inline struct cmodel_s *trap_CM_ModelForBBox( vec3_t mins, vec3_t maxs ) {
	return CGAME_IMPORT.CM_ModelForBBox( mins, maxs );
}

static inline int trap_CM_NumInlineModels( void ) {
	return CGAME_IMPORT.CM_NumInlineModels ();
}

static inline void trap_CM_BoxTrace( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask ) {
	CGAME_IMPORT.CM_BoxTrace( tr, start, end, mins, maxs, cmodel, brushmask );
}

static inline void trap_CM_TransformedBoxTrace( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles ) {
	CGAME_IMPORT.CM_TransformedBoxTrace( tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}

static inline int trap_CM_PointContents( vec3_t p, struct cmodel_s *cmodel ) {
	return CGAME_IMPORT.CM_PointContents( p, cmodel );
}

static inline int trap_CM_TransformedPointContents( vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles ) {
	return CGAME_IMPORT.CM_TransformedPointContents( p, cmodel, origin, angles );
}

static inline void trap_CM_InlineModelBounds( struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs ) {
	CGAME_IMPORT.CM_InlineModelBounds( cmodel, mins, maxs );
}

static inline struct sfx_s *trap_S_RegisterSound( char *name ) {
	return CGAME_IMPORT.S_RegisterSound( name );
}

static inline void trap_S_Update( vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up ) {
	CGAME_IMPORT.S_Update( origin, v_forward, v_right, v_up );
}

static inline void trap_S_StartSound( vec3_t origin, int entnum, int entchannel, struct sfx_s *sfx, float fvol, float attenuation, float timeofs ) {
	CGAME_IMPORT.S_StartSound( origin, entnum, entchannel, sfx, fvol, attenuation, timeofs );
}

static inline void trap_S_AddLoopSound( struct sfx_s *sfx, vec3_t origin ) {
	CGAME_IMPORT.S_AddLoopSound( sfx, origin );
}

static inline void trap_S_StartBackgroundTrack( char *intro, char *loop ) {
	CGAME_IMPORT.S_StartBackgroundTrack( intro, loop );
}

static inline void trap_S_StopBackgroundTrack( void ) {
	CGAME_IMPORT.S_StopBackgroundTrack ();
}

static inline struct mempool_s *trap_MemAllocPool( const char *name, const char *filename, int fileline ) {
	return CGAME_IMPORT.Mem_AllocPool( name, filename, fileline );
}

static inline void *trap_MemAlloc( struct mempool_s *pool, int size, const char *filename, int fileline ) {
	return CGAME_IMPORT.Mem_Alloc( pool, size, filename, fileline );
}

static inline void trap_MemFree( void *data, const char *filename, int fileline ) {
	CGAME_IMPORT.Mem_Free( data, filename, fileline );
}

static inline void trap_MemFreePool( struct mempool_s **pool, const char *filename, int fileline ) {
	CGAME_IMPORT.Mem_FreePool( pool, filename, fileline );
}

static inline void trap_MemEmptyPool( struct mempool_s *pool, const char *filename, int fileline ) {
	CGAME_IMPORT.Mem_EmptyPool( pool, filename, fileline );
}
