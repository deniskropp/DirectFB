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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/thread.h>
#include <direct/trace.h>

#include <fusion/build.h>
#include <fusion/types.h>

#include "fusion_internal.h"

#include <fusion/shmalloc.h>

#include <fusion/shm/shm.h>


#if FUSION_BUILD_MULTI

#include <linux/fusion.h>


D_DEBUG_DOMAIN( Fusion_Main, "Fusion/Main", "Fusion - High level IPC" );

/**********************************************************************************************************************/

static void                      *fusion_dispatch_loop ( DirectThread *thread,
                                                         void         *arg );

/**********************************************************************************************************************/

static FusionWorld     *fusion_worlds[FUSION_MAX_WORLDS];
static pthread_mutex_t  fusion_worlds_lock = PTHREAD_MUTEX_INITIALIZER;

/**********************************************************************************************************************/

int
_fusion_fd( const FusionWorldShared *shared )
{
     int          index;
     FusionWorld *world;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     index = shared->world_index;

     D_ASSERT( index >= 0 );
     D_ASSERT( index < FUSION_MAX_WORLDS );

     world = fusion_worlds[index];

     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fusion_fd;
}

FusionID
_fusion_id( const FusionWorldShared *shared )
{
     int          index;
     FusionWorld *world;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     index = shared->world_index;

     D_ASSERT( index >= 0 );
     D_ASSERT( index < FUSION_MAX_WORLDS );

     world = fusion_worlds[index];

     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fusion_id;
}

FusionWorld *
_fusion_world( const FusionWorldShared *shared )
{
     int          index;
     FusionWorld *world;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     index = shared->world_index;

     D_ASSERT( index >= 0 );
     D_ASSERT( index < FUSION_MAX_WORLDS );

     world = fusion_worlds[index];

     D_MAGIC_ASSERT( world, FusionWorld );

     return world;
}

/**********************************************************************************************************************/

static void
fusion_world_fork( FusionWorld *world )
{
     int                fd;
     char               buf1[20];
     char               buf2[20];
     FusionFork         fork;
     FusionWorldShared *shared;

     D_MAGIC_ASSERT( world, FusionWorld );

     shared = world->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     snprintf( buf1, sizeof(buf1), "/dev/fusion%d", shared->world_index );
     snprintf( buf2, sizeof(buf2), "/dev/fusion/%d", shared->world_index );

     /* Open Fusion Kernel Device. */
     fd = direct_try_open( buf1, buf2, O_RDWR | O_NONBLOCK, true );
     if (fd < 0) {
          D_PERROR( "Fusion/Main: Reopening fusion device (world %d) failed!\n", shared->world_index );
          raise(5);
     }

     /* Fill fork information. */
     fork.fusion_id = world->fusion_id;

     /* Fork within the fusion world. */
     while (ioctl( fd, FUSION_FORK, &fork )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Main: Could not fork in world '%d'!\n", shared->world_index );
               raise(5);
          }
     }

     D_DEBUG_AT( Fusion_Main, "  -> Fusion ID 0x%08lx\n", fork.fusion_id );

     /* Get new fusion id back. */
     world->fusion_id = fork.fusion_id;

     /* Close old file descriptor. */
     close( world->fusion_fd );

     /* Write back new file descriptor. */
     world->fusion_fd = fd;




     D_DEBUG_AT( Fusion_Main, "  -> restarting dispatcher loop...\n" );

     /* Restart the dispatcher thread. FIXME: free old struct */
     world->dispatch_loop = direct_thread_create( DTT_MESSAGING,
                                                  fusion_dispatch_loop,
                                                  world, "Fusion Dispatch" );
     if (!world->dispatch_loop)
          raise(5);
}

static void
fusion_fork_handler_child()
{
     int i;

     D_DEBUG_AT( Fusion_Main, "%s()\n", __FUNCTION__ );

     for (i=0; i<FUSION_MAX_WORLDS; i++) {
          FusionWorld *world = fusion_worlds[i];

          if (!world)
               continue;

          D_MAGIC_ASSERT( world, FusionWorld );

          switch (world->fork_action) {
               default:
                    D_BUG( "unknown fork action %d", world->fork_action );

               case FFA_CLOSE:
                    D_DEBUG_AT( Fusion_Main, "  -> closing world %d\n", i );

                    /* Remove world from global list. */
                    fusion_worlds[i] = NULL;

                    /* Unmap shared area. */
                    munmap( world->shared, sizeof(FusionWorldShared) );

                    /* Close Fusion Kernel Device. */
                    close( world->fusion_fd );

                    /* Free local world data. */
                    D_MAGIC_CLEAR( world );
                    D_FREE( world );

                    break;

               case FFA_FORK:
                    D_DEBUG_AT( Fusion_Main, "  -> forking in world %d\n", i );

                    fusion_world_fork( world );

                    break;
          }
     }
}

