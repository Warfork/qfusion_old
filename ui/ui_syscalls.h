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

#include "../client/ref.h"

void trap_Sys_Error ( int err_level, char *str, ... );

void trap_Cmd_AddCommand ( char *name, void(*cmd)(void) );
void trap_Cmd_RemoveCommand ( char *cmd_name );
void trap_Cmd_ExecuteText ( int exec_when, char *text );
void trap_Cmd_Execute (void);
qboolean trap_Con_Func (void);
void trap_Con_Printf ( int print_level, char *str, ... );

struct model_s *trap_RegisterModel ( char *name );
struct shader_s *trap_RegisterSkin ( char *name ) ;
struct image_s *trap_RegisterPic ( char *name );
int	trap_ModelNumFrames ( struct model_s *model );

void trap_RenderFrame ( refdef_t *fd );
void trap_EndFrame (void);

void trap_S_StartLocalSound ( char *s );

void trap_CL_Snd_Restart_f (void);
void trap_CL_PingServers_f (void);
float trap_CL_GetTime_f (void);
void trap_CL_SetKeyDest_f ( enum keydest_t keydest );
void trap_CL_ResetServerCount_f (void);
void trap_CL_Quit_f (void);

int trap_Com_ServerState (void);

char *trap_NET_AdrToString ( netadr_t *a );

char *trap_Key_GetBindingBuf ( int binding );
void trap_Key_ClearStates(void);
char *trap_Key_KeynumToString ( int keynum );
void trap_Key_SetBinding ( int keynum, char *binding );

int	trap_FS_LoadFile ( char *name, void **buf );
void trap_FS_FreeFile ( void *buf );
int	trap_FS_ListFiles ( char *path, char *ext, char *buf, int bufsize );
char *trap_FS_NextPath ( char *prevpath );
char *trap_FS_Gamedir (void);

cvar_t *trap_Cvar_Get ( char *name, char *value, int flags );
cvar_t *trap_Cvar_Set( char *name, char *value );
void trap_Cvar_SetValue ( char *name, float value );
cvar_t *trap_Cvar_ForceSet ( char *name, char *value );
float trap_Cvar_VariableValue ( char *name );
char *trap_Cvar_VariableString ( char *name );

void trap_DrawPic ( int x, int y, char *name );
void trap_DrawChar ( int x, int y, int c, fontstyle_t fntstl, vec4_t colour );
void trap_DrawStringLen ( int x, int y, char *str, int len, fontstyle_t fntstl, vec4_t colour );
void trap_DrawFill ( int x, int y, int w, int h, int c ) ;

void trap_Vid_GetCurrentInfo ( int *width, int *height );

int trap_GetWidth (void);
int trap_GetHeight (void);
