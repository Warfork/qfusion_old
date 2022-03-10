/*
Copyright (C) 2002-2003 Victor Luchits

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

#include "cg_local.h"

/*
===============================================================================

STRINGS DRAWING

===============================================================================
*/

/*
================
CG_DrawChar

Draws one graphics character with 0 being transparent.
================
*/
void CG_DrawChar ( int x, int y, int num, int fontstyle, vec4_t color )
{
	float	frow, fcol;
	int		width, height;

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

	frow = (num>>4)*0.0625f;
	fcol = (num&15)*0.0625f;

	if( fontstyle & FONT_SHADOWED )
		trap_R_DrawStretchPic ( x+2, y+2, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, colorBlack, cgs.shaderCharset );
	trap_R_DrawStretchPic ( x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, color, cgs.shaderCharset );
}

/*
=============
CG_DrawString
=============
*/
void CG_DrawString ( int x, int y, char *str, int fontstyle, vec4_t color )
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

	if( y <= -height )
		return;			// totally off screen

	Vector4Copy ( color, scolor );

	while (*str) {
		if ( Q_IsColorString( str ) ) {
			VectorCopy ( color_table[ColorIndex(str[1])], scolor );
			str += 2;
			continue;
		}

		num = *str++;
		num &= 255;

		if ( (num&127) != 32 ) {	// not a space
			frow = (num>>4)*0.0625f;
			fcol = (num&15)*0.0625f;

			if ( fontstyle & FONT_SHADOWED ) {
				trap_R_DrawStretchPic ( x+2, y+2, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, colorBlack, cgs.shaderCharset );
			}
			trap_R_DrawStretchPic ( x, y, width, height, fcol, frow, fcol+0.0625f, frow+0.0625f, scolor, cgs.shaderCharset );
		}

		x += width;
	}
}

/*
=============
CG_DrawStringLen

Same as CG_DrawString, but draws "len" bytes at max
=============
*/
void CG_DrawStringLen ( int x, int y, char *str, int len, int fontstyle, vec4_t color )
{
	if ( len < 0 ) {
		CG_DrawString ( x, y, str, fontstyle, color );
	} else {
		char saved_byte;

		saved_byte = str[len];
		str[len] = 0;
		CG_DrawString ( x, y, str, fontstyle, color );
		str[len] = saved_byte;
	}
}

//=============================================================================
//
//	Variable width (proportional) fonts
//

typedef struct {
	qbyte x, y, width;
} propchardesc_t;

static const propchardesc_t propfont1[] =
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
	{105, 152, 14},	// backslash 
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
CG_Draw_PropString
=============
*/
void CG_DrawPropString ( int x, int y, char *str, int fontstyle, vec4_t color )
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

	Vector4Copy ( color, scolor );

	while (*str) {
		if ( Q_IsColorString( str ) ) {
			VectorCopy ( color_table[ColorIndex(str[1])], scolor );
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

		if( fontstyle & FONT_SHADOWED )
			trap_R_DrawStretchPic ( x+2, y+2, swidth, sheight, fcol, frow, fcol + width/256.0f, frow + height*(1/256.0f), colorBlack, cgs.shaderPropfont );
		trap_R_DrawStretchPic ( x, y, swidth, sheight, fcol, frow, fcol + width/256.0f, frow + height*(1/256.0f), scolor, cgs.shaderPropfont );

		x += swidth + spacing;
	}
}


/*
=============
CG_PropStringLength

Calculate total width of a string, for centering text on the screen
=============
*/
int CG_PropStringLength ( char *str, int fontstyle )
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
CG_DrawCenteredPropString
=============
*/
void CG_DrawCenteredPropString ( int y, char *str, int fontstyle, vec4_t color )
{
	int x;

	x = (cgs.vidWidth - CG_PropStringLength (str, fontstyle)) / 2;
	CG_DrawPropString ( x, y, str, fontstyle, color );
}

/*
=============
CG_FillRect
=============
*/
void CG_FillRect ( int x, int y, int w, int h, vec4_t color ) {
	trap_R_DrawStretchPic ( x, y, w, h, 0, 0, 1, 1, color, cgs.shaderWhite );
}

