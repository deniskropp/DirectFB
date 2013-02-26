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
D_DEBUG_DOMAIN( DirectFB_Task_Display,    "DirectFB/Task/Display",    "DirectFB DisplayTask" );

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
    D_DEBUG_AT( DirectFB_CoreLayerRegion, "ILayerRegion_Requestor::%s()\n", __FUNCTION__ );

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

DisplayTask::DisplayTask( CoreLayerRegion       *region,
                          const DFBRegion       *left_update,
                          const DFBRegion       *right_update,
                          DFBSurfaceFlipFlags    flip_flags,
                          long long              pts,
                          CoreSurfaceAllocation *left_allocation,
                          CoreSurfaceAllocation *right_allocation,
                          bool                   stereo )
     :
     SurfaceTask( region->surface_accessor ),
     region( region ),
     left_update( NULL ),
     right_update( NULL ),
     flip_flags( flip_flags ),
     pts( pts ),
     left_allocation( left_allocation ),
     right_allocation( right_allocation ),
     stereo( stereo )
{
     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( %p )\n", __FUNCTION__, this );

     context = region->context;
     layer   = dfb_layer_at( context->layer_id );
     index   = dfb_surface_buffer_index( left_allocation->buffer );

     if (left_update) {
          this->left_update = &this->left_update_region;

          *this->left_update = *left_update;
     }

     if (right_update) {
          this->right_update = &this->right_update_region;

          *this->right_update = *right_update;
     }

     D_DEBUG_AT( DirectFB_Task_Display, "  -> index %d\n", index );

     flags = (TaskFlags)(flags | TASK_FLAG_NOSYNC);

//     if (region->config.buffermode == DLBM_FRONTONLY || region->config.buffermode == DLBM_BACKSYSTEM)
          flags = (TaskFlags)(flags | TASK_FLAG_EMITNOTIFIES);
}

DisplayTask::~DisplayTask()
{
     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( %p )\n", __FUNCTION__, this );
}

DFBResult
DisplayTask::Generate( CoreLayerRegion      *region,
                       const DFBRegion      *left_update,
                       const DFBRegion      *right_update,
                       DFBSurfaceFlipFlags   flags,
                       long long             pts,
                       DisplayTask         **ret_task )
{
     DFBResult              ret;
     CoreSurface           *surface;
     CoreSurfaceBuffer     *left_buffer      = NULL;
     CoreSurfaceBuffer     *right_buffer     = NULL;
     CoreSurfaceAllocation *left_allocation  = NULL;
     CoreSurfaceAllocation *right_allocation = NULL;
     bool                   stereo;

     D_ASSERT( region != NULL );
     FUSION_SKIRMISH_ASSERT( &region->lock );

     surface = region->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( region %p, surface %p, flips %d, flags 0x%04x, pts %lldus (%lldus from now), ret_task %p )\n",
                 __FUNCTION__, region, surface, surface->flips, flags, pts,
                 pts - direct_clock_get_time( DIRECT_CLOCK_MONOTONIC ), ret_task );

     stereo = !!(region->config.options & DLOP_STEREO);

     // FIXME: use helper class
     //

     left_buffer = dfb_surface_get_buffer3( surface, CSBR_FRONT, DSSE_LEFT, surface->flips );

     left_allocation = dfb_surface_buffer_find_allocation( left_buffer, region->surface_accessor, CSAF_READ, true );
     if (!left_allocation) {
          /* If no allocation exists, create one. */
          ret = dfb_surface_pools_allocate( left_buffer, region->surface_accessor, CSAF_READ, &left_allocation );
          if (ret) {
               D_DERROR( ret, "Core/LayerRegion: Buffer allocation failed!\n" );
               return ret;
          }
     }

     dfb_surface_allocation_update( left_allocation, CSAF_READ );

     if (stereo) {
          right_buffer = dfb_surface_get_buffer3( surface, CSBR_FRONT, DSSE_RIGHT, surface->flips );

          right_allocation = dfb_surface_buffer_find_allocation( right_buffer, region->surface_accessor, CSAF_READ, true );
          if (!right_allocation) {
               /* If no allocation exists, create one. */
               ret = dfb_surface_pools_allocate( right_buffer, region->surface_accessor, CSAF_READ, &right_allocation );
               if (ret) {
                    D_DERROR( ret, "Core/LayerRegion: Buffer allocation (right) failed!\n" );
                    return ret;
               }
          }

          dfb_surface_allocation_update( right_allocation, CSAF_READ );
     }


     DisplayTask *task = new DisplayTask( region, left_update, right_update, flags, pts, left_allocation, right_allocation, stereo );

     task->AddAccess( left_allocation, CSAF_READ );

     if (stereo)
          task->AddAccess( right_allocation, CSAF_READ );

     if (ret_task) {
          D_DEBUG_AT( DirectFB_Task_Display, "  -> returning task %p (not flushed)\n", task );

          *ret_task = task;
     }
     else {
          D_DEBUG_AT( DirectFB_Task_Display, "  -> flushing task %p\n", task );

          task->Flush();
     }

     return DFB_OK;
}

