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
// models.c -- model loading and caching

#include "gl_local.h"

model_t	*loadmodel;
int		modfilelen;

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
void Mod_LoadMd3Model ( struct model_s *mod, void *buffer );
model_t *Mod_LoadModel (model_t *mod, qboolean crash);

void R_RegisterMd3 ( struct model_s *mod );

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

// the inline * models from the current map are kept seperate
model_t	mod_inline[MAX_MOD_KNOWN];

int		registration_sequence;

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	cplane_t	*plane;
	
	if (!model || !model->nodes)
		Com_Error (ERR_DROP, "Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents != -1)
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
byte *Mod_ClusterPVS (int cluster, model_t *model)
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
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	unsigned *buf;
	int		i;
	
	if (!name[0])
		Com_Error (ERR_DROP, "Mod_ForName: NULL name");
		
	//
	// inline models are grabbed only from worldmodel
	//
	if (name[0] == '*')
	{
		i = atoi(name+1);
		if (i < 1 || !r_worldmodel || i >= r_worldmodel->numsubmodels)
			Com_Error (ERR_DROP, "bad inline model number");
		return &mod_inline[i];
	}

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
	strcpy (mod->name, name);
	
	//
	// load the file
	//
	modfilelen = FS_LoadFile (mod->name, &buf);
	if (!buf)
	{
		if (crash)
			Com_Error (ERR_DROP, "Mod_NumForName: %s not found", mod->name);
		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}
	
	loadmodel = mod;

	// call the apropriate loader
	
	switch (LittleLong(*(unsigned *)buf))
	{
	case IDALIASHEADER:
		loadmodel->extradata = Hunk_Begin (0x200000);
		Mod_LoadAliasModel (mod, buf);
		break;
		
	case IDSPRITEHEADER:
		loadmodel->extradata = Hunk_Begin (0x10000);
		Mod_LoadSpriteModel (mod, buf);
		break;

	case IDBSPHEADER:
		loadmodel->extradata = Hunk_Begin (0x4000000);
		Mod_LoadBrushModel (mod, buf);
		break;

	case MD3_ID_HEADER:
		loadmodel->extradata = Hunk_Begin (0x800000);
		Mod_LoadMd3Model (mod, buf);
		break;

	default:
		Com_Error (ERR_DROP,"Mod_NumForName: unknown fileid for %s", mod->name);
		break;
	}

	loadmodel->extradatasize = Hunk_End ();

	FS_FreeFile (buf);

	return mod;
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;

void R_BuildLightmaps (model_t *model);

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	if (!l->filelen || r_vertexlight->value)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->numlightmaps = l->filelen / ( 3 * 128 * 128);
	loadmodel->lightdata = Hunk_Alloc ( l->filelen);	
	memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);

	R_BuildLightmaps (loadmodel);
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
		loadmodel->vis = NULL;
		return;
	}
	loadmodel->vis = Hunk_Alloc (l->filelen);

	memcpy (loadmodel->vis, mod_base + l->fileofs, l->filelen);

	loadmodel->vis->numclusters = LittleLong ( loadmodel->vis->numclusters );
	loadmodel->vis->rowsize = LittleLong ( loadmodel->vis->rowsize );
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count, j;
	float		div;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	div = floor( r_mapoverbrightbits->value - r_overbrightbits->value );
	if ( div > 0 ) {
		div = pow( 2, div ) / 255.0;
	} else {
		div = 1.0 / 255.0;
	}

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for ( j=0 ; j < 3 ; j++)
		{
			out->position[j] = LittleFloat ( in->point[j] );
			out->normal[j] = LittleFloat ( in->normal[j] );
		}

		for ( j=0 ; j < 2 ; j++)
		{
			out->lm_st[j] = LittleFloat ( in->lm_st[j] );
			out->tex_st[j] = LittleFloat ( in->tex_st[j] );
		}

		for ( j=0 ; j < 3 ; j++) {
			out->colour[j] = (float)((double)in->colour[j] * div);
			out->colour[j] = min( out->colour[j], 1 );
		}
		out->colour[3] = (float)((double)in->colour[3] / 255.0);
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength (corner);
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
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

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
		out->firstbrush = LittleLong (in->firstbrush);
		out->numbrushes = LittleLong (in->numbrushes);
	}
}

