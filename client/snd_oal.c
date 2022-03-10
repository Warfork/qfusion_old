/*
Copyright (C) 2002-2007 Victor Luchits

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
// snd_oal.c

#include "client.h"
#include "snd_loc.h"

#ifdef NOOPENAL

void	SNDOAL_Info_f( void ) {}
qboolean SNDOAL_Init( void ) { return qfalse; }
void	SNDOAL_Shutdown( void ) {}
void	SNDOAL_RegisterSound( sfx_t *sfx ) {}
void	SNDOAL_FreeSounds( void ) {}
int		SNDOAL_ChannelFirstToDie( int ch_idx, int first_to_die, int *life_left ) { return first_to_die; }
void	SNDOAL_Spatialize( int channum, int entnum, vec3_t origin, vec3_t velocity, float rolloff, float gain ) {}
void	SNDOAL_StartPlaysound( playsound_t *ps ) {}
void	SNDOAL_ClearBuffer( void ) {}
void	SNDOAL_AddLoopSounds( void ) {}
void	SNDOAL_RawSamples( int samples, int rate, int width, int channels, qbyte *data ) {}
qboolean SNDOAL_UpdateBackgroundTrack( void ) { return qfalse; }
void	SNDOAL_StopBackgroundTrack( void ) {}
void	SNDOAL_Update( vec3_t origin, vec3_t velocity, vec3_t forward, vec3_t right, vec3_t up ) {}

#else

#ifdef _WIN32
# include <al.h>
# include <alc.h>
#else
# include <AL/al.h>
# include <AL/alc.h>
#endif

#ifndef APIENTRY
# define APIENTRY
#endif

#define MIN_OPENAL_VER	1.1

ALCdevice	*(APIENTRY *qalcOpenDevice)( const ALCchar *devicename );
ALCcontext	*(APIENTRY *qalcCreateContext)( ALCdevice *device, const ALCint* attrlist );
ALCenum     (APIENTRY *qalcGetError)( ALCdevice *device );
const ALCchar *(APIENTRY *qalcGetString)( ALCdevice *device, ALCenum param );
ALCboolean  (APIENTRY *qalcMakeContextCurrent)( ALCcontext *context );
void        (APIENTRY *qalcDestroyContext)( ALCcontext *context );
ALCboolean  (APIENTRY *qalcCloseDevice)( ALCdevice *device );
ALCcontext	*(APIENTRY *qalcGetCurrentContext)( ALCvoid );
ALCdevice*  (APIENTRY *qalcGetContextsDevice)( ALCcontext *context );

ALenum		(APIENTRY *qalGetError)( void );
const ALchar *(APIENTRY *qalGetString)( ALenum param );

void		(APIENTRY *qalGenBuffers)( ALsizei n, ALuint* buffers );
void		(APIENTRY *qalBufferData)( ALuint bid, ALenum format, const ALvoid* data, ALsizei size, ALsizei freq );
void		(APIENTRY *qalDeleteBuffers)( ALsizei n, const ALuint* buffers );

void		(APIENTRY *qalGenSources)( ALsizei n, ALuint* sources );
void		(APIENTRY *qalSourcei)( ALuint sid, ALenum param, ALint value );
void		(APIENTRY *qalSourcef)( ALuint sid, ALenum param, ALfloat value );
void		(APIENTRY *qalSourcefv)( ALuint sid, ALenum param, const ALfloat* values );
void		(APIENTRY *qalGetSourcei)( ALuint sid,  ALenum param, ALint* value );
void		(APIENTRY *qalSourceStop)( ALuint sid );
void		(APIENTRY *qalSourcePlay)( ALuint sid );
void		(APIENTRY *qalSourceUnqueueBuffers)( ALuint sid, ALsizei numEntries, ALuint *bids );
void		(APIENTRY *qalSourceQueueBuffers)( ALuint sid, ALsizei numEntries, const ALuint *bids );
void		(APIENTRY *qalDeleteSources)( ALsizei n, const ALuint* sources );

void		(APIENTRY *qalDistanceModel)( ALenum distanceModel );
void		(APIENTRY *qalListenerfv)( ALenum param, const ALfloat* values );

static dllfunc_t openalfuncs[] =
{
	{ "alcOpenDevice",			( void ** )&qalcOpenDevice },
	{ "alcCreateContext",		( void ** )&qalcCreateContext },
	{ "alcGetError",			( void ** )&qalcGetError },
	{ "alcGetString",			( void ** )&qalcGetString },
	{ "alcMakeContextCurrent",	( void ** )&qalcMakeContextCurrent },
	{ "alcDestroyContext",		( void ** )&qalcDestroyContext },
	{ "alcCloseDevice",			( void ** )&qalcCloseDevice },
	{ "alcGetCurrentContext",	( void ** )&qalcGetCurrentContext },
	{ "alcGetContextsDevice",	( void ** )&qalcGetContextsDevice },

	{ "alGetError",				( void ** )&qalGetError },
	{ "alGetString",			( void ** )&qalGetString },

	{ "alGenBuffers",			( void ** )&qalGenBuffers },
	{ "alBufferData",			( void ** )&qalBufferData },
	{ "alDeleteBuffers",		( void ** )&qalDeleteBuffers },

	{ "alGenSources",			( void ** )&qalGenSources },
	{ "alSourcei",				( void ** )&qalSourcei },
	{ "alSourcef",				( void ** )&qalSourcef },
	{ "alSourcefv",				( void ** )&qalSourcefv },
	{ "alGetSourcei",			( void ** )&qalGetSourcei },
	{ "alSourceStop",			( void ** )&qalSourceStop },
	{ "alSourcePlay",			( void ** )&qalSourcePlay },
	{ "alSourceUnqueueBuffers",	( void ** )&qalSourceUnqueueBuffers },
	{ "alSourceQueueBuffers",	( void ** )&qalSourceQueueBuffers },
	{ "alDeleteSources",		( void ** )&qalDeleteSources },

	{ "alDistanceModel",		( void ** )&qalDistanceModel },
	{ "alListenerfv",			( void ** )&qalListenerfv },

	{ NULL, NULL }
};

void *openALLibrary;

static int		numALSources;
static ALuint	alSources[MAX_CHANNELS+1+MAX_LOOPSFX];
static ALuint	alBuffers[MAX_SFX];

/*
================
SNDOAL_Info_f
================
*/
void SNDOAL_Info_f( void )
{
	ALCdevice *Device;
	ALCcontext *Context;

	Context = qalcGetCurrentContext ();
	Device = qalcGetContextsDevice( Context );

	Com_Printf( "\n" );
	Com_Printf( "ALC_DEVICE_SPECIFIER: %s\n", qalcGetString( Device, ALC_DEVICE_SPECIFIER ) );
	Com_Printf( "ALC_DEFAULT_DEVICE_SPECIFIER: %s\n", qalcGetString( NULL, ALC_DEFAULT_DEVICE_SPECIFIER ) );
	Com_Printf( "AL_VENDOR: %s\n", qalGetString( AL_VENDOR ) );
	Com_Printf( "AL_VERSION: %s\n", qalGetString( AL_VERSION ) );
	Com_Printf( "AL_RENDERER: %s\n", qalGetString( AL_RENDERER ) );
	Com_Printf( "AL_EXTENSIONS: %s\n", qalGetString( AL_EXTENSIONS ) );
	Com_Printf( "\n" );
    Com_Printf( "%5d stereo\n", dma.channels - 1 );
    Com_Printf( "%5d samplebits\n", dma.samplebits );
    Com_Printf( "%5d speed\n", dma.speed );
	Com_Printf( "%5d sources: \n", numALSources );
	Com_Printf( "%5d  regular\n", numChannels );
	Com_Printf( "%5d  streaming\n", numALSources - numChannels - numAutoLoops );
	Com_Printf( "%5d  autoloops\n", numAutoLoops );
}

