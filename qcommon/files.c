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

#include "zlib.h"
#include "qcommon.h"

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

#define FS_ZIP_BUFSIZE				0x00000800

#define FS_ZIP_BUFREADCOMMENT		0x00000400
#define FS_ZIP_SIZELOCALHEADER		0x0000001e
#define FS_ZIP_SIZECENTRALDIRITEM	0x0000002e

#define FS_ZIP_LOCALHEADERMAGIC		0x04034b50
#define FS_ZIP_CENTRALHEADERMAGIC	0x02014b50
#define FS_ZIP_ENDHEADERMAGIC		0x06054b50

typedef struct
{
	unsigned char readBuffer[FS_ZIP_BUFSIZE];	// internal buffer for compressed data
	z_stream	zstream;						// zLib stream structure for inflate
	unsigned	compressedSize;
	unsigned	restReadCompressed;				// number of bytes to be decompressed
} zipEntry_t;

#define FS_PACKFILE_DEFLATED		1
#define FS_PACKFILE_COHERENT		2

typedef struct packfile_s {
	char		*name;
	unsigned	flags;
    unsigned	compressedSize;		// compressed size
    unsigned	uncompressedSize;	// uncompressed size
    unsigned	offset;				// relative offset of local header
	struct packfile_s *hashNext;
} packfile_t;

//
// in memory
//
typedef struct
{
	char		filename[MAX_OSPATH];
	int			numFiles;
	int			hashSize;
	void		*sysHandle;
	packfile_t	*files;
	char		*fileNames;
	packfile_t	**filesHash;
} pack_t;

typedef struct filehandle_s
{
	FILE		*fstream;
	unsigned	offset;
    unsigned	uncompressedSize;		// uncompressed size
	unsigned	restReadUncompressed;	// number of bytes to be obtained after decompession
	zipEntry_t	*zipEntry;
	struct filehandle_s *prev, *next;
} filehandle_t;

typedef struct searchpath_s
{
	char		filename[MAX_OSPATH];
	pack_t		*pack;					// only one of filename / pack will be used
	struct		searchpath_s *next;
} searchpath_t;

typedef struct
{
	char		*name;
	searchpath_t	*searchPath;
} searchfile_t;

static searchfile_t *fs_searchfiles;
static int fs_numsearchfiles;
static int fs_cursearchfiles;

char			fs_gamedir[MAX_OSPATH];
cvar_t			*fs_basepath;
cvar_t			*fs_basedir;
cvar_t			*fs_cdpath;
cvar_t			*fs_gamedirvar;

static searchpath_t	*fs_searchpaths;
static searchpath_t	*fs_base_searchpaths;	// without gamedirs

static mempool_t	*fs_mempool;

#define FS_MAX_BLOCK_SIZE	0x10000
#define FS_MAX_HASH_SIZE	1024
#define FS_MAX_HANDLES		1024

static filehandle_t fs_filehandles[FS_MAX_HANDLES];
static filehandle_t fs_filehandles_headnode, *fs_free_filehandles;

// we mostly read from one file at a time, so keep a global copy of one
// zipEntry to save on malloc calls and linking in FS_FOpenFile
static zipEntry_t	fs_globalZipEntry;
static filehandle_t *fs_globalZipEntryUser;

/*

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

*/

/*
===========
FS_OpenFileHandle
===========
*/
int FS_OpenFileHandle( void )
{
	filehandle_t *fh;

	if ( !fs_free_filehandles )
		Sys_Error( "FS_OpenFileHandle: no free file handles" );

	fh = fs_free_filehandles;
	fs_free_filehandles = fh->next;

	// put the handle at the start of the list
	fh->prev = &fs_filehandles_headnode;
	fh->next = fs_filehandles_headnode.next;
	fh->next->prev = fh;
	fh->prev->next = fh;

	return (fh - fs_filehandles) + 1;
}

/*
===========
FS_FileHandleForNum
===========
*/
filehandle_t *FS_FileHandleForNum( int file )
{
	if( file < 1 || file > FS_MAX_HANDLES )
		Sys_Error( "FS_FileHandleForNum: bad handle" );
	if( !fs_filehandles[--file].fstream )
		Sys_Error( "FS_FileHandleForNum: bad handle" );
	return &fs_filehandles[file];
}

/*
==============
FS_CloseFileHandle
==============
*/
void FS_CloseFileHandle( filehandle_t *fh )
{
	// remove from linked open list
	fh->prev->next = fh->next;
	fh->next->prev = fh->prev;

	// insert into linked free list
	fh->next = fs_free_filehandles;
	fs_free_filehandles = fh;
}

/*
===========
FS_PackHashKey
===========
*/
static unsigned FS_PackHashKey( const char *str, int hashSize )
{
	int c;
	unsigned hashval = 0;

	while ( (c = *str++) != 0 ) {
		if( c == '\\' )
			c = '/';
		hashval = hashval*37 + tolower( c );
	}
	return hashval & (hashSize - 1);
}

/*
==============
FS_FileExists
==============
*/
static int FS_FileExists( const char *path )
{
	searchpath_t	*search;
	unsigned		hashKey;

	// search through the path, one element at a time
	for( search = fs_searchpaths; search; search = search->next ) {
		if( search->pack ) {		// is the element a pak file?
			pack_t		*pak = search->pack;
			packfile_t	*pakFile;

			hashKey = FS_PackHashKey( path, pak->hashSize );

			// look through all the pak file elements
			for( pakFile = pak->filesHash[hashKey]; pakFile; pakFile = pakFile->hashNext )	{
				if( !Q_stricmp( path, pakFile->name ) )
					return pakFile->uncompressedSize;	// found it!
			}
		} else {		
			// check a file in the directory tree
			FILE	*f;
			int		pos, end;
			char	netpath[MAX_OSPATH];

			Q_snprintfz( netpath, sizeof(netpath), "%s/%s", search->filename, path );
			f = fopen( netpath, "rb" );
			if( !f )
				continue;

			pos = ftell( f );
			fseek( f, 0, SEEK_END );
			end = ftell( f );
			fseek( f, pos, SEEK_SET );

			fclose( f );
			return end;
		}
	}

	return -1;
}

/*
===========
FS_FOpenFile

Finds the file in the search path. Returns filesize and an open handle
Used for streaming data out of either a pak file or a separate file.
===========
*/
int file_from_pak;

static unsigned FS_PK3CheckFileCoherency( FILE *f, packfile_t *file );

