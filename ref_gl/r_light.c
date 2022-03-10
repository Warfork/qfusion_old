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
R_SurfPotentiallyLit
=============
*/
qboolean R_SurfPotentiallyLit( msurface_t *surf )
{
	shader_t *shader;

	if( surf->shaderref->flags & (SURF_SKY|SURF_NODLIGHT|SURF_NODRAW) )
		return qtrue;

	shader = surf->shaderref->shader;
	if( (shader->flags & (SHADER_SKY|SHADER_FLARE)) || !shader->numpasses )
		return qfalse;

	return ( surf->mesh && (surf->facetype != FACETYPE_FLARE) );
}

/*
=============
R_LightBounds
=============
*/
void R_LightBounds( const vec3_t origin, float intensity, vec3_t mins, vec3_t maxs ) {
	VectorSet( mins, origin[0] - intensity, origin[1] - intensity, origin[2] - intensity/* - intensity*/ );
	VectorSet( maxs, origin[0] + intensity, origin[1] + intensity, origin[2] + intensity/* + intensity*/ );
}

/*
=================
R_MarkLightsWorldNode

Mark faces touched by dynamic lights using BSP tree.
Note that invisible lights are culled away by frustum and
visible lights are clipped by leafs' boundaries, leaving
almost a perfect bounding box.
=================
*/
void R_MarkLightsWorldNode( void )
{
	int			c, k, bit;
	int			stackdepth;
	float		dist;
	dlight_t	*lt;
	vec3_t		mins, maxs;
	mleaf_t		*pleaf;
	msurface_t	*surf, **mark;
	mnode_t		*node, *localstack[2048];

	if( !r_dynamiclight->integer || !r_numDlights || r_fullbright->value )
		return;

	for( k = 0, bit = 1, lt = r_dlights; k < r_numDlights; k++, bit <<= 1, lt++ ) {
		if( R_CullBox( lt->mins, lt->maxs, 15 ) )
			continue;

		VectorCopy( lt->origin, mins );
		VectorCopy( lt->origin, maxs );

		for( node = r_worldmodel->nodes, stackdepth = 0; ; ) {
			if( node->visframe != r_visframecount )
				goto nextNodeOnStack;
			if( node->plane == NULL ) {
				pleaf = ( mleaf_t * )node;

				if( r_refdef.areabits ) {
					if(! (r_refdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
						goto nextNodeOnStack;
				}
				if( !BoundsIntersect( pleaf->mins, pleaf->maxs, lt->mins, lt->maxs ) || !pleaf->firstVisSurface )
					goto nextNodeOnStack;

				// mark the polygons
				if( pleaf->firstLitSurface ) {
					mark = pleaf->firstLitSurface;
					do {
						surf = *mark++;
						if( BoundsIntersect( surf->mins, surf->maxs, lt->mins, lt->maxs ) )	{
							if( surf->facetype == FACETYPE_PLANAR ) {
								dist = 
									(lt->origin[0] - surf->mesh->xyzArray[0][0]) * surf->origin[0] +
									(lt->origin[1] - surf->mesh->xyzArray[0][1]) * surf->origin[1] + 
									(lt->origin[2] - surf->mesh->xyzArray[0][2]) * surf->origin[2];
								if( /*dist <= 0 || */dist >= lt->intensity )
									continue;	// how lucky...
							}
							if( surf->dlightframe != r_framecount ) {
								surf->dlightbits = 0;
								surf->dlightframe = r_framecount;
							}
							surf->dlightbits |= bit;
						}
					} while( *mark );
				}

				for( c = 0; c < 3; c++ ) {
					mins[c] = min( mins[c], pleaf->mins[c] );
					maxs[c] = max( maxs[c], pleaf->maxs[c] );
				}
nextNodeOnStack:
				if( !stackdepth )
					break;
				node = localstack[--stackdepth];
				continue;
			}

			// FIXME: use BOX_ON_PLANE_SIDE ?
			dist = PlaneDiff( lt->origin, node->plane );
			if( dist > lt->intensity ) {
				node = node->children[0];
				continue;
			}

			// go down both sides if needed
			if( (dist >= -lt->intensity) && (stackdepth < sizeof(localstack)/sizeof(mnode_t *)) )
				localstack[stackdepth++] = node->children[0];
			node = node->children[1];
		}

		for( c = 0; c < 3; c++ ) {
			if( mins[c] > lt->mins[c] ) lt->mins[c] = mins[c];
			if( maxs[c] < lt->maxs[c] ) lt->maxs[c] = maxs[c];
		}
	}
}

/*
=================
R_MarkLightsBmodel
=================
*/
void R_MarkLightsBmodel( const entity_t *e, const vec3_t mins, const vec3_t maxs )
{
	int			k, c, bit;
	dlight_t	*lt;
	model_t		*model = e->model;
	msurface_t	*psurf;

	if( !r_dynamiclight->integer || !r_numDlights || r_fullbright->integer )
		return;

	for( k = 0, bit = 1, lt = r_dlights; k < r_numDlights; k++, bit <<= 1, lt++ ) {
		if( !BoundsIntersect( mins, maxs, lt->mins, lt->maxs ) )
			continue;

		for( c = 0, psurf = model->firstmodelsurface; c < model->nummodelsurfaces; c++, psurf++ ) {
			if( R_SurfPotentiallyLit( psurf ) ) {
				if( psurf->dlightframe != r_framecount ) {
					psurf->dlightbits = 0;
					psurf->dlightframe = r_framecount;
				}
				psurf->dlightbits |= bit;
			}
		}
	}
}

/*
=================
R_AddDynamicLights
=================
*/
void R_AddDynamicLights( unsigned int dlightbits, GLenum func, GLenum sfactor, GLenum dfactor )
{
	int i, j, numTempIndexes;
	qboolean cullAway;
	dlight_t *light;
	image_t *image;
	shader_t *shader;
	vec3_t tvec, dlorigin, normal;
	index_t tempIndexesArray[MAX_ARRAY_INDEXES];
	float inverseIntensity, *v1, *v2, *v3, dist;
	mat4x4_t m = { 0.5, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 1, 0, 0.5, 0.5, 0, 1 }, m1, m2;
	GLfloat xyFallof[2][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 } };
	GLfloat zFallof[2][4] = { { 0, 0, 1, 0 }, { 0, 0, 0, 0 } };

	for( i = 0; i < min( glConfig.maxTextureUnits, 2 ); i++ ) {
		GL_SelectTexture( i );
		GL_TexEnv( GL_MODULATE );
		qglEnable( GL_BLEND );
		qglDisable( GL_ALPHA_TEST );
		qglDepthMask( GL_FALSE );
		qglDepthFunc( func );
		qglBlendFunc( sfactor, dfactor );

		qglTexGeni( GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		qglTexGeni( GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		qglEnable( GL_TEXTURE_GEN_S );
		qglEnable( GL_TEXTURE_GEN_T );
	}
	if( glConfig.maxTextureUnits > 1 ) {
		qglEnable( GL_TEXTURE_2D );
		numColors = 0;
	} else {
		qglEnableClientState( GL_COLOR_ARRAY );
		numColors = numVerts;
	}

	for( i = 0, light = r_dlights; i < r_numDlights; i++, light++ ) {
		if( !(dlightbits & (1<<i) ) )
			continue;		// not lit by this light

		VectorSubtract( light->origin, currententity->origin, dlorigin );
		if( !Matrix_Compare( currententity->axis, axis_identity ) ) {
			VectorCopy( dlorigin, tvec );
			Matrix_TransformVector( currententity->axis, tvec, dlorigin );
		}

		shader = light->shader;
		if( shader && (shader->flags & SHADER_CULL_BACK) )
			cullAway = qtrue;
		else
			cullAway = qfalse;

		numTempIndexes = 0;
		if( cullAway ) {
			for( j = 0; j < numIndexes; j += 3 ) {
				v1 = ( float * )( vertsArray + indexesArray[j+0] );
				v2 = ( float * )( vertsArray + indexesArray[j+1] );
				v3 = ( float * )( vertsArray + indexesArray[j+2] );

				normal[0] = (v1[1] - v2[1]) * (v3[2] - v2[2]) - (v1[2] - v2[2]) * (v3[1] - v2[1]);
				normal[1] = (v1[2] - v2[2]) * (v3[0] - v2[0]) - (v1[0] - v2[0]) * (v3[2] - v2[2]);
				normal[2] = (v1[0] - v2[0]) * (v3[1] - v2[1]) - (v1[1] - v2[1]) * (v3[0] - v2[0]);
				dist = (dlorigin[0] - v1[0]) * normal[0] + (dlorigin[1] - v1[1]) * normal[1] + (dlorigin[2] - v1[2]) * normal[2];

				if( dist <= 0 || dist * Q_RSqrt( DotProduct( normal, normal ) ) >= light->intensity )
					continue;

				tempIndexesArray[numTempIndexes++] = indexesArray[j+0];
				tempIndexesArray[numTempIndexes++] = indexesArray[j+1];
				tempIndexesArray[numTempIndexes++] = indexesArray[j+2];
			}
			if( !numTempIndexes )
				continue;
		}

		inverseIntensity = 1 / light->intensity;

		image = r_dlighttexture;
		Matrix4_Copy( m, m1 );
		if( shader && shader->numpasses ) {
			if( shader->flags & SHADER_VIDEOMAP )
				R_UploadCinematicShader( shader );

			image = R_ShaderpassTex( shader->passes, 0 );
			if( image == r_notexture )
				image = r_dlighttexture;
			R_ApplyTCMods( shader->passes, m1 );
		}
		GL_Bind( 0, image );
		GL_LoadTexMatrix( m1 );

		xyFallof[0][0] = inverseIntensity;
		xyFallof[0][3] = -dlorigin[0] * inverseIntensity;
		xyFallof[1][1] = inverseIntensity;
		xyFallof[1][3] = -dlorigin[1] * inverseIntensity;
		qglTexGenfv( GL_S, GL_OBJECT_PLANE, xyFallof[0] );
		qglTexGenfv( GL_T, GL_OBJECT_PLANE, xyFallof[1] );

		if( glConfig.maxTextureUnits > 1 ) {
			qglColor3fv( light->color );

			image = r_dlighttexture;
			Matrix4_Copy( m, m2 );
			if( shader && shader->numpasses > 1 ) {
				image = R_ShaderpassTex( shader->passes + 1, 1 );
				if( image == r_notexture )
					image = r_dlighttexture;
				R_ApplyTCMods( shader->passes + 1, m2 );
			}
			GL_Bind( 1, image );
			GL_LoadTexMatrix( m2 );

			zFallof[0][2] = inverseIntensity;
			zFallof[0][3] = -dlorigin[2] * inverseIntensity;
			qglTexGenfv( GL_S, GL_OBJECT_PLANE, zFallof[0] );
			qglTexGenfv( GL_T, GL_OBJECT_PLANE, zFallof[1] );
		} else {
			qbyte *bColor;

			for( j = 0, bColor = colorArray[0]; j < numColors; j++, bColor += 4 ) {
				dist = vertsArray[j][2] - dlorigin[2];
				if( dist < 0 )
					dist = -dist;

				if( dist < light->intensity ) {
					bColor[0] = R_FloatToByte( light->color[0] );
					bColor[1] = R_FloatToByte( light->color[1] );
					bColor[2] = R_FloatToByte( light->color[2] );
				} else {
					dist = Q_RSqrt( dist * dist - light->intensity * light->intensity );
					clamp( dist, 0, 1 );
					bColor[0] = R_FloatToByte( dist * light->color[0] );
					bColor[1] = R_FloatToByte( dist * light->color[1] );
					bColor[2] = R_FloatToByte( dist * light->color[2] );
				}
				bColor[3] = 255;
			}

			if( glConfig.vertexBufferObject ) {
				qglBindBufferARB( GL_ARRAY_BUFFER_ARB, r_vertexBufferObjects[VBO_COLORS] );
				qglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( byte_vec4_t ), colorArray, GL_STREAM_DRAW_ARB );
				qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
			}
		}

		if( numTempIndexes ) {
			if( glConfig.drawRangeElements )
				qglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVerts, numTempIndexes, GL_UNSIGNED_INT, tempIndexesArray );
			else
				qglDrawElements( GL_TRIANGLES, numTempIndexes, GL_UNSIGNED_INT, tempIndexesArray );
		} else {
			if( glConfig.drawRangeElements )
				qglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVerts, numIndexes, GL_UNSIGNED_INT, indexesArray );
			else
				qglDrawElements( GL_TRIANGLES, numIndexes, GL_UNSIGNED_INT, indexesArray );
		}
	}

	if( glConfig.maxTextureUnits > 1 ) {
		qglDisable( GL_TEXTURE_2D );
		qglDisable( GL_TEXTURE_GEN_S );
		qglDisable( GL_TEXTURE_GEN_T );
		GL_SelectTexture( 0 );
	} else {
		qglDisableClientState( GL_COLOR_ARRAY );
	}

	qglDisable( GL_TEXTURE_GEN_S );
	qglDisable( GL_TEXTURE_GEN_T );
}


