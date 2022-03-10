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

cvar_t	*cvar_vars;

/*
============
Cvar_InfoValidate
============
*/
static qboolean Cvar_InfoValidate (char *s, qboolean name)
{
	if (strstr (s, "\\"))
		return false;
	if (strstr (s, "\""))
		return false;
	if (strstr (s, ";"))
		return false;
	if (name && strstr (s, "^"))
		return false;
	return true;
}

/*
============
Cvar_FindVar
============
*/
static cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;
	
	for (var=cvar_vars ; var ; var=var->next)
		if (!Q_stricmp(var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue (char *var_name)
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
char *Cvar_VariableString (char *var_name)
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
char *Cvar_CompleteVariable (char *partial)
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
cvar_t *Cvar_Get (char *var_name, char *var_value, int flags)
{
	cvar_t	*var;
	
	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_name, true))
		{
			Com_Printf("invalid info cvar name\n");
			return NULL;
		}
	}

	var = Cvar_FindVar (var_name);
	if (var)
	{
		if ( var_value ) {
			Z_Free (var->dvalue);	// free the old default value string
			var->dvalue = CopyString (var_value);
		}

		var->flags |= flags;
		return var;
	}

	if (!var_value)
		return NULL;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_value, false))
		{
			Com_Printf("invalid info cvar value\n");
			return NULL;
		}
	}

	var = Z_Malloc (sizeof(*var));
	var->name = CopyString (var_name);
	var->dvalue = CopyString (var_value);
	var->string = CopyString (var_value);
	var->modified = true;
	var->value = atof (var->string);

	// link the variable in
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
cvar_t *Cvar_Set2 (char *var_name, char *value, qboolean force)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, 0);
	}

	if (var->flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (value, false))
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

		if ((var->flags & CVAR_LATCH) && !Com_ServerState())
			goto setit;		// server is not active, so don't latch

		if (var->flags & (CVAR_LATCH|CVAR_LATCH_VIDEO))
		{
			if (var->latched_string)
			{
				if (strcmp(value, var->latched_string) == 0)
					return var;
				Z_Free (var->latched_string);
			}
			else
			{
				if (strcmp(value, var->string) == 0)
					return var;
			}

			Com_Printf ("%s will be changed upon restarting%s.\n", var_name,
				var->flags & CVAR_LATCH_VIDEO ? " video" : "");
			var->latched_string = CopyString(value);

			if (!Com_ServerState())
			{
				if (!strcmp(var->name, "fs_game"))
				{
					FS_SetGamedir (var->string);
					FS_ExecAutoexec ();
				}
			}

			return var;
		}
	}
	else
	{
setit:
		if (var->latched_string)
		{
			Z_Free (var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var;		// not changed

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity
	
	Z_Free (var->string);	// free the old value string
	
	var->string = CopyString(value);
	var->value = atof (var->string);

	return var;
}

/*
============
Cvar_ForceSet
============
*/
cvar_t *Cvar_ForceSet (char *var_name, char *value)
{
	return Cvar_Set2 (var_name, value, true);
}

/*
============
Cvar_Set
============
*/
cvar_t *Cvar_Set (char *var_name, char *value)
{
	return Cvar_Set2 (var_name, value, false);
}

/*
============
Cvar_FullSet
============
*/
cvar_t *Cvar_FullSet (char *var_name, char *value, int flags)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, flags);
	}

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity
	
	Z_Free (var->string);	// free the old value string
	
	var->string = CopyString(value);
	var->value = atof (var->string);
	var->flags = flags;

	return var;
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (char *var_name, float value)
{
	char	val[32];

	if (value == (int)value)
		Com_sprintf (val, sizeof(val), "%i",(int)value);
	else
		Com_sprintf (val, sizeof(val), "%f", value);
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
		Z_Free (var->string);
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = atof(var->string);
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
		!(Com_ClientState() >= 3 /* ca_connected */) && !Com_ServerState())
		return;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!(var->flags & CVAR_CHEAT))
			continue;
		Z_Free (var->string);
		var->string = CopyString(var->dvalue);
		var->value = atof(var->string);
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
		return false;
		
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Com_Printf ("\"%s\" is \"%s%s\" default: \"%s%s\"\n", v->name, 
			v->string, S_COLOR_WHITE, v->dvalue, S_COLOR_WHITE);
		if (v->latched_string)
			Com_Printf ("latched: \"%s\"\n", v->latched_string);
		return true;
	}

	Cvar_Set (v->name, Cmd_Argv(1));
	return true;
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

	Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), CVAR_ARCHIVE);
}

void Cvar_Sets_f (void)
{
	if (Cmd_Argc() != 3)
	{
		Com_Printf ("usage: sets <variable> <value>\n");
		return;
	}

	Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), CVAR_SERVERINFO);
}

void Cvar_Setu_f (void)
{
	if (Cmd_Argc() != 3)
	{
		Com_Printf ("usage: setu <variable> <value>\n");
		return;
	}

	Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), CVAR_USERINFO);
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

	Cvar_Set (var->name, var->value ? "0" : "1");
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
			Com_sprintf (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->string);
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

	i = 0;
	for (var = cvar_vars ; var ; var = var->next, i++)
	{
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
		Com_Printf (" %s \"%s\"\n", var->name, var->string);
	}
	Com_Printf ("%i cvars\n", i);
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
Cvar_CompleteCountPossible (char *partial)
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
char **Cvar_CompleteBuildList (char *partial)
{
	cvar_t		*v;
	int			len;
	int			bpos = 0;
	int			sizeofbuf = (Cvar_CompleteCountPossible (partial) + 1) * sizeof (char *);
	char		**buf;

	len = strlen (partial);
	buf = Q_malloc (sizeofbuf + sizeof (char *));

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
