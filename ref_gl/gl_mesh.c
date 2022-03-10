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
// gl_mesh.c: triangle model functions

#include "gl_local.h"

qboolean R_CullMd3Model( vec3_t bbox[8], entity_t *e );
void GL_DrawMd3AliasFrameLerp ( md3header_t *paliashdr, float backlerp );

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};


typedef float vec4_t[4];

static	vec4_t	s_lerped[MAX_VERTS];
//static	vec3_t	lerped[MAX_VERTS];
static	float	shadescale;

vec3_t	shadevector;
float	shadelight[3];

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h"
;

float	*shadedots = r_avertexnormal_dots[0];

// Vic
// light lerping - pox@planetquake.com
float *shadedots2 = r_avertexnormal_dots[0];
float lightlerpoffset;
// Vic

void GL_LerpVerts( int nverts, dtrivertx_t *v, dtrivertx_t *ov, float *lerp, float move[3], float frontv[3], float backv[3] )
{
	int i;

	for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4)
	{
		lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0];
		lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1];
		lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2];
	}
}

/*
=============
GL_DrawAliasFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
void GL_DrawAliasFrameLerp (dmdl_t *paliashdr, float backlerp)
{
	daliasframe_t	*frame, *oldframe;
	dtrivertx_t	*v, *ov, *verts;
	int		*order;
	int		count;
	float	frontlerp;
	float	alpha;
	vec3_t	move, delta;
	vec3_t	frontv, backv;
	int		i;
	int		index_xyz;
	float	*lerp, l;

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->frame * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	if (currententity->flags & RF_TRANSLUCENT)
		alpha = currententity->alpha;
	else
		alpha = 1.0;

	frontlerp = 1.0 - backlerp;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract (currententity->oldorigin, currententity->origin, delta);

	move[0] = DotProduct (delta, currententity->angleVectors[0]);	// forward
	move[1] = -DotProduct (delta, currententity->angleVectors[1]);	// left
	move[2] = DotProduct (delta, currententity->angleVectors[2]);	// up

	VectorAdd (move, oldframe->translate, move);

	for (i=0 ; i<3 ; i++)
	{
		move[i] = backlerp*move[i] + frontlerp*frame->translate[i];
	}

	for (i=0 ; i<3 ; i++)
	{
		frontv[i] = frontlerp*frame->scale[i];
		backv[i] = backlerp*oldframe->scale[i];
	}

	lerp = s_lerped[0];

	GL_LerpVerts( paliashdr->num_xyz, v, ov, lerp, move, frontv, backv );
	
	while (count = *order++)
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		} else {
			qglBegin (GL_TRIANGLE_STRIP);
		}
		
		do
		{
			// texture coordinates come from the draw list
			if ( !( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE)) ) {
				qglTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			}
			
			index_xyz = order[2];
			order += 3;

			l = shadedots[verts[index_xyz].lightnormalindex];
			qglColor3f ( l, l, l );
			qglVertex3fv ( s_lerped[index_xyz] );
		} while (--count);

		qglEnd ();
	}
}


/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow (dmdl_t *paliashdr, int posenum)
{
	dtrivertx_t	*verts;
	int		*order;
	vec3_t	point;
	float	height, lheight;
	int		count;
	daliasframe_t	*frame;

	lheight = currententity->origin[2] - lightspot[2];

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->frame * paliashdr->framesize);
	verts = frame->verts;

	height = 0;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	height = -lheight + 0.01;

	// Vic
	if( gl_state.stencil_enabled ) {
		qglEnable ( GL_STENCIL_TEST );
		qglStencilFunc ( GL_EQUAL, 1, 2 );
		qglStencilOp ( GL_KEEP, GL_KEEP, GL_INCR );
	}
	// Vic

	while (count = *order++)
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		}
		else
			qglBegin (GL_TRIANGLE_STRIP);

		do
		{
			// normals and vertexes come from the frame list
			VectorCopy ( s_lerped[order[2]], point );

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;

			qglVertex3fv (point);

			order += 3;
		} while (--count);

		qglEnd ();
	}

	// Vic
	if( gl_state.stencil_enabled ) {
		qglDisable(GL_STENCIL_TEST);
	}
	// Vic
}

/*
** R_CullAliasModel
*/
static qboolean R_CullAliasModel( vec3_t bbox[8], entity_t *e )
{
	int			i;
	vec3_t		mins, maxs;
	dmdl_t		*paliashdr;
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;

	// Vic
	if ( currentmodel->aliastype == ALIASTYPE_MD3 ) {
		 return R_CullMd3Model ( bbox, e );
	}

	paliashdr = (dmdl_t *)currentmodel->extradata;

	if ( ( e->frame >= paliashdr->num_frames ) || ( e->frame < 0 ) )
	{
		Com_DPrintf ("R_CullAliasModel %s: no such frame %d\n", 
			currentmodel->name, e->frame);
		e->frame = 0;
	}
	if ( ( e->oldframe >= paliashdr->num_frames ) || ( e->oldframe < 0 ) )
	{
		Com_DPrintf ("R_CullAliasModel %s: no such oldframe %d\n", 
			currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + 
		                              paliashdr->ofs_frames +
									  e->frame * paliashdr->framesize);

	poldframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + 
		                              paliashdr->ofs_frames +
									  e->oldframe * paliashdr->framesize);

	/*
	** compute axially aligned mins and maxs
	*/
	if ( pframe == poldframe )
	{
		for ( i = 0; i < 3; i++ )
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i]*255;
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i]*255;

			oldmins[i]  = poldframe->translate[i];
			oldmaxs[i]  = oldmins[i] + poldframe->scale[i]*255;

			if ( thismins[i] < oldmins[i] )
				mins[i] = thismins[i];
			else
				mins[i] = oldmins[i];

			if ( thismaxs[i] > oldmaxs[i] )
				maxs[i] = thismaxs[i];
			else
				maxs[i] = oldmaxs[i];

			if ( currententity->scale != 1.0f ) {
				VectorScale ( mins, currententity->scale, mins );
				VectorScale ( maxs, currententity->scale, maxs );
			}
		}
	}

	/*
	** compute a full bounding box
	*/
	for ( i = 0; i < 8; i++ )
	{
		vec3_t   tmp;

		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		VectorCopy( tmp, bbox[i] );
	}

	/*
	** rotate the bounding box
	*/
	for ( i = 0; i < 8; i++ )
	{
		vec3_t tmp, angles, angleVectors[3];

		VectorCopy( e->angles, angles );
		angles[YAW] = -angles[YAW];
		AngleVectors( angles, angleVectors[0], angleVectors[1], angleVectors[2] );

		VectorCopy( bbox[i], tmp );

		bbox[i][0] = DotProduct( angleVectors[0], tmp );
		bbox[i][1] = -DotProduct( angleVectors[1], tmp );
		bbox[i][2] = DotProduct( angleVectors[2], tmp );

		VectorAdd( e->origin, bbox[i], bbox[i] );
	}

	{
		int p, f, aggregatemask = ~0;

		for ( p = 0; p < 8; p++ )
		{
			int mask = 0;

			for ( f = 0; f < 4; f++ )
			{
				float dp = DotProduct( frustum[f].normal, bbox[p] );

				if ( ( dp - frustum[f].dist ) < 0 )
				{
					mask |= ( 1 << f );
				}
			}

			aggregatemask &= mask;
		}

		if ( aggregatemask )
		{
			return true;
		}

		return false;
	}
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e)
{
	dmdl_t		*paliashdr;
	vec3_t		bbox[8];
	image_t		*skin;
	md3header_t *md3header;		// Vic

	if ( !e->scale ) {
		return;
	}

	if ( e->flags & RF_WEAPONMODEL )
	{
		if ( r_lefthand->value == 2 )
			return;
	}

	if ( !( e->flags & RF_WEAPONMODEL ) )
	{
		if ( R_CullAliasModel( bbox, e ) )
			return;
	}

	AngleVectors( e->angles, e->angleVectors[0], e->angleVectors[1], e->angleVectors[2] );

	if ( currentmodel->aliastype == ALIASTYPE_MD2 )
		paliashdr = (dmdl_t *)currentmodel->extradata;
	else if ( currentmodel->aliastype == ALIASTYPE_MD3 )
		md3header = (md3header_t *)currentmodel->extradata;

	//
	// locate the proper data
	//

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	if ( currentmodel->aliastype == ALIASTYPE_MD2 )
		c_alias_polys += paliashdr->num_tris;

	//
	// draw all the triangles
	//
	if (currententity->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	if ( ( currententity->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) )
	{
		extern void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar );

		qglMatrixMode( GL_PROJECTION );
		qglPushMatrix();
		qglLoadIdentity();
		qglScalef( -1, 1, 1 );
		MYgluPerspective( r_newrefdef.fov_y, ( float ) r_newrefdef.width / r_newrefdef.height, 4, 12288);
		qglMatrixMode( GL_MODELVIEW );

		qglCullFace( GL_BACK );
	}

	qglDepthFunc ( GL_LEQUAL );
	qglDepthMask ( GL_TRUE );

    qglPushMatrix ();

    qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

	if ( e->scale != 1.0f )
		qglScalef ( e->scale, e->scale, e->scale );

    qglRotatef (e->angles[1],  0, 0, 1);
    qglRotatef (e->angles[0],  0, 1, 0);		// sigh.
    qglRotatef (-e->angles[2],  1, 0, 0);

	if ( currentmodel->aliastype == ALIASTYPE_MD2 )
	{
		// select skin
		if (currententity->skin)
			skin = currententity->skin->pass[0].texref;	// custom player skin
		else
		{
			if (currententity->skinnum >= MAX_MD2SKINS)
				skin = currentmodel->skins[0][0]->pass[0].texref;
			else
			{
				skin = currentmodel->skins[0][currententity->skinnum]->pass[0].texref;
				if (!skin)
					skin = currentmodel->skins[0][0]->pass[0].texref;
			}
		}
		if (!skin)
			skin = r_notexture;	// fallback...
		GL_Bind(skin->texnum);
	}

	GL_TexEnv( GL_MODULATE );

	// draw it
	if ( currentmodel->aliastype == ALIASTYPE_MD2 )
	{
		if ( (currententity->frame >= paliashdr->num_frames) 
			|| (currententity->frame < 0) )
		{
			Com_DPrintf ("R_DrawAliasModel %s: no such frame %d\n",
				currentmodel->name, currententity->frame);
			currententity->frame = 0;
			currententity->oldframe = 0;
		}

		if ( (currententity->oldframe >= paliashdr->num_frames)
			|| (currententity->oldframe < 0))
		{
			Com_DPrintf ("R_DrawAliasModel %s: no such oldframe %d\n",
				currentmodel->name, currententity->oldframe);
			currententity->frame = 0;
			currententity->oldframe = 0;
		}
	}

	if ( !r_lerpmodels->value ) {
		currententity->backlerp = 0;
	}

	if ( currentmodel->aliastype == ALIASTYPE_MD2 ) {
		GL_DrawAliasFrameLerp (paliashdr, currententity->backlerp);
	} else if ( currentmodel->aliastype == ALIASTYPE_MD3 ) {
		GL_DrawMd3AliasFrameLerp ( md3header, currententity->backlerp );
	}

	qglPopMatrix ();

	if ( ( currententity->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) ) {
		qglMatrixMode( GL_PROJECTION );
		qglPopMatrix();
		qglMatrixMode( GL_MODELVIEW );
		qglCullFace( GL_FRONT );
	}

	if ( currententity->flags & RF_DEPTHHACK ) {
		qglDepthRange (gldepthmin, gldepthmax);
	}

	if ( gl_shadows->value && !(currententity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL)) &&
		( currentmodel->aliastype == ALIASTYPE_MD2 ) )
	{
		float an = -currententity->angles[1]/180*M_PI;
		
		if (!shadescale)
			shadescale = 1 / sqrt( 2 );

		shadevector[0] = cos(an) * shadescale;
		shadevector[1] = sin(an) * shadescale;
		shadevector[2] = shadescale;

		qglPushMatrix ();
// Vic
		qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);
		qglRotatef (e->angles[1], 0, 0, 1);
