/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
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
#include <unistd.h>

#include <string.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coretypes.h>

#include <core/fonts.h>
#include <core/gfxcard.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/state.h>
#include <core/surface.h>
#include <core/windows.h>
#include <core/windows_internal.h> /* FIXME */
#include <core/wm.h>

#include <core/CoreGraphicsState.h>
#include <core/CoreLayerRegion.h>
#include <core/CoreSurface.h>
#include <core/CoreWindow.h>
#include <core/CoreWindowStack.h>

#include <display/idirectfbsurface.h>
#include <display/idirectfbsurface_window.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/thread.h>

#include <misc/util.h>

#include <gfx/util.h>


D_DEBUG_DOMAIN( Surface, "IDirectFBSurfaceW", "IDirectFBSurface_Window Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBSurface_Window
 */
typedef struct {
     IDirectFBSurface_data base;   /* base Surface implementation */

     CoreWindow           *window; /* pointer to core data */

     DirectThread         *flip_thread; /* thread for non-flipping primary
                                           surfaces, to make changes visible */

//     CoreGraphicsSerial    serial;
} IDirectFBSurface_Window_data;

/**********************************************************************************************************************/

static void *Flipping_Thread( DirectThread *thread,
                              void         *arg );

/**********************************************************************************************************************/

static void
IDirectFBSurface_Window_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_Window_data *data;

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     D_ASSERT( thiz != NULL );

     data = thiz->priv;

     if (data->flip_thread) {
          direct_thread_cancel( data->flip_thread );
          direct_thread_join( data->flip_thread );
          direct_thread_destroy( data->flip_thread );
     }

     dfb_window_unref( data->window );

     IDirectFBSurface_Destruct( thiz );
}

static DirectResult
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
     DFBResult ret = DFB_OK;
     DFBRegion reg;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, thiz, region, flags );

     if (!data->base.surface)
          return DFB_DESTROYED;

     if (data->base.surface->config.caps & DWCAPS_STEREO) 
          return DFB_UNSUPPORTED;

     if (data->base.locked)
          return DFB_LOCKED;

     if (!data->base.area.current.w || !data->base.area.current.h ||
         (region && (region->x1 > region->x2 || region->y1 > region->y2)))
          return DFB_INVAREA;


     IDirectFBSurface_StopAll( &data->base );

     if (data->base.parent) {
          IDirectFBSurface_data *parent_data;

          DIRECT_INTERFACE_GET_DATA_FROM( data->base.parent, parent_data, IDirectFBSurface );

          /* Signal end of sequence of operations. */
          dfb_state_lock( &parent_data->state );
          dfb_state_stop_drawing( &parent_data->state );
          dfb_state_unlock( &parent_data->state );
     }



     dfb_region_from_rectangle( &reg, &data->base.area.current );

     if (region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( region,
                                                       data->base.area.wanted.x,
                                                       data->base.area.wanted.y );

          if (!dfb_region_region_intersect( &reg, &clip ))
               return DFB_INVAREA;
     }

     D_DEBUG_AT( Surface, "  -> FLIP %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &reg ) );

#if 0
     if (flags & DSFLIP_PIPELINE) {
          dfb_gfxcard_wait_serial( &data->window->serial2 );

          data->window->serial2 = data->window->serial1;

          dfb_state_get_serial( &data->base.state, &data->window->serial1 );
     }
#endif

     CoreGraphicsState_Flush( data->base.state_client.gfx_state );

     if (data->window->region) {
          ret = CoreLayerRegion_FlipUpdate( data->window->region, &reg, flags );
     }
     else {
          if (data->base.surface->config.caps & DSCAPS_FLIPPING) {
               if (!(flags & DSFLIP_BLIT) && reg.x1 == 0 && reg.y1 == 0 &&
                   reg.x2 == data->base.surface->config.size.w  - 1 &&
                   reg.y2 == data->base.surface->config.size.h - 1)
               {
                    ret = CoreSurface_Flip( data->base.surface, false );
                    if (ret)
                        return ret;
               }
               else
                    CoreSurface_BackToFrontCopy( data->base.surface, DSSE_LEFT, &reg, NULL );
          }

          dfb_surface_dispatch_update( data->base.surface, &reg, &reg );

          ret = CoreWindow_Repaint( data->window, &reg, &reg, flags );
          if (ret)
              return ret;
     }

     if (!data->window->config.opacity && data->base.caps & DSCAPS_PRIMARY) {
          CoreWindowConfig config = { .opacity = 0xff };

          ret = CoreWindow_SetConfig( data->window, &config, NULL, 0, CWCF_OPACITY );
     }

     IDirectFBSurface_WaitForBackBuffer( &data->base );

     return ret;
}

