/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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


#include <pthread.h>

#include <fusion/shmalloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/gfxcard.h>
#include <core/input.h>
#include <core/palette.h>
#include <core/state.h>
#include <core/system.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/wm.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/trace.h>
#include <direct/util.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <core/layers_internal.h>
#include <core/windows_internal.h>


D_DEBUG_DOMAIN( Core_Windows, "Core/Windows", "DirectFB Window Core" );


typedef struct {
     DirectLink       link;

     DFBInputDeviceID id;
     GlobalReaction   reaction;
} StackDevice;

/**************************************************************************************************/

static bool
core_window_filter( CoreWindow *window, const DFBWindowEvent *event );

/**************************************************************************************************/

static const ReactionFunc dfb_window_globals[] = {
     NULL
};

/**************************************************************************************************/

/*
 * Window destructor.
 */
static void
window_destructor( FusionObject *object, bool zombie, void *ctx )
{
     CoreWindow      *window = (CoreWindow*) object;
     CoreWindowStack *stack  = window->stack;

     D_DEBUG_AT( Core_Windows, "destroying %p (%d,%d - %dx%d%s)\n", window,
                 DFB_RECTANGLE_VALS( &window->config.bounds ), zombie ? " ZOMBIE" : "");

     D_ASSUME( window->stack != NULL );

     if (stack) {
          dfb_windowstack_lock( stack );

          dfb_window_destroy( window );

          /* Unlink the primary region of the context. */
          if (window->primary_region)
               dfb_layer_region_unlink( &window->primary_region );

          dfb_windowstack_unlock( stack );
     }

     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_window_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "Window Pool",
                                       sizeof(CoreWindow),
                                       sizeof(DFBWindowEvent),
                                       window_destructor, NULL, world );
}

/**************************************************************************************************/