void GL_CreateSurfaceLightmap (msurface_t *surf);
void GL_MeshCreate (msurface_t *surf, int numverts, mvertex_t *verts, int *mesh_cp);
void GL_PretransformAutosprites (msurface_t *surf);

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l, lump_t *sl)
{
	dface_t		*in;
	shaderref_t *in_shaderrefs, *shaderref;
	msurface_t 	*out;
	mfog_t		*fog;
	int			i, count, surfnum, bits;
	int			ti, mesh_cp[2], numshaderrefs;
	byte		type;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	in_shaderrefs = (void *)(mod_base + sl->fileofs);
	numshaderrefs = sl->filelen / sizeof(*in_shaderrefs);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	currentmodel = loadmodel;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		for (i=0 ; i<3 ; i++)
		{
			out->origin[i] = LittleFloat ( in->origin[i] );
			out->mins[i] = LittleFloat ( in->mins[i] );
			out->maxs[i] = LittleFloat ( in->maxs[i] );
		}

		for (i=0 ; i<2 ; i++)
		{
			mesh_cp[i] = LittleLong ( in->mesh_cp[i] );
		}

		out->facetype = LittleLong ( in->facetype );

	// lighting info
		if ( !r_vertexlight->value ) {
			out->mesh.lightmaptexturenum = LittleLong ( in->lm_texnum );

			if ( out->mesh.lightmaptexturenum >= loadmodel->numlightmaps )
				out->mesh.lightmaptexturenum = -1;
		} else {
			out->mesh.lightmaptexturenum = -1;
		}

		if ( mesh_cp[0] && mesh_cp[1] && out->facetype == FACETYPE_MESH ) {
			GL_MeshCreate ( out, LittleLong ( in->numverts ), loadmodel->vertexes + LittleLong ( in->firstvert ), mesh_cp );
		} else if ( out->facetype != FACETYPE_FLARE ) {
			out->mesh.numverts = LittleLong ( in->numverts );
			out->mesh.firstvert = loadmodel->vertexes + LittleLong ( in->firstvert );

			out->mesh.numindexes = LittleLong ( in->numindexes );
			out->mesh.firstindex = loadmodel->surfindexes + LittleLong ( in->firstindex );
		}

		if ( out->facetype == FACETYPE_MESH || out->facetype == FACETYPE_TRISURF ) {
			int j;
			mvertex_t *vert;

			ClearBounds ( out->mins, out->maxs );

			vert = out->mesh.firstvert;
			for ( j = 0; j < out->mesh.numverts; j++, vert++ )
				AddPointToBounds ( vert->position, out->mins, out->maxs );
		}

		if ( out->facetype == FACETYPE_PLANAR) {
			out->plane = (cplane_t *)Hunk_Alloc ( sizeof(cplane_t) );

			bits = 0;
			type = PLANE_ANYZ;

			for (i=0 ; i<3 ; i++) {
				out->plane->normal[i] = LittleFloat ( in->normal[i] );
				if (out->plane->normal[i] < 0)
					bits |= 1<<i;
				if (out->plane->normal[i] == 1.0f)
					type = i;
			}

			out->plane->type = type;
			out->plane->signbits = bits;
			out->plane->dist = DotProduct ( out->mesh.firstvert->position, out->plane->normal );
		}

		ti = LittleLong ( in->shadernum );
		if ( ti < 0 || ti >= numshaderrefs )
			Com_Error ( ERR_DROP, "MOD_LoadBmodel: bad shader number" );

		shaderref = in_shaderrefs + ti;
		out->flags = LittleLong ( shaderref->flags );
		ti = R_LoadShader (shaderref->shader, SHADER_BSP, out);

		if ( ti != -1 ) {
			out->mesh.shader = &r_shaders[ti];
		} else {
			out->mesh.shader = NULL;
		}

		ti = LittleLong ( in->fognum );
		if ( ti != -1 ) {
			fog = loadmodel->fogs + ti;

			if ( fog->brush && fog->shader )
				out->fog = fog;
		} else {
			out->fog = NULL;
		}

		GL_PretransformAutosprites ( out );
	}
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
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
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = (float)LittleLong ( in->mins[j] );
			out->maxs[j] = (float)LittleLong ( in->maxs[j] );
		}

		out->plane = loadmodel->planes + LittleLong ( in->planenum );
		out->contents = -1;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong ( in->children[j] );
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent ( loadmodel->nodes, NULL );	// sets nodes and leafs
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
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->brushsides = out;
	loadmodel->numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		out->plane = loadmodel->planes + LittleLong ( in->planenum );
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
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->brushes = out;
	loadmodel->numbrushes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->numsides = LittleLong ( in->numsides );
		out->firstside = loadmodel->brushsides + LittleLong ( in->firstside );
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
	mbrushside_t *brushside;
	int		i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->fogs = out;
	loadmodel->numfogs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		brush = loadmodel->brushes + LittleLong ( in->brushnum );
		brushside = brush->firstside + LittleLong ( in->visibleside );
		out->brush = brush;
		out->plane = brushside->plane;
		out->shader = R_RegisterShader ( in->shader );
	}
}