int FS_FOpenFile( const char *filename, int *filenum, int mode )
{
	FILE			*f;
	searchpath_t	*search;
	filehandle_t	*file;
	unsigned		hashKey;
	char			netpath[MAX_OSPATH];

	if( filenum )
		*filenum = 0;

	if( !filename || !filename[0] )
		return -1;
	if( strstr( filename, ".." )
		|| *filename == '.'		// leading dot is no good
		|| strstr( filename, "//" )
		|| strstr( filename, "\\\\" ) )
		return -1;
	if( *filename == '/' || *filename == '\\' )
		filename++;

	if( !filenum ) {
		if( mode == FS_READ )
			return FS_FileExists( filename );
		return -1;
	}

	if( mode == FS_WRITE ) {
		Q_snprintfz( netpath, sizeof(netpath), "%s/%s", FS_Gamedir (), filename );
		FS_CreatePath( netpath );

		f = fopen( netpath, "wb" );
		if( !f )
			return -1;

		*filenum = FS_OpenFileHandle ();
		file = &fs_filehandles[*filenum - 1];
		file->fstream = f;
		file->offset = 0;
		file->zipEntry = NULL;
		file->uncompressedSize = 0;
		file->restReadUncompressed = 0;
		return 0;
	} else if( mode == FS_APPEND ) {
		int		pos, end;

		Q_snprintfz( netpath, sizeof(netpath), "%s/%s", FS_Gamedir (), filename );
		FS_CreatePath( netpath );

		f = fopen( netpath, "ab" );
		if( !f )
			return -1;

		pos = ftell( f );
		fseek( f, 0, SEEK_END );
		end = ftell( f );
		fseek( f, pos, SEEK_SET );

		*filenum = FS_OpenFileHandle ();
		file = &fs_filehandles[*filenum - 1];
		file->fstream = f;
		file->zipEntry = NULL;
		file->offset = 0;
		file->uncompressedSize = end;
		file->restReadUncompressed = 0;
		return end;
	}

	// search through the path, one element at a time
	for( search = fs_searchpaths; search; search = search->next ) {
		if( search->pack ) {		// is the element a pak file?
			pack_t		*pak = search->pack;
			packfile_t	*pakFile;

			hashKey = FS_PackHashKey( filename, pak->hashSize );

			for( pakFile = pak->filesHash[hashKey]; pakFile; pakFile = pakFile->hashNext ) {
				// look through all the pak file elements
				if( !Q_stricmp( filename, pakFile->name ) ) {	// found it!
					*filenum = FS_OpenFileHandle ();
					file = &fs_filehandles[*filenum - 1];
					file->fstream = fopen( pak->filename, "rb" );
					file->uncompressedSize = pakFile->uncompressedSize;
					file->restReadUncompressed = pakFile->uncompressedSize;
					file->zipEntry = NULL;

					if( !(pakFile->flags & FS_PACKFILE_COHERENT) ) {
						unsigned offset = FS_PK3CheckFileCoherency( file->fstream, pakFile );
						if( !offset ) {
							Com_DPrintf( "FS_FOpenFile: can't get proper offset for %s\n", filename );
							FS_FCloseFile( *filenum );
							*filenum = 0;
							return -1;
						}
						pakFile->offset += offset;
						pakFile->flags |= FS_PACKFILE_COHERENT;
					}
					file->offset = pakFile->offset;

					if( pakFile->flags & FS_PACKFILE_DEFLATED ) {
						if( fs_globalZipEntryUser )
							file->zipEntry = Mem_Alloc( fs_mempool, sizeof(zipEntry_t) );
						else {
							// reset zstream here too, Mem_Alloc does it for us above
							memset( &fs_globalZipEntry.zstream, 0, sizeof( fs_globalZipEntry.zstream ) );

							file->zipEntry = &fs_globalZipEntry;
							fs_globalZipEntryUser = file;
						}

						file->zipEntry->compressedSize = pakFile->compressedSize;
						file->zipEntry->restReadCompressed = pakFile->compressedSize;

						// windowBits is passed < 0 to tell that there is no zlib header.
						// Note that in this case inflate *requires* an extra "dummy" byte
						// after the compressed stream in order to complete decompression and
						// return Z_STREAM_END. We don't want absolutely Z_STREAM_END because we known the 
						// size of both compressed and uncompressed data
						if( inflateInit2( &file->zipEntry->zstream, -MAX_WBITS ) != Z_OK ) {
							Com_DPrintf( "FS_FOpenFile: can't inflate %s\n", filename );
							FS_FCloseFile( *filenum );
							*filenum = 0;
							return -1;
						}
					}

					if( fseek( file->fstream, file->offset, SEEK_SET ) != 0 ) {
						Com_DPrintf( "FS_FOpenFile: can't inflate %s\n", filename );
						FS_FCloseFile( *filenum );
						*filenum = 0;
						return -1;
					}

					file_from_pak = 1;
					Com_DPrintf( "PackFile: %s : %s\n", pak->filename, filename );
					return pakFile->uncompressedSize;
				}
			}
		} else {		
			// check a file in the directory tree
			int		pos, end;

			Q_snprintfz( netpath, sizeof(netpath), "%s/%s", search->filename, filename );
			f = fopen( netpath, "rb" );
			if( !f )
				continue;

			pos = ftell( f );
			fseek( f, 0, SEEK_END );
			end = ftell( f );
			fseek( f, pos, SEEK_SET );

			*filenum = FS_OpenFileHandle ();
			file = &fs_filehandles[*filenum - 1];
			file->offset = 0;
			file->zipEntry = NULL;
			file->fstream = f;
			file->uncompressedSize = end;
			file->restReadUncompressed = end;

			file_from_pak = 0;
			Com_DPrintf( "FS_FOpenFile: %s\n", netpath );
			return end;
		}
	}

	*filenum = 0;
	file_from_pak = 0;
	Com_DPrintf( "FS_FOpenFile: can't find %s\n", filename );
	return -1;
}

/*
==============
FS_FCloseFile
==============
*/
void FS_FCloseFile( int file )
{
	filehandle_t *fh;

	if( !file )
		return;		// return silently

	fh = FS_FileHandleForNum( file );
	if( fh->zipEntry ) {
		inflateEnd( &fh->zipEntry->zstream );
		if( fh != fs_globalZipEntryUser )
			Mem_Free( fh->zipEntry );
		else
			fs_globalZipEntryUser = NULL;
		fh->zipEntry = NULL;
	}
	if( fh->fstream ) {
		fclose( fh->fstream );
		fh->fstream = NULL;
	}

	FS_CloseFileHandle( fh );
}

/*
=================
FS_ReadPK3File

Properly handles partial reads, used by FS_Read and FS_Seek
=================
*/
static int FS_ReadPK3File( qbyte *buf, size_t len, filehandle_t *fh )
{
	zipEntry_t *zipEntry;
	size_t block, total;
	int	read, error, totalOutBefore;

	zipEntry = fh->zipEntry;
	zipEntry->zstream.next_out = buf;
	zipEntry->zstream.avail_out = len;

	total = 0;
	do {	// read in chunks
		if( !zipEntry->zstream.avail_in && zipEntry->restReadCompressed ) {
			block = min( zipEntry->restReadCompressed, FS_ZIP_BUFSIZE );

			read = fread( zipEntry->readBuffer, 1, block, fh->fstream );
			if( read == 0 ) {	// we might have been trying to read from a CD
				read = fread( zipEntry->readBuffer, 1, block, fh->fstream );
				if( read == 0 )
					read = -1;
			}
			if( read == -1 )
				Sys_Error( "FS_Read: can't read %i bytes", block );

			zipEntry->restReadCompressed -= block;
			zipEntry->zstream.next_in = zipEntry->readBuffer;
			zipEntry->zstream.avail_in = block;
		}

		totalOutBefore = zipEntry->zstream.total_out;
		error = inflate( &zipEntry->zstream, Z_SYNC_FLUSH );
		total += (zipEntry->zstream.total_out - totalOutBefore);

		if( error == Z_STREAM_END )
			break;
		if( error != Z_OK )
			Sys_Error( "FS_ReadPK3File: can't inflate file" );
	} while( zipEntry->zstream.avail_out > 0 );

	return total;
}

