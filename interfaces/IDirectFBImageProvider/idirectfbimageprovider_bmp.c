/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/surface.h>

#include <gfx/convert.h>

#include <misc/gfx_util.h>
#include <misc/util.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/interface.h>


static DFBResult Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult Construct( IDirectFBImageProvider *thiz,
                            IDirectFBDataBuffer    *buffer );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, BMP )


typedef enum {
     BMPIC_NONE      = 0,
     BMPIC_RLE8      = 1,
     BMPIC_RLE4      = 2,
     BMPIC_BITFIELDS = 3
} BMPImageCompression;

typedef struct {
     int                    ref;

     IDirectFBDataBuffer   *buffer;

     int                    width;
     int                    height;
     int                    depth;
     bool                   indexed;
     BMPImageCompression    compression;
     unsigned int           img_offset;
     unsigned int           num_colors;

     DFBColor              *palette;

     u32                 *image;

     DIRenderCallback       render_callback;
     void                  *render_callback_ctx;
} IDirectFBImageProvider_BMP_data;


/*****************************************************************************/

static DFBResult
fetch_data( IDirectFBDataBuffer *buffer, void *ptr, int len )
{
     DFBResult ret;

     while (len > 0) {
          unsigned int read = 0;

          ret = buffer->WaitForData( buffer, len );
          if (ret == DFB_OK)
               ret = buffer->GetData( buffer, len, ptr, &read );

          if (ret)
               return ret;

          ptr += read;
          len -= read;
     }

     return DFB_OK;
}

static DFBResult
bmp_decode_header( IDirectFBImageProvider_BMP_data *data )
{
     DFBResult ret;
     u8        buf[54];
     u32       tmp;
     u32       bihsize;

     memset( buf, 0, sizeof(buf) );

     ret = fetch_data( data->buffer, buf, sizeof(buf) );
     if (ret)
          return ret;

     /* 2 bytes: Magic */    
     if (buf[0] != 'B' && buf[1] != 'M') {
          D_ERROR( "IDirectFBImageProvider_BMP: "
                   "Invalid magic (%c%c)!\n", buf[0], buf[1] );
          return DFB_UNSUPPORTED;
     }

     /* 4 bytes: FileSize */

     /* 4 bytes: Reserved */

     /* 4 bytes: DataOffset */
     data->img_offset = buf[10] | (buf[11]<<8) | (buf[12]<<16) | (buf[13]<<24);
     if (data->img_offset < 54) {
          D_ERROR( "IDirectFBImageProvider_BMP: "
                   "Invalid offset %08x!\n", data->img_offset );
          return DFB_UNSUPPORTED;
     }

     /* 4 bytes: HeaderSize */
     bihsize = buf[14] | (buf[15]<<8) | (buf[16]<<16) | (buf[17]<<24);
     if (bihsize < 40) {
          D_ERROR( "IDirectFBImageProvider_BMP: "
                   "Invalid image header size %d!\n", bihsize );
          return DFB_UNSUPPORTED;
     }

     /* 4 bytes: Width */
     data->width = buf[18] | (buf[19]<<8) | (buf[20]<<16) | (buf[21]<<24);
     if (data->width < 1 || data->width > 0xffff) {
          D_ERROR( "IDirectFBImageProvider_BMP: "
                   "Invalid width %d!\n", data->width );
          return DFB_UNSUPPORTED;
     }
          
     /* 4 bytes: Height */
     data->height = buf[22] | (buf[23]<<8) | (buf[24]<<16) | (buf[25]<<24);
     if (data->height < 1 || data->height > 0xffff) {
          D_ERROR( "IDirectFBImageProvider_BMP: "
                   "Invalid height %d!\n", data->height );
          return DFB_UNSUPPORTED;
     }

     /* 2 bytes: Planes */
     tmp = buf[26] | (buf[27]<<8);
     if (tmp != 1) {
          D_ERROR( "IDirectFBImageProvider_BMP: "
                   "Unsupported number of planes %d!\n", tmp );
          return DFB_UNSUPPORTED;
     }

     /* 2 bytes: Depth */
     data->depth = buf[28] | (buf[29]<<8);
     switch (data->depth) {
          case 1:
          case 4:
          case 8:
               data->indexed = true;
          case 16:
          case 24:
          case 32:
               break;
          default:
               D_ERROR( "IDirectFBImageProvider_BMP: "
                        "Unsupported depth %d!\n", data->depth );
               return DFB_UNSUPPORTED;
     }

     /* 4 bytes: Compression */
     data->compression = buf[30] | (buf[31]<<8) | (buf[32]<<16) | (buf[33]<<24);
     switch (data->compression) {
          case BMPIC_NONE:
          //case BMPIC_RLE8:
          //case BMPIC_RLE4:
          //case BMPIC_BITFIELDS:
               break;
          default:
               D_ERROR( "IDirectFBImageProvider_BMP: "
                        "Unsupported compression %d!\n", data->compression );
               return DFB_UNSUPPORTED;
     }

     /* 4 bytes: CompressedSize */

     /* 4 bytes: HorizontalResolution */

     /* 4 bytes: VerticalResolution */

     /* 4 bytes: UsedColors */
     data->num_colors = buf[46] | (buf[47]<<8) | (buf[48]<<16) | (buf[49]<<24);
     if (!data->num_colors)
          data->num_colors = 1 << data->depth;

     /* 4 bytes: ImportantColors */

     /* Skip remaining bytes */
     if (bihsize > 40) {
          bihsize -= 40;
          while (bihsize--) {
               u8 b; 
               ret = fetch_data( data->buffer, &b, 1 );
               if (ret)
                    return ret;
          }
     }

     /* Palette */
     if (data->indexed) {
          void *src;
          int   i, j;

          data->palette = src = D_MALLOC( 256*4 );
          if (!data->palette)
               return D_OOM();

          ret = fetch_data( data->buffer, src, data->num_colors*4 );
          if (ret)
               return ret;

          for (i = 0; i < data->num_colors; i++) {
               DFBColor c;

               c.a = 0xff;
               c.r = ((u8*)src)[i*4+2];
               c.g = ((u8*)src)[i*4+1];
               c.b = ((u8*)src)[i*4+0];

               /* For faster lookup, fill some of the 256 entries with duplicate data
                  for every bit position */
               switch (data->num_colors) {
                    case 2:
                         for (j = 0; j < 8; j++)
                              data->palette[i << j] = c;
                         break;
                    case 4:
               data->palette[i] = c;
                         data->palette[i << 4] = c;
                         break;
               }
          }
     }

     return DFB_OK;
}

