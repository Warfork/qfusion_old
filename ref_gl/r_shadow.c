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

static qboolean triangleFacingLight[MAX_ARRAY_TRIANGLES];
static index_t	shadowVolumeIndexes[MAX_SHADOWVOLUME_INDEXES];
static int		numShadowVolumeTris;

int R_FindTriangleWithEdge ( index_t *indexes, int numtris, index_t start, index_t end, int ignore)
{
	int i;
	int match, count;
	
	count = 0;
	match = -1;
	
	for (i = 0; i < numtris; i++, indexes += 3)
	{
		if ( (indexes[0] == start && indexes[1] == end)
			|| (indexes[1] == start && indexes[2] == end)
			|| (indexes[2] == start && indexes[0] == end) ) {
			if (i != ignore)
				match = i;
			count++;
		} else if ( (indexes[1] == start && indexes[0] == end)
			|| (indexes[2] == start && indexes[1] == end)
			|| (indexes[0] == start && indexes[2] == end) ) {
			count++;
		}
	}

	// detect edges shared by three triangles and make them seams
	if (count > 2)
		match = -1;

	return match;
}

/*
===============
R_BuildTriangleNeighbors
===============
*/
void R_BuildTriangleNeighbors ( int *neighbors, index_t *indexes, int numtris )
{
	int i, *n;
	index_t *index;

	for (i = 0, index = indexes, n = neighbors; i < numtris; i++, index += 3, n += 3)
	{
		n[0] = R_FindTriangleWithEdge (indexes, numtris, index[1], index[0], i);
		n[1] = R_FindTriangleWithEdge (indexes, numtris, index[2], index[1], i);
		n[2] = R_FindTriangleWithEdge (indexes, numtris, index[0], index[2], i);
	}
}

