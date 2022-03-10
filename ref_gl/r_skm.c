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
	unsigned int	i, j, k;
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
	unsigned int	i, j, k, l, m;
	vec_t			*v, *n;
	dskmheader_t	*pinmodel;
	mskmodel_t		*poutmodel;
	dskmmesh_t		*pinmesh;
	mskmesh_t		*poutmesh;
	dskmvertex_t	*pinskmvert;
	dskmbonevert_t	*pinbonevert;
	dskmcoord_t		*pinstcoord;
	vec2_t			*poutstcoord;
	index_t			*pintris, *pouttris;
	unsigned int	*pinreferences, *poutreferences;
	bonepose_t		*bp, *basepose, *poutbonepose;

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

		FS_LoadFile( configName, (void **)&buf, NULL, 0 );
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

		FS_LoadFile( poseName, (void **)&buf, NULL, 0 );
		if( !buf )
			Com_Error( ERR_DROP, "Could not find pose file for %s", mod->name );

		Mod_LoadSkeletalPose( poseName, mod, buf );
	}

	// clear model's bounding box
	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	// reconstruct frame 0 bone poses and inverse bone poses
	basepose = Mod_Malloc( mod, sizeof( *basepose ) * poutmodel->numbones );
	poutmodel->invbaseposes = Mod_Malloc( mod, sizeof( *poutmodel->invbaseposes ) * poutmodel->numbones );

	for( i = 0, bp = basepose; i < poutmodel->numbones; i++, bp++ ) {
		int parent = poutmodel->bones[i].parent;
		bonepose_t *obp = &poutmodel->frames[0].boneposes[i], *ibp = &poutmodel->invbaseposes[i];

		if (parent >= 0)
		{
			Quat_ConcatTransforms( basepose[parent].quat, basepose[parent].origin,
				obp->quat, obp->origin, bp->quat, bp->origin );
		}
		else
		{
			*bp = *obp;
		}

		// reconstruct invserse bone pose
		Quat_Conjugate( bp->quat, ibp->quat );
		Quat_TransformVector( ibp->quat, bp->origin, ibp->origin );
		VectorInverse( ibp->origin );
	}

	pinmesh = ( dskmmesh_t * )( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_meshes ) );
	poutmesh = poutmodel->meshes = Mod_Malloc( mod, sizeof( mskmesh_t ) * poutmodel->nummeshes );

	for( i = 0; i < poutmodel->nummeshes; i++, pinmesh++, poutmesh++ ) {
		float *influences;
		unsigned int size, *bones;

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

		pinreferences = ( index_t *)( ( qbyte * )pinmodel + LittleLong( pinmesh->ofs_references ) );
		poutreferences = poutmesh->references = Mod_Malloc( mod, sizeof( unsigned int ) * poutmesh->numreferences );
		for( j = 0; j < poutmesh->numreferences; j++, pinreferences++, poutreferences++ )
			*poutreferences = LittleLong( *pinreferences );

		pinskmvert = ( dskmvertex_t * )( ( qbyte * )pinmodel + LittleLong( pinmesh->ofs_verts ) );

		poutmesh->influences = ( float * )Mod_Malloc( mod, (sizeof( *poutmesh->influences ) + sizeof( *poutmesh->bones )) * SKM_MAX_WEIGHTS * poutmesh->numverts );
		poutmesh->bones = ( unsigned int * )(( qbyte * )poutmesh->influences + sizeof( *poutmesh->influences ) * SKM_MAX_WEIGHTS * poutmesh->numverts);

		size = sizeof( vec3_t ) * 2;		// xyz and normals
		if( glConfig.GLSL )
			size += sizeof( vec4_t );		// s-vectors
		size *= poutmesh->numverts;

		poutmesh->xyzArray = ( vec3_t * )(Mod_Malloc( mod, size ));
		poutmesh->normalsArray = ( vec3_t * )(( qbyte * )poutmesh->xyzArray + sizeof( vec3_t ) * poutmesh->numverts);

		v = ( vec_t * )poutmesh->xyzArray;
		n = ( vec_t * )poutmesh->normalsArray;
		influences = poutmesh->influences;
		bones = poutmesh->bones;
		for( j = 0; j < poutmesh->numverts; j++, v += 3, n += 3, influences += SKM_MAX_WEIGHTS, bones += SKM_MAX_WEIGHTS ) {
			float sum, influence;
			unsigned int bonenum, numweights;
			vec3_t origin, normal, t, matrix[3];

			pinbonevert = ( dskmbonevert_t * )( ( qbyte * )pinskmvert + sizeof( pinskmvert->numweights ) );
			numweights = LittleLong( pinskmvert->numweights );

			for( l = 0; l < numweights; l++, pinbonevert++ ) {
				bonenum = LittleLong( pinbonevert->bonenum );
				influence = LittleFloat( pinbonevert->influence );
				poutbonepose = basepose + bonenum;
				for( k = 0; k < 3; k++ ) {
					origin[k] = LittleFloat( pinbonevert->origin[k] );
					normal[k] = LittleFloat( pinbonevert->normal[k] );
				}

				// rebuild the base pose
				Quat_Matrix( poutbonepose->quat, matrix );

				Matrix_TransformVector( matrix, origin, t );
				VectorAdd( v, t, v );
				VectorMA( v, influence, poutbonepose->origin, v );

				Matrix_TransformVector( matrix, normal, t );
				VectorAdd( n, t, n );

				if( !l ) {		// store the first influence
					bones[0] = bonenum;
					influences[0] = influence;
				} else {		// store the new influence if needed
					for( k = 0; k < SKM_MAX_WEIGHTS; k++ ) {
						if( influence > influences[k] ) {
							// pop the most weak influences out of the array,
							// shifting the rest of them to the beginning
							for( m = SKM_MAX_WEIGHTS-1; m > k; m-- ) {
								bones[m] = bones[m-1];
								influences[m] = influences[m-1];
							}

							// store the new influence
							bones[k] = bonenum;
							influences[k] = influence;
							break;
						}
					}
				}
			}

			// normalize influences if needed
			for( l = 0, sum = 0; l < SKM_MAX_WEIGHTS && influences[l]; l++ )
				sum += influences[l];
			if( sum > 1.0f - 1.0/256.0f ) {
				for( l = 0, sum = 1.0f / sum; l < SKM_MAX_WEIGHTS && influences[l]; l++ )
					influences[l] *= sum;
			}

			// test vertex against the bounding box
			AddPointToBounds( v, mod->mins, mod->maxs );

			pinskmvert = ( dskmvertex_t * )( ( qbyte * )pinbonevert );
		}

		pinstcoord = ( dskmcoord_t * )( ( qbyte * )pinmodel + LittleLong( pinmesh->ofs_texcoords ) );
		poutstcoord = poutmesh->stArray = Mod_Malloc( mod, poutmesh->numverts * sizeof(vec2_t) );
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

	//
	// build S and T vectors
	//
		if( glConfig.GLSL ) {
			vec3_t tempTVectorsArray[MAX_ARRAY_VERTS];

			poutmesh->sVectorsArray = ( vec4_t * )(( qbyte * )poutmesh->normalsArray + sizeof( vec3_t ) * poutmesh->numverts);
			R_BuildTangentVectors( poutmesh->numverts, poutmesh->xyzArray, poutmesh->normalsArray, poutmesh->stArray, 
				poutmesh->numtris, poutmesh->indexes, poutmesh->sVectorsArray, tempTVectorsArray );
		}

	//
	// build triangle neighbors
	//
#if SHADOW_VOLUMES
		poutmesh->trneighbors = Mod_Malloc( mod, sizeof(int) * poutmesh->numtris * 3 );
		R_BuildTriangleNeighbors( poutmesh->trneighbors, poutmesh->indexes, poutmesh->numtris );
#endif
	}

