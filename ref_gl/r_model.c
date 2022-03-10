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
// r_model.c -- model loading and caching

#include "r_local.h"

model_t		*loadmodel;
int			modfilelen;

void Mod_LoadAliasMD2Model (model_t *mod, model_t *parent, void *buffer);
void Mod_LoadAliasMD3Model (model_t *mod, model_t *parent, void *buffer);
void Mod_LoadSpriteModel (model_t *mod, model_t *parent, void *buffer);
void Mod_LoadSkeletalModel (model_t *mod, model_t *parent, void *buffer);
void Mod_LoadBrushModel (model_t *mod, model_t *parent, void *buffer);

model_t *Mod_LoadModel (model_t *mod, qboolean crash);

qbyte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512*4
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

// the inline * models from the current map are kept separate
model_t	*mod_inline;

mempool_t *mod_mempool;

static modelformatdescriptor_t mod_supportedformats[] =
{
 // Quake2 .md2 models
	{ IDMD2HEADER,	4,	MD3_ALIAS_MAX_LODS,	Mod_LoadAliasMD2Model },

// Quake III Arena .md3 models
	{ IDMD3HEADER,	4,	MD3_ALIAS_MAX_LODS,	Mod_LoadAliasMD3Model },

// Quake2 .sp2 sprites
	{ IDSP2HEADER,	4,	0,					Mod_LoadSpriteModel	},

// Skeletal models
	{ SKMHEADER,	4,	SKM_MAX_LODS,		Mod_LoadSkeletalModel },

// Quake III Arena .bsp models
	{ IDBSPHEADER,	4,	0,					Mod_LoadBrushModel },

// trailing NULL
	{ NULL,			0,	0,					NULL }
};

static int mod_numsupportedformats = sizeof(mod_supportedformats) / sizeof(mod_supportedformats[0]) - 1;

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf( vec3_t p, model_t *model )
{
	mnode_t		*node;
	cplane_t	*plane;
	
	if( !model || !model->nodes )
		Com_Error( ERR_DROP, "Mod_PointInLeaf: bad model" );

	node = model->nodes;
	do {
		plane = node->plane;
		node = node->children[PlaneDiff (p, plane) < 0];
	} while( node->plane != NULL );

	return ( mleaf_t * )node;
}

