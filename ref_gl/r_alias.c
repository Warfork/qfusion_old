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
// r_alias.c: Quake 2 .md2 and Quake III Arena .md3 model formats support

#include "r_local.h"

static  mesh_t			alias_mesh;

static	vec3_t			alias_mins;
static	vec3_t			alias_maxs;
static	float			alias_radius;

/*
=================
Mod_AliasCalculateVertexNormals
=================
*/
void Mod_AliasCalculateVertexNormals ( int numIndexes, index_t *indexes, int numVerts, maliasvertex_t *v, qboolean flat )
{
	int i, j, k, vertRemap[MD2_MAX_VERTS];
	vec3_t dir1, dir2, normal, trnormals[MD2_MAX_TRIANGLES];
	int numUniqueVerts, uniqueVerts[MD2_MAX_VERTS];
	qbyte latlongs[MD2_MAX_VERTS][2];

	if( flat ) {
		for( i = 0, numUniqueVerts = 0; i < numVerts; i++ ) {
			vertRemap[i] = numUniqueVerts;
			uniqueVerts[numUniqueVerts++] = i;
		}
	} else {	// count unique verts
		for( i = 0, numUniqueVerts = 0; i < numVerts; i++ ) {
			for( j = 0; j < numUniqueVerts; j++ ) {
				if( VectorCompare (v[uniqueVerts[j]].point, v[i].point) ) {
					vertRemap[i] = j;
					break;
				}
			}

			if( j == numUniqueVerts ) {
				vertRemap[i] = numUniqueVerts;
				uniqueVerts[numUniqueVerts++] = i;
			}
		}
	}

	for( i = 0, j = 0; i < numIndexes; i += 3, j++ ) {
		// calculate two mostly perpendicular edge directions
		VectorSubtract( v[indexes[i+0]].point, v[indexes[i+1]].point, dir1 );
		VectorSubtract( v[indexes[i+2]].point, v[indexes[i+1]].point, dir2 );

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the surface normal
		CrossProduct( dir1, dir2, trnormals[j] );
		VectorNormalize( trnormals[j] );
	}

	// sum all triangle normals
	for( i = 0; i < numUniqueVerts; i++ ) {
		VectorClear( normal );

		for( j = 0, k = 0; j < numIndexes; j += 3, k++ ) {
			if( vertRemap[indexes[j+0]] == i 
				|| vertRemap[indexes[j+1]] == i
				|| vertRemap[indexes[j+2]] == i )
				VectorAdd( normal, trnormals[k], normal );
		}

		VectorNormalize( normal );
		NormToLatLong( normal, latlongs[i] );
	}

	// copy normals back
	for( i = 0; i < numVerts; i++ )
		*(short *)v[i].latlong = *(short *)latlongs[vertRemap[i]];
}

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
void Mod_LoadAliasMD2Model (model_t *mod, model_t *parent, void *buffer)
{
	int					i, j;
	int					version, framesize;
	int					skinwidth, skinheight;
	int					numverts, numindexes;
	double				isw, ish;
	int					indremap[MD2_MAX_TRIANGLES*3];
	index_t				ptempindex[MD2_MAX_TRIANGLES*3], ptempstindex[MD2_MAX_TRIANGLES*3];
	dmd2_t				*pinmodel;
	dstvert_t			*pinst;
	dtriangle_t			*pintri;
	daliasframe_t		*pinframe;
	index_t				*poutindex;
	maliasmodel_t		*poutmodel;
	maliasmesh_t		*poutmesh;
	vec2_t				*poutcoord;
	maliasframe_t		*poutframe;
	maliasvertex_t		*poutvertex;
	maliasskin_t		*poutskin;

	pinmodel = (dmd2_t *)buffer;
	version = LittleLong( pinmodel->version );
	framesize = LittleLong( pinmodel->framesize );

	if( version != MD2_ALIAS_VERSION )
		Com_Error( ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, version, MD2_ALIAS_VERSION );

	mod->type = mod_alias;
	mod->aliasmodel = poutmodel = Mod_Malloc( mod, sizeof(maliasmodel_t) );
	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	// byte swap the header fields and sanity check
	skinwidth = LittleLong( pinmodel->skinwidth );
	skinheight = LittleLong( pinmodel->skinheight );

	if( skinwidth <= 0 )
		Com_Error( ERR_DROP, "model %s has invalid skin width", mod->name );
	if( skinheight <= 0 )
		Com_Error( ERR_DROP, "model %s has invalid skin height", mod->name );

	isw = 1.0 / (double)skinwidth;
	ish = 1.0 / (double)skinheight;

	poutmodel->numframes = LittleLong( pinmodel->num_frames );
	poutmodel->numskins = LittleLong( pinmodel->num_skins );

	if( poutmodel->numframes > MD2_MAX_FRAMES )
		Com_Error( ERR_DROP, "model %s has too many frames", mod->name );
	else if( poutmodel->numframes <= 0 )
		Com_Error( ERR_DROP, "model %s has no frames", mod->name );
	if( poutmodel->numskins > MD2_MAX_SKINS )
		Com_Error( ERR_DROP, "model %s has too many skins", mod->name );
	else if ( poutmodel->numskins < 0 )
		Com_Error( ERR_DROP, "model %s has invalid number of skins", mod->name );

	poutmodel->numtags = 0;
	poutmodel->tags = NULL;
	poutmodel->nummeshes = 1;

	poutmesh = poutmodel->meshes = Mod_Malloc( mod, sizeof(maliasmesh_t) );
	Q_strncpyz( poutmesh->name, "default", MD3_MAX_PATH );

	poutmesh->numverts = LittleLong( pinmodel->num_xyz );
	poutmesh->numtris = LittleLong( pinmodel->num_tris );

	if( poutmesh->numverts <= 0 )
		Com_Error( ERR_DROP, "model %s has no vertices", mod->name );
	else if( poutmesh->numverts > MD2_MAX_VERTS )
		Com_Error( ERR_DROP, "model %s has too many vertices", mod->name );
	if( poutmesh->numtris > MD2_MAX_TRIANGLES )
		Com_Error( ERR_DROP, "model %s has too many triangles", mod->name );
	else if ( poutmesh->numtris <= 0 )
		Com_Error( ERR_DROP, "model %s has no triangles", mod->name );

//
// load triangle lists
//
	pintri = ( dtriangle_t * )( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_tris ) );

	for( i = 0; i < poutmesh->numtris; i++, pintri++ ) {
		for( j = 0; j < 3; j++ ) {
			ptempindex[i*3+j] = ( index_t )LittleShort( pintri->index_xyz[j] );
			ptempstindex[i*3+j] = ( index_t )LittleShort( pintri->index_st[j] );
		}
	}