static DFBResult
create_region( CoreDFB                 *core,
               CoreLayerContext        *context,
               CoreWindow              *window,
               DFBSurfacePixelFormat    format,
               DFBSurfaceCapabilities   surface_caps,
               CoreLayerRegion        **ret_region,
               CoreSurface            **ret_surface )
{
     DFBResult              ret;
     CoreLayerRegionConfig  config;
     CoreLayerRegion       *region;
     CoreSurface           *surface;

     D_ASSERT( core != NULL );
     D_ASSERT( context != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( ret_region != NULL );
     D_ASSERT( ret_surface != NULL );

     memset( &config, 0, sizeof(CoreLayerRegionConfig) );

     config.width         = window->config.bounds.w;
     config.height        = window->config.bounds.h;
     config.format        = format;
     config.options       = context->config.options & DLOP_FLICKER_FILTERING;
     config.source        = (DFBRectangle) { 0, 0, config.width, config.height };
     config.dest          = window->config.bounds;
     config.opacity       = 0;
     config.alpha_ramp[0] = 0x00;
     config.alpha_ramp[1] = 0x55;
     config.alpha_ramp[2] = 0xaa;
     config.alpha_ramp[3] = 0xff;

     if (surface_caps & DSCAPS_DOUBLE)
          config.buffermode = DLBM_BACKVIDEO;
     else if (surface_caps & DSCAPS_TRIPLE)
          config.buffermode = DLBM_TRIPLE;
     else
          config.buffermode = DLBM_FRONTONLY;

     if (((context->config.options & DLOP_ALPHACHANNEL) ||
          (window->config.options & DWOP_ALPHACHANNEL)) && DFB_PIXELFORMAT_HAS_ALPHA(format))
          config.options |= DLOP_ALPHACHANNEL;

     config.options |= DLOP_OPACITY;

     config.surface_caps = surface_caps & (DSCAPS_INTERLACED |
                                           DSCAPS_SEPARATED  |
                                           DSCAPS_PREMULTIPLIED);

     ret = dfb_layer_region_create( context, &region );
     if (ret)
          return ret;


     do {
          ret = dfb_layer_region_set_configuration( region, &config, CLRCF_ALL );
          if (ret) {
               if (config.options & DLOP_OPACITY)
                    config.options &= ~DLOP_OPACITY;
               else if (config.options & DLOP_ALPHACHANNEL)
                    config.options = (config.options & ~DLOP_ALPHACHANNEL) | DLOP_OPACITY;
               else {
                    D_DERROR( ret, "DirectFB/Core/Windows: Unable to set region configuration!\n" );
                    dfb_layer_region_unref( region );
                    return ret;
               }
          }
     } while (ret);


     ret = dfb_surface_create( core, config.width, config.height, format,
                               CSP_VIDEOONLY, surface_caps, NULL, &surface );
     if (ret) {
          dfb_layer_region_unref( region );
          return ret;
     }

     ret = dfb_layer_region_set_surface( region, surface );
     if (ret) {
          dfb_surface_unref( surface );
          dfb_layer_region_unref( region );
          return ret;
     }

     ret = dfb_layer_region_enable( region );
     if (ret) {
          dfb_surface_unref( surface );
          dfb_layer_region_unref( region );
          return ret;
     }

     *ret_region  = region;
     *ret_surface = surface;

     return DFB_OK;
}

DFBResult
dfb_window_create( CoreWindowStack             *stack,
                   const DFBWindowDescription  *desc,
                   CoreWindow                 **ret_window )
{
     DFBResult               ret;
     CoreSurface            *surface;
     CoreSurfacePolicy       surface_policy = CSP_SYSTEMONLY;
     CoreLayer              *layer;
     CoreLayerContext       *context;
     CoreWindow             *window;
     CardCapabilities        card_caps;
     CoreWindowConfig        config;
     DFBWindowCapabilities   caps;
     DFBSurfaceCapabilities  surface_caps;
     DFBSurfacePixelFormat   pixelformat;

     D_DEBUG_AT( Core_Windows, "%s( %p )\n", __FUNCTION__, stack );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );
     D_ASSERT( desc != NULL );
     D_ASSERT( desc->width > 0 );
     D_ASSERT( desc->height > 0 );
     D_ASSERT( ret_window != NULL );

     if (desc->width > 4096 || desc->height > 4096)
          return DFB_LIMITEXCEEDED;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     context = stack->context;
     layer   = dfb_layer_at( context->layer_id );


     caps         = desc->caps;
     pixelformat  = desc->pixelformat;
     surface_caps = desc->surface_caps & (DSCAPS_INTERLACED    | DSCAPS_SEPARATED  |
                                          DSCAPS_PREMULTIPLIED | DSCAPS_DEPTH      |
                                          DSCAPS_STATIC_ALLOC  | DSCAPS_SYSTEMONLY |
                                          DSCAPS_VIDEOONLY);

     if (!dfb_config->translucent_windows) {
          caps &= ~DWCAPS_ALPHACHANNEL;

          /*if (DFB_PIXELFORMAT_HAS_ALPHA(pixelformat))
               pixelformat = DSPF_UNKNOWN;*/
     }

     /* Choose pixel format. */
     if (caps & DWCAPS_ALPHACHANNEL) {
          if (pixelformat == DSPF_UNKNOWN) {
               if (context->config.flags & DLCONF_PIXELFORMAT)
                    pixelformat = context->config.pixelformat;

               if (! DFB_PIXELFORMAT_HAS_ALPHA(pixelformat))
                    pixelformat = DSPF_ARGB;
          }
          else if (! DFB_PIXELFORMAT_HAS_ALPHA(pixelformat)) {
               dfb_windowstack_unlock( stack );
               return DFB_INVARG;
          }
     }
     else if (pixelformat == DSPF_UNKNOWN) {
          if (context->config.flags & DLCONF_PIXELFORMAT)
               pixelformat = context->config.pixelformat;
          else {
               D_WARN( "layer config has no pixel format, using RGB16" );

               pixelformat = DSPF_RGB16;
          }
     }

     /* Choose window surface policy */
     if ((surface_caps & DSCAPS_VIDEOONLY) ||
         (context->config.buffermode == DLBM_WINDOWS))
     {
          surface_policy = CSP_VIDEOONLY;
     }
     else if (!(surface_caps & DSCAPS_SYSTEMONLY) &&
              context->config.buffermode != DLBM_BACKSYSTEM)
     {
          if (dfb_config->window_policy != -1) {
               /* Use the explicitly specified policy. */
               surface_policy = dfb_config->window_policy;
          }
          else {
               /* Examine the hardware capabilities. */
               dfb_gfxcard_get_capabilities( &card_caps );

               if (card_caps.accel & DFXL_BLIT) {
                    if ((card_caps.blitting & DSBLIT_BLEND_ALPHACHANNEL) ||
                        !(caps & DWCAPS_ALPHACHANNEL))
                         surface_policy = CSP_VIDEOHIGH;
               }
          }
     }

     dfb_surface_caps_apply_policy( surface_policy, &surface_caps );

     if (caps & DWCAPS_DOUBLEBUFFER)
          surface_caps |= DSCAPS_DOUBLE;


     memset( &config, 0, sizeof(CoreWindowConfig) );

     config.bounds.x = desc->posx;
     config.bounds.y = desc->posy;
     config.bounds.w = desc->width;
     config.bounds.h = desc->height;
     config.stacking = (desc->flags & DWDESC_STACKING) ? desc->stacking : DWSC_MIDDLE;

     config.events   = DWET_ALL;

     /* Auto enable blending for ARGB only, not indexed. */
     if ((caps & DWCAPS_ALPHACHANNEL) &&
          DFB_PIXELFORMAT_HAS_ALPHA (pixelformat) &&
         !DFB_PIXELFORMAT_IS_INDEXED(pixelformat))
          config.options |= DWOP_ALPHACHANNEL;

     /* Override automatic settings. */
     if (desc->flags & DWDESC_OPTIONS)
          config.options = desc->options;

     /* Create the window object. */
     window = dfb_core_create_window( layer->core );

     window->id        = ++stack->id_pool;
     window->caps      = caps;
     window->stack     = stack;
     window->config    = config;
     window->parent_id = (desc->flags & DWDESC_PARENT) ? desc->parent_id : 0;

     ret = dfb_wm_preconfigure_window( stack, window );
     if(ret) {
          fusion_object_destroy( &window->object );
          dfb_windowstack_unlock( stack );
          return ret;
     }

     /* wm may have changed values */
     config = window->config;
     caps   = window->caps;


     /* Create the window's surface using the layer's palette if possible. */
     if (! (caps & DWCAPS_INPUTONLY)) {
          if (context->config.buffermode == DLBM_WINDOWS) {
               CoreLayerRegion *region = NULL;

               /* Create a region for the window. */
               ret = create_region( layer->core, context, window,
                                    pixelformat, surface_caps, &region, &surface );
               if (ret) {
                    fusion_object_destroy( &window->object );
                    dfb_windowstack_unlock( stack );
                    return ret;
               }

               /* Link the region into the window structure. */
               dfb_layer_region_link( &window->region, region );
               dfb_layer_region_unref( region );

               /* Link the surface into the window structure. */
               dfb_surface_link( &window->surface, surface );
               dfb_surface_unref( surface );
          }
          else {
               CoreLayerRegion *region;

               /* Get the primary region of the layer context. */
               ret = dfb_layer_context_get_primary_region( context, true, &region );
               if (ret) {
                    fusion_object_destroy( &window->object );
                    dfb_windowstack_unlock( stack );
                    return ret;
               }

               /* Link the primary region into the window structure. */
               dfb_layer_region_link( &window->primary_region, region );
               dfb_layer_region_unref( region );

               D_DEBUG_AT( Core_Windows, "  -> %dx%d %s %s\n",
                           window->config.bounds.w, window->config.bounds.h,
                           dfb_pixelformat_name(pixelformat),
                           (surface_policy == CSP_VIDEOONLY) ?
                              "VIDEOONLY" :
                              ((surface_policy == CSP_SYSTEMONLY) ?
                                   "SYSTEM ONLY" :
                                   "AUTO VIDEO") );

               /* Give the WM a chance to provide its own surface. */
               if (!window->surface) {
                    /* Create the surface for the window. */
                    ret = dfb_surface_create( layer->core,
                                              config.bounds.w, config.bounds.h, pixelformat,
                                              surface_policy, surface_caps,
                                              region->surface ?
                                              region->surface->palette : NULL, &surface );
                    if (ret) {
                         dfb_layer_region_unlink( &window->primary_region );
                         fusion_object_destroy( &window->object );
                         dfb_windowstack_unlock( stack );
                         return ret;
                    }

                    /* Link the surface into the window structure. */
                    dfb_surface_link( &window->surface, surface );
                    dfb_surface_unref( surface );
               }
          }
     }
     else
          D_DEBUG_AT( Core_Windows, "  -> %dx%d - INPUT ONLY!\n",
                      window->config.bounds.w, window->config.bounds.h );

     D_DEBUG_AT( Core_Windows, "  -> %p\n", window );

     /* Pass the new window to the window manager. */
     ret = dfb_wm_add_window( stack, window );
     if (ret) {
          if (window->surface) {
               dfb_surface_unlink( &window->surface );
          }

          if (window->primary_region)
               dfb_layer_region_unlink( &window->primary_region );

          if (window->region)
               dfb_layer_region_unlink( &window->region );

          fusion_object_destroy( &window->object );
          dfb_windowstack_unlock( stack );
          return ret;
     }

     /* Indicate that initialization is complete. */
     D_FLAGS_SET( window->flags, CWF_INITIALIZED );

     /* Increase number of windows. */
     stack->num++;

     /* Finally activate the object. */
     fusion_object_activate( &window->object );

     fusion_reactor_direct( window->object.reactor, true );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     /* Return the new window. */
     *ret_window = window;

     return DFB_OK;;
}