#if 0
	// enable this to speed up loading
	for( j = 0; j < 3; j++ ) {
		mod->mins[j] -= 48;
		mod->maxs[j] += 48;
	}
#else
	// so much work just to calc the model's bounds, doh
	for( j = 1; j < poutmodel->numframes; j++ ) {
		for( i = 0, bp = basepose; i < poutmodel->numbones; i++, bp++ ) {
			int parent = poutmodel->bones[i].parent;
			bonepose_t *obp = &poutmodel->frames[j].boneposes[i];

			if (parent >= 0)
			{
				Quat_ConcatTransforms( basepose[parent].quat, basepose[parent].origin,
					obp->quat, obp->origin, bp->quat, bp->origin );
			}
			else
			{
				*bp = *obp;
			}
		}

		pinmesh = ( dskmmesh_t * )( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_meshes ) );
		for( i = 0, poutmesh = poutmodel->meshes; i < poutmodel->nummeshes; i++, pinmesh++, poutmesh++ ) {
			pinskmvert = ( dskmvertex_t * )( ( qbyte * )pinmodel + LittleLong( pinmesh->ofs_verts ) );

			for( k = 0; k < poutmesh->numverts; k++ ) {
				float influence;
				unsigned int numweights, bonenum;
				vec3_t v;

				pinbonevert = ( dskmbonevert_t * )( ( qbyte * )pinskmvert + sizeof( pinskmvert->numweights ) );
				numweights = LittleLong( pinskmvert->numweights );

				VectorClear( v );
				for( l = 0; l < numweights; l++, pinbonevert++ ) {
					vec3_t origin, t;

					bonenum = LittleLong( pinbonevert->bonenum );
					influence = LittleFloat( pinbonevert->influence );
					poutbonepose = basepose + bonenum;
					for( m = 0; m < 3; m++ )
						origin[m] = LittleFloat( pinbonevert->origin[m] );

					// transform vertex
					Quat_TransformVector( poutbonepose->quat, origin, t );
					VectorAdd( v, t, v );
					VectorMA( v, influence, poutbonepose->origin, v );
				}

				// test vertex against the bounding box
				AddPointToBounds( v, mod->mins, mod->maxs );

				pinskmvert = ( dskmvertex_t * )( ( qbyte * )pinbonevert );
			}
		}

	}
