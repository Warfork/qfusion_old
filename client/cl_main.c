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
// cl_main.c  -- client main loop

#include "client.h"

cvar_t	*cl_stereo_separation;
cvar_t	*cl_stereo;

cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*cl_timeout;
cvar_t	*cl_maxfps;

cvar_t	*cl_shownet;

cvar_t	*cl_paused;
cvar_t	*cl_timedemo;
cvar_t	*cl_avidemo;

//
// userinfo
//
cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*rate;
cvar_t	*msg;

cvar_t	*cl_masterServer;

client_static_t	cls;
client_state_t	cl;

entity_state_t	cl_baselines[MAX_EDICTS];

entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

void CL_RestartMedia( void );

extern	cvar_t *allow_download;
extern	cvar_t *allow_download_players;
extern	cvar_t *allow_download_models;
extern	cvar_t *allow_download_sounds;
extern	cvar_t *allow_download_maps;

//======================================================================

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
	char	*cmd;

	cmd = Cmd_Argv(0);
	if (cls.state <= ca_connected || *cmd == '-' || *cmd == '+')
	{
		Com_Printf ("Unknown command \"%s\"\n", cmd);
		return;
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, cmd);
	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}

void CL_Setenv_f( void )
{
	int argc = Cmd_Argc();

	if ( argc > 2 )
	{
		char buffer[1000];
		int i;

		strcpy( buffer, Cmd_Argv(1) );
		strcat( buffer, "=" );

		for ( i = 2; i < argc; i++ )
		{
			strcat( buffer, Cmd_Argv( i ) );
			strcat( buffer, " " );
		}

		putenv( buffer );
	}
	else if ( argc == 2 )
	{
		char *env = getenv( Cmd_Argv(1) );

		if ( env )
		{
			Com_Printf( "%s=%s\n", Cmd_Argv(1), env );
		}
		else
		{
			Com_Printf( "%s undefined\n", Cmd_Argv(1), env );
		}
	}
}


/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f (void)
{
	if (cls.state != ca_connected && cls.state != ca_active)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	// don't forward the first argument
	if (Cmd_Argc() > 1)
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}


/*
==================
CL_Pause_f
==================
*/
void CL_Pause_f (void)
{
	// never pause in multiplayer
	if (Cvar_VariableValue ("sv_maxclients") > 1 || !Com_ServerState ())
	{
		Cvar_SetValue ("paused", 0);
		return;
	}

	Cvar_SetValue ("paused", !cl_paused->integer);
}

/*
==================
CL_Quit
==================
*/
void CL_Quit (void)
{
	CL_Disconnect ();
	Com_Quit ();
}

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	CL_Quit ();
}

/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket (void)
{
	netadr_t	adr;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address\n");
		cls.connect_time = 0;
		cls.connect_count = 0;
		return;
	}
	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	cls.quakePort = Cvar_VariableValue ("qport");
	userinfo_modified = qfalse;

	Netchan_OutOfBandPrint (NS_CLIENT, adr, "connect %i %i %i \"%s\"\n",
		PROTOCOL_VERSION, cls.quakePort, cls.challenge, Cvar_Userinfo() );
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend (void)
{
	netadr_t	adr;

	// if the local server is running and we aren't then connect
	if (cls.state == ca_disconnected && Com_ServerState() )
	{
		CL_SetClientState (ca_connecting);
		Q_strncpyz (cls.servername, "localhost", sizeof(cls.servername));
		// we don't need a challenge on the localhost
		CL_SendConnectPacket ();
		return;
	}

	// resend if we haven't gotten a reply yet
	if (cls.state != ca_connecting)
		return;
	if (cls.realtime - cls.connect_time < 3000)
		return;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address\n");
		CL_SetClientState (ca_disconnected);
		return;
	}
	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	cls.connect_count++;
	cls.connect_time = cls.realtime;	// for retransmit requests

	Com_Printf ("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint (NS_CLIENT, adr, "getchallenge\n");
}


/*
================
CL_Connect_f

================
*/
void CL_Connect_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: connect <server>\n");
		return;	
	}

	if (Com_ServerState ())
	{	// if running a local server, kill it and reissue
		SV_Shutdown (va("Server quit\n", msg), qfalse);
	}
	else
	{
		CL_Disconnect ();
	}

	NET_Config( qtrue );		// allow remote

	memset (cl.configstrings, 0, sizeof(cl.configstrings));
	Q_strncpyz (cls.servername, Cmd_Argv (1), sizeof(cls.servername));
	CL_SetClientState (ca_connecting);
	cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
	cls.connect_count = 0;
}


