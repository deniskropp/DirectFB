/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, thiz, region, flags );

     ret = IDirectFBSurface_Flip( thiz, region, flags );
     if (ret)
          return ret;

     if (!data->window->config.opacity && data->base.caps & DSCAPS_PRIMARY) {
          CoreWindowConfig config = { .opacity = 0xff };

          ret = CoreWindow_SetConfig( data->window, &config, NULL, 0, CWCF_OPACITY );
     }

     return ret;
}

static DFBResult
IDirectFBSurface_Window_FlipStereo( IDirectFBSurface    *thiz,
                                    const DFBRegion     *left_region,
                                    const DFBRegion     *right_region,
                                    DFBSurfaceFlipFlags  flags )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p, %p, %p, 0x%08x )\n", __FUNCTION__, thiz, left_region, right_region, flags );

     ret = IDirectFBSurface_FlipStereo( thiz, left_region, right_region, flags );
     if (ret)
          return ret;

     if (!data->window->config.opacity && data->base.caps & DSCAPS_PRIMARY) {
          CoreWindowConfig config = { .opacity = 0xff };

          ret = CoreWindow_SetConfig( data->window, &config, NULL, 0, CWCF_OPACITY );
     }

     return ret;
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
     DFBResult    ret;
     DFBInsets    insets;
     CoreSurface *surface;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBSurface_Window)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     D_MAGIC_ASSERT( window, CoreWindow );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     ret = CoreWindow_GetInsets( window, &insets );
     if (ret)
          return ret;

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

