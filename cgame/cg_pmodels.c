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

// - Adding the Player models in split pieces
// by Jalisko

#include "cg_local.h"
#include "../game/gs_pmodels.h"


pmodel_t		cg_entPModels[MAX_EDICTS];
pmodelinfo_t	cg_PModelInfos[CG_MAX_PMODELS];
pskin_t			cg_pSkins[CG_MAX_PSKINS];

#ifndef SKELMOD
static const char *pmPartNames[] = { "lower", "upper", "head", NULL };
static const char *pmTagNames[] = { "tag_torso", "tag_head", NULL };
#endif

/*
================
CG_PModelsInit
================
*/
void CG_PModelsInit( void )
{
	memset( cg_entPModels, 0, sizeof(cg_entPModels) );
}

/*
================
CG_CopyAnimation
================
*/
void CG_CopyAnimation( pmodelinfo_t *pmodelinfo, int put, int pick )
{
	pmodelinfo->firstframe[put] = pmodelinfo->firstframe[pick];
	pmodelinfo->lastframe[put] = pmodelinfo->lastframe[pick];
	pmodelinfo->loopingframes[put] = pmodelinfo->loopingframes[pick];
#ifndef SKELMOD
	pmodelinfo->frametime[put] = pmodelinfo->frametime[pick];
#endif
}

/*
================
CG_FindBoneNum
================
*/
int CG_FindBoneNum( cgs_skeleton_t *skel, char *bonename )
{
	int	j;

	if( !skel )
		return -1;

	for( j = 0; j < skel->numBones; j++ ) {
		if( !Q_stricmp( skel->bones[j].name, bonename ) )
			return j;
	}

	return -1;
}
#ifdef SKELMOD
/*
================
CG_ParseRotationBone
================
*/
void CG_ParseRotationBone( pmodelinfo_t *pmodelinfo, char *token, int pmpart )
{
	int boneNumber;
	
	boneNumber = CG_FindBoneNum( CG_SkeletonForModel( pmodelinfo->model ), token );
	if( boneNumber < 0 ) {
		if( cg_debugPlayerModels->integer )
			CG_Printf("CG_ParseRotationBone: No such bone name %s\n", token);
		return;
	}
	//register it into pmodelinfo
	CG_Printf( "Script: CG_ParseRotationBone: %s is %i\n", token, boneNumber );
	pmodelinfo->rotator[pmpart][pmodelinfo->numRotators[pmpart]] = boneNumber;
	pmodelinfo->numRotators[pmpart]++;
}

/*
================
CG_ParseTagMask
================
*/
void CG_ParseTagMask( pmodelinfo_t *pmodelinfo, int bonenum, char *name )
{
	cg_tagmask_t *tagmask;
	cgs_skeleton_t	*skel;

	if( !name || !name[0] )
		return;

	skel = CG_SkeletonForModel( pmodelinfo->model );
	if( !skel || skel->numBones <= bonenum ) 
		return;

	//fixme: check the name isn't already in use, or it isn't the same as a bone name

	//now store it
	tagmask = CG_Malloc( sizeof(cg_tagmask_t) );
	Q_snprintfz( tagmask->tagname, sizeof(tagmask->tagname), name );
	Q_snprintfz( tagmask->bonename, sizeof(tagmask->bonename), skel->bones[bonenum].name );
	tagmask->bonenum = bonenum;
	tagmask->next = pmodelinfo->tagmasks;
	pmodelinfo->tagmasks = tagmask;
}
#endif // SKELMOD

