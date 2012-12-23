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

#include "CoreLayerRegion.h"
#include "Task.h"
#include "Util.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <gfx/util.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreLayerRegion, "DirectFB/CoreLayerRegion", "DirectFB CoreLayerRegion" );

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
ILayerRegion_Real::FlipUpdateStereo(
                    const DFBRegion                           *left,
                    const DFBRegion                           *right,
                    DFBSurfaceFlipFlags                        flags
)
{
    D_DEBUG_AT( DirectFB_CoreLayerRegion, "ILayerRegion_Requestor::%s()\n", __FUNCTION__ );

    return dfb_layer_region_flip_update_stereo( obj, left, right, flags );
}





D_DEBUG_DOMAIN( Core_Layers, "Core/Layers", "DirectFB Display Layer Core" );



DisplayTask::DisplayTask( CoreLayerRegion       *region,
                          const DFBRegion       *update,
                          DFBSurfaceFlipFlags    flip_flags,
                          CoreSurfaceAllocation *allocation )
     :
     SurfaceTask( region->surface_accessor ),
     region( region ),
     update( NULL ),
     flip_flags( flip_flags ),
     allocation( allocation )
{
     D_DEBUG_AT( Core_Layers, "DisplayTask::%s( %p )\n", __FUNCTION__, this );

     context = region->context;
     layer   = dfb_layer_at( context->layer_id );
     index   = dfb_surface_buffer_index( allocation->buffer );

     if (update) {
          this->update = &this->update_region;

          *this->update = *update;
     }

     D_DEBUG_AT( Core_Layers, "  -> index %d\n", index );

     flags = (TaskFlags)(flags | TASK_FLAG_NOSYNC | TASK_FLAG_EMITNOTIFIES);
}

DisplayTask::~DisplayTask()
{
}

DFBResult
DisplayTask::Generate( CoreLayerRegion       *region,
                       const DFBRegion       *update,
                       DFBSurfaceFlipFlags    flags )
{
     DFBResult              ret;
     CoreSurface           *surface;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;

     surface = region->surface;

     D_DEBUG_AT( Core_Layers, "DisplayTask::%s( region %p, surface %p, flip %d )\n", __FUNCTION__, region, surface, surface->flips );

     // FIXME: move to helper class
     //

     buffer = dfb_surface_get_buffer3( surface, CSBR_FRONT, DSSE_LEFT, surface->flips );

     allocation = dfb_surface_buffer_find_allocation( buffer, region->surface_accessor, CSAF_READ, true );
     if (!allocation) {
          /* If no allocation exists, create one. */
          ret = dfb_surface_pools_allocate( buffer, region->surface_accessor, CSAF_READ, &allocation );
          if (ret) {
               D_DERROR( ret, "Core/LayerRegion: Buffer allocation failed!\n" );
               return ret;
          }
     }

     dfb_surface_allocation_update( allocation, CSAF_READ );

     DisplayTask *task = new DisplayTask( region, update, flags, allocation );

     task->AddAccess( allocation, CSAF_READ );

     D_DEBUG_AT( Core_Layers, "  -> flushing task %p\n", task );

     task->Flush();

     return DFB_OK;
}

DFBResult
DisplayTask::Setup()
{
     D_DEBUG_AT( Core_Layers, "DisplayTask::%s( %p )\n", __FUNCTION__, this );

     if (layer->display_task)
          layer->display_task->addNotify( this, true );

     layer->display_task = this;

     return SurfaceTask::Setup();
}

void
DisplayTask::Finalise()
{
     D_DEBUG_AT( Core_Layers, "DisplayTask::%s( %p )\n", __FUNCTION__, this );

//     D_ASSERT( layer->display_tasks[index] == this );

//     layer->display_tasks[index] = NULL;

     if (layer->display_task == this)
          layer->display_task = NULL;

     SurfaceTask::Finalise();
}

