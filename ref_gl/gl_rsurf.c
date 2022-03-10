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
#include <assert.h>

#include "gl_local.h"

static vec3_t	mesh_vertex[MAX_VERTS];

msurface_t	*r_mtex_surfaces;
msurface_t	*r_generic_surfaces;
msurface_t	*r_additive_surfaces;
msurface_t	*r_flare_surfaces;
msurface_t	*r_fog_surfaces;

extern float bubble_sintable[17], bubble_costable[17];

entity_t	r_worldent;

#define LIGHTMAP_BYTES 4
#define GL_LIGHTMAP_FORMAT GL_RGBA

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	128

int		c_visible_lightmaps;
int		c_visible_textures;

qboolean alpha_surface;

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
** R_DrawTriangleOutlines
*/
void R_DrawTriangleOutlines (void)
{
	if (!gl_showtris->value)
		return;

	GL_EnableMultitexture ( false );

	qglDisable (GL_TEXTURE_2D);
	qglDisable (GL_DEPTH_TEST);
	qglColor4f (1,1,1,1);

	qglEnable (GL_DEPTH_TEST);
	qglEnable (GL_TEXTURE_2D);
}

/*
=================
R_AddSurfaceToList
=================
*/
qboolean R_AddSurfaceToList ( msurface_t *surf )
{
	shader_t *shader;

	if ( !surf->mesh.shader ) {
		return false;
	}
	if ( surf->shaderref->flags & SURF_NODRAW ) {
		return false;
	}

	shader = surf->mesh.shader;

	// perform culling
	if ( !r_nocull->value && ( shader->flags & SHADER_DOCULL ) ) {
		if ( surf->facetype == FACETYPE_PLANAR ) {
			if ( r_faceplanecull->value ) {
				if ( PlaneDiff (r_origin, surf->plane) < 0 )
					return false;
			}
		} else if ( surf->facetype == FACETYPE_MESH ) {
			if ( R_CullBox ( surf->mins, surf->maxs ) )
				return false;
		} else if ( surf->facetype == FACETYPE_TRISURF ) {
		} else if ( surf->facetype == FACETYPE_FLARE ) {
			if ( r_faceplanecull->value ) {
				if ( PlaneDiff (r_origin, surf->plane) < 0 )
					return false;
			}
		}
	}

	if ( surf->facetype == FACETYPE_FLARE ) {
		surf->texturechain = r_flare_surfaces;
		r_flare_surfaces = surf;
		return true;
	}

	if ( surf && surf->fog ) {
		surf->fogchain = r_fog_surfaces;
		r_fog_surfaces = surf;
	}

	if ( shader->flags & SHADER_SKY ) {
		R_AddSkySurface ( &surf->mesh );
		return true;
	}

	// nodraw, hint, fog, skybox, etc.
	if ( !shader->numpasses ) {
		return true;
	}

	if ( shader->flush == SHADER_FLUSH_MULTITEXTURE_2 ||
		 shader->flush == SHADER_FLUSH_MULTITEXTURE_COMBINE ) {
		surf->texturechain = r_mtex_surfaces;
		r_mtex_surfaces = surf;
		return true;
	}

	if ( shader->sort == SHADER_SORT_ADDITIVE ) {
		surf->texturechain = r_additive_surfaces;
		r_additive_surfaces = surf;
		return true;
	}

	surf->texturechain = r_generic_surfaces;
	r_generic_surfaces = surf;
	return true;
}

/*
================
DrawFogSurface
================
*/
void DrawFogSurface ( mesh_t *mesh, mfog_t *fog )
{
	int			i, *lindex;
	shader_t	*shader = fog->shader;
	float		tc[2];
	vec3_t		diff;
	float		dist, vdist;

	// upside-down
	if ( fog->ptype > 2 ) {
		dist = -r_origin[fog->ptype-3] - fog->pdist;
	} else {
		dist = r_origin[fog->ptype] - fog->pdist;
	}

	qglColor3ubv ( shader->fog_color );

	lindex = mesh->firstindex;
	for (i = 0; i < mesh->numindexes; i++, lindex++)
		R_PushElem (*lindex);

	R_DeformVertices ( mesh, mesh_vertex, MAX_VERTS );

	for (i = 0; i < mesh->numverts; i++)
	{
		VectorAdd ( currententity->origin, mesh_vertex[i], diff );

		if ( fog->ptype > 2 ) {
			vdist = -diff[fog->ptype-3] - fog->pdist;
		} else {
			vdist = diff[fog->ptype] - fog->pdist;
		}

		VectorSubtract ( diff, r_origin, diff );

		if ( dist < 0 ) {	// camera is inside the fog brush
			tc[0] = DotProduct ( diff, vpn );
		} else {
			if ( vdist < 0 ) {
				tc[0] = vdist / ( vdist - dist );
				tc[0] *= DotProduct ( diff, vpn );
			} else {
				tc[0] = 0.0f;
			}
		}
	
		tc[0] *= shader->fog_dist;
		tc[1] *= shader->fog_dist;
		tc[1] = -vdist + 1.5f/256.0f;

		R_PushCoord ( tc );
	}

	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();
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
	float radius = 20.0f;		// FIXME?
	float *bub_sin = bubble_sintable, 
		*bub_cos = bubble_costable;
	int i, j;

	VectorSubtract ( surf->origin, r_origin, v );
	dist = VectorNormalize( v );

	if ( dist <= radius ) {
		return;
	}

	intensity = (1024.0f - dist) / 1024.0f;
	intensity = 1.0f - intensity;

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
		surf->mins[0]*intensity, 
		surf->mins[1]*intensity, 
		surf->mins[2]*intensity);

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