/**********************************************************************************************************************/

/*
 * Enters a fusion world by joining or creating it.
 *
 * If <b>world</b> is negative, the next free index is used to create a new world.
 * Otherwise the world with the specified index is joined or created.
 */
DirectResult
fusion_enter( int               world_index,
              int               abi_version,
              FusionEnterRole   role,
              FusionWorld     **ret_world )
{
     DirectResult       ret    = DFB_OK;
     int                fd     = -1;
     FusionWorld       *world  = NULL;
     FusionWorldShared *shared = NULL;
     FusionEnter        enter;
     char               buf1[20];
     char               buf2[20];
     
     static bool atfork_called = false;

     if (!atfork_called) {
          pthread_atfork( NULL, NULL, fusion_fork_handler_child );

          atfork_called = true;
     }

     D_DEBUG_AT( Fusion_Main, "%s( %d, %d, %p )\n", __FUNCTION__, world_index, abi_version, ret_world );

     D_ASSERT( ret_world != NULL );

     if (world_index >= FUSION_MAX_WORLDS) {
          D_ERROR( "Fusion/Init: World index %d exceeds maximum index %d!\n", world_index, FUSION_MAX_WORLDS - 1 );
          return DFB_INVARG;
     }

     direct_initialize();

     pthread_mutex_lock( &fusion_worlds_lock );


     if (world_index < 0) {
          if (role == FER_SLAVE) {
               D_ERROR( "Fusion/Init: Slave role and a new world (index -1) was requested!\n" );
               pthread_mutex_unlock( &fusion_worlds_lock );
               return DFB_INVARG;
          }

          for (world_index=0; world_index<FUSION_MAX_WORLDS; world_index++) {
               world = fusion_worlds[world_index];
               if (world)
                    break;

               snprintf( buf1, sizeof(buf1), "/dev/fusion%d", world_index );
               snprintf( buf2, sizeof(buf2), "/dev/fusion/%d", world_index );

               /* Open Fusion Kernel Device. */
               fd = direct_try_open( buf1, buf2, O_RDWR | O_NONBLOCK | O_EXCL, false );
               if (fd < 0) {
                    if (errno != EBUSY)
                         D_PERROR( "Fusion/Init: Error opening '%s' and/or '%s'!\n", buf1, buf2 );
               }
               else
                    break;
          }
     }
     else {
          world = fusion_worlds[world_index];
          if (!world) {
               int flags = O_RDWR | O_NONBLOCK;

               snprintf( buf1, sizeof(buf1), "/dev/fusion%d", world_index );
               snprintf( buf2, sizeof(buf2), "/dev/fusion/%d", world_index );

               if (role == FER_MASTER)
                    flags |= O_EXCL;

               /* Open Fusion Kernel Device. */
               fd = direct_try_open( buf1, buf2, flags, true );
          }
     }

     /* Enter a world again? */
     if (world) {
          D_MAGIC_ASSERT( world, FusionWorld );
          D_ASSERT( world->refs > 0 );

          /* Check the role again. */
          switch (role) {
               case FER_MASTER:
                    if (world->fusion_id != FUSION_ID_MASTER) {
                         D_ERROR( "Fusion/Init: Master role requested for a world (%d) "
                                  "we're already slave in!\n", world_index );
                         ret = DFB_UNSUPPORTED;
                         goto error;
                    }
                    break;

               case FER_SLAVE:
                    if (world->fusion_id == FUSION_ID_MASTER) {
                         D_ERROR( "Fusion/Init: Slave role requested for a world (%d) "
                                  "we're already master in!\n", world_index );
                         ret = DFB_UNSUPPORTED;
                         goto error;
                    }
                    break;

               case FER_ANY:
                    break;
          }

          shared = world->shared;

          D_MAGIC_ASSERT( shared, FusionWorldShared );

          if (shared->world_abi != abi_version) {
               D_ERROR( "Fusion/Init: World ABI (%d) of world '%d' doesn't match own (%d)!\n",
                        shared->world_abi, world_index, abi_version );
               ret = DFB_VERSIONMISMATCH;
               goto error;
          }

          world->refs++;

          pthread_mutex_unlock( &fusion_worlds_lock );

          D_DEBUG_AT( Fusion_Main, "  -> using existing world %p [%d]\n", world, world_index );

          /* Return the world. */
          *ret_world = world;

          return DFB_OK;
     }

     if (fd < 0) {
          D_PERROR( "Fusion/Init: Opening fusion device (world %d) as '%s' failed!\n", world_index,
                    role == FER_ANY ? "any" : (role == FER_MASTER ? "master" : "slave")  );
          ret = DFB_INIT;
          goto error;
     }

     /* Drop "identity" when running another program. */
     if (fcntl( fd, F_SETFD, FD_CLOEXEC ) < 0)
          D_PERROR( "Fusion/Init: Setting FD_CLOEXEC flag failed!\n" );

     /* Fill enter information. */
     enter.api.major = FUSION_API_MAJOR;
     enter.api.minor = FUSION_API_MINOR;
     enter.fusion_id = 0;     /* Clear for check below. */

     /* Enter the fusion world. */
     while (ioctl( fd, FUSION_ENTER, &enter )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Init: Could not enter world '%d'!\n", world_index );
               ret = DFB_INIT;
               goto error;
          }
     }

     /* Check for valid Fusion ID. */
     if (!enter.fusion_id) {
          D_ERROR( "Fusion/Init: Got no ID from FUSION_ENTER! Kernel module might be too old.\n" );
          ret = DFB_INIT;
          goto error;
     }

     D_DEBUG_AT( Fusion_Main, "  -> Fusion ID 0x%08lx\n", enter.fusion_id );

     /* Check slave role only, master is handled by O_EXCL earlier. */
     if (role == FER_SLAVE && enter.fusion_id == FUSION_ID_MASTER) {
          D_PERROR( "Fusion/Init: Entering world '%d' as a slave failed!\n", world_index );
          ret = DFB_UNSUPPORTED;
          goto error;
     }

     /* Map shared area. */
     shared = mmap( (void*) 0x20000000 + 0x2000 * world_index, sizeof(FusionWorldShared),
                    PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0 );
     if (shared == MAP_FAILED) {
          D_PERROR( "Fusion/Init: Mapping shared area failed!\n" );
          goto error;
     }

     D_DEBUG_AT( Fusion_Main, "  -> shared area at %p, size %d\n", shared, sizeof(FusionWorldShared) );

     /* Initialize shared data. */
     if (enter.fusion_id == FUSION_ID_MASTER) {
          /* Set ABI version. */
          shared->world_abi = abi_version;

          /* Set the world index. */
          shared->world_index = world_index;

          /* Set start time of world clock. */
          gettimeofday( &shared->start_time, NULL );

          D_MAGIC_SET( shared, FusionWorldShared );
     }
     else {
          D_MAGIC_ASSERT( shared, FusionWorldShared );

          /* Check ABI version. */
          if (shared->world_abi != abi_version) {
               D_ERROR( "Fusion/Init: World ABI (%d) doesn't match own (%d)!\n",
                        shared->world_abi, abi_version );
               ret = DFB_VERSIONMISMATCH;
               goto error;
          }
     }

     /* Synchronize to world clock. */
     direct_clock_set_start( &shared->start_time );
     

     /* Allocate local data. */
     world = D_CALLOC( 1, sizeof(FusionWorld) );
     if (!world) {
          ret = D_OOM();
          goto error;
     }

     /* Initialize local data. */
     world->refs      = 1;
     world->shared    = shared;
     world->fusion_fd = fd;
     world->fusion_id = enter.fusion_id;

     D_MAGIC_SET( world, FusionWorld );

     fusion_worlds[world_index] = world;


     /* Initialize shared memory part. */
     ret = fusion_shm_init( world );
     if (ret)
          goto error2;


     D_DEBUG_AT( Fusion_Main, "  -> initializing other parts...\n" );

     /* Initialize other parts. */
     if (enter.fusion_id == FUSION_ID_MASTER) {
          fusion_skirmish_init( &shared->arenas_lock, "Fusion Arenas", world );
          fusion_skirmish_init( &shared->reactor_globals, "Fusion Reactor Globals", world );

          /* Create the main pool. */
          ret = fusion_shm_pool_create( world, "Fusion Main Pool", 0x100000,
                                        direct_config->debug, &shared->main_pool );
          if (ret)
               goto error3;
     }


     D_DEBUG_AT( Fusion_Main, "  -> starting dispatcher loop...\n" );

     /* Start the dispatcher thread. */
     world->dispatch_loop = direct_thread_create( DTT_MESSAGING,
                                                  fusion_dispatch_loop,
                                                  world, "Fusion Dispatch" );
     if (!world->dispatch_loop) {
          ret = DFB_FAILURE;
          goto error4;
     }


     /* Let others enter the world. */
     if (enter.fusion_id == FUSION_ID_MASTER) {
          D_DEBUG_AT( Fusion_Main, "  -> unblocking world...\n" );

          while (ioctl( fd, FUSION_UNBLOCK )) {
               if (errno != EINTR) {
                    D_PERROR( "Fusion/Init: Could not unblock world!\n" );
                    ret = DFB_FUSION;
                    goto error4;
               }
          }
     }

     D_DEBUG_AT( Fusion_Main, "  -> done. (%p)\n", world );

     pthread_mutex_unlock( &fusion_worlds_lock );

     /* Return the fusion world. */
     *ret_world = world;

     return DFB_OK;


