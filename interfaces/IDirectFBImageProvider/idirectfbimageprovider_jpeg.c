/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/layers.h>

#include <core/CoreSurface.h>

#include <misc/gfx_util.h>
#include <misc/util.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <setjmp.h>
#include <math.h>

#undef HAVE_STDLIB_H
#include <jpeglib.h>


static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, JPEG )

/*
 * private data struct of IDirectFBImageProvider_JPEG
 */
typedef struct {
     IDirectFBImageProvider_data base;

     int                  width;    /*  width of the JPEG image   */
     int                  height;   /*  height of the JPEG image  */

     u32                 *image;        /*  decoded image data    */
     int                  image_width;  /*  width of image data   */
     int                  image_height; /*  height of image data  */
} IDirectFBImageProvider_JPEG_data;


static DFBResult
IDirectFBImageProvider_JPEG_RenderTo( IDirectFBImageProvider *thiz,
                                      IDirectFBSurface       *destination,
                                      const DFBRectangle     *destination_rect );

static DFBResult
IDirectFBImageProvider_JPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                   DFBSurfaceDescription  *dsc);

static DFBResult
IDirectFBImageProvider_JPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                 DFBImageDescription    *dsc );


#define JPEG_PROG_BUF_SIZE    0x10000

typedef struct {
     struct jpeg_source_mgr  pub; /* public fields */

     JOCTET                 *data;       /* start of buffer */

     IDirectFBDataBuffer    *buffer;

     int                     peekonly;
     int                     peekoffset;
} buffer_source_mgr;

typedef buffer_source_mgr * buffer_src_ptr;

static void
buffer_init_source (j_decompress_ptr cinfo)
{
     buffer_src_ptr src          = (buffer_src_ptr) cinfo->src;
     IDirectFBDataBuffer *buffer = src->buffer;

     buffer->SeekTo( buffer, 0 ); /* ignore return value */
}

static boolean
buffer_fill_input_buffer (j_decompress_ptr cinfo)
{
     DFBResult            ret;
     unsigned int         nbytes = 0;
     buffer_src_ptr       src    = (buffer_src_ptr) cinfo->src;
     IDirectFBDataBuffer *buffer = src->buffer;

     buffer->WaitForDataWithTimeout( buffer, JPEG_PROG_BUF_SIZE, 1, 0 );

     if (src->peekonly) {
          ret = buffer->PeekData( buffer, JPEG_PROG_BUF_SIZE,
                                  src->peekoffset, src->data, &nbytes );
          src->peekoffset += MAX( nbytes, 0 );
     }
     else {
          ret = buffer->GetData( buffer, JPEG_PROG_BUF_SIZE, src->data, &nbytes );
     }
     
     if (ret || nbytes <= 0) {
          /* Insert a fake EOI marker */
          src->data[0] = (JOCTET) 0xFF;
          src->data[1] = (JOCTET) JPEG_EOI;
          nbytes = 2;

          if (ret && ret != DFB_EOF)
               DirectFBError( "(DirectFB/ImageProvider_JPEG) GetData failed", ret );
     }

     src->pub.next_input_byte = src->data;
     src->pub.bytes_in_buffer = nbytes;

     return TRUE;
}

static void
buffer_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
     buffer_src_ptr src = (buffer_src_ptr) cinfo->src;

     if (num_bytes > 0) {
          while (num_bytes > (long) src->pub.bytes_in_buffer) {
               num_bytes -= (long) src->pub.bytes_in_buffer;
               (void)buffer_fill_input_buffer(cinfo);
          }
          src->pub.next_input_byte += (size_t) num_bytes;
          src->pub.bytes_in_buffer -= (size_t) num_bytes;
     }
}

static void
buffer_term_source (j_decompress_ptr cinfo)
{
}

