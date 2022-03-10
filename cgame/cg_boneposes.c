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

#include "cg_local.h"

//========================================================================
//
//				SKELETONS
//
//========================================================================

cgs_skeleton_t *skel_headnode;

/*
=================
CG_SkeletonForModel
=================
*/
cgs_skeleton_t *CG_SkeletonForModel( struct model_s *model )
{
	int i, j;
	cgs_skeleton_t *skel;
	qbyte *buffer;
	cgs_bone_t *bone;
	bonepose_t *bonePose;
	int numBones, numFrames;

	if( !model )
		return NULL;

	numBones = trap_R_SkeletalGetNumBones( model, &numFrames );
	if( !numBones || !numFrames )
		return NULL;		// no bones or frames

	for( skel = skel_headnode; skel; skel = skel->next ) {
		if( skel->model == model )
			return skel;
	}

	// allocate one huge array to hold our data
	buffer = CG_Malloc( sizeof( cgs_skeleton_t ) + numBones * sizeof( cgs_bone_t ) +
		numFrames * (sizeof( bonepose_t * ) + numBones * sizeof( bonepose_t )) );

	skel = ( cgs_skeleton_t * )buffer; buffer += sizeof( cgs_skeleton_t );
	skel->bones = ( cgs_bone_t * )buffer; buffer += numBones * sizeof( cgs_bone_t );
	skel->numBones = numBones;
	skel->bonePoses = ( bonepose_t ** )buffer; buffer += numFrames * sizeof( bonepose_t * );
	skel->numFrames = numFrames;
	// register bones
	for( i = 0, bone = skel->bones; i < numBones; i++, bone++ )
		bone->parent = trap_R_SkeletalGetBoneInfo( model, i, bone->name, sizeof( bone->name ), &bone->flags );

	// register poses for all frames for all bones
	for( i = 0; i < numFrames; i++ ) {
		skel->bonePoses[i] = ( bonepose_t * )buffer; buffer += numBones * sizeof( bonepose_t );
		for( j = 0, bonePose = skel->bonePoses[i]; j < numBones; j++, bonePose++ )
			trap_R_SkeletalGetBonePose( model, j, i, bonePose );
	}

	skel->next = skel_headnode;
	skel_headnode = skel;

	skel->model = model;

	return skel;
}

/*
==================
CG_RegisterModel
Register model and skeleton (if any)
==================
*/
struct model_s *CG_RegisterModel( char *name )
{
	struct model_s	*model;

	model = trap_R_RegisterModel( name );
	// precache bones
	if( trap_R_SkeletalGetNumBones( model, NULL ) )
		CG_SkeletonForModel( model );

	return model;
}

/*
==================
CG_AddEntityToScene
Check for boneposes sanity before adding the entity to the scene
Using this one instead of trap_R_AddEntityToScene is
recommended under any circunstance
==================
*/
void CG_AddEntityToScene( entity_t *ent ) 
{
	if( cg_outlineModels->integer )
	{
		if( ent->flags & RF_WEAPONMODEL )
			ent->outlineHeight = 0.1;
		else
			ent->outlineHeight = 0.5;
	}
	else
	{
		ent->outlineHeight = 0;
	}

	if( ent->model && trap_R_SkeletalGetNumBones( ent->model, NULL ) ) {
		if( !ent->boneposes || !ent->oldboneposes )
			CG_SetBoneposesForTemporaryEntity( ent );
	}

	trap_R_AddEntityToScene( ent );
}


//========================================================================
//
//				BONEPOSES
//
//========================================================================

/*
===============
CG_TransformBoneposes
place bones in it's final position in the skeleton
===============
*/
void CG_TransformBoneposes( cgs_skeleton_t *skel, bonepose_t *outboneposes, bonepose_t *sourceboneposes )
{
	int				j;
	bonepose_t	temppose;

	for( j = 0; j < (int)skel->numBones; j++ ) {
		if( skel->bones[j].parent >= 0 ) {
			memcpy( &temppose, &sourceboneposes[j], sizeof(bonepose_t));
			Quat_ConcatTransforms ( outboneposes[skel->bones[j].parent].quat, outboneposes[skel->bones[j].parent].origin, temppose.quat, temppose.origin, outboneposes[j].quat, outboneposes[j].origin );
		} else
			memcpy( &outboneposes[j], &sourceboneposes[j], sizeof(bonepose_t));	
	}
}

