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

static  mesh_t			skm_mesh;

static	vec3_t			skm_mins;
static	vec3_t			skm_maxs;
static	float			skm_radius;

static	mskbonepose_t	skmbonepose[SKM_MAX_BONES];

/*
=================
R_InitSkeletalModels
=================
*/
void R_InitSkeletalModels (void)
{
	skm_mesh.xyz_array = tempVertexArray;
	skm_mesh.normals_array = tempNormalsArray;
#ifdef SHADOW_VOLUMES
	skm_mesh.trnormals = NULL;
#endif
}

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
void Mod_LoadSkeletalPose ( char *name, model_t *mod, void *buffer )
{
	int				i, j, k;
	mskmodel_t		*poutmodel;
	dskpheader_t	*pinmodel;
	dskpbone_t		*pinbone;
	mskbone_t		*poutbone;
	dskpframe_t		*pinframe;
	mskframe_t		*poutframe;
	dskpbonepose_t	*pinbonepose;
	mskbonepose_t	*poutbonepose, temppose;

	if ( strncmp ((const char *)buffer, SKMHEADER, 4) ) {
		Com_Error ( ERR_DROP, "uknown fileid for %s", name );
	}

	pinmodel = ( dskpheader_t * )buffer;
	poutmodel = mod->skmodel;

	if ( LittleLong (pinmodel->type) != SKM_MODELTYPE ) {
		Com_Error ( ERR_DROP, "%s has wrong type number (%i should be %i)",
				 name, LittleLong (pinmodel->type), SKM_MODELTYPE );
	}

	if ( LittleLong (pinmodel->filesize) > SKM_MAX_FILESIZE ) {
		Com_Error ( ERR_DROP, "%s has has wrong filesize (%i should be less than %i)",
				 name, LittleLong (pinmodel->filesize), SKM_MAX_FILESIZE );
	}

	if ( LittleLong (pinmodel->num_bones) != poutmodel->numbones ) {
		Com_Error ( ERR_DROP, "%s has has wrong number of bones (%i should be less than %i)",
				 name, LittleLong (pinmodel->num_bones), poutmodel->numbones );
	}

	poutmodel->numframes = LittleLong ( pinmodel->num_frames );
	if ( poutmodel->numframes <= 0 ) {
		Com_Error ( ERR_DROP, "%s has no frames", name );
	} else if ( poutmodel->numframes > SKM_MAX_FRAMES ) {
		Com_Error ( ERR_DROP, "%s has too many frames", name );
	}

	pinbone = ( dskpbone_t * )( ( qbyte * )pinmodel + LittleLong ( pinmodel->ofs_bones ) );
	poutbone = poutmodel->bones = Mod_Malloc ( mod, sizeof(mskbone_t) * poutmodel->numbones );

	for ( i = 0; i < poutmodel->numbones; i++, pinbone++, poutbone++ ) 
	{
		Com_sprintf ( poutbone->name, SKM_MAX_NAME, pinbone->name );
		poutbone->flags = LittleLong ( pinbone->flags );
		poutbone->parent = LittleLong ( pinbone->parent );
	}

	pinframe = ( dskpframe_t * )( ( qbyte * )pinmodel + LittleLong ( pinmodel->ofs_frames ) );
	poutframe = poutmodel->frames = Mod_Malloc ( mod, sizeof(mskframe_t) * poutmodel->numframes );

	for ( i = 0; i < poutmodel->numframes; i++, pinframe++, poutframe++ )
	{
		poutbone = poutmodel->bones;

		pinbonepose = ( dskpbonepose_t * )( ( qbyte * )pinmodel + LittleLong (pinframe->ofs_bonepositions) );
		poutbonepose = poutframe->boneposes = Mod_Malloc ( mod, sizeof(mskbonepose_t) * poutmodel->numbones );

		for ( j = 0; j < poutmodel->numbones; j++, poutbone++, pinbonepose++, poutbonepose++ )
		{
			for ( k = 0; k < 3; k++ )
			{
				temppose.matrix[k][0] = LittleFloat ( pinbonepose->matrix[k][0] );
				temppose.matrix[k][1] = LittleFloat ( pinbonepose->matrix[k][1] );
				temppose.matrix[k][2] = LittleFloat ( pinbonepose->matrix[k][2] );
				temppose.matrix[k][3] = LittleFloat ( pinbonepose->matrix[k][3] );
			}

			if ( poutbone->parent >= 0 ) {
				R_ConcatTransforms ( &poutframe->boneposes[poutbone->parent].matrix[0][0], &temppose.matrix[0][0], &poutbonepose->matrix[0][0] );
			} else {
				memcpy ( &poutbonepose->matrix[0][0], &temppose.matrix[0][0], sizeof(mskbonepose_t) );
			}
		}
	}
}