/*
=====================
CL_Rcon_f

Send the rest of the command line over as
an unconnected command.
=====================
*/
void CL_Rcon_f (void)
{
	char	message[1024];
	int		i;
	netadr_t	to;

	if (!rcon_client_password->string)
	{
		Com_Printf ("You must set 'rcon_password' before\n"
					"issuing an rcon command.\n");
		return;
	}

	//r1: buffer check ffs!
	if ((strlen(Cmd_Args()) + strlen(rcon_client_password->string) + 16) >= sizeof(message)) {
		Com_Printf( "Length of password + command exceeds maximum allowed length.\n" );
		return;
	}

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	NET_Config (qtrue);		// allow remote

	strcat (message, "rcon ");

	strcat (message, rcon_client_password->string);
	strcat (message, " ");

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		strcat (message, Cmd_Argv(i));
		strcat (message, " ");
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else
	{
		if (!strlen(rcon_address->string))
		{
			Com_Printf ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
		if (to.port == 0)
			to.port = BigShort (PORT_SERVER);
	}

	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, to);
}

/*
=====================
CL_GetClipboardData
=====================
*/
void CL_GetClipboardData (char *string, int size)
{
	char *cbd;

	if (!string || size <= 0)
		return;

	string[0] = 0;
	cbd = Sys_GetClipboardData ();
	if (cbd && cbd[0])
	{
		Q_strncpyz ( string, cbd, size );
		Q_free ( cbd );
	}
}

/*
=====================
CL_SetKeyDest
=====================
*/
void CL_SetKeyDest (int key_dest)
{
	if (key_dest < key_game || key_dest > key_menu)
		Com_Error (ERR_DROP, "CL_SetKeyDest: invalid key_dest");
	cls.key_dest = key_dest;
}


/*
=====================
CL_SetOldKeyDest
=====================
*/
void CL_SetOldKeyDest (int key_dest)
{
	if (key_dest < key_game || key_dest > key_menu)
		Com_Error (ERR_DROP, "CL_SetKeyDest: invalid key_dest");
	cls.old_key_dest = key_dest;
}

/*
=====================
CL_ResetServerCount
=====================
*/
void CL_ResetServerCount (void)
{
	cl.servercount = -1;
}

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));
	memset (cl_baselines, 0, sizeof(cl_baselines));
	memset (cl_parse_entities, 0, sizeof(cl_parse_entities));

	SZ_Clear (&cls.netchan.message);
}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Com_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
	qbyte	final[32];

	if (cls.state == ca_uninitialized)
		return;
	if (cls.state == ca_disconnected)
		goto done;

	if (cl_timedemo && cl_timedemo->integer)
	{
		int	time;
		
		time = Sys_Milliseconds () - cl.timedemo_start;
		if (time > 0)
			Com_Printf ("%i frames, %3.1f seconds: %3.1f fps\n", cl.timedemo_frames,
			time/1000.0, cl.timedemo_frames*1000.0 / time);
	}

	cls.connect_time = 0;
	cls.connect_count = 0;

	SCR_StopCinematic ();

	if (cls.demorecording)
		CL_Stop_f ();

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	strcpy ((char *)final+1, "disconnect");
	Netchan_Transmit (&cls.netchan, strlen(final), final);
	Netchan_Transmit (&cls.netchan, strlen(final), final);
	Netchan_Transmit (&cls.netchan, strlen(final), final);

	CL_RestartMedia ();

	// stop download
	if (cls.download) {
		fclose (cls.download);
		cls.download = NULL;
	}

	CL_ClearState ();
	CL_SetClientState (ca_disconnected);

done:
	;
	// drop loading plaque unless this is the initial game start
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque ();	// get rid of loading plaque
}

void CL_Disconnect_f (void)
{
	Com_Error (ERR_DROP, "Disconnected from server");
}


/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void CL_Packet_f (void)
{
	char	send[2048];
	int		i, l;
	char	*in, *out;
	netadr_t	adr;

	if (Cmd_Argc() != 3)
	{
		Com_Printf ("packet <destination> <contents>\n");
		return;
	}

	NET_Config (qtrue);		// allow remote

	if (!NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		Com_Printf ("Bad address\n");
		return;
	}
	if (!adr.port)
		adr.port = BigShort (PORT_SERVER);

	in = Cmd_Argv(2);
	out = send+4;
	send[0] = send[1] = send[2] = send[3] = (char)0xff;

	l = strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (NS_CLIENT, out-send, send, adr);
}

