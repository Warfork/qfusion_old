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
#include "client.h"

static cgame_export_t *cge;

static mempool_t *cl_gamemodulepool;

/*
===============
CL_GameModule_Error
===============
*/
static void CL_GameModule_Error( char *msg ) {
	Com_Error( ERR_DROP, "%s", msg );
}

/*
===============
CL_GameModule_Print
===============
*/
static void CL_GameModule_Print( char *msg ) {
	Com_Printf( "%s", msg );
}

/*
===============
CL_GameModule_GetConfigString
===============
*/
static void CL_GameModule_GetConfigString( int i, char *str, int size )
{
	if( i < 0 || i >= MAX_CONFIGSTRINGS )
		Com_DPrintf( S_COLOR_RED "CL_GameModule_GetConfigString: i > MAX_CONFIGSTRINGS" );
	if( !str || size <= 0 )
		Com_DPrintf( S_COLOR_RED "CL_GameModule_GetConfigString: NULL string" );
	Q_strncpyz( str, cl.configstrings[i], size );
}

/*
===============
CL_GameModule_NET_GetCurrentState
===============
*/
static void CL_GameModule_NET_GetCurrentState( int *incomingAcknowledged, int *outgoingSequence )
{  
	if( incomingAcknowledged )
		*incomingAcknowledged = cls.netchan.incoming_acknowledged;
	if( outgoingSequence )
		*outgoingSequence = cls.netchan.outgoing_sequence;
}

/*
===============
CL_GameModule_NET_GetUserCmd
===============
*/
static void CL_GameModule_NET_GetUserCmd( int frame, usercmd_t *cmd ) {
	if( cmd )
		*cmd = cl.cmds[frame & CMD_MASK];
} 

/*
===============
CL_GameModule_NET_GetCurrentUserCmdNum
===============
*/
static int CL_GameModule_NET_GetCurrentUserCmdNum( void ) {
	return cl.cmdNum;
} 

/*
===============
CL_GameModule_R_RegisterWorldModel
===============
*/
static void CL_GameModule_R_RegisterWorldModel( char *model ) {
	R_RegisterWorldModel( model, CM_VisData () );
} 

/*
===============
CL_GameModule_MemAlloc
===============
*/
static void *CL_GameModule_MemAlloc( mempool_t *pool, int size, const char *filename, int fileline ) {
	return _Mem_Alloc( pool, size, MEMPOOL_CLIENTGAME, 0, filename, fileline );
}

/*
===============
CL_GameModule_MemFree
===============
*/
static void CL_GameModule_MemFree( void *data, const char *filename, int fileline ) {
	_Mem_Free( data, MEMPOOL_CLIENTGAME, 0, filename, fileline );
}

/*
===============
CL_GameModule_MemAllocPool
===============
*/
static mempool_t *CL_GameModule_MemAllocPool( const char *name, const char *filename, int fileline ) {
	return _Mem_AllocPool( cl_gamemodulepool, name, MEMPOOL_CLIENTGAME, filename, fileline );
}

/*
===============
CL_GameModule_MemFreePool
===============
*/
static void CL_GameModule_MemFreePool( mempool_t **pool, const char *filename, int fileline ) {
	_Mem_FreePool( pool, MEMPOOL_CLIENTGAME, 0, filename, fileline );
}

/*
===============
CL_GameModule_MemEmptyPool
===============
*/
static void CL_GameModule_MemEmptyPool( mempool_t *pool, const char *filename, int fileline ) {
	_Mem_EmptyPool( pool, MEMPOOL_CLIENTGAME, 0, filename, fileline );
}