error4:
     if (world->dispatch_loop)
          direct_thread_destroy( world->dispatch_loop );

     if (enter.fusion_id == FUSION_ID_MASTER)
          fusion_shm_pool_destroy( world, shared->main_pool );

error3:
     if (enter.fusion_id == FUSION_ID_MASTER) {
          fusion_skirmish_destroy( &shared->arenas_lock );
          fusion_skirmish_destroy( &shared->reactor_globals );
     }

     fusion_shm_deinit( world );


error2:
     fusion_worlds[world_index] = world;

     D_MAGIC_CLEAR( world );

     D_FREE( world );

error:
     if (shared && shared != MAP_FAILED) {
          if (enter.fusion_id == FUSION_ID_MASTER)
               D_MAGIC_CLEAR( shared );

          munmap( shared, sizeof(FusionWorldShared) );
     }

     if (fd != -1)
          close( fd );

     pthread_mutex_unlock( &fusion_worlds_lock );

     direct_shutdown();

     return ret;
}

/*
 * Exits the fusion world.
 *
 * If 'emergency' is true the function won't join but kill the dispatcher thread.
 */
DirectResult
fusion_exit( FusionWorld *world,
             bool         emergency )
{
     FusionWorldShared *shared;

     D_DEBUG_AT( Fusion_Main, "%s( %p, %semergency )\n", __FUNCTION__, world, emergency ? "" : "no " );

     D_MAGIC_ASSERT( world, FusionWorld );

     shared = world->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );


     pthread_mutex_lock( &fusion_worlds_lock );

     D_ASSERT( world->refs > 0 );

     if (--world->refs) {
          pthread_mutex_unlock( &fusion_worlds_lock );
          return DFB_OK;
     }


     if (!emergency) {
          int               foo;
          FusionSendMessage msg;

          /* Wake up the read loop thread. */
          msg.fusion_id = world->fusion_id;
          msg.msg_id    = 0;
          msg.msg_data  = &foo;
          msg.msg_size  = sizeof(foo);

          while (ioctl( world->fusion_fd, FUSION_SEND_MESSAGE, &msg ) < 0) {
               if (errno != EINTR) {
                    D_PERROR ("FUSION_SEND_MESSAGE");
                    break;
               }
          }

          /* Wait for its termination. */
          direct_thread_join( world->dispatch_loop );
     }

     direct_thread_destroy( world->dispatch_loop );


     /* Master has to deinitialize shared data. */
     if (fusion_master( world )) {
          fusion_skirmish_destroy( &shared->reactor_globals );
          fusion_skirmish_destroy( &shared->arenas_lock );

          fusion_shm_pool_destroy( world, shared->main_pool );
     }

     /* Deinitialize or leave shared memory. */
     fusion_shm_deinit( world );

     /* Reset local dispatch nodes. */
     _fusion_reactor_free_all( world );


     /* Remove world from global list. */
     fusion_worlds[shared->world_index] = NULL;


     /* Unmap shared area. */
     if (fusion_master( world ))
          D_MAGIC_CLEAR( shared );

     munmap( shared, sizeof(FusionWorldShared) );


     /* Close Fusion Kernel Device. */
     close( world->fusion_fd );


     /* Free local world data. */
     D_MAGIC_CLEAR( world );
     D_FREE( world );


     pthread_mutex_unlock( &fusion_worlds_lock );

     direct_shutdown();

     return DFB_OK;
}