DFBResult
DisplayTask::Setup()
{
     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( %p )\n", __FUNCTION__, this );

     if (layer->display_task)
          layer->display_task->AddNotify( this, true );

     layer->display_task = this;

     return SurfaceTask::Setup();
}

void
DisplayTask::Finalise()
{
     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( %p )\n", __FUNCTION__, this );

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
     DFBRegion                left_rotated;
     DFBRegion                left_unrotated;
     DFBRegion                right_rotated;
     DFBRegion                right_unrotated;
     CoreLayer               *layer;
     CoreLayerContext        *context;
     CoreSurface             *surface;
     const DisplayLayerFuncs *funcs;
     CoreSurfaceBufferLock    left  = {0};
     CoreSurfaceBufferLock    right = {0};

     context = region->context;
     surface = region->surface;
     layer   = dfb_layer_at( context->layer_id );

     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( %p )\n", __FUNCTION__, this );

     funcs = layer->funcs;
     D_ASSERT( funcs != NULL );
     D_ASSERT( funcs->SetRegion != NULL );

     dfb_layer_region_lock( region );

     D_ASSUME( D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE ) );

     if (!D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
          ret = DFB_OK;
          goto out;
     }

     D_MAGIC_ASSERT( region->surface, CoreSurface );

     /*
      * Setup left lock
      */
     dfb_surface_buffer_lock_init( &left, accessor, CSAF_READ );

     left.task = this;

     ret = dfb_surface_pool_lock( left_allocation->pool, left_allocation, &left );
     if (ret)
          goto out;

     if (stereo) {
          /*
           * Setup right lock
           */
          dfb_surface_buffer_lock_init( &right, accessor, CSAF_READ );

          right.task = this;

          ret = dfb_surface_pool_lock( right_allocation->pool, right_allocation, &right );
          if (ret)
               goto out;
     }

     /* Unfreeze region? */
     if (D_FLAGS_IS_SET( region->state, CLRSF_FROZEN )) {
          D_FLAGS_CLEAR( region->state, CLRSF_FROZEN );

          if (!D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
               ret = dfb_layer_region_realize( region, false );
               if (ret) {
                    D_DERROR( ret, "Core/LayerRegion: realize_region() in DisplayTask::Run() failed!\n" );
                    goto out;
               }
          }

          if (ret == DFB_OK) {
               ret = funcs->SetRegion( layer, layer->driver_data, layer->layer_data,
                                       region->region_data, &region->config, CLRCF_ALL,
                                       surface, surface ? surface->palette : NULL,
                                       &left, stereo ? &right : NULL );
               if (ret) {
                    D_DERROR( ret, "Core/LayerRegion: SetRegion() in DisplayTask::Run() failed!\n" );
                    goto out;
               }
          }
     }
     else
          D_ASSUME( D_FLAGS_IS_SET( region->state, CLRSF_REALIZED ) );

     D_DEBUG_AT( DirectFB_Task_Display, "  -> setting task for index %d\n", index );

     /* Call SurfaceTask::CacheInvalidate() for cache invalidation */
     CacheInvalidate();

     if (dfb_config->layers_fps) {
          if (!layer->fps)
               layer->fps = new Util::FPS();

          if (layer->fps->Count( dfb_config->layers_fps ))
               D_INFO( "Core/Layer/%u: FPS %s\n", layer->shared->layer_id, layer->fps->Get().buffer() );
     }

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
                    D_DEBUG_AT( DirectFB_Task_Display, "  -> Flipping region using driver...\n" );

                    D_ASSUME( funcs->FlipRegion != NULL );

                    if (funcs->FlipRegion)
                         ret = funcs->FlipRegion( layer,
                                                  layer->driver_data,
                                                  layer->layer_data,
                                                  region->region_data,
                                                  surface, flip_flags,
                                                  left_update, &left,
                                                  stereo ? right_update : NULL, stereo ? &right : NULL );
                    break;
               }

               /* fall through */

          case DLBM_BACKSYSTEM:
          case DLBM_FRONTONLY:
               /* Tell the driver about the update if the region is realized. */
               if (funcs->UpdateRegion && D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                    const DFBRegion *_left_update  = left_update;
                    const DFBRegion *_right_update = right_update;

                    D_DEBUG_AT( DirectFB_Task_Display, "  -> Notifying driver about updated content...\n" );

                    if( !_left_update ) {
                         left_unrotated = DFB_REGION_INIT_FROM_RECTANGLE_VALS( 0, 0,
                                                                               region->config.width, region->config.height );
                         _left_update   = &left_unrotated;
                    }
                    dfb_region_from_rotated( &left_rotated, _left_update, &surface->config.size, surface->rotation );

                    if( !_right_update ) {
                         right_unrotated = DFB_REGION_INIT_FROM_RECTANGLE_VALS( 0, 0,
                                                                                region->config.width, region->config.height );
                         _right_update   = &right_unrotated;
                    }
                    dfb_region_from_rotated( &right_rotated, _right_update, &surface->config.size, surface->rotation );

                    ret = funcs->UpdateRegion( layer,
                                               layer->driver_data,
                                               layer->layer_data,
                                               region->region_data,
                                               surface,
                                               &left_rotated, &left,
                                               stereo ? &right_rotated : NULL, stereo ? &right : NULL );
               }
               break;

          default:
               D_BUG("unknown buffer mode");
               ret = DFB_BUG;
     }

