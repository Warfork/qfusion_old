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

// qcommon.h -- definitions common between client and server, but not game.dll

#include "../game/q_shared.h"
#include "qfiles.h"
#include "cmodel.h"
#include "mdfour.h"

//define	PARANOID			// speed sapping error checking

#ifndef VERSION
# define VERSION		3.07
#endif

#ifndef APPLICATION
# define APPLICATION	"QFusion"
#endif

#define	BASEDIRNAME		"baseq3"

//============================================================================

typedef struct
{
	qboolean	allowoverflow;	// if false, do a Com_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	qbyte		*data;
	int			maxsize;
	int			cursize;
	int			readcount;
} sizebuf_t;

void SZ_Init (sizebuf_t *buf, qbyte *data, int length);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, void *data, int length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

struct usercmd_s;
struct entity_state_s;

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteInt3 (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, char *s);
#define MSG_WriteCoord(sb,f) (MSG_WriteInt3((sb), Q_rint((f*16))))
#define MSG_WritePos(sb,pos) (MSG_WriteCoord((sb),(pos)[0]), MSG_WriteCoord(sb,(pos)[1]), MSG_WriteCoord(sb,(pos)[2]))
#define MSG_WriteAngle(sb,f) (MSG_WriteByte((sb), ANGLE2BYTE((f))))
#define MSG_WriteAngle16(sb,f) (MSG_WriteShort((sb), ANGLE2SHORT((f))))
void MSG_WriteDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd);
void MSG_WriteDeltaEntity (struct entity_state_s *from, struct entity_state_s *to, sizebuf_t *msg, qboolean force, qboolean newentity);
void MSG_WriteDir (sizebuf_t *sb, vec3_t vector);


void MSG_BeginReading (sizebuf_t *sb);

// returns -1 if no more characters are available
#define MSG_ReadChar(sb) (((sb)->readcount+1 > (sb)->cursize) ? -1 : (signed char)(sb)->data[(sb)->readcount++])
#define MSG_ReadByte(sb) (((sb)->readcount+1 > (sb)->cursize) ? -1 : (unsigned char)(sb)->data[(sb)->readcount++])
int	MSG_ReadShort (sizebuf_t *sb);
int	MSG_ReadInt3 (sizebuf_t *sb);
int	MSG_ReadLong (sizebuf_t *sb);
float MSG_ReadFloat (sizebuf_t *sb);
char *MSG_ReadString (sizebuf_t *sb);
char *MSG_ReadStringLine (sizebuf_t *sb);
#define MSG_ReadCoord(sb) ((float)MSG_ReadInt3((sb))*(1.0/16.0))
#define MSG_ReadPos(sb,pos) ((pos)[0]=MSG_ReadCoord((sb)),(pos)[1]=MSG_ReadCoord((sb)),(pos)[2]=MSG_ReadCoord((sb)))
#define MSG_ReadAngle(sb) (BYTE2ANGLE(MSG_ReadByte((sb))))
#define MSG_ReadAngle16(sb) (SHORT2ANGLE(MSG_ReadShort((sb))))
void MSG_ReadDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd);

void MSG_ReadDir (sizebuf_t *sb, vec3_t vector);
void MSG_ReadData (sizebuf_t *sb, void *buffer, int size);

//============================================================================

int	COM_Argc (void);
char *COM_Argv (int arg);	// range and null checked
void COM_ClearArgv (int arg);
int COM_CheckParm (char *parm);
void COM_AddParm (char *parm);

void COM_Init (void);
void COM_InitArgv (int argc, char **argv);

char *CopyString (char *in);

void Info_Print (char *s);

//============================================================================

/* crc.h */
void CRC_Init(unsigned short *crcvalue);
void CRC_ProcessByte(unsigned short *crcvalue, qbyte data);
unsigned short CRC_Value(unsigned short crcvalue);
unsigned short CRC_Block (qbyte *start, int count);

/* patch.h */
void Patch_GetFlatness( float maxflat, vec3_t *points, int *patch_cp, int *flat );
void Patch_Evaluate( vec_t *p, int *numcp, int *tess, vec_t *dest, int comp );