/*
 * Sets the fork() action of the calling Fusionee within the world.
 */
void
fusion_world_set_fork_action( FusionWorld      *world,
                              FusionForkAction  action )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     world->fork_action = action;
}

/*
 * Return the index of the specified world.
 */
int
fusion_world_index( const FusionWorld *world )
{
     FusionWorldShared *shared;

     D_MAGIC_ASSERT( world, FusionWorld );

     shared = world->shared;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     return shared->world_index;
}

/*
 * Return the own Fusion ID within the specified world.
 */
FusionID
fusion_id( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fusion_id;
}

/*
 * Return true if this process is the master.
 */
bool
fusion_master( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fusion_id == FUSION_ID_MASTER;
}

/*
 * Wait until all pending messages are processed.
 */
DirectResult
fusion_sync( const FusionWorld *world )
{
     int            result;
     fd_set         set;
     struct timeval tv;
     int            loops = 100;

     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Main, "%s( %p )\n", __FUNCTION__, world );

     D_DEBUG_AT( Fusion_Main, "syncing with fusion device...\n" );

     while (loops--) {
          FD_ZERO( &set );
          FD_SET( world->fusion_fd, &set );

          tv.tv_sec  = 0;
          tv.tv_usec = 20000;

          result = select( world->fusion_fd + 1, &set, NULL, NULL, &tv );

          switch (result) {
               case -1:
                    if (errno == EINTR)
                         continue;

                    D_PERROR( "Fusion/Sync: select() failed!\n");
                    return DFB_FAILURE;

               case 0:
                    D_DEBUG_AT( Fusion_Main, "  -> synced.\n");
                    return DFB_OK;

               default:
                    usleep( 20000 );
          }
     }

     D_DEBUG_AT( Fusion_Main, "  -> timeout!\n");

     D_ERROR( "Fusion/Main: Timeout waiting for empty message queue!\n" );

     return DFB_TIMEOUT;
}

