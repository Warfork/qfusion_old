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

#include "qcommon.h"
#include "unzip.h"

// if a packfile directory differs from this, it is assumed to be hacked
// Full version
#define	PAK0_CHECKSUM	0x40e614e0

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/


//
// in memory
//
typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	unzFile handle;
	int		numfiles;
} pack_t;

char	fs_gamedir[MAX_OSPATH];
cvar_t	*fs_basedir;
cvar_t	*fs_cddir;
cvar_t	*fs_gamedirvar;

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack;		// only one of filename / pack will be used
	struct  searchpath_s *next;
} searchpath_t;

searchpath_t	*fs_searchpaths;
searchpath_t	*fs_base_searchpaths;	// without gamedirs

typedef struct filehandle_s
{
	unzFile		zipFile;
	FILE		*f;
	qboolean	occupied;
} filehandle_t;
 
#define MAX_HANDLES		1000

filehandle_t fs_filehandles[MAX_HANDLES];
	
/*

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

*/

int FS_FileHandle (void)
{
	int i;

	for (i = 0; i < MAX_HANDLES; i++)
	{
		if (!fs_filehandles[i].occupied)
		{
			fs_filehandles[i].occupied = true;
			return i+1;
		}
	}

	Com_Error (ERR_FATAL, "No free file handles");
	return 0;
}

void FS_FCloseFile (int f)
{
	if (!fs_filehandles[--f].occupied)
		return;

	fs_filehandles[f].occupied = false;

	if (fs_filehandles[f].f)
	{
		fclose (fs_filehandles[f].f);
		fs_filehandles[f].f = NULL;
	}
	
	if (fs_filehandles[f].zipFile)
	{
		unzCloseCurrentFile (fs_filehandles[f].zipFile);
		fs_filehandles[f].zipFile = 0;
	}
}


/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void	FS_CreatePath (char *path)
{
	char	*ofs;
	
	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_Mkdir (path);
			*ofs = '/';
		}
	}
}

