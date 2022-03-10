
#include "../client/ref.h"

#define	UI_API_VERSION		5

typedef struct
{
	// if api_version is different, the dll cannot be used
	int		api_version;

	void	(*Init) ( void );
	void	(*Shutdown) ( void );

	void	(*Draw) ( void );
	void	(*Keydown) ( int key );

	void	(*MainMenu) (void);
	void	(*ForceMenuOff) (void);
	void	(*AddToServerList) ( netadr_t *adr, char *info );
} uiexport_t;

//
// these are the functions exported by the refresh module
//
typedef struct
{
	void	(*Sys_Error) (int err_level, char *str, ...);

	void	(*Cmd_AddCommand) ( char *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand) ( char *cmd_name );
	void	(*Cmd_ExecuteText) ( int exec_when, char *text );
	void	(*Cmd_Execute) (void);

	qboolean (*Con_Func) (void);
	void	(*Con_Printf) ( int print_level, char *str, ...);

	struct model_s *(*RegisterModel) ( char *name );
	struct shader_s *(*RegisterSkin) ( char *name );
	struct shader_s *(*RegisterPic) ( char *name );
	int		(*ModelNumFrames) ( struct model_s *model );

	void	(*RenderFrame) ( refdef_t *fd );

	void	(*S_StartLocalSound) (char *s);

	void	(*CL_PingServers_f) (void);
	float	(*CL_GetTime_f) (void);
	void	(*CL_SetKeyDest_f) ( enum keydest_t keydest );
	void	(*CL_ResetServerCount_f) (void);
	void	(*CL_Quit_f) (void);

	int		(*Com_ServerState) (void);

	char	*(*NET_AdrToString) ( netadr_t *a );

	char	*(*Key_GetBindingBuf)( int binding );
	void	(*Key_ClearStates) (void);
	char	*(*Key_KeynumToString) ( int keynum );
	void	(*Key_SetBinding) ( int keynum, char *binding );

	// files will be memory mapped read only
	// the returned buffer may be part of a larger pak file,
	// or a discrete file from anywhere in the quake search path
	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int		(*FS_LoadFile) ( char *name, void **buf );
	void	(*FS_FreeFile) ( void *buf );
	int		(*FS_ListFiles)( char *path, char *ext, char *buf, int bufsize );
	char	*(*FS_NextPath) ( char *prevpath );

	// gamedir will be the current directory that generated
	// files should be stored to, ie: "f:\quake\id1"
	char	*(*FS_Gamedir) (void);

	cvar_t *(*Cvar_Get) ( char *name, char *value, int flags );
	cvar_t *(*Cvar_Set) ( char *name, char *value );
	cvar_t *(*Cvar_ForceSet) ( char *name, char *value );	// will return 0 0 if not found
	void	 (*Cvar_SetValue)( char *name, float value );
	float	(*Cvar_VariableValue) ( char *name );
	char	*(*Cvar_VariableString) ( char *name );

	void	(*DrawPic) ( int x, int y, char *name );
	void	(*DrawChar) ( int x, int y, int c, enum fontstyle_t fntstl, vec4_t colour );
	void	(*DrawStringLen) ( int x, int y, char *str, int len, enum fontstyle_t fntstl, vec4_t colour );
	void	(*DrawFill) ( int x, int y, int w, int h, int c );

	void	(*EndFrame) (void);

	void	(*Vid_GetCurrentInfo)( int *width, int *height );
} uiimport_t;

// this is the only function actually exported at the linker level
typedef	uiexport_t	(*GetUiAPI_t) (uiimport_t);
