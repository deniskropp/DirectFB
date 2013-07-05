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



//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <directfb.h>    // include here to prevent it being included indirectly causing nested extern "C"

#include "DisplayTask.h"
#include "Util.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <core/layers_internal.h>
#include <core/surface_allocation.h>
#include <core/surface_pool.h>
#include <core/system.h>

#include <misc/conf.h>
}

#include <direct/Lists.h>

#include <core/Debug.h>
#include <core/Task.h>


D_DEBUG_DOMAIN( DirectFB_Task_Display,      "DirectFB/Task/Display",      "DirectFB DisplayTask" );
D_DEBUG_DOMAIN( DirectFB_Task_Display_List, "DirectFB/Task/Display/List", "DirectFB DisplayTask List" );

/*********************************************************************************************************************/

namespace DirectFB {


extern "C" {

DFBResult
DisplayTask_Generate( CoreLayerRegion      *region,
                      const DFBRegion      *left_update,
                      const DFBRegion      *right_update,
                      DFBSurfaceFlipFlags   flags,
                      long long             pts,
                      DisplayTask         **ret_task )
{
     D_DEBUG_AT( DirectFB_Task, "%s( region %p, "DFB_RECT_FORMAT", "DFB_RECT_FORMAT", flags 0x%04x, ret_task %p )\n", __FUNCTION__,
                 region, DFB_RECTANGLE_VALS_FROM_REGION( left_update ), DFB_RECTANGLE_VALS_FROM_REGION( right_update ), flags, ret_task );

     return DisplayTask::Generate( region, left_update, right_update, flags, pts, ret_task );
}

long long
DisplayTask_GetPTS( DFB_DisplayTask *task )
{
     long long pts;

     D_DEBUG_AT( DirectFB_Task, "%s( %p )\n", __FUNCTION__, task );

     pts = task->GetPTS();

     D_DEBUG_AT( DirectFB_Task, "  -> %lld\n", pts );

     return pts;
}

/*********************************************************************************************************************/

}


const Direct::String DisplayTask::_Type( "Display" );

const Direct::String &
DisplayTask::TypeName() const
{
     return _Type;
}



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

     if (left_allocation)
          dfb_surface_allocation_ref( left_allocation );

     if (right_allocation)
          dfb_surface_allocation_ref( right_allocation );

     layer = dfb_layer_at( region->layer_id );
     index = dfb_surface_buffer_index( left_allocation->buffer );

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

     if (region->config.buffermode == DLBM_FRONTONLY || region->config.buffermode == DLBM_BACKSYSTEM)
          flags = (TaskFlags)(flags | TASK_FLAG_EMITNOTIFIES);
}

DisplayTask::~DisplayTask()
{
     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s()\n", __FUNCTION__ );

     if (left_allocation)
          dfb_surface_allocation_unref( left_allocation );

     if (right_allocation)
          dfb_surface_allocation_unref( right_allocation );

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

     CORE_SURFACE_ALLOCATION_ASSERT( left_allocation );

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

          CORE_SURFACE_ALLOCATION_ASSERT( right_allocation );

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
     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( %p ) <- prev %p\n", __FUNCTION__, this, layer->display_task );

     if (layer->display_task)
          layer->display_task->AddNotify( this, true );

     layer->display_task = this;

     return SurfaceTask::Setup();
}

void
DisplayTask::Flush()
{
     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( %p )\n", __FUNCTION__, this );

     D_ASSERT( region != NULL );
     D_ASSERT( region->display_tasks != NULL );

     AddRef();
     D_DEBUG_AT( DirectFB_Task_Display_List, "  -> adding to list %p\n", region->display_tasks );
     region->display_tasks->Append( this );

     if (pts > 0 && !(dfb_system_caps() & CSCAPS_DISPLAY_PTS)) {
          D_DEBUG_AT( DirectFB_Task_Display, "  -> system WITHOUT display task PTS support, setting emit time stamp to %lld us\n", pts );

          ts_emit = pts;
     }

     SurfaceTask::Flush();
}

void
DisplayTask::Finalise()
{
     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( %p ) <- prev %p\n", __FUNCTION__, this, layer->display_task );

     D_ASSERT( layer != NULL );

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
     CoreSurface             *surface;
     const DisplayLayerFuncs *funcs;
     CoreSurfaceBufferLock    left  = {0};
     CoreSurfaceBufferLock    right = {0};

     D_DEBUG_AT( DirectFB_Task_Display, "DisplayTask::%s( %p [%s], region %p )\n", __FUNCTION__,
                 this, *ToString<DirectFB::Task>(*this), region );

     funcs = layer->funcs;
     D_ASSERT( funcs != NULL );
     D_ASSERT( funcs->SetRegion != NULL );

     /* Preinitialize state client for possible blits from layer driver (avoids dead lock with region lock) */
     dfb_gfx_init_tls();

     dfb_layer_region_lock( region );

     surface = region->surface;

     dfb_surface_ref( surface );

     D_ASSERT( region->display_tasks != NULL );

     D_DEBUG_AT( DirectFB_Task_Display_List, "  -> removing from list %p\n", region->display_tasks );
     region->display_tasks->Remove( this );


     D_ASSUME( D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE ) );

     if (!D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
          ret = DFB_SUSPENDED;
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

                    if (!(dfb_system_caps() & CSCAPS_NOTIFY_DISPLAY)) {
                         D_DEBUG_AT( DirectFB_Task_Display, "  -> system WITHOUT notify_display support, calling it now\n" );

                         dfb_surface_notify_display2( surface, left.allocation->index, this );
                    }

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

                    if (!(dfb_system_caps() & CSCAPS_NOTIFY_DISPLAY)) {
                         D_DEBUG_AT( DirectFB_Task_Display, "  -> system WITHOUT notify_display support, calling it now\n" );

                         dfb_surface_notify_display2( surface, left.allocation->index, this );
                    }
               }
               break;

          default:
               D_BUG("unknown buffer mode");
               ret = DFB_BUG;
     }


out:
     if (ret != DFB_SUSPENDED) {
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
     }

     dfb_surface_unref( surface );

     Release();

     if (ret) {
          dfb_layer_region_unlock( region );

          Done( ret );

          return ret;
     }

     if (!(dfb_system_caps() & CSCAPS_DISPLAY_TASKS)) {
          D_DEBUG_AT( DirectFB_Task_Display, "  -> system WITHOUT display task support, calling Task_Done on previous task\n" );

          if (layer->prev_task)
               layer->prev_task->Done();

          layer->prev_task = this;
     }

     dfb_layer_region_unlock( region );

     return DFB_OK;
}

void
DisplayTask::Describe( Direct::String &string ) const
{
     SurfaceTask::Describe( string );

     string.PrintF( "  Display buffer index %d (%s)", index, stereo ? "stereo" : "mono" );
}


}
