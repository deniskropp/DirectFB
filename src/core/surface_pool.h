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

#ifndef __CORE__SURFACE_POOL_H__
#define __CORE__SURFACE_POOL_H__

#include <directfb.h>

#include <core/coretypes.h>

#include <core/surface.h>
#include <core/surface_buffer.h>


typedef enum {
     CSPCAPS_NONE   = 0x00000000,

     CSPCAPS_ALL    = 0x00000000
} CoreSurfacePoolCapabilities;

typedef enum {
     CSPP_DEFAULT,
     CSPP_PREFERED,
     CSPP_ULTIMATE
} CoreSurfacePoolPriority;

/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFB_SURFACE_POOL_ABI_VERSION           1

#define DFB_SURFACE_POOL_DESC_NAME_LENGTH     40


typedef struct {
     CoreSurfacePoolCapabilities   caps;
     CoreSurfaceAccessFlags        access;
     CoreSurfaceTypeFlags          types;
     CoreSurfacePoolPriority       priority;
     char                          name[DFB_SURFACE_POOL_DESC_NAME_LENGTH];
} CoreSurfacePoolDescription;


typedef struct {
     int       (*PoolDataSize)();
     int       (*PoolLocalDataSize)();
     int       (*AllocationDataSize)();

     /*
      * Pool init/destroy
      */
     DFBResult (*InitPool)   ( CoreDFB                    *core,
                               CoreSurfacePool            *pool,
                               void                       *pool_data,
                               void                       *pool_local,
                               void                       *system_data,
                               CoreSurfacePoolDescription *ret_desc );

     DFBResult (*JoinPool)   ( CoreDFB                    *core,
                               CoreSurfacePool            *pool,
                               void                       *pool_data,
                               void                       *pool_local,
                               void                       *system_data );

     DFBResult (*DestroyPool)( CoreSurfacePool            *pool,
                               void                       *pool_data,
                               void                       *pool_local );

     DFBResult (*LeavePool)  ( CoreSurfacePool            *pool,
                               void                       *pool_data,
                               void                       *pool_local );



     DFBResult (*TestConfig) ( CoreSurfacePool            *pool,
                               void                       *pool_data,
                               void                       *pool_local,
                               CoreSurfaceBuffer          *buffer,
                               const CoreSurfaceConfig    *config );
     /*
      * Buffer management
      */
     DFBResult (*AllocateBuffer)  ( CoreSurfacePool       *pool,
                                    void                  *pool_data,
                                    void                  *pool_local,
                                    CoreSurfaceBuffer     *buffer,
                                    CoreSurfaceAllocation *allocation,
                                    void                  *alloc_data );

     DFBResult (*DeallocateBuffer)( CoreSurfacePool       *pool,
                                    void                  *pool_data,
                                    void                  *pool_local,
                                    CoreSurfaceBuffer     *buffer,
                                    CoreSurfaceAllocation *allocation,
                                    void                  *alloc_data );

     /*
      * Locking
      */
     DFBResult (*Lock)  ( CoreSurfacePool       *pool,
                          void                  *pool_data,
                          void                  *pool_local,
                          CoreSurfaceAllocation *allocation,
                          void                  *alloc_data,
                          CoreSurfaceBufferLock *lock );

     DFBResult (*Unlock)( CoreSurfacePool       *pool,
                          void                  *pool_data,
                          void                  *pool_local,
                          CoreSurfaceAllocation *allocation,
                          void                  *alloc_data,
                          CoreSurfaceBufferLock *lock );

     /*
      * Read/write
      */
     DFBResult (*Read)  ( CoreSurfacePool       *pool,
                          void                  *pool_data,
                          void                  *pool_local,
                          CoreSurfaceAllocation *allocation,
                          void                  *alloc_data,
                          void                  *destination,
                          int                    pitch,
                          const DFBRectangle    *rect );

     DFBResult (*Write) ( CoreSurfacePool       *pool,
                          void                  *pool_data,
                          void                  *pool_local,
                          CoreSurfaceAllocation *allocation,
                          void                  *alloc_data,
                          const void            *source,
                          int                    pitch,
                          const DFBRectangle    *rect );
} SurfacePoolFuncs;


struct __DFB_CoreSurfacePool {
     int                         magic;

     FusionSkirmish              lock;

     CoreSurfacePoolID           pool_id;

     CoreSurfacePoolDescription  desc;

     int                         pool_data_size;
     int                         pool_local_data_size;
     int                         alloc_data_size;

     void                       *data;

     FusionVector                allocs;

     FusionSHMPoolShared        *shmpool;
};


typedef DFBEnumerationResult (*CoreSurfacePoolCallback)( CoreSurfacePool *pool,
                                                         void            *ctx );

typedef DFBEnumerationResult (*CoreSurfaceAllocCallback)( CoreSurfaceAllocation *allocation,
                                                          void                  *ctx );



DFBResult dfb_surface_pools_negotiate( CoreSurfaceBuffer       *buffer,
                                       CoreSurfaceAccessFlags   access,
                                       CoreSurfacePool        **ret_pool );

DFBResult dfb_surface_pools_enumerate( CoreSurfacePoolCallback  callback,
                                       void                    *ctx );



DFBResult dfb_surface_pool_initialize( CoreDFB                 *core,
                                       const SurfacePoolFuncs  *funcs,
                                       CoreSurfacePool        **ret_pool );

DFBResult dfb_surface_pool_join      ( CoreDFB                 *core,
                                       CoreSurfacePool         *pool,
                                       const SurfacePoolFuncs  *funcs );

DFBResult dfb_surface_pool_destroy   ( CoreSurfacePool         *pool );

DFBResult dfb_surface_pool_leave     ( CoreSurfacePool         *pool );



DFBResult dfb_surface_pool_allocate  ( CoreSurfacePool         *pool,
                                       CoreSurfaceBuffer       *buffer,
                                       CoreSurfaceAllocation  **ret_allocation );

DFBResult dfb_surface_pool_deallocate( CoreSurfacePool         *pool,
                                       CoreSurfaceAllocation   *allocation );

DFBResult dfb_surface_pool_lock      ( CoreSurfacePool         *pool,
                                       CoreSurfaceAllocation   *allocation,
                                       CoreSurfaceBufferLock   *lock );

DFBResult dfb_surface_pool_unlock    ( CoreSurfacePool         *pool,
                                       CoreSurfaceAllocation   *allocation,
                                       CoreSurfaceBufferLock   *lock );

DFBResult dfb_surface_pool_enumerate ( CoreSurfacePool         *pool,
                                       CoreSurfaceAllocCallback  callback,
                                       void                    *ctx );


#endif

