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

#include "gl_local.h"
#include "jpeglib.h"

/*
==================
R_InitParticleTexture
==================
*/
byte	dottexture[8][8] =
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

void R_InitParticleTexture (void)
{
	int		x,y;
	byte	data[8][8][4];
	byte	whitedata[32][32][4];
	byte	fogdata[256*256];
	float	tw = 1.0f / (256.0f - 1.0f);
	float	th = 1.0f / (256.0f - 1.0f);
	float	tx = 0.0f, ty = 0.0f, t;

	//
	// particle texture
	//
	byte	data1[16][16][4];
	int		dx2, dy, d;

	for (x = 0; x < 16; x++) {
		dx2 = x - 8;
		dx2 *= dx2;
		for (y = 0; y < 16; y++) {
			dy = y - 8;
			d = 255 - 4 * (dx2 + (dy * dy));
			if (d <= 0) {
				d = 0;
				data1[y][x][0] = 0;
				data1[y][x][1] = 0;
				data1[y][x][2] = 0;
			} else {
				data1[y][x][0] = 255;
				data1[y][x][1] = 255;
				data1[y][x][2] = 255;
			}

			data1[y][x][3] = (byte) d;
		}
	}

	r_particletexture = GL_LoadPic ("***particle***", (byte *)data1, 16, 16, 0, 32);

	//
	// also use this for bad textures, but without alpha
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = dottexture[x&3][y&3]*255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}

	r_notexture = GL_LoadPic ("***r_notexture***", (byte *)data, 8, 8, 0, 32);

	memset ( whitedata, 255, 32*32*4 );
	r_whitetexture = GL_LoadPic ("***r_whitetexture***", (byte *)whitedata, 32, 32, 0, 32);

	r_dlighttexture = GL_FindImage ("textures/sfx/ball", IT_NOPICMIP|IT_NOMIPMAP|IT_CLAMP);

	for ( y = 0; y < 256; y++ )
	{
		tx = 0.0f;

		for ( x = 0; x < 256; x++, tx += tw )
		{
			t = (float)sqrt( tx ) * 255.0f;
			fogdata[y*256+x] = (byte)(min( t, 255.0f ));
		}

		ty += th;
	}

	for ( y = 0; y < 256; y++ ) {
		fogdata[y] = 0;
	}

	r_fogtexture = GL_LoadPic ("***r_fogtexture***", fogdata, 256, 256, IT_FOG|IT_NOMIPMAP, 8);
}

float bubble_sintable[17], bubble_costable[17];

void R_InitBubble(void) 
{
	float a;
	int i;
	float *bub_sin, *bub_cos;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i=16 ; i>=0 ; i--)
	{
		a = i/16.0 * M_TWOPI;
		*bub_sin++ = sin(a);
		*bub_cos++ = cos(a);
	}
}

/*
=================
R_InitFastsin
=================
*/
float r_fastsin[256];

