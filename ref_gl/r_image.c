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

#if defined ( __MACOSX__ )
#include <jpeglib.h>
#else
#include "jpeglib.h"
#endif

#define	MAX_GLIMAGES	    4096
#define IMAGES_HASH_SIZE    64

static image_t images[MAX_GLIMAGES];
image_t	*r_lightmapTextures[MAX_GLIMAGES];
static image_t *images_hash[IMAGES_HASH_SIZE];
static unsigned int image_cur_hash;
static int r_numImages;

static mempool_t *r_texturesPool;
static char *r_imagePathBuf, *r_imagePathBuf2;
static size_t r_sizeof_imagePathBuf, r_sizeof_imagePathBuf2;

#undef ENSUREBUFSIZE
#define ENSUREBUFSIZE(buf,need) \
	if( r_sizeof_ ##buf < need ) \
	{ \
		if( r_ ##buf ) \
			Mem_Free( r_ ##buf ); \
		r_sizeof_ ##buf += (((need) & (MAX_QPATH-1))+1) * MAX_QPATH; \
		r_ ##buf = Mem_Alloc( r_texturesPool, r_sizeof_ ##buf ); \
	}

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

int gl_filter_depth = GL_LINEAR;

void GL_SelectTexture( int tmu )
{
	if( !glConfig.ext.multitexture )
		return;
	if( tmu == glState.currentTMU )
		return;

	glState.currentTMU = tmu;

	if( qglActiveTextureARB )
	{
		qglActiveTextureARB( tmu + GL_TEXTURE0_ARB );
		qglClientActiveTextureARB( tmu + GL_TEXTURE0_ARB );
	}
	else if( qglSelectTextureSGIS )
	{
		qglSelectTextureSGIS( tmu + GL_TEXTURE0_SGIS );
	}
}

void GL_TexEnv( GLenum mode )
{
	if( mode != ( GLenum )glState.currentEnvModes[glState.currentTMU] )
	{
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
		glState.currentEnvModes[glState.currentTMU] = ( int )mode;
	}
}

void GL_Bind( int tmu, image_t *tex )
{
	GL_SelectTexture( tmu );

	if( r_nobind->integer )  // performance evaluation option
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

void GL_LoadTexMatrix( const mat4x4_t m )
{
	qglMatrixMode( GL_TEXTURE );
	qglLoadMatrixf( m );
	glState.texIdentityMatrix[glState.currentTMU] = qfalse;
}

void GL_LoadIdentityTexMatrix( void )
{
	if( !glState.texIdentityMatrix[glState.currentTMU] )
	{
		qglMatrixMode( GL_TEXTURE );
		qglLoadIdentity();
		glState.texIdentityMatrix[glState.currentTMU] = qtrue;
	}
}

void GL_EnableTexGen( int coord, int mode )
{
	int tmu = glState.currentTMU;
	int bit, gen;

	switch( coord )
	{
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
	case GL_Q:
		bit = 8;
		gen = GL_TEXTURE_GEN_Q;
		break;
	default:
		return;
	}

	if( mode )
	{
		if( !( glState.genSTEnabled[tmu] & bit ) )
		{
			qglEnable( gen );
			glState.genSTEnabled[tmu] |= bit;
		}
		qglTexGeni( coord, GL_TEXTURE_GEN_MODE, mode );
	}
	else
	{
		if( glState.genSTEnabled[tmu] & bit )
		{
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

	if( cmode != bit )
	{
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
	int minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

#define NUM_GL_MODES ( sizeof( modes ) / sizeof( glmode_t ) )

/*
===============
R_TextureMode
===============
*/
void R_TextureMode( char *string )
{
	int i;
	image_t	*glt;

	for( i = 0; i < NUM_GL_MODES; i++ )
	{
		if( !Q_stricmp( modes[i].name, string ) )
			break;
	}

	if( i == NUM_GL_MODES )
	{
		Com_Printf( "R_TextureMode: bad filter name\n" );
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for( i = 1, glt = images; i < r_numImages; i++, glt++ )
	{
		GL_Bind( 0, glt );

		if( glt->flags & IT_DEPTH )
		{
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_depth );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_depth );
		}
		else if( !( glt->flags & IT_NOMIPMAP ) )
		{
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
		else
		{
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
	int i, num_depth = 0;
	image_t	*image;
	double texels = 0, add;
	double depth_texels = 0;

	Com_Printf( "------------------\n" );

	for( i = 0, image = images; i < r_numImages; i++, image++ )
	{
		if( !image->upload_width || !image->upload_height || !image->upload_depth )
			continue;

		if( image->flags & IT_NOMIPMAP )
			add = image->upload_width * image->upload_height * image->upload_depth;
		else
			add = (unsigned)ceil( (double)image->upload_width * image->upload_height * image->upload_depth / 0.75 );

		if( image->flags & IT_CUBEMAP )
			add *= 6;

		if( image->flags & IT_DEPTH )
		{
			num_depth++;
			depth_texels += image->upload_width * image->upload_height * image->upload_depth;
			Com_Printf( " %4i %4i %4i: %s\n", image->upload_width, image->upload_height, image->upload_depth, 
				image->name );
		}
		else
		{
			texels += add;
			Com_Printf( " %4i %4i %4i: %s%s%s%s %.1f KB\n", image->upload_width, image->upload_height, image->upload_depth, 
				(image->flags & IT_NORGB ? "*A" : (image->flags & IT_NOALPHA ? "*C" : "")),
				image->name, image->extension, ((image->flags & IT_NOMIPMAP) ? "" : " (M)"), add / 1024.0 );
		}
	}

	Com_Printf( "Total texels count (counting mipmaps, approx): %.0f\n", texels );
	Com_Printf( "%i RGBA images, totalling %.3f megabytes\n", r_numImages - 1, texels / (1048576.0 / 4.0) );
	if( num_depth )
		Com_Printf( "%i depth images, totalling %.0f texels\n", num_depth, depth_texels );
}

/*
=================================================================

TEMPORARY IMAGE BUFFERS

=================================================================
*/

enum
{
	TEXTURE_LOADING_BUF0,TEXTURE_LOADING_BUF1,TEXTURE_LOADING_BUF2,TEXTURE_LOADING_BUF3,TEXTURE_LOADING_BUF4,TEXTURE_LOADING_BUF5,
	TEXTURE_RESAMPLING_BUF,
	TEXTURE_LINE_BUF,
	TEXTURE_FLIPPING_BUF0,TEXTURE_FLIPPING_BUF1,TEXTURE_FLIPPING_BUF2,TEXTURE_FLIPPING_BUF3,TEXTURE_FLIPPING_BUF4,TEXTURE_FLIPPING_BUF5,

	NUM_IMAGE_BUFFERS
};

static qbyte *r_aviBuffer;

static qbyte *r_imageBuffers[NUM_IMAGE_BUFFERS];
static size_t r_imageBufSize[NUM_IMAGE_BUFFERS];

#define R_PrepareImageBuffer(buffer,size) _R_PrepareImageBuffer(buffer,size,__FILE__,__LINE__)

/*
==============
R_PrepareImageBuffer
==============
*/
static qbyte *_R_PrepareImageBuffer( int buffer, size_t size, const char *filename, int fileline )
{
	if( r_imageBufSize[buffer] < size )
	{
		r_imageBufSize[buffer] = size;
		if( r_imageBuffers[buffer] )
			Mem_Free( r_imageBuffers[buffer] );
		r_imageBuffers[buffer] = _Mem_Alloc( r_texturesPool, size, 0, 0, filename, fileline );
	}

	memset( r_imageBuffers[buffer], 255, size );

	return r_imageBuffers[buffer];
}

/*
==============
R_FreeImageBuffers
==============
*/
void R_FreeImageBuffers( void )
{
	int i;

	for( i = 0; i < NUM_IMAGE_BUFFERS; i++ )
	{
		if( r_imageBuffers[i] )
		{
			Mem_Free( r_imageBuffers[i] );
			r_imageBuffers[i] = NULL;
		}
		r_imageBufSize[i] = 0;
	}
}

/*
=================================================================

PCX LOADING

=================================================================
*/

typedef struct
{
	char manufacturer;
	char version;
	char encoding;
	char bits_per_pixel;
	unsigned short xmin, ymin, xmax, ymax;
	unsigned short hres, vres;
	unsigned char palette[48];
	char reserved;
	char color_planes;
	unsigned short bytes_per_line;
	unsigned short palette_type;
	char filler[58];
	unsigned char data;         // unbounded
} pcx_t;

/*
==============
LoadPCX
==============
*/
static int LoadPCX( const char *filename, qbyte **pic, int *width, int *height, int side )
{
	qbyte *raw;
	pcx_t *pcx;
	int x, y, samples = 3;
	int len, columns, rows;
	int dataByte, runLength;
	qbyte pal[768], *pix;
	qbyte stack[0x4000];

	*pic = NULL;

	//
	// load the file
	//
	len = FS_LoadFile( filename, (void **)&raw, stack, sizeof( stack ) );
	if( !raw )
		return 0;

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;

	pcx->xmin = LittleShort( pcx->xmin );
	pcx->ymin = LittleShort( pcx->ymin );
	pcx->xmax = LittleShort( pcx->xmax );
	pcx->ymax = LittleShort( pcx->ymax );
	pcx->hres = LittleShort( pcx->hres );
	pcx->vres = LittleShort( pcx->vres );
	pcx->bytes_per_line = LittleShort( pcx->bytes_per_line );
	pcx->palette_type = LittleShort( pcx->palette_type );

	raw = &pcx->data;

	if( pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8 )
	{
		Com_DPrintf( S_COLOR_YELLOW "Bad pcx file %s\n", filename );
		if( ( qbyte *)pcx != stack )
			FS_FreeFile( pcx );
		return 0;
	}

	columns = pcx->xmax + 1;
	rows = pcx->ymax + 1;
	pix = *pic = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0+side, columns * rows * 4 );
	memcpy( pal, (qbyte *)pcx + len - 768, 768 );

	if( width )
		*width = columns;
	if( height )
		*height = rows;

	for( y = 0; y < rows; y++ )
	{
		for( x = 0; x < columns; )
		{
			dataByte = *raw++;

			if( ( dataByte & 0xC0 ) == 0xC0 )
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while( runLength-- > 0 )
			{
#ifdef QUAKE2_JUNK
				if( dataByte == 255 )
				{	// hack Quake2 palette
					pix[0] = 0;
					pix[1] = 0;
					pix[2] = 0;
					pix[3] = 0;
					samples = 4;
				}
				else
#endif
				{
					pix[0] = pal[dataByte*3+0];
					pix[1] = pal[dataByte*3+1];
					pix[2] = pal[dataByte*3+2];
					pix[3] = 255;
				}
				x++; pix += 4;
			}
		}
	}

	if( raw - (qbyte *)pcx > len )
	{
		Com_DPrintf( S_COLOR_YELLOW "PCX file %s was malformed", filename );
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

typedef struct _TargaHeader
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;


/*
=============
LoadTGA
=============
*/
static int LoadTGA( const char *name, qbyte **pic, int *width, int *height, int side )
{
	int i, columns, rows, row_inc, row, col;
	qbyte *buf_p, *buffer, *pixbuf, *targa_rgba;
	int length, samples, readpixelcount, pixelcount;
	qbyte palette[256][4], red = 0, green = 0, blue = 0, alpha = 0;
	qboolean compressed;
	TargaHeader targa_header;
	qbyte stack[0x4000];

	*pic = NULL;

	//
	// load the file
	//
	length = FS_LoadFile( name, (void **)&buffer, stack, sizeof( stack ) );
	if( !buffer )
		return 0;

	buf_p = buffer;
	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = buf_p[0] + buf_p[1] * 256;
	buf_p += 2;
	targa_header.colormap_length = buf_p[0] + buf_p[1] * 256;
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort( *( (short *)buf_p ) );
	buf_p += 2;
	targa_header.y_origin = LittleShort( *( (short *)buf_p ) );
	buf_p += 2;
	targa_header.width = LittleShort( *( (short *)buf_p ) );
	buf_p += 2;
	targa_header.height = LittleShort( *( (short *)buf_p ) );
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;
	if( targa_header.id_length != 0 )
		buf_p += targa_header.id_length; // skip TARGA image comment

	if( targa_header.image_type == 1 || targa_header.image_type == 9 )
	{
		// uncompressed colormapped image
		if( targa_header.pixel_size != 8 )
		{
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit images supported for type 1 and 9" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
		if( targa_header.colormap_length != 256 )
		{
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit colormaps are supported for type 1 and 9" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
		if( targa_header.colormap_index )
		{
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: colormap_index is not supported for type 1 and 9" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
		if( targa_header.colormap_size == 24 )
		{
			for( i = 0; i < targa_header.colormap_length; i++ )
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = 255;
			}
		}
		else if( targa_header.colormap_size == 32 )
		{
			for( i = 0; i < targa_header.colormap_length; i++ )
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
		}
		else
		{
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: only 24 and 32 bit colormaps are supported for type 1 and 9" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
	}
	else if( targa_header.image_type == 2 || targa_header.image_type == 10 )
	{
		// uncompressed or RLE compressed RGB
		if( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 )
		{
			Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 32 or 24 bit images supported for type 2 and 10" );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}
	}
	else if( targa_header.image_type == 3 || targa_header.image_type == 11 )
	{
		// uncompressed greyscale
		if( targa_header.pixel_size != 8 )
		{
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

	targa_rgba = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0+side, columns * rows * 4 );
	*pic = targa_rgba;

	// if bit 5 of attributes isn't set, the image has been stored from bottom to top
	if( targa_header.attributes & 0x20 )
	{
		pixbuf = targa_rgba;
		row_inc = 0;
	}
	else
	{
		pixbuf = targa_rgba + ( rows - 1 ) * columns * 4;
		row_inc = -columns * 4 * 2;
	}

	compressed = ( targa_header.image_type == 9 || targa_header.image_type == 10 || targa_header.image_type == 11 );
	for( row = col = 0, samples = 3; row < rows; )
	{
		pixelcount = 0x10000;
		readpixelcount = 0x10000;

		if( compressed )
		{
			pixelcount = *buf_p++;
			if( pixelcount & 0x80 )  // run-length packet
				readpixelcount = 1;
			pixelcount = 1 + ( pixelcount & 0x7f );
		}

		while( pixelcount-- && ( row < rows ) )
		{
			if( readpixelcount-- > 0 )
			{
				switch( targa_header.image_type )
				{
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
					if( targa_header.pixel_size == 32 )
					{
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
			if( ++col == columns )
			{            // run spans across rows
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
static qboolean WriteTGA( const char *name, qbyte *buffer, int width, int height, qboolean bgr )
{
	FILE *f;
	int i, c, temp;

	if( !(f = fopen( name, "wb" ) ) )
	{
		Com_Printf( "WriteTGA: Couldn't create %s\n", name );
		return qfalse;
	}

	buffer[2] = 2;  // uncompressed type
	buffer[12] = width&255;
	buffer[13] = width>>8;
	buffer[14] = height&255;
	buffer[15] = height>>8;
	buffer[16] = 24; // pixel size

	// swap rgb to bgr
	c = 18+width*height*3;
	if( !bgr )
	{
		for( i = 18; i < c; i += 3 )
		{
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

static void jpg_noop( j_decompress_ptr cinfo )
{
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
		( *cinfo->mem->alloc_small )( (j_common_ptr) cinfo,
		JPOOL_PERMANENT,
		sizeof( struct jpeg_source_mgr ) );
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
static int LoadJPG( const char *name, qbyte **pic, int *width, int *height, int side )
{
	unsigned int i, length, samples, l;
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

	if( samples != 3 && samples != 1 )
	{
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

	img = *pic = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0+side, cinfo.output_width * cinfo.output_height * 4 );
	l = cinfo.output_width * samples;
	if( sizeof( stack ) >= l + length )
		line = stack + length;
	else
		line = R_PrepareImageBuffer( TEXTURE_LINE_BUF, l );

	while( cinfo.output_scanline < cinfo.output_height )
	{
		scan = line;
		if( !jpeg_read_scanlines( &cinfo, &scan, 1 ) )
		{
			Com_Printf( S_COLOR_YELLOW "Bad jpeg file %s\n", name );
			jpeg_destroy_decompress( &cinfo );
			if( buffer != stack )
				FS_FreeFile( buffer );
			return 0;
		}

		if( samples == 1 )
		{
			for( i = 0; i < cinfo.output_width; i++, img += 4 )
				img[0] = img[1] = img[2] = *scan++;
		}
		else
		{
			for( i = 0; i < cinfo.output_width; i++, img += 4, scan += 3 )
				img[0] = scan[0], img[1] = scan[1], img[2] = scan[2];
		}
	}

	jpeg_finish_decompress( &cinfo );
	jpeg_destroy_decompress( &cinfo );

	if( buffer != stack )
		FS_FreeFile( buffer );

	return 3;
}

/*
==================
WriteJPG
==================
*/
static qboolean WriteJPG( const char *name, qbyte *buffer, int width, int height, int quality )
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *f;
	JSAMPROW s[1];
	int offset, w3;

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

	if( ( quality > 100 ) || ( quality <= 0 ) )
		quality = 85;

	jpeg_set_quality( &cinfo, quality, TRUE );

	// start compression
	jpeg_start_compress( &cinfo, qtrue );

	// feed scanline data
	w3 = cinfo.image_width * 3;
	offset = w3 * cinfo.image_height - w3;
	while( cinfo.next_scanline < cinfo.image_height )
	{
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
===============
R_LoadImageFromDisk
===============
*/
static int R_LoadImageFromDisk( char *pathname, size_t pathname_size, qbyte **pic, int *width, int *height, int side )
{
	int samples;

	*pic = NULL;
	*width = *height = 0;

	Q_strncatz( pathname, ".jpg", pathname_size );

	samples = LoadJPG( pathname, pic, width, height, side );
	if( samples )
		return samples;

	samples = LoadTGA( COM_ReplaceExtension( pathname, ".tga" ), pic, width, height, side );
	if( samples )
		return samples;

	samples = LoadPCX( COM_ReplaceExtension( pathname, ".pcx" ), pic, width, height, side );
	if( samples )
		return samples;

	return 0;
}

/*
================
R_FlipTexture
================
*/
static void R_FlipTexture( const qbyte *in, qbyte *out, int width, int height, int samples, qboolean flipx, qboolean flipy, qboolean flipdiagonal )
{
	int i, x, y;
	const qbyte *p, *line;
	int row_inc = ( flipy ? -samples : samples ) * width, col_inc = ( flipx ? -samples : samples );
	int row_ofs = ( flipy ? ( height - 1 ) * width * samples : 0 ), col_ofs = ( flipx ? ( width - 1 ) * samples : 0 );

	if( flipdiagonal )
	{
		for( x = 0, line = in + col_ofs; x < width; x++, line += col_inc )
			for( y = 0, p = line + row_ofs; y < height; y++, p += row_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	}
	else
	{
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
	int i, j;
	const unsigned *inrow, *inrow2;
	unsigned frac, fracstep;
	unsigned *p1, *p2;
	qbyte *pix1, *pix2, *pix3, *pix4;

	if( inwidth == outwidth && inheight == outheight )
	{
		memcpy( out, in, inwidth * inheight * 4 );
		return;
	}

	p1 = ( unsigned * )R_PrepareImageBuffer( TEXTURE_LINE_BUF, outwidth * 1 * sizeof( unsigned ) * 2 );
	p2 = p1 + outwidth;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for( i = 0; i < outwidth; i++ )
	{
		p1[i] = 4 * ( frac >> 16 );
		frac += fracstep;
	}

	frac = 3 * ( fracstep >> 2 );
	for( i = 0; i < outwidth; i++ )
	{
		p2[i] = 4 * ( frac >> 16 );
		frac += fracstep;
	}

	for( i = 0; i < outheight; i++, out += outwidth )
	{
		inrow = in + inwidth * (int)( ( i + 0.25 ) * inheight / outheight );
		inrow2 = in + inwidth * (int)( ( i + 0.75 ) * inheight / outheight );

		for( j = 0; j < outwidth; j++ )
		{
			pix1 = (qbyte *)inrow + p1[j];
			pix2 = (qbyte *)inrow + p2[j];
			pix3 = (qbyte *)inrow2 + p1[j];
			pix4 = (qbyte *)inrow2 + p2[j];
			( ( qbyte * )( out + j ) )[0] = ( pix1[0] + pix2[0] + pix3[0] + pix4[0] ) >> 2;
			( ( qbyte * )( out + j ) )[1] = ( pix1[1] + pix2[1] + pix3[1] + pix4[1] ) >> 2;
			( ( qbyte * )( out + j ) )[2] = ( pix1[2] + pix2[2] + pix3[2] + pix4[2] ) >> 2;
			( ( qbyte * )( out + j ) )[3] = ( pix1[3] + pix2[3] + pix3[3] + pix4[3] ) >> 2;
		}
	}
}

/*
================
R_HeightmapToNormalmap
================
*/
static int R_HeightmapToNormalmap( const qbyte *in, qbyte *out, int width, int height, float bumpScale )
{
	int x, y;
	vec3_t n;
	float ibumpScale;
	const qbyte *p0, *p1, *p2;

	if( !bumpScale )
		bumpScale = 1.0f;
	bumpScale *= max( 0, r_lighting_bumpscale->value );
	ibumpScale = ( 255.0 * 3.0 ) / bumpScale;

	memset( out, 255, width * height * 4 );
	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++, out += 4 )
		{
			p0 = in + ( y * width + x ) * 4;
			p1 = ( x == width - 1 ) ? p0 - x * 4 : p0 + 4;
			p2 = ( y == height - 1 ) ? in + x * 4 : p0 + width * 4;

			n[0] = ( p0[0] + p0[1] + p0[2] ) - ( p1[0] + p1[1] + p1[2] );
			n[1] = ( p2[0] + p2[1] + p2[2] ) - ( p0[0] + p0[1] + p0[2] );
			n[2] = ibumpScale;
			VectorNormalize( n );

			out[0] = ( n[0] + 1 ) * 127.5f;
			out[1] = ( n[1] + 1 ) * 127.5f;
			out[2] = ( n[2] + 1 ) * 127.5f;
			out[3] = ( p0[0] + p0[1] + p0[2] ) / 3;
		}
	}

	return 4;
}

/*
================
R_MergeNormalmapDepthmap
================
*/
static int R_MergeNormalmapDepthmap( const char *pathname, qbyte *in, int iwidth, int iheight )
{
	const char *p;
	int width, height, samples;
	qbyte *pic, *pic2;
	char *depthName;
	size_t depthNameSize;

	ENSUREBUFSIZE( imagePathBuf2, strlen( pathname ) + (strlen( "depth" ) + 1) + 5 );
	depthName = r_imagePathBuf2;
	depthNameSize = r_sizeof_imagePathBuf2;

	Q_strncpyz( depthName, pathname, depthNameSize );

	p = Q_strrstr( pathname, "_norm" );
	if( !p )
		p = pathname + strlen( pathname );
	Q_strncpyz( depthName + (p - pathname), "_depth", depthNameSize - (p - pathname) );

	pic = NULL;
	samples = R_LoadImageFromDisk( depthName, depthNameSize, &pic, &width, &height, 1 );

	if( pic )
	{
		if( (width == iwidth) && (height == iheight) )
		{
			int i;
			for( i = iwidth*iheight - 1, pic2 = pic; i > 0; i--, in += 4, pic2 += 4 )
				in[3] = ((int)pic2[0] + (int)pic2[1] + (int)pic2[2]) / 3;
			return 4;
		}
		else
		{
			Com_Printf( S_COLOR_YELLOW "WARNING: different depth map dimensions differ from parent (%s)\n", depthName );
		}
	}

	return 3;
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
note: if given odd width/height this discards the last row/column of
pixels, rather than doing a proper box-filter scale down (LordHavoc)
================
*/
static void R_MipMap( qbyte *in, int width, int height )
{
	int i, j;
	qbyte *out;

	width <<= 2;
	height >>= 1;

	out = in;
	for( i = 0; i < height; i++, in += width )
	{
		for( j = 0; j < width; j += 8, out += 4, in += 8 )
		{
			out[0] = ( in[0] + in[4] + in[width+0] + in[width+4] )>>2;
			out[1] = ( in[1] + in[5] + in[width+1] + in[width+5] )>>2;
			out[2] = ( in[2] + in[6] + in[width+2] + in[width+6] )>>2;
			out[3] = ( in[3] + in[7] + in[width+3] + in[width+7] )>>2;
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

	if( glConfig.ext.texture_compression && !noCompress )
	{
		if( samples == 3 )
			return GL_COMPRESSED_RGB_ARB;
		return GL_COMPRESSED_RGBA_ARB;
	}

	if( samples == 3 )
	{
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
void R_Upload32( qbyte **data, int width, int height, int flags, int *upload_width, int *upload_height, int *samples, qboolean subImage )
{
	int i, c, comp, format;
	int target, target2;
	int numTextures;
	unsigned *scaled = NULL;
	int scaledWidth, scaledHeight;

	assert( samples );

	if( glConfig.ext.texture_non_power_of_two )
	{
		scaledWidth = width;
		scaledHeight = height;
	}
	else
	{
		for( scaledWidth = 1; scaledWidth < width; scaledWidth <<= 1 );
		for( scaledHeight = 1; scaledHeight < height; scaledHeight <<= 1 );
	}

	if( flags & IT_SKY )
	{
		// let people sample down the sky textures for speed
		scaledWidth >>= r_skymip->integer;
		scaledHeight >>= r_skymip->integer;
	}
	else if( !( flags & IT_NOPICMIP ) )
	{
		// let people sample down the world textures for speed
		scaledWidth >>= r_picmip->integer;
		scaledHeight >>= r_picmip->integer;
	}

	// don't ever bother with > maxSize textures
	if( flags & IT_CUBEMAP )
	{
		numTextures = 6;
		target = GL_TEXTURE_CUBE_MAP_ARB;
		target2 = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
		clamp( scaledWidth, 1, glConfig.maxTextureCubemapSize );
		clamp( scaledHeight, 1, glConfig.maxTextureCubemapSize );
	}
	else
	{
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
	if( flags & ( IT_NORGB|IT_NOALPHA ) )
	{
		qbyte *scan;

		if( flags & IT_NORGB )
		{
			for( i = 0; i < numTextures && data[i]; i++ )
			{
				scan = ( qbyte * )data[i];
				for( c = width * height; c > 0; c--, scan += 4 )
					scan[0] = scan[1] = scan[2] = 255;
			}
		}
		else if( *samples == 4 )
		{
			for( i = 0; i < numTextures && data[i]; i++ )
			{
				scan = ( qbyte * )data[i] + 3;
				for( c = width * height; c > 0; c--, scan += 4 )
					*scan = 255;
			}
			*samples = 3;
		}
	}

	if( flags & IT_DEPTH )
	{
		comp = GL_DEPTH_COMPONENT;
		format = GL_DEPTH_COMPONENT;
	}
	else
	{
		comp = R_TextureFormat( *samples, flags & IT_NOCOMPRESS );
		format = GL_RGBA;
	}

	if( flags & IT_DEPTH )
	{
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_depth );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_depth );

		if( glConfig.ext.texture_filter_anisotropic )
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
	}
	else if( !( flags & IT_NOMIPMAP ) )
	{
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.ext.texture_filter_anisotropic )
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, glConfig.curTextureFilterAnisotropic );
	}
	else
	{
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_max );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.ext.texture_filter_anisotropic )
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
	}

	// clamp if required
	if( !( flags & IT_CLAMP ) )
	{
		qglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_REPEAT );
		qglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_REPEAT );
	}
	else if( glConfig.ext.texture_edge_clamp )
	{
		qglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}
	else
	{
		qglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP );
	}

	if( ( scaledWidth == width ) && ( scaledHeight == height ) && ( flags & IT_NOMIPMAP ) )
	{
		if( subImage )
		{
			for( i = 0; i < numTextures; i++, target2++ )
				qglTexSubImage2D( target2, 0, 0, 0, scaledWidth, scaledHeight, format, GL_UNSIGNED_BYTE, data[i] );
		}
		else
		{
			for( i = 0; i < numTextures; i++, target2++ )
				qglTexImage2D( target2, 0, comp, scaledWidth, scaledHeight, 0, format, GL_UNSIGNED_BYTE, data[i] );
		}
	}
	else
	{
		qboolean driverMipmap = glConfig.ext.generate_mipmap && !(flags & IT_CUBEMAP);

		for( i = 0; i < numTextures; i++, target2++ )
		{
			unsigned int *mip;

			if( scaledWidth == width && scaledHeight == height && driverMipmap )
			{
				mip = (unsigned *)(data[i]);
			}
			else
			{
				if( !scaled )
					scaled = ( unsigned * )R_PrepareImageBuffer( TEXTURE_RESAMPLING_BUF, scaledWidth * scaledHeight * 4 );

				// resample the texture
				mip = scaled;
				if( data[i] )
					R_ResampleTexture( (unsigned int *)( data[i] ), width, height, mip, scaledWidth, scaledHeight );
				else
					mip = (unsigned *)NULL;
			}

			// automatic mipmaps generation
			if( !( flags & IT_NOMIPMAP ) && mip && driverMipmap )
				qglTexParameteri( target2, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );

			if( subImage )
				qglTexSubImage2D( target2, 0, 0, 0, scaledWidth, scaledHeight, format, GL_UNSIGNED_BYTE, mip );
			else
				qglTexImage2D( target2, 0, comp, scaledWidth, scaledHeight, 0, format, GL_UNSIGNED_BYTE, mip );

			// mipmaps generation
			if( !( flags & IT_NOMIPMAP ) && mip && !driverMipmap )
			{
				int w, h;
				int miplevel = 0;

				w = scaledWidth;
				h = scaledHeight;
				while( w > 1 || h > 1 )
				{
					R_MipMap( (qbyte *)mip, w, h );

					w >>= 1;
					h >>= 1;
					if( w < 1 )
						w = 1;
					if( h < 1 )
						h = 1;
					miplevel++;

					if( subImage )
						qglTexSubImage2D( target2, miplevel, 0, 0, w, h, format, GL_UNSIGNED_BYTE, mip );
					else
						qglTexImage2D( target2, miplevel, comp, w, h, 0, format, GL_UNSIGNED_BYTE, mip );
				}
			}
		}
	}
}

/*
===============
R_Upload32_3D

No resampling, scaling, mipmapping. Just to make 3D attenuation work ;)
===============
*/
void R_Upload32_3D_Fast( qbyte **data, int width, int height, int depth, int flags, int *upload_width, int *upload_height, int *upload_depth, int *samples, qboolean subImage )
{
	int comp;
	int scaledWidth, scaledHeight, scaledDepth;

	assert( samples );
	assert( ( flags & (IT_NOMIPMAP|IT_NOPICMIP) ) );

	for( scaledWidth = 1; scaledWidth < width; scaledWidth <<= 1 ) ;
	for( scaledHeight = 1; scaledHeight < height; scaledHeight <<= 1 ) ;
	for( scaledDepth = 1; scaledDepth < depth; scaledDepth <<= 1 ) ;

	if( width != scaledWidth || height != scaledHeight || depth != scaledDepth )
		Com_Error( ERR_DROP, "R_Upload32_3D: bad texture dimensions (not a power of 2)" );
	if( scaledWidth > glConfig.maxTextureSize3D || scaledHeight > glConfig.maxTextureSize3D || scaledDepth > glConfig.maxTextureSize3D )
		Com_Error( ERR_DROP, "R_Upload32_3D: texture is too large (resizing is not supported)" );

	if( upload_width )
		*upload_width = scaledWidth;
	if( upload_height )
		*upload_height = scaledHeight;
	if( upload_depth )
		*upload_depth = scaledDepth;

	comp = R_TextureFormat( *samples, flags & IT_NOCOMPRESS );

	if( !( flags & IT_NOMIPMAP ) )
	{
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.ext.texture_filter_anisotropic )
			qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_ANISOTROPY_EXT, glConfig.curTextureFilterAnisotropic );
	}
	else
	{
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, gl_filter_max );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.ext.texture_filter_anisotropic )
			qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
	}

	// clamp if required
	if( !( flags & IT_CLAMP ) )
	{
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT );
	}
	else if( glConfig.ext.texture_edge_clamp )
	{
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		qglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	}
	else
	{
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
R_InitPic
================
*/
static inline image_t *R_InitPic( const char *name, qbyte **pic, int width, int height, int depth, int flags, int samples )
{
	image_t	*image;

	if( r_numImages == MAX_GLIMAGES )
		Com_Error( ERR_DROP, "R_LoadPic: r_numImages == MAX_GLIMAGES" );

	image = images + r_numImages;
	image->name = Mem_Alloc( r_texturesPool, strlen( name ) + 1 );
	strcpy( image->name, name );
	image->width = width;
	image->height = height;
	image->depth = image->upload_depth = depth;
	image->flags = flags;
	image->samples = samples;
	image->texnum = ++r_numImages;

	// add to hash table
	if( image_cur_hash >= IMAGES_HASH_SIZE )
		image_cur_hash = Com_HashKey( name, IMAGES_HASH_SIZE );
	image->hash_next = images_hash[image_cur_hash];
	images_hash[image_cur_hash] = image;

	qglDeleteTextures( 1, &image->texnum );
	GL_Bind( 0, image );

	if( depth == 1 )
		R_Upload32( pic, width, height, flags, &image->upload_width, &image->upload_height, &image->samples, qfalse );
	else
		R_Upload32_3D_Fast( pic, width, height, depth, flags, &image->upload_width, &image->upload_height, &image->upload_depth, &image->samples, qfalse );

	image_cur_hash = IMAGES_HASH_SIZE+1;
	return image;
}

/*
================
R_LoadPic
================
*/
image_t *R_LoadPic( const char *name, qbyte **pic, int width, int height, int flags, int samples )
{
	return R_InitPic( name, pic, width, height, 1, flags, samples );
}

/*
================
R_Load3DPic
================
*/
image_t *R_Load3DPic( const char *name, qbyte **pic, int width, int height, int depth, int flags, int samples )
{
	return R_InitPic( name, pic, width, height, depth, flags, samples );
}

/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t	*R_FindImage( const char *name, const char *suffix, int flags, float bumpScale )
{
	int i, lastDot;
	unsigned int len, key;
	image_t	*image;
	int width = 1, height = 1, samples = 0;
	char *pathname;
	size_t pathsize;

	if( !name || !name[0] )
		return NULL; //	Com_Error (ERR_DROP, "R_FindImage: NULL name");

	ENSUREBUFSIZE( imagePathBuf, strlen( name ) + (suffix ? strlen( suffix ) + 1 : 0) + 5 );
	pathname = r_imagePathBuf;
	pathsize = r_sizeof_imagePathBuf;

	lastDot = -1;
	for( i = ( name[0] == '/' || name[0] == '\\' ), len = 0; name[i]; i++ )
	{
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
	if( suffix )
	{
		pathname[len++] = '_';
		for( i = 0; suffix[i]; i++ )
			pathname[len++] = tolower( suffix[i] );
	}

	// look for it
	key = image_cur_hash = Com_HashKey( pathname, IMAGES_HASH_SIZE );
	if( flags & IT_HEIGHTMAP )
	{
		for( image = images_hash[key]; image; image = image->hash_next )
		{
			if( ( image->flags == flags ) && ( image->bumpScale == bumpScale ) && !strcmp( image->name, pathname ) )
				return image;
		}
	}
	else
	{
		for( image = images_hash[key]; image; image = image->hash_next )
		{
			if( ( image->flags == flags ) && !strcmp( image->name, pathname ) )
				return image;
		}
	}

	pathname[len] = 0;
	image = NULL;

	//
	// load the pic from disk
	//
	if( flags & IT_CUBEMAP )
	{
		qbyte *pic[6];
		struct cubemapSufAndFlip
		{
			char *suf; int flags;
		} cubemapSides[2][6] = {
			{ 
				{ "px", 0 }, { "nx", 0 }, { "py", 0 },
				{ "ny", 0 }, { "pz", 0 }, { "nz", 0 } 
			},
			{
				{ "rt", IT_FLIPDIAGONAL }, { "lf", IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL }, { "bk", IT_FLIPY },
				{ "ft", IT_FLIPX }, { "up", IT_FLIPDIAGONAL }, { "dn", IT_FLIPDIAGONAL }
			}
		};
		int j, lastSize = 0;

		pathname[len] = '_';
		for( i = 0; i < 2; i++ )
		{
			for( j = 0; j < 6; j++ )
			{
				pathname[len+1] = cubemapSides[i][j].suf[0];
				pathname[len+2] = cubemapSides[i][j].suf[1];
				pathname[len+3] = 0;

				samples = R_LoadImageFromDisk( pathname, pathsize, &(pic[j]), &width, &height, j );
				if( pic[j] )
				{
					if( width != height )
					{
						Com_Printf( "Not square cubemap image %s\n", pathname );
						break;
					}
					if( !j )
					{
						lastSize = width;
					}
					else if( lastSize != width )
					{
						Com_Printf( "Different cubemap image size: %s\n", pathname );
						break;
					}
					if( cubemapSides[i][j].flags & ( IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL ) )
					{
						int flags = cubemapSides[i][j].flags;
						qbyte *temp = R_PrepareImageBuffer( TEXTURE_FLIPPING_BUF0+j, width * height * 4 );
						R_FlipTexture( pic[j], temp, width, height, 4, (flags & IT_FLIPX), (flags & IT_FLIPY), (flags & IT_FLIPDIAGONAL) );
						pic[j] = temp;
					}
					continue;
				}
				break;
			}
			if( j == 6 )
				break;
		}

		if( i != 2 )
		{
			pathname[len] = 0;
			image = R_LoadPic( pathname, pic, width, height, flags, samples );
			image->extension[0] = '.';
			Q_strncpyz( &image->extension[1], &pathname[len+4], sizeof( image->extension )-1 );
		}
	}
	else
	{
		qbyte *pic;

		samples = R_LoadImageFromDisk( pathname, pathsize, &pic, &width, &height, 0 );

		if( pic )
		{
			qbyte *temp;

			if( flags & IT_NORMALMAP )
			{
				if( (samples == 3) && glConfig.ext.GLSL )
					samples = R_MergeNormalmapDepthmap( pathname, pic, width, height );
			}
			else if( flags & IT_HEIGHTMAP )
			{
				temp = R_PrepareImageBuffer( TEXTURE_FLIPPING_BUF0, width * height * 4 );
				samples = R_HeightmapToNormalmap( pic, temp, width, height, bumpScale );
				pic = temp;
			}

			if( flags & ( IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL ) )
			{
				temp = R_PrepareImageBuffer( TEXTURE_FLIPPING_BUF0, width * height * 4 );
				R_FlipTexture( pic, temp, width, height, 4, (flags & IT_FLIPX), (flags & IT_FLIPY), (flags & IT_FLIPDIAGONAL) );
				pic = temp;
			}

			image = R_LoadPic( pathname, &pic, width, height, flags, samples );
			image->extension[0] = '.';
			Q_strncpyz( &image->extension[1], &pathname[len+1], sizeof( image->extension )-1 );
		}
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
static void R_ScreenShot( const char *name, qboolean silent )
{
	int		i;
	FILE	*f;
	qbyte	*buffer;
	char	picname[80], checkname[MAX_OSPATH];

	// create the screenshots directory if it doesn't exist
	Q_snprintfz( checkname, sizeof(checkname), "%s/screenshots", FS_Gamedir() );
	Sys_Mkdir( checkname );

	if( name && *name ) {
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
		buffer = Mem_Alloc( r_texturesPool, glState.width * glState.height * 3 );
		qglReadPixels( 0, 0, glState.width, glState.height, GL_RGB, GL_UNSIGNED_BYTE, buffer );

		if( WriteJPG( checkname, buffer, glState.width, glState.height, r_screenshot_jpeg_quality->integer ) && !silent )
			Com_Printf( "Wrote %s\n", picname );
	} else {
		buffer = Mem_Alloc( r_texturesPool, 18 + glState.width * glState.height * 3 );
		qglReadPixels( 0, 0, glState.width, glState.height, glConfig.ext.bgra ? GL_BGR_EXT : GL_RGB, GL_UNSIGNED_BYTE, buffer + 18 );

		if( WriteTGA( checkname, buffer, glState.width, glState.height, glConfig.ext.bgra ) && !silent )
			Com_Printf( "Wrote %s\n", picname );
	}

	Mem_Free( buffer );
}

/*
==================
R_ScreenShot_f
==================
*/
void R_ScreenShot_f( void )
{
	R_ScreenShot( Cmd_Argv( 1 ), Cmd_Argc() >= 2 && !Q_stricmp( Cmd_Argv( 2 ), "silent" ) );
}

/*
==================
R_EnvShot_f
==================
*/
void R_EnvShot_f( void )
{
	int i;
	int size, maxSize;
	qbyte *buffer, *bufferFlipped;
	char checkname[MAX_OSPATH];
	struct	cubemapSufAndFlip
	{
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

	if( Cmd_Argc() != 3 )
	{
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

	buffer = Mem_Alloc( r_texturesPool, (size * size * 3) * 2 + 18 );
	bufferFlipped = buffer + size * size * 3;

	for( i = 0; i < 6; i++ ) {
		R_DrawCubemapView( r_lastRefdef.vieworg, cubemapShots[i].angles, size );

		if( glConfig.ext.bgra )
			qglReadPixels( 0, glState.height - size, size, size, GL_BGR_EXT, GL_UNSIGNED_BYTE, buffer ); 
		else
			qglReadPixels( 0, glState.height - size, size, size, GL_RGB, GL_UNSIGNED_BYTE, buffer ); 

		R_FlipTexture( buffer, bufferFlipped + 18, size, size, 3, (cubemapShots[i].flags & IT_FLIPX), (cubemapShots[i].flags & IT_FLIPY), (cubemapShots[i].flags & IT_FLIPDIAGONAL) );

		Q_snprintfz( checkname, sizeof( checkname ), "%s/env/%s_%s.tga", FS_Gamedir (), Cmd_Argv( 1 ), cubemapShots[i].suf );
		WriteTGA( checkname, bufferFlipped, size, size, glConfig.ext.bgra );
	}

	ri.params &= ~RP_ENVVIEW;

	Mem_Free( buffer );
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
	r_aviBuffer = Mem_Alloc( r_texturesPool, 18 + glState.width * glState.height * 3 );
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

	if( scissor )
	{
		x = r_lastRefdef.x;
		y = glState.height - r_lastRefdef.height - r_lastRefdef.y;
		w = r_lastRefdef.width;
		h = r_lastRefdef.height;
	}
	else
	{
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
		qglReadPixels( x, y, w, h, glConfig.ext.bgra ? GL_BGR_EXT : GL_RGB, GL_UNSIGNED_BYTE, r_aviBuffer + 18 );
		WriteTGA( checkname, r_aviBuffer, w, h, glConfig.ext.bgra );
	}
}

/*
==================
R_StopAviDemo
==================
*/
void R_StopAviDemo( void )
{
	if( r_aviBuffer )
	{
		Mem_Free( r_aviBuffer );
		r_aviBuffer = NULL;
	}
}

//=======================================================

/*
==================
R_InitNoTexture
==================
*/
static qbyte *R_InitNoTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	int x, y;
	qbyte *data;
	qbyte dottexture[8][8] =
	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 1, 1, 0, 0, 0, 0 },
		{ 0, 1, 1, 1, 1, 0, 0, 0 },
		{ 0, 1, 1, 1, 1, 0, 0, 0 },
		{ 0, 0, 1, 1, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
	};

	//
	// also use this for bad textures, but without alpha
	//
	*w = *h = 8;
	*depth = 1;
	*flags = 0;
	*samples = 3;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, 8 * 8 * 4 );
	for( x = 0; x < 8; x++ )
	{
		for( y = 0; y < 8; y++ )
		{
			data[( y*8 + x )*4+0] = dottexture[x&3][y&3]*127;
			data[( y*8 + x )*4+1] = dottexture[x&3][y&3]*127;
			data[( y*8 + x )*4+2] = dottexture[x&3][y&3]*127;
		}
	}

	return data;
}

/*
==================
R_InitDynamicLightTexture
==================
*/
static qbyte *R_InitDynamicLightTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	vec3_t v = { 0, 0, 0 };
	float intensity;
	int x, y, z, d, size;
	qbyte *data;

	//
	// dynamic light texture
	//
	if( glConfig.ext.texture3D )
	{
		size = 32;
		*depth = size;
	}
	else
	{
		size = 64;
		*depth = 1;
	}

	*w = *h = size;
	*flags = IT_NOPICMIP|IT_NOMIPMAP|IT_CLAMP|IT_NOCOMPRESS;
	*samples = 3;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, size * size * *depth * 4 );
	for( x = 0; x < size; x++ )
	{
		for( y = 0; y < size; y++ )
		{
			for( z = 0; z < *depth; z++ )
			{
				v[0] = ( ( x + 0.5f ) * ( 2.0f / (float)size ) - 1.0f );
				v[1] = ( ( y + 0.5f ) * ( 2.0f / (float)size ) - 1.0f );
				if( *depth > 1 )
					v[2] = ( ( z + 0.5f ) * ( 2.0f / (float)size ) - 1.0f );

				intensity = 1.0f - sqrt( DotProduct( v, v ) );
				if( intensity > 0 )
					intensity = intensity * intensity * 215.5f;
				d = bound( 0, intensity, 255 );

				data[( ( z*size+y )*size + x ) * 4 + 0] = d;
				data[( ( z*size+y )*size + x ) * 4 + 1] = d;
				data[( ( z*size+y )*size + x ) * 4 + 2] = d;
			}
		}
	}
	return data;
}

/*
==================
R_InitSolidColorTexture
==================
*/
static qbyte *R_InitSolidColorTexture( int *w, int *h, int *depth, int *flags, int *samples, int color )
{
	qbyte *data;

	//
	// solid color texture
	//
	*w = *h = 1;
	*depth = 1;
	*flags = IT_NOPICMIP|IT_NOCOMPRESS;
	*samples = 3;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, 1 * 1 * 4 );
	data[0] = data[1] = data[2] = color;
	return data;
}

/*
==================
R_InitParticleTexture
==================
*/
static qbyte *R_InitParticleTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	int x, y;
	int dx2, dy, d;
	qbyte *data;

	//
	// particle texture
	//
	*w = *h = 16;
	*depth = 1;
	*flags = IT_NOPICMIP|IT_NOMIPMAP;
	*samples = 4;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, 16 * 16 * 4 );
	for( x = 0; x < 16; x++ )
	{
		dx2 = x - 8;
		dx2 = dx2 * dx2;

		for( y = 0; y < 16; y++ )
		{
			dy = y - 8;
			d = 255 - 35 *sqrt( dx2 + dy *dy );
			data[( y*16 + x ) * 4 + 3] = bound( 0, d, 255 );
		}
	}
	return data;
}

/*
==================
R_InitWhiteTexture
==================
*/
static qbyte *R_InitWhiteTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	return R_InitSolidColorTexture( w, h, depth, flags, samples, 255 );
}

/*
==================
R_InitBlackTexture
==================
*/
static qbyte *R_InitBlackTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	return R_InitSolidColorTexture( w, h, depth, flags, samples, 0 );
}

/*
==================
R_InitBlankBumpTexture
==================
*/
static qbyte *R_InitBlankBumpTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	qbyte *data = R_InitSolidColorTexture( w, h, depth, flags, samples, 128 );

/*
	data[2] = 128;	// normal X
	data[1] = 128;	// normal Y
*/
	data[0] = 255;	// normal Z
	data[3] = 128;	// height

	return data;
}

/*
==================
R_InitFogTexture
==================
*/
static qbyte *R_InitFogTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	qbyte *data;
	int x, y;
	double tw = 1.0f / ( (float)FOG_TEXTURE_WIDTH - 1.0f );
	double th = 1.0f / ( (float)FOG_TEXTURE_HEIGHT - 1.0f );
	double tx, ty, t;

	//
	// fog texture
	//
	*w = FOG_TEXTURE_WIDTH;
	*h = FOG_TEXTURE_HEIGHT;
	*depth = 1;
	*flags = IT_NOMIPMAP|IT_CLAMP;
	*samples = 4;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, FOG_TEXTURE_WIDTH*FOG_TEXTURE_HEIGHT*4 );
	for( y = 0, ty = 0.0f; y < FOG_TEXTURE_HEIGHT; y++, ty += th )
	{
		for( x = 0, tx = 0.0f; x < FOG_TEXTURE_WIDTH; x++, tx += tw )
		{
			t = sqrt( tx ) * 255.0;
			data[( x+y*FOG_TEXTURE_WIDTH )*4+3] = (qbyte)( min( t, 255.0 ) );
		}
		data[y*4+3] = 0;
	}
	return data;
}

/*
==================
R_InitCoronaTexture
==================
*/
static qbyte *R_InitCoronaTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	int x, y, a;
	float dx, dy;
	qbyte *data;

	//
	// light corona texture
	//
	*w = *h = 32;
	*depth = 1;
	*flags = IT_NOMIPMAP|IT_NOPICMIP|IT_NOCOMPRESS|IT_CLAMP;
	*samples = 4;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, 32 * 32 * 4 );
	for( y = 0; y < 32; y++ )
	{
		dy = ( y - 15.5f ) * ( 1.0f / 16.0f );
		for( x = 0; x < 32; x++ )
		{
			dx = ( x - 15.5f ) * ( 1.0f / 16.0f );
			a = (int)( ( ( 1.0f / ( dx * dx + dy * dy + 0.2f ) ) - ( 1.0f / ( 1.0f + 0.2 ) ) ) * 32.0f / ( 1.0f / ( 1.0f + 0.2 ) ) );
			clamp( a, 0, 255 );
			data[( y*32+x )*4+0] = data[( y*32+x )*4+1] = data[( y*32+x )*4+2] = a;
		}
	}
	return data;
}

/*
==================
R_InitScreenTexture
==================
*/
static void R_InitScreenTexture( image_t **texture, const char *name, int id, int screenWidth, int screenHeight, int size, int flags, int samples )
{
	int limit;
	int width, height;

	limit = glConfig.maxTextureSize;
	if( size )
		limit = min( limit, size );

	if( glConfig.ext.texture_non_power_of_two ) {
		width = min( screenWidth, limit );
		height = min( screenHeight, limit );
	} else {
		limit = min( limit, min( screenWidth, screenHeight ) );
		for( size = 2; size <= limit; size <<= 1 );
		width = height = size >> 1;
	}

	if( !( *texture ) || ( *texture )->width != width || ( *texture )->height != height )
	{
		qbyte *data = NULL;

		if( !*texture )
		{
			char uploadName[128];

			Q_snprintfz( uploadName, sizeof( uploadName ), "***%s%i***", name, id );
			*texture = R_LoadPic( uploadName, &data, width, height, flags, samples );
			return;
		}

		GL_Bind( 0, *texture );
		( *texture )->width = width;
		( *texture )->height = height;
		R_Upload32( &data, width, height, flags, &( ( *texture )->upload_width ), &( ( *texture )->upload_height ),
			&( ( *texture )->samples ), qfalse );
	}
}

/*
==================
R_InitPortalTexture
==================
*/
void R_InitPortalTexture( image_t **texture, int id, int screenWidth, int screenHeight )
{
	R_InitScreenTexture( texture, "r_portaltexture", id, screenWidth, screenHeight, r_portalmaps_maxtexsize->integer, IT_PORTALMAP, 3 );
}

/*
==================
R_InitShadowmapTexture
==================
*/
void R_InitShadowmapTexture( image_t **texture, int id, int screenWidth, int screenHeight )
{
	R_InitScreenTexture( texture, "r_shadowmap", id, screenWidth, screenHeight, r_shadows_maxtexsize->integer, IT_SHADOWMAP, 1 );
}

/*
==================
R_InitCinematicTexture
==================
*/
static void R_InitCinematicTexture( void )
{
	// reserve a dummy texture slot
	r_cintexture = &images[r_numImages++];
	r_cintexture->texnum = 1;
	r_cintexture->depth = 1;
}

/*
==================
R_InitBuiltinTextures
==================
*/
static void R_InitBuiltinTextures( void )
{
	qbyte *data;
	int w, h, depth, flags, samples;
	image_t *image;
	const struct
	{
		char *name;
		image_t	**image;
		qbyte *( *init )( int *w, int *h, int *depth, int *flags, int *samples );
	}
	textures[] =
	{
		{ "***r_notexture***", &r_notexture, R_InitNoTexture },
		{ "***r_whitetexture***", &r_whitetexture, R_InitWhiteTexture },
		{ "***r_blacktexture***", &r_blacktexture, R_InitBlackTexture },
		{ "***r_blankbumptexture***", &r_blankbumptexture, R_InitBlankBumpTexture },
		{ "***r_dlighttexture***", &r_dlighttexture, R_InitDynamicLightTexture },
		{ "***r_particletexture***", &r_particletexture, R_InitParticleTexture },
		{ "***r_fogtexture***", &r_fogtexture, R_InitFogTexture },
		{ "***r_coronatexture***", &r_coronatexture, R_InitCoronaTexture },

		{ NULL, NULL, NULL }
	};
	size_t i, num_builtin_textures = sizeof( textures ) / sizeof( textures[0] ) - 1;

	for( i = 0; i < num_builtin_textures; i++ )
	{
		data = textures[i].init( &w, &h, &depth, &flags, &samples );
		assert( data );

		image = ( depth == 1 ?
			R_LoadPic( textures[i].name, &data, w, h, flags, samples ) :
		R_Load3DPic( textures[i].name, &data, w, h, depth, flags, samples )
			);

		if( textures[i].image )
			*( textures[i].image ) = image;
	}

	r_portaltexture = NULL;
	r_portaltexture2 = NULL;
}

//=======================================================

/*
===============
R_InitImages
===============
*/
void R_InitImages( void )
{
	r_texturesPool = Mem_AllocPool( NULL, "Textures" );
	image_cur_hash = IMAGES_HASH_SIZE+1;

	r_imagePathBuf = r_imagePathBuf2 = NULL;
	r_sizeof_imagePathBuf = r_sizeof_imagePathBuf2 = 0;

	R_InitCinematicTexture();
	R_InitBuiltinTextures();
	R_InitBloomTextures();
}

/*
===============
R_ShutdownImages
===============
*/
void R_ShutdownImages( void )
{
	int i;

	if( !r_texturesPool )
		return;

	R_StopAviDemo ();

	R_FreeImageBuffers ();

	for( i = 0; i < r_numImages; i++ )
		Mem_Free( images[i].name );

	if( r_imagePathBuf )
		Mem_Free( r_imagePathBuf );
	if( r_imagePathBuf2 )
		Mem_Free( r_imagePathBuf2 );

	Mem_FreePool( &r_texturesPool );

	r_portaltexture = NULL;
	r_portaltexture2 = NULL;
	memset( r_shadowmapTextures, 0, sizeof( image_t * ) * MAX_SHADOWGROUPS );

	r_imagePathBuf = r_imagePathBuf2 = NULL;
	r_sizeof_imagePathBuf = r_sizeof_imagePathBuf2 = 0;

	r_numImages = 0;
	memset( images, 0, sizeof( images ) );
	memset( r_lightmapTextures, 0, sizeof( r_lightmapTextures ) );
	memset( images_hash, 0, sizeof( images_hash ) );
}