/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	memset (cl.configstrings, 0, sizeof(cl.configstrings));
	CL_SetClientState (ca_connected);	// not active anymore, but not disconnected
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	cls.connect_count = 0;

	S_StopAllSounds ();
	if (cls.state == ca_connected) {
		Com_Printf ("reconnecting...\n");
		memset (cl.configstrings, 0, sizeof(cl.configstrings));
		CL_SetClientState (ca_connected);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");		
		return;
	}

	if (*cls.servername) {
		if (cls.state >= ca_connected) {
			CL_Disconnect();
			cls.connect_time = cls.realtime - 1500;
		} else
			cls.connect_time = -99999; // fire immediately
		Com_Printf ("reconnecting...\n");
	}

	CL_SetClientState (ca_connecting);
}

/*
=================
CL_ParseStatusMessage

Handle a reply from a ping
=================
*/
void CL_ParseStatusMessage (void)
{
	char	*s = MSG_ReadString( &net_recieved );

	Com_Printf( "%s", s );

	CL_UIModule_AddToServerList( NET_AdrToString(&net_from), s );
}

/*
=================
CL_ParseGetServersResponse

Handle a reply from getservers message to master server
=================
*/
void CL_ParseGetServersResponse( qbyte *s )
{
	char		adrString[32];
	int			port, len;
	netadr_t	adr;
	char		requestString[32];

	len = strlen( s );
	Q_snprintfz( requestString, sizeof( requestString ), "info %i full empty", PROTOCOL_VERSION );

	while( len && strncmp( s, "EOT", 3 ) ) {
		if( len < 6 )
			break;

		port = s[5] | (s[4] << 8);
		if( port < 1 || port >= 65535 )
			break;

		Q_snprintfz( adrString, sizeof( adrString ), "%u.%u.%u.%u:%i", s[0], s[1], s[2], s[3], port );

		Com_DPrintf( "%s\n", adrString );
		if( !NET_StringToAdr( adrString, &adr ) ) {
			Com_Printf( "Bad address: %s\n", adrString );
			continue;
		}
		Netchan_OutOfBandPrint( NS_CLIENT, adr, requestString );

		if( s[6] != '\\' )
			break;
		s += 7;
		len -= 7;
	}
}

/*
=================
CL_PingServers_f
=================
*/
void CL_PingServers_f( void )
{
	netadr_t	adr;
	char		*requeststring;
	cvar_t		*noudp;
	char		gameName[MAX_QPATH];

	NET_Config( qtrue );		// allow remote

	noudp = Cvar_Get( "noudp", "0", CVAR_NOSET );

	if( !strcmp( Cmd_Argv(1), "local" ) ) {
		// send a broadcast packet
		Com_Printf( "pinging broadcast...\n" );

		requeststring = va( "info %i %s %s", PROTOCOL_VERSION, Cmd_Argv(2), Cmd_Argv(3) );

		if( !noudp->integer ) {
			adr.type = NA_BROADCAST;
			adr.port = BigShort( PORT_SERVER );
			Netchan_OutOfBandPrint( NS_CLIENT, adr, requeststring );
		}
	} else if( !noudp->integer ) {
		// send a broadcast packet
		Com_Printf( "quering %s...\n", cl_masterServer->string );

		Q_strncpyz( gameName, Cvar_VariableString( "cl_gameName" ), sizeof( gameName ) );
		if( !gameName[0] ) {
			Q_strncpyz( gameName, APPLICATION, sizeof( gameName ) );
			Q_strlwr( gameName );
		}

		requeststring = va( "getservers %s %i %s %s", gameName, PROTOCOL_VERSION, Cmd_Argv(2), Cmd_Argv(3) );

		if( NET_StringToAdr( cl_masterServer->string, &adr ) ) {
			if( !adr.port )
				adr.port = BigShort( PORT_MASTER );
			Netchan_OutOfBandPrint( NS_CLIENT, adr, requeststring );
		}
		else
		{
			Com_Printf( "Bad address: %s\n", cl_masterServer->string );
		}
	}
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket (void)
{
	char	*s;
	char	*c;
	
	MSG_BeginReading (&net_recieved);
	MSG_ReadLong (&net_recieved);	// skip the -1

	s = MSG_ReadStringLine (&net_recieved);

	if (!strncmp(s, "getserversResponse\\", 19))
	{
		Com_Printf ("%s: %s\n", NET_AdrToString (&net_from), "getserversResponse");
		CL_ParseGetServersResponse (s + 19);
		return;
	}

	Cmd_TokenizeString (s, qfalse);
	c = Cmd_Argv(0);

	Com_Printf ("%s: %s\n", NET_AdrToString (&net_from), s);

	// server connection
	if (!strcmp(c, "client_connect"))
	{
		if (cls.state == ca_connected)
		{
			Com_Printf ("Dup connect received.  Ignored.\n");
			return;
		}
		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.quakePort);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		memset (cl.configstrings, 0, sizeof(cl.configstrings));
		CL_SetClientState (ca_connected);
		return;
	}

	// server responding to a status broadcast
	if (!strcmp(c, "info"))
	{
		CL_ParseStatusMessage ();
		return;
	}

	// remote command from gui front end
	if (!strcmp(c, "cmd"))
	{
		if (!NET_IsLocalAddress(&net_from))
		{
			Com_Printf ("Command packet from remote host.  Ignored.\n");
			return;
		}
		Sys_AppActivate ();
		s = MSG_ReadString (&net_recieved);
		Cbuf_AddText (s);
		Cbuf_AddText ("\n");
		return;
	}
	// print command from somewhere
	if (!strcmp(c, "print"))
	{
		s = MSG_ReadString (&net_recieved);
		Com_Printf ("%s", s);
		return;
	}

	// ping from somewhere
	if (!strcmp(c, "ping"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "ack");
		return;
	}

	// challenge from the server we are connecting to
	if (!strcmp(c, "challenge"))
	{
		cls.challenge = atoi(Cmd_Argv(1));
		//wsw : r1q2[start]
		//r1: reset the timer so we don't send dup. getchallenges
		cls.connect_time = Sys_Milliseconds ();
		//wsw : r1q2[end]
		CL_SendConnectPacket ();
		return;
	}

	// echo request from server
	if (!strcmp(c, "echo"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "%s", Cmd_Argv(1) );
		return;
	}

	Com_Printf ("Unknown command.\n");
}


