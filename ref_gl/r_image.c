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

static qbyte		*r_aviBuffer;

static int			resampleWidth;
static unsigned		*resampleBuffer;
static mempool_t	*resamplePool;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

void GL_SelectTexture( int tmu )
{
	if( !glConfig.multiTexture )
		return;
	if( tmu == glState.currentTMU )
		return;

	glState.currentTMU = tmu;

	if( qglSelectTextureSGIS ) {
		qglSelectTextureSGIS( tmu + GL_TEXTURE0_SGIS );
	} else if ( qglActiveTextureARB ) {
		qglActiveTextureARB( tmu + GL_TEXTURE0_ARB );
		qglClientActiveTextureARB( tmu + GL_TEXTURE0_ARB );
	}
}

void GL_TexEnv( GLenum mode )
{
	if( mode != ( int )glState.currentEnvModes[glState.currentTMU] ) {
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
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
	else if( tex->depth != 1 )
		qglBindTexture( GL_TEXTURE_3D, tex->texnum );
	else
		qglBindTexture( GL_TEXTURE_2D, tex->texnum );
}

void GL_LoadTexMatrix( mat4x4_t m )
{
	qglMatrixMode( GL_TEXTURE );
	qglLoadMatrixf( m );
	glState.texIdentityMatrix[glState.currentTMU] = qfalse;
}

void GL_LoadIdentityTexMatrix( void )
{
	if( !glState.texIdentityMatrix[glState.currentTMU] ) {
		qglMatrixMode( GL_TEXTURE );
		qglLoadIdentity ();
		glState.texIdentityMatrix[glState.currentTMU] = qtrue;
	}
}

void GL_EnableTexGen( int coord, int mode )
{
	int tmu = glState.currentTMU;
	int bit, gen;

	switch( coord ) {
		case GL_S:
			bit = 1;
			gen = GL_TEXTURE_GEN_S;
			break;
		case GL_T:
			bit = 2;
			gen = GL_TEXTURE_GEN_T;
			break;
		case GL_R:
			bit = 4;
			gen = GL_TEXTURE_GEN_R;
			break;
		default:
			return;
	}

	if( mode ) {
		if( !(glState.genSTEnabled[tmu] & bit) ) {
			qglEnable( gen );
			glState.genSTEnabled[tmu] |= bit;
		}
		qglTexGeni( coord, GL_TEXTURE_GEN_MODE, mode );
	} else {
		if( glState.genSTEnabled[tmu] & bit ) {
			qglDisable( gen );
			glState.genSTEnabled[tmu] &= ~bit;
		}
	}
}

void GL_SetTexCoordArrayMode( int mode )
{
	int tmu = glState.currentTMU;
	int bit, cmode = glState.texCoordArrayMode[tmu];

	if( mode == GL_TEXTURE_COORD_ARRAY )
		bit = 1;
	else if( mode == GL_TEXTURE_CUBE_MAP_ARB )
		bit = 2;
	else
		bit = 0;

	if( cmode != bit ) {
		if( cmode == 1 )
			qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		else if( cmode == 2 )
			qglDisable( GL_TEXTURE_CUBE_MAP_ARB );

		if( bit == 1 )
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		else if( bit == 2 )
			qglEnable( GL_TEXTURE_CUBE_MAP_ARB );

		glState.texCoordArrayMode[tmu] = bit;
	}
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
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		} else {
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
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

	for( i = 1, image = images + 1; i < r_numImages; i++, image++ ) {
		texels += image->upload_width * image->upload_height * image->upload_depth;
		Com_Printf( " %3i %3i %3i: %s%s\n", image->upload_width, image->upload_height, image->upload_depth, image->name, image->extension );
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
	qbyte	stack[0x4000];

	*pic = NULL;

	//
	// load the file
	//
	len = FS_LoadFile (filename, (void **)&raw, stack, sizeof(stack));
	if (!raw)
		return 0;

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
		if( ( qbyte *)pcx != stack )
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

	if( (qbyte *)pcx != stack )
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
	int		i, columns, rows, row_inc, row, col;
	qbyte	*buf_p, *buffer, *pixbuf, *targa_rgba;
	int		length, samples, readpixelcount, pixelcount;
	qbyte	palette[256][4], red, green, blue, alpha;
	qboolean compressed;
	TargaHeader	targa_header;
	qbyte	stack[0x4000];

	*pic = NULL;

	//
	// load the file
	//
	length = FS_LoadFile (name, (void **)&buffer, stack, sizeof(stack));
	if( !buffer )
		return 0;

	buf_p = buffer;
	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = buf_p[0] + buf_p[1] * 256;
	buf_p+=2;
	targa_header.colormap_length = buf_p[0] + buf_p[1] * 256;
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
	if( targa_header.id_length != 0 )
		buf_p += targa_header.id_length;  // skip TARGA image comment

	if( targa_header.image_type == 1 || targa_header.image_type == 9 ) {
		// uncompressed colormapped image
		if( targa_header.pixel_size != 8 ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit images supported for type 1 and 9" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
		if( targa_header.colormap_length != 256 ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit colormaps are supported for type 1 and 9" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
		if( targa_header.colormap_index ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: colormap_index is not supported for type 1 and 9" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
		if( targa_header.colormap_size == 24 ) {
			for( i = 0; i < targa_header.colormap_length; i++ ) {
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = 255;
			}
		} else if( targa_header.colormap_size == 32 ) {
			for( i = 0; i < targa_header.colormap_length; i++ ) {
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
		} else {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: only 24 and 32 bit colormaps are supported for type 1 and 9" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
	} else if( targa_header.image_type == 2 || targa_header.image_type == 10 ) {
		// uncompressed or RLE compressed RGB
		if( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 32 or 24 bit images supported for type 2 and 10" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
	} else if( targa_header.image_type == 3 || targa_header.image_type == 11 ) {
		// uncompressed greyscale
		if( targa_header.pixel_size != 8 ) {
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit images supported for type 3 and 11" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
	}

	columns = targa_header.width;
	if( width )
		*width = columns;

	rows = targa_header.height;
	if( height )
		*height = rows;

	targa_rgba = Mem_TempMallocExt( columns * rows * 4, 0 );
	*pic = targa_rgba;

	// if bit 5 of attributes isn't set, the image has been stored from bottom to top
	if( targa_header.attributes & 0x20 ) {
		pixbuf = targa_rgba;
		row_inc = 0;
	} else {
		pixbuf = targa_rgba + (rows - 1) * columns * 4;
		row_inc = -columns * 4 * 2;
	}

	compressed = ( targa_header.image_type == 9 || targa_header.image_type == 10 || targa_header.image_type == 11 );
	for( row = col = 0, samples = 3; row < rows; ) {
		pixelcount = 0x10000;
		readpixelcount = 0x10000;

		if( compressed ) {
			pixelcount = *buf_p++;
			if( pixelcount & 0x80 )	// run-length packet
				readpixelcount = 1;
			pixelcount = 1 + (pixelcount & 0x7f);
		}

		while( pixelcount-- && (row < rows) ) {
			if( readpixelcount-- > 0 ) {
				switch( targa_header.image_type ) {
				case 1:
				case 9:
					// colormapped image
					blue = *buf_p++;
					red = palette[blue][0];
					green = palette[blue][1];
					alpha = palette[blue][3];
					blue = palette[blue][2];
					if( alpha != 255 )
						samples = 4;
					break;
				case 2:
				case 10:
					// 24 or 32 bit image
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alpha = 255;
					if( targa_header.pixel_size == 32 ) {
						alpha = *buf_p++;
						if( alpha != 255 )
							samples = 4;
					}
					break;
				case 3:
				case 11:
					// greyscale image
					blue = green = red = *buf_p++;
					alpha = 255;
					break;
				}
			}

			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;
			if( ++col == columns ) { // run spans across rows
				row++;
				col = 0;
				pixbuf += row_inc;
			}
		}
	}

	if( buffer != stack )
		FS_FreeFile( buffer );

	return samples;
}

/* 
================== 
WriteTGA
================== 
*/
qboolean WriteTGA( char *name, qbyte *buffer, int width, int height, qboolean rgb )
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

	// swap rgb to bgr
	c = 18+width*height*3;
	if( rgb ) {
		for( i = 18; i < c; i += 3 ) {
			temp = buffer[i];
			buffer[i] = buffer[i+2];
			buffer[i+2] = temp;
		}
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

static void jpg_noop( j_decompress_ptr cinfo ) {
}

static boolean jpg_fill_input_buffer( j_decompress_ptr cinfo )
{
    Com_DPrintf( "Premature end of jpeg file\n" );
    return 1;
}

static void jpg_skip_input_data( j_decompress_ptr cinfo, long num_bytes )
{
    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void jpeg_mem_src( j_decompress_ptr cinfo, qbyte *mem, int len )
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
int LoadJPG( char *name, qbyte **pic, int *width, int *height )
{
	int i, length, samples, l;
	qbyte *img, *scan, *buffer, *line;
	struct jpeg_error_mgr jerr;
	struct jpeg_decompress_struct cinfo;
	qbyte stack[0x4000];

	*pic = NULL;

	// load the file
	length = FS_LoadFile( name, (void **)&buffer, stack, sizeof( stack ) );
	if( !buffer )
		return 0;

	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_decompress( &cinfo );
	jpeg_mem_src( &cinfo, buffer, length );
	jpeg_read_header( &cinfo, TRUE );
	jpeg_start_decompress( &cinfo );
	samples = cinfo.output_components;

	if( samples != 3 && samples != 1 ) {
		Com_DPrintf( S_COLOR_YELLOW "Bad jpeg file %s\n", name );
		jpeg_destroy_decompress( &cinfo );
		if( buffer != stack )
			FS_FreeFile( buffer );
		return 0;
	}

	if( width )
		*width = cinfo.output_width;
	if( height )
	    *height = cinfo.output_height;

	img = *pic = Mem_TempMallocExt( cinfo.output_width * cinfo.output_height * 4, 0 );
	memset( img, 255, cinfo.output_width * cinfo.output_height * 4 );

	l = cinfo.output_width * samples;
	if( sizeof( stack ) >= l + length )
		line = stack + length;
	else
		line = Mem_TempMallocExt( l, 0 );

	while( cinfo.output_scanline < cinfo.output_height ) {
		scan = line;
		if( !jpeg_read_scanlines( &cinfo, &scan, 1 ) ) {
			Com_Printf( S_COLOR_YELLOW "Bad jpeg file %s\n", name );
			jpeg_destroy_decompress( &cinfo );
			if( line != stack + length )
				Mem_TempFree( line );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}

		if( samples == 1 ) {
			for( i = 0; i < cinfo.output_width; i++, img += 4 )
				img[0] = img[1] = img[2] = *scan++;
		} else {
			for( i = 0; i < cinfo.output_width; i++, img += 4, scan += 3 )
				img[0] = scan[0], img[1] = scan[1], img[2] = scan[2];
		}
	}

	jpeg_finish_decompress( &cinfo );
	jpeg_destroy_decompress( &cinfo );

	if( line != stack + length )
		Mem_TempFree( line );
	if( buffer != stack )
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

	// initialize the JPEG compression object
	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress( &cinfo );
	jpeg_stdio_dest( &cinfo, f );

	// setup JPEG parameters
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;

	jpeg_set_defaults( &cinfo );

	if( (quality > 100) || (quality <= 0) )
		quality = 85;

	jpeg_set_quality( &cinfo, quality, TRUE );

	// start compression
	jpeg_start_compress( &cinfo, qtrue );

	// feed scanline data
	w3 = cinfo.image_width * 3;
	offset = w3 * cinfo.image_height - w3;
	while( cinfo.next_scanline < cinfo.image_height ) {
		s[0] = &buffer[offset - cinfo.next_scanline * w3];
		jpeg_write_scanlines( &cinfo, s, 1 );
	}

	// finish compression
	jpeg_finish_compress( &cinfo );
	jpeg_destroy_compress( &cinfo );

	fclose ( f );

	return qtrue;
}

//=======================================================

/*
================
R_FlipTexture
================
*/
static void R_FlipTexture( const qbyte *in, qbyte *out, int width, int height, int samples, qboolean flipx, qboolean flipy, qboolean flipdiagonal )
{
	int i, x, y;
	const qbyte *p, *line;
	int row_inc = (flipy ? -samples : samples) * width, col_inc = (flipx ? -samples : samples);
	int row_ofs = (flipy ? (height - 1) * width * samples : 0), col_ofs = (flipx ? (width - 1) * samples : 0);

	if( flipdiagonal ) {
		for( x = 0, line = in + col_ofs; x < width; x++, line += col_inc )
			for( y = 0, p = line + row_ofs; y < height; y++, p += row_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	} else {
		for( y = 0, line = in + row_ofs; y < height; y++, line += row_inc )
			for( x = 0, p = line + col_ofs; x < width; x++, p += col_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	}
}

/*
================
R_ResampleTexture
================
*/
static void R_ResampleTexture( const unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight )
{
	int			i, j;
	const unsigned	*inrow, *inrow2;
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
R_HeightmapToNormalmap
================
*/
static void R_HeightmapToNormalmap( const qbyte *in, qbyte *out, int width, int height, float bumpScale )
{
	int x, y;
	vec3_t n;
	float ibumpScale;
	const qbyte *p0, *p1, *p2;

	if( !bumpScale )
		bumpScale = 1.0f;
	bumpScale *= max( 0, r_lighting_bumpscale->value );
	ibumpScale = (255.0 * 3.0) / bumpScale;

	memset( out, 255, width * height * 4 );
	for( y = 0; y < height; y++ ) {
		for( x = 0; x < width; x++, out += 4 ) {
			p0 = in + (y * width + x) * 4;
			p1 = (x == width - 1) ? p0 - x * 4 : p0 + 4;
			p2 = (y == height - 1) ? in + x * 4 : p0 + width * 4;

			n[0] = (p0[0] + p0[1] + p0[2]) - (p1[0] + p1[1] + p1[2]);
			n[1] = (p2[0] + p2[1] + p2[2]) - (p0[0] + p0[1] + p0[2]);
			n[2] = ibumpScale;
			VectorNormalize( n );

			out[0] = (n[0] + 1) * 127.5f;
			out[1] = (n[1] + 1) * 127.5f;
			out[2] = (n[2] + 1) * 127.5f;
		}
	}
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap( qbyte *in, int width, int height )
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
R_TextureFormat
===============
*/
static int R_TextureFormat( int samples, qboolean noCompress )
{
	int bits = r_texturebits->integer;

	if( glConfig.compressedTextures && !noCompress )  {
		if( samples == 3 )
			return GL_COMPRESSED_RGB_ARB;
		return GL_COMPRESSED_RGBA_ARB;
	}
	
	if( samples == 3 ) {
		if( bits == 16 )
			return GL_RGB5;
		else if( bits == 32 )
			return GL_RGB8;
		return GL_RGB;
	}

	if( bits == 16 )
		return GL_RGBA4;
	else if( bits == 32 )
		return GL_RGBA8;
	return GL_RGBA;
}

/*
===============
R_Upload32
===============
*/
void R_Upload32( qbyte **data, int width, int height, int flags, int *upload_width, int *upload_height, int samples, qboolean subImage )
{
	int			i, c, comp;
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

	// scan the texture for any non-255 alpha
	if( flags & (IT_NORGB|IT_NOALPHA) ) {
		qbyte *scan;

		if( flags & IT_NORGB ) {
			for( i = 0; i < numTextures; i++ ) {
				scan = ( qbyte * )data[i];
				for( c = width * height; c > 0; c--, scan += 4 )
					scan[0] = scan[1] = scan[2] = 255;
			}
		} else if( samples == 4 ) {
			for( i = 0; i < numTextures; i++ ) {
				scan = ( qbyte * )data[i] + 3;
				for( c = width * height; c > 0; c--, scan += 4 )
					*scan = 255;
			}
			samples = 3;
		}
	}

	comp = R_TextureFormat( samples, flags & IT_NOCOMPRESS );

	if( !( flags & IT_NOMIPMAP ) ) {
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.textureFilterAnisotropic )
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, min( gl_ext_texture_filter_anisotropic->value, glConfig.maxTextureFilterAnisotropic ) );
	} else {
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_max );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.textureFilterAnisotropic )
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
	}

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
		if( subImage ) {
			for( i = 0; i < numTextures; i++, target2++ )
				qglTexSubImage2D( target2, 0, 0, 0, scaledWidth, scaledHeight, GL_RGBA, GL_UNSIGNED_BYTE, data[i] );
		} else {
			for( i = 0; i < numTextures; i++, target2++ )
				qglTexImage2D( target2, 0, comp, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data[i] );
		}
	} else {
		scaled = ( unsigned * )Mem_TempMallocExt( scaledWidth * scaledHeight * 4, 0 );

		for( i = 0; i < numTextures; i++, target2++ ) {
			// resample the texture
			R_ResampleTexture( (unsigned *)(data[i]), width, height, scaled, scaledWidth, scaledHeight );

			if( subImage )
				qglTexSubImage2D( target2, 0, 0, 0, scaledWidth, scaledHeight, GL_RGBA, GL_UNSIGNED_BYTE, scaled );
			else
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

					if( subImage )	// omg...
						qglTexSubImage2D( target2, miplevel, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, scaled );
					else
						qglTexImage2D( target2, miplevel, comp, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled );
				}
			}
		}

		Mem_TempFree ( scaled );
	}
}

/*
===============
R_Upload32_3D

No resampling, scaling, mipmapping. Just to make 3D attenuation work ;)
===============
*/
void R_Upload32_3D_Fast( qbyte **data, int width, int height, int depth, int flags, int *upload_width, int *upload_height, int *upload_depth, int samples, qboolean subImage )
{
	int			comp;
	int			scaledWidth, scaledHeight, scaledDepth;

	if( !( flags & IT_NOMIPMAP ) || !( flags & IT_NOPICMIP ) )
		Com_Error( ERR_DROP, "R_Upload32_3D: mipmaps and picmip are not supported" );

	for( scaledWidth = 1; scaledWidth < width; scaledWidth <<= 1 );
	for( scaledHeight = 1; scaledHeight < height; scaledHeight <<= 1 );
	for( scaledDepth = 1; scaledDepth < depth; scaledDepth <<= 1 );

	if( width != scaledWidth || height != scaledHeight || depth != scaledDepth )
		Com_Error( ERR_DROP, "R_Upload32_3D: bad texture dimensions (not a power of 2)" );
	if( scaledWidth > glConfig.max3DTextureSize || scaledHeight > glConfig.max3DTextureSize || scaledDepth > glConfig.max3DTextureSize )
		Com_Error( ERR_DROP, "R_Upload32_3D: texture is too large (resizing is not supported)" );

	if( upload_width )
		*upload_width = scaledWidth;
	if( upload_height )
		*upload_height = scaledHeight;
	if( upload_depth )
		*upload_depth = scaledDepth;

	comp = R_TextureFormat( samples, flags & IT_NOCOMPRESS );

	if( !( flags & IT_NOMIPMAP ) ) {
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.textureFilterAnisotropic )
			qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_ANISOTROPY_EXT, min( gl_ext_texture_filter_anisotropic->value, glConfig.maxTextureFilterAnisotropic ) );
	} else {
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, gl_filter_max );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.textureFilterAnisotropic )
			qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
	}

	// clamp if required
	if( !( flags & IT_CLAMP ) ) {
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT );
	} else if( glConfig.textureEdgeClamp ) {
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	} else {
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP );
	}

	if( subImage )
		qglTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, 0, scaledWidth, scaledHeight, scaledDepth, GL_RGBA, GL_UNSIGNED_BYTE, data[0] );
	else
		qglTexImage3D( GL_TEXTURE_3D, 0, comp, scaledWidth, scaledHeight, scaledDepth, 0, GL_RGBA, GL_UNSIGNED_BYTE, data[0] );
}

/*
================
R_LoadPic
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
	image->depth = 1;
	image->flags = flags;
	image->texnum = r_numImages++;
	image->upload_depth = 1;

	GL_Bind( 0, image );
	R_Upload32( pic, width, height, flags, &image->upload_width, &image->upload_height, samples, qfalse );

	return image;
}

/*
================
R_LoadPic3D
================
*/
image_t *R_LoadPic3D( char *name, qbyte **pic, int width, int height, int depth, int flags, int samples )
{
	image_t		*image;

	if( r_numImages == MAX_GLIMAGES )
		Com_Error( ERR_DROP, "R_LoadPic: r_numImages == MAX_GLIMAGES" );

	image = images + r_numImages;
	Q_strncpyz( image->name, name, sizeof(image->name) );
	image->width = width;
	image->height = height;
	image->depth = depth;
	image->flags = flags;
	image->texnum = r_numImages++;

	GL_Bind( 0, image );
	R_Upload32_3D_Fast( pic, width, height, depth, flags, &image->upload_width, &image->upload_height, &image->upload_depth, samples, qfalse );

	return image;
}

/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t	*R_FindImage( char *name, int flags, float bumpScale )
{
	int		i, lastDot;
	unsigned int len, key;
	image_t	*image;
	qbyte	*pic;
	int		width, height, samples;
	char	pathname[MAX_QPATH], uploadName[MAX_QPATH];

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

	if( flags & IT_NORGB ) {
		uploadName[0] = '*', uploadName[1] = 'A';
		Q_strncpyz( &uploadName[2], pathname, sizeof( uploadName ) - 2 );
	} else if( flags & IT_NOALPHA ) {
		uploadName[0] = '*', uploadName[1] = 'C';
		Q_strncpyz( &uploadName[2], pathname, sizeof( uploadName ) - 2 );
	} else {
		Q_strncpyz( uploadName, pathname, sizeof( uploadName ) );
	}

	// look for it
	key = Com_HashKey( uploadName, IMAGES_HASH_SIZE );
	if( flags & IT_HEIGHTMAP ) {
		for( image = images_hash[key]; image; image = image->hash_next ) {
			if( (image->flags == flags) && (image->bumpScale == bumpScale) && !strcmp( image->name, uploadName ) )
				return image;
		}
	} else {
		for( image = images_hash[key]; image; image = image->hash_next ) {
			if( (image->flags == flags) && !strcmp( image->name, uploadName ) )
				return image;
		}
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
		qbyte *temp;

checkpic:
		if( flags & IT_HEIGHTMAP ) {
			temp = Mem_TempMallocExt( width * height * 4, 0 );
			R_HeightmapToNormalmap( pic, temp, width, height, bumpScale );
			Mem_TempFree( pic );
			pic = temp;
		}

		if( flags & (IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL) ) {
			temp = Mem_TempMallocExt( width * height * 4, 0 );
			R_FlipTexture( pic, temp, width, height, 4, (flags & IT_FLIPX), (flags & IT_FLIPY), (flags & IT_FLIPDIAGONAL) );
			Mem_TempFree( pic );
			pic = temp;
		}

		image = R_LoadPic( uploadName, &pic, width, height, flags, samples );
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
	int		i, j, lastDot;
	unsigned int len, key;
	image_t	*image;
	qbyte	*pic[6];
	int		width, height, lastSize, samples;
	char	pathname[MAX_QPATH];
	struct cubemapSufAndFlip { 
		char *suf; int flags; 
	} cubemapSides[2][6] = {
		{
			{ "px", 0 },
			{ "nx", 0 },
			{ "py", 0 },
			{ "ny", 0 },
			{ "pz", 0 },
			{ "nz", 0 }
		},
		{
			{ "rt", IT_FLIPDIAGONAL },
			{ "lf", IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL },
			{ "bk", IT_FLIPY },
			{ "ft", IT_FLIPX },
			{ "up", IT_FLIPDIAGONAL },
			{ "dn", IT_FLIPDIAGONAL }
		}
	};

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
	// load the pics from disk
	//
	image = NULL;
	memset( pic, 0, sizeof( pic ) );

	for( i = 0; i < 2; i++ ) {
		for( j = 0; j < 6; j++ ) {
			pathname[len+1] = cubemapSides[i][j].suf[0];
			pathname[len+2] = cubemapSides[i][j].suf[1];

			pathname[len+4] = 'j'; pathname[len+5] = 'p'; pathname[len+6] = 'g';
			samples = LoadJPG( pathname, &(pic[j]), &width, &height );
			if( pic[j] )
				goto checkpic;

			pathname[len+4] = 't'; pathname[len+5] = 'g'; pathname[len+6] = 'a';
			samples = LoadTGA( pathname, &(pic[j]), &width, &height );
			if( pic[j] )
				goto checkpic;

			pathname[len+4] = 'p'; pathname[len+5] = 'c'; pathname[len+6] = 'x';
			samples = LoadPCX( pathname, &(pic[j]), &width, &height );
			if( pic[j] ) {
checkpic:
				if( width != height ) {
					Com_Printf( "Not square cubemap image %s\n", pathname );
					break;
				}
				if( !j ) {
					lastSize = width;
				} else if( lastSize != width ) {
					Com_Printf( "Different cubemap image size: %s\n", pathname );
					break;
				}
				if( cubemapSides[i][j].flags & (IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL) ) {
					int flags = cubemapSides[i][j].flags;
					qbyte *temp = Mem_TempMallocExt( width * height * 4, 0 );
					R_FlipTexture( pic[j], temp, width, height, 4, (flags & IT_FLIPX), (flags & IT_FLIPY), (flags & IT_FLIPDIAGONAL) );
					Mem_TempFree( pic[j] );
					pic[j] = temp;
				}
				continue;
			}
			break;
		}
		if( j == 6 )
			break;
		for( j = 0; j < 6 && pic[j]; j++ ) {
			Mem_TempFree( pic[j] );
			pic[j] = NULL;
		}
	}

	if( i != 2 ) {
		pathname[len] = 0;
		image = R_LoadPic( pathname, pic, width, height, flags, samples );
		image->extension[0] = '.';
		strcpy( &image->extension[1], &pathname[len+4] );

		// add to hash table
		image->hash_next = images_hash[key];
		images_hash[key] = image;
	}

	for( j = 0; j < 6 && pic[j]; j++ )
		Mem_TempFree( pic[j] );

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

	// 
	// find a file name to save it to 
	// 
	for( i = 0; i < 100000; i++ ) {
		if( r_screenshot_jpeg->integer )
			Q_snprintfz( picname, sizeof(picname), "qfusion%05i.jpg", i );
		else
			Q_snprintfz( picname, sizeof(picname), "qfusion%05i.tga", i );
		Q_snprintfz( checkname, sizeof(checkname), "%s/screenshots/%s", FS_Gamedir(), picname );
		f = fopen( checkname, "rb" );
		if( !f )
			break;	// file doesn't exist
		fclose( f );
	}

	if( i == 100000 ) {
		Com_Printf( "R_ScreenShot: Couldn't create a file\n" ); 
		return;
 	}

shot:
	if( r_screenshot_jpeg->integer ) {
		buffer = Mem_TempMalloc( glState.width * glState.height * 3 );
		qglReadPixels( 0, 0, glState.width, glState.height, GL_RGB, GL_UNSIGNED_BYTE, buffer ); 

		if( WriteJPG( checkname, buffer, glState.width, glState.height, r_screenshot_jpeg_quality->integer ) && !silent )
			Com_Printf( "Wrote %s\n", picname );
	} else {
		buffer = Mem_TempMalloc( 18 + glState.width * glState.height * 3 );
		if( glConfig.BGRA ) {
			qglReadPixels( 0, 0, glState.width, glState.height, GL_BGR_EXT, GL_UNSIGNED_BYTE, buffer + 18 ); 
			if( WriteTGA( checkname, buffer, glState.width, glState.height, qfalse ) && !silent )
				Com_Printf( "Wrote %s\n", picname );
		} else {
			qglReadPixels( 0, 0, glState.width, glState.height, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18 ); 
			if( WriteTGA( checkname, buffer, glState.width, glState.height, qtrue ) && !silent )
				Com_Printf( "Wrote %s\n", picname );
		}
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
	qbyte	*buffer, *bufferFlipped;
	char	checkname[MAX_OSPATH];
	struct	cubemapSufAndFlip { 
		char *suf; vec3_t angles; int flags; 
	} cubemapShots[6] = {
		{ "px", {   0,   0, 0 }, IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL },
		{ "nx", {   0, 180, 0 }, IT_FLIPDIAGONAL },
		{ "py", {   0,  90, 0 }, IT_FLIPY },
		{ "ny", {   0, 270, 0 }, IT_FLIPX },
		{ "pz", { -90, 180, 0 }, IT_FLIPDIAGONAL },
		{ "nz", {  90, 180, 0 }, IT_FLIPDIAGONAL }
	};

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
	ri.params |= RP_ENVVIEW;

	// create the screenshots directory if it doesn't exist
	Q_snprintfz( checkname, sizeof( checkname ), "%s/env", FS_Gamedir () );
	Sys_Mkdir( checkname );

	buffer = Mem_TempMalloc( (size * size * 3) * 2 + 18 );
	bufferFlipped = buffer + size * size * 3;

	for( i = 0; i < 6; i++ ) {
		R_DrawCubemapView( r_lastRefdef.vieworg, cubemapShots[i].angles, size );

		if( glConfig.BGRA )
			qglReadPixels( 0, glState.height - size, size, size, GL_BGR_EXT, GL_UNSIGNED_BYTE, buffer ); 
		else
			qglReadPixels( 0, glState.height - size, size, size, GL_RGB, GL_UNSIGNED_BYTE, buffer ); 

		R_FlipTexture( buffer, bufferFlipped + 18, size, size, 3, (cubemapShots[i].flags & IT_FLIPX), (cubemapShots[i].flags & IT_FLIPY), (cubemapShots[i].flags & IT_FLIPDIAGONAL) );

		Q_snprintfz( checkname, sizeof( checkname ), "%s/env/%s_%s.tga", FS_Gamedir (), Cmd_Argv( 1 ), cubemapShots[i].suf );
		WriteTGA( checkname, bufferFlipped, size, size, !glConfig.BGRA );
	}

	ri.params &= ~RP_ENVVIEW;

	Mem_TempFree( buffer );
}

/* 
==================
R_BeginAviDemo
==================
*/
void R_BeginAviDemo( void )
{
	if( r_aviBuffer )
		Mem_Free( r_aviBuffer );
	r_aviBuffer = Mem_Alloc( resamplePool, 18 + glState.width * glState.height * 3 );
}

/* 
==================
R_WriteAviFrame
==================
*/
void R_WriteAviFrame( int frame, qboolean scissor )
{
	int x, y, w, h;
	char checkname[MAX_OSPATH];

	if( !r_aviBuffer )
		return;

	if( scissor ) {
		x = r_lastRefdef.x;
		y = glState.height - r_lastRefdef.height - r_lastRefdef.y;
		w = r_lastRefdef.width;
		h = r_lastRefdef.height;
	} else {
		x = 0;
		y = 0;
		w = glState.width;
		h = glState.height;
	}

	// create the avi directory if it doesn't exist
	Q_snprintfz( checkname, sizeof( checkname ), "%s/avi", FS_Gamedir () );
	Sys_Mkdir( checkname );
	Q_snprintfz( checkname, sizeof( checkname ), "%s/avi/avi%06i.%s", FS_Gamedir (), frame, ( r_screenshot_jpeg->integer ) ? "jpg" : "tga" );

	if( r_screenshot_jpeg->integer ) {
		qglReadPixels( x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, r_aviBuffer ); 
		WriteJPG( checkname, r_aviBuffer, w, h, r_screenshot_jpeg_quality->integer );
	} else {
		if( glConfig.BGRA ) {
			qglReadPixels( x, y, w, h, GL_BGR_EXT, GL_UNSIGNED_BYTE, r_aviBuffer + 18 ); 
			WriteTGA( checkname, r_aviBuffer, w, h, qfalse );
		} else {
			qglReadPixels( x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, r_aviBuffer + 18 ); 
			WriteTGA( checkname, r_aviBuffer, w, h, qtrue );
		}
	}
}

/* 
==================
R_StopAviDemo
==================
*/
void R_StopAviDemo( void )
{
	if( r_aviBuffer ) {
		Mem_Free( r_aviBuffer );
		r_aviBuffer = NULL;
	}
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
	r_cintexture->depth = 1;
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
	vec3_t v;
	float intensity;
	int x, y, z, d, size;
	qbyte *data;

	//
	// dynamic light texture
	//
	if( glConfig.texture3D ) {
		size = 32;
		data = Mem_TempMalloc( size * size * size * 4 );
	} else {
		size = 64;
		data = Mem_TempMalloc( size * size * 4 );
	}

	v[0] = v[1] = v[2] = 0;
	for( x = 0; x < size; x++ ) {
		for( y = 0; y < size; y++ ) {
			for( z = 0; z < size; z++ ) {
				v[0] = ((x + 0.5f) * (2.0f / (float)size) - 1.0f);
				v[1] = ((y + 0.5f) * (2.0f / (float)size) - 1.0f);
				if( glConfig.texture3D )
					v[2] = ((z + 0.5f) * (2.0f / (float)size) - 1.0f);

				intensity = 1.0f - sqrt( DotProduct( v, v ) );
				if( intensity > 0 )
					intensity = intensity * intensity * 225.0f;
				else
					intensity = 0;
				d = bound( 0, intensity, 255 );

				data[((z*size+y)*size + x) * 4 + 0] = d;
				data[((z*size+y)*size + x) * 4 + 1] = d;
				data[((z*size+y)*size + x) * 4 + 2] = d;
				data[((z*size+y)*size + x) * 4 + 3] = 255;

				if( !glConfig.texture3D )
					break;
			}
		}
	}

	if( glConfig.texture3D )
		r_dlighttexture = R_LoadPic3D( "***r_dlighttexture***", &data, size, size, size, IT_NOPICMIP|IT_NOMIPMAP|IT_CLAMP|IT_NOCOMPRESS, 3 );
	else
		r_dlighttexture = R_LoadPic( "***r_dlighttexture***", &data, size, size, IT_NOPICMIP|IT_NOMIPMAP|IT_CLAMP|IT_NOCOMPRESS, 3 );

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

	r_particletexture = R_LoadPic( "***r_particletexture***", &data, 16, 16, IT_NOPICMIP|IT_NOMIPMAP|IT_NOCOMPRESS, 4 );

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
	data = Mem_TempMalloc( 1 * 1 * 4 );
	data[0] = 255;
	data[1] = 255;
	data[2] = 255;
	data[3] = 255;

	r_whitetexture = R_LoadPic( "***r_whitetexture***", &data, 1, 1, IT_NOPICMIP|IT_NOCOMPRESS, 4 );

	Mem_TempFree ( data );
}

/*
==================
R_InitBlackTexture
==================
*/
void R_InitBlackTexture( void )
{
	qbyte *data;

	//
	// black texture
	//
	data = Mem_TempMalloc( 1 * 1 * 4 );
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 255;

	r_blacktexture = R_LoadPic( "***r_blacktexture***", &data, 1, 1, IT_NOPICMIP|IT_NOCOMPRESS, 3 );

	Mem_TempFree ( data );
}

/*
==================
R_InitBlankNormalmapTexture
==================
*/
void R_InitBlankNormalmapTexture( void )
{
	qbyte *data;

	//
	// white texture
	//
	data = Mem_TempMalloc( 1 * 1 * 4 );
	data[0] = 128;
	data[1] = 128;
	data[2] = 128;
	data[3] = 255;
	r_blanknormalmaptexture = R_LoadPic( "***r_blanknormalmap***", &data, 1, 1, IT_NOPICMIP|IT_NOCOMPRESS, 3 );

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
	R_InitBlackTexture ();
	R_InitFogTexture ();
	R_InitBloomTextures ();
	R_InitBlankNormalmapTexture ();
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

	r_aviBuffer = NULL;

	resampleWidth = 0;
	resampleBuffer = NULL;

	for( i = 0; i < r_numImages; i++ )
		qglDeleteTextures( 1, &images[i].texnum );

	r_numImages = 0;
	memset( images, 0, sizeof(images) );
	memset( r_lightmapTextures, 0, sizeof(r_lightmapTextures) );
	memset( images_hash, 0, sizeof(images_hash) );
}