/*
=================
FS_ReadFile

Properly handles partial reads
=================
*/
static int FS_ReadFile( qbyte *buf, size_t len, filehandle_t *fh )
{
	size_t block, total;
	int	read;

	total = 0;
	do {	// read in chunks
		block = min( len, FS_MAX_BLOCK_SIZE );

		read = fread( buf, 1, block, fh->fstream );
		if( read == 0 ) {	// we might have been trying to read from a CD
			read = fread( buf, 1, block, fh->fstream );
			if( read == 0 )
				read = -1;
		}
		if( read == -1 )
			Sys_Error( "FS_Read: could not read %i bytes", block );

		// do some progress bar thing here...
		len -= block;
		buf += block;
		total += block;
	} while( len > 0 );

	return total;
}

/*
=================
FS_Read

Properly handles partial reads
=================
*/
int FS_Read( void *buffer, size_t len, int file )
{
	filehandle_t	*fh;
	size_t			total;

	fh = FS_FileHandleForNum( file );

	// read in chunks for progress bar
	if( len > fh->restReadUncompressed )
		len = fh->restReadUncompressed;
	if( !len || !buffer )
		return 0;

	if( fh->zipEntry )
		total = FS_ReadPK3File( ( qbyte * )buffer, len, fh );
	else
		total = FS_ReadFile( ( qbyte * )buffer, len, fh );
	
	fh->restReadUncompressed -= total;

	return total;
}

/*
============
FS_Write

Properly handles partial writes
============
*/
int FS_Write( const void *buffer, size_t len, int file )
{
	int write;
	filehandle_t *fh;
	int	block, remaining, total;
	qbyte *buf;

	fh = FS_FileHandleForNum( file );
	if( fh->zipEntry )
		Sys_Error( "FS_Write: writing to compressed file" );

	buf = ( qbyte * )buffer;
	total = 0;
	remaining = len;
	if( !remaining || !buf )
		return 0;

	do
	{
		block = remaining;
		if( block > FS_MAX_BLOCK_SIZE )
			block = FS_MAX_BLOCK_SIZE;

		write = fwrite( buf, 1, block, fh->fstream );
		if( write == 0 ) {		// try once more
			write = fwrite( buffer, 1, block, fh->fstream );
			if( write == 0 )
				write = -1;
		}
		if ( write == -1 )
			Sys_Error( "FS_Read: can't write %i bytes", block );

		remaining -= block;
		buf += block;
		total += block;
	} while( remaining > 0 );

	fh->uncompressedSize += total;
	return total;
}

/*
============
FS_Tell
============
*/
int FS_Tell( int file )
{
	filehandle_t *fh;

	fh = FS_FileHandleForNum( file );

	return fh->uncompressedSize - fh->restReadUncompressed;
}

/*
============
FS_Tell
============
*/
int	FS_Seek( int file, int offset, int whence )
{
	filehandle_t	*fh;
	zipEntry_t		*zipEntry;
	int				error, currentOffset;
	size_t			remaining, block;
	qbyte			buf[FS_ZIP_BUFSIZE * 8];

	fh = FS_FileHandleForNum( file );
	currentOffset = fh->uncompressedSize - fh->restReadUncompressed;

	if( whence == FS_SEEK_CUR )
		offset += currentOffset;
	else if( whence == FS_SEEK_END )
		offset += fh->uncompressedSize;
	else if( whence != FS_SEEK_SET )
		return -1;

	// clamp so we don't get out of bounds
	clamp( offset, 0, fh->uncompressedSize );
	if( offset == currentOffset )
		return 0;

	if( !fh->zipEntry ) {
		fh->restReadUncompressed = fh->uncompressedSize - offset;
		return fseek( fh->fstream, fh->offset + offset, SEEK_SET );
	}

	// compressed files, doh
	zipEntry = fh->zipEntry;

	if( offset > currentOffset ) {
		offset -= currentOffset;
	} else {
		if( fseek( fh->fstream, fh->offset, SEEK_SET ) != 0 )
			return -1;

		zipEntry->zstream.next_in = zipEntry->readBuffer;
		zipEntry->zstream.avail_in = 0;
		error = inflateReset( &zipEntry->zstream );
		if( error != Z_OK )
			Sys_Error( "FS_Seek: can't inflateReset file" );

		fh->restReadUncompressed = fh->uncompressedSize;
		zipEntry->restReadCompressed = zipEntry->compressedSize;
	}

	remaining = offset;
	do {
		block = min( remaining, sizeof( buf ) );

		FS_ReadPK3File( buf, block, fh );

		remaining -= block;
	} while( remaining > 0 );

	fh->restReadUncompressed -= offset;
	return 0;
}

/*
============
FS_Eof
============
*/
int	FS_Eof( int file )
{
	filehandle_t	*fh;

	fh = FS_FileHandleForNum( file );

	return !fh->restReadUncompressed;
}

/*
============
FS_FFlush
============
*/
int	FS_Flush( int file )
{
	filehandle_t	*fh;

	fh = FS_FileHandleForNum( file );

	return fflush( fh->fstream );
}

/*
============
FS_LoadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_LoadFile( const char *path, void **buffer, void *stack, size_t stackSize )
{
	qbyte	*buf;
	int		len, fhandle;

	buf = NULL;	// quiet compiler warning

	// look for it in the filesystem or pack files
	len = FS_FOpenFile( path, &fhandle, FS_READ );
	if( !fhandle ) {
		if( buffer )
			*buffer = NULL;
		return -1;
	}
	
	if( !buffer ) {
		FS_FCloseFile( fhandle );
		return len;
	}

	if( stack && (stackSize > len) )
		buf = stack;
	else
		buf = Mem_TempMallocExt( len + 1, 0 );
	buf[len] = 0;
	*buffer = buf;

	FS_Read( buf, len, fhandle );
	FS_FCloseFile( fhandle );

	return len;
}

/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile( void *buffer ) {
	Mem_TempFree( buffer );
}

/*
================
FS_CopyFile

FIXME: use FS_FOpenFile, FS_Read and FS_Write???
================
*/
void FS_CopyFile( const char *src, const char *dst )
{
	int		l;
	FILE	*f1, *f2;
	qbyte	buffer[FS_MAX_BLOCK_SIZE];

	Com_DPrintf( "FS_CopyFile (%s, %s)\n", src, dst );

	f1 = fopen( src, "rb" );
	if( !f1 )
		return;

	FS_CreatePath( dst );

	f2 = fopen( dst, "wb" );
	if( !f2 ) {
		fclose( f1 );
		return;
	}

	while( 1 ) {
		l = fread (buffer, 1, sizeof(buffer), f1);
		if( !l )	// we might have been trying to read from a CD
			l = fread (buffer, 1, sizeof(buffer), f1);
		if( !l )
			break;
		fwrite( buffer, 1, l, f2 );
	}

	fclose( f1 );
	fclose( f2 );
}

