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

// ui_pmodels.c - Add player models in split parts
// by Jalisko

#include "ui_local.h"

#ifndef SKELMOD

pmodelitem_t	uiPlayerModelItems[MAX_UI_PMODELS];

static const char *pmPartNames[] = { "lower", "upper", "head", NULL };
static const char *pmTagNames[] = { "tag_torso", "tag_head", NULL };
static const char *wmPartSufix[] = { "", "_barrel", "_flash", NULL };

/*
================
ui_CopyAnimation
================
*/
void ui_CopyAnimation( animationinfo_t *anim, int put, int pick )
{
	anim->firstframe[put] = anim->firstframe[pick];
	anim->lastframe[put] = anim->lastframe[pick];
	anim->loopingframes[put] = anim->loopingframes[pick];
	anim->frametime[put] = anim->frametime[pick];
}

/*
================
ui_ParseAnimationScript
================
*/
qboolean ui_ParseAnimationScript( animationinfo_t *anim, char *filename, int *gender )
{
	qbyte	*buf;
	char	*ptr, *token;
	int		rounder, counter, i, offset, nooffset, alljumps;
	int		anim_data[4][PMODEL_MAX_ANIMS];
	int		length, filenum;

	strcpy( anim->filename, "" );
	rounder = nooffset = alljumps = 0;
	counter = 1;

	// load the file
	length = trap_FS_FOpenFile( filename, &filenum, FS_READ );
	if( length == -1 ) {
		Com_Printf ("Couldn't load animation script: %s\n", filename);
		return qfalse;
	}
	if( !length ) {
		trap_FS_FCloseFile( filenum );
		return qfalse;
	}

	buf = UI_Malloc( length + 1 );
	trap_FS_Read( buf, length, filenum );
	trap_FS_FCloseFile( filenum );

	
	if( !buf ) {
		UI_Free( buf );
		Com_Printf( "Couldn't load animation script: %s\n", filename );
		return qfalse;
	}
	
	ptr = ( char * )buf;
	while( ptr ) {
		token = COM_ParseExt( &ptr, qtrue );
		if( !token )
			break;
		
		if( *token < '0' || *token > '9' ) {
			if( !Q_stricmp (token, "headoffset") ) {
				token = COM_ParseExt( &ptr, qfalse );
				if ( !token )
					break;
				token = COM_ParseExt ( &ptr, qfalse );
				if ( !token )
					break;
				token = COM_ParseExt ( &ptr, qfalse );
				if ( !token )
					break;
			} else if( !Q_stricmp( token, "sex" ) ) {
				token = COM_ParseExt( &ptr, qfalse );
				if( !token )
					break;
				
				if( token[0] == 'm' || token[0] == 'M' ) {
					*gender = GENDER_MALE;
					
				} else if( token[0] == 'f' || token[0] == 'F' ) {
					*gender = GENDER_FEMALE;

				} else
					*gender = GENDER_NEUTRAL;
			} else if( !Q_stricmp (token, "nooffset") ) {
				nooffset = 1;
			} else if( !Q_stricmp (token, "alljumps") )
				alljumps = 1;
		} else {
			i = (int)atoi( token );
			anim_data[rounder][counter] = i;
			rounder++;
			if( rounder > 3 ) {
				rounder = 0;
				if( ++counter == PMODEL_MAX_ANIMATIONS )
					break;
			}
		}
	}

	UI_Free( buf );

	if( counter-1 < LEGS_TURN ) {
		Com_Printf( "ERROR: Not enough animations(%i) at script: %s\n", counter, filename );
		return qfalse;
	}

	anim_data[0][ANIM_NONE] = 0;
	anim_data[1][ANIM_NONE]	= 0;
	anim_data[2][ANIM_NONE]	= 1;
	anim_data[3][ANIM_NONE] = 10;

	for( i = 0 ; i < counter; i++ ) {
		anim->firstframe[i] = anim_data[0][i];
		anim->lastframe[i] = ((anim_data[0][i]) + (anim_data[1][i]));
		anim->loopingframes[i] = anim_data[2][i];
		if( i == TORSO_FLIPOUT || i == TORSO_FLIPIN || anim_data[3][i] < 10 )
			anim_data[3][i] = 10;
		anim->frametime[i] = 1000/anim_data[3][i];
	}

	if( !nooffset ) {
		offset = anim->firstframe[LEGS_CRWALK] - anim->firstframe[TORSO_GESTURE];
		for( i = LEGS_CRWALK; i < counter; ++i ) {
			anim->firstframe[i] -= offset;
			anim->lastframe[i] -= offset;
		}
	}

	for( i = 0; i < counter; ++i ) {
		if( anim->loopingframes[i] )
			anim->loopingframes[i] -= 1;
		if( anim->lastframe[i] )
			anim->lastframe[i] -= 1;
	}

	if( !alljumps ) {
		ui_CopyAnimation( anim, LEGS_JUMP3, LEGS_JUMP1 );
		ui_CopyAnimation( anim, LEGS_JUMP3ST, LEGS_JUMP1ST );
		ui_CopyAnimation( anim, LEGS_JUMP2, LEGS_JUMP1 );
		ui_CopyAnimation( anim, LEGS_JUMP2ST, LEGS_JUMP1ST );
	}

	counter--;
	
	if( counter < TORSO_RUN )
		ui_CopyAnimation( anim, TORSO_RUN, TORSO_STAND );
	if( counter < TORSO_DROPHOLD )
		ui_CopyAnimation( anim, TORSO_DROPHOLD, TORSO_STAND );
	if( counter < TORSO_DROP )
		ui_CopyAnimation( anim, TORSO_DROP, TORSO_ATTACK2 );
	if( counter < TORSO_PAIN1 )
		ui_CopyAnimation( anim, TORSO_PAIN1, TORSO_STAND2 );
	if( counter < TORSO_PAIN2 )
		ui_CopyAnimation( anim, TORSO_PAIN2, TORSO_STAND2 );
	if( counter < TORSO_PAIN3 )
		ui_CopyAnimation( anim, TORSO_PAIN3, TORSO_STAND2 );
	if( counter < TORSO_SWIM )
		ui_CopyAnimation( anim, TORSO_SWIM, TORSO_STAND );

	if( counter < LEGS_WALKBACK )
		ui_CopyAnimation( anim, LEGS_WALKBACK, LEGS_RUNBACK );
	if( counter < LEGS_WALKLEFT )
		ui_CopyAnimation( anim, LEGS_WALKLEFT, LEGS_WALKFWD );
	if( counter < LEGS_WALKRIGHT )
		ui_CopyAnimation( anim, LEGS_WALKRIGHT, LEGS_WALKFWD );
	if( counter < LEGS_RUNLEFT )
		ui_CopyAnimation( anim, LEGS_RUNLEFT, LEGS_RUNFWD );
	if( counter < LEGS_RUNRIGHT )
		ui_CopyAnimation( anim, LEGS_RUNRIGHT, LEGS_RUNFWD );
	if( counter < LEGS_SWIM )
		ui_CopyAnimation( anim, LEGS_SWIM, LEGS_SWIMFWD );

	strcpy( anim->filename, filename );
	return qtrue;
}

