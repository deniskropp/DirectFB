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
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>

#include <fusion/build.h>
#include <fusion/types.h>

#include "fusion_internal.h"

#include <fusion/shmalloc.h>



#if FUSION_BUILD_MULTI

#include <linux/fusion.h>

#include "shmalloc/shmalloc_internal.h"


static void *fusion_read_loop( DirectThread *thread, void *arg );

/**************************
 *  Fusion internal data  *
 **************************/

static int    fusion_refs   =  0;
int           _fusion_fd    = -1;
FusionShared *_fusion_shared = NULL;

static DirectThread *read_loop;

/****************
 *  Public API  *
 ****************/

int _fusion_id = 0;  /* non-zero if Fusion is initialized */

int
fusion_init( int world, int abi_version, int *world_ret )
{
     char buf[20];

     /* Check against multiple initialization. */
     if (_fusion_id) {
          /* Increment local reference counter. */
          fusion_refs++;

          return _fusion_id;
     }

     /* Open Fusion Kernel Device. */
     if (world < 0) {
          for (world=0; world<256; world++) {
               snprintf( buf, sizeof(buf), "/dev/fusion/%d", world );

               _fusion_fd = open (buf, O_RDWR | O_NONBLOCK | O_EXCL);
               if (_fusion_fd < 0) {
                    if (errno == EBUSY)
                         continue;

                    D_PERROR( "Fusion/Init: opening '%s' failed!\n", buf );
                    return -1;
               }
               else
                    break;
          }
     }
     else {
          snprintf( buf, sizeof(buf), "/dev/fusion/%d", world );

          _fusion_fd = open (buf, O_RDWR | O_NONBLOCK);
          if (_fusion_fd < 0) {
               D_PERROR( "Fusion/Init: opening '%s' failed!\n", buf );
               return -1;
          }
     }

     /* Get our Fusion ID. */
     if (ioctl( _fusion_fd, FUSION_GET_ID, &_fusion_id )) {
          D_PERROR( "Fusion/Init: FUSION_GET_ID failed!\n" );
          close( _fusion_fd );
          _fusion_fd = -1;
          return -1;
     }

     /* Initialize local reference counter. */
     fusion_refs = 1;

     /* Initialize shmalloc part. */
     if (!__shmalloc_init( world, _fusion_id == 1 )) {
          fprintf( stderr, "\n"
                           "Shared memory initialization failed.\n"
                           "Please make sure that a tmpfs mount point with "
                             "at least 4MB of free space\n"
                           "is writable, see the DirectFB README "
                             "for more instructions.\n" );

          _fusion_id = 0;

          close( _fusion_fd );
          _fusion_fd = -1;
          return -1;
     }

     if (_fusion_id == 1) {
          _fusion_shared = __shmalloc_allocate_root( sizeof(FusionShared) );

          _fusion_shared->abi_version = abi_version;

          fusion_skirmish_init( &_fusion_shared->arenas_lock );

          gettimeofday( &_fusion_shared->start_time, NULL );
     }
     else {
          _fusion_shared = __shmalloc_get_root();

          if (_fusion_shared->abi_version != abi_version) {
               D_ERROR( "Fusion/Init: ABI version mismatch (my: %d, their: %d)\n",
                        abi_version, _fusion_shared->abi_version );

               _fusion_shared = NULL;

               __shmalloc_exit( false );

               _fusion_id = 0;

               close( _fusion_fd );
               _fusion_fd = -1;
               return -1;
          }
     }

     read_loop = direct_thread_create( DTT_MESSAGING, fusion_read_loop, NULL );

     if (world_ret)
          *world_ret = world;

     return _fusion_id;
}

void
fusion_exit()
{
     int               foo;
     FusionSendMessage msg;

     D_ASSERT( fusion_refs > 0 );

     /* decrement local reference counter */
     if (--fusion_refs)
          return;

     /* Wake up the read loop thread. */
     msg.fusion_id = _fusion_id;
     msg.msg_id    = 0;
     msg.msg_data  = &foo;
     msg.msg_size  = sizeof(foo);

     while (ioctl( _fusion_fd, FUSION_SEND_MESSAGE, &msg ) < 0) {
          if (errno != EINTR) {
               D_PERROR ("FUSION_SEND_MESSAGE");
               break;
          }
     }

     /* Wait for its termination. */
     direct_thread_join( read_loop );
     direct_thread_destroy( read_loop );

     /* Master has to deinitialize shared data. */
     if (_fusion_id == 1) {
          fusion_skirmish_destroy( &_fusion_shared->arenas_lock );
     }

     _fusion_shared = NULL;

     /* Deinitialize or leave shared memory. */
     __shmalloc_exit( _fusion_id == 1 );

     /* Reset local dispatch nodes. */
     _fusion_reactor_free_all();

     _fusion_id = 0;

     if (close( _fusion_fd ))
          D_PERROR( "Fusion/Exit: closing the fusion device failed!\n" );
     _fusion_fd = -1;
}

