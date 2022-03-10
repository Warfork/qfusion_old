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
// GL_RSURF.C: surface-related refresh code
#include "gl_local.h"

extern float bubble_sintable[17], bubble_costable[17];

entity_t	r_worldent;

static meshlist_t	r_meshlist;
static meshlist_t	*currentlist;

static vec3_t	modelorg;		// relative to viewpoint

static int num_flares;
static msurface_t *flare_surfaces[MAX_RENDER_MESHES];

static float r_flarescale;

#define GL_LIGHTMAP_FORMAT GL_RGBA

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	128

typedef struct
{
	int internal_format;
	int	current_lightmap_texture;
} gllightmapstate_t;

static gllightmapstate_t gl_lms;

static void		LM_InitBlock( void );
static void		LM_UploadBlock( void );
static qboolean	LM_AllocBlock (int w, int h, int *x, int *y);

void R_BuildLightMap (byte *data, byte *dest);

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
=================
R_AddMeshToList
=================
*/
meshlistmember_t *R_AddMeshToList ( mesh_t *mesh, mfog_t *fog )
{
	meshlistmember_t *meshmember;
	shader_t *shader;

	if ( r_meshlist.num_meshes >= MAX_RENDER_MESHES ) {
		return NULL;
	}
	if ( !mesh->shader ) {
		return NULL;
	}

	shader = mesh->shader;

	meshmember = &currentlist->meshlist[r_meshlist.num_meshes++];
	meshmember->mesh = mesh;
	meshmember->surf = NULL;
	meshmember->entity = currententity;
	meshmember->fog = fog;

	if ( currententity->model->type == mod_brush ) {
		meshmember->sortkey = (shader->sort << 28) | ((shader-r_shaders) << 18) | ((mesh->lightmaptexturenum+1) << 8) |
			(fog ? fog - r_worldmodel->fogs + 1 : 0);
	} else {
		meshmember->sortkey = (shader->sort << 28) | ((shader-r_shaders) << 18) | ((currententity-r_newrefdef.entities) << 1) |
			(fog ? fog - r_worldmodel->fogs + 1 : 0);
	}

	return meshmember;
}

/*
================
DrawFlareSurface
================
*/
void DrawFlareSurface ( msurface_t *surf )
{
	vec3_t v;
	vec3_t v_right, v_up;
	float dist, intensity;
	float radius = r_flaresize->value * 0.5f;
	float *bub_sin = bubble_sintable, 
		*bub_cos = bubble_costable;
	int i, j;

	VectorSubtract ( surf->origin, r_origin, v );
	dist = VectorNormalize( v );

	if ( dist <= radius ) {
		return;
	}

	intensity = dist / 1024.0f;
	radius = r_flaresize->value * intensity;

	if ( radius < r_flaresize->value * 0.3f )
		radius = r_flaresize->value * 0.3f;
	else if ( radius > r_flaresize->value )
		radius = r_flaresize->value;

	// clamp, but don't let the flare disappear.
	if (intensity > 1.0f) 
		intensity = 1.0f;
	else if (intensity < 0.0f) 
		intensity = 0.0f;

	v_right[0] = v[1];
	v_right[1] = -v[0];
	v_right[2] = 0;
	VectorNormalizeFast ( v_right );
	CrossProduct ( v_right, v, v_up );
	VectorScale ( v, radius, v );
	VectorSubtract ( surf->origin, v, v );

	qglBegin ( GL_TRIANGLE_FAN );
	qglColor3f (
		surf->mins[0] * r_flarescale, 
		surf->mins[1] * r_flarescale, 
		surf->mins[2] * r_flarescale);

	qglVertex3fv ( v );
	qglColor3f ( 0, 0, 0 );

	for ( i = 16; i >= 0; i--, bub_sin++, bub_cos++ ) {
		for (j = 0; j < 3; j++)
			v[j] = surf->origin[j] + (v_right[j]*(*bub_cos) +
				+ v_up[j]*(*bub_sin)) * radius;

		qglVertex3fv ( v );
	}

	qglEnd ();
}


static int face_cmp (const void *a, const void *b)
{
    return ((meshlistmember_t*)a)->sortkey - ((meshlistmember_t*)b)->sortkey;
}

static void R_SortFaces (void)
{
    qsort ((void*)r_meshlist.meshlist, r_meshlist.num_meshes, sizeof(meshlistmember_t), face_cmp);
}

