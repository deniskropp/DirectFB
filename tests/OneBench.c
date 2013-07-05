/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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
#include <direct/util.h>

#include <One/One.h>


static int parse_cmdline ( int argc, char *argv[] );
static int show_usage    ( void );

/**********************************************************************************************************************/

static OneQID       queue_id;
static bool         server;
static unsigned int data_length = 16000;

typedef struct {
     OneQID         response_qid;
} TestRequest;

typedef struct {
     DirectResult   result;
} TestResponse;

#define NUM_ITEMS 30000

static void
TestBandwidth( TestRequest *request )
{
     int         i;
     DirectClock clock;
     char        buf[512];
     size_t      received;
     long long   KperSec;
     OneQID      response_qid = request->response_qid;

     request->response_qid = 0;

     direct_clock_start( &clock );

     for (i=0; i<NUM_ITEMS; i++) {
          if (i == NUM_ITEMS-1)
               request->response_qid = response_qid;

          OneQueue_Dispatch( queue_id, request, sizeof(TestRequest) + data_length );

          if (i == NUM_ITEMS-1)
               OneQueue_Receive( &request->response_qid, 1, buf, sizeof(buf), &received, false, 0 );
     }

     direct_clock_stop( &clock );

     KperSec = (long long) NUM_ITEMS * (long long) data_length * 1000LL / direct_clock_diff( &clock );

     D_INFO( "OneBench/Bandwidth: Stopped after %lld.%03lld seconds... (%lld.%03lld MB/sec)\n",
             DIRECT_CLOCK_DIFF_SEC_MS( &clock ), KperSec / 1000, KperSec % 1000 );
}

static void
TestThroughputSync( TestRequest *request )
{
     int         i;
     DirectClock clock;
     char        buf[512];
     size_t      received;

     direct_clock_start( &clock );

     for (i=0; i<NUM_ITEMS; i++) {
          OneQueue_DispatchReceive( queue_id, request, sizeof(TestRequest),
                                    &request->response_qid, 1, buf, sizeof(buf), &received, false, 0 );
     }

     direct_clock_stop( &clock );


     D_INFO( "OneBench/Throughput/Sync: Stopped after %lld.%03lld seconds... (%lld items/sec)\n",
             DIRECT_CLOCK_DIFF_SEC_MS( &clock ), NUM_ITEMS * 1000000ULL / direct_clock_diff( &clock ) );
}

static void
TestThroughputAsync( TestRequest *request )
{
     int         i;
     DirectClock clock;
     char        buf[512];
     size_t      received;
     OneQID      response_qid = request->response_qid;

     request->response_qid = 0;

     direct_clock_start( &clock );

     for (i=0; i<NUM_ITEMS; i++) {
          if (i == NUM_ITEMS-1)
               request->response_qid = response_qid;

          OneQueue_Dispatch( queue_id, request, sizeof(TestRequest) );

          if (i == NUM_ITEMS-1)
               OneQueue_Receive( &request->response_qid, 1, buf, sizeof(buf), &received, false, 0 );
     }

     direct_clock_stop( &clock );


     D_INFO( "OneBench/Throughput/Async: Stopped after %lld.%03lld seconds... (%lld items/sec)\n",
             DIRECT_CLOCK_DIFF_SEC_MS( &clock ), NUM_ITEMS * 1000000ULL / direct_clock_diff( &clock ) );
}

static void
TestLatency( TestRequest *request )
{
     int         i, n;
     DirectClock clock;
     char        buf[512];
     size_t      received;

     for (n=0; n<3; n++) {
          long long diff, best = 999999999;

          for (i=0; i<100; i++) {
               direct_clock_start( &clock );

               OneQueue_DispatchReceive( queue_id, request, sizeof(TestRequest),
                                         &request->response_qid, 1, buf, sizeof(buf), &received, false, 0 );

               direct_clock_stop( &clock );

               diff = direct_clock_diff( &clock );
               if (diff < best)
                    best = diff;

               usleep( 10000 );
          }


          D_INFO( "OneBench/Latency: Best latency %lld.%03lld milli seconds...\n",
                  best / 1000, best % 1000 );
     }
}

int
main( int argc, char *argv[] )
{
     DirectResult ret;

     if (parse_cmdline( argc, argv ))
          return -1;

     ret = One_Initialize();
     if (ret)
          return ret;

     if (queue_id && !server) {
          /*
           * Client mode
           */
          TestRequest *request;

          request = D_MALLOC( sizeof(TestRequest) + data_length );
          if (!request)
               return D_OOM();

          ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, ONE_QID_NONE, &request->response_qid );
          if (ret)
               return ret;

          TestLatency( request );
          TestThroughputSync( request );
          TestThroughputAsync( request );
          TestBandwidth( request );
     }
     else {
          /*
           * Server mode
           */
          size_t  received;
          char   *buf = malloc(128*1024);

          ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, queue_id, &queue_id );
          if (ret) {
               free( buf );
               return ret;
          }

          D_INFO( "Server Queue ID %u, run client with '-q %u'\n", queue_id, queue_id );

          while (true) {
               size_t i;

               ret = OneQueue_Receive( &queue_id, 1, buf, 128*1024, &received, false, 0 );
               if (ret) {
                    free( buf );
                    return ret;
               }

               for (i=0; i<received; ) {
                    OnePacketHeader *header  = (OnePacketHeader*)(buf + i);
                    TestRequest     *request = (TestRequest*)(header + 1);

                    i += header->size + sizeof(OnePacketHeader);

                    if (request->response_qid) {
                         TestResponse response;
     
                         response.result = DR_OK;
     
                         OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
                    }
               }
          }

          free( buf );
     }

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
          } else
          if (!strcmp( argv[i], "-s" )) {
               server = true;
          } else
               return show_usage();
     }

     return 0;
}

static int
show_usage( void )
{
     fprintf( stderr, "\n"
                      "Usage:\n"
                      "   OneBench [options]\n"
                      "\n"
                      "Options:\n"
                      "   -q  QID  Client mode\n"
                      "\n"
              );

     return -1;
}

