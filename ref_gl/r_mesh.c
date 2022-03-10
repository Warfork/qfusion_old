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
// r_mesh.c: transformation and sorting

#include "r_local.h"

#define		QSORT_MAX_STACKDEPTH	4096

float		r_flarescale;

meshlist_t	*currentlist;

void R_DrawPortalSurface ( meshbuffer_t *mb );

/*
================
R_CopyMeshBuffer
================
*/
#define R_CopyMeshBuffer(in,out) ((out)=(in))

/*
================
R_QSortMeshBuffers

Quicksort
================
*/
static void R_QSortMeshBuffers (meshbuffer_t *meshes, int Li, int Ri)
{
	int median, li, ri;
	msortkey_t pivot;
	static meshbuffer_t tempbuf;
	int localstack[QSORT_MAX_STACKDEPTH], stackdepth = 0;

mark0:
	li = Li;
	ri = Ri;

	median = (Li+Ri) >> 1;

	if ( meshes[Li].sortkey > meshes[median].sortkey ) {
		if ( meshes[Li].sortkey < meshes[Ri].sortkey ) 
			median = Li;
	} else if ( meshes[Ri].sortkey < meshes[median].sortkey ) {
		median = Ri;
	}

	pivot = meshes[median].sortkey;
	while (li < ri)
	{
		while (meshes[li].sortkey < pivot) li++;
		while (meshes[ri].sortkey > pivot) ri--;

		if (li <= ri)
		{
			R_CopyMeshBuffer( meshes[ri], tempbuf );
			R_CopyMeshBuffer( meshes[li], meshes[ri] );
			R_CopyMeshBuffer( tempbuf, meshes[li] );

			li++;
			ri--;
		}
	}

	if ( (Li < ri) && (stackdepth < QSORT_MAX_STACKDEPTH) ) {
		localstack[stackdepth++] = li;
		localstack[stackdepth++] = Ri;
		li = Li;
		Ri = ri;
		goto mark0;
	}

	if ( li < Ri ) {
		Li = li;
		goto mark0;
	}

	if ( stackdepth ) {	
		Ri = ri = localstack[--stackdepth];
		Li = li = localstack[--stackdepth];
		goto mark0;
	}
}

/*
================
R_ISortMeshes

Insertion sort
================
*/
static void R_ISortMeshBuffers (meshbuffer_t *meshes, int num_meshes)
{
	int i, j;
	static meshbuffer_t tempbuf;

	for ( i = 1; i < num_meshes; i++ )
	{
		R_CopyMeshBuffer ( meshes[i], tempbuf );
		j = i - 1; 
		
		while ( (j >= 0) && (meshes[j].sortkey > tempbuf.sortkey) ) {
			R_CopyMeshBuffer ( meshes[j], meshes[j+1] );
			j--;
		}
		
		if (i != j+1)
			R_CopyMeshBuffer ( tempbuf, meshes[j+1] );
	}
}

/*
=================
R_AddMeshToBuffer

Calculate sortkey and store info used for batching and sorting.
All 3D-geometry passes this function.
=================
*/
meshbuffer_t *R_AddMeshToBuffer ( mesh_t *mesh, mfog_t *fog, msurface_t *surf, shader_t *shader, int infokey )
{
	int entnumber, fognumber;
	meshbuffer_t *meshbuf;

	if ( !shader || (mesh && R_InvalidMesh (mesh)) ) {
		return NULL;
	}
	if ( shader->sort >= SHADER_SORT_UNDERWATER ) {
		if ( currentlist->num_additive_meshes >= MAX_RENDER_ADDITIVE_MESHES )
			return NULL;
	} else {
		if ( (shader->sort == SHADER_SORT_PORTAL) && (r_mirrorview || r_portalview) )
			return NULL;
		if ( currentlist->num_meshes >= MAX_RENDER_MESHES )
			return NULL;
	}

	if ( shader->flags & SHADER_VIDEOMAP ) {
		Shader_UploadCinematic ( shader );
	}

	if ( shader->sort >= SHADER_SORT_UNDERWATER )
		meshbuf = &currentlist->meshbuffer_additives[currentlist->num_additive_meshes++];
	else
		meshbuf = &currentlist->meshbuffer[currentlist->num_meshes++];

	meshbuf->mesh = mesh;
	meshbuf->shader = shader;
	meshbuf->entity = currententity;
	meshbuf->infokey = infokey;

	if ( (meshbuf->fog = fog) ) {
		fognumber = (fog - r_worldbmodel->fogs) + 1;
	} else {
		fognumber = 0;
	}

	if ( currententity == &r_worldent ) {
		entnumber = MAX_ENTITIES - 1;
	} else if ( currententity == &r_polyent ) {
		entnumber = MAX_ENTITIES - 1;
	} else {
		entnumber = MAX_ENTITIES - (currententity - r_newrefdef.entities) - 1;
	}

	if ( surf ) {
		if ( surf->lightmaptexturenum > -1 ) {
			meshbuf->sortkey = (shader->sort << 28) | ((shader-r_shaders) << 18) | ((surf->lightmaptexturenum+1) << 8) | (fognumber);
		} else {
			meshbuf->sortkey = (shader->sort << 28) | ((shader-r_shaders) << 18) | (entnumber << 8) | (fognumber);
		}
		meshbuf->sortkey <<= 32;
		meshbuf->dlightbits = surf->dlightbits;

		if ( surf->dlightbits && (surf->dlightframe == r_framecount) ) {
			meshbuf->sortkey |= (msortkey_t)meshbuf->dlightbits;
		}
	} else {
		meshbuf->dlightbits = 0;
		meshbuf->sortkey = (shader->sort << 28) | ((shader-r_shaders) << 18) | (entnumber << 8) | (fognumber);
		meshbuf->sortkey <<= 32;
	}

	return meshbuf;
}