/*
================
ui_PModelLoadAnimations

store playermodel animations inside clientinfo
================
*/
qboolean ui_PModelLoadAnimations( ui_pmodel_t *pmodel )
{
	char	filename[MAX_QPATH];
	animationinfo_t	*anim;

	// no need to update?
	Q_snprintfz( filename, sizeof( filename ), "models/players/%s/animation.cfg", pmodel->model_name );
	if( !Q_stricmp( pmodel->anim.filename, filename ) )
		return qtrue;

	anim = &pmodel->anim;
	if( !ui_ParseAnimationScript( anim, filename, &pmodel->sex) )
		return qfalse;

	// update the frames for the new animation script
	if( !anim->current[UPPER] || !anim->current[LOWER] || anim->current[HEAD] != ANIM_NONE ) {
		anim->current[UPPER] = TORSO_STAND;
		anim->current[LOWER] = LEGS_STAND;
		anim->current[HEAD] = ANIM_NONE;
	}
	
	anim->oldframe[UPPER] = anim->frame[UPPER] = anim->lastframe[anim->current[UPPER]];
	anim->oldframe[LOWER] = anim->frame[LOWER] = anim->lastframe[anim->current[LOWER]];
	anim->oldframe[HEAD] = anim->frame[HEAD] = anim->lastframe[anim->current[HEAD]];
	return qtrue;
}

