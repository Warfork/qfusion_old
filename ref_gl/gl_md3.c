/*
Copyright (C) 2001-2002 Victor "Vic" Luchits

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
// gl_md3.c: Quake III Arena .md3 model format support by Vic

#include "gl_local.h"

static vec3_t	md3_mins, md3_maxs;
meshlistmember_t *R_AddMeshToList ( mesh_t *mesh, mfog_t *fog );

static	mvertex_t md3_verts[MAX_VERTS];
static	int		  md3_indexes[MAX_VERTS*3];
static  mesh_t	  md3_mesh;

/*
==============================================================================

MD3 MODELS

==============================================================================
*/

/*
=================
R_InitMd3
=================
*/
void R_InitMd3 (void)
{
	int i;

	md3_mesh.firstindex = md3_indexes;
	md3_mesh.firstvert = md3_verts;
	md3_mesh.lightmaptexturenum = -1;

	for ( i = 0; i < MAX_VERTS; i++ ) {
		md3_verts[i].colour[0] = 1;
		md3_verts[i].colour[1] = 1;
		md3_verts[i].colour[2] = 1;
		md3_verts[i].colour[3] = 1;
	}
}

/*
=================
Mod_LoadMd3Model
=================
*/
void Mod_LoadMd3Model ( model_t *mod, void *buffer )
{
	int					version, i, j, l;
	md3header_t			*pheader, *pinmodel;
	md3frame_t			*pinframe, *poutframe;
	md3tag_t			*pintag, *pouttag;
	md3mesh_file_t		*pinmeshheader, *poutmeshheader;
	md3elem_t			*pinelems, *poutelems;
	md3coord_t			*pincoords, *poutcoords;
	md3vertex_t			*pinverts, *poutverts;
	md3mesh_skin_t		*poutskin, *pinskin;
	byte				*pinmeshdata, *poutmeshdata;

	pinmodel = ( md3header_t * )buffer;
	version = LittleLong( pinmodel->version );

	if ( version != MD3_ALIAS_VERSION ) {
		Com_Error ( ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, version, MD3_ALIAS_VERSION );
	}

	pheader = ( md3header_t * )Hunk_Alloc( LittleLong( pinmodel->ofs_end ) );

	pheader->version = version;
	pheader->num_frames = LittleLong( pinmodel->num_frames );
	pheader->num_tags = LittleLong( pinmodel->num_tags );
	pheader->num_meshes = LittleLong( pinmodel->num_meshes );
	pheader->num_skins = LittleLong( pinmodel->num_skins );

	pheader->ofs_frames = LittleLong( pinmodel->ofs_frames );
	pheader->ofs_tags = LittleLong( pinmodel->ofs_tags );
	pheader->ofs_meshes = LittleLong( pinmodel->ofs_meshes );
	pheader->ofs_end = LittleLong( pinmodel->ofs_end );

	memcpy ( pheader->filename, pinmodel->filename, sizeof( pheader->filename ) );

	if (pheader->num_frames <= 0) {
		Com_Error ( ERR_DROP, "model %s has no frames", mod->name );
	}

	if (pheader->num_meshes <= 0) {
		Com_Error ( ERR_DROP, "model %s has no meshes", mod->name );
	}

//
// load the frames
//
	pinframe = ( md3frame_t * )( ( byte * )pinmodel + pheader->ofs_frames );
	poutframe = ( md3frame_t * )( ( byte * )pheader + pheader->ofs_frames );

	for ( i = 0; i < pheader->num_frames; i++, pinframe++, poutframe++ ) {
		for ( j = 0; j < 3; j++ ) {
			poutframe->mins[j] = LittleFloat ( pinframe->mins[j] );
			poutframe->maxs[j] = LittleFloat ( pinframe->maxs[j] );
			poutframe->translate[j] = LittleFloat ( pinframe->translate[j] );
		}

		poutframe->radius = LittleFloat ( pinframe->radius );
		memcpy ( poutframe->creator, pinframe->creator, sizeof( poutframe->creator ) );
	}

//
// load the tags
//
	pintag = ( md3tag_t * )( ( byte * )pinmodel + pheader->ofs_tags );
	pouttag = ( md3tag_t * )( ( byte * )pheader + pheader->ofs_tags );

	for ( i = 0; i < pheader->num_frames; i++ ) {
		for ( l = 0; l < pheader->num_tags; l++, pintag++, pouttag++ ) {
			for ( j = 0; j < 3; j++ ) {
				pouttag->orient.origin[j] = LittleFloat ( pintag->orient.origin[j] );
				pouttag->orient.axis[0][j] = LittleFloat ( pintag->orient.axis[0][j] );
				pouttag->orient.axis[1][j] = LittleFloat ( pintag->orient.axis[1][j] );
				pouttag->orient.axis[2][j] = LittleFloat ( pintag->orient.axis[2][j] );
			}

			memcpy ( pouttag->name, pintag->name, sizeof( pouttag->name ) );
		}
	}

//
// load the meshes
//
	pinmeshdata = ( byte * )pinmodel + pheader->ofs_meshes;
	poutmeshdata = ( byte * )pheader + pheader->ofs_meshes;

	for ( i = 0; i < pheader->num_meshes; i++ ) {
		pinmeshheader = ( md3mesh_file_t * )( pinmeshdata );
		poutmeshheader = ( md3mesh_file_t * )( poutmeshdata );

		poutmeshheader->id = LittleLong ( pinmeshheader->id );
		poutmeshheader->flags = LittleLong ( pinmeshheader->flags );

		poutmeshheader->num_frames = LittleLong ( pinmeshheader->num_frames );
		poutmeshheader->num_skins = LittleLong ( pinmeshheader->num_skins );
		poutmeshheader->num_verts = LittleLong ( pinmeshheader->num_verts );
		poutmeshheader->num_tris  = LittleLong ( pinmeshheader->num_tris );

		poutmeshheader->ofs_skins = LittleLong ( pinmeshheader->ofs_skins );
		poutmeshheader->ofs_tcs = LittleLong ( pinmeshheader->ofs_tcs );
		poutmeshheader->ofs_verts = LittleLong ( pinmeshheader->ofs_verts );
		poutmeshheader->ofs_elems = LittleLong ( pinmeshheader->ofs_elems );

		poutmeshheader->meshsize = LittleLong ( pinmeshheader->meshsize );

		memcpy ( poutmeshheader->name, pinmeshheader->name, sizeof( poutmeshheader->name ) );

		if ( poutmeshheader->id != MD3_ID_HEADER ) {
			Com_Error ( ERR_DROP, "%s has wrong mesh version number (%i should be %i)",
					 mod->name, version, MD3_ID_HEADER );
		}

		if ( poutmeshheader->num_skins <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no skins", i, mod->name );
		}

		if ( poutmeshheader->num_frames <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no frames", i, mod->name );
		}

		if ( poutmeshheader->num_tris <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no elements", i, mod->name );
		}

		if ( poutmeshheader->num_verts <= 0 ) {
			Com_Error ( ERR_DROP, "mesh %i in model %s has no vertices", i, mod->name );
		}

	//
	// load the skins
	//
		pinskin = ( md3mesh_skin_t * )( pinmeshdata + poutmeshheader->ofs_skins );
		poutskin = ( md3mesh_skin_t * )( poutmeshdata + poutmeshheader->ofs_skins );

		for ( j = 0, l = 0; j < poutmeshheader->num_skins; j++, pinskin++, poutskin++ ) {
			COM_StripExtension ( pinskin->name, poutskin->name );
			mod->skins[i][l++] = R_RegisterShaderMD3 ( poutskin->name );
		}

	//
	// load the elements
	//
		pinelems = ( md3elem_t * )( pinmeshdata + poutmeshheader->ofs_elems );
		poutelems = ( md3elem_t * )( poutmeshdata + poutmeshheader->ofs_elems );

		for ( j = 0; j < poutmeshheader->num_tris; j++, pinelems++, poutelems++ ) {
			poutelems->index[0] = LittleLong ( pinelems->index[0] );
			poutelems->index[1] = LittleLong ( pinelems->index[1] );
			poutelems->index[2] = LittleLong ( pinelems->index[2] );
		}

	//
	// load the texture coordinates
	//
		pincoords = ( md3coord_t * )( pinmeshdata + poutmeshheader->ofs_tcs );
		poutcoords = ( md3coord_t * )( poutmeshdata + poutmeshheader->ofs_tcs );

		for ( j = 0; j < poutmeshheader->num_verts; j++, pincoords++, poutcoords++ ) {
			poutcoords->tc[0] = LittleFloat ( pincoords->tc[0] );
			poutcoords->tc[1] = LittleFloat ( pincoords->tc[1] );
		}

	//
	// load the vertices and normals
	//
		pinverts = ( md3vertex_t * )( pinmeshdata + poutmeshheader->ofs_verts ); 
		poutverts = ( md3vertex_t * )( poutmeshdata + poutmeshheader->ofs_verts );

		for ( l = 0; l < poutmeshheader->num_frames; l++ ) {
			for ( j = 0; j < poutmeshheader->num_verts; j++, pinverts++, poutverts++ ) {
				poutverts->point[0] = LittleShort ( pinverts->point[0] );
				poutverts->point[1] = LittleShort ( pinverts->point[1] );
				poutverts->point[2] = LittleShort ( pinverts->point[2] );
				poutverts->norm[0] = pinverts->norm[0];
				poutverts->norm[1] = pinverts->norm[1];
			}
		}

		pinmeshdata += poutmeshheader->meshsize;
		poutmeshdata += poutmeshheader->meshsize;
	}

	mod->numframes = pheader->num_frames;
	mod->type = mod_alias;
	mod->aliastype = ALIASTYPE_MD3;
}

