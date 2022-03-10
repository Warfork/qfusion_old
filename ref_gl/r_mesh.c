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

#define		QSORT_MAX_STACKDEPTH	2048

float		r_flarescale;

meshlist_t	*r_currentlist;

static void R_QSortMeshBuffers( meshbuffer_t *meshes, int Li, int Ri );
static void R_ISortMeshBuffers( meshbuffer_t *meshes, int num_meshes );

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
		(mb1).shader->sortkey > (mb2).shader->sortkey ? qtrue : \
		(mb1).shader->sortkey < (mb2).shader->sortkey ? qfalse : \
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
static void R_QSortMeshBuffers( meshbuffer_t *meshes, int Li, int Ri )
{
	int li, ri, stackdepth = 0, total = Ri + 1;
	meshbuffer_t median, tempbuf;
	int lstack[QSORT_MAX_STACKDEPTH], rstack[QSORT_MAX_STACKDEPTH];

mark0:
	if( Ri - Li > 8 ) {
		li = Li;
		ri = Ri;

		R_MBCopy( meshes[(Li+Ri) >> 1], median );

		if( R_MBCmp( meshes[Li], median ) ) {
			if( R_MBCmp( meshes[Ri], meshes[Li] ) ) 
				R_MBCopy( meshes[Li], median );
		} else if( R_MBCmp( median, meshes[Ri] ) ) {
			R_MBCopy( meshes[Ri], median );
		}

		do {
			while( R_MBCmp( median, meshes[li] ) ) li++;
			while( R_MBCmp( meshes[ri], median ) ) ri--;

			if( li <= ri ) {
				R_MBCopy( meshes[ri], tempbuf );
				R_MBCopy( meshes[li], meshes[ri] );
				R_MBCopy( tempbuf, meshes[li] );

				li++;
				ri--;
			}
		} while( li < ri );

		if( (Li < ri) && (stackdepth < QSORT_MAX_STACKDEPTH) ) {
			lstack[stackdepth] = li;
			rstack[stackdepth] = Ri;
			stackdepth++;
			li = Li;
			Ri = ri;
			goto mark0;
		}

		if( li < Ri ) {
			Li = li;
			goto mark0;
		}
	}
	if( stackdepth ) {
		--stackdepth;
		Ri = ri = rstack[stackdepth];
		Li = li = lstack[stackdepth];
		goto mark0;
	}

	for( li = 1; li < total; li++ ) {
		R_MBCopy( meshes[li], tempbuf );
		ri = li - 1;

		while( (ri >= 0) && (R_MBCmp( meshes[ri], tempbuf )) ) {
			R_MBCopy( meshes[ri], meshes[ri+1] );
			ri--;
		}
		if( li != ri+1 )
			R_MBCopy( tempbuf, meshes[ri+1] );
	}
}

/*
================
R_ISortMeshes

Insertion sort
================
*/
static void R_ISortMeshBuffers( meshbuffer_t *meshes, int num_meshes )
{
	int i, j;
	meshbuffer_t tempbuf;

	for( i = 1; i < num_meshes; i++ ) {
		R_MBCopy( meshes[i], tempbuf );
		j = i - 1;

		while( (j >= 0) && (R_MBCmp( meshes[j], tempbuf )) ) {
			R_MBCopy( meshes[j], meshes[j+1] );
			j--;
		}
		if( i != j+1 )
			R_MBCopy( tempbuf, meshes[j+1] );
	}
}