/*
=================
Mod_LoadLeafBrushes
=================
*/
void Mod_LoadLeafBrushes (lump_t *l)
{
	int 	*in;
	int 	*out;
	int		i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->leafbrushes = out;
	loadmodel->numleafbrushes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		*out = LittleLong ( *in );
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
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for ( j=0 ; j<3 ; j++)
		{
			out->mins[j] = (float)LittleLong ( in->mins[j] );
			out->maxs[j] = (float)LittleLong ( in->maxs[j] );
		}

		out->cluster = LittleLong ( in->cluster );

		if ( loadmodel->vis ) {
			if ( out->cluster > loadmodel->vis->numclusters )
				Com_Error (ERR_DROP, "MOD_LoadBmodel: leaf cluster > numclusters");
		}

		out->area = LittleLong ( in->area ) + 1;
		out->nummarksurfaces = LittleLong ( in->numleaffaces );
		out->numleafbrushes = LittleLong ( in->numleafbrushes );

		out->firstleafbrush = loadmodel->leafbrushes + LittleLong ( in->firstleafbrush );
		out->firstmarksurface = loadmodel->marksurfaces + LittleLong ( in->firstleafface );

		out->contents = 0;
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
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleLong ( in[i] );
		if (j < 0 ||  j >= loadmodel->numsurfaces)
			Com_Error (ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
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

	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->surfindexes = out;
	loadmodel->numsurfindexes = count;

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
	int			bits;
	byte		type;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*2*sizeof(*out));	
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		type = PLANE_ANYZ;

		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
			if (out->normal[j] == 1.0f)
				type = j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = type;
		out->signbits = bits;
	}
}

