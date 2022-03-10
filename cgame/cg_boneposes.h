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
// cg_boneposes.h -- definitions for skeletons

//=============================================================================
//
//							BONEPOSES
//
//=============================================================================

typedef struct
{
	cgs_skeleton_t	*skel;
	bonepose_t		*lerpboneposes;

}cg_centboneposes_t;

cg_centboneposes_t	centBoneposes[MAX_EDICTS];


void CG_AddEntityToScene( entity_t *ent ) ;
struct model_s *CG_RegisterModel( char *name );
cgs_skeleton_t *CG_SkeletonForModel( struct model_s *model );

cgs_skeleton_t *CG_SetBoneposesForTemporaryEntity( entity_t *ent );
cgs_skeleton_t *CG_SetBoneposesForCGEntity( entity_t *ent, centity_t *cent );

void		CG_InitTemporaryBoneposesCache( void );
void		CG_ResetTemporaryBoneposesCache( void );
qboolean	CG_LerpBoneposes( cgs_skeleton_t *skel, bonepose_t *curboneposes, bonepose_t *oldboneposes, bonepose_t *lerpboneposes, float frontlerp );
void		CG_TransformBoneposes( cgs_skeleton_t *skel, bonepose_t *boneposes, bonepose_t *sourceboneposes );
void		CG_RotateBonePose( vec3_t angles, bonepose_t *bonepose );
qboolean	CG_SkeletalPoseGetAttachment( orientation_t *orient, cgs_skeleton_t *skel, bonepose_t *boneposes, const char *bonename );
void		CG_RegisterBoneposesForCGEntity( centity_t *cent, struct model_s *model );


