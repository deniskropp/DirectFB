/*
   Copyright (c) 2013 Haithem Rahmani <haithem.rahmani@gmail.com>
based on code:
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Haithem Rahmani <haithem.rahmani@gmail.com>
   Based on code by:
              Andre' Draszik <andre.draszik@st.com>,
              Denis Oliver Kropp <dok@directfb.org>,
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
#include <string.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/surface.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/gfx_util.h>
#include <misc/util.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/interface.h>

#include <webp/decode.h>

D_DEBUG_DOMAIN( imageProviderWebP,  "ImageProvider/WebP",  "libWebP based image decoder" );

static DFBResult Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult Construct( IDirectFBImageProvider *thiz,
                            ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, WebP )

typedef struct {
     IDirectFBImageProvider_data base;

     WebPDecoderConfig           config;      /* WebP decoder config to intialize before starting the decoding */

     int                         width;       /* image width */
     int                         height;      /* image height */
     DFBSurfacePixelFormat       pixelformat; /* image pixelformat DSPF_ARGB or DFB_RGB24 */
     uint32_t                    image_size;  /* image data size */

     CoreSurface                *decode_surface; /* surface containing the decoded image*/
     CoreGraphicsSerial         *serial;
} IDirectFBImageProvider_WebP_data;

static void
IDirectFBImageProvider_WebP_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_WebP_data *data = thiz->priv;

     if (data->decode_surface) {
          dfb_gfxcard_wait_serial( data->serial );
          dfb_surface_unref( data->decode_surface );
     }

     if (data->base.buffer)
          data->base.buffer->Release( data->base.buffer );
}

static DFBResult
WebP_decode_image( IDirectFBImageProvider_WebP_data *data,
                   CoreSurfaceBufferLock  *lock )
{
     VP8StatusCode status = VP8_STATUS_NOT_ENOUGH_DATA;
     DFBResult ret;

     uint32_t read_size;
     u8 image[data->image_size];

     WebPIDecoder* WebP_dec;
     IDirectFBDataBuffer *buffer = data->base.buffer;

     WebP_dec = WebPINewDecoder( &data->config.output );

     data->config.output.colorspace = (data->pixelformat == DSPF_ARGB) ? MODE_bgrA : MODE_BGR;

     data->config.output.u.RGBA.rgba = (uint8_t*)lock->addr;
     data->config.output.u.RGBA.stride = lock->pitch;
     data->config.output.u.RGBA.size = lock->pitch * data->height;

     data->config.output.is_external_memory = 1;

     ret = DFB_OK;
     buffer->SeekTo( buffer, 0 );

     while (ret != DFB_EOF && buffer->HasData( buffer ) == DFB_OK) {
          ret = buffer->GetData( buffer, data->image_size, image, &read_size );

          status = WebPIAppend( WebP_dec, image, read_size );
          if (!(status == VP8_STATUS_OK || status == VP8_STATUS_SUSPENDED))
               break;
     }

     WebPIDelete( WebP_dec );

     return  (status == VP8_STATUS_OK) ? DFB_OK : DFB_FAILURE;
}

static DFBResult
IDirectFBImageProvider_WebP_RenderTo( IDirectFBImageProvider *thiz,
                                      IDirectFBSurface       *destination,
                                      const DFBRectangle     *dest_rect )
{
     DFBResult               ret;

     DFBRegion               clip;
     CoreSurface            *dst_surface;
     CardState               state;

     CoreSurfaceBufferLock   lock;

     IDirectFBSurface_data  *dst_data;
     DIRenderCallbackResult  cb_result = DIRCR_OK;

     DFBRectangle           src_rect;
     DFBRectangle           rect;

     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_WebP )

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


     ret = dfb_surface_create_simple( data->base.core, data->width, data->height, data->pixelformat,
                                      DSCS_RGB, DSCAPS_NONE, CSTF_NONE,
                                      0, NULL, &data->decode_surface );
     if (ret) {
          D_ERROR( "Failed to create surface : '%s'\n", DirectResultString(ret) );
          goto error;
     }

     ret = dfb_surface_lock_buffer( data->decode_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
     if (ret) {
          D_ERROR( "Failed to lock the surface : '%s'\n", DirectResultString(ret) );
          goto error;
     }

     ret = WebP_decode_image( data, &lock );
     if (ret) {
          D_ERROR( "Failed to decode the image : '%s'\n", DirectResultString(ret) );
          goto error;
     }

     dfb_surface_unlock_buffer( data->decode_surface, &lock );

     dfb_state_init( &state, data->base.core );

     state.modified |= SMF_CLIP;

     state.clip =  DFB_REGION_INIT_FROM_RECTANGLE_VALS( rect.x, rect.y, rect.w, rect.h );
     src_rect = (DFBRectangle){0, 0, data->width, data->height};

     dfb_state_set_source( &state, data->decode_surface );
     dfb_state_set_destination( &state, dst_surface );

     dfb_gfxcard_batchstretchblit( &src_rect, &rect, 1, &state );

     data->serial = &state.serial;

     dfb_gfxcard_wait_serial( data->serial );
     dfb_surface_unref( data->decode_surface );

     dfb_state_set_source(&state, NULL);
     dfb_state_set_destination(&state, NULL);

     dfb_state_destroy(&state);

     if (data->base.render_callback) {
          DFBRectangle r = { 0, 0, data->width, data->height };
          cb_result=data->base.render_callback( &r, data->base.render_callback_context );
     }

     return DFB_OK;

error:
     if (data->decode_surface && lock.pitch)
          dfb_surface_unlock_buffer( data->decode_surface, &lock );

     dfb_surface_unref( data->decode_surface );

     return ret;
}