/*
================
CG_ParseAnimationScript

Reads the animation config file.

0 = first frame
1 = number of frames/lastframe
2 = looping frames
3 = frame time

New keywords:
nooffset: Uses the first frame value at the script, and no offsets, for the animation.
alljumps: Uses 3 different jump animations (bunnyhoping)
islastframe: second value of each animation is lastframe instead of numframes
	
Q3 keywords:
sex m/f : sets gender
	  
Q3 Unsupported:
headoffset <value> <value> <value>
footsteps
================
*/
qboolean CG_ParseAnimationScript( pmodelinfo_t *pmodelinfo, char *filename )
{
	qbyte		*buf;
	char		*ptr, *token;
	int			rounder, counter, i, offset;
	int			filenum, length;
	qboolean	alljumps = qfalse;
	qboolean	debug = qtrue;
#ifdef SKELMOD
	qboolean	lower_anims_have_offset = qfalse;
	qboolean	islastframe = qtrue;
	int			anim_data[3][PMODEL_MAX_ANIMS];
	int			rootanims[PMODEL_PARTS];
#else
	qboolean	nooffset = qfalse;
	qboolean	islastframe = qfalse;
	int			anim_data[4][PMODEL_MAX_ANIMS];
#endif
	
	pmodelinfo->sex = GENDER_MALE;
	rounder = 0;
	counter = 1; // reseve 0 for 'no animation'
#ifdef SKELMOD
	pmodelinfo->tagmasks = NULL;
	pmodelinfo->frametime = 1000/24;
	memset( rootanims, -1, sizeof(rootanims) );
#endif
	if( !cg_debugPlayerModels->integer )
		debug = qfalse;

	// load the file
	length = trap_FS_FOpenFile( filename, &filenum, FS_READ );
	if( length == -1 ) {
		CG_Printf ("%sCouldn't load animation script: %s%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE);
		return qfalse;
	}

	if( !length ) {
		trap_FS_FCloseFile( filenum );
		return qfalse;
	}

	buf = CG_Malloc( length + 1 );
	trap_FS_Read( buf, length, filenum );
	trap_FS_FCloseFile( filenum );
	
	if( !buf ) {
		CG_Free( buf );
		CG_Printf( "%sCouldn't load animation script: %s%s\n",S_COLOR_YELLOW, filename, S_COLOR_WHITE );
		return qfalse;
	}
	
	if( debug )
		CG_Printf( "%sLoading animation script: %s%s\n",S_COLOR_BLUE, filename, S_COLOR_WHITE );
	
	// proceed
	ptr = ( char * )buf;
	while( ptr ) {
		token = COM_ParseExt( &ptr, qtrue );
		if( !token )
			break;
		
		if( *token < '0' || *token > '9' ) {
#ifndef SKELMOD
			if( !Q_stricmp( token, "headoffset" ) ) {			// headoffset (ignored)
				
				if( debug )
					CG_Printf( "%sScript: Ignored: %s%s", S_COLOR_YELLOW, token, S_COLOR_WHITE );
				
				token = COM_ParseExt( &ptr, qfalse );
				if( !token )
					break;
				
				if( debug )
					CG_Printf( "%s%s - %s", S_COLOR_YELLOW, token, S_COLOR_WHITE );
				
				token = COM_ParseExt( &ptr, qfalse );
				if( !token )
					break;
				
				if( debug )
					CG_Printf( "%s%s - %s", S_COLOR_YELLOW, token, S_COLOR_WHITE );
				
				token = COM_ParseExt( &ptr, qfalse );
				if( !token )
					break;
				
				if( debug )
					CG_Printf( "%s%s %s\n", S_COLOR_YELLOW, token, S_COLOR_WHITE );
			} else
#endif
				if( !Q_stricmp( token, "sex" ) ) {			// gender
					if( debug )
						CG_Printf( "%sScript: %s:%s", S_COLOR_BLUE, token, S_COLOR_WHITE );
					
					token = COM_ParseExt( &ptr, qfalse );
					if( !token )
						break;
					
					if( token[0] == 'm' || token[0] == 'M' ) {
						pmodelinfo->sex = GENDER_MALE;
						if( debug )
							CG_Printf( "%s%s -Gender set to MALE%s\n", S_COLOR_BLUE, token, S_COLOR_WHITE );
						
					} else if( token[0] == 'f' || token[0] == 'F' ) {
						pmodelinfo->sex = GENDER_FEMALE;
						if( debug )
							CG_Printf( "%s%s -Gender set to FEMALE%s\n", S_COLOR_BLUE, token, S_COLOR_WHITE );
					} else if ( token[0] == 'n' || token[0] == 'N' ) {
						pmodelinfo->sex = GENDER_NEUTRAL;
						if (debug)
							CG_Printf( "%s%s -Gender set to NEUTRAL%s\n", S_COLOR_BLUE, token, S_COLOR_WHITE );
					} else {
						if (debug){
							if(token[0])
								CG_Printf (" WARNING: unrecognized token: %s\n", token);
							else
								CG_Printf (" WARNING: no value after cmd sex: %s\n", token);
						}
						break; //Error
					}
#ifdef SKELMOD
				} else if ( !Q_stricmp (token, "offset") ) {
					lower_anims_have_offset = qtrue;
					if (debug)
						CG_Printf ("Script: Using offset values for lower frames\n");
#else
				} else if( !Q_stricmp( token, "nooffset" ) ) {		// nooffset
					nooffset = qtrue;
					if( debug )
						CG_Printf( "%sScript: No offset on lower frames%s\n", S_COLOR_BLUE, S_COLOR_WHITE );
#endif
				} else if( !Q_stricmp( token, "alljumps" ) ) {		// alljumps
					alljumps = qtrue;
					if( debug )
						CG_Printf( "%sScript: Using all jump animations%s\n", S_COLOR_BLUE, S_COLOR_WHITE );
#ifdef SKELMOD
				} else if ( !Q_stricmp (token, "isnumframes") ) {
					islastframe = qfalse;
					if (debug)
						CG_Printf ("Script: Second value is read as numframes\n");
#endif
				} else if( !Q_stricmp( token, "islastframe" ) ) {	// islastframe
					islastframe = qtrue;
					if( debug )
						CG_Printf( "%sScript: Second value is read as lastframe%s\n", S_COLOR_BLUE, S_COLOR_WHITE );
					
#ifdef SKELMOD
				} else if ( !Q_stricmp (token, "fps") ) {
					int fps;
					token = COM_ParseExt( &ptr, qfalse );
					if( !token ) break;	//Error (fixme)
					fps = atoi(token);
					if( fps < 10 )	//never allow less than 10 fps
						fps = 10;
					
					pmodelinfo->frametime = ( 1000/(float)fps );
					if( debug ) CG_Printf( "Script: FPS: %i\n", fps );
					
				// Rotation bone
				} else if( !Q_stricmp(token, "rotationbone") ) {
					token = COM_ParseExt( &ptr, qfalse );
					if(!token || !token[0]) break;	//Error (fixme)
					
					if( !Q_stricmp(token, "upper") ) {
						token = COM_ParseExt( &ptr, qfalse );
						if(!token || !token[0]) break;	//Error (fixme)
						CG_ParseRotationBone( pmodelinfo, token, UPPER );
					}
					else if( !Q_stricmp(token, "head") ) {
						token = COM_ParseExt( &ptr, qfalse );
						if( !token || !token[0] ) break;	//Error (fixme)
						CG_ParseRotationBone( pmodelinfo, token, HEAD );
					}
					else if( debug ) {
						CG_Printf( "Script: ERROR: Unrecognized rotation pmodel part %s\n", token);
						CG_Printf( "Script: ERROR: Valid names are: 'upper', 'head'\n");
					}
					
				// Root animation bone
				} else if( !Q_stricmp(token, "rootanim") ) {
					token = COM_ParseExt( &ptr, qfalse );
					if( !token || !token[0] ) break;
					
					if( !Q_stricmp(token, "upper") ) {
						rootanims[UPPER] = CG_FindBoneNum( CG_SkeletonForModel( pmodelinfo->model ), COM_ParseExt( &ptr, qfalse ) );
					}
					else if( !Q_stricmp(token, "head") ) {
						rootanims[HEAD] = CG_FindBoneNum( CG_SkeletonForModel( pmodelinfo->model ), COM_ParseExt( &ptr, qfalse ) );
					}
					else if( !Q_stricmp(token, "lower") ) {
						rootanims[LOWER] = CG_FindBoneNum( CG_SkeletonForModel( pmodelinfo->model ), COM_ParseExt( &ptr, qfalse ) );
						//we parse it so it makes no error, but we ignore it later on
						CG_Printf ("Script: WARNING: Ignored rootanim lower: Valid names are: 'upper', 'head' (lower is always skeleton root)\n");
					}
					else if( debug ) {
						CG_Printf ("Script: ERROR: Unrecognized root animation pmodel part %s\n", token);
						CG_Printf ("Script: ERROR: Valid names are: 'upper', 'head'\n");
					}
					
				// Tag bone (format is: tagmask "bone name" "tag name")
				} else if ( !Q_stricmp (token, "tagmask") ) {
					int	bonenum;
					
					token = COM_ParseExt ( &ptr, qfalse );
					if(!token || !token[0])
						break;//Error
					
					bonenum =  CG_FindBoneNum( CG_SkeletonForModel( pmodelinfo->model ), token);
					if( bonenum != -1 ) {
						//store the tag mask
						CG_ParseTagMask(pmodelinfo, bonenum, COM_ParseExt ( &ptr, qfalse ) );
					} else {
						if(debug)
							CG_Printf ("Script: WARNING: Unknown bone name: %s\n", token);
					}
#endif // SKELMOD
				} else if( token[0] && debug )						// unknown
					CG_Printf( "%sScript: ignored unknown: %s%s\n", S_COLOR_YELLOW, token, S_COLOR_WHITE );
				
		} else {
			// frame & animation values
			i = (int)atoi( token );
			if( debug ) {
				if( rounder == 0 )
					CG_Printf( "%sScript: %s", S_COLOR_BLUE, S_COLOR_WHITE );
				CG_Printf( "%s%i - %s", S_COLOR_BLUE, i, S_COLOR_WHITE );
			}
			
			anim_data[rounder][counter] = i;
			rounder++;
#ifdef SKELMOD
			if( rounder > 2 ) {
#else
			if( rounder > 3 ) {
#endif
				rounder = 0;
				if( debug )
					CG_Printf( "%s anim: %i%s\n", S_COLOR_BLUE, counter, S_COLOR_WHITE );
				counter++;
				if( counter == PMODEL_MAX_ANIMATIONS )
					break;
			}
		}
	}
	
	CG_Free( buf );

	// it must contain at least as many animations as a Q3 script to be valid
	if( counter-1 < LEGS_TURN ) {
		CG_Printf( "%sScript: Error: Not enough animations(%i) at animations script: %s%s\n", S_COLOR_YELLOW, counter, filename, S_COLOR_WHITE );
		return qfalse;
	}

	// animation ANIM_NONE (0) is always at frame 0, and it's never 
	// received from the game, but just used on the client when none
	// animation was ever set for a model (head being an example).
	anim_data[0][ANIM_NONE] = 0;
	anim_data[1][ANIM_NONE]	= 0;
	anim_data[2][ANIM_NONE]	= 1;
#ifndef SKELMOD
	anim_data[3][ANIM_NONE] = 10;
#endif
	for( i = 0 ; i < counter ; i++ ) {
		pmodelinfo->firstframe[i] = anim_data[0][i];
		if( islastframe )
			pmodelinfo->lastframe[i] = anim_data[1][i];
		else
			pmodelinfo->lastframe[i] = ( anim_data[0][i] + anim_data[1][i] );

		pmodelinfo->loopingframes[i] = anim_data[2][i];
#ifndef SKELMOD
		// HACK: make weapon switching animations run at fixed 10 fps
		if( i == TORSO_FLIPOUT || i == TORSO_FLIPIN || anim_data[3][i] < 10 )
			anim_data[3][i] = 10;

		pmodelinfo->frametime[i] = 1000/anim_data[3][i];
#endif
	}
#ifdef SKELMOD
	// create a bones array listing the animations each bone will play
	{
		cgs_skeleton_t *skel;
		int	j, bonenum;
		
		skel = CG_SkeletonForModel( pmodelinfo->model );
		rootanims[LOWER] = 0;
		memset( pmodelinfo->boneAnims, LOWER, sizeof(pmodelinfo->boneAnims) );
		for( j = LOWER + 1; j < PMODEL_PARTS; j++ ) {
			if( rootanims[j] == -1 ) continue;
			
			for( i = 0; i < skel->numBones; i++ ) {
				if( i == rootanims[j] ) {
					if( pmodelinfo->boneAnims[i] < j )
						pmodelinfo->boneAnims[i] = j;
					continue;
				}
				// run up to the tree root searching for rootanim bone
				bonenum = i;
				while( skel->bones[bonenum].parent != -1 ) {
					if( bonenum == rootanims[j] ) { // has the desired bone as parent
						if( pmodelinfo->boneAnims[i] < j )
							pmodelinfo->boneAnims[i] = j;
						break;
					}
					bonenum = skel->bones[bonenum].parent;
				}
			}
		}
	}

	if( lower_anims_have_offset ) {
#else
	if( !nooffset ) {
#endif
		offset = pmodelinfo->firstframe[LEGS_CRWALK] - pmodelinfo->firstframe[TORSO_GESTURE];
		for( i = LEGS_CRWALK; i < counter; ++i ) {
			pmodelinfo->firstframe[i] -= offset;
			pmodelinfo->lastframe[i] -= offset;
		}
		if( debug )
			CG_Printf( "%sScript: adding offset on lower frames (Q3 format)%s\n", S_COLOR_BLUE, S_COLOR_WHITE );
	}

	// fix the rates to fit my looping calculations
	for( i = 0; i < counter; ++i ) {
		if( pmodelinfo->loopingframes[i] )
			pmodelinfo->loopingframes[i] -= 1;
		if( pmodelinfo->lastframe[i] )
			pmodelinfo->lastframe[i] -= 1;
	}

	// Alljumps: I use 3 jump animations for bunnyhoping
	// animation support. But they will only be loaded as
	// bunnyhoping animations if the keyword "alljumps" is
	// present at the animation.cfg. Otherwise, LEGS_JUMP1
	// will be used for all the jump styles.

	if( !alljumps ) {
		CG_CopyAnimation( pmodelinfo, LEGS_JUMP3, LEGS_JUMP1 );
		CG_CopyAnimation( pmodelinfo, LEGS_JUMP3ST, LEGS_JUMP1ST );
		CG_CopyAnimation( pmodelinfo, LEGS_JUMP2, LEGS_JUMP1 );
		CG_CopyAnimation( pmodelinfo, LEGS_JUMP2ST, LEGS_JUMP1ST );
	}

	// Fix uncomplete scripts (for Q3 player models)
	counter--;

	if( counter < TORSO_RUN )
		CG_CopyAnimation( pmodelinfo, TORSO_RUN, TORSO_STAND );
	if( counter < TORSO_DROPHOLD )
		CG_CopyAnimation( pmodelinfo, TORSO_DROPHOLD, TORSO_STAND );
	if( counter < TORSO_DROP )
		CG_CopyAnimation( pmodelinfo, TORSO_DROP, TORSO_ATTACK2 );
	if( counter < TORSO_PAIN1 )
		CG_CopyAnimation( pmodelinfo, TORSO_PAIN1, TORSO_STAND2 );
	if( counter < TORSO_PAIN2 )
		CG_CopyAnimation( pmodelinfo, TORSO_PAIN2, TORSO_STAND2 );
	if( counter < TORSO_PAIN3 )
		CG_CopyAnimation( pmodelinfo, TORSO_PAIN3, TORSO_STAND2 );
	if( counter < TORSO_SWIM )
		CG_CopyAnimation( pmodelinfo, TORSO_SWIM, TORSO_STAND );

	if( counter < LEGS_WALKBACK )
		CG_CopyAnimation( pmodelinfo, LEGS_WALKBACK, LEGS_RUNBACK );
	if( counter < LEGS_WALKLEFT )
		CG_CopyAnimation( pmodelinfo, LEGS_WALKLEFT, LEGS_WALKFWD );
	if( counter < LEGS_WALKRIGHT )
		CG_CopyAnimation( pmodelinfo, LEGS_WALKRIGHT, LEGS_WALKFWD );
	if( counter < LEGS_RUNLEFT )
		CG_CopyAnimation( pmodelinfo, LEGS_RUNLEFT, LEGS_RUNFWD );
	if( counter < LEGS_RUNRIGHT )
		CG_CopyAnimation( pmodelinfo, LEGS_RUNRIGHT, LEGS_RUNFWD );
	if( counter < LEGS_SWIM )
		CG_CopyAnimation( pmodelinfo, LEGS_SWIM, LEGS_SWIMFWD );

	return qtrue;
}

/*
================
CG_LoadPModelWeaponSet
================
*/
void CG_LoadPModelWeaponSet( pmodel_t *pmodel )
{
	int i;

	for( i = 0; i < cgs.numWeaponModels; i++ ) {
		pmodel->weaponIndex[i] = CG_RegisterWeaponModel( cgs.weaponModels[i] );
		if( !cg_vwep->integer )
			break; // only do weapon 0
	}

	// weapon 0 must always load some animation script
	if (!pmodel->weaponIndex[0])
		pmodel->weaponIndex[0] = CG_CreateWeaponZeroModel( cgs.weaponModels[0] );
}

/*
================
CG_PSkinUpdateRegistration
================
*/
qboolean CG_PSkinUpdateRegistration( pskin_t *pSkin, char *filename )
{
#ifndef SKELMOD
	int 		i;
#endif
	char		skin_name[MAX_QPATH];
	char		path[MAX_QPATH];
	char		scratch[MAX_QPATH];

	// extract path & skin_name from filename
	char		*t = NULL;
	char		*s;

	// find the word after the last slash
	s = filename;
	t = strstr( s, "/" );
	while( t ) {
		s = t+1;
		t = strstr( s, "/" );
	}
	strcpy( skin_name, s );

	// remove it from the path
	Q_snprintfz( path, strlen(filename) - strlen(skin_name), "%s", filename );
#ifdef SKELMOD
	//proceed
	Q_snprintfz( scratch, sizeof(scratch), "%s/%s.skin", path, skin_name );
	pSkin->skin = trap_R_RegisterSkinFile( scratch );
	
	//failed: clean up the wrong stuff
	if( !pSkin->skin ) {
		pSkin->name[0] = 0;
		pSkin->skin = NULL;
		return qfalse;
	}
#else
	// load skinfiles for each model
	for( i = 0; i < PMODEL_PARTS; i++ ) {
		Q_snprintfz( scratch, sizeof(scratch), "%s/%s_%s.skin", path, pmPartNames[i], skin_name );
		pSkin->skin[i] = trap_R_RegisterSkinFile (scratch);
		if (!pSkin->skin[i]) {
			int	j;

			pSkin->name[0] = 0;
			for( j = 0; j < PMODEL_PARTS; j++ )
				pSkin->skin[i] = NULL;

			return qfalse;
		}
	}
#endif
	// load icon and finish
	Q_snprintfz( scratch, sizeof(scratch), "%s/icon_%s", path, skin_name );
	pSkin->icon = trap_R_RegisterPic( scratch );
	strcpy( pSkin->name, filename );
	return qtrue;
}

/*
===============
CG_FindPSkinSpot
===============
*/
struct pskin_s *CG_FindPSkinSpot( char *filename )
{
	int 	i;
	int		freespot = -1;

	for( i = 0; i < CG_MAX_PSKINS; i++ ) {
		if( cg_pSkins[i].inuse == qtrue ) {
			if( !Q_stricmp( cg_pSkins[i].name, filename ) ) { // found it
				if( cg_debugPlayerModels->integer )
					CG_Printf( "%spSkin: found at spot %i: %s\n", S_COLOR_WHITE, i, filename );
				
				return &cg_pSkins[i];
			}
		} else if( freespot < 0 )
			freespot = i;
	}

	if( freespot < 0 )
		CG_Error( "%sCG_FindPSkinsSpot: Couldn't find a free player skin spot%s", S_COLOR_RED, S_COLOR_WHITE );

	if( cg_debugPlayerModels->integer )
		CG_Printf( "%spSkin: assigned free spot %i for pSkin %s\n", S_COLOR_WHITE, freespot, filename );

	return &cg_pSkins[freespot];
}

/*
===============
CG_RegisterPSkin
===============
*/
struct pskin_s *CG_RegisterPSkin( char *filename )
{
	pskin_t	*pSkin;

	COM_StripExtension( filename, filename );
	pSkin = CG_FindPSkinSpot( filename );
	if( pSkin->inuse == qtrue )
		return pSkin;

	pSkin->inuse = CG_PSkinUpdateRegistration( pSkin, filename );

	if( pSkin->inuse != qtrue ) {
		if( cg_debugPlayerModels->integer )
			CG_Printf( "%spSkin: Registration failed: %s%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE );

		memset( pSkin, 0, sizeof( pSkin ) );
		return NULL;
	}

	return pSkin;
}

#ifdef SKELMOD
/*
//===============
CG_PModel_RegisterBoneposes
Sets up skeleton with inline boneposes based on frame/oldframe values
//===============
*/
void CG_PModel_RegisterBoneposes( pmodel_t *pmodel )
{
	cgs_skeleton_t		*skel;

	if( !pmodel->pmodelinfo->model )
		return;

	skel = CG_SkeletonForModel( pmodel->pmodelinfo->model );
	if( skel && skel == pmodel->skel )
		return;

	if( !skel ) //not skeletal model
	{
		if( pmodel->skel )
		{
			if( pmodel->curboneposes )	{
				CG_Free( pmodel->curboneposes );
				pmodel->curboneposes = NULL;
			}
			if( pmodel->oldboneposes ) {
				CG_Free( pmodel->oldboneposes );
				pmodel->oldboneposes = NULL;
			}

			pmodel->skel = NULL;
		}
		return;
	}

	if( !pmodel->skel || pmodel->skel && (pmodel->skel->numBones != skel->numBones)
		|| !pmodel->curboneposes || !pmodel->oldboneposes )
	{
		if( pmodel->curboneposes )	
			CG_Free( pmodel->curboneposes );
		if( pmodel->oldboneposes )
			CG_Free( pmodel->oldboneposes );
		
		pmodel->curboneposes = CG_Malloc ( sizeof(bonepose_t) * skel->numBones );
		pmodel->oldboneposes = CG_Malloc ( sizeof(bonepose_t) * skel->numBones );
	}

	pmodel->skel = skel;
}
#else // SKELMOD
/*
================
CG_PModelLoadModelPart
================
*/
qboolean CG_PModelLoadModelPart( pmodelinfo_t *pmodelinfo, char *model_name, int part )
{
	char	scratch[MAX_QPATH];

	Q_snprintfz( scratch, sizeof( scratch ), "%s/%s.md3", model_name, pmPartNames[part] );
	pmodelinfo->model[part] = CG_RegisterModel( scratch );

	if( !pmodelinfo->model[part] ) {
		Q_snprintfz( scratch, sizeof( scratch ), "%s/%s.skm", model_name, pmPartNames[part] );
		pmodelinfo->model[part] = CG_RegisterModel( scratch );
	}

	if( !pmodelinfo->model[part] )
		return qfalse;

	return qtrue;
}
#endif // SKELMOD
/*
================
CG_PModelUpdateRegistration
================
*/
qboolean CG_PModelUpdateRegistration( pmodelinfo_t *pmodelinfo, char *filename )
{
#ifndef SKELMOD
	int 		i;
#endif
	qboolean 	loaded_model = qfalse;
	char		anim_filename[MAX_QPATH];
#ifdef SKELMOD
	char	scratch[MAX_QPATH];

	Q_snprintfz( scratch, sizeof(scratch), "%s/tris.skm", filename );
	pmodelinfo->model = CG_RegisterModel( scratch );
	if( !trap_R_SkeletalGetNumBones( pmodelinfo->model, NULL ) ) // pmodels only accept skeletal models
		return qfalse;
	
	//load animations script
	if( pmodelinfo->model ) {
		Q_snprintfz( anim_filename, sizeof(anim_filename), "%s/animation.cfg", filename );
		loaded_model = CG_ParseAnimationScript( pmodelinfo, anim_filename );
	}
#else
	for( i = 0; i < PMODEL_PARTS; i++ ) {
		loaded_model = CG_PModelLoadModelPart( pmodelinfo, filename, i );
		if( !loaded_model )
			break;
	}
	
	if( loaded_model ) {
		Q_snprintfz( anim_filename, sizeof( anim_filename ), "%s/animation.cfg", filename );
		loaded_model = CG_ParseAnimationScript( pmodelinfo, anim_filename );
	}
#endif
	if( !loaded_model ) {
		pmodelinfo->model_name[0] = 0;
#ifdef SKELMOD
		pmodelinfo->model = NULL;
#else
		for( i = 0; i < PMODEL_PARTS; i++ )
			pmodelinfo->model[i] = NULL;
#endif
		return qfalse;
	}

	strcpy( pmodelinfo->model_name, filename );
	return qtrue;
}

/*
===============
CG_FindPModelSpot
===============
*/
struct pmodelinfo_s *CG_FindPModelSpot( char *filename )
{
	int 			i;
	int				freespot = -1;

	for( i = 0; i < CG_MAX_PMODELS; i++ ) {
		if( cg_PModelInfos[i].inuse == qtrue ) {
			if( !Q_stricmp( cg_PModelInfos[i].model_name, filename ) ) {
				if( cg_debugPlayerModels->integer )
					CG_Printf( "%sPModel: found at spot %i: %s\n", S_COLOR_WHITE, i, filename );

				return &cg_PModelInfos[i];
			}
		}
		else if( freespot < 0 )
			freespot = i;
	}

	if( freespot < 0 )
		CG_Error( "%sCG_FindPModelSpot: Couldn't find a free player model spot%s", S_COLOR_RED, S_COLOR_WHITE );

	if( cg_debugPlayerModels->integer )
		CG_Printf( "%sPModel: assigned free spot %i for pmodel %s\n", S_COLOR_WHITE, freespot, filename );

	return &cg_PModelInfos[freespot];
}

/*
===============
CG_RegisterPModel
===============
*/
struct pmodelinfo_s *CG_RegisterPModel( char *filename )
{
	pmodelinfo_t	*pmodelinfo;

	COM_StripExtension( filename, filename );
	pmodelinfo = CG_FindPModelSpot( filename );
	if( pmodelinfo->inuse == qtrue )
		return pmodelinfo;

	pmodelinfo->inuse = CG_PModelUpdateRegistration( pmodelinfo, filename );

	if( pmodelinfo->inuse != qtrue ) {
		if( cg_debugPlayerModels->integer )
			CG_Printf( "%sPModel: Registration failed: %s%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE );

		memset( pmodelinfo, 0, sizeof( pmodelinfo ) );
		return NULL;
	}

	return pmodelinfo;
}

/*
================
CG_RegisterBasePModel
================
*/
void CG_RegisterBasePModel( void )
{
	char	filename[MAX_QPATH];

	Q_snprintfz( filename, sizeof( filename ), "%s/%s", "models/players", DEFAULT_PLAYERMODEL );
	cgs.basePModelInfo = CG_RegisterPModel( filename );

	Q_snprintfz( filename, sizeof( filename ), "%s/%s/%s","models/players" , DEFAULT_PLAYERMODEL, DEFAULT_PLAYERSKIN );
	cgs.basePSkin = CG_RegisterPSkin( filename );

	// default player model must load perfect
	if( !cgs.basePModelInfo )
		CG_Error( "%s'Default Player Model'(%s): failed to load%s\n", S_COLOR_RED, DEFAULT_PLAYERMODEL, S_COLOR_WHITE );
	if( !cgs.basePSkin )
		CG_Error( "%s'Default Player Model'(%s): Skin (%s) failed to load%s\n", S_COLOR_RED, DEFAULT_PLAYERMODEL, DEFAULT_PLAYERMODEL, S_COLOR_WHITE );
	if( !cgs.basePSkin->icon )
		CG_Error( "%s'Default Player Model'(%s): Icon failed to load%s\n", S_COLOR_RED, DEFAULT_PLAYERMODEL, S_COLOR_WHITE );
}

/*
================
CG_LoadClientPmodel
================
*/
void CG_LoadClientPmodel( int centNum, char *model_name, char *skin_name )
{
	pmodel_t			*pmodel;
	char				filename[MAX_QPATH];
	char				*path = "models/players";
	char				skin_filename[MAX_QPATH];

	if( centNum < 0 ) 
		centNum = 0;
	
	pmodel = &cg_entPModels[centNum];
#ifdef SKELMOD
	pmodel->number = centNum;
#endif
	// load user defined pmodelinfo
	Q_snprintfz( filename, sizeof( filename ), "%s/%s",path, model_name );
	pmodel->pmodelinfo = CG_RegisterPModel( filename );

	// load skin
	Q_snprintfz( skin_filename, sizeof( skin_filename ), "%s/%s/%s",path, model_name, skin_name );
	pmodel->pSkin = CG_RegisterPSkin( skin_filename );
	
	// didn't work: try DEFAULT_PLAYERMODEL with the user defined skin (for ctf skins)
	if( ( !pmodel->pmodelinfo || !pmodel->pSkin ) && Q_stricmp( model_name, DEFAULT_PLAYERMODEL ) ) {
		strcpy( model_name, DEFAULT_PLAYERMODEL );
		Q_snprintfz( filename, sizeof( filename ), "%s/%s",path, model_name );
		pmodel->pmodelinfo = CG_RegisterPModel( filename );
		Q_snprintfz( skin_filename, sizeof( skin_filename ), "%s/%s/%s",path, model_name, skin_name );
		pmodel->pSkin = CG_RegisterPSkin( skin_filename );
	}
	
	// if still didn't load, it means that the DEFAULT_PLAYERMODEL
	// didn't have the custom skin so back to DEFAULT_PLAYERSKIN
	if( !pmodel->pSkin ) {
		strcpy( skin_name, DEFAULT_PLAYERSKIN );
		Q_snprintfz( skin_filename, sizeof( skin_filename ), "%s/%s/%s",path, model_name, skin_name );
		pmodel->pSkin = CG_RegisterPSkin( skin_filename );
	}

	if( !pmodel->pmodelinfo )
		CG_Error( "Couldn't load default player model" );
#ifdef SKELMOD
	//Init the boneposes for animation blending
	CG_PModel_RegisterBoneposes( pmodel );
#endif
	// weaponIndexes
	CG_LoadPModelWeaponSet( pmodel );

	// update the frames for the new animation script
	if( !pmodel->anim.current[UPPER] || !pmodel->anim.current[LOWER] || pmodel->anim.current[HEAD] != ANIM_NONE ) {
		pmodel->anim.current[UPPER] = TORSO_STAND;
		pmodel->anim.current[LOWER] = LEGS_STAND;
		pmodel->anim.current[HEAD] = ANIM_NONE;
	}
	
	pmodel->anim.oldframe[UPPER] = pmodel->anim.frame[UPPER] = pmodel->pmodelinfo->lastframe[pmodel->anim.current[UPPER]];
	pmodel->anim.oldframe[LOWER] = pmodel->anim.frame[LOWER] = pmodel->pmodelinfo->lastframe[pmodel->anim.current[LOWER]];
	pmodel->anim.oldframe[HEAD] = pmodel->anim.frame[HEAD] = pmodel->pmodelinfo->lastframe[pmodel->anim.current[HEAD]];
}

/*
======================================================================
						tools
======================================================================
*/

#ifdef SKELMOD
/*
===============
CG_Pmodel_TagMask
===============
*/
char *CG_Pmodel_TagMask( char *maskname, pmodelinfo_t *pmodelinfo )
{
	cg_tagmask_t	*tagmask;

	if( !pmodelinfo )
		return maskname;

	for( tagmask = pmodelinfo->tagmasks; tagmask; tagmask = tagmask->next ) {
		if( !Q_stricmp( tagmask->tagname, maskname ) )
			return tagmask->bonename;
	}

	return maskname;
}
#endif
/*
===============
CG_GrabTag
In the case of skeletal models, boneposes must
be transformed prior to calling this function
===============
*/
qboolean CG_GrabTag( orientation_t *tag, entity_t *ent, const char *tagname )
{
	cgs_skeleton_t	*skel;

	if( !ent->model )
		return qfalse;

	skel = CG_SkeletonForModel( ent->model );
	if( skel )
		return CG_SkeletalPoseGetAttachment( tag, skel, ent->boneposes, tagname );

	return trap_R_LerpTag( tag, ent->model, ent->frame, ent->oldframe, ent->backlerp, tagname );
}

/*
===============
CG_PlaceRotatedModelOnTag
===============
*/
void CG_PlaceRotatedModelOnTag( entity_t *ent, entity_t *dest, orientation_t *tag )
{
	int		i;
	vec3_t	tmpAxis[3];

	VectorCopy( dest->origin, ent->origin );
	VectorCopy( dest->lightingOrigin, ent->lightingOrigin );
	
	for( i = 0 ; i < 3 ; i++ )
		VectorMA( ent->origin, tag->origin[i] * ent->scale, dest->axis[i], ent->origin );

	VectorCopy( ent->origin, ent->origin2 );
	Matrix_Multiply( ent->axis, tag->axis, tmpAxis );
	Matrix_Multiply( tmpAxis, dest->axis, ent->axis );
}

/*
===============
CG_PlaceModelOnTag
===============
*/
void CG_PlaceModelOnTag( entity_t *ent, entity_t *dest, orientation_t *tag )
{
	int i;
	
	VectorCopy( dest->origin, ent->origin );
	VectorCopy( dest->lightingOrigin, ent->lightingOrigin );
	
	for( i = 0 ; i < 3 ; i++ )
		VectorMA( ent->origin, tag->origin[i] * ent->scale, dest->axis[i], ent->origin );
	
	VectorCopy( ent->origin, ent->origin2 );
	Matrix_Multiply( tag->axis, dest->axis, ent->axis );
}

/*
===============
CG_MoveToTag
"move" tag must have an axis and origin set up. 
Use vec3_origin and axis_identity for "nothing"
===============
*/
void CG_MoveToTag( vec3_t move_origin, vec3_t move_axis[3], vec3_t dest_origin, vec3_t dest_axis[3], vec3_t tag_origin, vec3_t tag_axis[3] )
{
	int		i;
	vec3_t	tmpAxis[3];

	VectorCopy( dest_origin, move_origin );
	
	for( i = 0 ; i < 3 ; i++ )
		VectorMA( move_origin, tag_origin[i], dest_axis[i], move_origin );

	Matrix_Multiply( move_axis, tag_axis, tmpAxis );
	Matrix_Multiply( tmpAxis, dest_axis, move_axis );
}

/*
===============
CG_PModel_CalcProjectionSource
It asumes the player entity is up to date
===============
*/
qboolean CG_PModel_CalcProjectionSource( int entnum, orientation_t *tag_result )
{
	centity_t		*cent;
	pmodel_t		*pmodel;

	if( !tag_result )
		return qfalse;

	if( entnum < 1 || entnum >= MAX_EDICTS )
		return qfalse;

	cent = &cg_entities[entnum];
	if( cent->serverFrame != cg.frame.serverFrame ) 
		return qfalse;

	// see if it's the viewweapon
	if( vweap.active && (cg.chasedNum+1 == entnum) && !cg.thirdPerson ) {
		VectorCopy( vweap.projectionSource.origin, tag_result->origin );
		Matrix_Copy( vweap.projectionSource.axis, tag_result->axis );
		return qtrue;
	}
	
	// it's a 3rd person model
	pmodel = &cg_entPModels[entnum];
	VectorCopy( pmodel->projectionSource.origin, tag_result->origin );
	Matrix_Copy( pmodel->projectionSource.axis, tag_result->axis );
	return qtrue;
}

#ifndef SKELMOD // brass not supported
/*
===============
CG_PModel_CalcBrassSource
===============
*/
void CG_PModel_CalcBrassSource( pmodel_t *pmodel, orientation_t *projection )
{
	orientation_t	tag_weapon;

	// this code asumes that the pmodel ents are up-to-date
	if( !pmodel->pmodelinfo || !pmodel->ents[UPPER].model || !pmodel->ents[LOWER].model 
		|| !trap_R_LerpTag( &tag_weapon, pmodel->ents[UPPER].model, pmodel->ents[UPPER].frame, pmodel->ents[UPPER].oldframe, pmodel->ents[UPPER].backlerp, "tag_weapon" ) ) {
		VectorCopy( pmodel->ents[LOWER].origin, projection->origin );
		projection->origin[2] += 16;
		Matrix_Identity( projection->axis );
	}

	// brass projection is just tag_weapon
	VectorCopy( vec3_origin, projection->origin );
	Matrix_Copy( axis_identity, projection->axis );
	CG_MoveToTag( projection->origin, projection->axis,
		pmodel->ents[UPPER].origin, pmodel->ents[UPPER].axis,
		tag_weapon.origin, tag_weapon.axis );
}
#endif // SKELMOD
/*
===============
CG_AddQuadShell
===============
*/
void CG_AddQuadShell( entity_t *ent )
{
	entity_t	shell;

	shell = *ent;
	shell.customSkin = NULL;

	if( shell.flags & RF_WEAPONMODEL )
		shell.customShader = CG_MediaShader( cgs.media.shaderQuadWeapon );
	else
		shell.customShader = CG_MediaShader( cgs.media.shaderPowerupQuad );

	CG_AddEntityToScene( &shell );
}

/*
===============
CG_AddPentShell
===============
*/
void CG_AddPentShell( entity_t *ent )
{
	entity_t	shell;

	shell = *ent;
	shell.customSkin = NULL;

	if( shell.flags & RF_WEAPONMODEL )
		shell.customShader = CG_MediaShader( cgs.media.shaderPowerupPenta );//fixme: look for a PENTA shader for view weapon
	else
		shell.customShader = CG_MediaShader( cgs.media.shaderPowerupPenta );

	CG_AddEntityToScene( &shell );
}

/*
===============
CG_AddShellEffects
===============
*/
void CG_AddShellEffects( entity_t *ent, int effects )
{
	// quad and pent can do different things on client
	if( effects & EF_QUAD ) 
		CG_AddQuadShell( ent );

	if( effects & EF_PENT ) 
		CG_AddPentShell( ent );
}

/*
===============
CG_AddColorShell_Do
===============
*/
void CG_AddColorShell_Do( entity_t *ent, int renderfx )
{
	entity_t	shell;
	int			i;
	vec4_t shadelight = { 0, 0, 0, 0.3 };

	memset( &shell, 0, sizeof(shell) );

	shell = *ent;
	shell.customSkin = NULL;

	if( renderfx & RF_SHELL_RED )
		shadelight[0] = 1.0;
	if( renderfx & RF_SHELL_GREEN )
		shadelight[1] = 1.0;
	if( renderfx & RF_SHELL_BLUE )
		shadelight[2] = 1.0;

	for( i = 0 ; i < 4 ; i++ )
		shell.color[i] = shadelight[i] * 255;

	if( ent->flags & RF_WEAPONMODEL )
		return;		// fixme: try the shell shader for viewweapon, or build a good one
	
	shell.customShader = CG_MediaShader( cgs.media.shaderShellEffect );

	CG_AddEntityToScene( &shell );
}

/*
===============
CG_AddColorShell
===============
*/
void CG_AddColorShell( entity_t *ent, int renderfx )
{
	if( renderfx & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE) )
		CG_AddColorShell_Do( ent, renderfx );
}

/*
======================================================================
							animations
======================================================================
*/
#ifdef SKELMOD
/*
===============
CG_PModel_BlendSkeletalPoses
Sets up the boneposes array mixing up bones
from different frames, based on the defined
bone names. Result bones are not transformed.
===============
*/
static void CG_PModel_BlendSkeletalPoses( cgs_skeleton_t *skel, bonepose_t *boneposes, int *boneAnims, int *boneKeyFrames )
{
	int			i, frame;
	bonepose_t	*framed_bonepose;
	
	for( i = 0; i < skel->numBones; i++ ) {
		frame = boneKeyFrames[ boneAnims[i] ];
		if( frame < 0 || frame >= skel->numFrames )
			frame = 0;

		framed_bonepose = skel->bonePoses[frame];
		framed_bonepose += i;
		memcpy( &boneposes[i], framed_bonepose, sizeof(bonepose_t) );
	}		
}
#endif

/*
===============
CG_PModelAnimToFrame
===============
*/
#ifdef SKELMOD
void CG_PModelAnimToFrame( pmodel_t *pmodel, animationinfo_t *anim )
#else
void CG_PModelAnimToFrame( pmodelinfo_t *pmodelinfo, animationinfo_t *anim )
#endif
{
	int i;
#ifdef SKELMOD
	pmodelinfo_t	*pmodelinfo = pmodel->pmodelinfo;
	cgs_skeleton_t	*skel;

	//interpolate
	if (cg.time < anim->nextframetime)
	{
		anim->backlerp = 1.0f - ((cg.time - anim->prevframetime)/(anim->nextframetime - anim->prevframetime));
		if (anim->backlerp > 1)
			anim->backlerp = 1;
		else if (anim->backlerp < 0)
			anim->backlerp = 0;
		
		return;
	}
#endif
	for( i = 0; i < PMODEL_PARTS; i++ ) {
#ifndef SKELMOD
		// interpolate
		if( cg.time < anim->nextframetime[i] ) {
			anim->backlerp[i] = 1.0f - ( (cg.time - anim->prevframetime[i]) / (anim->nextframetime[i] - anim->prevframetime[i]) );
			if( anim->backlerp[i] > 1 )
				anim->backlerp[i] = 1;
			else if( anim->backlerp[i] < 0 )
				anim->backlerp[i] = 0;
			continue;
		}
#endif
		// advance frames
		anim->oldframe[i] = anim->frame[i];
		anim->frame[i]++;

		// looping
		if( anim->frame[i] > pmodelinfo->lastframe[anim->current[i]] ) {
			if( anim->currentChannel[i] )	
				anim->currentChannel[i] = BASIC_CHANNEL;//kill

			anim->frame[i] = (pmodelinfo->lastframe[anim->current[i]] - pmodelinfo->loopingframes[anim->current[i]]);
		}

		// new animation
		if( anim->buffer[EVENT_CHANNEL].newanim[i] ) {
			// backup if basic
			if( anim->buffer[BASIC_CHANNEL].newanim[i] +
				anim->currentChannel[i] == BASIC_CHANNEL ) { //both are zero
				anim->buffer[BASIC_CHANNEL].newanim[i] = anim->current[i];
			}

			// set up
			anim->current[i] = anim->buffer[EVENT_CHANNEL].newanim[i];
			anim->frame[i] = pmodelinfo->firstframe[anim->current[i]];
			anim->currentChannel[i] = EVENT_CHANNEL;
			anim->buffer[EVENT_CHANNEL].newanim[i] = 0;
		} else if ( anim->buffer[BASIC_CHANNEL].newanim[i]	&& ( anim->currentChannel[i] != EVENT_CHANNEL ) ) {
			// set up
			anim->current[i] = anim->buffer[BASIC_CHANNEL].newanim[i];
			anim->frame[i] = pmodelinfo->firstframe[anim->current[i]];
			anim->currentChannel[i] = BASIC_CHANNEL;
			anim->buffer[BASIC_CHANNEL].newanim[i] = 0;
		}
#ifndef SKELMOD
		// updated frametime
		anim->prevframetime[i] = cg.time;
		anim->nextframetime[i] = cg.time + pmodelinfo->frametime[anim->current[i]];
		anim->backlerp[i] = 1.0f;
#endif
	}

#ifdef SKELMOD
	skel = CG_SkeletonForModel( pmodel->pmodelinfo->model );
	if( !skel )
		CG_Error( "Non-skeletal PModel inside 'CG_PModelAnimToFrame'\n" );

	if( skel != pmodel->skel )
		CG_PModel_RegisterBoneposes( pmodel ); // update registration

	// blend animations into old-current poses
	CG_PModel_BlendSkeletalPoses( skel, pmodel->curboneposes, pmodel->pmodelinfo->boneAnims, pmodel->anim.frame );
	CG_PModel_BlendSkeletalPoses( skel, pmodel->oldboneposes, pmodel->pmodelinfo->boneAnims, pmodel->anim.oldframe );

	// updated frametime
	anim->prevframetime = cg.time;
	anim->nextframetime = cg.time + pmodelinfo->frametime;
	anim->backlerp = 1.0f;
#endif
}

/*
===============
CG_ClearEventAnimations
===============
*/
void CG_ClearEventAnimations( entity_state_t *state )
{
	int i;
	pmodel_t	*pmodel;

	pmodel = &cg_entPModels[state->number];
	for( i = LOWER; i < PMODEL_PARTS; i++ ) {
		pmodel->anim.buffer[EVENT_CHANNEL].newanim[i] = 0;
		if( pmodel->anim.currentChannel[i] == EVENT_CHANNEL ) {
			pmodel->anim.frame[i] = pmodel->pmodelinfo->lastframe[pmodel->anim.current[i]];
			pmodel->anim.currentChannel[i] = BASIC_CHANNEL;
		}
	}
}

/*
===============
CG_AddPModelAnimation
===============
*/
void CG_AddPModelAnimation( pmodel_t *pmodel, int loweranim, int upperanim, int headanim, int channel )
{
	int	i;
	int newanim[PMODEL_PARTS];
	animationbuffer_t	*buffer;

	newanim[LOWER] = loweranim;
	newanim[UPPER] = upperanim;
	newanim[HEAD] = headanim;
	buffer = &pmodel->anim.buffer[channel];

	for( i = 0 ; i < PMODEL_PARTS ; i++ ) {
		// ignore new events if in death
		if( channel && buffer->newanim[i] && ( buffer->newanim[i] < TORSO_GESTURE ) )
			continue;
		if( newanim[i] && ( newanim[i] < PMODEL_MAX_ANIMS ) )
			buffer->newanim[i] = newanim[i];
	}
}

/*
===============
CG_AddAnimationFromState
===============
*/
void CG_AddAnimationFromState( entity_state_t *state, int loweranim, int upperanim, int headanim, int channel ) {
	CG_AddPModelAnimation( &cg_entPModels[state->number], loweranim, upperanim, headanim, channel );
}

/*
==============
CG_PModels_ResetBarrelRotation
==============
*/
void CG_PModels_ResetBarrelRotation( entity_state_t *state )
{
	pmodel_t *pmodel;

	pmodel = &cg_entPModels[state->number];
					
	// new animation
	pmodel->pweapon.flashtime = 0;
	pmodel->pweapon.rotationSpeed = 0;
}

/*
==============
CG_PModels_AddFireEffects
==============
*/
void CG_PModels_AddFireEffects( entity_state_t *state )
{
	pmodel_t *pmodel;

	if( state->effects & EF_CORPSE )
		return;
	if( cg_paused->integer )
		return;

	pmodel = &cg_entPModels[state->number];

	// activate weapon flash
	if( cg_weaponFlashes->integer )
#ifdef SKELMOD
		pmodel->pweapon.flashtime = cg.time + (int)( (pmodel->pmodelinfo->frametime/4)*3 );
#else
		pmodel->pweapon.flashtime = cg.time + (int)( ( pmodel->pmodelinfo->frametime[TORSO_ATTACK1]/4 )*3 );
#endif

	// we set flashtime above because otherwise barrel
	// rotation and weapon flashes won't work for mirrors

	// if it's the user in 1st person, viewweapon code handles this
	if( state->number == cg.chasedNum+1 && !cg.thirdPerson )
		return;

	// add effects
	switch( state->weapon )
	{
		case WEAP_BLASTER:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
			break;
		case WEAP_HYPERBLASTER:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
			break;
		case WEAP_MACHINEGUN:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
#ifndef SKELMOD
			if( cg_ejectBrass->integer > 1 ) {
				orientation_t projection;
				CG_PModel_CalcBrassSource( pmodel, &projection );
				CG_EjectBrass( projection.origin, 1, CG_MediaModel( cgs.media.modEjectBrassMachinegun ) );	// eject brass-debris
			}
#endif
			break;
		case WEAP_SHOTGUN:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
#ifndef SKELMOD
			if( cg_ejectBrass->integer > 1 ) {
				orientation_t projection;
				CG_PModel_CalcBrassSource( pmodel, &projection );
				CG_EjectBrass( projection.origin, 1, CG_MediaModel( cgs.media.modEjectBrassShotgun ) );	// eject brass-debris
			}
#endif
			break;
		case WEAP_SUPERSHOTGUN:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
#ifndef SKELMOD
			if( cg_ejectBrass->integer > 1 ) {
				orientation_t projection;
				CG_PModel_CalcBrassSource( pmodel, &projection );
				CG_EjectBrass( projection.origin, 2, CG_MediaModel( cgs.media.modEjectBrassShotgun ) );	// eject brass-debris
			}
#endif
			break;
		case WEAP_CHAINGUN:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
#ifndef SKELMOD
			if( cg_ejectBrass->integer > 1 ) {
				orientation_t projection;
				CG_PModel_CalcBrassSource( pmodel, &projection );
				CG_EjectBrass( projection.origin, 3, CG_MediaModel( cgs.media.modEjectBrassMachinegun ) );	// eject brass-debris
			}
#endif
			break;
		case WEAP_RAILGUN:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
			break;
		case WEAP_ROCKETLAUNCHER:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
			break;
		case WEAP_GRENADELAUNCHER:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
			break;
		case WEAP_BFG:
			CG_AddPModelAnimation( pmodel, 0, TORSO_ATTACK1, 0, EVENT_CHANNEL );
			break;
		default:
			break;
	}
}

/*
======================================================================
							player model
======================================================================
*/
#ifdef SKELMOD
/*
==================
CG_PModelFixOldAnimationMiss
Run the animtoframe twice so it generates an old skeleton pose
==================
*/
void CG_PModelFixOldAnimationMiss( entity_state_t *state )
{
	pmodel_t	*pmodel;

	pmodel = &cg_entPModels[state->number];
	pmodel->number = state->number;

	//set up old frame
	pmodel->anim.nextframetime = cg.time;
	CG_PModelAnimToFrame( pmodel, &pmodel->anim );
	pmodel->anim.nextframetime = cg.time;
	CG_PModelAnimToFrame( pmodel, &pmodel->anim );
}
#endif

/*
==================
CG_PModelUpdateState
==================
*/
void CG_PModelUpdateState( entity_state_t *state )
{
	int newanim[PMODEL_PARTS];
	entity_state_t *oldstate;
	int			i;
	pmodel_t	*pmodel;

	pmodel = &cg_entPModels[state->number];
#ifdef SKELMOD
	pmodel->number = state->number;
#endif
	if( state->modelindex != 255 ) {
		// in ctf, corpses mirror it's client pmodel (ctf skins)
		if( cgs.gametype == GAMETYPE_CTF && state->effects & EF_CORPSE ) {
			pmodel->pmodelinfo = cg_entPModels[(state->skinnum & 0xff)+1].pmodelinfo;
			pmodel->pSkin = cg_entPModels[(state->skinnum & 0xff)+1].pSkin;
		} else {	// indexed
			pmodel->pmodelinfo = cgs.pModelsIndex[state->modelindex];
			pmodel->pSkin = cgs.pSkinsIndex[state->skinnum];
		}
	}

	// fallback
	if( !pmodel->pmodelinfo || !pmodel->pSkin ) {
		pmodel->pmodelinfo = cgs.basePModelInfo;
		pmodel->pSkin = cgs.basePSkin;
	}
#ifdef SKELMOD
	// make sure al poses have their memory space
	CG_RegisterBoneposesForCGEntity( &cg_entities[pmodel->number], pmodel->pmodelinfo->model );
	CG_PModel_RegisterBoneposes( pmodel );
#else
	// update ents models & skins
	for( i = LOWER ; i < PMODEL_PARTS ; i++ ) {
		pmodel->ents[i].rtype = RT_MODEL;
		pmodel->ents[i].customShader = NULL;
		pmodel->ents[i].model = pmodel->pmodelinfo->model[i];
		pmodel->ents[i].customSkin = pmodel->pSkin->skin[i];
	}
#endif
	// update pweapon
	pmodel->pweapon.weaponInfo = CG_GetWeaponFromPModelIndex( pmodel, state->weapon );

	// update parts rotation angles
	for( i = LOWER ; i < PMODEL_PARTS ; i++ )
		VectorCopy(pmodel->angles[i], pmodel->oldangles[i]);

	if( !(state->effects & EF_CORPSE) ) {
		// lower has horizontal direction, and zeroes vertical
		pmodel->angles[LOWER][PITCH] = 0;
		pmodel->angles[LOWER][YAW] = state->angles[YAW];
		pmodel->angles[LOWER][ROLL] = 0;

		// upper marks vertical direction (total angle, so it fits aim)
		if( state->angles[PITCH] > 180 )
			pmodel->angles[UPPER][PITCH] = -360 + state->angles[PITCH];
		else
			pmodel->angles[UPPER][PITCH] = state->angles[PITCH];

		pmodel->angles[UPPER][YAW] = 0;
		pmodel->angles[UPPER][ROLL] = 0;

		// head adds a fraction of vertical angle again
		if( state->angles[PITCH] > 180 )
			pmodel->angles[HEAD][PITCH] = (-360 + state->angles[PITCH])/3;
		else
			pmodel->angles[HEAD][PITCH] = state->angles[PITCH]/3;

		pmodel->angles[HEAD][YAW] = 0;
		pmodel->angles[HEAD][ROLL] = 0;
	}

	// spawning (EV_TELEPORT) forces nobacklerp and the interruption of EVENT_CHANNEL animations
	if( (state->events[0] == EV_TELEPORT) || (state->events[1] == EV_TELEPORT) ) {
		CG_ClearEventAnimations( state );
		CG_AddAnimationFromState( state, (state->frame)&0x3F, (state->frame>>6)&0x3F, (state->frame>>12)&0xF, BASIC_CHANNEL );
#ifdef SKELMOD
		CG_PModelFixOldAnimationMiss( state );
#else
		// set up old frame
		for( i = LOWER ; i < PMODEL_PARTS ; i++ )
			pmodel->anim.nextframetime[i] = cg.time;

		CG_PModelAnimToFrame( pmodel->pmodelinfo, &pmodel->anim );
		pmodel->anim.oldframe[LOWER] = pmodel->anim.frame[LOWER];
		pmodel->anim.oldframe[UPPER] = pmodel->anim.frame[UPPER];
		pmodel->anim.oldframe[HEAD] = pmodel->anim.frame[HEAD];
#endif
		for( i = LOWER; i < PMODEL_PARTS; i++ )
			VectorCopy(pmodel->angles[i], pmodel->oldangles[i]);

		return;
	}

	oldstate = &cg_entities[state->number].prev;

	// filter repeated animations coming from state->frame
	newanim[LOWER] = (state->frame&0x3F) * ((state->frame &0x3F) != (oldstate->frame &0x3F));
	newanim[UPPER] = (state->frame>>6 &0x3F) * ((state->frame>>6 &0x3F) != (oldstate->frame>>6 &0x3F));
	newanim[HEAD] = (state->frame>>12 &0xF) * ((state->frame>>12 &0xF) != (oldstate->frame>>12 &0xF));

	CG_AddAnimationFromState( state, newanim[LOWER], newanim[UPPER], newanim[HEAD], BASIC_CHANNEL);
}

/*
==================
CG_PModelsUpdateStates
==================
*/
void CG_PModelsUpdateStates( void )
{
	int					pnum;
	entity_state_t		*state;

	for( pnum = 0; pnum < cg.frame.numEntities; pnum++ ) {
		state = &cg_parseEntities[(cg.frame.parseEntities+pnum)&(MAX_PARSE_ENTITIES-1)];
		if( (state->type & ~ET_INVERSE) == ET_PLAYER )
			CG_PModelUpdateState( state );
	}
}

#ifdef SKELMOD
/*
===============
CG_AddPModelEnt
===============
*/
void CG_AddPModel( entity_t *ent, entity_state_t *state )
{
	int					i, j;
	pmodel_t			*pmodel;
	vec3_t				tmpangles;
	orientation_t		tag_weapon;
	
	pmodel = &cg_entPModels[state->number];
	
	//transform animation values into frames, and set up old-current poses pair
	CG_PModelAnimToFrame( pmodel, &pmodel->anim );

	//lerp old-current poses pair into interpolated pose
	CG_LerpBoneposes( pmodel->skel,
		pmodel->curboneposes, pmodel->oldboneposes,
		centBoneposes[state->number].lerpboneposes,
		1.0f - pmodel->anim.backlerp );

	//relink interpolated pose into ent
	ent->boneposes = ent->oldboneposes = centBoneposes[state->number].lerpboneposes;

	//add skeleton effects (pose is unmounted yet)
	if( state->effects & EF_CORPSE ) {
		AnglesToAxis ( state->angles, ent->axis );
	} else {
		//apply lerped LOWER angles to entity
		for( j = 0; j < 3; j++ )
			tmpangles[j] = LerpAngle( pmodel->oldangles[LOWER][j], pmodel->angles[LOWER][j], cg.lerpfrac );
		
		AnglesToAxis( tmpangles, ent->axis );
		
		//apply UPPER and HEAD angles to rotator bones
		for( i=1; i<PMODEL_PARTS; i++ )	
		{
			if( pmodel->pmodelinfo->numRotators[i] )
			{
				//lerp rotation and divide angles by the number of rotation bones
				for( j = 0; j < 3; j++ ){
					tmpangles[j] = LerpAngle( pmodel->oldangles[i][j], pmodel->angles[i][j], cg.lerpfrac );
					tmpangles[j] /= pmodel->pmodelinfo->numRotators[i];
				}
				for( j = 0; j < pmodel->pmodelinfo->numRotators[i]; j++ )
					CG_RotateBonePose( tmpangles, &ent->boneposes[pmodel->pmodelinfo->rotator[i][j]] );
			}
		}
	}

	//finish (mount) pose. Now it's the final skeleton just as it's drawn.
	CG_TransformBoneposes( centBoneposes[state->number].skel, centBoneposes[state->number].lerpboneposes, centBoneposes[state->number].lerpboneposes);

	ent->backlerp = 0.0f;
	ent->frame = ent->oldframe = 0;//frame fields are not used with external poses

	// Add playermodel ent
	ent->rtype = RT_MODEL;
	ent->customShader = NULL;
	ent->model = pmodel->pmodelinfo->model;
	ent->customSkin = pmodel->pSkin->skin;
	
	CG_AddEntityToScene( ent );
	if( !ent->model )
		return;

	CG_AddShellEffects( ent, state->effects );
	CG_AddColorShell( ent, state->renderfx );

	// always create a projectionSource fallback
	VectorMA( ent->origin, 24, ent->axis[2], pmodel->projectionSource.origin );
	VectorMA( pmodel->projectionSource.origin, 16, ent->axis[0], pmodel->projectionSource.origin );
	Matrix_Copy( ent->axis, pmodel->projectionSource.axis );

	// Add weapon model
	if (state->modelindex2 != 255) //player doesn't have a weapon
		return;

	if( CG_GrabTag( &tag_weapon, ent, CG_Pmodel_TagMask("tag_weapon", pmodel->pmodelinfo) ) )
		CG_AddWeaponOnTag( ent, &tag_weapon, &pmodel->pweapon, 0, &pmodel->projectionSource );
}

#else // SKELMOD

/*
===============
CG_AddPModelEnt
===============
*/
void CG_AddPModel( entity_t *ent, entity_state_t *state )
{
	orientation_t		tag;
	int					i, prev, j;
	pmodel_t			*pmodel;
	vec3_t				tmpangles;

	pmodel = &cg_entPModels[state->number];

	// transform animation values into frames
	CG_PModelAnimToFrame( pmodel->pmodelinfo, &pmodel->anim );
	for( i = LOWER; i < PMODEL_PARTS; i++ ) {
		pmodel->ents[i].backlerp = pmodel->anim.backlerp[i];
		pmodel->ents[i].frame = pmodel->anim.frame[i];
		pmodel->ents[i].oldframe = pmodel->anim.oldframe[i];
	}

	if( state->effects & EF_CORPSE ) {
		// zero rotations
		AnglesToAxis ( state->angles, pmodel->ents[LOWER].axis );
		for( i = UPPER; i < PMODEL_PARTS; i++ )
			Matrix_Copy( axis_identity, pmodel->ents[i].axis );
		
	} else {
		for( i = LOWER; i < PMODEL_PARTS; i++ ) {
			// lerp rotated angles
			for( j = 0; j < 3; j++ )
				tmpangles[j] = LerpAngle( pmodel->oldangles[i][j], pmodel->angles[i][j], cg.lerpfrac );
			AnglesToAxis( tmpangles, pmodel->ents[i].axis );
		}
	}

	// special set up LOWER for entity
	// if 1st person addapt the position so 
	// the shadow isn't delayed behind the player

	if( state->number == cg.chasedNum + 1 && !cg.thirdPerson ) {
		vec3_t		org;
		if( cg_predict->integer && !(cg.frame.playerState.pmove.pm_flags & PMF_NO_PREDICTION) ) {
			unsigned	delta;
			vec3_t		angles;
			float backlerp = 1.0f - cg.lerpfrac;

			for( i = 0; i < 3; i++ )
				org[i] = cg.predictedOrigin[i] - backlerp * cg.predictionError[i];
			
			// stairs
			delta = cg.realTime - cg.predictedStepTime;
			if( delta < 150 )
				org[2] -= cg.predictedStep * (150 - delta) / 150;
			
			// use refdef view angles on the model
			angles[YAW] = cg.refdef.viewangles[YAW];
			angles[PITCH] = 0;
			angles[ROLL] = 0;
			AnglesToAxis( angles, pmodel->ents[LOWER].axis );
		} else
			VectorCopy (ent->origin, org);
		
		// offset it some units back
		VectorMA( org, -24, pmodel->ents[LOWER].axis[0], org );
		VectorCopy( org, pmodel->ents[LOWER].origin);
		VectorCopy( org, pmodel->ents[LOWER].origin2 );
		VectorCopy( org, pmodel->ents[LOWER].lightingOrigin );
		VectorCopy( org, cg.lightingOrigin );
	} else {
		VectorCopy (ent->origin, pmodel->ents[LOWER].origin);
		VectorCopy (ent->origin2, pmodel->ents[LOWER].origin2);
		VectorCopy (ent->lightingOrigin, pmodel->ents[LOWER].lightingOrigin);
	}

	for( i = LOWER; i < PMODEL_PARTS; i++ ) {
		prev = i - 1;
		pmodel->ents[i].flags = ent->flags;
		pmodel->ents[i].scale = ent->scale;

		// allow player legs to be drawn
		if( cg_showLegs->integer && pmodel->ents[i].flags & RF_VIEWERMODEL && i == LOWER )
			pmodel->ents[i].flags &= ~RF_VIEWERMODEL;

		// don't try moving the first model to tag
		if( i && pmodel->ents[prev].model ) {	
			if( CG_GrabTag( &tag, &pmodel->ents[prev], pmTagNames[prev] ) )
				CG_PlaceRotatedModelOnTag( &pmodel->ents[i], &pmodel->ents[prev], &tag );
		}

		CG_AddEntityToScene( &pmodel->ents[i] );
		if( !pmodel->ents[i].model )	// don't continue without a model
			break;

		CG_AddShellEffects( &pmodel->ents[i], state->effects );
		CG_AddColorShell( &pmodel->ents[i], state->renderfx );
	}

	// always create a projectionSource fallback
	VectorMA( pmodel->ents[UPPER].origin, 24, pmodel->ents[UPPER].axis[2], pmodel->projectionSource.origin );
	VectorMA( pmodel->projectionSource.origin, 16, pmodel->ents[UPPER].axis[0], pmodel->projectionSource.origin );
	Matrix_Copy( pmodel->ents[UPPER].axis, pmodel->projectionSource.axis );

	// add weapon model
	if( state->modelindex2 != 255 || !state->weapon ) // player doesn't have a weapon
		return;

	if( CG_GrabTag( &tag, &pmodel->ents[UPPER], "tag_weapon" ) )
		CG_AddWeaponOnTag( &pmodel->ents[UPPER], &tag, &pmodel->pweapon, 0, &pmodel->projectionSource );
}
#endif // SKELMOD

