/*
 * Copyright (C) 2004 Claudio "KLaN" Ciccani <klan82@cheapnet.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>

#include <direct/interface.h>
#include <direct/messages.h>
#include <direct/mem.h>

#include <misc/util.h>



static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer );

static DFBResult
IDirectFBImageProvider_PNM_AddRef( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_PNM_Release( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_PNM_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *dest,
                                     const DFBRectangle     *dest_rect );

static DFBResult
IDirectFBImageProvider_PNM_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *ctx );

static DFBResult
IDirectFBImageProvider_PNM_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *desc );

static DFBResult
IDirectFBImageProvider_PNM_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *desc );



#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, PNM )


typedef struct __IDirectFBImageProvider_PNM_data IDirectFBImageProvider_PNM_data;

typedef enum
{
	PHDR_MAGIC,
	PHDR_WIDTH,
	PHDR_HEIGHT,
	PHDR_COLORS

} PHeader;

typedef enum
{
	PFMT_PBM = 0,
	PFMT_PGM = 1,
	PFMT_PPM = 2

} PFormat;

typedef enum
{
	PIMG_RAW   = 0,
	PIMG_PLAIN = 1

} PImgType;


typedef DFBResult (*PRowCallback) ( IDirectFBImageProvider_PNM_data *data,
		                    char                            *dest );

typedef struct
{
	DFBSurfacePixelFormat  fmt;
	
	struct
	{
		PRowCallback   rowcb;
		int            chunksize;
		
	} img[2];

} PFormatData;

struct __IDirectFBImageProvider_PNM_data
{
	int                    ref;
	IDirectFBDataBuffer   *buffer;

	PFormat                format;
	PImgType               type;
	CoreSurface           *img;
	int                    width;
	int                    height;
	int                    colors;
	DFBSurfacePixelFormat  pixel;

	PRowCallback           getrow;
	__u8                  *rowbuf;     /* buffer for ascii images */
	int                    bufp;       /* current position in buffer */
	int                    chunksize;  /* maximum size of each sample */

	DIRenderCallback       render_callback;
        void                  *render_callback_ctx;
};





#define P_GET( buf, n ) \
{\
	err = data->buffer->WaitForDataWithTimeout( data->buffer, n, 5, 0 );\
	if(err == DFB_TIMEOUT)\
	{\
		D_ERROR( "DirectFB/ImageProvider_PNM: reached timeout while" \
			 " waiting for %i bytes.\n", n );\
		return( DFB_TIMEOUT );\
	}\
	err = data->buffer->GetData( data->buffer, n, buf, &len );\
	if(err != DFB_OK || len < 1)\
	{\
		D_ERROR( "DirectFB/ImageProvider_PNM: couldn't get %i bytes" \
			 " from data buffer...\n\t-> %s\n", n,\
			 DirectFBErrorString( err ) );\
		return( err );\
	}\
}

#define P_LOADBUF() \
{\
	int size = data->chunksize * data->width;\
	if(data->bufp)\
	{\
		size -= data->bufp;\
		memset( data->rowbuf + data->bufp, 0, size + 1 );\
		P_GET( data->rowbuf + data->bufp, size );\
		len += data->bufp;\
		data->bufp = 0;\
	} else\
	{\
		memset( data->rowbuf, 0, size + 1 );\
		P_GET( data->rowbuf, size );\
	}\
}

#define P_STOREBUF() \
{\
	int size = data->chunksize * data->width;\
	if(i++ < len && i < size)\
	{\
		size -= i;\
		memcpy( data->rowbuf, data->rowbuf + i, size );\
		data->bufp = size;\
	}\
}




static DFBResult
__rawpbm_getrow( IDirectFBImageProvider_PNM_data *data,
		 char                            *dest )
{
	DFBResult  err;
	int        len;
	int        i, s;
	__u16     *d    = (__u16*) dest;

	P_GET( dest, data->width / 8 );

	dest += (len - 1);

	/* start from end */
	for(i = (len * 8), s = 0; --i >= 0; )
	{
		d[i] = (*dest & (1 << s)) 
			? 0x0000  /* alpha:0, color:black */
			: 0xffff; /* alpha:1, color:white */
		
		if(++s > 7)
		{
			s = 0;
			dest--;
		}
	}

	return( DFB_OK );
}


static DFBResult
__rawpgm_getrow( IDirectFBImageProvider_PNM_data *data,
                 char                            *dest )
{
	DFBResult  err;
	int        len;
	__u8      *d;
	__u8       b;

	P_GET( dest, data->width );

	d = (__u8*) dest + ((len - 1) * 3);

	/* start from end */
	while(--len >= 0)
	{
		b = dest[len];

		*d       = b;
		*(d - 1) = b;
		*(d - 2) = b;

		d -= 3;
	}

	return( DFB_OK );
}


