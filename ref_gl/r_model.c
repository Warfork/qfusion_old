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

static	bspFormatDesc_t *mod_bspFormat;

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

// SOF2 and JK2 .bsp models
	{ RBSPHEADER,	4,	0,					Mod_LoadBrushModel },

// qfusion .bsp models
	{ QFBSPHEADER,	4,	0,					Mod_LoadBrushModel },

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
static void Mod_LoadLighting( const lump_t *l )
{
	int size;

	if( !l->filelen )
		return;
	size = mod_bspFormat->lightmapWidth * mod_bspFormat->lightmapHeight * LIGHTMAP_BYTES;
	if( l->filelen % size )
		Com_Error( ERR_DROP, "Mod_LoadLighting: funny lump size in %s", loadmodel->name );

	loadmodel->numlightmaps = l->filelen / size;
	loadmodel->lightmapRects = Mod_Malloc( loadmodel, loadmodel->numlightmaps * sizeof(*loadmodel->lightmapRects) );
	R_BuildLightmaps( loadmodel->numlightmaps, mod_bspFormat->lightmapWidth, mod_bspFormat->lightmapHeight, mod_base + l->fileofs, loadmodel->lightmapRects );
}


/*
=================
Mod_LoadVisibility
=================
*/
static void Mod_LoadVisibility( const lump_t *l )
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
static void Mod_LoadVertexes( const lump_t *l )
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

	loadmodel->numvertexes = count;
	loadmodel->xyz_array = ( vec3_t * )Mod_Malloc( loadmodel, count*sizeof(vec3_t) );
	loadmodel->normals_array = ( vec3_t * )Mod_Malloc( loadmodel, count*sizeof(vec3_t) );
	loadmodel->st_array = ( vec2_t * )Mod_Malloc( loadmodel, count*sizeof(vec2_t) );
	loadmodel->lmst_array[0] = ( vec2_t * )Mod_Malloc( loadmodel, count*sizeof(vec2_t) );
	loadmodel->colors_array[0] = ( byte_vec4_t * )Mod_Malloc( loadmodel, count*sizeof(byte_vec4_t) );
	for( i = 1; i < MAX_LIGHTMAPS; i++ ) {
		loadmodel->lmst_array[i] = loadmodel->lmst_array[0];
		loadmodel->colors_array[i] = loadmodel->colors_array[0];
	}

	out_xyz = loadmodel->xyz_array[0];
	out_normals = loadmodel->normals_array[0];
	out_st = loadmodel->st_array[0];
	out_lmst = loadmodel->lmst_array[0][0];
	out_colors = loadmodel->colors_array[0][0];

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
Mod_LoadVertexes_RBSP
=================
*/
static void Mod_LoadVertexes_RBSP( const lump_t *l )
{
	int			i, count, j;
	rdvertex_t	*in;
	float		*out_xyz, *out_normals,	*out_st, *out_lmst[MAX_LIGHTMAPS];
	qbyte		*out_colors[MAX_LIGHTMAPS];
	vec3_t		color, fcolor;
	float		div;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadVertexes: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);

	loadmodel->numvertexes = count;
	loadmodel->xyz_array = ( vec3_t * )Mod_Malloc( loadmodel, count*sizeof(vec3_t) );
	loadmodel->normals_array = ( vec3_t * )Mod_Malloc( loadmodel, count*sizeof(vec3_t) );
	loadmodel->st_array = ( vec2_t * )Mod_Malloc( loadmodel, count*sizeof(vec2_t) );
	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {
		loadmodel->lmst_array[i] = ( vec2_t * )Mod_Malloc( loadmodel, count*sizeof(vec2_t) );
		loadmodel->colors_array[i] = ( byte_vec4_t * )Mod_Malloc( loadmodel, count*sizeof(byte_vec4_t) );
	}

	out_xyz = loadmodel->xyz_array[0];
	out_normals = loadmodel->normals_array[0];
	out_st = loadmodel->st_array[0];
	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {
		out_lmst[i] = loadmodel->lmst_array[i][0];
		out_colors[i] = loadmodel->colors_array[i][0];
	}

	if( r_mapoverbrightbits->integer > 0 )
		div = (float)(1 << r_mapoverbrightbits->integer) / 255.0;
	else
		div = 1.0 / 255.0;

	for( i = 0; i < count; i++, in++, out_xyz += 3, out_normals += 3, out_st += 2 ) {
		for( j = 0; j < 3 ; j++ ) {
			out_xyz[j] = LittleFloat( in->point[j] );
			out_normals[j] = LittleFloat( in->normal[j] );
		}

		for( j = 0; j < 2; j++ )
			out_st[j] = LittleFloat( in->tex_st[j] );

		for( j = 0; j < MAX_LIGHTMAPS; out_lmst[j] += 2, out_colors[j] += 4, j++ ) {
			out_lmst[j][0] = LittleFloat( in->lm_st[j][0] );
			out_lmst[j][1] = LittleFloat( in->lm_st[j][1] );
			
			if( r_fullbright->integer ) {
				out_colors[j][0] = 255;
				out_colors[j][1] = 255;
				out_colors[j][2] = 255;
				out_colors[j][3] = in->color[j][3];
			} else {
				color[0] = ( ( float )in->color[j][0] * div );
				color[1] = ( ( float )in->color[j][1] * div );
				color[2] = ( ( float )in->color[j][2] * div );
				ColorNormalize( color, fcolor );

				out_colors[j][0] = ( qbyte )( fcolor[0] * 255 );
				out_colors[j][1] = ( qbyte )( fcolor[1] * 255 );
				out_colors[j][2] = ( qbyte )( fcolor[2] * 255 );
				out_colors[j][3] = in->color[j][3];
			}
		}
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels( const lump_t *l )
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
static void Mod_LoadShaderrefs( const lump_t *l )
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
static mesh_t *Mod_CreateMeshForSurface( const rdface_t *in, msurface_t *out )
{
	mesh_t *mesh = NULL;

	switch( out->facetype ) {
		case FACETYPE_FLARE:
		{
			int r, g, b;

			mesh = out->mesh = ( mesh_t * )Mod_Malloc( loadmodel, sizeof( mesh_t ) + sizeof( vec3_t ) );
			mesh->xyzArray = ( vec3_t * )( ( qbyte * )mesh + sizeof( mesh_t ) );
			mesh->numVertexes = 1;
			mesh->indexes = ( index_t * )1;
			mesh->numIndexes = 1;
			VectorCopy( out->origin, mesh->xyzArray[0] );

			r = LittleFloat( in->mins[0] ) * 255.0f;
			clamp( r, 0, 255 );

			g = LittleFloat( in->mins[1] ) * 255.0f;
			clamp( g, 0, 255 );

			b = LittleFloat( in->mins[2] ) * 255.0f;
			clamp( b, 0, 255 );

			out->dlightbits = ( unsigned int )COLOR_RGB( r, g, b );
			break;
		}
		case FACETYPE_PATCH:
		{
			int bufsize;
			qbyte *buffer;
			int i, j, u, v, p;
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

			bufsize = sizeof( mesh_t ) + numVerts * (sizeof( vec3_t ) * 2 + sizeof( vec2_t ) );
			for( j = 0; j < MAX_LIGHTMAPS && in->lightmapStyles[j] != 255; j++ )
				bufsize += numVerts * sizeof( vec2_t );
			for( j = 0; j < MAX_LIGHTMAPS && in->vertexStyles[j] != 255; j++ )
				bufsize += numVerts * sizeof( byte_vec4_t );
			bufsize += (size[0] - 1) * (size[1] - 1) * 6 * sizeof( index_t );
			buffer = ( qbyte * )Mod_Malloc( loadmodel, bufsize );

			mesh = ( mesh_t * )buffer; buffer += sizeof( mesh_t );
			mesh->numVertexes = numVerts;
			mesh->xyzArray = ( vec3_t * )buffer; buffer += numVerts * sizeof( vec3_t );
			mesh->normalsArray = ( vec3_t * )buffer; buffer += numVerts * sizeof( vec3_t );
			mesh->stArray = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );
			mesh->numIndexes = (size[0] - 1) * (size[1] - 1) * 6;

			Patch_Evaluate( loadmodel->xyz_array[firstVert], patch_cp, step, mesh->xyzArray[0], 3 );
			Patch_Evaluate( loadmodel->normals_array[firstVert], patch_cp, step, mesh->normalsArray[0], 3 );
			Patch_Evaluate( loadmodel->st_array[firstVert], patch_cp, step, mesh->stArray[0], 2 );
			for( i = 0; i < numVerts; i++ )
				VectorNormalize( mesh->normalsArray[i] );

			for( j = 0; j < MAX_LIGHTMAPS && in->lightmapStyles[j] != 255; j++ ) {
				mesh->lmstArray[j] = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );
				Patch_Evaluate( loadmodel->lmst_array[j][firstVert], patch_cp, step, mesh->lmstArray[j][0], 2 );
			}

			for( j = 0; j < MAX_LIGHTMAPS && in->vertexStyles[j] != 255; j++ ) {
				mesh->colorsArray[j] = ( byte_vec4_t * )buffer; buffer += numVerts * sizeof( byte_vec4_t );
				for( i = 0; i < numVerts; i++ )
					Vector4Scale( loadmodel->colors_array[j][firstVert + i], (1.0 / 255.0), colors[i] );
				Patch_Evaluate( colors[0], patch_cp, step, colors2[0], 4 );

				for( i = 0; i < numVerts; i++ ) {
					f = max( max( colors2[i][0], colors2[i][1] ), colors2[i][2] );

					if( f > 1.0f ) {
						f = 255.0f / f;
						mesh->colorsArray[j][i][0] = colors2[i][0] * f;
						mesh->colorsArray[j][i][1] = colors2[i][1] * f;
						mesh->colorsArray[j][i][2] = colors2[i][2] * f;
					} else {
						mesh->colorsArray[j][i][0] = colors2[i][0] * 255;
						mesh->colorsArray[j][i][1] = colors2[i][1] * 255;
						mesh->colorsArray[j][i][2] = colors2[i][2] * 255;
					}
				}
			}

			// compute new indexes
			indexes = mesh->indexes = ( index_t * )buffer;
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
			break;
		}
		case FACETYPE_PLANAR:
		case FACETYPE_TRISURF:
		{
			int j, firstVert;

			firstVert = LittleLong( in->firstvert );
			mesh = ( mesh_t * )Mod_Malloc( loadmodel, sizeof( mesh_t ) );
			mesh->xyzArray = loadmodel->xyz_array + firstVert;
			mesh->normalsArray = loadmodel->normals_array + firstVert;
			mesh->stArray = loadmodel->st_array + firstVert;
			for( j = 0; j < MAX_LIGHTMAPS; j++ ) {
				mesh->lmstArray[j] = loadmodel->lmst_array[j] + firstVert;
				mesh->colorsArray[j] = loadmodel->colors_array[j] + firstVert;
			}
			mesh->numVertexes = LittleLong( in->numverts );
			mesh->indexes = loadmodel->surfindexes + LittleLong( in->firstindex );
			mesh->numIndexes = LittleLong( in->numindexes );
			break;
		}
	}

	return mesh;
}

