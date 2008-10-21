/*
   (c) Copyright 2001 Till Adam
   All rights reserved.

   Written by Till Adam <till@adam-lilienthal.de>.

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
#include <string.h>
#include <malloc.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/soundcard.h>

#include <pthread.h>

#include <directfb.h>

#include <media/idirectfbimageprovider.h>

#include <misc/util.h>
#include <misc/gfx_util.h>

#include <direct/interface.h>
#include <direct/mem.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/state.h>
#include <core/surface.h>
#include <core/gfxcard.h>

#include <gfx/convert.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbdatabuffer.h>

#define X_DISPLAY_MISSING
#include <Imlib2.h>


static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, IMLIB2 )

/*
 * private data struct of IDirectFBImageProvider_IMLIB2
 */
typedef struct {
     int            ref;      /* reference counter */
     char          *filename; /* filename of file to load */
     Imlib_Image    im;
} IDirectFBImageProvider_IMLIB2_data;



static DFBResult
IDirectFBImageProvider_IMLIB2_AddRef  ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_IMLIB2_Release ( IDirectFBImageProvider *thiz );

static DFBResult
IDirectFBImageProvider_IMLIB2_RenderTo( IDirectFBImageProvider *thiz,
                                        IDirectFBSurface       *destination,
                                        const DFBRectangle     *dest_rect );

static DFBResult
IDirectFBImageProvider_IMLIB2_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                 DIRenderCallback        callback,
                                                 void                   *context );

static DFBResult
IDirectFBImageProvider_IMLIB2_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                     DFBSurfaceDescription  *dsc );

static DFBResult
IDirectFBImageProvider_IMLIB2_GetImageDescription( IDirectFBImageProvider *thiz,
                                                   DFBImageDescription    *dsc );


static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     Imlib_Image im;
     Imlib_Load_Error err;

     if (!ctx->filename)
          return DFB_UNSUPPORTED;

     im = imlib_load_image_with_error_return (ctx->filename, &err);
     switch (err) {
          case IMLIB_LOAD_ERROR_NONE:
               return DFB_OK;
               break;
          case IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST:
          case IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY:
          case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ:
          case IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT:
          case IMLIB_LOAD_ERROR_PATH_TOO_LONG:
          case IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT:
          case IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY:
          case IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE:
          case IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS:
          case IMLIB_LOAD_ERROR_OUT_OF_MEMORY:
          case IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS:
          case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE:
          case IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE:
          case IMLIB_LOAD_ERROR_UNKNOWN:
          default:
               break;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     IDirectFBDataBuffer_data *buffer_data;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_IMLIB2)

     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;
     if (!buffer_data) {
          DIRECT_DEALLOCATE_INTERFACE(thiz);
          return DFB_DEAD;
     }

     data->ref = 1;
     data->filename = D_STRDUP( buffer_data->filename );

     /* The image is already loaded and in cache at this point.
      * Any errors should have been caught in Probe */
     data->im = imlib_load_image (data->filename);

     D_DEBUG( "DirectFB/Media: IMLIB2 Provider Construct '%s'\n", data->filename );

     thiz->AddRef = IDirectFBImageProvider_IMLIB2_AddRef;
     thiz->Release = IDirectFBImageProvider_IMLIB2_Release;
     thiz->RenderTo = IDirectFBImageProvider_IMLIB2_RenderTo;
     thiz->SetRenderCallback = IDirectFBImageProvider_IMLIB2_SetRenderCallback;
     thiz->GetImageDescription = IDirectFBImageProvider_IMLIB2_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_IMLIB2_GetSurfaceDescription;

     return DFB_OK;
}


static DFBResult
IDirectFBImageProvider_IMLIB2_AddRef  ( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_IMLIB2)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_IMLIB2_Release ( IDirectFBImageProvider *thiz )
{

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_IMLIB2)

     if (--data->ref == 0) {
          D_FREE( data->filename );
          imlib_free_image();
          D_FREE( thiz->priv );
          thiz->priv = NULL;
     }
#ifndef DFB_DEBUG
     D_FREE( thiz );
#endif
     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_IMLIB2_RenderTo( IDirectFBImageProvider *thiz,
                                        IDirectFBSurface       *destination,
                                        const DFBRectangle     *dest_rect )
{
     DFBResult              err;
     int                    src_width, src_height;
     u32                   *image_data;
     DFBRectangle           rect;
     DFBRegion              clip;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     CoreSurfaceBufferLock  lock;

     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_IMLIB2)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     imlib_context_set_image(data->im);

     src_width = imlib_image_get_width();
     src_height = imlib_image_get_height();

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

     image_data = imlib_image_get_data_for_reading_only();
     if (!image_data)
          return DFB_FAILURE; /* what else makes sense here? */

     err = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
     if (err)
          return err;

     dfb_scale_linear_32( image_data, src_width, src_height,
                          lock.addr, lock.pitch, &rect, dst_surface, &clip );

     dfb_surface_unlock_buffer( dst_surface, &lock );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_IMLIB2_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                 DIRenderCallback        callback,
                                                 void                   *context )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_IMLIB2_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                     DFBSurfaceDescription  *dsc)
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_IMLIB2)

     imlib_context_set_image(data->im);

     dsc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     dsc->width = imlib_image_get_width();
     dsc->height = imlib_image_get_height();
     dsc->pixelformat = imlib_image_has_alpha() ?
                        DSPF_ARGB : dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_IMLIB2_GetImageDescription( IDirectFBImageProvider *thiz,
                                                   DFBImageDescription    *dsc )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_IMLIB2)

     imlib_context_set_image(data->im);

     /* FIXME no color-keying yet */
     if (imlib_image_has_alpha()) {
          dsc->caps = DICAPS_ALPHACHANNEL;
     }
     else {
          dsc->caps = DICAPS_NONE;
     }

     return DFB_OK;
}
