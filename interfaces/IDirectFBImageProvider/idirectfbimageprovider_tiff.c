/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Haithem Rahmani <haithem.rahmani@gmail.com>
   Based on work done by:
              Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <errno.h>
#include <stdio.h>
#include <tiffio.h>
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
#include <core/surface.h>

#include <misc/gfx_util.h>
#include <misc/util.h>

#include <gfx/clip.h>
#include <gfx/convert.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include "config.h"

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, TIFF )


/*
 * private data struct of IDirectFBImageProvider_TIFF
 */
typedef struct {
     IDirectFBImageProvider_data base;

     TIFF                *tif;

     int                  image_width;
     int                  image_height;

     CoreSurface         *decode_surface;
     CoreGraphicsSerial  *serial;

     DIRenderCallback     render_callback;
     void                *render_callback_ctx;

} IDirectFBImageProvider_TIFF_data;


static tsize_t
_readTIFF( thandle_t, tdata_t, tsize_t );

static tsize_t
_writeTIFF( thandle_t, tdata_t, tsize_t );

static toff_t
_seekTIFF( thandle_t, toff_t, int );

static int
_closeTIFF( thandle_t );

static toff_t
_sizeTIFF( thandle_t );

static int
_mapdataTIFF( thandle_t, tdata_t*, toff_t* );

static void
_unmapdataTIFF( thandle_t, tdata_t, toff_t );

static DirectResult
IDirectFBImageProvider_TIFF_AddRef  ( IDirectFBImageProvider *thiz );

static DirectResult
IDirectFBImageProvider_TIFF_Release ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_TIFF_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *destination_rect );

static DFBResult
IDirectFBImageProvider_TIFF_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *context );

static DFBResult
IDirectFBImageProvider_TIFF_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *dsc );

static DFBResult
IDirectFBImageProvider_TIFF_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc );


/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     unsigned short tiff_magic = (ctx->header[0] | (ctx->header[1] << 8));

     if ((tiff_magic != TIFF_BIGENDIAN) &&
         (tiff_magic != TIFF_LITTLEENDIAN) &&
         (MDI_LITTLEENDIAN != tiff_magic))
     {
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_TIFF)

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     data->base.ref    = 1;
     data->base.buffer = buffer;
     data->base.core   = core;

     buffer->AddRef( buffer );

     data->tif = TIFFClientOpen( "TIFF", "rM", (thandle_t)data, _readTIFF,
                                _writeTIFF, _seekTIFF, _closeTIFF,
                                _sizeTIFF, _mapdataTIFF, _unmapdataTIFF );

     TIFFGetField( data->tif, TIFFTAG_IMAGEWIDTH, &(data->image_width) );
     TIFFGetField( data->tif, TIFFTAG_IMAGELENGTH, &(data->image_height) );
     if (!data->tif)
     {
          buffer->Release( buffer );
          data->base.buffer = NULL;
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     thiz->AddRef = IDirectFBImageProvider_TIFF_AddRef;
     thiz->Release = IDirectFBImageProvider_TIFF_Release;
     thiz->RenderTo = IDirectFBImageProvider_TIFF_RenderTo;
     thiz->SetRenderCallback = IDirectFBImageProvider_TIFF_SetRenderCallback;
     thiz->GetImageDescription = IDirectFBImageProvider_TIFF_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_TIFF_GetSurfaceDescription;

     return DFB_OK;

}

/**********************************************************************************************************************/

