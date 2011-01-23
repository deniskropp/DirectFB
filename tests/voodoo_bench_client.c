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

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

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


     int               err;
     int               fd;
     int               fds[2];
     VoodooManager    *manager;
     struct addrinfo   hints;
     struct addrinfo  *addr;
     const char       *hostname = argv[1] ?: "127.0.0.1";
     char              portstr[10];


     memset( &hints, 0, sizeof(hints) );
     hints.ai_flags    = AI_CANONNAME;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_family   = PF_UNSPEC;

     D_INFO( "Voodoo/Client: Looking up host '%s'...\n", hostname );

     snprintf( portstr, sizeof(portstr), "%d", 23239 );

     err = getaddrinfo( hostname, portstr, &hints, &addr );
     if (err) {
          switch (err) {
               case EAI_FAMILY:
                    D_ERROR( "Direct/Log: Unsupported address family!\n" );
                    return DR_UNSUPPORTED;

               case EAI_SOCKTYPE:
                    D_ERROR( "Direct/Log: Unsupported socket type!\n" );
                    return DR_UNSUPPORTED;

               case EAI_NONAME:
                    D_ERROR( "Direct/Log: Host not found!\n" );
                    return DR_FAILURE;

               case EAI_SERVICE:
                    D_ERROR( "Direct/Log: Service is unreachable!\n" );
                    return DR_FAILURE;

#ifdef EAI_ADDRFAMILY
               case EAI_ADDRFAMILY:
#endif
               case EAI_NODATA:
                    D_ERROR( "Direct/Log: Host found, but has no address!\n" );
                    return DR_FAILURE;

               case EAI_MEMORY:
                    return D_OOM();

               case EAI_FAIL:
                    D_ERROR( "Direct/Log: A non-recoverable name server error occurred!\n" );
                    return DR_FAILURE;

               case EAI_AGAIN:
                    D_ERROR( "Direct/Log: Temporary error, try again!\n" );
                    return DR_TEMPUNAVAIL;

               default:
                    D_ERROR( "Direct/Log: Unknown error occured!?\n" );
                    return DR_FAILURE;
          }
     }

     /* Create the client socket. */
     fd = socket( addr->ai_family, SOCK_STREAM, 0 );
     if (fd < 0) {
          D_PERROR( "Voodoo/Client: Could not create the socket via socket()!\n" );
          freeaddrinfo( addr );
          return -1;
     }

     D_INFO( "Voodoo/Client: Connecting to '%s:%s'...\n", addr->ai_canonname, portstr );

     /* Connect to the server. */
     err = connect( fd, addr->ai_addr, addr->ai_addrlen );
     freeaddrinfo( addr );

     if (err) {
          D_PERROR( "Voodoo/Client: Could not connect() to the server!\n" );
          close( fd );
          return -2;
     }



     fds[0] = fd;
     fds[1] = fd;


     voodoo_manager_create( fds, NULL, NULL, &manager );



     direct_clock_start( &clock );

#if !BENCH_SYNC
     do {
          voodoo_manager_request( manager, 1,
                                  VOODOOTEST_METHOD_ID_Push, VREQ_NONE, NULL,
                                  VMBT_INT, counter++,
                                  VMBT_NONE );
     } while (counter < NUM_ITEMS);
#else
     do {
          VoodooResponseMessage *response;

          voodoo_manager_request( manager, 1,
                                  VOODOOTEST_METHOD_ID_Sync, VREQ_RESPOND, &response,
                                  VMBT_NONE );

          voodoo_manager_finish_request( manager, response );

          counter++;
     } while (counter < NUM_ITEMS);
#endif

     {
          VoodooResponseMessage *response;

          voodoo_manager_request( manager, 1,
                                  VOODOOTEST_METHOD_ID_Sync, VREQ_RESPOND, &response,
                                  VMBT_NONE );

          voodoo_manager_finish_request( manager, response );
     }

     direct_clock_stop( &clock );


     D_INFO( "Voodoo/Test: Stopped after %d.%03d seconds... (%lld items/sec)\n",
             DIRECT_CLOCK_DIFF_SEC_MS( &clock ), NUM_ITEMS * 1000000ULL / direct_clock_diff( &clock ) );


     /* Shutdown libdirect. */
     direct_shutdown();

     return 0;
}