/* huff.h */
void Huff_Init (void);
void Huff_AddCounts ( unsigned char *data, unsigned int len );
unsigned Huff_DecodeStatic ( unsigned char *in, unsigned inSize, unsigned char *out, unsigned maxOutSize );
unsigned Huff_EncodeStatic ( unsigned char *in, unsigned inSize, unsigned char *out, unsigned maxOutSize );

/*
==============================================================

BSP FORMATS

==============================================================
*/

typedef struct
{ 
	char *header;
	int version;
	int lightmapWidth;
	int lightmapHeight;
	int flags;
} bspFormatDesc_t;

#define BSP_NONE	0
#define BSP_RAVEN	1

extern	int numBspFormats;
extern	bspFormatDesc_t bspFormats[];

/*
==============================================================

PROTOCOL

==============================================================
*/

// protocol.h -- communications protocols

#define	PROTOCOL_VERSION	42

//=========================================

#define	PORT_MASTER			27950
#define	PORT_CLIENT			22950+(rand()%5000)
#define	PORT_SERVER			27911

//=========================================

#define	UPDATE_BACKUP	16	// copies of entity_state_t to keep buffered
							// must be power of two
#define	UPDATE_MASK		(UPDATE_BACKUP-1)



//==================
// the svc_strings[] array in cl_parse.c should mirror this
//==================

//
// server to client
//
enum svc_ops_e
{
	svc_bad,

	// the rest are private to the client and server
	svc_layout,
	svc_nop,
	svc_disconnect,
	svc_reconnect,
	svc_servercmd,				// [string] string
	svc_sound,					// <see code>
	svc_stufftext,				// [string] stuffed into client's console buffer, should be \n terminated
	svc_serverdata,				// [long] protocol ...
	svc_spawnbaseline,		
	svc_download,				// [short] size [size bytes]
	svc_playerinfo,				// variable
	svc_packetentities,			// [...]
	svc_frame,
	svc_stringcmd				// [string] command to execute
};

//==============================================

//
// client to server
//
enum clc_ops_e
{
	clc_bad,
	clc_nop, 		
	clc_move,				// [[usercmd_t]
	clc_userinfo,			// [[userinfo string]
	clc_stringcmd			// [string] message
};

//==============================================

// plyer_state_t communication

#define	PS_M_TYPE			(1<<0)
#define	PS_M_ORIGIN0		(1<<1)
#define	PS_M_ORIGIN1		(1<<2)
#define	PS_M_ORIGIN2		(1<<3)
#define	PS_M_VELOCITY0		(1<<4)
#define	PS_M_VELOCITY1		(1<<5)
#define	PS_M_VELOCITY2		(1<<6)
#define PS_MOREBITS1		(1<<7)

#define	PS_M_TIME			(1<<8)
#define	PS_M_FLAGS			(1<<9)
#define	PS_M_DELTA_ANGLES0	(1<<10)
#define	PS_M_DELTA_ANGLES1	(1<<11)
#define	PS_M_DELTA_ANGLES2	(1<<12)
#define	PS_WEAPONFRAME		(1<<13)
#define	PS_VIEWANGLES		(1<<14)
#define PS_MOREBITS2		(1<<15)

#define	PS_M_GRAVITY		(1<<16)
#define	PS_KICKANGLES		(1<<17)
#define	PS_BLEND			(1<<18)
#define	PS_FOV				(1<<19)
#define	PS_WEAPONINDEX		(1<<20)
#define	PS_VIEWOFFSET		(1<<21)
#define PS_MOREBITS3		(1<<23)

//==============================================

// user_cmd_t communication

// ms and light always sent, the others are optional
#define	CM_ANGLE1 	(1<<0)
#define	CM_ANGLE2 	(1<<1)
#define	CM_ANGLE3 	(1<<2)
#define	CM_FORWARD	(1<<3)
#define	CM_SIDE		(1<<4)
#define	CM_UP		(1<<5)
#define	CM_BUTTONS	(1<<6)

//==============================================