/*
=================
Mod_LoadSkeletalModel
=================
*/
void Mod_LoadSkeletalModel ( model_t *mod, model_t *parent, void *buffer )
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
	mskcoord_t		*poutstcoord;
	index_t			*pintris, *pouttris;
	mskframe_t		*poutframe;
	unsigned int	*pinreferences, *poutreferences;

	pinmodel = ( dskmheader_t * )buffer;

	if ( LittleLong (pinmodel->type) != SKM_MODELTYPE ) {
		Com_Error ( ERR_DROP, "%s has wrong type number (%i should be %i)",
				 mod->name, LittleLong (pinmodel->type), SKM_MODELTYPE );
	}

	if ( LittleLong (pinmodel->filesize) > SKM_MAX_FILESIZE ) {
		Com_Error ( ERR_DROP, "%s has has wrong filesize (%i should be less than %i)",
				 mod->name, LittleLong (pinmodel->filesize), SKM_MAX_FILESIZE );
	}

	poutmodel = mod->skmodel = Mod_Malloc ( mod, sizeof(mskmodel_t) );

	poutmodel->nummeshes = LittleLong ( pinmodel->num_meshes );
	if ( poutmodel->nummeshes <= 0 ) {
		Com_Error ( ERR_DROP, "%s has no meshes", mod->name );
	} else if ( poutmodel->nummeshes > SKM_MAX_MESHES ) {
		Com_Error ( ERR_DROP, "%s has too many meshes", mod->name );
	}

	poutmodel->numbones = LittleLong ( pinmodel->num_bones );
	if ( poutmodel->numbones <= 0 ) {
		Com_Error ( ERR_DROP, "%s has no bones", mod->name );
	} else if ( poutmodel->numbones > SKM_MAX_BONES ) {
		Com_Error ( ERR_DROP, "%s has too many bones", mod->name );
	}

	pinmesh = ( dskmmesh_t * )( ( qbyte * )pinmodel + LittleLong ( pinmodel->ofs_meshes ) );
	poutmesh = poutmodel->meshes = Mod_Malloc ( mod, sizeof(mskmesh_t) * poutmodel->nummeshes );

	for ( i = 0; i < poutmodel->nummeshes; i++, pinmesh++, poutmesh++ ) 
	{
		poutmesh->numverts = LittleLong ( pinmesh->num_verts );
		if ( poutmesh->numverts <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no vertexes", i, mod->name );
		} else if ( poutmesh->numverts > SKM_MAX_VERTS ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has too many vertexes", i, mod->name );
		}

		poutmesh->numtris = LittleLong ( pinmesh->num_tris );
		if ( poutmesh->numtris <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no indices", i, mod->name );
		} else if ( poutmesh->numtris > SKM_MAX_TRIS ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has too many indices", i, mod->name );
		}

		poutmesh->numreferences = LittleLong ( pinmesh->num_references );
		if ( poutmesh->numreferences <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no bone references", i, mod->name );
		} else if ( poutmesh->numreferences > SKM_MAX_BONES ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has too many bone references", i, mod->name );
		}

		Q_strncpyz ( poutmesh->name, pinmesh->meshname, sizeof(poutmesh->name) );
		Mod_StripLODSuffix ( poutmesh->name );

		Q_strncpyz ( poutmesh->skin.shadername, pinmesh->shadername, sizeof(poutmesh->skin.shadername) );

		pinskmvert = ( dskmvertex_t * )( ( qbyte * )pinmodel + LittleLong ( pinmesh->ofs_verts ) );
		poutskmvert = poutmesh->vertexes = Mod_Malloc ( mod, sizeof(mskvertex_t) * poutmesh->numverts );

		for ( j = 0; j < poutmesh->numverts; j++, poutskmvert++ )
		{
			poutskmvert->numbones = LittleLong ( pinskmvert->numbones );

			pinbonevert = ( dskmbonevert_t * )( ( qbyte * )pinskmvert + sizeof(poutskmvert->numbones) );
			poutbonevert = poutskmvert->verts = Mod_Malloc ( mod, sizeof(mskbonevert_t) * poutskmvert->numbones );

			for ( l = 0; l < poutskmvert->numbones; l++, pinbonevert++, poutbonevert++ )
			{
				for ( k = 0; k < 3; k++ )
				{
					poutbonevert->origin[k] = LittleFloat ( pinbonevert->origin[k] );
					poutbonevert->normal[k] = LittleFloat ( pinbonevert->normal[k] );
				}

				poutbonevert->influence = LittleFloat ( pinbonevert->influence );
				poutbonevert->bonenum = LittleLong ( pinbonevert->bonenum );
			}

			pinskmvert = ( dskmvertex_t * )( ( qbyte * )pinbonevert );
		}

		pinstcoord = ( dskmcoord_t * )( ( qbyte * )pinmodel + LittleLong (pinmesh->ofs_texcoords) );
		poutstcoord = poutmesh->stcoords = Mod_Malloc ( mod, sizeof(mskcoord_t) * poutmesh->numverts );

		for ( j = 0; j < poutmesh->numverts; j++, pinstcoord++, poutstcoord++ )
		{
			poutstcoord->st[0] = LittleFloat ( pinstcoord->st[0] );
			poutstcoord->st[1] = LittleFloat ( pinstcoord->st[1] );
		}

		pintris = ( index_t * )( ( qbyte * )pinmodel + LittleLong (pinmesh->ofs_indices) );
		pouttris = poutmesh->indexes = Mod_Malloc ( mod, sizeof(index_t) * poutmesh->numtris * 3 );

		for ( j = 0; j < poutmesh->numtris; j++, pintris += 3, pouttris += 3 ) {
			pouttris[0] = LittleLong ( pintris[0] );
			pouttris[1] = LittleLong ( pintris[1] );
			pouttris[2] = LittleLong ( pintris[2] );
		}

		pinreferences = ( index_t *)( ( qbyte * )pinmodel + LittleLong (pinmesh->ofs_references) );
		poutreferences = poutmesh->references = Mod_Malloc ( mod, sizeof(unsigned int) * poutmesh->numreferences );

		for ( j = 0; j < poutmesh->numreferences; j++, pinreferences++, poutreferences++ ) {
			*poutreferences = LittleLong ( *pinreferences );
		}

	//
	// build triangle neighbors
	//
#ifdef SHADOW_VOLUMES
		poutmesh->trneighbors = Mod_Malloc ( mod, sizeof(int) * poutmesh->numtris * 3 );
		R_BuildTriangleNeighbors ( poutmesh->trneighbors, poutmesh->indexes, poutmesh->numtris );
#endif
	}

	// if we have a parent model then we are a LOD file and should use parent model's pose data
	if ( parent ) {
		if ( !parent->skmodel ) {
			Com_Error ( ERR_DROP, "%s is not a LOD model for %s",
					 mod->name, parent->name );
		}
		if ( poutmodel->numbones != parent->skmodel->numbones ) {
			Com_Error ( ERR_DROP, "%s has has wrong number of bones (%i should be less than %i)",
					 mod->name, poutmodel->numbones, parent->skmodel->numbones );
		}
		poutmodel->bones = parent->skmodel->bones;

		poutmodel->frames = parent->skmodel->frames;
		poutmodel->numframes = parent->skmodel->numframes;
	} else {		// load a config file
		qbyte *buf;
		char temp[MAX_QPATH];
		char poseName[MAX_QPATH], configName[MAX_QPATH];

		COM_StripExtension ( mod->name, temp );
		Com_sprintf ( configName, sizeof(configName), "%s.cfg", temp );

		memset ( poseName, 0, sizeof(poseName) );

		FS_LoadFile ( configName, (void **)&buf );
		if ( !buf ) {
			Com_sprintf ( poseName, sizeof(poseName), "%s.skp", temp );
		} else {
			char *ptr, *token;

			ptr = ( char * )buf;
			while ( ptr )
			{
				token = COM_ParseExt ( &ptr, qtrue );
				if ( !token ) {
					break;
				}

				if ( !Q_stricmp (token, "import") ) {
					token = COM_ParseExt ( &ptr, qfalse );
					COM_StripExtension ( token, temp );
					Com_sprintf ( poseName, sizeof(poseName), "%s.skp", temp );
					break;
				}
			}

			FS_FreeFile ( buf );
		}

		FS_LoadFile ( poseName, (void **)&buf );
		if ( !buf ) {
			Com_Error ( ERR_DROP, "Could not find pose file for %s", mod->name );
		}

		Mod_LoadSkeletalPose ( poseName, mod, buf );
	}

	// recalculate frame and model bounds according to pose data
	mod->radius = 0;
	ClearBounds ( mod->mins, mod->maxs );

	poutframe = poutmodel->frames;
	for ( i = 0; i < poutmodel->numframes; i++, poutframe++ )
	{
		vec3_t			v;
		mskbonepose_t	*poutbonepose;

		ClearBounds ( poutframe->mins, poutframe->maxs );

		poutmesh = poutmodel->meshes;
		for ( m = 0; m < poutmodel->nummeshes; m++, poutmesh++ ) 
		{
			poutskmvert = poutmesh->vertexes;
			for ( j = 0; j < poutmesh->numverts; j++, poutskmvert++ )
			{
				VectorClear ( v );

				poutbonevert = poutskmvert->verts;
				for ( l = 0; l < poutskmvert->numbones; l++, poutbonevert++ )
				{
					poutbonepose = poutframe->boneposes + poutbonevert->bonenum;

					for ( k = 0; k < 3; k++ )
					{
						v[k] += poutbonevert->origin[0] * poutbonepose->matrix[k][0] +
							poutbonevert->origin[1] * poutbonepose->matrix[k][1] +
							poutbonevert->origin[2] * poutbonepose->matrix[k][2] +
							poutbonevert->influence * poutbonepose->matrix[k][3];
					}
				}

				AddPointToBounds ( v, poutframe->mins, poutframe->maxs );
			}
		}

		poutframe->radius = RadiusFromBounds ( poutframe->mins, poutframe->maxs );
		VectorAdd ( poutframe->mins, mod->mins, mod->maxs );
		VectorAdd ( poutframe->maxs, mod->mins, mod->maxs );
		mod->radius = max ( mod->radius, poutframe->radius );
	}

	mod->type = mod_skeletal;
}

