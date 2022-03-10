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
#include "client.h"

/*
=================================================================

ROQ PLAYING

=================================================================
*/

/*
==================
SCR_StopCinematic
==================
*/
void SCR_StopCinematic( void )
{
	cinematics_t *cin = &cl.cin;

	if( !cin->file )
		return;

	cin->time = 0;	// done
	cin->pic = NULL;
	cin->pic_pending = NULL;

	FS_FCloseFile( cin->file );
	cin->file = 0;

	if( cin->vid_buffer ) {
		Mem_ZoneFree( cin->vid_buffer );
		cin->vid_buffer = NULL;
	}
}

/*
====================
SCR_FinishCinematic

Called when either the cinematic completes, or it is aborted
====================
*/
void SCR_FinishCinematic( void )
{
	// tell the server to advance to the next map / cinematic
	MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
	SZ_Print( &cls.netchan.message, va("nextserver %i\n", cl.servercount) );
	CL_SetClientState( ca_connected );
}

//==========================================================================

/*
==================
SCR_ReadNextCinematicFrame
==================
*/
static qbyte *SCR_ReadNextCinematicFrame( void )
{
	cinematics_t *cin = &cl.cin;
	roq_chunk_t *chunk = &cin->chunk;

	while( !FS_Eof( cin->file ) ) {
		RoQ_ReadChunk( cin );

		if( FS_Eof( cin->file ) )
			return NULL;
		if( chunk->size <= 0 )
			continue;

		if( chunk->id == RoQ_INFO )
			RoQ_ReadInfo( cin );
		else if( chunk->id == RoQ_SOUND_MONO || chunk->id == RoQ_SOUND_STEREO )
			RoQ_ReadAudio( cin );
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
SCR_RunCinematic
==================
*/
void SCR_RunCinematic( void )
{
	int		frame;
	cinematics_t *cin = &cl.cin;

	if( cin->time <= 0 ) {
		SCR_StopCinematic ();
		return;
	}

	if( cls.key_dest != key_game ) {
		// stop if menu or console is up
		SCR_StopCinematic ();
		SCR_FinishCinematic ();
		return;
	}

	if( cin->frame == -1 )
		return;

	frame = (cls.realtime - cin->time) * (float)(RoQ_FRAMERATE) / 1000;
	if( frame <= cin->frame )
		return;
	if( frame > cin->frame + 1 ) {
		Com_Printf( "Dropped frame: %i > %i\n", frame, cin->frame + 1 );
		cin->time = cls.realtime - cin->frame * 1000 / RoQ_FRAMERATE;
	}

	cin->pic = cin->pic_pending;
	cin->pic_pending = SCR_ReadNextCinematicFrame ();

	if( !cin->pic_pending ) {
		SCR_StopCinematic ();
		SCR_FinishCinematic ();
		return;
	}
}

/*
==================
SCR_DrawCinematic

Returns true if a cinematic is active, meaning the view rendering
should be skipped
==================
*/
qboolean SCR_DrawCinematic( void )
{
	cinematics_t *cin = &cl.cin;

	if( cin->time <= 0 )
		return qfalse;
	if( !cin->pic )
		return qtrue;

	R_DrawStretchRaw( 0, 0, viddef.width, viddef.height, cin->width, cin->height, cin->frame, cin->pic );

	return qtrue;
}

/*
==================
SCR_PlayCinematic
==================
*/
void SCR_PlayCinematic( char *arg )
{
	int	len;
	cinematics_t *cin = &cl.cin;
	roq_chunk_t *chunk = &cin->chunk;

	Q_snprintfz( cin->name, sizeof(cin->name), "video/%s", arg );

	// nasty hack
	cin->s_rate = 22050;
	cin->s_width = 2;
	cin->width = cin->height = 0;

	cin->frame = 0;
	len = FS_FOpenFile( cin->name, &cin->file, FS_READ );
	if( !cin->file || len < 1 ) {
		SCR_FinishCinematic ();
		cin->file = 0;
		cin->time = 0;	// done
		return;
	}

	SCR_EndLoadingPlaque ();

	CL_SetClientState( ca_active );

	// read header
	RoQ_ReadChunk( cin );

	if( LittleShort( chunk->id ) != RoQ_HEADER1 || LittleLong( chunk->size ) != RoQ_HEADER2 || LittleShort( chunk->argument ) != RoQ_HEADER3 ) {
		SCR_StopCinematic ();
		SCR_FinishCinematic ();
		return;
	}

	cin->headerlen = FS_Tell( cin->file );
	cin->frame = 0;
	cin->pic = cin->pic_pending = SCR_ReadNextCinematicFrame ();
	cin->time = Sys_Milliseconds ();
}
