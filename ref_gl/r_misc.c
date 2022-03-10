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
// r_misc.c

#include "r_local.h"
#include "jpeglib.h"

/*
==================
R_InitNoTexture
==================
*/
void R_InitNoTexture (void)
{
	int x, y;
	byte *data;
	byte dottexture[8][8] =
	{
		{0,0,0,0,0,0,0,0},
		{0,0,1,1,0,0,0,0},
		{0,1,1,1,1,0,0,0},
		{0,1,1,1,1,0,0,0},
		{0,0,1,1,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
	};

	//
	// also use this for bad textures, but without alpha
	//
	data = Q_malloc ( 8 * 8 * 4 );

	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[(y*8 + x)*4+0] = dottexture[x&3][y&3]*128;
			data[(y*8 + x)*4+1] = dottexture[x&3][y&3]*128;
			data[(y*8 + x)*4+2] = dottexture[x&3][y&3]*128;
			data[(y*8 + x)*4+3] = 255;
		}
	}

	r_notexture = GL_LoadPic ("***r_notexture***", data, 8, 8, 0, 32);

	Q_free ( data );
}

/*
==================
R_InitDynamicLightTexture
==================
*/
void R_InitDynamicLightTexture (void)
{
	int x, y;
	int dx2, dy, d;
	byte *data;

	//
	// dynamic light texture
	//
	data = Q_malloc ( 64 * 64 * 4 );

	for (x = 0; x < 64; x++) 
	{
		dx2 = x - 32;
		dx2 = dx2 * dx2 + 8;

		for (y = 0; y < 64; y++) 
		{
			dy = y - 32;
			d = (int)(65536.0f * ((1.0f / (dx2 + dy * dy + 32.0f)) - 0.0005) + 0.5f);
			if ( d < 50 ) d = 0; else if ( d > 255 ) d = 255;

			data[(y*64 + x) * 4 + 0] = d;
			data[(y*64 + x) * 4 + 1] = d;
			data[(y*64 + x) * 4 + 2] = d;
			data[(y*64 + x) * 4 + 3] = 255;
		}
	}

	r_dlighttexture = GL_LoadPic ("***r_dlighttexture***", data, 64, 64, IT_NOPICMIP|IT_NOMIPMAP|IT_CLAMP, 32);

	Q_free ( data );
}

/*
==================
R_InitParticleTexture
==================
*/
void R_InitParticleTexture (void)
{
	int x, y;
	int dx2, dy, d;
	byte *data;

	//
	// particle texture
	//
	data = Q_malloc ( 32 * 32 * 4 );

	for (x=0 ; x<32 ; x++)
	{
		dx2 = x - 16;
		dx2 = dx2 * dx2;

		for (y=0 ; y<32 ; y++)
		{
			dy = y - 16;
			d = 255 - 25 * sqrt (dx2 + dy * dy);
			clamp (d, 0, 255);

			data[(y*32 + x) * 4 + 0] = 255;
			data[(y*32 + x) * 4 + 1] = 255;
			data[(y*32 + x) * 4 + 2] = 255;
			data[(y*32 + x) * 4 + 3] = d;
		}
	}

	r_particletexture = GL_LoadPic ("***r_particletexture***", data, 32, 32, IT_NOMIPMAP, 32);

	Q_free ( data );
}

/*
==================
R_InitWhiteTexture
==================
*/
void R_InitWhiteTexture (void)
{
	byte *data;

	//
	// white texture
	//
	data = Q_malloc ( 32 * 32 * 4 );
	memset ( data, 255, 32 * 32 * 4 );

	r_whitetexture = GL_LoadPic ("***r_whitetexture***", data, 32, 32, 0, 32);
	
	Q_free ( data );
}

