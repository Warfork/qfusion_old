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

#include "..\client\client.h"
#include "..\client\ref.h"
#include "winquake.h"

int UI_FS_ListFiles( char *path, char *ext, char *buf, int bufsize )
{
	return FS_GetFileList ( path, ext, buf, bufsize );
}

// Structure containing functions exported from user interface DLL
uiexport_t	uie;

HINSTANCE	ui_library;		// Handle to refresh DLL 
qboolean	ui_active = false;

#define	MAXPRINTMSG	4096
void UI_Printf (int print_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean	inupdate;
	
	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	if (print_level == PRINT_ALL)
	{
		Com_Printf ("%s", msg);
	}
	else if ( print_level == PRINT_DEVELOPER )
	{
		Com_DPrintf ("%s", msg);
	}
}

void UI_Error (int err_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	
	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	Com_Error (err_level, "%s", msg);
}

void VID_GetCurrentInfo (int *width, int *height)
{
	if (width)
		*width = viddef.width;
	if (height)
		*height = viddef.height;
}

qboolean UI_ConFunc (void)
{
	/*
	** the proper way to do this is probably to have ToggleConsole_f accept a parameter
	*/
	extern void Key_ClearTyping( void );

	if ( cl.attractloop )
	{
		Cbuf_AddText ("killserver\n");
		return false;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();
	return true;
}

void UI_RenderFrame ( refdef_t *fd )
{
	R_RenderFrame ( fd );
}

struct model_s *UI_RegisterModel ( char *name )
{
	return R_RegisterModel ( name );
}

struct shader_s *UI_RegisterSkin (char *name)
{
	return R_RegisterShaderNoMip ( name );
}

struct shader_s *UI_RegisterPic (char *name)
{
	return R_RegisterShaderNoMip ( name );
}

void UI_EndFrame (void)
{
	GLimp_EndFrame ();
}

float CL_GetTime_f (void)
{
	return cls.realtime;
}

void CL_SetKeyDest_f ( enum keydest_t keydest )
{
	cls.key_dest = keydest;
}

void CL_ResetServerCount_f ( void )
{
	cl.servercount = -1;
}

void UI_DrawPic ( int x, int y, char *name )
{
	Draw_Pic ( x, y, name );
}

void UI_DrawChar ( int x, int y, int c, fontstyle_t fntstl, vec4_t colour )
{
	Draw_Char ( x, y, c, fntstl, colour );
}

void UI_DrawStringLen ( int x, int y, char *str, int len, fontstyle_t fntstl, vec4_t colour )
{
	Draw_StringLen ( x, y, str, len, fntstl, colour );
}

void UI_DrawFill ( int x, int y, int w, int h, int c )
{
	Draw_Fill ( x, y, w, h, c );
}

char *Key_GetBindingBuf (int binding)
{
	return keybindings[binding];
}

int UI_ModelNumFrames ( struct model_s *model )
{
	return R_ModelNumFrames ( model );
}

/*
==============
UI_Load
==============
*/
qboolean UI_Load( char *name )
{
	uiimport_t	ui;
	GetUiAPI_t	GetUiAPI;
	char ui_path[MAX_OSPATH];

	Com_Printf( "------- Loading %s -------\n", name );

	if ( ui_library )
	{
		uie.Shutdown ();
		FreeLibrary( ui_library );
	}

	ui_active = false;
	ui_library = NULL;

	Com_sprintf (ui_path, sizeof(ui_path), "%s/%s", FS_Gamedir(), name );

	if ( ( ui_library = LoadLibrary( ui_path ) ) == 0 )
	{
		Com_Error( ERR_FATAL, "LoadLibrary(\"%s\") failed\n", ui_path );
		return false;
	}

	ui.RenderFrame = UI_RenderFrame;
	ui.RegisterModel = UI_RegisterModel;
	ui.RegisterPic = UI_RegisterPic;
	ui.RegisterSkin = UI_RegisterSkin;
	ui.ModelNumFrames = UI_ModelNumFrames;
	ui.EndFrame = UI_EndFrame;

	ui.S_StartLocalSound = S_StartLocalSound;

	ui.CL_Snd_Restart_f = CL_Snd_Restart_f;
	ui.CL_PingServers_f = CL_PingServers_f;
	ui.CL_Quit_f = CL_Quit_f;
	ui.CL_GetTime_f = CL_GetTime_f;
	ui.CL_SetKeyDest_f = CL_SetKeyDest_f;
	ui.CL_ResetServerCount_f = CL_ResetServerCount_f;

	ui.Cmd_AddCommand = Cmd_AddCommand;
	ui.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ui.Cmd_ExecuteText = Cbuf_ExecuteText;
	ui.Cmd_Execute = Cbuf_Execute;

	ui.Com_ServerState = Com_ServerState;

	ui.Con_Printf = UI_Printf;
	ui.Con_Func = UI_ConFunc;
	ui.Sys_Error = UI_Error;

	ui.FS_LoadFile = FS_LoadFile;
	ui.FS_FreeFile = FS_FreeFile;
	ui.FS_Gamedir = FS_Gamedir;
	ui.FS_ListFiles = UI_FS_ListFiles;
	ui.FS_NextPath = FS_NextPath;

	ui.Cvar_Get = Cvar_Get;
	ui.Cvar_Set = Cvar_Set;
	ui.Cvar_SetValue = Cvar_SetValue;
	ui.Cvar_ForceSet = Cvar_ForceSet;
	ui.Cvar_VariableString = Cvar_VariableString;
	ui.Cvar_VariableValue = Cvar_VariableValue;

	ui.NET_AdrToString = NET_AdrToString;

	ui.Key_ClearStates = Key_ClearStates;
	ui.Key_GetBindingBuf = Key_GetBindingBuf;
	ui.Key_KeynumToString = Key_KeynumToString;
	ui.Key_SetBinding = Key_SetBinding;

	ui.DrawPic = UI_DrawPic;
	ui.DrawChar = UI_DrawChar;
	ui.DrawStringLen = UI_DrawStringLen;
	ui.DrawFill = UI_DrawFill;

	ui.Vid_GetCurrentInfo = VID_GetCurrentInfo;

	if ( ( GetUiAPI = (void *) GetProcAddress( ui_library, "GetUiAPI" ) ) == 0 )
		Com_Error( ERR_FATAL, "GetProcAddress failed on %s", name );

	uie = GetUiAPI( ui );

	if (uie.api_version != UI_API_VERSION)
		Com_Error (ERR_FATAL, "%s has incompatible api_version", name);

	Com_Printf( "------------------------------------\n");

	return (ui_active = true);
}

void UI_Init (void)
{
	if ( !UI_Load( "ui_x86.dll" ) )
		return;

	uie.Init ();
}

void UI_Draw ( void )
{
	if (cls.key_dest != key_menu)
		return;

	// repaint everything next frame
	SCR_DirtyScreen ();

	// dim everything behind it down
	if (cl.cinematictime > 0)
		Draw_Fill (0, 0, viddef.width, viddef.height, 0);
	else
		Draw_FadeScreen ();

	uie.Draw ();
}

void UI_Keydown (int key)
{
	uie.Keydown (key);
}

void UI_Menu_Main_f (void)
{
	uie.MainMenu ();
}

void UI_ForceMenuOff (void)
{
	uie.ForceMenuOff ();
}

void UI_AddToServerList ( netadr_t *adr, char *info )
{
	uie.AddToServerList ( adr, info );
}