/*
================
ui_PModelLoadModelPart
================
*/
qboolean ui_PModelLoadSkinPart( ui_pmodel_t *pmodel, char *model_name, char *skin_name, int part )
{
	char		scratch[MAX_QPATH];
	
	Q_snprintfz( scratch, sizeof(scratch), "models/players/%s/%s_%s.skin", model_name, pmPartNames[part], skin_name );
	pmodel->customSkin[part] = trap_R_RegisterSkinFile( scratch );

	if( !pmodel->customSkin[part] )
		return qfalse;
	return qtrue;
}

/*
================
ui_PModelLoadSkinPart
================
*/
qboolean ui_PModelLoadModelPart( ui_pmodel_t *pmodel, char *model_name, int part )
{
	char	scratch[MAX_QPATH];

	Q_snprintfz( scratch, sizeof(scratch), "models/players/%s/%s.md3", model_name, pmPartNames[part] );
	pmodel->model[part] = trap_R_RegisterModel (scratch);

	if(!pmodel->model[part]) {
		Q_snprintfz( scratch, sizeof(scratch), "models/players/%s/%s.skm", model_name, pmPartNames[part] );
		pmodel->model[part] = trap_R_RegisterModel (scratch);
	}

	if( !pmodel->model[part] )
		return qfalse;
	return qtrue;
}

/*
================
ui_LoadPModel
================
*/
qboolean ui_LoadPModel( ui_pmodel_t *pmodel, char *model_name, char *skin_name )
{
	int 		i;
	qboolean 	loaded_model, loaded_skin;
	
	// if the model has changed we must update both model & skin
	if( Q_stricmp(pmodel->model_name, model_name) ) {
		for( i = 0; i < PMODEL_PARTS; i++ ) {
			loaded_model = ui_PModelLoadModelPart( pmodel, model_name, i );
			if( !loaded_model )
				break;
		}

		for( i = 0; i < PMODEL_PARTS; i++ ) {
			loaded_skin = ui_PModelLoadSkinPart( pmodel, model_name, skin_name, i );
			if( !loaded_skin )
				break;
		}

		if( loaded_model ) {
			strcpy( pmodel->model_name, model_name );
			loaded_model = ui_PModelLoadAnimations( pmodel );
		}

		// clear up the wrong stuff only
		if( !loaded_model ) {
			strcpy( pmodel->model_name, "" );
			for( i = 0; i < PMODEL_PARTS; i++ )
				pmodel->model[i] = NULL;
		}

		if( !loaded_skin ) {
			strcpy( pmodel->skin_name, "" );
			for( i = 0; i < PMODEL_PARTS; i++ )
				pmodel->customSkin[i] = NULL;
		} else
			strcpy( pmodel->skin_name, skin_name );

		// return true only if both were loaded
		if( !loaded_model || !loaded_skin )
			return qfalse;
		return qtrue;
	}

	// model was the same, but skin is new
	if( Q_stricmp(pmodel->skin_name, skin_name) ) {
		for( i = 0; i < PMODEL_PARTS; i++ ) {
			loaded_skin = ui_PModelLoadSkinPart( pmodel, model_name, skin_name, i );
			if( !loaded_skin ) 
				break;
		}

		if( !loaded_skin ) {
			strcpy( pmodel->skin_name, "" );
			for( i = 0; i < PMODEL_PARTS; i++ )
				pmodel->customSkin[i] = NULL;
		} else
			strcpy( pmodel->skin_name, skin_name );

		return loaded_skin;
	}

	// it was the same model & skin
	return qtrue;
}

/*
==============================
ui_PModel_ValidModel
==============================
*/
qboolean ui_PModel_ValidModel( char *model_name )
{
	qboolean found = qfalse;
	char	scratch[MAX_QPATH];
	int		i;
	
	for( i = 0; i < PMODEL_PARTS; i++ ) {
		Q_snprintfz( scratch, sizeof( scratch ), "models/players/%s/%s.md3", model_name, pmPartNames[i] );
		found = ( trap_FS_FOpenFile (scratch, NULL, FS_READ) != -1 );
		if( !found )
			break;
	}
	return found;
}

