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

#include <config.h>

#include <direct/debug.h>

#include <fusion/fusion.h>
#include <fusion/shm/shm_internal.h>

#include <core/core.h>
#include <core/surface_pool.h>
#include <core/system.h>

#include <misc/conf.h>

D_DEBUG_DOMAIN( Core_SharedSecure, "Core/SharedSecure", "Core Shared Secure Surface Pool" );

/**********************************************************************************************************************/

typedef struct {
     char         tmpfs_dir[FUSION_SHM_TMPFS_PATH_NAME_LEN + 20];
} SharedPoolData;

typedef struct {
     CoreDFB     *core;
     FusionWorld *world;
} SharedPoolLocalData;

typedef struct {
     int          pitch;
     int          size;

     DFBSurfaceID surface_id;

     void         *master_map;
} SharedAllocationData;

/**********************************************************************************************************************/

static int
sharedSecurePoolDataSize( void )
{
     return sizeof(SharedPoolData);
}

static int
sharedSecurePoolLocalDataSize( void )
{
     return sizeof(SharedPoolLocalData);
}

static int
sharedSecureAllocationDataSize( void )
{
     return sizeof(SharedAllocationData);
}

static DFBResult
sharedSecureInitPool( CoreDFB                    *core,
                      CoreSurfacePool            *pool,
                      void                       *pool_data,
                      void                       *pool_local,
                      void                       *system_data,
                      CoreSurfacePoolDescription *ret_desc )
{
     SharedPoolData      *data  = pool_data;
     SharedPoolLocalData *local = pool_local;

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_INTERNAL;
     ret_desc->priority          = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "Shared Secure Memory" );

     local->core  = core;
     local->world = dfb_core_world( core );

     snprintf( data->tmpfs_dir, sizeof(data->tmpfs_dir), "%s/dfb.%d",
               fusion_get_tmpfs( local->world ), fusion_world_index( local->world ) );

     if (mkdir( data->tmpfs_dir, 0750 ) < 0) {
          DIR           *dir;
          struct dirent *entry = NULL;
          struct dirent  tmp;

          if (errno != EEXIST) {
               D_PERROR( "Core/Surface/SHM: Could not create '%s'!\n", data->tmpfs_dir );
               return DFB_IO;
          }

          D_WARN( "%s exists, cleaning up", data->tmpfs_dir );

          dir = opendir( data->tmpfs_dir );
          if (!dir) {
               D_PERROR( "Core/Surface/SHM: Could not open '%s'!\n", data->tmpfs_dir );
               return DFB_IO;
          }

          while (readdir_r( dir, &tmp, &entry ) == 0 && entry) {
               char buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 99];

               if (!direct_strcmp( entry->d_name, "." ) ||
                   !direct_strcmp( entry->d_name, ".." ))
                    continue;

               snprintf( buf, sizeof(buf), "%s/%s", data->tmpfs_dir, entry->d_name );

               if (unlink( buf ) < 0) {
                    D_PERROR( "Core/Surface/SHM: Could not remove '%s'!\n", buf );
                    closedir( dir );
                    return DFB_IO;
               }
          }

          closedir( dir );
     }

     return DFB_OK;
}

static DFBResult
sharedSecureDestroyPool( CoreSurfacePool *pool,
                         void            *pool_data,
                         void            *pool_local )
{
     SharedPoolData *data = pool_data;

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     if (rmdir( data->tmpfs_dir ) < 0)
          D_PERROR( "Core/Surface/SHM: Could not remove '%s'!\n", data->tmpfs_dir );

     return DFB_OK;
}

