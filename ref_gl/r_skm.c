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
// r_skm.c: skeletal animation model format

#include "r_local.h"

static  mesh_t	skm_mesh;

static	vec3_t	skm_mins;
static	vec3_t	skm_maxs;
static	float	skm_radius;

/*
==============================================================================

SKM MODELS

==============================================================================
*/

/*
=================
Mod_LoadSkeletalPose
=================
*/
void Mod_LoadSkeletalPose( char *name, model_t *mod, void *buffer )
{
	int				i, j, k;
	mskmodel_t		*poutmodel;
	dskpheader_t	*pinmodel;
	dskpbone_t		*pinbone;
	mskbone_t		*poutbone;
	dskpframe_t		*pinframe;
	mskframe_t		*poutframe;
	dskpbonepose_t	*pinbonepose;
	bonepose_t		*poutbonepose;

	if( strncmp( (const char *)buffer, SKMHEADER, 4 ) )
		Com_Error( ERR_DROP, "uknown fileid for %s", name );

	pinmodel = ( dskpheader_t * )buffer;
	poutmodel = mod->skmodel;

	if( LittleLong( pinmodel->type ) != SKM_MODELTYPE )
		Com_Error( ERR_DROP, "%s has wrong type number (%i should be %i)",
				 name, LittleLong (pinmodel->type), SKM_MODELTYPE );
	if( LittleLong( pinmodel->filesize ) > SKM_MAX_FILESIZE )
		Com_Error( ERR_DROP, "%s has has wrong filesize (%i should be less than %i)",
				 name, LittleLong (pinmodel->filesize), SKM_MAX_FILESIZE );
	if( LittleLong( pinmodel->num_bones ) != poutmodel->numbones )
		Com_Error( ERR_DROP, "%s has has wrong number of bones (%i should be less than %i)",
				 name, LittleLong (pinmodel->num_bones), poutmodel->numbones );

	poutmodel->numframes = LittleLong ( pinmodel->num_frames );
	if( poutmodel->numframes <= 0 )
		Com_Error( ERR_DROP, "%s has no frames", name );
	else if ( poutmodel->numframes > SKM_MAX_FRAMES )
		Com_Error( ERR_DROP, "%s has too many frames", name );

	pinbone = ( dskpbone_t * )( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_bones ) );
	poutbone = poutmodel->bones = Mod_Malloc ( mod, sizeof( mskbone_t ) * poutmodel->numbones );

	for( i = 0; i < poutmodel->numbones; i++, pinbone++, poutbone++ ) {
		Q_strncpyz ( poutbone->name, pinbone->name, SKM_MAX_NAME );
		poutbone->flags = LittleLong( pinbone->flags );
		poutbone->parent = LittleLong( pinbone->parent );
	}

	pinframe = ( dskpframe_t * )( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_frames ) );
	poutframe = poutmodel->frames = Mod_Malloc( mod, sizeof( mskframe_t ) * poutmodel->numframes );

	for( i = 0; i < poutmodel->numframes; i++, pinframe++, poutframe++ ) {
		pinbonepose = ( dskpbonepose_t * )( ( qbyte * )pinmodel + LittleLong( pinframe->ofs_bonepositions ) );
		poutbonepose = poutframe->boneposes = Mod_Malloc ( mod, sizeof( bonepose_t ) * poutmodel->numbones );

		for( j = 0; j < poutmodel->numbones; j++, pinbonepose++, poutbonepose++ ) {
			for( k = 0; k < 4; k++ )
				poutbonepose->quat[k] = LittleFloat( pinbonepose->quat[k] );
			for( k = 0; k < 3; k++ )
				poutbonepose->origin[k] = LittleFloat( pinbonepose->origin[k] );
		}
	}
}

