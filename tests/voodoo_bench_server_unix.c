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

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <direct/direct.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/internal.h>
#include <voodoo/manager.h>
#include <voodoo/server.h>


#define UNIX_PATH_MAX	108

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

int
main( int argc, char *argv[] )
{
     static const int one = 1;

     /* Initialize libdirect. */
     direct_initialize();

//     direct_config->log_level = DIRECT_LOG_ALL;

     direct_log_domain_config_level( "Voodoo/Input", DIRECT_LOG_DEBUG_9 );
     direct_log_domain_config_level( "Voodoo/Output", DIRECT_LOG_DEBUG_9 );
     direct_log_domain_config_level( "Voodoo/Dispatch", DIRECT_LOG_DEBUG_9 );
     direct_log_domain_config_level( "Voodoo/Manager", DIRECT_LOG_DEBUG_9 );


     int                 lfd;
     int                 fds[2];
     VoodooManager      *manager;
     VoodooInstanceID    instance;
     struct sockaddr_un  addr;
     socklen_t           addrlen = sizeof(addr);


     /* Create the player socket. */
     lfd = socket( PF_UNIX, SOCK_STREAM, 0 );
     if (lfd < 0) {
          D_PERROR( "Voodoo/Player: Could not create the Unix Domain socket!" );
          return -1;
     }

     /* Allow reuse of local address. */
     if (setsockopt( lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Player: Could not set SO_REUSEADDR on Unix Domain socket!\n" );

     memset( &addr, 0, sizeof(addr) );

     /* Bind the socket to the local port. */
     addr.sun_family = AF_UNIX;

     snprintf( addr.sun_path + 1, UNIX_PATH_MAX - 1, "Voodoo/%u", 23239 );

     if (bind( lfd, (struct sockaddr*) &addr, strlen(addr.sun_path+1)+1 + sizeof(addr.sun_family) )) {
          D_PERROR( "Voodoo/Player: Could not bind() the Unix Domain socket!\n" );
          close( lfd );
          return -2;
     }

     /* Start listening. */
     if (listen( lfd, 4 )) {
          D_PERROR( "Voodoo/Server: Could not listen() to the socket!\n" );
          return -3;
     }



     struct pollfd  pf;
     int            cfd = -1;

     pf.fd     = lfd;
     pf.events = POLLIN;


     while (cfd == -1) {
          switch (poll( &pf, 1, 20000 )) {
               default:
                    cfd = accept( lfd, &addr, &addrlen );
                    if (cfd < 0) {
                         D_PERROR( "Voodoo/Server: Could not accept() incoming connection!\n" );
                         return -4;
                    }
                    break;
     
               case 0:
                    D_ERROR( "  -> Timeout during poll()!?\n" );
                    break;
     
               case -1:
                    if (errno != EINTR) {
                         D_PERROR( "Voodoo/Server: Could not poll() the socket!\n" );
                         return -5;
                    }
                    break;
          }
     }


     fds[0] = cfd;
     fds[1] = cfd;


     voodoo_manager_create( fds, NULL, NULL, &manager );

     voodoo_manager_register_local( manager, VOODOO_INSTANCE_NONE, NULL, NULL, Dispatch, &instance );


     pause();


     /* Shutdown libdirect. */
     direct_shutdown();

     return 0;
}