/*
================
R_DrawSortedPolys
================
*/
void R_DrawSortedPolys (void)
{
	int i, lastsort;
	meshlistmember_t *meshmember;
	
	currententity = &r_worldent;
	currentmodel = r_worldmodel;

	if ( r_meshlist.num_meshes ) {
		qboolean skydrawn = false;

		R_SortFaces ();

		meshmember = r_meshlist.meshlist;
		lastsort = SHADER_SORT_NONE;
		for ( i = 0; i < r_meshlist.num_meshes; i++, meshmember++ ) {
			if ( (meshmember->mesh->shader->flags & SHADER_SKY) ) {
				if ( !skydrawn && !r_fastsky->value ) {
					R_DrawSkydome ();
					skydrawn = true;
				}

				if ( !r_fastsky->value ) {
					GL_EnableMultitexture ( false );
					R_RenderSkySurface ( meshmember->mesh, meshmember->fog );
				} else if ( !skydrawn ) {
					R_DrawFastSky ();
					skydrawn = true;
				}

				lastsort = SHADER_SORT_SKY;
				continue;
			}
			if ( (meshmember->mesh->shader->sort > SHADER_SORT_SKY ) && 
				lastsort < SHADER_SORT_SKY && !skydrawn)  {
				if ( r_fastsky->value ) {
					R_DrawFastSky ();
				} else {
					R_DrawSkydome ();
				}

				skydrawn = true;
			}
			lastsort = meshmember->mesh->shader->sort;

			currententity = meshmember->entity;
			currentmodel = currententity->model;

			if ( !currentmodel ) {		// FIXME?
				continue;
			}
			if ( currententity->visframe == r_framecount ) {
				continue;
			}

			if ( currentmodel->aliastype == ALIASTYPE_MD3 ) {
				R_DrawMd3Model ( currententity );
			} else {
				if ( currententity != &r_worldent ) {
					qglPushMatrix ();
					
					qglTranslatef ( currententity->origin[0], currententity->origin[1], currententity->origin[2] );
					
					qglRotatef ( currententity->angles[YAW],	0, 0, 1 );
					qglRotatef ( currententity->angles[PITCH],	0, 1, 0 );	// stupid quake bug
					qglRotatef ( currententity->angles[ROLL],	1, 0, 0 );	// stupid quake bug
				}

				meshmember->mesh->shader->flush ( meshmember->mesh, meshmember->fog, meshmember->surf );

				if ( currententity != &r_worldent ) {
					qglPopMatrix ();
				}
			}
		}

		r_meshlist.num_meshes = 0;
	}

	GL_EnableMultitexture ( false );
	qglDisable ( GL_POLYGON_OFFSET_FILL );
	GLSTATE_DISABLE_ALPHATEST
	GLSTATE_DISABLE_BLEND
	qglDepthFunc ( GL_LEQUAL );

	if ( num_flares ) {
		GLSTATE_ENABLE_BLEND
		qglDepthMask ( GL_FALSE );
		qglDisable ( GL_TEXTURE_2D );
//		qglDisable ( GL_DEPTH_TEST );
		qglBlendFunc ( GL_ONE, GL_ONE );

		r_flarescale = 1.0f / r_flarefade->value;

		for ( i = 0; i < num_flares; i++ ) {
			DrawFlareSurface ( flare_surfaces[i] );
		}

		qglColor3f ( 1, 1, 1 );
//		qglEnable ( GL_DEPTH_TEST );
		qglEnable ( GL_TEXTURE_2D );
		GLSTATE_DISABLE_BLEND
		num_flares = 0;
	}

	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDepthMask ( GL_TRUE );
}

/*
=================
R_AddSurfaceToList
=================
*/
void R_AddSurfaceToList (msurface_t *surf)
{
	meshlistmember_t *meshmember;

	if ( surf->facetype == FACETYPE_FLARE ) {
		if ( num_flares < MAX_RENDER_MESHES && r_flares->value && 
			(r_flaresize->value > 0) && (r_flarefade->value > 0) ) {
			flare_surfaces[num_flares++] = surf;
			c_brush_polys++;
		}
		return;
	}

	if ( !r_nocull->value && (surf->mesh.shader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK)) ) {
		switch ( surf->facetype )
		{
			case FACETYPE_PLANAR:
				if ( r_faceplanecull->value ) {
					if ( surf->plane->type < 3 ) {
//						if ( vpn[surf->plane->type] > r_newrefdef.cos_half_fox_x )
//							return;
						if ( modelorg[surf->plane->type] - surf->plane->dist <= BACKFACE_EPSILON )
							return;
					} else {
						if ( DotProduct (modelorg, surf->plane->normal) - surf->plane->dist <= BACKFACE_EPSILON )
							return;
					}
				}
				break;

			case FACETYPE_MESH:
			case FACETYPE_TRISURF:
				if ( R_CullBox ( surf->mins, surf->maxs ) )
					return;

			default:
				break;
		}
	}

	meshmember = R_AddMeshToList ( &surf->mesh, surf->fog );
	if ( meshmember ) {
		meshmember->surf = surf;

		if ( surf->mesh.shader->flags & SHADER_SKY )
			R_AddSkySurface ( surf );

		c_brush_polys++;
	}
}

