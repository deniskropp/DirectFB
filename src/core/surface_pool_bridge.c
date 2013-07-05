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

#include <directfb.h>
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <fusion/conf.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>

#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <core/surface_pool_bridge.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>


D_DEBUG_DOMAIN( Core_SurfPoolBridge, "Core/SurfPoolBridge", "DirectFB Core Surface Pool Bridge" );

/**********************************************************************************************************************/

static const SurfacePoolBridgeFuncs *bridge_funcs[MAX_SURFACE_POOL_BRIDGES];
static void                         *bridge_locals[MAX_SURFACE_POOL_BRIDGES];
static int                           bridge_count;
static CoreSurfacePoolBridge        *bridge_array[MAX_SURFACE_POOL_BRIDGES];
static unsigned int                  bridge_order[MAX_SURFACE_POOLS];

/**********************************************************************************************************************/

static inline const SurfacePoolBridgeFuncs *
get_funcs( const CoreSurfacePoolBridge *bridge )
{
     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

     D_ASSERT( bridge->bridge_id >= 0 );
     D_ASSERT( bridge->bridge_id < MAX_SURFACE_POOL_BRIDGES );
     D_ASSERT( bridge_funcs[bridge->bridge_id] != NULL );

     /* Return function table of the bridge. */
     return bridge_funcs[bridge->bridge_id];
}

static inline void *
get_local( const CoreSurfacePoolBridge *bridge )
{
     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

     D_ASSERT( bridge->bridge_id >= 0 );
     D_ASSERT( bridge->bridge_id < MAX_SURFACE_POOL_BRIDGES );

     /* Return local data of the bridge. */
     return bridge_locals[bridge->bridge_id];
}

/**********************************************************************************************************************/

static DFBResult init_bridge( CoreDFB                      *core,
                              CoreSurfacePoolBridge        *bridge,
                              const SurfacePoolBridgeFuncs *funcs,
                              void                         *context );

/**********************************************************************************************************************/

static void insert_bridge_local( CoreSurfacePoolBridge   *bridge );
static void remove_bridge_local( CoreSurfacePoolBridgeID  bridge_id );

/**********************************************************************************************************************/

DFBResult
dfb_surface_pool_bridge_initialize( CoreDFB                       *core,
                                    const SurfacePoolBridgeFuncs  *funcs,
                                    void                          *context,
                                    CoreSurfacePoolBridge        **ret_bridge )
{
     DFBResult              ret;
     CoreSurfacePoolBridge *bridge;
     FusionSHMPoolShared   *shmpool;

     D_DEBUG_AT( Core_SurfPoolBridge, "%s( %p, %p )\n", __FUNCTION__, funcs, context );

     D_ASSERT( core != NULL );
     D_ASSERT( funcs != NULL );
     D_ASSERT( ret_bridge != NULL );

     /* Check against bridge limit. */
     if (bridge_count == MAX_SURFACE_POOL_BRIDGES) {
          D_ERROR( "Core/SurfacePoolBridge: Maximum number of bridges (%d) reached!\n", MAX_SURFACE_POOL_BRIDGES );
          return DFB_LIMITEXCEEDED;
     }

     D_ASSERT( bridge_funcs[bridge_count] == NULL );

     shmpool = dfb_core_shmpool( core );

     /* Allocate bridge structure. */
     bridge = SHCALLOC( shmpool, 1, sizeof(CoreSurfacePoolBridge) );
     if (!bridge)
          return D_OOSHM();

     /* Assign a bridge ID. */
     bridge->bridge_id = bridge_count++;

     /* Remember shared memory pool. */
     bridge->shmpool = shmpool;

     /* Set function table of the bridge. */
     bridge_funcs[bridge->bridge_id] = funcs;

     /* Add to global bridge list. */
     bridge_array[bridge->bridge_id] = bridge;

     D_MAGIC_SET( bridge, CoreSurfacePoolBridge );

     ret = init_bridge( core, bridge, funcs, context );
     if (ret) {
          bridge_funcs[bridge->bridge_id] = NULL;
          bridge_array[bridge->bridge_id] = NULL;
          bridge_count--;
          D_MAGIC_CLEAR( bridge );
          SHFREE( shmpool, bridge );
          return ret;
     }

     /* Insert new bridge into priority order */
     insert_bridge_local( bridge );

     /* Return the new bridge. */
     *ret_bridge = bridge;

     return DFB_OK;
}