/*
=============
CG_DrawHUDString
=============
*/
void CG_DrawHUDString ( char *string, int x, int y, int centerwidth, int fontstyle, vec4_t color )
{
	char	*s;
	int		margin;
	int		length, l;
	int		width, height;

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

	margin = x;

	while ( *string ) {
		// scan out one line of text from the string
		s = string;
		length = l = 0;
		while (*string && *string != '\n') {
			if ( Q_IsColorString (string) ) {
				l += 2;
				string += 2;
			} else {
				l++;
				length++;
				string++;
			}
		}

		if( centerwidth )
			x = margin + (centerwidth - length*width)/2;
		else
			x = margin;

		CG_DrawStringLen ( x, y, s, l, fontstyle, color );

		if ( *string ) {
			string++;	// skip the \n
			x = margin;
			y += height;
		}
	}
}

/*
==============
CG_DrawHUDField
==============
*/
void CG_DrawHUDField ( int x, int y, float *color, int width, int value )
{
	char	num[16], *ptr;
	int		length, maxwidth;

	if( width < 0 )
		return;

	maxwidth = 5;

	// draw number string
	Q_snprintfz( num, sizeof(num), "%i", value );
	length = strlen ( num );
	if( !width )
		width = length;
	else if( width > maxwidth )
		width = maxwidth;
	x += 2 + BIG_CHAR_WIDTH * (width - length);

	ptr = num;
	while( *ptr && length ) {
		CG_DrawChar( x, y, *ptr, FONT_BIG, color );
		x += BIG_CHAR_WIDTH;
		ptr++;
		length--;
	}
}

/*
==============
CG_DrawHUDField2
==============
*/
void CG_DrawHUDField2( int x, int y, float *color, int width, int value )
{
	char	num[16], *ptr;
	int		frame, length, maxwidth;

	if( width < 0 )
		return;

	maxwidth = 5;

	// draw number string
	Q_snprintfz( num, sizeof(num), "%i", value );
	length = strlen ( num );
	if( !width )
		width = length;
	else if( width > maxwidth )
		width = maxwidth;
	x += 2 + 32 * (width - length);

	ptr = num;
	while( *ptr && length ) {
		if( *ptr == '-' )
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		trap_R_DrawStretchPic ( x, y, 32, 32, 0, 0, 1, 1, color, CG_MediaShader(cgs.media.sbNums[frame]) );
		x += 32;
		ptr++;
		length--;
	}
}

/*
================
CG_DrawModel
================
*/
void CG_DrawModel ( int x, int y, int w, int h, struct model_s *model, struct shader_s *shader, vec3_t origin, vec3_t angles )
{
	refdef_t refdef;
	entity_t entity;

	if( !model )
		return;

	memset( &refdef, 0, sizeof( refdef ) );

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;
	refdef.fov_x = 30;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = cg.time;
	refdef.rdflags = RDF_NOWORLDMODEL;

	memset( &entity, 0, sizeof( entity ) );
	entity.model = model;
	entity.customShader = shader;
	entity.scale = 1.0f;
	entity.flags = RF_FULLBRIGHT | RF_NOSHADOW | RF_FORCENOLOD;
	VectorCopy( origin, entity.origin );
	VectorCopy( entity.origin, entity.origin2 );

	AnglesToAxis ( angles, entity.axis );

	trap_R_ClearScene ();
	CG_SetBoneposesForTemporaryEntity( &entity );
	CG_AddEntityToScene( &entity );
	trap_R_RenderScene( &refdef );
}

/*
================
CG_DrawHUDModel
================
*/
void CG_DrawHUDModel( int x, int y, int w, int h, struct model_s *model, struct shader_s *shader, float yawspeed )
{
	vec3_t mins, maxs;
	vec3_t origin, angles;

	// get model bounds
	trap_R_ModelBounds ( model, mins, maxs );

	// try to fill the the window with the model
	origin[0] = 0.5 * (maxs[2] - mins[2]) * (1.0 / 0.179);
	origin[1] = 0.5 * (mins[1] + maxs[1]);
	origin[2] = -0.5 * (mins[2] + maxs[2]);
	VectorSet ( angles, 0, anglemod(yawspeed * ( cg.time & 2047 ) * (360.0 / 2048.0)), 0 );

	CG_DrawModel( x, y, w, h, model, shader, origin, angles );
}

/*
================
CG_DrawHUDRect
================
*/
void CG_DrawHUDRect ( int x, int y, int w, int h, int val, int maxval, vec4_t color )
{
	float frac;

	if( val < 1 || maxval < 1 || w < 1 || h < 1 )
		return;

	if( val >= maxval )
		frac = 1.0f;
	else
		frac = (float)val / (float)maxval;

	if( h > w )
		h = (int)((float)h * frac + 0.5);
	else
		w = (int)((float)w * frac + 0.5);

	CG_FillRect( x, y, w, h, color );
}