static void
jpeg_buffer_src (j_decompress_ptr cinfo, IDirectFBDataBuffer *buffer, int peekonly)
{
     buffer_src_ptr src;

     cinfo->src = (struct jpeg_source_mgr *)
                  cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                           sizeof (buffer_source_mgr));

     src = (buffer_src_ptr) cinfo->src;

     src->data = (JOCTET *)
                  cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                           JPEG_PROG_BUF_SIZE * sizeof (JOCTET));

     src->buffer = buffer;
     src->peekonly = peekonly;
     src->peekoffset = 0;

     src->pub.init_source       = buffer_init_source;
     src->pub.fill_input_buffer = buffer_fill_input_buffer;
     src->pub.skip_input_data   = buffer_skip_input_data;
     src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
     src->pub.term_source       = buffer_term_source;
     src->pub.bytes_in_buffer   = 0; /* forces fill_input_buffer on first read */
     src->pub.next_input_byte   = NULL; /* until buffer loaded */
}

struct my_error_mgr {
     struct jpeg_error_mgr pub;     /* "public" fields */
     jmp_buf  setjmp_buffer;          /* for return to caller */
};

static void
jpeglib_panic(j_common_ptr cinfo)
{
     struct my_error_mgr *myerr = (struct my_error_mgr*) cinfo->err;
     longjmp(myerr->setjmp_buffer, 1);
}

static inline void
copy_line32( u32 *argb, const u8 *rgb, int width )
{
     while (width--) {
          *argb++ = 0xFF000000 | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];

          rgb += 3;
     }
}

static inline void
copy_line_nv16( u16 *yy, u16 *cbcr, const u8 *src_ycbcr, int width )
{
     int x;

     for (x=0; x<width/2; x++) {
#ifdef WORDS_BIGENDIAN
          yy[x] = (src_ycbcr[0] << 8) | src_ycbcr[3];

          cbcr[x] = (((src_ycbcr[1] + src_ycbcr[4]) << 7) & 0xff00) |
                     ((src_ycbcr[2] + src_ycbcr[5]) >> 1);
#else
          yy[x] = (src_ycbcr[3] << 8) | src_ycbcr[0];

          cbcr[x] = (((src_ycbcr[2] + src_ycbcr[5]) << 7) & 0xff00) |
                     ((src_ycbcr[1] + src_ycbcr[4]) >> 1);
#endif

          src_ycbcr += 6;
     }

     if (width & 1) {
          u8 *y = (u8*) yy;

          y[width-1] = src_ycbcr[0];

#ifdef WORDS_BIGENDIAN
          cbcr[x] = (src_ycbcr[1] << 8) | src_ycbcr[2];
#else
          cbcr[x] = (src_ycbcr[2] << 8) | src_ycbcr[1];
#endif
     }
}