// Vic

		qglDisable (GL_TEXTURE_2D);
		GLSTATE_ENABLE_BLEND
		qglDepthMask (GL_FALSE);		// Vic: don't bother 
										// writing to Z-buffer

		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

		qglColor4f (0, 0, 0, 0.5);
		GL_DrawAliasShadow (paliashdr, currententity->frame);

		qglDepthMask (GL_TRUE);		// Vic: restore
									// writing to Z-buffer
		qglEnable (GL_TEXTURE_2D);

		GLSTATE_DISABLE_BLEND
		qglPopMatrix ();
	}

	qglColor4f (1,1,1,1);

	if ( 0 )
	{ 
		int i;

		if ( !( e->flags & RF_WEAPONMODEL ) )
		{
			qglDisable ( GL_CULL_FACE );
			qglPolygonMode ( GL_FRONT_AND_BACK, GL_LINE );
			qglDisable ( GL_TEXTURE_2D );
			qglBegin ( GL_TRIANGLE_STRIP );

			for (i = 0; i < 8; i++) {
				qglVertex3fv ( bbox[i] );
			}

			qglEnd ();
			qglEnable ( GL_TEXTURE_2D );
			qglPolygonMode ( GL_FRONT_AND_BACK, GL_FILL );
			qglEnable ( GL_CULL_FACE );
		}
	}
}