/*
===============
ui_PModelItemFindByName
===============
*/
struct pmodelitem_s *ui_PModelItemFindByName( char *name )
{
	int	i;
	int	found = -1;

	for( i = 0; i<MAX_UI_PMODELS; i++ ) {
		if( uiPlayerModelItems[i].inuse ) {
			if( !Q_stricmp( name, uiPlayerModelItems[i].name ) )
				return &uiPlayerModelItems[i];
		} else if( found == -1 )
			found = i;
	}

	if( found == -1 ) {
		Com_Printf( "ERROR: Free spot NOT found! (ui_FindPlayerModelByName)\n" );
		return NULL;
	}

	uiPlayerModelItems[found].inuse = qfalse;
	return &uiPlayerModelItems[found];
}

/*
===============
ui_PModelItem_UpdateRegistration
===============
*/
struct pmodelitem_s *ui_PModelItem_UpdateRegistration( char *name, char *model_name, char *skin_name )
{
	pmodelitem_t *pmodelitem;

	pmodelitem = ui_PModelItemFindByName( name );
	pmodelitem->inuse = ui_LoadPModel( &pmodelitem->pmodel, model_name, skin_name );
	if( pmodelitem->inuse )
		strcpy( pmodelitem->name, name );
	else
		strcpy( pmodelitem->name, "" );

	return pmodelitem;
}

/*
===============
ui_RegisterPModelItem
===============
*/
void ui_RegisterPModelItem( char *name, char *model_name, char *skin_name, animgroup_t *animgroup, int numanims)
{
	pmodelitem_t *pmodelitem;

	pmodelitem = ui_PModelItem_UpdateRegistration( name, model_name, skin_name );
	pmodelitem->numanims = numanims;
	pmodelitem->animgroup = animgroup;
}

/*
======================================================================
					PLAYERMODEL ANIMATION
======================================================================
*/

/*
===============
ui_PModelAnimToFrame
===============
*/
void ui_PModelAnimToFrame( animationinfo_t *anim )
{
	int i;
	int time;

	time = uis.time;
	for( i = 0; i < PMODEL_PARTS; i++ ) {
		if( time < anim->nextframetime[i] ) {
			anim->backlerp[i] = 1.0f - ( (time - anim->prevframetime[i])/(anim->nextframetime[i] - anim->prevframetime[i]) );
			if( anim->backlerp[i] > 1 )
				anim->backlerp[i] = 1;
			else if( anim->backlerp[i] < 0 )
				anim->backlerp[i] = 0;
			continue;
		}

		anim->prevframetime[i] = time;
		anim->nextframetime[i] = time + anim->frametime[anim->current[i]];
		anim->backlerp[i] = 1.0f;
		anim->oldframe[i] = anim->frame[i];
		anim->frame[i]++;

		if( anim->frame[i] > anim->lastframe[anim->current[i]] ) {
			if( anim->currentChannel[i] )
				anim->currentChannel[i] = BASIC_CHANNEL;
			anim->frame[i] = (anim->lastframe[anim->current[i]] - anim->loopingframes[anim->current[i]]);
		}

		if( anim->currentChannel[i] == EVENT_CHANNEL 
			&& (anim->buffer[MIXER_CHANNEL].channel[i] != EVENT_CHANNEL) )
			continue;

		if( anim->buffer[MIXER_CHANNEL].newanim[i] ) {
			if( ( anim->buffer[MIXER_CHANNEL].channel[i] == EVENT_CHANNEL )
				&& ( !anim->buffer[BASIC_CHANNEL].newanim[i] )
				&& ( anim->currentChannel[i] == BASIC_CHANNEL ) ) {
				anim->buffer[BASIC_CHANNEL].newanim[i] = anim->current[i];
				anim->buffer[BASIC_CHANNEL].channel[i] = BASIC_CHANNEL;
			}

			anim->current[i] = anim->buffer[MIXER_CHANNEL].newanim[i];
			anim->frame[i] = anim->firstframe[anim->current[i]];
			anim->currentChannel[i] = anim->buffer[MIXER_CHANNEL].channel[i];
			anim->buffer[MIXER_CHANNEL].newanim[i] = 0;
			anim->buffer[MIXER_CHANNEL].channel[i] = 0;
		}
	}
}

