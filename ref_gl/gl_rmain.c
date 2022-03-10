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
// r_main.c
#include "gl_local.h"

void R_Clear (void);

viddef_t	vid;

int GL_TEXTURE0, GL_TEXTURE1;

model_t		*r_worldmodel;

float		gldepthmin, gldepthmax;

glconfig_t gl_config;
glstate_t  gl_state;

image_t		*r_notexture;		// use for bad textures
image_t		*r_particletexture;	// little dot for particles
image_t		*r_whitetexture;
image_t		*r_dlighttexture;
image_t		*r_fogtexture;

entity_t	*currententity;
model_t		*currentmodel;

cplane_t	frustum[4];

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_alias_polys;

float		v_blend[4];			// final blending color

void GL_Strings_f( void );

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

mat4_t		r_world_matrix;
mat3x3_t	r_inverse_world_matrix;

//
// screen size info
//
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;
cvar_t	*r_fastsky;
cvar_t	*r_ignorehwgamma;
cvar_t	*r_overbrightbits;
cvar_t	*r_mapoverbrightbits;
cvar_t	*r_vertexlight;
cvar_t	*r_flares;
cvar_t	*r_flaresize;
cvar_t	*r_flarefade;
cvar_t	*r_dynamiclight;
cvar_t	*r_detailtextures;
cvar_t	*r_subdivisions;
cvar_t	*r_faceplanecull;

cvar_t	*gl_allow_software;

cvar_t	*gl_ext_swapinterval;
cvar_t	*gl_ext_multitexture;
cvar_t	*gl_ext_mtexcombine;
cvar_t	*gl_ext_compiled_vertex_array;

cvar_t	*gl_log;
cvar_t	*gl_bitdepth;
cvar_t	*gl_drawbuffer;
cvar_t  *gl_driver;
cvar_t	*gl_lightmap;
cvar_t	*gl_shadows;
cvar_t	*gl_mode;
cvar_t  *gl_monolightmap;
cvar_t	*gl_nobind;
cvar_t	*gl_round_down;
cvar_t	*gl_picmip;
cvar_t	*gl_skymip;
cvar_t	*gl_showtris;
cvar_t	*gl_ztrick;
cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*gl_cull;
cvar_t	*gl_polyblend;
cvar_t	*gl_playermip;
cvar_t	*gl_swapinterval;
cvar_t	*gl_texturemode;
cvar_t	*gl_texturealphamode;
cvar_t	*gl_texturesolidmode;
cvar_t	*gl_lockpvs;
cvar_t	*gl_stencilbuffer;		// Vic

cvar_t	*gl_screenshot_jpeg;			// Heffo - JPEG Screenshots
cvar_t	*gl_screenshot_jpeg_quality;	// Heffo - JPEG Screenshots

cvar_t	*gl_3dlabs_broken;

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	if (r_nocull->value)
		return false;

	for (i=0 ; i<4 ; i++)
		if ( BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	float alpha = 1.0F;
	vec3_t	point;
	dsprframe_t	*frame;
	float		*up, *right;
	dsprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (dsprite_t *)currentmodel->extradata;

	e->frame %= psprite->numframes;

	frame = &psprite->frames[e->frame];

	{	// normal sprite
		up = vup;
		right = vright;
	}

	if ( e->flags & RF_TRANSLUCENT )
		alpha = e->alpha;

	if ( alpha != 1.0F )
		GLSTATE_ENABLE_BLEND

	qglColor4f( 1, 1, 1, alpha );

    GL_Bind(currentmodel->skins[0][e->frame]->pass[0].anim_frames[0]->texnum);

	GL_TexEnv( GL_MODULATE );

	if ( alpha == 1.0 )
		GLSTATE_ENABLE_ALPHATEST

	qglBegin (GL_QUADS);

	qglTexCoord2f (0, 1);
	VectorMA (e->origin, -frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (0, 0);
	VectorMA (e->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 0);
	VectorMA (e->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 1);
	VectorMA (e->origin, -frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
	qglVertex3fv (point);
	
	qglEnd ();

	GLSTATE_DISABLE_ALPHATEST
	GL_TexEnv( GL_REPLACE );

	if ( alpha != 1.0F )
		GLSTATE_DISABLE_BLEND

	qglColor4f( 1, 1, 1, 1 );
}

//==================================================================================

static vec3_t nullmodel_vec[5];

void R_InitNullModel (void)
{
	int i;

	for (i = 0; i < 5; i++)
	{
		nullmodel_vec[i][0] = 16*cos(i*M_PI/2);
		nullmodel_vec[i][1] = 16*sin(i*M_PI/2);
		nullmodel_vec[i][2] = 0;
	}
}

/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel (void)
{
	int		i;

    qglPushMatrix ();

    qglTranslatef (currententity->origin[0], currententity->origin[1], currententity->origin[2]);

    qglRotatef (currententity->angles[1],  0, 0, 1);
    qglRotatef (-currententity->angles[0],  0, 1, 0);
    qglRotatef (-currententity->angles[2],  1, 0, 0);

	qglDisable (GL_TEXTURE_2D);

	qglColor3f ( 1, 1, 1 );

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (0, 0, -16);
	for (i=0 ; i<=4 ; i++)
		qglVertex3fv (nullmodel_vec[i]);
	qglEnd ();

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (0, 0, 16);
	for (i=4 ; i>=0 ; i--)
		qglVertex3fv (nullmodel_vec[i]);
	qglEnd ();

	qglPopMatrix ();
	qglEnable (GL_TEXTURE_2D);
}

/*
=============
R_SortEntitiesOnList
=============
*/
void R_SortEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->value)
		return;

	// draw non-transparent first
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if ( currententity->flags & RF_BEAM )
		{
			continue;
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				continue;
			}
			switch (currentmodel->type)
			{
			case mod_alias:
				if ( currentmodel->aliastype == ALIASTYPE_MD3 )
					R_AddMd3ToList (currententity);
				break;
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				break;
			default:
				Com_Error (ERR_DROP, "%s: bad modeltype", currentmodel->name);
				break;
			}
		}
	}
}