DFBResult
dfb_surface_pool_bridge_join( CoreDFB                      *core,
                              CoreSurfacePoolBridge        *bridge,
                              const SurfacePoolBridgeFuncs *funcs,
                              void                         *context )
{
     DFBResult ret;

     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

     D_DEBUG_AT( Core_SurfPoolBridge, "%s( %p [%d], %p, %p )\n", __FUNCTION__, bridge, bridge->bridge_id, funcs, context );

     D_ASSERT( core != NULL );
     D_ASSERT( funcs != NULL );

     D_ASSERT( bridge->bridge_id < MAX_SURFACE_POOL_BRIDGES );
     D_ASSERT( bridge->bridge_id == bridge_count );
     D_ASSERT( bridge_funcs[bridge->bridge_id] == NULL );

     /* Enforce same order as initialization to be used during join. */
     if (bridge->bridge_id != bridge_count) {
          D_ERROR( "Core/SurfacePoolBridge: Wrong order of joining bridges, got %d, should be %d!\n",
                   bridge->bridge_id, bridge_count );
          return DFB_BUG;
     }

     /* Allocate local bridge data. */
     if (bridge->bridge_local_data_size &&
         !(bridge_locals[bridge->bridge_id] = D_CALLOC( 1, bridge->bridge_local_data_size )))
         return D_OOM();

     /* Set function table of the bridge. */
     bridge_funcs[bridge->bridge_id] = funcs;

     /* Add to global bridge list. */
     bridge_array[bridge->bridge_id] = bridge;

     /* Adjust bridge count. */
     if (bridge_count < bridge->bridge_id + 1)
          bridge_count = bridge->bridge_id + 1;

     funcs = get_funcs( bridge );

     if (funcs->JoinPoolBridge) {
          ret = funcs->JoinPoolBridge( core, bridge, bridge->data, get_local(bridge), context );
          if (ret) {
               D_DERROR( ret, "Core/SurfacePoolBridge: Joining '%s' failed!\n", bridge->desc.name );

               if (bridge_locals[bridge->bridge_id]) {
                    D_FREE( bridge_locals[bridge->bridge_id] );
                    bridge_locals[bridge->bridge_id] = NULL;
               }

               bridge_array[bridge->bridge_id] = NULL;
               bridge_funcs[bridge->bridge_id] = NULL;

               bridge_count--;

               return ret;
          }
     }

     /* Insert new bridge into priority order */
     insert_bridge_local( bridge );

     return DFB_OK;
}

DFBResult
dfb_surface_pool_bridge_destroy( CoreSurfacePoolBridge *bridge )
{
     CoreSurfacePoolBridgeID       bridge_id;
     const SurfacePoolBridgeFuncs *funcs;

     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

     bridge_id = bridge->bridge_id;

     D_DEBUG_AT( Core_SurfPoolBridge, "%s( %p, '%s' [%d] )\n", __FUNCTION__, bridge, bridge->desc.name, bridge_id );

     D_ASSERT( bridge->bridge_id >= 0 );
     D_ASSERT( bridge_id < MAX_SURFACE_POOL_BRIDGES );
     D_ASSERT( bridge_array[bridge_id] == bridge );

     funcs = get_funcs( bridge );

     if (funcs->DestroyPoolBridge)
          funcs->DestroyPoolBridge( bridge, bridge->data, get_local(bridge) );

     /* Free shared bridge data. */
     if (bridge->data)
          SHFREE( bridge->shmpool, bridge->data );

     /* Free local pool data and remove from lists */
     remove_bridge_local( bridge_id );

     fusion_skirmish_destroy( &bridge->lock );

     D_MAGIC_CLEAR( bridge );

     SHFREE( bridge->shmpool, bridge );

     return DFB_OK;
}

