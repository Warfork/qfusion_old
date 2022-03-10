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
// cmd.c -- Quake script command processing module

#include "qcommon.h"

#define ALIAS_HASH_SIZE	32

#define	MAX_ALIAS_NAME	64

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	struct cmdalias_s	*hash_next;
	char	*name;
	char	*value;
	qboolean	archive;
} cmd_alias_t;

cmd_alias_t	*cmd_alias;
cmd_alias_t	*cmd_alias_hash[ALIAS_HASH_SIZE];

qboolean	cmd_wait;

#define	ALIAS_LOOP_COUNT	16
int		alias_count;		// for detecting runaway loops


//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
	cmd_wait = qtrue;
}


/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

sizebuf_t	cmd_text;
qbyte		cmd_text_buf[8192];

qbyte		defer_text_buf[8192];

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	SZ_Init (&cmd_text, cmd_text_buf, sizeof(cmd_text_buf));
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
	int		l;

	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}
	SZ_Write (&cmd_text, text, strlen (text));
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
	char	*temp;
	int		templen;

// copy off any commands still remaining in the exec buffer
	templen = cmd_text.cursize;
	if (templen)
	{
		temp = Mem_ZoneMalloc (templen);
		memcpy (temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);
	}
	else
		temp = NULL;	// shut up compiler

// add the entire text of the file
	Cbuf_AddText (text);

// add the copied off data
	if (templen)
	{
		SZ_Write (&cmd_text, temp, templen);
		Mem_ZoneFree (temp);
	}
}


/*
============
Cbuf_CopyToDefer
============
*/
void Cbuf_CopyToDefer (void)
{
	memcpy(defer_text_buf, cmd_text_buf, cmd_text.cursize);
	defer_text_buf[cmd_text.cursize] = 0;
	cmd_text.cursize = 0;
}

/*
============
Cbuf_InsertFromDefer
============
*/
void Cbuf_InsertFromDefer (void)
{
	Cbuf_InsertText (defer_text_buf);
	defer_text_buf[0] = 0;
}


/*
============
Cbuf_ExecuteText
============
*/
void Cbuf_ExecuteText (int exec_when, char *text)
{
	switch (exec_when)
	{
	case EXEC_NOW:
		Cmd_ExecuteString (text);
		break;
	case EXEC_INSERT:
		Cbuf_InsertText (text);
		break;
	case EXEC_APPEND:
		Cbuf_AddText (text);
		break;
	default:
		Com_Error (ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int		i;
	char	*text;
	char	line[1024];
	int		quotes;

	alias_count = 0;		// don't allow infinite alias loops

	while (cmd_text.cursize)
	{
// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i=0 ; i< cmd_text.cursize ; i++)
		{
			if (text[i] == '"')
				quotes++;
			if ( !(quotes&1) &&  text[i] == ';')
				break;	// don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}

		memcpy (line, text, i);
		line[i] = 0;

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec, alias) can insert data at the
// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove (text, text+i, cmd_text.cursize);
		}

// execute the command line
		Cmd_ExecuteString (line);

		if (cmd_wait)
		{
			// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = qfalse;
			break;
		}
	}
}


/*
===============
Cbuf_AddEarlyCommands

Adds command line parameters as script statements
Commands lead with a +, and continue until another +

Set commands are added early, so they are guaranteed to be set before
the client and server initialize for the first time.

Other commands are added late, after all initialization is complete.
===============
*/
void Cbuf_AddEarlyCommands (qboolean clear)
{
	int		i;
	char	*s;

	for (i=0 ; i<COM_Argc() ; i++)
	{
		s = COM_Argv(i);
		if (Q_stricmp(s, "+set"))
			continue;
		Cbuf_AddText (va("set %s %s\n", COM_Argv(i+1), COM_Argv(i+2)));
		if (clear)
		{
			COM_ClearArgv(i);
			COM_ClearArgv(i+1);
			COM_ClearArgv(i+2);
		}
		i+=2;
	}
}

/*
=================
Cbuf_AddLateCommands

Adds command line parameters as script statements
Commands lead with a + and continue until another + or -
quake +map amlev1

Returns true if any late commands were added, which
will keep the demoloop from immediately starting
=================
*/
qboolean Cbuf_AddLateCommands (void)
{
	int		i, j;
	int		s;
	char	*text, *build, c;
	int		argc;
	qboolean	ret;

// build the combined string to parse from
	s = 0;
	argc = COM_Argc();
	for (i=1 ; i<argc ; i++)
	{
		s += strlen (COM_Argv(i)) + 1;
	}
	if (!s)
		return qfalse;

	text = Mem_ZoneMalloc (s+1);
	text[0] = 0;
	for (i=1 ; i<argc ; i++)
	{
		strcat (text,COM_Argv(i));
		if (i != argc-1)
			strcat (text, " ");
	}

// pull out the commands
	build = Mem_ZoneMalloc (s+1);
	build[0] = 0;
	
	for (i=0 ; i<s-1 ; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j=i ; (text[j] != '+') && (text[j] != '-') && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;
			
			strcat (build, text+i);
			strcat (build, "\n");
			text[j] = c;
			i = j-1;
		}
	}

	ret = (build[0] != 0);
	if (ret)
		Cbuf_AddText (build);

	Mem_ZoneFree (text);
	Mem_ZoneFree (build);

	return ret;
}


