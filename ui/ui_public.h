
#include "../cgame/ref.h"

#define	UI_API_VERSION		4

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
	cvar_t 		*(*Cvar_Get)( char *name, char *value, int flags );
	cvar_t 		*(*Cvar_Set)( char *name, char *value );
	cvar_t 		*(*Cvar_ForceSet)( char *name, char *value );	// will return 0 0 if not found
	void		(*Cvar_SetValue)( char *name, float value );
	float		(*Cvar_VariableValue)( char *name );
	char		*(*Cvar_VariableString)( char *name );

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
	int			(*R_SkeletalGetBoneInfo)( struct model_s *mod, int bone, char *name, int size, int *flags );
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

	struct mempool_s *(*Mem_AllocPool)( const char *name, const char *filename, int fileline );

	void		*(*Mem_Alloc)( struct mempool_s *pool, int size, const char *filename, int fileline );
	void		(*Mem_Free)( void *data, const char *filename, int fileline );
	void		(*Mem_FreePool)( struct mempool_s **pool, const char *filename, int fileline );
	void		(*Mem_EmptyPool)( struct mempool_s *pool, const char *filename, int fileline );
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
