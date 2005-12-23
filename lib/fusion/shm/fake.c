/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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
#include <direct/messages.h>

#include <fusion/shm/pool.h>


struct __Fusion_FusionSHMPoolShared {
     int  magic;
     int  attached;
};


DirectResult
fusion_shm_pool_create( FusionWorld          *world,
                        const char           *name,
                        unsigned int          max_size,
                        FusionSHMPoolShared **ret_pool )
{
     FusionSHMPoolShared *pool;

     pool = D_CALLOC( 1, sizeof(FusionSHMPoolShared) );
     if (!pool)
          return D_OOM();

     D_MAGIC_SET( pool, FusionSHMPoolShared );

     *ret_pool = pool;

     return DFB_OK;
}

DirectResult
fusion_shm_pool_destroy( FusionWorld         *world,
                         FusionSHMPoolShared *pool )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     D_MAGIC_CLEAR( pool );

     D_FREE( pool );

     return DFB_OK;
}

DirectResult
fusion_shm_pool_attach( FusionSHM           *shm,
                        FusionSHMPoolShared *pool )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     pool->attached++;

     return DFB_OK;
}

DirectResult
fusion_shm_pool_detach( FusionSHM           *shm,
                        FusionSHMPoolShared *pool )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     D_ASSERT( pool->attached > 0 );

     pool->attached--;

     return DFB_OK;
}

DirectResult
fusion_shm_pool_allocate( FusionSHMPoolShared  *pool,
                          int                   size,
                          bool                  clear,
                          bool                  lock,
                          void                **ret_data )
{
     void *data;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     data = clear ? D_CALLOC( 1, size ) : D_MALLOC( size );
     if (!data)
          return DFB_NOSHAREDMEMORY;

     *ret_data = data;

     return DFB_OK;
}

DirectResult
fusion_shm_pool_reallocate( FusionSHMPoolShared  *pool,
                            void                 *data,
                            int                   size,
                            bool                  lock,
                            void                **ret_data )
{
     void *new_data;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     new_data = D_REALLOC( data, size );
     if (!new_data)
          return DFB_NOSHAREDMEMORY;

     *ret_data = new_data;

     return DFB_OK;
}

DirectResult
fusion_shm_pool_deallocate( FusionSHMPoolShared *pool,
                            void                *data,
                            bool                 lock )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     D_FREE( data );

     return DFB_OK;
}