/*
=================
Mod_LoadSkeletalModel
=================
*/
void Mod_LoadSkeletalModel( model_t *mod, model_t *parent, void *buffer )
{
	int				i, j, k, l, m;
	dskmheader_t	*pinmodel;
	mskmodel_t		*poutmodel;
	dskmmesh_t		*pinmesh;
	mskmesh_t		*poutmesh;
	dskmvertex_t	*pinskmvert;
	mskvertex_t		*poutskmvert;
	dskmbonevert_t	*pinbonevert;
	mskbonevert_t	*poutbonevert;
	dskmcoord_t		*pinstcoord;
	vec2_t			*poutstcoord;
	index_t			*pintris, *pouttris;
	mskframe_t		*poutframe;
	unsigned int	*pinreferences, *poutreferences;

	pinmodel = ( dskmheader_t * )buffer;

	if( LittleLong( pinmodel->type ) != SKM_MODELTYPE )
		Com_Error( ERR_DROP, "%s has wrong type number (%i should be %i)",
				 mod->name, LittleLong (pinmodel->type), SKM_MODELTYPE );
	if( LittleLong( pinmodel->filesize ) > SKM_MAX_FILESIZE )
		Com_Error( ERR_DROP, "%s has has wrong filesize (%i should be less than %i)",
				 mod->name, LittleLong (pinmodel->filesize), SKM_MAX_FILESIZE );

	poutmodel = mod->skmodel = Mod_Malloc( mod, sizeof( mskmodel_t ) );
	poutmodel->nummeshes = LittleLong( pinmodel->num_meshes );
	if( poutmodel->nummeshes <= 0 )
		Com_Error( ERR_DROP, "%s has no meshes", mod->name );
	else if( poutmodel->nummeshes > SKM_MAX_MESHES )
		Com_Error( ERR_DROP, "%s has too many meshes", mod->name );

	poutmodel->numbones = LittleLong( pinmodel->num_bones );
	if( poutmodel->numbones <= 0 )
		Com_Error( ERR_DROP, "%s has no bones", mod->name );
	else if( poutmodel->numbones > SKM_MAX_BONES )
		Com_Error( ERR_DROP, "%s has too many bones", mod->name );

	pinmesh = ( dskmmesh_t * )( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_meshes ) );
	poutmesh = poutmodel->meshes = Mod_Malloc( mod, sizeof( mskmesh_t ) * poutmodel->nummeshes );

	for( i = 0; i < poutmodel->nummeshes; i++, pinmesh++, poutmesh++ ) {
		poutmesh->numverts = LittleLong( pinmesh->num_verts );
		if( poutmesh->numverts <= 0 )
			Com_Error( ERR_DROP, "mesh %i in model %s has no vertexes", i, mod->name );
		else if( poutmesh->numverts > SKM_MAX_VERTS )
			Com_Error( ERR_DROP, "mesh %i in model %s has too many vertexes", i, mod->name );

		poutmesh->numtris = LittleLong( pinmesh->num_tris );
		if( poutmesh->numtris <= 0 )
			Com_Error( ERR_DROP, "mesh %i in model %s has no indices", i, mod->name );
		else if( poutmesh->numtris > SKM_MAX_TRIS )
			Com_Error( ERR_DROP, "mesh %i in model %s has too many indices", i, mod->name );

		poutmesh->numreferences = LittleLong( pinmesh->num_references );
		if( poutmesh->numreferences <= 0 )
			Com_Error( ERR_DROP, "mesh %i in model %s has no bone references", i, mod->name );
		else if( poutmesh->numreferences > SKM_MAX_BONES )
			Com_Error( ERR_DROP, "mesh %i in model %s has too many bone references", i, mod->name );

		Q_strncpyz( poutmesh->name, pinmesh->meshname, sizeof( poutmesh->name ) );
		Mod_StripLODSuffix( poutmesh->name );

		poutmesh->skin.shader = R_RegisterSkin( pinmesh->shadername );

		pinskmvert = ( dskmvertex_t * )( ( qbyte * )pinmodel + LittleLong( pinmesh->ofs_verts ) );
		poutskmvert = poutmesh->vertexes = Mod_Malloc( mod, sizeof( mskvertex_t ) * poutmesh->numverts );

		for( j = 0; j < poutmesh->numverts; j++, poutskmvert++ ) {
			poutskmvert->numbones = LittleLong( pinskmvert->numbones );

			pinbonevert = ( dskmbonevert_t * )( ( qbyte * )pinskmvert + sizeof( poutskmvert->numbones ) );
			poutbonevert = poutskmvert->verts = Mod_Malloc( mod, sizeof( mskbonevert_t ) * poutskmvert->numbones );

			for( l = 0; l < poutskmvert->numbones; l++, pinbonevert++, poutbonevert++ ) {
				for( k = 0; k < 3; k++ ) {
					poutbonevert->origin[k] = LittleFloat( pinbonevert->origin[k] );
					poutbonevert->normal[k] = LittleFloat( pinbonevert->normal[k] );
				}

				poutbonevert->influence = LittleFloat( pinbonevert->influence );
				poutbonevert->bonenum = LittleLong( pinbonevert->bonenum );
			}

			pinskmvert = ( dskmvertex_t * )( ( qbyte * )pinbonevert );
		}

		pinstcoord = ( dskmcoord_t * )( ( qbyte * )pinmodel + LittleLong( pinmesh->ofs_texcoords ) );
		poutstcoord = poutmesh->stcoords = Mod_Malloc( mod, poutmesh->numverts * sizeof(vec2_t) );

		for( j = 0; j < poutmesh->numverts; j++, pinstcoord++ ) {
			poutstcoord[j][0] = LittleFloat( pinstcoord->st[0] );
			poutstcoord[j][1] = LittleFloat( pinstcoord->st[1] );
		}

		pintris = ( index_t * )( ( qbyte * )pinmodel + LittleLong( pinmesh->ofs_indices ) );
		pouttris = poutmesh->indexes = Mod_Malloc( mod, sizeof( index_t ) * poutmesh->numtris * 3 );

		for( j = 0; j < poutmesh->numtris; j++, pintris += 3, pouttris += 3 ) {
			pouttris[0] = (index_t)LittleLong( pintris[0] );
			pouttris[1] = (index_t)LittleLong( pintris[1] );
			pouttris[2] = (index_t)LittleLong( pintris[2] );
		}

		pinreferences = ( index_t *)( ( qbyte * )pinmodel + LittleLong( pinmesh->ofs_references ) );
		poutreferences = poutmesh->references = Mod_Malloc( mod, sizeof( unsigned int ) * poutmesh->numreferences );

		for( j = 0; j < poutmesh->numreferences; j++, pinreferences++, poutreferences++ )
			*poutreferences = LittleLong( *pinreferences );

	//
	// build triangle neighbors
	//
#if SHADOW_VOLUMES
		poutmesh->trneighbors = Mod_Malloc( mod, sizeof(int) * poutmesh->numtris * 3 );
		R_BuildTriangleNeighbors( poutmesh->trneighbors, poutmesh->indexes, poutmesh->numtris );
#endif
	}

	// if we have a parent model then we are a LOD file and should use parent model's pose data
	if( parent ) {
		if( !parent->skmodel )
			Com_Error ( ERR_DROP, "%s is not a LOD model for %s",
					 mod->name, parent->name );
		if( poutmodel->numbones != parent->skmodel->numbones )
			Com_Error ( ERR_DROP, "%s has has wrong number of bones (%i should be less than %i)",
					 mod->name, poutmodel->numbones, parent->skmodel->numbones );
		poutmodel->bones = parent->skmodel->bones;
		poutmodel->frames = parent->skmodel->frames;
		poutmodel->numframes = parent->skmodel->numframes;
	} else {		// load a config file
		qbyte *buf;
		char temp[MAX_QPATH];
		char poseName[MAX_QPATH], configName[MAX_QPATH];

		COM_StripExtension( mod->name, temp );
		Q_snprintfz( configName, sizeof(configName), "%s.cfg", temp );

		memset( poseName, 0, sizeof(poseName) );

		FS_LoadFile( configName, (void **)&buf );
		if( !buf ) {
			Q_snprintfz( poseName, sizeof(poseName), "%s.skp", temp );
		} else {
			char *ptr, *token;

			ptr = ( char * )buf;
			while( ptr ) {
				token = COM_ParseExt( &ptr, qtrue );
				if( !token )
					break;

				if( !Q_stricmp( token, "import" ) ) {
					token = COM_ParseExt( &ptr, qfalse );
					COM_StripExtension( token, temp );
					Q_snprintfz( poseName, sizeof(poseName), "%s.skp", temp );
					break;
				}
			}

			FS_FreeFile( buf );
		}

		FS_LoadFile( poseName, (void **)&buf );
		if( !buf )
			Com_Error( ERR_DROP, "Could not find pose file for %s", mod->name );

		Mod_LoadSkeletalPose( poseName, mod, buf );
	}

	// recalculate frame and model bounds according to pose data
	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	poutframe = poutmodel->frames;
	for( i = 0; i < poutmodel->numframes; i++, poutframe++ ) {
		vec3_t		v, vtemp;
		bonepose_t	*poutbonepose;

		ClearBounds( poutframe->mins, poutframe->maxs );

		poutmesh = poutmodel->meshes;
		for( m = 0; m < poutmodel->nummeshes; m++, poutmesh++ ) {
			poutskmvert = poutmesh->vertexes;

			for( j = 0; j < poutmesh->numverts; j++, poutskmvert++ ) {
				VectorClear( v );

				poutbonevert = poutskmvert->verts;
				for( l = 0; l < poutskmvert->numbones; l++, poutbonevert++ ) {
					poutbonepose = poutframe->boneposes + poutbonevert->bonenum;
					Quat_TransformVector( poutbonepose->quat, poutbonevert->origin, vtemp );
					VectorMA( vtemp, poutbonevert->influence, poutbonepose->origin, vtemp );
					v[0] += vtemp[0]; v[1] += vtemp[1]; v[2] += vtemp[2];
				}

				AddPointToBounds( v, poutframe->mins, poutframe->maxs );
			}
		}

		// enlarge the bounding box a bit so it can be rendered
		// with external boneposes
		for( j = 0; j < 3; j++ ) {
			poutframe->mins[j] *= 1.5;
			poutframe->maxs[j] *= 1.5;
		}

		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );
		VectorAdd( poutframe->mins, mod->mins, mod->maxs );
		VectorAdd( poutframe->maxs, mod->mins, mod->maxs );
		mod->radius = max( mod->radius, poutframe->radius );
	}

	mod->type = mod_skeletal;
}