static DFBResult
__rawppm_getrow( IDirectFBImageProvider_PNM_data *data,
                 char                            *dest )
{
	DFBResult err;
	int       len;
	int       i;
	__u8      r;
	__u8      b;

	P_GET( dest, data->width * 3 );

	for(i = (len / 3); i--; )
	{
		r = *dest;
		b = *(dest + 2);

		*dest       = b;
		*(dest + 2) = r;

		dest += 3;
	}

	return( DFB_OK );
}


static DFBResult
__plainpbm_getrow( IDirectFBImageProvider_PNM_data *data,
		   char                            *dest )
{
	DFBResult  err;
	int        len;
	int        i;
	int        w    = data->width;
	__u8      *buf  = data->rowbuf;
	__u16     *d    = (__u16*) dest;

	P_LOADBUF();

	for(i = 0; i < len; i++)
	{
		if(buf[i] == 0)
			break;
		
		switch(buf[i])
		{
			case '0':
				*d++ = 0xffff; /* alpha:1, color:white */
			break;

			case '1':
				*d++ = 0x0000; /* alpha:0, color:black */
			break;

			default:
			continue;
		}

		/* assume next char is a space */
		i++;
		if(!--w)
			break;
	}

	P_STOREBUF();

	return( DFB_OK );
}


static DFBResult
__plainpgm_getrow( IDirectFBImageProvider_PNM_data *data,
		   char                            *dest )
{
	DFBResult  err;
	int        len;
	int        i, n;
	int        w    = data->width;
	__u8      *buf  = data->rowbuf;

	P_LOADBUF();

	for(i = 0, n = 0; i < len; i++)
	{
		if(buf[i] == 0)
			break;

		if(buf[i] < '0' || buf[i] > '9')
		{
			n = 0;
			continue;
		}

		n *= 10;
		n += buf[i] - '0';

		if(isspace( buf[i + 1] ))
		{
			*dest       = n;
			*(dest + 1) = n;
			*(dest + 2) = n;
			
			dest += 3;
			n     = 0;
			i++;
			
			if(!--w)
				break;
		}
	}

	P_STOREBUF();

	return( DFB_OK );
}


static DFBResult
__plainppm_getrow( IDirectFBImageProvider_PNM_data *data,
                   char                            *dest )
{
	DFBResult  err;
	int        len;
	int        i, n;
	int        j    = 2;
	int        w    = data->width;
	__u8      *buf  = data->rowbuf;

	P_LOADBUF();

	for(i = 0, n = 0; i < len; i++)
	{
		if(buf[i] == 0)
			break;

		if(buf[i] < '0' || buf[i] > '9')
		{
			n = 0;
			continue;
		}

		n *= 10;
		n += buf[i] - '0';
		
		if(isspace( buf[i + 1] ))
		{
			*(dest + j) = n;
			
			n = 0;
			i++;
			
			if(--j < 0)
			{
				j     = 2;
				dest += 3;
				if(!--w)
					break;
			}
		}
	}

	P_STOREBUF();

	return( DFB_OK );
}




static const PFormatData p_db[] =
{
	{ DSPF_ARGB1555, { {__rawpbm_getrow, 0}, {__plainpbm_getrow,  2} } }, /* PBM */
	{    DSPF_RGB24, { {__rawpgm_getrow, 0}, {__plainpgm_getrow,  4} } }, /* PGM */
	{    DSPF_RGB24, { {__rawppm_getrow, 0}, {__plainppm_getrow, 12} } }  /* PPM */
};



static DFBResult
p_getheader( IDirectFBImageProvider_PNM_data *data,
             char                            *to,
	     int                              size )
{
	DFBResult err;
	int       len;

	while(size--)
	{
		P_GET( to, 1 );

		if(*to == '#')
		{
			char c = 0;

			*to = 0;

			while(c != '\n')
				P_GET( &c, 1 );

			return( DFB_OK );
		} else
		if(isspace(*to))
		{
			*to = 0;
			return( DFB_OK );
		}

		to++;
	}

	return( DFB_OK );
}


