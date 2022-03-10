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

#include "r_local.h"
#include "jpeglib.h"

#define	MAX_GLIMAGES		4096
#define IMAGES_HASH_SIZE	64

static image_t		images[MAX_GLIMAGES];
image_t				*r_lightmapTextures[MAX_GLIMAGES];
static image_t		*images_hash[IMAGES_HASH_SIZE];
static int			r_numImages;

static int			resampleWidth;
static unsigned		*resampleBuffer;
static mempool_t	*resamplePool;

char	*r_cubemapSuff[6] = { "px", "nx", "py", "ny", "pz", "nz" };
vec3_t	r_cubemapAngles[6] = {
	{   0, 180,  90 },	// px
	{   0,   0, 270 },	// nx
	{   0,  90, 180 },	// py
	{   0, 270,   0 },	// ny
	{ -90, 270,   0 },	// pz
	{  90,  90,   0 }	// nz
};

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

void GL_SelectTexture( int tmu )
{
	if( !glConfig.multiTexture )
		return;
	if( tmu == glState.currentTMU )
		return;

	glState.currentTMU = tmu;

	if ( qglSelectTextureSGIS ) {
		qglSelectTextureSGIS( tmu + GL_TEXTURE0_SGIS );
	} else if ( qglActiveTextureARB ) {
		qglActiveTextureARB( tmu + GL_TEXTURE0_ARB );
		qglClientActiveTextureARB( tmu + GL_TEXTURE0_ARB );
	}
}

void GL_TexEnv( GLenum mode )
{
	if ( mode != ( int )glState.currentEnvModes[glState.currentTMU] ) {
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
		glState.currentEnvModes[glState.currentTMU] = ( int )mode;
	}
}

void GL_Bind( int tmu, image_t *tex )
{
	GL_SelectTexture( tmu );

	if( r_nobind->integer )		// performance evaluation option
		tex = r_notexture;
	if( glState.currentTextures[tmu] == tex->texnum )
		return;

	glState.currentTextures[tmu] = tex->texnum;
	if( tex->flags & IT_CUBEMAP )
		qglBindTexture( GL_TEXTURE_CUBE_MAP_ARB, tex->texnum );
	else
		qglBindTexture( GL_TEXTURE_2D, tex->texnum );
}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

#define NUM_GL_MODES (sizeof(modes) / sizeof (glmode_t))

