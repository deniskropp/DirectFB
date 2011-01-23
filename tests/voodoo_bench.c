/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <unistd.h>

#include <direct/direct.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/internal.h>
#include <voodoo/manager.h>
#include <voodoo/server.h>


#define BENCH_SYNC 1


#define VOODOOTEST_METHOD_ID_Push 1
#define VOODOOTEST_METHOD_ID_Sync 2

/**********************************************************************************************************************/

static DirectResult
Dispatch_Push( void *dispatcher, void *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser parser;
     int                 counter;

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, counter );
     VOODOO_PARSER_END( parser );

     return DR_OK;
}

static DirectResult
Dispatch_Sync( void *dispatcher, void *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DR_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "VoodooTest/Dispatch: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case VOODOOTEST_METHOD_ID_Push:
               return Dispatch_Push( dispatcher, real, manager, msg );

          case VOODOOTEST_METHOD_ID_Sync:
               return Dispatch_Sync( dispatcher, real, manager, msg );
     }

     return DR_NOSUCHMETHOD;
}

/**********************************************************************************************************************/

#if BENCH_SYNC
#define NUM_ITEMS   2000000
#else
#define NUM_ITEMS   20000000
#endif

int
main( int argc, char *argv[] )
{
     DirectClock clock;
     int         counter = 0;

     /* Initialize libdirect. */
     direct_initialize();

//     direct_config->log_level = DIRECT_LOG_ALL;

     direct_log_domain_config_level( "Voodoo/Input", DIRECT_LOG_DEBUG_9 );
     direct_log_domain_config_level( "Voodoo/Output", DIRECT_LOG_DEBUG_9 );
     direct_log_domain_config_level( "Voodoo/Dispatch", DIRECT_LOG_DEBUG_9 );
     direct_log_domain_config_level( "Voodoo/Manager", DIRECT_LOG_DEBUG_9 );


     int               pipe_1[2];
     int               pipe_2[2];
//     int               socket[2];
     int               fds_server[2];
     int               fds_client[2];
     VoodooManager    *manager_server;
     VoodooManager    *manager_client;
     VoodooInstanceID  instance;

//     socketpair( PF_LOCAL, SOCK_STREAM, 0, fd );

     pipe( pipe_1 );
     pipe( pipe_2 );

     fds_server[0] = pipe_1[0];
     fds_server[1] = pipe_2[1];

     fds_client[0] = pipe_2[0];
     fds_client[1] = pipe_1[1];


     voodoo_manager_create( fds_server, NULL, NULL, &manager_server );

     voodoo_manager_register_local( manager_server, true, NULL, NULL, Dispatch, &instance );


     voodoo_manager_create( fds_client, NULL, NULL, &manager_client );



     direct_clock_start( &clock );

#if !BENCH_SYNC
     do {
          voodoo_manager_request( manager_client, instance,
                                  VOODOOTEST_METHOD_ID_Push, VREQ_NONE, NULL,
                                  VMBT_INT, counter++,
                                  VMBT_NONE );
     } while (counter < NUM_ITEMS);
#else
     do {
          VoodooResponseMessage *response;

          voodoo_manager_request( manager_client, instance,
                                  VOODOOTEST_METHOD_ID_Sync, VREQ_RESPOND, &response,
                                  VMBT_NONE );

          voodoo_manager_finish_request( manager_client, response );

          counter++;
     } while (counter < NUM_ITEMS);
#endif

     {
          VoodooResponseMessage *response;

          voodoo_manager_request( manager_client, instance,
                                  VOODOOTEST_METHOD_ID_Sync, VREQ_RESPOND, &response,
                                  VMBT_NONE );

          voodoo_manager_finish_request( manager_client, response );
     }

     direct_clock_stop( &clock );


     D_INFO( "Voodoo/Test: Stopped after %d.%03d seconds... (%lld items/sec)\n",
             DIRECT_CLOCK_DIFF_SEC_MS( &clock ), NUM_ITEMS * 1000000ULL / direct_clock_diff( &clock ) );


     /* Shutdown libdirect. */
     direct_shutdown();

     return 0;
}