/*
==============
Mod_ClusterPVS
==============
*/
qbyte *Mod_ClusterPVS( int cluster, model_t *model )
{
	if( cluster == -1 || !model->vis )
		return mod_novis;
	return ((qbyte *)model->vis->data + cluster*model->vis->rowsize);
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f( void )
{
	int		i;
	model_t	*mod;
	int		total;

	total = 0;
	Com_Printf( "Loaded models:\n" );
	for( i = 0, mod = mod_known; i < mod_numknown ; i++, mod++ ) {
		if( !mod->name[0] )
			continue;
		Com_Printf( "%8i : %s\n", mod->extradata->totalsize, mod->name );
		total += mod->extradata->totalsize;
	}
	Com_Printf( "Total: %i\n", mod_numknown );
	Com_Printf( "Total resident: %i\n", total );
}

/*
===============
R_InitModels
===============
*/
void R_InitModels( void )
{
	memset( mod_novis, 0xff, sizeof(mod_novis) );
	mod_mempool = Mem_AllocPool( NULL, "Models" );
}

/*
================
R_ShutdownModels
================
*/
void R_ShutdownModels( void )
{
	int		i;

	for( i = 0; i < mod_numknown; i++ ) {
		if( mod_known[i].extradata )
			Mem_FreePool( &mod_known[i].extradata );
	}

	r_worldmodel = NULL;
	mod_numknown = 0;
	memset( mod_known, 0, sizeof(mod_known) );
	Mem_FreePool( &mod_mempool );
}

/*
=================
Mod_StripLODSuffix
=================
*/
void Mod_StripLODSuffix( char *name )
{
	int len, lodnum;

	len = strlen( name );
	if( len <= 2 )
		return;

	lodnum = atoi( &name[len - 1] );
	if( lodnum < MD3_ALIAS_MAX_LODS ) {
		if( name[len-2] == '_' )
			name[len-2] = 0;
	}
}

/*
==================
Mod_FindSlot

==================
*/
model_t *Mod_FindSlot( char *name )
{
	int		i;
	model_t	*mod;

	//
	// search the currently loaded models
	//
	for( i = 0, mod = mod_known; i < mod_numknown; i++, mod++ ) {
		if( !strcmp( mod->name, name ) )
			return mod;
	}

	//
	// find a free model slot spot
	//
	if( mod_numknown == MAX_MOD_KNOWN )
		Com_Error( ERR_DROP, "mod_numknown == MAX_MOD_KNOWN" );
	return &mod_known[mod_numknown];
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName( char *name, qboolean crash )
{
	int		i;
	model_t	*mod, *lod;
	unsigned *buf;
	char shortname[MAX_QPATH], lodname[MAX_QPATH];
	modelformatdescriptor_t *descr;

	if( !name[0] )
		Com_Error (ERR_DROP, "Mod_ForName: NULL name");
		
	//
	// inline models are grabbed only from worldmodel
	//
	if( name[0] == '*' ) {
		i = atoi( name+1 );
		if( i < 1 || !r_worldmodel || i >= r_worldmodel->numsubmodels )
			Com_Error( ERR_DROP, "bad inline model number" );
		return &mod_inline[i];
	}

	mod = Mod_FindSlot( name );
	if( mod->name[0] && !strcmp( mod->name, name ) )
		return mod;

	//
	// load the file
	//
	modfilelen = FS_LoadFile( name, (void **)&buf );
	if( !buf ) {
		if( crash )
			Com_Error( ERR_DROP, "Mod_NumForName: %s not found", name );
		return NULL;
	}

	Q_strncpyz( mod->name, name, sizeof(mod->name) );

	loadmodel = mod;
	mod_numknown++;

	// call the apropriate loader
	descr = mod_supportedformats;
	for( i = 0; i < mod_numsupportedformats; i++, descr++ ) {
		if ( !strncmp( (const char *)buf, descr->header, descr->headerLen ) )
			break;
	}

	if( i == mod_numsupportedformats )
		Com_Error( ERR_DROP, "Mod_NumForName: unknown fileid for %s", mod->name );

	mod->extradata = Mem_AllocPool( mod_mempool, mod->name );
	descr->loader( mod, NULL, buf );
	FS_FreeFile( buf );

	if( !descr->maxLods )
		return mod;

	// 
	// load level-of-detail models
	//
	COM_StripExtension( mod->name, shortname );

	for( i = 0, mod->numlods = 0; i < descr->maxLods; i++ ) {
		if( i >= MOD_MAX_LODS )
			break;

		Q_snprintfz( lodname, MAX_QPATH, "%s_%i%s", shortname, i+1, &mod->name[strlen(shortname)] );
		modfilelen = FS_LoadFile( lodname, (void **)&buf );
		if( !buf || strncmp( (const char *)buf, descr->header, descr->headerLen ) )
			break;

		mod->numlods++;
		FS_FreeFile( buf );
	}

	if( mod->numlods < 2 ) {
		mod->numlods = 0;
		return mod;
	}

	for( i = 0; i < mod->numlods; i++ ) {
		Q_snprintfz( lodname, MAX_QPATH, "%s_%i%s", shortname, i+1, &mod->name[strlen(shortname)] );
		FS_LoadFile( lodname, (void **)&buf );

		lod = mod->lods[i] = Mod_FindSlot( lodname );
		if( lod->name[0] && !strcmp( lod->name, lodname ) )
			continue;

		strcpy( lod->name, lodname );
		mod_numknown++;

		loadmodel = lod;
		lod->extradata = Mem_AllocPool( mod_mempool, lodname );
		descr->loader( lod, mod, buf );
		FS_FreeFile( buf );
	}

	loadmodel = mod;
	return mod;
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

qbyte	*mod_base;

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting( lump_t *l )
{
	if( !l->filelen || r_vertexlight->integer )
		return;
	if( l->filelen % LIGHTMAP_SIZE )
		Com_Error( ERR_DROP, "Mod_LoadLighting: funny lump size in %s", loadmodel->name );

	loadmodel->numlightmaps = l->filelen / LIGHTMAP_SIZE;
	loadmodel->lightmapRects = Mod_Malloc( loadmodel, loadmodel->numlightmaps * sizeof(*loadmodel->lightmapRects) );
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility( lump_t *l )
{
	if( !l->filelen ) {
		loadmodel->vis = NULL;
		return;
	}

	loadmodel->vis = Mod_Malloc( loadmodel, l->filelen );
	memcpy( loadmodel->vis, mod_base + l->fileofs, l->filelen );

	loadmodel->vis->numclusters = LittleLong( loadmodel->vis->numclusters );
	loadmodel->vis->rowsize = LittleLong( loadmodel->vis->rowsize );
}

/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes( lump_t *l )
{
	int			i, count, j;
	dvertex_t	*in;
	float		*out_xyz, *out_normals,	*out_st, *out_lmst;
	qbyte		*out_colors;
	vec3_t		color, fcolor;
	float		div;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadVertexes: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);

	loadmodel->xyz_array = ( vec3_t * )Mod_Malloc( loadmodel, count*sizeof(vec3_t) );
	loadmodel->normals_array = ( vec3_t * )Mod_Malloc( loadmodel, count*sizeof(vec3_t) );	
	loadmodel->st_array = ( vec2_t * )Mod_Malloc( loadmodel, count*sizeof(vec2_t) );	
	loadmodel->lmst_array = ( vec2_t * )Mod_Malloc( loadmodel, count*sizeof(vec2_t) );	
	loadmodel->colors_array = ( byte_vec4_t * )Mod_Malloc( loadmodel, count*sizeof(byte_vec4_t) );	
	loadmodel->numvertexes = count;

	out_xyz = loadmodel->xyz_array[0];
	out_normals = loadmodel->normals_array[0];
	out_st = loadmodel->st_array[0];
	out_lmst = loadmodel->lmst_array[0];
	out_colors = loadmodel->colors_array[0];

	if( r_mapoverbrightbits->integer > 0 )
		div = (float)(1 << r_mapoverbrightbits->integer) / 255.0;
	else
		div = 1.0 / 255.0;

	for( i = 0; i < count; i++, in++, out_xyz += 3, out_normals += 3, out_st += 2, out_lmst += 2, out_colors += 4 ) {
		for( j = 0; j < 3 ; j++ ) {
			out_xyz[j] = LittleFloat( in->point[j] );
			out_normals[j] = LittleFloat( in->normal[j] );
		}

		for( j = 0; j < 2; j++ ) {
			out_st[j] = LittleFloat( in->tex_st[j] );
			out_lmst[j] = LittleFloat( in->lm_st[j] );
		}

		if( r_fullbright->integer ) {
			out_colors[0] = 255;
			out_colors[1] = 255;
			out_colors[2] = 255;
			out_colors[3] = in->color[3];
		} else {
			for( j = 0; j < 3 ; j++ )
				color[j] = ( ( float )in->color[j] * div );
			ColorNormalize( color, fcolor );

			out_colors[0] = ( qbyte )( fcolor[0] * 255 );
			out_colors[1] = ( qbyte )( fcolor[1] * 255 );
			out_colors[2] = ( qbyte )( fcolor[2] * 255 );
			out_colors[3] = in->color[3];
		}
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels( lump_t *l )
{
	int			i, j, count;
	dmodel_t	*in;
	mmodel_t	*out;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadSubmodels: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );	

	mod_inline = Mod_Malloc( loadmodel, count*sizeof(*mod_inline) );

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		for( j = 0; j < 3; j++ ) {
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
		}

		out->radius = RadiusFromBounds( out->mins, out->maxs );
		out->firstface = LittleLong( in->firstface );
		out->numfaces = LittleLong( in->numfaces );
	}
}

/*
=================
Mod_LoadShaderrefs
=================
*/
void Mod_LoadShaderrefs( lump_t *l )
{
	int 			i, count;
	dshaderref_t	*in;
	mshaderref_t	*out;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadShaderrefs: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );	

	loadmodel->shaderrefs = out;
	loadmodel->numshaderrefs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		Q_strncpyz( out->name, in->name, sizeof( out->name ) );
		out->flags = LittleLong( in->flags );
		out->contents = LittleLong( in->contents );
		out->shader = NULL;
	}
}

/*
=================
Mod_CreateMeshForSurface
=================
*/
mesh_t *Mod_CreateMeshForSurface( dface_t *in, msurface_t *out )
{
	mesh_t *mesh = NULL;

	switch( out->facetype ) {
		case FACETYPE_FLARE:
			{
				int r, g, b;

				mesh = out->mesh = ( mesh_t * )Mod_Malloc( loadmodel, sizeof( mesh_t ) );
				mesh->xyzArray = ( vec3_t * )Mod_Malloc( loadmodel, sizeof( vec3_t ) );
				mesh->numVertexes = 1;
				mesh->indexes = r_quad_indexes;
				mesh->numIndexes = 6;
				VectorCopy( out->origin, mesh->xyzArray[0] );

				r = LittleFloat( in->mins[0] ) * 255.0f;
				clamp( r, 0, 255 );

				g = LittleFloat( in->mins[1] ) * 255.0f;
				clamp( g, 0, 255 );

				b = LittleFloat( in->mins[2] ) * 255.0f;
				clamp( b, 0, 255 );

				out->dlightbits = ( unsigned int )COLOR_RGB( r, g, b );
			}
			break;
		case FACETYPE_PATCH:
			{
				int i, u, v, p;
				int patch_cp[2], step[2], size[2], flat[2];
				float subdivLevel, f;
				int numVerts, firstVert;
				vec4_t colors[MAX_ARRAY_VERTS], colors2[MAX_ARRAY_VERTS];
				index_t	*indexes;

				patch_cp[0] = LittleLong( in->patch_cp[0] );
				patch_cp[1] = LittleLong( in->patch_cp[1] );

				if( !patch_cp[0] || !patch_cp[1] )
					break;

				subdivLevel = r_subdivisions->value;
				if( subdivLevel < 1 )
					subdivLevel = 1;

				numVerts = LittleLong( in->numverts );
				firstVert = LittleLong( in->firstvert );
				for( i = 0; i < numVerts; i++ )
					Vector4Scale( loadmodel->colors_array[firstVert + i], (1.0 / 255.0), colors[i] );

				// find the degree of subdivision in the u and v directions
				Patch_GetFlatness( subdivLevel, &loadmodel->xyz_array[firstVert], patch_cp, flat );

				// allocate space for mesh
				step[0] = (1 << flat[0]);
				step[1] = (1 << flat[1]);
				size[0] = (patch_cp[0] >> 1) * step[0] + 1;
				size[1] = (patch_cp[1] >> 1) * step[1] + 1;
				numVerts = size[0] * size[1];

				if( numVerts > MAX_ARRAY_VERTS )
					break;

				out->patchWidth = size[0];
				out->patchHeight = size[1];

				mesh = ( mesh_t * )Mod_Malloc( loadmodel, sizeof( mesh_t ) );
				mesh->numVertexes = numVerts;
				mesh->xyzArray = ( vec3_t * )Mod_Malloc( loadmodel, numVerts * sizeof( vec3_t ) );
				mesh->normalsArray = ( vec3_t * )Mod_Malloc( loadmodel, numVerts * sizeof( vec3_t ) );
				mesh->stArray = ( vec2_t * )Mod_Malloc( loadmodel, numVerts * sizeof( vec2_t ) );
				mesh->lmstArray = ( vec2_t * )Mod_Malloc( loadmodel, numVerts * sizeof( vec2_t ) );
				mesh->colorsArray = ( byte_vec4_t * )Mod_Malloc( loadmodel, numVerts * sizeof( byte_vec4_t ) );

				Patch_Evaluate( loadmodel->xyz_array[firstVert], patch_cp, step, mesh->xyzArray[0], 3 );
				Patch_Evaluate( loadmodel->normals_array[firstVert], patch_cp, step, tempNormalsArray[0], 3 );
				Patch_Evaluate( colors[0], patch_cp, step, colors2[0], 4 );
				Patch_Evaluate( loadmodel->st_array[firstVert], patch_cp, step, mesh->stArray[0], 2 );
				Patch_Evaluate( loadmodel->lmst_array[firstVert], patch_cp, step, mesh->lmstArray[0], 2 );

				for( i = 0; i < numVerts; i++ ) {
					VectorNormalize2( tempNormalsArray[i], mesh->normalsArray[i] );

					f = max( max( colors2[i][0], colors2[i][1] ), colors2[i][2] );
					if( f > 1.0f ) {
						f = 255.0f / f;
						mesh->colorsArray[i][0] = colors2[i][0] * f;
						mesh->colorsArray[i][1] = colors2[i][1] * f;
						mesh->colorsArray[i][2] = colors2[i][2] * f;
					} else {
						mesh->colorsArray[i][0] = colors2[i][0] * 255;
						mesh->colorsArray[i][1] = colors2[i][1] * 255;
						mesh->colorsArray[i][2] = colors2[i][2] * 255;
					}
				}

				// compute new indexes
				mesh->numIndexes = (size[0] - 1) * (size[1] - 1) * 6;
				indexes = mesh->indexes = ( index_t * )Mod_Malloc( loadmodel, mesh->numIndexes * sizeof( index_t ) );
				for( v = 0, i = 0; v < size[1] - 1; v++ ) {
					for( u = 0; u < size[0] - 1; u++ ) {
						indexes[0] = p = v * size[0] + u;
						indexes[1] = p + size[0];
						indexes[2] = p + 1;
						indexes[3] = p + 1;
						indexes[4] = p + size[0];
						indexes[5] = p + size[0] + 1;
						indexes += 6;
					}
				}
			}
			break;
		default:
			mesh = ( mesh_t * )Mod_Malloc( loadmodel, sizeof( mesh_t ) );
			mesh->xyzArray = loadmodel->xyz_array + LittleLong( in->firstvert );
			mesh->normalsArray = loadmodel->normals_array + LittleLong( in->firstvert );
			mesh->stArray = loadmodel->st_array + LittleLong( in->firstvert );
			mesh->lmstArray = loadmodel->lmst_array + LittleLong( in->firstvert );
			mesh->colorsArray = loadmodel->colors_array + LittleLong( in->firstvert );
			mesh->numVertexes = LittleLong( in->numverts );
			mesh->indexes = loadmodel->surfindexes + LittleLong( in->firstindex );
			mesh->numIndexes = LittleLong( in->numindexes );
			break;
	}

	return mesh;
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces( lump_t *l )
{
	int			i, j, count;
	dface_t		*in;
	msurface_t 	*out;
	mesh_t		*mesh;
	mfog_t		*fog;
	mshaderref_t *shaderref;
	int			shadernum, fognum, surfnum;
	float		*vert;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for( surfnum = 0; surfnum < count; surfnum++, in++, out++ ) {
		for( i = 0; i < 3; i++ )
			out->origin[i] = LittleFloat( in->origin[i] );
		out->facetype = LittleLong( in->facetype );

	// lighting info
		if( r_vertexlight->integer ) {
			out->lightmapnum = -1;
		} else {
			out->lightmapnum = LittleLong( in->lm_texnum );
			if( out->lightmapnum >= loadmodel->numlightmaps ) {
				Com_DPrintf( S_COLOR_RED "WARNING: bad lightmap number: %i\n", out->lightmapnum );
				out->lightmapnum = -1;
			}
		}

		shadernum = LittleLong( in->shadernum );
		if( shadernum < 0 || shadernum >= loadmodel->numshaderrefs )
			Com_Error( ERR_DROP, "MOD_LoadBmodel: bad shader number" );

		shaderref = loadmodel->shaderrefs + shadernum;
		out->shaderref = shaderref;

		if( !shaderref->shader ) {
			if( out->facetype == FACETYPE_FLARE )
				shaderref->shader = R_LoadShader( shaderref->name, SHADER_BSP_FLARE, qfalse );
			else if( out->facetype == FACETYPE_TRISURF || r_vertexlight->integer || out->lightmapnum < 0 )
				shaderref->shader = R_LoadShader( shaderref->name, SHADER_BSP_VERTEX, qfalse );
			else
				shaderref->shader = R_LoadShader( shaderref->name, SHADER_BSP, qfalse );
		}

		fognum = LittleLong( in->fognum );
		if( fognum != -1 && (fognum < loadmodel->numfogs) ) {
			fog = loadmodel->fogs + fognum;
			if( fog->numplanes && fog->shader )
				out->fog = fog;
		}

		mesh = out->mesh = Mod_CreateMeshForSurface( in, out );
		if( !mesh )
			continue;

		ClearBounds( out->mins, out->maxs );
		for( j = 0, vert = mesh->xyzArray[0]; j < mesh->numVertexes; j++, vert += 3 )
			AddPointToBounds( vert, out->mins, out->maxs );

		if( out->facetype == FACETYPE_PLANAR ) {
			for( j = 0; j < 3; j++ )
				out->origin[j] = LittleFloat( in->normal[j] );
		}

		R_FixAutosprites( out );
	}
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes( lump_t *l )
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;
	qboolean	badBounds;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadNodes: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->plane = loadmodel->planes + LittleLong( in->planenum );

		for( j = 0; j < 2; j++ ) {
			p = LittleLong( in->children[j] );

			if( p >= 0 )
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = ( mnode_t * )(loadmodel->leafs + (-1 - p));
		}

		badBounds = qfalse;
		for( j = 0; j < 3; j++ ) {
			out->mins[j] = LittleFloat( in->mins[j] );
			out->maxs[j] = LittleFloat( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] )
				badBounds = qtrue;
		}

		if( badBounds || VectorCompare( out->mins, out->maxs ) ) {
			Com_DPrintf( S_COLOR_YELLOW "WARNING: bad node %i bounds:\n", i );
			Com_DPrintf( S_COLOR_YELLOW "mins: %i %i %i\n", Q_rint(out->mins[0]), Q_rint(out->mins[1]), Q_rint(out->mins[2]) );
			Com_DPrintf( S_COLOR_YELLOW "maxs: %i %i %i\n", Q_rint(out->maxs[0]), Q_rint(out->maxs[1]), Q_rint(out->maxs[2]) );
		}
	}
}