/*
 * Sends a signal to one or more fusionees and optionally waits
 * for their processes to terminate.
 *
 * A fusion_id of zero means all fusionees but the calling one.
 * A timeout of zero means infinite waiting while a negative value
 * means no waiting at all.
 */
DirectResult
fusion_kill( FusionWorld *world,
             FusionID     fusion_id,
             int          signal,
             int          timeout_ms )
{
     FusionKill param;

     D_MAGIC_ASSERT( world, FusionWorld );

     param.fusion_id  = fusion_id;
     param.signal     = signal;
     param.timeout_ms = timeout_ms;

     while (ioctl( world->fusion_fd, FUSION_KILL, &param )) {
          switch (errno) {
               case EINTR:
                    continue;
               case ETIMEDOUT:
                    return DFB_TIMEOUT;
               default:
                    break;
          }

          D_PERROR ("FUSION_KILL");

          return DFB_FAILURE;
     }

     return DFB_OK;
}

/*
 * Check if a pointer points to the shared memory.
 */
bool
fusion_is_shared( FusionWorld *world,
                  const void  *ptr )
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

     if (ptr >= (void*) world->shared && ptr < (void*) world->shared + sizeof(FusionWorldShared))
          return true;

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret)
          return false;

     for (i=0; i<FUSION_SHM_MAX_POOLS; i++) {
          if (shared->pools[i].active) {
               shmalloc_heap       *heap;
               FusionSHMPoolShared *pool = &shared->pools[i];

               D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

               heap = pool->heap;

               D_MAGIC_ASSERT( heap, shmalloc_heap );

               if (ptr >= pool->addr_base && ptr < pool->addr_base + heap->size) {
                    fusion_skirmish_dismiss( &shared->lock );
                    return true;
               }
          }
     }

     fusion_skirmish_dismiss( &shared->lock );

     return false;
}

/**********************************************************************************************************************/

