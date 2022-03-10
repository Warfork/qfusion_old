/*
Copyright (C) 2001-2002 Victor Luchits

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
// r_alias.c: Quake 2 .md2 and Quake III Arena .md3 model formats support

#include "r_local.h"


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

static  mesh_t			alias_mesh;
static	meshbuffer_t	alias_mbuffer;

/*
==============================================================================

MD2 MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasMD2Model
=================
*/
void Mod_LoadAliasMD2Model (model_t *mod, void *buffer)
{
	int					i, j;
	int					version, framesize;
	int					skinwidth, skinheight;
	int					numverts, numindexes;
	double				isw, ish;
	vec3_t				scale;
	int					indremap[MD2_MAX_TRIANGLES*3];
	index_t				ptempindex[MD2_MAX_TRIANGLES*3], ptempstindex[MD2_MAX_TRIANGLES*3];
	dmd2_t				*pinmodel;
	dstvert_t			*pinst;
	dtriangle_t			*pintri;
	daliasframe_t		*pinframe;
	index_t				*poutindex;
	maliasmodel_t		*poutmodel;
	maliasmesh_t		*poutmesh;
	maliascoord_t		*poutcoord;
	maliasframe_t		*poutframe;
	maliasvertex_t		*poutvertex;
	maliasskin_t		*poutskin;

	pinmodel = (dmd2_t *)buffer;
	version = LittleLong (pinmodel->version);
	framesize = LittleLong (pinmodel->framesize);

	if (version != MD2_ALIAS_VERSION)
		Com_Error (ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, version, MD2_ALIAS_VERSION);

	poutmodel = mod->aliasmodel = Hunk_AllocName ( sizeof(maliasmodel_t), mod->name );

	// byte swap the header fields and sanity check
	skinwidth = LittleLong ( pinmodel->skinwidth );
	skinheight = LittleLong ( pinmodel->skinheight );

	if ( skinwidth <= 0 ) {
		Com_Error ( ERR_DROP, "model %s has invalid skin width", mod->name );
	}
	if ( skinheight <= 0 ) {
		Com_Error ( ERR_DROP, "model %s has invalid skin height", mod->name );
	}

	isw = 1.0 / (double)skinwidth;
	ish = 1.0 / (double)skinheight;

	poutmodel->numframes = LittleLong ( pinmodel->num_frames );
	poutmodel->numskins = LittleLong ( pinmodel->num_skins );

	if ( poutmodel->numframes > MD2_MAX_FRAMES ) {
		Com_Error ( ERR_DROP, "model %s has too many frames", mod->name );
	} else if ( poutmodel->numframes <= 0 ) {
		Com_Error ( ERR_DROP, "model %s has no frames", mod->name );
	}
	if ( poutmodel->numskins > MD2_MAX_SKINS ) {
		Com_Error ( ERR_DROP, "model %s has too many skins", mod->name );
	} else if ( poutmodel->numskins < 0 ) {
		Com_Error ( ERR_DROP, "model %s has invalid number of skins", mod->name );
	}

	poutmodel->numtags = 0;
	poutmodel->tags = NULL;

	poutmodel->nummeshes = 1;
	poutmesh = poutmodel->meshes = Hunk_AllocName ( sizeof(maliasmesh_t), mod->name );

	poutmesh->numverts = LittleLong ( pinmodel->num_xyz );
	poutmesh->numtris = LittleLong ( pinmodel->num_tris );

	if ( poutmesh->numverts <= 0 ) {
		Com_Error ( ERR_DROP, "model %s has no vertices", mod->name );
	} else if ( poutmesh->numverts > MD2_MAX_VERTS ) {
		Com_Error ( ERR_DROP, "model %s has too many vertices", mod->name );
	}
	if ( poutmesh->numtris > MD2_MAX_TRIANGLES ) {
		Com_Error ( ERR_DROP, "model %s has too many triangles", mod->name );
	} else if ( poutmesh->numtris <= 0 ) {
		Com_Error ( ERR_DROP, "model %s has no triangles", mod->name );
	}

//
// load triangle lists
//
	pintri = ( dtriangle_t * )( ( byte * )pinmodel + LittleLong (pinmodel->ofs_tris) );

	for (i=0 ; i<poutmesh->numtris ; i++, pintri++)
	{
		for (j=0 ; j<3 ; j++)
		{
			ptempindex[i*3+j] = (index_t)LittleShort (pintri->index_xyz[j]);
			ptempstindex[i*3+j] = (index_t)LittleShort (pintri->index_st[j]);
		}
	}

//
// build list of unique vertexes
//
	numverts = 0;
	numindexes = poutmesh->numtris * 3;
	poutindex = poutmesh->indexes = Hunk_AllocName ( numindexes * sizeof(index_t), mod->name );

	memset ( indremap, -1, MD2_MAX_TRIANGLES*3 * sizeof(int) );

	for ( i = 0; i < numindexes; i++ )
	{
		if ( indremap[i] != -1 ) {
			continue;
		}

		for ( j = 0; j < numindexes; j++ )
		{
			if ( j == i ) {
				continue;
			}

			if ( (ptempindex[i] == ptempindex[j]) && (ptempstindex[i] == ptempstindex[j]) ) {
				indremap[j] = i;
			}
		}
	}

	// count unique vertexes
	for ( i = 0; i < numindexes; i++ )
	{
		if ( indremap[i] != -1 ) {
			continue;
		}

		poutindex[i] = numverts++;
		indremap[i] = i;
	}

	poutmesh->numverts = numverts;

	// remap remaining indexes
	for ( i = 0; i < numindexes; i++ ) 
	{
		if ( indremap[i] != i ) {
			poutindex[i] = poutindex[indremap[i]];
		}
	}

//
// load base s and t vertices
//
	pinst = ( dstvert_t * ) ( ( byte * )pinmodel + LittleLong (pinmodel->ofs_st) );
	poutcoord = poutmesh->stcoords = Hunk_AllocName ( numverts * sizeof(maliascoord_t), mod->name );

	for (j=0 ; j<numindexes; j++)
	{
		poutcoord[poutindex[j]].st[0] = (float)(((double)LittleShort (pinst[ptempstindex[indremap[j]]].s) + 0.5f) * isw);
		poutcoord[poutindex[j]].st[1] = (float)(((double)LittleShort (pinst[ptempstindex[indremap[j]]].t) + 0.5f) * ish);
	}

//
// load the frames
//
	poutframe = poutmodel->frames = Hunk_AllocName ( poutmodel->numframes * sizeof(maliasframe_t), mod->name );
	poutvertex = poutmesh->vertexes = Hunk_AllocName ( poutmodel->numframes * numverts * sizeof(maliasvertex_t), mod->name );

	mod->radius = 0;
	ClearBounds ( mod->mins, mod->maxs );

	for (i=0 ; i<poutmodel->numframes ; i++, poutframe++, poutvertex += numverts)
	{
		pinframe = ( daliasframe_t * )( ( byte * )pinmodel + LittleLong (pinmodel->ofs_frames) + i * framesize );

		for (j=0 ; j<3 ; j++)
		{
			scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat (pinframe->translate[j]);
		}

		for (j=0 ; j<numindexes; j++)
		{
			// verts are all 8 bit, so no swapping needed
			poutvertex[poutindex[j]].point[0] = (float)pinframe->verts[ptempindex[indremap[j]]].v[0] * scale[0];
			poutvertex[poutindex[j]].point[1] = (float)pinframe->verts[ptempindex[indremap[j]]].v[1] * scale[1];
			poutvertex[poutindex[j]].point[2] = (float)pinframe->verts[ptempindex[indremap[j]]].v[2] * scale[2];

			VectorCopy ( r_avertexnormals[pinframe->verts[ptempindex[indremap[j]]].lightnormalindex], poutvertex[poutindex[j]].normal );
		}

		VectorCopy ( poutframe->translate, poutframe->mins );
		VectorMA ( poutframe->translate, 255, scale, poutframe->maxs );

		poutframe->radius = RadiusFromBounds ( poutframe->mins, poutframe->maxs );

		mod->radius = max ( mod->radius, poutframe->radius );
		AddPointToBounds ( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds ( poutframe->maxs, mod->mins, mod->maxs );
	}

//
// build triangle neighbors
//
	poutmesh->trneighbors = Hunk_AllocName ( sizeof(int) * poutmesh->numtris * 3, mod->name );
	R_BuildTriangleNeighbors ( poutmesh->trneighbors, poutmesh->indexes, poutmesh->numtris );

	// register all skins
	poutskin = poutmodel->skins = Hunk_AllocName ( poutmodel->numskins * sizeof(maliasskin_t), mod->name );

	for (i=0 ; i<poutmodel->numskins ; i++, poutskin++)
	{
		Q_strncpyz ( poutskin->name, ( char * )pinmodel + LittleLong (pinmodel->ofs_skins) 
			+ i * MD2_MAX_SKINNAME, MD2_MAX_SKINNAME );
	}

	mod->type = mod_alias;
}

/*
==============================================================================

MD3 MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasMD3Model
=================
*/
void Mod_LoadAliasMD3Model ( model_t *mod, void *buffer )
{
	int					version, i, j, l;
	float				sin_a, sin_b, cos_a, cos_b;
	dmd3header_t		*pinmodel;
	dmd3frame_t			*pinframe;
	dmd3tag_t			*pintag;
	dmd3mesh_t			*pinmesh;
	dmd3skin_t			*pinskin;
	dmd3coord_t			*pincoord;
	dmd3vertex_t		*pinvert;
	index_t				*pinindex, *poutindex;
	maliasvertex_t		*poutvert;
	maliascoord_t		*poutcoord;
	maliasskin_t		*poutskin;
	maliasmesh_t		*poutmesh;
	maliastag_t			*pouttag;
	maliasframe_t		*poutframe;
	maliasmodel_t		*poutmodel;

	pinmodel = ( dmd3header_t * )buffer;
	version = LittleLong( pinmodel->version );

	if ( version != MD3_ALIAS_VERSION ) {
		Com_Error ( ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, version, MD3_ALIAS_VERSION );
	}

	poutmodel = mod->aliasmodel = Hunk_AllocName ( sizeof(maliasmodel_t), mod->name );

	// byte swap the header fields and sanity check
	poutmodel->numframes = LittleLong ( pinmodel->num_frames );
	poutmodel->numtags = LittleLong ( pinmodel->num_tags );
	poutmodel->nummeshes = LittleLong ( pinmodel->num_meshes );
	poutmodel->numskins = 0;

	if ( poutmodel->numframes <= 0 ) {
		Com_Error ( ERR_DROP, "model %s has no frames", mod->name );
	} else if ( poutmodel->numframes > MD3_MAX_FRAMES ) {
		Com_Error ( ERR_DROP, "model %s has too many frames", mod->name );
	}
	if ( poutmodel->numtags > MD3_MAX_TAGS ) {
		Com_Error ( ERR_DROP, "model %s has too many tags", mod->name );
	} else if ( poutmodel->numtags < 0 ) {
		Com_Error ( ERR_DROP, "model %s has invalid number of tags", mod->name );
	}
	if ( poutmodel->nummeshes <= 0 ) {
		Com_Error ( ERR_DROP, "model %s has no meshes", mod->name );
	} else if ( poutmodel->nummeshes > MD3_MAX_MESHES ) {
		Com_Error ( ERR_DROP, "model %s has too many meshes", mod->name );
	}

//
// load the frames
//
	pinframe = ( dmd3frame_t * )( ( byte * )pinmodel + LittleLong (pinmodel->ofs_frames) );
	poutframe = poutmodel->frames = Hunk_AllocName ( sizeof(maliasframe_t) * poutmodel->numframes, mod->name );

	mod->radius = 0;
	ClearBounds ( mod->mins, mod->maxs );

	for ( i = 0; i < poutmodel->numframes; i++, pinframe++, poutframe++ ) {
		for ( j = 0; j < 3; j++ ) {
			poutframe->mins[j] = LittleFloat ( pinframe->mins[j] );
			poutframe->maxs[j] = LittleFloat ( pinframe->maxs[j] );
			poutframe->translate[j] = LittleFloat ( pinframe->translate[j] );
		}

		poutframe->radius = LittleFloat ( pinframe->radius );

		mod->radius = max ( mod->radius, poutframe->radius );
		AddPointToBounds ( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds ( poutframe->maxs, mod->mins, mod->maxs );
	}
	
//
// load the tags
//
	pintag = ( dmd3tag_t * )( ( byte * )pinmodel + LittleLong (pinmodel->ofs_tags) );
	pouttag = poutmodel->tags = Hunk_AllocName ( sizeof(maliastag_t) * poutmodel->numframes * poutmodel->numtags, mod->name );

	for ( i = 0; i < poutmodel->numframes; i++ ) {
		for ( l = 0; l < poutmodel->numtags; l++, pintag++, pouttag++ ) {
			for ( j = 0; j < 3; j++ ) {
				pouttag->orient.origin[j] = LittleFloat ( pintag->orient.origin[j] );
				pouttag->orient.axis[0][j] = LittleFloat ( pintag->orient.axis[0][j] );
				pouttag->orient.axis[1][j] = LittleFloat ( pintag->orient.axis[1][j] );
				pouttag->orient.axis[2][j] = LittleFloat ( pintag->orient.axis[2][j] );
			}

			Q_strncpyz ( pouttag->name, pintag->name, MD3_MAX_PATH );
		}
	}

//
// load the meshes
//
	pinmesh = ( dmd3mesh_t * )( ( byte * )pinmodel + LittleLong (pinmodel->ofs_meshes) );
	poutmesh = poutmodel->meshes = Hunk_AllocName ( sizeof(maliasmesh_t)*poutmodel->nummeshes, mod->name );

	for ( i = 0; i < poutmodel->nummeshes; i++, poutmesh++ ) {
		if ( strncmp ( (const char *)pinmesh->id, IDMD3HEADER, 4) ) {
			Com_Error ( ERR_DROP, "mesh %s in model %s has wrong id (%i should be %i)",
					 pinmesh->name, mod->name, LittleLong (pinmesh->id), IDMD3HEADER );
		}

		poutmesh->numtris = LittleLong ( pinmesh->num_tris );
		poutmesh->numskins = LittleLong ( pinmesh->num_skins );
		poutmesh->numverts = LittleLong ( pinmesh->num_verts );

		if ( poutmesh->numskins <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no skins", i, mod->name );
		} else if ( poutmesh->numskins > MD3_MAX_SHADERS ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has too many skins", i, mod->name );
		}
		if ( poutmesh->numtris <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no elements", i, mod->name );
		} else if ( poutmesh->numtris > MD3_MAX_TRIANGLES ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has too many triangles", i, mod->name );
		}
		if ( poutmesh->numverts <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no vertices", i, mod->name );
		} else if ( poutmesh->numverts > MD3_MAX_VERTS ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has too many vertices", i, mod->name );
		}

	//
	// load the skins
	//
		pinskin = ( dmd3skin_t * )( ( byte * )pinmesh + LittleLong (pinmesh->ofs_skins) );
		poutskin = poutmesh->skins = Hunk_AllocName ( sizeof(maliasskin_t) * poutmesh->numskins, mod->name );

		for ( j = 0, l = 0; j < poutmesh->numskins; j++, pinskin++, poutskin++ ) {
			COM_StripExtension ( pinskin->name, poutskin->name );
		}

	//
	// load the indexes
	//
		pinindex = ( index_t * )( ( byte * )pinmesh + LittleLong (pinmesh->ofs_indexes) );
		poutindex = poutmesh->indexes = Hunk_AllocName ( sizeof(index_t) * poutmesh->numtris * 3, mod->name );

		for ( j = 0; j < poutmesh->numtris; j++, pinindex += 3, poutindex += 3 ) {
			poutindex[0] = (index_t)LittleLong ( pinindex[0] );
			poutindex[1] = (index_t)LittleLong ( pinindex[1] );
			poutindex[2] = (index_t)LittleLong ( pinindex[2] );
		}

	//
	// load the texture coordinates
	//
		pincoord = ( dmd3coord_t * )( ( byte * )pinmesh + LittleLong (pinmesh->ofs_tcs) );
		poutcoord = poutmesh->stcoords = Hunk_AllocName ( sizeof(maliascoord_t) * poutmesh->numverts, mod->name );

		for ( j = 0; j < poutmesh->numverts; j++, pincoord++, poutcoord++ ) {
			poutcoord->st[0] = LittleFloat ( pincoord->st[0] );
			poutcoord->st[1] = LittleFloat ( pincoord->st[1] );
		}

	//
	// load the vertexes and normals
	//
		pinvert = ( dmd3vertex_t * )( ( byte * )pinmesh + LittleLong (pinmesh->ofs_verts) );
		poutvert = poutmesh->vertexes = Hunk_AllocName ( sizeof(maliasvertex_t) * poutmesh->numverts, mod->name );

		for ( l = 0; l < poutmodel->numframes; l++ ) {
			for ( j = 0; j < poutmesh->numverts; j++, pinvert++, poutvert++ ) {
				poutvert->point[0] = (float)LittleShort ( pinvert->point[0] ) * MD3_XYZ_SCALE;
				poutvert->point[1] = (float)LittleShort ( pinvert->point[1] ) * MD3_XYZ_SCALE;
				poutvert->point[2] = (float)LittleShort ( pinvert->point[2] ) * MD3_XYZ_SCALE;

				sin_a = (float)pinvert->norm[0] * M_TWOPI / 255.0;
				cos_a = cos ( sin_a );
				sin_a = sin ( sin_a );

				sin_b = (float)pinvert->norm[1] * M_TWOPI / 255.0;
				cos_b = cos ( sin_b );
				sin_b = sin ( sin_b );

				VectorSet ( poutvert->normal, cos_b * sin_a, sin_b * sin_a, cos_a );
			}
		}

	//
	// build triangle neighbors
	//
		poutmesh->trneighbors = Hunk_AllocName ( sizeof(int) * poutmesh->numtris * 3, mod->name );
		R_BuildTriangleNeighbors ( poutmesh->trneighbors, poutmesh->indexes, poutmesh->numtris );

		pinmesh = ( dmd3mesh_t * )( ( byte * )pinmesh + LittleLong (pinmesh->meshsize) );
	}

	mod->type = mod_alias;
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
R_InitAliasModels
=================
*/
void R_InitAliasModels (void)
{
	alias_mesh.xyz_array = tempVertexArray;
	alias_mesh.normals_array = tempNormalsArray;
	alias_mesh.trnormals = NULL;

	alias_mbuffer.mesh = &alias_mesh;
	alias_mbuffer.infokey = -1;
}

/*
=================
Mod_RegisterAliasModel
=================
*/
void Mod_RegisterAliasModel ( model_t *mod )
{
	int				i, j;
	maliasmodel_t	*model;
	maliasmesh_t	*mesh;
	maliasskin_t	*skin;

	if ( !(model = mod->aliasmodel) ) {
		return;
	}

	// register global skins first (md2)
	for ( j = 0, skin = model->skins; j < model->numskins; j++, skin++ ) {
		skin->shader = R_RegisterSkin ( skin->name );
	}

	// register skins for each mesh (md3)
	for ( i = 0, mesh = model->meshes; i < model->nummeshes; i++, mesh++ ) {
		for ( j = 0, skin = mesh->skins; j < mesh->numskins; j++, skin++ ) {
			skin->shader = R_RegisterSkin ( skin->name );
		}
	}
}

/*
** R_AliasModelBBox
*/
void R_AliasModelBBox ( entity_t *e )
{
	int				i;
	maliasmodel_t	*model = e->model->aliasmodel;
	maliasframe_t	*pframe, *poldframe;
	float			*thismins, *oldmins, *thismaxs, *oldmaxs;

	if ( ( e->frame >= model->numframes ) || ( e->frame < 0 ) )
	{
		Com_DPrintf ("R_DrawAliasModel %s: no such frame %d\n", e->model->name, e->frame);
		e->frame = 0;
	}
	if ( ( e->oldframe >= model->numframes ) || ( e->oldframe < 0 ) )
	{
		Com_DPrintf ("R_DrawAliasModel %s: no such oldframe %d\n", e->model->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = model->frames + e->frame;
	poldframe = model->frames + e->oldframe;

	/*
	** compute axially aligned mins and maxs
	*/
	if ( pframe == poldframe )
	{
		VectorCopy ( pframe->mins, alias_mesh.mins );
		VectorCopy ( pframe->maxs, alias_mesh.maxs );
		alias_mesh.radius = pframe->radius;
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
				alias_mesh.mins[i] = thismins[i];
			else
				alias_mesh.mins[i] = oldmins[i];

			if ( thismaxs[i] > oldmaxs[i] )
				alias_mesh.maxs[i] = thismaxs[i];
			else
				alias_mesh.maxs[i] = oldmaxs[i];
		}

		alias_mesh.radius = max ( poldframe->radius, pframe->radius );
	}

	if ( e->scale != 1.0f ) {
		VectorScale ( alias_mesh.mins, e->scale, alias_mesh.mins );
		VectorScale ( alias_mesh.maxs, e->scale, alias_mesh.maxs );
		alias_mesh.radius *= e->scale;
	}
}

/*
** R_CullAliasModel
*/
qboolean R_CullAliasModel( entity_t *e )
{
	R_AliasModelBBox ( e );

	if ( e->flags & RF_WEAPONMODEL ) {
		return false;
	}
	if ( e->flags & RF_VIEWERMODEL ) {
		return !(r_mirrorview || r_portalview);
	}

	if ( R_CullSphere (e->origin, alias_mesh.radius) ) {
		return true;
	}

	if ( (r_mirrorview || r_portalview) && !r_nocull->value ) {
		if ( PlaneDiff (e->origin, &r_clipplane) < -alias_mesh.radius )
			return true;
	}

	return false;
}

/*
=============
R_DrawAliasFrameLerp

Interpolates between two frames and origins
=============
*/
void R_DrawAliasFrameLerp ( meshbuffer_t *mb, float backlerp, qboolean shadow )
{
	int				i, meshnum;
	int				features;
	vec3_t			move, delta;
	maliasframe_t	*frame, *oldframe;
	maliasmesh_t	*mesh;
	maliasvertex_t	*v, *ov;
	entity_t		*e = mb->entity;
	maliasmodel_t	*model = e->model->aliasmodel;

	if ( !shadow && !r_entvisframe[e->number][(r_portalview || r_mirrorview) ? 1 : 0] ) {
		return;
	}

	meshnum = -mb->infokey - 1;
	if ( meshnum < 0 || meshnum >= model->nummeshes ) {
		return;
	}

	mesh = model->meshes + meshnum;

	if ( shadow && !mesh->trneighbors ) {
		return;
	}

	frame = model->frames + e->frame;
	oldframe = model->frames + e->oldframe;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract ( e->oldorigin, e->origin, delta );
	Matrix3_Multiply_Vec3 ( e->axis, delta, move );
	VectorAdd ( move, oldframe->translate, move );

	for (i=0 ; i<3 ; i++)
	{
		move[i] = frame->translate[i] + (move[i] - frame->translate[i]) * backlerp;
	}

	v = mesh->vertexes + e->frame*mesh->numverts;
	ov = mesh->vertexes + e->oldframe*mesh->numverts;

	if ( !shadow ) {
		for ( i = 0; i < mesh->numverts; i++, v++, ov++ ) {
			VectorSet ( tempVertexArray[i], 
				move[0] + v->point[0] + (ov->point[0] - v->point[0])*backlerp,
				move[1] + v->point[1] + (ov->point[1] - v->point[1])*backlerp,
				move[2] + v->point[2] + (ov->point[2] - v->point[2])*backlerp );
			VectorSet ( tempNormalsArray[i],
				v->normal[0] + (ov->normal[0] - v->normal[0])*backlerp,
				v->normal[1] + (ov->normal[1] - v->normal[1])*backlerp,
				v->normal[2] + (ov->normal[2] - v->normal[2])*backlerp );
		}
	} else {
		for ( i = 0; i < mesh->numverts; i++, v++, ov++ ) {
			VectorSet ( tempVertexArray[i], 
				move[0] + v->point[0] + (ov->point[0] - v->point[0])*backlerp,
				move[1] + v->point[1] + (ov->point[1] - v->point[1])*backlerp,
				move[2] + v->point[2] + (ov->point[2] - v->point[2])*backlerp );
		}
	}

	if ( shadow ) {
		alias_mesh.st_array = NULL;
		alias_mesh.trneighbors = mesh->trneighbors;
	} else {
		alias_mesh.st_array = ( vec2_t * )mesh->stcoords;
		alias_mesh.trneighbors = NULL;
	}

	alias_mesh.indexes = mesh->indexes;
	alias_mesh.numindexes = mesh->numtris * 3;
	alias_mesh.numvertexes = mesh->numverts;

	alias_mbuffer.shader = mb->shader;
	alias_mbuffer.entity = e;
	alias_mbuffer.fog = mb->fog;

	features = MF_NONBATCHED | mb->shader->features;
	if ( r_shownormals->value ) {
		features |= MF_NORMALS;
	}

	R_PushMesh ( &alias_mesh, features );
	R_RenderMeshBuffer ( &alias_mbuffer, shadow );

	if ( shadow ) {
		R_AliasModelBBox ( e );
		R_DrawShadowVolumes ( &alias_mesh );
	}
}

/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel ( meshbuffer_t *mb, qboolean shadow )
{
	entity_t *e = mb->entity;

	//
	// draw all the triangles
	//
	if (e->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	if ( ( e->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) )
	{
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

	R_RotateForEntity ( e );

	R_DrawAliasFrameLerp ( mb, e->backlerp, shadow );

	if ( ( e->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) ) {
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
R_AddAliasModelToList
=================
*/
void R_AddAliasModelToList (entity_t *e)
{
	int				i, j;
	mfog_t			*fog;
	maliasmodel_t	*model;
	maliasmesh_t	*mesh;

	if ( (e->flags & RF_WEAPONMODEL) && (r_lefthand->value == 2) ) {
		return;
	}
	if ( !(model = e->model->aliasmodel) ) {
		return;
	}
	if ( R_CullAliasModel( e ) ) {
		if ( !r_shadows->value ) {
			return;
		}
	} else {
		r_entvisframe[e->number][(r_portalview || r_mirrorview) ? 1 : 0] = r_framecount;
	}

	fog = R_FogForSphere ( e->origin, alias_mesh.radius );

	for ( i = 0, mesh = model->meshes; i < model->nummeshes; i++, mesh++ )
	{
		if ( e->customShader ) {
			R_AddMeshToBuffer ( NULL, fog, NULL, e->customShader, -(i+1) );
		} else if ( (e->skinnum >= 0) && (e->skinnum < model->numskins) ) {
			R_AddMeshToBuffer ( NULL, fog, NULL, model->skins[e->skinnum].shader, -(i+1) );
		} else {
			for ( j = 0; j < mesh->numskins; j++ ) {
				R_AddMeshToBuffer ( NULL, fog, NULL, mesh->skins[j].shader, -(i+1) );
			}
		}
	}
}
