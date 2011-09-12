/*
   Copyright (C) 2004-2005 Claudio Ciccani <klan@users.sf.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/


#include <config.h>

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
#include <core/surface.h>
#include <core/gfxcard.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/interface.h>

#include <gfx/convert.h>

#include <misc/gfx_util.h>
#include <misc/util.h>


static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer );

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


typedef enum {
     PHDR_MAGIC,
     PHDR_WIDTH,
     PHDR_HEIGHT,
     PHDR_COLORS
} PHeader;

typedef enum {
     PFMT_PBM = 0,
     PFMT_PGM = 1,
     PFMT_PPM = 2
} PFormat;

typedef enum {
     PIMG_RAW   = 0,
     PIMG_PLAIN = 1
} PImgType;


typedef DFBResult (*PRowCallback) ( IDirectFBImageProvider_PNM_data *data,
                                    u8                            *dest );


typedef struct {
     PRowCallback rowcb;
     int          chunksize;
} PFormatData;


struct __IDirectFBImageProvider_PNM_data {
     IDirectFBImageProvider_data   base;

     PFormat                format;
     PImgType               type;
     unsigned int           img_offset;
     
     u8                    *img;
     int                    width;
     int                    height;
     int                    colors;

     PRowCallback           getrow;
     u8                    *rowbuf;     /* buffer for ascii images */
     int                    bufp;       /* current position in buffer */
     int                    chunksize;  /* maximum size of each sample */

     DIRenderCallback       render_callback;
     void                  *render_callback_ctx;
};




#define P_GET( buf, n ) \
{\
     data->base.buffer->WaitForData( data->base.buffer, n );\
     err = data->base.buffer->GetData( data->base.buffer, n, buf, &len );\
     if (err) {\
          if (err == DFB_EOF)\
               return DFB_OK;\
          D_ERROR( "DirectFB/ImageProvider_PNM: "\
                   "couldn't get %i bytes from data buffer...\n\t-> %s\n",\
                   n, DirectFBErrorString( err ) );\
          return err;\
     }\
}

#define P_LOADBUF() \
{\
     int size = data->chunksize * data->width;\
     if (data->bufp) {\
          size -= data->bufp;\
          memset( data->rowbuf + data->bufp, 0, size + 1 );\
          P_GET( data->rowbuf + data->bufp, size );\
          len += data->bufp;\
          data->bufp = 0;\
     } else {\
          memset( data->rowbuf, 0, size + 1 );\
          P_GET( data->rowbuf, size );\
     }\
}

#define P_STOREBUF() \
{\
     int size = data->chunksize * data->width;\
     if (i++ < len && i < size) {\
          size -= i;\
          direct_memcpy( data->rowbuf, data->rowbuf + i, size );\
          data->bufp = size;\
     }\
}



static DFBResult
__rawpbm_getrow( IDirectFBImageProvider_PNM_data *data,
                 u8                            *dest )
{
     DFBResult     err;
     unsigned int  len;
     int           i, j;
     u8           *s    = dest;
     u32          *d    = (u32*) dest;

     P_GET( dest, data->width / 8 );

     /* start from end */
     for (i = (len * 8), j = 0; --i >= 0; ) {
          d[i] = (s[i >> 3] & (1 << j))
                 ? 0x00000000  /* alpha:0x00, color:black */
                 : 0xffffffff; /* alpha:0xff, color:white */
          
          if (++j > 7)
               j = 0;
     }

     return DFB_OK;
}

static DFBResult
__rawpgm_getrow( IDirectFBImageProvider_PNM_data *data,
                 u8                            *dest )
{
     DFBResult     err;
     unsigned int  len;
     u8           *s   = dest;
     u32          *d   = (u32*) dest;

     P_GET( dest, data->width );

     /* start from end */
     while (--len >= 0)
          d[len] = PIXEL_ARGB( s[len], s[len], s[len], s[len] );

     return DFB_OK;
}