/*
=================
CL_DumpPackets

A vain attempt to help bad TCP stacks that cause problems
when they overflow
=================
*/
void CL_DumpPackets (void)
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_recieved))
	{
		Com_Printf ("dumping a packet\n");
	}
}

/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_recieved))
	{
		//
		// remote command packet
		//
		if (*(int *)net_recieved.data == -1)
		{
			CL_ConnectionlessPacket ();
			continue;
		}

		if (cls.state == ca_disconnected || cls.state == ca_connecting)
			continue;		// dump it if not connected

		if (net_recieved.cursize < 8)
		{
			//wsw : r1q2[start]
			//r1: delegated to DPrintf (someone could spam your console with crap otherwise)
			Com_DPrintf ("%s: Runt packet\n", NET_AdrToString(&net_from));
			//wsw : r1q2[end]
			continue;
		}

		//
		// packet from server
		//
		if (!NET_CompareAdr (&net_from, &cls.netchan.remote_address))
		{
			Com_DPrintf ("%s:sequenced packet without connection\n"
				,NET_AdrToString(&net_from));
			continue;
		}
		if (!Netchan_Process(&cls.netchan, &net_recieved, &net_message))
			continue;		// wasn't accepted for some reason
		CL_ParseServerMessage ();
	}

	//
	// check timeout
	//
	if (cls.state >= ca_connected
	 && cls.realtime - cls.netchan.last_received > cl_timeout->value*1000)
	{
		if (++cl.timeoutcount > 5)	// timeoutcount saves debugger
		{
			Com_Printf ("\nServer connection timed out.\n");
			CL_Disconnect ();
			return;
		}
	}
	else
		cl.timeoutcount = 0;
	
}


//=============================================================================

/*
==============
CL_Userinfo_f
==============
*/
void CL_Userinfo_f (void)
{
	Com_Printf ("User info settings:\n");
	Info_Print (Cvar_Userinfo());
}

int precache_check; // for autodownload of precache items
int precache_spawncount;
int precache_tex;

#define PLAYER_MULT 5

// ENV_CNT is map load
#define ENV_CNT (CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT+1)