/*
================
R_DrawMeshBuffer

Draw the mesh or batch it.
================
*/
inline void R_DrawMeshBuffer ( meshbuffer_t *mb, meshbuffer_t *nextmb, qboolean shadow )
{
	int features;
	qboolean deformvBulge;
	shader_t *shader;

	currententity = mb->entity;
	currentmodel = currententity->model;
	shader = mb->shader;

	switch ( currententity->type )
	{
		case ET_MODEL:
			switch ( currentmodel->type )
			{
			// batched geometry
				case mod_brush:
					if ( shadow ) {
						break;
					}

					deformvBulge = ( (shader->flags & SHADER_DEFORMV_BULGE) != 0 );
					features = shader->features;
					if ( r_shownormals->value && !shadow ) {
						features |= MF_NORMALS;
					}
					if ( shader->flags & SHADER_AUTOSPRITE ) {
						features |= MF_NOCULL;
					}

					if ( shader->flags & SHADER_FLARE ) {
						R_PushFlare ( mb );
					} else if ( deformvBulge ) {
						R_PushMesh ( mb->mesh, MF_NONBATCHED|features );
					} else {
						R_PushMesh ( mb->mesh, MF_NONE|features );
					}

					if ( !nextmb
						|| nextmb->sortkey != mb->sortkey
						|| ((nextmb->entity != mb->entity) 
							&& (!(shader->flags & SHADER_ENTITY_MERGABLE) || !(nextmb->shader->flags & SHADER_ENTITY_MERGABLE)))
						|| deformvBulge || (nextmb->shader->flags & SHADER_DEFORMV_BULGE)
						|| R_BackendOverflow (nextmb->mesh) ) {

						if ( shader->flags & SHADER_FLARE )
							R_TranslateForEntity ( mb->entity );
						else if ( currentmodel != r_worldmodel )
							R_RotateForEntity ( mb->entity );
						else
							qglLoadMatrixf ( r_modelview_matrix );

						R_RenderMeshBuffer ( mb, shadow );
					}
					break;

			// non-batched geometry
				case mod_alias:
					R_DrawAliasModel ( mb, shadow );
					break;

				case mod_sprite:
					if ( shadow ) {
						break;
					}
					
					R_DrawSpriteModel ( mb );
					break;

				case mod_dpm:
					R_DrawDarkPlacesModel ( mb, shadow );
					break;

				default:
					break;
			}
			break;

		case ET_SPRITE:
			if ( shadow ) {
				break;
			}

			R_DrawSpritePoly ( mb );

			if ( !nextmb
				|| nextmb->sortkey != mb->sortkey
				|| ((nextmb->entity != mb->entity) 
					&& (!(shader->flags & SHADER_ENTITY_MERGABLE) || !(nextmb->shader->flags & SHADER_ENTITY_MERGABLE)))
				|| R_BackendOverflow (nextmb->mesh) ) {

				currententity = &r_worldent;
				currentmodel = r_worldmodel;
				qglLoadMatrixf ( r_modelview_matrix );

				R_RenderMeshBuffer ( mb, shadow );
			}
			break;

		case ET_BEAM:
			break;

		case ET_PORTALSURFACE:
			break;

		case ET_POLY:
			R_DrawPoly ( mb );
			break;
	}
}

