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
#include "r_local.h"

#if SHADOW_VOLUMES

static qboolean triangleFacingLight[MAX_ARRAY_TRIANGLES];
static index_t	shadowVolumeIndexes[MAX_SHADOWVOLUME_INDEXES];
static int		numShadowVolumeTris;

/*
===============
R_FindTriangleWithEdge
===============
*/
static int R_FindTriangleWithEdge( index_t *indexes, int numtris, index_t start, index_t end, int ignore)
{
	int i;
	int match, count;

	count = 0;
	match = -1;

	for( i = 0; i < numtris; i++, indexes += 3 ) {
		if( (indexes[0] == start && indexes[1] == end)
			|| (indexes[1] == start && indexes[2] == end)
			|| (indexes[2] == start && indexes[0] == end) ) {
			if (i != ignore)
				match = i;
			count++;
		} else if( (indexes[1] == start && indexes[0] == end)
			|| (indexes[2] == start && indexes[1] == end)
			|| (indexes[0] == start && indexes[2] == end) ) {
			count++;
		}
	}

	// detect edges shared by three triangles and make them seams
	if( count > 2 )
		match = -1;

	return match;
}

/*
===============
R_BuildTriangleNeighbors
===============
*/
void R_BuildTriangleNeighbors( int *neighbors, index_t *indexes, int numtris )
{
	int i, *n;
	index_t *index;

	for( i = 0, index = indexes, n = neighbors; i < numtris; i++, index += 3, n += 3 ) {
		n[0] = R_FindTriangleWithEdge( indexes, numtris, index[1], index[0], i );
		n[1] = R_FindTriangleWithEdge( indexes, numtris, index[2], index[1], i );
		n[2] = R_FindTriangleWithEdge( indexes, numtris, index[0], index[2], i );
	}
}

/*
===============
R_BuildShadowVolumeTriangles
===============
*/
static int R_BuildShadowVolumeTriangles( void )
{
	int i, j, tris;
	index_t *indexes = indexesArray;
	int *neighbors = neighborsArray;
	index_t *out = shadowVolumeIndexes;

	// check each frontface for bordering backfaces,
	// and cast shadow polygons from those edges,
	// also create front and back caps for shadow volume
	for( i = 0, j = 0, tris = 0; i < numIndexes; i += 3, j++, indexes += 3, neighbors += 3 ) {
		if( triangleFacingLight[j] ) {
			// triangle is frontface and therefore casts shadow,
			// output front and back caps for shadow volume front cap
			out[0] = indexes[0];
			out[1] = indexes[1];
			out[2] = indexes[2];

			// rear cap (with flipped winding order)
			out[3] = indexes[0] + numVerts;
			out[4] = indexes[2] + numVerts;
			out[5] = indexes[1] + numVerts;
			out += 6;
			tris += 2;

			// check the edges
			if( neighbors[0] < 0 || !triangleFacingLight[neighbors[0]] ) {
				out[0] = indexes[1];
				out[1] = indexes[0];
				out[2] = indexes[0] + numVerts;
				out[3] = indexes[1];
				out[4] = indexes[0] + numVerts;
				out[5] = indexes[1] + numVerts;
				out += 6;
				tris += 2;
			}

			if( neighbors[1] < 0 || !triangleFacingLight[neighbors[1]] ) {
				out[0] = indexes[2];
				out[1] = indexes[1];
				out[2] = indexes[1] + numVerts;
				out[3] = indexes[2];
				out[4] = indexes[1] + numVerts;
				out[5] = indexes[2] + numVerts;
				out += 6;
				tris += 2;
			}

			if( neighbors[2] < 0 || !triangleFacingLight[neighbors[2]] ) {
				out[0] = indexes[0];
				out[1] = indexes[2];
				out[2] = indexes[2] + numVerts;
				out[3] = indexes[0];
				out[4] = indexes[2] + numVerts;
				out[5] = indexes[0] + numVerts;
				out += 6;
				tris += 2;
			}
		}
	}

	return tris;
}