void R_RegisterMd3 ( model_t *mod )
{
	int i, j, l;
	md3header_t *pheader;
	md3mesh_file_t *pmeshheader;
	md3mesh_skin_t *pskin;
	byte *meshdata;
	
	pheader = ( md3header_t * )mod->extradata;
	meshdata = ( byte * )pheader + pheader->ofs_meshes;

	for ( i = 0; i < pheader->num_meshes; i++ ) {
		pmeshheader = ( md3mesh_file_t * )( meshdata );
		pskin = ( md3mesh_skin_t * )( meshdata + pmeshheader->ofs_skins );

		for ( j = 0, l = 0; j < pmeshheader->num_skins; j++, l++, pskin++ ) {
			mod->skins[i][l]->registration_sequence = registration_sequence;
		}

		meshdata += pmeshheader->meshsize;
	}

	mod->numframes = pheader->num_frames;
}

/*
** R_Md3BBox
*/
void R_Md3BBox ( int frame, int oldframe, entity_t *e )
{
	int			i;
	md3frame_t	*pframe, *poldframe;
	vec3_t		mins, maxs;
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	md3header_t	*md3header;

	md3header = ( md3header_t * )currentmodel->extradata;

	pframe = ( md3frame_t * )( ( byte * )md3header + md3header->ofs_frames +
			frame * sizeof( md3frame_t ) );
	poldframe = ( md3frame_t * )( ( byte * )md3header + md3header->ofs_frames +
			oldframe * sizeof( md3frame_t ) );

	/*
	** compute axially aligned mins and maxs
	*/
	if ( pframe == poldframe )
	{
		for ( i = 0; i < 3; i++ )
		{
			mins[i] = pframe->translate[i] - pframe->radius;
			maxs[i] = pframe->translate[i] + pframe->radius;
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
		{
			thismins[i] = pframe->translate[i] - pframe->radius;
			thismaxs[i] = pframe->translate[i] + pframe->radius;

			oldmins[i]  = poldframe->translate[i] - poldframe->radius;
			oldmaxs[i]  = poldframe->translate[i] + poldframe->radius;

			if ( thismins[i] < oldmins[i] )
				mins[i] = thismins[i];
			else
				mins[i] = oldmins[i];

			if ( thismaxs[i] > oldmaxs[i] )
				maxs[i] = thismaxs[i];
			else
				maxs[i] = oldmaxs[i];
		}
	}

	if ( e->scale != 1.0f ) {
		VectorScale ( mins, e->scale, mins );
		VectorScale ( maxs, e->scale, maxs );
	}

	VectorAdd ( e->origin, mins, md3_mins );
	VectorAdd ( e->origin, maxs, md3_maxs );
}

/*
** R_CullAliasModel
*/
qboolean R_CullMd3Model( vec3_t bbox[8], entity_t *e )
{
	int			i;
	vec3_t		mins, maxs;
	md3header_t	*md3header;

	md3header = ( md3header_t * )currentmodel->extradata;

	if ( ( e->frame >= md3header->num_frames ) || ( e->frame < 0 ) )
	{
		Com_Printf ("R_CullMd3Model %s: no such frame %d\n", 
			currentmodel->name, e->frame);
		e->frame = 0;
	}
	if ( ( e->oldframe >= md3header->num_frames ) || ( e->oldframe < 0 ) )
	{
		Com_Printf ("R_CullMd3Model %s: no such oldframe %d\n", 
			currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}


	R_Md3BBox ( e->frame, e->oldframe, e );

	VectorSubtract ( md3_mins, e->origin, mins );
	VectorSubtract ( md3_maxs, e->origin, maxs );

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

		VectorAdd( e->origin, tmp, bbox[i] );
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

void R_MD3PushMesh ( md3mesh_file_t *pmeshheader, float move[3], float backlerp )
{
	int				i;
	md3vertex_t		*v, *ov;
	md3coord_t		*coords;
	md3elem_t		*melems;
	float			frontlerp = (1.0 - backlerp) * MD3_XYZ_SCALE;
	backlerp *= MD3_XYZ_SCALE;

	c_alias_polys += pmeshheader->num_verts;

	v = ( md3vertex_t * )(( ( byte * )pmeshheader + pmeshheader->ofs_verts + 
		currententity->frame*pmeshheader->num_verts*sizeof( md3vertex_t )));
	ov = ( md3vertex_t * )(( ( byte * )pmeshheader + pmeshheader->ofs_verts + 
		currententity->oldframe*pmeshheader->num_verts*sizeof( md3vertex_t )));
	melems = ( md3elem_t * )( ( byte * )pmeshheader + pmeshheader->ofs_elems );
	coords = ( md3coord_t * )( ( byte * )pmeshheader + pmeshheader->ofs_tcs );

	md3_mesh.numverts = pmeshheader->num_verts;
	md3_mesh.numindexes = pmeshheader->num_tris * 3;
	
	for ( i = 0; i < md3_mesh.numindexes; i+=3, melems++ ) {
		md3_indexes[i+0] = melems->index[0];
		md3_indexes[i+1] = melems->index[1];
		md3_indexes[i+2] = melems->index[2];
	}

	for ( i = 0; i < pmeshheader->num_verts; i++, v++, ov++, coords++ ) {
		Vector2Copy ( coords->tc, md3_verts[i].tex_st );
		VectorCopy ( r_uvnorms[v->norm[0]][v->norm[1]], md3_verts[i].normal );
		VectorSet ( md3_verts[i].position, 
			move[0] + ov->point[0]*backlerp + v->point[0]*frontlerp,
			move[1] + ov->point[1]*backlerp + v->point[1]*frontlerp,
			move[2] + ov->point[2]*backlerp + v->point[2]*frontlerp);
	}
}

/*
=============
GL_Md3FindFog
=============
*/
mfog_t *GL_Md3FindFog (void)
{
	int i, j;
	mbrushside_t *brushside;
	cplane_t *plane;
	mfog_t *fog;

	if ( !r_worldmodel )
		return NULL;

	fog = r_worldmodel->fogs;
	for ( i = 0; i < r_worldmodel->numfogs; i++, fog++ ) {
		if ( !fog->brush )
			continue;

		brushside = fog->brush->firstside;

		for ( j = 0; j < fog->brush->numsides; j++, brushside++ ) {
			plane = brushside->plane;

			if ( plane->type < 3 ) {
				if ( plane->dist < md3_mins[plane->type] )
					break;
			} else {
				if ( plane->dist < DotProduct (md3_mins, plane->normal) ) 
					break;
			}
		}

		if ( j == fog->brush->numsides ) {
			return fog;
		}

		brushside = fog->brush->firstside;
		for ( j = 0; j < fog->brush->numsides; j++, brushside++ ) {
			plane = brushside->plane;

			if ( plane->type < 3 ) {
				if ( plane->dist < md3_maxs[plane->type] )
					break;
			} else {
				if ( plane->dist < DotProduct (md3_maxs, plane->normal) ) 
					break;
			}
		}

		if ( j == fog->brush->numsides ) {
			return fog;
		}
	}

	return NULL;
}

/*
=============
GL_DrawMd3AliasFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
void GL_DrawMd3AliasFrameLerp (md3header_t *paliashdr, float backlerp)
{
	int				i, j;
	vec3_t			move, delta;
	byte			*meshdata;
	md3frame_t		*frame, *oldframe;
	md3mesh_file_t	*pmeshheader;
	mfog_t			*fog;

	frame = ( md3frame_t * )( ( byte * )paliashdr + paliashdr->ofs_frames +
		currententity->frame * sizeof( md3frame_t ) );
	oldframe = ( md3frame_t * )( ( byte * )paliashdr + paliashdr->ofs_frames +
		currententity->oldframe * sizeof( md3frame_t ) );
	meshdata = ( byte * )paliashdr + paliashdr->ofs_meshes;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract ( currententity->oldorigin, currententity->origin, delta );

	move[0] = DotProduct ( delta, currententity->angleVectors[0] );	// forward
	move[1] = -DotProduct ( delta, currententity->angleVectors[1] );// left
	move[2] = DotProduct ( delta, currententity->angleVectors[2] );	// up

	VectorAdd (move, oldframe->translate, move);

	for (i=0 ; i<3 ; i++)
	{
		move[i] = backlerp*move[i] + (1.0f-backlerp)*frame->translate[i];
	}

	R_Md3BBox ( currententity->frame, currententity->oldframe, currententity );
	fog = GL_Md3FindFog();
	for ( i = 0; i < paliashdr->num_meshes; i++ ) {
		pmeshheader = ( md3mesh_file_t * )( meshdata );

		// select skin
		for ( j = 0; j < pmeshheader->num_skins; j++ ) {
			md3_mesh.shader = currentmodel->skins[i][j];
			R_MD3PushMesh ( pmeshheader, move, backlerp );
			md3_mesh.shader->flush ( &md3_mesh, fog, NULL );
		}

		meshdata += pmeshheader->meshsize;
	}
}

/*
=================
R_DrawMd3Model

=================
*/
void R_DrawMd3Model (entity_t *e)
{
	md3header_t	*md3header;
	md3header = ( md3header_t * )currentmodel->extradata;

	if ( ( e->frame >= md3header->num_frames ) || ( e->frame < 0 ) )
	{
		Com_Printf ("R_DrawMd3Model %s: no such frame %d\n", 
			currentmodel->name, e->frame);
		e->frame = 0;
	}
	if ( ( e->oldframe >= md3header->num_frames ) || ( e->oldframe < 0 ) )
	{
		Com_Printf ("R_DrawMd3Model %s: no such oldframe %d\n", 
			currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	AngleVectors( e->angles, e->angleVectors[0], e->angleVectors[1], e->angleVectors[2] );

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

    qglPushMatrix ();

    qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

	if ( e->scale != 1.0f )
		qglScalef ( e->scale, e->scale, e->scale );

    qglRotatef (e->angles[1],  0, 0, 1);
    qglRotatef (e->angles[0],  0, 1, 0);		// sigh.
    qglRotatef (-e->angles[2],  1, 0, 0);

	if ( !r_lerpmodels->value ) {
		currententity->backlerp = 0;
	}

	GL_DrawMd3AliasFrameLerp ( md3header, currententity->backlerp );

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
}

void R_AddMd3ToList (entity_t *e)
{
	int	i, j;
	md3header_t *paliashdr;
	md3mesh_file_t *pmeshheader;
	byte *meshdata;
	vec3_t bbox[8];

	if ( !e->scale ) {
		return;
	}

	if ( e->flags & RF_WEAPONMODEL )
	{
		if ( r_lefthand->value == 2 )
			return;
	}
	else
	{
		if ( R_CullMd3Model ( bbox, e ) ) {
			return;
		}
	}

	paliashdr = (md3header_t *)e->model->extradata;
	meshdata = ( byte * )paliashdr + paliashdr->ofs_meshes;

	for ( i = 0; i < paliashdr->num_meshes; i++ ) {
		pmeshheader = ( md3mesh_file_t * )( meshdata );

		// select skin
		for ( j = 0; j < pmeshheader->num_skins; j++ ) {
			md3_mesh.shader = currentmodel->skins[i][j];
			R_AddMeshToList ( &md3_mesh, NULL );
		}

		meshdata += pmeshheader->meshsize;
	}
}