static DFBResult
p_init( IDirectFBImageProvider_PNM_data *data )
{
	DFBResult err;
	PHeader   header  = PHDR_MAGIC;
	char      buf[33];

	memset( buf, 0, 33 );

	while((err = p_getheader( data, &buf[0], 32 )) == DFB_OK)
	{
		if(buf[0] == 0)
			continue;

		switch(header)
		{
			case PHDR_MAGIC:
			{
				if(buf[0] != 'P')
					return( DFB_UNSUPPORTED );

				switch(buf[1])
				{
					case '1':
					case '4':
						data->format = PFMT_PBM;
					break;
					
					case '2':
					case '5':
						data->format = PFMT_PGM;
					break;

					case '3':
					case '6':
						data->format = PFMT_PPM;
					break;

					default:
					return( DFB_UNSUPPORTED );
				}

				data->type = (buf[1] > '3') ? PIMG_RAW : PIMG_PLAIN;

				data->pixel     = p_db[data->format].fmt;
				data->getrow    = p_db[data->format].img[data->type].rowcb;
				data->chunksize = p_db[data->format].img[data->type].chunksize;

				header = PHDR_WIDTH;
			}
			break;

			case PHDR_WIDTH:
			{
				data->width = strtol( buf, NULL, 10 );

				if(data->width < 1)
					return( DFB_UNSUPPORTED );

				if(data->format == PFMT_PBM && data->width & 7)
				{
					D_ERROR( "DirectFB/ImageProvider_PNM: "
						 "PBM width must be a multiple of 8.\n" );
					return( DFB_UNIMPLEMENTED );
				}
				
				header = PHDR_HEIGHT;
			}
			break;

			case PHDR_HEIGHT:
			{
				data->height = strtol( buf, NULL, 10 );

				if(data->height < 1)
					return( DFB_UNSUPPORTED );

				if(data->format == PFMT_PBM)
					return( DFB_OK );
				
				header = PHDR_COLORS;
			}
			break;

			case PHDR_COLORS:
			{
				data->colors = strtoul( buf, NULL, 10 );

				if(data->colors < 1)
					return( DFB_UNSUPPORTED );
					
				if(data->colors > 0xff)
				{
					D_ERROR( "DirectFB/ImageProvider_PNM: "
						 "2-bytes samples are not supported.\n" );
					return( DFB_UNIMPLEMENTED );
				}

				return( DFB_OK );
			}
		}
	}
	
	return( err );
}



static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
	if(ctx->header[0] == 'P')
	{
		if(ctx->header[1] < '1' || ctx->header[1] > '6')
			return( DFB_UNSUPPORTED );

		if(!isspace(ctx->header[2]))
			return( DFB_UNSUPPORTED );

		return( DFB_OK );
	}

	return( DFB_UNSUPPORTED );
}


static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
#if !(DIRECT_BUILD_NOTEXT)
	static const char* format_names[] = 
	{
		"PBM", "PGM", "PPM"
	};
#endif
	DFBResult err;

	DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBImageProvider_PNM )

	data->ref    = 1;
	data->buffer = buffer;

	buffer->AddRef( buffer );

	err = p_init( data );
	if(err != DFB_OK)
	{
		buffer->Release( buffer );
		DIRECT_DEALLOCATE_INTERFACE( thiz );
		return( err );
	}

	D_DEBUG( "DirectFB/ImageProvider_PNM: found %s %s %ix%i.\n",
			(data->type == PIMG_RAW) ? "Raw" : "Plain",
			format_names[data->format], data->width, data->height );

	thiz->AddRef                = IDirectFBImageProvider_PNM_AddRef;
	thiz->Release               = IDirectFBImageProvider_PNM_Release;
	thiz->RenderTo              = IDirectFBImageProvider_PNM_RenderTo;
	thiz->SetRenderCallback     = IDirectFBImageProvider_PNM_SetRenderCallback;
	thiz->GetImageDescription   = IDirectFBImageProvider_PNM_GetImageDescription;
	thiz->GetSurfaceDescription = IDirectFBImageProvider_PNM_GetSurfaceDescription;

	return( DFB_OK );
}


static void
IDirectFBImageProvider_PNM_Destruct( IDirectFBImageProvider *thiz )
{
	IDirectFBImageProvider_PNM_data *data;

	data = (IDirectFBImageProvider_PNM_data*) thiz->priv;

	if(data->img)
		dfb_surface_unref( data->img );
	
	if(data->buffer)
		data->buffer->Release( data->buffer );

	DIRECT_DEALLOCATE_INTERFACE( thiz );
}


static DFBResult
IDirectFBImageProvider_PNM_AddRef( IDirectFBImageProvider *thiz )
{
	DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

	data->ref++;

	return( DFB_OK );
}


static DFBResult
IDirectFBImageProvider_PNM_Release( IDirectFBImageProvider *thiz )
{
	DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

	if(--data->ref == 0)
		 IDirectFBImageProvider_PNM_Destruct( thiz );

	return( DFB_OK );
}