/*
================
R_SkeletalGetNumBones
================
*/
int R_SkeletalGetNumBones( model_t *mod, int *numFrames )
{
	if( !mod || mod->type != mod_skeletal )
		return 0;

	if( numFrames )
		*numFrames = mod->skmodel->numframes;
	return mod->skmodel->numbones;
}

/*
================
R_SkeletalGetBoneInfo
================
*/
int R_SkeletalGetBoneInfo( model_t *mod, int bonenum, char *name, int size, int *flags )
{
	mskbone_t *bone;

	if( !mod || mod->type != mod_skeletal )
		return 0;
	if( bonenum < 0 || bonenum >= mod->skmodel->numbones )
		Com_Error( ERR_DROP, "R_SkeletalGetBone: bad bone number" );

	bone = &mod->skmodel->bones[bonenum];
	if( name && size )
		Q_strncpyz( name, bone->name, size );
	if( flags )
		*flags = bone->flags;
	return bone->parent;
}

/*
================
R_SkeletalGetBonePose
================
*/
void R_SkeletalGetBonePose( model_t *mod, int bonenum, int frame, bonepose_t *bonepose )
{
	if( !mod || mod->type != mod_skeletal )
		return;
	if( bonenum < 0 || bonenum >= mod->skmodel->numbones )
		Com_Error( ERR_DROP, "R_SkeletalGetBonePose: bad bone number" );
	if( frame < 0 || frame >= mod->skmodel->numframes )
		Com_Error( ERR_DROP, "R_SkeletalGetBonePose: bad frame number" );

	if( bonepose )
		*bonepose = mod->skmodel->frames[frame].boneposes[bonenum];
}