/*
================
SNDOAL_Init
================
*/
qboolean SNDOAL_Init( void )
{
	int i, err, defaultDeviceNum, numDevices;
	char *device, *devices[256], *defaultDevice;
	ALCdevice *Device = NULL;
	ALCcontext *Context = NULL;
	cvar_t *s_openAL_device = Cvar_Get( "s_openAL_device", "0", CVAR_ARCHIVE );

	// initialize OpenAL manually
	memset( alSources, 0, sizeof( alSources ) );
	memset( alBuffers, 0, sizeof( alBuffers ) );

	if( openALLibrary )
		SNDOAL_Shutdown ();

	openALLibrary = Sys_LoadLibrary( OPENAL_LIBNAME, openalfuncs );
	if( openALLibrary )
		Com_Printf( "Loaded %s\n", OPENAL_LIBNAME );
	else
		return qfalse;

	defaultDevice = (char *)qalcGetString( NULL, ALC_DEFAULT_DEVICE_SPECIFIER );
	defaultDeviceNum = 1;

	numDevices = 0;
	device = (char *)qalcGetString( NULL, ALC_DEVICE_SPECIFIER );
	if( device && *device ) {
		for( ; *device; device += strlen( device ) + 1 ) {
			if( numDevices == sizeof( devices ) / sizeof( *devices ) )
				break;

			devices[numDevices] = device;
			if( defaultDevice && !strcmp( defaultDevice, device ) )
				defaultDeviceNum = numDevices + 1;
			numDevices++;
		}
	}

	// open device
	if( !numDevices )
		Device = qalcOpenDevice( NULL );
	else if( s_openAL_device->integer == 0 )
		Device = qalcOpenDevice( (ALubyte *)devices[defaultDeviceNum-1] );
	else
		Device = qalcOpenDevice( (ALubyte *)devices[bound( 1, s_openAL_device->integer, numDevices ) - 1] );
	if( Device == NULL ) {
		Com_Printf( "SNDOAL_Init: Failed to open device\n" );
		goto error;
	}

	// create context(s)
	Context = qalcCreateContext( Device, NULL );
	if( Context == NULL ) {
		Com_Printf( "SNDOAL_Init: Failed to create context\n" );
		goto error;
	}

	// set active context
	qalcGetError( Device );
	qalcMakeContextCurrent( Context );
	if( qalcGetError( Device ) != ALC_NO_ERROR ) {
		Com_Printf( "SNDOAL_Init: Failed to make context current\n" );
		goto error;
	}

	if( atof( qalGetString( AL_VERSION ) ) < MIN_OPENAL_VER ) {
		Com_Printf( "SNDOAL_Init: Version %1.1f of OpenAL required\n", MIN_OPENAL_VER );
		goto error;
	}

	// clear error codes
	qalGetError();
	qalcGetError( Device );

	qalDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );

	// create sources... 
	for( i = 0; i < sizeof( alSources ) / sizeof( alSources[0] ); i++ ) {
		qalGenSources( 1, &alSources[i] );

		err = qalGetError ();
		if( err != AL_NO_ERROR )
			break;

		qalSourcei( alSources[i], AL_BUFFER, 0 );
		qalSourcef( alSources[i], AL_PITCH, 1.0f );
		qalSourcef( alSources[i], AL_REFERENCE_DISTANCE, SOUND_FULLVOLUME );
		qalSourcef( alSources[i], AL_GAIN, s_volume->value );
	}

	if( i == 0 ) {
		Com_Printf( "SNDOAL_Init: %s\n", qalGetString( err ) );
		goto error;
	}

	numALSources = i;
	for( numChannels = MAX_CHANNELS; numChannels >= i; numChannels /= 2 );

	// reserve one channel for streaming
	if( numChannels < MAX_CHANNELS )
		numChannels--;
	qalSourcei( alSources[numChannels], AL_SOURCE_RELATIVE, AL_TRUE );
	qalSourcefv( alSources[numChannels], AL_POSITION, vec3_origin );
	qalSourcefv( alSources[numChannels], AL_VELOCITY, vec3_origin );
	qalSourcef( alSources[numChannels], AL_ROLLOFF_FACTOR, 0 );
	qalSourcef( alSources[numChannels], AL_GAIN, s_musicvolume->value );

	if( numChannels <= 0 ) {
		Com_Printf( "SNDOAL_Init: could not allocate channels\n" );
		goto error;
	}

	// keep the rest for autolooping sounds
	numAutoLoops = numALSources - (numChannels + 1);
	if( numAutoLoops < 0 )
		numAutoLoops = 0;

	Com_Printf( "OpenAL devices: %i\n", numDevices );
	for( i = 0; i < numDevices; i++ )
		Com_Printf( "%3d. %s\n", i+1, devices[i] );

	// clear error code
	qalGetError ();

	dma.channels = 2;
	dma.samplebits = 16;
	dma.speed = KHZ2RATE (s_khz->integer);

	return qtrue;

