/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2002-2007 Victor Luchits

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

typedef struct
{
	char name[MAX_QPATH];
	int flags;
	shader_t *shader;
} mshaderref_t;

typedef struct
{
	const char *header;
	int headerLen;
	int maxLods;
	void ( *loader )( model_t *mod, model_t *parent, void *buffer );
} modelformatdescriptor_t;

static model_t *loadmodel;
static int loadmodel_numverts;
static vec4_t *loadmodel_xyz_array;                       // vertexes
static vec4_t *loadmodel_normals_array;                   // normals
static vec2_t *loadmodel_st_array;                        // texture coords
static vec2_t *loadmodel_lmst_array[MAX_LIGHTMAPS];       // lightmap texture coords
static byte_vec4_t *loadmodel_colors_array[MAX_LIGHTMAPS];     // colors used for vertex lighting

static int loadmodel_numsurfelems;
static elem_t *loadmodel_surfelems;

static int loadmodel_numlightmaps;
static mlightmapRect_t *loadmodel_lightmapRects;

static int loadmodel_numshaderrefs;
static mshaderref_t *loadmodel_shaderrefs;

#ifdef QUAKE2_JUNK
void Mod_LoadAliasMD2Model( model_t *mod, model_t *parent, void *buffer );
#endif
void Mod_LoadAliasMD3Model( model_t *mod, model_t *parent, void *buffer );
#ifdef QUAKE2_JUNK
void Mod_LoadSpriteModel( model_t *mod, model_t *parent, void *buffer );
#endif
void Mod_LoadSkeletalModel( model_t *mod, model_t *parent, void *buffer );
void Mod_LoadBrushModel( model_t *mod, model_t *parent, void *buffer );

model_t *Mod_LoadModel( model_t *mod, qboolean crash );

static qbyte mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512*4
static model_t mod_known[MAX_MOD_KNOWN];
static int mod_numknown;
static int modfilelen;

static bspFormatDesc_t *mod_bspFormat;

// the inline * models from the current map are kept separate
static model_t *mod_inline;

static mempool_t *mod_mempool;

static modelformatdescriptor_t mod_supportedformats[] =
{
#ifdef QUAKE2_JUNK
	// Quake2 .md2 models
	{ IDMD2HEADER, 4, MD3_ALIAS_MAX_LODS, Mod_LoadAliasMD2Model },
#endif
	// Quake III Arena .md3 models
	{ IDMD3HEADER, 4, MD3_ALIAS_MAX_LODS, Mod_LoadAliasMD3Model },
#ifdef QUAKE2_JUNK
	// Quake2 .sp2 sprites
	{ IDSP2HEADER, 4, 0, Mod_LoadSpriteModel },
#endif
	// Skeletal models
	{ SKMHEADER, 4,	SKM_MAX_LODS, Mod_LoadSkeletalModel },

	// Quake III Arena .bsp models
	{ IDBSPHEADER, 4, 0, Mod_LoadBrushModel },

	// SOF2 and JK2 .bsp models
	{ RBSPHEADER, 4, 0, Mod_LoadBrushModel },

	// qfusion .bsp models
	{ QFBSPHEADER, 4, 0, Mod_LoadBrushModel },

	// trailing NULL
	{ NULL,	0, 0, NULL }
};

static int mod_numsupportedformats = sizeof( mod_supportedformats ) / sizeof( mod_supportedformats[0] ) - 1;

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf( vec3_t p, model_t *model )
{
	mnode_t	*node;
	cplane_t *plane;
	mbrushmodel_t *bmodel;

	if( !model || !( bmodel = ( mbrushmodel_t * )model->extradata ) || !bmodel->nodes )
	{
		Com_Error( ERR_DROP, "Mod_PointInLeaf: bad model" );
		return NULL;
	}

	node = bmodel->nodes;
	do
	{
		plane = node->plane;
		node = node->children[PlaneDiff( p, plane ) < 0];
	}
	while( node->plane != NULL );

	return ( mleaf_t * )node;
}