/*
==================
R_InitFogTexture
==================
*/
void R_InitFogTexture (void)
{
	byte *data;
	int x, y;
	float tw = 1.0f / ((float)FOG_TEXTURE_WIDTH - 1.0f);
	float th = 1.0f / ((float)FOG_TEXTURE_HEIGHT - 1.0f);
	float tx, ty, t;

	//
	// fog texture
	//
	data = Q_malloc ( FOG_TEXTURE_WIDTH*FOG_TEXTURE_HEIGHT );

	for ( y = 0, ty = 0.0f; y < FOG_TEXTURE_HEIGHT; y++, ty += th )
	{
		for ( x = 0, tx = 0.0f; x < FOG_TEXTURE_WIDTH; x++, tx += tw )
		{
			t = (float)(sqrt( tx ) * 255.0);
			data[x+y*FOG_TEXTURE_WIDTH] = (byte)(min( t, 255.0f ));
		}

		data[y] = 0;
	}

	r_fogtexture = GL_LoadPic ("***r_fogtexture***", data, FOG_TEXTURE_WIDTH, FOG_TEXTURE_HEIGHT, IT_FOG|IT_NOMIPMAP, 8);
	
	Q_free ( data );
}

/*
==================
R_InitBuiltInTextures
==================
*/
void R_InitBuiltInTextures (void)
{
	R_InitNoTexture ();
	R_InitDynamicLightTexture ();
	R_InitParticleTexture ();
	R_InitWhiteTexture ();
	R_InitFogTexture ();
}

float	bubble_sintable[33], bubble_costable[33];

void R_InitBubble (void) 
{
	int i;
	float a;
	float *bub_sin, *bub_cos;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i=32 ; i>=0 ; i--)
	{
		a = i/32.0 * M_TWOPI;
		*bub_sin++ = sin(a);
		*bub_cos++ = cos(a);
	}
}

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 


/* 
================== 
GL_ScreenShot_JPG
By Robert 'Heffo' Heffernan
================== 
*/
void GL_ScreenShot_JPG (qboolean silent)
{
	struct jpeg_compress_struct		cinfo;
	struct jpeg_error_mgr			jerr;
	byte							*rgbdata;
	JSAMPROW						s[1];
	FILE							*f;
	char							picname[80], checkname[MAX_OSPATH];
	int								i, offset, w3;

	// Find a file name to save it to 
	strcpy (picname, "qfusion000.jpg");

	for (i=0 ; i<=999 ; i++) 
	{ 
		picname[7] = i/100 + '0'; 
		picname[8] = (i%100)/10 + '0'; 
		picname[9] = ((i%100)%10) + '0'; 
		Com_sprintf (checkname, sizeof(checkname), "%s/screenshots/%s", FS_Gamedir(), picname);
		f = fopen (checkname, "rb");
		if (!f)
			break;	// file doesn't exist
		fclose (f);
	} 

	if ( (i == 1000) || !(f = fopen (checkname, "wb")) )
	{
		Com_Printf ( "GL_ScreenShot_JPG: Couldn't create a file\n" ); 
		return;
 	}

	// Allocate room for a copy of the framebuffer
	rgbdata = Q_malloc(vid.width * vid.height * 3);
	if (!rgbdata)
	{
		fclose (f);
		return;
	}

	// Read the framebuffer into our storage
	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, rgbdata);

	// Initialise the JPEG compression object
	cinfo.err = jpeg_std_error (&jerr);
	jpeg_create_compress (&cinfo);
	jpeg_stdio_dest (&cinfo, f);

	// Setup JPEG Parameters
	cinfo.image_width = vid.width;
	cinfo.image_height = vid.height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;

	jpeg_set_defaults (&cinfo);

	if ((r_screenshot_jpeg_quality->value > 100) || (r_screenshot_jpeg_quality->value <= 0))
		Cvar_Set ("r_screenshot_jpeg_quality", "85");

	jpeg_set_quality (&cinfo, r_screenshot_jpeg_quality->value, TRUE);

	// Start Compression
	jpeg_start_compress (&cinfo, true);

	// Feed scanline data
	w3 = cinfo.image_width * 3;
	offset = w3 * cinfo.image_height - w3;
	while (cinfo.next_scanline < cinfo.image_height)
	{
		s[0] = &rgbdata[offset - cinfo.next_scanline * w3];
		jpeg_write_scanlines (&cinfo, s, 1);
	}

	// Finish Compression
	jpeg_finish_compress (&cinfo);

	jpeg_destroy_compress (&cinfo);

	fclose ( f );

	Q_free ( rgbdata );

	if ( !silent ) {
		Com_Printf ( "Wrote %s\n", picname );
	}
}

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/* 
================== 
GL_ScreenShot_TGA
================== 
*/  
void GL_ScreenShot_TGA (qboolean silent)
{
	byte		*buffer;
	char		picname[80], checkname[MAX_OSPATH];
	int			i, c, temp;
	FILE		*f;

// 
// find a file name to save it to 
// 
	strcpy(picname, "qfusion000.tga");

	for (i=0 ; i<=999 ; i++) 
	{ 
		picname[7] = i/100 + '0'; 
		picname[8] = (i%100)/10 + '0'; 
		picname[9] = ((i%100)%10) + '0'; 
		Com_sprintf (checkname, sizeof(checkname), "%s/screenshots/%s", FS_Gamedir(), picname);
		f = fopen (checkname, "rb");
		if (!f)
			break;	// file doesn't exist
		fclose (f);
	}

	if ( (i == 1000) || !(f = fopen (checkname, "wb")) )
	{
		Com_Printf ( "GL_ScreenShot_TGA: Couldn't create a file\n" ); 
		return;
 	}

	buffer = Q_malloc (vid.width*vid.height*3 + 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = vid.width&255;
	buffer[13] = vid.width>>8;
	buffer[14] = vid.height&255;
	buffer[15] = vid.height>>8;
	buffer[16] = 24;	// pixel size

	c = 18+vid.width*vid.height*3;

	if ( gl_config.bgra ) {
		qglReadPixels (0, 0, vid.width, vid.height, GL_BGR_EXT, GL_UNSIGNED_BYTE, buffer+18 ); 
	} else {
		qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 ); 

		// swap rgb to bgr
		for (i=18 ; i<c ; i+=3)
		{
			temp = buffer[i];
			buffer[i] = buffer[i+2];
			buffer[i+2] = temp;
		}
	}

	fwrite (buffer, 1, c, f);
	fclose (f);

	Q_free (buffer);

	if ( !silent ) {
		Com_Printf ( "Wrote %s\n", picname );
	}
} 

