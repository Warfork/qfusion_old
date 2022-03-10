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

#include "client.h"

static short snd_sqr_arr[256];

/*
==================
RoQ_Init
==================
*/
void RoQ_Init (void)
{
	int i;

	for (i = 0; i < 128; i++)
	{
		snd_sqr_arr[i] = i * i;
		snd_sqr_arr[i + 128] = -(i * i);
	}
}

/*
==================
RoQ_ReadChunk
==================
*/
void RoQ_ReadChunk ( cinematics_t *cin )
{
	roq_chunk_t *chunk = &cin->chunk;

	FS_Read( &chunk->id, sizeof(short), cin->file );
	FS_Read( &chunk->size, sizeof(int), cin->file );
	FS_Read( &chunk->argument, sizeof(short), cin->file );

	chunk->id = LittleShort( chunk->id );
	chunk->size = LittleLong( chunk->size );
	chunk->argument = LittleShort( chunk->argument );
}

/*
==================
RoQ_SkipBlock
==================
*/
static void RoQ_SkipBlock ( cinematics_t *cin, int size )
{
	qbyte compressed[0x20000];
	int block, remaining = size;

	if( !size )
		return;

	do {
		block = min( sizeof(compressed), remaining );
		remaining -= block;
		FS_Read( compressed, block, cin->file );
	} while( remaining );
}

/*
==================
RoQ_SkipChunk
==================
*/
void RoQ_SkipChunk( cinematics_t *cin ) {
	RoQ_SkipBlock( cin, cin->chunk.size );
}

/*
==================
RoQ_ReadInfo
==================
*/
void RoQ_ReadInfo( cinematics_t *cin )
{
	short t[4];

	FS_Read( t, sizeof(short) * 4, cin->file );

	if( cin->width != LittleShort( t[0] ) || cin->height != LittleShort( t[1] ) ) {
		cin->width = LittleShort( t[0] );
		cin->height = LittleShort( t[1] );

		if( cin->vid_buffer )
			Mem_ZoneFree( cin->vid_buffer );

		cin->vid_buffer = Mem_ZoneMallocExt( cin->width * cin->height * 4 * 2, 0 );

		// default to 255 for alpha
		memset( cin->vid_buffer, 0xFF, cin->width * cin->height * 4 * 2 );

		cin->vid_pic[0] = cin->vid_buffer;
		cin->vid_pic[1] = cin->vid_buffer + cin->width * cin->height * 4;
	}
}

/*
==================
RoQ_ReadCodebook
==================
*/
void RoQ_ReadCodebook ( cinematics_t *cin )
{
	int nv1, nv2;
	roq_chunk_t *chunk = &cin->chunk;

	nv1 = (chunk->argument >> 8) & 0xFF;
	if( !nv1 )
		nv1 = 256;

	nv2 = chunk->argument & 0xFF;
	if( !nv2 && (nv1 * 6 < chunk->size) )
		nv2 = 256;

	FS_Read( cin->cells, sizeof(roq_cell_t)*nv1, cin->file );
	FS_Read( cin->qcells, sizeof(roq_qcell_t)*nv2, cin->file );
}

/*
==================
RoQ_ApplyVector2x2
==================
*/
static void RoQ_DecodeBlock ( qbyte *dst0, qbyte *dst1, const qbyte *src0, const qbyte *src1, float u, float v )
{ 
	int c[3];

	// convert YCbCr to RGB
	VectorSet ( c, 1.402f * v, -0.34414f * u - 0.71414f * v, 1.772f * u );

	// 1st pixel
	dst0[0] = bound( 0, c[0] + src0[0], 255 );
	dst0[1] = bound( 0, c[1] + src0[0], 255 );
	dst0[2] = bound( 0, c[2] + src0[0], 255 );

	// 2nd pixel
	dst0[4] = bound( 0, c[0] + src0[1], 255 );
	dst0[5] = bound( 0, c[1] + src0[1], 255 );
	dst0[6] = bound( 0, c[2] + src0[1], 255 );

	// 3rd pixel
	dst1[0] = bound( 0, c[0] + src1[0], 255 );
	dst1[1] = bound( 0, c[1] + src1[0], 255 );
	dst1[2] = bound( 0, c[2] + src1[0], 255 );

	// 4th pixel
	dst1[4] = bound( 0, c[0] + src1[1], 255 );
	dst1[5] = bound( 0, c[1] + src1[1], 255 );
	dst1[6] = bound( 0, c[2] + src1[1], 255 );
} 

