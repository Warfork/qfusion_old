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

struct sfx_s;

void S_Init (void);
void S_Shutdown (void);
void S_Restart (qboolean noVideo);

void S_FreeSounds (void);
void S_SoundsInMemory (void);

// if origin is NULL, the sound will be dynamically sourced from the entity
void S_StartSound (vec3_t origin, int entnum, int entchannel, struct sfx_s *sfx, float fvol,  float attenuation, float timeofs);
void S_StartLocalSound (char *s);

void S_AddLoopSound (struct sfx_s *sfx, int entnum, vec3_t origin, vec3_t velocity);

void S_RawSamples (int samples, int rate, int width, int channels, qbyte *data);

void S_StartBackgroundTrack( char *intro, char *loop );
void S_StopBackgroundTrack( void );

void S_StopAllSounds(void);
void S_Update (vec3_t origin, vec3_t velocity, vec3_t v_forward, vec3_t v_right, vec3_t v_up);

void S_Activate (qboolean active);

struct sfx_s *S_RegisterSound (char *sample);

struct sfx_s *S_FindName (char *name, qboolean create);

// the sound code makes callbacks to the client for entitiy position
// information, so entities can be dynamically re-spatialized
void CL_GetEntitySoundOrigin (int entnum, vec3_t org);