/*
================
R_SkeletalModelLODForDistance
================
*/
static model_t *R_SkeletalModelLODForDistance( entity_t *e )
{
	int lod;
	float dist;

	if( !e->model->numlods || (e->flags & RF_FORCENOLOD) )
		return e->model;

	dist = Distance( e->origin, r_origin );
	dist *= tan( r_refdef.fov_x * (M_PI/180) * 0.5f );

	lod = (int)(dist / e->model->radius);
	if (r_lodscale->integer)
		lod /= r_lodscale->integer;
	lod += r_lodbias->integer;

	if( lod < 1 )
		return e->model;
	return e->model->lods[min(lod, e->model->numlods)-1];
}

/*
================
R_SkeletalModelBBox
================
*/
void R_SkeletalModelBBox( entity_t *e, model_t *mod )
{
	int			i;
	mskframe_t	*pframe, *poldframe;
	float		*thismins, *oldmins, *thismaxs, *oldmaxs;
	mskmodel_t	*skmodel = mod->skmodel;

	if( ( e->frame >= skmodel->numframes ) || ( e->frame < 0 ) ) {
		Com_DPrintf( "R_SkeletalModelBBox %s: no such frame %d\n", mod->name, e->frame );
		e->frame = 0;
	}
	if( ( e->oldframe >= skmodel->numframes ) || ( e->oldframe < 0 ) ) {
		Com_DPrintf( "R_SkeletalModelBBox %s: no such oldframe %d\n", mod->name, e->oldframe );
		e->oldframe = 0;
	}

	pframe = skmodel->frames + e->frame;
	poldframe = skmodel->frames + e->oldframe;

	// compute axially aligned mins and maxs
	if( pframe == poldframe ) {
		VectorCopy( pframe->mins, skm_mins );
		VectorCopy( pframe->maxs, skm_maxs );
		skm_radius = pframe->radius;
	} else {
		thismins = pframe->mins;
		thismaxs = pframe->maxs;

		oldmins = poldframe->mins;
		oldmaxs = poldframe->maxs;

		for( i = 0; i < 3; i++ ) {
			skm_mins[i] = min( thismins[i], oldmins[i] );
			skm_maxs[i] = max( thismaxs[i], oldmaxs[i] );
		}
		skm_radius = RadiusFromBounds( thismins, thismaxs );
	}

	if( e->scale != 1.0f ) {
		VectorScale( skm_mins, e->scale, skm_mins );
		VectorScale( skm_maxs, e->scale, skm_maxs );
		skm_radius *= e->scale;
	}
}