#endif

	mod->radius = RadiusFromBounds( mod->mins, mod->maxs );

	Mem_Free( basepose );
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
	if( (unsigned int)bonenum >= (int)mod->skmodel->numbones )
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
	if( bonenum < 0 || bonenum >= (int)mod->skmodel->numbones )
		Com_Error( ERR_DROP, "R_SkeletalGetBonePose: bad bone number" );
	if( frame < 0 || frame >= (int)mod->skmodel->numframes )
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

	dist = Distance( e->origin, ri.viewOrigin );
	dist *= ri.lod_dist_scale_for_fov;

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
static void R_SkeletalModelBBox( entity_t *e, model_t *mod )
{
	VectorCopy( mod->mins, skm_mins );
	VectorCopy( mod->maxs, skm_maxs );
	skm_radius = mod->radius;

	if( e->scale != 1.0f ) {
		VectorScale( skm_mins, e->scale, skm_mins );
		VectorScale( skm_maxs, e->scale, skm_maxs );
		skm_radius *= e->scale;
	}
}

typedef float fl12_t[12];

/*
================
R_SkeletalTransformVerts

FIXME: Optimize for SSE/SSE2
================
*/
static void R_SkeletalTransformVerts( int numverts, unsigned int *bones, float *influences, fl12_t *relbonepose, vec_t *v, vec_t *ov )
{
	int j;
	float i, *pose;

	for( ; numverts; numverts--, v += 3, ov += 3, bones += SKM_MAX_WEIGHTS, influences += SKM_MAX_WEIGHTS ) {
		i = *influences;
		pose = relbonepose[*bones];

		if( i == 1 ) {	// most common case
			ov[0] = v[0] * pose[0] + v[1] * pose[1] + v[2] * pose[2] + pose[3];
			ov[1] = v[0] * pose[4] + v[1] * pose[5] + v[2] * pose[6] + pose[7];
			ov[2] = v[0] * pose[8] + v[1] * pose[9] + v[2] * pose[10] + pose[11];
		} else {
			ov[0] = i * (v[0] * pose[0] + v[1] * pose[1] + v[2] * pose[2] + pose[3]);
			ov[1] = i * (v[0] * pose[4] + v[1] * pose[5] + v[2] * pose[6] + pose[7]);
			ov[2] = i * (v[0] * pose[8] + v[1] * pose[9] + v[2] * pose[10] + pose[11]);

			for( j = 1; j < SKM_MAX_WEIGHTS && influences[j]; j++ ) {
				i = influences[j];
				pose = relbonepose[bones[j]];

				ov[0] += i * (v[0] * pose[0] + v[1] * pose[1] + v[2] * pose[2] + pose[3]);
				ov[1] += i * (v[0] * pose[4] + v[1] * pose[5] + v[2] * pose[6] + pose[7]);
				ov[2] += i * (v[0] * pose[8] + v[1] * pose[9] + v[2] * pose[10] + pose[11]);
			}
		}
	}
}

