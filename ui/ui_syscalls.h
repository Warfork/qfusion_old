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

void trap_Error ( char *str );
void trap_Print ( char *str );

void trap_Cmd_AddCommand ( char *name, void(*cmd)(void) );
void trap_Cmd_RemoveCommand ( char *cmd_name );
void trap_Cmd_ExecuteText ( int exec_when, char *text );
void trap_Cmd_Execute (void);

void trap_R_RenderFrame ( refdef_t *fd );
void trap_R_EndFrame (void);
void trap_R_ModelBounds ( struct model_s *mod, vec3_t mins, vec3_t maxs );
struct model_s *trap_R_RegisterModel ( char *name );
struct shader_s *trap_R_RegisterSkin ( char *name );
struct shader_s *trap_R_RegisterPic ( char *name );
struct skinfile_s *trap_R_RegisterSkinFile ( char *name );
qboolean trap_R_LerpAttachment ( orientation_t *orient, struct model_s *mod, int frame, int oldframe, float backlerp, char *name );

void trap_S_StartLocalSound ( char *s );

void trap_CL_Quit (void);
void trap_CL_SetKeyDest ( int key_dest );
void trap_CL_ResetServerCount (void);
void trap_CL_GetClipboardData ( char *string, int size );

char *trap_Key_GetBindingBuf ( int binding );
void trap_Key_ClearStates(void);
char *trap_Key_KeynumToString ( int keynum );
void trap_Key_SetBinding ( int keynum, char *binding );
qboolean trap_Key_IsDown ( int keynum );

void trap_GetConfigString ( int i, char *str, int size );

int trap_FS_LoadFile ( const char *name, void **buf );
void trap_FS_FreeFile ( void *buf );
int trap_FS_FileExists ( const char *path );
int trap_FS_ListFiles ( const char *path, const char *ext, char *buf, int bufsize );
char *trap_FS_Gamedir (void);

cvar_t *trap_Cvar_Get ( char *name, char *value, int flags );
cvar_t *trap_Cvar_Set( char *name, char *value );
void trap_Cvar_SetValue ( char *name, float value );
cvar_t *trap_Cvar_ForceSet ( char *name, char *value );
float trap_Cvar_VariableValue ( char *name );
char *trap_Cvar_VariableString ( char *name );

struct mempool_s *trap_Mem_AllocPool ( const char *name, const char *filename, int fileline );
void *trap_Mem_Alloc ( struct mempool_s *pool, int size, const char *filename, int fileline );
void trap_Mem_Free ( void *data, const char *filename, int fileline );
void trap_Mem_FreePool ( struct mempool_s **pool, const char *filename, int fileline );
void trap_Mem_EmptyPool ( struct mempool_s *pool, const char *filename, int fileline );

void trap_Draw_StretchPic ( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader );
