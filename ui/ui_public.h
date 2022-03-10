
#include "../cgame/ref.h"

#define	UI_API_VERSION		1

//
// these are the functions exported by the refresh module
//
typedef struct
{
	// halts the application
	void		(*Error) ( char *str );

	// console messages
	void		(*Print) ( char *str );

	// console variable interaction
	cvar_t 		*(*Cvar_Get) ( char *name, char *value, int flags );
	cvar_t 		*(*Cvar_Set) ( char *name, char *value );
	cvar_t 		*(*Cvar_ForceSet) ( char *name, char *value );	// will return 0 0 if not found
	void		(*Cvar_SetValue) ( char *name, float value );
	float		(*Cvar_VariableValue) ( char *name );
	char		*(*Cvar_VariableString) ( char *name );

	void		(*Cmd_AddCommand) ( char *name, void(*cmd)(void) );
	void		(*Cmd_RemoveCommand) ( char *cmd_name );
	void		(*Cmd_ExecuteText) ( int exec_when, char *text );
	void		(*Cmd_Execute) (void);

	void		(*R_RenderFrame) ( refdef_t *fd );
	void		(*R_EndFrame) (void);
	void		(*R_ModelBounds) ( struct model_s *mod, vec3_t mins, vec3_t maxs );
	struct model_s 	*(*R_RegisterModel) ( char *name );
	struct shader_s *(*R_RegisterSkin) ( char *name );
	struct shader_s *(*R_RegisterPic) ( char *name );
	struct skinfile_s *(*R_RegisterSkinFile) ( char *name );
	qboolean 	(*R_LerpAttachment) ( orientation_t *orient, struct model_s *mod, int frame, int oldframe, float backlerp, char *name );

	void		(*S_StartLocalSound) ( char *s );

	void		(*CL_Quit) (void);
	void		(*CL_SetKeyDest) ( int key_dest );
	void		(*CL_ResetServerCount) (void);
	void		(*CL_GetClipboardData) ( char *string, int size );

	char		*(*Key_GetBindingBuf)( int binding );
	void		(*Key_ClearStates) (void);
	char		*(*Key_KeynumToString) ( int keynum );
	void		(*Key_SetBinding) ( int keynum, char *binding );
	qboolean 	(*Key_IsDown) ( int keynum );

	void		(*GetConfigString) ( int i, char *str, int size );

	// files will be memory mapped read only
	// the returned buffer may be part of a larger pak file,
	// or a discrete file from anywhere in the quake search path
	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int			(*FS_LoadFile) ( const char *name, void **buf );
	void		(*FS_FreeFile) ( void *buf );
	int			(*FS_FileExists) ( const char *path );
	int			(*FS_ListFiles)( const char *path, const char *ext, char *buf, int bufsize );

	// gamedir will be the current directory that generated
	// files should be stored to, ie: "f:\quake\id1"
	char		*(*FS_Gamedir) (void);

	struct mempool_s *(*Mem_AllocPool) ( const char *name, const char *filename, int fileline );

	void		*(*Mem_Alloc) ( struct mempool_s *pool, int size, const char *filename, int fileline );
	void		(*Mem_Free) ( void *data, const char *filename, int fileline );
	void		(*Mem_FreePool) ( struct mempool_s **pool, const char *filename, int fileline );
	void		(*Mem_EmptyPool) ( struct mempool_s *pool, const char *filename, int fileline );

	void		(*Draw_StretchPic) ( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader );
} ui_import_t;

typedef struct
{
	// if API is different, the dll cannot be used
	int			(*API) (void);

	void		(*Init) ( int vidWidth, int vidHeight );
	void		(*Shutdown) (void);

	void		(*Refresh) ( int time, int clientState, int serverState, qboolean backGround );
	void		(*DrawConnectScreen) ( char *serverName, int connectCount, qboolean backGround );

	void		(*Keydown) ( int key );

	void		(*MouseMove) ( int dx, int dy );

	void		(*MainMenu) (void);
	void		(*ForceMenuOff) (void);
	void		(*AddToServerList) ( char *adr, char *info );
} ui_export_t;