// a sound without an ent or pos will be a local only sound
#define	SND_VOLUME		(1<<0)		// a byte
#define	SND_ATTENUATION	(1<<1)		// a byte
#define SND_POS0_8		(1<<2)		// a byte
#define SND_POS0_16		(1<<3)		// a short
#define SND_POS1_8		(1<<4)		// a byte
#define SND_POS1_16		(1<<5)		// a short
#define SND_POS2_8		(1<<6)		// a byte
#define SND_POS2_16		(1<<7)		// a short

#define DEFAULT_SOUND_PACKET_VOLUME	1.0
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

//==============================================

// entity_state_t communication

// try to pack the common update flags into the first byte
#define	U_ORIGIN1	(1<<0)
#define	U_ORIGIN2	(1<<1)
#define	U_ORIGIN3	(1<<2)
#define	U_ANGLE2	(1<<3)
#define	U_ANGLE3	(1<<4)
#define	U_EVENT		(1<<5)
#define	U_REMOVE	(1<<6)		// REMOVE this entity, don't add it
#define	U_MOREBITS1	(1<<7)		// read one additional byte

// second byte
#define	U_NUMBER16	(1<<8)		// NUMBER8 is implicit if not set
#define	U_FRAME8	(1<<9)		// frame is a byte
#define	U_ANGLE1	(1<<10)
#define	U_MODEL		(1<<11)
#define U_TYPE		(1<<12)
#define	U_OLDORIGIN	(1<<13)		// FIXME: get rid of this
#define U_EVENT2	(1<<14)
#define	U_MOREBITS2	(1<<15)		// read one additional byte

// third byte
#define	U_EFFECTS8	(1<<16)		// autorotate, trails, etc
#define U_WEAPON	(1<<17)
#define	U_SOUND		(1<<18)
#define	U_MODEL2	(1<<19)		// weapons, flags, etc
#define	U_RENDERFX8 (1<<20)
#define	U_SOLID		(1<<21)		// angles are short if bmodel (precise)
#define	U_SKIN8		(1<<22)
#define	U_MOREBITS3	(1<<23)		// read one additional byte

// fourth byte
#define	U_SKIN16	(1<<24)
#define	U_MODEL3	(1<<25)
#define U_LIGHT		(1<<26)
#define	U_EFFECTS16	(1<<27)
#define U_RENDERFX16 (1<<28)
#define	U_FRAME16	(1<<29)		// frame is a short

/*
==============================================================

CMD

Command text buffering and command execution

==============================================================
*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/

void Cbuf_Init (void);
// allocates an initial text buffer that will grow as needed

void Cbuf_AddText (char *text);
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText (char *text);
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void Cbuf_ExecuteText (int exec_when, char *text);
// this can be used in place of either Cbuf_AddText or Cbuf_InsertText

void Cbuf_AddEarlyCommands (qboolean clear);
// adds all the +set commands from the command line

qboolean Cbuf_AddLateCommands (void);
// adds all the remaining + commands from the command line
// Returns true if any late commands were added, which
// will keep the demoloop from immediately starting

void Cbuf_Execute (void);
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!

void Cbuf_CopyToDefer (void);
void Cbuf_InsertFromDefer (void);
// These two functions are used to defer any pending commands while a map
// is being loaded

//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void (*xcommand_t) (void);

void	Cmd_Init (void);

void	Cmd_AddCommand (char *cmd_name, xcommand_t function);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_stringcmd instead of executed locally
void	Cmd_RemoveCommand (char *cmd_name);

qboolean Cmd_Exists (char *cmd_name);
// used by the cvar code to check for cvar / command name overlap

char 	*Cmd_CompleteCommand (char *partial);
// attempts to match a partial command for automatic command line completion
// returns NULL if nothing fits

void	Cmd_WriteAliases (FILE *f);

int		Cmd_CompleteAliasCountPossible (char *partial);
char	**Cmd_CompleteAliasBuildList (char *partial);
int		Cmd_CompleteCountPossible (char *partial);
char	**Cmd_CompleteBuildList (char *partial);
char	*Cmd_CompleteAlias (char *partial);

int		Cmd_Argc (void);
char	*Cmd_Argv (int arg);
char	*Cmd_Args (void);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

void	Cmd_TokenizeString (char *text, qboolean macroExpand);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void	Cmd_ExecuteString (char *text);
// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console

void	Cmd_ForwardToServer (void);
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.


/*
==============================================================

CVAR

==============================================================
*/

