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

image_t		gltextures[MAX_GLTEXTURES];
int			numgltextures;

int			gl_maxtexsize;

unsigned	d_8to24table[256];

void GL_Upload8 ( byte *data, int width, int height, int flags );
void GL_Upload32 ( unsigned *data, int width, int height, int flags );

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;


byte	default_pal[] = 
{
#include "def_pal.dat"
};


void GL_EnableMultitexture( qboolean enable )
{
	if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
		return;

	GL_SelectTexture( GL_TEXTURE_1 );

	if ( enable )
		qglEnable( GL_TEXTURE_2D );
	else
		qglDisable( GL_TEXTURE_2D );

	GL_TexEnv( GL_MODULATE );
	GL_SelectTexture( GL_TEXTURE_0 );
	GL_TexEnv( GL_MODULATE );
}

void GL_SelectTexture( GLenum texture )
{
	int tmu;

	if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
		return;

	tmu = texture-GL_TEXTURE_0;

	if ( tmu == gl_state.currenttmu )
	{
		return;
	}

	gl_state.currenttmu = tmu;

	if ( qglSelectTextureSGIS )
	{
		qglSelectTextureSGIS( texture );
	}
	else if ( qglActiveTextureARB )
	{
		qglActiveTextureARB( texture );
		qglClientActiveTextureARB( texture );
	}
}

void GL_TexEnv( GLenum mode )
{
	static int lastmodes[2] = { -1, -1 };

	if ( mode != lastmodes[gl_state.currenttmu] )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
		lastmodes[gl_state.currenttmu] = mode;
	}
}

void GL_Bind (int texnum)
{
	if (r_nobind->value)		// performance evaluation option
		texnum = r_notexture->texnum;
	if ( gl_state.currenttextures[gl_state.currenttmu] == texnum)
		return;
	gl_state.currenttextures[gl_state.currenttmu] = texnum;
	qglBindTexture (GL_TEXTURE_2D, texnum);
}