//
// build list of unique vertexes
//
	numverts = 0;
	numindexes = poutmesh->numtris * 3;
	poutindex = poutmesh->indexes = Mod_Malloc ( mod, numindexes * sizeof(index_t) );
	memset( indremap, -1, MD2_MAX_TRIANGLES*3 * sizeof(int) );

	for( i = 0; i < numindexes; i++ ) {
		if( indremap[i] != -1 )
			continue;

		for( j = 0; j < numindexes; j++ ) {
			if( j == i )
				continue;

			if( (ptempindex[i] == ptempindex[j]) && (ptempstindex[i] == ptempstindex[j]) )
				indremap[j] = i;
		}
	}

	// count unique vertexes
	for( i = 0; i < numindexes; i++ ) {
		if( indremap[i] != -1 )
			continue;

		poutindex[i] = numverts++;
		indremap[i] = i;
	}

	Com_DPrintf ( "%s: remapped %i verts to %i\n", mod->name, poutmesh->numverts, numverts );

	poutmesh->numverts = numverts;

	// remap remaining indexes
	for( i = 0; i < numindexes; i++ ) {
		if( indremap[i] != i )
			poutindex[i] = poutindex[indremap[i]];
	}

//
// load base s and t vertices
//
	pinst = ( dstvert_t * ) ( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_st ) );
	poutcoord = poutmesh->stcoords = Mod_Malloc( mod, numverts * sizeof(vec2_t) );

	for( j = 0; j < numindexes; j++ ) {
		poutcoord[poutindex[j]][0] = (float)(((double)LittleShort (pinst[ptempstindex[indremap[j]]].s) + 0.5) * isw);
		poutcoord[poutindex[j]][1] = (float)(((double)LittleShort (pinst[ptempstindex[indremap[j]]].t) + 0.5) * ish);
	}