#define LEVEL_WIDTH(lvl) ((1 << (lvl+1)) + 1)

int r_maxmeshlevel = 4;

static int mesh_find_level(vec3_t *v)
{
    int level;
    vec3_t a, b, dist;

    /* Subdivide on the left until tolerance is reached */
    for (level=0; level < r_maxmeshlevel-1; level++)
    {
		/* Subdivide on the left */
		VectorAvg(v[0], v[1], a);
		VectorAvg(v[1], v[2], b);
		VectorAvg(a, b, v[2]);

		/* Find distance moved */
		VectorSubtract(v[2], v[1], dist);

		/* Check for tolerance */
		if (DotProduct(dist, dist) < r_subdivisions->value * r_subdivisions->value)
			break;

		/* Insert new middle vertex */
		VectorCopy(a, v[1]);
    }

    return level;
}

static void mesh_find_size(int *numcp, vec3_t *cp, int *size)
{
    int u, v, found, level;
    float *a, *b;
    vec3_t test[3];
    
    /* Find non-coincident pairs in u direction */
    found = 0;
    for (v=0; v < numcp[1]; v++)
    {
		for (u=0; u < numcp[0]-1; u += 2)
		{
			a = cp[v * numcp[0] + u];
			b = cp[v * numcp[0] + u + 2];
			if (!VectorCompare(a, b))
			{
				found = 1;
				break;
			}
		}
		if (found) break;
    }
    if (!found) Com_Error (ERR_DROP, "Bad mesh control points");

    /* Find subdivision level in u */
    VectorCopy(a, test[0]);
    VectorCopy((a+3), test[1]);
    VectorCopy(b, test[2]);
    level = mesh_find_level(test);
    size[0] = (LEVEL_WIDTH(level) - 1) * ((numcp[0]-1) / 2) + 1;
    
    /* Find non-coincident pairs in v direction */
    found = 0;
    for (u=0; u < numcp[0]; u++)
    {
		for (v=0; v < numcp[1]-1; v += 2)
		{
			a = cp[v * numcp[0] + u];
			b = cp[(v + 2) * numcp[0] + u];
			if (!VectorCompare(a,b))
			{
				found = 1;
				break;
			}
		}
		if (found) break;
    }
    if (!found) Com_Error (ERR_DROP, "Bad mesh control points");

    /* Find subdivision level in v */
    VectorCopy(a, test[0]);
    VectorCopy((a+numcp[0]*3), test[1]);
    VectorCopy(b, test[2]);
    level = mesh_find_level(test);
    size[1] = (LEVEL_WIDTH(level) - 1)* ((numcp[1]-1) / 2) + 1;    
}

