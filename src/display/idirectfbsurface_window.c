/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

     pthread_t             flip_thread; /* thread for non-flipping primary
                                           surfaces, to make changes visible */
} IDirectFBSurface_Window_data;


static void *
Flipping_Thread( void *arg );


static void
IDirectFBSurface_Window_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_Window_data *data =
          (IDirectFBSurface_Window_data*)thiz->priv;

     if (data->flip_thread != -1) {
          pthread_cancel( data->flip_thread );
          pthread_join( data->flip_thread, NULL );
     }

     if (data->base.surface) {
          if (!(data->base.caps & DSCAPS_SUBSURFACE)  &&
               data->base.caps & DSCAPS_PRIMARY)
          {
               dfb_window_deinit( data->window );
               dfb_window_destroy( data->window, true );
          }
     }

     IDirectFBSurface_Destruct( thiz );
}

static DFBResult
IDirectFBSurface_Window_Release( IDirectFBSurface *thiz )
{
     INTERFACE_GET_DATA(IDirectFBSurface_Window)

     if (--data->base.ref == 0)
          IDirectFBSurface_Window_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Window_Flip( IDirectFBSurface    *thiz,
                              const DFBRegion     *region,
                              DFBSurfaceFlipFlags  flags )
{
     DFBRegion reg;

     INTERFACE_GET_DATA(IDirectFBSurface_Window)

     if (!data->base.surface)
          return DFB_DESTROYED;

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

          if (!dfb_unsafe_region_rectangle_intersect( &reg, &data->base.area.current ))
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

          if ((~flags & DSFLIP_BLIT) && rect.x == 0 && rect.y == 0 &&
              rect.w == data->window->width && rect.h == data->window->height)
               dfb_surface_flip_buffers( data->window->surface );
          else
               dfb_back_to_front_copy( data->window->surface, &rect );
     }

     dfb_window_repaint( data->window, &reg, flags );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Window_GetSubSurface( IDirectFBSurface    *thiz,
                                       const DFBRectangle  *rect,
                                       IDirectFBSurface   **surface )
{
     DFBRectangle wanted, granted;  

     INTERFACE_GET_DATA(IDirectFBSurface_Window)

     /* Check arguments */
     if (!data->base.surface)
          return DFB_DESTROYED;

     if (!surface)
          return DFB_INVARG;

     /* Compute wanted rectangle */
     if (rect) {
          wanted = *rect;

          wanted.x += data->base.area.wanted.x;
          wanted.y += data->base.area.wanted.y;

          if (wanted.w <= 0 || wanted.h <= 0) {
               wanted.w = 0;
               wanted.h = 0;
          }
     }
     else
          wanted = data->base.area.wanted;

     /* Compute granted rectangle */
     granted = wanted;

     dfb_rectangle_intersect( &granted, &data->base.area.granted );

     /* Allocate and construct */
     DFB_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

     return IDirectFBSurface_Window_Construct( *surface, &wanted, &granted,
                                               data->window, data->base.caps |
                                               DSCAPS_SUBSURFACE );
}

DFBResult
IDirectFBSurface_Window_Construct( IDirectFBSurface       *thiz,
                                   DFBRectangle           *wanted,
                                   DFBRectangle           *granted,
                                   CoreWindow             *window,
                                   DFBSurfaceCapabilities  caps )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBSurface_Window)

     IDirectFBSurface_Construct( thiz, wanted, granted, window->surface, caps );

     data->window = window;
     data->flip_thread = -1;

     /*
      * Create an auto flipping thread if the application
      * requested a (primary) surface that doesn't need to be flipped.
      * Window surfaces even need to be flipped when they are single buffered.
      */
     if (!(caps & DSCAPS_FLIPPING) && !(caps & DSCAPS_SUBSURFACE))
          pthread_create( &data->flip_thread, NULL, Flipping_Thread, thiz );


     thiz->Release = IDirectFBSurface_Window_Release;
     thiz->Flip = IDirectFBSurface_Window_Flip;
     thiz->GetSubSurface = IDirectFBSurface_Window_GetSubSurface;

     return DFB_OK;
}


/* file internal */

static void *
Flipping_Thread( void *arg )
{
     IDirectFBSurface             *thiz = (IDirectFBSurface*) arg;
     IDirectFBSurface_Window_data *data = (IDirectFBSurface_Window_data*) thiz->priv;

     while (data->base.surface) {
          usleep(40000);

          pthread_testcancel();

          /*
           * OPTIMIZE: only call if surface has been touched in the meantime
           */
          thiz->Flip( thiz, NULL, 0 );
     }

     return NULL;
}