/*
=================
R_DrawInlineBModel
=================
*/
void R_DrawInlineBModel (void)
{
	int			i;
	msurface_t	*psurf;

	psurf = &currentmodel->surfaces[currentmodel->firstmodelsurface];
	for ( i=0 ; i<currentmodel->nummodelsurfaces ; i++, psurf++ ) {
		if ( psurf->visframe == r_framecount ) {
			continue;
		}

		psurf->visframe = r_framecount;
		R_AddSurfaceToList ( psurf );
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	vec3_t		mins, maxs;
	int			i;
	qboolean	rotated;

	if (currentmodel->nummodelsurfaces == 0)
		return;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, currentmodel->mins, mins);
		VectorAdd (e->origin, currentmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	VectorSubtract (r_newrefdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	R_DrawInlineBModel ();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode (mnode_t *node, int clipflags)
{
	int			c;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;

	if ( node->contents == CONTENTS_SOLID )
		return;		// solid
	if ( node->visframe != r_visframecount )
		return;
	if ( !r_nocull->value )
	{
		int clipped;

		if (clipflags & 1)
			clipped = BoxOnPlaneSide (node->mins, node->maxs, &frustum[0]);
		else if (clipflags & 2)
			clipped = BoxOnPlaneSide (node->mins, node->maxs, &frustum[1]);
		else if (clipflags & 4) 
			clipped = BoxOnPlaneSide (node->mins, node->maxs, &frustum[2]);
		else if (clipflags & 8) 
			clipped = BoxOnPlaneSide (node->mins, node->maxs, &frustum[3]);
		if (clipped == 2)
			return;
		else if (clipped == 1)
			clipflags &= ~1;
	}

// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *)node;

		// check for door connected areas
		if (r_newrefdef.areabits)
		{
			if (! (r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) ) {
				return;		// not visible
			}
		}

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				surf = *mark++;

				if ( surf->visframe == r_framecount ) {
					continue;
				}

				surf->visframe = r_framecount;
				R_AddSurfaceToList ( surf );
			} while (--c);
		}
		return;
	}

	// recurse down the children
	R_RecursiveWorldNode (node->children[0], clipflags);
	R_RecursiveWorldNode (node->children[1], clipflags);
}