/*

cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
in C code.

The user can access cvars from the console in three ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
set r_draworder 0	as above, but creates the cvar if not present
Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.
*/

extern	cvar_t	*cvar_vars;

cvar_t *Cvar_Get (char *var_name, char *value, int flags);
// creates the variable if it doesn't exist, or returns the existing one
// if it exists, the value will not be changed, but flags will be ORed in
// that allows variables to be unarchived without needing bitflags

cvar_t 	*Cvar_Set (char *var_name, char *value);
// will create the variable if it doesn't exist

cvar_t *Cvar_ForceSet (char *var_name, char *value);
// will set the variable even if NOSET or LATCH

cvar_t 	*Cvar_FullSet (char *var_name, char *value, int flags);

void	Cvar_SetValue (char *var_name, float value);
// expands value to a string and calls Cvar_Set

float	Cvar_VariableValue (char *var_name);
// returns 0 if not defined or non numeric

char	*Cvar_VariableString (char *var_name);
// returns an empty string if not defined

char 	*Cvar_CompleteVariable (char *partial);
// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits

void	Cmd_WriteAliases (FILE *f);

int Cvar_CompleteCountPossible (char *partial);
char **Cvar_CompleteBuildList (char *partial);
char *Cvar_TabComplete (const char *partial);

void	Cvar_GetLatchedVars (int flags);
// any CVAR_LATCHED variables that have been set will now take effect

void	Cvar_FixCheatVars (void);
// all cheat variables with be reset to default unless cheats are allowed

qboolean Cvar_Command (void);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)

void 	Cvar_WriteVariables (FILE *f);
// appends lines containing "set variable value" for all variables
// with the archive flag set to true.

void	Cvar_Init (void);

char	*Cvar_Userinfo (void);
// returns an info string containing all the CVAR_USERINFO cvars

char	*Cvar_Serverinfo (void);
// returns an info string containing all the CVAR_SERVERINFO cvars

extern	qboolean	userinfo_modified;
// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server

/*
==============================================================

NET

==============================================================
*/

// net.h -- quake's interface to the networking layer

#define	PORT_ANY	-1

#define	PACKET_HEADER	10			// two ints, and a short
#define MAX_PACKETLEN	1440

#define	MAX_MSGLEN		2000		// max length of a message
#define MAX_DEMO_MSGLEN	32768		// max length of a faked demo message

typedef enum {NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX} netadrtype_t;

typedef enum {NS_CLIENT, NS_SERVER} netsrc_t;

typedef struct netadr_s
{
	netadrtype_t	type;

	qbyte	ip[4];
	qbyte	ipx[10];

	unsigned short	port;
} netadr_t;

void		NET_Init (void);
void		NET_Shutdown (void);

void		NET_Config (qboolean multiplayer);

qboolean	NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message);
void		NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to);

qboolean	NET_CompareAdr (netadr_t *a, netadr_t *b);
qboolean	NET_CompareBaseAdr (netadr_t *a, netadr_t *b);
qboolean	NET_IsLocalAddress (netadr_t *adr);
char		*NET_AdrToString (netadr_t *a);
qboolean	NET_StringToAdr (char *s, netadr_t *a);
void		NET_Sleep(int msec);

//============================================================================

typedef struct
{
	qboolean	fatal_error;

	netsrc_t	sock;

	int			dropped;			// between last packet and previous

	int			last_received;		// for timeouts
	int			last_sent;			// for retransmits

	netadr_t	remote_address;
	int			qport;				// qport value to write when transmitting

// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

// reliable staging and holding areas
	sizebuf_t	message;		// writing buffer to send to server
	qbyte		message_buf[MAX_MSGLEN];

// message is copied to this buffer when it is first transfered
	int			reliable_length;
	qbyte		reliable_buf[MAX_MSGLEN];	// unacked reliable message
} netchan_t;