static DFBResult
__rawppm_getrow( IDirectFBImageProvider_PNM_data *data,
                 u8                            *dest )
{
     DFBResult     err;
     unsigned int  len;
     int           i;
     u8           *s   = dest;
     u32          *d   = (u32*) dest;

     P_GET( dest, data->width * 3 );

     /* start from end */
     for (i = len/3; --i >= 0;)
          d[i] = PIXEL_ARGB( 0xff, s[i*3+0], s[i*3+1], s[i*3+2] );

     return DFB_OK;
}


static DFBResult
__plainpbm_getrow( IDirectFBImageProvider_PNM_data *data,
                   u8                            *dest )
{
     DFBResult     err;
     unsigned int  len;
     int           i;
     int           w    = data->width;
     u8           *buf  = data->rowbuf;
     u32          *d    = (u32*) dest;

     P_LOADBUF();

     for (i = 0; i < len; i++) {
          if (buf[i] == 0)
               break;
          
          switch (buf[i]) {
               case '0':
                    *d++ = 0xffffffff; /* alpha:0xff, color:white */
                    break;
               case '1':
                    *d++ = 0x00000000; /* alpha:0x00, color:black */
                    break;
               default:
                    continue;
          }

          /* assume next char is a space */
          i++;
          if (!--w)
               break;
     }

     P_STOREBUF();

     return DFB_OK;
}

static DFBResult
__plainpgm_getrow( IDirectFBImageProvider_PNM_data *data,
                   u8                            *dest )
{
     DFBResult     err;
     unsigned int  len;
     int           i, n;
     int           w    = data->width;
     u8           *buf  = data->rowbuf;
     u32          *d    = (u32*) dest;

     P_LOADBUF();

     for (i = 0, n = 0; i < len; i++) {
          if (buf[i] == 0)
               break;

          if (buf[i] < '0' || buf[i] > '9') {
               n = 0;
               continue;
          }

          n *= 10;
          n += buf[i] - '0';

          if (isspace( buf[i+1] )) {
               *d++ = PIXEL_ARGB( n, n, n, n );
               n = 0;
               i++;

               if (!--w)
                    break;
          }
     }

     P_STOREBUF();

     return DFB_OK;
}

static DFBResult
__plainppm_getrow( IDirectFBImageProvider_PNM_data *data,
                   u8                            *dest )
{
     DFBResult     err;
     unsigned int  len;
     int           i, n;
     int           j    = 16;
     int           w    = data->width;
     u8           *buf  = data->rowbuf;
     u32          *d    = (u32*) dest;

     P_LOADBUF();

     for (i = 0, n = 0; i < len; i++) {
          if (buf[i] == 0)
               break;

          if (buf[i] < '0' || buf[i] > '9') {
               n = 0;
               continue;
          }

          n *= 10;
          n += buf[i] - '0';
          
          if (isspace( buf[i+1] )) {
               *d |= (n & 0xff) << j;
               n = 0;
               i++;
               j -= 8;
               
               if (j < 0) {
                    *d++ |= 0xff000000;
                    j = 16;
                    
                    if (!--w)
                         break;
               }
          }
     }

     P_STOREBUF();

     return DFB_OK;
}


static const PFormatData p_dta[][2] = {
     { {__rawpbm_getrow, 0}, {__plainpbm_getrow,  2} }, /* PBM */
     { {__rawpgm_getrow, 0}, {__plainpgm_getrow,  4} }, /* PGM */
     { {__rawppm_getrow, 0}, {__plainppm_getrow, 12} }  /* PPM */
};


static DFBResult
p_getheader( IDirectFBImageProvider_PNM_data *data,
             char                            *to,
             int                              size )
{
     DFBResult    err;
     unsigned int len;

     while (size--) {
          P_GET( to, 1 );

          if (*to == '#') {
               char c = 0;

               *to = 0;

               while (c != '\n')
                    P_GET( &c, 1 );

               return DFB_OK;
          }
          else if (isspace( *to )) {
               *to = 0;
               return DFB_OK;
          }

          to++;
     }

     return DFB_OK;
}

