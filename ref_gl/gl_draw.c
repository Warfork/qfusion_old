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

// draw.c

#include "gl_local.h"

shader_t	*chars_shader;

void R_SetShaderState ( shader_t *shader );
void R_SetShaderpassState ( shaderpass_t *pass );
int R_ShaderpassTex ( shaderpass_t *pass, int lmtex );

mesh_t	r_picture_mesh;
mvertex_t r_picture_verts[4];
unsigned r_picture_indexes[6];
float r_picture_defcolour[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
	chars_shader = R_RegisterShaderNoMip("gfx/2d/bigchars");

	r_picture_mesh.numindexes = 6;
	r_picture_mesh.firstindex = r_picture_indexes;
	r_picture_mesh.firstindex[0] = 0;
	r_picture_mesh.firstindex[1] = 1;
	r_picture_mesh.firstindex[2] = 2;
	r_picture_mesh.firstindex[3] = 0;
	r_picture_mesh.firstindex[4] = 2;
	r_picture_mesh.firstindex[5] = 3;

	r_picture_mesh.numverts = 4;
	r_picture_mesh.firstvert = r_picture_verts;

	r_picture_mesh.tex_centre_tc[0] = 0.5f;
	r_picture_mesh.tex_centre_tc[1] = 0.5f;

	r_picture_mesh.lightmaptexturenum = -1;
}

/*
===============
Draw_GenericPic
===============
*/
void Draw_GenericPic ( shader_t *shader, int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *colour )
{
	r_picture_mesh.firstvert[0].position[0] = x;
	r_picture_mesh.firstvert[0].position[1] = y;
	r_picture_mesh.firstvert[0].position[2] = 0;
	r_picture_mesh.firstvert[0].tex_st[0] = s1; 
	r_picture_mesh.firstvert[0].tex_st[1] = t1;
	Vector4Copy ( colour, r_picture_mesh.firstvert[0].colour );

	r_picture_mesh.firstvert[1].position[0] = x+w;
	r_picture_mesh.firstvert[1].position[1] = y;
	r_picture_mesh.firstvert[1].position[2] = 0;
	r_picture_mesh.firstvert[1].tex_st[0] = s2; 
	r_picture_mesh.firstvert[1].tex_st[1] = t1;
	Vector4Copy ( colour, r_picture_mesh.firstvert[1].colour );

	r_picture_mesh.firstvert[2].position[0] = x+w;
	r_picture_mesh.firstvert[2].position[1] = y+h;
	r_picture_mesh.firstvert[2].position[2] = 0;
	r_picture_mesh.firstvert[2].tex_st[0] = s2; 
	r_picture_mesh.firstvert[2].tex_st[1] = t2;
	Vector4Copy ( colour, r_picture_mesh.firstvert[2].colour );

	r_picture_mesh.firstvert[3].position[0] = x;
	r_picture_mesh.firstvert[3].position[1] = y+h;
	r_picture_mesh.firstvert[3].position[2] = 0;
	r_picture_mesh.firstvert[3].tex_st[0] = s1; 
	r_picture_mesh.firstvert[3].tex_st[1] = t2;
	Vector4Copy ( colour, r_picture_mesh.firstvert[3].colour );

	r_picture_mesh.shader = shader;

	R_RenderMeshGeneric ( &r_picture_mesh );
}

