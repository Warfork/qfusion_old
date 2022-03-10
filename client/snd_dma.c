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
// snd_dma.c -- main control for any streaming sound output device

#include "client.h"
#include "snd_loc.h"

void S_Play(void);
void S_SoundList(void);
void S_Update_();
void S_StopAllSounds(void);
void S_Restart (qboolean noVideo);
void S_Music( void );
void S_StopBackgroundTrack( void );
void S_ClearBuffer (void);

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t   channels[MAX_CHANNELS];
int			numChannels;
int			numAutoLoops;

qboolean	snd_initialized = qfalse;
int			sound_started = 0;

dma_t		dma;

vec3_t		listener_origin;
vec3_t		listener_right;

int			soundtime;		// sample PAIRS
int   		paintedtime; 	// sample PAIRS

qboolean	s_openALEnabled = qfalse;

sfx_t		known_sfx[MAX_SFX];
int			num_sfx;

loopsfx_t	loop_sfx[MAX_LOOPSFX];
int			num_loopsfx;

playsound_t	s_playsounds[MAX_PLAYSOUNDS];
playsound_t	s_freeplays;
playsound_t	s_pendingplays;
playsound_t s_dummyplaysound;

int			s_beginofs;

cvar_t		*s_volume;
cvar_t		*s_musicvolume;
cvar_t		*s_testsound;
cvar_t		*s_khz;
cvar_t		*s_show;
cvar_t		*s_mixahead;
cvar_t		*s_swapstereo;
cvar_t		*s_vorbis;
cvar_t		*s_openAL;

mempool_t	*s_mempool;

int		s_rawend;
portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

bgTrack_t	*s_bgTrack;
bgTrack_t	s_bgTrackIntro;
bgTrack_t	s_bgTrackLoop;

// ====================================================================
// User-setable variables
// ====================================================================

/*
================
S_SoundInfo_f
================
*/
void S_SoundInfo_f( void )
{
	if( !sound_started ) {
		Com_Printf( "S_SoundInfo_f: sound system not started\n" );
		return;
	}
	if( s_openALEnabled ) {
		SNDOAL_Info_f ();
		return;
	}

    Com_Printf( "\n" );
    Com_Printf( "%5d stereo\n", dma.channels - 1 );
    Com_Printf( "%5d samples\n", dma.samples );
    Com_Printf( "%5d samplepos\n", dma.samplepos );
    Com_Printf( "%5d samplebits\n", dma.samplebits );
    Com_Printf( "%5d submission_chunk\n", dma.submission_chunk );
    Com_Printf( "%5d speed\n", dma.speed );
    Com_Printf( "0x%x dma buffer\n", dma.buffer );
}

/*
================
S_SoundRestart_f
================
*/
void S_SoundRestart_f( void ) {
	S_Restart( qfalse );
}

