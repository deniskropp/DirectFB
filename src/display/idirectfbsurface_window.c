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

#include <string.h>
#include <malloc.h>

#include "directfb.h"
#include "directfb_internals.h"

#include "core/core.h"
#include "core/coretypes.h"

#include "core/gfxcard.h"
#include "core/surfaces.h"
#include "core/fbdev.h"
#include "core/fonts.h"
#include "core/state.h"
#include "core/windows.h"

#include "idirectfbsurface.h"
#include "idirectfbsurface_window.h"

#include "misc/mem.h"
#include "misc/util.h"

#include "gfx/util.h"

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

     reactor_detach( data->base.surface->reactor,
                     IDirectFBSurface_listener, thiz );

     thiz->Unlock( thiz );

     if (!(data->base.caps & DSCAPS_SUBSURFACE)  &&
          data->base.caps & DSCAPS_PRIMARY)
     {
          window_remove( data->window );
          window_destroy( data->window );
     }

     if (data->base.font)
          data->base.font->Release (data->base.font);

     DFBFREE( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     DFBFREE( thiz );
#endif
}

DFBResult IDirectFBSurface_Window_Release( IDirectFBSurface *thiz )
{
     INTERFACE_GET_DATA(IDirectFBSurface_Window)

     if (--data->base.ref == 0)
          IDirectFBSurface_Window_Destruct( thiz );

     return DFB_OK;
}

DFBResult IDirectFBSurface_Window_Flip( IDirectFBSurface *thiz,
                                        DFBRegion *region,
                                        DFBSurfaceFlipFlags flags )
{
     DFBRegion reg;

     INTERFACE_GET_DATA(IDirectFBSurface_Window)

     if (data->base.locked)
          return DFB_LOCKED;

     if (!data->base.area.current.w || !data->base.area.current.h)
          return DFB_INVAREA;


     if (region) {
          reg = *region;

          reg.x1 += data->base.area.wanted.x;
          reg.x2 += data->base.area.wanted.x;
          reg.y1 += data->base.area.wanted.y;
          reg.y2 += data->base.area.wanted.y;

          if (!unsafe_region_rectangle_intersect( &reg, &data->base.area.current ))
               return DFB_OK;
     }
     else {
          reg.x1 = data->base.area.current.x;
          reg.y1 = data->base.area.current.y;
          reg.x2 = data->base.area.current.x + data->base.area.current.w - 1;
          reg.y2 = data->base.area.current.y + data->base.area.current.h - 1;
     }

     if (data->window->surface->caps & DSCAPS_FLIPPING) {
          DFBRectangle rect = { reg.x1, reg.y1,
                                reg.x2 - reg.x1 + 1,
                                reg.y2 - reg.y1 + 1 };

          if (rect.x == 0 && rect.y == 0 &&
              rect.w == data->window->width &&
              rect.h == data->window->height)
               surface_flip_buffers( data->window->surface );
          else
               back_to_front_copy( data->window->surface, &rect );
     }

     if (flags & DSFLIP_WAITFORSYNC)
          fbdev_wait_vsync();

     window_repaint( data->window, &reg );

     return DFB_OK;
}

DFBResult IDirectFBSurface_Window_GetSubSurface( IDirectFBSurface    *thiz,
                                                 DFBRectangle        *rect,
                                                 IDirectFBSurface    **surface )
{
     DFBRectangle wanted, granted;

     INTERFACE_GET_DATA(IDirectFBSurface_Window)


     if (!data->base.area.current.w || !data->base.area.current.h)
          return DFB_INVAREA;


     if (rect) {
          if (rect->w < 0  ||  rect->h < 0)
               return DFB_INVARG;

          wanted = *rect;

          wanted.x += data->base.area.wanted.x;
          wanted.y += data->base.area.wanted.y;

/*          if (!rectangle_intersect( &wanted, &data->base.area.wanted ))
               return DFB_INVAREA;*/
     }
     else
          wanted = data->base.area.wanted;

     granted = wanted;

     if (!rectangle_intersect( &granted, &data->base.area.granted ))
          return DFB_INVAREA;


     DFB_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

     return IDirectFBSurface_Window_Construct( *surface, &wanted, &granted,
                                               data->window, data->base.caps |
                                               DSCAPS_SUBSURFACE );
}

DFBResult IDirectFBSurface_Window_Construct( IDirectFBSurface       *thiz,
                                             DFBRectangle           *wanted,
                                             DFBRectangle           *granted,
                                             CoreWindow             *window,
                                             DFBSurfaceCapabilities caps )
{
     IDirectFBSurface_Window_data *data;

     if (!thiz->priv)
          thiz->priv = DFBCALLOC( 1, sizeof(IDirectFBSurface_Window_data) );

     IDirectFBSurface_Construct( thiz, wanted, granted, window->surface, caps );

     data = (IDirectFBSurface_Window_data*)(thiz->priv);
     data->window = window;

     thiz->Release = IDirectFBSurface_Window_Release;
     thiz->Flip = IDirectFBSurface_Window_Flip;
     thiz->GetSubSurface = IDirectFBSurface_Window_GetSubSurface;

     return DFB_OK;
}