static DFBResult
IDirectFBSurface_Window_FlipStereo( IDirectFBSurface    *thiz,
                                    const DFBRegion     *left_region,
                                    const DFBRegion     *right_region,
                                    DFBSurfaceFlipFlags  flags )
{
     DFBRegion l_reg, r_reg;
     DFBSurfaceStereoEye eye;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p, %p, %p, 0x%08x )\n", __FUNCTION__, thiz, left_region, right_region, flags );

     if (!data->base.surface)
          return DFB_DESTROYED;

     if (!(data->base.surface->config.caps & DSCAPS_STEREO)) 
          return DFB_UNSUPPORTED;

     if (data->base.locked)
          return DFB_LOCKED;

     if (!data->base.area.current.w || !data->base.area.current.h ||
         (left_region && (left_region->x1 > left_region->x2 || left_region->y1 > left_region->y2)) ||
         (right_region && (right_region->x1 > right_region->x2 || right_region->y1 > right_region->y2)))
          return DFB_INVAREA;


     IDirectFBSurface_StopAll( &data->base );

     if (data->base.parent) {
          IDirectFBSurface_data *parent_data;

          DIRECT_INTERFACE_GET_DATA_FROM( data->base.parent, parent_data, IDirectFBSurface );

          /* Signal end of sequence of operations. */
          dfb_state_lock( &parent_data->state );
          dfb_state_stop_drawing( &parent_data->state );
          dfb_state_unlock( &parent_data->state );
     }

     dfb_region_from_rectangle( &l_reg, &data->base.area.current );
     dfb_region_from_rectangle( &r_reg, &data->base.area.current );

     if (left_region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( left_region,
                                                       data->base.area.wanted.x,
                                                       data->base.area.wanted.y );

          if (!dfb_region_region_intersect( &l_reg, &clip ))
               return DFB_INVAREA;
     }
     if (right_region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( right_region,
                                                       data->base.area.wanted.x,
                                                       data->base.area.wanted.y );

          if (!dfb_region_region_intersect( &r_reg, &clip ))
               return DFB_INVAREA;
     }

     D_DEBUG_AT( Surface, "  -> FLIP Left: %4d,%4d-%4dx%4d Right: %4d,%4d-%4dx%4d\n", 
          DFB_RECTANGLE_VALS_FROM_REGION( &l_reg ), DFB_RECTANGLE_VALS_FROM_REGION( &r_reg ) );


#if 0
     if (flags & DSFLIP_PIPELINE) {
          dfb_gfxcard_wait_serial( &data->window->serial2 );

          data->window->serial2 = data->window->serial1;

          dfb_state_get_serial( &data->base.state, &data->window->serial1 );
     }