/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/


/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
	char	*f;
	int		len;

	if (Cmd_Argc () != 2)
	{
		Com_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	len = FS_LoadFile (Cmd_Argv(1), (void **)&f, NULL, 0);
	if (!f)
	{
		Com_Printf ("couldn't exec %s\n",Cmd_Argv(1));
		return;
	}
	Com_Printf ("execing %s\n",Cmd_Argv(1));
	
	Cbuf_InsertText (f);
	Cbuf_InsertText ("\n");

	FS_FreeFile (f);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	int		i;

	for (i=1 ; i<Cmd_Argc() ; i++)
		Com_Printf ("%s ",Cmd_Argv(i));
	Com_Printf ("\n");
}

/*
===============
Cmd_AliasList_f
===============
*/
void Cmd_AliasList_f (void)
{
	cmd_alias_t	*a;
	int			i;
	char		*pattern;

	if( !cmd_alias ) {
		Com_Printf( "No alias commands\n" );
		return;
	}

	if( Cmd_Argc () == 1 )
		pattern = NULL;		// no wildcard
	else
		pattern = Cmd_Args ();

	Com_Printf( "\nAlias commands:\n" );
	for( a = cmd_alias, i = 0; a; a = a->next ) {
		if( !pattern || Q_WildCmp( pattern, a->name ) ) {
			i++;
			Com_Printf( "%s : %s\n\n", a->name, a->value );
		}
	}
	Com_Printf( "%i commands\n", i );
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; separated)
===============
*/
void Cmd_Alias_f (void)
{
	cmd_alias_t	*a;
	char		cmd[1024];
	int			i, c;
	size_t		len;
	unsigned	hash;
	char		*s;

	if (Cmd_Argc() == 1)
	{
		Com_Printf ("usage: alias <name> <command>\n");
		return;
	}

	s = Cmd_Argv(1);
	len = strlen (s);
	if (len >= MAX_ALIAS_NAME)
	{
		Com_Printf ("Alias name is too long\n");
		return;
	}

	// if the alias already exists, reuse it
	hash = Com_HashKey (s, ALIAS_HASH_SIZE);
	for (a = cmd_alias_hash[hash] ; a ; a=a->hash_next)
	{
		if (!Q_stricmp(s, a->name))
		{
			Mem_ZoneFree (a->value);
			break;
		}
	}

	if (!a)
	{
		a = Mem_ZoneMalloc (sizeof(cmd_alias_t) + len + 1);
		a->name = (char *)((qbyte *)a + sizeof(cmd_alias_t));
		strcpy (a->name, s);
		a->hash_next = cmd_alias_hash[hash];
		cmd_alias_hash[hash] = a;
		a->next = cmd_alias;
		cmd_alias = a;
	}

	if (!Q_stricmp(Cmd_Argv(0), "aliasa"))
		a->archive = qtrue;

// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	c = Cmd_Argc();
	for (i=2 ; i< c ; i++)
	{
		strcat (cmd, Cmd_Argv(i));
		if (i != (c - 1))
			strcat (cmd, " ");
	}

	a->value = CopyString (cmd);
}

/*
===============
Cmd_Unalias_f

Removes an alias command
===============
*/
void Cmd_Unalias_f (void)
{
	char		*s;
	unsigned	hash;
	cmd_alias_t	*a, **back;

	if (Cmd_Argc() == 1)
	{
		Com_Printf ("usage: unalias <name>\n");
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Com_Printf ("Alias name is too long\n");
		return;
	}

	hash = Com_HashKey (s, ALIAS_HASH_SIZE);
	back = &cmd_alias_hash[hash];
	while (1)
	{
		a = *back;
		if (!a)
		{
			Com_Printf ("Cmd_Unalias_f: %s not added\n", s);
			return;
		}
		if (!Q_stricmp (s, a->name))
		{
			*back = a->hash_next;
			break;
		}
		back = &a->hash_next;
	}

	back = &cmd_alias;
	while (1)
	{
		a = *back;
		if (!a)
		{
			Com_Printf ("Cmd_RemoveCommand: %s not added\n", s);
			return;
		}
		if (!Q_stricmp (s, a->name))
		{
			*back = a->next;
			Mem_ZoneFree (a->value);
			Mem_ZoneFree (a);
			return;
		}
		back = &a->next;
	}
}