void
dfb_window_destroy( CoreWindow *window )
{
     DFBWindowEvent   evt;
     CoreWindowStack *stack;
     BoundWindow     *bound;

     D_ASSERT( window != NULL );
     D_ASSERT( DFB_WINDOW_INITIALIZED( window ) );

     D_DEBUG_AT( Core_Windows, "dfb_window_destroy (%p) [%4d,%4d - %4dx%4d]\n",
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ) );

     D_ASSUME( window->stack != NULL );

     stack = window->stack;
     if (!stack)
          return;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     /* Avoid multiple destructions. */
     if (DFB_WINDOW_DESTROYED( window )) {
          D_DEBUG_AT( Core_Windows, "%p already destroyed.\n", window );
          dfb_windowstack_unlock( stack );
          return;
     }

     /* Unbind bound windows. */
     bound = window->bound_windows;
     while (bound) {
          BoundWindow *next = (BoundWindow*)bound->link.next;

          direct_list_remove( (DirectLink**)&window->bound_windows, &bound->link );

          bound->window->boundto = NULL;

          SHFREE( stack->shmpool, bound );

          bound = next;
     }

     /* Unbind this window. */
     if (window->boundto)
          dfb_window_unbind( window->boundto, window );

     /* Make sure the window is no longer visible. */
     dfb_window_set_opacity( window, 0 );

     /* Stop managing the window. */
     dfb_wm_remove_window( stack, window );

     /* Indicate destruction. */
     D_FLAGS_SET( window->flags, CWF_DESTROYED );

     /* Hardware allocated? */
     if (window->region) {
          /* Disable region (removing it from hardware). */
          dfb_layer_region_disable( window->region );

          /* Unlink from structure. */
          dfb_layer_region_unlink( &window->region );
     }

     /* Unlink the window's surface. */
     if (window->surface) {
          dfb_surface_unlink( &window->surface );
     }

     /* Decrease number of windows. */
     stack->num--;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );


     /* Notify listeners. */
     evt.type = DWET_DESTROYED;
     dfb_window_post_event( window, &evt );
}