extern	netadr_t	net_from;

extern	sizebuf_t	net_recieved;
extern	qbyte		net_recieved_buffer[MAX_PACKETLEN];

extern	sizebuf_t	net_message;
extern	qbyte		net_message_buffer[MAX_MSGLEN];


void Netchan_Init (void);
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t adr, int qport);

qboolean Netchan_NeedReliable (netchan_t *chan);
void Netchan_Transmit (netchan_t *chan, int length, qbyte *data);
void Netchan_OutOfBand (int net_socket, netadr_t adr, int length, qbyte *data);
void Netchan_OutOfBandPrint (int net_socket, netadr_t adr, char *format, ...);
qboolean Netchan_Process (netchan_t *chan, sizebuf_t *recieved, sizebuf_t *msg);

/*
==============================================================

FILESYSTEM

==============================================================
*/

void	FS_InitFilesystem( void );
void	FS_SetGamedir( char *dir );
void	FS_CreatePath( const char *path );
char	*FS_NextPath( char *prevpath );
void	FS_ExecAutoexec( void );

// these functions are public and may be called from another DLL
int		FS_FOpenFile( const char *filename, int *filenum, int mode );
int		FS_Read( void *buffer, size_t len, int file );
int		FS_Write( const void *buffer, size_t len, int file );
int		FS_Tell( int file );
int		FS_Seek( int file, int offset, int whence );
int		FS_Eof( int file );
int		FS_Flush( int file );
void	FS_FCloseFile( int file );
void	FS_CopyFile( const char *src, const char *dst );
void	FS_RemoveFile( const char *name );
int		FS_RenameFile( const char *src, const char *dst );
int		FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize );
char	*FS_Gamedir( void );

// private engine functions
int		FS_LoadFile( const char *path, void **buffer );	// a null buffer will just return the file length without loading
														// a -1 length is not present
														// note: this can't be called from another DLL, due to MS libc issues
void	FS_FreeFile( void *buffer );

int		FS_GetFileListExt( const char *dir, const char *extension, char *buf, size_t *bufsize, char *buf2, size_t *buf2size );
char	**FS_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave );


/*
==============================================================

MISC

==============================================================
*/


#define	ERR_FATAL		0		// exit the entire game with a popup window
#define	ERR_DROP		1		// print to console and disconnect from game
#define	ERR_DISCONNECT	2		// don't kill server

#define	PRINT_ALL		0
#define PRINT_DEVELOPER	1	// only print when "developer 1"

#define MAX_PRINTMSG	2048

void		Com_BeginRedirect (int target, char *buffer, int buffersize, void (*flush));
void		Com_EndRedirect (void);
void 		Com_Printf (char *fmt, ...);
void 		Com_DPrintf (char *fmt, ...);
void 		Com_Error (int code, char *fmt, ...);
void 		Com_Quit (void);

int			Com_ClientState (void);		// this should have just been a cvar...
void		Com_SetClientState (int state);

int			Com_ServerState (void);		// this should have just been a cvar...
void		Com_SetServerState (int state);

unsigned	Com_BlockChecksum (void *buffer, int length);
qbyte		COM_BlockSequenceCRCByte (qbyte *base, int length, int sequence);

void		Com_PageInMemory (qbyte *buffer, int size);

unsigned int Com_HashKey (char *name, int hashsize);

extern	cvar_t	*developer;
extern	cvar_t	*dedicated;
extern	cvar_t	*host_speeds;
extern	cvar_t	*log_stats;

extern	FILE *log_stats_file;

// host_speeds times
extern	int		time_before_game;
extern	int		time_after_game;
extern	int		time_before_ref;
extern	int		time_after_ref;


//#define MEMCLUMPING
//#define MEMTRASH

#define POOLNAMESIZE 128

#ifdef MEMCLUMPING

// give malloc padding so we can't waste most of a page at the end
# define MEMCLUMPSIZE (65536 - 1536)

