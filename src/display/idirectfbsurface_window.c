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

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <core/fbdev.h>
#include <core/fonts.h>

#include <misc/util.h>

#include "idirectfbsurface.h"
#include "idirectfbsurface_window.h"

#include <directfb_internals.h>


/*
 * private data struct of IDirectFB
 */
typedef struct {
     IDirectFBSurface_data base;   /* base Surface implementation */

     CoreWindow           *window; /* pointer to core data */
} IDirectFBSurface_Window_data;


void IDirectFBSurface_Window_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_Window_data *data =
          (IDirectFBSurface_Window_data*)thiz->priv;

     state_set_destination( &data->base.state, NULL );
     state_set_source( &data->base.state, NULL );
     
     surface_remove_listener( data->base.surface, IDirectFBSurface_listener, thiz );
     
     if (!(data->base.caps & DSCAPS_SUBSURFACE)  &&
          data->base.caps & DSCAPS_PRIMARY)
     {
          window_remove( data->window );
          window_destroy( data->window );
     }

     free( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

DFBResult IDirectFBSurface_Window_Release( IDirectFBSurface *thiz )
{
     IDirectFBSurface_data *data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBSurface_Window_Destruct( thiz );
     }

     return DFB_OK;
}

DFBResult IDirectFBSurface_Window_Flip( IDirectFBSurface *thiz,
                                        DFBRegion *region,
                                        DFBSurfaceFlipFlags flags )
{
     IDirectFBSurface_Window_data *data =
          (IDirectFBSurface_Window_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->base.locked)
          return DFB_LOCKED;

     if (flags & DSFLIP_WAITFORSYNC)
          fbdev_wait_vsync();

     if (region) {
          DFBRegion reg = *region;
          DFBRectangle rect = data->base.req_rect;

          reg.x1 += rect.x;
          reg.x2 += rect.x;
          reg.y1 += rect.y;
          reg.y2 += rect.y;

          if (rectangle_intersect_by_unsafe_region( &rect, &reg ) &&
              rectangle_intersect( &rect, &data->base.clip_rect ))
               window_repaint( data->window, &rect );
     }
     else
          window_repaint( data->window, &data->base.clip_rect );

     return DFB_OK;
}

DFBResult IDirectFBSurface_Window_GetSubSurface( IDirectFBSurface    *thiz,
                                                 DFBRectangle        *rect,
                                                 IDirectFBSurface    **surface )
{
     DFBRectangle req, clip;
     IDirectFBSurface_Window_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_Window_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

//     if (data->locked)
//          return DFB_LOCKED;

     if (rect) {
          if (rect->w < 0  ||  rect->h < 0)
               return DFB_INVARG;

          req = *rect;

          req.x += data->base.req_rect.x;
          req.y += data->base.req_rect.y;
     }
     else {
          req = data->base.req_rect;
     }
     clip = req;

     /*if (!*/rectangle_intersect( &clip, &data->base.clip_rect )/*)
          return DFB_INVARG*/;

     DFB_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

     return IDirectFBSurface_Window_Construct( *surface, &req, &clip,
                                               data->window,
                                           data->base.caps | DSCAPS_SUBSURFACE);
}

DFBResult IDirectFBSurface_Window_Construct( IDirectFBSurface       *thiz,
                                             DFBRectangle           *req_rect,
                                             DFBRectangle           *clip_rect,
                                             CoreWindow             *window,
                                             DFBSurfaceCapabilities caps )
{
     IDirectFBSurface_Window_data *data;

     IDirectFBSurface_Construct( thiz, req_rect, clip_rect,
                                 window->surface, caps );

     thiz->priv = (IDirectFBSurface_Window_data*)
                  realloc( thiz->priv, sizeof( IDirectFBSurface_Window_data ) );
     data = (IDirectFBSurface_Window_data*)(thiz->priv);

     data->window = window;

     thiz->Release = IDirectFBSurface_Window_Release;
     thiz->Flip = IDirectFBSurface_Window_Flip;
     thiz->GetSubSurface = IDirectFBSurface_Window_GetSubSurface;

     return DFB_OK;
}

