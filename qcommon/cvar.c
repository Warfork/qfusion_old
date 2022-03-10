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
// cvar.c -- dynamic variable tracking

#include "qcommon.h"

#define CVAR_HASH_SIZE		32

cvar_t	*cvar_vars;
cvar_t	*cvar_vars_hash[CVAR_HASH_SIZE];

/*
============
Cvar_InfoValidate
============
*/
static qboolean Cvar_InfoValidate (const char *s, qboolean name)
{
	if (strstr (s, "\\"))
		return qfalse;
	if (strstr (s, "\""))
		return qfalse;
	if (strstr (s, ";"))
		return qfalse;
	if (name && strstr (s, "^"))
		return qfalse;
	return qtrue;
}

/*
============
Cvar_FindVar
============
*/
static cvar_t *Cvar_FindVar (const char *var_name)
{
	unsigned hash;
	cvar_t	*var;

	hash = Com_HashKey (var_name, CVAR_HASH_SIZE);
	for (var=cvar_vars_hash[hash] ; var ; var=var->next)
		if (!Q_stricmp(var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue (const char *var_name)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return atof (var->string);
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return "";
	return var->string;
}


/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (const char *partial)
{
	cvar_t		*cvar;
	int			len;

	len = strlen(partial);

	if (!len)
		return NULL;

	// check exact match
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!Q_stricmp(partial,cvar->name))
			return cvar->name;

	// check partial match
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!Q_strnicmp(partial,cvar->name, len))
			return cvar->name;

	return NULL;
}


/*
============
Cvar_Get

If the variable already exists, the value will not be set
The flags will be or'ed and default value overwritten in if the variable exists.
============
*/
cvar_t *Cvar_Get (const char *var_name, const char *var_value, int flags)
{
	unsigned hash;
	cvar_t	*var;

	if (!var_name || !var_name[0])
		return NULL;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_name, qtrue))
		{
			Com_Printf("invalid info cvar name\n");
			return NULL;
		}
	}

	hash = Com_HashKey (var_name, CVAR_HASH_SIZE);
	for (var=cvar_vars_hash[hash] ; var ; var=var->hash_next)
		if (!Q_stricmp (var_name, var->name))
			break;

	if (var)
	{
		if( var_value ) {
			Mem_ZoneFree (var->dvalue);	// free the old default value string
			var->dvalue = CopyString (var_value);
		}

		if ( (flags &CVAR_USERINFO) && !(var->flags & CVAR_USERINFO) )
			userinfo_modified = qtrue;	// transmit at next oportunity

		var->flags |= flags;
		return var;
	}

	if (!var_value)
		return NULL;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_value, qfalse))
		{
			Com_Printf("invalid info cvar value\n");
			return NULL;
		}
	}

	var = Mem_ZoneMalloc (sizeof(*var) + strlen(var_name) + 1);
	var->name = (char *)((qbyte *)var + sizeof(*var));
	strcpy (var->name, var_name);
	var->dvalue = CopyString (var_value);
	var->string = CopyString (var_value);
	var->modified = qtrue;
	var->value = atof (var->string);
	var->integer = Q_rint( var->value );

	// link the variable in
	var->hash_next = cvar_vars_hash[hash];
	cvar_vars_hash[hash] = var;
	var->next = cvar_vars;
	cvar_vars = var;

	var->flags = flags;

	return var;
}