out:
     dfb_layer_region_unlock( region );

     if (right.allocation) {
          /* Unlock region buffer since the lock is no longer needed. */
          dfb_surface_pool_unlock( right.allocation->pool, right.allocation, &right );
          dfb_surface_buffer_lock_deinit( &right );
     }

     if (left.allocation) {
          /* Unlock region buffer since the lock is no longer needed. */
          dfb_surface_pool_unlock( left.allocation->pool, left.allocation, &left );
          dfb_surface_buffer_lock_deinit( &left );
     }

     if (ret)
          Done( ret );

     return ret;
}

void
DisplayTask::Describe( Direct::String &string )
{
     SurfaceTask::Describe( string );

     string.PrintF( "  Display buffer index %d (%s)", index, stereo ? "stereo" : "mono" );
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
     CoreLayerContext    *context;
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


     D_ASSERT( region != NULL );
     D_ASSERT( region->context != NULL );

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

     context = region->context;
     surface = region->surface;
     layer   = dfb_layer_at( context->layer_id );

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
                    dfb_surface_lock( surface );

                    dfb_surface_flip_buffers( surface, false );

                    /* Use the driver's routine if the region is realized. */
                    if (D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
                         D_DEBUG_AT( DirectFB_Task_Display, "  -> Issuing display task...\n" );

                         DisplayTask::Generate( region, left_update, right_update, flags, pts, ret_task );
                    }

                    dfb_surface_unlock( surface );
                    break;
               }

               /* fall through */

          case DLBM_BACKSYSTEM:
               D_DEBUG_AT( DirectFB_Task_Display, "  -> Going to copy portion...\n" );

               dfb_layer_region_unlock( region );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) {
                    D_DEBUG_AT( DirectFB_Task_Display, "  -> Waiting for VSync...\n" );

                    dfb_layer_wait_vsync( layer );
               }

               D_DEBUG_AT( DirectFB_Task_Display, "  -> Copying content from back to front buffer...\n" );

               /* ...or copy updated contents from back to front buffer. */
               D_FLAGS_SET( eyes, DSSE_LEFT );

               if (region->config.options & DLOP_STEREO)
                    D_FLAGS_SET( eyes, DSSE_RIGHT );

               dfb_back_to_front_copy_stereo( surface, eyes, left_update, right_update, surface->rotation );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAIT) {
                    D_DEBUG_AT( DirectFB_Task_Display, "  -> Waiting for VSync...\n" );

                    dfb_layer_wait_vsync( layer );
               }

               dfb_layer_region_lock( region );

               /* fall through */

          case DLBM_FRONTONLY:
               if (D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
                    /* Tell the driver about the update if the region is realized. */
                    D_DEBUG_AT( DirectFB_Task_Display, "  -> Issuing display task...\n" );

                    dfb_surface_lock( surface );

                    DisplayTask::Generate( region, left_update, right_update, flags, pts, ret_task );

                    dfb_surface_unlock( surface );
               }
               break;

          default:
               D_BUG("unknown buffer mode");
               ret = DFB_BUG;
     }

     D_DEBUG_AT( DirectFB_Task_Display, "  -> done.\n" );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return ret;
}

}


}
