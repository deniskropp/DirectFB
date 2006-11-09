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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>

#include "directfb.h"

#include "core/core.h"
#include "core/coretypes.h"

#include "core/fonts.h"
#include "core/gfxcard.h"
#include "core/layer_region.h"
#include "core/state.h"
#include "core/surfaces.h"
#include "core/windows.h"
#include "core/wm.h"
#include "core/windows_internal.h" /* FIXME */

#include "idirectfbsurface.h"
#include "idirectfbsurface_window.h"

#include <direct/interface.h>
#include <direct/mem.h>
#include "misc/util.h"

#include "gfx/util.h"


D_DEBUG_DOMAIN( Surface, "IDirectFBSurfaceW", "IDirectFBSurface_Window Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBSurface_Window
 */
typedef struct {
     IDirectFBSurface_data base;   /* base Surface implementation */

     CoreWindow           *window; /* pointer to core data */

     pthread_t             flip_thread; /* thread for non-flipping primary
                                           surfaces, to make changes visible */

//     CoreGraphicsSerial    serial;
} IDirectFBSurface_Window_data;

/**********************************************************************************************************************/

static void *Flipping_Thread( void *arg );

/**********************************************************************************************************************/

static void
IDirectFBSurface_Window_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_Window_data *data;

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     D_ASSERT( thiz != NULL );

     data = thiz->priv;

     if ((int) data->flip_thread != -1) {
          pthread_cancel( data->flip_thread );
          pthread_join( data->flip_thread, NULL );
     }

     dfb_window_unref( data->window );

     IDirectFBSurface_Destruct( thiz );
}

static DFBResult
IDirectFBSurface_Window_Release( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

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

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, thiz, region, flags );

     if (!data->base.surface)
          return DFB_DESTROYED;

     if (data->base.locked)
          return DFB_LOCKED;

     if (!data->base.area.current.w || !data->base.area.current.h ||
         (region && (region->x1 > region->x2 || region->y1 > region->y2)))
          return DFB_INVAREA;


     dfb_region_from_rectangle( &reg, &data->base.area.current );

     if (region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( region,
                                                       data->base.area.wanted.x,
                                                       data->base.area.wanted.y );

          if (!dfb_region_region_intersect( &reg, &clip ))
               return DFB_INVAREA;
     }

     D_DEBUG_AT( Surface, "  -> %d, %d - %dx%d\n", DFB_RECTANGLE_VALS_FROM_REGION( &reg ) );


     if (flags & DSFLIP_PIPELINE) {
          dfb_gfxcard_wait_serial( &data->window->serial2 );

          data->window->serial2 = data->window->serial1;

          dfb_state_get_serial( &data->base.state, &data->window->serial1 );
     }


     if (data->window->region) {
          dfb_layer_region_flip_update( data->window->region, &reg, flags );
     }
     else {
          if (data->base.surface->caps & DSCAPS_FLIPPING) {
               if (!(flags & DSFLIP_BLIT) && reg.x1 == 0 && reg.y1 == 0 &&
                   reg.x2 == data->window->config.bounds.w - 1 &&
                   reg.y2 == data->window->config.bounds.h - 1)
                    dfb_surface_flip_buffers( data->base.surface, false );
               else
                    dfb_back_to_front_copy( data->base.surface, &reg );
          }

          dfb_window_repaint( data->window, &reg, flags );
     }

     if (!data->window->config.opacity && data->base.caps & DSCAPS_PRIMARY)
          dfb_window_set_opacity( data->window, 0xff );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Window_GetSubSurface( IDirectFBSurface    *thiz,
                                       const DFBRectangle  *rect,
                                       IDirectFBSurface   **surface )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (!data->base.surface || !data->window || !data->window->surface)
          return DFB_DESTROYED;

     if (!surface)
          return DFB_INVARG;
          
     /* Allocate interface */
     DIRECT_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

     if (rect || data->base.limit_set) {
          DFBRectangle wanted, granted;
          
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
          else {
               wanted = data->base.area.wanted;
          }
          
          /* Compute granted rectangle */
          granted = wanted;

          dfb_rectangle_intersect( &granted, &data->base.area.granted );
          
          /* Construct */
          ret = IDirectFBSurface_Window_Construct( *surface, &wanted, &granted,
                                                   data->window, data->base.caps |
                                                   DSCAPS_SUBSURFACE, data->base.core );
     }
     else {
          /* Construct */
          ret = IDirectFBSurface_Window_Construct( *surface, NULL, NULL,
                                                   data->window, data->base.caps |
                                                   DSCAPS_SUBSURFACE, data->base.core );
     }
     
     return ret;
}

DFBResult
IDirectFBSurface_Window_Construct( IDirectFBSurface       *thiz,
                                   DFBRectangle           *wanted,
                                   DFBRectangle           *granted,
                                   CoreWindow             *window,
                                   DFBSurfaceCapabilities  caps,
                                   CoreDFB                *core )
{
     DFBResult ret;
     DFBInsets insets;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );
     
     dfb_wm_get_insets( window->stack, window, &insets );
     
     ret = IDirectFBSurface_Construct( thiz, wanted, granted, &insets,
                                       window->surface, caps, core );
     if (ret)
          return ret;

     if (dfb_window_ref( window )) {
          IDirectFBSurface_Destruct( thiz );
          return DFB_FAILURE;
     }

     data->window = window;
     data->flip_thread = (pthread_t) -1;

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

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     while (data->base.surface && data->window->surface) {
          pthread_testcancel();

          /*
           * OPTIMIZE: only call if surface has been touched in the meantime
           */
          thiz->Flip( thiz, NULL, 0 );

          usleep(40000);
     }

     return NULL;
}