/*
================
FS_RemoveFile
================
*/
void FS_RemoveFile( const char *name ) {
	remove( name );
}

/*
================
FS_RenameFile
================
*/
int	FS_RenameFile( const char *src, const char *dst ) {
	return rename( src, dst );
}

/*
=================
FS_PK3SearchCentralDir

Locate the central directory of a zipfile (at the end, just before the global comment)
=================
*/
inline unsigned int LittleLongRaw( const qbyte *raw ) {
	return (raw[3] << 24) | (raw[2] << 16) | (raw[1] << 8) | raw[0];
}

inline unsigned short LittleShortRaw( const qbyte *raw ) {
	return (raw[1] << 8) | raw[0];
}

static unsigned FS_PK3SearchCentralDir( FILE *fin )
{
	unsigned fileSize, backRead;
	unsigned maxBack = 0xffff;	// maximum size of global comment
	unsigned char buf[FS_ZIP_BUFREADCOMMENT+4];

	if( fseek( fin, 0, SEEK_END ) != 0 )
		return 0;

	fileSize = ftell( fin );
	if( maxBack > fileSize )
		maxBack = fileSize;

	backRead = 4;
	while( backRead < maxBack ) {
		unsigned i, readSize, readPos;

		if( backRead + FS_ZIP_BUFREADCOMMENT > maxBack ) 
			backRead = maxBack;
		else
			backRead += FS_ZIP_BUFREADCOMMENT;

		readPos = fileSize - backRead;
		readSize = min( FS_ZIP_BUFREADCOMMENT + 4, backRead );
		if( readSize < 4 )
			continue;

		if( fseek( fin, readPos, SEEK_SET ) != 0 )
			break;
		if( fread( buf, 1, readSize, fin ) != readSize )
			break;

		for( i = readSize - 3; i--; ) {
			// check the magic
			if( LittleLongRaw( buf + i ) == FS_ZIP_ENDHEADERMAGIC )
				return readPos + i;
		}
	}

	return 0;
}

/*
=================
FS_PK3CheckFileCoherency

Read the local header of the current zipfile
Check the coherency of the local header and info in the end of central directory about this file
=================
*/
static unsigned FS_PK3CheckFileCoherency( FILE *f, packfile_t *file )
{
	unsigned flags;
	unsigned char localHeader[31], compressed;

	if( fseek( f, file->offset, SEEK_SET ) != 0 )
		return 0;
	if( fread( localHeader, 1, sizeof( localHeader ), f ) != sizeof( localHeader ) )
		return 0;

	// check the magic
	if( LittleLongRaw( &localHeader[0] ) != FS_ZIP_LOCALHEADERMAGIC )
		return 0;
	compressed = LittleShortRaw( &localHeader[8] );
	if( ( compressed == Z_DEFLATED ) && !( file->flags & FS_PACKFILE_DEFLATED ) )
		return 0;
	else if( !compressed && ( file->flags & FS_PACKFILE_DEFLATED ) )
		return 0;

	flags = LittleShortRaw( &localHeader[6] ) & 8;
	if( ( LittleLongRaw( &localHeader[18] ) != file->compressedSize ) && !flags )
		return 0;
	if( ( LittleLongRaw( &localHeader[22] ) != file->uncompressedSize ) && !flags )
		return 0;

	return FS_ZIP_SIZELOCALHEADER + LittleShortRaw( &localHeader[26] ) + ( unsigned )LittleShortRaw( &localHeader[28] );
}

/*
=================
FS_PK3GetFileInfo

Get Info about the current file in the zipfile, with internal only info
=================
*/
static unsigned FS_PK3GetFileInfo( FILE *f, unsigned pos, unsigned byteBeforeTheZipFile, packfile_t *file, size_t *fileNameLen, int *crc )
{
	size_t sizeRead;
	unsigned compressed;
	unsigned char infoHeader[46];	// we can't use a struct here because of packing

	if( fseek( f, pos, SEEK_SET ) != 0 )
		return 0;
	if( fread( infoHeader, 1, sizeof( infoHeader ), f ) != sizeof( infoHeader ) )
		return 0;

	// check the magic
	if( LittleLongRaw( &infoHeader[0] ) != FS_ZIP_CENTRALHEADERMAGIC )
		return 0;

	compressed = LittleShortRaw( &infoHeader[10] );
	if( compressed && (compressed != Z_DEFLATED) )
		return 0;

	if( crc )
		*crc = LittleLongRaw( &infoHeader[16] );
	if( file ) {
		if( compressed == Z_DEFLATED )
			file->flags |= FS_PACKFILE_DEFLATED;
		file->compressedSize = LittleLongRaw( &infoHeader[20] );
		file->uncompressedSize = LittleLongRaw( &infoHeader[24] );
		file->offset = LittleLongRaw( &infoHeader[42] ) + byteBeforeTheZipFile;
	}

	sizeRead = ( size_t )LittleShortRaw( &infoHeader[28] );
	if( !sizeRead )
		return 0;
	else if( sizeRead > MAX_QPATH - 1 )
		sizeRead = MAX_QPATH - 1;

	if( fileNameLen )
		*fileNameLen = sizeRead;

	if( file ) {
		if( fread( file->name, 1, sizeRead, f ) != sizeRead )
			return 0;
		*(file->name + sizeRead) = 0;
	}

	return FS_ZIP_SIZECENTRALDIRITEM + ( unsigned )LittleShortRaw( &infoHeader[28] ) +
				( unsigned )LittleShortRaw( &infoHeader[30] ) + ( unsigned )LittleShortRaw( &infoHeader[32] );
}

