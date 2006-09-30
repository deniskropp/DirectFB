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
#include <string.h>
#include <stdarg.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>

#include <misc/gfx_util.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <misc/util.h>

#include "mpeg2/mpeg2dec.h"

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, MPEG2 )


enum {
     STAGE_ERROR = -1,
     STAGE_START =  0,
     STAGE_INFO,
     STAGE_IMAGE,
     STAGE_END
};

/*
 * private data struct of IDirectFBImageProvider_MPEG2
 */
typedef struct {
     int                  ref;      /* reference counter */
     IDirectFBDataBuffer *buffer;

     MPEG2_Decoder       *dec;

     int                  stage;

     int                  width;
     int                  height;

     __u32               *image;
} IDirectFBImageProvider_MPEG2_data;

static DFBResult
IDirectFBImageProvider_MPEG2_AddRef  ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_MPEG2_Release ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_MPEG2_RenderTo( IDirectFBImageProvider *thiz,
                                       IDirectFBSurface       *destination,
                                       const DFBRectangle     *destination_rect );

static DFBResult
IDirectFBImageProvider_MPEG2_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                DIRenderCallback        callback,
                                                void                   *context );

static DFBResult
IDirectFBImageProvider_MPEG2_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                    DFBSurfaceDescription  *dsc );

static DFBResult
IDirectFBImageProvider_MPEG2_GetImageDescription( IDirectFBImageProvider *thiz,
                                                  DFBImageDescription    *dsc );


static int  mpeg2_read_func ( void *buf, int count, void *ctx );
static void mpeg2_write_func( int x, int y, __u32 argb, void *ctx );

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     unsigned char *sig = (unsigned char*) ctx->header;

     if (sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 0x01 && sig[3] == 0xb3)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     DFBResult ret = DFB_FAILURE;
     IDirectFBDataBuffer *buffer;
     va_list tag;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_MPEG2)
       
     va_start(tag, thiz);
     buffer = va_arg(tag, IDirectFBDataBuffer *);
     va_end(tag);
       
     data->ref    = 1;
     data->buffer = buffer;

     /* Increase the data buffer reference counter. */
     buffer->AddRef( buffer );

     /* Initialize mpeg2 decoding. */
     data->dec = MPEG2_Init( mpeg2_read_func, buffer, &data->width, &data->height );
     if (!data->dec)
          goto error;

     data->stage = STAGE_INFO;

     /* Allocate image data. */
     data->image = D_MALLOC( data->width * data->height * 4 );
     if (!data->image)
          goto error;

     data->stage = STAGE_IMAGE;

     thiz->AddRef = IDirectFBImageProvider_MPEG2_AddRef;
     thiz->Release = IDirectFBImageProvider_MPEG2_Release;
     thiz->RenderTo = IDirectFBImageProvider_MPEG2_RenderTo;
     thiz->SetRenderCallback = IDirectFBImageProvider_MPEG2_SetRenderCallback;
     thiz->GetImageDescription = IDirectFBImageProvider_MPEG2_GetImageDescription;
     thiz->GetSurfaceDescription =
                              IDirectFBImageProvider_MPEG2_GetSurfaceDescription;

     return DFB_OK;

error:
     if (data->dec)
          MPEG2_Close(data->dec);

     buffer->Release( buffer );

     DIRECT_DEALLOCATE_INTERFACE(thiz);

     return ret;
}

static void
IDirectFBImageProvider_MPEG2_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_MPEG2_data *data =
                              (IDirectFBImageProvider_MPEG2_data*)thiz->priv;

     MPEG2_Close(data->dec);

     /* Decrease the data buffer reference counter. */
     data->buffer->Release( data->buffer );

     /* Deallocate image data. */
     if (data->image)
          D_FREE( data->image );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBImageProvider_MPEG2_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_MPEG2)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_MPEG2_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_MPEG2)

     if (--data->ref == 0)
          IDirectFBImageProvider_MPEG2_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_MPEG2_RenderTo( IDirectFBImageProvider *thiz,
                                       IDirectFBSurface       *destination,
                                       const DFBRectangle     *dest_rect )
{
     DFBResult              ret;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     DFBRegion              clip;
     DFBRectangle           rect;

     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_MPEG2)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     dfb_region_from_rectangle( &clip, &dst_data->area.current );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
          rect = *dest_rect;
          rect.w += dst_data->area.wanted.w;
          rect.h += dst_data->area.wanted.h;
     }
     else {
          rect = dst_data->area.wanted;
     }

     switch (data->stage) {
          case STAGE_END:
               break;
          case STAGE_IMAGE:
               if (MPEG2_Decode( data->dec, mpeg2_write_func, data )) {
                    data->stage = STAGE_ERROR;
                    return DFB_FAILURE;
               }
               data->stage = STAGE_END;
               break;
          default:
               return DFB_FAILURE;
     }

     /* actual rendering */
     if (dfb_rectangle_region_intersects( &rect, &clip )) {
          void *dst;
          int   pitch;

          ret = dfb_surface_soft_lock( dst_surface, DSLF_WRITE, &dst, &pitch, 0 );
          if (ret)
               return ret;

          dfb_scale_linear_32( data->image, data->width, data->height,
                               dst, pitch, &rect, dst_surface, &clip );

          dfb_surface_unlock( dst_surface, 0 );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_MPEG2_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                DIRenderCallback        callback,
                                                void                   *context )
{
     return DFB_UNIMPLEMENTED;
}

/* Loading routines */

static DFBResult
IDirectFBImageProvider_MPEG2_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                    DFBSurfaceDescription *dsc )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_MPEG2)

     dsc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width       = data->width;
     dsc->height      = data->height;
     dsc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_MPEG2_GetImageDescription( IDirectFBImageProvider *thiz,
                                                  DFBImageDescription    *dsc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_MPEG2)

     if (!dsc)
          return DFB_INVARG;

     dsc->caps = DICAPS_NONE;

     return DFB_OK;
}

/******************************************************************************/

static int
mpeg2_read_func( void *buf, int count, void *ctx )
{
     unsigned int         len;
     DFBResult            ret;
     IDirectFBDataBuffer *buffer = (IDirectFBDataBuffer*) ctx;

     buffer->WaitForData( buffer, 1 );

     ret = buffer->GetData( buffer, count, buf, &len );
     if (ret)
          return 0;

     return len;
}

static void
mpeg2_write_func( int x, int y, __u32 argb, void *ctx )
{
     IDirectFBImageProvider_MPEG2_data *data =
                                       (IDirectFBImageProvider_MPEG2_data*) ctx;

     data->image[ data->width * y + x ] = argb;
}