/*
=============
R_DrawTransEntities
=============
*/
void R_DrawTransEntities (void)
{
	// need to draw back to front
	// fixme: this isn't my favourite option
	int		i;
	float bestdist, dist;
	entity_t *bestent;
	vec3_t start, test;
	
	VectorCopy(r_newrefdef.vieworg, start);

transgetent:
	bestdist = 0;
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];
		if (!(currententity->flags & RF_TRANSLUCENT))
			continue;	// transparent
		if (currententity->transignore)
			continue;
		if (currententity->alpha == 1 || currententity->alpha == 0)
			continue;

		VectorCopy (currententity->origin, test);
		if (currententity->model && currententity->model->type == mod_brush)
		{
			test[0] += currententity->model->mins[0];
			test[1] += currententity->model->mins[1];
			test[2] += currententity->model->mins[2];
		}
		dist = (((test[0] - start[0]) * (test[0] - start[0])) +
			((test[1] - start[1]) * (test[1] - start[1])) +
			((test[2] - start[2]) * (test[2] - start[2])));

		if (dist > bestdist)
		{
			bestdist = dist;
			bestent = currententity;
		
		}
	}
	if (bestdist == 0)
		return;
	bestent->transignore = true;

	currententity = bestent;
	if ( currententity->flags & RF_BEAM )
	{
		R_DrawBeam( currententity );
	}
	else
	{
		currentmodel = currententity->model;
		
		if (!currentmodel)
		{
			R_DrawNullModel ();
		} else {
			switch (currentmodel->type)
			{
			case mod_alias:
				if ( currentmodel->aliastype == ALIASTYPE_MD2 )
					R_DrawAliasModel (currententity);
				break;
			case mod_brush:
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
			default:
				Com_Error (ERR_DROP, "%s: bad modeltype", currentmodel->name);
				break;
			}
		}
	}
		
	goto transgetent;
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->value)
		return;

	// draw non-transparent first
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];
		if (currententity->flags & RF_TRANSLUCENT)
			continue;	// solid

		if ( currententity->flags & RF_BEAM )
		{
			R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				R_DrawNullModel ();
				continue;
			}
			switch (currentmodel->type)
			{
			case mod_alias:
				if ( currentmodel->aliastype == ALIASTYPE_MD2 )
					R_DrawAliasModel (currententity);
				break;
			case mod_brush:
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
			default:
				Com_Error (ERR_DROP, "%s: bad modeltype", currentmodel->name);
				break;
			}
		}
	}

	// draw transparent entities
	// we could sort these if it ever becomes a problem...
	qglDepthMask ( GL_FALSE );		// no z writes
	R_DrawTransEntities ();
	qglDepthMask ( GL_TRUE );		// back to writing
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	const particle_t *p;
	int				i, maxp = MAX_PARTICLES / 4;
	vec3_t			r_pup, r_pright, corner;
	float			scale;
	float			v[3], pcolor[4];
	float			tc[2];

	if( !r_newrefdef.particles )
		return;

	VectorScale ( vup, 1.5f, r_pup );
	VectorScale ( vright, 1.5f, r_pright );

	GL_EnableMultitexture ( false );
	GL_TexEnv( GL_MODULATE );
    GL_Bind( r_particletexture->texnum );

	qglDepthMask( GL_FALSE );		// no z buffering

	GLSTATE_ENABLE_BLEND
	qglBlendFunc ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	for ( p = r_newrefdef.particles, i=0 ; i < r_newrefdef.num_particles ; i++, p++ )
	{
		// hack a scale up to keep particles from disapearing
		scale = ( p->origin[0] - r_origin[0] ) * vpn[0] + 
			( p->origin[1] - r_origin[1] ) * vpn[1] +
			( p->origin[2] - r_origin[2] ) * vpn[2];
		
		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004f;
		
		VectorCopy (d_8to24floattable[p->color], pcolor);
		pcolor[3] = p->alpha;

		corner[0] = p->origin[0] + (r_pup[0] + r_pright[0])*scale*(-0.5);
		corner[1] = p->origin[1] + (r_pup[1] + r_pright[1])*scale*(-0.5);
		corner[2] = p->origin[2] + (r_pup[2] + r_pright[2])*scale*(-0.5);
		
		R_PushElem (0);
		R_PushElem (1);
		R_PushElem (2);
		R_PushElem (0);
		R_PushElem (2);
		R_PushElem (3);
		
		VectorSet (v, 
			corner[0], 
			corner[1], 
			corner[2]);
		tc[0] = 1; tc[1] = 1;
		R_PushCoord (tc);
		R_PushColor (pcolor);
		R_PushVertex (v);
		
		VectorSet (v, 
			corner[0] + r_pup[0]*scale, 
			corner[1] + r_pup[1]*scale, 
			corner[2] + r_pup[2]*scale);
		tc[0] = 0; tc[1] = 1;
		R_PushCoord (tc);
		R_PushColor (pcolor);
		R_PushVertex (v);
		
		VectorSet (v, 
			corner[0] + (r_pup[0]+r_pright[0])*scale, 
			corner[1] + (r_pup[1]+r_pright[1])*scale,
			corner[2] + (r_pup[2]+r_pright[2])*scale);
		tc[0] = 0; tc[1] = 0;
		R_PushCoord (tc);
		R_PushColor (pcolor);
		R_PushVertex (v);
		
		VectorSet (v,
			corner[0] + r_pright[0]*scale,
			corner[1] + r_pright[1]*scale,
			corner[2] + r_pright[2]*scale);
		
		tc[0] = 1; tc[1] = 0;
		R_PushCoord (tc);
		R_PushColor (pcolor);
		R_PushVertex (v);

		// draw if there are already too many particles,
		// that might not fit into our array
		if ( (i%maxp) == 0 ) {
			R_LockArrays ();
			R_FlushArrays ();
			R_UnlockArrays ();
			R_ClearArrays ();
		}
	}
	
	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();

	GLSTATE_DISABLE_BLEND
	qglDepthMask( GL_TRUE );		// back to normal Z buffering
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!gl_polyblend->value)
		return;
	if (v_blend[3] < 0.01f)
		return;

	GLSTATE_ENABLE_BLEND
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, 1, 1, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	qglColor4fv (v_blend);

	qglBegin (GL_TRIANGLES);
	qglVertex2f (-5, -5);
	qglVertex2f (10, -5);
	qglVertex2f (-5, 10);
	qglEnd ();

	GLSTATE_DISABLE_BLEND
	qglEnable (GL_TEXTURE_2D);

	qglColor4f(1,1,1,1);
}