/*
==============================
CG_LerpBoneposes
Get the interpolated pose from current and old poses. It doesn't
matter if they were transformed before or not
==============================
*/
qboolean CG_LerpBoneposes( cgs_skeleton_t *skel, bonepose_t *curboneposes, bonepose_t *oldboneposes, bonepose_t *lerpboneposes, float frontlerp )
{
	int			i;
	bonepose_t	*curbonepose, *oldbonepose, *lerpbpose;

	for( i = 0; i < (int)skel->numBones; i++ ) {
		curbonepose = curboneposes + i;
		oldbonepose = oldboneposes + i;
		lerpbpose = lerpboneposes + i;

		Quat_Lerp( oldbonepose->quat, curbonepose->quat, frontlerp, lerpbpose->quat );
		lerpbpose->origin[0] = oldbonepose->origin[0] + (curbonepose->origin[0] - oldbonepose->origin[0]) * frontlerp;
		lerpbpose->origin[1] = oldbonepose->origin[1] + (curbonepose->origin[1] - oldbonepose->origin[1]) * frontlerp;
		lerpbpose->origin[2] = oldbonepose->origin[2] + (curbonepose->origin[2] - oldbonepose->origin[2]) * frontlerp;
	}

	return qtrue;
}

/*
==============================
CG_RotateBonePose
==============================
*/
void CG_RotateBonePose( vec3_t angles, bonepose_t *bonepose )
{
	vec3_t		axis_rotator[3];
	quat_t		quat_rotator;
	bonepose_t	temppose;
	vec3_t		tempangles;

	tempangles[0] = -angles[YAW];
	tempangles[1] = -angles[PITCH];
	tempangles[2] = -angles[ROLL];
	AnglesToAxis( tempangles, axis_rotator );
	Matrix_Quat( axis_rotator, quat_rotator );
	memcpy( &temppose, bonepose, sizeof(bonepose_t) );
	Quat_ConcatTransforms( quat_rotator, vec3_origin, temppose.quat, temppose.origin, bonepose->quat, bonepose->origin );
}

/*
==============================
CG_SkeletalPoseGetAttachment
Get the tag from the finished (lerped and transformed) pose
==============================
*/
qboolean CG_SkeletalPoseGetAttachment( orientation_t *orient, cgs_skeleton_t *skel, bonepose_t *boneposes, const char *bonename )
{
	int			i;
	quat_t		quat;
	cgs_bone_t	*bone;
	bonepose_t	*bonepose;

	if( !boneposes || !skel ) {
		CG_Printf( "CG_SkeletalPoseLerpAttachment: Wrong model or boneposes %s\n", bonename );
		return qfalse;
	}

	// find the appropriate attachment bone
	bone = skel->bones;
	for( i = 0; i < skel->numBones; i++, bone++ ) {
		if( !Q_stricmp( bone->name, bonename ) )
			break;
	}

	if( i == skel->numBones ) {
		CG_Printf( "CG_SkeletalPoseLerpAttachment: no such bone %s\n", bonename );
		return qfalse;
	}

	//get the desired bone
	bonepose = boneposes + i;

	//copy the inverted bone into the tag
	Quat_Conjugate( bonepose->quat, quat );	//inverse the tag direction
	Quat_Matrix( quat, orient->axis );
	orient->origin[0] = bonepose->origin[0];
	orient->origin[1] = bonepose->origin[1];
	orient->origin[2] = bonepose->origin[2];

	return qtrue;
}