static void mesh_fill_curve_3(int numcp, int size, int stride, vec3_t *p)
{
    int step, halfstep, i, mid;
    vec3_t a, b;

    step = (size-1) / (numcp-1);

    while (step > 0)
    {
		halfstep = step / 2;
		for (i=0; i < size-1; i += step*2)
		{
			mid = (i+step)*stride;
			VectorAvg(p[i*stride], p[mid], a);
			VectorAvg(p[mid], p[(i+step*2)*stride], b);
			VectorAvg(a, b, p[mid]);

			if (halfstep > 0)
			{
				VectorCopy(a, p[(i+halfstep)*stride]);
				VectorCopy(b, p[(i+3*halfstep)*stride]);
			}
		}
		
		step /= 2;
    }
}

static void mesh_fill_curve_2(int numcp, int size, int stride, vec2_t *p)
{
    int step, halfstep, i, mid;
    vec2_t a, b;

    step = (size-1) / (numcp-1);

    while (step > 0)
    {
		halfstep = step / 2;
		for (i=0; i < size-1; i += step*2)
		{
			mid = (i+step)*stride;
			Vector2Avg(p[i*stride], p[mid], a);
			Vector2Avg(p[mid], p[(i+step*2)*stride], b);
			Vector2Avg(a, b, p[mid]);

			if (halfstep > 0)
			{
				Vector2Copy(a, p[(i+halfstep)*stride]);
				Vector2Copy(b, p[(i+3*halfstep)*stride]);
			}
		}
		
		step /= 2;
    }
}