//=======================================================================

int SignbitsForPlane (cplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_newrefdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_newrefdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_newrefdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_newrefdef.fov_y / 2 ) );

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

//=======================================================================

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	int i;
	mleaf_t	*leaf;

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_newrefdef.vieworg, r_origin);

	AngleVectors (r_newrefdef.viewangles, vpn, vright, vup);

// current viewcluster
	if ( !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}

	for (i=0 ; i<4 ; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_polys = 0;
	c_alias_polys = 0;

	// clear out the portion of the screen that the NOWORLDMODEL defines
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
	{
		qglEnable( GL_SCISSOR_TEST );
		qglClearColor( 0.3, 0.3, 0.3, 1 );
		qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		qglClearColor( 1, 0, 0.5, 0.5 );
		qglDisable( GL_SCISSOR_TEST );
	}
}


void MYgluPerspective( GLdouble fovy, GLdouble aspect,
		     GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;
	
	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;
	
	xmin = ymin * aspect;
	xmax = ymax * aspect;
	
	xmin += -( 2 * gl_state.camera_separation ) / zNear;
	xmax += -( 2 * gl_state.camera_separation ) / zNear;
	
	qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	int		x, x2, y2, y, w, h;
	mat3x3_t temp;

	//
	// set up viewport
	//
	x = floor(r_newrefdef.x * vid.width / vid.width);
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	y = floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	w = x2 - x;
	h = y - y2;

	qglViewport (x, y2, w, h);

	//
	// set up projection matrix
	//
    screenaspect = (float)r_newrefdef.width/r_newrefdef.height;
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	MYgluPerspective (r_newrefdef.fov_y, screenaspect, 4, 128000);

	qglCullFace(GL_FRONT);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

    qglRotatef (-90,  1, 0, 0);	    // put Z going up
    qglRotatef (90,  0, 0, 1);	    // put Z going up
    qglRotatef (-r_newrefdef.viewangles[2], 1, 0, 0);
    qglRotatef (-r_newrefdef.viewangles[0], 0, 1, 0);
    qglRotatef (-r_newrefdef.viewangles[1], 0, 0, 1);
    qglTranslatef (-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1],  -r_newrefdef.vieworg[2]);

	qglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	Matrix4_Matrix3 ( r_world_matrix, temp );
	Matrix3_Transponse ( temp, r_inverse_world_matrix );

	GLSTATE_DISABLE_BLEND
	GLSTATE_DISABLE_ALPHATEST
	qglEnable (GL_DEPTH_TEST);
}

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if (gl_ztrick->value)
	{
		static int trickframe;

		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc (GL_LEQUAL);
	}

	qglDepthRange (gldepthmin, gldepthmax);

	if( gl_shadows->value && gl_state.stencil_enabled ) {
		qglClearStencil ( 1 );
		qglClear ( GL_STENCIL_BUFFER_BIT );
	}
}