/*
=================
Mod_LoadFogs
=================
*/
void Mod_LoadFogs( lump_t *l, lump_t *brLump, lump_t *brSidesLump )
{
	int			i, j, count, p;
	dfog_t 		*in;
	mfog_t 		*out;
	dbrush_t 	*inbrushes, *brush;
	dbrushside_t *inbrushsides, *brushside;

	inbrushes = ( void * )(mod_base + brLump->fileofs);
	if( brLump->filelen % sizeof(*inbrushes) )
		Com_Error( ERR_DROP, "Mod_LoadBrushes: funny lump size in %s", loadmodel->name );

	inbrushsides = ( void * )(mod_base + brSidesLump->fileofs);
	if( brSidesLump->filelen % sizeof(*inbrushsides) )
		Com_Error( ERR_DROP, "Mod_LoadBrushsides: funny lump size in %s", loadmodel->name );

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadFogs: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );	

	loadmodel->fogs = out;
	loadmodel->numfogs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		p = LittleLong ( in->brushnum );
		if( p == -1 )
			continue;
		brush = inbrushes + p;

		p = LittleLong( brush->firstside );
		if( p == -1 )
			continue;
		brushside = inbrushsides + p;

		p = LittleLong ( in->visibleside );
		if( p == -1 )
			continue;

		out->shader = R_RegisterShader( in->shader );
		out->numplanes = LittleLong( brush->numsides );
		out->planes = Mod_Malloc( loadmodel, out->numplanes * sizeof( cplane_t ) );
		out->visibleplane = loadmodel->planes + LittleLong( brushside[p].planenum );

		for( j = 0; j < out->numplanes; j++ )
			out->planes[j] = *(loadmodel->planes + LittleLong( brushside[j].planenum ));
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs( lump_t *l )
{
	int			i, j, count;
	dleaf_t 	*in;
	mleaf_t 	*out;
	qboolean	badBounds;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadLeafs: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		badBounds = qfalse;
		for( j = 0; j < 3; j++ ) {
			out->mins[j] = (float)LittleLong( in->mins[j] );
			out->maxs[j] = (float)LittleLong( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] )
				badBounds = qtrue;
		}
		if( i && (badBounds || VectorCompare( out->mins, out->maxs )) ) {
			Com_DPrintf( S_COLOR_YELLOW "WARNING: bad leaf %i bounds:\n", i );
			Com_DPrintf( S_COLOR_YELLOW "mins: %i %i %i\n", Q_rint(out->mins[0]), Q_rint(out->mins[1]), Q_rint(out->mins[2]) );
			Com_DPrintf( S_COLOR_YELLOW "maxs: %i %i %i\n", Q_rint(out->maxs[0]), Q_rint(out->maxs[1]), Q_rint(out->maxs[2]) );
			Com_DPrintf( S_COLOR_YELLOW "cluster: %i\n", LittleLong( in->cluster ) );
			Com_DPrintf( S_COLOR_YELLOW "surfaces: %i\n", LittleLong( in->numleaffaces ) );
			Com_DPrintf( S_COLOR_YELLOW "brushes: %i\n", LittleLong( in->numleafbrushes ) );
		}

		out->cluster = LittleLong( in->cluster );

		if( loadmodel->vis ) {
			if( out->cluster >= loadmodel->vis->numclusters )
				Com_Error( ERR_DROP, "MOD_LoadBmodel: leaf cluster > numclusters" );
		}

		out->plane = NULL;
		out->area = LittleLong( in->area ) + 1;

		out->nummarksurfaces = LittleLong( in->numleaffaces );

		j = LittleLong ( in->firstleafface );
		if( j < 0 || out->nummarksurfaces + j > loadmodel->nummarksurfaces )
			out->nummarksurfaces = loadmodel->nummarksurfaces - j;
		out->firstmarksurface = loadmodel->marksurfaces + j;
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces( lump_t *l )
{	
	int		i, j, count;
	int		*in;
	msurface_t	**out;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for( i = 0; i < count; i++ ) {
		j = LittleLong( in[i] );

		if( j < 0 ||  j >= loadmodel->numsurfaces )
			Com_Error( ERR_DROP, "Mod_ParseMarksurfaces: bad surface number" );
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadIndexes
=================
*/
void Mod_LoadIndexes( lump_t *l )
{	
	int		i, count;
	int		*in, *out;
	
	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadIndexes: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );

	loadmodel->surfindexes = out;
	loadmodel->numsurfindexes = count;

	for( i = 0; i < count; i++ )
		out[i] = LittleLong( in[i] );
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes( lump_t *l )
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	
	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadPlanes: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->type = PLANE_NONAXIAL;
		out->signbits = 0;

		for( j = 0; j < 3; j++ ) {
			out->normal[j] = LittleFloat( in->normal[j] );
			if( out->normal[j] < 0 )
				out->signbits |= 1<<j;
			if( out->normal[j] == 1.0f )
				out->type = j;
		}
		out->dist = LittleFloat( in->dist );
	}
}