//
// load the frames
//
	poutframe = poutmodel->frames = Mod_Malloc( mod, poutmodel->numframes * sizeof(maliasframe_t) );
	poutvertex = poutmesh->vertexes = Mod_Malloc( mod, poutmodel->numframes * numverts * sizeof(maliasvertex_t) );

	for( i = 0; i < poutmodel->numframes; i++, poutframe++, poutvertex += numverts ) {
		pinframe = ( daliasframe_t * )( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_frames ) + i * framesize );

		for( j = 0; j < 3; j++ ) {
			poutframe->scale[j] = LittleFloat( pinframe->scale[j] );
			poutframe->translate[j] = LittleFloat( pinframe->translate[j] );
		}

		for( j = 0; j < numindexes; j++ ) {	// verts are all 8 bit, so no swapping needed
			poutvertex[poutindex[j]].point[0] = (short)pinframe->verts[ptempindex[indremap[j]]].v[0];
			poutvertex[poutindex[j]].point[1] = (short)pinframe->verts[ptempindex[indremap[j]]].v[1];
			poutvertex[poutindex[j]].point[2] = (short)pinframe->verts[ptempindex[indremap[j]]].v[2];
		}

		Mod_AliasCalculateVertexNormals( numindexes, poutindex, numverts, poutvertex, qfalse );

		VectorCopy( poutframe->translate, poutframe->mins );
		VectorMA( poutframe->translate, 255, poutframe->scale, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );

		mod->radius = max( mod->radius, poutframe->radius );
		AddPointToBounds( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds( poutframe->maxs, mod->mins, mod->maxs );
	}

//
// build triangle neighbors
//
#if SHADOW_VOLUMES
	poutmesh->trneighbors = Mod_Malloc( mod, sizeof(int) * poutmesh->numtris * 3 );
	R_BuildTriangleNeighbors( poutmesh->trneighbors, poutmesh->indexes, poutmesh->numtris );
