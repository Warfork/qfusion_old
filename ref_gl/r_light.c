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
// r_light.c

#include "r_local.h"

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

/*
=============
R_SurfMarkLight
=============
*/
void R_SurfMarkLight( int bit, msurface_t *surf )
{
	shader_t *shader;
	mshaderref_t *shaderref;

	shaderref = surf->shaderref;
	if( !shaderref || (shaderref->flags & (SURF_SKY|SURF_NODLIGHT)) )
		return;

	shader = shaderref->shader;
	if( (shader->flags & (SHADER_SKY|SHADER_FLARE)) || !shader->numpasses )
		return;

	if( surf->dlightframe != r_framecount ) {
		surf->dlightbits = 0;
		surf->dlightframe = r_framecount;
	}

	surf->dlightbits |= bit;
}

/*
=============
R_MarkLightWorldNode
=============
*/
#define DLIGHT_SCALE	0.5f

void R_MarkLightWorldNode( dlight_t *light, int bit, mnode_t *node )
{
	int			c;
	mleaf_t		*pleaf;
	mesh_t		*mesh;
	msurface_t	*surf, **mark;
	float		dist, intensity = light->intensity * DLIGHT_SCALE;

	while( 1 ) {
		if( node->visframe != r_visframecount )
			return;
		if( node->plane == NULL )
			break;

		dist = PlaneDiff( light->origin, node->plane );
		if( dist > intensity ) {
			node = node->children[0];
			continue;
		}

		if( dist >= -intensity )
			R_MarkLightWorldNode( light, bit, node->children[0] );
		node = node->children[1];
	}

	pleaf = (mleaf_t *)node;

	// check for door connected areas
	if( r_refdef.areabits ) {
		if(! (r_refdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
			return;		// not visible
	}

	if( !BoundsAndSphereIntersect( pleaf->mins, pleaf->maxs, light->origin, intensity ) )
		return;

	// mark the polygons
	if( !(c = pleaf->nummarksurfaces) )
		return;

	mark = pleaf->firstmarksurface;
	do {
		surf = *mark++;

		if( (mesh = surf->mesh) && (surf->facetype != FACETYPE_FLARE) ) {
			if( !BoundsAndSphereIntersect( surf->mins, surf->maxs, light->origin, intensity ) )
				continue;
		}
		R_SurfMarkLight( bit, surf );
	} while( --c );
}

/*
=================
R_MarkLights
=================
*/
void R_MarkLights( void )
{
	int			k;
	dlight_t	*lt;

	if( !r_dynamiclight->integer || !r_numDlights || r_vertexlight->integer || r_fullbright->value )
		return;

	lt = r_dlights;
	for( k = 0; k < r_numDlights; k++, lt++ )
		R_MarkLightWorldNode( lt, 1<<k, r_worldmodel->nodes );
}

/*
=================
R_AddDynamicLights
=================
*/
void R_AddDynamicLights( unsigned int dlightbits )
{
	int i, j, lnum;
	index_t *index;
	dlight_t *light;
	vec3_t tvec, dlorigin;
	vec3_t vright, vup;
	vec3_t dir1, dir2, normal, right, up, oldnormal = { 0, 0, 0 };
	float *v[3], dist, scale, intensity;
	mat4x4_t m = { 0.5, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 1, 0, 0.5, 0.5, 0, 1 };

	numColors = 0;

	GL_Bind( 0, r_dlighttexture );
	if( !glConfig.vertexBufferObject ) {
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, inCoordsArray[0] );
	}

	qglLoadMatrixf( m );
	qglDepthFunc( GL_EQUAL );
	qglBlendFunc( GL_DST_COLOR, GL_ONE );

	light = r_dlights;
	for( lnum = 0; lnum < r_numDlights; lnum++, light++ ) {
		if( !(dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		intensity = light->intensity * DLIGHT_SCALE;
		intensity *= intensity;

		VectorSubtract( light->origin, currententity->origin, dlorigin );
		if( !Matrix_Compare( currententity->axis, axis_identity ) ) {
			VectorCopy( dlorigin, tvec );
			Matrix_TransformVector( currententity->axis, tvec, dlorigin );
		}

		qglColor3fv( light->color );

		for( i = 0; i < numIndexes; i += 3 ) {
			index = indexesArray + i;
			v[0] = (float *)(vertsArray + index[0]);
			v[1] = (float *)(vertsArray + index[1]);
			v[2] = (float *)(vertsArray + index[2]);

			// calculate two mostly perpendicular edge directions
			VectorSubtract( v[0], v[1], dir1 );
			VectorSubtract( v[2], v[1], dir2 );

			// we have two edge directions, we can calculate a third vector from
			// them, which is the direction of the surface normal
			CrossProduct( dir1, dir2, normal );
			if( VectorNormalize( normal ) <= 0.1 )
				continue;

			VectorSubtract( dlorigin, v[0], tvec );
			dist = DotProduct( tvec, normal );
			dist = dist * dist;
			if( dist >= intensity )
				continue;

			scale = Q_RSqrt( intensity - dist );

			if( !VectorCompare( normal, oldnormal ) ) {
				MakeNormalVectors( normal, up, right );
				VectorCopy( normal, oldnormal );
			}

			VectorScale( right, scale, vright );
			VectorScale( up, scale, vup );

			qglBegin( GL_TRIANGLES );
			for( j = 0; j < 3; j++ ) {
				// Get our texture coordinates
				// Project the light image onto the face
				VectorSubtract( v[j], dlorigin, tvec );
				inCoordsArray[index[j]][0] = DotProduct( tvec, vright );
				inCoordsArray[index[j]][1] = DotProduct( tvec, vup );

				if( glConfig.vertexBufferObject )
					qglTexCoord2fv( inCoordsArray[index[j]] );
				qglArrayElement( index[j] );
			}
			qglEnd ();
		}
	}

	if( !glConfig.vertexBufferObject )
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
}

//===================================================================

/*
===============
R_LightDirForOrigin
===============
*/
void R_LightDirForOrigin( vec3_t origin, vec3_t dir )
{
	vec3_t vf, vf2, tdir;
	float t[8];
	int vi[3], i, index[4];

	if( !r_worldmodel || (r_refdef.rdflags & RDF_NOWORLDMODEL) ||
		 !r_worldmodel->lightgrid || !r_worldmodel->numlightgridelems ) {
		VectorSet( dir, 0.5, 0.2, -1 );
		VectorNormalizeFast( dir );
		return;
	}

	for( i = 0; i < 3; i++ ) {
		vf[i] = (origin[i] - r_worldmodel->gridMins[i]) / r_worldmodel->gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor(vf[i]);
		vf2[i] = 1.0f - vf[i];
	}

	index[0] = vi[2] * r_worldmodel->gridBounds[3] + vi[1] * r_worldmodel->gridBounds[0] + vi[0];
	index[1] = index[0] + r_worldmodel->gridBounds[0];
	index[2] = index[0] + r_worldmodel->gridBounds[3];
	index[3] = index[2] + r_worldmodel->gridBounds[0];

	for( i = 0; i < 4; i++ ) {
		if( index[i] < 0 || index[i] >= (r_worldmodel->numlightgridelems-1) ) {
			VectorSet( dir, 0.5, 0.2, -1 );
			VectorNormalizeFast( dir );
			return;
		}
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	VectorClear( dir );

	for( i = 0; i < 4; i++ ) {
		R_LatLongToNorm( r_worldmodel->lightgrid[ index[i] ].direction, tdir );
		VectorMA( dir, t[i*2], tdir, dir );
		R_LatLongToNorm( r_worldmodel->lightgrid[ index[i] + 1 ].direction, tdir );
		VectorMA( dir, t[i*2+1], tdir, dir );
	}

	VectorNormalizeFast( dir );
}

/*
===============
R_LightForEntity
===============
*/
void R_LightForEntity( entity_t *e, qbyte *bArray )
{
	vec3_t vf, vf2;
	float *cArray;
	float t[8], dot;
	int vi[3], i, j, index[4];
	vec3_t dlorigin, ambient, diffuse, dir, tdir, direction;
	vec3_t tempColorsArray[MAX_ARRAY_VERTS];

	if( (e->flags & RF_FULLBRIGHT) || r_fullbright->value ) {
		memset( bArray, 255, sizeof(byte_vec4_t) * numColors );
		return;
	}

	// probably weird shader, see mpteam4 for example
	if( !e->model || (e->model->type == mod_brush) ) {
		memset( bArray, 255, sizeof(byte_vec4_t) * numColors );
		return;
	}

	VectorSet( ambient, 0, 0, 0 );
	VectorSet( diffuse, 0, 0, 0 );
	VectorSet( direction, 1, 1, 1 );

	if( !r_worldmodel || (r_refdef.rdflags & RDF_NOWORLDMODEL) ||
		 !r_worldmodel->lightgrid || !r_worldmodel->numlightgridelems ) {
		cArray = tempColorsArray[0];
		for( i = 0; i < numColors; i++, cArray += 3 )
			cArray[0] = cArray[1] = cArray[2] = 0;
		goto dynamic;
	}

	for( i = 0; i < 3; i++ ) {
		vf[i] = (e->lightingOrigin[i] - r_worldmodel->gridMins[i]) / r_worldmodel->gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor(vf[i]);
		vf2[i] = 1.0f - vf[i];
	}

	index[0] = vi[2] * r_worldmodel->gridBounds[3] + vi[1] * r_worldmodel->gridBounds[0] + vi[0];
	index[1] = index[0] + r_worldmodel->gridBounds[0];
	index[2] = index[0] + r_worldmodel->gridBounds[3];
	index[3] = index[2] + r_worldmodel->gridBounds[0];

	for( i = 0; i < 4; i++ ) {
		if( index[i] < 0 || index[i] >= (r_worldmodel->numlightgridelems-1) ) {
			cArray = tempColorsArray[0];
			for( i = 0; i < numColors; i++, cArray += 3 )
				cArray[0] = cArray[1] = cArray[2] = 0;
			goto dynamic;
		}
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	for( j = 0; j < 3; j++ ) {
		ambient[j] = 0;
		diffuse[j] = 0;

		for ( i = 0; i < 4; i++ ) {
			ambient[j] += t[i*2] * r_worldmodel->lightgrid[ index[i] ].ambient[j];
			ambient[j] += t[i*2+1] * r_worldmodel->lightgrid[ index[i] + 1 ].ambient[j];

			diffuse[j] += t[i*2] * r_worldmodel->lightgrid[ index[i] ].diffuse[j];
			diffuse[j] += t[i*2+1] * r_worldmodel->lightgrid[ index[i] + 1 ].diffuse[j];
		}
	}

	VectorClear( dir );

	for( i = 0; i < 4; i++ ) {
		R_LatLongToNorm( r_worldmodel->lightgrid[ index[i] ].direction, tdir );
		VectorMA( dir, t[i*2], tdir, dir );
		R_LatLongToNorm( r_worldmodel->lightgrid[ index[i] + 1 ].direction, tdir );
		VectorMA( dir, t[i*2+1], tdir, dir );
	}

	VectorNormalizeFast( dir );

	dot = bound( 0.0f, r_ambientscale->value, 1.0f ) * glState.pow2MapOvrbr;
	VectorScale( ambient, dot, ambient );

	dot = bound( 0.0f, r_directedscale->value, 1.0f ) * glState.pow2MapOvrbr;
	VectorScale( diffuse, dot, diffuse );

	if( e->flags & RF_MINLIGHT ) {
		for( i = 0; i < 3; i++ )
			if( ambient[i] > 0.1 )
				break;

		if( i == 3 ) {
			ambient[0] = 0.1f;
			ambient[1] = 0.1f;
			ambient[2] = 0.1f;
		}
	}

	// rotate direction
	Matrix_TransformVector( e->axis, dir, direction );

	cArray = tempColorsArray[0];
	for( i = 0; i < numColors; i++, cArray += 3 ) {
		dot = DotProduct( normalsArray[i], direction );
		if( dot <= 0 )
			VectorCopy( ambient, cArray );
		else
			VectorMA( ambient, dot, diffuse, cArray );
	}

dynamic:
	// add dynamic lights
	if( r_dynamiclight->integer && r_numDlights ) {
		int lnum;
		dlight_t *dl;
		float dist, add, intensity8;

		dl = r_dlights;
		for( lnum = 0; lnum < r_numDlights; lnum++, dl++ ) {
			// translate
			VectorSubtract( dl->origin, e->origin, dir );
			dist = VectorLength( dir );

			if( dist > dl->intensity + e->model->radius * e->scale )
				continue;

			// rotate
			Matrix_TransformVector( e->axis, dir, dlorigin );

			intensity8 = dl->intensity * 8;

			cArray = tempColorsArray[0];
			for( i = 0; i < numColors; i++, cArray += 3 ) {
				VectorSubtract( dlorigin, vertsArray[i], dir );
				add = DotProduct( normalsArray[i], dir );

				if( add > 0 ) {
					dot = DotProduct( dir, dir );
					add *= (intensity8 / dot) * Q_RSqrt( dot );
					VectorMA( cArray, add, dl->color, cArray );
				}
			}
		}
	}

	cArray = tempColorsArray[0];
	for( i = 0; i < numColors; i++, bArray += 4, cArray += 3 ) {
		bArray[0] = R_FloatToByte( bound (0.0f, cArray[0], 1.0f) );
		bArray[1] = R_FloatToByte( bound (0.0f, cArray[1], 1.0f) );
		bArray[2] = R_FloatToByte( bound (0.0f, cArray[2], 1.0f) );
		bArray[3] = 255;
	}
}


/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

qboolean r_ligtmapsPacking;
static qbyte *r_lightmapBuffer;
static int r_lightmapBufferSize;
static int r_numUploadedLightmaps;

/*
=======================
R_BuildLightmap
=======================
*/
void R_BuildLightmap( qbyte *data, qbyte *dest, int blockWidth )
{
	int x, y;
	float scale;
	qbyte *rgba;
	float rgb[3], scaled[3];

	if ( !data || r_fullbright->integer ) {
		for( y = 0; y < LIGHTMAP_HEIGHT; y++, dest  )
			memset( dest + y * blockWidth, 255, LIGHTMAP_WIDTH * 4 );
		return;
	}

	scale = glState.pow2MapOvrbr;
	for( y = 0; y < LIGHTMAP_HEIGHT; y++ ) {
		for( x = 0, rgba = dest + y * blockWidth; x < LIGHTMAP_WIDTH; x++, rgba += 4 ) {
			scaled[0] = data[(y*LIGHTMAP_WIDTH + x) * LIGHTMAP_BYTES + 0] * scale;
			scaled[1] = data[(y*LIGHTMAP_WIDTH + x) * LIGHTMAP_BYTES + 1] * scale;
			scaled[2] = data[(y*LIGHTMAP_WIDTH + x) * LIGHTMAP_BYTES + 2] * scale;

			ColorNormalize( scaled, rgb );

			rgba[0] = ( qbyte )( rgb[0]*255 );
			rgba[1] = ( qbyte )( rgb[1]*255 );
			rgba[2] = ( qbyte )( rgb[2]*255 );
		}
	}
}

/*
=======================
R_PackLightmaps
=======================
*/
int R_PackLightmaps( int num, qbyte *data, mlightmapRect_t *rects )
{
	int i, x, y, root;
	qbyte *block;
	image_t *image;
	int	rectX, rectY, size;
	int maxX, maxY, max, xStride;
	double tw, th, tx, ty;

	maxX = glConfig.maxTextureSize / LIGHTMAP_WIDTH;
	maxY = glConfig.maxTextureSize / LIGHTMAP_HEIGHT;
	max = maxY;
	if( maxY > maxX )
		max = maxX;

	Com_DPrintf( "Packing %i lightmap(s) -> ", num );

	if( !max || num == 1 || !r_ligtmapsPacking/* || !r_packlightmaps->integer*/ ) {
		// process as it is
		R_BuildLightmap( data, r_lightmapBuffer, LIGHTMAP_WIDTH * 4 );

		image = R_LoadPic( va( "*lm%i", r_numUploadedLightmaps ), (qbyte **)(&r_lightmapBuffer), LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, IT_CLAMP|IT_NOPICMIP|IT_NOMIPMAP, 3 );

		r_lightmapTextures[r_numUploadedLightmaps] = image;
		rects[0].texNum = r_numUploadedLightmaps;

		// this is not a real texture matrix, but who cares?
		rects[0].texMatrix[0][0] = 1; rects[0].texMatrix[0][1] = 0;
		rects[0].texMatrix[1][0] = 1; rects[0].texMatrix[1][1] = 0;

		Com_DPrintf( "%ix%i\n", 1, 1 );

		r_numUploadedLightmaps++;
		return 1;
	}

	// find the nearest square block size
	root = ( int )sqrt( num );
	if( root > max )
		root = max;

	// keep row size a power of two
	for( i = 1; i < root; i <<= 1 );
	if( i > root )
		i >>= 1;
	root = i;

	num -= root * root;
	rectX = rectY = root;

	if( maxY > maxX ) {
		for( ; ( num >= root ) && ( rectY < maxY ); rectY++, num -= root );

		// sample down if not a power of two
		for( y = 1; y < rectY; y <<= 1 );
		if( y > rectY )
			y >>= 1;
		rectY = y;
	} else {
		for( ; ( num >= root ) && ( rectX < maxX ); rectX++, num -= root );

		// sample down if not a power of two
		for( x = 1; x < rectX; x <<= 1 );
		if( x > rectX )
			x >>= 1;
		rectX = x;
	}

	tw = 1.0 / (double)rectX;
	th = 1.0 / (double)rectY;

	xStride = LIGHTMAP_WIDTH * 4;
	size = (rectX * LIGHTMAP_WIDTH) * (rectY * LIGHTMAP_HEIGHT) * 4;
	if( size > r_lightmapBufferSize ) {
		if( r_lightmapBuffer )
			Mem_TempFree( r_lightmapBuffer );
		r_lightmapBuffer = Mem_TempMallocExt( size, 0 );
		memset (r_lightmapBuffer, 255, size );
		r_lightmapBufferSize = size;
	}

	block = r_lightmapBuffer;
	for( y = 0, ty = 0.0, num = 0; y < rectY; y++, ty += th, block += rectX * xStride * LIGHTMAP_HEIGHT  ) {
		for( x = 0, tx = 0.0; x < rectX; x++, tx += tw, num++ ) {
			R_BuildLightmap( data + num * LIGHTMAP_SIZE, block + x * xStride, rectX * xStride );

			// this is not a real texture matrix, but who cares?
			rects[num].texMatrix[0][0] = tw; rects[num].texMatrix[0][1] = tx;
			rects[num].texMatrix[1][0] = th; rects[num].texMatrix[1][1] = ty;
		}
	}

	image = R_LoadPic( va( "*lm%i", r_numUploadedLightmaps ), (qbyte **)(&r_lightmapBuffer), rectX * LIGHTMAP_WIDTH, rectY * LIGHTMAP_HEIGHT, IT_CLAMP|IT_NOPICMIP|IT_NOMIPMAP, 3 );

	r_lightmapTextures[r_numUploadedLightmaps] = image;
	for( i = 0; i < num; i++ )
		rects[i].texNum = r_numUploadedLightmaps;

	Com_DPrintf( "%ix%i\n", rectX, rectY );

	r_numUploadedLightmaps++;
	return num;
}

/*
=======================
R_BuildLightmaps
=======================
*/
void R_BuildLightmaps( int numLightmaps, qbyte *data, mlightmapRect_t *rects )
{
	int i;

	r_lightmapBufferSize = LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 4;
	r_lightmapBuffer = Mem_TempMalloc( r_lightmapBufferSize );
	r_numUploadedLightmaps = 0;

	for( i = 0; i < numLightmaps; )
		i += R_PackLightmaps( numLightmaps - i, data + i * LIGHTMAP_SIZE, &rects[i] );

	if( r_lightmapBuffer )
		Mem_TempFree( r_lightmapBuffer );

	Com_DPrintf( "Packed %i lightmaps into %i texture(s)\n", numLightmaps, r_numUploadedLightmaps );
}