/*
=================
R_AddMeshToList

Calculate sortkey and store info used for batching and sorting.
All 3D-geometry passes this function.
=================
*/
meshbuffer_t *R_AddMeshToList( int type, mfog_t *fog, shader_t *shader, int infokey )
{
	meshbuffer_t *meshbuf;

	if( !shader )
		return NULL;

	if( shader->sort > SHADER_SORT_OPAQUE ) {
		if( r_currentlist->num_additive_meshes >= MAX_RENDER_ADDITIVE_MESHES )
			return NULL;
		meshbuf = &r_currentlist->meshbuffer_additives[r_currentlist->num_additive_meshes++];
	} else {
		if( (shader->sort == SHADER_SORT_PORTAL) && (r_mirrorview || r_portalview) )
			return NULL;
		if( r_currentlist->num_meshes >= MAX_RENDER_MESHES )
			return NULL;
		meshbuf = &r_currentlist->meshbuffer[r_currentlist->num_meshes++];
	}

	if( shader->flags & SHADER_VIDEOMAP )
		R_UploadCinematicShader( shader );

	if( fog )
		meshbuf->sortkey = ((int)(currententity - r_entities) << 19) | (((int)(fog - r_worldmodel->fogs)+1) << 2) | type;
	else
		meshbuf->sortkey = ((int)(currententity - r_entities) << 19) | type;
	meshbuf->shader = shader;
	meshbuf->entity = currententity;
	meshbuf->infokey = infokey;
	meshbuf->dlightbits = 0;
	meshbuf->fog = fog;

	if( infokey > 0 ) {
		msurface_t *surf = &currentmodel->surfaces[infokey-1];

		meshbuf->sortkey |= ((surf->superLightStyle+1) << 9);
		if( surf->dlightbits && (surf->dlightframe == r_framecount) )
			meshbuf->dlightbits = surf->dlightbits;
	}

#if (defined(ENDIAN_BIG) || !defined(ENDIAN_LITTLE))
	meshbuf->sortkey = LittleLong( meshbuf->sortkey );
	meshbuf->dlightbits = LittleLong( meshbuf->dlightbits );
#endif

	return meshbuf;
}

/*
================
R_BatchMeshBuffer

Draw the mesh or batch it.
================
*/
static inline void R_BatchMeshBuffer( const meshbuffer_t *mb, const meshbuffer_t *nextmb, qboolean shadow )
{
	int type, features;
	qboolean nextFlare, deformvBulge;
	shader_t *shader;
	msurface_t *surf, *nextSurf;

	currententity = mb->entity;
	if( shadow && (currententity->flags & (RF_NOSHADOW|RF_WEAPONMODEL)) )
		return;

	type = mb->sortkey & 3;
	currentmodel = currententity->model;
	shader = mb->shader;

	switch( type ) {
		case MB_MODEL:
			switch( currentmodel->type ) {
			// batched geometry
				case mod_brush:
					if( shadow )
						break;
					surf = &currentmodel->surfaces[mb->infokey-1];
					nextSurf = ( (nextmb && nextmb->infokey > 0) ? &nextmb->entity->model->surfaces[nextmb->infokey-1] : NULL );

					deformvBulge = ( (shader->flags & SHADER_DEFORMV_BULGE) != 0 );
					nextFlare = ( nextSurf && ((shader->flags & SHADER_FLARE) != 0) );

					features = shader->features;
					if( r_shownormals->integer && !shadow )
						features |= MF_NORMALS;
					if( shader->flags & SHADER_AUTOSPRITE )
						features |= MF_NOCULL;
					features |= r_superLightStyles[surf->superLightStyle].features;

					if( shader->flags & SHADER_FLARE )
						R_PushFlareSurf( mb );
					else if( deformvBulge )
						R_PushMesh( surf->mesh, MF_NONBATCHED|features );
					else
						R_PushMesh( surf->mesh, features );

					if( (features & MF_NONBATCHED)
						|| !nextmb
						|| nextmb->sortkey != mb->sortkey
						|| nextmb->shader != mb->shader
						|| nextmb->dlightbits != mb->dlightbits
						|| deformvBulge || (nextmb->shader->flags & SHADER_DEFORMV_BULGE)
						|| (nextFlare ? R_SpriteOverflow () : R_MeshOverflow( nextSurf->mesh )) ) {

						if( (currentmodel != r_worldmodel) && !(shader->flags & SHADER_FLARE) )
							R_RotateForEntity( currententity );
						else
							R_LoadIdentity ();
						R_RenderMeshBuffer( mb, shadow );
					}
					break;
			// non-batched geometry
				case mod_alias:
					R_DrawAliasModel( mb, shadow );
					break;
				case mod_sprite:
					if( !shadow )
						R_DrawSpriteModel( mb );
					break;
				case mod_skeletal:
					R_DrawSkeletalModel( mb, shadow );
					break;
			}
			break;
		case MB_SPRITE:
			if( shadow )
				break;

			R_DrawSpritePoly( mb );

			if( !nextmb
				|| nextmb->shader != mb->shader
				|| nextmb->fog != mb->fog
				|| !(shader->flags & SHADER_ENTITY_MERGABLE)
				|| R_SpriteOverflow () ) {
				currententity = &r_worldent;
				currentmodel = r_worldmodel;

				R_LoadIdentity ();
				R_RenderMeshBuffer( mb, shadow );
			}
			break;
		case MB_POLY:
			if( shadow )
				break;

			R_PushPoly( mb );

			if( !nextmb
				|| nextmb->sortkey != mb->sortkey
				|| nextmb->shader != mb->shader
				|| !(shader->flags & SHADER_ENTITY_MERGABLE)
				|| R_PolyOverflow( nextmb ) ) {

				R_LoadIdentity ();
				R_DrawPoly( mb );
			}
			break;
	}
}