/*
================
S_Init
================
*/
void S_Init( void )
{
	cvar_t	*cv;

	s_openALEnabled = qfalse;

	numChannels = 0;

	Com_Printf( "\n------- sound initialization -------\n" );

	Cmd_AddCommand( "snd_restart", S_SoundRestart_f );

	cv = Cvar_Get( "s_initsound", "1", 0 );
	if( !cv->integer ) {
		Com_Printf( "not initializing.\n" );
	} else {
		s_volume = Cvar_Get( "s_volume", "0.7", CVAR_ARCHIVE );
		s_musicvolume = Cvar_Get( "s_musicvolume", "0.25", CVAR_ARCHIVE );
		s_khz = Cvar_Get( "s_khz", "22", CVAR_ARCHIVE );
		s_mixahead = Cvar_Get( "s_mixahead", "0.2", CVAR_ARCHIVE );
		s_show = Cvar_Get( "s_show", "0", CVAR_CHEAT );
		s_testsound = Cvar_Get( "s_testsound", "0", 0 );
		s_swapstereo = Cvar_Get( "s_swapstereo", "0", CVAR_ARCHIVE );
		s_vorbis = Cvar_Get( "s_vorbis", "1", CVAR_ARCHIVE );
		s_openAL = Cvar_Get( "s_openAL", "1", CVAR_ARCHIVE );

		s_volume->modified = qtrue;
		s_musicvolume->modified = qtrue;

		Cmd_AddCommand( "play", S_Play );
		Cmd_AddCommand( "music", S_Music );
		Cmd_AddCommand( "stopsound", S_StopAllSounds );
		Cmd_AddCommand( "stopmusic", S_StopBackgroundTrack );
		Cmd_AddCommand( "soundlist", S_SoundList );
		Cmd_AddCommand( "soundinfo", S_SoundInfo_f );

		if( s_openAL->integer )
			s_openALEnabled = SNDOAL_Init ();

		if( !s_openALEnabled ) {
			if( !SNDDMA_Init() )
				return;

			numChannels = MAX_CHANNELS;
			numAutoLoops = MAX_LOOPSFX;
		}

		SNDOGG_Init ();

		sound_started = 1;
		num_sfx = 0;
		num_loopsfx = 0;

		soundtime = 0;
		paintedtime = 0;

		S_SoundInfo_f ();

		s_mempool = Mem_AllocPool ( NULL, "Sounds" );

		S_StopAllSounds ();
	}

	Com_Printf ("------------------------------------\n");
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown( void )
{
	Cmd_RemoveCommand( "snd_restart" );

	if( !sound_started )
		return;

	// clear sound buffer
	// according to OpenAL specs all sources should be stopped
	// automatically, but I wouldn't rely on that
	S_ClearBuffer ();

	// free all sounds
	S_FreeSounds ();

	if( s_openALEnabled )
		SNDOAL_Shutdown ();
	else
		SNDDMA_Shutdown ();

	SNDOGG_Shutdown ();

	sound_started = 0;

	Cmd_RemoveCommand( "play" );
	Cmd_RemoveCommand( "music" );
	Cmd_RemoveCommand( "stopsound" );
	Cmd_RemoveCommand( "stopmusic" );
	Cmd_RemoveCommand( "soundlist" );
	Cmd_RemoveCommand( "soundinfo" );

	Mem_FreePool( &s_mempool );

	num_sfx = 0;
	num_loopsfx = 0;
}


/*
=================
S_Restart

Restart the sound subsystem so it can pick up new parameters and flush all sounds
=================
*/
void S_Restart( qboolean noVideo )
{
	S_Shutdown ();
	S_Init ();

	if( !noVideo )
		VID_Restart ();
}

// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
sfx_t *S_FindName (char *name, qboolean create)
{
	int		i;
	sfx_t	*sfx;

	if (!name)
		Com_Error (ERR_FATAL, "S_FindName: NULL");
	if (!name[0])
	{
		*((int *)0) = -1;
		Com_Error (ERR_FATAL, "S_FindName: empty name");
	}

	if (strlen(name) >= MAX_QPATH)
		Com_Error (ERR_FATAL, "Sound name too long: %s", name);

	// see if already loaded
	for (i=0 ; i < num_sfx ; i++)
		if (!strcmp(known_sfx[i].name, name))
		{
			return &known_sfx[i];
		}

	if (!create)
		return NULL;

	// find a free sfx
	for (i=0 ; i < num_sfx ; i++)
		if (!known_sfx[i].name[0])
			break;

	if (i == num_sfx)
	{
		if (num_sfx == MAX_SFX)
			Com_Error (ERR_FATAL, "S_FindName: out of sfx_t");
		num_sfx++;
	}

	sfx = &known_sfx[i];
	memset (sfx, 0, sizeof(*sfx));
	Q_strncpyz (sfx->name, name, sizeof(sfx->name));

	return sfx;
}

/*
==================
S_RegisterSound
==================
*/
sfx_t *S_RegisterSound (char *name)
{
	sfx_t	*sfx;

	if (!sound_started)
		return NULL;

	sfx = S_FindName (name, qtrue);
	S_LoadSound (sfx);

	if( sfx && s_openALEnabled )
		SNDOAL_RegisterSound( sfx );

	return sfx;
}


/*
=====================
S_FreeSounds
=====================
*/
void S_FreeSounds (void)
{
	int		i;
	sfx_t	*sfx;

	if( s_openALEnabled )
		SNDOAL_FreeSounds ();

	// free all sounds
	for (i=0, sfx=known_sfx ; i < num_sfx ; i++,sfx++)
	{
		if (!sfx->name[0])
			continue;
		if (sfx->cache)
			S_Free (sfx->cache);
		memset (sfx, 0, sizeof(*sfx));
	}

	memset( loop_sfx, 0, sizeof(loop_sfx) );

	S_StopBackgroundTrack ();
}

/*
=====================
S_SoundsInMemory

=====================
*/
void S_SoundsInMemory (void)
{
	int		i;
	sfx_t	*sfx;
	int		size;

	if( s_openALEnabled )
		return;

	// free any sounds not from this registration sequence
	for (i=0, sfx=known_sfx ; i < num_sfx ; i++,sfx++)
	{
		if (!sfx->name[0])
			continue;

		// make sure it is paged in
		if (sfx->cache)
		{
			size = sfx->cache->length*sfx->cache->width*sfx->cache->channels;
			Com_PageInMemory ((qbyte *)sfx->cache->data, size);
		}
	}
}

//=============================================================================

/*
=================
S_PickChannel
=================
*/
channel_t *S_PickChannel(int entnum, int entchannel)
{
    int			ch_idx;
    int			first_to_die;
    int			life_left;
	channel_t	*ch;

	if (entchannel < 0)
		Com_Error (ERR_DROP, "S_PickChannel: entchannel < 0");

// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = 0x7fffffff;
    for (ch_idx=0 ; ch_idx < numChannels ; ch_idx++)
    {
		if (entchannel != 0		// channel 0 never overrides
		&& channels[ch_idx].entnum == entnum
		&& channels[ch_idx].entchannel == entchannel)
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.playernum+1 && entnum != cl.playernum+1 && channels[ch_idx].sfx)
			continue;

		// if at end of loop, restart
		if( !channels[ch_idx].sfx ) {
			first_to_die = ch_idx;
			break;
		}

		if( s_openALEnabled ) {
			first_to_die = SNDOAL_ChannelFirstToDie( ch_idx, first_to_die, &life_left );
			if( life_left == 0 )
				break;
		} else {
			if (channels[ch_idx].end - paintedtime < life_left)
			{
				life_left = channels[ch_idx].end - paintedtime;
				first_to_die = ch_idx;
			}
		}
	}

	if (first_to_die == -1)
		return NULL;

	ch = &channels[first_to_die];
	memset (ch, 0, sizeof(*ch));

    return ch;
}