static DFBResult
bmp_decode_rgb_row( IDirectFBImageProvider_BMP_data *data, int row )
{
     DFBResult  ret;
     int        pitch = (((data->width*data->depth + 7) >> 3) + 3) & ~3;
     u8         buf[pitch];
     u32       *dst;
     int        i;

     ret = fetch_data( data->buffer, buf, pitch );
     if (ret)
          return ret;

     dst = data->image + row * data->width;

     switch (data->depth) {
          case 1:
               for (i = 0; i < data->width; i++) {
                    unsigned idx = buf[i>>3] & (0x80 >> (i&7));
                    DFBColor c   = data->palette[idx];
                    dst[i] = c.b | (c.g << 8) | (c.r << 16) | (c.a << 24);
               }
               break;
          case 4:
               for (i = 0; i < data->width; i++) {
                    unsigned idx = buf[i>>1] & (0xf0 >> ((i&1) << 2));
                    DFBColor c   = data->palette[idx];
                    dst[i] = c.b | (c.g << 8) | (c.r << 16) | (c.a << 24);
               }
               break;
          case 8:
               for (i = 0; i < data->width; i++) {
                    DFBColor c = data->palette[buf[i]];
                    dst[i] = c.b | (c.g << 8) | (c.r << 16) | (c.a << 24);
               }
               break;
          case 16:
               for (i = 0; i < data->width; i++) {
                    u32 r, g, b;
                    u16 c;

                    c = buf[i*2+0] | (buf[i*2+1]<<8);
                    r = (c >> 10) & 0x1f;
                    g = (c >>  5) & 0x1f;
                    b = (c      ) & 0x1f;
                    r = (r << 3) | (r >> 2);
                    g = (g << 3) | (g >> 2);
                    b = (b << 3) | (b >> 2);

                    dst[i] = b | (g<<8) | (r<<16) | 0xff000000;
               }
               break;
          case 24:
               for (i = 0; i < data->width; i++) {
                    dst[i] = (buf[i*3+2]    ) | 
                             (buf[i*3+1]<< 8) |
                             (buf[i*3+0]<<16) |
                             0xff000000;
               }
               break;
          case 32:
               for (i = 0; i < data->width; i++) {
                    dst[i] = (buf[i*4+2]    ) | 
                             (buf[i*4+1]<< 8) |
                             (buf[i*4+0]<<16) |
                             (buf[i*4+3]<<24);
               }
               break; 
          default:
               break;
     }

     return DFB_OK;
}    
     
/*****************************************************************************/