/*
================
Mod_RegisterSkeletalModel
================
*/
void Mod_RegisterSkeletalModel ( model_t *mod )
{
	int				i;
	mskmodel_t		*skmodel;
	mskmesh_t		*mesh;
	mskskin_t		*skin;

	if ( !(skmodel = mod->skmodel) ) {
		return;
	}

	for ( i = 0, mesh = skmodel->meshes; i < skmodel->nummeshes; i++, mesh++ ) {
		skin = &mesh->skin;
		skin->shader = R_RegisterSkin ( skin->shadername );
	}
}

/*
** R_SkeletalModelLODForDistance
*/
static model_t *R_SkeletalModelLODForDistance ( entity_t *e )
{
	int lod;
	float dist;

	if ( !e->model->numlods ) {
		return e->model;
	}

	dist = Distance ( e->origin, r_origin );
	dist *= tan (r_newrefdef.fov_x * (M_PI/180) * 0.5f);

	lod = (int)(dist / e->model->radius);
	if (r_lodscale->value)
		lod /= r_lodscale->value;
	lod += (int)max(r_lodbias->value, 0);

	if ( lod < 1 ) {
		return e->model;
	} else {
		return e->model->lods[min(lod, e->model->numlods)-1];
	}
}

/*
** R_SkeletalModelBBox
*/
void R_SkeletalModelBBox ( entity_t *e, model_t *mod )
{
	int			i;
	mskframe_t	*pframe, *poldframe;
	float		*thismins, *oldmins, *thismaxs, *oldmaxs;
	mskmodel_t	*skmodel = mod->skmodel;

	if ( ( e->frame >= skmodel->numframes ) || ( e->frame < 0 ) )
	{
		Com_DPrintf ("R_SkeletalModelBBox %s: no such frame %d\n", mod->name, e->frame);
		e->frame = 0;
	}
	if ( ( e->oldframe >= skmodel->numframes ) || ( e->oldframe < 0 ) )
	{
		Com_DPrintf ("R_SkeletalModelBBox %s: no such oldframe %d\n", mod->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = skmodel->frames + e->frame;
	poldframe = skmodel->frames + e->oldframe;

	/*
	** compute axially aligned mins and maxs
	*/
	if ( pframe == poldframe )
	{
		VectorCopy ( pframe->mins, skm_mins );
		VectorCopy ( pframe->maxs, skm_maxs );
		skm_radius = pframe->radius;
	}
	else
	{
		thismins = pframe->mins;
		thismaxs = pframe->maxs;

		oldmins  = poldframe->mins;
		oldmaxs  = poldframe->maxs;

		for ( i = 0; i < 3; i++ )
		{
			if ( thismins[i] < oldmins[i] )
				skm_mins[i] = thismins[i];
			else
				skm_mins[i] = oldmins[i];

			if ( thismaxs[i] > oldmaxs[i] )
				skm_maxs[i] = thismaxs[i];
			else
				skm_maxs[i] = oldmaxs[i];
		}

		skm_radius = max ( poldframe->radius, pframe->radius );
	}

	if ( e->scale != 1.0f ) {
		VectorScale ( skm_mins, e->scale, skm_mins );
		VectorScale ( skm_maxs, e->scale, skm_maxs );
		skm_radius *= e->scale;
	}
}

/*
** R_CullSkeletalModel
*/
qboolean R_CullSkeletalModel( entity_t *e, model_t *mod )
{
	if ( e->flags & RF_WEAPONMODEL ) {
		return qfalse;
	}
	if ( e->flags & RF_VIEWERMODEL ) {
		return !(r_mirrorview || r_portalview);
	}

	if ( !Matrix3_Compare (e->axis, mat3_identity) ) {
		if ( R_CullSphere (e->origin, skm_radius, 15) ) {
			return qtrue;
		}
	} else {
		vec3_t mins, maxs;

		VectorAdd ( skm_mins, e->origin, mins );
		VectorAdd ( skm_maxs, e->origin, maxs );

		if ( R_CullBox (mins, maxs, 15) ) {
			return qtrue;
		}
	}

	if ( (r_mirrorview || r_portalview) && !r_nocull->value ) {
		if ( PlaneDiff (e->origin, &r_clipplane) < -skm_radius )
			return qtrue;
	}

	return qfalse;
}

/*
================
R_SkeletalModelLerpAttachment
================
*/
qboolean R_SkeletalModelLerpAttachment ( orientation_t *orient, mskmodel_t *skmodel, int framenum, int oldframenum, 
									  float backlerp, char *name )
{
	int i;
	mskbone_t		*bone;
	mskframe_t		*frame, *oldframe;
	mskbonepose_t	*bonepose, *oldbonepose;

	// find the appropriate attachment bone
	bone = skmodel->bones;
	for ( i = 0; i < skmodel->numbones; i++, bone++ )
	{
		if ( !(bone->flags & SKM_BONEFLAG_ATTACH) ) {
			continue;
		}
		if ( !Q_stricmp (bone->name, name) ) {
			break;
		}
	}

	if ( i == skmodel->numbones ) {
		Com_DPrintf ("R_SkeletalModelLerpAttachment: no such bone %s\n", name );
		return qfalse;
	}

	// ignore invalid frames
	if ( ( framenum >= skmodel->numframes ) || ( framenum < 0 ) )
	{
		Com_DPrintf ("R_SkeletalModelLerpAttachment %s: no such oldframe %i\n", name, framenum);
		framenum = 0;
	}
	if ( ( oldframenum >= skmodel->numframes ) || ( oldframenum < 0 ) )
	{
		Com_DPrintf ("R_SkeletalModelLerpAttachment %s: no such oldframe %i\n", name, oldframenum);
		oldframenum = 0;
	}

	frame = skmodel->frames + framenum;
	oldframe = skmodel->frames + oldframenum;

	bonepose = frame->boneposes + i;
	oldbonepose = oldframe->boneposes + i;

	// interpolate matrices
	orient->axis[0][0] = bonepose->matrix[0][0] + (oldbonepose->matrix[0][0] - bonepose->matrix[0][0]) * backlerp;
	orient->axis[0][1] = bonepose->matrix[0][1] + (oldbonepose->matrix[0][1] - bonepose->matrix[0][1]) * backlerp;
	orient->axis[0][2] = bonepose->matrix[0][2] + (oldbonepose->matrix[0][2] - bonepose->matrix[0][2]) * backlerp;
	orient->origin[0] = bonepose->matrix[0][3] + (oldbonepose->matrix[0][3] - bonepose->matrix[0][3]) * backlerp;
	orient->axis[1][0] = bonepose->matrix[1][0] + (oldbonepose->matrix[1][0] - bonepose->matrix[1][0]) * backlerp;
	orient->axis[1][1] = bonepose->matrix[1][1] + (oldbonepose->matrix[1][1] - bonepose->matrix[1][1]) * backlerp;
	orient->axis[1][2] = bonepose->matrix[1][2] + (oldbonepose->matrix[1][2] - bonepose->matrix[1][2]) * backlerp;
	orient->origin[1] = bonepose->matrix[1][3] + (oldbonepose->matrix[1][3] - bonepose->matrix[1][3]) * backlerp;
	orient->axis[2][0] = bonepose->matrix[2][0] + (oldbonepose->matrix[2][0] - bonepose->matrix[2][0]) * backlerp;
	orient->axis[2][1] = bonepose->matrix[2][1] + (oldbonepose->matrix[2][1] - bonepose->matrix[2][1]) * backlerp;
	orient->axis[2][2] = bonepose->matrix[2][2] + (oldbonepose->matrix[2][2] - bonepose->matrix[2][2]) * backlerp;
	orient->origin[2] = bonepose->matrix[2][3] + (oldbonepose->matrix[2][3] - bonepose->matrix[2][3]) * backlerp;

	return qtrue;
}

/*
================
R_DrawBonesFrameLerp
================
*/
void R_DrawBonesFrameLerp ( meshbuffer_t *mb, model_t *mod, float backlerp, qboolean shadow )
{
	int				i, meshnum;
	int				j, k, l;
	int				features;
	vec3_t			move, delta;
	mskmesh_t		*mesh;
	mskframe_t		*frame, *oldframe;
	mskbonepose_t	*bonepose, *oldbonepose;
	mskbonepose_t	*out = skmbonepose;
	mskvertex_t		*skmverts;
	mskbonevert_t	*boneverts;
	entity_t		*e = mb->entity;
	mskmodel_t		*skmodel = mod->skmodel;

	if ( !shadow && (e->flags & RF_VIEWERMODEL) && !r_mirrorview && !r_portalview ) {
		return;
	}

	meshnum = -(mb->infokey + 1);
	if ( meshnum < 0 || meshnum >= skmodel->nummeshes ) {
		return;
	}

	mesh = skmodel->meshes + meshnum;

#ifdef SHADOW_VOLUMES
	if ( shadow && !mesh->trneighbors ) {
		return;
	}
#endif

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract ( e->oldorigin, e->origin, delta );
	Matrix3_Multiply_Vec3 ( e->axis, delta, move );
	VectorScale ( move, e->scale * backlerp, move );

	frame = skmodel->frames + e->frame;
	oldframe = skmodel->frames + e->oldframe;
	for ( i = 0; i < mesh->numreferences; i++ )
	{
		out = skmbonepose + mesh->references[i];
		bonepose = frame->boneposes + mesh->references[i];
		oldbonepose = oldframe->boneposes + mesh->references[i];

		// interpolate matrices
		out->matrix[0][0] = (bonepose->matrix[0][0] + (oldbonepose->matrix[0][0] - bonepose->matrix[0][0]) * backlerp) * e->scale;
		out->matrix[0][1] = (bonepose->matrix[0][1] + (oldbonepose->matrix[0][1] - bonepose->matrix[0][1]) * backlerp) * e->scale;
		out->matrix[0][2] = (bonepose->matrix[0][2] + (oldbonepose->matrix[0][2] - bonepose->matrix[0][2]) * backlerp) * e->scale;
		out->matrix[0][3] = (bonepose->matrix[0][3] + (oldbonepose->matrix[0][3] - bonepose->matrix[0][3]) * backlerp) * e->scale;
		out->matrix[1][0] = (bonepose->matrix[1][0] + (oldbonepose->matrix[1][0] - bonepose->matrix[1][0]) * backlerp) * e->scale;
		out->matrix[1][1] = (bonepose->matrix[1][1] + (oldbonepose->matrix[1][1] - bonepose->matrix[1][1]) * backlerp) * e->scale;
		out->matrix[1][2] = (bonepose->matrix[1][2] + (oldbonepose->matrix[1][2] - bonepose->matrix[1][2]) * backlerp) * e->scale;
		out->matrix[1][3] = (bonepose->matrix[1][3] + (oldbonepose->matrix[1][3] - bonepose->matrix[1][3]) * backlerp) * e->scale;
		out->matrix[2][0] = (bonepose->matrix[2][0] + (oldbonepose->matrix[2][0] - bonepose->matrix[2][0]) * backlerp) * e->scale;
		out->matrix[2][1] = (bonepose->matrix[2][1] + (oldbonepose->matrix[2][1] - bonepose->matrix[2][1]) * backlerp) * e->scale;
		out->matrix[2][2] = (bonepose->matrix[2][2] + (oldbonepose->matrix[2][2] - bonepose->matrix[2][2]) * backlerp) * e->scale;
		out->matrix[2][3] = (bonepose->matrix[2][3] + (oldbonepose->matrix[2][3] - bonepose->matrix[2][3]) * backlerp) * e->scale;
	}

	skmverts = mesh->vertexes;
	for ( j = 0; j < mesh->numverts; j++, skmverts++ ) 
	{
		VectorCopy ( move, tempVertexArray[j] );
		VectorClear ( tempNormalsArray[j] );

		for ( l = 0, boneverts = skmverts->verts; l < skmverts->numbones; l++, boneverts++ )
		{
			bonepose = skmbonepose + boneverts->bonenum;

			for ( k = 0; k < 3; k++ )
			{
				tempVertexArray[j][k] += boneverts->origin[0] * bonepose->matrix[k][0] +
					boneverts->origin[1] * bonepose->matrix[k][1] +
					boneverts->origin[2] * bonepose->matrix[k][2] +
					boneverts->influence * bonepose->matrix[k][3];

				tempNormalsArray[j][k] += boneverts->normal[0] * bonepose->matrix[k][0] +
					boneverts->normal[1] * bonepose->matrix[k][1] +
					boneverts->normal[2] * bonepose->matrix[k][2];
			}
		}

		VectorNormalizeFast ( tempNormalsArray[j] );
	}

	if ( shadow ) {
		skm_mesh.st_array = NULL;
#ifdef SHADOW_VOLUMES
		skm_mesh.trneighbors = mesh->trneighbors;
#endif
	} else {
		skm_mesh.st_array = ( vec2_t * )mesh->stcoords;
#ifdef SHADOW_VOLUMES
		skm_mesh.trneighbors = NULL;
#endif
	}

	skm_mesh.indexes = mesh->indexes;
	skm_mesh.numindexes = mesh->numtris * 3;
	skm_mesh.numvertexes = mesh->numverts;

	features = MF_NONBATCHED | mb->shader->features;
	if ( r_shownormals->value && !shadow ) {
		features |= MF_NORMALS;
	}
	if ( mb->shader->flags & SHADER_AUTOSPRITE ) {
		features |= MF_NOCULL;
	}

	R_RotateForEntity ( e );

	R_PushMesh ( &skm_mesh, features );
	R_RenderMeshBuffer ( mb, shadow );

	if ( shadow ) {
		if ( r_shadows->value == 1) {
			R_Draw_SimpleShadow ( e );
		} else {
#ifdef SHADOW_VOLUMES
			R_SkeletalModelBBox ( e, mod );
			R_DrawShadowVolumes ( &skm_mesh );
#endif
		}
	}
}

/*
=================
R_DrawSkeletalModel
=================
*/
void R_DrawSkeletalModel ( meshbuffer_t *mb, qboolean shadow )
{
	entity_t *e = mb->entity;

	//
	// draw all the triangles
	//
	if (e->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	if ( ( e->flags & (RF_WEAPONMODEL|RF_LEFTHAND) ) == (RF_WEAPONMODEL|RF_LEFTHAND) ) {
		mat4_t m;

		Matrix4_Identity ( m );
		Matrix4_Scale ( m, -1, 1, 1 );

		qglMatrixMode( GL_PROJECTION );
		qglPushMatrix();
		qglLoadMatrixf ( m );

		MYgluPerspective( r_newrefdef.fov_y, ( float ) r_newrefdef.width / r_newrefdef.height, 4, 12288);
		qglMatrixMode( GL_MODELVIEW );

		qglFrontFace( GL_CW );
	}

	if ( !r_lerpmodels->value ) {
		e->backlerp = 0;
	}

	R_DrawBonesFrameLerp ( mb, R_SkeletalModelLODForDistance ( e ), e->backlerp, shadow );

	if ( ( e->flags & (RF_WEAPONMODEL|RF_LEFTHAND) ) == (RF_WEAPONMODEL|RF_LEFTHAND) ) {
		qglMatrixMode( GL_PROJECTION );
		qglPopMatrix();
		qglMatrixMode( GL_MODELVIEW );
		qglFrontFace( GL_CCW );
	}

	if ( e->flags & RF_DEPTHHACK ) {
		qglDepthRange (gldepthmin, gldepthmax);
	}
}

/*
=================
R_AddSkeletalModelToList
=================
*/
void R_AddSkeletalModelToList ( entity_t *e )
{
	int				i;
	mfog_t			*fog;
	model_t			*mod;
	shader_t		*shader;
	mskmesh_t		*mesh;
	mskmodel_t		*skmodel;

	mod = R_SkeletalModelLODForDistance ( e );
	if ( !(skmodel = mod->skmodel) ) {
		return;
	}

	R_SkeletalModelBBox ( e, mod );
	if ( !r_shadows->value && R_CullSkeletalModel( e, mod ) ) {
		return;
	}
	
	mesh = skmodel->meshes;
	fog = R_FogForSphere ( e->origin, skm_radius );

	for ( i = 0; i < skmodel->nummeshes; i++, mesh++ )
	{
		if ( e->customSkin ) {
			shader = R_SkinFile_FindShader ( e->customSkin, mesh->name );
			if ( shader ) {
				R_AddMeshToList ( fog, shader, -(i+1) );
			}
		} else if ( e->customShader )
			R_AddMeshToList ( fog, e->customShader, -(i+1) );
		else
			R_AddMeshToList ( fog, mesh->skin.shader, -(i+1) );
	}
}