/*
==================
ui_PModelBufferMixer
==================
*/
void ui_PModelBufferMixer( animationinfo_t	*anim )
{
	int	i;

	for( i = 0 ; i < PMODEL_PARTS ; i++ ) {
		if( anim->buffer[EVENT_CHANNEL].newanim[i] ) {
			if( ( anim->buffer[MIXER_CHANNEL].newanim[i] ) 
				&& ( anim->buffer[MIXER_CHANNEL].channel[i] == BASIC_CHANNEL ) ) {
				if( !anim->buffer[BASIC_CHANNEL].newanim[i] ) {
					anim->buffer[BASIC_CHANNEL].newanim[i] = anim->buffer[MIXER_CHANNEL].newanim[i];
					anim->buffer[BASIC_CHANNEL].channel[i] = BASIC_CHANNEL;
					anim->buffer[MIXER_CHANNEL].newanim[i] = 0;
					anim->buffer[MIXER_CHANNEL].channel[i] = BASIC_CHANNEL;
				}
			}

			anim->buffer[MIXER_CHANNEL].newanim[i] = anim->buffer[EVENT_CHANNEL].newanim[i];
			anim->buffer[MIXER_CHANNEL].channel[i] = EVENT_CHANNEL;
			anim->buffer[EVENT_CHANNEL].newanim[i] = 0;
			anim->buffer[EVENT_CHANNEL].channel[i] = EVENT_CHANNEL;
		} else {
			if( anim->buffer[BASIC_CHANNEL].newanim[i] ) {
				anim->buffer[MIXER_CHANNEL].newanim[i] = anim->buffer[BASIC_CHANNEL].newanim[i];
				anim->buffer[MIXER_CHANNEL].channel[i] = BASIC_CHANNEL;
				anim->buffer[BASIC_CHANNEL].newanim[i] = 0;
				anim->buffer[BASIC_CHANNEL].channel[i] = BASIC_CHANNEL;
			}
		}
	}
}

/*
===============
ui_UpdateAnimationBuffer
===============
*/
void ui_UpdateAnimationBuffer( animationinfo_t *anim, int newanim[PMODEL_PARTS], int channel )
{
	int					i;
	animationbuffer_t *buffer;

	buffer = &anim->buffer[channel];
	for( i = 0 ; i < PMODEL_PARTS ; i++ ) {
		if( newanim[i] && ( newanim[i] < PMODEL_MAX_ANIMS) ) {
			buffer->newanim[i] = newanim[i];
			buffer->channel[i] = channel;
		}
	}

	ui_PModelBufferMixer ( anim );
}

/*
===============
ui_AddPModelAnimation
===============
*/
void ui_AddPModelAnimation( ui_pmodel_t *pmodel, int loweranim, int upperanim, int headanim, int channel)
{
	int newanim[PMODEL_PARTS];

	newanim[LOWER] = loweranim;
	newanim[UPPER] = upperanim;
	newanim[HEAD] = headanim;
	ui_UpdateAnimationBuffer( &pmodel->anim, newanim, channel );
}

