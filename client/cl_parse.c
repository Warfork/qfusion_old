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
// cl_parse.c  -- parse a message received from the server

#include "client.h"

char *svc_strings[256] =
{
	"svc_bad",

	"svc_layout",
	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_servercmd",
	"svc_sound",
	"svc_stufftext",
	"svc_serverdata",
	"svc_spawnbaseline",	
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_frame",
	"svc_stringcmd"
};

//=============================================================================

void CL_DownloadFileName(char *dest, int destlen, char *fn)
{
	if (strncmp(fn, "players", 7) == 0)
		Q_snprintfz (dest, destlen, "%s/%s", BASEDIRNAME, fn);
	else
		Q_snprintfz (dest, destlen, "%s/%s", FS_Gamedir(), fn);
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean CL_CheckOrDownloadFile (char *filename)
{
	FILE *fp;
	char	name[MAX_OSPATH];

	if (strstr (filename, ".."))
	{
//		Com_Printf ("Refusing to download a path with ..\n");
		Com_Printf ("Refusing to download a path with ..: %s\n", filename);
		return qtrue;
	}

	if ( FS_FOpenFile ( filename, NULL, FS_READ ) != -1 )
	{	// it exists, no need to download
		return qtrue;
	}

	strcpy (cls.downloadname, filename);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

//ZOID
	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

//	FS_CreatePath (name);

	fp = fopen (name, "r+b");
	if (fp) { // it exists
		int len;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		cls.download = fp;

		// give the server an offset to start the download
		Com_Printf ("Resuming %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
			va("download %s %i", cls.downloadname, len));
	} else {
		Com_Printf ("Downloading %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
			va("download %s", cls.downloadname));
	}

	cls.downloadnumber++;

	return qfalse;
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void CL_Download_f (void)
{
	char filename[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: download <filename>\n");
		return;
	}

	Q_snprintfz(filename, sizeof(filename), "%s", Cmd_Argv(1));

	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to download a path with ..: %s\n", filename);
		return;
	}

	if ( FS_FOpenFile (filename, NULL, FS_READ) != -1 )
	{	// it exists, no need to download
		Com_Printf("File already exists.\n");
		return;
	}

	strcpy (cls.downloadname, filename);
	Com_Printf ("Downloading %s\n", cls.downloadname);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message,
		va("download %s", cls.downloadname));

	cls.downloadnumber++;
}