/*
===============
R_TextureMode
===============
*/
void R_TextureMode( char *string )
{
	int		i;
	image_t	*glt;

	for( i = 0; i < NUM_GL_MODES; i++ ) {
		if( !Q_stricmp( modes[i].name, string ) )
			break;
	}

	if( i == NUM_GL_MODES ) {
		Com_Printf( "R_TextureMode: bad filter name\n" );
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for( i = 1, glt = images; i < r_numImages; i++, glt++ ) {
		GL_Bind( 0, glt );

		if( !(glt->flags & IT_NOMIPMAP) ) {
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		} else {
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
	}
}

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f( void )
{
	int			i;
	image_t		*image;
	unsigned	texels = 0;

	Com_Printf( "------------------\n" );

	for( i = 1, image = images; i < r_numImages; i++, image++ ) {
		texels += image->upload_width * image->upload_height;
		Com_Printf( " %3i %3i: %s%s\n", image->upload_width, image->upload_height, image->name, image->extension );
	}

	Com_Printf( "Total texels count (not counting mipmaps): %i texels\n", texels );
	Com_Printf( "%i images total\n", r_numImages - 1 );
}

/*
=================================================================

PCX LOADING

=================================================================
*/

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;

/*
==============
LoadPCX
==============
*/
int LoadPCX (char *filename, qbyte **pic, int *width, int *height)
{
	qbyte	*raw;
	pcx_t	*pcx;
	int		x, y, samples = 3;
	int		len, columns, rows;
	int		dataByte, runLength;
	qbyte	pal[768], *pix;

	*pic = NULL;

	//
	// load the file
	//
	len = FS_LoadFile (filename, (void **)&raw);
	if (!raw)
	{
//		Com_DPrintf ("Bad pcx file %s\n", filename);
		return 0;
	}

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;

    pcx->xmin = LittleShort(pcx->xmin);
    pcx->ymin = LittleShort(pcx->ymin);
    pcx->xmax = LittleShort(pcx->xmax);
    pcx->ymax = LittleShort(pcx->ymax);
    pcx->hres = LittleShort(pcx->hres);
    pcx->vres = LittleShort(pcx->vres);
    pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
    pcx->palette_type = LittleShort(pcx->palette_type);

	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		Com_DPrintf (S_COLOR_YELLOW "Bad pcx file %s\n", filename);
		FS_FreeFile (pcx);
		return 0;
	}

	columns = pcx->xmax + 1;
	rows = pcx->ymax + 1;
	pix = *pic = Mem_TempMallocExt ( columns * rows * 4, 0 );
	memcpy (pal, (qbyte *)pcx + len - 768, 768);

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	for (y=0 ; y<rows ; y++)
	{
		for (x=0 ; x<columns ; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while( runLength-- > 0 ) {
				if( dataByte == 255 ) {		// hack Quake2 palette
					pix[0] = 0;
					pix[1] = 0;
					pix[2] = 0;
					pix[3] = 0;
					samples = 4;
				} else {
					pix[0] = pal[dataByte*3+0];
					pix[1] = pal[dataByte*3+1];
					pix[2] = pal[dataByte*3+2];
					pix[3] = 255;
				}
				x++; pix += 4;
			}
		}
	}

	if( raw - (qbyte *)pcx > len) {
		Com_DPrintf( S_COLOR_YELLOW "PCX file %s was malformed", filename );
		Mem_TempFree( *pic );
		*pic = NULL;
	}

	FS_FreeFile( pcx );

	return samples;
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
=============
LoadTGA
=============
*/
int LoadTGA (char *name, qbyte **pic, int *width, int *height)
{
	int		i, j, columns, rows, row_inc;
	qbyte	*pixbuf;
	int		row, column;
	qbyte	*buf_p;
	qbyte	*buffer;
	int		length, samples = 3;
	TargaHeader	targa_header;
	qbyte	*targa_rgba;
	qbyte	palette[256][4];
	qbyte	tmp[2];
	qboolean compressed;

	*pic = NULL;

	//
	// load the file
	//
	length = FS_LoadFile (name, (void **)&buffer);
	if (!buffer)
	{
//		Com_DPrintf ("Bad tga file %s\n", name);
		return 0;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_index = LittleShort ( *((short *)tmp) );
	buf_p+=2;
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_length = LittleShort ( *((short *)tmp) );
	buf_p+=2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.y_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.width = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.height = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  // skip TARGA image comment

	if( targa_header.image_type == 1 || targa_header.image_type == 9 ) {
		// uncompressed colormapped image
		if( targa_header.pixel_size != 8 ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit images supported for type 1 and 9" );
			FS_FreeFile( buffer );
			return 0;
		}
		if( targa_header.colormap_length != 256 ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit colormaps are supported for type 1 and 9" );
			FS_FreeFile( buffer );
			return 0;
		}
		if( targa_header.colormap_index ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: colormap_index is not supported for type 1 and 9" );
			FS_FreeFile( buffer );
			return 0;
		}
		if( targa_header.colormap_size == 24 ) {
			for( i = 0; i < targa_header.colormap_length; i++ ) {
				palette[i][0] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][2] = *buf_p++;
				palette[i][3] = 255;
			}
		} else if( targa_header.colormap_size == 32 ) {
			for( i = 0; i < targa_header.colormap_length; i++ ) {
				palette[i][0] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][2] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
		} else {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: only 24 and 32 bit colormaps are supported for type 1 and 9" );
			FS_FreeFile( buffer );
			return 0;
		}
	} else if( targa_header.image_type == 2 || targa_header.image_type == 10 ) {
		// uncompressed or RLE compressed RGB
		if( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 32 or 24 bit images supported for type 2 and 10" );
			FS_FreeFile( buffer );
			return 0;
		}
	} else if( targa_header.image_type == 3 || targa_header.image_type == 11 ) {
		// uncompressed greyscale
		if( targa_header.pixel_size != 8 ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit images supported for type 3 and 11" );
			FS_FreeFile( buffer );
			return 0;
		}
	}

	columns = targa_header.width;
	rows = targa_header.height;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = Mem_TempMallocExt( columns * rows * 4, 0 );
	*pic = targa_rgba;

    // If bit 5 of attributes isn't set, the image has been stored from bottom to top
    if (targa_header.attributes & 0x20)
    {
        pixbuf = targa_rgba;
        row_inc = 0;
	}
    else
    {
        pixbuf = targa_rgba + (rows - 1)*columns*4;
        row_inc = -columns*4*2;
    }

	compressed = ( targa_header.image_type == 9 || targa_header.image_type == 10 || targa_header.image_type == 11 );

	if( !compressed ) {
		unsigned char red, green, blue, alpha;

		for( row = 0; row < rows; row++ ) {
			for( column = 0; column < columns; column++ ) {
				switch( targa_header.image_type ) {
					case 1:
						// colormapped image
						blue = *buf_p++;
						red = palette[blue][0];
						green = palette[blue][1];
						alpha = palette[blue][3];
						blue = palette[blue][2];
						break;
					case 2:
						// 24 or 32 bit image
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						if( targa_header.pixel_size == 32 )
							alpha = *buf_p++;
						else
							alpha = 255;
						break;
					case 3:
						// greyscale image
						blue = green = red = *buf_p++;
						alpha = 255;
						break;
				}
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alpha;
				if( alpha != 255 )
					samples = 4;
			}

			pixbuf += row_inc;
		}
	} else {
		unsigned char red, green, blue, alpha, packetHeader, packetSize;

		for( row = 0; row < rows; row++ ) {
			for( column = 0; column < columns; ) {
				packetHeader = *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);

				if( packetHeader & 0x80 ) {        // run-length packet
					switch( targa_header.image_type ) {
						case 9:
							// colormapped image
							blue = *buf_p++;
							red = palette[blue][0];
							green = palette[blue][1];
							alpha = palette[blue][3];
							blue = palette[blue][2];
							break;
						case 10:
							// 24 or 32 bit image
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							if( targa_header.pixel_size == 32 )
								alpha = *buf_p++;
							else
								alpha = 255;
							break;
						case 11:
							// greyscale image
							blue = green = red = *buf_p++;
							alpha = 255;
							break;
					}
					if( packetSize && (alpha != 255) )
						samples = 4;
					for( j = 0; j < packetSize; j++ ) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alpha;
						column++;
						if( column == columns ) { // run spans across rows
							column = 0;
							if( row < rows-1 )
								row++;
							else
								goto breakOut;
							pixbuf += row_inc;
						}
					}
				} else {
					for( j = 0; j <packetSize; j++ ) {
						switch( targa_header.image_type ) {
							case 9:
								// colormapped image
								blue = *buf_p++;
								red = palette[blue][0];
								green = palette[blue][1];
								alpha = palette[blue][3];
								blue = palette[blue][2];
								break;
							case 10:
								// 24 or 32 bit image
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								if( targa_header.pixel_size == 32 )
									alpha = *buf_p++;
								else
									alpha = 255;
								break;
							case 11:
								// greyscale image
								blue = green = red = *buf_p++;
								alpha = 255;
								break;
						}
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alpha;
						if( alpha != 255 )
							samples = 4;
						column++;
						if( column == columns ) { // pixel packet run spans across rows
							column = 0;
							if( row < rows-1 )
								row++;
							else
								goto breakOut;
							pixbuf += row_inc;
						}	
					}
				}
			}

			pixbuf += row_inc;
			breakOut:;
		}
	}

	FS_FreeFile( buffer );

	return samples;
}