#endif

	// register all skins
	poutskin = poutmodel->skins = Mod_Malloc( mod, poutmodel->numskins * sizeof(maliasskin_t) );

	for( i = 0; i < poutmodel->numskins; i++, poutskin++ ) {
		if( LittleLong( pinmodel->ofs_skins ) == -1 )
			continue;
		poutskin->shader = R_RegisterSkin( ( char * )pinmodel + LittleLong( pinmodel->ofs_skins ) + i*MD2_MAX_SKINNAME );
	}
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
void Mod_LoadAliasMD3Model ( model_t *mod, model_t *parent, void *buffer )
{
	int					version, i, j, l;
	dmd3header_t		*pinmodel;
	dmd3frame_t			*pinframe;
	dmd3tag_t			*pintag;
	dmd3mesh_t			*pinmesh;
	dmd3skin_t			*pinskin;
	dmd3coord_t			*pincoord;
	dmd3vertex_t		*pinvert;
	index_t				*pinindex, *poutindex;
	maliasvertex_t		*poutvert;
	vec2_t				*poutcoord;
	maliasskin_t		*poutskin;
	maliasmesh_t		*poutmesh;
	maliastag_t			*pouttag;
	maliasframe_t		*poutframe;
	maliasmodel_t		*poutmodel;

	pinmodel = ( dmd3header_t * )buffer;
	version = LittleLong( pinmodel->version );

	if ( version != MD3_ALIAS_VERSION )
		Com_Error( ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, version, MD3_ALIAS_VERSION );

	mod->type = mod_alias;
	mod->aliasmodel = poutmodel = Mod_Malloc( mod, sizeof(maliasmodel_t) );
	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	// byte swap the header fields and sanity check
	poutmodel->numframes = LittleLong( pinmodel->num_frames );
	poutmodel->numtags = LittleLong( pinmodel->num_tags );
	poutmodel->nummeshes = LittleLong( pinmodel->num_meshes );
	poutmodel->numskins = 0;

	if( poutmodel->numframes <= 0 )
		Com_Error( ERR_DROP, "model %s has no frames", mod->name );
	else if( poutmodel->numframes > MD3_MAX_FRAMES )
		Com_Error( ERR_DROP, "model %s has too many frames", mod->name );

	if( poutmodel->numtags > MD3_MAX_TAGS )
		Com_Error( ERR_DROP, "model %s has too many tags", mod->name );
	else if( poutmodel->numtags < 0 )
		Com_Error( ERR_DROP, "model %s has invalid number of tags", mod->name );

	if( poutmodel->nummeshes < 0 )
		Com_Error( ERR_DROP, "model %s has invalid number of meshes", mod->name );
	else if( !poutmodel->nummeshes && !poutmodel->numtags )
		Com_Error( ERR_DROP, "model %s has no meshes and no tags", mod->name );
	else if( poutmodel->nummeshes > MD3_MAX_MESHES )
		Com_Error( ERR_DROP, "model %s has too many meshes", mod->name );

//
// load the frames
//
	pinframe = ( dmd3frame_t * )( ( qbyte * )pinmodel + LittleLong (pinmodel->ofs_frames) );
	poutframe = poutmodel->frames = Mod_Malloc ( mod, sizeof(maliasframe_t) * poutmodel->numframes );

	for( i = 0; i < poutmodel->numframes; i++, pinframe++, poutframe++ ) {
		for( j = 0; j < 3; j++ ) {
			poutframe->scale[j] = MD3_XYZ_SCALE;
			poutframe->translate[j] = LittleFloat( pinframe->translate[j] );
		}

		// never trust the modeler utility and recalculate bbox and radius
		ClearBounds( poutframe->mins, poutframe->maxs );
	}
	
//
// load the tags
//
	pintag = ( dmd3tag_t * )( ( qbyte * )pinmodel + LittleLong (pinmodel->ofs_tags) );
	pouttag = poutmodel->tags = Mod_Malloc( mod, sizeof(maliastag_t) * poutmodel->numframes * poutmodel->numtags );

	for( i = 0; i < poutmodel->numframes; i++ ) {
		for( l = 0; l < poutmodel->numtags; l++, pintag++, pouttag++ ) {
			for ( j = 0; j < 3; j++ ) {
				vec3_t axis[3];

				axis[0][j] = LittleFloat( pintag->axis[0][j] );
				axis[1][j] = LittleFloat( pintag->axis[1][j] );
				axis[2][j] = LittleFloat( pintag->axis[2][j] );
				Matrix_Quat( axis, pouttag->quat );
				Quat_Normalize( pouttag->quat );
				pouttag->origin[j] = LittleFloat( pintag->origin[j] );
			}

			Q_strncpyz( pouttag->name, pintag->name, MD3_MAX_PATH );
		}
	}

//
// load the meshes
//
	pinmesh = ( dmd3mesh_t * )( ( qbyte * )pinmodel + LittleLong (pinmodel->ofs_meshes) );
	poutmesh = poutmodel->meshes = Mod_Malloc ( mod, sizeof(maliasmesh_t)*poutmodel->nummeshes );

	for( i = 0; i < poutmodel->nummeshes; i++, poutmesh++ ) {
		if( strncmp( (const char *)pinmesh->id, IDMD3HEADER, 4) )
			Com_Error( ERR_DROP, "mesh %s in model %s has wrong id (%i should be %i)",
					 pinmesh->name, mod->name, LittleLong (pinmesh->id), IDMD3HEADER );

		Q_strncpyz( poutmesh->name, pinmesh->name, MD3_MAX_PATH );

		Mod_StripLODSuffix( poutmesh->name );

		poutmesh->numtris = LittleLong( pinmesh->num_tris );
		poutmesh->numskins = LittleLong( pinmesh->num_skins );
		poutmesh->numverts = LittleLong( pinmesh->num_verts );

		if( poutmesh->numskins <= 0 )
			Com_Error ( ERR_DROP, "mesh %i in model %s has no skins", i, mod->name );
		else if( poutmesh->numskins > MD3_MAX_SHADERS )
			Com_Error ( ERR_DROP, "mesh %i in model %s has too many skins", i, mod->name );
		if( poutmesh->numtris <= 0 )
			Com_Error ( ERR_DROP, "mesh %i in model %s has no elements", i, mod->name );
		else if( poutmesh->numtris > MD3_MAX_TRIANGLES )
			Com_Error ( ERR_DROP, "mesh %i in model %s has too many triangles", i, mod->name );
		if( poutmesh->numverts <= 0 )
			Com_Error ( ERR_DROP, "mesh %i in model %s has no vertices", i, mod->name );
		else if ( poutmesh->numverts > MD3_MAX_VERTS )
			Com_Error ( ERR_DROP, "mesh %i in model %s has too many vertices", i, mod->name );

	//
	// load the skins
	//
		pinskin = ( dmd3skin_t * )( ( qbyte * )pinmesh + LittleLong (pinmesh->ofs_skins) );
		poutskin = poutmesh->skins = Mod_Malloc ( mod, sizeof(maliasskin_t) * poutmesh->numskins );

		for( j = 0; j < poutmesh->numskins; j++, pinskin++, poutskin++ )
			poutskin->shader = R_RegisterSkin( pinskin->name );

	//
	// load the indexes
	//
		pinindex = ( index_t * )( ( qbyte * )pinmesh + LittleLong (pinmesh->ofs_indexes) );
		poutindex = poutmesh->indexes = Mod_Malloc ( mod, poutmesh->numtris * sizeof(index_t) * 3 );

		for( j = 0; j < poutmesh->numtris; j++, pinindex += 3, poutindex += 3 ) {
			poutindex[0] = (index_t)LittleLong( pinindex[0] );
			poutindex[1] = (index_t)LittleLong( pinindex[1] );
			poutindex[2] = (index_t)LittleLong( pinindex[2] );
		}

	//
	// load the texture coordinates
	//
		pincoord = ( dmd3coord_t * )( ( qbyte * )pinmesh + LittleLong (pinmesh->ofs_tcs) );
		poutcoord = poutmesh->stcoords = Mod_Malloc ( mod, poutmesh->numverts * sizeof(vec2_t) );

		for( j = 0; j < poutmesh->numverts; j++, pincoord++ ) {
			poutcoord[j][0] = LittleFloat( pincoord->st[0] );
			poutcoord[j][1] = LittleFloat( pincoord->st[1] );
		}

	//
	// load the vertexes and normals
	//
		pinvert = ( dmd3vertex_t * )( ( qbyte * )pinmesh + LittleLong (pinmesh->ofs_verts) );
		poutvert = poutmesh->vertexes = Mod_Malloc ( mod, sizeof(maliasvertex_t) * poutmodel->numframes * poutmesh->numverts );
		poutframe = poutmodel->frames;

		for( l = 0; l < poutmodel->numframes; l++, poutframe++ ) {
			vec3_t v;

			for( j = 0; j < poutmesh->numverts; j++, pinvert++, poutvert++ ) {
				poutvert->point[0] = LittleShort( pinvert->point[0] );
				poutvert->point[1] = LittleShort( pinvert->point[1] );
				poutvert->point[2] = LittleShort( pinvert->point[2] );

				VectorCopy( poutvert->point, v );
				AddPointToBounds( v, poutframe->mins, poutframe->maxs );

				poutvert->latlong[0] = pinvert->norm[0];
				poutvert->latlong[1] = pinvert->norm[1];
			}
		}

	//
	// build triangle neighbors
	//
#if SHADOW_VOLUMES
		poutmesh->trneighbors = Mod_Malloc( mod, sizeof(int) * poutmesh->numtris * 3 );
		R_BuildTriangleNeighbors ( poutmesh->trneighbors, poutmesh->indexes, poutmesh->numtris );
#endif

		pinmesh = ( dmd3mesh_t * )( ( qbyte * )pinmesh + LittleLong (pinmesh->meshsize) );
	}

//
// calculate model bounds
//
	poutframe = poutmodel->frames;
	for( i = 0; i < poutmodel->numframes; i++, poutframe++ ) {
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->mins, poutframe->mins );
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->maxs, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );

		AddPointToBounds( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds( poutframe->maxs, mod->mins, mod->maxs );
		mod->radius = max( mod->radius, poutframe->radius );
	}
}

