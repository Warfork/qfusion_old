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
bmodel_t	*loadbmodel;
int			modfilelen;

void Mod_LoadAliasMD2Model (model_t *mod, void *buffer);
void Mod_LoadAliasMD3Model (model_t *mod, void *buffer);
void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadDarkPlacesModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);

model_t *Mod_LoadModel (model_t *mod, qboolean crash);

void Mod_RegisterAliasModel ( model_t *mod );
void Mod_RegisterDarkPlacesModel ( model_t *mod );

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

// the inline * models from the current map are kept separate
model_t	mod_inline[MAX_MOD_KNOWN];

int		registration_sequence;

static modelformatdescriptor_t mod_supportedformats[] =
{
 // Quake2 .md2 models
	{ IDMD2HEADER,	4,	0x800000,			MD3_ALIAS_MAX_LODS,	Mod_LoadAliasMD2Model },

// Quake III Arena .md3 models
	{ IDMD3HEADER,	4,	0x800000,			MD3_ALIAS_MAX_LODS,	Mod_LoadAliasMD3Model },

// Quake2 .sp2 sprites
	{ IDSP2HEADER,	4,	0x10000,			0,					Mod_LoadSpriteModel	},

// DarkPlaces models
	{ DPMHEADER,	16, DPM_MAX_FILESIZE,	DPM_MAX_LODS,		Mod_LoadDarkPlacesModel },

// Quake III Arena .bsp models
	{ IDBSPHEADER,	4,	0x4000000,			0,					Mod_LoadBrushModel },

// trailing NULL
	{ NULL,			0,	0,					0,					NULL }
};

static int mod_numsupportedformats = sizeof(mod_supportedformats) / sizeof(mod_supportedformats[0]) - 1;

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, bmodel_t *model)
{
	mnode_t		*node;
	cplane_t	*plane;
	
	if (!model || !model->nodes)
		Com_Error (ERR_DROP, "Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->plane == NULL)
			return (mleaf_t *)node;
		plane = node->plane;
		node = node->children[PlaneDiff (p, plane) <= 0];
	}

	return NULL;	// never reached
}

/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS (int cluster, bmodel_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;

	return ((byte *)model->vis->data + cluster*model->vis->rowsize);
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f (void)
{
	int		i;
	model_t	*mod;
	int		total;

	total = 0;
	Com_Printf ("Loaded models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		Com_Printf ("%8i : %s\n",mod->extradatasize, mod->name);
		total += mod->extradatasize;
	}
	Com_Printf ("Total resident: %i\n", total);
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}


/*
==================
Mod_FindSlot

==================
*/
model_t *Mod_FindSlot (char *name)
{
	int		i;
	model_t	*mod;

	//
	// search the currently loaded models
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (!strcmp (mod->name, name) )
			return mod;
	}

	//
	// find a free model slot spot
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			break;	// free spot
	}

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Com_Error (ERR_DROP, "mod_numknown == MAX_MOD_KNOWN");
		mod_numknown++;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	int		i;
	model_t	*mod, *lod;
	unsigned *buf;
	char shortname[MAX_QPATH], lodname[MAX_QPATH];
	modelformatdescriptor_t *descr;

	if (!name[0])
		Com_Error (ERR_DROP, "Mod_ForName: NULL name");
		
	//
	// inline models are grabbed only from worldmodel
	//
	if (name[0] == '*')
	{
		i = atoi(name+1);
		if (i < 1 || !r_worldbmodel || !r_worldbmodel || i >= r_worldbmodel->numsubmodels)
			Com_Error (ERR_DROP, "bad inline model number");
		return &mod_inline[i];
	}

	mod = Mod_FindSlot (name);
	if ( mod->name[0] && !strcmp (mod->name, name) ) {
		return mod;
	}

	strcpy (mod->name, name);

	//
	// load the file
	//
	modfilelen = FS_LoadFile (mod->name, (void **)&buf);
	if (!buf)
	{
		if (crash)
			Com_Error (ERR_DROP, "Mod_NumForName: %s not found", mod->name);
		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}
	
	loadmodel = mod;

	// call the apropriate loader
	descr = mod_supportedformats;
	for ( i = 0; i < mod_numsupportedformats; i++, descr++ ) {
		if ( !strncmp ((const char *)buf, descr->header, descr->headerLen) ) {
			break;
		}
	}

	if ( i == mod_numsupportedformats ) {
		Com_Error (ERR_DROP, "Mod_NumForName: unknown fileid for %s", mod->name);
	}

	mod->extradata = Hunk_Begin ( descr->maxSize );
	descr->loader ( mod, buf );

	FS_FreeFile (buf);

	if ( !descr->maxLods ) {
		loadmodel->extradatasize = Hunk_End ();
		return mod;
	}

	// 
	// load level-of-detail models
	//
	COM_StripExtension ( mod->name, shortname );

	for ( i = 0, mod->numlods = 0; i < mod_supportedformats[i].maxLods; i++ )
	{
		Com_sprintf ( lodname, MAX_QPATH, "%s_%i%s", shortname, i+1, &mod->name[strlen(shortname)] );
		
		modfilelen = FS_LoadFile (lodname, (void **)&buf);
		if ( !buf ) {
			break;
		}
		if ( strncmp ((const char *)buf, descr->header, descr->headerLen) ) {
			break;
		}
		
		mod->numlods++;
		FS_FreeFile (buf);
	}

	if ( mod->numlods < 2 ) {
		mod->numlods = 0;
		mod->extradatasize = Hunk_End ();
		return mod;
	}

	mod->lods = Hunk_AllocName ( sizeof(model_t *)*mod->numlods, mod->name );
	mod->extradatasize = Hunk_End ();

	for ( i = 0; i < mod->numlods; i++ )
	{
		Com_sprintf ( lodname, MAX_QPATH, "%s_%i%s", shortname, i+1, &mod->name[strlen(shortname)] );

		FS_LoadFile ( lodname, (void **)&buf );

		lod = mod->lods[i] = Mod_FindSlot ( lodname );
		if ( lod->name[0] && !strcmp(lod->name, lodname) ) {
			continue;
		}

		strcpy ( lod->name, lodname );

		loadmodel = lod;

		lod->extradata = Hunk_Begin ( descr->maxSize );
		descr->loader ( lod, buf );
		lod->extradatasize = Hunk_End ();

		FS_FreeFile (buf);
	}

	loadmodel = mod;

	return mod;
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;