static DFBResult
sharedSecureAllocateBuffer( CoreSurfacePool       *pool,
                            void                  *pool_data,
                            void                  *pool_local,
                            CoreSurfaceBuffer     *buffer,
                            CoreSurfaceAllocation *allocation,
                            void                  *alloc_data )
{
     CoreSurface          *surface;
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;
     char                  buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 99];
     int                   fd;

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     alloc->surface_id = surface->object.id;

     dfb_surface_calc_buffer_size( surface, 8, 0, &alloc->pitch, &alloc->size );

     snprintf( buf, sizeof(buf), "%s/surface_0x%08x_shared_allocation_%p", data->tmpfs_dir, alloc->surface_id, alloc );

     fd = open( buf, O_RDWR | O_CREAT | O_EXCL, 0660 );
     if (fd < 0) {
          D_PERROR( "Core/Surface/SHM: Could not create '%s'!\n", buf );
          return DFB_IO;
     }

     fchmod( fd, 0660 );

     if (ftruncate( fd, alloc->size ) < 0) {
          D_PERROR( "Core/Surface/SHM: Setting file size for '%s' to %d failed!\n", buf, alloc->size );
          unlink( buf );
          return DFB_IO;
     }

     alloc->master_map = mmap( NULL, alloc->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );

     close( fd );

     if (alloc->master_map == MAP_FAILED) {
          D_PERROR( "Core/Surface/SHM: Could not mmap '%s'!\n", buf );

          if (unlink( buf ) < 0)
               D_PERROR( "Core/Surface/SHM: Could not remove '%s'!\n", buf );

          return DFB_IO;
     }

     allocation->flags = CSALF_VOLATILE;
     allocation->size  = alloc->size;

     return DFB_OK;
}

static DFBResult
sharedSecureDeallocateBuffer( CoreSurfacePool       *pool,
                              void                  *pool_data,
                              void                  *pool_local,
                              CoreSurfaceBuffer     *buffer,
                              CoreSurfaceAllocation *allocation,
                              void                  *alloc_data )
{
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;
     char                  buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 99];

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
//     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     snprintf( buf, sizeof(buf), "%s/surface_0x%08x_shared_allocation_%p", data->tmpfs_dir, alloc->surface_id, alloc );

     munmap( alloc->master_map, alloc->size );

     if (unlink( buf ) < 0) {
          D_PERROR( "Core/Surface/SHM: Could not remove '%s'!\n", buf );
          return DFB_IO;
     }

     return DFB_OK;
}

static DFBResult
sharedSecureLock( CoreSurfacePool       *pool,
                  void                  *pool_data,
                  void                  *pool_local,
                  CoreSurfaceAllocation *allocation,
                  void                  *alloc_data,
                  CoreSurfaceBufferLock *lock )
{
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;
     char                  buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 99];
     int                   fd;

     D_DEBUG_AT( Core_SharedSecure, "%s() <- size %d\n", __FUNCTION__, alloc->size );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     if (dfb_core_is_master( core_dfb )) {
          lock->addr = alloc->master_map;
     }
     else {
          snprintf( buf, sizeof(buf), "%s/surface_0x%08x_shared_allocation_%p", data->tmpfs_dir, alloc->surface_id, alloc );

          fd = open( buf, O_RDWR );
          if (fd < 0) {
               D_PERROR( "Core/Surface/SHM: Could not open '%s'!\n", buf );
               return DFB_IO;
          }


          lock->addr = lock->handle = mmap( NULL, alloc->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );

          D_DEBUG_AT( Core_SharedSecure, "  -> mapped to %p\n", lock->addr );

          close( fd );

          if (lock->addr == MAP_FAILED) {
               D_PERROR( "Core/Surface/SHM: Could not mmap '%s'!\n", buf );
               return DFB_IO;
          }
     }


     lock->pitch = alloc->pitch;

     return DFB_OK;
}

static DFBResult
sharedSecureUnlock( CoreSurfacePool       *pool,
                    void                  *pool_data,
                    void                  *pool_local,
                    CoreSurfaceAllocation *allocation,
                    void                  *alloc_data,
                    CoreSurfaceBufferLock *lock )
{
     SharedAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     if (!dfb_core_is_master( core_dfb ))
          munmap( lock->handle, alloc->size );

     return DFB_OK;
}

const SurfacePoolFuncs sharedSecureSurfacePoolFuncs = {
     .PoolDataSize       = sharedSecurePoolDataSize,
     .PoolLocalDataSize  = sharedSecurePoolLocalDataSize,
     .AllocationDataSize = sharedSecureAllocationDataSize,
     .InitPool           = sharedSecureInitPool,
     .DestroyPool        = sharedSecureDestroyPool,

     .AllocateBuffer     = sharedSecureAllocateBuffer,
     .DeallocateBuffer   = sharedSecureDeallocateBuffer,

     .Lock               = sharedSecureLock,
     .Unlock             = sharedSecureUnlock,
};