/*
===============
Cmd_UnaliasAll_f

Removes an alias command
===============
*/
void Cmd_UnaliasAll_f (void)
{
	cmd_alias_t	*a, *next;

	for (a = cmd_alias ; a ; a=next)
	{
		next = a->next;
		Mem_ZoneFree (a->value);
		Mem_ZoneFree (a);
	}

	cmd_alias = NULL;
	memset (cmd_alias_hash, 0, sizeof(cmd_alias_hash));
}

/*
===============
Cmd_WriteAliases

Write lines containing "aliasa alias value" for all aliases
with the archive flag set to true
===============
*/
void Cmd_WriteAliases (FILE *f)
{
	cmd_alias_t	*a;

	for (a = cmd_alias ; a ; a=a->next)
		if (a->archive)
			fprintf (f, "aliasa %s \"%s\"\n", a->name, a->value);
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

#define CMD_HASH_SIZE		32

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	struct cmd_function_s	*hash_next;
	char					*name;
	xcommand_t				function;
} cmd_function_t;


static	int			cmd_argc;
static	char		*cmd_argv[MAX_STRING_TOKENS];
static	char		*cmd_null_string = "";
static	char		cmd_args[MAX_STRING_CHARS];

static	cmd_function_t	*cmd_functions;		// possible commands to execute
static	cmd_function_t	*cmd_functions_hash[CMD_HASH_SIZE];

