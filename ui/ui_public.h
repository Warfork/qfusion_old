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
#include "../cgame/ref.h"

#define	UI_API_VERSION		6

//
// these are the functions exported by the refresh module
//
typedef struct
{
	// halts the application
	void		(*Error)( char *str );

	// console messages
	void		(*Print)( char *str );

	// console variable interaction
	cvar_t 		*(*Cvar_Get)( const char *name, const char *value, int flags );
	cvar_t 		*(*Cvar_Set)( const char *name, const char *value );
	cvar_t 		*(*Cvar_ForceSet)( const char *name, const char *value );	// will return 0 0 if not found
	void		(*Cvar_SetValue)( const char *name, float value );
	float		(*Cvar_VariableValue)( const char *name );
	char		*(*Cvar_VariableString)( const char *name );

	void		(*Cmd_AddCommand)( char *name, void(*cmd)(void) );
	void		(*Cmd_RemoveCommand)( char *cmd_name );
	void		(*Cmd_ExecuteText)( int exec_when, char *text );
	void		(*Cmd_Execute)( void );

	void		(*R_ClearScene)( void );
	void		(*R_AddEntityToScene)( entity_t *ent );
	void		(*R_AddLightToScene)( vec3_t org, float intensity, float r, float g, float b, struct shader_s *shader );
	void		(*R_AddPolyToScene)( poly_t *poly );
	void		(*R_RenderScene)( refdef_t *fd );
	void		(*R_EndFrame)( void );
	void		(*R_ModelBounds)( struct model_s *mod, vec3_t mins, vec3_t maxs );
	struct model_s 	*(*R_RegisterModel)( char *name );
	struct shader_s *(*R_RegisterSkin)( char *name );
	struct shader_s *(*R_RegisterPic)( char *name );
	struct skinfile_s *(*R_RegisterSkinFile)( char *name );
	qboolean 	(*R_LerpTag)( orientation_t *orient, struct model_s *mod, int oldframe, int frame, float lerpfrac, const char *name );
	void		(*R_DrawStretchPic)( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader );
	void		(*R_TransformVectorToScreen)( refdef_t *rd, vec3_t in, vec2_t out );
	int			(*R_SkeletalGetNumBones)( struct model_s *mod, int *numFrames );
	int			(*R_SkeletalGetBoneInfo)( struct model_s *mod, int bone, char *name, size_t name_size, int *flags );
	void		(*R_SkeletalGetBonePose)( struct model_s *mod, int bone, int frame, bonepose_t *bonepose );
	void		(*R_SetCustomColor)( int num, int r, int g, int b );

	char		*(*CM_LoadMapMessage)( char *name, char *message, int size );

	void		(*S_StartLocalSound)( char *s );
	void		(*S_StartBackgroundTrack)( char *intro, char *loop );
	void		(*S_StopBackgroundTrack)( void );

	void		(*CL_Quit)( void );
	void		(*CL_SetKeyDest)( int key_dest );
	void		(*CL_ResetServerCount)( void );
	void		(*CL_GetClipboardData)( char *string, int size );

	char		*(*Key_GetBindingBuf)( int binding );
	void		(*Key_ClearStates)( void );
	char		*(*Key_KeynumToString)( int keynum );
	void		(*Key_SetBinding)( int keynum, char *binding );
	qboolean 	(*Key_IsDown)( int keynum );

	void		(*GetConfigString)( int i, char *str, int size );
	int			(*Milliseconds)( void );

	qboolean	(*VID_GetModeInfo)( int *width, int *height, qboolean *wideScreen, int mode );

	// files will be memory mapped read only
	// the returned buffer may be part of a larger pak file,
	// or a discrete file from anywhere in the quake search path
	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int			(*FS_FOpenFile)( const char *filename, int *filenum, int mode );
	int			(*FS_Read)( void *buffer, size_t len, int file );
	int			(*FS_Write)( const void *buffer, size_t len, int file );
	int			(*FS_Tell)( int file );
	int			(*FS_Seek)( int file, int offset, int whence );
	int			(*FS_Eof)( int file );
	int			(*FS_Flush)( int file );
	void		(*FS_FCloseFile)( int file );
	int			(*FS_GetFileList)( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end );
	char		*(*FS_Gamedir)( void );

	void		*(*Mem_Alloc)( size_t size, const char *filename, int fileline );
	void		(*Mem_Free)( void *data, const char *filename, int fileline );
} ui_import_t;

typedef struct
{
	// if API is different, the dll cannot be used
	int			(*API)( void );

	void		(*Init)( int vidWidth, int vidHeight );
	void		(*Shutdown)( void );

	void		(*Refresh)( int time, int clientState, int serverState, qboolean backGround );
	void		(*DrawConnectScreen)( char *serverName, int connectCount, qboolean backGround );

	void		(*Keydown)( int key );

	void		(*MouseMove)( int dx, int dy );

	void		(*MainMenu)( void );
	void		(*ForceMenuOff)( void );
	void		(*AddToServerList)( char *adr, char *info );
} ui_export_t;