DFBResult
dfb_surface_pool_bridge_leave( CoreSurfacePoolBridge *bridge )
{
     CoreSurfacePoolBridgeID       bridge_id;
     const SurfacePoolBridgeFuncs *funcs;

     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

     bridge_id = bridge->bridge_id;

     D_DEBUG_AT( Core_SurfPoolBridge, "%s( %p, '%s' [%d] )\n", __FUNCTION__, bridge, bridge->desc.name, bridge_id );

     D_ASSERT( bridge->bridge_id >= 0 );
     D_ASSERT( bridge_id < MAX_SURFACE_POOL_BRIDGES );
     D_ASSERT( bridge_array[bridge_id] == bridge );

     funcs = get_funcs( bridge );

     if (funcs->LeavePoolBridge)
          funcs->LeavePoolBridge( bridge, bridge->data, get_local(bridge) );

     /* Free local pool data and remove from lists */
     remove_bridge_local( bridge_id );

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_pool_bridges_enumerate( CoreSurfacePoolBridgeCallback  callback,
                                    void                          *ctx )
{
     int i;

     D_ASSERT( callback != NULL );

     D_DEBUG_AT( Core_SurfPoolBridge, "%s( %p, %p )\n", __FUNCTION__, callback, ctx );

     for (i=0; i<bridge_count; i++) {
          CoreSurfacePoolBridge *bridge = bridge_array[i];

          D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

          if (callback( bridge, ctx ) == DFENUM_CANCEL)
               break;
     }

     return DFB_OK;
}

static DFBResult
allocate_transfer( CoreSurfacePoolBridge    *bridge,
                   CoreSurfaceBuffer        *buffer,
                   CoreSurfaceAllocation    *from,
                   CoreSurfaceAllocation    *to,
                   const DFBRectangle       *rects,
                   unsigned int              num_rects,
                   CoreSurfacePoolTransfer **ret_transfer )
{
     CoreSurfacePoolTransfer *transfer;
     unsigned int             alloc_size;

     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     CORE_SURFACE_ALLOCATION_ASSERT( from );
     CORE_SURFACE_ALLOCATION_ASSERT( to );
     D_ASSERT( rects != NULL );
     D_ASSERT( num_rects > 0 );
     D_ASSERT( ret_transfer != NULL );

     alloc_size = sizeof(CoreSurfacePoolTransfer) + num_rects * sizeof(DFBRectangle) + bridge->transfer_data_size;

     transfer = SHCALLOC( bridge->shmpool, 1, alloc_size );
     if (!transfer)
          return D_OOSHM();
          
     transfer->bridge = bridge;
     transfer->buffer = buffer;
     transfer->from   = from;
     transfer->to     = to;

     transfer->rects  = (DFBRectangle*)(transfer + 1);

     if (bridge->transfer_data_size)
          transfer->data = transfer->rects + num_rects;

     transfer->num_rects = num_rects;

     direct_memcpy( transfer->rects, rects, num_rects * sizeof(DFBRectangle) );

     D_MAGIC_SET( transfer, CoreSurfacePoolTransfer );

     *ret_transfer = transfer;

     return DFB_OK;
}

static void
deallocate_transfer( CoreSurfacePoolTransfer *transfer )
{
     CoreSurfacePoolBridge *bridge;

     D_MAGIC_ASSERT( transfer, CoreSurfacePoolTransfer );

     bridge = transfer->bridge;
     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

     D_MAGIC_CLEAR( transfer );

     SHFREE( bridge->shmpool, transfer );
}

DFBResult
dfb_surface_pool_bridges_transfer( CoreSurfaceBuffer     *buffer,
                                   CoreSurfaceAllocation *from,
                                   CoreSurfaceAllocation *to,
                                   const DFBRectangle    *rects,
                                   unsigned int           num_rects )
{
     DFBResult                     ret;
     int                           i;
     DFBRectangle                  rect;
     CoreSurfacePoolBridge        *bridge = NULL;
     const SurfacePoolBridgeFuncs *funcs;
     CoreSurfacePoolTransfer      *transfer;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     CORE_SURFACE_ALLOCATION_ASSERT( from );
     CORE_SURFACE_ALLOCATION_ASSERT( to );
     D_ASSERT( rects != NULL || num_rects == 0 );
     D_ASSERT( num_rects > 0 || rects == NULL );

     D_DEBUG_AT( Core_SurfPoolBridge, "%s( %p [%dx%d %s], %p -> %p, %d rects )\n", __FUNCTION__,
                 buffer, buffer->config.size.w, buffer->config.size.h,
                 dfb_pixelformat_name( buffer->format ), from, to, num_rects );

     if (!rects) {
          rect.x = rect.y = 0;
          rect.w = buffer->config.size.w;
          rect.h = buffer->config.size.h;

          rects = &rect;
          num_rects = 1;
     }

     for (i=0; i<bridge_count; i++) {
          D_ASSERT( bridge_order[i] >= 0 );
          D_ASSERT( bridge_order[i] < bridge_count );

          bridge = bridge_array[bridge_order[i]];
          D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

          funcs = get_funcs( bridge );
          D_ASSERT( funcs->CheckTransfer != NULL );

          ret = funcs->CheckTransfer( bridge, bridge->data, get_local(bridge), buffer, from, to );
          if (ret)
               bridge = NULL;
          else
               break;
     }

     if (!bridge)
          return DFB_UNSUPPORTED;
          
     D_DEBUG_AT( Core_SurfPoolBridge, "  -> using '%s'\n", bridge->desc.name );

     ret = allocate_transfer( bridge, buffer, from, to, rects, num_rects, &transfer );
     if (ret)
          return ret;

     D_ASSERT( funcs->StartTransfer != NULL );
          
     D_DEBUG_AT( Core_SurfPoolBridge, "  -> start...\n" );

     ret = funcs->StartTransfer( bridge, bridge->data, get_local(bridge), transfer, transfer->data );
     if (ret)
          D_DERROR( ret, "Core/SurfacePoolBridge: Starting transfer via '%s' failed!\n", bridge->desc.name );
     else if (funcs->FinishTransfer) {
          D_DEBUG_AT( Core_SurfPoolBridge, "  -> finish...\n" );

          ret = funcs->FinishTransfer( bridge, bridge->data, get_local(bridge), transfer, transfer->data );
          if (ret)
               D_DERROR( ret, "Core/SurfacePoolBridge: Finishing transfer via '%s' failed!\n", bridge->desc.name );
     }

     D_DEBUG_AT( Core_SurfPoolBridge, "  => %s\n", DirectResultString(ret) );

     deallocate_transfer( transfer );

     return ret;
}

/**********************************************************************************************************************/

static DFBResult
init_bridge( CoreDFB                      *core,
             CoreSurfacePoolBridge        *bridge,
             const SurfacePoolBridgeFuncs *funcs,
             void                         *context )
{
     DFBResult ret;

     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );
     D_ASSERT( funcs != NULL );
     D_ASSERT( funcs->InitPoolBridge != NULL );

     D_DEBUG_AT( Core_SurfPoolBridge, "%s( %p, %p )\n", __FUNCTION__, bridge, funcs );

     if (funcs->PoolBridgeDataSize)
          bridge->bridge_data_size = funcs->PoolBridgeDataSize();

     if (funcs->PoolBridgeLocalDataSize)
          bridge->bridge_local_data_size = funcs->PoolBridgeLocalDataSize();

     if (funcs->PoolTransferDataSize)
          bridge->transfer_data_size = funcs->PoolTransferDataSize();

     /* Allocate shared bridge data. */
     if (bridge->bridge_data_size) {
          bridge->data = SHCALLOC( bridge->shmpool, 1, bridge->bridge_data_size );
          if (!bridge->data)
               return D_OOSHM();
     }

     /* Allocate local bridge data. */
     if (bridge->bridge_local_data_size &&
         !(bridge_locals[bridge->bridge_id] = D_CALLOC( 1, bridge->bridge_local_data_size )))
     {
          SHFREE( bridge->shmpool, bridge->data );
          return D_OOM();
     }

     ret = funcs->InitPoolBridge( core, bridge, bridge->data, get_local(bridge), context, &bridge->desc );
     if (ret) {
          D_DERROR( ret, "Core/SurfacePoolBridge: Initializing '%s' failed!\n", bridge->desc.name );

          if (bridge_locals[bridge->bridge_id]) {
               D_FREE( bridge_locals[bridge->bridge_id] );
               bridge_locals[bridge->bridge_id] = NULL;
          }
          if (bridge->data) {
               SHFREE( bridge->shmpool, bridge->data );
               bridge->data = NULL;
          }
          return ret;
     }

     fusion_skirmish_init2( &bridge->lock, bridge->desc.name, dfb_core_world(core), fusion_config->secure_fusion );

     return DFB_OK;
}

