/*
Copyright (C) 2002-2003 Victor Luchits

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
// snd_ogg.c

#include "client.h"
#include "snd_loc.h"

/* The function prototypes for the callbacks are basically the same as for
 * the stdio functions fread, fseek, fclose, ftell. 
 * The one difference is that the FILE * arguments have been replaced with
 * a void * - this is to be used as a pointer to whatever internal data these
 * functions might need. In the stdio case, it's just a FILE * cast to a void *
 * 
 * If you use other functions, check the docs for these functions and return
 * the right values. For seek_func(), you *MUST* return -1 if the stream is
 * unseekable
 */

#ifdef _MSC_VER
typedef __int64 ogg_int64_t;
#else
typedef long long ogg_int64_t;
#endif

typedef struct {
	size_t			(*read_func)  (void *ptr, size_t size, size_t nmemb, void *datasource);
	int				(*seek_func)  (void *datasource, ogg_int64_t offset, int whence);
	int				(*close_func) (void *datasource);
	long			(*tell_func)  (void *datasource);
} ov_callbacks;

typedef struct
{
	long			endbyte;
	int				endbit;

	unsigned char	*buffer;
	unsigned char	*ptr;
	long			storage;
} oggpack_buffer;

typedef struct
{
	unsigned char	*data;
	int				storage;
	int				fill;
	int				returned;

	int				unsynced;
	int				headerbytes;
	int				bodybytes;
} ogg_sync_state;

typedef struct vorbis_info
{
	int				version;
	int				channels;
	long			rate;

	long			bitrate_upper;
	long			bitrate_nominal;
	long			bitrate_lower;
	long			bitrate_window;

	void			*codec_setup;
} vorbis_info;

typedef struct
{
	unsigned char	*body_data;
	long			body_storage;
	long			body_fill;
	long			body_returned;

	int				*lacing_vals;
	ogg_int64_t		*granule_vals;

	long			lacing_storage;
	long			lacing_fill;
	long			lacing_packet;
	long			lacing_returned;
	
	unsigned char	header[282];
	int				header_fill;

	int				e_o_s;
	int				b_o_s;
	long			serialno;
	long			pageno;
	ogg_int64_t		packetno;
	ogg_int64_t		granulepos;
} ogg_stream_state;

typedef struct vorbis_dsp_state
{
	int				analysisp;
	vorbis_info		*vi;

	float			**pcm;
	float			**pcmret;
	int				pcm_storage;
	int				pcm_current;
	int				pcm_returned;

	int				preextrapolate;
	int				eofflag;

	long			lW;
	long			W;
	long			nW;
	long			centerW;

	ogg_int64_t		granulepos;
	ogg_int64_t		sequence;

	ogg_int64_t		glue_bits;
	ogg_int64_t		time_bits;
	ogg_int64_t		floor_bits;
	ogg_int64_t		res_bits;

	void			*backend_state;
} vorbis_dsp_state;

typedef struct vorbis_block
{
	float			**pcm;
	oggpack_buffer	opb;
	
	long			lW;
	long			W;
	long			nW;
	int				pcmend;
	int				mode;
	
	int				eofflag;
	ogg_int64_t		granulepos;
	ogg_int64_t		sequence;
	vorbis_dsp_state *vd;

	void            *localstore;
	long            localtop;
	long            localalloc;
	long            totaluse;
	struct alloc_chain *reap;

	long			glue_bits;
	long			time_bits;
	long			floor_bits;
	long			res_bits;

	void			*internal;
} vorbis_block;

typedef struct OggVorbis_File
{
	void            *datasource;
	int             seekable;
	ogg_int64_t     offset;
	ogg_int64_t     end;
	ogg_sync_state  oy; 

	int             links;
	ogg_int64_t     *offsets;
	ogg_int64_t     *dataoffsets;
	long            *serialnos;
	ogg_int64_t     *pcmlengths;

	vorbis_info     *vi;
	void			*vc;

	ogg_int64_t     pcm_offset;
	int             ready_state;
	long            current_serialno;
	int             current_link;

	double          bittrack;
	double          samptrack;

	ogg_stream_state os;
	vorbis_dsp_state vd;
	vorbis_block     vb;

	ov_callbacks	callbacks;
} OggVorbis_File;

//=============================================================================

int (*qov_clear)( OggVorbis_File *vf );
int (*qov_open_callbacks)( void *datasource, OggVorbis_File *vf, char *initial, long ibytes, ov_callbacks callbacks );
ogg_int64_t (*qov_pcm_total)( OggVorbis_File *vf, int i );
int (*qov_raw_seek)( OggVorbis_File *vf, ogg_int64_t pos );
ogg_int64_t (*qov_raw_tell)( OggVorbis_File *vf );
vorbis_info *(*qov_info)( OggVorbis_File *vf, int link );
long (*qov_read)( OggVorbis_File *vf, char *buffer, int length, int bigendianp, int word, int sgned, int *bitstream );