/*
================
R_SkeletalTransformNormals

FIXME: Optimize for SSE/SSE2
================
*/
static void R_SkeletalTransformNormals( int numverts, unsigned int *bones, float *influences, fl12_t *relbonepose, vec_t *n, vec_t *on, int nstride )
{
	int j;
	float i, *pose;

	for( ; numverts; numverts--, n += nstride, on += nstride, bones += SKM_MAX_WEIGHTS, influences += SKM_MAX_WEIGHTS ) {
		i = *influences;
		pose = relbonepose[*bones];

		if( i == 1 ) {	// most common case
			on[0] = n[0] * pose[0] + n[1] * pose[1] + n[2] * pose[2];
			on[1] = n[0] * pose[4] + n[1] * pose[5] + n[2] * pose[6];
			on[2] = n[0] * pose[8] + n[1] * pose[9] + n[2] * pose[10];
		} else {
			on[0] = i * (n[0] * pose[0] + n[1] * pose[1] + n[2] * pose[2]);
			on[1] = i * (n[0] * pose[4] + n[1] * pose[5] + n[2] * pose[6]);
			on[2] = i * (n[0] * pose[8] + n[1] * pose[9] + n[2] * pose[10]);

			for( j = 1; j < SKM_MAX_WEIGHTS && influences[j]; j++ ) {
				i = influences[j];
				pose = relbonepose[bones[j]];

				on[0] += i * (n[0] * pose[0] + n[1] * pose[1] + n[2] * pose[2]);
				on[1] += i * (n[0] * pose[4] + n[1] * pose[5] + n[2] * pose[6]);
				on[2] += i * (n[0] * pose[8] + n[1] * pose[9] + n[2] * pose[10]);
			}
		}
	}
}