void R_BuildLightmaps (bmodel_t *bmodel);

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	if (!l->filelen || r_vertexlight->value)
	{
		loadbmodel->lightdata = NULL;
		return;
	}

	loadbmodel->numlightmaps = l->filelen / ( 3 * 128 * 128);
	loadbmodel->lightdata = Hunk_AllocName ( l->filelen, loadmodel->name );	
	memcpy (loadbmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadbmodel->vis = NULL;
		return;
	}
	loadbmodel->vis = Hunk_AllocName (l->filelen, loadmodel->name);

	memcpy (loadbmodel->vis, mod_base + l->fileofs, l->filelen);

	loadbmodel->vis->numclusters = LittleLong ( loadbmodel->vis->numclusters );
	loadbmodel->vis->rowsize = LittleLong ( loadbmodel->vis->rowsize );
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	float		*out_xyz, *out_normals,	*out_st, *out_lmst;
	byte		*out_colors;
	vec3_t		color, fcolor;
	int			i, count, j;
	float		div;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);

	loadbmodel->xyz_array = Hunk_AllocName ( count*sizeof(vec4_t), loadmodel->name );
	loadbmodel->normals_array = Hunk_AllocName ( count*sizeof(vec3_t), loadmodel->name );	
	loadbmodel->st_array = Hunk_AllocName ( count*sizeof(vec2_t), loadmodel->name );	
	loadbmodel->lmst_array = Hunk_AllocName ( count*sizeof(vec2_t), loadmodel->name );	
	loadbmodel->colors_array = Hunk_AllocName ( count*sizeof(vec4_t), loadmodel->name );	
	loadbmodel->numvertexes = count;

	out_xyz = loadbmodel->xyz_array[0];
	out_normals = loadbmodel->normals_array[0];
	out_st = loadbmodel->st_array[0];
	out_lmst = loadbmodel->lmst_array[0];
	out_colors = loadbmodel->colors_array[0];

	div = gl_state.pow2_mapovrbr;

	for ( i=0 ; i<count ; i++, in++, out_xyz+=4, out_normals+=3, out_st+=2, out_lmst+=2, out_colors+=4)
	{
		for ( j=0 ; j < 3 ; j++)
		{
			out_xyz[j] = LittleFloat ( in->point[j] );
			out_normals[j] = LittleFloat ( in->normal[j] );
		}

		for ( j=0 ; j < 2 ; j++)
		{
			out_st[j] = LittleFloat ( in->tex_st[j] );
			out_lmst[j] = LittleFloat ( in->lm_st[j] );
		}

		if ( r_fullbright->value ) {
			for ( j=0 ; j < 3 ; j++) {
				out_colors[j] = 255;
			}
		} else {
			for ( j=0 ; j < 3 ; j++) {
				color[j] = (float)((double)in->color[j] * div);
			}

			ColorNormalize ( color, fcolor );

			out_colors[0] = FloatToByte ( fcolor[0] );
			out_colors[1] = FloatToByte ( fcolor[1] );
			out_colors[2] = FloatToByte ( fcolor[2] );
		}

		out_colors[3] = in->color[3];
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	mmodel_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->submodels = out;
	loadbmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
		}

		out->radius = RadiusFromBounds (out->mins, out->maxs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadShaderrefs
=================
*/
void Mod_LoadShaderrefs (lump_t *l)
{
	dshaderref_t	*in;
	mshaderref_t	*out;
	int 			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->shaderrefs = out;
	loadbmodel->numshaderrefs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		Com_sprintf ( out->name, sizeof( out->name ), in->name );

		out->flags = LittleLong ( in->flags );
		out->contents = LittleLong ( in->contents );
		out->shader = NULL;
	}
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	mesh_t		*mesh;
	mfog_t		*fog;
	mshaderref_t *shaderref;
	int			i, j, count;
	int			shadernum, fognum, surfnum;
	float		*vert;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->surfaces = out;
	loadbmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		for (i=0 ; i<3 ; i++)
		{
			out->origin[i] = LittleFloat ( in->origin[i] );
		}

		out->facetype = LittleLong ( in->facetype );

	// lighting info
		if ( !r_vertexlight->value ) {
			out->lightmaptexturenum = LittleLong ( in->lm_texnum );

			if ( out->lightmaptexturenum >= loadbmodel->numlightmaps )
				out->lightmaptexturenum = -1;
		} else {
			out->lightmaptexturenum = -1;
		}

		if ( out->facetype == FACETYPE_PATCH ) {
			mesh = GL_CreateMeshForPatch ( loadmodel, in );
			if ( mesh ) {
				out->mesh = mesh;
			}
		} else if ( out->facetype != FACETYPE_FLARE ) {
			mesh = out->mesh = (mesh_t *)Hunk_AllocName ( sizeof(mesh_t), loadmodel->name );
			mesh->xyz_array = loadbmodel->xyz_array + LittleLong ( in->firstvert );
			mesh->normals_array = loadbmodel->normals_array + LittleLong ( in->firstvert );
			mesh->st_array = loadbmodel->st_array + LittleLong ( in->firstvert );
			mesh->lmst_array = loadbmodel->lmst_array + LittleLong ( in->firstvert );
			mesh->colors_array = loadbmodel->colors_array + LittleLong ( in->firstvert );
			mesh->numvertexes = LittleLong ( in->numverts );

			mesh->indexes = loadbmodel->surfindexes + LittleLong ( in->firstindex );
			mesh->numindexes = LittleLong ( in->numindexes );
		} else {
			int r, g, b;

			mesh = out->mesh = (mesh_t *)Hunk_AllocName ( sizeof(mesh_t), loadmodel->name );
			mesh->xyz_array = (vec4_t *)Hunk_AllocName ( sizeof(vec4_t), loadmodel->name );
			mesh->numvertexes = 1;
			mesh->indexes = r_quad_indexes;
			mesh->numindexes = 6;
			VectorCopy ( out->origin, mesh->xyz_array[0] );

			r = LittleFloat ( in->mins[0] ) * 255.0f;
			r = bound ( 0, r, 255 );

			g = LittleFloat ( in->mins[1] ) * 255.0f;
			g = bound ( 0, g, 255 );

			b = LittleFloat ( in->mins[2] ) * 255.0f;
			b = bound ( 0, b, 255 );

			out->dlightbits = (unsigned int)COLOR_RGB ( r, g, b );
		}

		shadernum = LittleLong ( in->shadernum );
		if ( shadernum < 0 || shadernum >= loadbmodel->numshaderrefs )
			Com_Error ( ERR_DROP, "MOD_LoadBmodel: bad shader number" );

		shaderref = loadbmodel->shaderrefs + shadernum;
		if ( !shaderref->shader ) {
			if ( out->facetype == FACETYPE_FLARE ) {
				shadernum = R_LoadShader ( shaderref->name, SHADER_BSP_FLARE );
			} else if ( out->facetype == FACETYPE_TRISURF || r_vertexlight->value || out->lightmaptexturenum < 0 ) {
				shadernum = R_LoadShader ( shaderref->name, SHADER_BSP_VERTEX );
			} else {
				shadernum = R_LoadShader ( shaderref->name, SHADER_BSP );
			}

			shaderref->shader = &r_shaders[shadernum];
		}

		out->shaderref = shaderref;
		out->fog = NULL;

		fognum = LittleLong ( in->fognum );
		if ( fognum != -1 ) {
			fog = loadbmodel->fogs + fognum;

			if ( fog->numplanes && fog->shader ) {
				out->fog = fog;
			}
		}

		if ( !mesh || out->facetype == FACETYPE_FLARE ) {
			continue;
		}

		ClearBounds ( mesh->mins, mesh->maxs );

		vert = mesh->xyz_array[0];
		for ( j = 0; j < out->mesh->numvertexes; j++, vert += 4 )
			AddPointToBounds ( vert, mesh->mins, mesh->maxs );

		mesh->radius = RadiusFromBounds ( mesh->mins, mesh->maxs );

		if ( out->facetype == FACETYPE_PLANAR ) {
			for ( j = 0; j < 3; j++ ) {
				out->origin[j] = LittleFloat ( in->normal[j] );
			}
		}
	}
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->nodes = out;
	loadbmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->plane = loadbmodel->planes + LittleLong ( in->planenum );

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong ( in->children[j] );
			if (p >= 0)
				out->children[j] = loadbmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadbmodel->leafs + (-1 - p));
		}
	}
}