/*
=================
Mod_LoadLightgrid
=================
*/
void Mod_LoadLightgrid( lump_t *l )
{
	int	count;
	dgridlight_t 	*in;
	mgridlight_t 	*out;
			
	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadLightgrid: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );

	loadmodel->lightgrid = out;
	loadmodel->numlightgridelems = count;

	// lightgrid is all 8 bit
	memcpy( out, in, count*sizeof(*out) );
}

/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities( lump_t *l )
{
	char *data;
	mlight_t *out;
	int count, total, gridsizei[3];
	qboolean islight, isworld;
	float scale, gridsizef[3];
	char key[MAX_KEY], value[MAX_VALUE], target[MAX_VALUE], *token;

	data = (char *)mod_base + l->fileofs;
	if( !data || !data[0] )
		return;

	for( total = 0; (token = COM_Parse (&data)) && token[0] == '{'; ) {
		islight = qfalse;
		isworld = qfalse;

		while( 1 ) {
			if( !(token = COM_Parse (&data)) )
				break; // error
			if ( token[0] == '}' )
				break; // end of entity

			Q_strncpyz( key, token, sizeof(key) );
			while( key[strlen(key)-1] == ' ' )	// remove trailing spaces
				key[strlen(key)-1] = 0;

			if( !(token = COM_Parse (&data)) )
				break; // error

			Q_strncpyz( value, token, sizeof(value) );

			// now that we have the key pair worked out...
			if( !strcmp (key, "classname") ) {
				if( !strncmp (value, "light", 5) )
					islight = qtrue;
				else if ( !strcmp (value, "worldspawn") )
					isworld = qtrue;
			} else if ( !strcmp (key, "gridsize") ) {
				sscanf( value, "%f %f %f", &gridsizef[0], &gridsizef[1], &gridsizef[2] );

				if( !gridsizef[0] || !gridsizef[1] || !gridsizef[2] ) {
					sscanf( value, "%i %i %i", &gridsizei[0], &gridsizei[1], &gridsizei[2] );
					VectorCopy( gridsizei, gridsizef );
				}
			}
		}

		if( isworld ) {
			VectorCopy( gridsizef, loadmodel->gridSize );
			continue;
		}

		if( islight )
			total++;
	}

#if !(SHADOW_VOLUMES)
	total = 0;
#endif

	if( !total )
		return;

	out = Mod_Malloc( loadmodel, total*sizeof(*out) );
	loadmodel->worldlights = out;
	loadmodel->numworldlights = total;

	data = mod_base + l->fileofs;
	for( count = 0; (token = COM_Parse (&data)) && token[0] == '{';  ) {
		if( count == total )
			break;

		islight = qfalse;

		while( 1 ) {
			if( !(token = COM_Parse (&data)) )
				break; // error
			if( token[0] == '}' )
				break; // end of entity

			Q_strncpyz( key, token, sizeof(key) );
			while( key[strlen(key)-1] == ' ' )		// remove trailing spaces
				key[strlen(key)-1] = 0;

			if( !(token = COM_Parse (&data)) )
				break; // error

			Q_strncpyz( value, token, sizeof(value) );

			// now that we have the key pair worked out...
			if( !strcmp (key, "origin") )
				sscanf ( value, "%f %f %f", &out->origin[0], &out->origin[1], &out->origin[2] );
			else if( !strcmp (key, "color") || !strcmp (key, "_color") )
				sscanf ( value, "%f %f %f", &out->color[0], &out->color[1], &out->color[2] );
			else if( !strcmp (key, "light") || !strcmp (key, "_light") )
				out->intensity = atof ( value );
			else if (!strcmp (key, "classname") ) {
				if( !strncmp (value, "light", 5) )
					islight = qtrue;
			} else if( !strcmp (key, "target") )
				Q_strncpyz( target, value, sizeof(target) );
		}

		if( !islight )
			continue;

		if( out->intensity <= 0 )
			out->intensity = 300;
		out->intensity += 15;

		scale = max( max (out->color[0], out->color[1]), out->color[2] );
		if( !scale ) {
			VectorSet( out->color, 1, 1, 1 );
		} else {
			// normalize
			scale = 1.0f / scale;
			VectorScale( out->color, scale, out->color );
		}

		out++;
		count++;
	}
}