error:
	// disable context
	qalcMakeContextCurrent( NULL );
	
	// release context
	if( Context )
		qalcDestroyContext( Context );

	// close device
	if( Device )
		qalcCloseDevice( Device );

	if( openALLibrary )
		Sys_UnloadLibrary( &openALLibrary );

	return qfalse;
}

/*
================
SNDOAL_Shutdown
================
*/
void SNDOAL_Shutdown( void )
{
	int		i;
	ALCdevice *Device;
	ALCcontext *Context;

	for( i = 0; i < numALSources; i++ ) {
		if( alSources[i] )
			qalDeleteSources( 1, &alSources[i] );
	}

	// get active context
	Context = qalcGetCurrentContext ();

	// get device for active context
	Device = qalcGetContextsDevice( Context );

	// disable context
	qalcMakeContextCurrent( NULL );

	// release context(s)
	qalcDestroyContext( Context );

	// close device
	qalcCloseDevice( Device );

	if( openALLibrary )
		Sys_UnloadLibrary( &openALLibrary );
}

/*
================
SNDOAL_RegisterSound
================
*/
void SNDOAL_RegisterSound( sfx_t *sfx )
{
	sfxcache_t *sc = sfx->cache;
	int bufNum = sfx - known_sfx;

	if( !sc || alBuffers[bufNum] != 0 || !sc->data )
		return;

	qalGenBuffers( 1, &alBuffers[bufNum] );
	if( alBuffers[bufNum] ) {
		if ( sc->width == 2 ) {
			if( sc->channels == 2 )
				qalBufferData( alBuffers[bufNum], AL_FORMAT_STEREO16, &sc->data[0], sc->length*2*2, sc->speed );
			else
				qalBufferData( alBuffers[bufNum], AL_FORMAT_MONO16, &sc->data[0], sc->length*2, sc->speed );
		} else {
			if( sc->channels == 2 )
				qalBufferData( alBuffers[bufNum], AL_FORMAT_STEREO8, &sc->data[0], sc->length*2, sc->speed );
			else
				qalBufferData( alBuffers[bufNum], AL_FORMAT_MONO8, &sc->data[0], sc->length, sc->speed );
		}
	}
}