DFBResult
dfb_window_change_stacking( CoreWindow             *window,
                            DFBWindowStackingClass  stacking )
{
     DFBResult         ret;
     CoreWindowStack  *stack;
     CoreWindowConfig  config;

     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.stacking = stacking;

     /* Let the window manager do its work. */
     ret = dfb_wm_set_window_config( window, &config, CWCF_STACKING );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_raise( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, window, 1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_lower( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, window, -1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_raisetotop( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, NULL, 1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_lowertobottom( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, NULL, 0 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_putatop( CoreWindow *window,
                    CoreWindow *lower )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, lower, 1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_putbelow( CoreWindow *window,
                     CoreWindow *upper )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, upper, -1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_config( CoreWindow             *window,
                       const CoreWindowConfig *config,
                       CoreWindowConfigFlags   flags )
{
     DFBResult         ret;
     CoreWindowStack  *stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     ret = dfb_wm_set_window_config( window, config, flags );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

static DFBResult
move_window( CoreWindow *window,
             int         x,
             int         y )
{
     DFBResult         ret; 
     CoreWindowConfig  config;
     BoundWindow      *bound;

     config.bounds.x = x;
     config.bounds.y = y;

     ret = dfb_wm_set_window_config( window, &config, CWCF_POSITION );
     if (ret)
          return ret;

     direct_list_foreach (bound, window->bound_windows) {
          move_window( bound->window,
                       window->config.bounds.x + bound->x,
                       window->config.bounds.y + bound->y );
     }

     return DFB_OK;
}

DFBResult
dfb_window_move( CoreWindow *window,
                 int         x,
                 int         y,
                 bool        relative )
{
     DFBResult        ret;    
     CoreWindowStack *stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (window->boundto) {
          dfb_windowstack_unlock( stack );
          return DFB_UNSUPPORTED;
     }

     if (relative) {
          x += window->config.bounds.x;
          y += window->config.bounds.y;
     }

     if (x == window->config.bounds.x && y == window->config.bounds.y) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     ret = move_window( window, x, y );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_bounds( CoreWindow *window,
                       int         x,
                       int         y,
                       int         width,
                       int         height )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;
     int               old_x;
     int               old_y;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     old_x = window->config.bounds.x;
     old_y = window->config.bounds.y;

     if (window->boundto) {
          if (old_x != x || old_y != y) {
               dfb_windowstack_unlock( stack );
               return DFB_UNSUPPORTED;
          }
     }

     config.bounds.x = x;
     config.bounds.y = y;
     config.bounds.w = width;
     config.bounds.h = height;

     if (window->config.bounds.x == x &&
         window->config.bounds.y == y &&
         window->config.bounds.w == width &&
         window->config.bounds.h == height)
     {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     ret = dfb_wm_set_window_config( window, &config, CWCF_POSITION | CWCF_SIZE );
     if (ret) {
          dfb_windowstack_unlock( stack );
          return ret;
     }

     if (old_x != x || old_y != y) {
          BoundWindow *bound;

          direct_list_foreach (bound, window->bound_windows) {
               move_window( bound->window,
                            window->config.bounds.x + bound->x,
                            window->config.bounds.y + bound->y );
          }
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_window_resize( CoreWindow   *window,
                   int           width,
                   int           height )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_DEBUG_AT( Core_Windows, "dfb_window_resize (%p) [%4d,%4d - %4dx%4d -> %dx%d]\n",
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ), width, height );

     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     if (width > 4096 || height > 4096)
          return DFB_LIMITEXCEEDED;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (window->config.bounds.w == width && window->config.bounds.h == height) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     config.bounds.w = width;
     config.bounds.h = height;

     ret = dfb_wm_set_window_config( window, &config, CWCF_SIZE );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_bind( CoreWindow *window,
                 CoreWindow *source,
                 int         x,
                 int         y )
{
     DFBResult        ret;
     CoreWindowStack *stack = window->stack;
     BoundWindow     *bound;

     if (window == source)
          return DFB_UNSUPPORTED;
     
     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (DFB_WINDOW_DESTROYED( source )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     bound = SHCALLOC( stack->shmpool, 1, sizeof(BoundWindow) );
     if (!bound) {
          dfb_windowstack_unlock( stack );
          return DFB_NOSHAREDMEMORY;
     }                    

     if (source->boundto)
          dfb_window_unbind( source->boundto, source );

     ret = move_window( source,
                        window->config.bounds.x + x,
                        window->config.bounds.y + y );
     if (ret) {
          SHFREE( stack->shmpool, bound );
          dfb_windowstack_unlock( stack );
          return ret;
     }

     bound->window = source;
     bound->x      = x;
     bound->y      = y;

     direct_list_append( (DirectLink**)&window->bound_windows, &bound->link );

     source->boundto = window;

     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_window_unbind( CoreWindow *window,
                   CoreWindow *source )
{
     CoreWindowStack *stack = window->stack;
     BoundWindow     *bound;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (DFB_WINDOW_DESTROYED( source )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (source->boundto != window) {
          dfb_windowstack_unlock( stack );
          return DFB_UNSUPPORTED;
     }

     direct_list_foreach (bound, window->bound_windows) {
          if (bound->window == source) {
               direct_list_remove( (DirectLink**)&window->bound_windows, &bound->link );

               bound->window->boundto = NULL;

               SHFREE( stack->shmpool, bound );

               break;
          }
     }

     if (!bound)
          D_BUG( "window not found" );

     dfb_windowstack_unlock( stack );

     return bound ? DFB_OK : DFB_ITEMNOTFOUND;
}
     
DFBResult
dfb_window_set_colorkey( CoreWindow *window,
                         u32         color_key )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (window->config.color_key == color_key) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     config.color_key = color_key;

     ret = dfb_wm_set_window_config( window, &config, CWCF_COLOR_KEY );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_opacity( CoreWindow *window,
                        u8          opacity )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (window->config.opacity == opacity) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     config.opacity = opacity;

     ret = dfb_wm_set_window_config( window, &config, CWCF_OPACITY );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_change_options( CoreWindow       *window,
                           DFBWindowOptions  disable,
                           DFBWindowOptions  enable )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_ASSUME( disable | enable );

     if (!disable && !enable)
          return DFB_OK;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.options = (window->config.options & ~disable) | enable;

     ret = dfb_wm_set_window_config( window, &config, CWCF_OPTIONS );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_opaque( CoreWindow      *window,
                       const DFBRegion *region )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     DFB_REGION_ASSERT_IF( region );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.opaque.x1 = 0;
     config.opaque.y1 = 0;
     config.opaque.x2 = window->config.bounds.w - 1;
     config.opaque.y2 = window->config.bounds.h - 1;

     if (region && !dfb_region_region_intersect( &config.opaque, region ))
          ret = DFB_INVAREA;
     else
          ret = dfb_wm_set_window_config( window, &config, CWCF_OPAQUE );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_change_events( CoreWindow         *window,
                          DFBWindowEventType  disable,
                          DFBWindowEventType  enable )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_ASSUME( disable | enable );

     if (!disable && !enable)
          return DFB_OK;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.events = (window->config.events & ~disable) | enable;

     ret = dfb_wm_set_window_config( window, &config, CWCF_EVENTS );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_key_selection( CoreWindow                    *window,
                              DFBWindowKeySelection          selection,
                              const DFBInputDeviceKeySymbol *keys,
                              unsigned int                   num_keys )
{
    DFBResult         ret;
    CoreWindowConfig  config;
    CoreWindowStack  *stack = window->stack;

    D_ASSERT( selection == DWKS_ALL || selection == DWKS_NONE || selection == DWKS_LIST );
    D_ASSERT( keys != NULL || selection != DWKS_LIST );
    D_ASSERT( num_keys > 0 || selection != DWKS_LIST );

    /* Lock the window stack. */
    if (dfb_windowstack_lock( stack ))
         return DFB_FUSION;

    /* Never call WM after destroying the window. */
    if (DFB_WINDOW_DESTROYED( window )) {
         dfb_windowstack_unlock( stack );
         return DFB_DESTROYED;
    }

    config.key_selection = selection;
    config.keys          = (DFBInputDeviceKeySymbol*) keys; /* FIXME */
    config.num_keys      = num_keys;

    ret = dfb_wm_set_window_config( window, &config, CWCF_KEY_SELECTION );

    /* Unlock the window stack. */
    dfb_windowstack_unlock( stack );

    return ret;
}

DFBResult
dfb_window_change_grab( CoreWindow       *window,
                        CoreWMGrabTarget  target,
                        bool              grab )
{
     DFBResult        ret;
     CoreWMGrab       wmgrab;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     wmgrab.target = target;

     if (grab)
         ret = dfb_wm_grab( window, &wmgrab );
     else
         ret = dfb_wm_ungrab( window, &wmgrab );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_grab_key( CoreWindow                 *window,
                     DFBInputDeviceKeySymbol     symbol,
                     DFBInputDeviceModifierMask  modifiers )
{
     DFBResult        ret;
     CoreWMGrab       grab;
     CoreWindowStack *stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     grab.target    = CWMGT_KEY;
     grab.symbol    = symbol;
     grab.modifiers = modifiers;

     ret = dfb_wm_grab( window, &grab );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_ungrab_key( CoreWindow                 *window,
                       DFBInputDeviceKeySymbol     symbol,
                       DFBInputDeviceModifierMask  modifiers )
{
     DFBResult        ret;
     CoreWMGrab       grab;
     CoreWindowStack *stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     grab.target    = CWMGT_KEY;
     grab.symbol    = symbol;
     grab.modifiers = modifiers;

     ret = dfb_wm_ungrab( window, &grab );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_repaint( CoreWindow          *window,
                    const DFBRegion     *region,
                    DFBSurfaceFlipFlags  flags )
{
     DFBResult        ret;
     CoreWindowStack *stack = window->stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );

     DFB_REGION_ASSERT_IF( region );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     ret = dfb_wm_update_window( window, region, flags );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

void
dfb_window_post_event( CoreWindow     *window,
                       DFBWindowEvent *event )
{
     D_ASSERT( window != NULL );
     D_ASSERT( event != NULL );

     D_ASSUME( !DFB_WINDOW_DESTROYED( window ) || event->type == DWET_DESTROYED );

     if (! (event->type & window->config.events))
          return;

     gettimeofday( &event->timestamp, NULL );

     event->clazz     = DFEC_WINDOW;
     event->window_id = window->id;

     D_ASSUME( window->stack != NULL );

     if (window->stack) {
          CoreWindowStack *stack = window->stack;

          event->cx = stack->cursor.x;
          event->cy = stack->cursor.y;
     }

     if (!core_window_filter( window, event ))
          dfb_window_dispatch( window, event, dfb_window_globals );
}

DFBResult
dfb_window_send_configuration( CoreWindow *window )
{
     DFBWindowEvent event;

     D_ASSUME( !DFB_WINDOW_DESTROYED( window ) );

     event.type = DWET_POSITION_SIZE;
     event.x    = window->config.bounds.x;
     event.y    = window->config.bounds.y;
     event.w    = window->config.bounds.w;
     event.h    = window->config.bounds.h;

     dfb_window_post_event( window, &event );

     return DFB_OK;
}

DFBResult
dfb_window_request_focus( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     ret = dfb_wm_request_focus( window );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBWindowID
dfb_window_id( const CoreWindow *window )
{
     D_ASSERT( window != NULL );

     return window->id;
}

CoreSurface *
dfb_window_surface( const CoreWindow *window )
{
     D_ASSERT( window != NULL );

     return window->surface;
}

/******************************************************************************/

static bool
core_window_filter( CoreWindow *window, const DFBWindowEvent *event )
{
     switch (event->type) {
          case DWET_GOTFOCUS:
               D_FLAGS_SET( window->flags, CWF_FOCUSED );
               break;

          case DWET_LOSTFOCUS:
               D_FLAGS_CLEAR( window->flags, CWF_FOCUSED );
               break;

          case DWET_ENTER:
               D_FLAGS_SET( window->flags, CWF_ENTERED );
               break;

          case DWET_LEAVE:
               D_FLAGS_CLEAR( window->flags, CWF_ENTERED );
               break;

          default:
               break;
     }

     return false;
}

