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

void trap_Sys_Error ( int err_level, char *str, ... );

void trap_Cmd_AddCommand ( char *name, void(*cmd)(void) );
void trap_Cmd_RemoveCommand ( char *cmd_name );
void trap_Cmd_ExecuteText ( int exec_when, char *text );
void trap_Cmd_Execute (void);
void trap_Con_Printf ( int print_level, char *str, ... );

struct model_s *trap_RegisterModel ( char *name );
struct shader_s *trap_RegisterSkin ( char *name );
struct shader_s *trap_RegisterPic ( char *name );

void trap_RenderFrame ( refdef_t *fd );
void trap_EndFrame (void);

void trap_S_StartLocalSound ( char *s );

int trap_CL_GetTime (void);
void trap_CL_SetKeyDest ( int key_dest );
void trap_CL_ResetServerCount (void);
void trap_CL_Quit (void);

int trap_GetClientState (void);
int trap_GetServerState (void);

char *trap_Key_GetBindingBuf ( int binding );
void trap_Key_ClearStates(void);
char *trap_Key_KeynumToString ( int keynum );
void trap_Key_SetBinding ( int keynum, char *binding );

int	trap_FS_LoadFile ( char *name, void **buf );
void trap_FS_FreeFile ( void *buf );
int trap_FS_FileExists ( char *path );
int	trap_FS_ListFiles ( char *path, char *ext, char *buf, int bufsize );
char *trap_FS_NextPath ( char *prevpath );
char *trap_FS_Gamedir (void);

cvar_t *trap_Cvar_Get ( char *name, char *value, int flags );
cvar_t *trap_Cvar_Set( char *name, char *value );
void trap_Cvar_SetValue ( char *name, float value );
cvar_t *trap_Cvar_ForceSet ( char *name, char *value );
float trap_Cvar_VariableValue ( char *name );
char *trap_Cvar_VariableString ( char *name );

void trap_DrawStretchPic (int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *color, struct shader_s *shader);
void trap_DrawChar ( int x, int y, int c, int fontstyle, vec4_t color );
void trap_DrawString ( int x, int y, char *str, int fontstyle, vec4_t color );
void trap_DrawPropString ( int x, int y, char *str, int fontstyle, vec4_t color );
int	 trap_PropStringLength ( char *str, int fontstyle );
void trap_FillRect ( int x, int y, int w, int h, vec4_t color );

void trap_Vid_GetCurrentInfo ( int *width, int *height );