FusionResult
fusion_kill( int fusion_id, int signal, int timeout_ms )
{
     FusionKill param;

     D_ASSERT( _fusion_fd != -1 );

     param.fusion_id  = fusion_id;
     param.signal     = signal;
     param.timeout_ms = timeout_ms;

     while (ioctl (_fusion_fd, FUSION_KILL, &param)) {
          switch (errno) {
               case EINTR:
                    continue;
               case ETIMEDOUT:
                    return FUSION_TIMEOUT;
               default:
                    break;
          }

          D_PERROR ("FUSION_KILL");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

long long
fusion_get_millis()
{
     struct timeval tv;

     if (!_fusion_shared)
          return direct_clock_get_millis();

     gettimeofday( &tv, NULL );

     return (tv.tv_sec - _fusion_shared->start_time.tv_sec) * 1000LL +
            (tv.tv_usec - _fusion_shared->start_time.tv_usec) / 1000LL;
}

int
fusion_id()
{
     return _fusion_id;
}

void
fusion_sync()
{
     int            result;
     fd_set         set;
     struct timeval tv;
     int            loops = 100;

     D_DEBUG( "Fusion/Sync: syncing with fusion device...\n" );

     while (loops--) {
          FD_ZERO(&set);
          FD_SET(_fusion_fd,&set);

          tv.tv_sec  = 0;
          tv.tv_usec = 1000;

          result = select (_fusion_fd+1, &set, NULL, NULL, &tv);

          switch (result) {
               case -1:
                    if (errno == EINTR)
                         continue;

                    D_PERROR( "Fusion/Sync: select() failed!\n");
                    return;

               case 0:
                    D_DEBUG( "Fusion/Sync: ...synced.\n");
                    return;

               default:
                    D_DEBUG( "Fusion/Sync: ...syncing...\n");
                    usleep( 10000 );
          }
     }

     D_ERROR( "Fusion/Sync: timeout waiting for empty read buffer\n" );
}

/*****************************
 *  File internal functions  *
 *****************************/

static void *
fusion_read_loop( DirectThread *thread, void *arg )
{
     int    len = 0, result;
     char   buf[1024];
     fd_set set;

     FD_ZERO(&set);
     FD_SET(_fusion_fd,&set);

     D_DEBUG( "Fusion/Receiver: entering loop...\n" );

     while ((result = select (_fusion_fd+1, &set, NULL, NULL, NULL)) >= 0 ||
            errno == EINTR)
     {
          char *buf_p = buf;

          FD_ZERO(&set);
          FD_SET(_fusion_fd,&set);

          if (result <= 0)
               continue;

          len = read (_fusion_fd, buf, 1024);

          while (buf_p < buf + len) {
               FusionReadMessage *header = (FusionReadMessage*) buf_p;
               void              *data   = buf_p + sizeof(FusionReadMessage);

               switch (header->msg_type) {
                    case FMT_SEND:
                         if (!fusion_refs)
                              return NULL;
                         break;
                    case FMT_CALL:
                         _fusion_call_process( header->msg_id, data );
                         break;
                    case FMT_REACTOR:
                         _fusion_reactor_process_message( header->msg_id, data );
                         break;
                    default:
                         D_DEBUG( "Fusion/Receiver: discarding message of unknown type '%d'\n",
                                  header->msg_type );
                         break;
               }

               buf_p = data + header->msg_size;
          }
     }

     D_PERROR( "Fusion/Receiver: reading from fusion device failed!\n" );

     return NULL;
}

#else

int
fusion_init( int world, int abi_version, int *ret_world )
{
     if (ret_world)
          *ret_world = 0;

     return 1;
}

void
fusion_exit()
{
     fusion_dbg_print_memleaks();
}

FusionResult
fusion_kill( int fusion_id, int signal, int timeout_ms )
{
     return FUSION_SUCCESS;
}

long long
fusion_get_millis()
{
     return direct_clock_get_millis();
}

int
fusion_id()
{
     return 1;
}

void
fusion_sync()
{
}

#endif

