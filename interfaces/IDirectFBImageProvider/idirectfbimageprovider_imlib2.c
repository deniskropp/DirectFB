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
#include <directfb_internals.h>

#include <media/idirectfbimageprovider.h>

#include <misc/util.h>
#include <misc/gfx_util.h>
#include <misc/mem.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/state.h>
#include <core/surfaces.h>
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

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, IMLIB2 )

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

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_IMLIB2)

     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;
     if (!buffer_data) {
          DFB_DEALLOCATE_INTERFACE(thiz);
          return DFB_DEAD;
     }

     data->ref = 1;
     data->filename = DFBSTRDUP( buffer_data->filename );

     /* The image is already loaded and in cache at this point.
      * Any errors should have been caught in Probe */
     data->im = imlib_load_image (data->filename);

     DEBUGMSG( "DirectFB/Media: IMLIB2 Provider Construct '%s'\n", data->filename );

     thiz->AddRef = IDirectFBImageProvider_IMLIB2_AddRef;
     thiz->Release = IDirectFBImageProvider_IMLIB2_Release;
     thiz->RenderTo = IDirectFBImageProvider_IMLIB2_RenderTo;
     thiz->SetRenderCallback = IDirectFBImageProvider_IMLIB2_SetRenderCallback;
     thiz->GetImageDescription =
                         IDirectFBImageProvider_IMLIB2_GetImageDescription;
     thiz->GetSurfaceDescription =
                         IDirectFBImageProvider_IMLIB2_GetSurfaceDescription;

     return DFB_OK;
}


static DFBResult
IDirectFBImageProvider_IMLIB2_AddRef  ( IDirectFBImageProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBImageProvider_IMLIB2)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_IMLIB2_Release ( IDirectFBImageProvider *thiz )
{

     INTERFACE_GET_DATA(IDirectFBImageProvider_IMLIB2)

     if (--data->ref == 0) {
          DFBFREE( data->filename );
          imlib_free_image();
          DFBFREE( thiz->priv );
          thiz->priv = NULL;
     }
#ifndef DFB_DEBUG
     DFBFREE( thiz );
#endif
     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_IMLIB2_RenderTo( IDirectFBImageProvider *thiz,
                                        IDirectFBSurface       *destination,
                                        const DFBRectangle     *dest_rect )
{
     int    err;
     void  *dst;
     int    pitch;
     int    src_width, src_height;
     __u32 *image_data;
     DFBRectangle           rect = { 0, 0, 0, 0 };
     DFBSurfacePixelFormat  format;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;

     INTERFACE_GET_DATA (IDirectFBImageProvider_IMLIB2)

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     imlib_context_set_image(data->im);

     src_width = imlib_image_get_width();
     src_height = imlib_image_get_height();

     err = destination->GetSize( destination, &rect.w, &rect.h );
     if (err)
          return err;

     err = destination->GetPixelFormat( destination, &format );
     if (err)
          return err;

     if (dest_rect && !dfb_rectangle_intersect( &rect, dest_rect ))
          return DFB_OK;

     image_data = imlib_image_get_data_for_reading_only();

     if (!image_data)
          return DFB_FAILURE; /* what else makes sense here? */

     err = destination->Lock( destination, DSLF_WRITE, &dst, &pitch );
     if (err)
          return err;

     dst += rect.x * DFB_BYTES_PER_PIXEL(format) + rect.y * pitch;

     dfb_scale_linear_32( dst, image_data,
                          src_width, src_height, rect.w, rect.h,
                          pitch, format, dst_surface->palette,
                          dst_surface->caps );

     destination->Unlock( destination );
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
     INTERFACE_GET_DATA (IDirectFBImageProvider_IMLIB2)

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
     INTERFACE_GET_DATA (IDirectFBImageProvider_IMLIB2)

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
