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

#include <config.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <fusion/shm/pool.h>

#include <fusion/shm/shm_internal.h>


DirectResult
fusion_shm_pool_create( FusionWorld          *world,
                        const char           *name,
                        unsigned int          max_size,
                        bool                  debug,
                        FusionSHMPoolShared **ret_pool )
{
     FusionSHMPoolShared *pool;

#if !DIRECT_BUILD_DEBUGS
     debug = false;
#endif

     pool = D_CALLOC( 1, sizeof(FusionSHMPoolShared) );
     if (!pool)
          return D_OOM();

     pool->debug = debug;

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

     pool->index++;

     return DFB_OK;
}

DirectResult
fusion_shm_pool_detach( FusionSHM           *shm,
                        FusionSHMPoolShared *pool )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     D_ASSERT( pool->index > 0 );

     pool->index--;

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