/*
=================
Mod_LoadBrushes
=================
*/
void Mod_LoadBrushsides (lump_t *l)
{
	dbrushside_t 	*in;
	mbrushside_t 	*out;
	int				i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->brushsides = out;
	loadbmodel->numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		out->plane = loadbmodel->planes + LittleLong ( in->planenum );
}

/*
=================
Mod_LoadBrushes
=================
*/
void Mod_LoadBrushes (lump_t *l)
{
	dbrush_t 	*in;
	mbrush_t 	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->brushes = out;
	loadbmodel->numbrushes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->numsides = LittleLong ( in->numsides );
		out->firstside = loadbmodel->brushsides + LittleLong ( in->firstside );
	}
}

/*
=================
Mod_LoadFogs
=================
*/
void Mod_LoadFogs (lump_t *l)
{
	dfog_t 	*in;
	mfog_t 	*out;
	mbrush_t *brush;
	mbrushside_t *visibleside, *brushsides;
	int		i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );

	loadbmodel->fogs = out;
	loadbmodel->numfogs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		if ( LittleLong ( in->visibleside ) == -1 ) {
			continue;
		}

		brush = loadbmodel->brushes + LittleLong ( in->brushnum );
		brushsides = brush->firstside;
		visibleside = brushsides + LittleLong ( in->visibleside );

		out->visibleplane = visibleside->plane;
		out->shader = R_RegisterShader ( in->shader );
		out->numplanes = brush->numsides;
		out->planes = Hunk_AllocName ( out->numplanes*sizeof(cplane_t *), loadmodel->name );

		for ( j = 0; j < out->numplanes; j++ )
		{
			out->planes[j] = brushsides[j].plane;
		}
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->leafs = out;
	loadbmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for ( j=0 ; j<3 ; j++)
		{
			out->mins[j] = (float)LittleLong ( in->mins[j] );
			out->maxs[j] = (float)LittleLong ( in->maxs[j] );
		}
		
		out->cluster = LittleLong ( in->cluster );

		if ( loadbmodel->vis ) {
			if ( out->cluster >= loadbmodel->vis->numclusters )
				Com_Error (ERR_DROP, "MOD_LoadBmodel: leaf cluster > numclusters");
		}

		out->plane = NULL;
		out->area = LittleLong ( in->area ) + 1;

		// check if degenerate
		if ( out->mins[0] > out->maxs[0] || out->mins[1] > out->maxs[1] ||
			out->mins[2] > out->maxs[2] || VectorCompare (out->mins, out->maxs) ) {
			out->nummarksurfaces = 0;
		} else {
			out->nummarksurfaces = LittleLong ( in->numleaffaces );
		}

		j = LittleLong ( in->firstleafface );
		if ( j < 0 || out->nummarksurfaces + j > loadbmodel->nummarksurfaces ) {
			out->nummarksurfaces = loadbmodel->nummarksurfaces - j;
		}

		out->firstmarksurface = loadbmodel->marksurfaces + j;
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	int		*in;
	msurface_t	**out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->marksurfaces = out;
	loadbmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleLong ( in[i] );
		if (j < 0 ||  j >= loadbmodel->numsurfaces)
			Com_Error (ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadbmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadIndexes
=================
*/
void Mod_LoadIndexes (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_INDICES)
		Com_Error (ERR_DROP, "MOD_LoadBmodel: bad surfedges count in %s: %i",
		loadmodel->name, count);

	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->surfindexes = out;
	loadbmodel->numsurfindexes = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	
	
	loadbmodel->planes = out;
	loadbmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
		}
		out->dist = LittleFloat (in->dist);

		CategorizePlane ( out );
	}
}