/*
===========
FS_FileSize
===========
*/
int FS_FileSize (char *filename)
{
	int file;
	
	FS_FOpenFile (filename, &file);
	file--;

	if (fs_filehandles[file].f)
	{
		int		pos;
		int		end;

		pos = ftell (fs_filehandles[file].f);
		fseek (fs_filehandles[file].f, 0, SEEK_END);
		end = ftell (fs_filehandles[file].f);
		fseek (fs_filehandles[file].f, pos, SEEK_SET);

		fs_filehandles[file].occupied = false;
		fclose (fs_filehandles[file].f);
		fs_filehandles[file].f = NULL;
		return end;
	}

	if (fs_filehandles[file].zipFile)
	{
		unz_file_info info;

		if (unzGetCurrentFileInfo (fs_filehandles[file].zipFile, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
			Com_Error (ERR_FATAL, "Couldn't get size of %s", filename);

		fs_filehandles[file].occupied = false;
		unzCloseCurrentFile (fs_filehandles[file].zipFile);
		fs_filehandles[file].zipFile = 0;
		return info.uncompressed_size;
	}

	return -1;
}

/*
===========
FS_FOpenFile

Finds the file in the search path.
returns filesize and an open FILE *
Used for streaming data out of either a pak file or
a seperate file.
===========
*/
int file_from_pak = 0;

int FS_FOpenFile (char *filename, int *file)
{
	searchpath_t	*search;
	char			netpath[MAX_OSPATH];
	pack_t			*pak;
	int				fhandle = FS_FileHandle ();

	file_from_pak = 0;
	*file = fhandle--;

//
// search through the path, one element at a time
//
	for (search = fs_searchpaths ; search ; search = search->next)
	{
	// is the element a pak file?
		if (search->pack)
		{
		// look through all the pak file elements
			pak = search->pack;

			if (unzLocateFile(pak->handle, filename, 2) == UNZ_OK)
			{	// found it!
				if (unzOpenCurrentFile (pak->handle) == UNZ_OK)
				{
					unz_file_info info;

					file_from_pak = 1;

					Com_DPrintf ("PackFile: %s : %s\n",pak->filename, filename);

					if (unzGetCurrentFileInfo (pak->handle, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
						Com_Error (ERR_FATAL, "Couldn't get size of %s in %s", filename, pak->filename);

					fs_filehandles[fhandle].zipFile = pak->handle;
					fs_filehandles[fhandle].f = NULL;

					return info.uncompressed_size;
				}
			}
		}
		else
		{		
	// check a file in the directory tree
			int		pos;
			int		end;
			FILE	*f;

			Com_sprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);
			
			f = fopen (netpath, "rb");
			if (!f)
				continue;
			
			Com_DPrintf ("FindFile: %s\n",netpath);

			pos = ftell (f);
			fseek (f, 0, SEEK_END);
			end = ftell (f);
			fseek (f, pos, SEEK_SET);

			fs_filehandles[fhandle].zipFile = 0;
			fs_filehandles[fhandle].f = f;

			return end;
		}
	}

	Com_DPrintf ("FindFile: can't find %s\n", filename);
	
	fs_filehandles[fhandle].occupied = false;
	*file = 0;
	return -1;
}

/*
==============
FS_FileExists

==============
*/
qboolean FS_FileExists (char *path)
{
	int f;

	FS_FOpenFile (path, &f);

	if (f)
	{
		FS_FCloseFile (f);
		return true;
	}

	return false;
}

/*
=================
FS_ReadFile

Properly handles partial reads
=================
*/
// Vic
// void CDAudio_Stop(void);
#define	MAX_READ	0x10000		// read in blocks of 64k

void FS_Read (void *buffer, int len, int f)
{
	int		block, remaining;
	int		read;
	byte	*buf;
	int		tries;

	buf = (byte *)buffer;

	// read in chunks for progress bar
	remaining = len;
	tries = 0;
	f--;

	if (fs_filehandles[f].zipFile)
	{
		read = unzReadCurrentFile (fs_filehandles[f].zipFile, buf, len);

		if (read == -1)
			Com_Error (ERR_FATAL, "FS_ReadFromFile: -1 bytes read");
	}
	else 
	{
		while (remaining)
		{
			block = remaining;
			if (block > MAX_READ)
				block = MAX_READ;
			read = fread (buf, 1, block, fs_filehandles[f].f);
			if (read == 0)
			{
				// we might have been trying to read from a CD
				if (!tries)
					tries = 1;
				else
					Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
			}

			if (read == -1)
				Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");

			// do some progress bar thing here...

			remaining -= read;
			buf += read;
		}
	}
}

/*
============
FS_LoadFile

Filename are reletive to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_LoadFile (char *path, void **buffer)
{
	byte	*buf;
	int		len, fhandle;

	buf = NULL;	// quiet compiler warning

// look for it in the filesystem or pack files
	len = FS_FOpenFile (path, &fhandle);
	if (!fhandle)
	{
		if (buffer)
			*buffer = NULL;
		return -1;
	}
	
	if (!buffer)
	{
		FS_FCloseFile (fhandle);
		return len;
	}

	buf = Z_Malloc(len);
	*buffer = buf;

	FS_Read (buf, len, fhandle);
	FS_FCloseFile (fhandle);

	return len;
}


/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile (void *buffer)
{
	Z_Free (buffer);
}

/*
=================
FS_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPackFile (char *packfile)
{
	pack_t			*pack;

	pack = Z_Malloc (sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = unzOpen (packfile);

	if (!pack->handle)
	{
		Z_Free (pack);
		Com_Error (ERR_FATAL, "%s is not a packfile", packfile);
	}

	pack->numfiles = Unz_NumEntries (pack->handle);

	Com_Printf ("Added packfile %s (%i files)\n", packfile, pack->numfiles);
	return pack;
}


/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
char **FS_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave );

void FS_AddGameDirectory (char *dir)
{
	int				numpaks;
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];
	char			**paknames;

	strcpy (fs_gamedir, dir);

	//
	// add the directory to the search path
	//
	search = Z_Malloc (sizeof(searchpath_t));
	strcpy (search->filename, dir);
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	//
	// add any pak files
	//
	Com_sprintf (pakfile, sizeof(pakfile), "%s/*.pk3", dir);

 	if ( ( paknames = FS_ListFiles( pakfile, &numpaks, 0, 0 ) ) != 0 )
	{
		int i;
		
		for ( i = 0; i < numpaks-1; i++ )
		{
			pak = FS_LoadPackFile (paknames[i]);
			if (!pak)
				continue;
			search = Z_Malloc (sizeof(searchpath_t));
			search->pack = pak;
			search->next = fs_searchpaths;
			fs_searchpaths = search;
			
			free( paknames[i] );
		}

		free( paknames );
	}
}

/*
============
FS_Gamedir

Called to find where to write a file (demos, savegames, etc)
============
*/
char *FS_Gamedir (void)
{
	if (*fs_gamedir)
		return fs_gamedir;
	else
		return BASEDIRNAME;
}

/*
=============
FS_ExecAutoexec
=============
*/
void FS_ExecAutoexec (void)
{
	char *dir;
	char name [MAX_QPATH];

	dir = Cvar_VariableString("fs_gamedir");
	if (*dir)
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, dir); 
	else
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, BASEDIRNAME); 
	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
		Cbuf_AddText ("exec autoexec.cfg\n");
	Sys_FindClose();
}


/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir (char *dir)
{
	searchpath_t	*next;

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	//
	// free up any current game dir info
	//
	while (fs_searchpaths != fs_base_searchpaths)
	{
		if (fs_searchpaths->pack) 
		{
			unzCloseCurrentFile (fs_searchpaths->pack->handle);
			Z_Free (fs_searchpaths->pack);
		}

		next = fs_searchpaths->next;
		Z_Free (fs_searchpaths);
		fs_searchpaths = next;
	}

	//
	// flush all data, so it will be forced to reload
	//
	if (dedicated && !dedicated->value)
		Cbuf_AddText ("vid_restart\nsnd_restart\n");

	Com_sprintf (fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);

	if (!strcmp(dir,BASEDIRNAME) || (*dir == 0))
	{
		Cvar_FullSet ("fs_gamedir", "", CVAR_SERVERINFO|CVAR_NOSET);
		Cvar_FullSet ("fs_game", "", CVAR_LATCH|CVAR_SERVERINFO);
	}
	else
	{
		Cvar_FullSet ("fs_gamedir", dir, CVAR_SERVERINFO|CVAR_NOSET);
		if (fs_cddir->string[0])
			FS_AddGameDirectory (va("%s/%s", fs_cddir->string, dir) );
		FS_AddGameDirectory (va("%s/%s", fs_basedir->string, dir) );
	}
}

/*
** FS_ListFiles
*/
char **FS_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave )
{
	char *s;
	int nfiles = 0;
	char **list = 0;

	s = Sys_FindFirst( findname, musthave, canthave );
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
			nfiles++;
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	if ( !nfiles )
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = Q_malloc( sizeof( char * ) * nfiles );
	memset( list, 0, sizeof( char * ) * nfiles );

	s = Sys_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
		{
			list[nfiles] = strdup( s );
#ifdef _WIN32
			strlwr( list[nfiles] );
#endif
			nfiles++;
		}
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	return list;
}