static DFBResult
IDirectFBImageProvider_PNM_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *dest,
                                     const DFBRectangle     *dest_rect )
{
	DFBResult              err      = DFB_OK;
	IDirectFBSurface_data *dst_data = NULL;
	DFBRectangle           d, s     = {0, 0, 0, 0};
	CardState              state;

	DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

	if(!dest)
		return( DFB_INVARG );

	dst_data = (IDirectFBSurface_data*) dest->priv;

	if(!dst_data || !dst_data->surface)
		return( DFB_DEAD );

	d = dst_data->area.wanted;

	if(dest_rect)
	{
		if(dest_rect->w < 1 || dest_rect->h < 1)
			return( DFB_INVARG );

		d = *dest_rect;

		d.x += dst_data->area.wanted.x;
		d.y += dst_data->area.wanted.y;
	}
	
	{
		DFBRectangle drect = d;

		if(!dfb_rectangle_intersect( &drect, &dst_data->area.current ))
			return( DFB_INVARG );

		memset( &state, 0, sizeof(CardState) );
		
		state.source      = data->img;
		state.destination = dst_data->surface;
		state.clip.x1     = drect.x;
		state.clip.x2     = drect.x + drect.w - 1;
		state.clip.y1     = drect.y;
		state.clip.y2     = drect.y + drect.h - 1;
		state.modified    = SMF_ALL;

		D_MAGIC_SET( &state, CardState );
	}

	if(!data->img)
	{
		__u8 *img;
		int   pitch;
		int   ysc   = (d.h << 16) / data->height;
		int   y     = d.y << 16;

		err = dfb_surface_create( NULL, data->width, data->height, 
				          data->pixel, CSP_SYSTEMONLY, 
					  DSCAPS_SYSTEMONLY, NULL, &data->img );
		if(!data->img)
		{
			D_ERROR( "DirectFB/ImageProvider_PNM: "
				 "couldn't create a surface %ix%i...\n\t-> %s\n",
				 data->width, data->height, 
				 DirectFBErrorString( err ) );
			return( err );
		}

		img   = data->img->back_buffer->system.addr;
		pitch = data->img->back_buffer->system.pitch;

		state.source    = data->img;
		state.modified |= SMF_SOURCE;

		if(data->chunksize)
		{
			int size = (data->chunksize * data->width) + 1;
			
			data->rowbuf = (__u8*) D_MALLOC( size );
			if(!data->rowbuf)
			{
				D_ERROR( "DirectFB/ImageProvider_PNM: "
					 "couldn't allocate %i bytes of memory.\n",
					 size );
				return( DFB_NOSYSTEMMEMORY );
			}
		}

		d.h = ysc >> 16;
		if(d.h < 1)
			d.h = 1;
		
		for(s.w = data->width, s.h = 1; s.y < data->height; s.y++)
		{
			err = data->getrow( data, img );
			if(err != DFB_OK )
			{
				D_ERROR( "DirectFB/ImageProvider_PNM: "
					 "failed to retrieve row %i...\n\t-> %s\n",
					 s.y, DirectFBErrorString( err ) );
				break;
			}

			d.y = y >> 16;
			
			if(d.w != data->width || ysc != 65536)
				dfb_gfxcard_stretchblit( &s, &d, &state );
			else
				dfb_gfxcard_blit( &s, d.x, d.y, &state );

			if(data->render_callback)
				data->render_callback( &d, data->render_callback_ctx );
			
			img += pitch;
			y   += ysc;
		}

		if(data->rowbuf)
			D_FREE( data->rowbuf );
		
		data->buffer->Release( data->buffer );
		data->buffer = NULL;
	} else
	{
		if(data->render_callback)
		{
			int ysc = (d.h << 16) / data->height;
			int y   = d.y << 16;

			d.h = ysc >> 16;
			if(d.h < 1)
				d.h = 1;

			for(s.w = data->width, s.h = 1; s.y < data->height; s.y++)
			{
				d.y = y >> 16;
				
				if(d.w != data->width || ysc != 65536)
					dfb_gfxcard_stretchblit( &s, &d, &state );
				else
					dfb_gfxcard_blit( &s, d.x, d.y, &state );

				data->render_callback( &d, data->render_callback_ctx );

				y += ysc;
			}
		} else
		{
			s.w = data->width;
			s.h = data->height;
			
			if(d.w != data->width || d.h != data->height)
				dfb_gfxcard_stretchblit( &s, &d, &state );
			else
				dfb_gfxcard_blit( &s, d.x, d.y, &state );
		}
	}

	return( err );
}


static DFBResult
IDirectFBImageProvider_PNM_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *ctx )
{
	DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

	data->render_callback     = callback;
	data->render_callback_ctx = ctx;

	return( DFB_OK );
}


static DFBResult
IDirectFBImageProvider_PNM_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *desc )
{
	DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

	desc->flags       = (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);
	desc->width       = data->width;
	desc->height      = data->height;
	desc->pixelformat = data->pixel;

	return( DFB_OK );
}


static DFBResult
IDirectFBImageProvider_PNM_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *desc )
{
	DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

	if(!desc)
		return( DFB_INVARG );

	desc->caps = (data->format == PFMT_PBM) 
		      ? DICAPS_ALPHACHANNEL : DICAPS_NONE;

	return( DFB_OK );
}