/*
=================
S_SpatializeOrigin

Used for spatializing channels and autosounds
=================
*/
void S_SpatializeOrigin (vec3_t origin, float master_vol, float dist_mult, int *left_vol, int *right_vol)
{
    vec_t		dot;
    vec_t		dist;
    vec_t		lscale, rscale, scale;
    vec3_t		source_vec;

	if (cls.state != ca_active)
	{
		*left_vol = *right_vol = 255;
		return;
	}

// calculate stereo seperation and distance attenuation
	VectorSubtract (origin, listener_origin, source_vec);

	dist = VectorNormalize (source_vec);
	dist -= SOUND_FULLVOLUME;
	if (dist < 0)
		dist = 0;			// close enough to be at full volume
	dist *= dist_mult;		// different attenuation levels

	dot = DotProduct (listener_right, source_vec);

	if (dma.channels == 1 || !dist_mult)
	{ // no attenuation = no spatialization
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 0.5 * (1.0 + dot);
		lscale = 0.5 * (1.0 - dot);
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	*right_vol = (int) (master_vol * scale);
	if (*right_vol < 0)
		*right_vol = 0;

	scale = (1.0 - dist) * lscale;
	*left_vol = (int) (master_vol * scale);
	if (*left_vol < 0)
		*left_vol = 0;
}

/*
=================
S_Spatialize
=================
*/
void S_Spatialize(channel_t *ch)
{
	vec3_t		origin;

	// anything coming from the view entity will always be full volume
	if (ch->entnum == cl.playernum+1)
	{
		if( s_openALEnabled )
			SNDOAL_Spatialize( ch - channels, ch->entnum, vec3_origin, vec3_origin, 0, s_volume->value * ch->master_vol );
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	if (ch->fixed_origin)
		VectorCopy (ch->origin, origin);
	else
		CL_GameModule_GetEntitySoundOrigin (ch->entnum, origin);

	if( s_openALEnabled )
		SNDOAL_Spatialize( ch - channels, ch->entnum, origin, vec3_origin, ch->dist_mult, s_volume->value * ch->master_vol );
	else
		S_SpatializeOrigin (origin, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol);
}


/*
=================
S_AllocPlaysound
=================
*/
playsound_t *S_AllocPlaysound (void)
{
	playsound_t	*ps;

	ps = s_freeplays.next;
	if (ps == &s_freeplays)
		return NULL;		// no free playsounds

	// unlink from freelist
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	return ps;
}

/*
=================
S_FreePlaysound
=================
*/
void S_FreePlaysound (playsound_t *ps)
{
	if (ps == &s_dummyplaysound)
		return;

	// unlink from channel
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// add to free list
	ps->next = s_freeplays.next;
	s_freeplays.next->prev = ps;
	ps->prev = &s_freeplays;
	s_freeplays.next = ps;
}


/*
=================
S_PickPlaysoundChannel
=================
*/
channel_t *S_PickPlaysoundChannel (playsound_t *ps)
{
	channel_t	*ch;

	// pick a channel to play on
	ch = S_PickChannel(ps->entnum, ps->entchannel);
	if (!ch)
	{
		S_FreePlaysound (ps);
		return NULL;
	}

	if (ps->attenuation == ATTN_STATIC)
		ch->dist_mult = ps->attenuation * 0.001;
	else
		ch->dist_mult = ps->attenuation * 0.0005;
	ch->master_vol = ps->volume;
	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sfx = ps->sfx;
	VectorCopy (ps->origin, ch->origin);
	ch->fixed_origin = ps->fixed_origin;

	return ch;
}

/*
===============
S_IssuePlaysound

Take the next playsound and begin it on the channel
This is never called directly by S_Play*, but only
by the update loop.
===============
*/
void S_IssuePlaysound (playsound_t *ps)
{
	channel_t *ch;

	if (s_show->integer)
		Com_Printf ("Issue %i\n", ps->begin);

	// pick channel
	ch = S_PickPlaysoundChannel (ps);
	if (!ch)
		return;
	ch->pos = 0;
    ch->end = paintedtime + ps->sfx->cache->length;

	// spatialize
	S_Spatialize(ch);

	// free the playsound
	S_FreePlaysound (ps);
}

/*
===============
S_StartPlaysound
===============
*/
void S_StartPlaysound (playsound_t *ps)
{
	int	start;
	playsound_t	*sort;

	// drift s_beginofs
	start = cl.frame.serverTime * 0.001 * dma.speed + s_beginofs;
	if (start < paintedtime)
	{
		start = paintedtime;
		s_beginofs = start - (cl.frame.serverTime * 0.001 * dma.speed);
	}
	else if (start > paintedtime + 0.3 * dma.speed)
	{
		start = paintedtime + 0.1 * dma.speed;
		s_beginofs = start - (cl.frame.serverTime * 0.001 * dma.speed);
	}
	else
	{
		s_beginofs -= 10;
	}

	if (!ps->begin)
		ps->begin = paintedtime;
	else
		ps->begin += start;

	// sort into the pending sound list
	for (sort = s_pendingplays.next ; 
	sort != &s_pendingplays && sort->begin < ps->begin ;
	sort = sort->next);

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}

// =======================================================================
// Start a sound effect
// =======================================================================

/*
====================
S_StartSound

Validates the parms and ques the sound up
if pos is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
void S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t *sfx, float fvol, float attenuation, float timeofs)
{
	sfxcache_t	*sc;
	playsound_t	*ps;

	if (!sound_started)
		return;
	if (!sfx)
		return;

	// make sure the sound is loaded
	sc = S_LoadSound (sfx);
	if (!sc)
		return;		// couldn't load the sound's data

	// make the playsound_t
	if( s_openALEnabled ) {
		ps = &s_dummyplaysound;
		ps->volume = fvol;
	} else {
		ps = S_AllocPlaysound ();
		if (!ps)
			return;
		ps->volume = fvol*255;
	}

	if (origin)
	{
		VectorCopy (origin, ps->origin);
		ps->fixed_origin = qtrue;
	}
	else
		ps->fixed_origin = qfalse;
	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->sfx = sfx;
	ps->attenuation = attenuation;
	ps->begin = timeofs * dma.speed;

	if( s_openALEnabled )
		SNDOAL_StartPlaysound( ps );
	else
		S_StartPlaysound( ps );
}


/*
==================
S_StartLocalSound
==================
*/
void S_StartLocalSound (char *sound)
{
	sfx_t	*sfx;

	if (!sound_started)
		return;
		
	sfx = S_RegisterSound (sound);
	if (!sfx)
	{
		Com_Printf ("S_StartLocalSound: can't cache %s\n", sound);
		return;
	}

	S_StartSound (NULL, cl.playernum+1, 0, sfx, 1, 1, 0);
}


/*
==================
S_ClearBuffer
==================
*/
void S_ClearBuffer (void)
{
	if (!sound_started)
		return;

	s_rawend = 0;

	if( s_openALEnabled ) {
		SNDOAL_ClearBuffer ();
	} else {
		int		clear;

		if (dma.samplebits == 8)
			clear = 0x80;
		else
			clear = 0;

		SNDDMA_BeginPainting ();
		if (dma.buffer)
			memset(dma.buffer, clear, dma.samples * dma.samplebits/8);
		SNDDMA_Submit ();
	}
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds(void)
{
	int		i;

	if (!sound_started)
		return;

	// clear all the playsounds
	memset(s_playsounds, 0, sizeof(s_playsounds));
	s_freeplays.next = s_freeplays.prev = &s_freeplays;
	s_pendingplays.next = s_pendingplays.prev = &s_pendingplays;

	for (i=0 ; i<MAX_PLAYSOUNDS ; i++)
	{
		s_playsounds[i].prev = &s_freeplays;
		s_playsounds[i].next = s_freeplays.next;
		s_playsounds[i].prev->next = &s_playsounds[i];
		s_playsounds[i].next->prev = &s_playsounds[i];
	}

	// clear all the channels
	memset(channels, 0, sizeof(channels));

	// clear all autoloops
	memset( loop_sfx, 0, sizeof(loop_sfx) );

	S_ClearBuffer ();

	S_StopBackgroundTrack ();
}

/*
==================
S_AddLoopSound
==================
*/
void S_AddLoopSound (sfx_t *sfx, int entnum, vec3_t origin, vec3_t velocity)
{
	int i;
	int free = -1;

	if (!sfx)
		return;

	for( i = 0; i < numAutoLoops; i++ ) {
		if( !loop_sfx[i].mode && free < 0 )
			free = i;
		if( loop_sfx[i].entnum == entnum+1 && loop_sfx[i].sfx == sfx ) {
			if( loop_sfx[i].mode == 1 )
				break;
			else if( loop_sfx[i].mode > 1 )
				return;
		}
	}

	if( i < numAutoLoops ) {		// resume
		// do nothing on next loop
		loop_sfx[i].mode = 2;
		VectorCopy (origin, loop_sfx[i].origin);
		return;
	}
	if( free < 0 )
		return;

	loop_sfx[free].mode = 3;		// start or restart
	loop_sfx[free].sfx = sfx;
	loop_sfx[free].entnum = entnum+1;
	VectorCopy (origin, loop_sfx[free].origin);
}

/*
==================
S_AddLoopSounds

Entities with a ->sound field will generated looped sounds
that are automatically started, stopped, and merged together
as the entities are sent to the client
==================
*/
void S_AddLoopSounds (void)
{
	int			i;
	int			j;
	int			left, right, left_total, right_total;
	channel_t	*ch;
	sfx_t		*sfx;
	sfxcache_t	*sc;

	if( s_openALEnabled ) {
		// add loopsounds
		SNDOAL_AddLoopSounds ();
		return;
	}

	if (cl_paused->integer || cls.state != ca_active || !cl.soundPrepped)
	{
		memset( loop_sfx, 0, sizeof(loop_sfx) );
		return;
	}

	for (i=0 ; i<numAutoLoops ; i++)
	{
		if (!loop_sfx[i].sfx)
			continue;
		if (!loop_sfx[i].mode)
			continue;

		sfx = loop_sfx[i].sfx;
		sc = sfx->cache;
		if (!sc)
			continue;

		// find the total contribution of all sounds of this type
		S_SpatializeOrigin (loop_sfx[i].origin, 255.0, SOUND_LOOPATTENUATE,
			&left_total, &right_total);
		for (j=i+1 ; j<num_loopsfx ; j++)
		{
			if (loop_sfx[j].sfx != loop_sfx[i].sfx || !loop_sfx[j].mode)
				continue;
			loop_sfx[j].mode = 0;	// don't check this again later

			S_SpatializeOrigin (loop_sfx[j].origin, 255.0, SOUND_LOOPATTENUATE, 
				&left, &right);
			left_total += left;
			right_total += right;
		}

		if (left_total == 0 && right_total == 0)
			continue;		// not audible

		// allocate a channel
		ch = S_PickChannel(0, 0);
		if (!ch)
			return;

		if (left_total > 255)
			left_total = 255;
		if (right_total > 255)
			right_total = 255;
		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->autosound = qtrue;	// remove next frame
		ch->sfx = sfx;
		ch->pos = paintedtime % sc->length;
		ch->end = paintedtime + sc->length - ch->pos;

		loop_sfx[i].mode = 0;
	}
}

//=============================================================================

/*
============
S_RawSamples

Cinematic streaming and voice over network. This code eats babies.
============
*/
void S_RawSamples (int samples, int rate, int width, int channels, qbyte *data)
{
	if( !sound_started )
		return;

	if( s_openALEnabled ) {
		SNDOAL_RawSamples( samples, rate, width, channels, data );
	} else {
		int		i, snd_vol;
		int		a, b, src, dst;
		int		fracstep, samplefrac;
		int		incount, outcount;

		snd_vol = (int)(s_musicvolume->value * 256);
		if( snd_vol < 0 )
			snd_vol = 0;

		src = 0;
		samplefrac = 0;
		fracstep = (((double)rate) / (double)dma.speed) * 256.0;
		outcount = (double)samples * (double) dma.speed / (double) rate;
		incount = samples * channels;

#define TAKE_SAMPLE(s)	(sizeof(*in) == 1 ? (a = (in[src+(s)]-128)<<8,\
		b = (src < incount - channels) ? (in[src+channels+(s)]-128)<<8 : 128) : \
		(a = in[src+(s)],\
		b = (src < incount - channels) ? (in[src+channels+(s)]) : 0))

#define LERP_SAMPLE ((((((b - a) * (samplefrac & 255)) >> 8) + a) * snd_vol))

#define RESAMPLE_RAW \
		if( channels == 2 ) { \
			for( i = 0; i < outcount; i++, samplefrac += fracstep, src = (samplefrac >> 8) << 1 ) { \
				dst = s_rawend++ & (MAX_RAW_SAMPLES - 1); \
				TAKE_SAMPLE(0); \
				s_rawsamples[dst].left = LERP_SAMPLE; \
				TAKE_SAMPLE(1); \
				s_rawsamples[dst].right = LERP_SAMPLE; \
			} \
		} else { \
			for( i = 0; i < outcount; i++, samplefrac += fracstep, src = (samplefrac >> 8) << 0 ) { \
				dst = s_rawend++ & (MAX_RAW_SAMPLES - 1); \
				TAKE_SAMPLE(0); \
				s_rawsamples[dst].left = LERP_SAMPLE; \
				s_rawsamples[dst].right = s_rawsamples[dst].left; \
			} \
		}

		if( s_rawend < paintedtime )
			s_rawend = paintedtime;

		if( width == 2 ) {
			short *in = (short *)data;
			RESAMPLE_RAW
		} else {
			unsigned char *in = (unsigned char *)data;
			RESAMPLE_RAW
		}
	}
}

//=============================================================================

/*
============
S_BackgroundTrack_FindNextChunk
============
*/
qboolean S_BackgroundTrack_FindNextChunk( char *name, int *last_chunk, int file )
{
	char chunkName[4];
	int iff_chunk_len;

	while( 1 ) {
		FS_Seek( file, *last_chunk, FS_SEEK_SET );

		if( FS_Eof( file ) )
			return qfalse;	// didn't find the chunk

		FS_Seek( file, 4, FS_SEEK_CUR );
		FS_Read( &iff_chunk_len, sizeof( iff_chunk_len ), file );
		iff_chunk_len = LittleLong( iff_chunk_len );
		if( iff_chunk_len < 0 )
			return qfalse;	// didn't find the chunk

		FS_Seek( file, -8, FS_SEEK_CUR );
		*last_chunk = FS_Tell( file ) + 8 + ( (iff_chunk_len + 1) & ~1 );
		FS_Read( chunkName, 4, file );
		if( !strncmp( chunkName, name, 4 ) )
			return qtrue;
	}
}

/*
============
S_BackgroundTrack_GetWavinfo
============
*/
int S_BackgroundTrack_GetWavinfo( char *name, wavinfo_t *info )
{
	short	t;
	int		samples, file;
	int 	iff_data, last_chunk;
	char	chunkName[4];

	last_chunk = 0;
	memset( info, 0, sizeof( wavinfo_t ) );

	if( FS_FOpenFile( name, &file, FS_READ ) == -1 ) {
		Com_DPrintf("no mus\n");
		return 0;
	}

	// find "RIFF" chunk
	if( !S_BackgroundTrack_FindNextChunk( "RIFF", &last_chunk, file ) ) {
		Com_Printf( "Missing RIFF chunk\n" );
		return 0;
	}

	FS_Read( chunkName, 4, file );
	if( !strncmp( chunkName, "WAVE", 4 ) ) {
		Com_Printf( "Missing WAVE chunk\n" );
		return 0;
	}

	// get "fmt " chunk
	iff_data = FS_Tell( file ) + 4;
	last_chunk = iff_data;
	if( !S_BackgroundTrack_FindNextChunk( "fmt ", &last_chunk, file ) ) {
		Com_Printf( "Missing fmt chunk\n" );
		return 0;
	}

	FS_Read( chunkName, 4, file );

	FS_Read( &t, sizeof( t ), file );
	if( LittleShort( t ) != 1 ) {
		Com_Printf("Microsoft PCM format only\n");
		return 0;
	}

	FS_Read( &t, sizeof( t ), file );
	info->channels = LittleShort( t );

	FS_Read( &info->rate, sizeof( info->rate ), file );
	info->rate = LittleLong( info->rate );

	FS_Seek( file, 4 + 2, FS_SEEK_CUR );

	FS_Read( &t, sizeof( t ), file );
	info->width = LittleShort( t ) / 8;

	info->loopstart = 0;

	// find data chunk
	last_chunk = iff_data;
	if( !S_BackgroundTrack_FindNextChunk( "data", &last_chunk, file ) ) {
		Com_Printf( "Missing data chunk\n" );
		return 0;
	}

	FS_Read( &samples, sizeof( samples ), file );
	info->samples = LittleLong( samples ) / info->width / info->channels;

	info->dataofs = FS_Tell( file );

	return file;
}

/*
============
S_StartBackgroundTrack
============
*/
void S_StartBackgroundTrack( char *intro, char *loop )
{
	if( !sound_started )
		return;

	S_StopBackgroundTrack ();

	if( !intro || !intro[0] )
		return;

	if( !SNDOGG_OpenTrack( intro, &s_bgTrackIntro ) ) {
		s_bgTrackIntro.file = S_BackgroundTrack_GetWavinfo( intro, &s_bgTrackIntro.info );
		if( !s_bgTrackIntro.file || !s_bgTrackIntro.info.samples )
			return;
	}

	if( loop && loop[0] && Q_stricmp( intro, loop ) ) {
		if( !SNDOGG_OpenTrack( loop, &s_bgTrackLoop ) )
			s_bgTrackLoop.file = S_BackgroundTrack_GetWavinfo( loop, &s_bgTrackLoop.info );
	}

	if( !s_bgTrackLoop.file || !s_bgTrackLoop.info.samples )
		s_bgTrackLoop = s_bgTrackIntro;
	s_bgTrack = &s_bgTrackIntro;
}

/*
============
S_StopBackgroundTrack
============
*/
void S_StopBackgroundTrack( void )
{
	if( !sound_started || !s_bgTrack )
		return;

	if( s_openALEnabled )
		SNDOAL_StopBackgroundTrack ();

	if( s_bgTrackIntro.file != s_bgTrackLoop.file ) {
		if( s_bgTrackIntro.close )
			s_bgTrackIntro.close( &s_bgTrackIntro );
		else
			FS_FCloseFile( s_bgTrackIntro.file );
	}

	if( s_bgTrackLoop.close )
		s_bgTrackLoop.close( &s_bgTrackLoop );
	else
		FS_FCloseFile( s_bgTrackLoop.file );

	s_bgTrack = NULL;
	memset( &s_bgTrackIntro, 0, sizeof( bgTrack_t ) );
	memset( &s_bgTrackLoop, 0, sizeof( bgTrack_t ) );
}

/*
============
S_Music
============
*/
void S_Music( void )
{
	if( Cmd_Argc () < 2 ) {
		Com_Printf( "music: <introfile> [loopfile]\n" );
		return;
	}

	S_StartBackgroundTrack( Cmd_Argv( 1 ), Cmd_Argv( 2 ) );
}

//=============================================================================

/*
============
S_UpdateBackgroundTrack
============
*/
void S_UpdateBackgroundTrack( void )
{
	int		samples, maxSamples;
	int		read, maxRead, total;
	float	scale;
	qbyte	data[MAX_RAW_SAMPLES*4];
	qboolean swap = qfalse;
#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
	qbyte	swaptest[2] = {1, 0};
#endif

#ifdef ENDIAN_BIG
	swap = qtrue;
#elif !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
	swap = (*(short *)swaptest == 0);
#endif

	if( s_openALEnabled ) {
		if( !SNDOAL_UpdateBackgroundTrack () )
			return;
	}

	if( !s_bgTrack || !s_musicvolume->value )
		return;

	if( s_rawend < paintedtime )
		s_rawend = paintedtime;

	scale = (float)s_bgTrack->info.rate / dma.speed;
	maxSamples = sizeof( data ) / s_bgTrack->info.channels / s_bgTrack->info.width;

	swap = swap && (s_bgTrack->info.width == 2);

	do {
		samples = ( paintedtime + MAX_RAW_SAMPLES - s_rawend ) * scale;
		if( samples <= 0 )
			return;
		if( samples > maxSamples )
			samples = maxSamples;
		maxRead = samples * s_bgTrack->info.channels * s_bgTrack->info.width;

		total = 0;
		while( total < maxRead ) {
			if( s_bgTrack->read )
				read = s_bgTrack->read( s_bgTrack, data + total, maxRead - total );
			else
				read = FS_Read( data + total, maxRead - total, s_bgTrack->file );

			if( !read ){
				if( s_bgTrackIntro.file != s_bgTrackLoop.file ) {
					if( s_bgTrackIntro.close )
						s_bgTrackIntro.close( &s_bgTrackIntro );
					else
						FS_FCloseFile( s_bgTrackIntro.file );
					s_bgTrackIntro = s_bgTrackLoop;
				}

				s_bgTrack = &s_bgTrackLoop;
				if( s_bgTrack->seek )
					s_bgTrack->seek( s_bgTrack, s_bgTrack->info.dataofs );
				else
					FS_Seek( s_bgTrack->file, s_bgTrack->info.dataofs, FS_SEEK_SET );
			}

			total += read;
		}

		if( swap ) {
			int j;

			if( s_bgTrack->info.channels == 2 ) {
				for( j = 0; j < samples; j++ ) {
					((short *)data)[j*2] = LittleShort( ((short *)data)[j*2] );
					((short *)data)[j*2+1] = LittleShort( ((short *)data)[j*2+1] );
				}
			} else {
				for( j = 0; j < samples; j++ ) {
					((short *)data)[j] = LittleShort( ((short *)data)[j] );
				}
			}
		}

		S_RawSamples( samples, s_bgTrack->info.rate, s_bgTrack->info.width, s_bgTrack->info.channels, data );
	} while( !s_openALEnabled );
}

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(vec3_t origin, vec3_t velocity, vec3_t forward, vec3_t right, vec3_t up)
{
	int			i;
	channel_t	*ch;

	if (!sound_started)
		return;

	// if the loading plaque is up, clear everything
	// out to make sure we aren't looping a dirty
	// dma buffer while loading
	if (cls.disable_screen)
	{
		S_ClearBuffer ();
		return;
	}

	VectorCopy (origin, listener_origin);
	VectorCopy (right, listener_right);

	if( s_openALEnabled ) {
		SNDOAL_Update( origin, velocity, forward, right, up );
	} else {
		// rebuild scale tables if volume is modified
		if (s_volume->modified)
			S_InitScaletable ();

		// update spatialization for dynamic sounds	
		ch = channels;
		for (i=0 ; i<numChannels; i++, ch++)
		{
			if (!ch->sfx)
				continue;
			if (ch->autosound)
			{	// autosounds are regenerated fresh each frame
				memset (ch, 0, sizeof(*ch));
				continue;
			}

			S_Spatialize(ch);         // respatialize channel

			if (!ch->leftvol && !ch->rightvol)
			{
				memset (ch, 0, sizeof(*ch));
				continue;
			}
		}
	}

	// add loopsounds
	S_AddLoopSounds ();

// mix some sound
	S_UpdateBackgroundTrack ();

	if( !s_openALEnabled )
		S_Update_();
}

void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

	fullsamples = dma.samples / dma.channels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped
		
		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds ();
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers*fullsamples + samplepos/dma.channels;
}