/*
================
SNDOAL_FreeSounds
================
*/
void SNDOAL_FreeSounds( void )
{
	int i;

	for( i = 0; i < num_sfx; i++ ) { 
		if( alBuffers[i] ) { 
			qalDeleteBuffers( 1, &alBuffers[i] );
			alBuffers[i] = 0;
		}
	}
}

/*
=================
SNDOAL_ChannelFirstToDie
=================
*/
int SNDOAL_ChannelFirstToDie( int ch_idx, int first_to_die, int *life_left )
{
	ALint state;

	qalGetSourcei( alSources[ch_idx], AL_SOURCE_STATE, &state );
	if( state == AL_PLAYING )
		return first_to_die;

	*life_left = 0;
	return ch_idx;
}

/*
=================
SNDOAL_Spatialize
=================
*/
void SNDOAL_Spatialize( int channum, int entnum, vec3_t origin, vec3_t velocity, float rolloff, float gain )
{
	ALuint source = alSources[channum];

	// anything coming from the view entity will always be full volume
	if( entnum == cl.playernum+1 ) {
		qalSourcei( source, AL_SOURCE_RELATIVE, AL_TRUE );
		qalSourcefv( source, AL_POSITION, vec3_origin );
		qalSourcefv( source, AL_VELOCITY, vec3_origin );
		qalSourcef( source, AL_ROLLOFF_FACTOR, 0 );
		qalSourcef( source, AL_GAIN, gain );
		return;
	}

	if( cls.state != ca_active ) {
		qalSourcei( source, AL_SOURCE_RELATIVE, AL_TRUE );
		qalSourcefv( source, AL_POSITION, vec3_origin );
		qalSourcefv( source, AL_VELOCITY, vec3_origin );
		qalSourcef( source, AL_ROLLOFF_FACTOR, 0 );
		qalSourcef( source, AL_GAIN, gain );
	} else {
		qalSourcei( source, AL_SOURCE_RELATIVE, AL_FALSE );
		qalSourcefv( source, AL_POSITION, origin ); 
		qalSourcefv( source, AL_VELOCITY, velocity ); 
		qalSourcef( source, AL_ROLLOFF_FACTOR, rolloff );
		qalSourcef( source, AL_GAIN, gain );
	}
}