/*
=================
FS_LoadPK3File

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPK3File( const char *packfilename )
{
	int i, hashSize;
	int numFiles;
	size_t namesLen, len;
	pack_t *pack = NULL;
	packfile_t *file;
	FILE *fin = NULL;
	char *names;
	unsigned hashKey;
	unsigned char zipHeader[20];	// we can't use a struct here because of packing
	unsigned offset, centralPos, sizeCentralDir, offsetCentralDir, byteBeforeTheZipFile;
	void *handle = NULL;

	// lock the file for shared reading, don't throw a fatal error if failed for some reason
	handle = Sys_LockFile( packfilename );
	if( handle == NULL )
		goto error;

    fin = fopen( packfilename, "rb" );
	if( fin == NULL )
		goto error;
	centralPos = FS_PK3SearchCentralDir( fin );
	if( centralPos == 0 )
		goto error;
	if( fseek( fin, centralPos, SEEK_SET ) != 0 )
		goto error;
	if( fread( zipHeader, 1, sizeof( zipHeader ), fin ) != sizeof( zipHeader ) )
		goto error;

	// total number of entries in the central dir on this disk
	numFiles = LittleShortRaw( &zipHeader[8] );
	if( !numFiles )
		goto error;
	if( LittleShortRaw( &zipHeader[10] ) != numFiles
		|| LittleShortRaw( &zipHeader[6] ) != 0
		|| LittleShortRaw( &zipHeader[4] ) != 0 )
		goto error;

	// size of the central directory
	sizeCentralDir = LittleLongRaw( &zipHeader[12] );

	// offset of start of central directory with respect to the starting disk number
	offsetCentralDir = LittleLongRaw( &zipHeader[16] );
	if( centralPos < offsetCentralDir + sizeCentralDir )
		goto error;
	byteBeforeTheZipFile = centralPos - offsetCentralDir - sizeCentralDir;

	for( hashSize = 1; ( hashSize <= numFiles ) && (hashSize < FS_MAX_HASH_SIZE); hashSize <<= 1 );

	for( i = 0, namesLen = 0, centralPos = offsetCentralDir + byteBeforeTheZipFile; i < numFiles; i++, centralPos += offset ) {
		offset = FS_PK3GetFileInfo( fin, centralPos, byteBeforeTheZipFile, NULL, &len, NULL );
		if( !offset )
			goto error;		// something wrong occured
		namesLen += len + 1;
	}

	namesLen += 1; // add space for a guard

	pack = Mem_Alloc( fs_mempool, ( sizeof( pack_t ) + numFiles * sizeof( packfile_t ) + namesLen + hashSize * sizeof( packfile_t * ) ) );
	strcpy( pack->filename, packfilename );
	pack->files = ( packfile_t * )( ( qbyte * )pack + sizeof( pack_t ) );
	pack->fileNames = names = ( char * )( ( qbyte * )pack->files + numFiles * sizeof( packfile_t ) );
	pack->filesHash = ( packfile_t ** )( ( qbyte * )names + namesLen );
	pack->numFiles = numFiles;
	pack->hashSize = hashSize;
	pack->sysHandle = handle;

	// add all files to hash table
	for( i = 0, file = pack->files, centralPos = offsetCentralDir + byteBeforeTheZipFile; i < numFiles; i++, file++, centralPos += offset, names += len + 1 ) {
		file->name = names;

		offset = FS_PK3GetFileInfo( fin, centralPos, byteBeforeTheZipFile, file, &len, NULL );

		hashKey = FS_PackHashKey( file->name, hashSize );
		file->hashNext = pack->filesHash[hashKey];
		pack->filesHash[hashKey] = file;
	}

	fclose( fin );
	fin = NULL;

	Com_Printf( "Added pk3 file %s (%i files)\n", pack->filename, pack->numFiles );

	return pack;

error:
	if( fin )
		fclose( fin );
	if( pack )
		Mem_Free( pack );
	if( handle != NULL )
		Sys_UnlockFile( handle ); 

	Com_Printf( "%s is not a valid pk3 file", packfilename );

	return NULL;
}

/*
=================
FS_LoadPakFile

Quick and dirty
=================
*/
#define IDPAKHEADER		(('K'<<24)+('C'<<16)+('A'<<8)+'P')

pack_t *FS_LoadPakFile( const char *packfilename )
{
	int				i, j;
	FILE			*fin = NULL;
	char			*p, *names, dirName[56];
	packfile_t		*file;
	pack_t			*pack = NULL;
	struct { int ident; int	dirofs; int dirlen; } header;
	struct { char name[56]; int filepos, filelen; } *info = NULL;
	struct { char name[56]; } *dirs = NULL;
	unsigned		numFiles, numDirs, maxDirs, hashSize, hashKey, len, namesLen;
	void			*handle = NULL;

	// lock the file for shared reading, don't throw a fatal error if failed for some reason
	handle = Sys_LockFile( packfilename );
	if( handle == NULL )
		goto error;

	fin = fopen( packfilename, "rb" );
	if( !fin )
		goto error;

	fread( &header, 1, sizeof( header ), fin );
	if( LittleLong( header.ident ) != IDPAKHEADER )
		goto error;

	header.dirofs = LittleLong( header.dirofs );
	header.dirlen = LittleLong( header.dirlen );

	numFiles = header.dirlen / sizeof( *info );
	if( !numFiles )
		goto error;

	info = Mem_TempMallocExt( header.dirlen + numFiles * sizeof( *dirs ), 0 );

	numDirs = 0;
	maxDirs = numFiles;
	dirs = Mem_TempMallocExt( maxDirs * sizeof( *dirs ), 1 );

	fseek( fin, header.dirofs, SEEK_SET );
	for( i = 0, namesLen = 0; i < numFiles; i++ ) {
		fread( &info[i], 1, sizeof( *info ), fin );
		len = strlen( info[i].name );
		if( !len )
			goto error;		// something wrong occured

		// add subdirectories too
		strcpy( dirName, info[i].name );
		while( (p = strrchr( dirName, '/' )) ) {
			*(p + 1) = 0;

			for( j = 0; j < numDirs; j++ ) {
				if( !strcmp( dirs[j].name, dirName ) )
					break;
			}
			if( j == numDirs ) {
			    if( numDirs == maxDirs ) {
			        // happens occasionally when there are more directories
			        // (subdirectories) than files (vanilla Quake2 pak2.pak
                    // is a good example)
			        maxDirs++;
			        dirs = Mem_Realloc( dirs, maxDirs * sizeof( *dirs ) );
			    }
				strcpy( dirs[numDirs].name, dirName );
				numDirs++;
				namesLen += strlen( dirName ) + 1;
			}
			*p = 0;        // remove the trailing '/'
		}
		namesLen += len + 1;
	}

	namesLen += 1; // add space for a guard

	for( hashSize = 1; (hashSize <= numFiles) && (hashSize < FS_MAX_HASH_SIZE); hashSize <<= 1 );

	pack = Mem_Alloc( fs_mempool, ( sizeof( pack_t ) + (numFiles + numDirs) * sizeof( packfile_t ) + namesLen + hashSize * sizeof( packfile_t * ) ) );
	strcpy( pack->filename, packfilename );
	pack->files = ( packfile_t * )( ( qbyte * )pack + sizeof( pack_t ) );
	pack->fileNames = names = ( char * )( ( qbyte * )pack->files + (numFiles + numDirs) * sizeof( packfile_t ) );
	pack->filesHash = ( packfile_t ** )( ( qbyte * )names + namesLen );
	pack->numFiles = numFiles + numDirs;
	pack->hashSize = hashSize;
	pack->sysHandle = handle;

	// add all files to hash table
	for( i = 0, file = pack->files; i < numFiles; i++, file++, names += len + 1 ) {
		file->name = names;
		strcpy( file->name, info[i].name );
		len = strlen( file->name );

		file->flags = FS_PACKFILE_COHERENT;
		file->offset = LittleLong( info[i].filepos );
		file->compressedSize = file->uncompressedSize = LittleLong( info[i].filelen );

		hashKey = FS_PackHashKey( file->name, hashSize );
		file->hashNext = pack->filesHash[hashKey];
		pack->filesHash[hashKey] = file;
	}

	// add all dirs then
	for( i = 0; i < numDirs; i++, file++, names += len + 1 ) {
		file->name = names;
		strcpy( file->name, dirs[i].name );
		len = strlen( file->name );
		file->flags = FS_PACKFILE_COHERENT;
	}

	fclose( fin );
	fin = NULL;

	Mem_TempFree( info );
	Mem_TempFree( dirs );
	
	Com_Printf( "Added pak file %s (%i files)\n", pack->filename, pack->numFiles );

	return pack;

error:
	if( fin )
		fclose( fin );
	if( pack )
		Mem_Free( pack );
	if( info )
		Mem_TempFree( info );
	if( dirs )
        Mem_TempFree( dirs );
	if( handle != NULL )
		Sys_UnlockFile( handle ); 

	return NULL;
}