void R_Flash( void )
{
	R_PolyBlend ();
}

/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
void R_RenderView (refdef_t *fd)
{
	if (r_norefresh->value)
		return;

	r_newrefdef = *fd;

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		Com_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish->value)
		qglFinish ();

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();

	R_SortEntitiesOnList ();

	R_DrawSortedPolys ();

	R_DrawEntitiesOnList ();

	R_DrawParticles ();

	R_Flash();

	if (r_speeds->value)
	{
		Com_Printf( "%4i wpoly %4i epoly\n",
			c_brush_polys, 
			c_alias_polys); 
	}
}


void R_SetGL2D (void)
{
	// set 2D virtual screen size
	qglViewport (0,0, vid.width, vid.height);
	qglMatrixMode (GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode (GL_MODELVIEW);
    qglLoadIdentity ();
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglColor4f (1, 1, 1, 1);
	gl_state.in2d = true;
}

static void GL_DrawColoredStereoLinePair( float r, float g, float b, float y )
{
	qglColor3f( r, g, b );
	qglVertex2f( 0, y );
	qglVertex2f( vid.width, y );
	qglColor3f( 0, 0, 0 );
	qglVertex2f( 0, y + 1 );
	qglVertex2f( vid.width, y + 1 );
}

static void GL_DrawStereoPattern( void )
{
	int i;

	if ( !( gl_config.renderer & GL_RENDERER_INTERGRAPH ) )
		return;

	if ( !gl_state.stereo_enabled )
		return;

	R_SetGL2D();

	qglDrawBuffer( GL_BACK_LEFT );

	for ( i = 0; i < 20; i++ )
	{
		qglBegin( GL_LINES );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 0 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 2 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 4 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 6 );
			GL_DrawColoredStereoLinePair( 0, 1, 0, 8 );
			GL_DrawColoredStereoLinePair( 1, 1, 0, 10);
			GL_DrawColoredStereoLinePair( 1, 1, 0, 12);
			GL_DrawColoredStereoLinePair( 0, 1, 0, 14);
		qglEnd();
		
		GLimp_EndFrame();
	}
}

/*
@@@@@@@@@@@@@@@@@@@@@
R_RenderFrame

@@@@@@@@@@@@@@@@@@@@@
*/
void R_RenderFrame (refdef_t *fd)
{
	gl_state.in2d = false;

	R_RenderView( fd );
	R_SetGL2D ();
}