/*
=================
SNDOAL_StartPlaysound
=================
*/
void SNDOAL_StartPlaysound( playsound_t *ps )
{
	int channum;
	channel_t *ch;
	ALint state, source;

	ch = S_PickPlaysoundChannel( ps );
	if( !ch )
		return;
	if( !alBuffers[ch->sfx - known_sfx] )
		return;

	ch->dist_mult *= SOUND_FULLVOLUME * 20;
	channum = ch - channels;
	source = alSources[channum];

	qalGetSourcei( source, AL_SOURCE_STATE, &state );
	if( state == AL_PLAYING )	// overriding
		qalSourceStop( source );

	qalSourcei( source, AL_BUFFER, alBuffers[ch->sfx - known_sfx] );
	qalSourcei( source, AL_LOOPING, ch->sfx->cache->loopstart != -1 ? AL_TRUE : AL_FALSE );

	// spatialize
	S_Spatialize( ch );

	qalSourcePlay( source );
}

/*
=================
SNDOAL_ClearBuffer
=================
*/
void SNDOAL_ClearBuffer( void )
{
	int i;
	ALint state;

	for( i = 0; i < numALSources; i++ ) {
		if( i == numChannels ) {
			SNDOAL_StopBackgroundTrack ();
			continue;
		}

		qalGetSourcei( alSources[i], AL_SOURCE_STATE, &state );
		if( state == AL_PLAYING )
			qalSourceStop( alSources[i] );
		qalSourcei( alSources[i], AL_BUFFER, 0 );
	}
}

/*
==================
SNDOAL_AddLoopSounds
==================
*/
void SNDOAL_AddLoopSounds( void )
{
	int			i;
	sfx_t		*sfx;
	sfxcache_t	*sc;
	int			channum;
	ALint		state, source;

	if( cl_paused->integer || cls.state != ca_active || !cl.soundPrepped ) {
		memset( loop_sfx, 0, sizeof( loop_sfx ) );

		for( i = 0, channum = numChannels + 1; i < numAutoLoops; i++, channum++ ) {
			qalGetSourcei( alSources[channum], AL_SOURCE_STATE, &state );
			if( state == AL_PLAYING )
				qalSourceStop( alSources[channum] );
		}
		return;
	}

	for( i = 0, channum = numChannels + 1; i < numAutoLoops; i++, channum++ ) {
		if( !loop_sfx[i].sfx || !loop_sfx[i].mode )
			continue;

		sfx = loop_sfx[i].sfx;
		sc = sfx->cache;
		if( !sc || !sc->length )
			continue;

		source = alSources[channum];

		switch( loop_sfx[i].mode ) {
			case 1:
				qalGetSourcei( source, AL_SOURCE_STATE, &state );
				if( state == AL_PLAYING )
					qalSourceStop( source );
				break;
			case 3:
				qalGetSourcei( source, AL_SOURCE_STATE, &state );
				if( state == AL_PLAYING )
					qalSourceStop( source );

				qalSourcei( source, AL_BUFFER, alBuffers[sfx - known_sfx] );
				qalSourcei( source, AL_LOOPING, AL_TRUE );
				qalSourcei( source, AL_SAMPLE_OFFSET, rand() % sc->length );
				qalSourcePlay( source );
			default:
				SNDOAL_Spatialize( channum, loop_sfx[i].entnum-1, loop_sfx[i].origin, vec3_origin, SOUND_FULLVOLUME * SOUND_LOOPATTENUATE * 40, s_volume->value );
				break;
		}

		loop_sfx[i].mode--;
	}
}