/*
=================
Mod_LoadFaceCommon
=================
*/
static inline void Mod_LoadFaceCommon( const rdface_t *in, msurface_t *out )
{
	int			j;
	mesh_t		*mesh;
	mfog_t		*fog;
	mshaderref_t *shaderref;
	int			shadernum, fognum;
	float		*vert;
	int			lightmaps[MAX_LIGHTMAPS];
	qbyte		lightmapStyles[MAX_LIGHTMAPS], vertexStyles[MAX_LIGHTMAPS];

	out->facetype = LittleLong( in->facetype );
	if( out->facetype == FACETYPE_PLANAR ) {
		for( j = 0; j < 3; j++ )
			out->origin[j] = LittleFloat( in->normal[j] );
	} else {
		for( j = 0; j < 3; j++ )
			out->origin[j] = LittleFloat( in->origin[j] );
	}

	// lighting info
	for( j = 0; j < MAX_LIGHTMAPS; j++ ) {
		lightmaps[j] = LittleLong( in->lm_texnum[j] );
		if( lightmaps[j] < 0 || out->facetype == FACETYPE_FLARE ) {
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
		} else if( lightmaps[j] >= loadmodel->numlightmaps ) {
			Com_DPrintf( S_COLOR_RED "WARNING: bad lightmap number: %i\n", lightmaps[j] );
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
		} else {
			lightmaps[j] = loadmodel->lightmapRects[lightmaps[j]].texNum;
			lightmapStyles[j] = in->lightmapStyles[j];
		}
		vertexStyles[j] = in->vertexStyles[j];
	}

	// add this super style
	R_AddSuperLightStyle( lightmaps, lightmapStyles, vertexStyles );

	shadernum = LittleLong( in->shadernum );
	if( shadernum < 0 || shadernum >= loadmodel->numshaderrefs )
		Com_Error( ERR_DROP, "MOD_LoadBmodel: bad shader number" );
	shaderref = loadmodel->shaderrefs + shadernum;
	out->shaderref = shaderref;

	if( !shaderref->shader ) {
		if( out->facetype == FACETYPE_FLARE ) {
			shaderref->shader = R_LoadShader( shaderref->name, SHADER_BSP_FLARE, qfalse, 0 );
			shaderref->shader->flags |= SHADER_FLARE;	// force SHADER_FLARE flag
		} else if( out->facetype == FACETYPE_TRISURF || lightmaps[0] < 0 || lightmapStyles[0] == 255 ) {
			shaderref->shader = R_LoadShader( shaderref->name, SHADER_BSP_VERTEX, qfalse, 0 );
		} else {
			shaderref->shader = R_LoadShader( shaderref->name, SHADER_BSP, qfalse, 0 );
		}
	}

	fognum = LittleLong( in->fognum );
	if( fognum != -1 && (fognum < loadmodel->numfogs) ) {
		fog = loadmodel->fogs + fognum;
		if( fog->shader && fog->shader->fog_dist )
			out->fog = fog;
	}

	mesh = out->mesh = Mod_CreateMeshForSurface( in, out );
	if( !mesh )
		return;

	ClearBounds( out->mins, out->maxs );
	for( j = 0, vert = mesh->xyzArray[0]; j < mesh->numVertexes; j++, vert += 3 )
		AddPointToBounds( vert, out->mins, out->maxs );

	R_FixAutosprites( out );
}

