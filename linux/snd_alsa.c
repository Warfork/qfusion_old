/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
*(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <alsa/asoundlib.h>

#include "../client/client.h"
#include "../client/snd_loc.h"

#define BUFFER_SAMPLES		8192
#define SUBMISSION_DIV_POW2	1

static snd_pcm_t *pcm_handle;
static snd_pcm_hw_params_t *hw_params;

static struct sndinfo *si;

static int sample_bytes;
static int buffer_bytes;

// The sample rates which will be attempted.
static int RATES[] = { 44100, 22050, 11025, 48000 };

static int SNDDMA_SetSpeed (unsigned int speed);

/*
==============
SNDDMA_Shutdown

Initialize ALSA pcm device, and bind it to sndinfo.
==============
*/
qboolean SNDDMA_Init (void)
{
	int i, err, dir;
	unsigned int r;
	snd_pcm_uframes_t p;
	cvar_t *s_device = Cvar_Get ("s_device", "default", CVAR_ARCHIVE);

	if ((err = snd_pcm_open (&pcm_handle, s_device->string, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0)
	{
		Com_Printf ("ALSA: cannot open device %s (%s)\n", s_device->string, snd_strerror (err));
		return qfalse;
	}

	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0)
	{
		Com_Printf("ALSA: cannot allocate hw params (%s)\n", snd_strerror (err));
		return qfalse;
	}

	if ((err = snd_pcm_hw_params_any (pcm_handle, hw_params)) < 0)
	{
		Com_Printf ("ALSA: cannot init hw params (%s)\n", snd_strerror (err));
		snd_pcm_hw_params_free (hw_params);
		return qfalse;
	}

	if ((err = snd_pcm_hw_params_set_access (pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
	{
		Com_Printf ("ALSA: cannot set access (%s)\n", snd_strerror (err));
		snd_pcm_hw_params_free (hw_params);
		return qfalse;
	}

	dma.samplebits = 16;	// try 16 by default
	if ((err = snd_pcm_hw_params_set_format (pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0)
	{
		Com_Printf("ALSA: 16 bit not supported, trying 8\n");
		dma.samplebits = 8;
		if ((err = snd_pcm_hw_params_set_format (pcm_handle, hw_params, SND_PCM_FORMAT_U8)) < 0)
		{
			Com_Printf ("ALSA: cannot set format (%s)\n", snd_strerror (err));
			snd_pcm_hw_params_free (hw_params);
			return qfalse;
		}
	}

	r = KHZ2RATE (s_khz->integer);
	dma.speed = SNDDMA_SetSpeed (r);	// try specified rate

	if (!dma.speed)
	{	// or all available ones
		for (i = 0; i < sizeof (RATES); i++)
		{
			if (RATES[i] == r)
				continue;
			dma.speed = SNDDMA_SetSpeed (RATES[i]);
			if (dma.speed)
				break;
		}
	}

	if (!dma.speed)
	{	// failed
		Com_Printf ("ALSA: cannot set rate\n");
		snd_pcm_hw_params_free (hw_params);
		return qfalse;
	}

	dma.channels = 2;  // stereo
	if ((err = snd_pcm_hw_params_set_channels (pcm_handle, hw_params, dma.channels)) < 0)
	{
		Com_Printf("ALSA: cannot set channels %d(%s)\n", dma.channels, snd_strerror (err));
		snd_pcm_hw_params_free (hw_params);
		return qfalse;
	}

	p = BUFFER_SAMPLES / dma.channels;
	if ((err = snd_pcm_hw_params_set_period_size_near (pcm_handle, hw_params, &p, &dir)) < 0)
	{
		Com_Printf ("ALSA: cannot set period size (%s)\n", snd_strerror (err));
		snd_pcm_hw_params_free (hw_params);
		return qfalse;
	}
	else
	{	// period succeeded, but is perhaps slightly different
		if (dir != 0) 
			Com_Printf ("ALSA: period %d not supported, using %d\n", (BUFFER_SAMPLES/dma.channels), p);
	}

	if ((err = snd_pcm_hw_params(pcm_handle, hw_params)) < 0)
	{	//set params
		Com_Printf ("ALSA: cannot set params (%s)\n", snd_strerror (err));
		snd_pcm_hw_params_free (hw_params);
		return qfalse;
	}

	dma.samplepos = 0;
	for (dma.samples = 1; dma.samples < p; dma.samples<<=1);
	dma.samples = (dma.samples >> 1) * dma.channels;
	dma.submission_chunk = dma.samples >> SUBMISSION_DIV_POW2;

	sample_bytes = dma.samplebits / 8;
	buffer_bytes = dma.samples * sample_bytes;

	dma.buffer = Mem_ZoneMalloc (buffer_bytes);  // allocate pcm frame buffer
	memset (dma.buffer, 0, buffer_bytes);

	snd_pcm_prepare (pcm_handle);

	return qtrue;
}

/*
==============
SNDDMA_SetSpeed

==============
*/
int SNDDMA_SetSpeed (unsigned int speed)
{
	int err, dir = 0;
	unsigned int r = speed;

	if ((err = snd_pcm_hw_params_set_rate_near (pcm_handle, hw_params, &r, &dir)) < 0)
	{
		Com_Printf ("ALSA: cannot set rate %d (%s)\n", r, snd_strerror (err));
		return 0;
	}

	// rate succeeded, but is perhaps slightly different
	if (dir != 0) 
		Com_Printf ("ALSA: rate %d not supported, using %d\n", speed, r);
	return r;
}

/*
==============
SNDDMA_Shutdown

Returns the current sample position, if sound is running.
===============
*/
int SNDDMA_GetDMAPos (void)
{
	if (dma.buffer)
		return dma.samplepos;
	return 0;
}

/*
==============
SNDDMA_Shutdown

Closes the ALSA pcm device and frees the dma buffer.
===============
*/
void SNDDMA_Shutdown (void)
{
	if (!dma.buffer)
		return;

	snd_pcm_drop (pcm_handle);
	snd_pcm_close (pcm_handle);

	Mem_ZoneFree (dma.buffer);
	dma.buffer = NULL;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit (void)
{
	int w, len;
	qbyte *chunk;

	if (!dma.buffer)
		return;

	len = dma.submission_chunk;
	chunk = ( qbyte * )dma.buffer;
	while( len > 0 ) {
		w = snd_pcm_writei(pcm_handle, chunk, len);  // write to card

		if ( w < 0 ) {
			if ( w == -EAGAIN )
				continue;
			if ( w == -ESTRPIPE ) {
				do {
					w = snd_pcm_resume (pcm_handle);
				} while ( w == -EAGAIN );
			}
			if ( w < 0 )
				w = snd_pcm_prepare (pcm_handle);
			if ( w < 0 )
				return;
			continue;
		}

		len -= w;
		chunk += w * dma.channels;
		dma.samplepos += w * dma.channels;
	}
}

/*
==============
SNDDMA_BeginPainting

Callback provided by the engine in case we need it.  We don't.
===============
*/
void SNDDMA_BeginPainting (void) {
}