/*
================
DrawSurfaceChains
================
*/
void DrawSurfaceChains (void)
{
	msurface_t *s;

	if ( r_mtex_surfaces ) {
		GLSTATE_DISABLE_BLEND
		GLSTATE_DISABLE_ALPHATEST
		qglDisable ( GL_POLYGON_OFFSET );
		qglDepthMask ( GL_TRUE );
		qglDepthFunc ( GL_LEQUAL );

		GL_EnableMultitexture ( true );

		for ( s = r_mtex_surfaces; s; s = s->texturechain ) {
			if ( s->mesh.shader->flush == SHADER_FLUSH_MULTITEXTURE_2 ) {
				R_RenderMeshMultitextured ( &s->mesh );
			} else {
				R_RenderMeshCombined ( &s->mesh );
			}
		}

		GL_EnableMultitexture ( false );
	}

	if ( r_generic_surfaces ) {
		for ( s = r_generic_surfaces; s; s = s->texturechain ) {
			R_RenderMeshGeneric ( &s->mesh );
		}
	}

	if ( r_additive_surfaces ) {
		GL_TexEnv ( GL_MODULATE );

		for ( s = r_additive_surfaces; s; s = s->texturechain ) {
			R_RenderMeshGeneric ( &s->mesh );
		}
	}

	qglDisable ( GL_POLYGON_OFFSET );
	GLSTATE_DISABLE_ALPHATEST

	if ( r_fog_surfaces ) {
		shaderpass_t *pass;

		GLSTATE_ENABLE_BLEND
		qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		GL_TexEnv ( GL_MODULATE );
		GL_Bind ( r_fogtexture->texnum );

		for ( s = r_fog_surfaces; s; s = s->fogchain ) {
			if ( s->mesh.shader->numpasses ) {
				pass = &s->mesh.shader->pass[s->mesh.shader->numpasses-1];
				if ( pass->flags & SHADER_PASS_DEPTHWRITE ) {
					qglDepthMask ( GL_TRUE ); // enable zbuffer updates
					if ( pass->depthfunc == GL_LEQUAL )
						qglDepthFunc ( GL_EQUAL );
					else
						qglDepthFunc ( pass->depthfunc );
				} else {
					qglDepthMask ( GL_FALSE ); // disable zbuffer updates
					qglDepthFunc ( GL_LEQUAL );
				}
			} else {
				qglDepthMask ( GL_FALSE ); // disable zbuffer updates
				qglDepthFunc ( GL_LEQUAL );
			}

			DrawFogSurface ( &s->mesh, s->fog );
		}
	}

	qglDepthFunc ( GL_LEQUAL );

	if ( r_flare_surfaces && r_flare->value ) {
		GLSTATE_ENABLE_BLEND
		qglDepthMask ( GL_FALSE );
		qglDisable ( GL_TEXTURE_2D );
		qglBlendFunc ( GL_ONE, GL_SRC_ALPHA );

		for ( s = r_flare_surfaces; s; s = s->texturechain ) {
			DrawFlareSurface ( s );
		}

		qglEnable ( GL_TEXTURE_2D );
		GLSTATE_DISABLE_BLEND
	}

	qglColor3f ( 1, 1, 1 );
	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDepthMask ( GL_TRUE );
	GLSTATE_DISABLE_BLEND
	GL_TexEnv ( GL_REPLACE );
}