void R_Register( void )
{
	Cvar_GetLatchedVars ();

	r_lefthand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_norefresh = Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = Cvar_Get ("r_drawworld", "1", 0);
	r_novis = Cvar_Get ("r_novis", "0", 0);
	r_nocull = Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = Cvar_Get ("r_speeds", "0", 0);

	r_fastsky = Cvar_Get ("r_fastsky", "0", CVAR_ARCHIVE);
	r_ignorehwgamma = Cvar_Get ("r_ignorehwgamma", "0", CVAR_ARCHIVE|CVAR_LATCH);
	r_overbrightbits = Cvar_Get ("r_overbrightbits", "1", CVAR_ARCHIVE|CVAR_LATCH);
	r_mapoverbrightbits = Cvar_Get ("r_mapoverbrightbits", "2", CVAR_ARCHIVE|CVAR_LATCH);
	r_vertexlight = Cvar_Get ("r_vertexlight", "0", CVAR_ARCHIVE|CVAR_LATCH);
	r_flares = Cvar_Get ("r_flares", "1", CVAR_ARCHIVE);
	r_flaresize = Cvar_Get ("r_flaresize", "40", CVAR_ARCHIVE);
	r_flarefade = Cvar_Get ("r_flarefade", "7", CVAR_ARCHIVE);
	r_dynamiclight = Cvar_Get ("r_dynamiclight", "1", CVAR_ARCHIVE);
	r_detailtextures = Cvar_Get ("r_detailtextures", "1", CVAR_ARCHIVE);
	r_subdivisions = Cvar_Get ("r_subdivisions", "4", CVAR_ARCHIVE);
	r_faceplanecull = Cvar_Get ("r_faceplanecull", "1", CVAR_ARCHIVE);

	gl_allow_software = Cvar_Get( "gl_allow_software", "0", 0 );

	gl_log = Cvar_Get( "gl_log", "0", 0 );
	gl_bitdepth = Cvar_Get( "gl_bitdepth", "0", 0 );
	gl_mode = Cvar_Get( "gl_mode", "3", CVAR_ARCHIVE|CVAR_LATCH );
	gl_lightmap = Cvar_Get ("gl_lightmap", "0", 0);
	gl_shadows = Cvar_Get ("gl_shadows", "0", CVAR_ARCHIVE );
	gl_nobind = Cvar_Get ("gl_nobind", "0", 0);
	gl_round_down = Cvar_Get ("gl_round_down", "1", 0);
	gl_picmip = Cvar_Get ("gl_picmip", "0", 0);
	gl_skymip = Cvar_Get ("gl_skymip", "0", 0);
	gl_showtris = Cvar_Get ("gl_showtris", "0", 0);
	gl_ztrick = Cvar_Get ("gl_ztrick", "0", 0);
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = Cvar_Get ("gl_clear", "0", 0);
	gl_cull = Cvar_Get ("gl_cull", "1", 0);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", 0);
	gl_playermip = Cvar_Get ("gl_playermip", "0", 0);
	gl_monolightmap = Cvar_Get( "gl_monolightmap", "0", 0 );
	gl_driver = Cvar_Get( "gl_driver", "opengl32", CVAR_ARCHIVE|CVAR_LATCH );
	gl_texturemode = Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	gl_texturealphamode = Cvar_Get( "gl_texturealphamode", "default", CVAR_ARCHIVE );
	gl_texturesolidmode = Cvar_Get( "gl_texturesolidmode", "default", CVAR_ARCHIVE );
	gl_lockpvs = Cvar_Get( "gl_lockpvs", "0", 0 );

	gl_screenshot_jpeg = Cvar_Get( "gl_screenshot_jpeg", "1", CVAR_ARCHIVE );					// Heffo - JPEG Screenshots
	gl_screenshot_jpeg_quality = Cvar_Get( "gl_screenshot_jpeg_quality", "85", CVAR_ARCHIVE );	// Heffo - JPEG Screenshots

	gl_ext_swapinterval = Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE|CVAR_LATCH );
	gl_ext_multitexture = Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE|CVAR_LATCH );
	gl_ext_mtexcombine = Cvar_Get( "gl_ext_mtexcombine", "1", CVAR_ARCHIVE|CVAR_LATCH );
	gl_ext_compiled_vertex_array = Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE|CVAR_LATCH );

	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );
	gl_swapinterval = Cvar_Get( "gl_swapinterval", "0", CVAR_ARCHIVE );

	gl_3dlabs_broken = Cvar_Get( "gl_3dlabs_broken", "1", CVAR_ARCHIVE );

	// Vic
	gl_stencilbuffer = Cvar_Get( "gl_stencilbuffer", "0", CVAR_LATCH );

	vid_fullscreen = Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );

	Cmd_AddCommand( "imagelist", GL_ImageList_f );
	Cmd_AddCommand( "screenshot", GL_ScreenShot_f );
	Cmd_AddCommand( "modellist", Mod_Modellist_f );
	Cmd_AddCommand( "gl_strings", GL_Strings_f );
}

/*
==================
R_SetMode
==================
*/
qboolean R_SetMode (void)
{
	rserr_t err;
	qboolean fullscreen;

	if ( vid_fullscreen->modified && !gl_config.allow_cds )
	{
		Com_Printf( "R_SetMode() - CDS not allowed with this driver\n" );
		Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->value );
		vid_fullscreen->modified = false;
	}

	fullscreen = vid_fullscreen->value;

	vid_fullscreen->modified = false;
	gl_mode->modified = false;

	if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->value, fullscreen ) ) == rserr_ok )
	{
		gl_state.prev_mode = gl_mode->value;
	}
	else
	{
		if ( err == rserr_invalid_fullscreen )
		{
			Cvar_SetValue( "vid_fullscreen", 0);
			vid_fullscreen->modified = false;
			Com_Printf( "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n" );
			if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->value, false ) ) == rserr_ok )
				return true;
		}
		else if ( err == rserr_invalid_mode )
		{
			Cvar_SetValue( "gl_mode", gl_state.prev_mode );
			gl_mode->modified = false;
			Com_Printf( "ref_gl::R_SetMode() - invalid mode\n" );
		}

		// try setting it back to something safe
		if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_state.prev_mode, false ) ) != rserr_ok )
		{
			Com_Printf( "ref_gl::R_SetMode() - could not revert to safe mode\n" );
			return false;
		}
	}
	return true;
}