/*
===============
ui_PModelItemAnimGroup
===============
*/
void ui_PModelItemAnimGroup( pmodelitem_t *pmodelitem )
{
	int i, time;
	int loweranim, upperanim, headanim, channel;
	animgroup_t	*animgroup;

	if( pmodelitem->animgroup == NULL ) {
		pmodelitem->animgroupInitTime = 0;
		pmodelitem->animgroupTime = 0;
		return;
	}

	animgroup = pmodelitem->animgroup;
	time = uis.time;

	if( !pmodelitem->animgroupInitTime ) {
		pmodelitem->animgroupInitTime = time;
		pmodelitem->animgroupTime = time;
	}

	if( time < (pmodelitem->animgroupTime ) )
		return;

	for( i = 0; i<pmodelitem->numanims; i++, animgroup++ ) {
		if( ( pmodelitem->animgroupInitTime + animgroup->delayTime ) < pmodelitem->animgroupTime )
			continue;

		// special token: -1, -1, -1, -1, <time value> relaunches the cycle
		if( animgroup->loweranim < 0 && animgroup->upperanim < 0 &&
			animgroup->headanim < 0 && animgroup->channel < 0 ) {
			pmodelitem->animgroupInitTime = 0;
			pmodelitem->animgroupTime = 0;
			break;
		}

		// filter for invalid values
		loweranim = animgroup->loweranim;
		upperanim = animgroup->upperanim;
		headanim = animgroup->headanim;
		channel = animgroup->channel;

		if( loweranim < 0 || loweranim > PMODEL_MAX_ANIMS-1 )
			loweranim = 0;
		if( upperanim < 0 || upperanim > PMODEL_MAX_ANIMS-1 )
			upperanim = 0;
		if( headanim < 0 || headanim > PMODEL_MAX_ANIMS-1 )
			headanim = 0;
		if( channel < 0 || channel > ANIMBUFFER_CHANNELS-1 )
			channel = 0;

		if( animgroup->playsound[0] != 0 )
			trap_S_StartLocalSound( animgroup->playsound );

		// set
		if( i+1 < pmodelitem->numanims ) {
			pmodelitem->animgroupTime = pmodelitem->animgroupInitTime + (pmodelitem->animgroup[i+1].delayTime);
			ui_AddPModelAnimation( &pmodelitem->pmodel, loweranim, upperanim, headanim, channel );
			break;
		}

		// last animation & exit
		ui_AddPModelAnimation( &pmodelitem->pmodel, loweranim, upperanim, headanim, channel );
		pmodelitem->animgroup = NULL;
		break;
	}
}

/*
===============
ui_PModelItemAnimation
===============
*/
void ui_PModelItemAnimation( pmodelitem_t *pmodelitem )
{
	ui_PModelAnimToFrame (&pmodelitem->pmodel.anim);
	ui_PModelItemAnimGroup (pmodelitem);
}

/*
======================================================================
					DRAW PLAYERMODEL
======================================================================
*/