/*
================
R_CullSkeletalModel
================
*/
qboolean R_CullSkeletalModel( entity_t *e )
{
	int i;

	if( e->flags & RF_WEAPONMODEL )
		return qfalse;
	if( e->flags & RF_VIEWERMODEL )
		return !(r_mirrorview || r_portalview);

	if( r_spherecull->integer ) {
		if( R_CullSphere( e->origin, skm_radius, 15 ) )
			return qtrue;
	} else {
		vec3_t tmp, bbox[8];

		// compute and rotate a full bounding box
		for( i = 0; i < 8; i++ ) {
			tmp[0] = ( ( i & 1 ) ? skm_mins[0] : skm_maxs[0] );
			tmp[1] = ( ( i & 2 ) ? skm_mins[1] : skm_maxs[1] );
			tmp[2] = ( ( i & 4 ) ? skm_mins[2] : skm_maxs[2] );

			Matrix_TransformVector( e->axis, tmp, bbox[i] );
			bbox[i][0] += e->origin[0];
			bbox[i][1] = -bbox[i][1] + e->origin[1];
			bbox[i][2] += e->origin[2];
		}

		{
			int p, f, aggregatemask = ~0;

			for( p = 0; p < 8; p++ ) {
				int mask = 0;

				for( f = 0; f < 4; f++ ) {
					if ( DotProduct( frustum[f].normal, bbox[p] ) < frustum[f].dist )
						mask |= ( 1 << f );
				}
				aggregatemask &= mask;
			}

			if ( aggregatemask )
				return qtrue;
			return qfalse;
		}
	}

	if( R_VisCullSphere( e->origin, skm_radius ) )
		return qtrue;

	if( (r_mirrorview || r_portalview) && !r_nocull->integer )
		if( PlaneDiff (e->origin, &r_clipplane) < -skm_radius )
			return qtrue;

	return qfalse;
}

