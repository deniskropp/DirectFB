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

#include <display/idirectfbsurface.h>
#include "idirectfbvideoprovider.h"

/*
 * increments reference count of input buffer
 */
DFBResult IDirectFBVideoProvider_AddRef( IDirectFBVideoProvider *thiz );

/*
 * decrements reference count, destructs buffer if reference count is 0
 */
DFBResult IDirectFBVideoProvider_Release( IDirectFBVideoProvider *thiz );

/*
 * Play the video rendering it into the specified rectangle
 * of the destination surface.
 * Optionally a callback can be registered.
 */
DFBResult IDirectFBVideoProvider_PlayTo( IDirectFBVideoProvider *thiz,
                                         IDirectFBSurface       *destination,
                                         DFBRectangle           *dstrect,
                                         DVFrameCallback         callback,
                                         void                   *ctx );

/*
 * Stop rendering into the destination surface.
 */
DFBResult IDirectFBVideoProvider_Stop( IDirectFBVideoProvider *thiz );

/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
     int               ref;       /* reference counter */
} IDirectFBVideoProvider_data;


DFBResult IDirectFBVideoProvider_Construct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_data *data;

     data = (IDirectFBVideoProvider_data*)malloc( sizeof(IDirectFBVideoProvider_data) );
     memset( data, 0, sizeof(IDirectFBVideoProvider_data) );
     thiz->priv = data;

     data->ref = 1;

     thiz->AddRef = IDirectFBVideoProvider_AddRef;
     thiz->Release = IDirectFBVideoProvider_Release;
     thiz->PlayTo = IDirectFBVideoProvider_PlayTo;
     thiz->Stop = IDirectFBVideoProvider_Stop;

     return DFB_OK;
}

void IDirectFBVideoProvider_Destruct( IDirectFBVideoProvider *thiz )
{
     v4l_stop();
     free( thiz->priv );
     thiz->priv = NULL;
}

DFBResult IDirectFBVideoProvider_AddRef( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_data *data = (IDirectFBVideoProvider_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

DFBResult IDirectFBVideoProvider_Release( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_data *data = (IDirectFBVideoProvider_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBVideoProvider_Destruct( thiz );
     }

     return DFB_OK;
}

DFBResult IDirectFBVideoProvider_PlayTo( IDirectFBVideoProvider *thiz,
                                         IDirectFBSurface       *destination,
                                         DFBRectangle           *dstrect,
                                         DVFrameCallback         callback,
                                         void                   *ctx )
{
     DFBResult ret;
     DFBRectangle rect;
     IDirectFBVideoProvider_data *data = (IDirectFBVideoProvider_data*)thiz->priv;
     IDirectFBSurface_data *dst_data = (IDirectFBSurface_data*)destination->priv;

     if (!data)
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
     
     ret = v4l_to_surface( dst_data->renderbuffer, &rect, callback, ctx );
     if (ret)
          return ret;

     return DFB_OK;
}

DFBResult IDirectFBVideoProvider_Stop( IDirectFBVideoProvider *thiz )
{
     DFBResult ret;
     IDirectFBVideoProvider_data *data = (IDirectFBVideoProvider_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     ret = v4l_stop();
     if (ret)
          return ret;

     return DFB_OK;
}