/*
===============
R_MakeTriangleShadowFlagsFromScratch
===============
*/
static void R_MakeTriangleShadowFlagsFromScratch ( vec3_t lightdist, float lightradius )
{
	float f;
	int i, j;
	float *v0, *v1, *v2;
	vec3_t dir0, dir1, temp;
	float *trnormal = trNormalsArray[0];
	index_t *indexes = indexesArray;

	for( i = 0, j = 0; i < numIndexes; i += 3, j++, trnormal += 3, indexes += 3 ) {
		// calculate triangle facing flag
		v0 = ( float * )(inVertsArray + indexes[0]);
		v1 = ( float * )(inVertsArray + indexes[1]);
		v2 = ( float * )(inVertsArray + indexes[2]);

		// calculate two mostly perpendicular edge directions
		VectorSubtract( v1, v0, dir0 );
		VectorSubtract( v2, v0, dir1 );

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the surface normal (it's magnitude
		// is not 1 however)
		CrossProduct( dir0, dir1, temp );

		// compare distance of light along normal, with distance of any point
		// of the triangle along the same normal (the triangle is planar,
		// I.E. flat, so all points give the same answer)
		f = ( lightdist[0] - v0[0] ) * temp[0] + ( lightdist[1] - v0[1] ) * temp[1] + ( lightdist[2] - v0[2] ) * temp[2];
		triangleFacingLight[j] = f > 0;
	}
}

/*
===============
R_MakeTriangleShadowFlags
===============
*/
static void R_MakeTriangleShadowFlags( vec3_t lightdist, float lightradius )
{
	int i, j;
	float f;
	float *v0;
	float *trnormal = trNormalsArray[0];
	index_t *indexes = indexesArray;

	for( i = 0, j = 0; i < numIndexes; i += 3, j++, trnormal += 3, indexes += 3 ) {
		v0 = ( float * )(vertsArray + indexes[0]);

		// compare distance of light along normal, with distance of any point
		// of the triangle along the same normal (the triangle is planar,
		// I.E. flat, so all points give the same answer)
		f = ( lightdist[0] - v0[0] ) * trnormal[0] + ( lightdist[1] - v0[1] ) * trnormal[1] + ( lightdist[2] - v0[2] ) * trnormal[2];
		triangleFacingLight[j] = f > 0;
	}
}

/*
===============
R_ShadowProjectVertices
===============
*/
static void R_ShadowProjectVertices( vec3_t lightdist, float projectdistance )
{
	int i;
	vec3_t diff;
	float *in, *out;

	in = (float *)(vertsArray[0]);
	out = (float *)(vertsArray[numVerts]);

	for( i = 0; i < numVerts; i++, in += 3, out += 3 ) {
		VectorSubtract( in, lightdist, diff );
		VectorNormalizeFast( diff );
		VectorMA( in, projectdistance, diff, out );
//		VectorMA( in, r_shadows_nudge->value, diff, in );
	}
}

/*
===============
R_BuildShadowVolume
===============
*/
static void R_BuildShadowVolume( vec3_t lightdist, float projectdistance )
{
	if( currentTrNormal != trNormalsArray[0] )
		R_MakeTriangleShadowFlags( lightdist, projectdistance );
	else
		R_MakeTriangleShadowFlagsFromScratch( lightdist, projectdistance );

	R_ShadowProjectVertices( lightdist, projectdistance );
	numShadowVolumeTris = R_BuildShadowVolumeTriangles ();
}

/*
===============
R_DrawShadowVolume
===============
*/
static void R_DrawShadowVolume( void )
{
#ifdef VERTEX_BUFFER_OBJECTS
	if( glConfig.vertexBufferObject ) {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_VERTS] );
		qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * 2 * sizeof( vec3_t ), vertsArray, GL_STREAM_DRAW_ARB );
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	}
#endif

	if( glConfig.drawRangeElements )
		qglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVerts * 2, numShadowVolumeTris * 3, GL_UNSIGNED_INT, shadowVolumeIndexes );
	else
		qglDrawElements( GL_TRIANGLES, numShadowVolumeTris * 3, GL_UNSIGNED_INT, shadowVolumeIndexes );
}