/*
================
R_DrawBonesFrameLerp
================
*/
void R_DrawBonesFrameLerp( const meshbuffer_t *mb, model_t *mod, float backlerp, qboolean shadow )
{
	unsigned int	i, j, meshnum;
	int				features;
	float			frontlerp = 1.0 - backlerp, *pose;
	mskmesh_t		*mesh;
	bonepose_t		*bonepose, *oldbonepose, tempbonepose[SKM_MAX_BONES], *lerpedbonepose;
	bonepose_t		*bp, *oldbp, *out, tp;
	entity_t		*e = ri.currententity;
	mskmodel_t		*skmodel = mod->skmodel;
	fl12_t			relbonepose[SKM_MAX_BONES];
	shader_t		*shader;
	mskbone_t		*bone;
	vec3_t			*xyzArray, *normalsArray;
	vec4_t			*sVectorsArray;

	if( !shadow && (e->flags & RF_VIEWERMODEL) && !(ri.params & (RP_MIRRORVIEW|RP_PORTALVIEW|RP_SKYPORTALVIEW)) )
		return;

	meshnum = -(mb->infokey + 1);
	if( meshnum >= skmodel->nummeshes )
		return;

	mesh = skmodel->meshes + meshnum;

#if SHADOW_VOLUMES
	if( shadow && !mesh->trneighbors )
		return;
#endif

	xyzArray = inVertsArray;
	normalsArray = inNormalsArray;
	sVectorsArray = inSVectorsArray;

	MB_NUM2SHADER( mb->shaderkey, shader );

	features = MF_NONBATCHED | shader->features;
	if( (features && (MF_SVECTORS)) || r_shownormals->integer && !shadow )
		features |= MF_NORMALS;
	if( shader->flags & SHADER_AUTOSPRITE )
		features |= MF_NOCULL;
	if( shadow )
	{
		features |= MF_DEFORMVS;
		features &= ~MF_SVECTORS;
		if( !(shader->features & SHADER_DEFORMV_NORMAL) )
			features &= ~MF_NORMALS;
	}

	// not sure if it's really needed
	if( e->boneposes == skmodel->frames[0].boneposes ) {
		e->boneposes = NULL;
		e->frame = e->oldframe = 0;
	}

	// choose boneposes for lerping
	if( e->boneposes )
	{
		bp = e->boneposes;
		if( e->oldboneposes )
			oldbp = e->oldboneposes;
		else
			oldbp = bp;
	}
	else
	{
		if( ( e->frame >= (int)skmodel->numframes ) || ( e->frame < 0 ) ) {
			Com_DPrintf( "R_DrawBonesFrameLerp %s: no such frame %d\n", mod->name, e->frame );
			e->frame = 0;
		}
		if( ( e->oldframe >= (int)skmodel->numframes ) || ( e->oldframe < 0 ) ) {
			Com_DPrintf( "R_DrawBonesFrameLerp %s: no such oldframe %d\n", mod->name, e->oldframe );
			e->oldframe = 0;
		}

		bp = skmodel->frames[e->frame].boneposes;
		oldbp = skmodel->frames[e->oldframe].boneposes;
	}

	lerpedbonepose = tempbonepose;
	if( bp == oldbp || frontlerp == 1 )
	{
		if( e->boneposes )
		{	// assume that parent transforms have already been applied
			lerpedbonepose = bp;
		}
		else
		{	// transform
			if( !e->frame )
			{	// fastpath: render frame 0 as is (with possible scaling)
				if( e->scale == 1 )
				{
					xyzArray = mesh->xyzArray;
				}
				else
				{
					for( i = 0; i < mesh->numverts; i++ )
						VectorScale( mesh->xyzArray[i], e->scale, inVertsArray[i] );
				}

				normalsArray = mesh->normalsArray;
				sVectorsArray = mesh->sVectorsArray;

				goto pushmesh;
			}

			for( i = 0; i < mesh->numreferences; i++ )
			{
				j = mesh->references[i];
				out = lerpedbonepose + j;
				bonepose = bp + j;
				bone = skmodel->bones + j;

				if( bone->parent >= 0 )
				{
					Quat_ConcatTransforms( lerpedbonepose[bone->parent].quat, lerpedbonepose[bone->parent].origin,
						bonepose->quat, bonepose->origin, out->quat, out->origin );
				}
				else
				{
					Quat_Copy( bonepose->quat, out->quat );
					VectorCopy( bonepose->origin, out->origin );
				}
			}
		}
	}
	else
	{
		if( e->boneposes )
		{	// lerp, assume that parent transforms have already been applied
			for( i = 0, out = lerpedbonepose, bonepose = bp, oldbonepose = oldbp, bone = skmodel->bones; i < skmodel->numbones; i++, out++, bonepose++, oldbonepose++, bone++ )
			{
				Quat_Lerp( oldbonepose->quat, bonepose->quat, frontlerp, out->quat );
				out->origin[0] = oldbonepose->origin[0] + (bonepose->origin[0] - oldbonepose->origin[0]) * frontlerp;
				out->origin[1] = oldbonepose->origin[1] + (bonepose->origin[1] - oldbonepose->origin[1]) * frontlerp;
				out->origin[2] = oldbonepose->origin[2] + (bonepose->origin[2] - oldbonepose->origin[2]) * frontlerp;
			}
		}
		else
		{	// lerp and transform
			for( i = 0; i < mesh->numreferences; i++ )
			{
				j = mesh->references[i];
				out = lerpedbonepose + j;
				bonepose = bp + j;
				oldbonepose = oldbp + j;
				bone = skmodel->bones + j;

				Quat_Lerp( oldbonepose->quat, bonepose->quat, frontlerp, tp.quat );
				tp.origin[0] = oldbonepose->origin[0] + (bonepose->origin[0] - oldbonepose->origin[0]) * frontlerp;
				tp.origin[1] = oldbonepose->origin[1] + (bonepose->origin[1] - oldbonepose->origin[1]) * frontlerp;
				tp.origin[2] = oldbonepose->origin[2] + (bonepose->origin[2] - oldbonepose->origin[2]) * frontlerp;

				if( bone->parent >= 0 )
				{
					Quat_ConcatTransforms( tempbonepose[bone->parent].quat, tempbonepose[bone->parent].origin,
						tp.quat, tp.origin, out->quat, out->origin );
				} 
				else
				{
					Quat_Copy( tp.quat, out->quat );
					VectorCopy( tp.origin, out->origin );
				}
			}
		}
	}

	for( i = 0; i < mesh->numreferences; i++ ) {
		j = mesh->references[i];
		pose = relbonepose[j];

		Quat_ConcatTransforms( lerpedbonepose[j].quat, lerpedbonepose[j].origin,
			skmodel->invbaseposes[j].quat, skmodel->invbaseposes[j].origin, tp.quat, tp.origin );

		// make origin the forth column instead of row so that
		// things can be optimized more easily
		Quat_Vectors( tp.quat, &pose[0], &pose[4], &pose[8] );
		if( e->scale == 1 ) {
			pose[3] = tp.origin[0];
			pose[7] = tp.origin[1];
			pose[11] = tp.origin[2];
		} else {
			pose[3] = tp.origin[0] * e->scale;
			pose[7] = tp.origin[1] * e->scale;
			pose[11] = tp.origin[2] * e->scale;

			VectorScale( &pose[0], e->scale, &pose[0] );
			VectorScale( &pose[4], e->scale, &pose[4] );
			VectorScale( &pose[8], e->scale, &pose[8] );
		}
	}

	R_SkeletalTransformVerts( mesh->numverts, mesh->bones, mesh->influences, relbonepose, 
		( vec_t *)mesh->xyzArray, ( vec_t * )inVertsArray );

	if( features & MF_NORMALS )
		R_SkeletalTransformNormals( mesh->numverts, mesh->bones, mesh->influences, relbonepose, 
			( vec_t * )mesh->normalsArray, ( vec_t * )inNormalsArray, 3 );

	if( features & MF_SVECTORS ) 
		R_SkeletalTransformNormals( mesh->numverts, mesh->bones, mesh->influences, relbonepose, 
			( vec_t * )mesh->sVectorsArray, ( vec_t * )inSVectorsArray, 4 );

pushmesh:
	skm_mesh.indexes = mesh->indexes;
	skm_mesh.numIndexes = mesh->numtris * 3;
	skm_mesh.numVertexes = mesh->numverts;
	skm_mesh.xyzArray = xyzArray;
	skm_mesh.stArray = mesh->stArray;
	skm_mesh.normalsArray = normalsArray;
	skm_mesh.sVectorsArray = sVectorsArray;
#if SHADOW_VOLUMES
	skm_mesh.trneighbors = mesh->trneighbors;
	skm_mesh.trnormals = NULL;
#endif

	R_RotateForEntity( e );

	R_PushMesh( &skm_mesh, features );
	R_RenderMeshBuffer( mb, shadow );

	if ( shadow ) {
		if ( r_shadows->integer == SHADOW_PLANAR ) {
			R_Draw_SimpleShadow( e );
		} else {
#if SHADOW_VOLUMES
			R_SkeletalModelBBox( e, mod );
			R_DrawShadowVolumes( &skm_mesh, skm_mins, skm_maxs, skm_radius );
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
	entity_t *e = ri.currententity;

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
	if( !(r_shadows->integer >= SHADOW_PLANAR) && R_CullModel( e, skm_mins, skm_maxs, skm_radius ) )
		return;

	fog = R_FogForSphere ( e->origin, skm_radius );
	if( !(e->flags & RF_WEAPONMODEL) && R_CompletelyFogged( fog, e->origin, skm_radius ) )
		return;

	mesh = skmodel->meshes;
	for( i = 0; i < (int)skmodel->nummeshes; i++, mesh++ ) {
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