/*
===============
R_BuildShadowVolumeTriangles
===============
*/
int R_BuildShadowVolumeTriangles (void)
{
	int i, j, tris;
	index_t *indexes = indexesArray;
	int *neighbors = neighborsArray;
	index_t *out = shadowVolumeIndexes;

	// check each frontface for bordering backfaces,
	// and cast shadow polygons from those edges,
	// also create front and back caps for shadow volume
	for (i = 0, j = 0, tris = 0; i < numIndexes; i += 3, j++, indexes += 3, neighbors += 3)
	{
		if (triangleFacingLight[j])
		{
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
			if (neighbors[0] < 0 || !triangleFacingLight[neighbors[0]])
			{
				out[0] = indexes[1];
				out[1] = indexes[0];
				out[2] = indexes[0] + numVerts;
				out[3] = indexes[1];
				out[4] = indexes[0] + numVerts;
				out[5] = indexes[1] + numVerts;
				out += 6;
				tris += 2;
			}

			if (neighbors[1] < 0 || !triangleFacingLight[neighbors[1]])
			{
				out[0] = indexes[2];
				out[1] = indexes[1];
				out[2] = indexes[1] + numVerts;
				out[3] = indexes[2];
				out[4] = indexes[1] + numVerts;
				out[5] = indexes[2] + numVerts;
				out += 6;
				tris += 2;
			}

			if (neighbors[2] < 0 || !triangleFacingLight[neighbors[2]])
			{
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

void R_MakeTriangleShadowFlagsFromScratch ( vec3_t lightdist, float lightradius )
{
	float f;
	int i, j;
	float *v0, *v1, *v2;
	vec3_t dir0, dir1, temp;
	float *trnormal = trNormalsArray[0];
	index_t *indexes = indexesArray;

	for (i = 0, j = 0; i < numIndexes; i += 3, j++, trnormal += 3, indexes += 3)
	{
		// calculate triangle facing flag
		v0 = (float *)(vertexArray + indexes[0]);
		v1 = (float *)(vertexArray + indexes[1]);
		v2 = (float *)(vertexArray + indexes[2]);

		// calculate two mostly perpendicular edge directions
		VectorSubtract ( v0, v1, dir0 );
		VectorSubtract ( v2, v1, dir1 );

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the surface normal (it's magnitude
		// is not 1 however)
		CrossProduct ( dir0, dir1, temp );

		// compare distance of light along normal, with distance of any point
		// of the triangle along the same normal (the triangle is planar,
		// I.E. flat, so all points give the same answer)
		f = ( lightdist[0] - v0[0] ) * temp[0] + ( lightdist[1] - v0[1] ) * temp[1] + ( lightdist[2] - v0[2] ) * temp[2];
		triangleFacingLight[j] = f > 0;
	}
}

void R_MakeTriangleShadowFlags ( vec3_t lightdist, float lightradius )
{
	int i, j;
	float f;
	float *v0;
	float *trnormal = trNormalsArray[0];
	index_t *indexes = indexesArray;

	for (i = 0, j = 0; i < numIndexes; i += 3, j++, trnormal += 3, indexes += 3)
	{
		v0 = (float *)(vertexArray + indexes[0]);

		// compare distance of light along normal, with distance of any point
		// of the triangle along the same normal (the triangle is planar,
		// I.E. flat, so all points give the same answer)
		f = ( lightdist[0] - v0[0] ) * trnormal[0] + ( lightdist[1] - v0[1] ) * trnormal[1] + ( lightdist[2] - v0[2] ) * trnormal[2];
		triangleFacingLight[j] = f > 0;
	}
}

void R_ShadowProjectVertices ( vec3_t lightdist, float projectdistance )
{
	int i;
	vec3_t diff;
	float *in, *out;

	in = (float *)(vertexArray[0]);
	out = (float *)(currentVertex);

	for ( i = 0; i < numVerts; i++, in += 4, out += 4 )
	{
		VectorSubtract ( in, lightdist, diff );
		VectorNormalizeFast ( diff );
		VectorMA ( in, projectdistance, diff, out );
		VectorMA ( in, r_shadows_nudge->value, diff, in );
	}
}

void R_BuildShadowVolume ( vec3_t lightdist, float projectdistance )
{
	if ( currentTrNormal != trNormalsArray[0] ) {
		R_MakeTriangleShadowFlags ( lightdist, projectdistance );
	} else {
		R_MakeTriangleShadowFlagsFromScratch ( lightdist, projectdistance );
	}

	R_ShadowProjectVertices ( lightdist, projectdistance );
	numShadowVolumeTris = R_BuildShadowVolumeTriangles ();
}

void R_DrawShadowVolume (void)
{
	R_LockArrays ( numVerts * 2 );

	if ( !r_arrays_locked ) {
		R_DrawTriangleStrips ( shadowVolumeIndexes, numShadowVolumeTris * 3 );
	} else {
		qglDrawElements ( GL_TRIANGLES, numShadowVolumeTris * 3, GL_UNSIGNED_INT, shadowVolumeIndexes );
	}

	R_UnlockArrays ();
}

void R_CastShadowVolume ( mesh_t *mesh, vec3_t lightorg, float intensity )
{
	float projectdistance;
	vec3_t lightdist, lightdist2;

	VectorSubtract ( lightorg, currententity->origin, lightdist2 );

	if ( !BoundsAndSphereIntersect (mesh->mins, mesh->maxs, lightdist2, intensity) ) {
		return;
	}

	projectdistance = mesh->radius - VectorLength ( lightdist2 );
	if ( projectdistance > 0 ) {	// light is inside the bbox
		return;
	}

	projectdistance += intensity;
	if ( projectdistance <= 0.1 ) {	// too far away
		return;
	}

	if ( !Matrix3_Compare (currententity->axis, axis_identity) ) {
		Matrix3_Multiply_Vec3 ( currententity->axis, lightdist2, lightdist );
	} else {
		VectorCopy ( lightdist2, lightdist );
	}

	R_BuildShadowVolume ( lightdist, projectdistance );

	if ( r_shadows->value == 1 ) {
		qglCullFace (GL_BACK); // quake is backwards, this culls front faces
		qglStencilOp (GL_KEEP, GL_INCR, GL_KEEP);
		R_DrawShadowVolume ();

		// decrement stencil if frontface is behind depthbuffer
		qglCullFace (GL_FRONT); // quake is backwards, this culls back faces
		qglStencilOp (GL_KEEP, GL_DECR, GL_KEEP);
	}

	R_DrawShadowVolume ();
}

void R_DrawShadowVolumes ( mesh_t *mesh )
{
	int i;
	mlight_t *wlight;
	dlight_t *dlight;	

	if ( !r_worldmodel || !r_worldmodel->bmodel || !r_worldmodel->bmodel->numworldlights ) {
		return;
	}

	wlight = r_worldmodel->bmodel->worldlights;
	for ( i = 0; i < r_worldmodel->bmodel->numworldlights; i++, wlight++ ) {
		R_CastShadowVolume ( mesh, wlight->origin, wlight->intensity );
	}

	dlight = r_newrefdef.dlights;
	for ( i = 0; i < r_newrefdef.num_dlights; i++, dlight++ ) {
		R_CastShadowVolume ( mesh, dlight->origin, dlight->intensity );
	}

	R_ClearArrays ();
}