#endif

     CoreGraphicsState_Flush( data->base.state_client.gfx_state );

     if (data->window->region) {
          /* TODO STEREO: Add support for hardware windows. */
          /*dfb_layer_region_flip_update( data->window->region, &reg, flags );*/
          return DFB_UNSUPPORTED;
     }
     else {
          if (data->base.surface->config.caps & DSCAPS_FLIPPING) {
               if (!(flags & DSFLIP_BLIT)) {
                    if (l_reg.x1 == 0 && l_reg.y1 == 0 &&
                        l_reg.x2 == data->base.surface->config.size.w  - 1 &&
                        l_reg.y2 == data->base.surface->config.size.h - 1 &&
                        r_reg.x1 == 0 && r_reg.y1 == 0 &&
                        r_reg.x2 == data->base.surface->config.size.w  - 1 &&
                        r_reg.y2 == data->base.surface->config.size.h - 1)
                    {
                         CoreSurface_Flip( data->base.surface, false );
                    }
                    else
                         CoreSurface_BackToFrontCopy( data->base.surface, DSSE_LEFT | DSSE_RIGHT, &l_reg, &r_reg );
               }
          }

          dfb_surface_dispatch_update( data->base.surface, &l_reg, &r_reg );

          CoreWindow_Repaint( data->window, &l_reg, &r_reg, flags );
     }

     if (!data->window->config.opacity && data->base.caps & DSCAPS_PRIMARY) {
          CoreWindowConfig config = { .opacity = 0xff };

          CoreWindow_SetConfig( data->window, &config, NULL, 0, CWCF_OPACITY );
     }

     IDirectFBSurface_WaitForBackBuffer( &data->base );

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
          ret = IDirectFBSurface_Window_Construct( *surface, thiz, &wanted, &granted,
                                                   data->window, data->base.caps |
                                                   DSCAPS_SUBSURFACE, data->base.core, data->base.idirectfb );
     }
     else {
          /* Construct */
          ret = IDirectFBSurface_Window_Construct( *surface, thiz, NULL, NULL,
                                                   data->window, data->base.caps |
                                                   DSCAPS_SUBSURFACE, data->base.core, data->base.idirectfb );
     }
     
     return ret;
}

DFBResult
IDirectFBSurface_Window_Construct( IDirectFBSurface       *thiz,
                                   IDirectFBSurface       *parent,
                                   DFBRectangle           *wanted,
                                   DFBRectangle           *granted,
                                   CoreWindow             *window,
                                   DFBSurfaceCapabilities  caps,
                                   CoreDFB                *core,
                                   IDirectFB              *dfb )
{
     DFBResult        ret;
     DFBInsets        insets;
     CoreWindowStack *stack;
     CoreSurface     *surface;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     stack = window->stack;
     D_MAGIC_ASSERT( stack, CoreWindowStack );

     CoreWindowStack_GetInsets( stack, window, &insets );

     ret = CoreWindow_GetSurface( window, &surface );
     if (ret)
          return ret;

     ret = IDirectFBSurface_Construct( thiz, parent, wanted, granted, &insets, surface, caps, core, dfb );

     dfb_surface_unref( surface );

     if (ret)
          return ret;

     if (dfb_window_ref( window )) {
          IDirectFBSurface_Destruct( thiz );
          return DFB_FAILURE;
     }

     data->window = window;

     /*
      * Create an auto flipping thread if the application
      * requested a (primary) surface that doesn't need to be flipped.
      * Window surfaces even need to be flipped when they are single buffered.
      */
     if (!(caps & DSCAPS_FLIPPING) && !(caps & DSCAPS_SUBSURFACE)) {
          if (dfb_config->autoflip_window)
               data->flip_thread = direct_thread_create( DTT_DEFAULT, Flipping_Thread, thiz, "Flipping" );
          else
               D_WARN( "Non-flipping window surface and no 'autoflip-window' option used" );
     }

     thiz->Release       = IDirectFBSurface_Window_Release;
     thiz->Flip          = IDirectFBSurface_Window_Flip;
     thiz->FlipStereo    = IDirectFBSurface_Window_FlipStereo;
     thiz->GetSubSurface = IDirectFBSurface_Window_GetSubSurface;
     
     return DFB_OK;
}


/* file internal */

static void *
Flipping_Thread( DirectThread *thread,
                 void         *arg )
{
     IDirectFBSurface             *thiz = (IDirectFBSurface*) arg;
     IDirectFBSurface_Window_data *data = (IDirectFBSurface_Window_data*) thiz->priv;

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     while (data->base.surface && data->window->surface) {
          direct_thread_testcancel( thread );

          /*
           * OPTIMIZE: only call if surface has been touched in the meantime
           */
          thiz->Flip( thiz, NULL, 0 );

          direct_thread_sleep( 40000 );
     }

     return NULL;
}