/*
==================
RoQ_ApplyVector2x2
==================
*/
static void RoQ_ApplyVector2x2 ( cinematics_t *cin, int x, int y, const roq_cell_t *cell )
{
	qbyte *dst0, *dst1; 

	dst0 = cin->vid_pic[0] + (y * cin->width + x) * 4;
	dst1 = dst0 + cin->width * 4;

	RoQ_DecodeBlock ( dst0, dst1, cell->y, cell->y+2, (float)((int)cell->u-128), (float)((int)cell->v-128) );
}

/*
==================
RoQ_ApplyVector4x4
==================
*/
static void RoQ_ApplyVector4x4 ( cinematics_t *cin, int x, int y, const roq_cell_t *cell )
{
	qbyte *dst0, *dst1; 
	qbyte p[4]; 
	float u, v; 

	u = (float)((int)cell->u - 128);
	v = (float)((int)cell->v - 128);

	p[0] = p[1] = cell->y[0];
	p[2] = p[3] = cell->y[1];
	dst0 = cin->vid_pic[0] + (y * cin->width + x) * 4; dst1 = dst0 + cin->width * 4;
	RoQ_DecodeBlock ( dst0, dst0+8, p, p+2, u, v );
	RoQ_DecodeBlock ( dst1, dst1+8, p, p+2, u, v );

	p[0] = p[1] = cell->y[2];
	p[2] = p[3] = cell->y[3];
	dst0 += cin->width * 4 * 2; dst1 += cin->width * 4 * 2; 
	RoQ_DecodeBlock ( dst0, dst0+8, p, p+2, u, v );
	RoQ_DecodeBlock ( dst1, dst1+8, p, p+2, u, v );
}

/*
==================
RoQ_ApplyMotion4x4
==================
*/
static void RoQ_ApplyMotion4x4 ( cinematics_t *cin, int x, int y, qbyte mv, char mean_x, char mean_y )
{
	int x0, y0;
	qbyte *src, *dst;

	// calc source coords 
	x0 = x + 8 - (mv >> 4) - mean_x;
	y0 = y + 8 - (mv & 0xF) - mean_y;

	src = cin->vid_pic[1] + (y0 * cin->width + x0) * 4; 
	dst = cin->vid_pic[0] + (y * cin->width + x) * 4; 

	for( y = 0; y < 4; y++, src += cin->width * 4, dst += cin->width * 4 )
		memcpy ( dst, src, 4 * 4 );
}

/*
==================
RoQ_ApplyMotion8x8
==================
*/
static void RoQ_ApplyMotion8x8 ( cinematics_t *cin, int x, int y, qbyte mv, char mean_x, char mean_y )
{
	int x0, y0;
	qbyte *src, *dst;

	// calc source coords 
	x0 = x + 8 - (mv >> 4) - mean_x; 
	y0 = y + 8 - (mv & 0xF) - mean_y; 
	
	src = cin->vid_pic[1] + (y0 * cin->width + x0) * 4; 
	dst = cin->vid_pic[0] + (y * cin->width + x) * 4; 
	
	for( y = 0; y < 8; y++, src += cin->width * 4, dst += cin->width * 4 )
		memcpy ( dst, src, 8 * 4 );
}