/*
=================
FS_SortStrings
=================
*/
static int FS_SortStrings( const char **first, const char **second ) {
	return Q_stricmp( *first, *second );
}

/*
=================
FS_PathGetFileListExt
=================
*/
static int FS_PathGetFileListExt( searchpath_t *search, const char *dir, const char *extension, searchfile_t *files, size_t size )
{
	int i, found;
	char *p, *s, *name;
	size_t dirlen, extlen, tokenlen;

	if( !size )
		return 0;

	found = 0;
	dirlen = 0;
	extlen = 0;

	if( dir ) {
		dirlen = strlen( dir );
		if( dirlen ) {
			if( dir[dirlen-1] == '/' )
				dirlen--;
		}
	}

	if( extension )
		extlen = strlen( extension );

	if( !search->pack ) {
		size_t			searchlen;
		int				numfiles;
		char			**filenames;
		char			findname[MAX_OSPATH];
		unsigned int	musthave, canthave;

		musthave = 0;
		canthave = SFF_HIDDEN | SFF_SYSTEM;

		Q_snprintfz( findname, sizeof( findname ), "%s/", search->filename );
		searchlen = strlen( findname );

		if( dirlen ) {
			Q_strncatz( findname, dir, sizeof( findname ) );
			searchlen = strlen( findname );

			if( findname[searchlen-1] != '/' ) {
				Q_strncatz( findname, "/", sizeof( findname ) );
				searchlen++;
			}
		}

		if( extlen ) {
			if( extension[0] != '/' ) {
				Q_strncatz( findname, "*", sizeof( findname ) );
				Q_strncatz( findname, extension, sizeof( findname ) );
				canthave |= SFF_SUBDIR;
			} else {
				Q_strncatz( findname, "*.*", sizeof( findname ) );
				musthave |= SFF_SUBDIR;
			}
		} else {
			Q_strncatz( findname, "*.*", sizeof( findname ) );
		}

		if( (filenames = FS_ListFiles( findname, &numfiles, musthave, canthave )) ) {
			for( i = 0; i < numfiles - 1; i++ ) {
				if( found < size ) {
					size_t len = strlen( filenames[i] + searchlen );

					if( (musthave & SFF_SUBDIR) ) {
						if( filenames[i][searchlen+len-1] != '/' ) {
							files[found].name = Mem_ZoneMalloc( len + 2 );
							strcpy( files[found].name, filenames[i] + searchlen );
							files[found].name[len] = '/';
							files[found].name[len+1] = 0;
						} else {
							if( extension && (len <= extlen) ) {
								Mem_ZoneFree( filenames[i] );
								continue;
							}

							files[found].name = CopyString( filenames[i] + searchlen );
						}
					} else {
						files[found].name = CopyString( filenames[i] + searchlen );
					}
					files[found].searchPath = search;
					found++;
				}
				Mem_ZoneFree( filenames[i] );
			}
		}

		return found;
	}

	for( s = search->pack->fileNames; *s; s += tokenlen + 1 ) {
		tokenlen = strlen( s );

		// check directory
		if( dirlen )
			if( tokenlen <= dirlen + 1 || s[dirlen] != '/' || Q_strnicmp( s, dir, dirlen ) )
				continue;

		// check extension
		if( extlen && (extension[0] != '/') )
			if( tokenlen < extlen || Q_strnicmp( extension, s + tokenlen - extlen, extlen ) )
				continue;

		if( dirlen )
			name = s + dirlen + 1;
		else
			name = s;

		// ignore subdirectories
		p = strchr( name, '/' );
		if( p )
			if( *(p + 1) )
				continue;

		files[found].name = CopyString( name );
		files[found].searchPath = search;
		if( ++found == size )
			return found;
	}

	return found;
}

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
void FS_AddGameDirectory( const char *dir )
{
	int				i, j, numpaks;
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];
	char			**paknames;

	strcpy( fs_gamedir, dir );

	// add the directory to the search path
	search = Mem_Alloc( fs_mempool, sizeof( searchpath_t ) );
	strcpy( search->filename, dir );
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	Com_DPrintf( "FS_AddGameDirectory (\"%s\")\n", dir );

	// add any pk3 and pak files
	for( i = 0; i < 2; i++ ) {
		Q_snprintfz( pakfile, sizeof(pakfile), "%s/*.%s", dir, (i ? "pk3" : "pak") );

 		if( ( paknames = FS_ListFiles( pakfile, &numpaks, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) ) != 0 ) {
			// sort pak files so that pak1.pk3 overrides pak0.pk3, etc
			qsort( paknames, numpaks - 1, sizeof(char*), (int (*)(const void *, const void *))FS_SortStrings );

			for( j = 0; j < numpaks - 1; j++ ) {
				pak = (i ? FS_LoadPK3File( paknames[j] ) : FS_LoadPakFile( paknames[j] ));
				if( !pak )
					continue;

				search = Mem_Alloc( fs_mempool, sizeof( searchpath_t ) );
				strcpy( search->filename, paknames[j] );
				search->pack = pak;
				search->next = fs_searchpaths;
				fs_searchpaths = search;
				Mem_ZoneFree( paknames[j] );
			}
			Mem_ZoneFree( paknames );
		}
	}
}

/*
================
FS_AddHomeAsGameDirectory

Use ~/.qfusion/dir as fs_gamedir

icculus.org
================
*/
void FS_AddHomeAsGameDirectory( const char *dir )
{
	int len;
	char gdir[MAX_OSPATH];
	char *homedir;

	homedir = Sys_GetHomeDirectory ();
	if( !homedir || !homedir[0] )
		return;

	len = snprintf( gdir, sizeof(gdir), "%s/.qfusion/%s/", homedir, dir );
	Com_Printf( "using %s for writing\n", gdir );
	FS_CreatePath( gdir );
	if( (len > 0) && (len < sizeof(gdir)) && (gdir[len-1] == '/') )
		gdir[len-1] = 0;

	FS_AddGameDirectory( gdir );
}