static DFBResult
IDirectFBImageProvider_WebP_SetRenderCallback( IDirectFBImageProvider *thiz,
                                               DIRenderCallback        callback,
                                               void                   *ctx )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_WebP )

     data->base.render_callback = callback;
     data->base.render_callback_context = ctx;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_WebP_GetImageDescription( IDirectFBImageProvider *thiz,
                                                 DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_WebP )

     if (!desc)
          return DFB_INVARG;

     desc->caps =  DFB_PIXELFORMAT_HAS_ALPHA( data->pixelformat ) ?  DICAPS_ALPHACHANNEL : DICAPS_NONE;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_WebP_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                   DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_WebP )

     if (!desc)
          return DFB_INVARG;

     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_CAPS;

     desc->caps        = DFB_PIXELFORMAT_HAS_ALPHA( data->pixelformat ) ? DSCAPS_PREMULTIPLIED : DSCAPS_NONE;

     desc->width       = data->width;
     desc->height      = data->height;

     desc->pixelformat = data->pixelformat;

     return DFB_OK;
}

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{

     if (WebPGetInfo( ctx->header, D_ARRAY_SIZE( ctx->header ), NULL, NULL ) != 0)
	      return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
get_image_features( IDirectFBImageProvider_WebP_data *data )
{
     u32 buffer_size = 32;
     u32 read = 0;
     u8  buffer[buffer_size];

     WebPBitstreamFeatures image_features;
     DFBResult ret;

     ret = data->base.buffer->WaitForData( data->base.buffer, buffer_size );

     /*
      * Get the actual image size manually
      * as webp api doesn't return that info
      */
     if (ret == DFB_OK)
          ret = data->base.buffer->PeekData( data->base.buffer, 4, 4, &data->image_size, &read );

     data->image_size += 8;

     ret = data->base.buffer->PeekData( data->base.buffer, buffer_size, 0, buffer, &read );
     if (ret)
          return ret;

     if (WebPGetFeatures( buffer, buffer_size, &image_features ) != VP8_STATUS_OK)
          return DFB_FAILURE;

     data->width = image_features.width;
     data->height = image_features.height;
     data->pixelformat = image_features.has_alpha ? DSPF_ARGB : DSPF_RGB24;

     return DFB_OK;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     D_DEBUG_AT( imageProviderWebP, "%s(%d)\n", __FUNCTION__, __LINE__ );

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_WebP)

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     data->base.ref    = 1;
     data->base.buffer = buffer;
     data->base.core   = core;

     /* Increase the data buffer reference counter. */
     buffer->AddRef( buffer );

     if (get_image_features( data ))
	      goto error;

     D_DEBUG_AT( imageProviderWebP, "%s( %dx%d -%s)\n", __FUNCTION__,  data->width,
                 data->height, dfb_pixelformat_name( data->pixelformat ) );

     data->base.Destruct = IDirectFBImageProvider_WebP_Destruct;

     thiz->RenderTo              = IDirectFBImageProvider_WebP_RenderTo;
     thiz->GetImageDescription   = IDirectFBImageProvider_WebP_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_WebP_GetSurfaceDescription;
     thiz->SetRenderCallback     = IDirectFBImageProvider_WebP_SetRenderCallback;

     return DFB_OK;

error:
     buffer->Release( buffer );
     DIRECT_DEALLOCATE_INTERFACE(thiz);

     return DFB_FAILURE;
}
