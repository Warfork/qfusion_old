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
// - Adding the weapon models in split pieces
// by Jalisko


#include "cg_local.h"



weaponinfo_t	cg_pWeaponModelInfos[WEAP_TOTAL];

static char *wmPartSufix[] = { "", "_barrel", "_flash", "_hand", NULL };

/*
===============
CG_vWeap_ParseAnimationScript

script:
0 = first frame
1 = lastframe/number of frames
2 = looping frames
3 = frame time

keywords: 
  "islastframe":Will read the second value of each animation as lastframe (usually means numframes)
  "rotationscale": value witch will scale the barrel rotation speed 
===============
*/
qboolean CG_vWeap_ParseAnimationScript( weaponinfo_t *weaponinfo, char *filename )
{
	qbyte		*buf;
	char		*ptr, *token;
	int			rounder, counter, i;
	qboolean	debug = qtrue;
	qboolean	islastframe = qfalse;
	int			anim_data[4][VWEAP_MAXANIMS];
	int			length, filenum;
	
	rounder = 0;
	counter = 1;	// reseve 0 for 'no animation'
	weaponinfo->rotationscale = 1;

	if( !cg_debugWeaponModels->integer )
		debug = qfalse;

	// load the file
	length = trap_FS_FOpenFile( filename, &filenum, FS_READ );
	if( length == -1 )
		return qfalse;
	if( !length ) {
		trap_FS_FCloseFile( filenum );
		return qfalse;
	}

	buf = CG_Malloc( length + 1 );
	trap_FS_Read( buf, length, filenum );
	trap_FS_FCloseFile( filenum );

	if( !buf ) {
		CG_Free( buf );
		return qfalse;
	}
	
	if( debug )
		CG_Printf( "%sLoading weapon animation script:%s%s\n", S_COLOR_BLUE, filename, S_COLOR_WHITE );

	// proceed
	ptr = ( char * )buf;
	while( ptr ) {
		token = COM_ParseExt( &ptr, qtrue );
		if( !token )
			break;
		
		if( *token < '0' || *token > '9' ) {
			if( !Q_stricmp( token, "islastframe" ) ) {				// islastframe
				islastframe = qtrue;
				if( debug )
					CG_Printf( "%sScript: Second value is read as lastframe%s\n", S_COLOR_BLUE, S_COLOR_WHITE );
			} else if( !Q_stricmp( token, "rotationscale" ) ) {		// rotationscale
				if( debug )
					CG_Printf( "%sScript: rotation scale:%s", S_COLOR_BLUE, S_COLOR_WHITE );
				token = COM_ParseExt( &ptr, qfalse );
				weaponinfo->rotationscale = (float)atoi( token );
				if( debug )
					CG_Printf( "%s%f%s\n", S_COLOR_BLUE, weaponinfo->rotationscale, S_COLOR_WHITE );
			} else if( token[0] && debug )
				CG_Printf( "%signored: %s%s\n", S_COLOR_YELLOW, token, S_COLOR_WHITE );
		} else {													// frame & animation values
			i = (int)atoi( token );
			if( debug ) {
				if( rounder == 0 )
					CG_Printf( "%sScript: %s", S_COLOR_BLUE, S_COLOR_WHITE );
				CG_Printf( "%s%i - %s", S_COLOR_BLUE, i, S_COLOR_WHITE );
			}

			anim_data[rounder][counter] = i;
			rounder++;
			if( rounder > 3 ) {
				rounder = 0;
				if( debug )
					CG_Printf( "%s anim: %i%s\n", S_COLOR_BLUE, counter, S_COLOR_WHITE );
				counter++;
			}
		}
	}
	
	CG_Free( buf );

	if( counter < VWEAP_MAXANIMS ) {
		CG_Printf( "%sERROR: incomplete WEAPON script: %s - Using default%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE );
		return qfalse;
	}

	// reorganize to make my life easier
	for( i = 0; i < VWEAP_MAXANIMS; i++ ) {
		weaponinfo->firstframe[i] = anim_data[0][i];

		if( islastframe )
			weaponinfo->lastframe[i] = anim_data[1][i];
		else
			weaponinfo->lastframe[i] = ((anim_data[0][i]) + (anim_data[1][i]));

		weaponinfo->loopingframes[i] = anim_data[2][i];
		if( anim_data[3][i] < 10 )	// never allow less than 10 fps
			anim_data[3][i] = 10;

		weaponinfo->frametime[i] = 1000/anim_data[3][i];
	}

	return qtrue;
}

/*
===============
CG_LoadHandAnimations
===============
*/
void CG_CreateHandDefaultAnimations( weaponinfo_t *weaponinfo )
{
	int defaultfps = 15;

	weaponinfo->rotationscale = 1;

	// default Q3 hand
	weaponinfo->firstframe[VWEAP_STANDBY] = 0;
	weaponinfo->lastframe[VWEAP_STANDBY] = 0;
	weaponinfo->loopingframes[VWEAP_STANDBY] = 1;
	weaponinfo->frametime[VWEAP_STANDBY] = 1000/defaultfps;

	weaponinfo->firstframe[VWEAP_ATTACK] = 1;
	weaponinfo->lastframe[VWEAP_ATTACK] = 5;
	weaponinfo->loopingframes[VWEAP_ATTACK] = 0;
	weaponinfo->frametime[VWEAP_ATTACK] = 1000/defaultfps;

	weaponinfo->firstframe[VWEAP_ATTACK2_HOLD] = 0;
	weaponinfo->lastframe[VWEAP_ATTACK2_HOLD] = 0;
	weaponinfo->loopingframes[VWEAP_ATTACK2_HOLD] = 1;
	weaponinfo->frametime[VWEAP_ATTACK2_HOLD] = 1000/defaultfps;

	weaponinfo->firstframe[VWEAP_ATTACK2_RELEASE] = 0;
	weaponinfo->lastframe[VWEAP_ATTACK2_RELEASE] = 0;
	weaponinfo->loopingframes[VWEAP_ATTACK2_RELEASE] = 1;
	weaponinfo->frametime[VWEAP_ATTACK2_RELEASE] = 1000/defaultfps;

	weaponinfo->firstframe[VWEAP_RELOAD] = 0;
	weaponinfo->lastframe[VWEAP_RELOAD] = 0;
	weaponinfo->loopingframes[VWEAP_RELOAD] = 1;
	weaponinfo->frametime[VWEAP_RELOAD] = 1000/defaultfps;

	weaponinfo->firstframe[VWEAP_WEAPDOWN] = 6;
	weaponinfo->lastframe[VWEAP_WEAPDOWN] = 10;
	weaponinfo->loopingframes[VWEAP_WEAPDOWN] = 1;
	weaponinfo->frametime[VWEAP_WEAPDOWN] = 1000/defaultfps;

	weaponinfo->firstframe[VWEAP_WEAPONUP] = 11;
	weaponinfo->lastframe[VWEAP_WEAPONUP] = 15;
	weaponinfo->loopingframes[VWEAP_WEAPONUP] = 0;
	weaponinfo->frametime[VWEAP_WEAPONUP] = 1000/defaultfps;
}

/*
===============
CG_BuildProjectionOrigin
 store the orientation_t closer to the tag_flash we can create,
 or create one using an offset we consider acceptable.
 NOTE: This tag will ignore weapon models animations. You'd have to 
 do it in realtime to use it with animations. 
===============
*/
static void CG_BuildProjectionOrigin( weaponinfo_t *weaponinfo )
{
	orientation_t	tag, tag_barrel;
	static entity_t	ent;

	if( !weaponinfo )
		return;

	if( weaponinfo->model[WEAPON] ) {
		
		// assign the model to an entity_t, so we can build boneposes
		memset( &ent, 0, sizeof(ent) );
		ent.rtype = RT_MODEL;
		ent.scale = 1.0f;
		ent.model = weaponinfo->model[WEAPON];
		CG_SetBoneposesForTemporaryEntity( &ent ); // assigns and builds the skeleton so we can use grabtag
		// try getting the tag_flash from the weapon model
		if( CG_GrabTag( &weaponinfo->tag_projectionsource, &ent, "tag_flash" ) )
			return; // succesfully

		// if it didn't work, try getting it from the barrel model
		if( CG_GrabTag( &tag_barrel, &ent, "tag_barrel" ) && weaponinfo->model[BARREL] ) {
			// assign the model to an entity_t, so we can build boneposes
			memset( &ent, 0, sizeof(ent) );
			ent.rtype = RT_MODEL;
			ent.scale = 1.0f;
			ent.model = weaponinfo->model[BARREL];
			CG_SetBoneposesForTemporaryEntity( &ent );
			if( CG_GrabTag( &tag, &ent, "tag_flash" ) && weaponinfo->model[BARREL] ) {
				VectorCopy( vec3_origin, weaponinfo->tag_projectionsource.origin );
				Matrix_Identity( weaponinfo->tag_projectionsource.axis );
				CG_MoveToTag( weaponinfo->tag_projectionsource.origin,
					weaponinfo->tag_projectionsource.axis,
					tag_barrel.origin,
					tag_barrel.axis,
					tag.origin,
					tag.axis );
				return; // succesfully
			}
		}
	} 
	
	// doesn't have a weapon model, or the weapon model doesn't have a tag
	VectorSet( weaponinfo->tag_projectionsource.origin, 16, 0, 8 );
	Matrix_Identity( weaponinfo->tag_projectionsource.axis );
}

/*
===============
CG_WeaponModelUpdateRegistration
===============
*/
qboolean CG_WeaponModelUpdateRegistration( weaponinfo_t *weaponinfo, char *filename )
{
	int 			p;
	char			scratch[MAX_QPATH];

	for( p = 0; p < WEAPMODEL_PARTS; p++ ) {
		if( !weaponinfo->model[p] ) {	// md3	
			Q_snprintfz( scratch, sizeof( scratch ), "models/weapons2/%s%s.md3", filename, wmPartSufix[p] );
			weaponinfo->model[p] = CG_RegisterModel( scratch );
		}

		if( !weaponinfo->model[p] ) {	// skm	
			Q_snprintfz( scratch, sizeof( scratch ), "models/weapons2/%s%s.skm", filename, wmPartSufix[p] );
			weaponinfo->model[p] = CG_RegisterModel( scratch );
		}

		if( !weaponinfo->model[p] ) {	// md2	
			Q_snprintfz( scratch, sizeof( scratch ), "models/weapons2/%s%s.md2", filename, wmPartSufix[p] );
			weaponinfo->model[p] = CG_RegisterModel( scratch );
		}
	}

	// load failed
	if( !weaponinfo->model[HAND] ) {
		weaponinfo->name[0] = 0;
		for( p = 0; p < WEAPMODEL_PARTS; p++ )
			weaponinfo->model[p] = NULL;
		return qfalse;
	}

	// load animation script for the hand model
	Q_snprintfz( scratch, sizeof( scratch ), "models/weapons2/%s.cfg", filename );

	if ( !CG_vWeap_ParseAnimationScript ( weaponinfo, scratch ) )
		CG_CreateHandDefaultAnimations ( weaponinfo );

	// create a tag_projection from tag_flash, to possition fire effects
	CG_BuildProjectionOrigin( weaponinfo );

	if( cg_debugWeaponModels->integer )
		CG_Printf( "%sWEAPmodel: Loaded successful%s\n", S_COLOR_BLUE, S_COLOR_WHITE );

	strcpy( weaponinfo->name, filename );
	return qtrue;
}

/*
===============
CG_FindWeaponModelSpot
===============
*/
struct weaponinfo_s *CG_FindWeaponModelSpot( char *filename )
{
	int 			i;
	int				freespot = -1;

	for( i = 0; i < WEAP_TOTAL; i++ ) {
		if( cg_pWeaponModelInfos[i].inuse == qtrue ) {
			if( !Q_stricmp( cg_pWeaponModelInfos[i].name, filename ) ) { // found it
				if( cg_debugWeaponModels->integer )
					CG_Printf( "WEAPModel: found at spot %i: %s\n", i, filename );

				return &cg_pWeaponModelInfos[i];
			}
		} else if( freespot < 0 )
			freespot = i;
	}

	if( freespot < 0 )
		CG_Error( "%sCG_FindWeaponModelSpot: Couldn't find a free weaponinfo spot%s", S_COLOR_RED, S_COLOR_WHITE );

	// we have a free spot
	if( cg_debugWeaponModels->integer )
		CG_Printf( "WEAPmodel: assigned free spot %i for weaponinfo %s\n", freespot, filename );

	return &cg_pWeaponModelInfos[freespot];
}

/*
===============
CG_RegisterWeaponModel
===============
*/
struct weaponinfo_s *CG_RegisterWeaponModel( char *cgs_name )
{
	char			filename[MAX_QPATH];
	weaponinfo_t	*weaponinfo;

	COM_StripExtension( cgs_name, filename );
	weaponinfo = CG_FindWeaponModelSpot( filename );
	if( weaponinfo->inuse == qtrue )
		return weaponinfo;

	weaponinfo->inuse = CG_WeaponModelUpdateRegistration( weaponinfo, filename );
	if( !weaponinfo->inuse ) {
		if( cg_debugWeaponModels->integer )
			CG_Printf( "%sWEAPmodel: Failed:%s%s\n", S_COLOR_YELLOW, filename, S_COLOR_WHITE );
		return NULL;
	}

	return weaponinfo;
}


/*
===============
CG_CreateWeaponZeroModel

we can't allow NULL weaponmodels to be passed to the viewweapon.
They will produce crashes because the lack of animation script. 
We need to have at least one weaponinfo with a script to be used
as a replacement, so, weapon 0 will have the animation script 
even if the registration failed
===============
*/
struct weaponinfo_s *CG_CreateWeaponZeroModel( char *filename )
{
	weaponinfo_t	*weaponinfo;

	COM_StripExtension( filename, filename );
	weaponinfo = CG_FindWeaponModelSpot( filename );

	if( weaponinfo->inuse == qtrue )
		return weaponinfo;

	if( cg_debugWeaponModels->integer )
		CG_Printf( "%sWEAPmodel: Failed to load generic weapon. Creatin fake one%s\n", S_COLOR_YELLOW, S_COLOR_WHITE );

	CG_CreateHandDefaultAnimations( weaponinfo );
	weaponinfo->inuse = qtrue;
	strcpy( weaponinfo->name, filename );
	return weaponinfo;
}

/*
======================================================================
							weapons
======================================================================
*/

/*
===============
CG_GetWeaponFromClientIndex
===============
*/
struct weaponinfo_s *CG_GetWeaponFromPModelIndex( pmodel_t *pmodel, int currentweapon )
{
	weaponinfo_t	*weaponinfo;

	if( ( !cg_vwep->integer ) || ( currentweapon > WEAP_TOTAL - 1 ) )
		currentweapon = WEAP_NONE;

	weaponinfo = pmodel->weaponIndex[currentweapon];
	if( !weaponinfo )									// we can't allow NULL newweapons to be passed.
		weaponinfo = pmodel->weaponIndex[WEAP_NONE];	// They will produce crashes because the lack of animation script
														// weapon 0 will have the animation script even if the registration failed
	return weaponinfo;
}

/*
===============
CG_AddWeaponOnTag
===============
*/
void CG_AddWeaponOnTag( entity_t *ent, orientation_t *tag, pweapon_t *pweapon, int effects, orientation_t *projectionSource )
{
	entity_t		weapon;
	vec3_t			flashLight;

	if( !ent->model || !pweapon->weaponInfo )
		return;		// don't try without base model
	if( !tag )
		return;		// don't try without a tag

	// weapon
	memset( &weapon, 0, sizeof( weapon ) );
	weapon.scale = ent->scale;
	weapon.flags = ent->flags;
	weapon.frame = 0;
	weapon.oldframe = 0;
	weapon.model = pweapon->weaponInfo->model[WEAPON];
	Vector4Set( weapon.color, 100, 100, 200, 0 );

	CG_PlaceModelOnTag( &weapon, ent, tag );
	CG_AddEntityToScene( &weapon );
	if( !weapon.model )
		return;

	CG_AddShellEffects( &weapon, effects );
	// CG_AddColorShell( &weapon, renderfx );

	// barrel
	if( pweapon->weaponInfo->model[BARREL] ) {
		if( CG_GrabTag( tag, &weapon, "tag_barrel" ) ) {
			entity_t	barrel;
			float		scaledTime;

			memset( &barrel, 0, sizeof( barrel ) );
			barrel.model = pweapon->weaponInfo->model[BARREL];
			barrel.scale = ent->scale;
			barrel.flags = ent->flags;
			barrel.frame = 0;
			barrel.oldframe = 0;

			// rotation
			scaledTime = cg_paused->integer ? 0 : cg.frameTime*100; // not precise, but enough
			pweapon->rotationSpeed += scaledTime * ( ( pweapon->flashtime > cg.time ) * ( pweapon->rotationSpeed < 8 ) );
			pweapon->rotationSpeed -= scaledTime/15;
			if( pweapon->rotationSpeed < 0 )
				pweapon->rotationSpeed = 0.0f;

			pweapon->angles[2] += scaledTime * pweapon->rotationSpeed * pweapon->weaponInfo->rotationscale;
			if( pweapon->angles[2] > 360 )
				pweapon->angles[2] -= 360;

			AnglesToAxis( pweapon->angles, barrel.axis );
			CG_PlaceRotatedModelOnTag( &barrel, &weapon, tag );
			CG_AddEntityToScene( &barrel );

			CG_AddShellEffects( &barrel, effects );
			//CG_AddColorShell( &barrel, renderfx );
		}
	}

	// update projection source
	if( projectionSource != NULL ) {
		VectorCopy( vec3_origin, projectionSource->origin );
		Matrix_Copy( axis_identity, projectionSource->axis );
		CG_MoveToTag( projectionSource->origin, projectionSource->axis,
			weapon.origin, weapon.axis,
			pweapon->weaponInfo->tag_projectionsource.origin,
			pweapon->weaponInfo->tag_projectionsource.axis );
	}

	if( pweapon->flashtime < cg.time )
	{
		pweapon->flashradius = 0;
		return;
	}

	VectorCopy( weapon.origin, flashLight );

	// flash
	if( CG_GrabTag( tag, &weapon, "tag_flash" ) ) {
		if( pweapon->weaponInfo->model[FLASH] ) {
			entity_t	flash;
			memset( &flash, 0, sizeof( flash ) );
			flash.model = pweapon->weaponInfo->model[FLASH];
			flash.scale = ent->scale;
			flash.flags = ent->flags | RF_NOSHADOW;
			flash.frame = 0;
			flash.oldframe = 0;

			CG_PlaceModelOnTag( &flash, &weapon, tag );

			CG_AddEntityToScene( &flash );

			VectorCopy( flash.origin, flashLight );
		}
	}

	if( pweapon->flashradius ) {
		// spawn light if not cleared
		CG_AllocDlight( pweapon->flashradius, flashLight, pweapon->flashcolor );
		pweapon->flashradius = 0;
	}
}