/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	if (!r_drawworld->value)
		return;

	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
		return;

	currentmodel = r_worldmodel;
	VectorCopy (r_newrefdef.vieworg, modelorg);

	// auto cycle the world frame for texture animation
	r_worldent.frame = (int)(r_newrefdef.time*2);
	r_worldent.model = r_worldmodel;
	currententity = &r_worldent;
	currentlist = &r_meshlist;

	R_ClearSkyBox ();
	R_RecursiveWorldNode( r_worldmodel->nodes, 15 );
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	byte	fatvis[MAX_MAP_LEAFS/8];
	mnode_t	*node;
	int		i, c;
	mleaf_t	*leaf;
	int		cluster;

	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2 && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if ( Com_ServerState() && developer->value ) {
		if (gl_lockpvs->value)
			return;
	}

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<r_worldmodel->numleafs ; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_worldmodel->numnodes ; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS (r_viewcluster, r_worldmodel);
	// may have to combine two clusters because of solid water boundaries
	if (r_viewcluster2 != r_viewcluster)
	{
		memcpy (fatvis, vis, (r_worldmodel->numleafs+7)/8);
		vis = Mod_ClusterPVS (r_viewcluster2, r_worldmodel);
		c = (r_worldmodel->numleafs+31)/32;
		for (i=0 ; i<c ; i++)
			((int *)fatvis)[i] |= ((int *)vis)[i];
		vis = fatvis;
	}
	
	for (i=0,leaf=r_worldmodel->leafs ; i<r_worldmodel->numleafs ; i++, leaf++)
	{
		cluster = leaf->cluster;
		if (cluster == -1)
			continue;
		if (vis[cluster>>3] & (1<<(cluster&7)))
		{
			node = (mnode_t *)leaf;
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

/*
================
GL_PretransformAutosprites
================
*/
void GL_PretransformAutosprites (msurface_t *surf)
{
	int i, j;
	shader_t *shader;
	mvertex_t *v[4];
	vec3_t normal, right, up, tv, tempvec;
	mat3x3_t matrix, imatrix;

	if ( !surf->mesh.shader )
		return;

	shader = surf->mesh.shader;

	if ( !shader->numdeforms )
		return;

	if ( surf->facetype != FACETYPE_PLANAR && 
		surf->facetype != FACETYPE_TRISURF )
		return;

	for ( i = 0; i < shader->numdeforms; i++ )
	{
		if ( shader->deform_vertices[i] == DEFORMV_AUTOSPRITE ||
			shader->deform_vertices[i] == DEFORMV_AUTOSPRITE2 )
			break;
	}

	if ( i == shader->numdeforms )
		return;

	for ( i = 0; i < surf->mesh.numindexes; i += 6 )
	{
		v[0] = surf->mesh.firstvert + surf->mesh.firstindex[i+0];
		v[1] = surf->mesh.firstvert + surf->mesh.firstindex[i+3];
		v[2] = surf->mesh.firstvert + surf->mesh.firstindex[i+4];
		v[3] = surf->mesh.firstvert + surf->mesh.firstindex[i+5];

		for ( j = 0; j < 3; j++ )
			normal[j] = (v[0]->normal[j]+v[1]->normal[j]+v[2]->normal[j]+v[3]->normal[j]) * 0.25;

		MakeNormalVectors ( normal, right, up );

		for ( j = 0; j < 3; j++ )
		{
			matrix[0][j] = right[j];
			matrix[1][j] = up[j];
			matrix[2][j] = normal[j];
		}

		Matrix3_Transponse ( matrix, imatrix ); 

		for ( j = 0; j < 3; j++ )
			normal[j] = (v[0]->position[j]+v[1]->position[j]+v[2]->position[j]+v[3]->position[j]) * 0.25;

		for ( j = 0; j < 4; j++ )
		{
			VectorSubtract ( v[j]->position, normal, tempvec );
			Matrix3_Multiply_Vec3 ( imatrix, tempvec, tv );
			VectorAdd ( normal, tv, v[j]->position );
		}
	}
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

/*
==================
GL_BeginBuildingLightmaps

==================
*/
void GL_BeginBuildingLightmaps (model_t *m)
{
	r_framecount = 1;

	memset ( &r_meshlist, 0, sizeof(meshlist_t) );

	if (!gl_state.lightmap_textures)
		gl_state.lightmap_textures	= TEXNUM_LIGHTMAPS;

	gl_lms.current_lightmap_texture = 1;

	/*
	** if mono lightmaps are enabled and we want to use alpha
	** blending (a,1-a) then we're likely running on a 3DLabs
	** Permedia2.  In a perfect world we'd use a GL_ALPHA lightmap
	** in order to conserve space and maximize bandwidth, however 
	** this isn't a perfect world.
	**
	** So we have to use alpha lightmaps, but stored in GL_RGBA format,
	** which means we only get 1/16th the color resolution we should when
	** using alpha lightmaps.  If we find another board that supports
	** only alpha lightmaps but that can at least support the GL_ALPHA
	** format then we should change this code to use real alpha maps.
	*/
	if ( toupper( gl_monolightmap->string[0] ) == 'A' ) {
		gl_lms.internal_format = gl_tex_alpha_format;
	}
	// try to do hacked colored lighting with a blended texture
	else if ( toupper( gl_monolightmap->string[0] ) == 'C' ) {
		gl_lms.internal_format = gl_tex_alpha_format;
	} else if ( toupper( gl_monolightmap->string[0] ) == 'I' ) {
		gl_lms.internal_format = GL_INTENSITY8;
	} else if ( toupper( gl_monolightmap->string[0] ) == 'L' ) {
		gl_lms.internal_format = GL_LUMINANCE8;
	} else {
		gl_lms.internal_format = gl_tex_solid_format;
	}
}

/*
=======================
GL_EndBuildingLightmaps
=======================
*/
void GL_EndBuildingLightmaps (void)
{
}

void R_BuildLightmaps (model_t *model)
{
	int i;
	byte lightmap_buffer[4*BLOCK_WIDTH*BLOCK_HEIGHT];

	if ( model->numlightmaps >= MAX_LIGHTMAPS )
		Com_Error( ERR_DROP, "R_BuildLightmaps() - MAX_LIGHTMAPS exceeded\n" );

	GL_BeginBuildingLightmaps (model);

	for (i = 0; i < model->numlightmaps; i++)
	{
        R_BuildLightMap (model->lightdata + i * 3 * 128 * 128, lightmap_buffer);

		GL_Bind( gl_state.lightmap_textures + i );
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		qglTexImage2D( GL_TEXTURE_2D, 
						   0, 
						   gl_lms.internal_format,
						   BLOCK_WIDTH, BLOCK_HEIGHT, 
						   0, 
						   GL_LIGHTMAP_FORMAT, 
						   GL_UNSIGNED_BYTE, 
						   lightmap_buffer );
	}

	GL_EndBuildingLightmaps ();
}