/*
==================
RoQ_ReadVideo
==================
*/
qbyte *RoQ_ReadVideo ( cinematics_t *cin )
{
	roq_chunk_t *chunk = &cin->chunk;
	int i, vqflg_pos, vqid, bpos, xpos, ypos, x, y, xp, yp;
	short vqflg;
	qbyte c[4], *tp;
	roq_qcell_t *qcell;

	vqflg = 0;
	vqflg_pos = -1;

	xpos = ypos = 0;
	bpos = chunk->size;

	while (bpos > 0)
	{
		for (yp = ypos; yp < ypos + 16; yp += 8)
			for (xp = xpos; xp < xpos + 16; xp += 8)
			{
				if ( vqflg_pos < 0 ) {
					FS_Read ( &vqflg, sizeof(short), cin->file );
					bpos -= sizeof(short);

					vqflg = LittleShort ( vqflg );
					vqflg_pos = 7;
				}

				vqid = (vqflg >> (vqflg_pos * 2)) & 0x3;
				vqflg_pos--;
				
				switch (vqid)
				{
				case RoQ_ID_MOT: 
					break;

				case RoQ_ID_FCC:
					FS_Read ( c, 1, cin->file );
					bpos--;

					RoQ_ApplyMotion8x8 ( cin, xp, yp, c[0], (char)((chunk->argument >> 8) & 0xff), (char)(chunk->argument & 0xff) );
					break;

				case RoQ_ID_SLD:
					FS_Read ( c, 1, cin->file );
					bpos--;

					qcell = cin->qcells + c[0];
					RoQ_ApplyVector4x4 ( cin, xp, yp, cin->cells + qcell->idx[0] );
					RoQ_ApplyVector4x4 ( cin, xp+4, yp, cin->cells + qcell->idx[1] );
					RoQ_ApplyVector4x4 ( cin, xp, yp+4, cin->cells + qcell->idx[2] );
					RoQ_ApplyVector4x4 ( cin, xp+4, yp+4, cin->cells + qcell->idx[3] );
					break;

				case RoQ_ID_CCC:
					for (i = 0; i < 4; i++)
					{
						x = xp; 
						y = yp;

						if (i & 0x01) 
							x += 4;
						if (i & 0x02) 
							y += 4;
						
						if ( vqflg_pos < 0 ) {
							FS_Read ( &vqflg, sizeof(short), cin->file );
							bpos -= sizeof(short);

							vqflg = LittleShort ( vqflg );
							vqflg_pos = 7;
						}

						vqid = (vqflg >> (vqflg_pos * 2)) & 0x3;
						vqflg_pos--;

						switch (vqid)
						{
						case RoQ_ID_MOT: 
							break;

						case RoQ_ID_FCC:
							FS_Read ( c, 1, cin->file );
							bpos--;

							RoQ_ApplyMotion4x4 ( cin, x, y, c[0], (char)((chunk->argument >> 8) & 0xff), (char)(chunk->argument & 0xff) );
							break;

						case RoQ_ID_SLD:
							FS_Read ( &c, 1, cin->file );
							bpos--;

							qcell = cin->qcells + c[0];
							RoQ_ApplyVector2x2 ( cin, x, y, cin->cells + qcell->idx[0] );
							RoQ_ApplyVector2x2 ( cin, x+2, y, cin->cells + qcell->idx[1] );
							RoQ_ApplyVector2x2 ( cin, x, y+2, cin->cells + qcell->idx[2] );
							RoQ_ApplyVector2x2 ( cin, x+2, y+2, cin->cells + qcell->idx[3] );
							break;

						case RoQ_ID_CCC:
							FS_Read ( &c, 4, cin->file );
							bpos -= 4;

							RoQ_ApplyVector2x2 ( cin, x, y, cin->cells + c[0] );
							RoQ_ApplyVector2x2 ( cin, x+2, y, cin->cells + c[1] );
							RoQ_ApplyVector2x2 ( cin, x, y+2, cin->cells + c[2] );
							RoQ_ApplyVector2x2 ( cin, x+2, y+2, cin->cells + c[3] );
							break;
						}
					}
					break;

				default:
					Com_DPrintf ("Unknown vq code: %d\n", vqid);
					break;
				}
			}
			
		xpos += 16;
		if ( xpos >= cin->width ) {
			xpos -= cin->width;
			ypos += 16;
		}

		if ( ypos >= cin->height && bpos ) {	// skip the remaining trash
			RoQ_SkipBlock ( cin, bpos );
			break;
		}
	}

	if ( cin->frame++ == 0 ) {		// copy initial values to back buffer for motion
		memcpy ( cin->vid_pic[1], cin->vid_pic[0], cin->width * cin->height * 4 );
	} else {	// swap buffers
		tp = cin->vid_pic[0]; cin->vid_pic[0] = cin->vid_pic[1]; cin->vid_pic[1] = tp;
	}

	return cin->vid_pic[1];
}

/*
==================
RoQ_ReadAudio
==================
*/
void RoQ_ReadAudio ( cinematics_t *cin )
{
	int i;
	short snd_left, snd_right;
	short samples[0x20000];
	qbyte compressed[0x20000];
	roq_chunk_t *chunk = &cin->chunk;

	FS_Read (compressed, chunk->size, cin->file);

	if ( chunk->id == RoQ_SOUND_MONO ) {
		snd_left = chunk->argument;

		for (i = 0; i < chunk->size; i++)
		{
			snd_left += snd_sqr_arr[compressed[i]];
			samples[i] = snd_left;
		}

		S_RawSamples ( chunk->size, cin->s_rate, 2, 1, (qbyte *)samples, qfalse );
	} else if ( chunk->id == RoQ_SOUND_STEREO ) {
		snd_left = chunk->argument & 0xff00;
		snd_right = (chunk->argument & 0xff) << 8;

		for (i = 0; i < chunk->size; i += 2)
		{
			snd_left += snd_sqr_arr[compressed[i]];
			snd_right += snd_sqr_arr[compressed[i+1]];

			samples[i+0] = snd_left;
			samples[i+1] = snd_right;
		}

		S_RawSamples ( chunk->size / 2, cin->s_rate, 2, 2, (qbyte *)samples, qfalse );
	}
}