/*
=================
Mod_LoadFaces
=================
*/
static void Mod_LoadFaces( const lump_t *l )
{
	int			i, j, count;
	dface_t		*in;
	rdface_t	rdf;
	msurface_t 	*out;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	rdf.lightmapStyles[0] = rdf.vertexStyles[0] = 0;
	for( j = 1; j < MAX_LIGHTMAPS; j++ )
		rdf.lightmapStyles[j] = rdf.vertexStyles[j] = 255;

	for( i = 0; i < count; i++, in++, out++ ) {
		rdf.facetype = in->facetype;
		rdf.lm_texnum[0] = in->lm_texnum;
		rdf.lightmapStyles[0] = rdf.vertexStyles[0] = 0;
		for( j = 1; j < MAX_LIGHTMAPS; j++ ) {
			rdf.lm_texnum[j] = -1;
			rdf.lightmapStyles[j] = rdf.vertexStyles[j] = 255;
		}
		for( j = 0; j < 3; j++ ) {
			rdf.origin[j] = in->origin[j];
			rdf.normal[j] = in->normal[j];
			rdf.mins[j] = in->mins[j];
			rdf.maxs[j] = in->maxs[j];
		}
		rdf.shadernum = in->shadernum;
		rdf.fognum = in->fognum;
		rdf.numverts = in->numverts;
		rdf.firstvert = in->firstvert;
		rdf.patch_cp[0] = in->patch_cp[0];
		rdf.patch_cp[1] = in->patch_cp[1];
		rdf.firstindex = in->firstindex;
		rdf.numindexes = in->numindexes;
		Mod_LoadFaceCommon( &rdf, out );
	}
}