/*
============
FS_Gamedir

Called to find where to write a file (demos, savegames, etc)
============
*/
char *FS_Gamedir( void ) {
	return ( *fs_gamedir ? fs_gamedir : BASEDIRNAME );
}

/*
=============
FS_ExecAutoexec
=============
*/
void FS_ExecAutoexec( void )
{
	char *dir;
	char name[MAX_OSPATH];

	dir = Cvar_VariableString( "fs_gamedir" );
	Q_snprintfz( name, sizeof( name ), "%s/%s/autoexec.cfg", fs_basepath->string, ( *dir ) ? dir : BASEDIRNAME );

	if( Sys_FindFirst( name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) )
		Cbuf_AddText( "exec autoexec.cfg\n" );

	Sys_FindClose ();
}

/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir( char *dir )
{
	searchpath_t	*next;

	if( strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":") ) {
		Com_Printf( "Gamedir should be a single filename, not a path\n" );
		return;
	}

	// free up any current game dir info
	while( fs_searchpaths != fs_base_searchpaths ) {
		if( fs_searchpaths->pack ) {
			if( fs_searchpaths->pack->sysHandle )
				Sys_UnlockFile( fs_searchpaths->pack->sysHandle );
			Mem_Free( fs_searchpaths->pack );
		}
		next = fs_searchpaths->next;
		Mem_Free( fs_searchpaths );
		fs_searchpaths = next;
	}

	// flush all data, so it will be forced to reload
	if( dedicated && !dedicated->integer )
		Cbuf_AddText( "snd_restart\nin_restart\n" );

	Q_snprintfz( fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basepath->string, dir );

	if( !strcmp (dir, BASEDIRNAME) || (*dir == 0) ) {
		Cvar_FullSet( "fs_gamedir", "", CVAR_SERVERINFO|CVAR_NOSET, qtrue );
		Cvar_FullSet( "fs_game", "", CVAR_LATCH|CVAR_SERVERINFO, qtrue );
	} else {
		Cvar_FullSet( "fs_gamedir", dir, CVAR_SERVERINFO|CVAR_NOSET, qtrue );
		if( fs_cdpath->string[0] )
			FS_AddGameDirectory( va("%s/%s", fs_cdpath->string, dir) );
		FS_AddGameDirectory( va("%s/%s", fs_basepath->string, dir) );
		FS_AddHomeAsGameDirectory( dir );
	}
}

