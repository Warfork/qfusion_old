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

static mempool_t *r_meshlistmempool;

meshlist_t		r_worldlist;
static meshlist_t r_portallist;

static qboolean	r_shadowPass;
static qboolean r_triangleOutlines;

static void R_QSortMeshBuffers( meshbuffer_t *meshes, int Li, int Ri );
static void R_ISortMeshBuffers( meshbuffer_t *meshes, int num_meshes );

#define R_MBCopy(in,out) \
	(\
		(out).sortkey = (in).sortkey, \
		(out).infokey = (in).infokey, \
		(out).dlightbits = (in).dlightbits, \
		(out).shaderkey = (in).shaderkey \
	)

#define R_MBCmp(mb1,mb2) \
	(\
		(mb1).shaderkey > (mb2).shaderkey ? qtrue : \
		(mb1).shaderkey < (mb2).shaderkey ? qfalse : \
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
	int i, oldsize;
	meshlist_t *list;
	meshbuffer_t *meshbuf, *oldmb;

	if( !shader )
		return NULL;

	list = ri.meshlist;
	if( shader->sort > SHADER_SORT_OPAQUE ) {
		if( list->num_translucent_meshes >= list->max_translucent_meshes ) {	// reallocate if needed
			oldmb = list->meshbuffer_translucent;
			oldsize = list->max_translucent_meshes;

			list->max_translucent_meshes = max( MIN_RENDER_MESHES/2, oldsize * 2 );
			list->meshbuffer_translucent = Mem_Alloc( r_meshlistmempool, list->max_translucent_meshes * sizeof( meshbuffer_t ) );
			if( oldmb ) {
				memcpy( list->meshbuffer_translucent, oldmb, oldsize * sizeof( meshbuffer_t ) );
				Mem_Free( oldmb );
			}

			// NULL all pointers to old membuffers so we don't crash
			if( r_worldmodel ) {
				for( i = 0; i < r_worldmodel->numsurfaces; i++ )
					r_worldmodel->surfaces[i].meshBuffer = NULL;
			}
		}

		meshbuf = &list->meshbuffer_translucent[list->num_translucent_meshes++];
	} else {
		if( (shader->sort == SHADER_SORT_PORTAL) && (ri.params & (RP_MIRRORVIEW|RP_PORTALVIEW|RP_SKYPORTALVIEW)) )
			return NULL;

		if( list->num_meshes >= list->max_meshes ) {	// reallocate if needed
			oldmb = list->meshbuffer;
			oldsize = list->max_meshes;

			list->max_meshes = max( MIN_RENDER_MESHES, oldsize * 2 );
			list->meshbuffer = Mem_Alloc( r_meshlistmempool, list->max_meshes * sizeof( meshbuffer_t ) );
			if( oldmb ) {
				memcpy( list->meshbuffer, oldmb, oldsize * sizeof( meshbuffer_t ) );
				Mem_Free( oldmb );
			}

			// NULL all pointers to old membuffers so we don't crash
			if( r_worldmodel ) {
				for( i = 0; i < r_worldmodel->numsurfaces; i++ )
					r_worldmodel->surfaces[i].meshBuffer = NULL;
			}
		}

		meshbuf = &list->meshbuffer[list->num_meshes++];
	}

	if( shader->flags & SHADER_VIDEOMAP )
		R_UploadCinematicShader( shader );

	meshbuf->sortkey = MB_ENTITY2NUM( ri.currententity ) | MB_FOG2NUM( fog ) | type;
	meshbuf->shaderkey = shader->sortkey;
	meshbuf->infokey = infokey;
	meshbuf->dlightbits = 0;

	return meshbuf;
}