/*
================
R_SortMeshList
================
*/
void R_SortMeshes( void )
{
	if( r_draworder->integer )
		return;

	if( r_currentlist->num_meshes )
		R_QSortMeshBuffers( r_currentlist->meshbuffer, 0, r_currentlist->num_meshes - 1 );
	if( r_currentlist->num_additive_meshes )
		R_ISortMeshBuffers( r_currentlist->meshbuffer_additives, r_currentlist->num_additive_meshes );
}

/*
================
R_DrawMeshes
================
*/
void R_DrawMeshes( qboolean triangleOutlines )
{
	int i;
	meshbuffer_t *meshbuf;
	shader_t *shader;

	currententity = &r_worldent;

	if( r_currentlist->num_meshes ) {
		meshbuf = r_currentlist->meshbuffer;
		for( i = 0; i < r_currentlist->num_meshes - 1; i++, meshbuf++ ) {
			shader = meshbuf->shader;

			if( shader->flags & SHADER_SKY ) {
				if( !r_currentlist->skyDrawn ) {
					R_DrawSky( shader );
					r_currentlist->skyDrawn = qtrue;
				}
			} else {
				if( shader->sort == SHADER_SORT_PORTAL ) {
					if( !triangleOutlines )
						R_DrawPortalSurface( meshbuf );
				}
				R_BatchMeshBuffer( meshbuf, meshbuf+1, qfalse );
			}
		}

		shader = meshbuf->shader;
		if( shader->flags & SHADER_SKY ) {
			if( !r_currentlist->skyDrawn ) {
				R_DrawSky( shader );
				r_currentlist->skyDrawn = qtrue;
			}
		} else {
			if( shader->sort == SHADER_SORT_PORTAL ) {
				if( !triangleOutlines )
					R_DrawPortalSurface( meshbuf );
			}
			R_BatchMeshBuffer( meshbuf, NULL, qfalse );
		}
	}

#if SHADOW_VOLUMES
	if( r_currentlist->num_meshes && r_shadows->integer ) {
#else
	if( r_currentlist->num_meshes && r_shadows->integer == 1 ) {
#endif
		if( !triangleOutlines ) {
			qglDisable( GL_TEXTURE_2D );

			if( r_shadows->integer == 1 ) {
				qglDepthFunc( GL_LEQUAL );
				qglEnable( GL_BLEND );
				qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				qglColor4f( 0, 0, 0, bound( 0.0f, r_shadows_alpha->value, 1.0f ) );

				qglEnable( GL_STENCIL_TEST );
				qglStencilFunc( GL_EQUAL, 128, 0xFF );
				qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
#if SHADOW_VOLUMES
			} else if( r_shadows->integer == SHADOW_VOLUMES ) {
				qglEnable( GL_STENCIL_TEST );
				qglEnable( GL_CULL_FACE );
				qglEnable( GL_BLEND );

				qglColor4f( 1, 1, 1, 1 );
				qglColorMask( 0, 0, 0, 0 );
				qglDepthFunc( GL_LESS );
				qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
				qglStencilFunc( GL_ALWAYS, 128, 0xFF );
			} else {
				qglDisable( GL_STENCIL_TEST );
				qglDisable( GL_CULL_FACE );
				qglEnable( GL_BLEND );

				qglBlendFunc( GL_ONE, GL_ONE );
				qglDepthFunc( GL_LEQUAL );

				qglColor3f( 1.0, 0.1, 0.1 );
#endif
			}
			qglDisable( GL_ALPHA_TEST );
			qglDepthMask( GL_FALSE );
		}

		meshbuf = r_currentlist->meshbuffer;
		for( i = 0; i < r_currentlist->num_meshes - 1; i++, meshbuf++ )
			R_BatchMeshBuffer( meshbuf, meshbuf+1, qtrue );
		R_BatchMeshBuffer( meshbuf, NULL, qtrue );

		if( !triangleOutlines ) {
			if( r_shadows->integer == 1 ) {
				qglDisable( GL_STENCIL_TEST );
#if SHADOW_VOLUMES
			} else if ( r_shadows->integer == SHADOW_VOLUMES ) {
				qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
				qglDisable( GL_STENCIL_TEST );
				qglColorMask( 1, 1, 1, 1 );
			} else {
#endif
				qglDisable( GL_BLEND );
			}

			qglEnable( GL_TEXTURE_2D );
			qglDepthMask( GL_TRUE );
			qglDepthFunc( GL_LEQUAL );
		}
	}

	if( r_currentlist->num_additive_meshes ) {
		meshbuf = r_currentlist->meshbuffer_additives;
		for( i = 0; i < r_currentlist->num_additive_meshes - 1; i++, meshbuf++ )
			R_BatchMeshBuffer( meshbuf, meshbuf + 1, qfalse );
		R_BatchMeshBuffer( meshbuf, NULL, qfalse );
	}

	R_LoadIdentity ();

	qglDisable( GL_POLYGON_OFFSET_FILL );
	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_BLEND );
	qglDepthFunc( GL_LEQUAL );

	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDepthMask( GL_TRUE );
}