/*
================
Draw_Char

Draws one graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Char ( int x, int y, int num, fontstyle_t fntstl, vec4_t colour )
{
	float	frow, fcol;
	int		width, height;

	if ( fntstl == FONT_SMALL ) {
		width = SMALL_CHAR_WIDTH;
		height = SMALL_CHAR_HEIGHT;
	} else if ( fntstl == FONT_BIG ) {
		width = BIG_CHAR_WIDTH;
		height = BIG_CHAR_HEIGHT;
	} else if ( fntstl == FONT_GIANT ) {
		width = GIANT_CHAR_WIDTH;
		height = GIANT_CHAR_HEIGHT;
	}

	if (y <= -height)
		return;			// totally off screen

	num &= 255;
	
	if ( (num&127) == 32 )
		return;		// space

	frow = (num>>4)*0.0625f;
	fcol = (num&15)*0.0625f;

	Draw_GenericPic ( chars_shader, x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, colour );
}

/*
=============
Draw_StringX
=============
*/
void Draw_StringLen (int x, int y, char *str, int len, fontstyle_t fntstl, vec4_t colour)
{
	int		num;
	float	frow, fcol;
	int		width, height;
	vec4_t	scolour;

	if ( fntstl == FONT_SMALL ) {
		width = SMALL_CHAR_WIDTH;
		height = SMALL_CHAR_HEIGHT;
	} else if ( fntstl == FONT_BIG ) {
		width = BIG_CHAR_WIDTH;
		height = BIG_CHAR_HEIGHT;
	} else if ( fntstl == FONT_GIANT ) {
		width = GIANT_CHAR_WIDTH;
		height = GIANT_CHAR_HEIGHT;
	}

	if ( y <= -height ) {
		return;			// totally off screen
	}

	Vector4Copy ( colour, scolour );

	while (*str && len--) {
		if ( Q_IsColorString( str ) ) {
			VectorCopy ( color_table[ColorIndex(str[1])], scolour );
			str += 2;
			continue;
		}

		num = *str++;
		num &= 255;
	
		if ( (num&127) == 32 ) {
			x += width;
			continue;		// space
		}

		frow = (num>>4)*0.0625f;
		fcol = (num&15)*0.0625f;

		Draw_GenericPic ( chars_shader, x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, scolour );
		x += width;
	}
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int w, int h, char *pic)
{
	shader_t *shader = R_RegisterShaderNoMip (pic);
	Draw_GenericPic ( shader, x, y, w, h, 0, 0, 1, 1, r_picture_defcolour );
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, char *pic)
{
	image_t	*gl;
	shader_t *shader = R_RegisterShaderNoMip(pic);

	if ( !shader->numpasses )
		return;

	if (shader->pass[0].flags & SHADER_PASS_ANIMMAP) {
		int frame = (int)(r_shadertime * shader->pass[0].anim_fps) % shader->pass[0].anim_numframes;
		gl = shader->pass[0].anim_frames[frame];
	} else {
		gl = shader->pass[0].texref;
	}

	if ( !gl )
		return;

	Draw_GenericPic ( shader, x, y, gl->width, gl->height, 0, 0, 1, 1, r_picture_defcolour );
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	float	v[3];
	float	tc[2];
	shader_t *shader = R_RegisterShaderNoMip ( pic );
	image_t  *image = shader->pass[0].texref;

	if (!image)
	{
		Com_Printf ("Can't find pic: %s\n", pic);
		return;
	}

	GL_Bind (image->texnum);

	R_PushElem (0);
	R_PushElem (1);
	R_PushElem (2);
	R_PushElem (0);
	R_PushElem (2);
	R_PushElem (3);

	VectorSet (v, x, y, 0);
	tc[0] = x*(1.0/64.0); tc[1] = y*(1.0/64.0);
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x+w, y, 0);
	tc[0] = (x+w)*(1.0/64.0); tc[1] = y*(1.0/64.0);
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x+w, y+h, 0);
	tc[0] = (x+w)*(1.0/64.0); tc[1] = (y+h)*(1.0/64.0);
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x, y+h, 0);
	tc[0] = x*(1.0/64.0); tc[1] = (y+h)*(1.0/64.0);
	R_PushCoord (tc);
	R_PushVertex (v);

	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	float	v[3];
	float	vcolor[4];

	if ( (unsigned)c > 255)
		Com_Error (ERR_FATAL, "Draw_Fill: bad color");

	qglDisable (GL_TEXTURE_2D);

	R_PushElem (0);
	R_PushElem (1);
	R_PushElem (2);
	R_PushElem (0);
	R_PushElem (2);
	R_PushElem (3);

	VectorCopy (d_8to24floattable[c], vcolor);
	vcolor[3] = 1.0;

	VectorSet (v, x, y, 0);
	R_PushColor (vcolor);
	R_PushVertex (v);

	VectorSet (v, x+w, y, 0);
	R_PushColor (vcolor);
	R_PushVertex (v);

	VectorSet (v, x+w, y+h, 0);
	R_PushColor (vcolor);
	R_PushVertex (v);

	VectorSet (v, x, y+h, 0);
	R_PushColor (vcolor);
	R_PushVertex (v);

	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();

	qglColor3f (1,1,1);
	qglEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	float	v[3];
	byte	vcolor[4];

	GLSTATE_ENABLE_BLEND
	qglDisable (GL_TEXTURE_2D);
	
	R_PushElem (0);
	R_PushElem (1);
	R_PushElem (2);
	R_PushElem (0);
	R_PushElem (2);
	R_PushElem (3);

	vcolor[0] = 0;
	vcolor[1] = 0;
	vcolor[2] = 0;
	vcolor[3] = 204;

	VectorSet (v, 0, 0, 0);
	R_PushColorByte (vcolor);
	R_PushVertex (v);

	VectorSet (v, vid.width, 0, 0);
	R_PushColorByte (vcolor);
	R_PushVertex (v);

	VectorSet (v, vid.width, vid.height, 0);
	R_PushColorByte (vcolor);
	R_PushVertex (v);

	VectorSet (v, 0, vid.height, 0);
	R_PushColorByte (vcolor);
	R_PushVertex (v);

	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();

	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	extern unsigned	r_rawpalette[256];
	extern unsigned *gl_texscaled;
	unsigned	*dest = gl_texscaled;
	unsigned char *inrow;
	int			i, j;
	int			frac, fracstep;
	float		t, v[3], tc[2], hscale = (float)rows/256.0;

	t = hscale*hscale - 1.0/512.0;
	fracstep = cols*0x10000/256;

	memset ( gl_texscaled, 0, sizeof(gl_texscaled) );

	for (i=0 ; i<256 ; i++, dest += 256)
	{
		inrow = data + cols*(int)(i*hscale);
		frac = fracstep >> 1;

		for (j = 0 ; j<256 ; j+=4)
		{
			dest[j] = r_rawpalette[inrow[frac>>16]];
			frac += fracstep;
			dest[j+1] = r_rawpalette[inrow[frac>>16]];
			frac += fracstep;
			dest[j+2] = r_rawpalette[inrow[frac>>16]];
			frac += fracstep;
			dest[j+3] = r_rawpalette[inrow[frac>>16]];
			frac += fracstep;
		}
	}

	GL_Bind (0);

	qglTexImage2D (GL_TEXTURE_2D, 0, gl_tex_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, gl_texscaled);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) 
		GLSTATE_DISABLE_ALPHATEST
	
	R_PushElem (0);
	R_PushElem (1);
	R_PushElem (2);
	R_PushElem (0);
	R_PushElem (2);
	R_PushElem (3);

	VectorSet (v, x, y, 0);
	tc[0] = 1.0/512.0; tc[1] = 1.0/512.0;
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x+w, y, 0);
	tc[0] = 511.0/512.0; tc[1] = 1.0/512.0;
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x+w, y+h, 0);
	tc[0] = 511.0/512.0; tc[1] = t;
	R_PushCoord (tc);
	R_PushVertex (v);

	VectorSet (v, x, y+h, 0);
	tc[0] = 1.0/512.0; tc[1] = t;
	R_PushCoord (tc);
	R_PushVertex (v);

	R_LockArrays ();
	R_FlushArrays ();
	R_UnlockArrays ();
	R_ClearArrays ();
}

