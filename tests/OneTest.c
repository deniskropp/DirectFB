/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <One/One.h>


static int parse_cmdline ( int argc, char *argv[] );
static int show_usage    ( void );

/**********************************************************************************************************************/

static OneQID     queue_id;
static OneThread *thread;

static DirectMutex      lock;
static DirectWaitQueue  queue;

static void
TestDispatch( void                  *context,
              const OnePacketHeader *header,
              void                  *data,
              OneThread             *thread )
{
//     D_INFO( "TestDispatch called\n" );

     direct_mutex_lock( &lock );

     direct_waitqueue_signal( &queue );

     direct_mutex_unlock( &lock );
}


static DirectResult
TestWakeUp()
{
     DirectResult ret;
     char         buf[512];
     size_t       received;
     int          loops = 1000000;

     while (loops--) {
          ret = OneThread_AddQueue( thread, queue_id, TestDispatch, NULL );
          if (ret) {
               D_DERROR( ret, "OneTest: AddQueue error!\n" );
               return ret;
          }
     
          direct_mutex_lock( &lock );

          ret = OneQueue_Dispatch( queue_id, buf, 12 );
          if (ret) {
               D_DERROR( ret, "OneTest: Dispatch error!\n" );
               direct_mutex_unlock( &lock );
               return ret;
          }

          direct_waitqueue_wait( &queue, &lock );

          direct_mutex_unlock( &lock );
     
          ret = OneThread_RemoveQueue( thread, queue_id );
          if (ret) {
               D_DERROR( ret, "OneTest: RemoveQueue error!\n" );
               return ret;
          }
     }

     return DR_OK;
}

int
main( int argc, char *argv[] )
{
     DirectResult ret;

     if (parse_cmdline( argc, argv ))
          return -1;

     direct_mutex_init( &lock );
     direct_waitqueue_init( &queue );

     ret = One_Initialize();
     if (ret)
          return ret;

     ret = OneThread_Create( "OneTest", &thread );
     if (ret)
          return ret;

     ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, queue_id, &queue_id );
     if (ret)
          return ret;

     D_INFO( "Queue ID %u\n", queue_id );

     TestWakeUp();

     direct_mutex_deinit( &lock );
     direct_waitqueue_deinit( &queue );

     return 0;
}

/**********************************************************************************************************************/

static int
parse_cmdline( int argc, char *argv[] )
{
     int i;

     for (i=1; i<argc; i++) {
          if (!strcmp( argv[i], "-q" )) {
               if (++i == argc)
                    return show_usage();

               sscanf( argv[i], "%u", &queue_id );
          }
          else
               return show_usage();
     }

     return 0;
}

static int
show_usage( void )
{
     fprintf( stderr, "\n"
                      "Usage:\n"
                      "   OneTest [options]\n"
                      "\n"
                      "Options:\n"
                      "   -q  QID  Queue ID\n"
                      "\n"
              );

     return -1;
}