/*
============
Cvar_Set2
============
*/
cvar_t *Cvar_Set2 (const char *var_name, const char *value, qboolean force)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, 0);
	}

	if (var->flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (value, qfalse))
		{
			Com_Printf("invalid info cvar value\n");
			return var;
		}
	}

	if (!force)
	{
		if (var->flags & CVAR_NOSET)
		{
			Com_Printf ("%s is write protected.\n", var_name);
			return var;
		}

		if ((var->flags & CVAR_CHEAT) && strcmp(value, var->dvalue))
		{
			if ((Com_ServerState() && !Cvar_VariableValue("sv_cheats")) ||
				(Com_ClientState() >= 3 /* ca_connected */) && !Com_ServerState())
			{
				Com_Printf ("%s is cheat protected.\n", var_name);
				return var;
			}
		}

		if (var->flags & (CVAR_LATCH|CVAR_LATCH_VIDEO))
		{
			if (var->latched_string)
			{
				if (strcmp(value, var->latched_string) == 0)
					return var;
				Mem_ZoneFree (var->latched_string);
			}
			else
			{
				if (strcmp(value, var->string) == 0)
					return var;
			}

			if (Com_ServerState())
			{
				Com_Printf ("%s will be changed upon restarting.\n", var->name);
				var->latched_string = CopyString(value);
			}
			else
			{
				if (var->flags & CVAR_LATCH_VIDEO)
				{
					Com_Printf ("%s will be changed upon restarting video.\n", var->name);
					var->latched_string = CopyString(value);
				}
				else
				{
					Mem_ZoneFree (var->string);	// free the old value string
					var->string = CopyString(value);
					var->value = atof (var->string);
					var->integer = Q_rint( var->value );
					var->modified = qtrue;

					if (!strcmp (var->name, "fs_game"))
					{
						FS_SetGamedir (var->string);
						FS_ExecAutoexec ();
					}
				}
			}

			return var;
		}
	}
	else
	{
		if (var->latched_string)
		{
			Mem_ZoneFree (var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var;		// not changed

	var->modified = qtrue;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = qtrue;	// transmit at next oportunity

	Mem_ZoneFree (var->string);	// free the old value string

	var->string = CopyString(value);
	var->value = atof (var->string);
	var->integer = Q_rint( var->value );

	return var;
}

/*
============
Cvar_ForceSet
============
*/
cvar_t *Cvar_ForceSet (const char *var_name, const char *value)
{
	return Cvar_Set2 (var_name, value, qtrue);
}

/*
============
Cvar_Set
============
*/
cvar_t *Cvar_Set (const char *var_name, const char *value)
{
	return Cvar_Set2 (var_name, value, qfalse);
}

/*
============
Cvar_FullSet
============
*/
cvar_t *Cvar_FullSet (const char *var_name, const char *value, int flags, qboolean overwriteFlags)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, flags);
	}

	var->modified = qtrue;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = qtrue;	// transmit at next oportunity

	Mem_ZoneFree (var->string);	// free the old value string

	var->string = CopyString(value);
	var->value = atof (var->string);
	var->integer = Q_rint( var->value );
	if( overwriteFlags )
	    var->flags = flags;
	else
		var->flags |= flags;
	return var;
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (const char *var_name, float value)
{
	char	val[32];

	if (value == Q_rint(value))
		Q_snprintfz (val, sizeof(val), "%i", Q_rint(value));
	else
		Q_snprintfz (val, sizeof(val), "%f", value);
	Cvar_Set (var_name, val);
}


/*
============
Cvar_GetLatchedVars

Any variables with latched values will now be updated
============
*/
void Cvar_GetLatchedVars (int flags)
{
	cvar_t	*var;

	flags &= CVAR_LATCH|CVAR_LATCH_VIDEO;
	if (!flags)
		return;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!(var->flags & flags))
			continue;
		if (!var->latched_string)
			continue;
		Mem_ZoneFree (var->string);
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = atof(var->string);
		var->integer = Q_rint( var->value );
		if (!strcmp(var->name, "fs_game"))
		{
			FS_SetGamedir (var->string);
			FS_ExecAutoexec ();
		}
	}
}

/*
============
Cvar_FixCheatVars

All cheat variables with be reset to default unless cheats are allowed
============
*/
void Cvar_FixCheatVars (void)
{
	cvar_t	*var;

	// if running a local server, check sv_cheats
	// never allow cheats on a remote server
	if (!(Com_ServerState() && !Cvar_VariableValue("sv_cheats")) &&
		!(Com_ClientState() >= ca_connected) && !Com_ServerState())
		return;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!(var->flags & CVAR_CHEAT))
			continue;
		Mem_ZoneFree (var->string);
		var->string = CopyString(var->dvalue);
		var->value = atof(var->string);
		var->integer = Q_rint( var->value );
	}
}


/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command (void)
{
	cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return qfalse;

// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Com_Printf ("\"%s\" is \"%s%s\" default: \"%s%s\"\n", v->name, 
			v->string, S_COLOR_WHITE, v->dvalue, S_COLOR_WHITE);
		if (v->latched_string)
			Com_Printf ("latched: \"%s%s\"\n", v->latched_string, S_COLOR_WHITE);
		return qtrue;
	}

	Cvar_Set (v->name, Cmd_Argv(1));
	return qtrue;
}


/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console
============
*/
void Cvar_Set_f (void)
{
	if (Cmd_Argc() != 3)
	{
		Com_Printf ("usage: set <variable> <value>\n");
		return;
	}

	Cvar_Set (Cmd_Argv(1), Cmd_Argv(2));
}

void Cvar_Seta_f (void)
{
	if (Cmd_Argc() != 3)
	{
		Com_Printf ("usage: seta <variable> <value>\n");
		return;
	}

	Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), CVAR_ARCHIVE, qfalse);
}

