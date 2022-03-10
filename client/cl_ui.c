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

#include "client.h"
#include "../ui/ui.h"

// Structure containing functions exported from user interface DLL
ui_export_t	*uie;

#define	MAXPRINTMSG	4096
void UI_Printf (int print_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
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
	vsnprintf (msg, sizeof(msg), fmt, argptr);
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

int CL_GetTime (void)
{
	return cls.realtime;
}

void CL_SetKeyDest ( int key_dest )
{
	if (key_dest < key_game || key_dest > key_menu)
		Com_Error (ERR_DROP, "CL_SetKeyDest_f: invalid key_dest");
	cls.key_dest = key_dest;
}

void CL_ResetServerCount ( void )
{
	cl.servercount = -1;
}

char *Key_GetBindingBuf (int binding)
{
	return keybindings[binding];
}

/*
==============
UI_Init
==============
*/
void UI_Init (void)
{
	ui_import_t	ui;

	UI_Shutdown ();

	ui.RenderFrame = R_RenderFrame;
	ui.RegisterModel = R_RegisterModel;
	ui.RegisterPic = R_RegisterPic;
	ui.RegisterSkin = R_RegisterSkin;
	ui.EndFrame = GLimp_EndFrame;

	ui.S_StartLocalSound = S_StartLocalSound;

	ui.CL_Quit = CL_Quit_f;
	ui.CL_GetTime = CL_GetTime;
	ui.CL_SetKeyDest = CL_SetKeyDest;
	ui.CL_ResetServerCount = CL_ResetServerCount;

	ui.Cmd_AddCommand = Cmd_AddCommand;
	ui.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ui.Cmd_ExecuteText = Cbuf_ExecuteText;
	ui.Cmd_Execute = Cbuf_Execute;

	ui.GetClientState = Com_ClientState;
	ui.GetServerState = Com_ServerState;

	ui.Con_Printf = UI_Printf;
	ui.Sys_Error = UI_Error;

	ui.FS_LoadFile = FS_LoadFile;
	ui.FS_FreeFile = FS_FreeFile;
	ui.FS_FileExists = FS_FileExists;
	ui.FS_Gamedir = FS_Gamedir;
	ui.FS_ListFiles = FS_GetFileList;
	ui.FS_NextPath = FS_NextPath;

	ui.Cvar_Get = Cvar_Get;
	ui.Cvar_Set = Cvar_Set;
	ui.Cvar_SetValue = Cvar_SetValue;
	ui.Cvar_ForceSet = Cvar_ForceSet;
	ui.Cvar_VariableString = Cvar_VariableString;
	ui.Cvar_VariableValue = Cvar_VariableValue;

	ui.Key_ClearStates = Key_ClearStates;
	ui.Key_GetBindingBuf = Key_GetBindingBuf;
	ui.Key_KeynumToString = Key_KeynumToString;
	ui.Key_SetBinding = Key_SetBinding;

	ui.DrawStretchPic = Draw_StretchPic;
	ui.DrawChar = Draw_Char;
	ui.DrawString = Draw_String;
	ui.DrawPropString = Draw_PropString;
	ui.PropStringLength = Q_PropStringLength;
	ui.FillRect = Draw_FillRect;

	ui.Vid_GetCurrentInfo = VID_GetCurrentInfo;

	uie = (ui_export_t *) Sys_LoadLibrary (LIB_UI, &ui);

	if (!uie)
		Com_Error (ERR_DROP, "Failed to load UI dll");

	if (uie->api_version != UI_API_VERSION)
		Com_Error (ERR_FATAL, "ui version is %i, not %i", uie->api_version,
			UI_API_VERSION);

	uie->Init ();
}

void UI_Shutdown ( void )
{
	if ( !uie )
		return;

	uie->Shutdown ();
	Sys_UnloadLibrary (LIB_UI);
	uie = NULL;
}

void UI_Refresh ( int frametime )
{
	if ( !uie )
		return;

	// repaint everything next frame
	SCR_DirtyScreen ();

	// dim everything behind it down
	if (cl.cin.time > 0)
		Draw_FillRect (0, 0, viddef.width, viddef.height, colorBlack);

	uie->Refresh ( frametime );
}

void UI_Update (void)
{
	if ( !uie )
		return;

	uie->Update ();
}

void UI_Keydown (int key)
{
	if ( !uie )
		return;

	uie->Keydown (key);
}

void UI_Menu_Main_f (void)
{
	if ( !uie )
		return;

	uie->MainMenu ();
}

void UI_ForceMenuOff (void)
{
	if ( !uie )
		return;

	uie->ForceMenuOff ();
}

void UI_AddToServerList ( char *adr, char *info )
{
	if ( !uie )
		return;

	uie->AddToServerList ( adr, info );
}

void UI_MouseMove ( int dx, int dy )
{
	if ( !uie )
		return;

	uie->MouseMove ( dx, dy );
}


