/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <core/coredefs.h>
#include <core/thread.h>

#include <misc/mem.h>

#include "fusion_types.h"

#include "fusion_internal.h"

#ifndef FUSION_FAKE

#include <linux/fusion.h>
#include "shmalloc/shmalloc_internal.h"


static void *fusion_read_loop( CoreThread *thread, void *arg );

/**************************
 *  Fusion internal data  *
 **************************/

static int    fusion_refs   =  0;
int           _fusion_fd    = -1;
FusionShared *_fusion_shared = NULL;

static CoreThread *read_loop;

/****************
 *  Public API  *
 ****************/

int _fusion_id = 0;  /* non-zero if Fusion is initialized */

int
fusion_init( int world, int *world_ret )
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
                    
                    FPERROR( "opening '%s' failed!\n", buf );
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
               FPERROR( "opening '%s' failed!\n", buf );
               return -1;
          }
     }

     /* Get our Fusion ID. */
     if (ioctl( _fusion_fd, FUSION_GET_ID, &_fusion_id )) {
          FPERROR( "FUSION_GET_ID failed!\n" );
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

          fusion_skirmish_init( &_fusion_shared->arenas_lock );

          gettimeofday( &_fusion_shared->start_time, NULL );
     }
     else
          _fusion_shared = __shmalloc_get_root();

     read_loop = dfb_thread_create( CTT_MESSAGING, fusion_read_loop, NULL );

     if (world_ret)
          *world_ret = world;

     return _fusion_id;
}

void
fusion_exit()
{
     DFB_ASSERT( fusion_refs > 0 );

     /* decrement local reference counter */
     if (--fusion_refs)
          return;

     dfb_thread_cancel( read_loop );
     dfb_thread_join( read_loop );
     dfb_thread_destroy( read_loop );

     if (_fusion_id == 1) {
          fusion_skirmish_destroy( &_fusion_shared->arenas_lock );
     }

     _fusion_shared = NULL;

     __shmalloc_exit( _fusion_id == 1 );

     _fusion_reactor_free_all();

     _fusion_id = 0;
     
     close( _fusion_fd );
     _fusion_fd = -1;
}

FusionResult
fusion_kill( int fusion_id, int signal, int timeout_ms )
{
     FusionKill param;

     DFB_ASSERT( _fusion_fd != -1 );
     
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
          
          FPERROR ("FUSION_KILL");

          return FUSION_FAILURE;
     }
     
     return FUSION_SUCCESS;
}

long long
fusion_get_millis()
{
     struct timeval tv;
     
     if (!_fusion_shared)
          return dfb_get_millis();
     
     gettimeofday( &tv, NULL );

     return (tv.tv_sec - _fusion_shared->start_time.tv_sec) * 1000 +
            (tv.tv_usec - _fusion_shared->start_time.tv_usec) / 1000;
}

/*****************************
 *  File internal functions  *
 *****************************/

static void *
fusion_read_loop( CoreThread *thread, void *arg )
{
     int    len = 0, result;
     char   buf[1024];
     fd_set set;

     FD_ZERO(&set);
     FD_SET(_fusion_fd,&set);
     
     FDEBUG( "entering loop...\n" );

     dfb_thread_testcancel( thread );
     
     while ((result = select (_fusion_fd+1, &set, NULL, NULL, NULL)) >= 0 ||
            errno == EINTR)
     {
          char *buf_p = buf;

          dfb_thread_testcancel( thread );
          
          FD_ZERO(&set);
          FD_SET(_fusion_fd,&set);
          
          if (result <= 0)
               continue;
          
          //FDEBUG( "going to read...\n" );
          
          len = read (_fusion_fd, buf, 1024);
          
          //FDEBUG( "read %d bytes.\n", len );
          
          dfb_thread_testcancel( thread );
          
          while (buf_p < buf + len) {
               FusionReadMessage *header = (FusionReadMessage*) buf_p;
               void              *data   = buf_p + sizeof(FusionReadMessage);

               switch (header->msg_type) {
                    case FMT_CALL:
                         _fusion_call_process( header->msg_id, data );
                         break;
                    case FMT_REACTOR:
                         _fusion_reactor_process_message( header->msg_id, data );
                         break;
                    default:
                         FDEBUG( "discarding message of unknown type '%d'\n",
                                 header->msg_type );
                         break;
               }

               dfb_thread_testcancel( thread );
               
               buf_p = data + header->msg_size;
          }
     }

     FPERROR( "reading from fusion device failed\n" );

     return NULL;
}

#else

int
fusion_init( int world, int *world_ret )
{
     if (ret_world)
          *ret_world = 0;
     
     return 1;
}

void
fusion_exit()
{
}

FusionResult
fusion_kill( int fusion_id, int signal, int timeout_ms )
{
     return FUSION_SUCCESS;
}

long long
fusion_get_millis()
{
     return dfb_get_millis();
}

#endif

