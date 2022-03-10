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

meshlist_t	*r_currentlist;

void R_DrawPortalSurface ( meshbuffer_t *mb );
static void R_QSortMeshBuffers ( meshbuffer_t *meshes, int Li, int Ri );
static void R_ISortMeshBuffers ( meshbuffer_t *meshes, int num_meshes );

/*
================
R_CopyMeshBuffer
================
*/
#define R_MBCopy(in,out) \
	(\
		(out).sortkey = (in).sortkey, \
		(out).infokey = (in).infokey, \
		(out).dlightbits = (in).dlightbits, \
		(out).entity = (in).entity, \
		(out).shader = (in).shader, \
		(out).fog = (in).fog \
	)

#define R_MBCmp(mb1,mb2) \
	(\
		(mb1).shader->sort > (mb2).shader->sort ? qtrue : \
		(mb1).shader->sort < (mb2).shader->sort ? qfalse : \
		(mb1).shader > (mb2).shader ? qtrue : \
		(mb1).shader < (mb2).shader ? qfalse : \
		(mb1).sortkey > (mb2).sortkey ? qtrue : \
		(mb1).sortkey < (mb2).sortkey ? qfalse : \
		(mb1).dlightbits > (mb2).dlightbits \
	)

/*
================
R_QSortMeshBuffers

Quicksort
================
*/
static void R_QSortMeshBuffers ( meshbuffer_t *meshes, int Li, int Ri )
{
	int li, ri, stackdepth = 0;
	static meshbuffer_t median, tempbuf;
	static int localstack[QSORT_MAX_STACKDEPTH];

mark0:
	li = Li;
	ri = Ri;

	R_MBCopy ( meshes[(Li+Ri) >> 1], median );

	if ( R_MBCmp (meshes[Li], median) ) {
		if ( R_MBCmp (meshes[Ri], meshes[Li]) ) 
			R_MBCopy ( meshes[Li], median );
	} else if ( R_MBCmp (median, meshes[Ri]) ) {
		R_MBCopy ( meshes[Ri], median );
	}

	while (li < ri)
	{
		while ( R_MBCmp(median, meshes[li]) ) li++;
		while ( R_MBCmp(meshes[ri], median) ) ri--;

		if ( li <= ri ) {
			R_MBCopy( meshes[ri], tempbuf );
			R_MBCopy( meshes[li], meshes[ri] );
			R_MBCopy( tempbuf, meshes[li] );

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
static void R_ISortMeshBuffers ( meshbuffer_t *meshes, int num_meshes )
{
	int i, j;
	static meshbuffer_t tempbuf;

	for ( i = 1; i < num_meshes; i++ )
	{
		R_MBCopy ( meshes[i], tempbuf );
		j = i - 1; 
		
		while ( (j >= 0) && (R_MBCmp(meshes[j], tempbuf) ) ) {
			R_MBCopy ( meshes[j], meshes[j+1] );
			j--;
		}
		if ( i != j+1 ) {
			R_MBCopy ( tempbuf, meshes[j+1] );
		}
	}
}

/*
=================
R_AddMeshToList

Calculate sortkey and store info used for batching and sorting.
All 3D-geometry passes this function.
=================
*/
meshbuffer_t *R_AddMeshToList ( mfog_t *fog, shader_t *shader, int infokey )
{
	int entnumber, fognumber;
	meshbuffer_t *meshbuf;

	if ( !shader ) {
		return NULL;
	}
	if ( shader->sort > SHADER_SORT_OPAQUE ) {
		if ( r_currentlist->num_additive_meshes >= MAX_RENDER_ADDITIVE_MESHES ) {
			return NULL;
		}

		meshbuf = &r_currentlist->meshbuffer_additives[r_currentlist->num_additive_meshes++];
	} else {
		if ( (shader->sort == SHADER_SORT_PORTAL) && (r_mirrorview || r_portalview) )
			return NULL;
		if ( r_currentlist->num_meshes >= MAX_RENDER_MESHES )
			return NULL;

		meshbuf = &r_currentlist->meshbuffer[r_currentlist->num_meshes++];
	}

	if ( shader->flags & SHADER_VIDEOMAP ) {
		Shader_UploadCinematic ( shader );
	}

	meshbuf->shader = shader;
	meshbuf->entity = currententity;
	meshbuf->infokey = infokey;
	meshbuf->dlightbits = 0;
	meshbuf->fog = fog;

	if ( meshbuf->fog ) {
		fognumber = (meshbuf->fog - r_worldbmodel->fogs);
	} else {
		fognumber = -1;
	}

	if ( currententity == &r_worldent ) {
		entnumber = -1;
	} else if ( currententity == &r_polyent ) {
		entnumber = -1;
	} else {
		entnumber = (currententity - r_newrefdef.entities);
	}

	meshbuf->sortkey = ((entnumber+1) << 16) | (fognumber+1);

	if ( infokey > 0 ) {
		msurface_t *surf = &currentmodel->bmodel->surfaces[infokey-1];

		if ( surf->lightmaptexturenum > -1 ) {
			meshbuf->sortkey |= ((surf->lightmaptexturenum+1) << 8);
		}
		if ( surf->dlightbits && (surf->dlightframe == r_framecount) ) {
			meshbuf->dlightbits = surf->dlightbits;
		}
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
	qboolean nextFlare, deformvBulge;
	shader_t *shader;
	msurface_t *surf, *nextSurf;

	currententity = mb->entity;
	currentmodel = currententity->model;
	shader = mb->shader;

	switch ( currententity->rtype )
	{
		case RT_MODEL:
			switch ( currentmodel->type )
			{
			// batched geometry
				case mod_brush:
					if ( shadow ) {
						break;
					}

					surf = mb->infokey > 0 ? &currentmodel->bmodel->surfaces[mb->infokey-1] : NULL;
					nextSurf = ( (nextmb && nextmb->infokey > 0) ? &nextmb->entity->model->bmodel->surfaces[nextmb->infokey-1] : NULL );

					deformvBulge = ( (shader->flags & SHADER_DEFORMV_BULGE) != 0 );
					nextFlare = ( nextSurf && ((shader->flags & SHADER_FLARE) != 0) );

					features = shader->features;
					if ( r_shownormals->value && !shadow ) {
						features |= MF_NORMALS;
					}
					if ( shader->flags & SHADER_AUTOSPRITE ) {
						features |= MF_NOCULL;
					}

					if ( shader->flags & SHADER_FLARE ) {
						R_PushFlareSurf ( mb );
					} else if ( deformvBulge ) {
						R_PushMesh ( surf->mesh, MF_NONBATCHED|features );
					} else {
						R_PushMesh ( surf->mesh, MF_NONE|features );
					}

					if ( !nextmb
						|| nextmb->shader != mb->shader
						|| nextmb->sortkey != mb->sortkey
						|| nextmb->dlightbits != mb->dlightbits
						|| deformvBulge || (nextmb->shader->flags & SHADER_DEFORMV_BULGE)
						|| (nextFlare ? R_SpriteOverflow () : R_BackendOverflow (nextSurf->mesh)) ) {

						if ( shader->flags & SHADER_FLARE )
							R_TranslateForEntity ( mb->entity );
						else if ( currentmodel != r_worldmodel )
							R_RotateForEntity ( mb->entity );
						else
							qglLoadMatrixf ( r_worldview_matrix );

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

				case mod_skeletal:
					R_DrawSkeletalModel ( mb, shadow );
					break;

				default:
					break;
			}
			break;

		case RT_SPRITE:
			if ( shadow ) {
				break;
			}

			R_DrawSpritePoly ( mb );

			if ( !nextmb
				|| nextmb->shader != mb->shader
				|| nextmb->fog != mb->fog
				|| !(shader->flags & SHADER_ENTITY_MERGABLE)
				|| R_SpriteOverflow () ) {
				currententity = &r_worldent;
				currentmodel = r_worldmodel;
				qglLoadMatrixf ( r_worldview_matrix );

				R_RenderMeshBuffer ( mb, shadow );
			}
			break;

		case RT_PORTALSURFACE:
			break;

		case RT_POLY:
			R_PushPoly ( mb );

			if ( !nextmb
				|| nextmb->sortkey != mb->sortkey
				|| nextmb->shader != mb->shader
				|| !(shader->flags & SHADER_ENTITY_MERGABLE)
				|| R_PolyOverflow (nextmb) ) {

				qglLoadMatrixf ( r_worldview_matrix );

				R_DrawPoly ();
			}
			break;
	}
}

/*
================
R_SortMeshList
================
*/
void R_SortMeshes (void)
{
	if ( r_currentlist->num_meshes ) {
		R_QSortMeshBuffers ( r_currentlist->meshbuffer, 0, r_currentlist->num_meshes - 1 );
	}
	if ( r_currentlist->num_additive_meshes ) {
		R_ISortMeshBuffers ( r_currentlist->meshbuffer_additives, r_currentlist->num_additive_meshes );
	}
}

/*
================
R_DrawMeshes
================
*/
void R_DrawMeshes ( qboolean triangleOutlines )
{
	int i;
	meshbuffer_t *meshbuf;
	shader_t *shader;

	currententity = &r_worldent;

	if ( r_currentlist->num_meshes ) {
		meshbuf = r_currentlist->meshbuffer;
		for ( i = 0; i < r_currentlist->num_meshes; i++, meshbuf++ ) {
			shader = meshbuf->shader;

			if ( shader->sort == SHADER_SORT_PORTAL ) {
				if ( !triangleOutlines ) {
					R_DrawPortalSurface ( meshbuf );
				}
			} else if ( shader->flags & SHADER_SKY ) {
				if ( !r_currentlist->skyDrawn ) {
					R_DrawSky ( shader );
					r_currentlist->skyDrawn = qtrue;
				}
				continue;
			}

			if ( i == r_currentlist->num_meshes - 1 ) {
				R_DrawMeshBuffer ( meshbuf, NULL, qfalse );
			} else {
				R_DrawMeshBuffer ( meshbuf, meshbuf+1, qfalse );
			}
		}
	}

	if ( r_currentlist->num_meshes && r_shadows->value ) {
		if( !triangleOutlines ) {
			GL_EnableMultitexture ( qfalse );
			qglDisable (GL_TEXTURE_2D);

			if ( r_shadows->value == 1 ) {
				qglDepthFunc (GL_LEQUAL);
				qglEnable ( GL_BLEND );
				qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				qglColor4f ( 0, 0, 0, bound (0.0f, r_shadows_alpha->value, 1.0f) );

				qglEnable ( GL_STENCIL_TEST );
				qglStencilFunc (GL_EQUAL, 128, 0xFF);
				qglStencilOp (GL_KEEP, GL_KEEP, GL_INCR);
#ifdef SHADOW_VOLUMES
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
#endif
			}

			qglDisable ( GL_ALPHA_TEST );
			qglDepthMask ( GL_FALSE );
		}

		meshbuf = r_currentlist->meshbuffer;
		for ( i = 0; i < r_currentlist->num_meshes; i++, meshbuf++ ) {
			if ( meshbuf->entity->flags & (RF_NOSHADOW|RF_WEAPONMODEL) ) {
				continue;
			}

			if ( i == r_currentlist->num_meshes - 1 ) {
				R_DrawMeshBuffer ( meshbuf, NULL, qtrue );
			} else {
				R_DrawMeshBuffer ( meshbuf, meshbuf+1, qtrue );
			}
		}

		if( !triangleOutlines ) {
			if ( r_shadows->value == 1 ) {
				qglDisable ( GL_STENCIL_TEST );
#ifdef SHADOW_VOLUMES
			} else if ( r_shadows->value == 2 ) {
				qglStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
				qglDisable ( GL_STENCIL_TEST );
				qglColorMask (1, 1, 1, 1);
			} else {
#endif
				qglDisable ( GL_BLEND );
			}

			qglEnable (GL_TEXTURE_2D);
			qglDepthMask (GL_TRUE);
			qglDepthFunc (GL_LEQUAL);
		}
	}

	if ( r_currentlist->num_additive_meshes ) {
		meshbuf = r_currentlist->meshbuffer_additives;
		for ( i = 0; i < r_currentlist->num_additive_meshes; i++, meshbuf++ ) {
			if ( i == r_currentlist->num_additive_meshes - 1 ) {
				R_DrawMeshBuffer ( meshbuf, NULL, qfalse );
			} else {
				R_DrawMeshBuffer ( meshbuf, meshbuf+1, qfalse );
			}
		}
	}

	qglLoadMatrixf ( r_worldview_matrix );

	GL_EnableMultitexture ( qfalse );
	qglDisable ( GL_POLYGON_OFFSET_FILL );
	qglDisable ( GL_ALPHA_TEST );
	qglDisable ( GL_BLEND );
	qglDepthFunc ( GL_LEQUAL );

	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDepthMask ( GL_TRUE );
}

/*
===============
R_DrawTriangleOutlines
===============
*/
void R_DrawTriangleOutlines (void)
{
	if ( !r_showtris->value && !r_shownormals->value ) {
		return;
	}

	R_BackendBeginTriangleOutlines ();
	R_DrawMeshes ( qtrue );
	R_BackendEndTriangleOutlines ();
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
	mat4_t worldviewm, projectionm;
	vec3_t v[3], prev_vpn, prev_vright, prev_vup;
	entity_t *ent;
	mesh_t *mesh;
	model_t *model;
	msurface_t *surf;
	cplane_t *portal_plane = &r_clipplane;

	if ( !mb->entity || !(model = mb->entity->model) || !model->bmodel ) {
		return;
	}

	surf = mb->infokey > 0 ? &model->bmodel->surfaces[mb->infokey-1] : NULL;
	if ( !surf || !(mesh = surf->mesh) || !mesh->xyz_array ) {
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
		if ( ent->rtype == RT_PORTALSURFACE ) {
			d = PlaneDiff ( ent->origin, portal_plane );
			if ( (d >= -64) && (d <= 64) ) {
				break;
			}
		}
	}

	if ( i == r_newrefdef.num_entities ) {
		return;
	}

	prevlist = r_currentlist;
	memcpy ( &r, &r_newrefdef, sizeof (refdef_t) );
	VectorCopy ( vpn, prev_vpn );
	VectorCopy ( vright, prev_vright );
	VectorCopy ( vup, prev_vup );

	Matrix4_Copy ( r_worldview_matrix, worldviewm );
	Matrix4_Copy ( r_projection_matrix, projectionm );

	r_portallist.skyDrawn = qfalse;
	r_portallist.num_meshes = 0;
	r_portallist.num_additive_meshes = 0;

	if ( VectorCompare (ent->origin, ent->oldorigin) )
	{	// mirror
		mat3_t M;

		d = -2 * (DotProduct (r_origin, portal_plane->normal) - portal_plane->dist);
		VectorMA (r_origin, d, portal_plane->normal, r_newrefdef.vieworg);

		d = -2 * DotProduct ( vpn, portal_plane->normal );
		VectorMA ( vpn, d, portal_plane->normal, M[0] );
		VectorNormalize ( M[0] );

		d = -2 * DotProduct ( vright, portal_plane->normal );
		VectorMA ( vright, d, portal_plane->normal, M[1] );
		VectorNormalize ( M[1] );

		d = -2 * DotProduct ( vup, portal_plane->normal );
		VectorMA ( vup, d, portal_plane->normal, M[2] );
		VectorNormalize ( M[2] );

		Matrix3_EulerAngles ( M, r_newrefdef.viewangles );
		r_newrefdef.viewangles[ROLL] = -r_newrefdef.viewangles[ROLL]; 

		r_mirrorview = qtrue;
	}
	else
	{	// portal
		mat3_t A, B, Bt, rot, tmp, D;
		vec3_t tvec;

		if ( mb->shader->flags & SHADER_AGEN_PORTAL ) {
			
			VectorSubtract ( mesh->xyz_array[0], r_origin, tvec );

			for ( i = 0; i < mb->shader->numpasses; i++ ) {
				if ( mb->shader->passes[i].alphagen.type != ALPHA_GEN_PORTAL ) {
					continue;
				}

				if ( !r_fastsky->value && (VectorLength(tvec) > (1.0/mb->shader->passes[i].alphagen.args[0])) ) {
					return;
				}
			}
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
		ByteToDir ( ent->skinnum, portal_plane->normal );

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
		r_portalview = qtrue;
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

	qglMatrixMode ( GL_PROJECTION );
	qglLoadMatrixf ( projectionm );

	if ( r_mirrorview ) {
		qglFrontFace( GL_CCW );
	}

	qglMatrixMode ( GL_MODELVIEW );
	qglLoadMatrixf ( worldviewm );

	qglDepthMask( GL_TRUE );
	if ( !r_fastsky->value ) {
		qglClear ( GL_DEPTH_BUFFER_BIT );
	}

	Matrix4_Copy ( worldviewm, r_worldview_matrix );
	Matrix4_Copy ( projectionm, r_projection_matrix );

	memcpy ( &r_newrefdef, &r, sizeof (refdef_t) );
	r_currentlist = prevlist;

	VectorCopy ( r_newrefdef.vieworg, r_origin );
	VectorCopy ( prev_vpn, vpn );
	VectorCopy ( prev_vright, vright );
	VectorCopy ( prev_vup, vup );

	if ( r_portalview ) {
		r_oldviewcluster = r_viewcluster = -1;	// force markleafs next frame
	}

	r_mirrorview = qfalse;
	r_portalview = qfalse;
}