void S_Update_(void)
{
	unsigned        endtime;
	int				samps;

	if (!sound_started)
		return;

	//
	// debugging output
	//
	if (s_show->integer)
	{
		int			i, total = 0;
		channel_t	*ch = channels;

		for (i=0 ; i<numChannels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol) )
			{
				Com_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}
		
		Com_Printf ("----(%i)---- painted: %i\n", total, paintedtime);
	}

	SNDDMA_BeginPainting ();

	if (!dma.buffer)
		return;

// Updates DMA time
	GetSoundtime();

// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		Com_DPrintf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

// mix ahead of current position
	endtime = soundtime + s_mixahead->value * dma.speed;

	// mix to an even submission block size
	endtime = (endtime + dma.submission_chunk-1)
		& ~(dma.submission_chunk-1);
	samps = dma.samples >> (dma.channels-1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	S_PaintChannels (endtime);

	SNDDMA_Submit ();
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play(void)
{
	int 	i;
	char name[256];
	sfx_t	*sfx;
	
	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			strcpy(name, Cmd_Argv(i));
			strcat(name, ".wav");
		}
		else
			strcpy(name, Cmd_Argv(i));
		sfx = S_RegisterSound(name);
		S_StartSound(NULL, cl.playernum+1, 0, sfx, 1.0, 1.0, 0);
		i++;
	}
}

void S_SoundList(void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;

	total = 0;
	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		if (!sfx->name[0])
			continue;
		sc = sfx->cache;
		if (sc)
		{
			size = sc->length*sc->width*sc->channels;
			total += size;
			if (sc->loopstart >= 0)
				Com_Printf ("L");
			else
				Com_Printf (" ");
			Com_Printf ("(%2db) %6i : %s\n",sc->width*8,  size, sfx->name);
		}
		else
		{
			Com_Printf ("  not loaded  : %s\n", sfx->name);
		}
	}
	Com_Printf ("Total resident: %i\n", total);
}
