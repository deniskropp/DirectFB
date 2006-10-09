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

#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/signals.h>
#include <direct/messages.h>

#include <fusion/conf.h>
#include <fusion/fusion_internal.h>

#include <fusion/shm/pool.h>
#include <fusion/shm/shm.h>
#include <fusion/shm/shm_internal.h>


/**********************************************************************************************************************/

static int
find_tmpfs( char *name, int len )
{
     int    largest = 0;
     char   buffer[1024];
     FILE  *mounts_handle;

     mounts_handle = fopen( "/proc/mounts", "r" );
     if (!mounts_handle)
          return 0;

     while (fgets( buffer, sizeof(buffer), mounts_handle )) {
          char *mount_point;
          char *mount_fs;
          char *pointer = buffer;

          strsep( &pointer, " " );

          mount_point = strsep( &pointer, " " );
          mount_fs = strsep( &pointer, " " );

          if (mount_fs && mount_point && (!strcmp( mount_fs, "tmpfs" ) ||
                                          !strcmp( mount_fs, "shmfs" )))
          {
               struct statfs stat;
               int           bytes;

               if (statfs( mount_point, &stat )) {
                    D_PERROR( "Fusion/SHM: statfs on '%s' failed!\n", mount_point );
                    continue;
               }

               bytes = stat.f_blocks * stat.f_bsize;

               if (bytes > largest || (bytes == largest && !strcmp(mount_point,"/dev/shm"))) {
                    largest = bytes;

                    strncpy( name, mount_point, len );
               }
          }
     }

     fclose( mounts_handle );

     return largest;
}

/**********************************************************************************************************************/

static DirectSignalHandlerResult
segfault_handler( int   num,
                  void *addr,
                  void *ctx )
{
     FusionWorld *world = ctx;

     D_ASSERT( num == SIGSEGV );
     D_MAGIC_ASSERT( world, FusionWorld );

     return _fusion_shmalloc_cure( world, addr ) ? DSHR_RESUME : DSHR_OK;
}

/**********************************************************************************************************************/

DirectResult
fusion_shm_init( FusionWorld *world )
{
     int              i;
     int              num;
     DirectResult     ret;
     FusionSHM       *shm;
     FusionSHMShared *shared;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     shm    = &world->shm;
     shared = &world->shared->shm;

     /* Initialize local data. */
     memset( shm, 0, sizeof(FusionSHM) );

     shm->world  = world;
     shm->shared = shared;

     /* Initialize shared data. */
     if (fusion_master( world )) {
          memset( shared, 0, sizeof(FusionSHMShared) );

          if (fusion_config->tmpfs) {
               snprintf( shared->tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN, fusion_config->tmpfs );
          }
          else if (!find_tmpfs( shared->tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN )) {
               D_ERROR( "Fusion/SHM: Could not find tmpfs mount point!\n" );
               return DFB_FILENOTFOUND;
          }

          shared->world = world->shared;

          /* Initialize shared lock. */
          ret = fusion_skirmish_init( &shared->lock, "Fusion SHM", world );
          if (ret) {
               D_DERROR( ret, "Fusion/SHM: Failed to create skirmish!\n" );
               return ret;
          }

          /* Initialize static pool array. */
          for (i=0; i<FUSION_SHM_MAX_POOLS; i++)
               shared->pools[i].index = i;

          D_MAGIC_SET( shm, FusionSHM );
          D_MAGIC_SET( shared, FusionSHMShared );
     }
     else {
          D_MAGIC_ASSERT( shared, FusionSHMShared );

          ret = fusion_skirmish_prevail( &shared->lock );
          if (ret)
               return ret;

          D_MAGIC_SET( shm, FusionSHM );

          for (i=0, num=0; i<FUSION_SHM_MAX_POOLS; i++) {
               if (shared->pools[i].active) {
                    D_MAGIC_ASSERT( &shared->pools[i], FusionSHMPoolShared );

                    ret = fusion_shm_pool_attach( shm, &shared->pools[i] );
                    if (ret) {
                         for (--i; i>=0; i--) {
                              if (shared->pools[i].active)
                                   fusion_shm_pool_detach( shm, &shared->pools[i] );
                         }

                         fusion_skirmish_dismiss( &shared->lock );

                         D_MAGIC_CLEAR( shm );

                         return ret;
                    }

                    num++;
               }
          }

          D_ASSERT( num == shared->num_pools );

          fusion_skirmish_dismiss( &shared->lock );
     }

     direct_signal_handler_add( SIGSEGV, segfault_handler, world, &shm->signal_handler );

     return DFB_OK;
}

DirectResult
fusion_shm_deinit( FusionWorld *world )
{
     int              i;
     DirectResult     ret;
     FusionSHM       *shm;
     FusionSHMShared *shared;

     D_MAGIC_ASSERT( world, FusionWorld );

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );

     shared = shm->shared;

     D_MAGIC_ASSERT( shared, FusionSHMShared );

     direct_signal_handler_remove( shm->signal_handler );

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret)
          return ret;

     /* Deinitialize shared data. */
     if (fusion_master( world )) {
          D_ASSUME( shared->num_pools == 0 );

          for (i=0; i<FUSION_SHM_MAX_POOLS; i++) {
               if (shared->pools[i].active) {
                    D_MAGIC_ASSERT( &shared->pools[i], FusionSHMPoolShared );
                    D_MAGIC_ASSERT( &shm->pools[i], FusionSHMPool );

                    D_WARN( "destroying remaining '%s'", shared->pools[i].name );

                    fusion_shm_pool_destroy( world, &shared->pools[i] );
               }
          }

          /* Destroy shared lock. */
          fusion_skirmish_destroy( &shared->lock );

          D_MAGIC_CLEAR( shared );
     }
     else {
          for (i=0; i<FUSION_SHM_MAX_POOLS; i++) {
               if (shared->pools[i].active) {
                    D_MAGIC_ASSERT( &shared->pools[i], FusionSHMPoolShared );
                    D_MAGIC_ASSERT( &shm->pools[i], FusionSHMPool );

                    fusion_shm_pool_detach( shm, &shared->pools[i] );
               }
          }

          fusion_skirmish_dismiss( &shared->lock );
     }

     D_MAGIC_CLEAR( shm );

     return DFB_OK;
}