/*
===============
R_DrawTriangleOutlines
===============
*/
void R_DrawTriangleOutlines( void )
{
	if( !r_showtris->integer && !r_shownormals->integer )
		return;

	R_BackendBeginTriangleOutlines ();
	R_DrawMeshes( qtrue );
	R_BackendEndTriangleOutlines ();
}

/*
===============
R_ScissorForPortal
===============
*/
qboolean R_ScissorForPortal( entity_t *ent, msurface_t *surf )
{
	int i;
	int ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2;

	x1 = y1 = 999999;
	x2 = y2 = -999999;
	for( i = 0; i < 8; i++ ) {	// compute and rotate a full bounding box
		vec3_t v;
		vec3_t tmp, corner;

		tmp[0] = ( ( i & 1 ) ? surf->mins[0] : surf->maxs[0] );
		tmp[1] = ( ( i & 2 ) ? surf->mins[1] : surf->maxs[1] );
		tmp[2] = ( ( i & 4 ) ? surf->mins[2] : surf->maxs[2] );

		Matrix_TransformVector( ent->axis, tmp, corner );
		VectorMA( ent->origin, ent->scale, corner, corner );
		R_TransformToScreen_Vec3( corner, v );

		x1 = min( x1, v[0] ); y1 = min( y1, v[1] );
		x2 = max( x2, v[0] ); y2 = max( y2, v[1] );
	}

	ix1 = max( x1 - 1.0f, r_refdef.x ); ix2 = min( x2 + 1.0f, r_refdef.x + r_refdef.width );
	if( ix1 >= ix2 )
//		return qfalse;
		return qtrue;		// FIXME

	iy1 = max( y1 - 1.0f, r_refdef.y ); iy2 = min( y2 + 1.0f, r_refdef.y + r_refdef.height );
	if( iy1 >= iy2 )
//		return qfalse;
		return qtrue;		// FIXME

	qglScissor( ix1, iy1, ix2 - ix1, iy2 - iy1 );

	return qtrue;
}

/*
===============
R_DrawPortalSurface
===============
*/
static meshlist_t r_portallist;