void GL_MBind( GLenum target, int texnum )
{
	GL_SelectTexture( target );

	if ( gl_state.currenttextures[texnum-GL_TEXTURE_0] == texnum )
		return;

	GL_Bind( texnum );
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
GL_TextureMode
===============
*/
void GL_TextureMode( char *string )
{
	int		i;
	image_t	*glt;

	for (i=0 ; i< NUM_GL_MODES ; i++)
	{
		if ( !Q_stricmp( modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_MODES)
	{
		Com_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if ( !(glt->flags & IT_NOMIPMAP) )
		{
			GL_Bind (glt->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
GL_ImageList_f
===============
*/
void	GL_ImageList_f (void)
{
	int		i;
	image_t	*image;
	int		texels;

	Com_Printf ("------------------\n");
	texels = 0;

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (image->texnum <= 0)
			continue;
		texels += image->upload_width*image->upload_height;

		Com_Printf (" %3i %3i: %s\n",
			image->upload_width, image->upload_height, image->name);
	}

	Com_Printf ("Total texel count (not counting mipmaps): %i\n", texels);
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
void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;

	*pic = NULL;
	*palette = NULL;

	//
	// load the file
	//
	len = FS_LoadFile (filename, (void **)&raw);
	if (!raw)
	{
		Com_DPrintf ("Bad pcx file %s\n", filename);
		return;
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
		Com_DPrintf (PRINT_ALL, "Bad pcx file %s\n", filename);
		FS_FreeFile (pcx);
		return;
	}

	out = Q_malloc ( (pcx->ymax+1) * (pcx->xmax+1) );

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = Q_malloc(768);
		memcpy (*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
		*width = pcx->xmax+1;
	if (height)
		*height = pcx->ymax+1;

	for (y=0 ; y<=pcx->ymax ; y++, pix += pcx->xmax+1)
	{
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if ( raw - (byte *)pcx > len)
	{
		Com_DPrintf ("PCX file %s was malformed", filename);
		Q_free (*pic);
		*pic = NULL;
	}

	FS_FreeFile (pcx);
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
void LoadTGA (char *name, byte **pic, int *width, int *height)
{
	int		columns, rows, numPixels, row_inc;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	int		length;
	TargaHeader		targa_header;
	byte			*targa_rgba;
	byte tmp[2];

	*pic = NULL;

	//
	// load the file
	//
	length = FS_LoadFile (name, (void **)&buffer);
	if (!buffer)
	{
		Com_DPrintf ("Bad tga file %s\n", name);
		return;
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

	if (targa_header.image_type != 2 
		&& targa_header.image_type != 10
		&& targa_header.image_type != 3) 
		Com_Error (ERR_DROP, "LoadTGA: Only type 2, 3 and 10 targa images supported");

	if (targa_header.image_type != 3 
		&& (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
		Com_Error (ERR_DROP, "LoadTGA: Only 32 or 24 bit images supported (no colormaps)");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = Q_malloc (numPixels*4);
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  // skip TARGA image comment

    // If bit 5 of attributes isn't set, the image has been stored from bottom to top
    if ((targa_header.attributes & 0x20) == 0)
    {
        pixbuf = targa_rgba + (rows - 1)*columns*4;
        row_inc = -columns*4*2;
	}
    else
    {
        pixbuf = targa_rgba;
        row_inc = 0;
    }

	if (targa_header.image_type == 2 || targa_header.image_type == 3) {  
		// Uncompressed, RGB images or greyscale
		for (row=0; row<rows; row++) {
			for(column=0; column<columns; column++) {
				unsigned char red, green, blue, alphabyte;
				switch (targa_header.pixel_size) {
					case 8:
						blue = *buf_p++;
						green = red = blue;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
						break;

					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
						break;
					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						break;
				}
			}

			pixbuf += row_inc;
		}
	}
	else if (targa_header.image_type == 10) {   // Runlength encoded RGB images
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;
		for(row=0; row<rows; row++) {
			for(column=0; column<columns; ) {
				packetHeader = *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        // run-length packet
					switch (targa_header.pixel_size) {
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = 255;
							break;
						case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							break;
					}
	
					for(j=0;j<packetSize;j++) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if (column == columns) { // run spans across rows
							column = 0;
							if (row < rows-1)
								row++;
							else
								goto breakOut;
							pixbuf += row_inc;
						}
					}
				}
				else {                            // non run-length packet
					for(j=0;j<packetSize;j++) {
						switch (targa_header.pixel_size) {
							case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = 255;
								break;
							case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alphabyte;
								break;
						}
						column++;
						if (column == columns) { // pixel packet run spans across rows
							column = 0;
							if (row < rows-1)
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

	FS_FreeFile (buffer);
}

static void jpg_noop(j_decompress_ptr cinfo)
{
}

static boolean jpg_fill_input_buffer(j_decompress_ptr cinfo)
{
    Com_DPrintf ("Premeture end of jpeg file\n");
    return true;
}

static void jpg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;

    if (cinfo->src->bytes_in_buffer < 0)
		Com_DPrintf ("Premeture end of jpeg file\n");
}

static void jpeg_mem_src(j_decompress_ptr cinfo, byte *mem, int len)
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

void LoadJPG (char *name, byte **pic, int *width, int *height)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    byte *img, *c, *buffer, *dummy;
    int i, length, b;

	*pic = NULL;

	//
	// load the file
	//
	length = FS_LoadFile (name, (void **)&buffer);
	if (!buffer)
	{
		Com_DPrintf ("Bad jpeg file %s\n", name);
		return;
	}
	
    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_decompress (&cinfo);
    jpeg_mem_src (&cinfo, buffer, length);
    jpeg_read_header (&cinfo, TRUE);
    jpeg_start_decompress (&cinfo);

    if (cinfo.output_components != 3 && cinfo.output_components != 1)
	{
		jpeg_destroy_decompress (&cinfo);
		FS_FreeFile (buffer);
		return;
	}

	img = *pic = Q_malloc (cinfo.output_width * cinfo.output_height * 4);

	if ( cinfo.output_components == 1 ) {
		dummy = c = Q_malloc (cinfo.output_width * cinfo.output_height);
		while (cinfo.output_scanline < cinfo.output_height)
		{
			jpeg_read_scanlines (&cinfo, &c, 1);
			for (i = 0; i < cinfo.output_width; i++, img+=4)
			{
				b = *c++;

				img[0] = b;
				img[1] = b;
				img[2] = b;
				img[3] = 255;
			}
		}
	} else {
		dummy = c = Q_malloc (cinfo.output_width * cinfo.output_height * 3);
		while (cinfo.output_scanline < cinfo.output_height)
		{
			jpeg_read_scanlines (&cinfo, &c, 1);
			for (i = 0; i < cinfo.output_width; i++, c += 3, img+=4)
			{
				img[0] = c[0];
				img[1] = c[1];
				img[2] = c[2];
				img[3] = 255;
			}
		}
	}

	if (width)
		*width = cinfo.output_width;
	if (height)
	    *height = cinfo.output_height;

    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);

    Q_free (dummy);
	FS_FreeFile (buffer);
}

//=======================================================

void R_ResampleTextureLerpLine (byte *in, byte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx;

	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);

	for (j = 0, f = 0; j < outwidth; j++, f += fstep)
	{
		xi = (int) f >> 16;

		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}

		if (xi < endx)
		{
			int lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (byte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *indata, int inwidth, int inheight, unsigned *outdata, int outwidth, int outheight)
{
	int		i, j, yi, oldy, f, fstep, endy = inheight - 1;
	byte	*inrow, *out, *row1, *row2;
	out = (byte *) outdata;
	fstep = (int) (inheight*65536.0f/outheight);
	
	row1 = Q_malloc (outwidth*4);
	row2 = Q_malloc (outwidth*4);
	inrow = (byte *) indata;
	oldy = 0;

	R_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
	R_ResampleTextureLerpLine (inrow + inwidth*4, row2, inwidth, outwidth);

	for (i = 0, f = 0; i < outheight; i++, f += fstep)
	{
		yi = f >> 16;

		if (yi < endy)
		{
			int lerp = f & 0xFFFF;

			if (yi != oldy)
			{
				inrow = (byte *)indata + inwidth*4*yi;

				if (yi == oldy+1)
					memcpy (row1, row2, outwidth*4);
				else
					R_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);

				R_ResampleTextureLerpLine (inrow + inwidth*4, row2, inwidth, outwidth);
				oldy = yi;
			}

			for (j = outwidth; j; j--)
			{
				out[0] = (byte) ((((row2[0] - row1[0]) * lerp) >> 16) + row1[0]);
				out[1] = (byte) ((((row2[1] - row1[1]) * lerp) >> 16) + row1[1]);
				out[2] = (byte) ((((row2[2] - row1[2]) * lerp) >> 16) + row1[2]);
				out[3] = (byte) ((((row2[3] - row1[3]) * lerp) >> 16) + row1[3]);
				out += 4;
				row1 += 4;
				row2 += 4;
			}

			row1 -= outwidth*4;
			row2 -= outwidth*4;
		}
		else
		{
			if (yi != oldy)
			{
				inrow = (byte *)indata + inwidth*4*yi;

				if (yi == oldy+1)
					memcpy (row1, row2, outwidth*4);
				else
					R_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);

				oldy = yi;
			}

			memcpy (out, row1, outwidth * 4);
		}
	}

	Q_free (row1);
	Q_free (row2);
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
===============
GL_Upload32
===============
*/
int		upload_width, upload_height;
void GL_Upload32 (unsigned *data, int width, int height, int flags)
{
	int			samples, bits;
	unsigned	*scaled;
	int			scaled_width, scaled_height;
	int			i, c;
	byte		*scan;
	int			comp;

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	if ( flags & IT_SKY ) 
	{	// let people sample down the sky textures for speed
		scaled_width >>= (int)r_skymip->value;
		scaled_height >>= (int)r_skymip->value;
	} 
	else if ( !(flags & IT_NOPICMIP) )
	{	// let people sample down the world textures for speed
		scaled_width >>= (int)r_picmip->value;
		scaled_height >>= (int)r_picmip->value;
	}

	// don't ever bother with > gl_maxtexsize textures
	clamp ( scaled_width, 1, gl_maxtexsize );
	clamp ( scaled_height, 1, gl_maxtexsize );

	upload_width = scaled_width;
	upload_height = scaled_height;

	if ( flags & IT_CINEMATICS ) {
		return;
	}

	if (scaled_width * scaled_height > gl_maxtexsize*gl_maxtexsize)
		Com_Error (ERR_DROP, "GL_Upload32: too big");

	scaled = (unsigned *)Q_malloc(scaled_width*scaled_height*4);

	// scan the texture for any non-255 alpha
	c = width*height;
	scan = ((byte *)data) + 3;
	samples = 3;
	for (i=0 ; i<c ; i++, scan += 4)
	{
		if ( *scan != 255 )
		{
			samples = 4;
			break;
		}
	}

	if ( gl_config.compressed_textures )  {
		if ( samples == 3 ) {
			comp = GL_COMPRESSED_RGB_ARB;
		} else if ( samples == 4 ) {
			comp = GL_COMPRESSED_RGBA_ARB;
		}
	} else {
		bits = (int)r_texturebits->value;

		if ( samples == 3 ) {
			switch ( bits ) {
				case 16:
					comp = GL_RGB5;
					break;
				case 32:
					comp = GL_RGB8;
					break;
				default:
					comp = GL_RGB;
					break;
			}
		} else if ( samples == 4 ) {
			switch ( bits ) {
				case 16:
					comp = GL_RGBA4;
					break;
				case 32:
					comp = GL_RGBA8;
					break;
				default:
					comp = GL_RGBA;
					break;
			}
		}
	}

	if (scaled_width == width && scaled_height == height)
	{
		if (flags & IT_NOMIPMAP)
		{
			qglTexImage2D (GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}

		memcpy (scaled, data, width*height*4);
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

	if ( !(flags & IT_NOMIPMAP) && gl_config.sgis_mipmap ) {
		qglTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, true );
		qglTexImage2D( GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled );
	} else {
		qglTexImage2D( GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled );

		if (!(flags & IT_NOMIPMAP))
		{
			int		miplevel;

			miplevel = 0;
			while (scaled_width > 1 || scaled_height > 1)
			{
				GL_MipMap ((byte *)scaled, scaled_width, scaled_height);
				scaled_width >>= 1;
				scaled_height >>= 1;
				if (scaled_width < 1)
					scaled_width = 1;
				if (scaled_height < 1)
					scaled_height = 1;
				miplevel++;
				qglTexImage2D (GL_TEXTURE_2D, miplevel, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
			}
		}
	}

done: ;

	if (!(flags & IT_NOMIPMAP))
	{
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	if (flags & IT_CLAMP)
	{
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
	else
	{
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	Q_free ( scaled );
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (byte *data, int width, int height, int flags )
{
	unsigned	trans[512*256];
	int			i, s;
	int			p;

	s = width*height;

	if (s > sizeof(trans)/4)
		Com_Error (ERR_DROP, "GL_Upload8: too large");

	for (i=0 ; i<s ; i++)
	{
		p = data[i];
		trans[i] = d_8to24table[p];
		
		if (p == 255)
		{	// transparent, so scan around for another color
			// to avoid alpha fringes
			// FIXME: do a full flood fill so mips work...
			if (i > width && data[i-width] != 255)
				p = data[i-width];
			else if (i < s-width && data[i+width] != 255)
				p = data[i+width];
			else if (i > 0 && data[i-1] != 255)
				p = data[i-1];
			else if (i < s-1 && data[i+1] != 255)
				p = data[i+1];
			else
				p = 0;

			// copy rgb components
			((byte *)&trans[i])[0] = ((byte *)&d_8to24table[p])[0];
			((byte *)&trans[i])[1] = ((byte *)&d_8to24table[p])[1];
			((byte *)&trans[i])[2] = ((byte *)&d_8to24table[p])[2];
		}
	}
	
	GL_Upload32 (trans, width, height, flags);
}


/*
================
GL_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *GL_LoadPic (char *name, byte *pic, int width, int height, int flags, int bits)
{
	image_t		*image = NULL;
	int			i;

	// find a free image_t
	for (i=0, image=gltextures ; i<numgltextures ; i++,image++)
	{
		if (!image->texnum)
			break;
	}

	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
			Com_Error (ERR_DROP, "MAX_GLTEXTURES");
		numgltextures++;
	}

	if (strlen(name) >= sizeof(image->name))
		Com_Error (ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);

	strcpy (image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->flags = flags;
	image->texnum = TEXNUM_IMAGES + (image - gltextures);

	GL_Bind (image->texnum);

	if ( !(flags & IT_FOG) ) {
		if (bits == 8)
			GL_Upload8 ( pic, width, height, flags );
		else
			GL_Upload32 ((unsigned *)pic, width, height, flags );

		image->upload_width = upload_width;		// after power of 2 and scales
		image->upload_height = upload_height;
	} else {
		qglTexImage2D (GL_TEXTURE_2D, 0, GL_ALPHA, width, height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, pic);
		
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		image->upload_width = width;
		image->upload_height = height;
	}

	return image;
}

/*
===============
GL_FindImage

Finds or loads the given image
===============
*/
image_t	*GL_FindImage (char *name, int flags)
{
	image_t	*image;
	int		i, len;

	if (!name)
		return NULL;	//	Com_Error (ERR_DROP, "GL_FindImage: NULL name");
	len = strlen(name);
	if (len < 5)
		return NULL;	//	Com_Error (ERR_DROP, "GL_FindImage: bad name: %s", name);

	// look for it
	for (i=0, image=gltextures ; i<numgltextures ; i++,image++)
	{
		if (!strcmp(name, image->name) && (image->flags == flags))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	//
	// load the pic from disk
	//
	return GL_LoadImage ( name, flags );
}


/*
===============
GL_LoadImage

Tries to load images in this sequence: .jpg->.tga->.pcx.
===============
*/
image_t	*GL_LoadImage (char *name, int flags)
{
	image_t	*image;
	int		i, len;
	byte	*pic, *palette;
	int		width, height;
	char	tempname[MAX_QPATH-4], newname[MAX_QPATH];

	len = strlen(name);

	//
	// load the pic from disk
	//
	pic = NULL;
	palette = NULL;
	image = NULL;

	if ( name[0] == '/' || name[0] == '\\' ) {
		strcpy ( tempname, &name[1] );

		if ( name[len-4] == '.' )
			tempname[len-4-1] = 0;
	} else {
		strcpy ( tempname, name );
	
		if ( name[len-4] == '.' )
			tempname[len-4] = 0;
	}
	
	// replace all '\' with '/'
	for ( i = 0; tempname[i]; i++ ) {
		if ( tempname[i] == '\\' ) 
			tempname[i] = '/';
	}

	Com_sprintf ( newname, MAX_QPATH, "%s.jpg", tempname );
	LoadJPG ( newname, &pic, &width, &height );

	if (pic) {
		image = GL_LoadPic ( name, pic, width, height, flags, 32 );
		goto freetrash;
	}

	Com_sprintf ( newname, MAX_QPATH, "%s.tga", tempname );
	LoadTGA ( newname, &pic, &width, &height );

	if (pic) {
		image = GL_LoadPic ( name, pic, width, height, flags, 32 );
		goto freetrash;
	}

	Com_sprintf ( newname, MAX_QPATH, "%s.pcx", tempname );
	LoadPCX ( newname, &pic, &palette, &width, &height );

	if (pic) {
		image = GL_LoadPic ( name, pic, width, height, flags, 8 );
	}

freetrash:
	Q_free (pic);
	Q_free (palette);

	return image;
}

/*
================
GL_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void GL_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	// never free r_notexture, particle or white texture
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;
	r_whitetexture->registration_sequence = registration_sequence; 
	r_dlighttexture->registration_sequence = registration_sequence; 
	r_fogtexture->registration_sequence = registration_sequence; 

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue;		// used this sequence
		if (!image->registration_sequence)
			continue;		// free image_t slot
		// free it
		qglDeleteTextures (1, &image->texnum);
		memset (image, 0, sizeof(*image));
	}
}


/*
===============
GL_GetPalette
===============
*/
void GL_GetPalette (void)
{
	int		i;
	int		r, g, b;
	unsigned	v;
	byte	*pic, *pal;
	int		width, height;

	// get the palette

	LoadPCX ("pics/colormap.pcx", &pic, &pal, &width, &height);
	if (!pal)
		pal = default_pal;

	for (i=0 ; i<256 ; i++)
	{
		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);

		d_8to24table[i] = LittleLong(v);
	}

	d_8to24table[255] &= LittleLong(0xffffff);	// 255 is transparent
}


/*
===============
GL_InitImages
===============
*/
void GL_InitImages (void)
{
	registration_sequence = 1;

	GL_GetPalette ();

	qglGetIntegerv (GL_MAX_TEXTURE_SIZE, &gl_maxtexsize);
}

/*
===============
GL_ShutdownImages
===============
*/
void GL_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free image_t slot
		// free it
		qglDeleteTextures (1, &image->texnum);
		memset (image, 0, sizeof(*image));
	}
}

