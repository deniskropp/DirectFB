/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <malloc.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surfaces.h>

#include <misc/gfx_util.h>
#include <misc/util.h>
#include <misc/mem.h>

#include <jpeglib.h>
#include <setjmp.h>
#include <math.h>

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, JPEG )

/*
 * private data struct of IDirectFBImageProvider_JPEG
 */
typedef struct {
     int                  ref;      /* reference counter */

     IDirectFBDataBuffer *buffer;
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
} buffer_source_mgr;

typedef buffer_source_mgr * buffer_src_ptr;

static void
buffer_init_source (j_decompress_ptr cinfo)
{
     DFBResult            ret;
     buffer_src_ptr       src    = (buffer_src_ptr) cinfo->src;
     IDirectFBDataBuffer *buffer = src->buffer;

     /* FIXME: support streamed buffers */
     ret = buffer->SeekTo( buffer, 0 );
     if (ret)
          DirectFBError( "(DirectFB/ImageProvider_JPEG) Unable to seek", ret );
}

static boolean
buffer_fill_input_buffer (j_decompress_ptr cinfo)
{
     DFBResult            ret;
     unsigned int         nbytes;
     buffer_src_ptr       src    = (buffer_src_ptr) cinfo->src;
     IDirectFBDataBuffer *buffer = src->buffer;

     ret = buffer->GetData( buffer, JPEG_PROG_BUF_SIZE, src->data, &nbytes );
     if (ret || nbytes <= 0) {
#if 0
          if (src->start_of_file)   /* Treat empty input file as fatal error */
               ERREXIT(cinfo, JERR_INPUT_EMPTY);
          WARNMS(cinfo, JWRN_JPEG_EOF);
#endif
          /* Insert a fake EOI marker */
          src->data[0] = (JOCTET) 0xFF;
          src->data[1] = (JOCTET) JPEG_EOI;
          nbytes = 2;
          
          if (ret)
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
jpeg_buffer_src (j_decompress_ptr cinfo, IDirectFBDataBuffer *buffer)
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
copy_line32( __u32 *dst, __u8 *src, int width)
{
     __u32 r, g , b;
     while (width--) {
          r = (*src++) << 16;
          g = (*src++) << 8;
          b = (*src++);
          *dst++ = (0xFF000000 |r|g|b);
     }
}

static void
copy_line24( __u8 *dst, __u8 *src, int width)
{
     while (width--) {
          dst[0] = src[2];
          dst[1] = src[1];
          dst[2] = src[0];

          dst += 3;
          src += 3;
     }
}

static void
copy_line16( __u16 *dst, __u8 *src, int width)
{
     __u32 r, g , b;
     while (width--) {
          r = (*src++ >> 3) << 11;
          g = (*src++ >> 2) << 5;
          b = (*src++ >> 3);
          *dst++ = (r|g|b);
     }
}

static void
copy_line15( __u16 *dst, __u8 *src, int width)
{
     __u32 r, g , b;
     while (width--) {
          r = (*src++ >> 3) << 10;
          g = (*src++ >> 3) << 5;
          b = (*src++ >> 3);
          *dst++ = (r|g|b);
     }
}

#ifdef SUPPORT_RGB332
static void
copy_line8( __u8 *dst, __u8 *src, int width)
{
     __u32 r, g , b;
     while (width--) {
          r = (*src++ >> 5) << 5;
          g = (*src++ >> 5) << 2;
          b = (*src++ >> 6);
          *dst++ = (r|g|b);
     }
}
#endif

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (strncmp (ctx->header + 6, "JFIF", 4) == 0 ||
         strncmp (ctx->header + 6, "Exif", 4) == 0)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_JPEG)

     data->ref    = 1;
     data->buffer = buffer;

     buffer->AddRef( buffer );

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

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBImageProvider_JPEG_AddRef( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_Release( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

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
     int                    direct;
     DFBRectangle           rect = { 0, 0, 0, 0};
     DFBSurfacePixelFormat  format;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;

     INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     err = destination->GetPixelFormat( destination, &format );
     if (err)
          return err;

     switch (format) {
#ifdef SUPPORT_RGB332
          case DSPF_RGB332:
#endif
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
               direct = 1;
               break;

          case DSPF_LUT8:
          default:
               direct = 0;
               break;
     }

     err = destination->GetSize( destination, &rect.w, &rect.h );
     if (err)
          return err;

     if (dest_rect && !dfb_rectangle_intersect( &rect, dest_rect ))
          return DFB_OK;

     err = destination->Lock( destination, DSLF_WRITE, &dst, &pitch );
     if (err)
          return err;

     dst += rect.x * DFB_BYTES_PER_PIXEL(format) + rect.y * pitch;

     /* actual loading and rendering */
     {
          struct jpeg_decompress_struct cinfo;
          struct my_error_mgr jerr;
          JSAMPARRAY buffer;      /* Output row buffer */
          int row_stride;         /* physical row width in output buffer */
          void *image_data;
          void *row_ptr;

          cinfo.err = jpeg_std_error(&jerr.pub);
          jerr.pub.error_exit = jpeglib_panic;

          if (setjmp(jerr.setjmp_buffer)) {
               jpeg_destroy_decompress(&cinfo);
               destination->Unlock( destination );
               return DFB_FAILURE;
          }

          jpeg_create_decompress(&cinfo);
          jpeg_buffer_src(&cinfo, data->buffer);
          jpeg_read_header(&cinfo, TRUE);

          cinfo.out_color_space = JCS_RGB;
          cinfo.output_components = 3;
          jpeg_start_decompress(&cinfo);

          row_stride = cinfo.output_width * 3;

          buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo,
                                              JPOOL_IMAGE, row_stride, 1);

          if (rect.w == cinfo.output_width && rect.h == cinfo.output_height && direct) {
               /* image must not be scaled */
               row_ptr = dst;

               while (cinfo.output_scanline < cinfo.output_height) {
                    jpeg_read_scanlines(&cinfo, buffer, 1);
                    switch (format) {
#ifdef SUPPORT_RGB332
                         case DSPF_RGB332:
                              copy_line8( (__u8*)row_ptr, *buffer,
                                          cinfo.output_width);
                              break;
#endif
                         case DSPF_RGB16:
                              copy_line16( (__u16*)row_ptr, *buffer,
                                           cinfo.output_width);
                              break;
                         case DSPF_RGB15:
                              copy_line15( (__u16*)row_ptr, *buffer,
                                           cinfo.output_width);
                              break;
                         case DSPF_RGB24:
                              copy_line24( row_ptr, *buffer,
                                           cinfo.output_width);
                              break;
                         case DSPF_ARGB:
                         case DSPF_RGB32:
                              copy_line32( (__u32*)row_ptr, *buffer,
                                           cinfo.output_width);
                              break;
                         default:
                              BUG("unsupported format not filtered before");
                              return DFB_BUG;
                    }
                    (__u8*)row_ptr += pitch;
               }
          }
          else {     /* image must be scaled */
               image_data = malloc(cinfo.output_width * cinfo.output_height*4);
               row_ptr = image_data;

               while (cinfo.output_scanline < cinfo.output_height) {
                    jpeg_read_scanlines(&cinfo, buffer, 1);
                    copy_line32( (__u32*)row_ptr, *buffer, cinfo.output_width);
                    (__u32*)row_ptr += cinfo.output_width;
               }
               dfb_scale_linear_32( dst, image_data, cinfo.output_width,
                                    cinfo.output_height, rect.w, rect.h,
                                    pitch, format, dst_surface->palette );

               free( image_data );
          }

          jpeg_finish_decompress(&cinfo);
          jpeg_destroy_decompress(&cinfo);
     }

     err = destination->Unlock( destination );
     if (err)
          return err;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_SetRenderCallback( IDirectFBImageProvider *thiz,
                                               DIRenderCallback        callback,
                                               void                   *context )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_JPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                   DFBSurfaceDescription  *dsc )
{
     struct jpeg_decompress_struct cinfo;
     struct my_error_mgr jerr;

     INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     cinfo.err = jpeg_std_error(&jerr.pub);
     jerr.pub.error_exit = jpeglib_panic;

     if (setjmp(jerr.setjmp_buffer)) {
          jpeg_destroy_decompress(&cinfo);
          return DFB_FAILURE;
     }

     jpeg_create_decompress(&cinfo);
     jpeg_buffer_src(&cinfo, data->buffer);
     jpeg_read_header(&cinfo, TRUE);
     jpeg_start_decompress(&cinfo);

     dsc->flags  = DSDESC_WIDTH |  DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->height = cinfo.output_height;
     dsc->width  = cinfo.output_width;
     dsc->pixelformat = dfb_primary_layer_pixelformat();

     jpeg_destroy_decompress(&cinfo);

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                 DFBImageDescription    *dsc )
{
     INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     if (!dsc)
          return DFB_INVARG;

     dsc->caps = DICAPS_NONE;

     return DFB_OK;
}