/* 
================== 
WriteTGA
================== 
*/
qboolean WriteTGA( char *name, qbyte *buffer, int width, int height )
{
	FILE		*f;
	int			i, c, temp;

	if( !(f = fopen( name, "wb" ) ) ) {
		Com_Printf( "WriteTGA: Couldn't create a file\n" ); 
		return qfalse;
	}

	buffer[2] = 2;		// uncompressed type
	buffer[12] = width&255;
	buffer[13] = width>>8;
	buffer[14] = height&255;
	buffer[15] = height>>8;
	buffer[16] = 24;	// pixel size

	c = 18+width*height*3;
	// swap rgb to bgr
	for( i = 18; i < c; i += 3 ) {
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}

	fwrite( buffer, 1, c, f );
	fclose( f );

	return qtrue;
} 

/*
=========================================================

JPEG LOADING

=========================================================
*/

static void jpg_noop(j_decompress_ptr cinfo)
{
}

static boolean jpg_fill_input_buffer(j_decompress_ptr cinfo)
{
    Com_DPrintf( "Premeture end of jpeg file\n" );

    return 1;
}

static void jpg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void jpeg_mem_src(j_decompress_ptr cinfo, qbyte *mem, int len)
{
    cinfo->src = (struct jpeg_source_mgr *)
	(*cinfo->mem->alloc_small)((j_common_ptr) cinfo,
				   JPOOL_PERMANENT,
				   sizeof(struct jpeg_source_mgr));
    cinfo->src->init_source = jpg_noop;
    cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
    cinfo->src->skip_input_data = jpg_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart;
    cinfo->src->term_source = jpg_noop;
    cinfo->src->bytes_in_buffer = len;
    cinfo->src->next_input_byte = mem;
}

/*
=============
LoadJPG
=============
*/
int LoadJPG (char *name, qbyte **pic, int *width, int *height)
{
    int i, length, samples;
    qbyte *img, *scan, *buffer, *dummy;
    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;

	*pic = NULL;

	// load the file
	length = FS_LoadFile( name, (void **)&buffer );
	if( !buffer ) {
//		Com_DPrintf( "Bad jpeg file %s\n", name );
		return 0;
	}

	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_decompress( &cinfo );
	jpeg_mem_src( &cinfo, buffer, length );
	jpeg_read_header( &cinfo, TRUE );
	jpeg_start_decompress( &cinfo );
	samples = cinfo.output_components;

    if( samples != 3 && samples != 1 ) {
		Com_DPrintf( S_COLOR_YELLOW "Bad jpeg file %s\n", name );
		jpeg_destroy_decompress( &cinfo );
		FS_FreeFile( buffer );
		return 0;
	}

	if( width )
		*width = cinfo.output_width;
	if( height )
	    *height = cinfo.output_height;

	img = *pic = Mem_TempMallocExt( cinfo.output_width * cinfo.output_height * 4, 0 );
	dummy = Mem_TempMallocExt( cinfo.output_width * samples, 0 );

	if( samples == 1 ) {
		while( cinfo.output_scanline < cinfo.output_height ) {
			scan = dummy;
			if( !jpeg_read_scanlines( &cinfo, &scan, 1 ) ) {
				Com_Printf( S_COLOR_YELLOW "Bad jpeg file %s\n", name );
				jpeg_destroy_decompress( &cinfo );
			    Mem_TempFree( dummy );
				FS_FreeFile( buffer );
				return 0;
			}

			for( i = 0; i < cinfo.output_width; i++, img += 4 ) {
				img[0] = img[1] = img[2] = *scan++; img[3] = 255;
			}
		}
	} else {
		while( cinfo.output_scanline < cinfo.output_height ) {
			scan = dummy;
			if( !jpeg_read_scanlines( &cinfo, &scan, 1 ) ) {
				Com_Printf( S_COLOR_YELLOW "Bad jpeg file %s\n", name );
				jpeg_destroy_decompress( &cinfo );
			    Mem_TempFree( dummy );
				FS_FreeFile( buffer );
				return 0;
			}

			for( i = 0; i < cinfo.output_width; i++, img += 4, scan += 3 ) {
				img[0] = scan[0]; img[1] = scan[1]; img[2] = scan[2]; img[3] = 255;
			}
		}
	}

    jpeg_finish_decompress( &cinfo );
    jpeg_destroy_decompress( &cinfo );

    Mem_TempFree( dummy );
	FS_FreeFile( buffer );

	return 3;
}

