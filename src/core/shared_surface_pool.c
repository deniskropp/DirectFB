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

#include <misc/conf.h>

/**********************************************************************************************************************/

typedef struct {
     char         tmpfs_dir[FUSION_SHM_TMPFS_PATH_NAME_LEN + 20];

     u64          indices;
} SharedPoolData;

typedef struct {
     CoreDFB     *core;
     FusionWorld *world;
} SharedPoolLocalData;

typedef struct {
     u64          index;
     int          pitch;
     int          size;
} SharedAllocationData;

/**********************************************************************************************************************/

static int
sharedPoolDataSize( void )
{
     return sizeof(SharedPoolData);
}

static int
sharedPoolLocalDataSize( void )
{
     return sizeof(SharedPoolLocalData);
}

static int
sharedAllocationDataSize( void )
{
     return sizeof(SharedAllocationData);
}

static DFBResult
sharedInitPool( CoreDFB                    *core,
                CoreSurfacePool            *pool,
                void                       *pool_data,
                void                       *pool_local,
                void                       *system_data,
                CoreSurfacePoolDescription *ret_desc )
{
     SharedPoolData      *data  = pool_data;
     SharedPoolLocalData *local = pool_local;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_INTERNAL | CSTF_EXTERNAL;
     ret_desc->priority          = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "Shared Memory" );

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
sharedDestroyPool( CoreSurfacePool *pool,
                   void            *pool_data,
                   void            *pool_local )
{
     SharedPoolData *data = pool_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     if (rmdir( data->tmpfs_dir ) < 0)
          D_PERROR( "Core/Surface/SHM: Could not remove '%s'!\n", data->tmpfs_dir );

     return DFB_OK;
}

static DFBResult
sharedAllocateBuffer( CoreSurfacePool       *pool,
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

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     dfb_surface_calc_buffer_size( surface, 8, 0, &alloc->pitch, &alloc->size );

     alloc->index = ++(data->indices);

     snprintf( buf, sizeof(buf), "%s/surface_0x%08x_shared_allocation_%zu", data->tmpfs_dir, surface->object.id, alloc->index );

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

     close( fd );

     allocation->flags = CSALF_VOLATILE;
     allocation->size  = alloc->size;

     return DFB_OK;
}

static DFBResult
sharedDeallocateBuffer( CoreSurfacePool       *pool,
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

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     snprintf( buf, sizeof(buf), "%s/surface_0x%08x_shared_allocation_%zu", data->tmpfs_dir, surface->object.id, alloc->index );

     if (unlink( buf ) < 0) {
          D_PERROR( "Core/Surface/SHM: Could not remove '%s'!\n", buf );
          return DFB_IO;
     }

     return DFB_OK;
}

static DFBResult
sharedLock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     CoreSurface          *surface;
     CoreSurfaceBuffer    *buffer;
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;
     char                  buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 99];
     int                   fd;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     buffer = lock->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     snprintf( buf, sizeof(buf), "%s/surface_0x%08x_shared_allocation_%zu", data->tmpfs_dir, surface->object.id, alloc->index );

     fd = open( buf, O_RDWR );
     if (fd < 0) {
          D_PERROR( "Core/Surface/SHM: Could not open '%s'!\n", buf );
          return DFB_IO;
     }


     lock->addr = mmap( NULL, alloc->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );

     close( fd );

     if (lock->addr == MAP_FAILED) {
          D_PERROR( "Core/Surface/SHM: Could not mmap '%s'!\n", buf );
          return DFB_IO;
     }


     lock->pitch = alloc->pitch;

     return DFB_OK;
}

static DFBResult
sharedUnlock( CoreSurfacePool       *pool,
              void                  *pool_data,
              void                  *pool_local,
              CoreSurfaceAllocation *allocation,
              void                  *alloc_data,
              CoreSurfaceBufferLock *lock )
{
     SharedAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     munmap( lock->addr, alloc->size );

     return DFB_OK;
}

const SurfacePoolFuncs sharedSurfacePoolFuncs = {
     .PoolDataSize       = sharedPoolDataSize,
     .PoolLocalDataSize  = sharedPoolLocalDataSize,
     .AllocationDataSize = sharedAllocationDataSize,
     .InitPool           = sharedInitPool,
     .DestroyPool        = sharedDestroyPool,

     .AllocateBuffer     = sharedAllocateBuffer,
     .DeallocateBuffer   = sharedDeallocateBuffer,

     .Lock               = sharedLock,
     .Unlock             = sharedUnlock,
};