// smallest unit we care about is this many bytes
# define MEMUNIT 8
# define MEMBITS (MEMCLUMPSIZE / MEMUNIT)
# define MEMBITINTS (MEMBITS / 32)
# define MEMCLUMP_SENTINEL 0xABADCAFE

#endif

#define MEMHEADER_SENTINEL1 0xDEADF00D
#define MEMHEADER_SENTINEL2 0xDF

typedef struct memheader_s
{
	// next and previous memheaders in chain belonging to pool
	struct memheader_s *next;
	struct memheader_s *prev;

	// pool this memheader belongs to
	struct mempool_s *pool;

#ifdef MEMCLUMPING
	// clump this memheader lives in, NULL if not in a clump
	struct memclump_s *clump;
#endif

	// size of the memory after the header (excluding header and sentinel2)
	int size;

	// file name and line where Mem_Alloc was called
	const char *filename;
	int fileline;

	// should always be MEMHEADER_SENTINEL1
	unsigned int sentinel1;
	// immediately followed by data, which is followed by a MEMHEADER_SENTINEL2 byte
} memheader_t;

#ifdef MEMCLUMPING

typedef struct memclump_s
{
	// contents of the clump
	qbyte block[MEMCLUMPSIZE];

	// should always be MEMCLUMP_SENTINEL
	unsigned int sentinel1;

	// if a bit is on, it means that the MEMUNIT bytes it represents are
	// allocated, otherwise free
	int bits[MEMBITINTS];

	// should always be MEMCLUMP_SENTINEL
	unsigned int sentinel2;

	// if this drops to 0, the clump is freed
	int blocksinuse;

	// largest block of memory available (this is reset to an optimistic
	// number when anything is freed, and updated when alloc fails the clump)
	int largestavailable;

	// next clump in the chain
	struct memclump_s *chain;
} memclump_t;

#endif

#define MEMPOOL_TEMPORARY		1
#define MEMPOOL_GAMEPROGS		2
#define MEMPOOL_USERINTERFACE	4
#define MEMPOOL_CLIENTGAME		8

typedef struct mempool_s
{
	// should always be MEMHEADER_SENTINEL1
	unsigned int sentinel1;

	// chain of individual memory allocations
	struct memheader_s *chain;

#ifdef MEMCLUMPING
	// chain of clumps (if any)
	struct memclump_s *clumpchain;
#endif

	// temporary, etc
	int	flags;

	// total memory allocated in this pool (inside memheaders)
	int totalsize;

	// total memory allocated in this pool (actual malloc total)
	int realsize;

	// updated each time the pool is displayed by memlist, shows change from previous time (unless pool was freed)
	int lastchecksize;

	// name of the pool
	char name[POOLNAMESIZE];

	// linked into global mempool list or parent's children list
	struct mempool_s *next;

	struct mempool_s *parent;
	struct mempool_s *child;

	// file name and line where Mem_AllocPool was called
	const char *filename;

	int fileline;

	// should always be MEMHEADER_SENTINEL1
	unsigned int sentinel2;
} mempool_t;

void Memory_Init (void);
void Memory_InitCommands (void);
void Memory_Shutdown (void);

void *_Mem_AllocExt ( mempool_t *pool, int size, int z, int musthave, int canthave, const char *filename, int fileline );
void *_Mem_Alloc ( mempool_t *pool, int size, int musthave, int canthave, const char *filename, int fileline );
void *_Mem_Realloc ( void *data, int size, const char *filename, int fileline );
void _Mem_Free ( void *data, int musthave, int canthave, const char *filename, int fileline );
mempool_t *_Mem_AllocPool ( mempool_t *parent, const char *name, int flags, const char *filename, int fileline );
mempool_t *_Mem_AllocTempPool ( const char *name, const char *filename, int fileline );
void _Mem_FreePool ( mempool_t **pool, int musthave, int canthave, const char *filename, int fileline );
void _Mem_EmptyPool ( mempool_t *pool, int musthave, int canthave, const char *filename, int fileline );

void _Mem_CheckSentinels ( void *data, const char *filename, int fileline );
void _Mem_CheckSentinelsGlobal ( const char *filename, int fileline );