/*
================
R_DrawBonesFrameLerp
================
*/
void R_DrawBonesFrameLerp( const meshbuffer_t *mb, model_t *mod, float backlerp, qboolean shadow )
{
	int				i, meshnum;
	int				j, k, l;
	int				features;
	quat_t			quaternion;
	float			frontlerp = 1.0 - backlerp;
	vec3_t			move, delta;
	mskmesh_t		*mesh;
	bonepose_t		*bonepose, *oldbonepose, *bp, *oldbp;
	mskvertex_t		*skmverts;
	mskbonevert_t	*boneverts;
	entity_t		*e = mb->entity;
	mskmodel_t		*skmodel = mod->skmodel;
	struct { vec3_t axis[3], origin; } skmbonepose[SKM_MAX_BONES], *pose, *out;

	if( !shadow && (e->flags & RF_VIEWERMODEL) && !r_mirrorview && !r_portalview )
		return;

	meshnum = -(mb->infokey + 1);
	if( meshnum < 0 || meshnum >= skmodel->nummeshes )
		return;

	mesh = skmodel->meshes + meshnum;

#if SHADOW_VOLUMES
	if( shadow && !mesh->trneighbors )
		return;
#endif

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract( e->oldorigin, e->origin, delta );
	Matrix_TransformVector( e->axis, delta, move );
	VectorScale( move, e->scale * backlerp, move );

	if( e->boneposes )
		bp = e->boneposes;
	else
		bp = skmodel->frames[e->frame].boneposes;

	if( e->oldboneposes )
		oldbp = e->oldboneposes;
	else
		oldbp = skmodel->frames[e->oldframe].boneposes;

	for( i = 0; i < mesh->numreferences; i++ ) {
		out = skmbonepose + mesh->references[i];
		bonepose = bp + mesh->references[i];
		oldbonepose = oldbp + mesh->references[i];

		// interpolate quaternions and origins
		Quat_Lerp( oldbonepose->quat, bonepose->quat, frontlerp, quaternion );
		Quat_Matrix( quaternion, out->axis );
		out->axis[0][0] *= e->scale; out->axis[0][1] *= e->scale; out->axis[0][2] *= e->scale;
		out->axis[1][0] *= e->scale; out->axis[1][1] *= e->scale; out->axis[1][2] *= e->scale;
		out->axis[2][0] *= e->scale; out->axis[2][1] *= e->scale; out->axis[2][2] *= e->scale;
		out->origin[0] = oldbonepose->origin[0] + (bonepose->origin[0] - oldbonepose->origin[0]) * frontlerp;
		out->origin[1] = oldbonepose->origin[1] + (bonepose->origin[1] - oldbonepose->origin[1]) * frontlerp;
		out->origin[2] = oldbonepose->origin[2] + (bonepose->origin[2] - oldbonepose->origin[2]) * frontlerp;
	}

	features = MF_NONBATCHED | mb->shader->features;
	if( r_shownormals->integer && !shadow )
		features |= MF_NORMALS;
	if( mb->shader->flags & SHADER_AUTOSPRITE )
		features |= MF_NOCULL;
	if( shadow )
		features |= MF_DEFORMVS;

	if( (features & MF_NORMALS) && !shadow ) {
		for( j = 0, skmverts = mesh->vertexes; j < mesh->numverts; j++, skmverts++ ) {
			VectorCopy( move, inVertsArray[j] );
			VectorClear( inNormalsArray[j] );

			for( l = 0, boneverts = skmverts->verts; l < skmverts->numbones; l++, boneverts++ ) {
				pose = skmbonepose + boneverts->bonenum;

				for( k = 0; k < 3; k++ ) {
					inVertsArray[j][k] += boneverts->origin[0] * pose->axis[k][0] +
						boneverts->origin[1] * pose->axis[k][1] +
						boneverts->origin[2] * pose->axis[k][2] +
						boneverts->influence * pose->origin[k];
					inNormalsArray[j][k] += boneverts->normal[0] * pose->axis[k][0] +
						boneverts->normal[1] * pose->axis[k][1] +
						boneverts->normal[2] * pose->axis[k][2];
				}
			}

			VectorNormalizeFast( inNormalsArray[j] );
		}
	} else {
		for( j = 0, skmverts = mesh->vertexes; j < mesh->numverts; j++, skmverts++ ) {
			VectorCopy( move, inVertsArray[j] );

			for( l = 0, boneverts = skmverts->verts; l < skmverts->numbones; l++, boneverts++ ) {
				pose = skmbonepose + boneverts->bonenum;

				for( k = 0; k < 3; k++ ) {
					inVertsArray[j][k] += boneverts->origin[0] * pose->axis[k][0] +
						boneverts->origin[1] * pose->axis[k][1] +
						boneverts->origin[2] * pose->axis[k][2] +
						boneverts->influence * pose->origin[k];
				}
			}
		}
	}

	if( (features & MF_STVECTORS) && !shadow )
		R_BuildTangentVectors( mesh->numverts, inVertsArray, mesh->stcoords, mesh->numtris, mesh->indexes, inSVectorsArray, inTVectorsArray );

	skm_mesh.indexes = mesh->indexes;
	skm_mesh.numIndexes = mesh->numtris * 3;
	skm_mesh.numVertexes = mesh->numverts;
	skm_mesh.xyzArray = inVertsArray;
	skm_mesh.stArray = mesh->stcoords;
	skm_mesh.normalsArray = inNormalsArray;
	skm_mesh.sVectorsArray = inSVectorsArray;
	skm_mesh.tVectorsArray = inTVectorsArray;
#if SHADOW_VOLUMES
	skm_mesh.trneighbors = mesh->trneighbors;
	skm_mesh.trnormals = NULL;
#endif

	R_RotateForEntity( e );

	R_PushMesh( &skm_mesh, features );
	R_RenderMeshBuffer( mb, shadow );

	if ( shadow ) {
		if ( r_shadows->integer == 1 ) {
			R_Draw_SimpleShadow( e );
		} else {
#if SHADOW_VOLUMES
			R_SkeletalModelBBox( e, mod );
			R_DrawShadowVolumes( &skm_mesh, e->lightingOrigin, skm_mins, skm_maxs, skm_radius );
#endif
		}
	}
}

