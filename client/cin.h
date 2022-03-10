/* Aftershock 3D rendering engine
 * Copyright (C) 1999 Stephen C. Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define RoQ_HEADER1				4228
#define RoQ_HEADER2				-1
#define RoQ_HEADER3				30

#define RoQ_FRAMERATE			30

#define RoQ_INFO				0x1001
#define RoQ_QUAD_CODEBOOK		0x1002
#define RoQ_QUAD_VQ				0x1011
#define RoQ_SOUND_MONO			0x1020
#define RoQ_SOUND_STEREO		0x1021

#define RoQ_ID_MOT				0x00
#define RoQ_ID_FCC				0x01
#define RoQ_ID_SLD				0x02
#define RoQ_ID_CCC				0x03

typedef struct 
{
	byte y0, y1, y2, y3, u, v;
} roq_cell_t;

typedef struct 
{
	byte idx[4];
} roq_qcell_t;

typedef struct 
{
	unsigned short id;
	unsigned int size;
	unsigned short argument;
} roq_chunk_t;

typedef struct
{
	char		name[MAX_QPATH];

	roq_chunk_t chunk;
	roq_cell_t	cells[256];
	roq_qcell_t qcells[256];

	byte		*y[2], *u[2], *v[2];

	qboolean	restart_sound;

	int			s_rate;
	int			s_width;
	int			s_channels;

	int			width;
	int			width_2;			// width / 2
	int			height;

	int			file;
	int			remaining;

	int			time;				// Sys_Milliseconds for first cinematic frame
	int			frame;

	byte		*buf;
	byte		*pic;
	byte		*pic_pending;
} cinematics_t;

void RoQ_Init (void);
void RoQ_ReadChunk (cinematics_t *cin);
void RoQ_SkipChunk (cinematics_t *cin);
void RoQ_ReadInfo (cinematics_t *cin);
void RoQ_ReadCodebook (cinematics_t *cin);
byte *RoQ_ReadVideo (cinematics_t *cin);
void RoQ_ReadAudio (cinematics_t *cin);

