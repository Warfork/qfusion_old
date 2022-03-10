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
vec4_t			pic_colors[4];

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
	pic_mesh.trneighbors = NULL;

	pic_mesh.numvertexes = 4;
	pic_mesh.xyz_array = pic_xyz;
	pic_mesh.st_array = pic_st;
	pic_mesh.colors_array = pic_colors;

	pic_mbuffer.infokey = -1;
	pic_mbuffer.mesh = &pic_mesh;
}

/*
===============
Draw_GenericPic
===============
*/
void Draw_StretchPic ( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *color, shader_t *shader )
{
	if ( !shader ) {
		return;
	}

	// lower-left
	pic_mesh.xyz_array[0][0] = x;
	pic_mesh.xyz_array[0][1] = y;
	pic_mesh.st_array[0][0] = s1; 
	pic_mesh.st_array[0][1] = t1;
	Vector4Copy ( color, pic_mesh.colors_array[0] );

	// lower-right
	pic_mesh.xyz_array[1][0] = x+w;
	pic_mesh.xyz_array[1][1] = y;
	pic_mesh.st_array[1][0] = s2; 
	pic_mesh.st_array[1][1] = t1;
	Vector4Copy ( color, pic_mesh.colors_array[1] );

	// upper-right
	pic_mesh.xyz_array[2][0] = x+w;
	pic_mesh.xyz_array[2][1] = y+h;
	pic_mesh.st_array[2][0] = s2; 
	pic_mesh.st_array[2][1] = t2;
	Vector4Copy ( color, pic_mesh.colors_array[2] );

	// upper-left
	pic_mesh.xyz_array[3][0] = x;
	pic_mesh.xyz_array[3][1] = y+h;
	pic_mesh.st_array[3][0] = s1; 
	pic_mesh.st_array[3][1] = t2;
	Vector4Copy ( color, pic_mesh.colors_array[3] );

	pic_mbuffer.shader = shader;

	// upload video right before rendering
	if ( shader->flags & SHADER_VIDEOMAP ) {
		Shader_UploadCinematic ( shader );
	}

	R_PushMesh ( &pic_mesh, MF_NONBATCHED | shader->features );
	R_RenderMeshBuffer ( &pic_mbuffer, false );
}

