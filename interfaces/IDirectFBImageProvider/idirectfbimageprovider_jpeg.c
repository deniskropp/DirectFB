/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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
           const char             *filename );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, JPEG )

/*
 * private data struct of IDirectFBImageProvider_JPEG
 */
typedef struct {
     int            ref;      /* reference counter */
     char          *filename; /* filename of file to load */
} IDirectFBImageProvider_JPEG_data;

static DFBResult
IDirectFBImageProvider_JPEG_AddRef  ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_JPEG_Release ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_JPEG_RenderTo( IDirectFBImageProvider *thiz,
                                      IDirectFBSurface       *destination,
                                      DFBRectangle           *destination_rect );

static DFBResult
IDirectFBImageProvider_JPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                   DFBSurfaceDescription  *dsc);

static DFBResult
IDirectFBImageProvider_JPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                 DFBImageDescription    *dsc );


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
           const char             *filename )
{
     IDirectFBImageProvider_JPEG_data *data;

     data = (IDirectFBImageProvider_JPEG_data*)
          DFBCALLOC( 1, sizeof(IDirectFBImageProvider_JPEG_data) );

     thiz->priv = data;

     data->ref = 1;
     data->filename = (char*)DFBMALLOC( strlen(filename)+1 );
     strcpy( data->filename, filename );

     DEBUGMSG( "DirectFB/Media: JPEG Provider Construct '%s'\n", filename );

     thiz->AddRef = IDirectFBImageProvider_JPEG_AddRef;
     thiz->Release = IDirectFBImageProvider_JPEG_Release;
     thiz->RenderTo = IDirectFBImageProvider_JPEG_RenderTo;
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

     DFBFREE( data->filename );

     DFBFREE( thiz->priv );
     thiz->priv = NULL;

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
                                      DFBRectangle           *dest_rect )
{
     int err;
     void *dst;
     int pitch;
     DFBRectangle rect = { 0, 0, 0, 0 };
     DFBSurfacePixelFormat format;
     DFBSurfaceCapabilities caps;

     INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

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
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     err = destination->GetCapabilities( destination, &caps );
     if (err)
          return err;

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
          FILE *f;
          int row_stride;         /* physical row width in output buffer */
          void *image_data;
          void *row_ptr;

          f = fopen( data->filename, "rb" );
          if (!f) {
               destination->Unlock( destination );
               switch (errno) {
                    case EACCES:
                         return DFB_ACCESSDENIED;
                    case EIO:
                         return DFB_IO;
                    case ENOENT:
                         return DFB_FILENOTFOUND;
                    default:
                         return DFB_FAILURE;
               }
          }

          cinfo.err = jpeg_std_error(&jerr.pub);
          jerr.pub.error_exit = jpeglib_panic;

          if (setjmp(jerr.setjmp_buffer)) {
               jpeg_destroy_decompress(&cinfo);
               fclose(f);
               destination->Unlock( destination );
               return DFB_FAILURE;
          }

          jpeg_create_decompress(&cinfo);
          jpeg_stdio_src(&cinfo, f);
          jpeg_read_header(&cinfo, TRUE);

          cinfo.out_color_space = JCS_RGB;
          cinfo.output_components = 3;
          jpeg_start_decompress(&cinfo);

          row_stride = cinfo.output_width * 3;

          buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo,
                                              JPOOL_IMAGE, row_stride, 1);

          if (rect.w == cinfo.output_width && rect.h == cinfo.output_height) {
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
                                    pitch, format );

               free( image_data );
          }

          jpeg_finish_decompress(&cinfo);
          jpeg_destroy_decompress(&cinfo);
          fclose( f );
     }

     err = destination->Unlock( destination );
     if (err)
          return err;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                   DFBSurfaceDescription  *dsc )
{
     FILE *f;

     INTERFACE_GET_DATA(IDirectFBImageProvider_JPEG)

     f = fopen( data->filename, "rb" );
     if (!f) {
          switch (errno) {
               case EACCES:
                    return DFB_ACCESSDENIED;
               case EIO:
                    return DFB_IO;
               case ENOENT:
                    return DFB_FILENOTFOUND;
               default:
                    return DFB_FAILURE;
          }
     }

     {
          struct jpeg_decompress_struct cinfo;
          struct my_error_mgr jerr;

          cinfo.err = jpeg_std_error(&jerr.pub);
          jerr.pub.error_exit = jpeglib_panic;

          if (setjmp(jerr.setjmp_buffer)) {
               jpeg_destroy_decompress(&cinfo);
               fclose(f);
               return DFB_FAILURE;
          }

          jpeg_create_decompress(&cinfo);
          jpeg_stdio_src(&cinfo, f);
          jpeg_read_header(&cinfo, TRUE);
          jpeg_start_decompress(&cinfo);

          dsc->flags  = DSDESC_WIDTH |  DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
          dsc->height = cinfo.output_height;
          dsc->width  = cinfo.output_width;
          dsc->pixelformat = dfb_primary_layer_pixelformat();

          jpeg_destroy_decompress(&cinfo);
          fclose(f);
     }

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