/*
================
R_DrawSortedMeshes
================
*/
void R_DrawSortedMeshes (void)
{
	int i;
	meshbuffer_t *meshbuf;
	shader_t *shader;
	qboolean skydrawn;

	currententity = &r_worldent;
	skydrawn = false;

	if ( currentlist->num_meshes ) {
		R_QSortMeshBuffers ( currentlist->meshbuffer, 0, currentlist->num_meshes - 1 );

		meshbuf = currentlist->meshbuffer;
		for ( i = 0; i < currentlist->num_meshes; i++, meshbuf++ ) {
			shader = meshbuf->shader;

			if ( shader->sort == SHADER_SORT_PORTAL ) {
				R_DrawPortalSurface ( meshbuf );
			} else if ( shader->flags & SHADER_SKY ) {
				if ( !skydrawn ) {
					R_DrawSky ( shader );
					skydrawn = true;
				}
				continue;
			}

			if ( i == currentlist->num_meshes - 1 ) {
				R_DrawMeshBuffer ( meshbuf, NULL, false );
			} else {
				R_DrawMeshBuffer ( meshbuf, meshbuf+1, false );
			}
		}
	}

	if ( currentlist->num_meshes && r_shadows->value ) {
		GL_EnableMultitexture ( false );
		qglDisable (GL_TEXTURE_2D);

		if ( r_shadows->value == 1 ) {
			qglDepthFunc (GL_LEQUAL);
			qglEnable ( GL_BLEND );
			qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglColor4f ( 0, 0, 0, bound (0.0f, r_shadows_alpha->value, 1.0f) );

			qglEnable ( GL_STENCIL_TEST );
			qglStencilFunc (GL_EQUAL, 128, 0xFF);
			qglStencilOp (GL_KEEP, GL_KEEP, GL_INCR);
		} else if ( r_shadows->value == 2 ) {
			qglEnable ( GL_STENCIL_TEST );
			qglEnable ( GL_CULL_FACE );
			qglEnable ( GL_BLEND );

			qglColor4f (1, 1, 1, 1);
			qglColorMask (0, 0, 0, 0);
			qglDepthFunc (GL_LESS);
			qglStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
			qglStencilFunc (GL_ALWAYS, 128, 0xFF);
		} else {
			qglDisable ( GL_STENCIL_TEST );
			qglDisable ( GL_CULL_FACE );
			qglEnable ( GL_BLEND );

			qglBlendFunc (GL_ONE, GL_ONE);
			qglDepthFunc (GL_LEQUAL);

			qglColor3f ( 1.0, 0.1, 0.1 );
		}

		qglDisable ( GL_ALPHA_TEST );
		qglDepthMask ( GL_FALSE );

		meshbuf = currentlist->meshbuffer;
		for ( i = 0; i < currentlist->num_meshes; i++, meshbuf++ ) {
			if ( meshbuf->entity->flags & (RF_NOSHADOW|RF_WEAPONMODEL) ) {
				continue;
			}

			if ( i == currentlist->num_meshes - 1 ) {
				R_DrawMeshBuffer ( meshbuf, NULL, true );
			} else {
				R_DrawMeshBuffer ( meshbuf, meshbuf+1, true );
			}
		}

		if ( r_shadows->value == 1 ) {
			qglDisable ( GL_STENCIL_TEST );
		} else if ( r_shadows->value == 2 ) {
			qglStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
			qglDisable ( GL_STENCIL_TEST );
			qglColorMask (1, 1, 1, 1);
		} else {
			qglDisable ( GL_BLEND );
		}

		qglEnable (GL_TEXTURE_2D);

		qglDepthMask (GL_TRUE);
		qglDepthFunc (GL_LEQUAL);
	}

	if ( currentlist->num_additive_meshes ) {
		R_ISortMeshBuffers ( currentlist->meshbuffer_additives, currentlist->num_additive_meshes );

		meshbuf = currentlist->meshbuffer_additives;
		for ( i = 0; i < currentlist->num_additive_meshes; i++, meshbuf++ ) {
			if ( i == currentlist->num_additive_meshes - 1 ) {
				R_DrawMeshBuffer ( meshbuf, NULL, false );
			} else {
				R_DrawMeshBuffer ( meshbuf, meshbuf+1, false );
			}
		}
	}

	currentlist->num_meshes = 0;
	currentlist->num_additive_meshes = 0;

	qglLoadMatrixf ( r_modelview_matrix );

	GL_EnableMultitexture ( false );
	qglDisable ( GL_POLYGON_OFFSET_FILL );
	qglDisable ( GL_ALPHA_TEST );
	qglDisable ( GL_BLEND );
	qglDepthFunc ( GL_LEQUAL );

	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDepthMask ( GL_TRUE );
}