/*
=================
Mod_LoadFaces_RBSP
=================
*/
static void Mod_LoadFaces_RBSP( const lump_t *l )
{
	int			i, count;
	rdface_t	*in;
	msurface_t 	*out;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for( i = 0; i < count; i++, in++, out++ )
		Mod_LoadFaceCommon( in, out );
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes( const lump_t *l )
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
static void Mod_LoadFogs( const lump_t *l, const lump_t *brLump, const lump_t *brSidesLump )
{
	int			i, j, count, p;
	dfog_t 		*in;
	mfog_t 		*out;
	dbrush_t 	*inbrushes, *brush;
	dbrushside_t *inbrushsides, *brushside;
	rdbrushside_t *inrbrushsides, *rbrushside;

	inbrushes = ( void * )(mod_base + brLump->fileofs);
	if( brLump->filelen % sizeof(*inbrushes) )
		Com_Error( ERR_DROP, "Mod_LoadBrushes: funny lump size in %s", loadmodel->name );

	if( mod_bspFormat->flags & BSP_RAVEN ) {
		inrbrushsides = ( void * )(mod_base + brSidesLump->fileofs);
		if( brSidesLump->filelen % sizeof(*inrbrushsides) )
			Com_Error( ERR_DROP, "Mod_LoadBrushsides: funny lump size in %s", loadmodel->name );
	} else {
		inbrushsides = ( void * )(mod_base + brSidesLump->fileofs);
		if( brSidesLump->filelen % sizeof(*inbrushsides) )
			Com_Error( ERR_DROP, "Mod_LoadBrushsides: funny lump size in %s", loadmodel->name );
	}

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadFogs: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );	

	loadmodel->fogs = out;
	loadmodel->numfogs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->shader = R_RegisterShader( in->shader );

		p = LittleLong( in->brushnum );
		if( p == -1 )	// global fog
			continue;
		brush = inbrushes + p;

		p = LittleLong( brush->firstside );
		if( p == -1 ) {
			out->shader = NULL;
			continue;
		}

		if( mod_bspFormat->flags & BSP_RAVEN )
			rbrushside = inrbrushsides + p;
		else
			brushside = inbrushsides + p;

		p = LittleLong( in->visibleside );
		if( p == -1 ) {
			out->shader = NULL;
			continue;
		}

		out->numplanes = LittleLong( brush->numsides );
		out->planes = Mod_Malloc( loadmodel, out->numplanes * sizeof( cplane_t ) );

		if( mod_bspFormat->flags & BSP_RAVEN ) {
			out->visibleplane = loadmodel->planes + LittleLong( rbrushside[p].planenum );
			for( j = 0; j < out->numplanes; j++ )
				out->planes[j] = *(loadmodel->planes + LittleLong( rbrushside[j].planenum ));
		} else {
			out->visibleplane = loadmodel->planes + LittleLong( brushside[p].planenum );
			for( j = 0; j < out->numplanes; j++ )
				out->planes[j] = *(loadmodel->planes + LittleLong( brushside[j].planenum ));
		}
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs( const lump_t *l, const lump_t *msLump )
{
	int			i, j, k, count, countMarkSurfaces;
	dleaf_t 	*in;
	mleaf_t 	*out;
	size_t		size;
	qbyte		*buffer;
	qboolean	badBounds;
	int			*inMarkSurfaces;
	int			numMarkSurfaces, firstMarkSurface;
	int			numVisSurfaces, numLitSurfaces, numFragmentSurfaces;

	inMarkSurfaces = ( void * )(mod_base + msLump->fileofs);
	if( msLump->filelen % sizeof(*inMarkSurfaces) )
		Com_Error( ERR_DROP, "Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name );
	countMarkSurfaces = msLump->filelen / sizeof(*inMarkSurfaces);

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
		out->cluster = LittleLong( in->cluster );

		if( i && (badBounds || VectorCompare( out->mins, out->maxs )) ) {
			Com_DPrintf( S_COLOR_YELLOW "WARNING: bad leaf %i bounds:\n", i );
			Com_DPrintf( S_COLOR_YELLOW "mins: %i %i %i\n", Q_rint(out->mins[0]), Q_rint(out->mins[1]), Q_rint(out->mins[2]) );
			Com_DPrintf( S_COLOR_YELLOW "maxs: %i %i %i\n", Q_rint(out->maxs[0]), Q_rint(out->maxs[1]), Q_rint(out->maxs[2]) );
			Com_DPrintf( S_COLOR_YELLOW "cluster: %i\n", LittleLong( in->cluster ) );
			Com_DPrintf( S_COLOR_YELLOW "surfaces: %i\n", LittleLong( in->numleaffaces ) );
			Com_DPrintf( S_COLOR_YELLOW "brushes: %i\n", LittleLong( in->numleafbrushes ) );
			out->cluster = -1;
		}

		if( loadmodel->vis ) {
			if( out->cluster >= loadmodel->vis->numclusters )
				Com_Error( ERR_DROP, "MOD_LoadBmodel: leaf cluster > numclusters" );
		}

		out->plane = NULL;
		out->area = LittleLong( in->area ) + 1;

		numMarkSurfaces = LittleLong( in->numleaffaces );
		if( !numMarkSurfaces )
			continue;
		firstMarkSurface = LittleLong( in->firstleafface );
		if( firstMarkSurface < 0 || numMarkSurfaces + firstMarkSurface > countMarkSurfaces )
			Com_Error( ERR_DROP, "MOD_LoadBmodel: bad marksurfaces in leaf %i", i );

		numVisSurfaces = numLitSurfaces = numFragmentSurfaces = 0;

		for( j = 0; j < numMarkSurfaces; j++ ) {
			k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );
			if( k < 0 ||  k >= loadmodel->numsurfaces )
				Com_Error( ERR_DROP, "Mod_LoadMarksurfaces: bad surface number" );

			if( R_SurfPotentiallyVisible( loadmodel->surfaces + k ) ) {
				numVisSurfaces++;
				if( R_SurfPotentiallyLit( loadmodel->surfaces + k ) )
					numLitSurfaces++;
				if( R_SurfPotentiallyFragmented( loadmodel->surfaces + k ) )
					numFragmentSurfaces++;
			}
		}

		if( !numVisSurfaces )
			continue;

		size = numVisSurfaces + 1;
		if( numLitSurfaces )
			size += numLitSurfaces + 1;
		if( numFragmentSurfaces )
			size += numFragmentSurfaces + 1;
		size *= sizeof( msurface_t * );

		buffer = ( qbyte * )Mod_Malloc( loadmodel, size );

		out->firstVisSurface = ( msurface_t ** )buffer;
		buffer += ( numVisSurfaces + 1 ) * sizeof( msurface_t * );
		if( numLitSurfaces ) {
			out->firstLitSurface = ( msurface_t ** )buffer;
			buffer += ( numLitSurfaces + 1 ) * sizeof( msurface_t * );
		}
		if( numFragmentSurfaces ) {
			out->firstFragmentSurface = ( msurface_t ** )buffer;
			buffer += ( numFragmentSurfaces + 1 ) * sizeof( msurface_t * );
		}

		numVisSurfaces = numLitSurfaces = numFragmentSurfaces = 0;

		for( j = 0; j < numMarkSurfaces; j++ ) {
			k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );

			if( R_SurfPotentiallyVisible( loadmodel->surfaces + k ) ) {
				out->firstVisSurface[numVisSurfaces++] = loadmodel->surfaces + k;
				if( R_SurfPotentiallyLit( loadmodel->surfaces + k ) )
					out->firstLitSurface[numLitSurfaces++] = loadmodel->surfaces + k;
				if( R_SurfPotentiallyFragmented( loadmodel->surfaces + k ) )
					out->firstFragmentSurface[numFragmentSurfaces++] = loadmodel->surfaces + k;
			}
		}
	}
}