/*
//==============================
CG_SkeletalPoseLerpAttachment
Interpolate and return bone from TRANSFORMED bonepose and oldbonepose
(not used. I let it here as reference)
//==============================
*/
qboolean CG_SkeletalPoseLerpAttachment( orientation_t *orient, cgs_skeleton_t *skel, bonepose_t *boneposes, bonepose_t *oldboneposes, float backlerp, char *bonename )
{
	int			i;
	quat_t		quat;
	cgs_bone_t	*bone;
	bonepose_t	*bonepose, *oldbonepose;
	float frontlerp = 1.0 - backlerp;

	if( !boneposes || !oldboneposes || !skel ) {
		CG_Printf( "CG_SkeletalPoseLerpAttachment: Wrong model or boneposes %s\n", bonename );
		return qfalse;
	}

	// find the appropriate attachment bone
	bone = skel->bones;
	for( i = 0; i < skel->numBones; i++, bone++ ) {
		if( !Q_stricmp( bone->name, bonename ) )
			break;
	}

	if( i == skel->numBones ) {
		CG_Printf( "CG_SkeletalPoseLerpAttachment: no such bone %s\n", bonename );
		return qfalse;
	}

	//get the desired bone
	bonepose = boneposes + i;
	oldbonepose = oldboneposes + i;

	// lerp
	Quat_Lerp( oldbonepose->quat, bonepose->quat, frontlerp, quat );
	Quat_Conjugate( quat, quat );	//inverse the tag direction
	Quat_Matrix( quat, orient->axis );
	orient->origin[0] = oldbonepose->origin[0] + (bonepose->origin[0] - oldbonepose->origin[0]) * frontlerp;
	orient->origin[1] = oldbonepose->origin[1] + (bonepose->origin[1] - oldbonepose->origin[1]) * frontlerp;
	orient->origin[2] = oldbonepose->origin[2] + (bonepose->origin[2] - oldbonepose->origin[2]) * frontlerp;

	return qtrue;
}

/*
==============================
CG_SkeletalUntransformedPoseLerpAttachment
Build both old and new frame poses, interpolate them, and return the tag bone inverted
(Slow. not used. I let it here as reference)
//==============================
*/
qboolean CG_SkeletalUntransformedPoseLerpAttachment( orientation_t *orient, cgs_skeleton_t *skel, bonepose_t *boneposes, bonepose_t *oldboneposes, float backlerp, char *bonename )
{
	int			i;
	quat_t		quat;
	cgs_bone_t	*bone;
	bonepose_t	*bonepose, *oldbonepose;
	bonepose_t *tr_boneposes, *tr_oldboneposes;
	quat_t		oldbonequat, bonequat;
	float frontlerp = 1.0 - backlerp;

	if( !boneposes || !oldboneposes || !skel ) {
		CG_Printf( "CG_SkeletalPoseLerpAttachment: Wrong model or boneposes %s\n", bonename );
		return qfalse;
	}

	// find the appropriate attachment bone
	bone = skel->bones;
	for( i = 0; i < skel->numBones; i++, bone++ ) {
		if( !Q_stricmp( bone->name, bonename ) ){
			break;
		}
	}

	if( i == skel->numBones ) {
		CG_Printf( "CG_SkeletalPoseLerpAttachment: no such bone %s\n", bonename );
		return qfalse;
	}

	//alloc new space for them: fixme: should be using a cache
	tr_boneposes = CG_Malloc ( sizeof(bonepose_t) * skel->numBones );
	CG_TransformBoneposes( skel, tr_boneposes, boneposes );
	tr_oldboneposes = CG_Malloc ( sizeof(bonepose_t) * skel->numBones );
	CG_TransformBoneposes( skel, tr_oldboneposes, oldboneposes );
	
	bonepose = tr_boneposes + i;
	oldbonepose = tr_oldboneposes + i;

	//inverse the direction
	Quat_Conjugate( oldbonepose->quat, oldbonequat );
	Quat_Conjugate( bonepose->quat, bonequat );

	// interpolate quaternions and origin
	Quat_Lerp( oldbonequat, bonequat, frontlerp, quat );
	Quat_Matrix( quat, orient->axis );
	orient->origin[0] = oldbonepose->origin[0] + (bonepose->origin[0] - oldbonepose->origin[0]) * frontlerp;
	orient->origin[1] = oldbonepose->origin[1] + (bonepose->origin[1] - oldbonepose->origin[1]) * frontlerp;
	orient->origin[2] = oldbonepose->origin[2] + (bonepose->origin[2] - oldbonepose->origin[2]) * frontlerp;

	CG_Free(tr_boneposes);
	CG_Free(tr_oldboneposes);

	return qtrue;
}



//========================================================================
//
//		TMP BONEPOSES
//
//========================================================================

#define TBC_Block_Size		1024
static int TBC_Size;

bonepose_t *TBC;		//Temporary Boneposes Cache
static int	TBC_Count;

/*
===============
CG_InitTemporaryBoneposesCache
===============
*/
void CG_InitTemporaryBoneposesCache( void )
{
	TBC_Size = TBC_Block_Size;
	TBC = CG_Malloc ( sizeof(bonepose_t) * TBC_Size );
	TBC_Count = 0;
}

