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
#include <netdb.h>
extern int h_errno;

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/internal.h>


struct __V_VoodooClient {
     int            fd;
     VoodooManager *manager;
};

static const int one = 1;

DirectResult
voodoo_client_create( const char     *hostname,
                      int             session,
                      VoodooClient  **ret_client )
{
     DirectResult        ret;
     int                 fd;
     struct sockaddr_in  sock_addr;
     VoodooClient       *client;
     struct in_addr      addr;

     D_ASSERT( ret_client != NULL );

     if (!inet_aton( hostname, &addr )) {
          struct hostent *host;

          D_INFO( "Voodoo/Client: Looking up host '%s'...\n", hostname );

          /* TODO: use getaddrinfo(3) and support IPv6 */
          host = gethostbyname( hostname );
          if (!host) {
               switch (h_errno) {
                    case HOST_NOT_FOUND:
                         D_ERROR( "Voodoo/Client: Host not found!\n" );
                         return DFB_FAILURE;

                    case NO_ADDRESS:
                         D_ERROR( "Voodoo/Client: Host found, but has no address!\n" );
                         return DFB_FAILURE;

                    case NO_RECOVERY:
                         D_ERROR( "Voodoo/Client: A non-recoverable name server error occurred!\n" );
                         return DFB_FAILURE;

                    case TRY_AGAIN:
                         D_ERROR( "Voodoo/Client: Temporary error, try again!\n" );
                         return DFB_TEMPUNAVAIL;
               }

               D_ERROR( "Voodoo/Client: Unknown error occured!?\n" );
               return DFB_FAILURE;
          }

          if (host->h_addrtype != AF_INET || host->h_length != 4) {
               D_ERROR( "Voodoo/Client: Not an IPv4 address!\n" );
               return DFB_FAILURE;
          }

          memcpy( &addr, host->h_addr_list[0], sizeof(addr) );
     }

     /* Create the client socket. */
     fd = socket( PF_INET, SOCK_STREAM, 0 );
     if (fd < 0) {
          D_PERROR( "Voodoo/Client: Could not create the socket via socket()!\n" );
          return errno2result( errno );
     }

     /* Avoid pending messages. */
     if (setsockopt( fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Server: Could not set TCP_NODELAY!\n" );

     sock_addr.sin_family = AF_INET;
     sock_addr.sin_addr   = addr;
     sock_addr.sin_port   = htons( 2323 );

     D_INFO( "Voodoo/Client: Connecting to '%s:2323'...\n", inet_ntoa( addr ) );

     /* Connect to the server. */
     if (connect( fd, &sock_addr, sizeof(sock_addr) )) {
          ret = errno2result( errno );
          D_PERROR( "Voodoo/Client: Could not connect() to the server!\n" );
          close( fd );
          return ret;
     }

     /* Allocate client structure. */
     client = D_CALLOC( 1, sizeof(VoodooClient) );
     if (!client) {
          D_WARN( "out of memory" );
          close( fd );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Initialize client structure. */
     client->fd = fd;

     /* Create the manager. */
     ret = voodoo_manager_create( fd, client, NULL, &client->manager );
     if (ret) {
          D_FREE( client );
          close( fd );
          return ret;
     }

     /* Return the new client. */
     *ret_client = client;

     return DFB_OK;
}

DirectResult
voodoo_client_destroy( VoodooClient *client )
{
     D_ASSERT( client != NULL );

     voodoo_manager_destroy( client->manager );

     close( client->fd );

     D_FREE( client );

     return DFB_OK;
}

VoodooManager *
voodoo_client_manager( const VoodooClient *client )
{
     D_ASSERT( client != NULL );

     return client->manager;
}