/*
================
ui_PModel_Draw
================
*/
void ui_PModel_Draw( pmodelitem_t *pmodelitem, refdef_t *refdef )
{
	int i;
	int	entcounter;
	entity_t entities[PMODEL_PARTS];
	orientation_t	tag;
	ui_pmodel_t		*pmodel;

	ui_PModelItemAnimation( pmodelitem );

	pmodel = &pmodelitem->pmodel;
	memset( &entities, 0, sizeof( entities ) );
	entcounter = 0;

	// rotate
	if( pmodelitem->effectRotate == qtrue ) {
		pmodelitem->angles[1] += 1.0f;
		if ( pmodelitem->angles[1] > 360 )
			pmodelitem->angles[1] -= 360;
	}

	AnglesToAxis( pmodelitem->angles, entities[LOWER].axis );

	// legs
	entities[LOWER].rtype = RT_MODEL;
	entities[LOWER].flags = RF_FULLBRIGHT | RF_NOSHADOW | RF_FORCENOLOD;

	entities[LOWER].model = pmodel->model[LOWER];
	entities[LOWER].customSkin = pmodel->customSkin[LOWER];
	entities[LOWER].customShader = NULL;

	entities[LOWER].origin[0] = pmodelitem->origin[0];
	entities[LOWER].origin[1] = pmodelitem->origin[1];
	entities[LOWER].origin[2] = pmodelitem->origin[2];
	entities[LOWER].scale = 1.0f;
	VectorCopy( entities[LOWER].origin, entities[LOWER].origin2 );

	entities[LOWER].frame = pmodel->anim.frame[LOWER];
	entities[LOWER].oldframe = pmodel->anim.oldframe[LOWER];
	entities[LOWER].backlerp = pmodel->anim.backlerp[LOWER];

	if( entities[LOWER].model ) {
		entcounter = 1;

		// torso
		if( trap_R_LerpTag( &tag, entities[LOWER].model, entities[LOWER].frame, entities[LOWER].oldframe, entities[LOWER].backlerp, "tag_torso") ) {
			entities[UPPER] = entities[LOWER];
			entities[UPPER].model = pmodel->model[UPPER];
			entities[UPPER].customSkin = pmodel->customSkin[UPPER];
			entities[UPPER].customShader = NULL;
			entities[UPPER].frame = pmodel->anim.frame[UPPER];
			entities[UPPER].oldframe = pmodel->anim.oldframe[UPPER];
			entities[UPPER].backlerp = pmodel->anim.backlerp[UPPER];

			if( entities[UPPER].model ) {
				for( i = 0 ; i < 3 ; i++ )  
					VectorMA( entities[UPPER].origin, tag.origin[i], entities[LOWER].axis[i], entities[UPPER].origin ); 

				VectorCopy (entities[UPPER].origin, entities[UPPER].origin2);
				Matrix_Multiply( tag.axis, entities[LOWER].axis, entities[UPPER].axis );
				entcounter++;

				// head
				if( trap_R_LerpTag( &tag, entities[UPPER].model, entities[UPPER].frame, entities[UPPER].oldframe, entities[UPPER].backlerp, "tag_head" ) ) {
					entities[HEAD] = entities[UPPER];
					entities[HEAD].model = pmodel->model[HEAD];
					entities[HEAD].customSkin = pmodel->customSkin[HEAD];			
					entities[HEAD].customShader = NULL;
					entities[HEAD].frame = pmodel->anim.frame[HEAD];
					entities[HEAD].oldframe = pmodel->anim.oldframe[HEAD];
					entities[HEAD].backlerp = pmodel->anim.backlerp[HEAD];

					if( entities[HEAD].model ) {
						for( i = 0 ; i < 3 ; i++ )  
							VectorMA( entities[HEAD].origin, tag.origin[i], entities[UPPER].axis[i], entities[HEAD].origin ); 
						VectorCopy( entities[HEAD].origin, entities[HEAD].origin2 );
						Matrix_Multiply( tag.axis, entities[UPPER].axis, entities[HEAD].axis );
						entcounter++;
					}
				}
			}
		}
	}

	if( entcounter == PMODEL_PARTS ) {
		trap_R_ClearScene ();
		for( i = 0; i < PMODEL_PARTS; i++ )
			trap_R_AddEntityToScene( &entities[i] );
		trap_R_RenderScene( refdef );
	}
}

/*
================
ui_DrawPModel
================
*/
void ui_DrawPModel( char *name, int originz, int originx, int originy, int angle )
{
	refdef_t refdef;
	int w, h;
	pmodelitem_t *pmodelitem;
	pmwindow_t	*window;

	pmodelitem = ui_PModelItemFindByName( name );
	if( pmodelitem == NULL ) {
		pmodelitem->inuse = qfalse;
		return;
	}

	window = &pmodelitem->window;
	memset( &refdef, 0, sizeof( refdef ) );

	w = uis.vidWidth;
	h = uis.vidHeight;

	// model window size
	if( window->width )
		refdef.width = window->width;
	else
		refdef.width = w;

	if( window->height )
		refdef.height = window->height;
	else
		refdef.height = h;

	// window placement (relative to center of screen)
	refdef.x = (w / 2 - (refdef.width/2)) + window->x;
	refdef.y = (h / 2 - (refdef.height/2)) + window->y;

	if( window->fov ) {
		refdef.fov_x = window->fov;
		refdef.fov_y = window->fov;
	} else {
		refdef.fov_x = 30;
		refdef.fov_y = 30;
	}

	refdef.time = uis.time * 0.001f;
	refdef.areabits = 0;
	refdef.rdflags = RDF_NOWORLDMODEL;

	if( originx || originy || originz ) {
		pmodelitem->origin[0] = originz;
		pmodelitem->origin[1] = originx;
		pmodelitem->origin[2] = originy;
	}

	// setting the angle to 0 adds rotation effect
	if( angle ) {
		pmodelitem->angles[1] = angle;
		pmodelitem->effectRotate = qfalse;
	} else
		pmodelitem->effectRotate = qtrue;

	if( window->shader )
		trap_R_DrawStretchPic( refdef.x, refdef.y, refdef.width, refdef.height, 0, 0, 1, 1, colorWhite, window->shader );

	ui_PModel_Draw( pmodelitem, &refdef );
}

#endif // SKELMOD