static void
IDirectFBImageProvider_TIFF_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_TIFF_data *data =
                                   (IDirectFBImageProvider_TIFF_data*)thiz->priv;

     if (data->decode_surface) {
          dfb_gfxcard_wait_serial( data->serial );
          dfb_surface_unref( data->decode_surface );
     }

     if (data->base.buffer)
          data->base.buffer->Release( data->base.buffer );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBImageProvider_TIFF_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_TIFF)

     data->base.ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBImageProvider_TIFF_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_TIFF )

     if (--data->base.ref == 0) {
          IDirectFBImageProvider_TIFF_Destruct( thiz );
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
IDirectFBImageProvider_TIFF_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *dest_rect )
{
     DFBResult              ret;
     DFBRegion              clip;

     IDirectFBSurface_data *dst_data;

     CoreSurface           *dst_surface;
     CardState              state;

     CoreSurfaceBufferLock  lock;

     DFBRectangle           rect;
     DFBRectangle           src_rect;

     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_TIFF)

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
          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }
     else {
          rect = dst_data->area.wanted;
     }

     if (!dfb_rectangle_region_intersects( &rect, &clip ))
          return DFB_OK;

     ret = dfb_surface_create_simple( data->base.core, data->image_width, data->image_height, DSPF_ARGB,
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

     TIFFReadRGBAImageOriented( data->tif, data->image_width, data->image_height, (uint32 *)(lock.addr), ORIENTATION_TOPLEFT, 0 );

     dfb_surface_unlock_buffer( data->decode_surface, &lock );

     dfb_state_init( &state, data->base.core );

     state.modified |= SMF_CLIP;

     state.clip =  DFB_REGION_INIT_FROM_RECTANGLE_VALS( rect.x, rect.y, rect.w, rect.h );
     src_rect = (DFBRectangle){0, 0, data->image_width, data->image_height};

     dfb_state_set_source( &state, data->decode_surface );
     dfb_state_set_destination( &state, dst_surface );

     dfb_gfxcard_batchstretchblit( &src_rect, &rect, 1, &state );

     data->serial = &state.serial;

     dfb_state_set_source( &state, NULL );
     dfb_state_set_destination( &state, NULL );

     dfb_state_destroy( &state );

     if (data->render_callback) {
          DIRenderCallbackResult r;

          rect.x = 0;
          rect.y = 0;
          rect.w = data->image_width;
          rect.h = data->image_height;

          r = data->render_callback( &rect, data->render_callback_ctx );

          if (r != DIRCR_OK)
                  return DFB_INTERRUPTED;
     }

     return DFB_OK;

error:
     if (data->decode_surface && lock.pitch)
          dfb_surface_unlock_buffer( data->decode_surface, &lock );

     dfb_surface_unref( data->decode_surface );

     data->base.buffer->Release( data->base.buffer );
     data->base.buffer = NULL;

     return ret;
}

static DFBResult
IDirectFBImageProvider_TIFF_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *context )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_TIFF )

     data->render_callback     = callback;
     data->render_callback_ctx = context;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_TIFF_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription *dsc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_TIFF )



     dsc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width       = data->image_width;
     dsc->height      = data->image_height;
     dsc->pixelformat = DSPF_ARGB;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_TIFF_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *dsc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_TIFF )

     dsc->caps = DICAPS_NONE;

     return DFB_OK;
}

static tsize_t
_readTIFF(thandle_t handle, tdata_t data, tsize_t size)
{
     unsigned int ret_size;

     IDirectFBImageProvider_TIFF_data *priv = (IDirectFBImageProvider_TIFF_data*)handle;
     priv->base.buffer->GetData( priv->base.buffer, size, data, &ret_size );

     return (tsize_t)ret_size;
}

static tsize_t
_writeTIFF( thandle_t handle, tdata_t data, tsize_t size )
{
     return -1;
}
static toff_t _seekTIFF( thandle_t handle, toff_t offset, int whence )
{
     unsigned int off;

     IDirectFBImageProvider_TIFF_data *data = (IDirectFBImageProvider_TIFF_data*)handle;

     switch (whence)
     {
          case SEEK_SET:
               off = offset;
               break;
          case SEEK_CUR:
               data->base.buffer->GetPosition( data->base.buffer, &off );
               off += offset;
               break;
          case SEEK_END:
               data->base.buffer->GetLength( data->base.buffer, &off );
               off += offset;
               break;
          default:
               data->base.buffer->GetPosition( data->base.buffer, &off );
               break;
     }

     data->base.buffer->SeekTo( data->base.buffer, off );

     return (tsize_t)off;
}

static int
_closeTIFF( thandle_t handle )
{
     return 0;
}

static toff_t
_sizeTIFF( thandle_t handle )
{
     unsigned int length;

     IDirectFBImageProvider_TIFF_data *data = (IDirectFBImageProvider_TIFF_data*)handle;

     data->base.buffer->GetLength( data->base.buffer, &length );

     return (toff_t)length;
}

static int
_mapdataTIFF(thandle_t handle, tdata_t* data, toff_t* offset)
{
     return 0;
}

static void
_unmapdataTIFF( thandle_t handle, tdata_t data, toff_t offset )
{
}