/*
=============
R_AliasLODForDistance
=============
*/
static model_t *R_AliasLODForDistance( entity_t *e )
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
=============
R_AliasModelBBox
=============
*/
void R_AliasModelBBox ( entity_t *e, model_t *mod )
{
	int				i;
	maliasmodel_t	*aliasmodel = mod->aliasmodel;
	maliasframe_t	*pframe, *poldframe;
	float			*thismins, *oldmins, *thismaxs, *oldmaxs;

	if ( ( e->frame >= aliasmodel->numframes ) || ( e->frame < 0 ) )
	{
		Com_DPrintf ("R_DrawAliasModel %s: no such frame %d\n", mod->name, e->frame);
		e->frame = 0;
	}
	if ( ( e->oldframe >= aliasmodel->numframes ) || ( e->oldframe < 0 ) )
	{
		Com_DPrintf ("R_DrawAliasModel %s: no such oldframe %d\n", mod->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = aliasmodel->frames + e->frame;
	poldframe = aliasmodel->frames + e->oldframe;

	// compute axially aligned mins and maxs
	if( pframe == poldframe ) {
		VectorCopy( pframe->mins, alias_mins );
		VectorCopy( pframe->maxs, alias_maxs );
		alias_radius = pframe->radius;
	} else {
		thismins = pframe->mins;
		thismaxs = pframe->maxs;

		oldmins = poldframe->mins;
		oldmaxs = poldframe->maxs;

		for( i = 0; i < 3; i++ ) {
			alias_mins[i] = min( thismins[i], oldmins[i] );
			alias_maxs[i] = max( thismaxs[i], oldmaxs[i] );
		}
		alias_radius = RadiusFromBounds( thismins, thismaxs );
	}

	if( e->scale != 1.0f ) {
		VectorScale( alias_mins, e->scale, alias_mins );
		VectorScale( alias_maxs, e->scale, alias_maxs );
		alias_radius *= e->scale;
	}
}

/*
=============
R_CullAliasModel
=============
*/
qboolean R_CullAliasModel( entity_t *e, model_t *mod )
{
	int i;
	vec3_t bbox[8];

	if( e->flags & RF_WEAPONMODEL )
		return qfalse;
	if( e->flags & RF_VIEWERMODEL )
		return !(r_mirrorview || r_portalview);

	if( r_spherecull->integer ) {
		if( R_CullSphere( e->origin, alias_radius, 15 ) )
			return qtrue;
	} else {
		// compute and rotate a full bounding box
		for( i = 0; i < 8; i++ ) {
			vec3_t tmp;

			tmp[0] = ( ( i & 1 ) ? alias_mins[0] : alias_maxs[0] );
			tmp[1] = ( ( i & 2 ) ? alias_mins[1] : alias_maxs[1] );
			tmp[2] = ( ( i & 4 ) ? alias_mins[2] : alias_maxs[2] );

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

	if( R_VisCullSphere( e->origin, alias_radius ) )
		return qtrue;

	if( (r_mirrorview || r_portalview) && !r_nocull->integer ) {
		if( PlaneDiff( e->origin, &r_clipplane ) < -alias_radius )
			return qtrue;
	}

	return qfalse;
}

/*
=============
R_AliasModelLerpTag
=============
*/
qboolean R_AliasModelLerpTag( orientation_t *orient, maliasmodel_t *aliasmodel, int oldframenum, int framenum, float lerpfrac, char *name )
{
	int			i;
	quat_t		quat;
	maliastag_t	*tag, *oldtag;

	// find the appropriate tag
	for( i = 0; i < aliasmodel->numtags; i++ ) {
		if( !Q_stricmp( aliasmodel->tags[i].name, name ) )
			break;
	}

	if( i == aliasmodel->numtags ) {
		Com_DPrintf ("R_AliasModelLerpTag: no such tag %s\n", name );
		return qfalse;
	}

	// ignore invalid frames
	if( ( framenum >= aliasmodel->numframes ) || ( framenum < 0 ) ) {
		Com_DPrintf( "R_AliasModelLerpTag %s: no such oldframe %i\n", name, framenum );
		framenum = 0;
	}
	if ( ( oldframenum >= aliasmodel->numframes ) || ( oldframenum < 0 ) ) {
		Com_DPrintf( "R_AliasModelLerpTag %s: no such oldframe %i\n", name, oldframenum );
		oldframenum = 0;
	}

	tag = aliasmodel->tags + framenum * aliasmodel->numtags + i;
	oldtag = aliasmodel->tags + oldframenum * aliasmodel->numtags + i;

	// interpolate axis and origin
	Quat_Lerp( oldtag->quat, tag->quat, lerpfrac, quat );
	Quat_Matrix( quat, orient->axis );

	orient->origin[0] = oldtag->origin[0] + (tag->origin[0] - oldtag->origin[0]) * lerpfrac;
	orient->origin[1] = oldtag->origin[1] + (tag->origin[1] - oldtag->origin[1]) * lerpfrac;
	orient->origin[2] = oldtag->origin[2] + (tag->origin[2] - oldtag->origin[2]) * lerpfrac;

	return qtrue;
}

/*
=============
R_DrawAliasFrameLerp

Interpolates between two frames and origins
=============
*/
void R_DrawAliasFrameLerp( meshbuffer_t *mb, model_t *mod, float backlerp, qboolean shadow )
{
	int				i, meshnum;
	int				features;
	float			backv[3], frontv[3];
	vec3_t			move, delta;
	maliasframe_t	*frame, *oldframe;
	maliasmesh_t	*mesh;
	maliasvertex_t	*v, *ov;
	entity_t		*e = mb->entity;
	maliasmodel_t	*model = mod->aliasmodel;

	if( !shadow && (e->flags & RF_VIEWERMODEL) && !r_mirrorview && !r_portalview )
		return;

	meshnum = -mb->infokey - 1;
	if( meshnum < 0 || meshnum >= model->nummeshes )
		return;

	mesh = model->meshes + meshnum;

#if SHADOW_VOLUMES
	if( shadow && !mesh->trneighbors )
		return;
#endif

	frame = model->frames + e->frame;
	oldframe = model->frames + e->oldframe;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract( e->oldorigin, e->origin, delta );
	Matrix_TransformVector( e->axis, delta, move );
	VectorAdd( move, oldframe->translate, move );

	for (i=0 ; i<3 ; i++)
	{
		move[i] = frame->translate[i] + (move[i] - frame->translate[i]) * backlerp;
		move[i] *= e->scale;
		backv[i] = backlerp * oldframe->scale[i] * e->scale;
		frontv[i] = (1.0f - backlerp) * frame->scale[i] * e->scale;
	}

	v = mesh->vertexes + e->frame*mesh->numverts;
	ov = mesh->vertexes + e->oldframe*mesh->numverts;

	if( !shadow ) {
		vec3_t normal, oldnormal;

		for( i = 0; i < mesh->numverts; i++, v++, ov++ ) {
			VectorSet( tempVertexArray[i], 
				move[0] + v->point[0]*frontv[0] + ov->point[0]*backv[0],
				move[1] + v->point[1]*frontv[1] + ov->point[1]*backv[1],
				move[2] + v->point[2]*frontv[2] + ov->point[2]*backv[2] );

			R_LatLongToNorm( v->latlong, normal );
			R_LatLongToNorm( ov->latlong, oldnormal );

			VectorSet( tempNormalsArray[i], 
				normal[0] + (oldnormal[0] - normal[0])*backlerp, 
				normal[1] + (oldnormal[1] - normal[1])*backlerp, 
				normal[2] + (oldnormal[2] - normal[2])*backlerp );
		}
	} else {
		for( i = 0; i < mesh->numverts; i++, v++, ov++ ) {
			VectorSet( tempVertexArray[i], 
				move[0] + v->point[0]*frontv[0] + ov->point[0]*backv[0],
				move[1] + v->point[1]*frontv[1] + ov->point[1]*backv[1],
				move[2] + v->point[2]*frontv[2] + ov->point[2]*backv[2] );
		}
	}

	if( shadow ) {
		alias_mesh.stArray = NULL;
#if SHADOW_VOLUMES
		alias_mesh.trneighbors = mesh->trneighbors;
#endif
	} else {
		alias_mesh.stArray = mesh->stcoords;
#if SHADOW_VOLUMES
		alias_mesh.trneighbors = NULL;
#endif
	}

	alias_mesh.indexes = mesh->indexes;
	alias_mesh.numIndexes = mesh->numtris * 3;
	alias_mesh.numVertexes = mesh->numverts;
	alias_mesh.xyzArray = tempVertexArray;
	alias_mesh.normalsArray = tempNormalsArray;
#if SHADOW_VOLUMES
	alias_mesh.trnormals = NULL;
#endif

	features = MF_NONBATCHED | mb->shader->features;
	if( r_shownormals->integer && !shadow )
		features |= MF_NORMALS;
	if( mb->shader->flags & SHADER_AUTOSPRITE )
		features |= MF_NOCULL;
	if( shadow )
		features |= MF_DEFORMVS;

	R_RotateForEntity( e );

	R_PushMesh( &alias_mesh, features );
	R_RenderMeshBuffer( mb, shadow );

	if( shadow ) {
		if( r_shadows->integer == 1 ) {
			R_Draw_SimpleShadow( e );
		} else {
#if SHADOW_VOLUMES
			R_AliasModelBBox( e, mod );
			R_DrawShadowVolumes( &alias_mesh, alias_mins, alias_maxs, alias_radius );
#endif
		}
	}
}

/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel( meshbuffer_t *mb, qboolean shadow )
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

	R_DrawAliasFrameLerp( mb, R_AliasLODForDistance( e ), e->backlerp, shadow );

	if( e->flags & RF_WEAPONMODEL )
		qglDepthRange( gldepthmin, gldepthmax );

	if( e->flags & RF_CULLHACK )
		qglFrontFace( GL_CCW );
}

/*
=================
R_AddAliasModelToList
=================
*/
void R_AddAliasModelToList( entity_t *e )
{
	int				i, j;
	mfog_t			*fog;
	model_t			*mod;
	shader_t		*shader;
	maliasmodel_t	*aliasmodel;
	maliasmesh_t	*mesh;

	mod = R_AliasLODForDistance( e );
	if ( !(aliasmodel = mod->aliasmodel) )
		return;

	R_AliasModelBBox( e, mod );
	if( !r_shadows->integer && R_CullAliasModel( e, mod ) )
		return;

	fog = R_FogForSphere( e->origin, alias_radius );
	mesh = aliasmodel->meshes;

	for( i = 0; i < aliasmodel->nummeshes; i++, mesh++ ) {
		if( e->customSkin ) {
			shader = R_FindShaderForSkinFile( e->customSkin, mesh->name );
			if( shader )
				R_AddMeshToList( MB_MODEL, fog, shader, -(i+1) );
		} else if( e->customShader ) {
			R_AddMeshToList( MB_MODEL, fog, e->customShader, -(i+1) );
		} else if( (e->skinnum >= 0) && (e->skinnum < aliasmodel->numskins) ) {
			R_AddMeshToList( MB_MODEL, fog, aliasmodel->skins[e->skinnum].shader, -(i+1) );
		} else if( mesh->numskins == 1 ) {
			R_AddMeshToList( MB_MODEL, fog, mesh->skins->shader, -(i+1) );
		} else if( mesh->numskins ) {
			for( j = 0; j < mesh->numskins; j++ )
				R_AddMeshToList( MB_MODEL, fog, mesh->skins[j].shader, -(i+1) );
		}
	}
}