void Cvar_Sets_f (void)
{
	if (Cmd_Argc() != 3)
	{
		Com_Printf ("usage: sets <variable> <value>\n");
		return;
	}

	Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), CVAR_SERVERINFO, qfalse);
}

void Cvar_Setu_f (void)
{
	if (Cmd_Argc() != 3)
	{
		Com_Printf ("usage: setu <variable> <value>\n");
		return;
	}

	Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), CVAR_USERINFO, qfalse);
}

/*
=============
Cvar_Toggle_f
=============
*/
void Cvar_Toggle_f (void)
{
	cvar_t *var;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: toggle <variable>\n");
		return;
	}

	var = Cvar_FindVar (Cmd_Argv(1));
	if (!var)
	{
		Com_Printf ("no such variable: \"%s\"\n", Cmd_Argv(1));
		return;
	}

	Cvar_Set (var->name, var->integer ? "0" : "1");
}

/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (FILE *f)
{
	cvar_t	*var;
	char	buffer[1024];

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & CVAR_ARCHIVE)
		{
			if (var->flags & (CVAR_LATCH|CVAR_LATCH_VIDEO))
			{
				if (var->latched_string)
					Q_snprintfz (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->latched_string);
				else
					Q_snprintfz (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->string);
			}
			else
			{
				Q_snprintfz (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->string);
			}

			fprintf (f, "%s", buffer);
		}
	}
}

/*
============
Cvar_List_f

============
*/
void Cvar_List_f (void)
{
	cvar_t	*var;
	int		i;
	char	*pattern;

	if( Cmd_Argc () == 1 )
		pattern = NULL;		// no wildcard
	else
		pattern = Cmd_Args ();

	Com_Printf( "\nConsole variables:\n" );
	for (var = cvar_vars, i = 0 ; var ; var = var->next)
	{
		if (!pattern || Q_WildCmp( pattern, var->name ) ) {
			if (var->flags & CVAR_ARCHIVE)
				Com_Printf ("*");
			else
				Com_Printf (" ");
			if (var->flags & CVAR_USERINFO)
				Com_Printf ("U");
			else
				Com_Printf (" ");
			if (var->flags & CVAR_SERVERINFO)
				Com_Printf ("S");
			else
				Com_Printf (" ");
			if (var->flags & CVAR_NOSET)
				Com_Printf ("-");
			else if (var->flags & (CVAR_LATCH|CVAR_LATCH_VIDEO))
				Com_Printf ("L");
			else
				Com_Printf (" ");
			i++;
			Com_Printf (" %s \"%s\"\n", var->name, var->string);
		}
	}
	Com_Printf( "%i variables\n", i );
}


qboolean userinfo_modified;


char	*Cvar_BitInfo (int bit)
{
	static char	info[MAX_INFO_STRING];
	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & bit)
			Info_SetValueForKey (info, var->name, var->string);
	}
	return info;
}

// returns an info string containing all the CVAR_USERINFO cvars
char	*Cvar_Userinfo (void)
{
	return Cvar_BitInfo (CVAR_USERINFO);
}

// returns an info string containing all the CVAR_SERVERINFO cvars
char	*Cvar_Serverinfo (void)
{
	return Cvar_BitInfo (CVAR_SERVERINFO);
}

/*
	CVar_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com

*/
int
Cvar_CompleteCountPossible (const char *partial)
{
	cvar_t	*v;
	int		len, h = 0;

	len = strlen(partial);

	if (!len)
		return	0;

	// Loop through the cvars and count all possible matches
	for (v = cvar_vars; v; v = v->next)
		if (!Q_strnicmp (partial, v->name, len))
			h++;

	return h;
}

/*
	CVar_CompleteBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
char **Cvar_CompleteBuildList (const char *partial)
{
	cvar_t		*v;
	int			len;
	int			bpos = 0;
	int			sizeofbuf = (Cvar_CompleteCountPossible (partial) + 1) * sizeof (char *);
	char		**buf;

	len = strlen (partial);
	buf = (char **)Mem_TempMalloc (sizeofbuf + sizeof (char *));

	// Loop through the alias list and print all matches
	for (v = cvar_vars; v; v = v->next)
		if (!Q_strnicmp(partial, v->name, len))
			buf[bpos++] = v->name;

	buf[bpos] = NULL;
	return buf;
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init (void)
{
	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_AddCommand ("seta", Cvar_Seta_f);
	Cmd_AddCommand ("setu", Cvar_Setu_f);
	Cmd_AddCommand ("sets", Cvar_Sets_f);
	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_AddCommand ("cvarlist", Cvar_List_f);
}