/*
================
R_BatchMeshBuffer

Draw the mesh or batch it.
================
*/
static void R_BatchMeshBuffer( const meshbuffer_t *mb, const meshbuffer_t *nextmb )
{
	int type, features;
	qboolean nonMergable;
	shader_t *shader;
	msurface_t *surf, *nextSurf;
	entity_t *ent, *nextEnt;

	MB_NUM2ENTITY( mb->sortkey, ent );

	if( r_shadowPass && (ent->flags & (RF_NOSHADOW|RF_WEAPONMODEL)) )
		return;

	if( ri.currententity != ent ) {
		ri.previousentity = ri.currententity;
		ri.currententity = ent;
		ri.currentmodel = ent->model;
	}

	type = mb->sortkey & 3;

	switch( type ) {
		case MB_MODEL:
			switch( ent->model->type ) {
				case mod_brush:
					if( r_shadowPass )
						break;

					MB_NUM2SHADER( mb->shaderkey, shader );

					if( shader->flags & SHADER_SKY ) {	// draw sky
						if( !(ri.params & RP_NOSKY) ) {
							R_DrawSky( shader );
							ri.params |= RP_NOSKY;
						}
						return;
					}

					surf = &ent->model->surfaces[mb->infokey-1];
					nextSurf = NULL;

					features = shader->features;
					if( r_shownormals->integer && !r_shadowPass )
						features |= MF_NORMALS;
					if( shader->flags & SHADER_AUTOSPRITE )
						features |= MF_NOCULL;
					features |= r_superLightStyles[surf->superLightStyle].features;

					// check if we need to render batched geometry this frame
					if( !(features & MF_NONBATCHED)
						&& nextmb 
						&& (nextmb->shaderkey == mb->shaderkey) 
						&& (nextmb->sortkey == mb->sortkey) 
						&& (nextmb->dlightbits == mb->dlightbits) ) {
						if( nextmb->infokey > 0 ) {
							MB_NUM2ENTITY( nextmb->sortkey, nextEnt );
							nextSurf = &nextEnt->model->surfaces[nextmb->infokey-1];
						}
					}

					R_PushMesh( surf->mesh, features );

					nonMergable = nextSurf ? R_MeshOverflow( nextSurf->mesh ) : qtrue;
					if( nonMergable ) {
						if( ri.previousentity != ri.currententity )
							R_RotateForEntity( ri.currententity );
						R_RenderMeshBuffer( mb, r_shadowPass );
					}
					break;
				case mod_alias:
					R_DrawAliasModel( mb, r_shadowPass );
					break;
				case mod_sprite:
					if( !r_shadowPass ) {
						R_PushSpriteModel( mb );

						// no rotation for sprites
						R_TranslateForEntity( ri.currententity );
						R_RenderMeshBuffer( mb, qfalse );
					}
					break;
				case mod_skeletal:
					R_DrawSkeletalModel( mb, r_shadowPass );
					break;
				default:
					assert( 0 );		// shut up compiler
					break;
			}
			break;
		case MB_SPRITE:
			if( r_shadowPass )
				break;

			nonMergable = R_PushSpritePoly( mb );
			if( nonMergable
				|| !nextmb 
				|| ((nextmb->shaderkey & 0xFC000FFF) != (mb->shaderkey & 0xFC000FFF))
				|| ((nextmb->sortkey & 0xFFFFF) != (mb->sortkey & 0xFFFFF))
				|| R_SpriteOverflow () ) {
				if( !nonMergable ) {
					ri.currententity = r_worldent;
					ri.currentmodel = r_worldmodel;
				}

				// no rotation for sprites
				if( ri.previousentity != ri.currententity )
					R_TranslateForEntity( ri.currententity );
				R_RenderMeshBuffer( mb, qfalse );
			}
			break;
		case MB_POLY:
			if( r_shadowPass )
				break;

			// polys are already batched at this point
			R_PushPoly( mb );

			if( ri.previousentity != ri.currententity )
				R_LoadIdentity ();
			R_RenderMeshBuffer( mb, qfalse );
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

	if( ri.meshlist->num_meshes )
		R_QSortMeshBuffers( ri.meshlist->meshbuffer, 0, ri.meshlist->num_meshes - 1 );
	if( ri.meshlist->num_translucent_meshes )
		R_ISortMeshBuffers( ri.meshlist->meshbuffer_translucent, ri.meshlist->num_translucent_meshes );
}

/*
================
R_DrawPortals
================
*/
void R_DrawPortals( void )
{
	int i;
	meshbuffer_t *mb;
	shader_t *shader;

	if( r_fastsky->integer || (ri.params & (RP_MIRRORVIEW|RP_PORTALVIEW|RP_SKYPORTALVIEW)) )
		return;

	for( i = 0, mb = ri.meshlist->meshbuffer; i < ri.meshlist->num_meshes; i++, mb++ ) {
		MB_NUM2SHADER( mb->shaderkey, shader );

		if( shader->sort != SHADER_SORT_PORTAL )
			break;
		R_DrawPortalSurface( mb );
	}

	if( ri.refdef.rdflags & RDF_SKYPORTALINVIEW ) {
		if( ri.params & RP_NOSKY )
			return;

		for( i = 0, mb = ri.meshlist->meshbuffer; i < ri.meshlist->num_meshes; i++, mb++ ) {
			MB_NUM2SHADER( mb->shaderkey, shader );

			if( shader->flags & SHADER_SKY ) {
				R_DrawSky( shader );
				ri.params |= RP_NOSKY;
				break;
			}
		}
	}
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

	ri.previousentity = NULL;
	if( ri.meshlist->num_meshes ) {
		r_shadowPass = qfalse;
		r_triangleOutlines = triangleOutlines;

		meshbuf = ri.meshlist->meshbuffer;
		for( i = 0; i < ri.meshlist->num_meshes - 1; i++, meshbuf++ )
			R_BatchMeshBuffer( meshbuf, meshbuf+1 );
		R_BatchMeshBuffer( meshbuf, NULL );

		if( r_shadows->integer >= SHADOW_PLANAR ) {
			if( !triangleOutlines ) {
				R_BeginShadowPass ();
			}

			r_shadowPass = qtrue;
			r_triangleOutlines = qfalse;

			meshbuf = ri.meshlist->meshbuffer;
			for( i = 0; i < ri.meshlist->num_meshes - 1; i++, meshbuf++ )
				R_BatchMeshBuffer( meshbuf, meshbuf+1 );
			R_BatchMeshBuffer( meshbuf, NULL );

			if( !triangleOutlines ) {
				R_EndShadowPass ();
			}
		}
	}

	r_shadowPass = qfalse;
	r_triangleOutlines = triangleOutlines;

	if( ri.meshlist->num_translucent_meshes ) {
		meshbuf = ri.meshlist->meshbuffer_translucent;
		for( i = 0; i < ri.meshlist->num_translucent_meshes - 1; i++, meshbuf++ )
			R_BatchMeshBuffer( meshbuf, meshbuf + 1 );
		R_BatchMeshBuffer( meshbuf, NULL );
	}

	R_LoadIdentity ();
}

/*
===============
R_InitMeshLists
===============
*/
void R_InitMeshLists( void )
{
	if( !r_meshlistmempool )
		r_meshlistmempool = Mem_AllocPool( NULL, "MeshList" );
}

/*
===============
R_FreeMeshLists
===============
*/
void R_FreeMeshLists( void )
{
	if( !r_meshlistmempool )
		return;

	Mem_FreePool( &r_meshlistmempool );

	memset( &r_worldlist, 0, sizeof( meshlist_t ) );
	memset( &r_portallist, 0, sizeof( meshlist_t ) );
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
qboolean R_ScissorForPortal( entity_t *ent, vec3_t mins, vec3_t maxs, int *x, int *y, int *w, int *h )
{
	int i;
	int ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2;
	vec3_t v, axis[3], tmp, corner;

	if( Matrix_Compare( ent->axis, axis_identity ) ) {
		Matrix_Copy( axis_identity, axis );
	} else {
		VectorCopy( ent->axis[0], axis[0] );
		VectorNegate( ent->axis[1], axis[1] );
		VectorCopy( ent->axis[2], axis[2] );
	}

	x1 = y1 = 999999;
	x2 = y2 = -999999;
	for( i = 0; i < 8; i++ ) {	// compute and rotate a full bounding box
		tmp[0] = ( ( i & 1 ) ? mins[0] : maxs[0] );
		tmp[1] = ( ( i & 2 ) ? mins[1] : maxs[1] );
		tmp[2] = ( ( i & 4 ) ? mins[2] : maxs[2] );

		Matrix_TransformVector( axis, tmp, corner );
		VectorMA( ent->origin, ent->scale, corner,  corner );
		R_TransformToScreen_Vec3( corner, v );

		if( v[2] < 0 || v[2] > 1 ) { // the test point is behind the nearclip plane
			if( PlaneDiff( corner, &ri.frustum[0] ) < PlaneDiff( corner, &ri.frustum[1] ) )
				v[0] = 0;
			else
				v[0] = ri.refdef.width;
			if( PlaneDiff( corner, &ri.frustum[2] ) < PlaneDiff( corner, &ri.frustum[3] ) )
				v[1] = 0;
			else
				v[1] = ri.refdef.height;
		} 

		x1 = min( x1, v[0] ); y1 = min( y1, v[1] );
		x2 = max( x2, v[0] ); y2 = max( y2, v[1] );
	}

	ix1 = max( x1 - 1.0f, 0 ); ix2 = min( x2 + 1.0f, ri.refdef.width );
	if( ix1 >= ix2 )
		return qfalse;		// FIXME

	iy1 = max( y1 - 1.0f, 0 ); iy2 = min( y2 + 1.0f, ri.refdef.height );
	if( iy1 >= iy2 )
		return qfalse;		// FIXME

	*x = ix1;
	*y = iy1;
	*w = ix2 - ix1;
	*h = iy2 - iy1;

	return qtrue;
}

/*
===============
R_DrawPortalSurface
===============
*/
void R_DrawPortalSurface( const meshbuffer_t *mb )
{
	int i, x, y, w, h;
	float dist,	d;
	refinst_t oldRI;
	vec3_t v[3], entity_rotation[3], origin, angles;
	entity_t *ent;
	mesh_t *mesh;
	model_t *model;
	msurface_t *surf;
	cplane_t plane, *portal_plane = &plane, original_plane;

	MB_NUM2ENTITY( mb->sortkey, ent );
	if( !(model = ent->model) )
		return;

	surf = mb->infokey > 0 ? &model->surfaces[mb->infokey-1] : NULL;
	if( !surf || !(mesh = surf->mesh) || !mesh->xyzArray )
		return;

	Matrix_Transpose( ent->axis, entity_rotation );
	Matrix_TransformVector( entity_rotation, mesh->xyzArray[mesh->indexes[0]], v[0] ); VectorMA( ent->origin, ent->scale, v[0], v[0] );
	Matrix_TransformVector( entity_rotation, mesh->xyzArray[mesh->indexes[1]], v[1] ); VectorMA( ent->origin, ent->scale, v[1], v[1] );
	Matrix_TransformVector( entity_rotation, mesh->xyzArray[mesh->indexes[2]], v[2] ); VectorMA( ent->origin, ent->scale, v[2], v[2] );
	PlaneFromPoints( v, portal_plane );
	CategorizePlane( portal_plane );

	if( ( dist = PlaneDiff( ri.viewOrigin, portal_plane ) ) <= BACKFACE_EPSILON )
		return;

	if( !R_ScissorForPortal( ent, surf->mins, surf->maxs, &x, &y, &w, &h ) )
		return;

	VectorCopy( mesh->xyzArray[mesh->indexes[0]], v[0] );
	VectorCopy( mesh->xyzArray[mesh->indexes[1]], v[1] );
	VectorCopy( mesh->xyzArray[mesh->indexes[2]], v[2] );
	PlaneFromPoints( v, &original_plane );
	original_plane.dist += DotProduct( ent->origin, original_plane.normal );
	CategorizePlane( &original_plane );

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

	ri.previousentity = NULL;
	memcpy( &oldRI, &ri, sizeof( ri ) );
	ri.refdef.rdflags &= ~RDF_SKYPORTALINVIEW;

	if( VectorCompare( ent->origin, ent->origin2 ) ) {	// mirror
		vec3_t M[3];

		d = -2 * (DotProduct( ri.viewOrigin, portal_plane->normal ) - portal_plane->dist);
		VectorMA( ri.viewOrigin, d, portal_plane->normal, origin );

		d = -2 * DotProduct( ri.vpn, portal_plane->normal );
		VectorMA( ri.vpn, d, portal_plane->normal, M[0] );
		VectorNormalize( M[0] );

		d = -2 * DotProduct( ri.vright, portal_plane->normal );
		VectorMA( ri.vright, d, portal_plane->normal, M[1] );
		VectorNormalize( M[1] );

		d = -2 * DotProduct( ri.vup, portal_plane->normal );
		VectorMA( ri.vup, d, portal_plane->normal, M[2] );
		VectorNormalize( M[2] );

		Matrix_EulerAngles( M, angles );
		angles[ROLL] = -angles[ROLL];

		ri.params = RP_MIRRORVIEW;
	} else {		// portal
		vec3_t tvec;
		vec3_t A[3], B[3], Bt[3], rot[3], tmp[3], D[3];
		shader_t *shader;

		MB_NUM2SHADER( mb->shaderkey, shader );
		if( shader->flags & SHADER_AGEN_PORTAL ) {
			dist = PlaneDiff( ri.viewOrigin, portal_plane );

			for( i = 0; i < shader->numpasses; i++ ) {
				if( shader->passes[i].alphagen.type != ALPHA_GEN_PORTAL )
					continue;
				if( dist > (1.0/shader->passes[i].alphagen.args[0]) )
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
		ByteToDir( ent->skinNum, portal_plane->normal );

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
			Matrix_TransformVector( A, ri.vpn, tmp[0] );
			Matrix_TransformVector( A, ri.vright, tmp[1] );
			Matrix_TransformVector( A, ri.vup, tmp[2] );
			Matrix_Rotate( tmp, 5 * R_FastSin( ent->scale + ri.refdef.time * ent->frame * 0.01f ), 1, 0, 0 );
			Matrix_TransformVector( Bt, tmp[0], D[0] );
			Matrix_TransformVector( Bt, tmp[1], D[1] );
			Matrix_TransformVector( Bt, tmp[2], D[2] );
		} else {
			Matrix_TransformVector( rot, ri.vpn, D[0] );
			Matrix_TransformVector( rot, ri.vright, D[1] );
			Matrix_TransformVector( rot, ri.vup, D[2] );
		}

		// set up portal_plane
		portal_plane->dist = DotProduct( ent->origin2, portal_plane->normal );
		CategorizePlane( portal_plane );

		// translate view origin
		VectorSubtract( ri.viewOrigin, ent->origin, tvec );
		Matrix_TransformVector( rot, tvec, origin );
		VectorAdd( origin, ent->origin2, origin );

		// calculate Euler angles for our rotation matrix
		Matrix_EulerAngles( D, angles );

		// for portals, vis data is taken from portal origin, not
		// view origin, because the view point moves around and
		// might fly into (or behind) a wall
		ri.params = RP_PORTALVIEW;
		VectorCopy( ent->origin2, ri.pvsOrigin );
	}

	ri.meshlist = &r_portallist;
	ri.clipPlane = plane;
	Vector4Set( ri.scissor, ri.refdef.x + x, ri.refdef.y + y, w, h );
	VectorCopy( origin, ri.refdef.vieworg );
	VectorCopy( angles, ri.refdef.viewangles );

	R_RenderView( &ri.refdef );

	if( ri.params & RP_PORTALVIEW )
		r_oldviewcluster = r_viewcluster = -1;	// force markleafs next frame

	memcpy( &ri, &oldRI, sizeof( ri ) );
}

/*
===============
R_DrawSkyPortal
===============
*/
void R_DrawSkyPortal( skyportal_t *skyportal, vec3_t mins, vec3_t maxs )
{
	int x, y, w, h;
	refinst_t oldRI;

	if( !R_ScissorForPortal( r_worldent, mins, maxs, &x, &y, &w, &h ) )
		return;

	ri.previousentity = NULL;
	memcpy( &oldRI, &ri, sizeof( ri ) );

	ri.params = RP_SKYPORTALVIEW;
	VectorCopy( skyportal->origin, ri.pvsOrigin );

	ri.meshlist = &r_portallist;
	Vector4Set( ri.scissor, ri.refdef.x + x, ri.refdef.y + y, w, h );
	VectorCopy( skyportal->origin, ri.refdef.vieworg );

	ri.refdef.rdflags &= ~(RDF_UNDERWATER|RDF_SKYPORTALINVIEW);
	if( skyportal->fov ) {
		ri.refdef.fov_x = skyportal->fov;
		ri.refdef.fov_y = CalcFov( ri.refdef.fov_x, ri.refdef.width, ri.refdef.height );
	}

	R_RenderView( &ri.refdef );

	r_oldviewcluster = r_viewcluster = -1;	// force markleafs next frame

	memcpy( &ri, &oldRI, sizeof( ri ) );
}

/*
===============
R_DrawCubemapView
===============
*/
void R_DrawCubemapView( vec3_t origin, vec3_t angles, int size )
{
	refdef_t *fd;

	fd = &ri.refdef;
	*fd = r_lastRefdef;
	fd->time = 0;
	fd->x = ri.refdef.y = 0;
	fd->width = size;
	fd->height = size;
	fd->fov_x = 90;
	fd->fov_y = 90;
	VectorCopy( origin, fd->vieworg );
	VectorCopy( angles, fd->viewangles );

	r_numPolys = 0;
	r_numDlights = 0;

	R_RenderScene( fd );

	r_oldviewcluster = r_viewcluster = -1;	// force markleafs next frame
}

/*
===============
R_BuildTangentVectors
===============
*/
void R_BuildTangentVectors( int numVertexes, vec3_t *xyzArray, vec3_t *normalsArray, vec2_t *stArray, int numTris, index_t *indexes, vec4_t *sVectorsArray, vec3_t *tVectorsArray )
{
	int i, j;
	float d, *v[3], *tc[3];
	vec_t *s, *t, *n;
	vec3_t stvec[3], cross;
	qboolean broken = qfalse;

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
		VectorSubtract( v[1], v[0], stvec[0] );
		VectorSubtract( v[2], v[0], stvec[1] );

		// we have two edge directions, we can calculate the normal then
		CrossProduct( stvec[1], stvec[0], cross );

		for( j = 0; j < 3; j++ ) {
			stvec[0][j] = ((tc[1][1] - tc[0][1]) * (v[2][j] - v[0][j]) - (tc[2][1] - tc[0][1]) * (v[1][j] - v[0][j]));
			stvec[1][j] = ((tc[1][0] - tc[0][0]) * (v[2][j] - v[0][j]) - (tc[2][0] - tc[0][0]) * (v[1][j] - v[0][j]));
		}

		// inverse tangent vectors if their cross product goes in the opposite
		// direction to triangle normal
		CrossProduct( stvec[1], stvec[0], stvec[2] );
		if( DotProduct( stvec[2], cross ) < 0 ) {
			VectorInverse( stvec[0] );
			VectorInverse( stvec[1] );
		}

		for( j = 0; j < 3; j++ ) {
			VectorAdd( sVectorsArray[indexes[j]], stvec[0], sVectorsArray[indexes[j]] );
			VectorAdd( tVectorsArray[indexes[j]], stvec[1], tVectorsArray[indexes[j]] );
		}
	}

	// normalize
	for( i = 0, s = *sVectorsArray, t = *tVectorsArray, n = *normalsArray; i < numVertexes; i++, s+=4, t+=3, n+=3 ) {
		// keep s\t vectors perpendicular
		d = -DotProduct( s, n );
		VectorMA( s, d, n, s );
		VectorNormalize( s );

		d = -DotProduct( t, n );
		VectorMA( t, d, n, t );

		// store polarity of t-vector in the 4-th coordinate of s-vector
		CrossProduct( n, s, cross );
		if( DotProduct( cross, t ) < 0 )
			s[3] = -1;
		else
			s[3] = 1;
	}
}
