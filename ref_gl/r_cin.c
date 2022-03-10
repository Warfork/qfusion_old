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
#include "r_local.h"

//=============================================================

/*
==================
R_StopCinematic
==================
*/
void R_StopCinematic( cinematics_t *cin )
{
	cin->time = 0;	// done
	cin->pic = NULL;
	cin->pic_pending = NULL;

	if( cin->file ) {
		FS_FCloseFile( cin->file );
		cin->file = 0;
	}
	if( cin->vid_buffer ) {
		Mem_ZoneFree( cin->vid_buffer );
		cin->vid_buffer = NULL;
	}
}

//==========================================================================

/*
==================
R_ReadNextCinematicFrame
==================
*/
static qbyte *R_ReadNextCinematicFrame( cinematics_t *cin )
{
	roq_chunk_t *chunk = &cin->chunk;

	while( !FS_Eof( cin->file ) ) {
		RoQ_ReadChunk( cin );

		if( FS_Eof( cin->file ) )
			return NULL;
		if( chunk->size <= 0 )
			continue;

		if( chunk->id == RoQ_INFO )
			RoQ_ReadInfo( cin );
		else if( chunk->id == RoQ_QUAD_VQ )
			return RoQ_ReadVideo( cin );
		else if( chunk->id == RoQ_QUAD_CODEBOOK )
			RoQ_ReadCodebook( cin );
		else
			RoQ_SkipChunk( cin );
	}

	return NULL;
}

/*
==================
R_ResampleCinematicFrame
==================
*/
image_t *R_ResampleCinematicFrame( shaderpass_t *pass )
{
	image_t			*image;
	cinematics_t	*cin = pass->cin;

	if( !cin->pic )
		return NULL;

	if( pass->anim_frames[0] ) {
		image = pass->anim_frames[0];
	} else {
		image = R_LoadPic( cin->name, &cin->pic, cin->width, cin->height, IT_CINEMATIC, 3 );
		cin->new_frame = qfalse;
	}

	if( !cin->new_frame )
		return image;
	cin->new_frame = qfalse;

	GL_Bind( 0, image );
	if( image->width != cin->width || image->height != cin->height )
		R_Upload32( &cin->pic, image->width, image->height, IT_CINEMATIC, NULL, NULL, 3, qfalse );
	else
		R_Upload32( &cin->pic, image->width, image->height, IT_CINEMATIC, NULL, NULL, 3, qtrue );
	image->width = cin->width;
	image->height = cin->height;

	return image;
}

/*
==================
R_RunCinematic
==================
*/
void R_RunCinematic( cinematics_t *cin )
{
	int		frame;

	frame = (Sys_Milliseconds () - cin->time) * (float)(RoQ_FRAMERATE) / 1000;
	if( frame <= cin->frame )
		return;
	if( frame > cin->frame + 1 )
		cin->time = Sys_Milliseconds () - cin->frame * 1000 / RoQ_FRAMERATE;

	cin->pic = cin->pic_pending;
	cin->pic_pending = R_ReadNextCinematicFrame( cin );

	if( !cin->pic_pending ) {
		FS_Seek( cin->file, cin->headerlen, FS_SEEK_SET );
		cin->frame = 0;
		cin->pic_pending = R_ReadNextCinematicFrame( cin );
		cin->time = Sys_Milliseconds ();
	}

	cin->new_frame = qtrue;
}

/*
==================
R_PlayCinematic
==================
*/
void R_PlayCinematic( cinematics_t *cin )
{
	int len;
	roq_chunk_t *chunk = &cin->chunk;

	cin->width = cin->height = 0;
	len = FS_FOpenFile( cin->name, &cin->file, FS_READ );
	if( !cin->file || len < 1 ) {
		cin->time = 0;		// done
		return;
	}

	// read header
	RoQ_ReadChunk( cin );

	if( LittleShort( chunk->id ) != RoQ_HEADER1 || LittleLong( chunk->size ) != RoQ_HEADER2 || LittleShort( chunk->argument ) != RoQ_HEADER3 ) {
		R_StopCinematic( cin );
		cin->time = 0;		// done
		return;
	}

	cin->headerlen = FS_Tell( cin->file );
	cin->frame = 0;
	cin->pic = cin->pic_pending = R_ReadNextCinematicFrame( cin );
	cin->time = Sys_Milliseconds ();

	cin->new_frame = qtrue;
}
