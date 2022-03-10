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

// r_draw.c

#include "r_local.h"

shader_t		*chars_shader;
shader_t		*propfont1_shader, *propfont1_glow_shader;
shader_t		*propfont2_shader;

vec4_t			pic_xyz[4];
vec2_t			pic_st[4];
byte_vec4_t		pic_colors[4];

mesh_t			pic_mesh;
meshbuffer_t	pic_mbuffer;

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
	chars_shader = R_RegisterPic( "gfx/2d/bigchars" );
	propfont1_shader = R_RegisterPic( "menu/art/font1_prop" );
	propfont1_glow_shader = R_RegisterPic( "menu/art/font1_prop_glo" );
	propfont2_shader = R_RegisterPic( "menu/art/font2_prop" );

	pic_mesh.numindexes = 6;
	pic_mesh.indexes = r_quad_indexes;
#ifdef SHADOW_VOLUMES
	pic_mesh.trneighbors = NULL;
#endif

	pic_mesh.numvertexes = 4;
	pic_mesh.xyz_array = pic_xyz;
	pic_mesh.st_array = pic_st;
	pic_mesh.colors_array = pic_colors;
}

/*
===============
Draw_GenericPic
===============
*/
void Draw_StretchPic ( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, shader_t *shader )
{
	byte_vec4_t bcolor;

	if ( !shader ) {
		return;
	}

	// FIXME: this is a gross hack
	if ( shader == chars_shader || 
		shader == propfont1_shader ||
		shader == propfont1_glow_shader ||
		shader == propfont2_shader ) {
		bcolor[0] = R_FloatToByte ( color[0] * gl_state.inv_pow2_ovrbr );
		bcolor[1] = R_FloatToByte ( color[1] * gl_state.inv_pow2_ovrbr );
		bcolor[2] = R_FloatToByte ( color[2] * gl_state.inv_pow2_ovrbr );
		bcolor[3] = R_FloatToByte ( color[3] );
	} else {
		bcolor[0] = R_FloatToByte ( color[0] );
		bcolor[1] = R_FloatToByte ( color[1] );
		bcolor[2] = R_FloatToByte ( color[2] );
		bcolor[3] = R_FloatToByte ( color[3] );
	}

	// lower-left
	pic_mesh.xyz_array[0][0] = x;
	pic_mesh.xyz_array[0][1] = y;
	pic_mesh.st_array[0][0] = s1; 
	pic_mesh.st_array[0][1] = t1;
	Vector4Copy ( bcolor, pic_mesh.colors_array[0] );

	// lower-right
	pic_mesh.xyz_array[1][0] = x+w;
	pic_mesh.xyz_array[1][1] = y;
	pic_mesh.st_array[1][0] = s2; 
	pic_mesh.st_array[1][1] = t1;
	Vector4Copy ( bcolor, pic_mesh.colors_array[1] );

	// upper-right
	pic_mesh.xyz_array[2][0] = x+w;
	pic_mesh.xyz_array[2][1] = y+h;
	pic_mesh.st_array[2][0] = s2; 
	pic_mesh.st_array[2][1] = t2;
	Vector4Copy ( bcolor, pic_mesh.colors_array[2] );

	// upper-left
	pic_mesh.xyz_array[3][0] = x;
	pic_mesh.xyz_array[3][1] = y+h;
	pic_mesh.st_array[3][0] = s1; 
	pic_mesh.st_array[3][1] = t2;
	Vector4Copy ( bcolor, pic_mesh.colors_array[3] );

	pic_mbuffer.shader = shader;

	// upload video right before rendering
	if ( shader->flags & SHADER_VIDEOMAP ) {
		Shader_UploadCinematic ( shader );
	}

	R_PushMesh ( &pic_mesh, MF_NONBATCHED | shader->features );
	R_RenderMeshBuffer ( &pic_mbuffer, qfalse );
}


//=============================================================================

/*
=============
Draw_StretchRaw
=============
*/
void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, int frame, qbyte *data)
{
	int			image_width, image_height;
	unsigned	image32[512*256], *pic;

	GL_EnableMultitexture ( qfalse );

	GL_Bind (0);

	for (image_width = 1 ; image_width < cols ; image_width <<= 1)
		;
	for (image_height = 1 ; image_height < rows ; image_height <<= 1)
		;

	// don't bother with large textures
	clamp ( image_width, 1, min(512, gl_maxtexsize) );
	clamp ( image_height, 1, min(256, gl_maxtexsize) );

	if ( (image_width == cols) && (image_height == rows) )
	{
		pic = (unsigned *)data;
	}
	else
	{
		pic = image32;
		GL_ResampleTexture ( (unsigned *)data, cols, rows, image32, image_width, image_height );
	}

	if ( frame != 1 ) {
		qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
			image_width, image_height, 
			GL_RGBA, GL_UNSIGNED_BYTE, pic);
	} else {
		qglTexImage2D (GL_TEXTURE_2D, 0, 
			GL_RGB, 
			image_width, image_height, 0, 
			GL_RGBA, GL_UNSIGNED_BYTE, pic);

		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	qglBegin (GL_QUADS);

	qglTexCoord2f ( 1.0/1024, 1.0/1024.0 );
	qglVertex2f ( x, y );
	qglTexCoord2f ( 1023.0/1024.0, 1.0/1024.0 );
	qglVertex2f ( x+w, y );
	qglTexCoord2f ( 1023.0/1024.0, 1023.0/1024.0 );
	qglVertex2f ( x+w, y+h );
	qglTexCoord2f ( 1.0/1024.0, 1023.0/1024.0 );
	qglVertex2f ( x, y+h );

	qglEnd ();
}