/*
===============
R_CheckLightBoundaries
===============
*/
static qboolean R_CheckLightBoundaries( vec3_t mins, vec3_t maxs, vec3_t lightorg, float intensity2 )
{
	vec3_t v;

	v[0] = bound( mins[0], lightorg[0], maxs[0] );
	v[1] = bound( mins[1], lightorg[1], maxs[1] );
	v[2] = bound( mins[2], lightorg[2], maxs[2] );

	return( DotProduct(v, v) < intensity2 );
}

/*
===============
R_CastShadowVolume
===============
*/
static void R_CastShadowVolume( vec3_t mins, vec3_t maxs, float radius, vec3_t lightorg, float intensity )
{
	float projectdistance, intensity2;
	vec3_t lightdist, lightdist2;

	if( R_CullSphere( lightorg, intensity, 15 ) )
		return;

	intensity2 = intensity * intensity;
	VectorSubtract( lightorg, ri.currententity->origin, lightdist2 );

	if( !R_CheckLightBoundaries( mins, maxs, lightdist2, intensity2 ) )
		return;

	projectdistance = radius - VectorLength( lightdist2 );
	if( projectdistance > 0 )		// light is inside the bbox
		return;

	projectdistance += intensity;
	if( projectdistance <= 0.1 )	// too far away
		return;

	if( !Matrix_Compare( ri.currententity->axis, axis_identity ) )
		Matrix_TransformVector( ri.currententity->axis, lightdist2, lightdist );
	else
		VectorCopy( lightdist2, lightdist );

	R_BuildShadowVolume( lightdist, projectdistance );

	R_UnlockArrays ();

	R_LockArrays( numVerts * 2 );

	if( r_shadows->integer == SHADOW_VOLUMES ) {
		GL_Cull( GL_BACK );		// quake is backwards, this culls front faces
		qglStencilOp( GL_KEEP, GL_DECR, GL_KEEP );
		R_DrawShadowVolume ();

		// decrement stencil if frontface is behind depthbuffer
		GL_Cull( GL_FRONT );	// quake is backwards, this culls back faces
		qglStencilOp( GL_KEEP, GL_INCR, GL_KEEP );
	}

	R_DrawShadowVolume ();
}

/*
===============
R_DrawShadowVolumes
===============
*/
void R_DrawShadowVolumes( mesh_t *mesh, vec3_t mins, vec3_t maxs, float radius )
{
	int i;

	if( !r_worldmodel ) {
		R_ClearArrays ();
		return;
	}

	if (0)
	{
		mlight_t *wlight;
		dlight_t *dlight;	

		wlight = r_worldmodel->worldlights;
		for ( i = 0; i < r_worldmodel->numworldlights; i++, wlight++ )
			R_CastShadowVolume( mins, maxs, radius, wlight->origin, wlight->intensity );

		dlight = r_dlights;
		for( i = 0; i < r_numDlights; i++, dlight++ )
			R_CastShadowVolume( mins, maxs, radius, dlight->origin, dlight->intensity );
	}
	else
	{
		vec4_t diffuse;
		vec3_t lightdir, neworigin;

		R_LightForOrigin( ri.currententity->lightingOrigin, lightdir, NULL, diffuse, radius );
		VectorSet( lightdir, -lightdir[0], -lightdir[1], -1 );
		VectorNormalize( lightdir );
		VectorMA( ri.currententity->origin, -(radius + 10), lightdir, neworigin );

		R_CastShadowVolume( mins, maxs, radius, neworigin, r_shadows_projection_distance->value * VectorLength( diffuse ) );
	}

	R_ClearArrays ();
}