/* 
================== 
WriteJPG
================== 
*/
qboolean WriteJPG( char *name, qbyte *buffer, int width, int height, int quality )
{
	struct jpeg_compress_struct		cinfo;
	struct jpeg_error_mgr			jerr;
	FILE							*f;
	JSAMPROW						s[1];
	int								offset, w3;

	if( !(f = fopen( name, "wb" )) ) {
		Com_Printf( "WriteJPG: Couldn't create a file\n" ); 
		return qfalse;
	}

	// Initialise the JPEG compression object
	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress( &cinfo );
	jpeg_stdio_dest( &cinfo, f );

	// Setup JPEG Parameters
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;

	jpeg_set_defaults( &cinfo );

	if( (quality > 100) || (quality <= 0) )
		quality = 85;

	jpeg_set_quality( &cinfo, quality, TRUE );

	// Start Compression
	jpeg_start_compress( &cinfo, qtrue );

	// Feed scanline data
	w3 = cinfo.image_width * 3;
	offset = w3 * cinfo.image_height - w3;
	while( cinfo.next_scanline < cinfo.image_height ) {
		s[0] = &buffer[offset - cinfo.next_scanline * w3];
		jpeg_write_scanlines( &cinfo, s, 1 );
	}

	// Finish Compression
	jpeg_finish_compress( &cinfo );
	jpeg_destroy_compress( &cinfo );

	fclose ( f );

	return qtrue;
}

//=======================================================