static void mesh_fill_curve_c(int numcp, int size, int stride, vec4_t *p)
{
    int step, halfstep, i, mid;
    vec4_t a, b;

    step = (size-1) / (numcp-1);

    while (step > 0)
    {
		halfstep = step / 2;
		for (i=0; i < size-1; i += step*2)
		{
			mid = (i+step)*stride;
			Vector4Avg(p[i*stride], p[mid], a);
			Vector4Avg(p[mid], p[(i+step*2)*stride], b);
			Vector4Avg(a, b, p[mid]);

			if (halfstep > 0)
			{
				Vector4Copy(a, p[(i+halfstep)*stride]);
				Vector4Copy(b, p[(i+3*halfstep)*stride]);
			}
		}
		
		step /= 2;
    }
}

static void mesh_fill_patch_3(int *numcp, int *size, vec3_t *p)
{
    int step, u, v;

    /* Fill in control points in v direction */
    step = (size[0]-1) / (numcp[0]-1);    
    for (u = 0; u < size[0]; u += step)
    {
		mesh_fill_curve_3(numcp[1], size[1], size[0], p + u);
    }

    /* Fill in the rest in the u direction */
    for (v = 0; v < size[1]; v++)
    {
		mesh_fill_curve_3(numcp[0], size[0], 1, p + v * size[0]);
    }
}

static void mesh_fill_patch_2(int *numcp, int *size, vec2_t *p)
{
    int step, u, v;

    /* Fill in control points in v direction */
    step = (size[0]-1) / (numcp[0]-1);    
    for (u = 0; u < size[0]; u += step)
    {
		mesh_fill_curve_2(numcp[1], size[1], size[0], p + u);
    }

    /* Fill in the rest in the u direction */
    for (v = 0; v < size[1]; v++)
    {
		mesh_fill_curve_2(numcp[0], size[0], 1, p + v * size[0]);
    }
}

static void mesh_fill_patch_c(int *numcp, int *size, vec4_t *p)
{
    int step, u, v;

    /* Fill in control points in v direction */
    step = (size[0]-1) / (numcp[0]-1);    
    for (u = 0; u < size[0]; u += step)
    {
		mesh_fill_curve_c(numcp[1], size[1], size[0], p + u);
    }

    /* Fill in the rest in the u direction */
    for (v = 0; v < size[1]; v++)
    {
		mesh_fill_curve_c(numcp[0], size[0], 1, p + v * size[0]);
    }
}