static DFBResult
p_init( IDirectFBImageProvider_PNM_data *data )
{
     DFBResult err;
     PHeader   header  = PHDR_MAGIC;
     char      buf[33];

     memset( buf, 0, 33 );

     while ((err = p_getheader( data, &buf[0], 32 )) == DFB_OK) {
          if (buf[0] == 0)
               continue;

          switch (header) {
               case PHDR_MAGIC: {
                    if (buf[0] != 'P')
                         return DFB_UNSUPPORTED;

                    switch (buf[1]) {
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
                              return DFB_UNSUPPORTED;
                    }

                    data->type      = (buf[1] > '3') ? PIMG_RAW : PIMG_PLAIN;
                    data->getrow    = p_dta[data->format][data->type].rowcb;
                    data->chunksize = p_dta[data->format][data->type].chunksize;

                    header = PHDR_WIDTH;
               }    break;

               case PHDR_WIDTH: {
                    data->width = strtol( buf, NULL, 10 );

                    if (data->width < 1)
                         return DFB_UNSUPPORTED;

                    if (data->format == PFMT_PBM && data->width & 7) {
                         D_ERROR( "DirectFB/ImageProvider_PNM: "
                                  "PBM width must be a multiple of 8.\n" );
                         return DFB_UNIMPLEMENTED;
                    }
                    
                    header = PHDR_HEIGHT;
               }    break;

               case PHDR_HEIGHT: {
                    data->height = strtol( buf, NULL, 10 );

                    if (data->height < 1)
                         return DFB_UNSUPPORTED;

                    if (data->format == PFMT_PBM)
                         return DFB_OK;
                    
                    header = PHDR_COLORS;
               }    break;

               case PHDR_COLORS: {
                    data->colors = strtoul( buf, NULL, 10 );

                    if (data->colors < 1)
                         return DFB_UNSUPPORTED;
                         
                    if (data->colors > 0xff) {
                         D_ERROR( "DirectFB/ImageProvider_PNM: "
                                  "2-bytes samples are not supported.\n" );
                         return DFB_UNIMPLEMENTED;
                    }

                    return DFB_OK;
               } break;
          }
     }

     data->base.buffer->GetPosition( data->base.buffer, &data->img_offset );
     
     return err;
}



static void
IDirectFBImageProvider_PNM_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_PNM_data *data;

     data = (IDirectFBImageProvider_PNM_data*) thiz->priv;
     
     if (data->img)
          D_FREE( data->img );
}