/*
==============
Mod_ClusterPVS
==============
*/
qbyte *Mod_ClusterPVS( int cluster, model_t *model )
{
	mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;

	if( cluster == -1 || !bmodel->vis )
		return mod_novis;
	return ( (qbyte *)bmodel->vis->data + cluster*bmodel->vis->rowsize );
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f( void )
{
	int i;
	model_t	*mod;
	int total;

	total = 0;
	Com_Printf( "Loaded models:\n" );
	for( i = 0, mod = mod_known; i < mod_numknown; i++, mod++ )
	{
		if( !mod->name )
			break;
		Com_Printf( "%8i : %s\n", mod->mempool->totalsize, mod->name );
		total += mod->mempool->totalsize;
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
	memset( mod_novis, 0xff, sizeof( mod_novis ) );
	mod_mempool = Mem_AllocPool( NULL, "Models" );
}

/*
================
R_ShutdownModels
================
*/
void R_ShutdownModels( void )
{
	int i;

	if( !mod_mempool )
		return;

	if( mod_inline )
	{
		Mem_Free( mod_inline );
		mod_inline = NULL;
	}

	for( i = 0; i < mod_numknown; i++ )
	{
		if( mod_known[i].mempool )
			Mem_FreePool( &mod_known[i].mempool );
	}

	r_worldmodel = NULL;
	r_worldbrushmodel = NULL;

	mod_numknown = 0;
	memset( mod_known, 0, sizeof( mod_known ) );

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
	if( lodnum < MD3_ALIAS_MAX_LODS )
	{
		if( name[len-2] == '_' )
			name[len-2] = 0;
	}
}

/*
==================
Mod_FindSlot
==================
*/
static model_t *Mod_FindSlot( const char *name, const char *shortname )
{
	int i;
	model_t	*mod, *best;
	size_t shortlen = shortname ? strlen( shortname ) : 0;

	//
	// search the currently loaded models
	//
	for( i = 0, mod = mod_known, best = NULL; i < mod_numknown; i++, mod++ )
	{
		if( !Q_stricmp( mod->name, name ) )
			return mod;

		if( ( mod->type == mod_bad ) && shortlen )
		{
			if( !Q_strnicmp( mod->name, shortname, shortlen ) )
			{                                               // same basename, different extension
				best = mod;
				shortlen = 0;
			}
		}
	}

	//
	// return best candidate
	//
	if( best )
		return best;

	//
	// find a free model slot spot
	//
	if( mod_numknown == MAX_MOD_KNOWN )
		Com_Error( ERR_DROP, "mod_numknown == MAX_MOD_KNOWN" );
	return &mod_known[mod_numknown];
}

/*
==================
Mod_Handle
==================
*/
unsigned int Mod_Handle( model_t *mod )
{
	return mod - mod_known;
}

/*
==================
Mod_ForHandle
==================
*/
model_t *Mod_ForHandle( unsigned int elem )
{
	return mod_known + elem;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName( const char *name, qboolean crash )
{
	int i;
	model_t	*mod, *lod;
	unsigned *buf;
	char shortname[MAX_QPATH], lodname[MAX_QPATH];
	const char *extension;
	modelformatdescriptor_t *descr;
	qbyte stack[0x4000];

	if( !name[0] )
		Com_Error( ERR_DROP, "Mod_ForName: NULL name" );

	//
	// inline models are grabbed only from worldmodel
	//
	if( name[0] == '*' )
	{
		i = atoi( name+1 );
		if( i < 1 || !r_worldmodel || i >= r_worldbrushmodel->numsubmodels )
			Com_Error( ERR_DROP, "bad inline model number" );
		return &mod_inline[i];
	}

	COM_StripExtension( name, shortname );
	extension = &name[strlen( shortname )+1];

	mod = Mod_FindSlot( name, shortname );
	if( mod->name && !strcmp( mod->name, name ) )
		return mod->type != mod_bad ? mod : NULL;

	//
	// load the file
	//
	modfilelen = FS_LoadFile( name, (void **)&buf, stack, sizeof( stack ) );
	if( !buf && crash )
		Com_Error( ERR_DROP, "Mod_NumForName: %s not found", name );

	if( mod->mempool )  // overwrite
		Mem_FreePool( &mod->mempool );
	else
		mod_numknown++;

	mod->type = mod_bad;
	mod->mempool = Mem_AllocPool( mod_mempool, name );
	mod->name = Mod_Malloc( mod, strlen( name ) + 1 );
	strcpy( mod->name, name );

	loadmodel = mod;
	loadmodel_xyz_array = NULL;
	loadmodel_surfelems = NULL;
	loadmodel_lightmapRects = NULL;
	loadmodel_shaderrefs = NULL;

	// return the NULL model
	if( !buf )
		return NULL;

	// call the apropriate loader
	descr = mod_supportedformats;
	for( i = 0; i < mod_numsupportedformats; i++, descr++ )
	{
		if( !strncmp( (const char *)buf, descr->header, descr->headerLen ) )
			break;
	}

	if( i == mod_numsupportedformats )
		Com_Error( ERR_DROP, "Mod_NumForName: unknown fileid for %s", mod->name );

	descr->loader( mod, NULL, buf );
	if( ( qbyte *)buf != stack )
		FS_FreeFile( buf );

	if( !descr->maxLods )
		return mod;

	//
	// load level-of-detail models
	//
	mod->numlods = 0;
	for( i = 0; i < descr->maxLods; i++ )
	{
		Q_snprintfz( lodname, sizeof( lodname ), "%s_%i.%s", shortname, i+1, extension );
		FS_LoadFile( lodname, (void **)&buf, stack, sizeof( stack ) );
		if( !buf || strncmp( (const char *)buf, descr->header, descr->headerLen ) )
			break;

		lod = mod->lods[i] = Mod_FindSlot( lodname, NULL );
		if( lod->name && !strcmp( lod->name, lodname ) )
			continue;

		lod->type = mod_bad;
		lod->mempool = Mem_AllocPool( mod_mempool, lodname );
		lod->name = Mod_Malloc( lod, strlen( lodname ) + 1 );
		strcpy( lod->name, lodname );

		loadmodel = lod;
		loadmodel_xyz_array = NULL;
		loadmodel_surfelems = NULL;
		loadmodel_lightmapRects = NULL;
		loadmodel_shaderrefs = NULL;
		mod_numknown++;

		descr->loader( lod, mod, buf );
		if( (qbyte *)buf != stack )
			FS_FreeFile( buf );

		mod->numlods++;
	}

	loadmodel = mod;
	return mod;
}

/*
===============================================================================

BRUSHMODEL LOADING

===============================================================================
*/

static qbyte *mod_base;
static mbrushmodel_t *loadbmodel;

/*
=================
Mod_CheckDeluxemaps
=================
*/
static void Mod_CheckDeluxemaps( const lump_t *l, qbyte *lmData )
{
	int i, j;
	int surfaces, lightmap;

	// there are no deluxemaps in the map if the number of lightmaps is
	// less than 2 or odd
	if( !r_lighting_deluxemapping->integer || loadmodel_numlightmaps < 2 || loadmodel_numlightmaps & 1 )
		return;

	if( mod_bspFormat->flags & BSP_RAVEN )
	{
		rdface_t *in = ( void * )( mod_base + l->fileofs );

		surfaces = l->filelen / sizeof( *in );
		for( i = 0; i < surfaces; i++, in++ )
		{
			for( j = 0; j < MAX_LIGHTMAPS; j++ )
			{
				lightmap = LittleLong( in->lm_texnum[j] );
				if( lightmap <= 0 )
					continue;
				if( lightmap & 1 )
					return;
			}
		}
	}
	else
	{
		dface_t	*in = ( void * )( mod_base + l->fileofs );

		surfaces = l->filelen / sizeof( *in );
		for( i = 0; i < surfaces; i++, in++ )
		{
			lightmap = LittleLong( in->lm_texnum );
			if( lightmap <= 0 )
				continue;
			if( lightmap & 1 )
				return;
		}
	}

	// check if the deluxemap is actually empty (q3map2, yay!)
	if( loadmodel_numlightmaps == 2 )
	{
		int lW = mod_bspFormat->lightmapWidth, lH = mod_bspFormat->lightmapHeight;

		lmData += lW * lH * LIGHTMAP_BYTES;
		for( i = lW * lH; i > 0; i--, lmData += LIGHTMAP_BYTES )
		{
			for( j = 0; j < LIGHTMAP_BYTES; j++ )
			{
				if( lmData[j] )
					break;
			}
			if( j != LIGHTMAP_BYTES )
				break;
		}

		// empty deluxemap
		if( !i )
		{
			loadmodel_numlightmaps = 1;
			return;
		}
	}

	mapConfig.deluxeMaps = qtrue;
	if( glConfig.ext.GLSL )
		mapConfig.deluxeMappingEnabled = qtrue;
}

/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting( const lump_t *l, const lump_t *faces )
{
	int size;

	if( !l->filelen )
		return;
	size = mod_bspFormat->lightmapWidth * mod_bspFormat->lightmapHeight * LIGHTMAP_BYTES;
	if( l->filelen % size )
		Com_Error( ERR_DROP, "Mod_LoadLighting: funny lump size in %s", loadmodel->name );

	loadmodel_numlightmaps = l->filelen / size;
	loadmodel_lightmapRects = Mod_Malloc( loadmodel, loadmodel_numlightmaps * sizeof( *loadmodel_lightmapRects ) );

	Mod_CheckDeluxemaps( faces, mod_base + l->fileofs );

	// set overbright bits for lightmaps and lightgrid
	// deluxemapped maps have zero scale because most surfaces
	// have a gloss stage that makes them look brighter anyway
	/*if( mapConfig.deluxeMapping )
	mapConfig.pow2MapOvrbr = 0;
	else */if(r_ignorehwgamma->integer)
		mapConfig.pow2MapOvrbr = r_mapoverbrightbits->integer;
	else
		mapConfig.pow2MapOvrbr = r_mapoverbrightbits->integer - r_overbrightbits->integer;
	if( mapConfig.pow2MapOvrbr < 0 )
		mapConfig.pow2MapOvrbr = 0;

	R_BuildLightmaps( loadmodel_numlightmaps, mod_bspFormat->lightmapWidth, mod_bspFormat->lightmapHeight, mod_base + l->fileofs, loadmodel_lightmapRects );
}

/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes( const lump_t *l )
{
	int i, count, j;
	dvertex_t *in;
	float *out_xyz, *out_normals, *out_st, *out_lmst;
	qbyte *buffer, *out_colors;
	size_t bufSize;
	vec3_t color, fcolor;
	float div;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadVertexes: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );

	bufSize = 0;
	bufSize += count * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t )*2 + sizeof( byte_vec4_t ) );
	buffer = Mod_Malloc( loadmodel, bufSize );

	loadmodel_numverts = count;
	loadmodel_xyz_array = ( vec4_t * )buffer; buffer += count*sizeof( vec4_t );
	loadmodel_normals_array = ( vec4_t * )buffer; buffer += count*sizeof( vec4_t );
	loadmodel_st_array = ( vec2_t * )buffer; buffer += count*sizeof( vec2_t );
	loadmodel_lmst_array[0] = ( vec2_t * )buffer; buffer += count*sizeof( vec2_t );
	loadmodel_colors_array[0] = ( byte_vec4_t * )buffer; buffer += count*sizeof( byte_vec4_t );
	for( i = 1; i < MAX_LIGHTMAPS; i++ )
	{
		loadmodel_lmst_array[i] = loadmodel_lmst_array[0];
		loadmodel_colors_array[i] = loadmodel_colors_array[0];
	}

	out_xyz = loadmodel_xyz_array[0];
	out_normals = loadmodel_normals_array[0];
	out_st = loadmodel_st_array[0];
	out_lmst = loadmodel_lmst_array[0][0];
	out_colors = loadmodel_colors_array[0][0];

	if( r_mapoverbrightbits->integer > 0 )
		div = (float)( 1 << r_mapoverbrightbits->integer ) / 255.0f;
	else
		div = 1.0f / 255.0f;

	for( i = 0; i < count; i++, in++, out_xyz += 4, out_normals += 4, out_st += 2, out_lmst += 2, out_colors += 4 )
	{
		for( j = 0; j < 3; j++ )
		{
			out_xyz[j] = LittleFloat( in->point[j] );
			out_normals[j] = LittleFloat( in->normal[j] );
		}
		out_xyz[3] = 1;
		out_normals[3] = 0;

		for( j = 0; j < 2; j++ )
		{
			out_st[j] = LittleFloat( in->tex_st[j] );
			out_lmst[j] = LittleFloat( in->lm_st[j] );
		}

		if( r_fullbright->integer )
		{
			out_colors[0] = 255;
			out_colors[1] = 255;
			out_colors[2] = 255;
			out_colors[3] = in->color[3];
		}
		else
		{
			for( j = 0; j < 3; j++ )
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
	int i, count, j;
	rdvertex_t *in;
	float *out_xyz, *out_normals, *out_st, *out_lmst[MAX_LIGHTMAPS];
	qbyte *buffer, *out_colors[MAX_LIGHTMAPS];
	size_t bufSize;
	vec3_t color, fcolor;
	float div;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadVertexes: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );

	bufSize = 0;
	bufSize += count * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) + ( sizeof( vec2_t ) + sizeof( byte_vec4_t ) )*MAX_LIGHTMAPS );
	buffer = Mod_Malloc( loadmodel, bufSize );

	loadmodel_numverts = count;
	loadmodel_xyz_array = ( vec4_t * )buffer; buffer += count*sizeof( vec4_t );
	loadmodel_normals_array = ( vec4_t * )buffer; buffer += count*sizeof( vec4_t );
	loadmodel_st_array = ( vec2_t * )buffer; buffer += count*sizeof( vec2_t );
	for( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		loadmodel_lmst_array[i] = ( vec2_t * )buffer; buffer += count*sizeof( vec2_t );
		loadmodel_colors_array[i] = ( byte_vec4_t * )buffer; buffer += count*sizeof( byte_vec4_t );
	}

	out_xyz = loadmodel_xyz_array[0];
	out_normals = loadmodel_normals_array[0];
	out_st = loadmodel_st_array[0];
	for( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		out_lmst[i] = loadmodel_lmst_array[i][0];
		out_colors[i] = loadmodel_colors_array[i][0];
	}

	if( r_mapoverbrightbits->integer > 0 )
		div = (float)( 1 << r_mapoverbrightbits->integer ) / 255.0f;
	else
		div = 1.0f / 255.0f;

	for( i = 0; i < count; i++, in++, out_xyz += 4, out_normals += 4, out_st += 2 )
	{
		for( j = 0; j < 3; j++ )
		{
			out_xyz[j] = LittleFloat( in->point[j] );
			out_normals[j] = LittleFloat( in->normal[j] );
		}
		out_xyz[3] = 1;
		out_normals[3] = 0;

		for( j = 0; j < 2; j++ )
			out_st[j] = LittleFloat( in->tex_st[j] );

		for( j = 0; j < MAX_LIGHTMAPS; out_lmst[j] += 2, out_colors[j] += 4, j++ )
		{
			out_lmst[j][0] = LittleFloat( in->lm_st[j][0] );
			out_lmst[j][1] = LittleFloat( in->lm_st[j][1] );

			if( r_fullbright->integer )
			{
				out_colors[j][0] = 255;
				out_colors[j][1] = 255;
				out_colors[j][2] = 255;
				out_colors[j][3] = in->color[j][3];
			}
			else
			{
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
	int i, j, count;
	dmodel_t *in;
	mmodel_t *out;
	mbrushmodel_t *bmodel;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadSubmodels: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	mod_inline = Mod_Malloc( loadmodel, count*( sizeof( *mod_inline )+sizeof( *bmodel ) ) );
	loadmodel->extradata = bmodel = ( mbrushmodel_t * )( ( qbyte * )mod_inline + count*sizeof( *mod_inline ) );

	loadbmodel = bmodel;
	loadbmodel->submodels = out;
	loadbmodel->numsubmodels = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		mod_inline[i].extradata = bmodel + i;

		for( j = 0; j < 3; j++ )
		{
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
	int i, count;
	int contents;
	dshaderref_t *in;
	mshaderref_t *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadShaderrefs: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadmodel_shaderrefs = out;
	loadmodel_numshaderrefs = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		Q_strncpyz( out->name, in->name, sizeof( out->name ) );
		out->flags = LittleLong( in->flags );
		contents = LittleLong( in->contents );
		if( contents & ( MASK_WATER|CONTENTS_FOG ) )
			out->flags |= SURF_NOMARKS;
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
	qboolean createSTverts;
	qbyte *buffer;
	size_t bufSize;

	if( ( mapConfig.deluxeMappingEnabled && !(LittleLong( in->lm_texnum[0] ) < 0 || in->lightmapStyles[0] == 255) ) ||
		( out->shader->flags & SHADER_PORTAL_CAPTURE2 ) )
	{
		createSTverts = qtrue;
	}
	else
	{
		createSTverts = qfalse;
	}

	switch( out->facetype )
	{
	case FACETYPE_FLARE:
		{
			int j;

			for( j = 0; j < 3; j++ )
			{
				out->origin[j] = LittleFloat( in->origin[j] );
				out->color[j] = bound( 0, LittleFloat( in->mins[j] ), 1 );
			}
			break;
		}
	case FACETYPE_PATCH:
		{
			int i, j, u, v, p;
			int patch_cp[2], step[2], size[2], flat[2];
			float subdivLevel, f;
			int numVerts, firstVert;
			vec4_t tempv[MAX_ARRAY_VERTS];
			vec4_t colors[MAX_ARRAY_VERTS];
			elem_t	*elems;

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
			Patch_GetFlatness( subdivLevel, (vec_t *)loadmodel_xyz_array[firstVert], 4, patch_cp, flat );

			// allocate space for mesh
			step[0] = ( 1 << flat[0] );
			step[1] = ( 1 << flat[1] );
			size[0] = ( patch_cp[0] >> 1 ) * step[0] + 1;
			size[1] = ( patch_cp[1] >> 1 ) * step[1] + 1;
			numVerts = size[0] * size[1];

			if( numVerts > MAX_ARRAY_VERTS )
				break;

			bufSize = sizeof( mesh_t ) + numVerts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) );
			for( j = 0; j < MAX_LIGHTMAPS && in->lightmapStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( vec2_t );
			for( j = 0; j < MAX_LIGHTMAPS && in->vertexStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( byte_vec4_t );
			if( createSTverts )
				bufSize += numVerts * sizeof( vec4_t );
			buffer = ( qbyte * )Mod_Malloc( loadmodel, bufSize );

			mesh = ( mesh_t * )buffer; buffer += sizeof( mesh_t );
			mesh->numVertexes = numVerts;
			mesh->xyzArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
			mesh->normalsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
			mesh->stArray = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );

			Patch_Evaluate( loadmodel_xyz_array[firstVert], patch_cp, step, mesh->xyzArray[0], 4 );
			Patch_Evaluate( loadmodel_normals_array[firstVert], patch_cp, step, mesh->normalsArray[0], 4 );
			Patch_Evaluate( loadmodel_st_array[firstVert], patch_cp, step, mesh->stArray[0], 2 );
			for( i = 0; i < numVerts; i++ )
				VectorNormalize( mesh->normalsArray[i] );

			for( j = 0; j < MAX_LIGHTMAPS && in->lightmapStyles[j] != 255; j++ )
			{
				mesh->lmstArray[j] = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );
				Patch_Evaluate( loadmodel_lmst_array[j][firstVert], patch_cp, step, mesh->lmstArray[j][0], 2 );
			}

			for( j = 0; j < MAX_LIGHTMAPS && in->vertexStyles[j] != 255; j++ )
			{
				mesh->colorsArray[j] = ( byte_vec4_t * )buffer; buffer += numVerts * sizeof( byte_vec4_t );
				for( i = 0; i < numVerts; i++ )
					Vector4Scale( loadmodel_colors_array[j][firstVert + i], ( 1.0f / 255.0f ), colors[i] );
				Patch_Evaluate( colors[0], patch_cp, step, tempv[0], 4 );

				for( i = 0; i < numVerts; i++ )
				{
					f = max( max( tempv[i][0], tempv[i][1] ), tempv[i][2] );
					if( f > 1.0f )
						f = 255.0f / f;
					else
						f = 255;

					mesh->colorsArray[j][i][0] = tempv[i][0] * f;
					mesh->colorsArray[j][i][1] = tempv[i][1] * f;
					mesh->colorsArray[j][i][2] = tempv[i][2] * f;
					mesh->colorsArray[j][i][3] = bound( 0, tempv[i][3], 1 ) * 255;
				}
			}

			// compute new elems
			mesh->numElems = ( size[0] - 1 ) * ( size[1] - 1 ) * 6;
			elems = mesh->elems = ( elem_t * )Mod_Malloc( loadmodel, mesh->numElems * sizeof( elem_t ) );
			for( v = 0, i = 0; v < size[1] - 1; v++ )
			{
				for( u = 0; u < size[0] - 1; u++ )
				{
					p = v * size[0] + u;
					elems[0] = p;
					elems[1] = p + size[0];
					elems[2] = p + 1;
					elems[3] = p + 1;
					elems[4] = p + size[0];
					elems[5] = p + size[0] + 1;
					elems += 6;
				}
			}

			if( createSTverts )
			{
				mesh->sVectorsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
				R_BuildTangentVectors( mesh->numVertexes, mesh->xyzArray, mesh->normalsArray, mesh->stArray, mesh->numElems / 3, mesh->elems, mesh->sVectorsArray );
			}
			break;
		}
	case FACETYPE_PLANAR:
	case FACETYPE_TRISURF:
		{
			int j, numVerts, firstVert, numElems, firstElem;

			numVerts = LittleLong( in->numverts );
			firstVert = LittleLong( in->firstvert );

			numElems = LittleLong( in->numelems );
			firstElem = LittleLong( in->firstelem );

			bufSize = sizeof( mesh_t ) + numVerts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) + numElems * sizeof( elem_t ) );
			for( j = 0; j < MAX_LIGHTMAPS && in->lightmapStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( vec2_t );
			for( j = 0; j < MAX_LIGHTMAPS && in->vertexStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( byte_vec4_t );
			if( createSTverts )
				bufSize += numVerts * sizeof( vec4_t );
			if( out->facetype == FACETYPE_PLANAR )
				bufSize += sizeof( cplane_t );
			buffer = ( qbyte * )Mod_Malloc( loadmodel, bufSize );

			mesh = ( mesh_t * )buffer; buffer += sizeof( mesh_t );
			mesh->numVertexes = numVerts;
			mesh->numElems = numElems;

			mesh->xyzArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
			mesh->normalsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
			mesh->stArray = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );

			memcpy( mesh->xyzArray, loadmodel_xyz_array + firstVert, numVerts * sizeof( vec4_t ) );
			memcpy( mesh->normalsArray, loadmodel_normals_array + firstVert, numVerts * sizeof( vec4_t ) );
			memcpy( mesh->stArray, loadmodel_st_array + firstVert, numVerts * sizeof( vec2_t ) );

			for( j = 0; j < MAX_LIGHTMAPS && in->lightmapStyles[j] != 255; j++ )
			{
				mesh->lmstArray[j] = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );
				memcpy( mesh->lmstArray[j], loadmodel_lmst_array[j] + firstVert, numVerts * sizeof( vec2_t ) );
			}
			for( j = 0; j < MAX_LIGHTMAPS && in->vertexStyles[j] != 255; j++ )
			{
				mesh->colorsArray[j] = ( byte_vec4_t * )buffer; buffer += numVerts * sizeof( byte_vec4_t );
				memcpy( mesh->colorsArray[j], loadmodel_colors_array[j] + firstVert, numVerts * sizeof( byte_vec4_t ) );
			}

			mesh->elems = ( elem_t * )buffer; buffer += numElems * sizeof( elem_t );
			memcpy( mesh->elems, loadmodel_surfelems + firstElem, numElems * sizeof( elem_t ) );

			if( createSTverts )
			{
				mesh->sVectorsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
				R_BuildTangentVectors( mesh->numVertexes, mesh->xyzArray, mesh->normalsArray, mesh->stArray, mesh->numElems / 3, mesh->elems, mesh->sVectorsArray );
			}

			if( out->facetype == FACETYPE_PLANAR )
			{
				cplane_t *plane;

				plane = out->plane = ( cplane_t * )buffer; buffer += sizeof( cplane_t );
				plane->type = PLANE_NONAXIAL;
				plane->signbits = 0;
				for( j = 0; j < 3; j++ )
				{
					plane->normal[j] = LittleFloat( in->normal[j] );
					if( plane->normal[j] == 1.0f )
						plane->type = j;
				}
				plane->dist = DotProduct( mesh->xyzArray[0], plane->normal );
			}
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
	int j, shaderType;
	mesh_t *mesh;
	mfog_t *fog;
	mshaderref_t *shaderref;
	int shadernum, fognum;
	float *vert;
	mlightmapRect_t *lmRects[MAX_LIGHTMAPS];
	int lightmaps[MAX_LIGHTMAPS];
	qbyte lightmapStyles[MAX_LIGHTMAPS], vertexStyles[MAX_LIGHTMAPS];
	vec3_t ebbox = { 0, 0, 0 };

	out->facetype = LittleLong( in->facetype );

	// lighting info
	for( j = 0; j < MAX_LIGHTMAPS; j++ )
	{
		lightmaps[j] = LittleLong( in->lm_texnum[j] );
		if( lightmaps[j] < 0 || out->facetype == FACETYPE_FLARE )
		{
			lmRects[j] = NULL;
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
		}
		else if( lightmaps[j] >= loadmodel_numlightmaps )
		{
			Com_DPrintf( S_COLOR_RED "WARNING: bad lightmap number: %i\n", lightmaps[j] );
			lmRects[j] = NULL;
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
		}
		else
		{
			lmRects[j] = &loadmodel_lightmapRects[lightmaps[j]];
			lightmaps[j] = lmRects[j]->texNum;
			lightmapStyles[j] = in->lightmapStyles[j];
		}
		vertexStyles[j] = in->vertexStyles[j];
	}

	// add this super style
	R_AddSuperLightStyle( lightmaps, lightmapStyles, vertexStyles, lmRects );

	shadernum = LittleLong( in->shadernum );
	if( shadernum < 0 || shadernum >= loadmodel_numshaderrefs )
		Com_Error( ERR_DROP, "MOD_LoadBmodel: bad shader number" );
	shaderref = loadmodel_shaderrefs + shadernum;

	if( out->facetype == FACETYPE_FLARE )
		shaderType = SHADER_BSP_FLARE;
	else if( /*out->facetype == FACETYPE_TRISURF || */ lightmaps[0] < 0 || lightmapStyles[0] == 255 )
		shaderType = SHADER_BSP_VERTEX;
	else
		shaderType = SHADER_BSP;

	if( !shaderref->shader )
	{
		shaderref->shader = R_LoadShader( shaderref->name, shaderType, qfalse, 0, SHADER_INVALID );
		out->shader = shaderref->shader;
		if( out->facetype == FACETYPE_FLARE )
			out->shader->flags |= SHADER_FLARE; // force SHADER_FLARE flag
	}
	else
	{
		// some q3a maps specify a lightmap shader for surfaces that do not have a lightmap,
		// workaround that... see pukka3tourney2 for example
		if( ( shaderType == SHADER_BSP_VERTEX && ( shaderref->shader->flags & SHADER_LIGHTMAP ) &&
			( shaderref->shader->passes[0].flags & SHADERPASS_LIGHTMAP ) ) )
			out->shader = R_LoadShader( shaderref->name, shaderType, qfalse, 0, shaderref->shader->type );
		else
			out->shader = shaderref->shader;
	}

	out->flags = shaderref->flags;
	R_DeformvBBoxForShader( out->shader, ebbox );

	fognum = LittleLong( in->fognum );
	if( fognum != -1 && ( fognum < loadbmodel->numfogs ) )
	{
		fog = loadbmodel->fogs + fognum;
		if( fog->shader && fog->shader->fog_dist )
			out->fog = fog;
	}

	mesh = out->mesh = Mod_CreateMeshForSurface( in, out );
	if( !mesh )
		return;

	ClearBounds( out->mins, out->maxs );
	for( j = 0, vert = mesh->xyzArray[0]; j < mesh->numVertexes; j++, vert += 4 )
		AddPointToBounds( vert, out->mins, out->maxs );
	VectorSubtract( out->mins, ebbox, out->mins );
	VectorAdd( out->maxs, ebbox, out->maxs );
}

/*
=================
Mod_LoadFaces
=================
*/
static void Mod_LoadFaces( const lump_t *l )
{
	int i, j, count;
	dface_t	*in;
	rdface_t rdf;
	msurface_t *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->surfaces = out;
	loadbmodel->numsurfaces = count;

	rdf.lightmapStyles[0] = rdf.vertexStyles[0] = 0;
	for( j = 1; j < MAX_LIGHTMAPS; j++ )
		rdf.lightmapStyles[j] = rdf.vertexStyles[j] = 255;

	for( i = 0; i < count; i++, in++, out++ )
	{
		rdf.facetype = in->facetype;
		rdf.lm_texnum[0] = in->lm_texnum;
		rdf.lightmapStyles[0] = rdf.vertexStyles[0] = 0;
		for( j = 1; j < MAX_LIGHTMAPS; j++ )
		{
			rdf.lm_texnum[j] = -1;
			rdf.lightmapStyles[j] = rdf.vertexStyles[j] = 255;
		}
		for( j = 0; j < 3; j++ )
		{
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
		rdf.firstelem = in->firstelem;
		rdf.numelems = in->numelems;
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
	int i, count;
	rdface_t *in;
	msurface_t *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->surfaces = out;
	loadbmodel->numsurfaces = count;

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
	int i, j, count, p;
	dnode_t	*in;
	mnode_t	*out;
	qboolean badBounds;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadNodes: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->nodes = out;
	loadbmodel->numnodes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->plane = loadbmodel->planes + LittleLong( in->planenum );

		for( j = 0; j < 2; j++ )
		{
			p = LittleLong( in->children[j] );
			if( p >= 0 )
				out->children[j] = loadbmodel->nodes + p;
			else
				out->children[j] = ( mnode_t * )( loadbmodel->leafs + ( -1 - p ) );
		}

		badBounds = qfalse;
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = (float)LittleLong( in->mins[j] );
			out->maxs[j] = (float)LittleLong( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] )
				badBounds = qtrue;
		}

		if( badBounds || VectorCompare( out->mins, out->maxs ) )
		{
			Com_DPrintf( S_COLOR_YELLOW "WARNING: bad node %i bounds:\n", i );
			Com_DPrintf( S_COLOR_YELLOW "mins: %i %i %i\n", Q_rint( out->mins[0] ), Q_rint( out->mins[1] ), Q_rint( out->mins[2] ) );
			Com_DPrintf( S_COLOR_YELLOW "maxs: %i %i %i\n", Q_rint( out->maxs[0] ), Q_rint( out->maxs[1] ), Q_rint( out->maxs[2] ) );
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
	int i, j, count, p;
	dfog_t *in;
	mfog_t *out;
	dbrush_t *inbrushes, *brush;
	dbrushside_t *inbrushsides = NULL, *brushside = NULL;
	rdbrushside_t *inrbrushsides = NULL, *rbrushside = NULL;

	inbrushes = ( void * )( mod_base + brLump->fileofs );
	if( brLump->filelen % sizeof( *inbrushes ) )
		Com_Error( ERR_DROP, "Mod_LoadBrushes: funny lump size in %s", loadmodel->name );

	if( mod_bspFormat->flags & BSP_RAVEN )
	{
		inrbrushsides = ( void * )( mod_base + brSidesLump->fileofs );
		if( brSidesLump->filelen % sizeof( *inrbrushsides ) )
			Com_Error( ERR_DROP, "Mod_LoadBrushsides: funny lump size in %s", loadmodel->name );
	}
	else
	{
		inbrushsides = ( void * )( mod_base + brSidesLump->fileofs );
		if( brSidesLump->filelen % sizeof( *inbrushsides ) )
			Com_Error( ERR_DROP, "Mod_LoadBrushsides: funny lump size in %s", loadmodel->name );
	}

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadFogs: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->fogs = out;
	loadbmodel->numfogs = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->shader = R_RegisterShader( in->shader );
		p = LittleLong( in->brushnum );
		if( p == -1 )
			continue;

		brush = inbrushes + p;
		p = LittleLong( brush->firstside );
		if( p == -1 )
		{
			out->shader = NULL;
			continue;
		}

		if( mod_bspFormat->flags & BSP_RAVEN )
			rbrushside = inrbrushsides + p;
		else
			brushside = inbrushsides + p;

		p = LittleLong( in->visibleside );
		out->numplanes = LittleLong( brush->numsides );
		out->planes = Mod_Malloc( loadmodel, out->numplanes * sizeof( cplane_t ) );

		if( mod_bspFormat->flags & BSP_RAVEN )
		{
			if( p != -1 )
				out->visibleplane = loadbmodel->planes + LittleLong( rbrushside[p].planenum );
			for( j = 0; j < out->numplanes; j++ )
				out->planes[j] = *( loadbmodel->planes + LittleLong( rbrushside[j].planenum ) );
		}
		else
		{
			if( p != -1 )
				out->visibleplane = loadbmodel->planes + LittleLong( brushside[p].planenum );
			for( j = 0; j < out->numplanes; j++ )
				out->planes[j] = *( loadbmodel->planes + LittleLong( brushside[j].planenum ) );
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
	int i, j, k, count, countMarkSurfaces;
	dleaf_t	*in;
	mleaf_t	*out;
	size_t size;
	qbyte *buffer;
	qboolean badBounds;
	int *inMarkSurfaces;
	int numVisLeafs;
	int numMarkSurfaces, firstMarkSurface;
	int numVisSurfaces, numFragmentSurfaces;

	inMarkSurfaces = ( void * )( mod_base + msLump->fileofs );
	if( msLump->filelen % sizeof( *inMarkSurfaces ) )
		Com_Error( ERR_DROP, "Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name );
	countMarkSurfaces = msLump->filelen / sizeof( *inMarkSurfaces );

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadLeafs: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->leafs = out;
	loadbmodel->numleafs = count;

	numVisLeafs = 0;
	loadbmodel->visleafs = Mod_Malloc( loadmodel, ( count+1 )*sizeof( out ) );
	memset( loadbmodel->visleafs, 0, ( count+1 )*sizeof( out ) );

	for( i = 0; i < count; i++, in++, out++ )
	{
		badBounds = qfalse;
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = (float)LittleLong( in->mins[j] );
			out->maxs[j] = (float)LittleLong( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] )
				badBounds = qtrue;
		}
		out->cluster = LittleLong( in->cluster );

		if( i && ( badBounds || VectorCompare( out->mins, out->maxs ) ) )
		{
			Com_DPrintf( S_COLOR_YELLOW "WARNING: bad leaf %i bounds:\n", i );
			Com_DPrintf( S_COLOR_YELLOW "mins: %i %i %i\n", Q_rint( out->mins[0] ), Q_rint( out->mins[1] ), Q_rint( out->mins[2] ) );
			Com_DPrintf( S_COLOR_YELLOW "maxs: %i %i %i\n", Q_rint( out->maxs[0] ), Q_rint( out->maxs[1] ), Q_rint( out->maxs[2] ) );
			Com_DPrintf( S_COLOR_YELLOW "cluster: %i\n", LittleLong( in->cluster ) );
			Com_DPrintf( S_COLOR_YELLOW "surfaces: %i\n", LittleLong( in->numleaffaces ) );
			Com_DPrintf( S_COLOR_YELLOW "brushes: %i\n", LittleLong( in->numleafbrushes ) );
			out->cluster = -1;
		}

		if( loadbmodel->vis )
		{
			if( out->cluster >= loadbmodel->vis->numclusters )
				Com_Error( ERR_DROP, "MOD_LoadBmodel: leaf cluster > numclusters" );
		}

		out->plane = NULL;
		out->area = LittleLong( in->area ) + 1;

		numMarkSurfaces = LittleLong( in->numleaffaces );
		if( !numMarkSurfaces )
		{
			//			out->cluster = -1;
			continue;
		}

		firstMarkSurface = LittleLong( in->firstleafface );
		if( firstMarkSurface < 0 || numMarkSurfaces + firstMarkSurface > countMarkSurfaces )
			Com_Error( ERR_DROP, "MOD_LoadBmodel: bad marksurfaces in leaf %i", i );

		numVisSurfaces = numFragmentSurfaces = 0;

		for( j = 0; j < numMarkSurfaces; j++ )
		{
			k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );
			if( k < 0 || k >= loadbmodel->numsurfaces )
				Com_Error( ERR_DROP, "Mod_LoadMarksurfaces: bad surface number" );

			if( R_SurfPotentiallyVisible( loadbmodel->surfaces + k ) )
			{
				numVisSurfaces++;
				if( R_SurfPotentiallyFragmented( loadbmodel->surfaces + k ) )
					numFragmentSurfaces++;
			}
		}

		if( !numVisSurfaces )
		{
			//out->cluster = -1;
			continue;
		}

		size = numVisSurfaces + 1;
		if( numFragmentSurfaces )
			size += numFragmentSurfaces + 1;
		size *= sizeof( msurface_t * );

		buffer = ( qbyte * )Mod_Malloc( loadmodel, size );

		out->firstVisSurface = ( msurface_t ** )buffer;
		buffer += ( numVisSurfaces + 1 ) * sizeof( msurface_t * );
		if( numFragmentSurfaces )
		{
			out->firstFragmentSurface = ( msurface_t ** )buffer;
			buffer += ( numFragmentSurfaces + 1 ) * sizeof( msurface_t * );
		}

		numVisSurfaces = numFragmentSurfaces = 0;

		for( j = 0; j < numMarkSurfaces; j++ )
		{
			k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );

			if( R_SurfPotentiallyVisible( loadbmodel->surfaces + k ) )
			{
				out->firstVisSurface[numVisSurfaces++] = loadbmodel->surfaces + k;
				if( R_SurfPotentiallyFragmented( loadbmodel->surfaces + k ) )
					out->firstFragmentSurface[numFragmentSurfaces++] = loadbmodel->surfaces + k;
			}
		}

		loadbmodel->visleafs[numVisLeafs++] = out;
	}

	loadbmodel->visleafs = Mem_Realloc( loadbmodel->visleafs, ( numVisLeafs+1 )*sizeof( out ) );
}