/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent( mnode_t *node, mnode_t *parent )
{
	node->parent = parent;
	if( !node->plane )
		return;
	Mod_SetParent( node->children[0], node );
	Mod_SetParent( node->children[1], node );
}

/*
=================
Mod_Finish
=================
*/
void Mod_Finish( lump_t *lightmaps )
{
	int				i, j;
	float			*lmArray;
	mesh_t			*mesh;
	msurface_t 		*surf;
	mlightmapRect_t *lmRect;

	if( loadmodel->numlightmaps ) {
		R_BuildLightmaps( loadmodel->numlightmaps, mod_base + lightmaps->fileofs, loadmodel->lightmapRects );

		// now walk list of surface and apply lightmap info
		for( i = 0, surf = loadmodel->surfaces; i < loadmodel->numsurfaces; i++, surf++ ) {
			if( surf->lightmapnum < 0 || surf->facetype == FACETYPE_FLARE || !(mesh = surf->mesh) )
				continue;

			lmRect = &loadmodel->lightmapRects[surf->lightmapnum];
			surf->lightmapnum = lmRect->texNum;

			if( r_ligtmapsPacking ) { // scale/shift lightmap coords
				lmArray = mesh->lmstArray[0];
				for( j = 0; j < mesh->numVertexes; j++, lmArray += 2 ) {
					lmArray[0] = (double)(lmArray[0]) * lmRect->texMatrix[0][0] + lmRect->texMatrix[0][1];
					lmArray[1] = (double)(lmArray[1]) * lmRect->texMatrix[1][0] + lmRect->texMatrix[1][1];
				}
			}
		}
		Mod_Free( loadmodel->lightmapRects );
	}

	Mod_SetParent( loadmodel->nodes, NULL );
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel( model_t *mod, model_t *parent, void *buffer )
{
	int			i;
	dheader_t	*header;
	mmodel_t 	*bm;
	vec3_t		maxs;

	mod->type = mod_brush;
	if( mod != mod_known )
		Com_Error( ERR_DROP, "Loaded a brush model after the world" );

	header = (dheader_t *)buffer;

	i = LittleLong( header->version );
	if( (i != Q3BSPVERSION) && (i != RTCWBSPVERSION) )
		Com_Error( ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i or %i)", mod->name, i, Q3BSPVERSION, RTCWBSPVERSION );

	mod_base = (qbyte *)header;

	// swap all the lumps
	for( i = 0; i < sizeof(dheader_t)/4; i++ ) 
		((int *)header)[i] = LittleLong( ((int *)header)[i]);

	// load into heap
	Mod_LoadEntities( &header->lumps[LUMP_ENTITIES] );
	Mod_LoadVertexes( &header->lumps[LUMP_VERTEXES] );
	Mod_LoadIndexes( &header->lumps[LUMP_INDEXES] );
	Mod_LoadLighting( &header->lumps[LUMP_LIGHTING] );
	Mod_LoadLightgrid( &header->lumps[LUMP_LIGHTGRID] );
	Mod_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	Mod_LoadShaderrefs( &header->lumps[LUMP_SHADERREFS] );
	Mod_LoadPlanes( &header->lumps[LUMP_PLANES] );
	Mod_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES] );
	Mod_LoadFaces( &header->lumps[LUMP_FACES] );
	Mod_LoadMarksurfaces( &header->lumps[LUMP_LEAFFACES] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );

	Mod_Finish( &header->lumps[LUMP_LIGHTING] );

	// set up the submodels
	for( i = 0; i < mod->numsubmodels; i++ ) {
		model_t		*starmod;

		bm = &mod->submodels[i];
		starmod = &mod_inline[i];

		*starmod = *mod;

		starmod->firstmodelsurface = starmod->surfaces + bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;

		VectorCopy( bm->maxs, starmod->maxs );
		VectorCopy( bm->mins, starmod->mins );
		starmod->radius = bm->radius;

		if( i == 0 )
			*mod = *starmod;
	}

	//
	// set up lightgrid
	//
	if( mod->gridSize[0] < 1 || mod->gridSize[1] < 1 || mod->gridSize[2] < 1 )
		VectorSet ( mod->gridSize, 64, 64, 128 );

	for( i = 0; i < 3; i++ ) {
		mod->gridMins[i] = mod->gridSize[i] * ceil( (mod->mins[i] + 1) / mod->gridSize[i] );
		maxs[i] = mod->gridSize[i] * floor( (mod->maxs[i] - 1) / mod->gridSize[i] );
		mod->gridBounds[i] = (maxs[i] - mod->gridMins[i])/mod->gridSize[i] + 1;
	}
	mod->gridBounds[3] = mod->gridBounds[1] * mod->gridBounds[0];
}

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel( model_t *mod, model_t *parent, void *buffer )
{
	int			i;
	dsprite_t	*sprin;
	smodel_t	*sprout;
	dsprframe_t *sprinframe;
	sframe_t	*sproutframe;

	sprin = (dsprite_t *)buffer;

	if( LittleLong (sprin->version) != SPRITE_VERSION )
		Com_Error( ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, LittleLong (sprin->version), SPRITE_VERSION );

	mod->smodel = sprout = Mod_Malloc( mod, sizeof(smodel_t) );
	sprout->numframes = LittleLong( sprin->numframes );

	sprinframe = sprin->frames;
	sprout->frames = sproutframe = Mod_Malloc( mod, sizeof(sframe_t) * sprout->numframes );

	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	// byte swap everything
	for( i = 0; i < sprout->numframes; i++, sprinframe++, sproutframe++ ) {
		sproutframe->width = LittleLong( sprinframe->width );
		sproutframe->height = LittleLong( sprinframe->height );
		sproutframe->origin_x = LittleLong( sprinframe->origin_x );
		sproutframe->origin_y = LittleLong( sprinframe->origin_y );
		sproutframe->shader = R_RegisterPic( sprinframe->name );
		sproutframe->radius = sqrt( sproutframe->width * sproutframe->width + sproutframe->height * sproutframe->height );
		mod->radius = max( mod->radius, sproutframe->radius );
	}

	mod->type = mod_sprite;
}


//=============================================================================

/*
=================
R_RegisterWorldModel

Specifies the model that will be used as the world
=================
*/
void R_RegisterWorldModel( char *model )
{
	if( r_packlightmaps->integer )
		r_ligtmapsPacking = qtrue;
	else
		r_ligtmapsPacking = qfalse;
	r_worldmodel = Mod_ForName( model, qtrue );

	r_worldent.scale = 1.0f;
	r_worldent.model = r_worldmodel;
	r_worldent.rtype = RT_MODEL;
	Matrix_Identity( r_worldent.axis );

	r_framecount = 1;
	r_oldviewcluster = r_viewcluster = -1;		// force markleafs
}

/*
=================
R_RegisterModel
=================
*/
struct model_s *R_RegisterModel( char *name ) {
	return Mod_ForName( name, qfalse );
}

/*
=================
R_ModelBounds
=================
*/
void R_ModelBounds( model_t *model, vec3_t mins, vec3_t maxs )
{
	if( model ) {
		VectorCopy( model->mins, mins );
		VectorCopy( model->maxs, maxs );
	}
}