/*
=================
Mod_LoadLightgrid
=================
*/
void Mod_LoadLightgrid (lump_t *l)
{
	dgridlight_t 	*in;
	mgridlight_t 	*out;
	int	count;
			
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->lightgrid = out;
	loadbmodel->numlightgridelems = count;

	// lightgrid is all 8 bit
	memcpy ( out, in, count*sizeof(*out) );
}

/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l)
{
	char *data;
	mlight_t *out;
	int count, gridsizei[3];
	qboolean islight, isworld;
	float scale, gridsizef[3];
	char key[MAX_KEY], value[MAX_VALUE], target[MAX_VALUE], *token;

	data = (char *)mod_base + l->fileofs;
	if ( !data || !data[0] ) {
		return;
	}

	for ( count = 0; (token = COM_Parse (&data)) && token[0] == '{'; )
	{
		islight = false;
		isworld = false;

		while (1)
		{
			if ( !(token = COM_Parse (&data)) ) {
				break; // error
			}
			if ( token[0] == '}' ) {
				break; // end of entity
			}

			Q_strncpyz ( key, token, sizeof(key) );
			while ( key[strlen(key)-1] == ' ' ) { // remove trailing spaces
				key[strlen(key)-1] = 0;
			}

			if ( !(token = COM_Parse (&data)) ) {
				break; // error
			}

			Q_strncpyz ( value, token, sizeof(value) );

			// now that we have the key pair worked out...
			if ( !strcmp (key, "classname") ) {
				if ( !strncmp (value, "light", 5) ) {
					islight = true;
				} else if ( !strcmp (value, "worldspawn") ) {
					isworld = true;
				}
			} else if ( !strcmp (key, "gridsize") ) {
				sscanf ( value, "%f %f %f", &gridsizef[0], &gridsizef[1], &gridsizef[2] );

				if ( !gridsizef[0] || !gridsizef[1] || !gridsizef[2] ) {
					sscanf ( value, "%i %i %i", &gridsizei[0], &gridsizei[1], &gridsizei[2] );
					VectorCopy ( gridsizei, gridsizef );
				}
			}
		}

		if ( isworld ) {
			VectorCopy ( gridsizef, loadmodel->gridSize );
			continue;
		}

		if ( islight ) {
			count++;
		}
	}

	if ( !count ) {
		return;
	}

	out = Hunk_AllocName ( count*sizeof(*out), loadmodel->name );	

	loadbmodel->numworldlights = count;
	loadbmodel->worldlights = out;

	data = mod_base + l->fileofs;
	for ( ; (token = COM_Parse (&data)) && token[0] == '{';  )
	{
		islight = false;

		while (1)
		{
			if ( !(token = COM_Parse (&data)) ) {
				break; // error
			}
			if ( token[0] == '}' ) {
				break; // end of entity
			}

			Q_strncpyz ( key, token, sizeof(key) );
			while ( key[strlen(key)-1] == ' ' ) { // remove trailing spaces
				key[strlen(key)-1] = 0;
			}

			if ( !(token = COM_Parse (&data)) ) {
				break; // error
			}

			Q_strncpyz ( value, token, sizeof(value) );

			// now that we have the key pair worked out...
			if ( !strcmp (key, "origin") ) {
				sscanf ( value, "%f %f %f", &out->origin[0], &out->origin[1], &out->origin[2] );
			} else if ( !strcmp (key, "color") || !strcmp (key, "_color") ) {
				sscanf ( value, "%f %f %f", &out->color[0], &out->color[1], &out->color[2] );
			} else if ( !strcmp (key, "light") || !strcmp (key, "_light") ) {
				out->intensity = atof ( value );
			} else if (!strcmp (key, "classname") ) {
				if ( !strncmp (value, "light", 5) ) {
					islight = true;
				}
			} else if ( !strcmp (key, "target") ) {
				Q_strncpyz ( target, value, sizeof(target) );
			}
		}

		if ( !islight ) {
			continue;
		}

		if ( out->intensity <= 0 ) {
			out->intensity = 300;
		}
		out->intensity += 15;

		scale = max ( max (out->color[0], out->color[1]), out->color[2] );
		if ( !scale ) {
			VectorSet ( out->color, 1, 1, 1 );
		} else {
			// normalize
			scale = 1.0f / scale;
			VectorScale ( out->color, scale, out->color );
		}

		out++;
	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i;
	dheader_t	*header;
	mmodel_t 	*bm;
	vec3_t		maxs;

	mod->type = mod_brush;
	if (mod != mod_known)
		Com_Error (ERR_DROP, "Loaded a brush model after the world");

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		Com_Error (ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

	mod_base = (byte *)header;
	mod->bmodel = loadbmodel = Hunk_AllocName ( sizeof(bmodel_t), mod->name );

// swap all the lumps
	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap
	
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadIndexes (&header->lumps[LUMP_INDEXES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadLightgrid (&header->lumps[LUMP_LIGHTGRID]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadShaderrefs (&header->lumps[LUMP_SHADERREFS]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadBrushsides (&header->lumps[LUMP_BRUSHSIDES]);
	Mod_LoadBrushes (&header->lumps[LUMP_BRUSHES]);
	Mod_LoadFogs (&header->lumps[LUMP_FOGS]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	R_BuildLightmaps ( mod->bmodel );

//
// set up the submodels
//
	for (i=0 ; i<mod->bmodel->numsubmodels ; i++)
	{
		model_t		*starmod;

		bm = &mod->bmodel->submodels[i];
		starmod = &mod_inline[i];

		*starmod = *loadmodel;

		starmod->firstmodelsurface = starmod->bmodel->surfaces + bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, starmod->maxs);
		VectorCopy (bm->mins, starmod->mins);
		starmod->radius = bm->radius;

		if (i == 0)
			*loadmodel = *starmod;
	}

//
// set up lightgrid
//
	if ( mod->gridSize[0] < 1 || mod->gridSize[1] < 1 || mod->gridSize[2] < 1 ) {
		VectorSet ( mod->gridSize, 64, 64, 128 );
	}

	for ( i = 0; i < 3; i++ )
	{
		mod->gridMins[i] = mod->gridSize[i] * ceil( (mod->mins[i] + 1) / mod->gridSize[i] );
		maxs[i] = mod->gridSize[i] * floor( (mod->maxs[i] - 1) / mod->gridSize[i] );
		mod->gridBounds[i] = (maxs[i] - mod->gridMins[i])/mod->gridSize[i] + 1;
	}

	mod->gridBounds[3] = mod->gridBounds[1] * mod->gridBounds[0];
}

/*
=================
Mod_RegisterBrushModel
=================
*/
void Mod_RegisterBrushModel (model_t *mod)
{
	int				i;
	bmodel_t		*bmodel;
	mshaderref_t	*shaderref;

	if ( !(bmodel = mod->bmodel) ) {
		return;
	}

	shaderref = bmodel->shaderrefs;
	for ( i = 0; i < bmodel->numshaderrefs; i++, shaderref++ ) {
		if ( shaderref->shader )
			shaderref->shader->registration_sequence = registration_sequence;
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
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int			i;
	dsprite_t	*sprin;
	smodel_t	*sprout;
	dsprframe_t *sprinframe;
	sframe_t	*sproutframe;

	sprin = (dsprite_t *)buffer;

	if ( LittleLong (sprin->version) != SPRITE_VERSION ) {
		Com_Error ( ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, LittleLong (sprin->version), SPRITE_VERSION );
	}

	mod->smodel = sprout = Hunk_AllocName ( sizeof(smodel_t), mod->name);
	sprout->numframes = LittleLong ( sprin->numframes );

	if ( sprout->numframes > SPRITE_MAX_FRAMES ) {
		Com_Error ( ERR_DROP, "%s has too many frames (%i > %i)",
				 mod->name, sprout->numframes, SPRITE_MAX_FRAMES );
	}

	sprinframe = sprin->frames;
	sprout->frames = sproutframe = Hunk_AllocName ( sizeof(sframe_t) * sprout->numframes, mod->name );

	mod->radius = 0;
	ClearBounds ( mod->mins, mod->maxs );

	// byte swap everything
	for (i=0 ; i<sprout->numframes ; i++, sprinframe++, sproutframe++)
	{
		sproutframe->width = LittleLong ( sprinframe->width );
		sproutframe->height = LittleLong ( sprinframe->height );
		sproutframe->origin_x = LittleLong ( sprinframe->origin_x );
		sproutframe->origin_y = LittleLong ( sprinframe->origin_y );
		
		Q_strncpyz ( sproutframe->name, sprinframe->name, SPRITE_MAX_NAME );

		sproutframe->radius = 
			max (sproutframe->origin_x * sproutframe->origin_x, (sproutframe->origin_x + sproutframe->width) * (sproutframe->origin_x + sproutframe->width)) +
			max (sproutframe->origin_y * sproutframe->origin_y, (sproutframe->origin_y - sproutframe->height) * (sproutframe->origin_y - sproutframe->height));
		mod->radius = max ( mod->radius, sproutframe->radius );
	}

	mod->type = mod_sprite;
}

/*
=================
Mod_RegisterSpriteodel
=================
*/
void Mod_RegisterSpriteModel (model_t *mod)
{
	int			i;
	smodel_t	*smodel;

	if ( !(smodel = mod->smodel) ) {
		return;
	}

	for (i = 0; i < smodel->numframes; i++ ) {
		smodel->frames[i].shader = R_RegisterPic ( smodel->frames[i].name );
	}
}

//=============================================================================

/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginRegistration

Specifies the model that will be used as the world
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginRegistration (char *model)
{
	char	fullname[MAX_QPATH];
	cvar_t	*flushmap;
	extern void CL_LoadingString (char *str);

	registration_sequence++;

	Com_sprintf (fullname, sizeof(fullname), "maps/%s.bsp", model);
	CL_LoadingString (fullname);

	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	flushmap = Cvar_Get ("flushmap", "0", 0);
	if ( strcmp(mod_known[0].name, fullname) || flushmap->value)
		Mod_Free (&mod_known[0]);

	r_worldmodel = Mod_ForName(fullname, true);
	r_worldbmodel = r_worldmodel->bmodel;
	r_oldviewcluster = r_viewcluster = -1;		// force markleafs
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
struct model_s *R_RegisterModel (char *name)
{
	int			i;
	model_t		*mod;
	void		(*registerer) ( model_t *mod );

	mod = Mod_ForName (name, false);
	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		// register any images used by the models
		if ( mod->type == mod_sprite ) {
			registerer = Mod_RegisterSpriteModel;
		} else if ( mod->type == mod_alias ) {
			registerer = Mod_RegisterAliasModel;
		} else if ( mod->type == mod_dpm ) {
			registerer = Mod_RegisterDarkPlacesModel;
		} else if ( mod->type == mod_brush ) {
			registerer = Mod_RegisterBrushModel;
		}

		registerer ( mod );
		for ( i = 0; i < mod->numlods; i++ ) {
			mod->lods[i]->registration_sequence = registration_sequence;
			registerer ( mod->lods[i] );
		}
	}

	return mod;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_EndRegistration

@@@@@@@@@@@@@@@@@@@@@
*/
void R_EndRegistration (void)
{
	int		i;
	model_t	*mod;

	for (i=0, mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (mod->registration_sequence != registration_sequence)
		{	// don't need this model
			Mod_Free (mod);
		}
	}

	Shader_UpdateRegistration ();
	GL_FreeUnusedImages ();
}

//=============================================================================


/*
================
Mod_Free
================
*/
void Mod_Free (model_t *mod)
{
	Hunk_Free (mod->extradata);
	memset (mod, 0, sizeof(*mod));
}

/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll (void)
{
	int		i;

	for (i=0 ; i<mod_numknown ; i++)
	{
		if (mod_known[i].extradatasize)
			Mod_Free (&mod_known[i]);
	}
}