/*
=================
Mod_LoadElems
=================
*/
static void Mod_LoadElems( const lump_t *l )
{
	int i, count;
	int *in;
	elem_t	*out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadElems: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadmodel_surfelems = out;
	loadmodel_numsurfelems = count;

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
	int i, j;
	cplane_t *out;
	dplane_t *in;
	int count;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadPlanes: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->planes = out;
	loadbmodel->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->type = PLANE_NONAXIAL;
		out->signbits = 0;

		for( j = 0; j < 3; j++ )
		{
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
	int i, j, count;
	dgridlight_t *in;
	mgridlight_t *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadLightgrid: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->lightgrid = out;
	loadbmodel->numlightgridelems = count;

	// lightgrid is all 8 bit
	for( i = 0; i < count; i++, in++, out++ )
	{
		out->styles[0] = 0;
		for( j = 1; j < MAX_LIGHTMAPS; j++ )
			out->styles[j] = 255;
		out->direction[0] = in->direction[0];
		out->direction[1] = in->direction[1];
		for( j = 0; j < 3; j++ )
		{
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
	int count;
	rdgridlight_t *in;
	mgridlight_t *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadLightgrid: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->lightgrid = out;
	loadbmodel->numlightgridelems = count;

	// lightgrid is all 8 bit
	memcpy( out, in, count*sizeof( *out ) );
}

/*
=================
Mod_LoadLightArray
=================
*/
static void Mod_LoadLightArray( void )
{
	int i, count;
	mgridlight_t **out;

	count = loadbmodel->numlightgridelems;
	out = Mod_Malloc( loadmodel, sizeof( *out )*count );

	loadbmodel->lightarray = out;
	loadbmodel->numlightarrayelems = count;

	for( i = 0; i < count; i++, out++ )
		*out = loadbmodel->lightgrid + i;
}

/*
=================
Mod_LoadLightArray_RBSP
=================
*/
static void Mod_LoadLightArray_RBSP( const lump_t *l )
{
	int i, count;
	short *in;
	mgridlight_t **out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Com_Error( ERR_DROP, "Mod_LoadLightArray: funny lump size in %s", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->lightarray = out;
	loadbmodel->numlightarrayelems = count;

	for( i = 0; i < count; i++, in++, out++ )
		*out = loadbmodel->lightgrid + LittleShort( *in );
}

/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities( const lump_t *l, vec3_t gridSize, vec3_t ambient, vec3_t outline )
{
	int n;
	char *data;
	qboolean isworld;
	float gridsizef[3] = { 0, 0, 0 }, colorf[3] = { 0, 0, 0 }, ambientf = 0;
	char key[MAX_KEY], value[MAX_VALUE], *token;
#ifdef HARDWARE_OUTLINES
	float celcolorf[3] = { 0, 0, 0 };
#endif

	assert( gridSize );
	assert( ambient );
#ifdef HARDWARE_OUTLINES
	assert( outline );
#endif

	VectorClear( gridSize );
	VectorClear( ambient );
#ifdef HARDWARE_OUTLINES
	VectorClear( outline );
#endif

	data = (char *)mod_base + l->fileofs;
	if( !data || !data[0] )
		return;

	for(; ( token = COM_Parse( &data ) ) && token[0] == '{'; )
	{
		isworld = qfalse;

		while( 1 )
		{
			token = COM_Parse( &data );
			if( !token[0] )
				break; // error
			if( token[0] == '}' )
				break; // end of entity

			Q_strncpyz( key, token, sizeof( key ) );
			while( key[strlen( key )-1] == ' ' )  // remove trailing spaces
				key[strlen( key )-1] = 0;

			token = COM_Parse( &data );
			if( !token[0] )
				break; // error

			Q_strncpyz( value, token, sizeof( value ) );

			// now that we have the key pair worked out...
			if( !strcmp( key, "classname" ) )
			{
				if( !strcmp( value, "worldspawn" ) )
					isworld = qtrue;
			}
			else if( !strcmp( key, "gridsize" ) )
			{
				n = sscanf( value, "%f %f %f", &gridsizef[0], &gridsizef[1], &gridsizef[2] );
				if( n != 3 )
				{
					int gridsizei[3] = { 0, 0, 0 };
					sscanf( value, "%i %i %i", &gridsizei[0], &gridsizei[1], &gridsizei[2] );
					VectorCopy( gridsizei, gridsizef );
				}
			}
			else if( !strcmp( key, "_ambient" ) || ( !strcmp( key, "ambient" ) && ambientf == 0.0f ) )
			{
				sscanf( value, "%f", &ambientf );
				if( !ambientf )
				{
					int ia = 0;
					n = sscanf( value, "%i", &ia );
					ambientf = ia;
				}
			}
			else if( !strcmp( key, "_color" ) )
			{
				n = sscanf( value, "%f %f %f", &colorf[0], &colorf[1], &colorf[2] );
				if( n != 3 )
				{
					int colori[3] = { 0, 0, 0 };
					sscanf( value, "%i %i %i", &colori[0], &colori[1], &colori[2] );
					VectorCopy( colori, colorf );
				}
			}
#ifdef HARDWARE_OUTLINES
			else if( !strcmp( key, "_outlinecolor" ) )
			{
				n = sscanf( value, "%f %f %f", &celcolorf[0], &celcolorf[1], &celcolorf[2] );
				if( n != 3 )
				{
					int celcolori[3] = { 0, 0, 0 };
					sscanf( value, "%i %i %i", &celcolori[0], &celcolori[1], &celcolori[2] );
					VectorCopy( celcolori, celcolorf );
				}
			}
#endif
		}

		if( isworld )
		{
			VectorCopy( gridsizef, gridSize );

			if( VectorCompare( colorf, vec3_origin ) )
				VectorSet( colorf, 1.0, 1.0, 1.0 );
			VectorScale( colorf, ambientf, ambient );

#ifdef HARDWARE_OUTLINES
			if( max( celcolorf[0], max( celcolorf[1], celcolorf[2] ) ) > 1.0f )
				VectorScale( celcolorf, 1.0f/255.0f, celcolorf );	// [0..1] RGB -> [0..255] RGB
			VectorCopy( celcolorf, outline );
#endif
			break;
		}
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
	int j, k;
	float *lmArray;
	mesh_t *mesh;
	mlightmapRect_t *lmRects[MAX_LIGHTMAPS];
	int lightmaps[MAX_LIGHTMAPS];
	qbyte lightmapStyles[MAX_LIGHTMAPS], vertexStyles[MAX_LIGHTMAPS];

	for( j = 0; j < MAX_LIGHTMAPS; j++ )
	{
		lightmaps[j] = LittleLong( in->lm_texnum[j] );

		if( lightmaps[j] < 0 || out->facetype == FACETYPE_FLARE || lightmaps[j] >= loadmodel_numlightmaps )
		{
			lmRects[j] = NULL;
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
		}
		else
		{
			lmRects[j] = &loadmodel_lightmapRects[lightmaps[j]];
			lightmaps[j] = lmRects[j]->texNum;

			if( mapConfig.lightmapsPacking )
			{                       // scale/shift lightmap coords
				mesh = out->mesh;
				lmArray = mesh->lmstArray[j][0];
				for( k = 0; k < mesh->numVertexes; k++, lmArray += 2 )
				{
					lmArray[0] = (double)( lmArray[0] ) * lmRects[j]->texMatrix[0][0] + lmRects[j]->texMatrix[0][1];
					lmArray[1] = (double)( lmArray[1] ) * lmRects[j]->texMatrix[1][0] + lmRects[j]->texMatrix[1][1];
				}
			}
			lightmapStyles[j] = in->lightmapStyles[j];
		}
		vertexStyles[j] = in->vertexStyles[j];
	}
	out->superLightStyle = R_AddSuperLightStyle( lightmaps, lightmapStyles, vertexStyles, lmRects );
}

/*
=================
Mod_Finish
=================
*/
static void Mod_Finish( const lump_t *faces, const lump_t *light, vec3_t gridSize, vec3_t ambient, vec3_t outline )
{
	int i, j;
	msurface_t *surf;
	mfog_t *testFog;
	qboolean globalFog;

	// set up lightgrid
	if( gridSize[0] < 1 || gridSize[1] < 1 || gridSize[2] < 1 )
		VectorSet( loadbmodel->gridSize, 64, 64, 128 );
	else
		VectorCopy( gridSize, loadbmodel->gridSize );

	for( j = 0; j < 3; j++ )
	{
		vec3_t maxs;

		loadbmodel->gridMins[j] = loadbmodel->gridSize[j] *ceil( ( loadbmodel->submodels[0].mins[j] + 1 ) / loadbmodel->gridSize[j] );
		maxs[j] = loadbmodel->gridSize[j] *floor( ( loadbmodel->submodels[0].maxs[j] - 1 ) / loadbmodel->gridSize[j] );
		loadbmodel->gridBounds[j] = ( maxs[j] - loadbmodel->gridMins[j] )/loadbmodel->gridSize[j] + 1;
	}
	loadbmodel->gridBounds[3] = loadbmodel->gridBounds[1] * loadbmodel->gridBounds[0];

	// ambient lighting
	for( i = 0; i < 3; i++ )
		mapConfig.ambient[i] = bound( 0, ambient[i]*( (float)( 1 << mapConfig.pow2MapOvrbr )/255.0f ), 1 );

#ifdef HARDWARE_OUTLINES
	for( i = 0; i < 3; i++ )
		mapConfig.outlineColor[i] = (qbyte)(bound( 0, outline[i]*255.0f, 255 ));
	mapConfig.outlineColor[3] = 255;
#endif

	R_SortSuperLightStyles();

	for( i = 0, testFog = loadbmodel->fogs; i < loadbmodel->numfogs; testFog++, i++ )
	{
		if( !testFog->shader )
			continue;
		if( testFog->visibleplane )
			continue;

		testFog->visibleplane = Mod_Malloc( loadmodel, sizeof( cplane_t ) );
		VectorSet( testFog->visibleplane->normal, 0, 0, 1 );
		testFog->visibleplane->type = PLANE_Z;
		testFog->visibleplane->dist = loadbmodel->submodels[0].maxs[0] + 1;
	}

	// make sure that the only fog in the map has valid shader
	globalFog = ( loadbmodel->numfogs == 1 );
	if( globalFog )
	{
		testFog = &loadbmodel->fogs[0];
		if( !testFog->shader )
			globalFog = qfalse;
	}

	// apply super-lightstyles to map surfaces
	if( mod_bspFormat->flags & BSP_RAVEN )
	{
		rdface_t *in = ( void * )( mod_base + faces->fileofs );

		for( i = 0, surf = loadbmodel->surfaces; i < loadbmodel->numsurfaces; i++, in++, surf++ )
		{
			if( globalFog && surf->mesh && surf->fog != testFog )
			{
				if( !( surf->shader->flags & SHADER_SKY ) && !surf->shader->fog_dist )
					globalFog = qfalse;
			}

			if( !R_SurfPotentiallyVisible( surf ) )
				continue;
			Mod_ApplySuperStylesToFace( in, surf );
		}
	}
	else
	{
		rdface_t rdf;
		dface_t	*in = ( void * )( mod_base + faces->fileofs );

		rdf.lightmapStyles[0] = rdf.vertexStyles[0] = 0;
		for( j = 1; j < MAX_LIGHTMAPS; j++ )
		{
			rdf.lm_texnum[j] = -1;
			rdf.lightmapStyles[j] = rdf.vertexStyles[j] = 255;
		}

		for( i = 0, surf = loadbmodel->surfaces; i < loadbmodel->numsurfaces; i++, in++, surf++ )
		{
			if( globalFog && surf->mesh && surf->fog != testFog )
			{
				if( !( surf->shader->flags & SHADER_SKY ) && !surf->shader->fog_dist )
					globalFog = qfalse;
			}

			if( !R_SurfPotentiallyVisible( surf ) )
				continue;
			rdf.lm_texnum[0] = LittleLong( in->lm_texnum );
			Mod_ApplySuperStylesToFace( &rdf, surf );
		}
	}

	if( globalFog )
	{
		loadbmodel->globalfog = testFog;
		Com_DPrintf( "Global fog detected: %s\n", testFog->shader->name );
	}

	if( loadmodel_xyz_array )
		Mod_Free( loadmodel_xyz_array );
	if( loadmodel_surfelems )
		Mod_Free( loadmodel_surfelems );
	if( loadmodel_lightmapRects )
		Mod_Free( loadmodel_lightmapRects );
	if( loadmodel_shaderrefs )
		Mod_Free( loadmodel_shaderrefs );

	Mod_SetParent( loadbmodel->nodes, NULL );
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel( model_t *mod, model_t *parent, void *buffer )
{
	int i;
	int version;
	dheader_t *header;
	mmodel_t *bm;
	vec3_t gridSize, ambient, outline;

	mod->type = mod_brush;
	if( mod != mod_known )
		Com_Error( ERR_DROP, "Loaded a brush model after the world" );

	header = (dheader_t *)buffer;

	version = LittleLong( header->version );
	for( i = 0, mod_bspFormat = bspFormats; i < numBspFormats; i++, mod_bspFormat++ )
	{
		if( !strncmp( (char *)buffer, mod_bspFormat->header, 4 ) && ( version == mod_bspFormat->version ) )
			break;
	}
	if( i == numBspFormats )
		Com_Error( ERR_DROP, "Mod_LoadBrushModel: %s: unknown bsp format, version %i", mod->name, version );

	mod_base = (qbyte *)header;

	// swap all the lumps
	for( i = 0; i < sizeof( dheader_t )/4; i++ )
		( (int *)header )[i] = LittleLong( ( (int *)header )[i] );

	// load into heap
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );
	Mod_LoadEntities( &header->lumps[LUMP_ENTITIES], gridSize, ambient, outline );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadVertexes_RBSP( &header->lumps[LUMP_VERTEXES] );
	else
		Mod_LoadVertexes( &header->lumps[LUMP_VERTEXES] );
	Mod_LoadElems( &header->lumps[LUMP_ELEMENTS] );
	Mod_LoadLighting( &header->lumps[LUMP_LIGHTING], &header->lumps[LUMP_FACES] );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadLightgrid_RBSP( &header->lumps[LUMP_LIGHTGRID] );
	else
		Mod_LoadLightgrid( &header->lumps[LUMP_LIGHTGRID] );
	Mod_LoadShaderrefs( &header->lumps[LUMP_SHADERREFS] );
	Mod_LoadPlanes( &header->lumps[LUMP_PLANES] );
	Mod_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES] );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadFaces_RBSP( &header->lumps[LUMP_FACES] );
	else
		Mod_LoadFaces( &header->lumps[LUMP_FACES] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS], &header->lumps[LUMP_LEAFFACES] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadLightArray_RBSP( &header->lumps[LUMP_LIGHTARRAY] );
	else
		Mod_LoadLightArray();

	Mod_Finish( &header->lumps[LUMP_FACES], &header->lumps[LUMP_LIGHTING], gridSize, ambient, outline );

	// set up the submodels
	for( i = 0; i < loadbmodel->numsubmodels; i++ )
	{
		model_t	*starmod;
		mbrushmodel_t *bmodel;

		bm = &loadbmodel->submodels[i];
		starmod = &mod_inline[i];
		bmodel = ( mbrushmodel_t * )starmod->extradata;

		memcpy( starmod, mod, sizeof( model_t ) );
		memcpy( bmodel, mod->extradata, sizeof( mbrushmodel_t ) );

		bmodel->firstmodelsurface = bmodel->surfaces + bm->firstface;
		bmodel->nummodelsurfaces = bm->numfaces;
		starmod->extradata = bmodel;

		VectorCopy( bm->maxs, starmod->maxs );
		VectorCopy( bm->mins, starmod->mins );
		starmod->radius = bm->radius;

		if( i == 0 )
			*mod = *starmod;
		else
			bmodel->numsubmodels = 0;
	}
}

#ifdef QUAKE2_JUNK

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
	int i;
	dsprite_t *sprin;
	smodel_t *sprout;
	dsprframe_t *sprinframe;
	sframe_t *sproutframe;

	sprin = (dsprite_t *)buffer;

	if( LittleLong( sprin->version ) != SPRITE_VERSION )
		Com_Error( ERR_DROP, "%s has wrong version number (%i should be %i)",
		mod->name, LittleLong( sprin->version ), SPRITE_VERSION );

	mod->extradata = sprout = Mod_Malloc( mod, sizeof( smodel_t ) );
	sprout->numframes = LittleLong( sprin->numframes );

	sprinframe = sprin->frames;
	sprout->frames = sproutframe = Mod_Malloc( mod, sizeof( sframe_t ) * sprout->numframes );

	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	// byte swap everything
	for( i = 0; i < sprout->numframes; i++, sprinframe++, sproutframe++ )
	{
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

#endif

//=============================================================================

/*
=================
R_RegisterWorldModel

Specifies the model that will be used as the world
=================
*/
void R_RegisterWorldModel( const char *model, const dvis_t *visData )
{
	mapConfig.pow2MapOvrbr = 0;
	mapConfig.lightmapsPacking = qfalse;
	mapConfig.deluxeMaps = qfalse;
	mapConfig.deluxeMappingEnabled = qfalse;

	VectorClear( mapConfig.ambient );
#ifdef HARDWARE_OUTLINES
	VectorClear( mapConfig.outlineColor );
#endif

	if( r_lighting_packlightmaps->integer )
	{
		char lightmapsPath[MAX_QPATH], *p;

		mapConfig.lightmapsPacking = qtrue;

		Q_strncpyz( lightmapsPath, model, sizeof( lightmapsPath ) );
		p = strrchr( lightmapsPath, '.' );
		if( p )
		{
			*p = 0;
			Q_strncatz( lightmapsPath, "/lm_0000.tga", sizeof( lightmapsPath ) );
			if( FS_FOpenFile( lightmapsPath, NULL, FS_READ ) != -1 )
			{
				Com_DPrintf( S_COLOR_YELLOW "External lightmap stage: lightmaps packing is disabled\n" );
				mapConfig.lightmapsPacking = qfalse;
			}
		}
	}

	r_farclip_min = Z_NEAR; // sky shaders will most likely modify this value
	r_environment_color->modified = qtrue;

	r_worldmodel = Mod_ForName( model, qtrue );
	r_worldbrushmodel = ( mbrushmodel_t * )r_worldmodel->extradata;
	r_worldbrushmodel->vis = ( dvis_t * )visData;

	r_worldent->scale = 1.0f;
	r_worldent->model = r_worldmodel;
	r_worldent->rtype = RT_MODEL;
	Matrix_Identity( r_worldent->axis );

	r_framecount = 1;
	r_oldviewcluster = r_viewcluster = -1;  // force markleafs
}

/*
=================
R_RegisterModel
=================
*/
struct model_s *R_RegisterModel( const char *name )
{
	return Mod_ForName( name, qfalse );
}

/*
=================
R_ModelBounds
=================
*/
void R_ModelBounds( const model_t *model, vec3_t mins, vec3_t maxs )
{
	if( model )
	{
		VectorCopy( model->mins, mins );
		VectorCopy( model->maxs, maxs );
	}
	else if( r_worldmodel )
	{
		VectorCopy( r_worldmodel->mins, mins );
		VectorCopy( r_worldmodel->maxs, maxs );
	}
}
