/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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


DirectResult
voodoo_client_create( const char     *hostname,
                      int             session,
                      VoodooClient  **ret_client )
{
     DirectResult     ret;
     int              err, fd;
     struct addrinfo  hints;
     struct addrinfo *addr;
     VoodooClient    *client;

     D_ASSERT( ret_client != NULL );

     memset( &hints, 0, sizeof(hints) );
     hints.ai_flags    = AI_CANONNAME;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_family   = PF_UNSPEC;

     D_INFO( "Voodoo/Client: Looking up host '%s'...\n", hostname );

     err = getaddrinfo( hostname, "2323", &hints, &addr );
     if (err) {
          switch (err) {
               case EAI_FAMILY:
                    D_ERROR( "Direct/Log: Unsupported address family!\n" );
                    return DFB_UNSUPPORTED;
               
               case EAI_SOCKTYPE:
                    D_ERROR( "Direct/Log: Unsupported socket type!\n" );
                    return DFB_UNSUPPORTED;
               
               case EAI_NONAME:
                    D_ERROR( "Direct/Log: Host not found!\n" );
                    return DFB_FAILURE;
                    
               case EAI_SERVICE:
                    D_ERROR( "Direct/Log: Port 2323 is unreachable!\n" );
                    return DFB_FAILURE;
               
               case EAI_ADDRFAMILY:
               case EAI_NODATA:
                    D_ERROR( "Direct/Log: Host found, but has no address!\n" );
                    return DFB_FAILURE;
                    
               case EAI_MEMORY:
                    return D_OOM();

               case EAI_FAIL:
                    D_ERROR( "Direct/Log: A non-recoverable name server error occurred!\n" );
                    return DFB_FAILURE;

               case EAI_AGAIN:
                    D_ERROR( "Direct/Log: Temporary error, try again!\n" );
                    return DFB_TEMPUNAVAIL;
                    
               default:
                    D_ERROR( "Direct/Log: Unknown error occured!?\n" );
                    return DFB_FAILURE;
          }
     }

     /* Create the client socket. */
     fd = socket( addr->ai_family, SOCK_STREAM, 0 );
     if (fd < 0) {
          D_PERROR( "Voodoo/Client: Could not create the socket via socket()!\n" );
          freeaddrinfo( addr );
          return errno2result( errno );
     }

     D_INFO( "Voodoo/Client: Connecting to '%s:2323'...\n", addr->ai_canonname );

     /* Connect to the server. */
     err = connect( fd, addr->ai_addr, addr->ai_addrlen );
     freeaddrinfo( addr );

     if (err) {
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