/*
=================
Mod_LoadLightgrid
=================
*/
void Mod_LoadLightgrid (lump_t *l)
{
	dlightgrid_t 	*in;
	mlightgrid_t 	*out;
	int	i, count, j;
	double div;
			
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->lightgrid = out;
	loadmodel->numlightgridelems = count;

	div = floor ( r_mapoverbrightbits->value - r_overbrightbits->value );
	if ( div > 0 ) {
		div = pow( 2, div ) / 255.0;
	} else {
		div = 1.0 / 255.0;
	}

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for ( j = 0; j < 3; j++ ) {
			out->ambient[j] = (float)((double)in->ambient[j] * div);
			out->diffuse[j] = (float)((double)in->diffuse[j] * div);
			out->ambient[j] = min ( out->ambient[j], 1 );
			out->diffuse[j] = min ( out->diffuse[j], 1 );
		}

		for ( j = 0; j < 2; j++ ) {
			out->direction[j] = (float)((double)in->direction[j] / 255.0);
		}
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
	
	loadmodel->type = mod_brush;
	if (loadmodel != mod_known)
		Com_Error (ERR_DROP, "Loaded a brush model after the world");

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		Com_Error (ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap
	
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadIndexes (&header->lumps[LUMP_INDEXES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadLightgrid (&header->lumps[LUMP_LIGHTGRID]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadBrushsides (&header->lumps[LUMP_BRUSHSIDES]);
	Mod_LoadBrushes (&header->lumps[LUMP_BRUSHES]);
	Mod_LoadFogs (&header->lumps[LUMP_FOGS]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES], &header->lumps[LUMP_SHADERREFS]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadLeafBrushes (&header->lumps[LUMP_LEAFBRUSHES]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	mod->numframes = 2;		// regular and alternate animation

//
// set up the submodels
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		model_t	*starmod;

		bm = &mod->submodels[i];
		starmod = &mod_inline[i];

		*starmod = *loadmodel;
		
		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, starmod->maxs);
		VectorCopy (bm->mins, starmod->mins);
		starmod->radius = bm->radius;
	
		if (i == 0)
			*loadmodel = *starmod;
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i, j;
	dmdl_t				*pinmodel, *pheader;
	dstvert_t			*pinst, *poutst;
	dtriangle_t			*pintri, *pouttri;
	daliasframe_t		*pinframe, *poutframe;
	int					*pincmd, *poutcmd;
	int					version;

	pinmodel = (dmdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Com_Error (ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

	pheader = Hunk_Alloc (LittleLong(pinmodel->ofs_end));
	
	// byte swap the header fields and sanity check
	for (i=0 ; i<sizeof(dmdl_t)/4 ; i++)
		((int *)pheader)[i] = LittleLong (((int *)buffer)[i]);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Com_Error (ERR_DROP, "model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	if (pheader->num_xyz <= 0)
		Com_Error (ERR_DROP, "model %s has no vertices", mod->name);

	if (pheader->num_xyz > MAX_VERTS)
		Com_Error (ERR_DROP, "model %s has too many vertices", mod->name);

	if (pheader->num_st <= 0)
		Com_Error (ERR_DROP, "model %s has no st vertices", mod->name);

	if (pheader->num_tris <= 0)
		Com_Error (ERR_DROP, "model %s has no triangles", mod->name);

	if (pheader->num_frames <= 0)
		Com_Error (ERR_DROP, "model %s has no frames", mod->name);

//
// load base s and t vertices (not used in gl version)
//
	pinst = (dstvert_t *) ((byte *)pinmodel + pheader->ofs_st);
	poutst = (dstvert_t *) ((byte *)pheader + pheader->ofs_st);

	for (i=0 ; i<pheader->num_st ; i++)
	{
		poutst[i].s = LittleShort (pinst[i].s);
		poutst[i].t = LittleShort (pinst[i].t);
	}

//
// load triangle lists
//
	pintri = (dtriangle_t *) ((byte *)pinmodel + pheader->ofs_tris);
	pouttri = (dtriangle_t *) ((byte *)pheader + pheader->ofs_tris);

	for (i=0 ; i<pheader->num_tris ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			pouttri[i].index_xyz[j] = LittleShort (pintri[i].index_xyz[j]);
			pouttri[i].index_st[j] = LittleShort (pintri[i].index_st[j]);
		}
	}

//
// load the frames
//
	for (i=0 ; i<pheader->num_frames ; i++)
	{
		pinframe = (daliasframe_t *) ((byte *)pinmodel 
			+ pheader->ofs_frames + i * pheader->framesize);
		poutframe = (daliasframe_t *) ((byte *)pheader 
			+ pheader->ofs_frames + i * pheader->framesize);

		memcpy (poutframe->name, pinframe->name, sizeof(poutframe->name));
		for (j=0 ; j<3 ; j++)
		{
			poutframe->scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat (pinframe->translate[j]);
		}
		// verts are all 8 bit, so no swapping needed
		memcpy (poutframe->verts, pinframe->verts, 
			pheader->num_xyz*sizeof(dtrivertx_t));

	}

	mod->type = mod_alias;

	// Vic
	mod->aliastype = ALIASTYPE_MD2;

	//
	// load the glcmds
	//
	pincmd = (int *) ((byte *)pinmodel + pheader->ofs_glcmds);
	poutcmd = (int *) ((byte *)pheader + pheader->ofs_glcmds);
	for (i=0 ; i<pheader->num_glcmds ; i++)
		poutcmd[i] = LittleLong (pincmd[i]);


	// register all skins
	memcpy ((char *)pheader + pheader->ofs_skins, (char *)pinmodel + pheader->ofs_skins,
		pheader->num_skins*MAX_SKINNAME);
	for (i=0 ; i<pheader->num_skins ; i++)
	{
		mod->skins[0][i] = R_RegisterShaderMD3 ((char *)pheader + pheader->ofs_skins + i*MAX_SKINNAME);
	}

	mod->mins[0] = -32;
	mod->mins[1] = -32;
	mod->mins[2] = -32;
	mod->maxs[0] = 32;
	mod->maxs[1] = 32;
	mod->maxs[2] = 32;
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
	dsprite_t	*sprin, *sprout;
	int			i;

	sprin = (dsprite_t *)buffer;
	sprout = Hunk_Alloc (modfilelen);

	sprout->ident = LittleLong (sprin->ident);
	sprout->version = LittleLong (sprin->version);
	sprout->numframes = LittleLong (sprin->numframes);

	if (sprout->version != SPRITE_VERSION)
		Com_Error (ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, sprout->version, SPRITE_VERSION);

	if (sprout->numframes > MAX_MD2SKINS)
		Com_Error (ERR_DROP, "%s has too many frames (%i > %i)",
				 mod->name, sprout->numframes, MAX_MD2SKINS);

	// byte swap everything
	for (i=0 ; i<sprout->numframes ; i++)
	{
		sprout->frames[i].width = LittleLong (sprin->frames[i].width);
		sprout->frames[i].height = LittleLong (sprin->frames[i].height);
		sprout->frames[i].origin_x = LittleLong (sprin->frames[i].origin_x);
		sprout->frames[i].origin_y = LittleLong (sprin->frames[i].origin_y);
		memcpy (sprout->frames[i].name, sprin->frames[i].name, MAX_SKINNAME);
		mod->skins[0][i] = R_RegisterShaderMD3 (sprout->frames[i].name);
	}

	mod->type = mod_sprite;
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

	registration_sequence++;
	r_oldviewcluster = -1;		// force markleafs

	Com_sprintf (fullname, sizeof(fullname), "maps/%s.bsp", model);

	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	flushmap = Cvar_Get ("flushmap", "0", 0);
	if ( strcmp(mod_known[0].name, fullname) || flushmap->value)
		Mod_Free (&mod_known[0]);
	r_worldmodel = Mod_ForName(fullname, true);

	r_viewcluster = -1;


}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
struct model_s *R_RegisterModel (char *name)
{
	model_t	*mod;
	int		i;
	dsprite_t	*sprout;
	dmdl_t		*pheader;

	mod = Mod_ForName (name, false);
	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		// register any images used by the models
		if (mod->type == mod_sprite)
		{
			sprout = (dsprite_t *)mod->extradata;
			for (i=0 ; i<sprout->numframes ; i++)
				mod->skins[0][i] = R_RegisterShaderMD3 (sprout->frames[i].name);
		}
		else if (mod->type == mod_alias)
		{
			if ( mod->aliastype == ALIASTYPE_MD2 )
			{
				pheader = (dmdl_t *)mod->extradata;
				for (i=0 ; i<pheader->num_skins ; i++)
					mod->skins[0][i] = R_RegisterShaderMD3 ((char *)pheader + pheader->ofs_skins + i*MAX_SKINNAME);
				mod->numframes = pheader->num_frames;
			} else {
				R_RegisterMd3 ( mod );
			}
		}
		else if (mod->type == mod_brush)
		{
			msurface_t *surf;

			surf = mod->surfaces;
			for (i=0 ; i<mod->numsurfaces ; i++, surf++ ) {
				if ( surf->mesh.shader )
					surf->mesh.shader->registration_sequence = registration_sequence;
			}
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

/*
================
R_ModelNumFrames
================
*/
int R_ModelNumFrames ( struct model_s *model )
{
	return model->numframes;
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