/*
================
Draw_Char

Draws one graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Char ( int x, int y, int num, int fontstyle, vec4_t color )
{
	float	frow, fcol;
	int		width, height;
	vec4_t	scolor;

	num &= 255;
	
	if ( (num&127) == 32 )
		return;		// space

	if ( fontstyle & FONT_BIG ) {
		width = BIG_CHAR_WIDTH;
		height = BIG_CHAR_HEIGHT;
	} else if ( fontstyle & FONT_GIANT ) {
		width = GIANT_CHAR_WIDTH;
		height = GIANT_CHAR_HEIGHT;
	} else {	// FONT_SMALL is default
		width = SMALL_CHAR_WIDTH;
		height = SMALL_CHAR_HEIGHT;
	}

	if (y <= -height)
		return;			// totally off screen

	VectorScale ( color, gl_state.inv_pow2_ovrbr, scolor );
	scolor[3] = color[3];

	frow = (num>>4)*0.0625f;
	fcol = (num&15)*0.0625f;

	if ( fontstyle & FONT_SHADOWED ) {
		Draw_StretchPic ( x+2, y+2, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, colorBlack, chars_shader );
	}
	Draw_StretchPic ( x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, scolor, chars_shader );
}

/*
=============
Draw_String
=============
*/
void Draw_String (int x, int y, char *str, int fontstyle, vec4_t color)
{
	int		num;
	float	frow, fcol;
	int		width, height;
	vec4_t	scolor;

	if ( fontstyle & FONT_BIG ) {
		width = BIG_CHAR_WIDTH;
		height = BIG_CHAR_HEIGHT;
	} else if ( fontstyle & FONT_GIANT ) {
		width = GIANT_CHAR_WIDTH;
		height = GIANT_CHAR_HEIGHT;
	} else {	// FONT_SMALL is default
		width = SMALL_CHAR_WIDTH;
		height = SMALL_CHAR_HEIGHT;
	}

	if ( y <= -height ) {
		return;			// totally off screen
	}

	VectorScale ( color, gl_state.inv_pow2_ovrbr, scolor );
	scolor[3] = color[3];

	while (*str) {
		if ( Q_IsColorString( str ) ) {
			VectorScale ( color_table[ColorIndex(str[1])], gl_state.inv_pow2_ovrbr, scolor );
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

		if ( fontstyle & FONT_SHADOWED ) {
			Draw_StretchPic ( x+2, y+2, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, colorBlack, chars_shader );
		}
		Draw_StretchPic ( x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, scolor, chars_shader );

		x += width;
	}
}

/*
=============
Draw_StringLen

Same as Draw_String, but draws "len" bytes at max
=============
*/
void Draw_StringLen (int x, int y, char *str, int len, int fontstyle, vec4_t color)
{
	char saved_byte;

	if (len < 0)
		Draw_String (x, y, str, fontstyle, color);

	saved_byte = str[len];
	str[len] = 0;
	Draw_String (x, y, str, fontstyle, color);
	str[len] = saved_byte;
}

//=============================================================================
//
//	Variable width (proportional) fonts
//

typedef struct {
	byte x, y, width;
} propchardesc_t;

static propchardesc_t propfont1[] =
{
	{0, 0, 0},		// space
	{10, 122, 9},	// !
	{153, 181, 16},	// "
	{55, 122, 17},	// #
	{78, 122, 20},	// $
	{100, 122, 24},	// %
	{152, 122, 18},	// &
	{133, 181, 7},	// '
	{206, 122, 10},	// (
	{230, 122, 10},	// )
	{177, 122, 18},	// *
	{30, 152, 18},	// +
	{85, 181, 7},	// ,
	{34, 94, 11},	// -
	{110, 181, 7},	// .
	{130, 152, 14},	// /

	{21, 64, 19},	// 0
	{41, 64, 13},	// 1
	{57, 64, 19},	// 2
	{77, 64, 20},	// 3
	{97, 64, 21},	// 4
	{119, 64, 20},	// 5
	{140, 64, 20},	// 6
	{204, 64, 16},	// 7
	{161, 64, 19},	// 8
	{182, 64, 19},	// 9
	{59, 181, 7},	// :
	{35, 181, 7},	// ;
	{203, 152, 15},	// <
	{57, 94, 14},	// =
	{228, 152, 15},	// >
	{177, 181, 19},	// ?

	{28, 122, 23},	// @
	{4, 4, 20},		// A
	{26, 4, 20},	// B
	{47, 4, 20},	// C
	{68, 4, 20},	// D
	{89, 4, 14},	// E
	{105, 4, 14},	// F
	{120, 4, 20},	// G
	{142, 4, 19},	// H
	{163, 4, 10},	// I
	{174, 4, 18},	// J
	{194, 4, 20},	// K
	{215, 4, 13},	// L
	{229, 4, 25},	// M
	{5, 34, 20},	// N
	{26, 34, 20},	// O

	{47, 34, 19},	// P
	{67, 34, 20},	// Q
	{89, 34, 19},	// R
	{110, 34, 19},	// S
	{130, 34, 14},	// T
	{145, 34, 20},	// U
	{166, 34, 19},	// V
	{186, 34, 28},	// W
	{214, 34, 20},	// X
	{234, 34, 19},	// Y
	{4, 64, 15},	// Z
	{59, 152, 9},	// [
	{105, 152, 14},	// \ 
	{82, 152, 9},	// ]
	{128, 122, 17},	// ^
	{4, 152, 21},	// _

	{9, 94, 8},		// `
	{153, 152, 14},	// {
	{9, 181, 10},	// |
	{179, 152, 14},	// }
	{79, 94, 17},	// ~
};


/*
=============
Draw_PropString
=============
*/
void Draw_PropString (int x, int y, char *str, int fontstyle, vec4_t color)
{
	int		num;
	vec4_t	scolor;
	float	frow, fcol;
	int		width, height;
	float	scale, sheight, swidth, spacing;

	height = PROP_CHAR_HEIGHT;

	if ( y <= -height )
		return;			// totally off screen

	if ( fontstyle & FONT_SMALL ) {
		scale = PROP_SMALL_SCALE;
		spacing = PROP_SMALL_SPACING;
	} else {
		scale = PROP_BIG_SCALE;
		spacing = PROP_BIG_SPACING;
	}

	sheight = height * scale;

	VectorScale ( color, gl_state.inv_pow2_ovrbr, scolor );
	scolor[3] = color[3];

	while (*str) {
		if ( Q_IsColorString( str ) ) {
			VectorScale ( color_table[ColorIndex(str[1])], gl_state.inv_pow2_ovrbr, scolor );
			str += 2;
			continue;
		}

		num = toupper(*str++);

		if ( num == 32 ) {
			x += 11 * scale;
			continue;		// space
		}

		if (num >= '{' && num <= '~')
			num -= '{' - 'a';

		num -= 32;

		if ((unsigned)num >= sizeof(propfont1)/sizeof(propfont1[0]))
			continue;

		fcol = propfont1[num].x * (1/256.0f);
		frow = propfont1[num].y * (1/256.0f);
		width = propfont1[num].width;
		swidth = width * scale;

		if ( fontstyle & FONT_SHADOWED ) {
			Draw_StretchPic ( x+2, y+2, swidth, sheight, fcol, frow, fcol + width/256.0f, frow + height*(1/256.0f), colorBlack, propfont1_shader );
		}
		Draw_StretchPic ( x, y, swidth, sheight, fcol, frow, fcol + width/256.0f, frow + height*(1/256.0f), scolor, propfont1_shader );

		x += swidth + spacing;
	}
}


/*
=============
Q_PropStringLength

Calculate total width of a string, for centering text on the screen
=============
*/
int Q_PropStringLength (char *str, int fontstyle)
{
	int		num;
	float	scale, swidth, spacing;
	int		x;

	if ( fontstyle & FONT_SMALL ) {
		scale = PROP_SMALL_SCALE;
		spacing = PROP_SMALL_SPACING;
	} else {
		scale = PROP_BIG_SCALE;
		spacing = PROP_BIG_SPACING;
	}

	x = 0;

	while (*str) {
		if ( Q_IsColorString( str ) ) {
			str += 2;
			continue;
		}

		num = toupper(*str++);

		if ( num == 32 ) {
			x += 11 * scale;
			continue;		// space
		}

		if (num >= '{' && num <= '~')
			num -= '{' - 'a';

		num -= 32;

		if ((unsigned)num >= sizeof(propfont1)/sizeof(propfont1[0]))
			continue;

		swidth = propfont1[num].width * scale;

		x += swidth + spacing;
	}

	return x;
}


/*
=============
Draw_CenteredPropString
=============
*/
void Draw_CenteredPropString (int y, char *str, int fontstyle, vec4_t color)
{
	int x;

	x = (vid.width - Q_PropStringLength (str, fontstyle)) / 2;
	Draw_PropString (x, y, str, fontstyle, color);
}

/*
=============
Draw_TileClear

This repeats tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	shader_t *shader;
	shaderpass_t *pass;
	image_t  *image;
	float iw, ih;

	shader = R_RegisterPic ( pic );
	pass = shader->passes;
	if (pass->flags & SHADER_PASS_ANIMMAP) {
		image = pass->anim_frames[(int)(r_shadertime * pass->anim_fps) % pass->anim_numframes];
	} else {
		image = pass->anim_frames[0];
	}

	if (!image)
	{
		Com_Printf ("Can't find pic: %s\n", pic);
		return;
	}

	iw = 1.0f / image->width;
	ih = 1.0f / image->height;

	GL_Bind (image->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (x*iw, y*ih);
	qglVertex2f (x, y);
	qglTexCoord2f ((x+w)*iw, y*ih);
	qglVertex2f (x+w, y);
	qglTexCoord2f ((x+w)*iw, (y+h)*ih);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f (x*iw, (y+h)*ih);
	qglVertex2f (x, y+h);
	qglEnd ();
}

/*
=============
Draw_FillRect

Fills a box of pixels with a single color
=============
*/
void Draw_FillRect (int x, int y, int w, int h, vec4_t color)
{
	qglDisable (GL_TEXTURE_2D);
	qglColor4fv ( color );
	qglBegin (GL_QUADS);

	qglVertex2f (x,y);
	qglVertex2f (x+w, y);
	qglVertex2f (x+w, y+h);
	qglVertex2f (x, y+h);

	qglEnd ();

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
	GLSTATE_ENABLE_BLEND;
	qglDisable (GL_TEXTURE_2D);
	
	qglColor4f (0, 0, 0, 0.8);
	qglBegin (GL_QUADS);

	qglVertex2f (0,0);
	qglVertex2f (vid.width, 0);
	qglVertex2f (vid.width, vid.height);
	qglVertex2f (0, vid.height);

	qglEnd ();
	qglColor4f (1,1,1,1);

	qglEnable (GL_TEXTURE_2D);
	GLSTATE_DISABLE_BLEND;
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, int frame, byte *data)
{
	int			image_width, image_height;
	unsigned	image32[512*256], *pic;

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
			gl_tex_solid_format, 
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