/*
=================
R_RenderDlights
=================
*/
void R_RenderDlights ( void )
{
	if ( !r_dynamiclight->value )
		return;
	if ( !r_dlighttexture )
		return;

	if ( !r_newrefdef.num_dlights ||
		(!r_mtex_surfaces && !r_generic_surfaces ))
		return;

	GL_EnableMultitexture ( false );

	GL_Bind ( r_dlighttexture->texnum );

	GLSTATE_ENABLE_BLEND
	qglBlendFunc ( GL_DST_COLOR, GL_ONE );
	GL_TexEnv ( GL_MODULATE );

	if ( r_mtex_surfaces ) {
		R_MarkLights ( r_mtex_surfaces );
	}

	if ( r_generic_surfaces ) {
		R_MarkLights ( r_generic_surfaces );
	}

	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	GLSTATE_DISABLE_BLEND
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
		if ( psurf->visframe != r_framecount ) {
			if ( R_AddSurfaceToList ( psurf ) ) {
				psurf->visframe = r_framecount;
				c_brush_polys++;
			}
		}
	}

	DrawSurfaceChains();
	R_RenderDlights ();

	R_DrawTriangleOutlines ();

	r_mtex_surfaces = NULL;
	r_generic_surfaces = NULL;
	r_additive_surfaces = NULL;
	r_flare_surfaces = NULL;
	r_fog_surfaces = NULL;
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

    qglPushMatrix ();

	qglTranslatef ( e->origin[0], e->origin[1], e->origin[2] );

    qglRotatef ( e->angles[YAW],	0, 0, 1 );
    qglRotatef ( e->angles[PITCH],	0, 1, 0 );	// stupid quake bug
    qglRotatef ( e->angles[ROLL],	1, 0, 0 );	// stupid quake bug

	R_DrawInlineBModel ();

	qglPopMatrix ();
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
	cplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	float		dot;

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

				if ( surf->visframe != r_framecount ) {
					if ( R_AddSurfaceToList ( surf ) ) {
						surf->visframe = r_framecount;
						c_brush_polys++;
					}
				}
			} while (--c);
		}

		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	if ( plane->type < 3 ) {
		dot = r_origin[plane->type] - plane->dist;
	} else {
		dot = DotProduct (r_origin, plane->normal) - plane->dist;
	}

// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[(dot < 0)], clipflags);

// recurse down the back side
	R_RecursiveWorldNode (node->children[(dot >= 0)], clipflags);
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

	// auto cycle the world frame for texture animation
	r_worldent.frame = (int)(r_newrefdef.time*2);
	r_worldent.model = r_worldmodel;

	currententity = &r_worldent;

	gl_state.currenttextures[0] = gl_state.currenttextures[1] = -1;

	r_mtex_surfaces = NULL;
	r_generic_surfaces = NULL;
	r_additive_surfaces = NULL;
	r_flare_surfaces = NULL;
	r_fog_surfaces = NULL;

	R_ClearSkyBox ();

	R_RecursiveWorldNode (r_worldmodel->nodes, 15);

	R_DrawSkydome (&r_worldmodel->skydome);

	DrawSurfaceChains ();

	R_RenderDlights ();

	R_DrawTriangleOutlines ();

	r_mtex_surfaces = NULL;
	r_generic_surfaces = NULL;
	r_additive_surfaces = NULL;
	r_flare_surfaces = NULL;
	r_fog_surfaces = NULL;
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
	if (gl_lockpvs->value)
		return;

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
GL_CalcCentreTC
================
*/
void GL_CalcMeshCentre ( mesh_t *mesh )
{
	int i;
	mvertex_t *vert;
	vec2_t total = { 0.0f, 0.0f };
	float scale;

	if ( !mesh->numverts )
		return;

	scale = 1.0f / (float)mesh->numverts;

	vert = mesh->firstvert;
	for (i = 0; i < mesh->numverts; i++, vert++)
	{
		total[0] += vert->tex_st[0];
		total[1] += vert->tex_st[1];
	}

	mesh->tex_centre_tc[0] = total[0] * scale;
	mesh->tex_centre_tc[1] = total[1] * scale;
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

	if ( !( shader->flags & SHADER_DEFORMVERTS ) )
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
			VectorCopy ( normal, v[j]->rot_centre );
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
	unsigned		dummy[128*128];

	r_framecount = 1;

	GL_EnableMultitexture( true );
	GL_SelectTexture( GL_TEXTURE1 );

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

	// initialize the dynamic lightmap texture
	GL_Bind( gl_state.lightmap_textures + 0 );
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D( GL_TEXTURE_2D, 
				   0, 
				   gl_lms.internal_format,
				   BLOCK_WIDTH, BLOCK_HEIGHT, 
				   0, 
				   GL_LIGHTMAP_FORMAT, 
				   GL_UNSIGNED_BYTE, 
				   dummy );
}

/*
=======================
GL_EndBuildingLightmaps
=======================
*/
void GL_EndBuildingLightmaps (void)
{
	GL_EnableMultitexture( false );
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

