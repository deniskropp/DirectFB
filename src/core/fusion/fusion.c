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

#include <linux/fusion.h>

#include <core/coredefs.h>
#include <core/thread.h>

#include <misc/mem.h>

#include "fusion_types.h"

#include "fusion_internal.h"


#include "shmalloc/shmalloc_internal.h"

static void *fusion_read_loop( CoreThread *thread, void *arg );

/**************************
 *  Fusion internal data  *
 **************************/

static int    fusion_refs   =  0;
int           fusion_fd     = -1;
FusionShared *fusion_shared = NULL;

static CoreThread *read_loop;

/****************
 *  Public API  *
 ****************/

int fusion_id = 0;  /* non-zero if Fusion is initialized */

int
fusion_init()
{
     /* Check against multiple initialization. */
     if (fusion_id) {
          /* Increment local reference counter. */
          fusion_refs++;

          return fusion_id;
     }

     /* Open Fusion Kernel Device. */
     fusion_fd = dfb_try_open ("/dev/fusion",
                               "/dev/misc/fusion", O_RDWR | O_NONBLOCK);
     if (fusion_fd < 0)
          return -1;

     /* Get our Fusion ID. */
     if (ioctl( fusion_fd, FUSION_GET_ID, &fusion_id)) {
          FPERROR( "FUSION_GET_ID failed!\n" );
          close( fusion_fd );
          return -1;
     }

     /* Initialize local reference counter. */
     fusion_refs = 1;

     /* Initialize Reactor part. */
     _reactor_init();

     /* Initialize shmalloc part. */
     if (!__shmalloc_init( fusion_id == 1 )) {
          fusion_id = 0;

          close( fusion_fd );
          return -1;
     }

     if (fusion_id == 1) {
          fusion_shared = __shmalloc_allocate_root( sizeof(FusionShared) );

          skirmish_init( &fusion_shared->arenas_lock );

          gettimeofday( &fusion_shared->start_time, NULL );
     }
     else
          fusion_shared = __shmalloc_get_root();

     read_loop = dfb_thread_create( CTT_MESSAGING, fusion_read_loop, NULL );

     return fusion_id;
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

     if (fusion_id == 1) {
          skirmish_destroy( &fusion_shared->arenas_lock );
     }

     fusion_shared = NULL;

     __shmalloc_exit( fusion_id == 1 );

     fusion_id = 0;
     
     close( fusion_fd );
}

long long
fusion_get_millis()
{
     struct timeval tv;
     
     if (!fusion_shared)
          return dfb_get_millis();
     
     gettimeofday( &tv, NULL );

     return (tv.tv_sec - fusion_shared->start_time.tv_sec) * 1000 +
            (tv.tv_usec - fusion_shared->start_time.tv_usec) / 1000;
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
     FD_SET(fusion_fd,&set);
     
     FDEBUG( "entering loop...\n" );

     while ((result = select (fusion_fd+1, &set, NULL, NULL, NULL)) >= 0 ||
            errno == EINTR)
     {
          char *buf_p = buf;

          dfb_thread_testcancel( thread );
          
          FD_ZERO(&set);
          FD_SET(fusion_fd,&set);
          
          if (result <= 0)
               continue;
          
          FDEBUG( "going to read...\n" );
          
          len = read (fusion_fd, buf, 1024);
          
          FDEBUG( "read %d bytes.\n", len );
          
          dfb_thread_testcancel( thread );
          
          while (buf_p < buf + len) {
               FusionReadMessage *header = (FusionReadMessage*) buf_p;
               void              *data   = buf_p + sizeof(FusionReadMessage);

               switch (header->msg_type) {
                    case FMT_REACTOR:
                         _reactor_process_message( header->msg_id, data );
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

     if (len < 0)
          FPERROR( "reading from fusion device failed\n" );
     else
          FERROR( "read zero bytes from fusion device\n" );

     return NULL;
}