/*
** FS_Dir_f
*/
void FS_Dir_f( void )
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	char	**dirnames;
	int		ndirs;

	if ( Cmd_Argc() != 1 )
	{
		strcpy( wildcard, Cmd_Argv( 1 ) );
	}

	while ( ( path = FS_NextPath( path ) ) != NULL )
	{
		char *tmp = findname;

		Com_sprintf( findname, sizeof(findname), "%s/%s", path, wildcard );

		while ( *tmp != 0 )
		{
			if ( *tmp == '\\' ) 
				*tmp = '/';
			tmp++;
		}
		Com_Printf( "Directory of %s\n", findname );
		Com_Printf( "----\n" );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, 0, 0 ) ) != 0 )
		{
			int i;

			for ( i = 0; i < ndirs-1; i++ )
			{
				if ( strrchr( dirnames[i], '/' ) )
					Com_Printf( "%s\n", strrchr( dirnames[i], '/' ) + 1 );
				else
					Com_Printf( "%s\n", dirnames[i] );

				free( dirnames[i] );
			}
			free( dirnames );
		}
		Com_Printf( "\n" );
	}
}


/*
================
FS_GetFileList
================
*/
int FS_GetFileList (const char *dir, const char *extension, char *str, int bufsize)
{
	int len = 0, alllen = 0;
	int state = 0;
	int found = 0, allfound = 0;
	searchpath_t	*search;
	pack_t	*pak;

	for (search = fs_searchpaths ; search ; search = search->next)
	{
	// is the element a pak file?
		if (search->pack)
		{
		// look through all the pak file elements
			pak = search->pack;

			found = Unz_GetStringForDir(pak->handle, dir, extension, str + alllen, bufsize - alllen, &len);

			if (found)
			{
				allfound += found;
				state = 1;
				alllen += len + 1;
			}
		}
	}

	return allfound;
}

/*
============
FS_Path_f

============
*/
void FS_Path_f (void)
{
	searchpath_t	*s;

	Com_Printf ("Current search path:\n");
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s == fs_base_searchpaths)
			Com_Printf ("----------\n");
		if (s->pack)
			Com_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf ("%s\n", s->filename);
	}
}

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char *FS_NextPath (char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	if (!prevpath)
		return fs_gamedir;

	prev = fs_gamedir;
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
			continue;
		if (prevpath == prev)
			return s->filename;
		prev = s->filename;
	}

	return NULL;
}


/*
================
FS_InitFilesystem
================
*/
void FS_InitFilesystem (void)
{
	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("dir", FS_Dir_f );

	//
	// basedir <path>
	// allows the game to run from outside the data tree
	//
	fs_basedir = Cvar_Get ("basedir", ".", CVAR_NOSET);

	//
	// cddir <path>
	// Logically concatenates the cddir after the basedir for 
	// allows the game to run from outside the data tree
	//
	fs_cddir = Cvar_Get ("cddir", "", CVAR_NOSET);
	if (fs_cddir->string[0])
		FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_cddir->string) );

	//
	// start up with baseq3 by default
	//
	FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_basedir->string) );

	// any set gamedirs will be freed up to here
	fs_base_searchpaths = fs_searchpaths;

	// check for game override
	fs_gamedirvar = Cvar_Get ("fs_game", "", CVAR_LATCH|CVAR_SERVERINFO);
	if (fs_gamedirvar->string[0])
		FS_SetGamedir (fs_gamedirvar->string);
}