void R_DrawPortalSurface( meshbuffer_t *mb )
{
	int i;
	float dist,	d;
	meshlist_t *prevlist;
	refdef_t r;
	mat4x4_t worldviewm, projectionm, modelviewm;
	vec3_t v[3], prev_vpn, prev_vright, prev_vup, entity_rotation[3];
	entity_t *ent;
	mesh_t *mesh;
	model_t *model;
	msurface_t *surf;
	cplane_t *portal_plane = &r_clipplane, original_plane;

	if( !(ent = mb->entity) || !(model = ent->model) )
		return;

	surf = mb->infokey > 0 ? &model->surfaces[mb->infokey-1] : NULL;
	if( !surf || !(mesh = surf->mesh) || !mesh->xyzArray )
		return;

	if( !R_ScissorForPortal( ent, surf ) )
		return;

	VectorCopy( mesh->xyzArray[mesh->indexes[0]], v[0] );
	VectorCopy( mesh->xyzArray[mesh->indexes[1]], v[1] );
	VectorCopy( mesh->xyzArray[mesh->indexes[2]], v[2] );
	PlaneFromPoints( v, &original_plane );
	original_plane.dist += DotProduct( ent->origin, original_plane.normal );
	CategorizePlane( &original_plane );

	Matrix_Transpose( ent->axis, entity_rotation );
	Matrix_TransformVector( entity_rotation, mesh->xyzArray[mesh->indexes[0]], v[0] ); VectorMA( ent->origin, ent->scale, v[0], v[0] );
	Matrix_TransformVector( entity_rotation, mesh->xyzArray[mesh->indexes[1]], v[1] ); VectorMA( ent->origin, ent->scale, v[1], v[1] );
	Matrix_TransformVector( entity_rotation, mesh->xyzArray[mesh->indexes[2]], v[2] ); VectorMA( ent->origin, ent->scale, v[2], v[2] );
	PlaneFromPoints( v, portal_plane );
	CategorizePlane( portal_plane );

	if( ( dist = PlaneDiff( r_origin, portal_plane ) ) <= BACKFACE_EPSILON )
		return;

	for( i = 1, ent = r_entities; i < r_numEntities; i++, ent++ ) {
		if( ent->rtype == RT_PORTALSURFACE ) {
			d = PlaneDiff( ent->origin, &original_plane );
			if( (d >= -64) && (d <= 64) ) {
				ent->rtype = -1;
				break;
			}
		}
	}

	if( i == r_numEntities )
		return;

	memcpy( &r, &r_refdef, sizeof (refdef_t) );
	prevlist = r_currentlist;
	VectorCopy( vpn, prev_vpn );
	VectorCopy( vright, prev_vright );
	VectorCopy( vup, prev_vup );

	Matrix4_Copy( r_worldview_matrix, worldviewm );
	Matrix4_Copy( r_projection_matrix, projectionm );
	Matrix4_Copy( r_modelview_matrix, modelviewm );

	r_portallist.skyDrawn = qfalse;
	r_portallist.num_meshes = 0;
	r_portallist.num_additive_meshes = 0;

	if( VectorCompare( ent->origin, ent->oldorigin ) ) {	// mirror
		vec3_t M[3];

		d = -2 * (DotProduct( r_origin, portal_plane->normal ) - portal_plane->dist);
		VectorMA( r_origin, d, portal_plane->normal, r_refdef.vieworg );

		d = -2 * DotProduct( vpn, portal_plane->normal );
		VectorMA( vpn, d, portal_plane->normal, M[0] );
		VectorNormalize( M[0] );

		d = -2 * DotProduct( vright, portal_plane->normal );
		VectorMA ( vright, d, portal_plane->normal, M[1] );
		VectorNormalize( M[1] );

		d = -2 * DotProduct( vup, portal_plane->normal );
		VectorMA( vup, d, portal_plane->normal, M[2] );
		VectorNormalize( M[2] );

		Matrix_EulerAngles( M, r_refdef.viewangles );
		r_refdef.viewangles[ROLL] = -r_refdef.viewangles[ROLL]; 

		r_mirrorview = qtrue;
	} else {		// portal
		vec3_t tvec;
		vec3_t A[3], B[3], Bt[3], rot[3], tmp[3], D[3];

		if( mb->shader->flags & SHADER_AGEN_PORTAL ) {
			VectorSubtract( mesh->xyzArray[0], r_origin, tvec );

			for( i = 0; i < mb->shader->numpasses; i++ ) {
				if( mb->shader->passes[i].alphagen.type != ALPHA_GEN_PORTAL )
					continue;
				if( !r_fastsky->integer && (VectorLength(tvec) > (1.0/mb->shader->passes[i].alphagen.args[0])) )
					return;
			}
		}

		// build world-to-portal rotation matrix
		VectorNegate( portal_plane->normal, A[0] );
        if( A[0][0] || A[0][1] ) {
			VectorSet( A[1], A[0][1], -A[0][0], 0 );
			VectorNormalize( A[1] );
			CrossProduct( A[0], A[1], A[2] );
		} else {
			VectorSet( A[1], 1, 0, 0 );
			VectorSet( A[2], 0, 1, 0 );
		}

		// build portal_dest-to-world rotation matrix
		ByteToDir( ent->skinnum, portal_plane->normal );

		VectorCopy( portal_plane->normal, B[0] );
		if( B[0][0] || B[0][1] ) {
			VectorSet( B[1], B[0][1], -B[0][0], 0 );
			VectorNormalize( B[1] );
			CrossProduct( B[0], B[1], B[2] );
		} else {
			VectorSet( B[1], 1, 0, 0 );
			VectorSet( B[2], 0, 1, 0 );
		}

		Matrix_Transpose( B, Bt );

		// multiply to get world-to-world rotation matrix
		Matrix_Multiply( Bt, A, rot );

		if( ent->frame ) {
			Matrix_TransformVector( A, vpn, tmp[0] );
			Matrix_TransformVector( A, vright, tmp[1] );
			Matrix_TransformVector( A, vup, tmp[2] );
			Matrix_Rotate( tmp, 5 * R_FastSin( ent->scale + r_refdef.time * ent->frame * 0.01f ), 1, 0, 0 );
			Matrix_TransformVector( Bt, tmp[0], D[0] );
			Matrix_TransformVector( Bt, tmp[1], D[1] );
			Matrix_TransformVector( Bt, tmp[2], D[2] );
		} else {
			Matrix_TransformVector( rot, vpn, D[0] );
			Matrix_TransformVector( rot, vright, D[1] );
			Matrix_TransformVector( rot, vup, D[2] );
		}

		// calculate Euler angles for our rotation matrix
		Matrix_EulerAngles( D, r_refdef.viewangles );

		VectorCopy( D[0], vpn );
		VectorCopy( D[1], vright );
		VectorCopy( D[2], vup );

		// translate view origin
		VectorSubtract( r_refdef.vieworg, ent->origin, tvec );
		Matrix_TransformVector( rot, tvec, r_refdef.vieworg );
		VectorAdd( r_refdef.vieworg, ent->oldorigin, r_refdef.vieworg );

		// set up portal_plane
		portal_plane->dist = DotProduct( ent->oldorigin, portal_plane->normal );
		CategorizePlane( portal_plane );

		// for portals, vis data is taken from portal origin, not
		// view origin, because the view point moves around and
		// might fly into (or behind) a wall
		r_portalview = qtrue;
		VectorCopy( ent->oldorigin, r_portalorg );
	}

	qglDepthMask( GL_TRUE );
	qglDepthFunc( GL_LEQUAL );
	qglDisable( GL_POLYGON_OFFSET_FILL );
	qglDisable( GL_BLEND );
	if( r_mirrorview )
		qglFrontFace( GL_CW );

	R_RenderView( &r_refdef, &r_portallist );

	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( projectionm );
	if( r_mirrorview )
		qglFrontFace( GL_CCW );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadMatrixf( worldviewm );

	qglDepthMask( GL_TRUE );
	if( !r_fastsky->integer )
		qglClear( GL_DEPTH_BUFFER_BIT );

	Matrix4_Copy( worldviewm, r_worldview_matrix );
	Matrix4_Copy( projectionm, r_projection_matrix );
	Matrix4_Copy( modelviewm, r_modelview_matrix );

	memcpy ( &r_refdef, &r, sizeof (refdef_t) );
	qglScissor( r_refdef.x, glState.height - r_refdef.height - r_refdef.y, r_refdef.width, r_refdef.height );

	r_currentlist = prevlist;
	VectorCopy( r_refdef.vieworg, r_origin );
	VectorCopy( prev_vpn, vpn );
	VectorCopy( prev_vright, vright );
	VectorCopy( prev_vup, vup );

	if( r_portalview )
		r_oldviewcluster = r_viewcluster = -1;	// force markleafs next frame

	r_mirrorview = qfalse;
	r_portalview = qfalse;
}