DFBResult
DisplayTask::Run()
{
     DFBResult                ret;
     DFBRegion                rotated;
     DFBRegion                unrotated;
     CoreLayer               *layer;
     CoreLayerContext        *context;
     CoreSurface             *surface;
     const DisplayLayerFuncs *funcs;
     CoreSurfaceBufferLock    left;

     context = region->context;
     surface = region->surface;
     layer   = dfb_layer_at( context->layer_id );

     D_DEBUG_AT( Core_Layers, "DisplayTask::%s( %p )\n", __FUNCTION__, this );

     D_ASSERT( layer->funcs != NULL );
     //D_ASSERT( layer->display_tasks[index] == NULL );

     funcs = layer->funcs;

     D_ASSUME( funcs->FlipRegion != NULL );

     dfb_surface_buffer_lock_init( &left, accessor, CSAF_READ );

     left.task = this;

     ret = dfb_surface_pool_lock( allocation->pool, allocation, &left );
     if (ret) {
          dfb_layer_region_unlock( region );
          return ret;
     }

     D_DEBUG_AT( Core_Layers, "  -> setting task for index %d\n", index );

//     layer->display_tasks[index] = this;

     /* Depending on the buffer mode... */
     switch (region->config.buffermode) {
          case DLBM_TRIPLE:
          case DLBM_BACKVIDEO:
               /* Check if simply swapping the buffers is possible... */
               if ((flip_flags & DSFLIP_SWAP) ||
                   (!(flip_flags & DSFLIP_BLIT) && !surface->rotation &&
                    (!update || (update->x1 == 0 &&
                                 update->y1 == 0 &&
                                 update->x2 == surface->config.size.w - 1 &&
                                 update->y2 == surface->config.size.h - 1))))
               {
                    D_DEBUG_AT( Core_Layers, "  -> Flipping region using driver...\n" );

                    if (funcs->FlipRegion)
                         ret = funcs->FlipRegion( layer,
                                                  layer->driver_data,
                                                  layer->layer_data,
                                                  region->region_data,
                                                  surface, flip_flags,
                                                  update, &left,
                                                  NULL, NULL );
                    break;
               }

               /* fall through */

          case DLBM_BACKSYSTEM:
          case DLBM_FRONTONLY:
               /* Tell the driver about the update if the region is realized. */
               if (funcs->UpdateRegion && D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                    const DFBRegion *_update = update;

                    D_DEBUG_AT( Core_Layers, "  -> Notifying driver about updated content...\n" );

                    if( !_update ) {
                         unrotated = DFB_REGION_INIT_FROM_RECTANGLE_VALS( 0, 0,
                                        region->config.width, region->config.height );
                         _update    = &unrotated;
                    }
                    dfb_region_from_rotated( &rotated, _update, &surface->config.size, surface->rotation );

                    ret = funcs->UpdateRegion( layer,
                                               layer->driver_data,
                                               layer->layer_data,
                                               region->region_data,
                                               surface,
                                               &rotated, &left,
                                               NULL, NULL );
               }
               break;

          default:
               D_BUG("unknown buffer mode");
               ret = DFB_BUG;
     }

     /* Unlock region buffer since the lock is no longer needed. */
     dfb_surface_pool_unlock( left.allocation->pool, left.allocation, &left );
     dfb_surface_buffer_lock_deinit( &left );

     return DFB_OK;
}

std::string
DisplayTask::Describe()
{
     return SurfaceTask::Describe() + Util::PrintF( "  index %d", index );
}