/*
============
Cmd_Argc
============
*/
int	Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char *Cmd_Argv (int arg)
{
	if ( (unsigned)arg >= cmd_argc )
		return cmd_null_string;
	return cmd_argv[arg];	
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char *Cmd_Args (void)
{
	return cmd_args;
}


/*
======================
Cmd_MacroExpandString
======================
*/
char *Cmd_MacroExpandString (char *text)
{
	int		i, j, count, len;
	qboolean	inquote;
	char	*scan;
	static	char	expanded[MAX_STRING_CHARS];
	char	temporary[MAX_STRING_CHARS];
	char	*token, *start;

	inquote = qfalse;
	scan = text;

	len = strlen (scan);
	if (len >= MAX_STRING_CHARS)
	{
		Com_Printf ("Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	count = 0;

	for (i=0 ; i<len ; i++)
	{
		if (scan[i] == '"')
			inquote ^= 1;
		if (inquote)
			continue;	// don't expand inside quotes
		if (scan[i] != '$')
			continue;
		// scan out the complete macro
		start = scan+i+1;
		token = COM_Parse (&start);
		if (!start)
			continue;

		token = Cvar_VariableString (token);

		j = strlen(token);
		len += j;
		if (len >= MAX_STRING_CHARS)
		{
			Com_Printf ("Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return NULL;
		}

		strncpy (temporary, scan, i);
		strcpy (temporary+i, token);
		strcpy (temporary+i+j, start);

		strcpy (expanded, temporary);
		scan = expanded;
		i--;

		if (++count == 100)
		{
			Com_Printf ("Macro expansion loop, discarded.\n");
			return NULL;
		}
	}

	if (inquote)
	{
		Com_Printf ("Line has unmatched quote, discarded.\n");
		return NULL;
	}

	return scan;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
$Cvars will be expanded unless they are in a quoted token
============
*/
void Cmd_TokenizeString (char *text, qboolean macroExpand)
{
	int		i;
	char	*com_token;

// clear the args from the last string
	for (i=0 ; i<cmd_argc ; i++)
		Mem_ZoneFree (cmd_argv[i]);

	cmd_argc = 0;
	cmd_args[0] = 0;

	// macro expand the text
	if (macroExpand)
		text = Cmd_MacroExpandString (text);
	if (!text)
		return;

	while (1)
	{
// skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\n')
			text++;

		if (*text == '\n')
		{	// a newline separates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		// set cmd_args to everything after the first arg
		if (cmd_argc == 1)
		{
			int		l;

			strncpy (cmd_args, text, sizeof(cmd_args)-1); 
			cmd_args[sizeof(cmd_args)-1] = 0; 

			// strip off any trailing whitespace
			l = strlen(cmd_args) - 1;
			for ( ; l >= 0 ; l--)
				if (cmd_args[l] <= ' ')
					cmd_args[l] = 0;
				else
					break;
		}

		com_token = COM_Parse (&text);
		if (!text)
			return;

		if (cmd_argc < MAX_STRING_TOKENS)
		{
			cmd_argv[cmd_argc] = Mem_ZoneMalloc (strlen(com_token)+1);
			strcpy (cmd_argv[cmd_argc], com_token);
			cmd_argc++;
		}
	}
}


/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	unsigned		hash;
	cmd_function_t	*cmd;

	if (!cmd_name || !cmd_name[0])
	{
		Com_DPrintf ("Cmd_AddCommand: empty name pass as an argument\n");
		return;
	}

// fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Com_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

// fail if the command already exists
	hash = Com_HashKey (cmd_name, CMD_HASH_SIZE);
	for (cmd=cmd_functions_hash[hash] ; cmd ; cmd=cmd->hash_next)
	{
		if (!Q_stricmp(cmd_name, cmd->name))
		{
			Com_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = Mem_ZoneMalloc (sizeof(cmd_function_t) + strlen(cmd_name) + 1);
	cmd->name = (char *)((qbyte *)cmd + sizeof(cmd_function_t));
	strcpy (cmd->name, cmd_name);
	cmd->function = function;
	cmd->hash_next = cmd_functions_hash[hash];
	cmd_functions_hash[hash] = cmd;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/*
============
Cmd_RemoveCommand
============
*/
void Cmd_RemoveCommand (char *cmd_name)
{
	unsigned		hash;
	cmd_function_t	*cmd, **back;

	hash = Com_HashKey (cmd_name, CMD_HASH_SIZE);
	back = &cmd_functions_hash[hash];
	while (1)
	{
		cmd = *back;
		if (!cmd)
		{
			Com_Printf ("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if (!Q_stricmp (cmd_name, cmd->name))
		{
			*back = cmd->hash_next;
			break;
		}
		back = &cmd->hash_next;
	}

	back = &cmd_functions;
	while (1)
	{
		cmd = *back;
		if (!cmd)
		{
			Com_Printf ("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if (!Q_stricmp (cmd_name, cmd->name))
		{
			*back = cmd->next;
			Mem_ZoneFree (cmd);
			return;
		}
		back = &cmd->next;
	}
}

/*
============
Cmd_Exists
============
*/
qboolean Cmd_Exists (char *cmd_name)
{
	unsigned		hash;
	cmd_function_t	*cmd;

	hash = Com_HashKey (cmd_name, CMD_HASH_SIZE);
	for (cmd=cmd_functions_hash[hash] ; cmd ; cmd=cmd->hash_next)
	{
		if (!Q_stricmp(cmd_name,cmd->name))
			return qtrue;
	}

	return qfalse;
}

/*
============
Cmd_VStr_f
============
*/
void Cmd_VStr_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf ("vstr <variable> : execute a variable command\n");
		return;
	}

	Cbuf_InsertText (Cvar_VariableString(Cmd_Argv(1)));
}



/*
============
Cmd_CompleteCommand
============
*/
char *Cmd_CompleteCommand (char *partial)
{
	int				len;
	cmd_alias_t		*a;
	cmd_function_t	*cmd;

	len = strlen (partial);

	if (!len)
		return NULL;

	// check functions
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!Q_strnicmp (partial, cmd->name, len))
			return cmd->name;
	for (a = cmd_alias; a; a = a->next)
		if (!Q_stricmp (partial, a->name))
			return a->name;

	// check for partial match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!Q_strnicmp (partial, cmd->name, len))
			return cmd->name;
	for (a = cmd_alias; a; a = a->next)
		if (!Q_strnicmp (partial, a->name, len))
			return a->name;

	return NULL;
}

/*
============
Cmd_CompleteCountPossible
Thanks for Taniwha's Help -EvilTypeGuy
============
*/
int Cmd_CompleteCountPossible (char *partial)
{
	cmd_function_t	*cmd;
	int				len, h = 0;

	len = strlen(partial);
	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!Q_strnicmp (partial, cmd->name, len))
			h++;

	return h;
}

/*
============
Cmd_CompleteBuildList
Thanks for Taniwha's Help -EvilTypeGuy
============
*/
char **Cmd_CompleteBuildList (char *partial)
{
	cmd_function_t	*cmd;
	int				len, bpos = 0;
	int				sizeofbuf = (Cmd_CompleteCountPossible (partial) + 1) * sizeof (char *);
	char			**buf;

	len = strlen (partial);
	buf = (char **)Mem_TempMalloc(sizeofbuf + sizeof (char *));

	// Loop through the alias list and print all matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!Q_strnicmp (partial, cmd->name, len))
			buf[bpos++] = cmd->name;

	buf[bpos] = NULL;
	return buf;
}

/*
Cmd_CompleteAlias
Thanks for Taniwha's Help -EvilTypeGuy
*/
char *Cmd_CompleteAlias (char *partial)
{
	static cmd_alias_t	*alias;
	int			len;

	len = strlen(partial);

	if (!len)
		return NULL;

	// Check aliases
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!Q_strnicmp(partial, alias->name, len))
			return alias->name;

	return NULL;
}

/*
Cmd_CompleteAliasCountPossible
Thanks for Taniwha's Help -EvilTypeGuy
*/
int Cmd_CompleteAliasCountPossible (char *partial)
{
	cmd_alias_t	*alias;
	int			len, h = 0;

	len = strlen (partial);

	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!Q_strnicmp (partial, alias->name, len))
			h++;

	return h;
}

/*
Cmd_CompleteAliasBuildList
Thanks for Taniwha's Help -EvilTypeGuy
*/
char **Cmd_CompleteAliasBuildList (char *partial)
{
	cmd_alias_t	*alias;
	int			len, bpos = 0;
	int			sizeofbuf = (Cmd_CompleteAliasCountPossible (partial) + 1) * sizeof (char *);
	char		**buf;

	len = strlen (partial);
	buf = (char **)Mem_TempMalloc (sizeofbuf + sizeof (char *));

	// Loop through the alias list and print all matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!Q_strnicmp (partial, alias->name, len))
			buf[bpos++] = alias->name;

	buf[bpos] = NULL;
	return buf;
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void Cmd_ExecuteString (char *text)
{
	char			*str;
	unsigned		hash;
	cmd_function_t	*cmd;
	cmd_alias_t		*a;

	Cmd_TokenizeString (text, qtrue);

	// execute the command line
	if (!Cmd_Argc())
		return;		// no tokens

	str = cmd_argv[0];

	// check functions
	hash = Com_HashKey (str, CMD_HASH_SIZE);
	for (cmd=cmd_functions_hash[hash] ; cmd ; cmd=cmd->hash_next)
	{
		if (!Q_stricmp (str,cmd->name))
		{
			if (!cmd->function)
			{	// forward to server command
				Cmd_ExecuteString (va("cmd %s", text));
			}
			else
				cmd->function ();
			return;
		}
	}

	// check alias
	hash = Com_HashKey (str, ALIAS_HASH_SIZE);
	for (a=cmd_alias_hash[hash] ; a ; a=a->hash_next)
	{
		if (!Q_stricmp (cmd_argv[0], a->name))
		{
			if (++alias_count == ALIAS_LOOP_COUNT)
			{
				Com_Printf ("ALIAS_LOOP_COUNT\n");
				return;
			}
			Cbuf_InsertText ("\n");
			Cbuf_InsertText (a->value);
			return;
		}
	}

	// check cvars
	if (Cvar_Command ())
		return;

	// send it as a server command if we are connected
	Cmd_ForwardToServer ();
}

/*
============
Cmd_List_f
============
*/
void Cmd_List_f (void)
{
	cmd_function_t	*cmd;
	int				i;
	char			*pattern;

	if( Cmd_Argc () == 1 )
		pattern = NULL;		// no wildcard
	else
		pattern = Cmd_Args ();

	Com_Printf ("\nCommands:\n");
	for( cmd = cmd_functions, i = 0; cmd; cmd = cmd->next ) {
		if( !pattern || Q_WildCmp( pattern, cmd->name ) ) {
			i++;
			Com_Printf( "%s\n", cmd->name );
		}
	}
	Com_Printf( "%i commands\n", i );
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
//
// register our commands
//
	Cmd_AddCommand ("cmdlist",Cmd_List_f);
	Cmd_AddCommand ("exec",Cmd_Exec_f);
	Cmd_AddCommand ("echo",Cmd_Echo_f);
	Cmd_AddCommand ("aliaslist",Cmd_AliasList_f);
	Cmd_AddCommand ("aliasa",Cmd_Alias_f);
	Cmd_AddCommand ("unalias", Cmd_Unalias_f);
	Cmd_AddCommand ("unaliasall", Cmd_UnaliasAll_f);
	Cmd_AddCommand ("alias",Cmd_Alias_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
	Cmd_AddCommand ("vstr", Cmd_VStr_f);
}