/*
===============
R_DrawCubemapView
===============
*/
void R_DrawCubemapView( vec3_t origin, vec3_t angles, int size )
{
	r_refdef = r_lastRefdef;
	r_refdef.time = 0;
	r_refdef.x = r_refdef.y = 0;
	r_refdef.width = size;
	r_refdef.height = size;
	r_refdef.fov_x = 90;
	r_refdef.fov_y = 90;
	VectorCopy( origin, r_refdef.vieworg );
	VectorCopy( angles, r_refdef.viewangles );

	r_numPolys = 0;
	r_numDlights = 0;

	R_RenderScene( &r_refdef );

	r_oldviewcluster = r_viewcluster = -1;	// force markleafs next frame
}

/*
===============
R_BuildTangentVectors
===============
*/
void R_BuildTangentVectors( int numVertexes, vec3_t *xyzArray, vec2_t *stArray, int numTris, index_t *indexes, vec3_t *sVectorsArray, vec3_t *tVectorsArray )
{
	int i, j;
	float d, *v[3], *tc[3];
	vec3_t stvec[3], normal;

	// assuming arrays have already been allocated
	// this also does some nice precaching
	memset( sVectorsArray, 0, numVertexes * sizeof( *sVectorsArray ) );
	memset( tVectorsArray, 0, numVertexes * sizeof( *tVectorsArray ) );

	for( i = 0; i < numTris; i++, indexes += 3 ) {
		for( j = 0; j < 3; j++ ) {
			v[j] = ( float * )( xyzArray + indexes[j] );
			tc[j] = ( float * )( stArray + indexes[j] );
		}

		// calculate two mostly perpendicular edge directions
		VectorSubtract( v[0], v[1], stvec[0] );
		VectorSubtract( v[2], v[1], stvec[1] );

		// we have two edge directions, we can calculate the normal then
		CrossProduct( stvec[0], stvec[1], normal );
		VectorNormalize( normal );

		for( j = 0; j < 3; j++ ) {
			stvec[0][j] = ((tc[1][1] - tc[0][1]) * (v[2][j] - v[0][j]) - (tc[2][1] - tc[0][1]) * (v[1][j] - v[0][j]));
			stvec[1][j] = ((tc[1][0] - tc[0][0]) * (v[2][j] - v[0][j]) - (tc[2][0] - tc[0][0]) * (v[1][j] - v[0][j]));
		}

		// keep s\t vectors orthogonal
		for( j = 0; j < 2; j++ ) {
			d = -DotProduct( stvec[j], normal );
			VectorMA( stvec[j], d, normal, stvec[j] );
			VectorNormalize( stvec[j] );
		}

		// inverse tangent vectors if needed
		CrossProduct( stvec[1], stvec[0], stvec[2] );
		if( DotProduct( stvec[2], normal ) < 0 ) {
			VectorInverse( stvec[0] );
			VectorInverse( stvec[1] );
		}

		for( j = 0; j < 3; j++ ) {
			VectorAdd( sVectorsArray[indexes[j]], stvec[0], sVectorsArray[indexes[j]] );
			VectorAdd( tVectorsArray[indexes[j]], stvec[1], tVectorsArray[indexes[j]] );
		}
	}

	// normalize
	for( i = 0; i < numVertexes; i++ ) {
		VectorNormalize( sVectorsArray[i] );
		VectorNormalize( tVectorsArray[i] );
	}
}