/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload (void)
{
	int		size, percent;
	char	name[MAX_OSPATH];
	int		r;

	// read the data
	size = MSG_ReadShort (&net_message);
	percent = MSG_ReadByte (&net_message);
	if (size == -1)
	{
		Com_Printf ("Server does not have this file.\n");
		if (cls.download)
		{
			// if here, we tried to resume a file but the server said no
			fclose (cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download)
	{
		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

		FS_CreatePath (name);

		cls.download = fopen (name, "wb");
		if (!cls.download)
		{
			net_message.readcount += size;
			Com_Printf ("Failed to open %s\n", cls.downloadtempname);
			CL_RequestNextDownload ();
			return;
		}
	}

	fwrite (net_message.data + net_message.readcount, 1, size, cls.download);
	net_message.readcount += size;

	if (percent != 100)
	{
		// request next block
// change display routines by zoid
		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	}
	else
	{
		char	oldn[MAX_OSPATH];
		char	newn[MAX_OSPATH];

		fclose (cls.download);

		// rename the temp file to its final name
		CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
		CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);
		r = FS_RenameFile (oldn, newn);
		if (r)
			Com_Printf ("failed to rename.\n");

		cls.download = NULL;
		cls.downloadpercent = 0;

		// get another file if needed

		CL_RequestNextDownload ();
	}
}


/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData (void)
{
	extern cvar_t	*fs_gamedirvar;
	char	*str;
	int		i;

	Com_DPrintf ("Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();
	CL_SetClientState (ca_connected);

// parse protocol version number
	i = MSG_ReadLong (&net_message);

	if (i != PROTOCOL_VERSION)
		Com_Error (ERR_DROP, "Server returned version %i, not %i", i, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong (&net_message);
	cl.attractloop = MSG_ReadByte (&net_message);

	// game directory
	str = MSG_ReadString (&net_message);
	Q_strncpyz (cl.gamedir, str, sizeof(cl.gamedir));

	// set gamedir
	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str))) || (!*str && (fs_gamedirvar->string && *fs_gamedirvar->string))) {
		Cvar_Set ("fs_game", str);
	}

	// parse player entity number
	cl.playernum = MSG_ReadShort (&net_message);

	// get the full level name
	Q_strncpyz (cl.servermessage, MSG_ReadString (&net_message), sizeof(cl.servermessage));

	CL_RestartMedia ();

	if (cl.playernum == -1)
	{	// playing a cinematic or showing a pic, not a level
		SCR_PlayCinematic (cl.servermessage);
	}
	else
	{
		// separate the printfs so the server message can have a color
		Com_Printf ("\n%s\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n", S_COLOR_RED);
		Com_Printf ("%s%s\n\n", S_COLOR_WHITE, cl.servermessage);
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (void)
{
	entity_state_t	*es;
	unsigned		bits;
	int				newnum;
	entity_state_t	nullstate;

	memset (&nullstate, 0, sizeof(nullstate));

	newnum = CL_ParseEntityBits (&bits);
	es = &cl_baselines[newnum];
	CL_ParseDelta (&nullstate, es, newnum, bits);
}

/*
==================
CL_ParseServerCommand
==================
*/
void CL_ParseServerCommand (void)
{
	char *s, *text;

	text = MSG_ReadString (&net_message);
	Cmd_TokenizeString (text, qfalse);
	s = Cmd_Argv (0);

	if (!s || !s[0])
		return;
	if (!strcmp (s, "cs"))
	{
		int		i = atoi (Cmd_Argv(1));
		char	*s = Cmd_Argv (2);

		if (i < 0 || i >= MAX_CONFIGSTRINGS)
			Com_Error (ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
		Q_strncpyz (cl.configstrings[i], s, sizeof(cl.configstrings[i]));
	}

	CL_GameModule_ServerCommand ();
}

/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket (void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    float 	volume;
    float 	attenuation;  
	int		flags;

	flags = MSG_ReadByte (&net_message);
	sound_num = MSG_ReadByte (&net_message);

	// entity relative
	channel = MSG_ReadShort(&net_message); 
	ent = channel>>3;
	if (ent > MAX_EDICTS)
		Com_Error (ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);
	channel &= 7;

	// positioned in space
	if ((flags & (SND_POS0_8|SND_POS0_16)) == SND_POS0_8)
		pos[0] = MSG_ReadChar (&net_message);
	else if ((flags & (SND_POS0_8|SND_POS0_16)) == SND_POS0_16)
		pos[0] = MSG_ReadShort (&net_message);
	else
		pos[0] = MSG_ReadInt3 (&net_message);

	if ((flags & (SND_POS1_8|SND_POS1_16)) == SND_POS1_8)
		pos[1] = MSG_ReadChar (&net_message);
	else if ((flags & (SND_POS1_8|SND_POS1_16)) == SND_POS1_16)
		pos[1] = MSG_ReadShort (&net_message);
	else
		pos[1] = MSG_ReadInt3 (&net_message);

	if ((flags & (SND_POS2_8|SND_POS2_16)) == SND_POS2_8)
		pos[2] = MSG_ReadChar (&net_message);
	else if ((flags & (SND_POS2_8|SND_POS2_16)) == SND_POS2_16)
		pos[2] = MSG_ReadShort (&net_message);
	else
		pos[2] = MSG_ReadInt3 (&net_message);

    if (flags & SND_VOLUME)
		volume = MSG_ReadByte (&net_message) / 255.0;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte (&net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;	

	if (cl.configstrings[CS_SOUNDS+sound_num][0])
		S_StartSound (pos, ent, channel, S_RegisterSound (cl.configstrings[CS_SOUNDS+sound_num]), volume, attenuation, 0.0);
}


void SHOWNET(char *s)
{
	if (cl_shownet->integer>=2)
		Com_Printf ("%3i:%s\n", net_message.readcount-1, s);
}

void CL_Reconnect_f (void);
void CL_Changing_f (void);
void CL_Precache_f (void);
void CL_ForwardToServer_f (void);

typedef struct {
	char	*name;
	void	(*func) (void);
} svcmd_t;

svcmd_t svcmds[] =
{
	{"reconnect", CL_Reconnect_f},
	{"changing", CL_Changing_f},
	{"precache", CL_Precache_f},
	{"cmd", CL_ForwardToServer_f},
	{NULL}
};

/*
==================
CL_ParseStringCmd
==================
*/
void CL_ParseStringCmd (void)
{
	char *text, *s;
	svcmd_t *cmd;

	text = MSG_ReadString (&net_message);
	Cmd_TokenizeString (text, qfalse);
	s = Cmd_Argv (0);

	for (cmd = svcmds; cmd->name; cmd++)
		if (!strcmp (s, cmd->name) ) {
			cmd->func ();
			return;
		}

	Com_Printf ("CL_ParseStringCmd: bad cmd \"%s\"\n", s);
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	char		*s;
	int			cmd;

//
// if recording demos, copy the message out
//
	if (cl_shownet->integer == 1)
		Com_Printf ("%i ",net_message.cursize);
	else if (cl_shownet->integer >= 2)
		Com_Printf ("------------------\n");


//
// parse the message
//
	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cl_shownet->integer>=2)
		{
			if (!svc_strings[cmd])
				Com_Printf ("%3i:BAD CMD %i\n", net_message.readcount-1,cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}

	// other commands
		switch (cmd)
		{
		default:
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Illegible server message");
			break;

		case svc_nop:
//			Com_Printf ("svc_nop\n");
			break;

		case svc_disconnect:
			Com_Error (ERR_DISCONNECT,"Server disconnected");
			break;

		case svc_reconnect:
			Com_Printf ("Server disconnected, reconnecting\n");
			if (cls.download) {
				//ZOID, close download
				fclose (cls.download);
				cls.download = NULL;
			}
			memset (cl.configstrings, 0, sizeof(cl.configstrings));
			CL_SetClientState (ca_connecting);
			cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
			cls.connect_count = 0;
			break;

		case svc_servercmd:
			CL_ParseServerCommand ();
			break;

		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CL_ParseServerData ();
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			break;

		case svc_stufftext:
			s = MSG_ReadString (&net_message);
			Com_DPrintf ("stufftext: %s\n", s);
			Cbuf_AddText (s);
			break;

		case svc_spawnbaseline:
			CL_ParseBaseline ();
			break;

		case svc_download:
			CL_ParseDownload ();
			break;

		case svc_frame:
			CL_ParseFrame ();
			break;

		case svc_layout:
			s = MSG_ReadString (&net_message);
			CL_GameModule_LoadLayout ( s );
			break;

		case svc_playerinfo:
		case svc_packetentities:
			Com_Error (ERR_DROP, "Out of place frame data");
			break;

		case svc_stringcmd:
			CL_ParseStringCmd ();
			break;
		}
	}

	CL_AddNetgraph ();

	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//
	if (cls.demorecording && !cls.demowaiting)
		CL_WriteDemoMessage ();
}
