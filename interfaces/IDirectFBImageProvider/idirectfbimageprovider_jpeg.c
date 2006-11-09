/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surfaces.h>

#include <misc/gfx_util.h>
#include <misc/util.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <jpeglib.h>
#include <setjmp.h>
#include <math.h>

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
     int                  ref;      /* reference counter */

     IDirectFBDataBuffer *buffer;

     DIRenderCallback     render_callback;
     void                *render_callback_context;

     u32                 *image;
     int                  width;
     int                  height;

     CoreDFB             *core;
} IDirectFBImageProvider_JPEG_data;

static DFBResult
IDirectFBImageProvider_JPEG_AddRef  ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_JPEG_Release ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_JPEG_RenderTo( IDirectFBImageProvider *thiz,
                                      IDirectFBSurface       *destination,
                                      const DFBRectangle     *destination_rect );

static DFBResult
IDirectFBImageProvider_JPEG_SetRenderCallback( IDirectFBImageProvider *thiz,
                                               DIRenderCallback        callback,
                                               void                   *context );

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


static void
copy_line32( u32 *dst, u8 *src, int width)
{
     u32 r, g , b;
     while (width--) {
          r = (*src++) << 16;
          g = (*src++) << 8;
          b = (*src++);
          *dst++ = (0xFF000000 |r|g|b);
     }
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

     data->ref    = 1;
     data->buffer = buffer;
     data->core   = core;

     buffer->AddRef( buffer );

     cinfo.err = jpeg_std_error(&jerr.pub);
     jerr.pub.error_exit = jpeglib_panic;

     if (setjmp(jerr.setjmp_buffer)) {
          jpeg_destroy_decompress(&cinfo);
          buffer->Release( buffer );
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

     thiz->AddRef = IDirectFBImageProvider_JPEG_AddRef;
     thiz->Release = IDirectFBImageProvider_JPEG_Release;
     thiz->RenderTo = IDirectFBImageProvider_JPEG_RenderTo;
     thiz->SetRenderCallback = IDirectFBImageProvider_JPEG_SetRenderCallback;
     thiz->GetImageDescription =IDirectFBImageProvider_JPEG_GetImageDescription;
     thiz->GetSurfaceDescription =
     IDirectFBImageProvider_JPEG_GetSurfaceDescription;

     return DFB_OK;
}

static void
IDirectFBImageProvider_JPEG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_JPEG_data *data =
                              (IDirectFBImageProvider_JPEG_data*)thiz->priv;

     data->buffer->Release( data->buffer );

     if (data->image)
          D_FREE( data->image );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBImageProvider_JPEG_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     if (--data->ref == 0) {
          IDirectFBImageProvider_JPEG_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_RenderTo( IDirectFBImageProvider *thiz,
                                      IDirectFBSurface       *destination,
                                      const DFBRectangle     *dest_rect )
{
     int                    err;
     void                  *dst;
     int                    pitch;
     int                    direct = 0;
     DFBRegion              clip;
     DFBRectangle           rect;
     DFBSurfacePixelFormat  format;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     DIRenderCallbackResult cb_result = DIRCR_OK;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     err = destination->GetPixelFormat( destination, &format );
     if (err)
          return err;

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

     err = dfb_surface_soft_lock( data->core, dst_surface, DSLF_WRITE, &dst, &pitch, 0 );
     if (err)
          return err;

     /* actual loading and rendering */
     if (!data->image) {
          struct jpeg_decompress_struct cinfo;
          struct my_error_mgr jerr;
          JSAMPARRAY buffer;      /* Output row buffer */
          int row_stride;         /* physical row width in output buffer */
          u32 *row_ptr;
          int y = 0;

          cinfo.err = jpeg_std_error(&jerr.pub);
          jerr.pub.error_exit = jpeglib_panic;

          if (setjmp(jerr.setjmp_buffer)) {
               jpeg_destroy_decompress(&cinfo);
               destination->Unlock( destination );
               return DFB_FAILURE;
          }

          jpeg_create_decompress(&cinfo);
          jpeg_buffer_src(&cinfo, data->buffer, 0);
          jpeg_read_header(&cinfo, TRUE);

          cinfo.out_color_space = JCS_RGB;
          cinfo.output_components = 3;
          jpeg_start_decompress(&cinfo);

          data->width = cinfo.output_width;
          data->height = cinfo.output_height;

          row_stride = cinfo.output_width * 3;

          buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo,
                                              JPOOL_IMAGE, row_stride, 1);

          data->image = D_MALLOC( data->width * data->height * 4 );
          if (!data->image)
               return D_OOM();
          row_ptr = data->image;

          direct = (rect.w == data->width && rect.h == data->height);

          while (cinfo.output_scanline < cinfo.output_height && cb_result == DIRCR_OK) {
               jpeg_read_scanlines(&cinfo, buffer, 1);
               copy_line32( row_ptr, *buffer, data->width);
               
               if (direct) {
                    DFBRectangle r = { rect.x, rect.y+y, rect.w, 1 };
                    dfb_copy_buffer_32( row_ptr, dst, pitch,
                                        &r, dst_surface, &clip );
                    if (data->render_callback) {
                         r = (DFBRectangle){ 0, y, data->width, 1 };
                         cb_result = data->render_callback( &r, 
                                             data->render_callback_context );
                    }
               }

               row_ptr += data->width;
               y++;
          }

          if (!direct) {
               dfb_scale_linear_32( data->image, data->width, data->height,
                                    dst, pitch, &rect, dst_surface, &clip );
               if (data->render_callback) {
                    DFBRectangle r = { 0, 0, data->width, data->height };
                    data->render_callback( &r, data->render_callback_context );
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
          dfb_scale_linear_32( data->image, data->width, data->height,
                               dst, pitch, &rect, dst_surface, &clip );
          if (data->render_callback) {
               DFBRectangle r = { 0, 0, data->width, data->height };
               data->render_callback( &r, data->render_callback_context );
          }
     }
     
     dfb_surface_unlock( dst_surface, 0 );

     if (cb_result != DIRCR_OK)
         return DFB_INTERRUPTED;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_SetRenderCallback( IDirectFBImageProvider *thiz,
                                               DIRenderCallback        callback,
                                               void                   *context )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_JPEG)

     data->render_callback         = callback;
     data->render_callback_context = context;

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