static void
insert_bridge_local( CoreSurfacePoolBridge *bridge )
{
     int i, n;

     for (i=0; i<bridge_count-1; i++) {
          D_ASSERT( bridge_order[i] >= 0 );
          D_ASSERT( bridge_order[i] < bridge_count-1 );

          D_MAGIC_ASSERT( bridge_array[bridge_order[i]], CoreSurfacePoolBridge );

          if (bridge_array[bridge_order[i]]->desc.priority < bridge->desc.priority)
               break;
     }

     for (n=bridge_count-1; n>i; n--) {
          D_ASSERT( bridge_order[n-1] >= 0 );
          D_ASSERT( bridge_order[n-1] < bridge_count-1 );

          D_MAGIC_ASSERT( bridge_array[bridge_order[n-1]], CoreSurfacePoolBridge );

          bridge_order[n] = bridge_order[n-1];
     }

     bridge_order[n] = bridge_count - 1;

#if D_DEBUG_ENABLED
     for (i=0; i<bridge_count; i++) {
          D_DEBUG_AT( Core_SurfPoolBridge, "  %c> [%d] %p - '%s' [%d] (%d), %p\n",
                      (i == n) ? '=' : '-', i, bridge_array[bridge_order[i]], bridge_array[bridge_order[i]]->desc.name,
                      bridge_array[bridge_order[i]]->bridge_id, bridge_array[bridge_order[i]]->desc.priority,
                      bridge_funcs[bridge_order[i]] );
          D_ASSERT( bridge_order[i] == bridge_array[bridge_order[i]]->bridge_id );
     }
#endif
}

static void
remove_bridge_local( CoreSurfacePoolBridgeID bridge_id )
{
     int i;

     /* Free local bridge data. */
     if (bridge_locals[bridge_id]) {
          D_FREE( bridge_locals[bridge_id] );
          bridge_locals[bridge_id] = NULL;
     }

     /* Erase entries of the bridge. */
     bridge_array[bridge_id] = NULL;
     bridge_funcs[bridge_id] = NULL;

     while (bridge_count > 0 && !bridge_array[bridge_count-1]) {
          bridge_count--;

          for (i=0; i<bridge_count; i++) {
               if (bridge_order[i] == bridge_count) {
                    direct_memmove( &bridge_order[i], &bridge_order[i+1], sizeof(bridge_order[0]) * (bridge_count - i) );
                    break;
               }
          }
     }
}