//===================================================================

/*
===============
R_LightForOrigin
===============
*/
void R_LightForOrigin( vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius )
{
	int i, j;
	int k, s;
	int vi[3], index[4];
	float dot, t[8];
	vec3_t vf, vf2, tdir;
	vec3_t ambientLocal, diffuseLocal;
	vec_t *gridSize, *gridMins;
	int	*gridBounds;

	VectorSet( ambientLocal, 0, 0, 0 );
	VectorSet( diffuseLocal, 0, 0, 0 );

	if( !r_worldmodel || (r_refdef.rdflags & RDF_NOWORLDMODEL) ||
		 !r_worldmodel->lightgrid || !r_worldmodel->numlightgridelems ) {
		VectorSet( dir, 0.5, 0.2, -1 );
		VectorNormalizeFast( dir );
		goto dynamic;
	}

	gridSize = r_worldmodel->submodels[0].gridSize;
	gridMins = r_worldmodel->submodels[0].gridMins;
	gridBounds = r_worldmodel->submodels[0].gridBounds;

	for( i = 0; i < 3; i++ ) {
		vf[i] = (origin[i] - gridMins[i]) / gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor(vf[i]);
		vf2[i] = 1.0f - vf[i];
	}

	index[0] = vi[2] * gridBounds[3] + vi[1] * gridBounds[0] + vi[0];
	index[1] = index[0] + gridBounds[0];
	index[2] = index[0] + gridBounds[3];
	index[3] = index[2] + gridBounds[0];

	for( i = 0; i < 4; i++ ) {
		if( index[i] < 0 || index[i] >= (r_worldmodel->numlightarrayelems-1) ) {
			VectorSet( dir, 0.5, 0.2, -1 );
			VectorNormalizeFast( dir );
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

	VectorClear( dir );

	for( i = 0; i < 4; i++ ) {
		R_LatLongToNorm( r_worldmodel->lightarray[ index[i] ]->direction, tdir );
		VectorScale( tdir, t[i*2], tdir );
		for( k = 0; k < MAX_LIGHTMAPS && (s = r_worldmodel->lightarray[ index[i] ]->styles[k]) != 255; k++ ) {
			dir[0] += r_lightStyles[s].rgb[0] * tdir[0];
			dir[1] += r_lightStyles[s].rgb[1] * tdir[1];
			dir[2] += r_lightStyles[s].rgb[2] * tdir[2];
		}

		R_LatLongToNorm( r_worldmodel->lightarray[ index[i] + 1 ]->direction, tdir );
		VectorScale( tdir, t[i*2+1], tdir );
		for( k = 0; k < MAX_LIGHTMAPS && (s = r_worldmodel->lightarray[ index[i] + 1 ]->styles[k]) != 255; k++ ) {
			dir[0] += r_lightStyles[s].rgb[0] * tdir[0];
			dir[1] += r_lightStyles[s].rgb[1] * tdir[1];
			dir[2] += r_lightStyles[s].rgb[2] * tdir[2];
		}
	}

	VectorNormalizeFast( dir );

	for( j = 0; j < 3; j++ ) {
		if( ambient ) {
			for( i = 0; i < 4; i++ ) {
				for( k = 0; k < MAX_LIGHTMAPS; k++ ) {
					if( (s = r_worldmodel->lightarray[ index[i] ]->styles[k]) != 255 )
						ambientLocal[j] += t[i*2] * r_worldmodel->lightarray[ index[i] ]->ambient[k][j] * r_lightStyles[s].rgb[j];
					if( (s = r_worldmodel->lightarray[ index[i] + 1 ]->styles[k]) != 255 )
						ambientLocal[j] += t[i*2+1] * r_worldmodel->lightarray[ index[i] + 1 ]->ambient[k][j] * r_lightStyles[s].rgb[j];
				}
			}
		}
		if( diffuse || radius ) {
			for( i = 0; i < 4; i++ ) {
				for( k = 0; k < MAX_LIGHTMAPS; k++ ) {
					if( (s = r_worldmodel->lightarray[ index[i] ]->styles[k]) != 255 )
						diffuseLocal[j] += t[i*2] * r_worldmodel->lightarray[ index[i] ]->diffuse[k][j] * r_lightStyles[s].rgb[j];
					if( (s = r_worldmodel->lightarray[ index[i] + 1 ]->styles[k]) != 255 )
						diffuseLocal[j] += t[i*2+1] * r_worldmodel->lightarray[ index[i] + 1 ]->diffuse[k][j] * r_lightStyles[s].rgb[j];
				}
			}
		}
	}

dynamic:
	// add dynamic lights
	if( radius && r_dynamiclight->integer && r_numDlights ) {
		int lnum;
		dlight_t *dl;
		float dist, add;
		vec3_t direction;

		for( i = 0; i < 3; i++ )
			dir[i] *= diffuseLocal[i] * (1.0 / 255.0);

		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ ) {
			if( !BoundsAndSphereIntersect( dl->mins, dl->maxs, origin, radius ) )
				continue;

			VectorSubtract( dl->origin, origin, direction );
			dist = VectorLength( direction );

			if( !dist || dist > dl->intensity + radius )
				continue;

			dist = 1.0 / dist;
			add = (dl->intensity + radius) * 2 * (dist * dist);
			for( i = 0; i < 3; i++ ) {
				dot = dl->color[i] * add;
				diffuseLocal[i] += dot;
				ambientLocal[i] += dot * 0.05;
				dir[i] += dot * direction[i] * dist;
			}
		}

		VectorNormalizeFast( dir );
	}

	if( ambient ) {
		dot = bound( 0.0f, r_ambientscale->value, 1.0f ) * glState.pow2MapOvrbr;
		for( i = 0; i < 3; i++ ) {
			ambient[i] = ambientLocal[i] * dot;
			clamp( ambient[i], 0, 1 );
		}
		ambient[3] = 1.0f;
	}

	if( diffuse ) {
		dot = bound( 0.0f, r_directedscale->value, 1.0f ) * glState.pow2MapOvrbr;
		for( i = 0; i < 3; i++ ) {
			diffuse[i] = diffuseLocal[i] * dot;
			clamp( diffuse[i], 0, 1 );
		}
		diffuse[3] = 1.0f;
	}
}

/*
===============
R_LightForEntity
===============
*/
void R_LightForEntity( entity_t *e, qbyte *bArray )
{
	int i;
	float dot, *cArray;
	vec4_t ambient, diffuse;
	vec3_t dlorigin, dir, direction;
	vec3_t tempColorsArray[MAX_ARRAY_VERTS];

	if( (e->flags & RF_FULLBRIGHT) || r_fullbright->value )
		return;

	// probably weird shader, see mpteam4 for example
	if( !e->model || (e->model->type == mod_brush) )
		return;

	R_LightForOrigin( e->lightingOrigin, dir, ambient, diffuse, 0 );

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

	// add dynamic lights
	if( r_dynamiclight->integer && r_numDlights ) {
		int lnum;
		dlight_t *dl;
		float dist, add, intensity8;

		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ ) {
			// translate
			VectorSubtract( dl->origin, e->origin, dir );
			dist = VectorLength( dir );

			if( !dist || dist > dl->intensity + e->model->radius * e->scale )
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
		bArray[0] = R_FloatToByte( bound( 0.0f, cArray[0], 1.0f ) );
		bArray[1] = R_FloatToByte( bound( 0.0f, cArray[1], 1.0f ) );
		bArray[2] = R_FloatToByte( bound( 0.0f, cArray[2], 1.0f ) );
	}
}

/*
===============
R_LightForEntityDot3
===============
*/
void R_LightForEntityDot3( entity_t *e, qbyte *bArray, vec3_t ambient, vec3_t diffuse )
{
	int i;
	vec3_t tvec;
	vec3_t dir, direction;

	if( (e->flags & RF_FULLBRIGHT) || r_fullbright->value ) {
		ambient[0] = ambient[1] = ambient[2] = ambient[3] = 1;
		diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = 1;
		memset( bArray, 255, sizeof( byte_vec4_t ) * numColors );
		return;
	}

	// probably weird shader, see mpteam4 for example
	if( !e->model || (e->model->type == mod_brush) ) {
		ambient[0] = ambient[1] = ambient[2] = ambient[3] = 1;
		diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = 1;
		memset( bArray, 255, sizeof( byte_vec4_t ) * numColors );
		return;
	}

	R_LightForOrigin( e->lightingOrigin, dir, ambient, diffuse, e->model->radius * e->scale );

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

	// [-1, 1] -> [0, 1] -> [0, 255]
	VectorScale( direction, 0.5f, direction );
	for( i = 0; i < numColors; i++, bArray += 4 ) {
		tvec[0] = direction[0] * sVectorsArray[i][0] + direction[1] * sVectorsArray[i][1] + direction[2] * sVectorsArray[i][2] + 0.5f;
		tvec[1] = direction[0] * tVectorsArray[i][0] + direction[1] * tVectorsArray[i][1] + direction[2] * tVectorsArray[i][2] + 0.5f;
		tvec[2] = direction[0] * normalsArray[i][0] + direction[1] * normalsArray[i][1] + direction[2] * normalsArray[i][2] + 0.5f;

		bArray[0] = R_FloatToByte( bound( 0.0f, tvec[0], 1.0f ) );
		bArray[1] = R_FloatToByte( bound( 0.0f, tvec[1], 1.0f ) );
		bArray[2] = R_FloatToByte( bound( 0.0f, tvec[2], 1.0f ) );
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
static int r_maxLightmapBlockSize;

/*
=======================
R_BuildLightmap
=======================
*/
static void R_BuildLightmap( int w, int h, const qbyte *data, qbyte *dest, int blockWidth )
{
	int x, y;
	float scale;
	qbyte *rgba;
	float rgb[3], scaled[3];

	if ( !data || r_fullbright->integer ) {
		for( y = 0; y < h; y++, dest  )
			memset( dest + y * blockWidth, 255, w * 4 );
		return;
	}

	scale = glState.pow2MapOvrbr;
	for( y = 0; y < h; y++ ) {
		for( x = 0, rgba = dest + y * blockWidth; x < w; x++, rgba += 4 ) {
			scaled[0] = data[(y*w + x) * LIGHTMAP_BYTES + 0] * scale;
			scaled[1] = data[(y*w + x) * LIGHTMAP_BYTES + 1] * scale;
			scaled[2] = data[(y*w + x) * LIGHTMAP_BYTES + 2] * scale;

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
static int R_PackLightmaps( int num, int w, int h, int size, const qbyte *data, mlightmapRect_t *rects )
{
	int i, x, y, root;
	qbyte *block;
	image_t *image;
	int	rectX, rectY, rectSize;
	int maxX, maxY, max, xStride;
	double tw, th, tx, ty;

	maxX = r_maxLightmapBlockSize / w;
	maxY = r_maxLightmapBlockSize / h;
	max = maxY;
	if( maxY > maxX )
		max = maxX;

	Com_DPrintf( "Packing %i lightmap(s) -> ", num );

	if( !max || num == 1 || !r_ligtmapsPacking/* || !r_packlightmaps->integer*/ ) {
		// process as it is
		R_BuildLightmap( w, h, data, r_lightmapBuffer, w * 4 );

		image = R_LoadPic( va( "*lm%i", r_numUploadedLightmaps ), (qbyte **)(&r_lightmapBuffer), w, h, IT_CLAMP|IT_NOPICMIP|IT_NOMIPMAP, LIGHTMAP_BYTES );

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

	xStride = w * 4;
	rectSize = (rectX * w) * (rectY * h) * 4;
	if( rectSize > r_lightmapBufferSize ) {
		if( r_lightmapBuffer )
			Mem_TempFree( r_lightmapBuffer );
		r_lightmapBuffer = Mem_TempMallocExt( rectSize, 0 );
		memset( r_lightmapBuffer, 255, rectSize );
		r_lightmapBufferSize = rectSize;
	}

	block = r_lightmapBuffer;
	for( y = 0, ty = 0.0, num = 0; y < rectY; y++, ty += th, block += rectX * xStride * h  ) {
		for( x = 0, tx = 0.0; x < rectX; x++, tx += tw, num++ ) {
			R_BuildLightmap( w, h, data + num * size, block + x * xStride, rectX * xStride );

			// this is not a real texture matrix, but who cares?
			rects[num].texMatrix[0][0] = tw; rects[num].texMatrix[0][1] = tx;
			rects[num].texMatrix[1][0] = th; rects[num].texMatrix[1][1] = ty;
		}
	}

	image = R_LoadPic( va( "*lm%i", r_numUploadedLightmaps ), (qbyte **)(&r_lightmapBuffer), rectX * w, rectY * h, IT_CLAMP|IT_NOPICMIP|IT_NOMIPMAP, LIGHTMAP_BYTES );

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
void R_BuildLightmaps( int numLightmaps, int w, int h, const qbyte *data, mlightmapRect_t *rects )
{
	int i;
	int size;

	for (size = 1; (size < r_maxlmblocksize->integer) && (size < glConfig.maxTextureSize); size <<= 1);

	r_maxLightmapBlockSize = size;
	size = w * h * LIGHTMAP_BYTES;
	r_lightmapBufferSize = w * h * 4;
	r_lightmapBuffer = Mem_TempMalloc( r_lightmapBufferSize );
	r_numUploadedLightmaps = 0;

	for( i = 0; i < numLightmaps; )
		i += R_PackLightmaps( numLightmaps - i, w, h, size, data + i * size, &rects[i] );

	if( r_lightmapBuffer )
		Mem_TempFree( r_lightmapBuffer );

	Com_DPrintf( "Packed %i lightmaps into %i texture(s)\n", numLightmaps, r_numUploadedLightmaps );
}

/*
=============================================================================

LIGHT STYLE MANAGEMENT

=============================================================================
*/

int		r_numSuperLightStyles;
superLightStyle_t	r_superLightStyles[MAX_SUPER_STYLES];

/*
=======================
R_InitLightStyles
=======================
*/
void R_InitLightStyles( void )
{
	int i;

	for( i = 0; i < MAX_LIGHTSTYLES; i++ ) {
		r_lightStyles[i].rgb[0] = 1;
		r_lightStyles[i].rgb[1] = 1;
		r_lightStyles[i].rgb[2] = 1;
	}
	r_numSuperLightStyles = 0;
}

/*
=======================
R_AddSuperLightStyle
=======================
*/
int R_AddSuperLightStyle( const int *lightmaps, const qbyte *lightmapStyles, const qbyte *vertexStyles )
{
	int i, j;
	superLightStyle_t *sls;

	for( i = 0, sls = r_superLightStyles; i < r_numSuperLightStyles; i++, sls++ ) {
		for( j = 0; j < MAX_LIGHTMAPS; j++ )
			if( sls->lightmapNum[j] != lightmaps[j] ||
				sls->lightmapStyles[j] != lightmapStyles[j] ||
				sls->vertexStyles[j] != vertexStyles[j] )
				break;
		if( j == MAX_LIGHTMAPS )
			return i;
	}

	if( r_numSuperLightStyles == MAX_SUPER_STYLES )
		Com_Error( ERR_DROP, "R_AddSuperLightStyle: r_numSuperLightStyles == MAX_SUPER_STYLES" );

	sls->features = 0;
	for( j = 0; j < MAX_LIGHTMAPS; j++ ) {
		sls->lightmapNum[j] = lightmaps[j];
		sls->lightmapStyles[j] = lightmapStyles[j];
		sls->vertexStyles[j] = vertexStyles[j];

		if( j ) {
			if( lightmapStyles[j] != 255 )
				sls->features |= (MF_LMCOORDS << j);
			if( vertexStyles[j] != 255 )
				sls->features |= (MF_COLORS << j);
		}
	}

	return r_numSuperLightStyles++;
}

/*
=======================
R_SuperLightStylesCmp

Compare function for qsort
=======================
*/
static int R_SuperLightStylesCmp( superLightStyle_t *sls1, superLightStyle_t *sls2 )
{
	int i;

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {		// compare lightmaps
		if( sls2->lightmapNum[i] > sls1->lightmapNum[i] )
			return 1;
		else if( sls1->lightmapNum[i] > sls2->lightmapNum[i] )
			return -1;
	}

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {		// compare lightmap styles
		if( sls2->lightmapStyles[i] > sls1->lightmapStyles[i] )
			return 1;
		else if( sls1->lightmapStyles[i] > sls2->lightmapStyles[i] )
			return -1;
	}

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {		// compare vertex styles
		if( sls2->vertexStyles[i] > sls1->vertexStyles[i] )
			return 1;
		else if( sls1->vertexStyles[i] > sls2->vertexStyles[i] )
			return -1;
	}

	return 0;	// equal
}

/*
=======================
R_SortSuperLightStyles
=======================
*/
void R_SortSuperLightStyles( void ) {
	qsort( r_superLightStyles, r_numSuperLightStyles, sizeof( superLightStyle_t ), (int (*)(const void *, const void *))R_SuperLightStylesCmp );
}
