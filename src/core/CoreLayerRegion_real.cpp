/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <directfb.h>    // include here to prevent it being included indirectly causing nested extern "C"

#include "CoreLayerRegion.h"
#include "DisplayTask.h"
#include "Util.h"


extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/screen.h>
#include <core/surface_pool.h>
#include <core/system.h>

#include <gfx/util.h>
}


#include <direct/Lists.h>

#include <core/Debug.h>
#include <core/Task.h>
#include <core/Util.h>


#include "CoreLayerRegion.h"


D_DEBUG_DOMAIN( DirectFB_CoreLayerRegion,   "DirectFB/CoreLayerRegion",   "DirectFB CoreLayerRegion" );
D_DEBUG_DOMAIN( DirectFB_Task_Display,      "DirectFB/Task/Display",      "DirectFB DisplayTask" );

/*********************************************************************************************************************/

namespace DirectFB {



DFBResult
ILayerRegion_Real::GetSurface(
                    CoreSurface                              **ret_surface
)
{
     D_DEBUG_AT( DirectFB_CoreLayerRegion, "ILayerRegion_Requestor::%s()\n", __FUNCTION__ );

     return dfb_layer_region_get_surface( obj, ret_surface );
}


DFBResult
ILayerRegion_Real::FlipUpdate(
                    const DFBRegion                           *update,
                    DFBSurfaceFlipFlags                        flags
)
{
    D_DEBUG_AT( DirectFB_CoreLayerRegion, "ILayerRegion_Requestor::%s()\n", __FUNCTION__ );

    return dfb_layer_region_flip_update( obj, update, flags );
}


DFBResult
ILayerRegion_Real::FlipUpdate2(
                    const DFBRegion                           *left_update,
                    const DFBRegion                           *right_update,
                    DFBSurfaceFlipFlags                        flags,
                    s64                                        pts
)
{
    D_DEBUG_AT( DirectFB_CoreLayerRegion, "ILayerRegion_Requestor::%s( flags 0x%08x, pts %lld )\n", __FUNCTION__, flags, (long long) pts );

    return dfb_layer_region_flip_update2( obj, left_update, right_update, flags, pts, NULL );
}


DFBResult
ILayerRegion_Real::FlipUpdateStereo(
                    const DFBRegion                           *left,
                    const DFBRegion                           *right,
                    DFBSurfaceFlipFlags                        flags
)
{
    D_DEBUG_AT( DirectFB_CoreLayerRegion, "ILayerRegion_Requestor::%s()\n", __FUNCTION__ );

    return dfb_layer_region_flip_update_stereo( obj, left, right, flags );
}

/*********************************************************************************************************************/

void
CoreLayersFPSHandle( CoreLayer *layer )
{
     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     if (dfb_config->layers_fps) {
          if (!layer->fps)
               layer->fps = new Util::FPS();

          D_ASSERT( layer->fps != NULL );

          if (layer->fps->Count( dfb_config->layers_fps ))
               D_INFO( "Core/Layer/%u: FPS %s\n", layer->shared->layer_id, layer->fps->Get().buffer() );
     }
}

/*********************************************************************************************************************/


extern "C" {

DFBResult
dfb_layer_region_flip_update2( CoreLayerRegion      *region,
                               const DFBRegion      *left_update,
                               const DFBRegion      *right_update,
                               DFBSurfaceFlipFlags   flags,
                               long long             pts,
                               DisplayTask         **ret_task )
{
     DFBResult            ret = DFB_OK;
     CoreLayer           *layer;
     CoreSurface         *surface;
     DFBSurfaceStereoEye  eyes = DSSE_NONE;

     if (!dfb_config->task_manager) {
          if (region->config.options & DLOP_STEREO)
               return dfb_layer_region_flip_update_stereo( region, left_update, right_update, flags );

          return dfb_layer_region_flip_update( region, left_update, flags );
     }

     D_DEBUG_AT( DirectFB_Task_Display, "%s( %p, %p, %p, 0x%08x, pts %lld )\n",
                 __FUNCTION__, region, left_update, right_update, flags, pts );
     if (left_update)
          D_DEBUG_AT( DirectFB_Task_Display, "Left: [%d, %d - %dx%d]\n", DFB_RECTANGLE_VALS_FROM_REGION( left_update ) );
     if (right_update)
          D_DEBUG_AT( DirectFB_Task_Display, "Right: [%d, %d - %dx%d]\n", DFB_RECTANGLE_VALS_FROM_REGION( right_update ) );

     if (flags & DSFLIP_UPDATE)
          D_UNIMPLEMENTED();

     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( region->surface != NULL );

     /* Check for NULL surface. */
     if (!region->surface) {
          D_DEBUG_AT( DirectFB_Task_Display, "  -> No surface => no update!\n" );
          dfb_layer_region_unlock( region );
          return DFB_UNSUPPORTED;
     }

     surface = region->surface;
     layer   = dfb_layer_at( region->layer_id );

     CoreLayersFPSHandle( layer );

     dfb_surface_lock( surface );

     if (!(surface->frametime_config.flags & DFTCF_INTERVAL))
          dfb_screen_get_frame_interval( layer->screen, &surface->frametime_config.interval );

     if (ret_task)
          *ret_task = NULL;

     /* Depending on the buffer mode... */
     switch (region->config.buffermode) {
          case DLBM_TRIPLE:
          case DLBM_BACKVIDEO:
               /* Check if simply swapping the buffers is possible... */
               if ((flags & DSFLIP_SWAP) ||
                   (!(flags & DSFLIP_BLIT) && !surface->rotation &&
                    ((!left_update && !right_update) ||      // FIXME: below code crashes if only one is set
                     ((left_update->x1 == 0 &&
                       left_update->y1 == 0 &&
                       left_update->x2 == surface->config.size.w - 1 &&
                       left_update->y2 == surface->config.size.h - 1) &&
                      (right_update->x1 == 0 &&
                       right_update->y1 == 0 &&
                       right_update->x2 == surface->config.size.w - 1 &&
                       right_update->y2 == surface->config.size.h - 1)))))
               {
                    dfb_surface_flip_buffers( surface, false );

                    /* Use the driver's routine if the region is realized. */
                    if (D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
                         D_DEBUG_AT( DirectFB_Task_Display, "  -> Issuing display task...\n" );

                         DisplayTask::Generate( region, left_update, right_update, flags, pts, ret_task );
                    }
                    break;
               }

               /* fall through */

          case DLBM_BACKSYSTEM:
               D_DEBUG_AT( DirectFB_Task_Display, "  -> Going to copy portion...\n" );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) {
                    D_DEBUG_AT( DirectFB_Task_Display, "  -> Waiting for VSync...\n" );

                    dfb_surface_unlock( surface );
                    dfb_layer_region_unlock( region );

                    dfb_layer_wait_vsync( layer );

                    dfb_layer_region_lock( region );
                    surface = region->surface;
                    dfb_surface_lock( surface );
               }

               D_DEBUG_AT( DirectFB_Task_Display, "  -> Copying content from back to front buffer...\n" );

               /* ...or copy updated contents from back to front buffer. */
               D_FLAGS_SET( eyes, DSSE_LEFT );

               if (region->config.options & DLOP_STEREO)
                    D_FLAGS_SET( eyes, DSSE_RIGHT );

               dfb_back_to_front_copy_stereo( surface, eyes, left_update, right_update, surface->rotation );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAIT) {
                    D_DEBUG_AT( DirectFB_Task_Display, "  -> Waiting for VSync...\n" );

                    dfb_surface_unlock( surface );
                    dfb_layer_region_unlock( region );

                    dfb_layer_wait_vsync( layer );

                    dfb_layer_region_lock( region );
                    surface = region->surface;
                    dfb_surface_lock( surface );
               }

               /* fall through */

          case DLBM_FRONTONLY:
               if (D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
                    /* Tell the driver about the update if the region is realized. */
                    D_DEBUG_AT( DirectFB_Task_Display, "  -> Issuing display task...\n" );

                    DisplayTask::Generate( region, left_update, right_update, flags, pts, ret_task );
               }
               break;

          default:
               D_BUG("unknown buffer mode");
               ret = DFB_BUG;
     }

     D_DEBUG_AT( DirectFB_Task_Display, "  -> done.\n" );

     dfb_surface_dispatch_update( region->surface, left_update, right_update, pts );

     dfb_surface_unlock( surface );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return ret;
}

}


}