/*
=================
R_DrawSkeletalModel
=================
*/
void R_DrawSkeletalModel( const meshbuffer_t *mb, qboolean shadow )
{
	entity_t *e = mb->entity;

	// hack the depth range to prevent view model from poking into walls
	if( e->flags & RF_WEAPONMODEL )
		qglDepthRange( gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin) );

	// backface culling for left-handed weapons
	if( e->flags & RF_CULLHACK )
		qglFrontFace( GL_CW );

	if( !r_lerpmodels->integer )
		e->backlerp = 0;

	R_DrawBonesFrameLerp( mb, R_SkeletalModelLODForDistance( e ), e->backlerp, shadow );

	if( e->flags & RF_WEAPONMODEL )
		qglDepthRange( gldepthmin, gldepthmax );

	if( e->flags & RF_CULLHACK )
		qglFrontFace( GL_CCW );
}

/*
=================
R_AddSkeletalModelToList
=================
*/
void R_AddSkeletalModelToList( entity_t *e )
{
	int				i;
	mfog_t			*fog;
	model_t			*mod;
	shader_t		*shader;
	mskmesh_t		*mesh;
	mskmodel_t		*skmodel;

	mod = R_SkeletalModelLODForDistance ( e );
	if( !(skmodel = mod->skmodel) )
		return;

	R_SkeletalModelBBox ( e, mod );
	if( !r_shadows->integer && R_CullSkeletalModel( e ) )
		return;

	mesh = skmodel->meshes;
	fog = R_FogForSphere ( e->origin, skm_radius );

	for( i = 0; i < skmodel->nummeshes; i++, mesh++ ) {
		if( e->customSkin ) {
			shader = R_FindShaderForSkinFile( e->customSkin, mesh->name );
			if( shader )
				R_AddMeshToList( MB_MODEL, fog, shader, -(i+1) );
		} else if( e->customShader ) {
			R_AddMeshToList( MB_MODEL, fog, e->customShader, -(i+1) );
		} else {
			R_AddMeshToList( MB_MODEL, fog, mesh->skin.shader, -(i+1) );
		}
	}
}