void GL_MeshCreate ( msurface_t *surf, int numverts, mvertex_t *verts, int patchWidth, int patchHeight )
{
    int step[2], size[2], mesh_cp[2], len, i, u, v, p;
    vec3_t points[MAX_VERTS], normals[MAX_VERTS];
	vec2_t lm_st[MAX_VERTS], tex_st[MAX_VERTS];
	vec4_t colour[MAX_VERTS];
    mvertex_t *vert;
	mesh_t *mesh = &surf->mesh;

	mesh->patchWidth = patchWidth;
	mesh->patchHeight = patchHeight;
	mesh_cp[0] = patchWidth;
	mesh_cp[1] = patchHeight;
	
    vert = verts;
    for ( i = 0; i < numverts; i++, vert++ )
		VectorCopy ( vert->position, points[i] );

// find the degree of subdivision in the u and v directions
    mesh_find_size ( mesh_cp, points, size );

// allocate space for mesh
    len = size[0] * size[1];
	mesh->numverts = len;
	mesh->firstvert = (mvertex_t *)Hunk_Alloc(len * sizeof(mvertex_t));

	mesh->lm_mins = (vec3_t *)Hunk_Alloc(len * sizeof(vec3_t));
	mesh->lm_maxs = (vec3_t *)Hunk_Alloc(len * sizeof(vec3_t));

// fill in sparse mesh control points
    step[0] = (size[0]-1) / (patchWidth-1);
    step[1] = (size[1]-1) / (patchHeight-1);

    vert = verts;
    for (v = 0; v < size[1]; v += step[1])
    {
		for (u = 0; u < size[0]; u += step[0])
		{
			p = v * size[0] + u;
			VectorCopy (vert->position, points[p]);
			VectorCopy (vert->normal, normals[p]);
			Vector4Copy (vert->colour, colour[p]);
			Vector2Copy (vert->tex_st, tex_st[p]);
			Vector2Copy (vert->lm_st, lm_st[p]);
			vert++;
		}
    }

// fill in each mesh
	mesh_fill_patch_3 (mesh_cp, size, points);
	mesh_fill_patch_3 (mesh_cp, size, normals);
	mesh_fill_patch_c (mesh_cp, size, colour);
	mesh_fill_patch_2 (mesh_cp, size, tex_st);
	mesh_fill_patch_2 (mesh_cp, size, lm_st);

    vert = mesh->firstvert;
	p = 0;

	ClearBounds ( surf->mins, surf->maxs );

    for (v = 0; v < size[1]; v++)
    {
		for (u = 0; u < size[0]; u++)
		{
			VectorCopy (points[p], vert->position);
			VectorCopy (normals[p], vert->normal);
			Vector4Copy (colour[p], vert->colour);
			Vector2Copy (tex_st[p], vert->tex_st);
			Vector2Copy (lm_st[p], vert->lm_st);

			// reletive position (for deformv "bulge")
			vert->patchRel = (float)v / (float)size[1];

			AddPointToBounds ( points[p], surf->mins, surf->maxs );

			MakeNormalVectors ( normals[p], mesh->lm_mins[p], mesh->lm_maxs[p] );

			vert++;
			p++;
		}
    }

// allocate and fill index table
    mesh->numindexes = (size[0]-1) * (size[1]-1) * 6;
    mesh->firstindex = (unsigned int *)Hunk_Alloc (mesh->numindexes * sizeof(unsigned int));

    i = 0;
    for (v = 0; v < size[1]-1; v++)
    {
		for (u = 0; u < size[0]-1; u++)
		{
			mesh->firstindex[i++] = v * size[0] + u;
			mesh->firstindex[i++] = (v+1) * size[0] + u;
			mesh->firstindex[i++] = v * size[0] + u + 1;
			mesh->firstindex[i++] = v * size[0] + u + 1;
			mesh->firstindex[i++] = (v+1) * size[0] + u;
			mesh->firstindex[i++] = (v+1) * size[0] + u + 1;
		}
    }
}