static void
IDirectFBImageProvider_JPEG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_JPEG_data *data =
                              (IDirectFBImageProvider_JPEG_data*)thiz->priv;

     if (data->image)
          D_FREE( data->image );
}

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (ctx->header[0] == 0xff && ctx->header[1] == 0xd8) {
          if (strncmp ((char*) ctx->header + 6, "JFIF", 4) == 0 ||
              strncmp ((char*) ctx->header + 6, "Exif", 4) == 0)
               return DFB_OK;

          if (ctx->filename && strchr (ctx->filename, '.' ) &&
             (strcasecmp ( strchr (ctx->filename, '.' ), ".jpg" ) == 0 ||
              strcasecmp ( strchr (ctx->filename, '.' ), ".jpeg") == 0))
               return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     struct jpeg_decompress_struct cinfo;
     struct my_error_mgr jerr;
     
     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_JPEG)

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     data->base.ref    = 1;
     data->base.buffer = buffer;
     data->base.core   = core;

     buffer->AddRef( buffer );

     cinfo.err = jpeg_std_error(&jerr.pub);
     jerr.pub.error_exit = jpeglib_panic;

     if (setjmp(jerr.setjmp_buffer)) {
          D_ERROR( "ImageProvider/JPEG: Error while reading headers!\n" );

          jpeg_destroy_decompress(&cinfo);
          buffer->Release( buffer );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     jpeg_create_decompress(&cinfo);
     jpeg_buffer_src(&cinfo, buffer, 1);
     jpeg_read_header(&cinfo, TRUE);
     jpeg_start_decompress(&cinfo);
     
     data->width = cinfo.output_width;
     data->height = cinfo.output_height;
     
     jpeg_abort_decompress(&cinfo);
     jpeg_destroy_decompress(&cinfo);

     data->base.Destruct = IDirectFBImageProvider_JPEG_Destruct;

     thiz->RenderTo = IDirectFBImageProvider_JPEG_RenderTo;
     thiz->GetImageDescription =IDirectFBImageProvider_JPEG_GetImageDescription;
     thiz->GetSurfaceDescription =
     IDirectFBImageProvider_JPEG_GetSurfaceDescription;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_RenderTo( IDirectFBImageProvider *thiz,
                                      IDirectFBSurface       *destination,
                                      const DFBRectangle     *dest_rect )
{
     DFBResult              ret;
     bool                   direct = false;
     DFBRegion              clip;
     DFBRectangle           rect;
     DFBSurfacePixelFormat  format;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     CoreSurfaceBufferLock  lock;
     DIRenderCallbackResult cb_result = DIRCR_OK;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     ret = destination->GetPixelFormat( destination, &format );
     if (ret)
          return ret;

     dfb_region_from_rectangle( &clip, &dst_data->area.current );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
          
          rect = *dest_rect;
          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;

          if (!dfb_rectangle_region_intersects( &rect, &clip ))
               return DFB_OK;
     }
     else {
          rect = dst_data->area.wanted;
     }

     ret = CoreSurface_LockBuffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
     if (ret)
          return ret;

     if (data->image &&
         (rect.x || rect.y || rect.w != data->image_width || rect.h != data->image_height)) {
           D_FREE( data->image );
           data->image        = NULL;
           data->image_width  = 0;
           data->image_height = 0;
     }

     /* actual loading and rendering */
     if (!data->image) {
          struct jpeg_decompress_struct cinfo;
          struct my_error_mgr jerr;
          JSAMPARRAY buffer;      /* Output row buffer */
          int row_stride;         /* physical row width in output buffer */
          u32 *row_ptr;
          int y = 0;
          int uv_offset = 0;

          cinfo.err = jpeg_std_error(&jerr.pub);
          jerr.pub.error_exit = jpeglib_panic;

          if (setjmp(jerr.setjmp_buffer)) {
               D_ERROR( "ImageProvider/JPEG: Error during decoding!\n" );

               jpeg_destroy_decompress(&cinfo);

               if (data->image) {
                    dfb_scale_linear_32( data->image, data->image_width, data->image_height,
                                         lock.addr, lock.pitch, &rect, dst_surface, &clip );
                    CoreSurface_UnlockBuffer( dst_surface, &lock );
                    if (data->base.render_callback) {
                         DFBRectangle r = { 0, 0, data->image_width, data->image_height };

                         if (data->base.render_callback( &r,
                                                         data->base.render_callback_context ) != DIRCR_OK)
                              return DFB_INTERRUPTED;
                    }

                    return DFB_INCOMPLETE;
               }
               else
                    CoreSurface_UnlockBuffer( dst_surface, &lock );

               return DFB_FAILURE;
          }

          jpeg_create_decompress(&cinfo);
          jpeg_buffer_src(&cinfo, data->base.buffer, 0);
          jpeg_read_header(&cinfo, TRUE);

#if JPEG_LIB_VERSION >= 70
          cinfo.scale_num = 8;
          cinfo.scale_denom = 8;
#else
          cinfo.scale_num = 1;
          cinfo.scale_denom = 1;
#endif
          jpeg_calc_output_dimensions(&cinfo);

          if (cinfo.output_width == rect.w && cinfo.output_height == rect.h) {
               direct = true;
          }
          else if (rect.x == 0 && rect.y == 0) {
#if JPEG_LIB_VERSION >= 70
               /*  The supported scaling ratios in libjpeg 7 and 8
                *  are N/8 with all N from 1 to 16.
                */
               cinfo.scale_num = 1;
               jpeg_calc_output_dimensions (&cinfo);
               while (cinfo.scale_num < 16
                      && cinfo.output_width < rect.w
                      && cinfo.output_height < rect.h) {
                    ++cinfo.scale_num;
                    jpeg_calc_output_dimensions (&cinfo);
               }
#else
               /*  The supported scaling ratios in libjpeg 6
                *  are 1/1, 1/2, 1/4, and 1/8.
                */
               while (cinfo.scale_denom < 8
                      && ((cinfo.output_width >> 1) >= rect.w)
                      && ((cinfo.output_height >> 1) >= rect.h)) {
                    cinfo.scale_denom <<= 1;
                    jpeg_calc_output_dimensions (&cinfo);
               }
#endif
          }

          cinfo.output_components = 3;

          switch (dst_surface->config.format) {
               case DSPF_NV16:
                    uv_offset = dst_surface->config.size.h * lock.pitch;

                    if (direct && !rect.x && !rect.y) {
                         D_INFO( "JPEG: Using YCbCr color space directly! (%dx%d)\n",
                                 cinfo.output_width, cinfo.output_height );
                         cinfo.out_color_space = JCS_YCbCr;
                         break;
                    }
                    D_INFO( "JPEG: Going through RGB color space! (%dx%d -> %dx%d @%d,%d)\n",
                            cinfo.output_width, cinfo.output_height, rect.w, rect.h, rect.x, rect.y );
               
               default:
                    cinfo.out_color_space = JCS_RGB;
                    break;
          }

          jpeg_start_decompress(&cinfo);

          data->image_width = cinfo.output_width;
          data->image_height = cinfo.output_height;

          row_stride = cinfo.output_width * 3;

          buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo,
                                              JPOOL_IMAGE, row_stride, 1);

          data->image = D_CALLOC( data->image_height, data->image_width * 4 );
          if (!data->image) {
               CoreSurface_UnlockBuffer( dst_surface, &lock );
               return D_OOM();
          }
          row_ptr = data->image;

          while (cinfo.output_scanline < cinfo.output_height && cb_result == DIRCR_OK) {
               jpeg_read_scanlines(&cinfo, buffer, 1);

               switch (dst_surface->config.format) {
                    case DSPF_NV16:
                         if (direct) {
                              copy_line_nv16( lock.addr, lock.addr + uv_offset, *buffer, rect.w );

                              lock.addr += lock.pitch;

                              if (data->base.render_callback) {
                                   DFBRectangle r = { 0, y, data->image_width, 1 };

                                   cb_result = data->base.render_callback( &r,
                                                                           data->base.render_callback_context );
                              }
                              break;
                         }

                    default:
                         copy_line32( row_ptr, *buffer, data->image_width );

                         if (direct) {
                              DFBRectangle r = { rect.x, rect.y+y, rect.w, 1 };
                              dfb_copy_buffer_32( row_ptr, lock.addr, lock.pitch,
                                                  &r, dst_surface, &clip );
                              if (data->base.render_callback) {
                                   r = (DFBRectangle){ 0, y, data->image_width, 1 };
                                   cb_result = data->base.render_callback( &r,
                                                                           data->base.render_callback_context );
                              }
                         }
                         break;
               }

               row_ptr += data->image_width;
               y++;
          }

          if (!direct) {
               dfb_scale_linear_32( data->image, data->image_width, data->image_height,
                                    lock.addr, lock.pitch, &rect, dst_surface, &clip );
               if (data->base.render_callback) {
                    DFBRectangle r = { 0, 0, data->image_width, data->image_height };
                    cb_result = data->base.render_callback( &r,
                                                            data->base.render_callback_context );
               }
          }

          if (cb_result != DIRCR_OK) {
               jpeg_abort_decompress(&cinfo);
               D_FREE( data->image );
               data->image = NULL;
          }
          else {
               jpeg_finish_decompress(&cinfo);
          }
          jpeg_destroy_decompress(&cinfo);
     }
     else {
          dfb_scale_linear_32( data->image, data->image_width, data->image_height,
                               lock.addr, lock.pitch, &rect, dst_surface, &clip );
          if (data->base.render_callback) {
               DFBRectangle r = { 0, 0, data->image_width, data->image_height };
               data->base.render_callback( &r,
                                           data->base.render_callback_context );
          }
     }
     
     CoreSurface_UnlockBuffer( dst_surface, &lock );

     if (cb_result != DIRCR_OK)
         return DFB_INTERRUPTED;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                   DFBSurfaceDescription  *dsc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)
     
     dsc->flags  = DSDESC_WIDTH |  DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->height = data->height;
     dsc->width  = data->width;
     dsc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                 DFBImageDescription    *dsc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     if (!dsc)
          return DFB_INVARG;

     dsc->caps = DICAPS_NONE;

     return DFB_OK;
}