/*
===============
CG_ExpandTemporaryBoneposesCache
===============
*/
void CG_ExpandTemporaryBoneposesCache( void )
{
	bonepose_t *temp;

	temp = TBC;

	TBC = CG_Malloc ( sizeof(bonepose_t) * (TBC_Size + TBC_Block_Size) );
	memcpy( TBC, temp, sizeof(bonepose_t) * TBC_Size );
	TBC_Size += TBC_Block_Size;

	CG_Free( temp );
}

/*
===============
CG_ResetTemporaryBoneposesCache
These boneposes are REMOVED EACH FRAME after drawing.
===============
*/
void CG_ResetTemporaryBoneposesCache( void )
{
	TBC_Count = 0;
}

/*
===============
CG_RegisterTemporaryExternalBoneposes
These boneposes are REMOVED EACH FRAME after drawing. Register
here only in the case you create an entity which is not cg_entity.
===============
*/
bonepose_t *CG_RegisterTemporaryExternalBoneposes( cgs_skeleton_t *skel, bonepose_t *poses )
{
	bonepose_t	*boneposes;
	if( (TBC_Count + skel->numBones) > TBC_Size )
		CG_ExpandTemporaryBoneposesCache();

	boneposes = &TBC[TBC_Count];
	TBC_Count += skel->numBones;

	return boneposes;
}

/*
===============
CG_SetBoneposesForTemporaryEntity
Sets up skeleton with inline boneposes based on frame/oldframe values
These boneposes will be REMOVED EACH FRAME. Use only for temporary entities,
cg_entities have a persistant registration method available.
===============
*/
cgs_skeleton_t *CG_SetBoneposesForTemporaryEntity( entity_t *ent )
{
	cgs_skeleton_t	*skel;

	skel = CG_SkeletonForModel( ent->model );
	if( skel ) {
		//get space in cache, lerp, transform, link
		ent->boneposes = CG_RegisterTemporaryExternalBoneposes( skel, ent->boneposes );
		CG_LerpBoneposes( skel, skel->bonePoses[ent->frame], skel->bonePoses[ent->oldframe], ent->boneposes, 1.0 - ent->backlerp );
		CG_TransformBoneposes( skel, ent->boneposes, ent->boneposes );
		ent->oldboneposes = ent->boneposes;
	}
	return skel;
}



//========================================================================
//
//		CG_ENTITIES BONEPOSES
//
//========================================================================

/*
===============
CG_RegisterBoneposesForCGEntity
Sets up a special boneposes cache for the cg_entity_t
===============
*/
void CG_RegisterBoneposesForCGEntity( centity_t *cent, struct model_s *model )
{
	cgs_skeleton_t		*skel;
	cg_centboneposes_t	*cb;

	skel = CG_SkeletonForModel( model );
	cb = &centBoneposes[cent->current.number];
	if( skel == cb->skel )
		return;

	if( !skel ) {
		if( cb->skel ) {
			if( cb->lerpboneposes ) {
				CG_Free( cb->lerpboneposes );
				cb->lerpboneposes = NULL;
			}

			cb->skel = NULL;
		}
		return;
	}

	if( !cb->skel || cb->skel && cb->skel->numBones != skel->numBones ) {
		if( cb->lerpboneposes )
			CG_Free( cb->lerpboneposes );

		cb->lerpboneposes = CG_Malloc ( sizeof(bonepose_t) * skel->numBones );
	}

	cb->skel = skel;
}

/*
===============
CG_SetBoneposesForCGEntity
Sets up skeleton with inline lerped pose, ready to be drawn
===============
*/
cgs_skeleton_t *CG_SetBoneposesForCGEntity( entity_t *ent, centity_t *cent )
{
	cgs_skeleton_t		*skel;
	cg_centboneposes_t	*cb;

	skel = CG_SkeletonForModel( ent->model );
	if( !skel ) 
		return NULL;

	//find the cg_entity boneposes
	cb = &centBoneposes[cent->current.number];
	if( skel != cb->skel )
		CG_RegisterBoneposesForCGEntity( cent, ent->model );

	//lerp, transform, link
	CG_LerpBoneposes( cb->skel, skel->bonePoses[ent->frame], skel->bonePoses[ent->oldframe], cb->lerpboneposes, 1.0 - ent->backlerp );
	CG_TransformBoneposes( cb->skel, cb->lerpboneposes, cb->lerpboneposes );
	ent->boneposes = ent->oldboneposes = cb->lerpboneposes;

	return skel;
}


