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
// snd_mem.c: sound caching

#include "client.h"
#include "snd_loc.h"

/*
================
ResampleSfx
================
*/
void ResampleSfx (sfxcache_t *sc, byte *data, char *name)
{
	int i, outcount, srcsample, srclength, samplefrac, fracstep;
	float scale = (float) sc->speed / (float) dma.speed;

	// this is usually 0.5 (128), 1 (256), or 2 (512)
	fracstep = ((double) sc->speed / (double) dma.speed) * 256.0;

	srclength = sc->length << sc->stereo;
	outcount = (double) sc->length * (double) dma.speed / (double) sc->speed;

	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = (double) sc->loopstart * (double) dma.speed / (double) sc->speed;

	sc->speed = dma.speed;

// resample / decimate to the current source rate
	if (fracstep == 256)
	{
		if (sc->width == 1) // 8bit
			for (i = 0;i < srclength;i++)
				((signed char *)sc->data)[i] = ((unsigned char *)data)[i] - 128;
		else
			for (i = 0;i < srclength;i++)
				((short *)sc->data)[i] = LittleShort (((short *)data)[i]);
	}
	else
	{
// general case
		Com_DPrintf("ResampleSfx: resampling sound %s\n", name);
		samplefrac = 0;

		if ((fracstep & 255) == 0) // skipping points on perfect multiple
		{
			srcsample = 0;
			fracstep >>= 8;
			if (sc->width == 2)
			{
				short *out = (void *)sc->data, *in = (void *)data;
				if (sc->stereo)
				{
					fracstep <<= 1;
					for (i=0 ; i<outcount ; i++)
					{
						*out++ = LittleShort (in[srcsample  ]);
						*out++ = LittleShort (in[srcsample+1]);
						srcsample += fracstep;
					}
				}
				else
				{
					for (i=0 ; i<outcount ; i++)
					{
						*out++ = LittleShort (in[srcsample  ]);
						srcsample += fracstep;
					}
				}
			}
			else
			{
				signed char *out = (void *)sc->data;
				unsigned char *in = (void *)data;

				if (sc->stereo)
				{
					fracstep <<= 1;
					for (i=0 ; i<outcount ; i++)
					{
						*out++ = in[srcsample  ] - 128;
						*out++ = in[srcsample+1] - 128;
						srcsample += fracstep;
					}
				}
				else
				{
					for (i=0 ; i<outcount ; i++)
					{
						*out++ = in[srcsample  ] - 128;
						srcsample += fracstep;
					}
				}
			}
		}
		else
		{
			int sample;
			int a, b;
			if (sc->width == 2)
			{
				short *out = (void *)sc->data, *in = (void *)data;

				if (sc->stereo)
				{
					for (i=0 ; i<outcount ; i++)
					{
						srcsample = (samplefrac >> 8) << 1;
						a = in[srcsample  ];
						if (srcsample+2 >= srclength)
							b = 0;
						else
							b = in[srcsample+2];
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (short) sample;
						a = in[srcsample+1];
						if (srcsample+2 >= srclength)
							b = 0;
						else
							b = in[srcsample+3];
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (short) sample;
						samplefrac += fracstep;
					}
				}
				else
				{
					for (i=0 ; i<outcount ; i++)
					{
						srcsample = samplefrac >> 8;
						a = in[srcsample  ];
						if (srcsample+1 >= srclength)
							b = 0;
						else
							b = in[srcsample+1];
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (short) sample;
						samplefrac += fracstep;
					}
				}
			}
			else
			{
				signed char *out = (void *)sc->data;
				unsigned char *in = (void *)data;
				if (sc->stereo) // LordHavoc: stereo sound support
				{
					for (i=0 ; i<outcount ; i++)
					{
						srcsample = (samplefrac >> 8) << 1;
						a = (int) in[srcsample  ] - 128;
						if (srcsample+2 >= srclength)
							b = 0;
						else
							b = (int) in[srcsample+2] - 128;
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (signed char) sample;
						a = (int) in[srcsample+1] - 128;
						if (srcsample+2 >= srclength)
							b = 0;
						else
							b = (int) in[srcsample+3] - 128;
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (signed char) sample;
						samplefrac += fracstep;
					}
				}
				else
				{
					for (i=0 ; i<outcount ; i++)
					{
						srcsample = samplefrac >> 8;
						a = (int) in[srcsample  ] - 128;
						if (srcsample+1 >= srclength)
							b = 0;
						else
							b = (int) in[srcsample+1] - 128;
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (signed char) sample;
						samplefrac += fracstep;
					}
				}
			}
		}
	}
}