/*
=================
Mod_LoadIndexes
=================
*/
static void Mod_LoadIndexes( const lump_t *l )
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
static void Mod_LoadPlanes( const lump_t *l )
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
static void Mod_LoadLightgrid( const lump_t *l )
{
	int				i, j, count;
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
	for( i = 0; i < count; i++, in++, out++ ) {
		out->styles[0] = 0;
		for( j = 1; j < MAX_LIGHTMAPS; j++ )
			out->styles[j] = 255;
		out->direction[0] = in->direction[0];
		out->direction[1] = in->direction[1];
		for( j = 0; j < 3; j++ ) {
			out->diffuse[0][j] = in->diffuse[j];
			out->ambient[0][j] = in->diffuse[j];
		}
	}
}

/*
=================
Mod_LoadLightgrid_RBSP
=================
*/
static void Mod_LoadLightgrid_RBSP( const lump_t *l )
{
	int				count;
	rdgridlight_t 	*in;
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
Mod_LoadLightArray
=================
*/
static void Mod_LoadLightArray( void )
{
	int				i, count;
	mgridlight_t 	**out;

	count = loadmodel->numlightgridelems;
	out = Mod_Malloc( loadmodel, sizeof(*out)*count );

	loadmodel->lightarray = out;
	loadmodel->numlightarrayelems = count;

	for( i = 0; i < count; i++, out++ )
		*out = loadmodel->lightgrid + i;
}

/*
=================
Mod_LoadLightArray_RBSP
=================
*/
static void Mod_LoadLightArray_RBSP( const lump_t *l )
{
	int				i, count;
	short			*in;
	mgridlight_t 	**out;

	in = ( void * )(mod_base + l->fileofs);
	if( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "Mod_LoadLightArray: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof(*in);
	out = Mod_Malloc( loadmodel, count*sizeof(*out) );

	loadmodel->lightarray = out;
	loadmodel->numlightarrayelems = count;

	for( i = 0; i < count; i++, in++, out++ )
		*out = loadmodel->lightgrid + LittleShort( *in );
}

/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities( const lump_t *l, vec3_t gridSize )
{
	char *data;
	mlight_t *out;
	int count, total, gridsizei[3];
	qboolean islight, isworld;
	float scale, gridsizef[3] = { 0, 0, 0 };
	char key[MAX_KEY], value[MAX_VALUE], target[MAX_VALUE], *token;

	data = (char *)mod_base + l->fileofs;
	if( !data || !data[0] )
		return;

	for( total = 0; (token = COM_Parse( &data )) && token[0] == '{'; ) {
		islight = qfalse;
		isworld = qfalse;

		while( 1 ) {
			if( !(token = COM_Parse( &data )) )
				break; // error
			if( token[0] == '}' )
				break; // end of entity

			Q_strncpyz( key, token, sizeof(key) );
			while( key[strlen(key)-1] == ' ' )	// remove trailing spaces
				key[strlen(key)-1] = 0;

			if( !(token = COM_Parse( &data )) )
				break; // error

			Q_strncpyz( value, token, sizeof(value) );

			// now that we have the key pair worked out...
			if( !strcmp( key, "classname" ) ) {
				if( !strncmp( value, "light", 5 ) )
					islight = qtrue;
				else if( !strcmp( value, "worldspawn" ) )
					isworld = qtrue;
			} else if( !strcmp( key, "gridsize" ) ) {
				sscanf( value, "%f %f %f", &gridsizef[0], &gridsizef[1], &gridsizef[2] );

				if( !gridsizef[0] || !gridsizef[1] || !gridsizef[2] ) {
					sscanf( value, "%i %i %i", &gridsizei[0], &gridsizei[1], &gridsizei[2] );
					VectorCopy( gridsizei, gridsizef );
				}
			}
		}

		if( isworld ) {
			VectorCopy( gridsizef, gridSize );
#if SHADOW_VOLUMES
			continue;
#else
			break;
#endif
		}

		if( islight )
			total++;
	}

#if SHADOW_VOLUMES
	if( !total )
#endif
		return;

	out = Mod_Malloc( loadmodel, total*sizeof(*out) );
	loadmodel->worldlights = out;
	loadmodel->numworldlights = total;

	data = mod_base + l->fileofs;
	for( count = 0; (token = COM_Parse( &data )) && token[0] == '{';  ) {
		if( count == total )
			break;

		islight = qfalse;

		while( 1 ) {
			if( !(token = COM_Parse( &data )) )
				break; // error
			if( token[0] == '}' )
				break; // end of entity

			Q_strncpyz( key, token, sizeof(key) );
			while( key[strlen(key)-1] == ' ' )		// remove trailing spaces
				key[strlen(key)-1] = 0;

			if( !(token = COM_Parse( &data )) )
				break; // error

			Q_strncpyz( value, token, sizeof(value) );

			// now that we have the key pair worked out...
			if( !strcmp( key, "origin" ) )
				sscanf ( value, "%f %f %f", &out->origin[0], &out->origin[1], &out->origin[2] );
			else if( !strcmp( key, "color" ) || !strcmp( key, "_color" ) )
				sscanf ( value, "%f %f %f", &out->color[0], &out->color[1], &out->color[2] );
			else if( !strcmp( key, "light" ) || !strcmp( key, "_light" ) )
				out->intensity = atof( value );
			else if (!strcmp( key, "classname" ) ) {
				if( !strncmp( value, "light", 5 ) )
					islight = qtrue;
			} else if( !strcmp( key, "target" ) )
				Q_strncpyz( target, value, sizeof( target ) );
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
static void Mod_SetParent( mnode_t *node, mnode_t *parent )
{
	node->parent = parent;
	if( !node->plane )
		return;
	Mod_SetParent( node->children[0], node );
	Mod_SetParent( node->children[1], node );
}

/*
=================
Mod_ApplySuperStylesToFace
=================
*/
static inline void Mod_ApplySuperStylesToFace( const rdface_t *in, msurface_t *out )
{
	int				j, k;
	float			*lmArray;
	mesh_t			*mesh;
	mlightmapRect_t *lmRect;
	int				lightmaps[MAX_LIGHTMAPS];
	qbyte			lightmapStyles[MAX_LIGHTMAPS], vertexStyles[MAX_LIGHTMAPS];

	for( j = 0; j < MAX_LIGHTMAPS; j++ ) {
		lightmaps[j] = LittleLong( in->lm_texnum[j] );

		if( lightmaps[j] < 0 || out->facetype == FACETYPE_FLARE || lightmaps[j] >= loadmodel->numlightmaps ) {
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
		} else {
			mesh = out->mesh;
			lmRect = &loadmodel->lightmapRects[lightmaps[j]];
			lightmaps[j] = loadmodel->lightmapRects[lightmaps[j]].texNum;

			if( r_ligtmapsPacking ) {	// scale/shift lightmap coords
				lmArray = mesh->lmstArray[j][0];
				for( k = 0; k < mesh->numVertexes; k++, lmArray += 2 ) {
					lmArray[0] = (double)(lmArray[0]) * lmRect->texMatrix[0][0] + lmRect->texMatrix[0][1];
					lmArray[1] = (double)(lmArray[1]) * lmRect->texMatrix[1][0] + lmRect->texMatrix[1][1];
				}
			}
			lightmapStyles[j] = in->lightmapStyles[j];
		}
		vertexStyles[j] = in->vertexStyles[j];
	}
	out->superLightStyle = R_AddSuperLightStyle( lightmaps, lightmapStyles, vertexStyles );
}

/*
=================
Mod_Finish
=================
*/
static void Mod_Finish( const lump_t *faces, const lump_t *light )
{
	int				i, j;
	msurface_t 		*surf;

	R_SortSuperLightStyles ();

	if( mod_bspFormat->flags & BSP_RAVEN ) {
		rdface_t	*in = ( void * )(mod_base + faces->fileofs);

		for( i = 0, surf = loadmodel->surfaces; i < loadmodel->numsurfaces; i++, in++, surf++ ) {
			if( !R_SurfPotentiallyVisible( surf ) )
				continue;
			Mod_ApplySuperStylesToFace( in, surf );
		}
	} else {
		rdface_t	rdf;
		dface_t		*in = ( void * )(mod_base + faces->fileofs);

		rdf.lightmapStyles[0] = rdf.vertexStyles[0] = 0;
		for( j = 1; j < MAX_LIGHTMAPS; j++ ) {
			rdf.lm_texnum[j] = -1;
			rdf.lightmapStyles[j] = rdf.vertexStyles[j] = 255;
		}

		for( i = 0, surf = loadmodel->surfaces; i < loadmodel->numsurfaces; i++, in++, surf++ ) {
			if( !R_SurfPotentiallyVisible( surf ) )
				continue;
			rdf.lm_texnum[0] = in->lm_texnum;
			Mod_ApplySuperStylesToFace( &rdf, surf );
		}
	}

	if( loadmodel->numlightmaps )
		Mod_Free( loadmodel->lightmapRects );

	Mod_SetParent( loadmodel->nodes, NULL );
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel( model_t *mod, model_t *parent, void *buffer )
{
	int			i, j;
	int			version;
	dheader_t	*header;
	mmodel_t 	*bm;
	vec3_t		maxs, gridSize;
			
	mod->type = mod_brush;
	if( mod != mod_known )
		Com_Error( ERR_DROP, "Loaded a brush model after the world" );

	header = (dheader_t *)buffer;

	version = LittleLong( header->version );
	for( i = 0, mod_bspFormat = bspFormats; i < numBspFormats; i++, mod_bspFormat++ ) {
		if( !strncmp( (unsigned char *)buffer, mod_bspFormat->header, 4 ) && (version == mod_bspFormat->version) )
			break;
	}
	if( i == numBspFormats )
		Com_Error( ERR_DROP, "Mod_LoadBrushModel: %s: unknown bsp format, version %i", mod->name, version );

	mod_base = (qbyte *)header;

	// swap all the lumps
	for( i = 0; i < sizeof(dheader_t)/4; i++ ) 
		((int *)header)[i] = LittleLong( ((int *)header)[i]);

	// load into heap
	Mod_LoadEntities( &header->lumps[LUMP_ENTITIES], gridSize );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadVertexes_RBSP( &header->lumps[LUMP_VERTEXES] );
	else
		Mod_LoadVertexes( &header->lumps[LUMP_VERTEXES] );
	Mod_LoadIndexes( &header->lumps[LUMP_INDEXES] );
	Mod_LoadLighting( &header->lumps[LUMP_LIGHTING] );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadLightgrid_RBSP( &header->lumps[LUMP_LIGHTGRID] );
	else
		Mod_LoadLightgrid( &header->lumps[LUMP_LIGHTGRID] );
	Mod_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	Mod_LoadShaderrefs( &header->lumps[LUMP_SHADERREFS] );
	Mod_LoadPlanes( &header->lumps[LUMP_PLANES] );
	Mod_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES] );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadFaces_RBSP( &header->lumps[LUMP_FACES] );
	else
		Mod_LoadFaces( &header->lumps[LUMP_FACES] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS], &header->lumps[LUMP_LEAFFACES] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadLightArray_RBSP( &header->lumps[LUMP_LIGHTARRAY] );
	else
		Mod_LoadLightArray ();

	Mod_Finish( &header->lumps[LUMP_FACES], &header->lumps[LUMP_LIGHTING] );

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

		if( i == 0 ) {
			*mod = *starmod;

			// set up lightgrid
			if( gridSize[0] < 1 || gridSize[1] < 1 || gridSize[2] < 1 )
				VectorSet( bm->gridSize, 64, 64, 128 );
			else
				VectorCopy( gridSize, bm->gridSize );

			for( j = 0; j < 3; j++ ) {
				bm->gridMins[j] = bm->gridSize[j] * ceil( (mod->mins[j] + 1) / bm->gridSize[j] );
				maxs[j] = bm->gridSize[j] * floor( (mod->maxs[j] - 1) / bm->gridSize[j] );
				bm->gridBounds[j] = (maxs[j] - bm->gridMins[j])/bm->gridSize[j] + 1;
			}
			bm->gridBounds[3] = bm->gridBounds[1] * bm->gridBounds[0];
		}
	}
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
	char lightmapsPath[MAX_QPATH], *p;

	if( r_packlightmaps->integer )
		r_ligtmapsPacking = qtrue;
	else
		r_ligtmapsPacking = qfalse;

	Q_strncpyz( lightmapsPath, model, sizeof (lightmapsPath) );
	p = strrchr( lightmapsPath, '.' );
	if( p ) {
		*p = 0;
		Q_strncatz( lightmapsPath, "/lm_0000.tga", sizeof (lightmapsPath) );
		if( FS_FOpenFile( lightmapsPath, NULL, FS_READ ) != -1 ) {
			Com_DPrintf( S_COLOR_YELLOW "External lightmap stage: lightmaps packing is disabled\n" );
			r_ligtmapsPacking = qfalse;
		}
	}

	r_farclip_min = 0;		// sky shaders will most likely modify this value
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
