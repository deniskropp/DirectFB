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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <png.h>
#include <string.h>

#include <directfb.h>
#include <interface.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>

#include <misc/gfx_util.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <misc/util.h>

#include "config.h"

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
     png_uint_32          color_key;
     bool                 color_keyed;

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
          D_FREE( data->image );

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
          D_FREE( data->image );

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

          dfb_scale_linear_32( data->image, data->width, data->height,
                               dst, pitch, &rect, dst_surface );

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

     dsc->caps = DICAPS_NONE;

     if (data->color_type & PNG_COLOR_MASK_ALPHA)
          dsc->caps |= DICAPS_ALPHACHANNEL;

     if (data->color_keyed) {
          dsc->caps |= DICAPS_COLORKEY;

          dsc->colorkey_r = (data->color_key & 0xff0000) >> 16;
          dsc->colorkey_g = (data->color_key & 0x00ff00) >>  8;
          dsc->colorkey_b = (data->color_key & 0x0000ff);
     }

     return DFB_OK;
}


#define MAXCOLORMAPSIZE 256

static int SortColors (const void *a, const void *b)
{
     return (*((const __u8 *) a) - *((const __u8 *) b));
}

/*  looks for a color that is not in the colormap and ideally not
    even close to the colors used in the colormap  */
static __u32 FindColorKey( int n_colors, __u8 cmap[3][MAXCOLORMAPSIZE] )
{
     __u32 color = 0xFF000000;
     __u8  csort[MAXCOLORMAPSIZE];
     int   i, j, index, d;

     if (n_colors < 1)
          return color;

     D_ASSERT( n_colors <= MAXCOLORMAPSIZE );

     for (i = 0; i < 3; i++) {
          direct_memcpy( csort, cmap[i], n_colors );
          qsort( csort, n_colors, 1, SortColors );

          for (j = 1, index = 0, d = 0; j < n_colors; j++) {
               if (csort[j] - csort[j-1] > d) {
                    d = csort[j] - csort[j-1];
                    index = j;
               }
          }
          if ((csort[0] - 0x0) > d) {
               d = csort[0] - 0x0;
               index = n_colors;
          }
          if (0xFF - (csort[n_colors - 1]) > d) {
               index = n_colors + 1;
          }

          if (index < n_colors)
               csort[0] = csort[index] - (d/2);
          else if (index == n_colors)
               csort[0] = 0x0;
          else
               csort[0] = 0xFF;

          color |= (csort[0] << (8 * (2 - i)));
     }

     return color;
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

     if (png_get_valid( data->png_ptr, data->info_ptr, PNG_INFO_tRNS )) {
          data->color_keyed = true;

          /* generate color key based on palette... */
          if (data->color_type == PNG_COLOR_TYPE_PALETTE) {
               int        i;
               __u32      key;
               png_colorp palette    = data->info_ptr->palette;
               png_bytep  trans      = data->info_ptr->trans;
               int        num_colors = MIN( MAXCOLORMAPSIZE,
                                            data->info_ptr->num_palette );
               __u8       cmap[3][num_colors];

               for (i=0; i<num_colors; i++) {
                    cmap[0][i] = palette[i].red;
                    cmap[1][i] = palette[i].green;
                    cmap[2][i] = palette[i].blue;
               }

               key = FindColorKey( num_colors, cmap );

               for (i=0; i<data->info_ptr->num_trans; i++) {
                    if (!trans[i]) {
                         //trans[i] = 0;

                         palette[i].red   = (key & 0xff0000) >> 16;
                         palette[i].green = (key & 0x00ff00) >>  8;
                         palette[i].blue  = (key & 0x0000ff);
                    }
               }

               data->color_key = key;
          }
          else {
               /* ...or based on trans rgb value */
               png_color_16p trans = &data->info_ptr->trans_values;

               D_WARN("color key from non-palette source is untested");

               data->color_key = (((trans->red & 0xff00) << 8) |
                                  ((trans->green & 0xff00)) |
                                  ((trans->blue & 0xff00) >> 8));
          }
     }

     if (data->bpp == 16)
          png_set_strip_16( data->png_ptr );

#ifdef WORDS_BIGENDIAN
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
          data->image = D_MALLOC( size );
          if (!data->image) {
               D_ERROR("DirectFB/ImageProvider_PNG: Could not "
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

          if (buffer->WaitForData( buffer, 1 ) == DFB_BUFFEREMPTY)
               return DFB_FAILURE;

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