//=============================================================================

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound (sfx_t *s)
{
    char	namebuffer[MAX_QPATH];
	byte	*data;
	wavinfo_t	info;
	int		len;
	sfxcache_t	*sc;
	int		size;
	char	*name;

	if (!s->name[0] || s->name[0] == '*')
		return NULL;

// see if still in memory
	sc = s->cache;
	if (sc)
		return sc;

// load it in
	if (s->truename)
		name = s->truename;
	else
		name = s->name;

	if (name[0] == '#')
		Com_sprintf (namebuffer, sizeof(namebuffer), &name[1]);
	else
		Com_sprintf (namebuffer, sizeof(namebuffer), name);

	size = FS_LoadFile (namebuffer, (void **)&data);

	if (!data)
	{
		Com_DPrintf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = GetWavinfo (s->name, data, size);
	if (info.channels < 1 || info.channels > 2)
	{
		Com_Printf ("%s has an invalid number of channels\n", s->name);
		FS_FreeFile (data);
		return NULL;
	}

	// calculate resampled length
	len = (int) ((double) info.samples * (double) dma.speed / (double) info.rate);
	len = len * info.width * info.channels;

	sc = s->cache = Z_Malloc (len + sizeof(sfxcache_t));
	if (!sc)
	{
		FS_FreeFile (data);
		return NULL;
	}
	
	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = (info.channels == 2);
	sc->music = !strncmp (namebuffer, "music/", 6);

	// force loopstart if it's a music file
	if ( sc->music && (sc->loopstart == -1) ) {
		sc->loopstart = 0;
	}

	ResampleSfx (sc, data + info.dataofs, s->name);

	FS_FreeFile (data);

	return sc;
}



/*
===============================================================================

WAV loading

===============================================================================
*/


byte	*data_p;
byte 	*iff_end;
byte 	*last_chunk;
byte 	*iff_data;
int 	iff_chunk_len;


short GetLittleShort(void)
{
	short val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

int GetLittleLong(void)
{
	int val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	val = val + (*(data_p+2)<<16);
	val = val + (*(data_p+3)<<24);
	data_p += 4;
	return val;
}

void FindNextChunk(char *name)
{
	while (1)
	{
		data_p = last_chunk;

		if (data_p >= iff_end)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}
		
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = NULL;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!strncmp(data_p, name, 4))
			return;
	}
}

void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}


void DumpChunks(void)
{
	char	str[5];
	
	str[4] = 0;
	data_p = iff_data;
	do
	{
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Com_Printf ("0x%x : %s (%d)\n", (int)(data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}

/*
============
GetWavinfo
============
*/
wavinfo_t GetWavinfo (char *name, byte *wav, int wavlength)
{
	wavinfo_t	info;
	int     i;
	int     format;
	int		samples;

	memset (&info, 0, sizeof(info));

	if (!wav)
		return info;
		
	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp(data_p+8, "WAVE", 4)))
	{
		Com_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p)
	{
		Com_Printf("Missing fmt chunk\n");
		return info;
	}

	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Com_Printf("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4+2;
	info.width = GetLittleShort() / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p)
		{
			if (!strncmp (data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Com_Printf("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width / info.channels;

	if (info.samples)
	{
		if (samples < info.samples)
			Com_Error (ERR_DROP, "Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;
	
	return info;
}