void CL_RequestNextDownload (void)
{
	char fn[MAX_OSPATH];

	if (cls.state != ca_connected)
		return;

	if (!allow_download->integer && precache_check < ENV_CNT)
		precache_check = ENV_CNT;

//ZOID
	if (precache_check == CS_MODELS) { // confirm map
		precache_check = CS_MODELS+2; // 0 isn't used
		if (allow_download_maps->integer)
			if (!CL_CheckOrDownloadFile(cl.configstrings[CS_MODELS+1]))
				return; // started a download
	}
	if (precache_check >= CS_MODELS && precache_check < CS_MODELS+MAX_MODELS) {
		if (allow_download_models->integer) {
			while (precache_check < CS_MODELS+MAX_MODELS &&
				cl.configstrings[precache_check][0]) {
				if (cl.configstrings[precache_check][0] == '*' ||
					cl.configstrings[precache_check][0] == '#') {
					precache_check++;
					continue;
				}

				if (!CL_CheckOrDownloadFile(cl.configstrings[precache_check++])) {
					return; // started a download
				}
			}
		}
		precache_check = CS_SOUNDS;
	}

	if (precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS+MAX_SOUNDS) { 
		if (allow_download_sounds->integer) {
			if (precache_check == CS_SOUNDS)
				precache_check++; // zero is blank
			while (precache_check < CS_SOUNDS+MAX_SOUNDS &&
				cl.configstrings[precache_check][0]) {
				if (cl.configstrings[precache_check][0] == '*') {
					precache_check++;
					continue;
				}

				Q_strncpyz(fn, cl.configstrings[precache_check++], sizeof(fn));
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = CS_IMAGES;
	}
	if (precache_check >= CS_IMAGES && precache_check < CS_IMAGES+MAX_IMAGES) {
		if (precache_check == CS_IMAGES)
			precache_check++; // zero is blank
		precache_check = CS_PLAYERSKINS;
	}
	// skins are special, since a player has three things to download:
	// model, weapon model and skin
	// so precache_check is now *3
	if (precache_check >= CS_PLAYERSKINS && precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT) {
		if (allow_download_players->integer) {
			while (precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT) {
				int i, n;
#if 0
				char model[MAX_QPATH], skin[MAX_QPATH], *p;
#endif

				i = (precache_check - CS_PLAYERSKINS)/PLAYER_MULT;
				n = (precache_check - CS_PLAYERSKINS)%PLAYER_MULT;

				// Vic: disabled for now
#if 1
				precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
#else
				if (!cl.configstrings[CS_PLAYERSKINS+i][0]) {
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr(cl.configstrings[CS_PLAYERSKINS+i], '\\')) != NULL)
				{
					p++;
					strcpy(model, p);
					if ((p = strchr(model, '\\')) != NULL)
						p++;
				}
				else
					p = cl.configstrings[CS_PLAYERSKINS+i];

				strcpy(model, p);
				p = strchr(model, '/');
				if (!p)
					p = strchr(model, '\\');
				if (p) {
					*p++ = 0;
					strcpy(skin, p);
				} else
					*skin = 0;

				switch (n) {
				case 0: // model
					Q_snprintfz(fn, sizeof(fn), "players/%s/tris.md2", model);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 1: // weapon model
					Q_snprintfz(fn, sizeof(fn), "players/%s/weapon.md2", model);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 2: // weapon skin
					Q_snprintfz(fn, sizeof(fn), "players/%s/weapon.pcx", model);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 3;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 3: // skin
					Q_snprintfz(fn, sizeof(fn), "players/%s/%s.pcx", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 4;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 4: // skin_i
					Q_snprintfz(fn, sizeof(fn), "players/%s/%s_i.pcx", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 5;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				}
#endif
			}
		}
		// precache phase completed
		precache_check = ENV_CNT;
	}

	if (precache_check == ENV_CNT) {
		unsigned map_checksum;

		// check memory integrity
		Mem_CheckSentinelsGlobal ();

		CM_LoadMap (cl.configstrings[CS_MODELS+1], qtrue, &map_checksum);

		// check memory integrity
		Mem_CheckSentinelsGlobal ();

		if (map_checksum != atoi(cl.configstrings[CS_MAPCHECKSUM])) {
			Com_Error (ERR_DROP, "Local map version differs from server: %i != '%s'",
				map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		}

		precache_check = TEXTURE_CNT;
	}

	if (precache_check == TEXTURE_CNT) {
		precache_check = TEXTURE_CNT+1;
		precache_tex = 0;
	}

	// confirm existance of textures, download any that don't exist
	if (precache_check == TEXTURE_CNT+1) {
		precache_check = TEXTURE_CNT+999;
	}

//ZOID

	// load client game module
	CL_GameModule_Init ();

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("begin %i\n", precache_spawncount) );
}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
void CL_Precache_f (void)
{
	if (Cmd_Argc() < 2)
	{	// demo playback
		unsigned map_checksum;

		// check memory integrity
		Mem_CheckSentinelsGlobal ();

		CM_LoadMap (cl.configstrings[CS_MODELS+1], qtrue, &map_checksum);

		// check memory integrity
		Mem_CheckSentinelsGlobal ();

		CL_GameModule_Init ();

		return;
	}

	precache_check = CS_MODELS;
	precache_spawncount = atoi(Cmd_Argv(1));

	CL_RequestNextDownload();
}

/*
===============
CL_WriteConfiguration

Writes key bindings and archived cvars to a config file
===============
*/
void CL_WriteConfiguration (char *name)
{
	FILE	*f;
	char	path[MAX_QPATH];

	Q_snprintfz (path, sizeof(path),"%s/%s", FS_Gamedir(), name);
	f = fopen (path, "wt");
	if (!f)
	{
		Com_Printf ("Couldn't write %s.\n", name);
		return;
	}

	fprintf (f, "// generated by quake, do not modify\n");

	fprintf (f, "\n// key bindings\n");
	Key_WriteBindings (f);

	fprintf (f, "\n// variables\n");
	Cvar_WriteVariables (f);

	fprintf (f, "\n// aliases\n");
	Cmd_WriteAliases (f);

	fclose (f);
}


/*
===============
CL_WriteConfig_f
===============
*/
void CL_WriteConfig_f (void)
{
	char	name[MAX_QPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf ("usage: writeconfig <filename>\n");
		return;
	}

	Q_strncpyz (name, Cmd_Argv(1), sizeof(name)-4);
	COM_DefaultExtension (name, ".cfg");

	Com_Printf ("Writing %s\n", name);

	CL_WriteConfiguration (name);
}


/*
=================
CL_SetClientState
=================
*/
void CL_SetClientState( int state )
{
	cls.state = state;
	Com_SetClientState( state );

	switch( state ) {
		case ca_disconnected:
			Con_Close ();
			CL_UIModule_MenuMain ();
			CL_SetKeyDest( key_menu );
//			SCR_UpdateScreen ();
			break;
		case ca_connecting:
			Con_Close ();
			CL_SetKeyDest( key_game );
			SCR_EndLoadingPlaque ();
//			SCR_UpdateScreen ();
			break;
		case ca_connected:
			Con_Close ();
			Cvar_FixCheatVars ();
//			SCR_UpdateScreen ();
			break;
		case ca_active:
			Con_Close ();
			CL_SetKeyDest( key_game );
//			SCR_UpdateScreen ();
			break;
		default:
			break;
	}
}

/*
=================
CL_InitMedia
=================
*/
void CL_InitMedia( void )
{
	if( cls.mediaInitialized )
		return;
	if( cls.state == ca_uninitialized )
		return;

	cls.mediaInitialized = qtrue;

	// restart renderer
	R_Restart ();

	// free all sounds
	S_FreeSounds ();

	// register console font and background
	SCR_RegisterConsoleMedia ();

	// load user interface
	CL_UIModule_Init ();

	// check memory integrity
	Mem_CheckSentinelsGlobal ();

	S_SoundsInMemory ();
}

/*
=================
CL_ShutdownMedia
=================
*/
void CL_ShutdownMedia( void )
{
	if( !cls.mediaInitialized )
		return;

	cls.mediaInitialized = qfalse;

	// shutdown cgame
	CL_GameModule_Shutdown ();

	// shutdown user interface
	CL_UIModule_Shutdown ();

	// stop and free all sounds
	S_StopAllSounds ();
	S_FreeSounds ();
}

/*
=================
CL_RestartMedia
=================
*/
void CL_RestartMedia( void )
{
	CL_ShutdownMedia ();
	CL_InitMedia ();
}

/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal (void)
{
	cls.state = ca_disconnected;
	Com_SetClientState( ca_disconnected );
	cls.realtime = Sys_Milliseconds ();

	CL_InitInput ();

//
// register our variables
//
	cl_stereo_separation = Cvar_Get( "cl_stereo_separation", "0.4", CVAR_ARCHIVE );
	cl_stereo = Cvar_Get( "cl_stereo", "0", 0 );

	cl_maxfps = Cvar_Get ("cl_maxfps", "90", 0);

	cl_upspeed = Cvar_Get ("cl_upspeed", "200", 0);
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", 0);
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "200", 0);

	cl_masterServer = Cvar_Get ("cl_masterServer", va("127.10.0.1:%i", PORT_MASTER), CVAR_ARCHIVE);

	cl_shownet = Cvar_Get ("cl_shownet", "0", 0);
	cl_timeout = Cvar_Get ("cl_timeout", "120", 0);
	cl_paused = Cvar_Get ("paused", "0", 0);
	cl_timedemo = Cvar_Get ("timedemo", "0", 0);
	cl_avidemo = Cvar_Get ("cl_avidemo", "0", 0);

	rcon_client_password = Cvar_Get ("rcon_password", "", 0);
	rcon_address = Cvar_Get ("rcon_address", "", 0);

	//
	// userinfo
	//
	info_password = Cvar_Get ("password", "", CVAR_USERINFO);
	info_spectator = Cvar_Get ("spectator", "0", CVAR_USERINFO);
	rate = Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE);	// FIXME
	msg = Cvar_Get ("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE);

	Cvar_Get ("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE);
	Cvar_Get ("skin", "", CVAR_USERINFO | CVAR_ARCHIVE);
	Cvar_Get ("gender", "male", CVAR_USERINFO | CVAR_ARCHIVE);
	Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	Cvar_Get ("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);

	//
	// register our commands
	//
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("pause", CL_Pause_f);
	Cmd_AddCommand ("pingservers", CL_PingServers_f);

	Cmd_AddCommand ("userinfo", CL_Userinfo_f);

	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);

	Cmd_AddCommand ("quit", CL_Quit_f);

	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddCommand ("rcon", CL_Rcon_f);

	Cmd_AddCommand ("setenv", CL_Setenv_f );

	Cmd_AddCommand ("download", CL_Download_f);

	Cmd_AddCommand ("writeconfig", CL_WriteConfig_f);

	//
	// forward to server commands
	//
	// the only thing this does is allow command completion
	// to work -- all unknown commands are automatically
	// forwarded to the server
	Cmd_AddCommand ("wave", NULL);
	Cmd_AddCommand ("inven", NULL);
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("use", NULL);
	Cmd_AddCommand ("weapon", NULL);
	Cmd_AddCommand ("drop", NULL);
	Cmd_AddCommand ("say", NULL);
	Cmd_AddCommand ("say_team", NULL);
	Cmd_AddCommand ("info", NULL);
	Cmd_AddCommand ("prog", NULL);
	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("god", NULL);
	Cmd_AddCommand ("notarget", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("invuse", NULL);
	Cmd_AddCommand ("invprev", NULL);
	Cmd_AddCommand ("invnext", NULL);
	Cmd_AddCommand ("invdrop", NULL);
	Cmd_AddCommand ("weapnext", NULL);
	Cmd_AddCommand ("weapprev", NULL);
}


/*
==================
CL_FixCvarCheats

==================
*/

typedef struct
{
	char	*name;
	char	*value;
	cvar_t	*var;
} cheatvar_t;

cheatvar_t	cheatvars[] = {
	{"timescale", "1", NULL},
	{"timedemo", "0", NULL},
	{"paused", "0", NULL},
	{"fixedtime", "0", NULL},
	{NULL, NULL, NULL}
};

int		numcheatvars;

void CL_FixCvarCheats (void)
{
	int			i;
	cheatvar_t	*var;

	if( cl.attractloop )
		return;
	if ( !strcmp(cl.configstrings[CS_MAXCLIENTS], "1") 
		|| !cl.configstrings[CS_MAXCLIENTS][0] )
		return;		// single player can cheat

	// find all the cvars if we haven't done it yet
	if (!numcheatvars)
	{
		while (cheatvars[numcheatvars].name)
		{
			cheatvars[numcheatvars].var = Cvar_Get (cheatvars[numcheatvars].name,
					cheatvars[numcheatvars].value, 0);
			numcheatvars++;
		}
	}

	// make sure they are all set to the proper values
	for (i=0, var = cheatvars ; i<numcheatvars ; i++, var++)
	{
		if ( strcmp (var->var->string, var->value) )
		{
			Cvar_Set (var->name, var->value);
		}
	}
}

//============================================================================

/*
==================
CL_SendCommand
==================
*/
void CL_SendCommand (void)
{
	// get new key events
	Sys_SendKeyEvents ();

	// allow mice or other external controllers to add commands
	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	// fix any cheating cvars
	CL_FixCvarCheats ();

	// send intentions now
	CL_SendCmd ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}

/*
==================
CL_MinFrameFrame
==================
*/
double CL_MinFrameFrame (void)
{
	if (!cl_timedemo->integer)
	{
		if (cls.state == ca_connected)
			return 0.1;			// don't flood packets out while connecting
		if (cl_maxfps->integer)
			return 1.0 / (double)cl_maxfps->integer;
	}

	return 0;
}

/*
==================
CL_Frame
==================
*/
void CL_Frame (int msec)
{
	static double	extratime = 0.001;
	static double	trueframetime;
	double			minframetime;
	static int		lasttimecalled;
	static int		aviframe;

	if (dedicated->integer)
		return;

	extratime += msec * 0.001;

	minframetime = CL_MinFrameFrame ();
	if (extratime < minframetime)
		return;

	// let the mouse activate or deactivate
	IN_Frame ();

	// decide the simulation time
	trueframetime = extratime - 0.001;
	if (trueframetime < minframetime)
		trueframetime = minframetime;
	extratime -= trueframetime;

	cls.frametime = trueframetime;
	cls.trueframetime = trueframetime;
	cls.realtime = curtime;

	if (cls.frametime > (1.0 / 5))
		cls.frametime = (1.0 / 5);

	// if in the debugger last frame, don't timeout
	if (msec > 5000)
		cls.netchan.last_received = Sys_Milliseconds ();

	// fetch results from server
	CL_ReadPackets ();

	// send a new command message to the server
	CL_SendCommand ();

	// allow rendering DLL change
	VID_CheckChanges ();

	// drop to main menu if nothing else
	if (cls.state == ca_disconnected && !cls.uiActive && !Com_ServerState ())
	{
		S_StopAllSounds ();
		CL_UIModule_MenuMain ();
		SCR_EndLoadingPlaque ();
	}

	// update the screen
	if (host_speeds->integer)
		time_before_ref = Sys_Milliseconds ();
	SCR_UpdateScreen ();
	if (host_speeds->integer)
		time_after_ref = Sys_Milliseconds ();

	// update audio
	if (cls.state != ca_active || cl.cin.time > 0)
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	if( cl_avidemo->modified ) {
		aviframe = 0;
		if( cl_avidemo->integer )
			R_BeginAviDemo ();
		else
			R_StopAviDemo ();
		cl_avidemo->modified = qfalse;
	}

	if( cl_avidemo->integer )
		R_WriteAviFrame( aviframe++, cl_avidemo->integer == 2 );

	// advance local effects for next frame
	SCR_RunCinematic ();
	SCR_RunConsole ();

	cls.framecount++;

	if ( log_stats->integer )
	{
		if ( cls.state == ca_active )
		{
			if ( !lasttimecalled )
			{
				lasttimecalled = Sys_Milliseconds();
				if ( log_stats_file )
					fprintf( log_stats_file, "0\n" );
			}
			else
			{
				int now = Sys_Milliseconds();

				if ( log_stats_file )
					fprintf( log_stats_file, "%d\n", now - lasttimecalled );
				lasttimecalled = now;
			}
		}
	}
}


//============================================================================

/*
====================
CL_Init
====================
*/
void CL_Init (void)
{
	if (dedicated->integer)
		return;		// nothing running on the client

	// all archived variables will now be loaded

	Con_Init ();

#ifndef VID_INITFIRST
	S_Init ();	
	VID_Init ();
#else
	VID_Init ();
	S_Init ();	// sound must be initialized after window is created
#endif

	SZ_Init (&net_recieved, net_recieved_buffer, sizeof(net_recieved_buffer));
	SZ_Init (&net_message, net_message_buffer, sizeof(net_message_buffer));

	RoQ_Init ();

	SCR_InitScreen ();
	cls.disable_screen = qtrue;	// don't draw yet

	CL_InitLocal ();
	IN_Init ();

//	Cbuf_AddText ("exec autoexec.cfg\n");
	FS_ExecAutoexec ();
	Cbuf_Execute ();

	CL_InitMedia ();
//	CL_UIModule_MenuMain ();
}


/*
===============
CL_Shutdown

FIXME: this is a callback from Sys_Quit and Com_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown (void)
{
	static qboolean isdown = qfalse;

	if( cls.state == ca_uninitialized )
		return;
	if( isdown )
		return;

	isdown = qtrue;

	CL_WriteConfiguration( "qfconfig.cfg" );

	CL_UIModule_Shutdown ();
	CL_GameModule_Shutdown ();
	S_Shutdown ();
	IN_Shutdown ();
	VID_Shutdown ();
}
