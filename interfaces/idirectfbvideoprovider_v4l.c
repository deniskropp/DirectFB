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

#include <malloc.h>

#include <directfb.h>

#include <misc/util.h>

#include <core/core.h>
#include <core/v4l.h>
#include <core/layers.h>

#include <display/idirectfbsurface.h>

/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
     int               ref;       /* reference counter */
} IDirectFBVideoProvider_V4L_data;


static void IDirectFBVideoProvider_V4L_Destruct( IDirectFBVideoProvider *thiz )
{
     v4l_stop();
     free( thiz->priv );
     thiz->priv = NULL;
}

static DFBResult IDirectFBVideoProvider_V4L_AddRef( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_Release( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBVideoProvider_V4L_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_GetSurfaceDescription(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBSurfaceDescription  *desc )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz || !desc)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     memset( desc, 0, sizeof(DFBSurfaceDescription) );
     desc->flags = (DFBSurfaceDescriptionFlags)
                             (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_BPP);
     desc->width = 768;
     desc->height = 576;
     desc->bpp = BYTES_PER_PIXEL(layers->surface->format)*8;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_PlayTo(IDirectFBVideoProvider *thiz,
                                            IDirectFBSurface       *destination,
                                            DFBRectangle           *dstrect,
                                            DVFrameCallback         callback,
                                            void                   *ctx)
{
     DFBRectangle rect;

     IDirectFBVideoProvider_V4L_data *data;
     IDirectFBSurface_data           *dst_data;

     if (!thiz || !destination)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;
     dst_data = (IDirectFBSurface_data*)destination->priv;

     if (!data || !dst_data)
          return DFB_DEAD;

     if (dstrect) {
          if (dstrect->w < 0  ||  dstrect->h < 0)
               return DFB_INVARG;

          rect = *dstrect;

          rect.x += dst_data->req_rect.x;
          rect.y += dst_data->req_rect.y;
     }
     else
          rect = dst_data->req_rect;

     if (!rectangle_intersect( &rect, &dst_data->clip_rect ))
          return DFB_INVARG;

     return v4l_to_surface( dst_data->surface, &rect, callback, ctx );
}

static DFBResult IDirectFBVideoProvider_V4L_Stop( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return v4l_stop();
}



/* exported symbols */

const char *get_type()
{
     return "IDirectFBVideoProvider";
}

const char *get_implementation()
{
     return "V4L";
}

DFBResult Probe( const char *filename )
{
     /* unprobable? */

     return DFB_UNSUPPORTED;
}

DFBResult Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     DFBResult ret;
     IDirectFBVideoProvider_V4L_data *data;

     ret = v4l_open();
     if (ret)
          return ret;

     data = (IDirectFBVideoProvider_V4L_data*)
                         malloc( sizeof(IDirectFBVideoProvider_V4L_data) );
     memset( data, 0, sizeof(IDirectFBVideoProvider_V4L_data) );
     thiz->priv = data;

     data->ref = 1;

     thiz->AddRef = IDirectFBVideoProvider_V4L_AddRef;
     thiz->Release = IDirectFBVideoProvider_V4L_Release;
     thiz->GetSurfaceDescription =
          IDirectFBVideoProvider_V4L_GetSurfaceDescription;
     thiz->PlayTo = IDirectFBVideoProvider_V4L_PlayTo;
     thiz->Stop = IDirectFBVideoProvider_V4L_Stop;

     return DFB_OK;
}