/*
===============
R_DrawPortalSurface
===============
*/
static meshlist_t r_portallist;

void R_DrawPortalSurface ( meshbuffer_t *mb )
{
	int i;
	float dist,	d;
	meshlist_t *prevlist;
	refdef_t r;
	mat4_t modelviewm, projectionm;
	vec3_t v[3], prev_vpn, prev_vright, prev_vup;
	entity_t *ent;
	mesh_t *mesh;
	cplane_t *portal_plane = &r_clipplane;

	if ( !(mesh = mb->mesh) || !mesh->xyz_array ) {
		return;
	}

	VectorCopy ( mesh->xyz_array[mesh->indexes[0+0]], v[0] );
	VectorCopy ( mesh->xyz_array[mesh->indexes[0+1]], v[1] );
	VectorCopy ( mesh->xyz_array[mesh->indexes[0+2]], v[2] );
	
	PlaneFromPoints ( v, portal_plane );
	CategorizePlane ( portal_plane );

	if ( (dist = PlaneDiff (r_origin, portal_plane)) <= BACKFACE_EPSILON ) {
		return;
	}

	ent = r_newrefdef.entities;
	for ( i = 0; i < r_newrefdef.num_entities; i++, ent++ ) {
		if ( ent->type == ET_PORTALSURFACE ) {
			d = PlaneDiff ( ent->origin, portal_plane );
			if ( (d >= -64) && (d <= 64) ) {
				break;
			}
		}
	}

	if ( i == r_newrefdef.num_entities ) {
		return;
	}

	prevlist = currentlist;
	memcpy ( &r, &r_newrefdef, sizeof (refdef_t) );
	VectorCopy ( vpn, prev_vpn );
	VectorCopy ( vright, prev_vright );
	VectorCopy ( vup, prev_vup );

	Matrix4_Copy ( r_modelview_matrix, modelviewm );
	Matrix4_Copy ( r_projection_matrix, projectionm );

	r_portallist.num_meshes = 0;
	r_portallist.num_additive_meshes = 0;

	if ( VectorCompare (ent->origin, ent->oldorigin) )
	{	// mirror
		if ( portal_plane->type < 3 ) {
			d = 2*(r_origin[portal_plane->type] - portal_plane->dist);
			r_newrefdef.vieworg[portal_plane->type] -= d;
		
			d = 2 * vpn[portal_plane->type];
			vpn[portal_plane->type] -= d;
		} else {
			d = -2*(DotProduct (r_origin, portal_plane->normal) - portal_plane->dist);
			VectorMA (r_origin, d, portal_plane->normal, r_newrefdef.vieworg);
			
			d = -2*DotProduct ( vpn, portal_plane->normal );
			VectorMA ( vpn, d, portal_plane->normal, vpn );
		}

		VectorNormalize ( vpn );
		VecToAngles ( vpn, r_newrefdef.viewangles );
		r_newrefdef.viewangles[2] = -r_newrefdef.viewangles[2];

		r_mirrorview = true;
	}
	else
	{	// portal
		extern vec3_t r_avertexnormals[];
		mat3_t A, B, Bt, rot, tmp, D;
		vec3_t tvec;

		if ( mb->shader->flags & SHADER_AGEN_PORTAL ) {
			VectorSubtract ( mb->mesh->xyz_array[0], r_origin, tvec );

			if ( !r_fastsky->value && (VectorLength(tvec) > 255) )
				return;
		}

		// build world-to-portal rotation matrix
		VectorNegate ( portal_plane->normal, A[0] );
        if (A[0][0] || A[0][1])
		{
			VectorSet ( A[1], A[0][1], -A[0][0], 0 );
			VectorNormalizeFast ( A[1] );
			CrossProduct ( A[0], A[1], A[2] );
		}
		else
		{
			VectorSet ( A[1], 1, 0, 0 );
			VectorSet ( A[2], 0, 1, 0 );
		}

		// build portal_dest-to-world rotation matrix
		VectorCopy ( r_avertexnormals[ent->skinnum], portal_plane->normal );

		VectorCopy ( portal_plane->normal, B[0] );
		if (B[0][0] || B[0][1])
		{
			VectorSet ( B[1], B[0][1], -B[0][0], 0 );
			VectorNormalizeFast ( B[1] );
			CrossProduct ( B[0], B[1], B[2] );
		}
		else
		{
			VectorSet ( B[1], 1, 0, 0 );
			VectorSet ( B[2], 0, 1, 0 );
		}

		Matrix3_Transpose ( B, Bt );

		// multiply to get world-to-world rotation matrix
		Matrix3_Multiply ( Bt, A, rot );

		if ( ent->frame ) {
			Matrix3_Multiply_Vec3 ( A, vpn, tmp[0] );
			Matrix3_Multiply_Vec3 ( A, vright, tmp[1] );
			Matrix3_Multiply_Vec3 ( A, vup, tmp[2] );

			Matrix3_Rotate (tmp, 5*R_FastSin ( ent->scale + r_newrefdef.time * ent->frame * 0.01f ),
				1, 0, 0);

			Matrix3_Multiply_Vec3 ( Bt, tmp[0], D[0] );
			Matrix3_Multiply_Vec3 ( Bt, tmp[1], D[1] );
			Matrix3_Multiply_Vec3 ( Bt, tmp[2], D[2] );
		} else {
			Matrix3_Multiply_Vec3 ( rot, vpn, D[0] );
			Matrix3_Multiply_Vec3 ( rot, vright, D[1] );
			Matrix3_Multiply_Vec3 ( rot, vup, D[2] );
		}

		// calculate Euler angles for our rotation matrix
		Matrix3_EulerAngles ( D, r_newrefdef.viewangles );

		VectorCopy ( D[0], vpn );
		VectorCopy ( D[1], vright );
		VectorCopy ( D[2], vup );

		// translate view origin
		VectorSubtract ( r_newrefdef.vieworg, ent->origin, tvec );
		Matrix3_Multiply_Vec3 ( rot, tvec, r_newrefdef.vieworg );
		VectorAdd ( r_newrefdef.vieworg, ent->oldorigin, r_newrefdef.vieworg );

		// set up portal_plane
		portal_plane->dist = DotProduct ( ent->oldorigin, portal_plane->normal );
		CategorizePlane ( portal_plane );

		// for portals, vis data is taken from portal origin, not
		// view origin, because the view point moves around and
		// might fly into (or behind) a wall
		r_portalview = true;
		VectorCopy ( ent->oldorigin, r_portalorg );
	}

	qglDepthMask ( GL_TRUE );
	qglDepthFunc ( GL_LEQUAL );
	qglDisable ( GL_POLYGON_OFFSET_FILL );
	qglDisable ( GL_BLEND );

	if ( r_mirrorview ) {
		qglFrontFace( GL_CW );
	}

	R_RenderView ( &r_newrefdef, &r_portallist );

	if ( r_fastsky->value ) {
		R_DrawSky ( NULL );
	}

	qglMatrixMode ( GL_PROJECTION );
	qglLoadMatrixf ( projectionm );

	if ( r_mirrorview ) {
		qglFrontFace( GL_CCW );
	}

	qglMatrixMode ( GL_MODELVIEW );
	qglLoadMatrixf ( modelviewm );

	qglDepthMask( GL_TRUE );
	if ( !r_fastsky->value ) {
		qglClear ( GL_DEPTH_BUFFER_BIT );
	}

	Matrix4_Copy ( modelviewm, r_modelview_matrix );
	Matrix4_Copy ( projectionm, r_projection_matrix );

	memcpy ( &r_newrefdef, &r, sizeof (refdef_t) );
	currentlist = prevlist;

	VectorCopy ( r_newrefdef.vieworg, r_origin );
	VectorCopy ( prev_vpn, vpn );
	VectorCopy ( prev_vright, vright );
	VectorCopy ( prev_vup, vup );

	if ( r_portalview ) {
		r_oldviewcluster = r_viewcluster = -1;	// force markleafs next frame
	}

	r_mirrorview = false;
	r_portalview = false;
}