static void
IDirectFBImageProvider_BMP_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_BMP_data *data = thiz->priv;

     if (data->buffer)
          data->buffer->Release( data->buffer );

     if (data->image)
          D_FREE( data->image );

     if (data->palette)
          D_FREE( data->palette );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBImageProvider_BMP_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_BMP )

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBImageProvider_BMP_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_BMP )

     if (--data->ref == 0)
           IDirectFBImageProvider_BMP_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_BMP_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *dest_rect )
{
     IDirectFBSurface_data  *dst_data;
     CoreSurface            *dst_surface;
     CoreSurfaceBufferLock   lock;
     DFBRectangle            rect;
     DFBRegion               clip;
     DIRenderCallbackResult  cb_result = DIRCR_OK;
     DFBResult               ret       = DFB_OK;

     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_BMP )

     if (!destination)
          return DFB_INVARG;

     dst_data = destination->priv;
     if (!dst_data || !dst_data->surface)
          return DFB_DESTROYED;

     dst_surface = dst_data->surface;

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

     dfb_region_from_rectangle( &clip, &dst_data->area.current );
     if (!dfb_rectangle_region_intersects( &rect, &clip ))
          return DFB_OK;

     ret = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
     if (ret)
          return ret;

     if (!data->image) {
          bool direct = (rect.w == data->width  &&
                         rect.h == data->height &&
                         data->render_callback);
          int  y;

          if (data->indexed && dst_surface->config.format == DSPF_LUT8) {
               IDirectFBPalette *palette;
               
               ret = destination->GetPalette( destination, &palette );
               if (ret) {
                    dfb_surface_unlock_buffer( dst_surface, &lock );
                    return ret;
               }

               palette->SetEntries( palette, data->palette, data->num_colors, 0 );
               palette->Release( palette );
          }

          data->image = D_MALLOC( data->width*data->height*4 );
          if (!data->image) {
               dfb_surface_unlock_buffer( dst_surface, &lock );
               return D_OOM();
          }

          data->buffer->SeekTo( data->buffer, data->img_offset );

          for (y = data->height-1; y >= 0 && cb_result == DIRCR_OK; y--) {
               ret = bmp_decode_rgb_row( data, y );
               if (ret)
                    break;

               if (direct) {
                    DFBRectangle r = { rect.x, rect.y+y, data->width, 1 };

                    dfb_copy_buffer_32( data->image+y*data->width,
                                        lock.addr, lock.pitch, &r, dst_surface, &clip );

                    if (data->render_callback) {
                         r = (DFBRectangle) { 0, y, data->width, 1 };
                         cb_result = data->render_callback( &r,
                                             data->render_callback_ctx );
                    }
               }
          }

          if (!direct) {
               dfb_scale_linear_32( data->image, data->width, data->height,
                                    lock.addr, lock.pitch, &rect, dst_surface, &clip );

               if (data->render_callback) {
                    DFBRectangle r = { 0, 0, data->width, data->height };
                    data->render_callback( &r, data->render_callback_ctx );
               }
          }

          if (cb_result == DIRCR_OK) {
               data->buffer->Release( data->buffer );
               data->buffer = NULL;
          }
          else {
               D_FREE( data->image );
               data->image = NULL;
               ret = DFB_INTERRUPTED;
          }
     }
     else {
          dfb_scale_linear_32( data->image, data->width, data->height,
                               lock.addr, lock.pitch, &rect, dst_surface, &clip );
          
          if (data->render_callback) {
               DFBRectangle r = {0, 0, data->width, data->height};
               data->render_callback( &r, data->render_callback_ctx );
          }
     }

     dfb_surface_unlock_buffer( dst_surface, &lock );

     return ret;
}

static DFBResult
IDirectFBImageProvider_BMP_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *ctx )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_BMP )

     data->render_callback     = callback;
     data->render_callback_ctx = ctx;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_BMP_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_BMP )

     if (!desc)
          return DFB_INVARG;

     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->width       = data->width;
     desc->height      = data->height;
     desc->pixelformat = (data->indexed) ? DSPF_LUT8 : DSPF_RGB32;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_BMP_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_BMP )

     if (!desc)
          return DFB_INVARG;

     desc->caps = DICAPS_NONE;

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (ctx->header[0] == 'B' && ctx->header[1] == 'M')
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBImageProvider_BMP )

     data->ref    = 1;
     data->buffer = buffer;

     buffer->AddRef( buffer );

     ret = bmp_decode_header( data );
     if (ret) {
          IDirectFBImageProvider_BMP_Destruct( thiz );
          return ret;
     }

     thiz->AddRef                = IDirectFBImageProvider_BMP_AddRef;
     thiz->Release               = IDirectFBImageProvider_BMP_Release;
     thiz->RenderTo              = IDirectFBImageProvider_BMP_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_BMP_SetRenderCallback;
     thiz->GetImageDescription   = IDirectFBImageProvider_BMP_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_BMP_GetSurfaceDescription;

     return DFB_OK;
}