/*
===============
CL_GameModule_Init
===============
*/
void CL_GameModule_Init (void)
{
	int apiversion;
	int oldState, start;
	cgame_import_t import;

	// unload anything we have now
	CL_GameModule_Shutdown ();

	cl_gamemodulepool = Mem_AllocPool ( NULL, "Client Game Progs" );

	import.Error = CL_GameModule_Error;
	import.Print = CL_GameModule_Print;

	import.Cvar_Get = Cvar_Get;
	import.Cvar_Set = Cvar_Set;
	import.Cvar_SetValue = Cvar_SetValue;
	import.Cvar_ForceSet = Cvar_ForceSet;
	import.Cvar_VariableString = Cvar_VariableString;
	import.Cvar_VariableValue = Cvar_VariableValue;

	import.Cmd_Argc = Cmd_Argc;
	import.Cmd_Argv = Cmd_Argv;
	import.Cmd_Args = Cmd_Args;

	import.Cmd_AddCommand = Cmd_AddCommand;
	import.Cmd_RemoveCommand = Cmd_RemoveCommand;
	import.Cmd_ExecuteText = Cbuf_ExecuteText;
	import.Cmd_Execute = Cbuf_Execute;

	import.FS_FOpenFile = FS_FOpenFile;
	import.FS_Read = FS_Read;
	import.FS_Write = FS_Write;
	import.FS_Tell = FS_Tell;
	import.FS_Seek = FS_Seek;
	import.FS_Eof = FS_Eof;
	import.FS_Flush = FS_Flush;
	import.FS_FCloseFile = FS_FCloseFile;
	import.FS_GetFileList = FS_GetFileList;
	import.FS_Gamedir = FS_Gamedir;

	import.Key_GetBindingBuf = Key_GetBindingBuf;
	import.Key_KeynumToString = Key_KeynumToString;

	import.GetConfigString = CL_GameModule_GetConfigString;

	import.Milliseconds = Sys_Milliseconds;

	import.NET_GetUserCmd = CL_GameModule_NET_GetUserCmd;
	import.NET_GetCurrentUserCmdNum = CL_GameModule_NET_GetCurrentUserCmdNum;
	import.NET_GetCurrentState = CL_GameModule_NET_GetCurrentState;

	import.R_UpdateScreen = SCR_UpdateScreen;
	import.R_GetClippedFragments = R_GetClippedFragments;
	import.R_ClearScene = R_ClearScene;
	import.R_AddEntityToScene = R_AddEntityToScene;
	import.R_AddLightToScene = R_AddLightToScene;
	import.R_AddPolyToScene = R_AddPolyToScene;
	import.R_AddLightStyleToScene = R_AddLightStyleToScene;
	import.R_RenderScene = R_RenderScene;
	import.R_RegisterWorldModel = CL_GameModule_R_RegisterWorldModel;
	import.R_ModelBounds = R_ModelBounds;
	import.R_RegisterModel = R_RegisterModel;
	import.R_RegisterPic = R_RegisterPic;
	import.R_RegisterSkin = R_RegisterSkin;
	import.R_RegisterSkinFile = R_RegisterSkinFile;
	import.R_LerpTag = R_LerpTag;
	import.R_DrawStretchPic = R_DrawStretchPic;
	import.R_TransformVectorToScreen = R_TransformVectorToScreen;
	import.R_SkeletalGetNumBones = R_SkeletalGetNumBones;
	import.R_SkeletalGetBoneInfo = R_SkeletalGetBoneInfo;
	import.R_SkeletalGetBonePose = R_SkeletalGetBonePose;
	import.R_SetCustomColor = R_SetCustomColor;
	import.R_LightForOrigin = R_LightForOrigin;

	import.CM_NumInlineModels = CM_NumInlineModels;
	import.CM_InlineModel = CM_InlineModel;
	import.CM_BoxTrace = CM_BoxTrace;
	import.CM_TransformedBoxTrace = CM_TransformedBoxTrace;
	import.CM_PointContents = CM_PointContents;
	import.CM_TransformedPointContents = CM_TransformedPointContents;
	import.CM_ModelForBBox = CM_ModelForBBox;
	import.CM_InlineModelBounds = CM_InlineModelBounds;
	import.CM_LoadMapMessage = CM_LoadMapMessage;

	import.S_RegisterSound = S_RegisterSound;
	import.S_StartSound = S_StartSound;
	import.S_Update = S_Update;
	import.S_AddLoopSound = S_AddLoopSound;
	import.S_StartBackgroundTrack = S_StartBackgroundTrack;
	import.S_StopBackgroundTrack = S_StopBackgroundTrack;

	import.Mem_Alloc = CL_GameModule_MemAlloc;
	import.Mem_Free = CL_GameModule_MemFree;
	import.Mem_AllocPool = CL_GameModule_MemAllocPool;
	import.Mem_FreePool = CL_GameModule_MemFreePool;
	import.Mem_EmptyPool = CL_GameModule_MemEmptyPool;

	cge = ( cgame_export_t * )Sys_LoadGameLibrary( LIB_CGAME, &import );
	if( !cge )
		Com_Error( ERR_DROP, "failed to load client game DLL" );

	apiversion = cge->API ();
	if ( apiversion != CGAME_API_VERSION ) {
		Sys_UnloadGameLibrary( LIB_CGAME );
		Mem_FreePool( &cl_gamemodulepool );
		cge = NULL;
		Com_Error( ERR_DROP, "client game is version %i, not %i", apiversion, CGAME_API_VERSION );
	}

	oldState = cls.state;
	cls.state = ca_loading;

	start = Sys_Milliseconds ();
	cge->Init( cl.playernum, cl.attractloop, cl.serverframetime, viddef.width, viddef.height );
	Com_DPrintf( "CL_GameModule_Init: %.2f seconds\n", (float)(Sys_Milliseconds () - start) * 0.001f );

	cls.state = oldState;
	cls.cgameActive = qtrue;

	// check memory integrity
	Mem_CheckSentinelsGlobal ();

	S_SoundsInMemory ();

	Sys_SendKeyEvents ();	// pump message loop
}