/*
================
FS_ListFiles
================
*/
char **FS_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave )
{
	char *s;
	int nfiles = 0;
	char **list = 0;

	s = Sys_FindFirst( findname, musthave, canthave );
	while( s ) {
		if( s[strlen(s)-1] != '.' )
			nfiles++;
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	if( !nfiles )
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = Mem_ZoneMalloc( sizeof( char * ) * nfiles );
	s = Sys_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while( s ) {
		if( s[strlen(s)-1] != '.' ) {
			list[nfiles] = CopyString( s );
#ifdef _WIN32
			Q_strlwr( list[nfiles] );
#endif
			nfiles++;
		}
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	return list;
}

#define FS_MIN_SEARCHFILES	0x400
#define FS_MAX_SEARCHFILES	0xFFFF		// cap
int FS_SortFiles( const searchfile_t *file1, const searchfile_t *file2 ) {
	return Q_stricmp( (file1)->name, (file2)->name );
}

/*
================
FS_Dir_f
================
*/
void FS_Dir_f( void )
{
	int i, allfound;
	char dir[MAX_OSPATH];
	char extension[MAX_OSPATH];
	searchpath_t *search;
	searchfile_t *files;
	qboolean truncated;

	extension[0] = 0;
	if( Cmd_Argc() == 1 ) {
		dir[0] = '/';
		dir[1] = 0;
	} else {
		if( Cmd_Argc() > 2 )
			Q_strncpyz( extension, Cmd_Argv( 2 ), sizeof( extension ) );

		Q_strncpyz( dir, Cmd_Argv( 1 ), sizeof( dir ) );
		if( !strcmp( dir, "./" ) || !strcmp( dir, "." ) )
			strcpy( dir, "/" );
	}

	allfound = 0;
	files = fs_searchfiles;

	for( search = fs_searchpaths ; search ; search = search->next ) {
		allfound += FS_PathGetFileListExt( search, dir, extension, files + allfound, fs_numsearchfiles - allfound );
		if( allfound == fs_numsearchfiles ) {
			truncated = qtrue;
			break;	// we are done
		}
	}

	qsort( files, allfound, sizeof (searchfile_t ), (int (*)(const void *, const void *))FS_SortFiles );

	Com_Printf( "Directory of %s\n", dir );
	Com_Printf( "----\n" );

	// free temp memory
	for( i = 0; i < allfound; i++ ) {
		Com_Printf( "%s/%s\n", files[i].searchPath->filename, files[i].name );
		Mem_ZoneFree( files[i].name );
	}
	if( truncated )
		Com_Printf( "...\n" );

	Com_Printf( "\n%i files listed\n", allfound );
}

/*
================
FS_GetFileListExt_
================
*/
static int FS_GetFileListExt_( const char *dir, const char *extension, char *buf, size_t *bufsize, int maxFiles, int start, int end )
{
	int i;
	int allfound, found, limit;
	size_t len, alllen;
	searchpath_t *search;
	searchfile_t *files;
	static int maxFilesCache;
	static char dircache[MAX_QPATH], extcache[MAX_QPATH];
	qboolean useCache;

	assert( !dir || dir[strlen(dir)-1] != '/' );

	if( (dir && dir[strlen(dir)-1] == '/') || !bufsize )
		return 0;
	
	if( fs_cursearchfiles )
	{
		useCache = (maxFilesCache == maxFiles);
		if( useCache ) {
			useCache = dir ? !strcmp( dircache, dir ) : (dircache[0] == '\0');
			if( useCache )
				useCache = extension ? !strcmp( extcache, extension ) : (extcache[0] == '\0');
		}
	}
	else
	{
		useCache = qfalse;
	}

	maxFilesCache = maxFiles;
	if( dir )
		Q_strncpyz( dircache, dir, sizeof( dircache ) );
	else
		dircache[0] = '\0';
	if( extension )
		Q_strncpyz( extcache, extension, sizeof( extcache ) );
	else
		extcache[0] = '\0';

	files = fs_searchfiles;
	if( !useCache ) {
		allfound = 0;
		search = fs_searchpaths;

		while( search ) {
			limit = maxFiles ? min( fs_numsearchfiles, maxFiles ) : fs_numsearchfiles;
			found = FS_PathGetFileListExt( search, dir, extension, files + allfound,
				fs_numsearchfiles - allfound );

			if( allfound+found == fs_numsearchfiles )
			{
				if( limit == maxFiles || fs_numsearchfiles == FS_MAX_SEARCHFILES )
					break;	// we are done
				fs_numsearchfiles *= 2;
				if( fs_numsearchfiles > FS_MAX_SEARCHFILES )
					fs_numsearchfiles = FS_MAX_SEARCHFILES;
				fs_searchfiles = files = Mem_Realloc( fs_searchfiles, sizeof( searchfile_t ) * fs_numsearchfiles );
				if( !search->pack ) {
					for( i = 0; i < found; i++ )
						Mem_ZoneFree( files[allfound+i].name );
				}
				continue;
			}

			allfound += found;
			search = search->next;
		}

		fs_cursearchfiles = allfound;
	}
	else
	{
		allfound = fs_cursearchfiles;
	}

	if( start < 0 )
		start = 0;
	if( !end )
		end = allfound;
	else if( end > allfound )
		end = allfound;

	if( !useCache )
		qsort( files, allfound, sizeof(searchfile_t), (int (*)(const void *, const void *))FS_SortFiles );

	if( bufsize ) {
		found = 0;

		if( buf ) {
			alllen = 0;
			for( i = start; i < end; i++ ) {
				len = strlen( files[i].name );
				if( *bufsize <= len + alllen )
					break;	// we are done
				strcpy( buf + alllen, files[i].name );
				alllen += len + 1;
				found++;
			}
		} else {
			*bufsize = 0;
			for( i = start; i < end; found++, i++ )
				*bufsize += strlen( files[i].name ) + 1;
		}

		return found;
	}

	return allfound;
}

/*
================
FS_GetFileList
================
*/
int FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end ) {
	return FS_GetFileListExt_( dir, extension, buf, &bufsize, FS_MAX_SEARCHFILES, start, end );
}

/*
================
FS_GetFileListExt
================
*/
int FS_GetFileListExt( const char *dir, const char *extension, char *buf, size_t *bufsize, int start, int end ) {
	return FS_GetFileListExt_( dir, extension, buf, bufsize, FS_MAX_SEARCHFILES, start, end );
}

/*
============
FS_Path_f
============
*/
void FS_Path_f (void)
{
	searchpath_t	*s;

	Com_Printf( "Current search path:\n" );
	for( s = fs_searchpaths; s; s = s->next ) {
		if( s == fs_base_searchpaths )
			Com_Printf( "----------\n" );
		if( s->pack )
			Com_Printf( "%s (%i files)\n", s->pack->filename, s->pack->numFiles );
		else
			Com_Printf( "%s\n", s->filename );
	}
}

/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void FS_CreatePath( const char *path )
{
	char	*ofs;
	
	for( ofs = ( char * )path + 1; *ofs; ofs++ ) {
		if( *ofs == '/' ) {
			// create the directory
			*ofs = 0;
			Sys_Mkdir( path );
			*ofs = '/';
		}
	}
}

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char *FS_NextPath( char *prevpath )
{
	searchpath_t	*s;
	char			*prev;

	if( !prevpath )
		return fs_gamedir;

	prev = fs_gamedir;
	for( s = fs_searchpaths; s; s = s->next ) {
		if( s->pack )
			continue;
		if( prevpath == prev )
			return s->filename;
		prev = s->filename;
	}
	return NULL;
}

/*
================
FS_FreeSearchFiles
================
*/
static void FS_FreeSearchFiles (void)
{
	int i;

	// free temp memory
	for( i = 0; i < fs_cursearchfiles; i++ ) {
		if( !fs_searchfiles[i].searchPath->pack )
			Mem_ZoneFree( fs_searchfiles[i].name );
	}
	fs_cursearchfiles = 0;
}

/*
================
FS_Init
================
*/
void FS_Init (void)
{
	int i;

	fs_mempool = Mem_AllocPool( NULL, "Filesystem" );

	Cmd_AddCommand( "fs_path", FS_Path_f );
	Cmd_AddCommand( "fs_dir", FS_Dir_f );

	fs_numsearchfiles = FS_MIN_SEARCHFILES;
	fs_searchfiles = Mem_Alloc( fs_mempool, sizeof(searchfile_t) * fs_numsearchfiles );

	fs_globalZipEntryUser = NULL;
	memset( fs_filehandles, 0, sizeof(fs_filehandles) );

	//
	// link filehandles
	//
	fs_free_filehandles = fs_filehandles;
	fs_filehandles_headnode.prev = &fs_filehandles_headnode;
	fs_filehandles_headnode.next = &fs_filehandles_headnode;
	for( i = 0; i < FS_MAX_HANDLES - 1; i++ )
		fs_filehandles[i].next = &fs_filehandles[i+1];

	// basedir <path>
	// allows the game to run from outside the data tree
	fs_basepath = Cvar_Get( "fs_basepath", ".", CVAR_NOSET );
	fs_basedir = Cvar_Get( "fs_basedir", BASEDIRNAME, CVAR_NOSET );

	// cddir <path>
	// Logically concatenates the cddir after the basedir for allows the game to run from outside the data tree
	fs_cdpath = Cvar_Get( "fs_cdpath", "", CVAR_NOSET );
	if( fs_cdpath->string[0] )
		FS_AddGameDirectory( va("%s/"BASEDIRNAME, fs_cdpath->string) );

	// start up with baseq3 by default
	FS_AddGameDirectory( va("%s/"BASEDIRNAME, fs_basepath->string) );

	// then add a '.qfusion/BASEDIRNAME' directory in home directory by default
	FS_AddHomeAsGameDirectory( BASEDIRNAME );

	// any set gamedirs will be freed up to here
	fs_base_searchpaths = fs_searchpaths;

	// check for game override
	fs_gamedirvar = Cvar_Get( "fs_game", "baseqf", CVAR_LATCH|CVAR_SERVERINFO );
	if( fs_gamedirvar->string[0] )
		FS_SetGamedir( fs_gamedirvar->string );
}

/*
================
FS_Frame
================
*/
void FS_Frame (void)
{
	FS_FreeSearchFiles ();
}

/*
================
FS_Shutdown
================
*/
void FS_Shutdown (void)
{
	searchpath_t	*search;
	filehandle_t	*fh, *hnode, *next;

	// close all currently open filehandles
	hnode = &fs_filehandles_headnode;
	for( fh = hnode->next; fh != hnode; fh = next ) {
		next = fh->next;
		FS_FCloseFile( fh - fs_filehandles + 1 );
	}
	memset( fs_filehandles, 0, sizeof( fs_filehandles ) );

	fs_globalZipEntryUser = 0;

	FS_FreeSearchFiles ();

	// free up any current game dir info
	while( fs_searchpaths != fs_base_searchpaths ) {
		if( fs_searchpaths->pack ) {
			if( fs_searchpaths->pack->sysHandle )
				Sys_UnlockFile( fs_searchpaths->pack->sysHandle );
			Mem_Free( fs_searchpaths->pack );
		}
		search = fs_searchpaths->next;
		Mem_Free( fs_searchpaths );
		fs_searchpaths = search;
	}

	fs_numsearchfiles = 0;
	Mem_Free( fs_searchfiles );

	Cmd_RemoveCommand( "fs_path" );
	Cmd_RemoveCommand( "fs_dir" );

	Mem_FreePool( &fs_mempool );
}