static void *
fusion_dispatch_loop( DirectThread *thread, void *arg )
{
     int          len = 0;
     int          result;
     char         buf[1024];
     fd_set       set;
     FusionWorld *world = arg;

     while (true) {
          char *buf_p = buf;

          D_MAGIC_ASSERT( world, FusionWorld );

          FD_ZERO( &set );
          FD_SET( world->fusion_fd, &set );

          result = select( world->fusion_fd + 1, &set, NULL, NULL, NULL );
          if (result < 0) {
               switch (errno) {
                    case EINTR:
                         continue;

                    default:
                         D_PERROR( "Fusion/Dispatcher: select() failed!\n" );
                         return NULL;
               }
          }

          D_MAGIC_ASSERT( world, FusionWorld );

          if (FD_ISSET( world->fusion_fd, &set )) {
               len = read( world->fusion_fd, buf, 1024 );
               if (len < 0)
                    break;

               while (buf_p < buf + len) {
                    FusionReadMessage *header = (FusionReadMessage*) buf_p;
                    void              *data   = buf_p + sizeof(FusionReadMessage);

                    D_MAGIC_ASSERT( world, FusionWorld );

                    switch (header->msg_type) {
                         case FMT_SEND:
                              if (!world->refs)
                                   return NULL;
                              break;
                         case FMT_CALL:
                              _fusion_call_process( world, header->msg_id, data );
                              break;
                         case FMT_REACTOR:
                              _fusion_reactor_process_message( world, header->msg_id, data );
                              break;
                         case FMT_SHMPOOL:
                              _fusion_shmpool_process( world, header->msg_id, data );
                              break;
                         default:
                              D_DEBUG( "Fusion/Receiver: discarding message of unknown type '%d'\n",
                                       header->msg_type );
                              break;
                    }

                    buf_p = data + header->msg_size;
               }
          }
     }

     D_PERROR( "Fusion/Receiver: reading from fusion device failed!\n" );

     return NULL;
}

#else

/*
 * Enters a fusion world by joining or creating it.
 *
 * If <b>world_index</b> is negative, the next free index is used to create a new world.
 * Otherwise the world with the specified index is joined or created.
 */
DirectResult fusion_enter( int               world_index,
                           int               abi_version,
                           FusionEnterRole   role,
                           FusionWorld     **ret_world )
{
     DirectResult  ret;
     FusionWorld  *world = NULL;

     D_ASSERT( ret_world != NULL );

     ret = direct_initialize();
     if (ret)
          return ret;

     world = D_CALLOC( 1, sizeof(FusionWorld) );
     if (!world) {
          ret = D_OOM();
          goto error;
     }

     world->shared = D_CALLOC( 1, sizeof(FusionWorldShared) );
     if (!world->shared) {
          ret = D_OOM();
          goto error;
     }

     /* Create the main pool. */
     ret = fusion_shm_pool_create( world, "Fusion Main Pool", 0x100000,
                                   direct_config->debug, &world->shared->main_pool );
     if (ret)
          goto error;

     D_MAGIC_SET( world, FusionWorld );
     D_MAGIC_SET( world->shared, FusionWorldShared );

     *ret_world = world;

     return DFB_OK;


error:
     if (world) {
          if (world->shared)
               D_FREE( world->shared );

          D_FREE( world );
     }

     direct_shutdown();

     return ret;
}

/*
 * Exits the fusion world.
 *
 * If 'emergency' is true the function won't join but kill the dispatcher thread.
 */
DirectResult fusion_exit( FusionWorld *world,
                          bool         emergency )
{
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     fusion_shm_pool_destroy( world, world->shared->main_pool );

     D_MAGIC_CLEAR( world->shared );

     D_FREE( world->shared );

     D_MAGIC_CLEAR( world );

     D_FREE( world );

     direct_shutdown();

     return DFB_OK;
}

/*
 * Sets the fork() action of the calling Fusionee within the world.
 */
void
fusion_world_set_fork_action( FusionWorld      *world,
                              FusionForkAction  action )
{
     D_MAGIC_ASSERT( world, FusionWorld );
}

/*
 * Return the index of the specified world.
 */
int
fusion_world_index( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return 0;
}


/*
 * Return true if this process is the master.
 */
bool
fusion_master( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return true;
}

/*
 * Sends a signal to one or more fusionees and optionally waits
 * for their processes to terminate.
 *
 * A fusion_id of zero means all fusionees but the calling one.
 * A timeout of zero means infinite waiting while a negative value
 * means no waiting at all.
 */
DirectResult
fusion_kill( FusionWorld *world,
             FusionID     fusion_id,
             int          signal,
             int          timeout_ms )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return DFB_OK;
}

/*
 * Return the own Fusion ID within the specified world.
 */
FusionID
fusion_id( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return 1;
}

/*
 * Wait until all pending messages are processed.
 */
DirectResult
fusion_sync( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return DFB_OK;
}

/* Check if a pointer points to the shared memory. */
bool
fusion_is_shared( FusionWorld *world,
                  const void  *ptr )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return true;
}

#endif