/*
===============
CL_GameModule_Shutdown
===============
*/
void CL_GameModule_Shutdown (void)
{
	if ( !cge )
		return;

	cls.cgameActive = qfalse;

	cge->Shutdown ();
	Sys_UnloadGameLibrary( LIB_CGAME );
	Mem_FreePool( &cl_gamemodulepool );
	cge = NULL;
}

/*
==============
CL_GameModule_LoadLayout
==============
*/
void CL_GameModule_LoadLayout( char *s )
{
	if( cge )
		cge->LoadLayout( s );
}

/*
==============
CG_Module_BeginFrameSequence
==============
*/
void CL_GameModule_BeginFrameSequence( void )
{
	if( cge )
		cge->BeginFrameSequence( cl.frame );
}

/*
==============
CL_GameModule_NewPacketEntityState
==============
*/
void CL_GameModule_NewPacketEntityState( int entnum, entity_state_t *state )
{
	if( cge )
		cge->NewPacketEntityState( entnum, *state );
}

/*
==============
CL_GameModule_EndFrameSequence
==============
*/
void CL_GameModule_EndFrameSequence( void )
{
	if( cge )
		cge->EndFrameSequence( cl.frame.numEntities );
}

/*
===============
CL_GameModule_GlobalSound
===============
*/
void CL_GameModule_GlobalSound( vec3_t origin, int entNum, int entChannel, int soundNum, float fvol, float attenuation )
{
	if( cge )
		cge->GlobalSound( origin, entNum, entChannel, soundNum, fvol, attenuation );
}

/*
==============
CL_GameModule_GetEntitySoundOrigin
==============
*/
void CL_GameModule_GetEntitySoundOrigin( int entNum, vec3_t origin )
{
	if( cge )
		cge->GetEntitySoundOrigin( entNum, origin );
}

/*
===============
CL_GameModule_ServerCommand
===============
*/
void CL_GameModule_ServerCommand( void )
{
	if( cge )
		cge->ServerCommand ();
}

/*
===============
CL_GameModule_RenderView
===============
*/
void CL_GameModule_RenderView( float stereo_separation )
{
	if( cge )
		cge->RenderView( cls.trueframetime, cls.realtime, stereo_separation, qfalse );
}

/*
===============
CL_GameModule_Trace
===============
*/
void CL_GameModule_Trace( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask )
{
	if( tr && cge )
		cge->Trace( tr, start, mins, maxs, end, passent, contentmask );
}