dllfunc_t oggvorbisfuncs[] =
{
	{ "ov_clear",			( void ** )&qov_clear },
	{ "ov_open_callbacks",	( void ** )&qov_open_callbacks },
	{ "ov_pcm_total",		( void ** )&qov_pcm_total },
	{ "ov_raw_seek",		( void ** )&qov_raw_seek },
	{ "ov_raw_tell",		( void ** )&qov_raw_tell },
	{ "ov_info",			( void ** )&qov_info },
	{ "ov_read",			( void ** )&qov_read },

	{ NULL, NULL }
};

void *vorbisLibrary;

/*
===================
SNDOGG_Init
===================
*/
void SNDOGG_Init( void )
{
	if( vorbisLibrary )
		SNDOGG_Shutdown ();

	vorbisLibrary = Sys_LoadLibrary( VORBISFILE_LIBNAME, oggvorbisfuncs );
	if( vorbisLibrary )
		Com_Printf( "Loaded %s\n", VORBISFILE_LIBNAME );
}

/*
===================
SNDOGG_Shutdown
===================
*/
void SNDOGG_Shutdown( void )
{
	if( vorbisLibrary )
		Sys_UnloadLibrary( &vorbisLibrary );
}

//=============================================================================

static size_t ovcb_read( void *ptr, size_t size, size_t nb, void *datasource )
{
	bgTrack_t *track = ( bgTrack_t * )datasource;

	return FS_Read( ptr, size * nb, track->file ) / size;
}

static int ovcb_seek( void *datasource, ogg_int64_t offset, int whence )
{
	bgTrack_t *track = ( bgTrack_t * )datasource;

	switch( whence ) {
		case SEEK_SET:
			return FS_Seek( track->file, (int)offset, FS_SEEK_SET );
		case SEEK_CUR:
			return FS_Seek( track->file, (int)offset, FS_SEEK_CUR );
		case SEEK_END:
			return FS_Seek( track->file, (int)offset, FS_SEEK_END );
	}

	return -1;
}

static int ovcb_close( void *datasource ) {
	return 0;
}

static long ovcb_tell( void *datasource )
{
	bgTrack_t *track = ( bgTrack_t * )datasource;

	return FS_Tell( track->file );
}

int SNDOGG_FRead( bgTrack_t *track, void *ptr, size_t size );
int SNDOGG_FSeek( bgTrack_t *track, int pos );
void SNDOGG_FClose( bgTrack_t *track );

/*
===================
SNDOGG_OpenTrack
===================
*/
qboolean SNDOGG_OpenTrack( char *name, bgTrack_t *track )
{
	int		file;
	char	path[MAX_QPATH];
	vorbis_info	*vi;
	OggVorbis_File *vf;
	ov_callbacks callbacks = { ovcb_read, ovcb_seek, ovcb_close, ovcb_tell };

	if( !vorbisLibrary || !track )
		return qfalse;

	COM_StripExtension( name, path );
	Q_strncatz( path, ".ogg", sizeof( path ) );

	if( FS_FOpenFile( path, &file, FS_READ ) == -1 )
		return qfalse;

	track->file = file;
	track->vorbisFile = vf = S_Malloc( sizeof( OggVorbis_File ) );

	if( qov_open_callbacks( track, vf, NULL, 0, callbacks ) < 0 ) {
		Com_Printf( "SNDOGG_OpenTrack: couldn't open %s for reading\n", path );
		goto fail;
	}

	vi = qov_info( vf, -1 );
	if( (vi->channels != 1) && (vi->channels != 2) ) {
		Com_Printf( "SNDOGG_OpenTrack: %s has an unsupported number of channels: %i\n", path, vi->channels );
		goto fail;
	}

	track->info.channels = vi->channels;
	track->info.rate = vi->rate;
	track->info.width = 2;
	track->info.loopstart = -1;
	track->info.dataofs = qov_raw_tell( vf );
	track->info.samples = qov_pcm_total( vf, -1 );

	track->read = SNDOGG_FRead;
	track->seek = SNDOGG_FSeek;
	track->close = SNDOGG_FClose;

	return qtrue;

fail:
	if( file )
		FS_FCloseFile( file );
	if( vf ) {
		qov_clear( vf );
		S_Free( vf );
	}

	track->file = 0;
	track->vorbisFile = NULL;

	return qfalse;
}

/*
===================
SNDOGG_FRead
===================
*/
int SNDOGG_FRead( bgTrack_t *track, void *ptr, size_t size ) {
	int bs;

	return qov_read( track->vorbisFile, ( char * )ptr, (int)size, 0, 2, 1, &bs );
}

/*
===================
SNDOGG_FSeek
===================
*/
int SNDOGG_FSeek( bgTrack_t *track, int pos ) {
	return qov_raw_seek( track->vorbisFile, (ogg_int64_t)pos );
}

/*
===================
SNDOGG_FClose
===================
*/
void SNDOGG_FClose( bgTrack_t *track )
{
	FS_FCloseFile( track->file );
	qov_clear( track->vorbisFile );
	S_Free( track->vorbisFile );
}
