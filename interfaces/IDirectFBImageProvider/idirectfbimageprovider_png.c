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

#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <png.h>
#include <string.h>

#include <malloc.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>

#include <misc/gfx_util.h>
#include <misc/mem.h>
#include <misc/util.h>

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, PNG )


enum {
     STAGE_ERROR = -1,
     STAGE_START =  0,
     STAGE_INFO,
     STAGE_IMAGE,
     STAGE_END
};

/*
 * private data struct of IDirectFBImageProvider_PNG
 */
typedef struct {
     int                  ref;      /* reference counter */
     IDirectFBDataBuffer *buffer;

     int                  stage;
     int                  rows;

     png_structp          png_ptr;
     png_infop            info_ptr;

     png_uint_32          width;
     png_uint_32          height;
     int                  bpp;
     int                  color_type;

     __u32               *image;

     DIRenderCallback     render_callback;
     void                *render_callback_context;
} IDirectFBImageProvider_PNG_data;

static DFBResult
IDirectFBImageProvider_PNG_AddRef  ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_PNG_Release ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_PNG_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *destination_rect );

static DFBResult
IDirectFBImageProvider_PNG_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *context );

static DFBResult
IDirectFBImageProvider_PNG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *dsc );

static DFBResult
IDirectFBImageProvider_PNG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc );

/* Called at the start of the progressive load, once we have image info */
static void
png_info_callback (png_structp png_read_ptr,
                   png_infop   png_info_ptr);

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void
png_row_callback  (png_structp png_read_ptr,
                   png_bytep   new_row,
                   png_uint_32 row_num,
                   int         pass_num);

/* Called after reading the entire image */
static void
png_end_callback  (png_structp png_read_ptr,
                   png_infop   png_info_ptr);

/* Pipes data into libpng until stage is different from the one specified. */
static DFBResult
push_data_until_stage (IDirectFBImageProvider_PNG_data *data,
                       int                              stage,
                       int                              buffer_size);

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (png_check_sig( ctx->header, 8 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     DFBResult ret = DFB_FAILURE;

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_PNG)

     data->ref    = 1;
     data->buffer = buffer;

     /* Increase the data buffer reference counter. */
     buffer->AddRef( buffer );
     
     /* Create the PNG read handle. */
     data->png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING,
                                             NULL, NULL, NULL );
     if (!data->png_ptr)
          goto error;

     /* Create the PNG info handle. */
     data->info_ptr = png_create_info_struct( data->png_ptr );
     if (!data->info_ptr)
          goto error;

     /* Setup progressive image loading. */
     png_set_progressive_read_fn( data->png_ptr, data,
                                  png_info_callback,
                                  png_row_callback,
                                  png_end_callback );

     
     /* Read until info callback is called. */
     ret = push_data_until_stage( data, STAGE_INFO, 4 );
     if (ret)
          goto error;

     thiz->AddRef = IDirectFBImageProvider_PNG_AddRef;
     thiz->Release = IDirectFBImageProvider_PNG_Release;
     thiz->RenderTo = IDirectFBImageProvider_PNG_RenderTo;
     thiz->SetRenderCallback = IDirectFBImageProvider_PNG_SetRenderCallback;
     thiz->GetImageDescription = IDirectFBImageProvider_PNG_GetImageDescription;
     thiz->GetSurfaceDescription =
                              IDirectFBImageProvider_PNG_GetSurfaceDescription;

     return DFB_OK;

error:
     if (data->png_ptr)
          png_destroy_read_struct( &data->png_ptr, &data->info_ptr, NULL );

     buffer->Release( buffer );
     
     if (data->image)
          DFBFREE( data->image );
     
     DFB_DEALLOCATE_INTERFACE(thiz);

     return ret;
}

static void
IDirectFBImageProvider_PNG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_PNG_data *data =
                              (IDirectFBImageProvider_PNG_data*)thiz->priv;

     png_destroy_read_struct( &data->png_ptr, &data->info_ptr, NULL );
     
     /* Decrease the data buffer reference counter. */
     data->buffer->Release( data->buffer );

     /* Deallocate image data. */
     if (data->image)
          DFBFREE( data->image );
     
     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBImageProvider_PNG_AddRef( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_PNG_Release( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)

     if (--data->ref == 0) {
          IDirectFBImageProvider_PNG_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_PNG_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *dest_rect )
{
     DFBResult              ret;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     DFBRectangle           rect = { 0, 0, 0, 0 };

     INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     ret = destination->GetSize( destination, &rect.w, &rect.h );
     if (ret)
          return ret;

     /* Read until image is completely decoded. */
     ret = push_data_until_stage( data, STAGE_END, 4096 );
     if (ret)
          return ret;

     /* actual rendering */
     if (dest_rect == NULL || dfb_rectangle_intersect ( &rect, dest_rect )) {
          void *dst;
          int   pitch;

          ret = destination->Lock( destination, DSLF_WRITE, &dst, &pitch );
          if (ret)
               return ret;

          dst += (rect.x * DFB_BYTES_PER_PIXEL(dst_surface->format)
                  + rect.y * pitch);

          dfb_scale_linear_32( (__u32*)dst, (__u32*)data->image, data->width,
                               data->height, rect.w, rect.h, pitch,
                               dst_surface->format, dst_surface->palette );
          
          destination->Unlock( destination );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_PNG_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *context )
{
     INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)

     data->render_callback         = callback;
     data->render_callback_context = context;

     return DFB_UNIMPLEMENTED;
}

/* Loading routines */

static DFBResult
IDirectFBImageProvider_PNG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription *dsc )
{
     INTERFACE_GET_DATA (IDirectFBImageProvider_PNG)
          
     dsc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width  = data->width;
     dsc->height = data->height;

     if (data->color_type & PNG_COLOR_MASK_ALPHA)
          dsc->pixelformat = DSPF_ARGB;
     else
          dsc->pixelformat = dfb_primary_layer_pixelformat();
     
     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_PNG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc )
{
     INTERFACE_GET_DATA(IDirectFBImageProvider_PNG)

     if (!dsc)
          return DFB_INVARG;

     /* FIXME: colorkeyed PNGs are currently converted to alphachannel PNGs */
     if (data->color_type & PNG_COLOR_MASK_ALPHA)
          dsc->caps = DICAPS_ALPHACHANNEL;
     else
          dsc->caps = DICAPS_NONE;
     
     return DFB_OK;
}