/*
================
R_ResampleTexture
================
*/
void R_ResampleTexture( unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight )
{
	int		i, j;
	unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	*p1, *p2;
	qbyte		*pix1, *pix2, *pix3, *pix4;

	if( inwidth == outwidth && inheight == outheight ) {
		memcpy( out, in, inwidth * inheight * 4 );
		return;
	}

	if( outwidth > resampleWidth ) {
		if( resampleBuffer )
			Mem_Free( resampleBuffer );
		resampleWidth = outwidth;
		resampleBuffer = Mem_Alloc( resamplePool, resampleWidth * sizeof( unsigned ) * 2 );
	}

	p1 = resampleBuffer;
	p2 = resampleBuffer + outwidth;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for( i = 0; i < outwidth; i++ ) {
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	frac = 3 * (fracstep >> 2);
	for( i = 0; i < outwidth ; i++ ) {
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for( i = 0; i < outheight; i++, out += outwidth ) {
		inrow = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);

		for( j = 0; j < outwidth; j++ ) {
			pix1 = (qbyte *)inrow + p1[j];
			pix2 = (qbyte *)inrow + p2[j];
			pix3 = (qbyte *)inrow2 + p1[j];
			pix4 = (qbyte *)inrow2 + p2[j];
			(( qbyte * )( out + j ))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			(( qbyte * )( out + j ))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			(( qbyte * )( out + j ))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			(( qbyte * )( out + j ))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
void R_MipMap( qbyte *in, int width, int height )
{
	int		i, j;
	qbyte	*out;

	width <<= 2;
	height >>= 1;

	out = in;
	for( i = 0; i < height; i++, in += width ) {
		for( j = 0; j < width; j += 8, out += 4, in += 8 ) {
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
===============
R_Upload32
===============
*/
void R_Upload32( qbyte **data, int width, int height, int flags, int *upload_width, int *upload_height, int samples )
{
	int			i, comp;
	int			target, target2;
	int			numTextures;
	unsigned	*scaled;
	int			scaledWidth, scaledHeight;

	for( scaledWidth = 1; scaledWidth < width; scaledWidth <<= 1 );
	for( scaledHeight = 1; scaledHeight < height; scaledHeight <<= 1 );

	if( flags & IT_SKY ) {
		// let people sample down the sky textures for speed
		scaledWidth >>= r_skymip->integer;
		scaledHeight >>= r_skymip->integer;
	} else if( !( flags & IT_NOPICMIP ) ){
		// let people sample down the world textures for speed
		scaledWidth >>= r_picmip->integer;
		scaledHeight >>= r_picmip->integer;
	}

	// don't ever bother with > maxSize textures
	if( flags & IT_CUBEMAP ) {
		numTextures = 6;
		target = GL_TEXTURE_CUBE_MAP_ARB;
		target2 = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
		clamp( scaledWidth, 1, glConfig.maxTextureCubemapSize );
		clamp( scaledHeight, 1, glConfig.maxTextureCubemapSize );
	} else {
		numTextures = 1;
		target = GL_TEXTURE_2D;
		target2 = GL_TEXTURE_2D;
		clamp( scaledWidth, 1, glConfig.maxTextureSize );
		clamp( scaledHeight, 1, glConfig.maxTextureSize );
	}

	if( upload_width )
		*upload_width = scaledWidth;
	if( upload_height )
		*upload_height = scaledHeight;

	if( glConfig.compressedTextures )  {
		if( samples == 3 )
			comp = GL_COMPRESSED_RGB_ARB;
		else if( samples == 4 )
			comp = GL_COMPRESSED_RGBA_ARB;
	} else {
		int bits = r_texturebits->integer;

		if( samples == 3 ) {
			if( bits == 16 )
				comp = GL_RGB5;
			else if( bits == 32 )
				comp = GL_RGB8;
			else
				comp = GL_RGB;
		} else if( samples == 4 ) {
			if( bits == 16 )
				comp = GL_RGBA4;
			else if( bits == 32 )
				comp = GL_RGBA8;
			else
				comp = GL_RGBA;
		}
	}

	if( !( flags & IT_NOMIPMAP ) ) {
		qglTexParameterf( target, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameterf( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );
	} else {
		qglTexParameterf( target, GL_TEXTURE_MIN_FILTER, gl_filter_max );
		qglTexParameterf( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );
	}

	if( glConfig.textureFilterAnisotropic )
		qglTexParameterf( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, min( gl_ext_texture_filter_anisotropic->value, glConfig.maxTextureFilterAnisotropic ) );

	// clamp if required
	if( !( flags & IT_CLAMP ) ) {
		qglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_REPEAT );
		qglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_REPEAT );
	} else if( glConfig.textureEdgeClamp ) {
		qglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	} else {
		qglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP );
	}

	if( (scaledWidth == width) && (scaledHeight == height) && (flags & IT_NOMIPMAP) ) {
		for( i = 0; i < numTextures; i++, target2++ )
			qglTexImage2D( target2, 0, comp, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data[i] );
	} else {
		scaled = ( unsigned * )Mem_TempMallocExt( scaledWidth * scaledHeight * 4, 0 );

		for( i = 0; i < numTextures; i++, target2++ ) {
			// resample the texture
			R_ResampleTexture( (unsigned *)(data[i]), width, height, scaled, scaledWidth, scaledHeight );

			qglTexImage2D( target2, 0, comp, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled );

			// mipmaps generation
			if( !( flags & IT_NOMIPMAP ) ) {
				int		w, h;
				int		miplevel = 0;

				w = scaledWidth;
				h = scaledHeight;
				while( w > 1 || h > 1 ) {
					R_MipMap( (qbyte *)scaled, w, h );

					w >>= 1;
					h >>= 1;
					if( w < 1 )
						w = 1;
					if( h < 1 )
						h = 1;
					miplevel++;
					qglTexImage2D( target2, miplevel, comp, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled );
				}
			}
		}

		Mem_TempFree ( scaled );
	}
}

/*
================
R_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *R_LoadPic( char *name, qbyte **pic, int width, int height, int flags, int samples )
{
	image_t		*image;

	if( r_numImages == MAX_GLIMAGES )
		Com_Error( ERR_DROP, "R_LoadPic: r_numImages == MAX_GLIMAGES" );

	image = images + r_numImages;
	Q_strncpyz( image->name, name, sizeof(image->name) );
	image->width = width;
	image->height = height;
	image->flags = flags;
	image->texnum = r_numImages++;

	GL_Bind( 0, image );
	R_Upload32( pic, width, height, flags, &image->upload_width, &image->upload_height, samples );

	return image;
}

/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t	*R_FindImage( char *name, int flags )
{
	int		i, lastDot;
	unsigned int len, key;
	image_t	*image;
	qbyte	*pic;
	int		width, height, samples;
	char	pathname[MAX_QPATH];

	if( !name || !name[0] )
		return NULL;	//	Com_Error (ERR_DROP, "R_FindImage: NULL name");

	lastDot = -1;
	for( i = ( name[0] == '/' || name[0] == '\\' ), len = 0; name[i] && (len < sizeof(pathname)-5); i++ ) {
		if( name[i] == '.' )
			lastDot = len;
		if( name[i] == '\\' ) 
			pathname[len++] = '/';
		else
			pathname[len++] = tolower( name[i] );
	}

	if( len < 5 )
		return NULL;
	else if( lastDot != -1 )
		len = lastDot;

	pathname[len] = 0;
	flags &= ~IT_CUBEMAP;

	// look for it
	key = Com_HashKey( pathname, IMAGES_HASH_SIZE );
	for( image = images_hash[key]; image; image = image->hash_next ) {
		if( (image->flags == flags) && !strcmp( image->name, pathname ) )
			return image;
	}
	pathname[len] = '.';
	pathname[len+4] = 0;

	//
	// load the pic from disk
	//
	pic = NULL;
	image = NULL;

	pathname[len+1] = 'j'; pathname[len+2] = 'p'; pathname[len+3] = 'g';
	samples = LoadJPG( pathname, &pic, &width, &height );

	if( pic )
		goto checkpic;

	pathname[len+1] = 't'; pathname[len+2] = 'g'; pathname[len+3] = 'a';
	samples = LoadTGA( pathname, &pic, &width, &height );

	if( pic )
		goto checkpic;

	pathname[len+1] = 'p'; pathname[len+2] = 'c'; pathname[len+3] = 'x';
	samples = LoadPCX( pathname, &pic, &width, &height );

	if( pic ) {
checkpic:
		pathname[len] = 0;
		image = R_LoadPic( pathname, &pic, width, height, flags, samples );
		image->extension[0] = '.';
		strcpy( &image->extension[1], &pathname[len+1] );

		// add to hash table
		image->hash_next = images_hash[key];
		images_hash[key] = image;

		Mem_TempFree( pic );
	}

	return image;
}

/*
===============
R_FindCubemapImage

Finds or loads the given image
===============
*/
image_t	*R_FindCubemapImage( char *name, int flags )
{
	int		i, lastDot;
	unsigned int len, key;
	image_t	*image;
	qbyte	*pic[6];
	int		width, height, lastSize, samples;
	char	pathname[MAX_QPATH];

	if( !name || !name[0] )
		return NULL;	//	Com_Error (ERR_DROP, "R_FindCubemapImage: NULL name");

	lastDot = -1;
	for( i = ( name[0] == '/' || name[0] == '\\' ), len = 0; name[i] && (len < sizeof(pathname)-8); i++ ) {
		if( name[i] == '.' )
			lastDot = len;
		if( name[i] == '\\' ) 
			pathname[len++] = '/';
		else
			pathname[len++] = tolower( name[i] );
	}

	if( len < 5 )
		return NULL;
	else if( lastDot != -1 )
		len = lastDot;

	pathname[len] = 0;
	flags |= IT_CUBEMAP;

	// look for it
	key = Com_HashKey( pathname, IMAGES_HASH_SIZE );
	for( image = images_hash[key]; image; image = image->hash_next ) {
		if( (image->flags == flags) && !strcmp( image->name, pathname ) )
			return image;
	}
	pathname[len] = '_';
	pathname[len+3] = '.';
	pathname[len+7] = 0;

	//
	// load the pic from disk
	//
	image = NULL;
	memset( pic, 0, sizeof(pic) );

	for( i = 0; i < 6; i++ ) {
		pathname[len+1] = r_cubemapSuff[i][0];
		pathname[len+2] = r_cubemapSuff[i][1];

		pathname[len+4] = 'j'; pathname[len+5] = 'p'; pathname[len+6] = 'g';
		samples = LoadJPG( pathname, &(pic[i]), &width, &height );
		if( pic[i] )
			goto checkpic;

		pathname[len+4] = 't'; pathname[len+5] = 'g'; pathname[len+6] = 'a';
		samples = LoadTGA( pathname, &(pic[i]), &width, &height );
		if( pic[i] )
			goto checkpic;

		pathname[len+4] = 'p'; pathname[len+5] = 'c'; pathname[len+6] = 'x';
		samples = LoadPCX( pathname, &(pic[i]), &width, &height );
		if( pic[i] ) {
checkpic:
			if( width != height ) {
				Com_Printf( "Not square cubemap image %s\n", pathname );
				break;
			}
			if( !i ) {
				lastSize = width;
			} else if( lastSize != width ) {
				Com_Printf( "Different cubemap image size: %s\n", pathname );
				break;
			}
			continue;
		}
		break;
	}

	if( i == 6 ) {
		pathname[len] = 0;
		image = R_LoadPic( pathname, pic, width, height, flags, samples );
		image->extension[0] = '.';
		strcpy( &image->extension[1], &pathname[len+4] );

		// add to hash table
		image->hash_next = images_hash[key];
		images_hash[key] = image;
	}

	for( i = 0; i < 6; i++ ) {
		if( pic[i] )
			Mem_TempFree( pic[i] );
	}

	return image;
}

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

/* 
================== 
R_ScreenShot
================== 
*/  
void R_ScreenShot( char *name, qboolean silent )
{
	int		i;
	FILE	*f;
	qbyte	*buffer;
	char	picname[80], checkname[MAX_OSPATH];

	// create the screenshots directory if it doesn't exist
	Q_snprintfz( checkname, sizeof(checkname), "%s/screenshots", FS_Gamedir() );
	Sys_Mkdir( checkname );

	if( name ) {
		Q_snprintfz( checkname, sizeof(checkname), "%s/screenshots/%s", FS_Gamedir(), name );

		if( r_screenshot_jpeg->integer )
			COM_DefaultExtension (checkname, ".jpg");
		else
			COM_DefaultExtension (checkname, ".tga");
		goto shot;
	}

	if( r_screenshot_jpeg->integer )
		strcpy( picname, "qfusion000.jpg" );
	else
		strcpy( picname, "qfusion000.tga" );

	// 
	// find a file name to save it to 
	// 
	for( i = 0; i < 1000; i++ ) { 
		picname[7] = i/100 + '0'; 
		picname[8] = (i%100)/10 + '0'; 
		picname[9] = ((i%100)%10) + '0'; 
		Q_snprintfz( checkname, sizeof(checkname), "%s/screenshots/%s", FS_Gamedir(), picname );
		f = fopen( checkname, "rb" );
		if (!f)
			break;	// file doesn't exist
		fclose( f );
	}

	if( i == 1000 ) {
		Com_Printf( "R_ScreenShot: Couldn't create a file\n" ); 
		return;
 	}

shot:
	if ( r_screenshot_jpeg->integer ) {
		buffer = Mem_TempMalloc( glState.width * glState.height * 3 );
		qglReadPixels( 0, 0, glState.width, glState.height, GL_RGB, GL_UNSIGNED_BYTE, buffer ); 

		if( WriteJPG( checkname, buffer, glState.width, glState.height, r_screenshot_jpeg_quality->integer ) && !silent )
			Com_Printf ( "Wrote %s\n", picname );
	} else {
		buffer = Mem_TempMalloc( 18 + glState.width * glState.height * 3 );
		qglReadPixels( 0, 0, glState.width, glState.height, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18 ); 

		if( WriteTGA( checkname, buffer, glState.width, glState.height ) && !silent )
			Com_Printf ( "Wrote %s\n", picname );
	}

	Mem_TempFree( buffer );
}

/*
================== 
R_ScreenShot_f
================== 
*/  
void R_ScreenShot_f( void ) {
	R_ScreenShot( NULL, Cmd_Argc () >= 2 && !Q_stricmp (Cmd_Argv(1), "silent") );
}

/* 
================== 
R_EnvShot_f
================== 
*/  
void R_EnvShot_f( void )
{
	int		i;
	int		size, maxSize;
	qbyte	*buffer;
	char	checkname[MAX_OSPATH];

	if( !r_worldmodel )
		return;

	if( Cmd_Argc () != 3 ) {
		Com_Printf( "usage: envshot <name> <size>\n" );
		return;
	}

	maxSize = min( glState.width, glState.height );
	if( maxSize > atoi( Cmd_Argv( 2 ) ) )
		maxSize = atoi( Cmd_Argv( 2 ) );

	for( size = 1; size < maxSize; size <<= 1 );
	if( size > maxSize )
		size >>= 1;

	// do not render non-bmodel entities
	r_envview = qtrue;

	// create the screenshots directory if it doesn't exist
	Q_snprintfz( checkname, sizeof(checkname), "%s/cubemaps", FS_Gamedir() );
	Sys_Mkdir( checkname );

	buffer = Mem_TempMalloc( size * size * 3 + 18 );
	for( i = 0; i < 6; i++ ) {
		R_DrawCubemapView( r_lastRefdef.vieworg, r_cubemapAngles[i], size );
		qglReadPixels( 0, glState.height - size, size, size, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18 ); 

		Q_snprintfz( checkname, sizeof(checkname), "%s/cubemaps/%s_%s.tga", FS_Gamedir(), Cmd_Argv( 1 ), r_cubemapSuff[i] );
		WriteTGA( checkname, buffer, size, size );
	}

	r_envview = qfalse;

	Mem_TempFree( buffer );
}

//=======================================================

/*
==================
R_InitCinematicTexture
==================
*/
void R_InitCinematicTexture( void )
{
	// reserve a dummy texture slot
	r_cintexture = &images[r_numImages++];
	r_cintexture->texnum = 0;
}

/*
==================
R_InitNoTexture
==================
*/
void R_InitNoTexture( void )
{
	int x, y;
	qbyte *data;
	qbyte dottexture[8][8] =
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
	data = Mem_TempMalloc( 8 * 8 * 4 );

	for( x = 0; x < 8; x++ ) {
		for( y = 0; y < 8; y++ ) {
			data[(y*8 + x)*4+0] = dottexture[x&3][y&3]*127;
			data[(y*8 + x)*4+1] = dottexture[x&3][y&3]*127;
			data[(y*8 + x)*4+2] = dottexture[x&3][y&3]*127;
			data[(y*8 + x)*4+3] = 255;
		}
	}

	r_notexture = R_LoadPic( "***r_notexture***", &data, 8, 8, 0, 3 );

	Mem_TempFree ( data );
}

/*
==================
R_InitDynamicLightTexture
==================
*/
void R_InitDynamicLightTexture( void )
{
	int x, y;
	int dx2, dy, d;
	qbyte *data;

	//
	// dynamic light texture
	//
	data = Mem_TempMalloc( 64 * 64 * 4 );

	for( x = 0; x < 64; x++ ) {
		dx2 = x - 32;
		dx2 = dx2 * dx2 + 8;

		for( y = 0; y < 64; y++) {
			dy = y - 32;
			d = (int)(65536.0f * ((1.0f / (dx2 + dy * dy + 32.0f)) - 0.0005) + 0.5f);
			if ( d < 50 ) d = 0; else if ( d > 255 ) d = 255;

			data[(y*64 + x) * 4 + 0] = d;
			data[(y*64 + x) * 4 + 1] = d;
			data[(y*64 + x) * 4 + 2] = d;
			data[(y*64 + x) * 4 + 3] = 255;
		}
	}

	r_dlighttexture = R_LoadPic( "***r_dlighttexture***", &data, 64, 64, IT_NOPICMIP|IT_NOMIPMAP|IT_CLAMP, 3 );

	Mem_TempFree( data );
}

/*
==================
R_InitParticleTexture
==================
*/
void R_InitParticleTexture( void )
{
	int x, y;
	int dx2, dy, d;
	qbyte *data;

	//
	// particle texture
	//
	data = Mem_TempMalloc( 16 * 16 * 4 );

	for( x = 0; x < 16; x++ ) {
		dx2 = x - 8;
		dx2 = dx2 * dx2;

		for( y = 0; y < 16; y++ ) {
			dy = y - 8;
			d = 255 - 35 * sqrt( dx2 + dy * dy );

			data[(y*16 + x) * 4 + 0] = 255;
			data[(y*16 + x) * 4 + 1] = 255;
			data[(y*16 + x) * 4 + 2] = 255;
			data[(y*16 + x) * 4 + 3] = bound( 0, d, 255 );
		}
	}

	r_particletexture = R_LoadPic( "***r_particletexture***", &data, 16, 16, IT_NOMIPMAP, 4 );

	Mem_TempFree( data );
}

/*
==================
R_InitWhiteTexture
==================
*/
void R_InitWhiteTexture( void )
{
	qbyte *data;

	//
	// white texture
	//
	data = Mem_TempMalloc( 32 * 32 * 4 );
	memset( data, 255, 32 * 32 * 4 );

	r_whitetexture = R_LoadPic( "***r_whitetexture***", &data, 32, 32, 0, 3 );
	
	Mem_TempFree ( data );
}

/*
==================
R_InitFogTexture
==================
*/
void R_InitFogTexture( void )
{
	qbyte *data;
	int x, y;
	double tw = 1.0f / ((float)FOG_TEXTURE_WIDTH - 1.0f);
	double th = 1.0f / ((float)FOG_TEXTURE_HEIGHT - 1.0f);
	double tx, ty, t;

	//
	// fog texture
	//
	data = Mem_TempMalloc( FOG_TEXTURE_WIDTH*FOG_TEXTURE_HEIGHT*4 );
	memset( data, 255, FOG_TEXTURE_WIDTH*FOG_TEXTURE_HEIGHT*4 );

	for( y = 0, ty = 0.0f; y < FOG_TEXTURE_HEIGHT; y++, ty += th ) {
		for ( x = 0, tx = 0.0f; x < FOG_TEXTURE_WIDTH; x++, tx += tw ) {
			t = sqrt( tx ) * 255.0;
			data[(x+y*FOG_TEXTURE_WIDTH)*4+3] = (qbyte)(min( t, 255.0 ));
		}
		data[y*4+3] = 0;
	}

	r_fogtexture = R_LoadPic( "***r_fogtexture***", &data, FOG_TEXTURE_WIDTH, FOG_TEXTURE_HEIGHT, IT_NOMIPMAP|IT_CLAMP, 4 );
	
	Mem_TempFree( data );
}

/*
===============
R_InitImages
===============
*/
void R_InitImages( void )
{
	resamplePool = Mem_AllocPool( NULL, "Resample buffer" );

	R_InitCinematicTexture ();
	R_InitNoTexture ();
	R_InitDynamicLightTexture ();
	R_InitParticleTexture ();
	R_InitWhiteTexture ();
	R_InitFogTexture ();
}

/*
===============
R_ShutdownImages
===============
*/
void R_ShutdownImages (void)
{
	int	i;

	Mem_FreePool( &resamplePool );

	resampleWidth = 0;
	resampleBuffer = NULL;

	for( i = 0; i < r_numImages; i++ )
		qglDeleteTextures( 1, &images[i].texnum );

	r_numImages = 0;
	memset( images, 0, sizeof(images) );
	memset( r_lightmapTextures, 0, sizeof(r_lightmapTextures) );
	memset( images_hash, 0, sizeof(images_hash) );
}
