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
GL_StopCinematic
==================
*/
void GL_StopCinematic ( cinematics_t *cin )
{
	cin->time = 0;	// done
	cin->pic = NULL;
	cin->pic_pending = NULL;

	if ( cin->file ) {
		FS_FCloseFile ( cin->file );
		cin->file = 0;
	}
	if ( cin->vid_buffer ) {
		Mem_ZoneFree ( cin->vid_buffer );
		cin->vid_buffer = NULL;
	}
}

//==========================================================================

/*
==================
GL_ReadNextFrame
==================
*/
qbyte *GL_ReadNextFrame ( cinematics_t *cin )
{
	roq_chunk_t *chunk = &cin->chunk;

	while ( cin->remaining > 0 ) 
	{
		RoQ_ReadChunk ( cin );

		if ( cin->remaining <= 0 || chunk->size > cin->remaining ) {
			return NULL;
		}
		if ( chunk->size <= 0 ) {
			continue;
		}

		if ( chunk->id == RoQ_INFO ) {
			RoQ_ReadInfo ( cin );
		} else if ( chunk->id == RoQ_QUAD_VQ ) {
			return RoQ_ReadVideo ( cin );
		} else if ( chunk->id == RoQ_QUAD_CODEBOOK ) {
			RoQ_ReadCodebook ( cin );
		} else {
			RoQ_SkipChunk ( cin );
		}
	}

	return NULL;
}

/*
==================
GL_ResampleCinematicFrame

==================
*/
image_t *GL_ResampleCinematicFrame ( shaderpass_t *pass )
{
	image_t			*image;
	cinematics_t	*cin = pass->cin;

	if (!cin->pic)
		return NULL;

	if ( !pass->anim_frames[0] ) {
		image = GL_LoadPic ( cin->name, cin->pic, cin->width, cin->height, IT_CINEMATICS|IT_NOPICMIP|IT_NOMIPMAP, 8 );
	} else {
		image = pass->anim_frames[0];
	}

	if ( !cin->restart_sound ) {
		return image;
	}
	cin->restart_sound = qfalse;

	GL_EnableMultitexture ( qfalse );
	GL_Bind ( image->texnum );

	if ( pass->anim_frames[0] ) {
		qglTexSubImage2D ( GL_TEXTURE_2D, 
			0, 0, 0,
			image->upload_width, image->upload_height, 
			GL_RGBA, GL_UNSIGNED_BYTE, cin->pic );
	} else {
		qglTexImage2D ( GL_TEXTURE_2D, 
			0, GL_RGB, 
			image->upload_width, image->upload_height, 
			0, GL_RGBA, GL_UNSIGNED_BYTE, cin->pic );
	}

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	return image;
}

/*
==================
GL_RunCinematic

==================
*/
void GL_RunCinematic ( cinematics_t *cin )
{
	int		frame;

	frame = (Sys_Milliseconds() - cin->time)*RoQ_FRAMERATE/1000;
	if (frame <= cin->frame)
		return;
	if (frame > cin->frame+1)
		cin->time = Sys_Milliseconds() - cin->frame*1000/RoQ_FRAMERATE;

	cin->pic = cin->pic_pending;
	cin->pic_pending = GL_ReadNextFrame ( cin );

	if (!cin->pic_pending)
	{
		FS_FCloseFile ( cin->file );
		cin->remaining = FS_FOpenFile ( cin->name, &cin->file );

		// skip header
		RoQ_ReadChunk ( cin );

		cin->frame = 0;
		cin->pic_pending = GL_ReadNextFrame ( cin );
		cin->time = Sys_Milliseconds();
	}

	cin->restart_sound = qtrue;
}

/*
==================
GL_PlayCinematic

==================
*/
void GL_PlayCinematic ( cinematics_t *cin )
{
	roq_chunk_t *chunk = &cin->chunk;

	cin->remaining = FS_FOpenFile ( cin->name, &cin->file );
	if (!cin->file || !cin->remaining)
	{
		cin->time = 0;	// done
		return;
	}

	// read header
	RoQ_ReadChunk ( cin );

	if ( LittleShort ( chunk->id ) != RoQ_HEADER1 || LittleLong ( chunk->size ) != RoQ_HEADER2 || LittleShort ( chunk->argument ) != RoQ_HEADER3 ) {
		GL_StopCinematic ( cin );
		cin->time = 0;	// done
		return;
	}

	cin->frame = 0;
	cin->pic = GL_ReadNextFrame ( cin );
	cin->time = Sys_Milliseconds ();

	cin->restart_sound = qtrue;
}