/* 
================== 
R_ScreenShot
================== 
*/  
void R_ScreenShot (qboolean silent)
{
	char checkname[MAX_OSPATH];

	// create the screenshots directory if it doesn't exist
	Com_sprintf (checkname, sizeof(checkname), "%s/screenshots", FS_Gamedir());
	Sys_Mkdir (checkname);

	if ( r_screenshot_jpeg->value ) {
		GL_ScreenShot_JPG ( silent );
	} else {
		GL_ScreenShot_TGA ( silent );
	}
}

/*
================== 
R_ScreenShot_f
================== 
*/  
void R_ScreenShot_f (void)
{
	R_ScreenShot ( Cmd_Argc () >= 2 && !Q_stricmp (Cmd_Argv(1), "silent") );
}

/*
** GL_Strings_f
*/
void GL_Strings_f( void )
{
	Com_Printf ( "GL_VENDOR: %s\n", gl_config.vendor_string );
	Com_Printf ( "GL_RENDERER: %s\n", gl_config.renderer_string );
	Com_Printf ( "GL_VERSION: %s\n", gl_config.version_string );
	Com_Printf ( "GL_EXTENSIONS: %s\n", gl_config.extensions_string );
}

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void )
{
	qglClearColor (1,0, 0.5, 0.5);
	qglEnable (GL_TEXTURE_2D);

	qglDisable (GL_DEPTH_TEST);
	qglDisable ( GL_CULL_FACE );
	qglDisable ( GL_STENCIL_TEST );
	qglDisable ( GL_BLEND );

	qglColor4f (1,1,1,1);

	qglShadeModel (GL_SMOOTH);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglPolygonOffset (-1, -2);

	GL_TextureMode( r_texturemode->string );

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_SelectTexture( GL_TEXTURE_0 );
	GL_TexEnv( GL_MODULATE );

	// make sure gl_swapinterval is checked after vid_restart
	gl_swapinterval->modified = true;

	GL_UpdateSwapInterval();
}

void GL_UpdateSwapInterval( void )
{
	if ( gl_swapinterval->modified )
	{
		gl_swapinterval->modified = false;

		if ( !gl_state.stereo_enabled ) 
		{
#ifdef _WIN32
			if ( qwglSwapIntervalEXT )
				qwglSwapIntervalEXT( gl_swapinterval->value );
#endif
		}
	}
}