/*
===============
R_ShadowBlend
===============
*/
void R_ShadowBlend( void )
{
	if( r_shadows->integer != SHADOW_VOLUMES || !glState.stencilEnabled )
		return;

	qglScissor( 0, 0, glState.width, glState.height );
	qglViewport( 0, 0, glState.width, glState.height );
	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity ();
	qglOrtho( 0, 1, 1, 0, -10, 100 );
	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity ();

	GL_Cull( 0 );
	GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	qglColor4f( 0, 0, 0, bound (0.0f, r_shadows_alpha->value, 1.0f) );

	qglDisable( GL_TEXTURE_2D );

	qglDepthFunc( GL_ALWAYS );

	qglEnable( GL_STENCIL_TEST );
	qglStencilFunc( GL_NOTEQUAL, 128, ~0 );
	qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	qglStencilMask( ~0 );

	qglBegin( GL_TRIANGLES );
	qglVertex2f( -5, -5 );
	qglVertex2f( 10, -5 );
	qglVertex2f( -5, 10 );
	qglEnd ();

	qglDepthFunc( GL_LEQUAL );

	qglDisable( GL_STENCIL_TEST );
	qglEnable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );
}
#endif

/*
===============
R_BeginShadowPass
===============
*/
void R_BeginShadowPass( void )
{
	R_BackendCleanUpTextureUnits ();

	qglDisable( GL_TEXTURE_2D );

	if( r_shadows->integer == SHADOW_PLANAR ) {
		GL_SetState( GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );
		qglColor4f( 0, 0, 0, bound( 0.0f, r_shadows_alpha->value, 1.0f ) );

		qglEnable( GL_STENCIL_TEST );
		qglStencilMask( ~0 );
		qglStencilFunc( GL_EQUAL, 128, 0xFF );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
#if SHADOW_VOLUMES
	} else if( r_shadows->integer == SHADOW_VOLUMES ) {
		GL_SetState( GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );
		qglColor4f( 1, 1, 1, 1 );
		qglColorMask( 0, 0, 0, 0 );

		qglEnable( GL_STENCIL_TEST );
		qglStencilMask( ~0 );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		qglStencilFunc( GL_ALWAYS, 128, ~0 );
	} else {
		GL_Cull( 0 );
		GL_SetState( GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE );
		qglColor4f( 1.0, 0.1, 0.1, 1 );
#endif
	}
}

/*
===============
R_EndShadowPass
===============
*/
void R_EndShadowPass( void )
{
	if( r_shadows->integer == SHADOW_PLANAR ) {
		qglDisable( GL_STENCIL_TEST );
#if SHADOW_VOLUMES
	} else if( r_shadows->integer == SHADOW_VOLUMES ) {
		qglDisable( GL_STENCIL_TEST );
		qglColorMask( 1, 1, 1, 1 );
	} else {
#endif
		GL_SetState( GLSTATE_DEPTHWRITE );
	}

	qglEnable( GL_TEXTURE_2D );
}

/*
===============
R_Draw_SimpleShadow
===============
*/
void R_Draw_SimpleShadow( entity_t *e )
{
	int i;
	float *v;
	float planedist, dist;
	vec3_t planenormal, lightdir, lightdir2, point;
	trace_t tr;

	if( e->flags & RF_NOSHADOW )
		return;

	R_LightForOrigin( e->lightingOrigin, lightdir, NULL, NULL, e->model->radius * e->scale );

	VectorSet( lightdir, -lightdir[0], -lightdir[1], -1 );	
	VectorNormalizeFast( lightdir );
	VectorMA( e->origin, 1024.0f, lightdir, point );

	CL_GameModule_Trace( &tr, e->origin, vec3_origin, vec3_origin, point, -1, CONTENTS_SOLID );
	if( tr.fraction == 1.0f ) {
		R_ClearArrays ();
		return;
	}

	Matrix_TransformVector( e->axis, lightdir, lightdir2 );
	Matrix_TransformVector( e->axis, tr.plane.normal, planenormal );

	VectorSubtract( tr.endpos, e->origin, point );
	planedist = DotProduct ( point, tr.plane.normal ) + 1;

	dist = -1.0f / DotProduct( lightdir2, planenormal );
	VectorScale( lightdir2, dist, lightdir2 );

	v = (float *)(inVertsArray[0]);
	for( i = 0; i < numVerts; i++, v += 3 ) {
		dist = DotProduct( v, planenormal ) - planedist;
		if( dist > 0 )
			VectorMA( v, dist, lightdir2, v );
	}

	R_UnlockArrays ();

	R_LockArrays( numVerts );

	R_FlushArrays ();

	R_ClearArrays ();
}
