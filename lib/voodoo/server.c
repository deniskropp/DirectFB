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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/server.h>
#include <voodoo/internal.h>


typedef struct {
     DirectLink     link;

     int            fd;
     VoodooManager *manager;
} Connection;

typedef struct {
     const char           *name;
     VoodooSuperConstruct  func;
     void                 *ctx;

     IAny                 *interface;
} Super;

#define MAX_SUPER   8

struct __V_VoodooServer {
     int         fd;
     bool        quit;
     DirectLink *connections;

     int         num_super;
     Super       supers[MAX_SUPER];
};

static const int one = 1;

/**************************************************************************************************/

static DirectResult accept_connection( VoodooServer *server );

/**************************************************************************************************/

DirectResult
voodoo_server_create( VoodooServer **ret_server )
{
     DirectResult        ret;
     int                 fd;
     struct sockaddr_in  addr;
     VoodooServer       *server;

     D_ASSERT( ret_server != NULL );

     /* Create the server socket. */
     fd = socket( PF_INET, SOCK_STREAM, 0 );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Server: Could not create the socket via socket()!\n" );
          return ret;
     }

     /* Allow reuse of local address. */
     if (setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Server: Could not set SO_REUSEADDR!\n" );

     /* Avoid pending messages. */
     if (setsockopt( fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Server: Could not set TCP_NODELAY!\n" );

     /* Bind the socket to the local port. */
     addr.sin_family      = AF_INET;
     addr.sin_addr.s_addr = inet_addr( "0.0.0.0" );
     addr.sin_port        = htons( 2323 );

     if (bind( fd, &addr, sizeof(addr) )) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Server: Could not bind() the socket!\n" );
          close( fd );
          return ret;
     }

     /* Start listening. */
     if (listen( fd, 4 )) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Server: Could not listen() to the socket!\n" );
          close( fd );
          return ret;
     }

     /* Allocate server structure. */
     server = D_CALLOC( 1, sizeof(VoodooServer) );
     if (!server) {
          D_WARN( "out of memory" );
          close( fd );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Initialize server structure. */
     server->fd = fd;

     /* Return the new server. */
     *ret_server = server;

     return DFB_OK;
}

DirectResult
voodoo_server_register( VoodooServer         *server,
                        const char           *name,
                        VoodooSuperConstruct  func,
                        void                 *ctx )
{
     Super *super;

     D_ASSERT( server != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( func != NULL );

     if (server->num_super == MAX_SUPER)
          return DFB_LIMITEXCEEDED;

     super = &server->supers[server->num_super++];

     super->name = name;
     super->func = func;
     super->ctx  = ctx;

     return DFB_OK;
}

static inline Super *
lookup_super( VoodooServer *server,
              const char   *name )
{
     int i;

     D_ASSERT( server != NULL );
     D_ASSERT( name != NULL );

     for (i=0; i<server->num_super; i++) {
          Super *super = &server->supers[i];

          if (! strcmp( name, super->name ))
               return super;
     }

     return NULL;
}

DirectResult
voodoo_server_construct( VoodooServer      *server,
                         VoodooManager     *manager,
                         const char        *name,
                         VoodooInstanceID  *ret_instance )
{
     DirectResult      ret;
     Super            *super;
     VoodooInstanceID  instance;

     D_ASSERT( server != NULL );
     D_ASSERT( manager != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_instance != NULL );

     super = lookup_super( server, name );
     if (!super) {
          D_ERROR( "Voodoo/Server: Super interface '%s' is not available!\n", name );
          return DFB_UNSUPPORTED;
     }

     ret = super->func( server, manager, name, super->ctx, &instance );
     if (ret) {
          D_ERROR( "Voodoo/Server: "
                   "Creating super interface '%s' failed (%s)!\n", name, DirectResultString(ret) );
          return ret;
     }

     *ret_instance = instance;

     return DFB_OK;
}

DirectResult
voodoo_server_run( VoodooServer *server )
{
     DirectLink *l, *n;
     struct pollfd pf;

     D_ASSERT( server != NULL );

     while (!server->quit) {
          /* Cleanup dead connections. */
          direct_list_foreach_safe (l, n, server->connections) {
               Connection *connection = (Connection*) l;

               if (voodoo_manager_is_closed( connection->manager )) {
                    voodoo_manager_destroy( connection->manager );

                    close( connection->fd );

                    direct_list_remove( &server->connections, l );

                    D_INFO( "Voodoo/Server: Closed connection.\n" );

                    D_FREE( connection );

                    if (!server->connections) {
                         return DFB_OK;
                    }
               }
          }

          pf.fd     = server->fd;
          pf.events = POLLIN;

          switch (poll( &pf, 1, 200 )) {
               default:
                    accept_connection( server );
                    break;

               case 0:
                    D_HEAVYDEBUG( "Voodoo/Server: Timeout during poll()\n" );
                    break;

               case -1:
                    if (errno != EINTR) {
                         D_PERROR( "Voodoo/Server: Could not poll() the socket!\n" );
                         server->quit = true;
                    }
                    break;
          }
     }

     return DFB_OK;
}

DirectResult
voodoo_server_destroy( VoodooServer *server )
{
     DirectLink *l;

     D_ASSERT( server != NULL );

     close( server->fd );

     /* Close all connections. */
     direct_list_foreach (l, server->connections) {
          Connection *connection = (Connection*) l;

          voodoo_manager_destroy( connection->manager );

          close( connection->fd );

          D_FREE( connection );
     }

     D_FREE( server );

     return DFB_OK;
}

/**************************************************************************************************/

static DirectResult
accept_connection( VoodooServer *server )
{
     DirectResult     ret;
     struct sockaddr  addr;
     socklen_t        addrlen;
     Connection      *connection;

     connection = D_CALLOC( 1, sizeof(Connection) );
     if (!connection) {
          D_WARN( "out of memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     connection->fd = accept( server->fd, &addr, &addrlen );
     if (connection->fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Server: Could not accept() incoming connection!\n" );
          D_FREE( connection );
          return ret;
     }

     /* Avoid pending messages. */
     if (setsockopt( connection->fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Server: Could not set TCP_NODELAY!\n" );

     D_INFO( "Voodoo/Server: Accepted connection.\n" );

     ret = voodoo_manager_create( connection->fd, NULL, server, &connection->manager );
     if (ret) {
          close( connection->fd );
          D_FREE( connection );
          return ret;
     }

     direct_list_prepend( &server->connections, &connection->link );

     return DFB_OK;
}