void R_InitFastsin (void)
{
	int i;

	for (i = 0; i < 256; i++)
		r_fastsin[i] = (float)sin(i * M_TWOPI);
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
void GL_ScreenShot_JPG (void)
{
	struct jpeg_compress_struct		cinfo;
	struct jpeg_error_mgr			jerr;
	byte							*rgbdata;
	JSAMPROW						s[1];
	FILE							*file;
	char							picname[80], checkname[MAX_OSPATH];
	int								i, offset;

	// Create the scrnshots directory if it doesn't exist
	Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot", FS_Gamedir());
	Sys_Mkdir (checkname);

	// Find a file name to save it to 
	strcpy (picname, "l33t00.jpg");

	for (i=0 ; i<=99 ; i++) 
	{ 
		picname[4] = i/10 + '0'; 
		picname[5] = i%10 + '0'; 
		Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot/%s", FS_Gamedir(), picname);
		file = fopen (checkname, "rb");
		if (!file)
			break;	// file doesn't exist
		fclose (file);
	} 

	if (i == 100) 
	{
		Com_Printf ( "SCR_JPGScreenShot_f: Couldn't create a file\n" ); 
		return;
 	}

	// Open the file for Binary Output
	file = fopen(checkname, "wb");
	if (!file)
	{
		Com_Printf ( "SCR_JPGScreenShot_f: Couldn't create a file\n" ); 
		return;
 	}

	// Allocate room for a copy of the framebuffer
	rgbdata = Q_malloc(vid.width * vid.height * 3);
	if (!rgbdata)
	{
		fclose(file);
		return;
	}

	// Read the framebuffer into our storage
	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, rgbdata);

	// Initialise the JPEG compression object
	cinfo.err = jpeg_std_error (&jerr);
	jpeg_create_compress (&cinfo);
	jpeg_stdio_dest (&cinfo, file);

	// Setup JPEG Parameters
	cinfo.image_width = vid.width;
	cinfo.image_height = vid.height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;

	jpeg_set_defaults(&cinfo);

	if ((gl_screenshot_jpeg_quality->value > 100) || (gl_screenshot_jpeg_quality->value <= 0))
		Cvar_Set ("gl_screenshot_jpeg_quality", "85");

	jpeg_set_quality (&cinfo, gl_screenshot_jpeg_quality->value, TRUE);

	// Start Compression
	jpeg_start_compress (&cinfo, true);

	// Feed Scanline data
	offset = (cinfo.image_width * cinfo.image_height * 3) - (cinfo.image_width * 3);
	while (cinfo.next_scanline < cinfo.image_height)
	{
		s[0] = &rgbdata[offset - (cinfo.next_scanline * (cinfo.image_width * 3))];
		jpeg_write_scanlines(&cinfo, s, 1);
	}

	// Finish Compression
	jpeg_finish_compress (&cinfo);

	jpeg_destroy_compress ( &cinfo );

	fclose ( file );
	Q_free ( rgbdata );

	Com_Printf ( "Wrote %s\n", picname );
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
GL_ScreenShot_f
================== 
*/  
void GL_ScreenShot_f (void) 
{
	byte		*buffer;
	char		picname[80]; 
	char		checkname[MAX_OSPATH];
	int			i, c, temp;
	FILE		*f;

	// Heffo - JPEG Screenshots
	if (gl_screenshot_jpeg->value)
	{
		GL_ScreenShot_JPG ();
		return;
	}

	// create the scrnshots directory if it doesn't exist
	Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot", FS_Gamedir());
	Sys_Mkdir (checkname);

// 
// find a file name to save it to 
// 
	strcpy(picname, "l33t00.tga");

	for (i=0 ; i<=99 ; i++) 
	{ 
		picname[4] = i/10 + '0'; 
		picname[5] = i%10 + '0'; 
		Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot/%s", FS_Gamedir(), picname);
		f = fopen (checkname, "rb");
		if (!f)
			break;	// file doesn't exist
		fclose (f);
	}

	if (i == 100) 
	{
		Com_Printf ("SCR_ScreenShot_f: Couldn't create a file\n"); 
		return;
 	}


	buffer = Q_malloc(vid.width*vid.height*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = vid.width&255;
	buffer[13] = vid.width>>8;
	buffer[14] = vid.height&255;
	buffer[15] = vid.height>>8;
	buffer[16] = 24;	// pixel size

	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 ); 

	// swap rgb to bgr
	c = 18+vid.width*vid.height*3;
	for (i=18 ; i<c ; i+=3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}

	f = fopen (checkname, "wb");
	fwrite (buffer, 1, c, f);
	fclose (f);

	Q_free (buffer);
	Com_Printf ( "Wrote %s\n", picname );
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
	qglClearColor (1,0, 0.5 , 0.5);
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	GLSTATE_DISABLE_BLEND

	qglColor4f (1,1,1,1);

	qglShadeModel (GL_SMOOTH);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglPolygonOffset (-1, -2);

	GL_TextureMode( gl_texturemode->string );
	GL_TextureAlphaMode( gl_texturealphamode->string );
	GL_TextureSolidMode( gl_texturesolidmode->string );

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_TexEnv( GL_REPLACE );

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