#define Mem_AllocExt(pool,size,z) _Mem_AllocExt(pool,size,z,0,0,__FILE__,__LINE__)
#define Mem_Alloc(pool,size) _Mem_Alloc(pool,size,0,0,__FILE__,__LINE__)
#define Mem_Realloc(data,size) _Mem_Realloc(data,size,__FILE__,__LINE__)
#define Mem_Free(mem) _Mem_Free(mem,0,0,__FILE__,__LINE__)
#define Mem_AllocPool(parent,name) _Mem_AllocPool(parent,name,0,__FILE__,__LINE__)
#define Mem_AllocTempPool(name) _Mem_AllocTempPool(name,__FILE__,__LINE__)
#define Mem_FreePool(pool) _Mem_FreePool(pool,0,0,__FILE__,__LINE__)
#define Mem_EmptyPool(pool) _Mem_EmptyPool(pool,0,0,__FILE__,__LINE__)

#define Mem_CheckSentinels(data) _Mem_CheckSentinels(data,__FILE__,__LINE__)
#define Mem_CheckSentinelsGlobal() _Mem_CheckSentinelsGlobal(__FILE__,__LINE__)

// used for temporary allocations
extern mempool_t *tempMemPool;
extern mempool_t *zoneMemPool;

#define Mem_ZoneMallocExt(size,z) Mem_AllocExt(zoneMemPool,size,z)
#define Mem_ZoneMalloc(size) Mem_Alloc(zoneMemPool,size)
#define Mem_ZoneFree(data) Mem_Free(data)

#define Mem_TempMallocExt(size,z) Mem_AllocExt(tempMemPool,size,z)
#define Mem_TempMalloc(size) Mem_Alloc(tempMemPool,size)
#define Mem_TempFree(data) Mem_Free(data)

void *Q_malloc (int cnt);
void Q_free (void *buf);

void Qcommon_Init (int argc, char **argv);
void Qcommon_Frame (int msec);
void Qcommon_Shutdown (void);

/*
==============================================================

NON-PORTABLE SYSTEM SERVICES

==============================================================
*/

extern	int	curtime;		// time returned by last Sys_Milliseconds

// directory searching
#define SFF_ARCH    0x01
#define SFF_HIDDEN  0x02
#define SFF_RDONLY  0x04
#define SFF_SUBDIR  0x08
#define SFF_SYSTEM  0x10

typedef enum { LIB_GAME, LIB_CGAME, LIB_UI } gamelib_t;

typedef struct { char *name; void **funcPointer; } dllfunc_t;

void	Sys_Init (void);

void	Sys_AppActivate (void);

void	Sys_UnloadGameLibrary (gamelib_t gamelib);
void	*Sys_LoadGameLibrary (gamelib_t gamelib, void *parms);
// loads the game dll and calls the api init function

void	Sys_UnloadLibrary( void **lib );
void	*Sys_LoadLibrary( char *name, dllfunc_t *funcs );	// NULL-terminated array of functions

void	Sys_InitTimer (void);
int		Sys_Milliseconds (void);
void	Sys_Mkdir (const char *path);

char	*Sys_ConsoleInput (void);
void	Sys_ConsoleOutput (char *string);
void	Sys_SendKeyEvents (void);
void	Sys_Error (char *error, ...);
void	Sys_Quit (void);
char	*Sys_GetClipboardData( void );

char	*Sys_GetHomeDirectory (void);

/*
** pass in an attribute mask of things you wish to REJECT
*/
char	*Sys_FindFirst (char *path, unsigned musthave, unsigned canthave );
char	*Sys_FindNext ( unsigned musthave, unsigned canthave );
void	Sys_FindClose (void);

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/

void CL_Init (void);
void CL_Disconnect (void);
void CL_Shutdown (void);
void CL_Frame (int msec);
void Con_Print (char *text);
void SCR_BeginLoadingPlaque (void);

void SV_Init (void);
void SV_Shutdown (char *finalmsg, qboolean reconnect);
void SV_Frame (int msec);