/*
===============
R_Init
===============
*/
qboolean R_Init( void *hinstance, void *hWnd )
{	
	char renderer_buffer[1000];
	char vendor_buffer[1000];
	int		err;

	Com_Printf( "ref_gl version: "REF_VERSION"\n");

	Draw_GetPalette ();

	R_Register();

	// initialize our QGL dynamic bindings
	if ( !QGL_Init( gl_driver->string ) )
	{
		QGL_Shutdown();
        Com_Printf( "ref_gl::R_Init() - could not load \"%s\"\n", gl_driver->string );
		return -1;
	}

	// initialize OS-specific parts of OpenGL
	if ( !GLimp_Init( hinstance, hWnd ) )
	{
		QGL_Shutdown();
		return -1;
	}

	// set our "safe" modes
	gl_state.prev_mode = 3;

	// create the window and set up the context
	if ( !R_SetMode () )
	{
		QGL_Shutdown();
        Com_Printf( "ref_gl::R_Init() - could not R_SetMode()\n" );
		return -1;
	}

	/*
	** get our various GL strings
	*/
	gl_config.vendor_string = qglGetString (GL_VENDOR);
	Com_Printf( "GL_VENDOR: %s\n", gl_config.vendor_string );
	gl_config.renderer_string = qglGetString (GL_RENDERER);
	Com_Printf( "GL_RENDERER: %s\n", gl_config.renderer_string );
	gl_config.version_string = qglGetString (GL_VERSION);
	Com_Printf( "GL_VERSION: %s\n", gl_config.version_string );
	gl_config.extensions_string = qglGetString (GL_EXTENSIONS);
	Com_Printf( "GL_EXTENSIONS: %s\n", gl_config.extensions_string );

	strcpy( renderer_buffer, gl_config.renderer_string );
	strlwr( renderer_buffer );

	strcpy( vendor_buffer, gl_config.vendor_string );
	strlwr( vendor_buffer );

	if ( strstr( renderer_buffer, "voodoo" ) )
	{
		if ( !strstr( renderer_buffer, "rush" ) )
			gl_config.renderer = GL_RENDERER_VOODOO;
		else
			gl_config.renderer = GL_RENDERER_VOODOO_RUSH;
	}
	else if ( strstr( vendor_buffer, "sgi" ) )
		gl_config.renderer = GL_RENDERER_SGI;
	else if ( strstr( renderer_buffer, "permedia" ) )
		gl_config.renderer = GL_RENDERER_PERMEDIA2;
	else if ( strstr( renderer_buffer, "glint" ) )
		gl_config.renderer = GL_RENDERER_GLINT_MX;
	else if ( strstr( renderer_buffer, "glzicd" ) )
		gl_config.renderer = GL_RENDERER_REALIZM;
	else if ( strstr( renderer_buffer, "gdi" ) )
		gl_config.renderer = GL_RENDERER_MCD;
	else if ( strstr( renderer_buffer, "pcx2" ) )
		gl_config.renderer = GL_RENDERER_PCX2;
	else if ( strstr( renderer_buffer, "verite" ) )
		gl_config.renderer = GL_RENDERER_RENDITION;
//	else if ( strstr( renderer, "rage pro" ) || strstr( renderer, "Rage Pro" ) )
//		gl_config.renderer = GL_RENDERER_RAGEPRO;
	else
		gl_config.renderer = GL_RENDERER_OTHER;

	if ( toupper( gl_monolightmap->string[1] ) != 'F' )
	{
		if ( gl_config.renderer == GL_RENDERER_PERMEDIA2 )
		{
			Cvar_Set( "gl_monolightmap", "A" );
			Com_Printf( "...using gl_monolightmap 'a'\n" );
		}
		else if ( gl_config.renderer & GL_RENDERER_POWERVR ) 
		{
			Cvar_Set( "gl_monolightmap", "0" );
		}
		else
		{
			Cvar_Set( "gl_monolightmap", "0" );
		}
	}

	// power vr can't have anything stay in the framebuffer, so
	// the screen needs to redraw the tiled background every frame
	if ( gl_config.renderer & GL_RENDERER_POWERVR ) 
	{
		Cvar_Set( "scr_drawall", "1" );
	}
	else
	{
		Cvar_Set( "scr_drawall", "0" );
	}

#ifdef __linux__
	Cvar_SetValue( "gl_finish", 1 );
#endif

	// MCD has buffering issues
	if ( gl_config.renderer == GL_RENDERER_MCD )
	{
		Cvar_SetValue( "gl_finish", 1 );
	}

	if ( gl_config.renderer & GL_RENDERER_3DLABS )
	{
		if ( gl_3dlabs_broken->value )
			gl_config.allow_cds = false;
		else
			gl_config.allow_cds = true;
	}
	else
	{
		gl_config.allow_cds = true;
	}

	if ( gl_config.allow_cds )
		Com_Printf( "...allowing CDS\n" );
	else
		Com_Printf( "...disabling CDS\n" );

	/*
	** grab extensions
	*/
	if ( strstr( gl_config.extensions_string, "GL_EXT_compiled_vertex_array" ) || 
		 strstr( gl_config.extensions_string, "GL_SGI_compiled_vertex_array" ) )
	{
		Com_Printf( "...enabling GL_EXT_compiled_vertex_array\n" );
		qglLockArraysEXT = ( void * ) qwglGetProcAddress( "glLockArraysEXT" );
		qglUnlockArraysEXT = ( void * ) qwglGetProcAddress( "glUnlockArraysEXT" );
	}
	else
	{
		Com_Printf( "...GL_EXT_compiled_vertex_array not found\n" );
	}

	gl_config.env_add = false;

	if ( strstr( gl_config.extensions_string, "GL_ARB_texture_env_add" ) )
	{
		Com_Printf( "...enabling GL_ARB_texture_env_add\n" );
		gl_config.env_add = true;
	}
	else
	{
		Com_Printf( "...GL_ARB_texture_env_add not found\n" );
	}

#ifdef _WIN32
	if ( strstr( gl_config.extensions_string, "WGL_EXT_swap_control" ) )
	{
		qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qwglGetProcAddress( "wglSwapIntervalEXT" );
		Com_Printf( "...enabling WGL_EXT_swap_control\n" );
	}
	else
	{
		Com_Printf( "...WGL_EXT_swap_control not found\n" );
	}
#endif

	if ( strstr( gl_config.extensions_string, "GL_ARB_multitexture" ) )
	{
		if ( gl_ext_multitexture->value )
		{
			Com_Printf( "...using GL_ARB_multitexture\n" );
			qglMTexCoord2fSGIS = ( void * ) qwglGetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ( void * ) qwglGetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( void * ) qwglGetProcAddress( "glClientActiveTextureARB" );
			GL_TEXTURE0 = GL_TEXTURE0_ARB;
			GL_TEXTURE1 = GL_TEXTURE1_ARB;
		}
		else
		{
			Com_Printf( "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		Com_Printf( "...GL_ARB_multitexture not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_SGIS_multitexture" ) )
	{
		if ( qglActiveTextureARB )
		{
			Com_Printf( "...GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n" );
		}
		else if ( gl_ext_multitexture->value )
		{
			Com_Printf( "...using GL_SGIS_multitexture\n" );
			qglMTexCoord2fSGIS = ( void * ) qwglGetProcAddress( "glMTexCoord2fSGIS" );
			qglSelectTextureSGIS = ( void * ) qwglGetProcAddress( "glSelectTextureSGIS" );
			GL_TEXTURE0 = GL_TEXTURE0_SGIS;
			GL_TEXTURE1 = GL_TEXTURE1_SGIS;
		}
		else
		{
			Com_Printf( "...ignoring GL_SGIS_multitexture\n" );
		}
	}
	else
	{
		Com_Printf( "...GL_SGIS_multitexture not found\n" );
	}

	if( strstr( gl_config.extensions_string, "WGL_3DFX_gamma_control" )) {
		if( !r_ignorehwgamma->value ) {
			qwglGetDeviceGammaRamp3DFX	= ( BOOL (WINAPI *)(HDC, WORD *)) qwglGetProcAddress( "wglGetDeviceGammaRamp3DFX" );
			qwglSetDeviceGammaRamp3DFX	= ( BOOL (WINAPI *)(HDC, WORD *)) qwglGetProcAddress( "wglSetDeviceGammaRamp3DFX" );
			Com_Printf( "...using WGL_3DFX_gamma_control\n" );
		} else {
			Com_Printf( "...ignoring WGL_3DFX_gamma_control\n" );
		}
	} else {
		Com_Printf( "...WGL_3DFX_gamma_control not found\n" );
	}

	gl_config.mtexcombine = false;

	if ( strstr( gl_config.extensions_string, "GL_ARB_texture_env_combine" ) )
	{
		if ( gl_ext_mtexcombine->value && qglMTexCoord2fSGIS )
		{
			Com_Printf( "...using GL_ARB_texture_env_combine\n" );
			gl_config.mtexcombine = true;
		}
		else
		{
			Com_Printf( "...ignoring GL_ARB_texture_env_combine\n" );
		}
	}
	else
	{
		Com_Printf( "...GL_ARB_texture_env_combine not found\n" );
	}

	if ( !gl_config.mtexcombine )
	{
		if ( strstr( gl_config.extensions_string, "GL_EXT_texture_env_combine" ) )
		{
			if ( gl_ext_mtexcombine->value && qglMTexCoord2fSGIS )
			{
				Com_Printf( "...using GL_EXT_texture_env_combine\n" );
				gl_config.mtexcombine = true;
			}
			else
			{
				Com_Printf( "...ignoring GL_EXT_texture_env_combine\n" );
			}
		}
		else
		{
			Com_Printf( "...GL_EXT_texture_env_combine not found\n" );
		}
	}

	gl_config.nvtexcombine4 = false;

	if ( gl_config.mtexcombine )
	{
		if ( strstr( gl_config.extensions_string, "NV_texture_env_combine4" ) )
		{
			if ( gl_ext_mtexcombine->value )
			{
				Com_Printf( "...using NV_texture_env_combine4\n" );
				gl_config.nvtexcombine4 = true;
			}
			else
			{
				Com_Printf( "...ignoring NV_texture_env_combine4\n" );
			}
		}
		else
		{
			Com_Printf( "...NV_texture_env_combine4 not found\n" );
		}
	}

	GL_SetDefaultState();

	/*
	** draw our stereo patterns
	*/
#if 0 // commented out until H3D pays us the money they owe us
	GL_DrawStereoPattern();
#endif

	GL_InitImages ();
	Mod_Init ();
	R_InitParticleTexture ();
	R_InitBubble ();
	R_InitMd3 ();

	Shader_Init ();

	Draw_InitLocal ();

	R_InitArrays ();
	R_InitNullModel ();

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		Com_Printf( "glGetError() = 0x%x\n", err);

	return false;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown (void)
{	
	Cmd_RemoveCommand ("modellist");
	Cmd_RemoveCommand ("screenshot");
	Cmd_RemoveCommand ("imagelist");
	Cmd_RemoveCommand ("gl_strings");

	Mod_FreeAll ();

	Shader_Shutdown ();

	GL_ShutdownImages ();

	/*
	** shut down OS specific OpenGL stuff like contexts, etc.
	*/
	GLimp_Shutdown();

	/*
	** shutdown our QGL subsystem
	*/
	QGL_Shutdown();
}



/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginFrame
@@@@@@@@@@@@@@@@@@@@@
*/
void UpdateGammaRamp (void);

void R_BeginFrame( float camera_separation )
{
	gl_state.camera_separation = camera_separation;

	if ( gl_log->modified )
	{
		GLimp_EnableLogging( gl_log->value );
		gl_log->modified = false;
	}

	if ( gl_log->value )
	{
		GLimp_LogNewFrame();
	}

	/*
	** update 3Dfx gamma -- it is expected that a user will do a vid_restart
	** after tweaking this value
	*/
	if ( vid_gamma->modified )
	{
		vid_gamma->modified = false;
		UpdateGammaRamp ();
	}

	GLimp_BeginFrame( camera_separation );

	/*
	** go into 2D mode
	*/
	R_SetGL2D ();

	/*
	** draw buffer stuff
	*/
	if ( gl_drawbuffer->modified )
	{
		gl_drawbuffer->modified = false;

		if ( gl_state.camera_separation == 0 || !gl_state.stereo_enabled )
		{
			if ( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 )
				qglDrawBuffer( GL_FRONT );
			else
				qglDrawBuffer( GL_BACK );
		}
	}

	/*
	** texturemode stuff
	*/
	if ( gl_texturemode->modified )
	{
		GL_TextureMode( gl_texturemode->string );
		gl_texturemode->modified = false;
	}

	if ( gl_texturealphamode->modified )
	{
		GL_TextureAlphaMode( gl_texturealphamode->string );
		gl_texturealphamode->modified = false;
	}

	if ( gl_texturesolidmode->modified )
	{
		GL_TextureSolidMode( gl_texturesolidmode->string );
		gl_texturesolidmode->modified = false;
	}

	/*
	** swapinterval stuff
	*/
	GL_UpdateSwapInterval();

	//
	// clear screen if desired
	//
	R_Clear ();
}

/*
=============
R_SetPalette
=============
*/
unsigned r_rawpalette[256];

void R_SetPalette ( const unsigned char *palette)
{
	int		i;

	byte *rp = ( byte * ) r_rawpalette;

	if ( palette )
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = palette[i*3+0];
			rp[i*4+1] = palette[i*3+1];
			rp[i*4+2] = palette[i*3+2];
			rp[i*4+3] = 0xff;
		}
	}
	else
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = d_8to24table[i] & 0xff;
			rp[i*4+1] = ( d_8to24table[i] >> 8 ) & 0xff;
			rp[i*4+2] = ( d_8to24table[i] >> 16 ) & 0xff;
			rp[i*4+3] = 0xff;
		}
	}

	qglClearColor (0,0,0,0);
	qglClear (GL_COLOR_BUFFER_BIT);
	qglClearColor (1,0, 0.5 , 0.5);
}

/*
** R_DrawBeam
Vic: FIXME: should beams be affected by the fog?
*/
void R_DrawBeam( entity_t *e )
{
#define NUM_BEAM_SEGS 6

	int	i;
	float r, g, b;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, e->frame / 2, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	qglDisable( GL_TEXTURE_2D );
	GLSTATE_ENABLE_BLEND
	qglDepthMask( GL_FALSE );

	r = ( d_8to24table[e->skinnum & 0xFF] ) & 0xFF;
	g = ( d_8to24table[e->skinnum & 0xFF] >> 8 ) & 0xFF;
	b = ( d_8to24table[e->skinnum & 0xFF] >> 16 ) & 0xFF;

	r *= 1/255.0F;
	g *= 1/255.0F;
	b *= 1/255.0F;

	qglColor4f( r, g, b, e->alpha );

	qglBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		qglVertex3fv( start_points[i] );
		qglVertex3fv( end_points[i] );
		qglVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] );
		qglVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] );
	}
	qglEnd();
	
	qglEnable( GL_TEXTURE_2D );
	GLSTATE_DISABLE_BLEND
	qglDepthMask( GL_TRUE );
}
