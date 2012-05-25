/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

#include <direct/debug.h>
#include <direct/mem.h>

#include <core/core.h>
#include <core/surface_pool.h>
#include <core/system.h>
#include <misc/conf.h>


/**********************************************************************************************************************/

typedef struct {
} LocalPoolData;

typedef struct {
} LocalPoolLocalData;

typedef struct {
     int         magic;

     void       *addr;
     int         pitch;
     int         size;
} LocalAllocationData;

/**********************************************************************************************************************/

static int
localPoolDataSize( void )
{
     return sizeof(LocalPoolData);
}

static int
localPoolLocalDataSize( void )
{
     return sizeof(LocalPoolLocalData);
}

static int
localAllocationDataSize( void )
{
     return sizeof(LocalAllocationData);
}

static DFBResult
localInitPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data,
               CoreSurfacePoolDescription *ret_desc )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_INTERNAL;
     ret_desc->priority          = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "System Memory" );

     return DFB_OK;
}

static DFBResult
localJoinPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
localDestroyPool( CoreSurfacePool *pool,
                  void            *pool_data,
                  void            *pool_local )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
localLeavePool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
localAllocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     CoreSurface         *surface;
     LocalAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_ASSERT( alloc != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     /* Create aligned local system surface buffer if both base address and pitch are non-zero. */
     if (dfb_config->system_surface_align_base && dfb_config->system_surface_align_pitch) {
          /* Make sure base address and pitch are a positive power of two. */
          D_ASSERT( dfb_config->system_surface_align_base >= 4 );
          D_ASSERT( !(dfb_config->system_surface_align_base & (dfb_config->system_surface_align_base-1)) );
          D_ASSERT( dfb_config->system_surface_align_pitch >= 2 );
          D_ASSERT( !(dfb_config->system_surface_align_pitch & (dfb_config->system_surface_align_pitch-1)) );

          dfb_surface_calc_buffer_size( surface, dfb_config->system_surface_align_pitch, 0,
                                        &alloc->pitch, &alloc->size );

          /* Note: The posix_memalign function requires base alignment to actually be at least four. */
          int tempRet = posix_memalign( &alloc->addr, dfb_config->system_surface_align_base, alloc->size );
          if ( tempRet != 0 ) {
              D_ERROR( "Local surface pool: Error from posix_memalign:%d with base alignment value:%d. "
                       "%s()-%s:%d\n",
                       tempRet, dfb_config->system_surface_align_base,
                       __FUNCTION__, __FILE__, __LINE__ );
              return DFB_FAILURE;
          }
     }
     else {
          /* Create un-aligned local system surface buffer. */

          dfb_surface_calc_buffer_size( surface, 8, 0, &alloc->pitch, &alloc->size );

          alloc->addr = D_MALLOC( alloc->size );
          if (!alloc->addr)
               return D_OOM();
     }

     D_MAGIC_SET( alloc, LocalAllocationData );

     allocation->flags = CSALF_VOLATILE;
     allocation->size  = alloc->size;

     return DFB_OK;
}

static DFBResult
localDeallocateBuffer( CoreSurfacePool       *pool,
                       void                  *pool_data,
                       void                  *pool_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *allocation,
                       void                  *alloc_data )
{
     LocalAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( alloc, LocalAllocationData );

     D_FREE( alloc->addr );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
localLock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     LocalAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );
     D_MAGIC_ASSERT( alloc, LocalAllocationData );

     lock->addr  = alloc->addr;
     lock->pitch = alloc->pitch;

     return DFB_OK;
}

static DFBResult
localUnlock( CoreSurfacePool       *pool,
             void                  *pool_data,
             void                  *pool_local,
             CoreSurfaceAllocation *allocation,
             void                  *alloc_data,
             CoreSurfaceBufferLock *lock )
{
     LocalAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );
     D_MAGIC_ASSERT( alloc, LocalAllocationData );

     (void) alloc;

     return DFB_OK;
}

const SurfacePoolFuncs localSurfacePoolFuncs = {
     .PoolDataSize       = localPoolDataSize,
     .PoolLocalDataSize  = localPoolLocalDataSize,
     .AllocationDataSize = localAllocationDataSize,

     .InitPool           = localInitPool,
     .JoinPool           = localJoinPool,
     .DestroyPool        = localDestroyPool,
     .LeavePool          = localLeavePool,

     .AllocateBuffer     = localAllocateBuffer,
     .DeallocateBuffer   = localDeallocateBuffer,

     .Lock               = localLock,
     .Unlock             = localUnlock,
};