/*
============
SNDOAL_RawSamples
============
*/
void SNDOAL_RawSamples( int samples, int rate, int width, int channels, qbyte *data )
{
	ALint state;
	ALuint buffer;
	ALuint source = alSources[numChannels];
	int queued;

	qalGenBuffers( 1, &buffer );
	if( !buffer )
		return;

	if( width == 2 ) {
		if( channels == 2 )
			qalBufferData( buffer, AL_FORMAT_STEREO16, data, samples*2*2, rate );
		else
			qalBufferData( buffer, AL_FORMAT_MONO16, data, samples*2, rate );
	} else {
		if( channels == 2 )
			qalBufferData( buffer, AL_FORMAT_STEREO8, data, samples*2, rate );
		else
			qalBufferData( buffer, AL_FORMAT_MONO8, data, samples, rate );
	}

	// kick the playback if needed
	qalGetSourcei( source, AL_SOURCE_STATE, &state );
	if( state == AL_INITIAL || state == AL_STOPPED ) {
		qalGetSourcei( source, AL_BUFFERS_QUEUED, &queued );

		// for continuous playback we need to wait until there are two buffers in queue
		if( queued )
			qalSourcePlay( source );
	}

	qalSourceQueueBuffers( source, 1, &buffer );
}

/*
============
SNDOAL_UpdateBackgroundTrack
============
*/
qboolean SNDOAL_UpdateBackgroundTrack( void )
{
	ALuint buffer;
	ALuint source = alSources[numChannels];
	int processed, queued;

	qalGetSourcei( source, AL_BUFFERS_PROCESSED, &processed );
	while( processed-- ) {	// unqueue processed buffers
		qalSourceUnqueueBuffers( source, 1, &buffer );
		qalDeleteBuffers( 1, &buffer );
	}

	// update only if we need to queue more buffers for continuous playback
	qalGetSourcei( source, AL_BUFFERS_QUEUED, &queued );
	if( queued < 2 )
		return qtrue;

	return qfalse;
}

/*
=================
SNDOAL_StopBackgroundTrack
=================
*/
void SNDOAL_StopBackgroundTrack( void )
{
	ALuint buffer, source = alSources[numChannels];
	int queued;
	ALint state;

	qalGetSourcei( source, AL_BUFFERS_QUEUED, &queued );
	while( queued-- ) {
		qalSourceUnqueueBuffers( source, 1, &buffer );
		qalDeleteBuffers( 1, &buffer );
	}

	qalGetSourcei( source, AL_SOURCE_STATE, &state );
	if( state == AL_PLAYING )
		qalSourceStop( source );
}

/*
============
SNDOAL_Update

Called once each time through the main loop
============
*/
void SNDOAL_Update( vec3_t origin, vec3_t velocity, vec3_t forward, vec3_t right, vec3_t up )
{
	int			i;
	ALfloat		lo[3], lv[6];

	if( s_volume->modified ) {
		s_volume->modified = qfalse;

		for( i = 0; i < numALSources; i++ ) {
			if( i != numChannels )
				qalSourcef( alSources[i], AL_GAIN, s_volume->value );
		}
	}

	if( s_musicvolume->modified ) {
		s_musicvolume->modified = qfalse;

		qalSourcef( alSources[numChannels], AL_GAIN, s_musicvolume->value );
	}

	VectorCopy( origin, lo );
	VectorCopy( forward, &lv[0] );
	VectorCopy( up, &lv[3] );

	// update listener
	qalListenerfv( AL_POSITION, lo );
	qalListenerfv( AL_ORIENTATION, lv );
	qalListenerfv( AL_VELOCITY, vec3_origin );

	// clear error code
	qalGetError ();
}

#endif
