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

#ifndef __CORE__SURFACE_POOL_BRIDGE_H__
#define __CORE__SURFACE_POOL_BRIDGE_H__

#include <directfb.h>

#include <core/coretypes.h>

#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>


typedef enum {
     CSPBCAPS_NONE       = 0x00000000,

     CSPBCAPS_ALL        = 0x00000000
} CoreSurfacePoolBridgeCapabilities;

/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFB_SURFACE_POOL_BRIDGE_ABI_VERSION        1

#define DFB_SURFACE_POOL_BRIDGE_DESC_NAME_LENGTH  44


typedef struct {
     CoreSurfacePoolBridgeCapabilities  caps;
     char                               name[DFB_SURFACE_POOL_BRIDGE_DESC_NAME_LENGTH];
     CoreSurfacePoolPriority            priority;
} CoreSurfacePoolBridgeDescription;


typedef struct {
     int       (*PoolBridgeDataSize)( void );
     int       (*PoolBridgeLocalDataSize)( void );

     int       (*PoolTransferDataSize)( void );

     /*
      * Bridge init/destroy
      */
     DFBResult (*InitPoolBridge)   ( CoreDFB                          *core,
                                     CoreSurfacePoolBridge            *bridge,
                                     void                             *bridge_data,
                                     void                             *bridge_local,
                                     void                             *context,
                                     CoreSurfacePoolBridgeDescription *ret_desc );

     DFBResult (*JoinPoolBridge)   ( CoreDFB                          *core,
                                     CoreSurfacePoolBridge            *bridge,
                                     void                             *bridge_data,
                                     void                             *bridge_local,
                                     void                             *context );

     DFBResult (*DestroyPoolBridge)( CoreSurfacePoolBridge            *bridge,
                                     void                             *bridge_data,
                                     void                             *bridge_local );

     DFBResult (*LeavePoolBridge)  ( CoreSurfacePoolBridge            *bridge,
                                     void                             *bridge_data,
                                     void                             *bridge_local );


     /*
      * Probe
      */
     DFBResult (*CheckTransfer)    ( CoreSurfacePoolBridge            *bridge,
                                     void                             *bridge_data,
                                     void                             *bridge_local,
                                     CoreSurfaceBuffer                *buffer,
                                     CoreSurfaceAllocation            *from,
                                     CoreSurfaceAllocation            *to );

     /*
      * Transfer
      */
     DFBResult (*StartTransfer)    ( CoreSurfacePoolBridge            *bridge,
                                     void                             *bridge_data,
                                     void                             *bridge_local,
                                     CoreSurfacePoolTransfer          *transfer,
                                     void                             *transfer_data );

     DFBResult (*FinishTransfer)   ( CoreSurfacePoolBridge            *bridge,
                                     void                             *bridge_data,
                                     void                             *bridge_local,
                                     CoreSurfacePoolTransfer          *transfer,
                                     void                             *transfer_data );
} SurfacePoolBridgeFuncs;


struct __DFB_CoreSurfacePoolBridge {
     int                                magic;

     FusionSkirmish                     lock;

     CoreSurfacePoolBridgeID            bridge_id;

     CoreSurfacePoolBridgeDescription   desc;

     int                                bridge_data_size;
     int                                bridge_local_data_size;
     int                                transfer_data_size;

     void                              *data;

     FusionSHMPoolShared               *shmpool;

     DirectLink                        *transfers;
};

struct __DFB_CoreSurfacePoolTransfer {
     DirectLink                         link;

     int                                magic;

     CoreSurfacePoolBridge             *bridge;

     CoreSurfaceBuffer                 *buffer;
     CoreSurfaceAllocation             *from;
     CoreSurfaceAllocation             *to;

     DFBRectangle                      *rects;
     unsigned int                       num_rects;

     void                              *data;
};


typedef DFBEnumerationResult (*CoreSurfacePoolBridgeCallback)( CoreSurfacePoolBridge *bridge,
                                                               void                  *ctx );



DFBResult dfb_surface_pool_bridges_enumerate( CoreSurfacePoolBridgeCallback  callback,
                                              void                          *ctx );

DFBResult dfb_surface_pool_bridges_transfer ( CoreSurfaceBuffer             *buffer,
                                              CoreSurfaceAllocation         *from,
                                              CoreSurfaceAllocation         *to,
                                              const DFBRectangle            *rects,
                                              unsigned int                   num_rects );


DFBResult dfb_surface_pool_bridge_initialize( CoreDFB                       *core,
                                              const SurfacePoolBridgeFuncs  *funcs,
                                              void                          *context,
                                              CoreSurfacePoolBridge        **ret_bridge );

DFBResult dfb_surface_pool_bridge_join      ( CoreDFB                       *core,
                                              CoreSurfacePoolBridge         *pool,
                                              const SurfacePoolBridgeFuncs  *funcs,
                                              void                          *context );

DFBResult dfb_surface_pool_bridge_destroy   ( CoreSurfacePoolBridge         *bridge );

DFBResult dfb_surface_pool_bridge_leave     ( CoreSurfacePoolBridge         *bridge );


#endif