static DFBResult
IDirectFBImageProvider_PNM_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *dest,
                                     const DFBRectangle     *dest_rect )
{
     DFBResult              err       = DFB_OK;
     DIRenderCallbackResult cb_result = DIRCR_OK;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     CoreSurfaceBufferLock  lock;
     DFBRectangle           rect;
     DFBRegion              clip;
     u8                    *img       = NULL;
     int                    img_p;

     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

     if (!dest)
          return DFB_INVARG;

     dst_data = (IDirectFBSurface_data*) dest->priv;
     if (!dst_data || !dst_data->surface)
          return DFB_DESTROYED;
     dst_surface = dst_data->surface;

     dfb_region_from_rectangle( &clip, &dst_data->area.current );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
          rect = *dest_rect;
          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }
     else {
          rect = dst_data->area.wanted;
     }

     err = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
     if (err)
          return err;

     img   = data->img;
     img_p = data->width * 4; 

     if (!img) {
          bool cpy = (rect.w == data->width && rect.h == data->height);
          int  y;

          data->img = img = (u8*) D_MALLOC( img_p * data->height );
          
          if (!img) {
               D_ERROR( "DirectFB/ImageProvider_PNM: "
                        "couldn't allocate %i bytes for image.\n",
                        img_p * data->height );
               dfb_surface_unlock_buffer( dst_surface, &lock );
               return DFB_NOSYSTEMMEMORY;
          }

          if (data->chunksize) {
               int size = (data->chunksize * data->width) + 1;
               
               data->rowbuf = (u8*) D_MALLOC( size );
               if (!data->rowbuf) {
                    D_ERROR( "DirectFB/ImageProvider_PNM: "
                             "couldn't allocate %i bytes for buffering.\n",
                             size );
                    dfb_surface_unlock_buffer( dst_surface, &lock );
                    return DFB_NOSYSTEMMEMORY;
               }
          }
          
          for (y = 0; y < data->height && cb_result == DIRCR_OK; y++) {
               err = data->getrow( data, (unsigned char*) img );
               
               if (err != DFB_OK ) {
                    D_ERROR( "DirectFB/ImageProvider_PNM: "
                             "failed to retrieve row %i...\n\t-> %s\n",
                             y, DirectFBErrorString( err ) );
                    break;
               }

               if (cpy) {
                    DFBRectangle r = { rect.x, rect.y+y, data->width, 1 };
                    
                    dfb_copy_buffer_32( (u32*)img, lock.addr, lock.pitch,
                                        &r, dst_surface, &clip );

                    if (data->render_callback) {
                         r = (DFBRectangle) { 0, y, data->width, 1 };
                         cb_result = data->render_callback( &r,
                                             data->render_callback_ctx );
                    }
               }
               
               img += img_p;
          }

          if (!cpy) {
               dfb_scale_linear_32( (u32*)data->img, data->width, data->height,
                                    lock.addr, lock.pitch, &rect, dst_surface, &clip );

               if (data->render_callback) {
                    DFBRectangle r = { 0, 0, data->width, data->height };
                    data->render_callback( &r, data->render_callback_ctx );
               }
          }                    

          if (data->rowbuf) {
               D_FREE( data->rowbuf );
               data->rowbuf = NULL;
          }          
          
          if (cb_result == DIRCR_OK) {
               data->base.buffer->Release( data->base.buffer );
               data->base.buffer = NULL;
          } else {
               data->base.buffer->SeekTo( data->base.buffer, data->img_offset );
               D_FREE( data->img );
               data->img = NULL;
          }
     
     } /* image already in buffer */ 
     else {
          dfb_scale_linear_32( (u32*)img, data->width, data->height,
                               lock.addr, lock.pitch, &rect, dst_surface, &clip );
          
          if (data->render_callback) {
               DFBRectangle r = {0, 0, data->width, data->height};
               data->render_callback( &r, data->render_callback_ctx );
          }
     }

     dfb_surface_unlock_buffer( dst_surface, &lock );

     return (cb_result == DIRCR_OK) ? err : DFB_INTERRUPTED;
}

static DFBResult
IDirectFBImageProvider_PNM_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *ctx )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

     data->render_callback     = callback;
     data->render_callback_ctx = ctx;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_PNM_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

     if (!desc)
          return DFB_INVARG;

     desc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->width  = data->width;
     desc->height = data->height;

     switch (data->format) {
          case PFMT_PBM:
               desc->pixelformat = DSPF_A1;
               break;
          case PFMT_PGM:
               desc->pixelformat = DSPF_A8;
               break;
          default:
               desc->pixelformat = DSPF_RGB32;
               break;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_PNM_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_PNM )

     if (!desc)
          return DFB_INVARG;

     switch (data->format) {
          case PFMT_PBM:
          case PFMT_PGM:
               desc->caps = DICAPS_ALPHACHANNEL;
               break;
          default:
               desc->caps = DICAPS_NONE;
               break;
     }

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (ctx->header[0] == 'P') {
          if (ctx->header[1] < '1' || ctx->header[1] > '6')
               return DFB_UNSUPPORTED;

          if (!isspace( ctx->header[2] ))
               return DFB_UNSUPPORTED;

          return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
#if !(DIRECT_BUILD_NOTEXT)
     static const char* format_names[] = {
          "PBM", "PGM", "PPM"
     };
#endif
     DFBResult err;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBImageProvider_PNM )

     data->base.ref    = 1;
     data->base.buffer = buffer;

     buffer->AddRef( buffer );

     err = p_init( data );
     if (err != DFB_OK) {
          buffer->Release( buffer );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return err;
     }

     D_DEBUG( "DirectFB/ImageProvider_PNM: found %s %s %ix%i.\n",
               (data->type == PIMG_RAW) ? "Raw" : "Plain",
               format_names[data->format], data->width, data->height );

     data->base.Destruct = IDirectFBImageProvider_PNM_Destruct;

     thiz->RenderTo              = IDirectFBImageProvider_PNM_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_PNM_SetRenderCallback;
     thiz->GetImageDescription   = IDirectFBImageProvider_PNM_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_PNM_GetSurfaceDescription;

     return DFB_OK;
}