extern "C" {


DFBResult
dfb_layer_region_flip_update_TASK( CoreLayerRegion     *region,
                                   const DFBRegion     *update,
                                   DFBSurfaceFlipFlags  flags )
{
     DFBResult                ret = DFB_OK;
     CoreLayer               *layer;
     CoreLayerContext        *context;
     CoreSurface             *surface;
     const DisplayLayerFuncs *funcs;

     if (update)
          D_DEBUG_AT( Core_Layers,
                      "dfb_layer_region_flip_update( %p, %p, 0x%08x ) <- [%d, %d - %dx%d]\n",
                      region, update, flags, DFB_RECTANGLE_VALS_FROM_REGION( update ) );
     else
          D_DEBUG_AT( Core_Layers,
                      "dfb_layer_region_flip_update( %p, %p, 0x%08x )\n", region, update, flags );


     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     /* Check for stereo region */
     if (region->config.options & DLOP_STEREO) {
          ret = dfb_layer_region_flip_update_stereo( region, update, update, flags );
          dfb_layer_region_unlock( region );
          return ret;
     }

     D_ASSUME( region->surface != NULL );

     /* Check for NULL surface. */
     if (!region->surface) {
          D_DEBUG_AT( Core_Layers, "  -> No surface => no update!\n" );
          dfb_layer_region_unlock( region );
          return DFB_UNSUPPORTED;
     }

     context = region->context;
     surface = region->surface;
     layer   = dfb_layer_at( context->layer_id );

     D_ASSERT( layer->funcs != NULL );

     funcs = layer->funcs;

     /* Unfreeze region? */
     if (D_FLAGS_IS_SET( region->state, CLRSF_FROZEN )) {
          D_FLAGS_CLEAR( region->state, CLRSF_FROZEN );

          if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
               ret = dfb_layer_region_set( region, &region->config, CLRCF_ALL, surface );
               if (ret)
                    D_DERROR( ret, "Core/LayerRegion: set_region() in dfb_layer_region_flip_update() failed!\n" );
          }
          else if (D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
               ret = dfb_layer_region_realize( region );
               if (ret)
                    D_DERROR( ret, "Core/LayerRegion: realize_region() in dfb_layer_region_flip_update() failed!\n" );
          }

          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     /* Depending on the buffer mode... */
     switch (region->config.buffermode) {
          case DLBM_TRIPLE:
          case DLBM_BACKVIDEO:
               /* Check if simply swapping the buffers is possible... */
               if ((flags & DSFLIP_SWAP) ||
                   (!(flags & DSFLIP_BLIT) && !surface->rotation &&
                    (!update || (update->x1 == 0 &&
                                 update->y1 == 0 &&
                                 update->x2 == surface->config.size.w - 1 &&
                                 update->y2 == surface->config.size.h - 1))))
               {
                    dfb_surface_lock( surface );

                    dfb_surface_flip( surface, false );

                    /* Use the driver's routine if the region is realized. */
                    if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                         D_DEBUG_AT( Core_Layers, "  -> Issuing display task...\n" );

                         DisplayTask::Generate( region, update, flags );
                    }

                    dfb_surface_unlock( surface );
                    break;
               }

               /* fall through */

          case DLBM_BACKSYSTEM:
               D_DEBUG_AT( Core_Layers, "  -> Going to copy portion...\n" );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) {
                    D_DEBUG_AT( Core_Layers, "  -> Waiting for VSync...\n" );

                    dfb_layer_wait_vsync( layer );
               }

               D_DEBUG_AT( Core_Layers, "  -> Copying content from back to front buffer...\n" );

               /* ...or copy updated contents from back to front buffer. */
               if (surface->rotation) {
                    D_UNIMPLEMENTED();
                    // FIXME: use graphics state client
                    dfb_back_to_front_copy_rotation( surface, update, surface->rotation );
               }
               else {
                    // FIXME: don't create and destroy a client each time
                    CardState               state;
                    CoreGraphicsStateClient client;

                    /* Initialise the graphics state used for rendering */
                    dfb_state_init( &state, core_dfb );

                    /* Create a client to use the task manager if enabled */
                    ret = CoreGraphicsStateClient_Init( &client, &state );
                    if (ret) {
                         dfb_state_destroy( &state );
                         return ret;
                    }

                    /* Make legacy functions use state client */
                    state.client = &client;

                    dfb_gfx_copy_regions_client( surface, CSBR_BACK, DSSE_LEFT, surface, CSBR_FRONT, DSSE_LEFT, update, 1, 0, 0, &client );

                    CoreGraphicsStateClient_Deinit( &client );
                    dfb_state_destroy( &state );
               }

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAIT) {
                    D_DEBUG_AT( Core_Layers, "  -> Waiting for VSync...\n" );

                    dfb_layer_wait_vsync( layer );
               }

               /* fall through */

          case DLBM_FRONTONLY:
               /* Tell the driver about the update if the region is realized. */
               if (funcs->UpdateRegion && D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                    D_DEBUG_AT( Core_Layers, "  -> Issuing display task...\n" );

                    dfb_surface_lock( surface );

                    DisplayTask::Generate( region, update, flags );

                    dfb_surface_unlock( surface );
               }
               break;

          default:
               D_BUG("unknown buffer mode");
               ret = DFB_BUG;
     }

     D_DEBUG_AT( Core_Layers, "  -> done.\n" );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return ret;
}

}


}