/* Called at the start of the progressive load, once we have image info */
static void
png_info_callback   (png_structp png_read_ptr,
                     png_infop   png_info_ptr)
{
     IDirectFBImageProvider_PNG_data *data;

     data = png_get_progressive_ptr( png_read_ptr );

     /* error stage? */
     if (data->stage < 0)
          return;

     /* set info stage */
     data->stage = STAGE_INFO;

     png_get_IHDR( data->png_ptr, data->info_ptr,
                   &data->width, &data->height, &data->bpp, &data->color_type,
                   NULL, NULL, NULL );
     
     if (data->color_type == PNG_COLOR_TYPE_PALETTE)
          png_set_palette_to_rgb( data->png_ptr );

     if (data->color_type == PNG_COLOR_TYPE_GRAY
         || data->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
          png_set_gray_to_rgb( data->png_ptr );

     if (png_get_valid( data->png_ptr, data->info_ptr, PNG_INFO_tRNS ))
          png_set_tRNS_to_alpha( data->png_ptr );

     if (data->bpp == 16)
          png_set_strip_16( data->png_ptr );

#if __BYTE_ORDER == __BIG_ENDIAN
     if (!(data->color_type & PNG_COLOR_MASK_ALPHA))
          png_set_filler( data->png_ptr, 0xFF, PNG_FILLER_BEFORE );

     png_set_swap_alpha( data->png_ptr );
#else
     if (!(data->color_type & PNG_COLOR_MASK_ALPHA))
          png_set_filler( data->png_ptr, 0xFF, PNG_FILLER_AFTER );

     png_set_bgr( data->png_ptr );
#endif
     
     png_set_interlace_handling( data->png_ptr );

     /* Update the info to reflect our transformations */
     png_read_update_info( data->png_ptr, data->info_ptr );
}

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void
png_row_callback   (png_structp png_read_ptr,
                    png_bytep   new_row,
                    png_uint_32 row_num,
                    int         pass_num)
{
     IDirectFBImageProvider_PNG_data *data;

     data = png_get_progressive_ptr( png_read_ptr );

     /* error stage? */
     if (data->stage < 0)
          return;

     /* set image decoding stage */
     data->stage = STAGE_IMAGE;

     /* check image data pointer */
     if (!data->image) {
          int size = data->width * data->height * 4;

          /* allocate image data */
          data->image = DFBMALLOC( size );
          if (!data->image) {
               ERRORMSG("DirectFB/ImageProvider_PNG: Could not "
                        "allocate %d bytes of system memory!\n", size);

               /* set error stage */
               data->stage = STAGE_ERROR;

               return;
          }
     }

     /* write to image data */
     png_progressive_combine_row( data->png_ptr, (png_bytep) (data->image +
                                  row_num * data->width), new_row );

     /* increase row counter, FIXME: interlaced? */
     data->rows++;
}

/* Called after reading the entire image */
static void
png_end_callback   (png_structp png_read_ptr,
                    png_infop   png_info_ptr)
{
     IDirectFBImageProvider_PNG_data *data;

     data = png_get_progressive_ptr( png_read_ptr );

     /* error stage? */
     if (data->stage < 0)
          return;
     
     /* set end stage */
     data->stage = STAGE_END;
}

/* Pipes data into libpng until stage is different from the one specified. */
static DFBResult
push_data_until_stage (IDirectFBImageProvider_PNG_data *data,
                       int                              stage,
                       int                              buffer_size)
{
     DFBResult            ret;
     IDirectFBDataBuffer *buffer = data->buffer;

     while (data->stage < stage) {
          unsigned int  len;
          unsigned char buf[buffer_size];

          if (data->stage < 0)
               return DFB_FAILURE;

          buffer->WaitForData( buffer, 1 );
          
          while (buffer->HasData( buffer ) == DFB_OK) {
               ret = buffer->GetData( buffer, buffer_size, buf, &len );
               if (ret)
                    return ret;

               png_process_data( data->png_ptr, data->info_ptr, buf, len );

               /* are we there yet? */
               if (data->stage < 0 || data->stage >= stage)
                    break;
          }
     }

     return DFB_OK;
}
